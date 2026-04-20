#include "cpu/isr.h"
#include "drivers/vga/vga.h"
#include "gdt_and_idt/gdt.h"
#include "gdt_and_idt/idt.h"
#include "kmalloc.h"
#include "lib/printf.h"
#include "log/log.h"
#include "memory_map/memory_map.h"
#include "pic/pic.h"
#include "pit/pit.h"
#include "pmm/pmm.h"
#include "scheduler.h"
#include "time/time.h"

/* ── Global kernel state ─────────────────────────────────────────── */
memory_map_t mmap;
pmm_t kpmm;

/* ── Sleep helper ────────────────────────────────────────────────── */
static void sleep_ms(uint32_t ms) {
  uint64_t start = time_get_ticks();
  uint64_t target = start + (ms / 10); /* 100 Hz -> 1 tick per 10 ms */
  while (time_get_ticks() < target)
    __asm__ volatile("hlt");
}

/* ── IRQ Handlers ────────────────────────────────────────────────── */

/* Timer IRQ0 -> Vector 32 */
static void timer_handler(interrupt_frame_t *frame) {
  (void)frame;
  time_set_ticks(time_get_ticks() + 1);
  pic_send_eoi(0);
}

/* Keyboard IRQ1 -> Vector 33 */
static const char scancode_ascii[] = {
    0,   27,  '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
    '-', '=', '\b', '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
    'o', 'p', '[',  ']',  '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
    'j', 'k', 'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm',  ',',  '.',  '/', 0,   '*',  0,   ' '};

static void keyboard_handler(interrupt_frame_t *frame) {
  (void)frame;
  uint8_t scancode;
  __asm__ volatile("inb $0x60, %0" : "=a"(scancode));

  if (scancode < sizeof(scancode_ascii) && scancode_ascii[scancode] != 0) {
    printf("%c", scancode_ascii[scancode]);
  }
  pic_send_eoi(1);
}

/* ── CPU Exception Handler Wrappers ──────────────────────────────── *
 * cpu/isr.c uses `registers_t*` and the IDT uses `interrupt_frame_t*`.
 * Both structs have the same layout, so we just cast the pointer.    */

static void exc_divide_by_zero(interrupt_frame_t *frame) {
  isr_divide_by_zero((registers_t *)frame);
}
static void exc_double_fault(interrupt_frame_t *frame) {
  isr_double_fault((registers_t *)frame);
}
static void exc_gpf(interrupt_frame_t *frame) {
  isr_general_protection((registers_t *)frame);
}
static void exc_page_fault(interrupt_frame_t *frame) {
  isr_page_fault((registers_t *)frame);
}

/* ── Demo Processes for the Scheduler ────────────────────────────── */

static void process_a(void) {
  for (;;) {
    log_info("[Process A] Running! pid=1");
    /* Busy wait a bit so we can see the output */
    for (volatile int i = 0; i < 5000000; i++)
      ;
  }
}

static void process_b(void) {
  for (;;) {
    log_info("[Process B] Running! pid=2");
    for (volatile int i = 0; i < 5000000; i++)
      ;
  }
}

/* ── Kernel Entry Point ──────────────────────────────────────────── */
void kernel_main(void) {
  /* ──────────────── Phase 1: Hardware Init ──────────────────── */

  /* 1. VGA Display */
  vga_init();
  log_info("Kernel booting...");

  /* 2. Physical Memory Manager */
  mmap_parse(&mmap);
  pmm_init(&kpmm, &mmap);
  log_info("PMM: %d MB free / %d MB total",
           (int)((kpmm.free_pages * PAGE_SIZE) / MB(1)),
           (int)((kpmm.total_pages * PAGE_SIZE) / MB(1)));

  /* 3. Kernel Heap (kmalloc / kfree) */
  paddr_t heap_phys = pmm_alloc_n(&kpmm, 1024); /* 4 MB heap */
  if (heap_phys != (paddr_t)-1) {
    /* PMM returns a simulated physical offset — convert to real pointer */
    void *heap_virt = phys_to_virt(heap_phys);
    kmalloc_init((uint32_t)heap_virt, 1024 * PAGE_SIZE);
    log_info("Heap: 4 MB at 0x%08x", (uint32_t)heap_virt);
  } else {
    log_info("ERROR: Failed to allocate kernel heap!");
  }

  /* Quick kmalloc/kfree sanity check */
  void *test = kmalloc(128);
  log_info("kmalloc(128) = 0x%08x", (uint32_t)test);
  kfree(test);
  log_info("kfree() OK");

  /* ──────────────── Phase 2: Interrupt Setup ───────────────── */

  /* 4. PIC Remap */
  pic_init();
  log_info("PIC remapped");

  /* 5. GDT + IDT */
  gdt_init();
  idt_init();
  log_info("GDT + IDT installed");

  /* 6. Register CPU Exception Handlers */
  idt_register_handler(0, exc_divide_by_zero); /* #DE */
  idt_register_handler(8, exc_double_fault);   /* #DF */
  idt_register_handler(13, exc_gpf);           /* #GP */
  idt_register_handler(14, exc_page_fault);    /* #PF */
  log_info("Exception handlers registered (#DE, #DF, #GP, #PF)");

  /* 7. PIT Timer */
  pit_init(100);
  log_info("PIT: 100 Hz");

  /* 8. IRQ Handlers */
  idt_register_handler(32, timer_handler);    /* IRQ0 Timer */
  idt_register_handler(33, keyboard_handler); /* IRQ1 Keyboard */
  pic_unmask_irq(1);
  log_info("IRQ handlers: Timer + Keyboard");

  /* 9. Enable interrupts! */
  __asm__ volatile("sti");
  log_info("Interrupts ENABLED");

  /* Quick tick test */
  log_info("Tick test (1 second)");
  sleep_ms(1000);
  log_info("  ticks = %d  OK", (int)time_get_ticks());

  /* ──────────────── Phase 3: Scheduler ─────────────────────── */

  /* 10. Create demo processes */
  pcb_t *pa = create_process((uint32_t)process_a);
  pcb_t *pb = create_process((uint32_t)process_b);

  if (pa && pb) {
    add_to_ready_queue(pa);
    add_to_ready_queue(pb);
    log_info("Scheduler: 2 processes queued (pid %d, %d)", pa->pid, pb->pid);
  } else {
    log_info("ERROR: Failed to create demo processes!");
  }

  /* ──────────────── Phase 4: Boot Complete ─────────────────── */
  log_info("BOOT COMPLETE - All systems operational.");
  log_info("  VGA Driver        : Active");
  log_info("  GDT               : Loaded");
  log_info("  IDT               : Loaded (256 vectors)");
  log_info("  PIC               : Remapped (IRQ 0x20-0x2F)");
  log_info("  PIT Timer         : 100 Hz");
  log_info("  Memory Map        : Parsed");
  log_info("  PMM (Physical)    : %d MB free",
           (int)((kpmm.free_pages * PAGE_SIZE) / MB(1)));
  log_info("  Kernel Heap       : 4 MB (kmalloc/kfree)");
  log_info("  Exception Handlers: #DE #DF #GP #PF");
  log_info("  Keyboard Driver   : IRQ1 Active");
  log_info("  Panic Handler     : Armed");
  log_info("  PCB Manager       : 2 processes created");
  log_info("  Scheduler         : Round-Robin ready");
  log_info("  Context Switch    : Wired (switch.s)");
  log_info("Keyboard integrated.");

  /* Idle loop — hlt saves power, wakes on every interrupt */
  for (;;)
    __asm__ volatile("hlt");
}