#include "drivers/vga/vga.h"
#include "lib/printf.h"
#include "log/log.h"
#include "gdt_and_idt/idt.h" // Include the IDT header
#include "gdt_and_idt/gdt.h" // ← ADD THIS

#include "pic/pic.h"
#include "pit/pit.h"
#include "time/time.h"
#include "kmalloc.h" // add this at the top
#include "tests.h"
/*
 * These are the raw ASM entry points defined in cpu/isr_stub.asm.
 * The IDT (built in Phase 2) stores their addresses so the CPU knows
 * where to jump when each interrupt fires.
 * We declare them here as extern so the linker can resolve them.
 */
extern void isr0_stub();  /* INT 0x00 - Divide By Zero        */
extern void isr8_stub();  /* INT 0x08 - Double Fault          */
extern void isr13_stub(); /* INT 0x0D - General Protection    */
extern void isr14_stub(); /* INT 0x0E - Page Fault            */
extern void irq0_stub();  /* INT 0x20 - PIT Timer (IRQ0)      */

/* ---------------------------------------------------------------
 * A simple busy-wait using the tick counter as a time source.
 * Waits until `ms` milliseconds worth of ticks have passed.
 * At 100 Hz, 1 tick = 10 ms, so ms is rounded to the nearest 10.
 * --------------------------------------------------------------- */
static void sleep_ms(uint32_t ms)
{
    // Use the actual PIT frequency for more accurate sleep calculations
    uint32_t pit_hz = pit_get_frequency();
    if (pit_hz == 0)
    {
        // Fallback or error if PIT not initialized or frequency is 0
        // For now, a simple busy loop, but ideally would use a working timer.
        for (volatile uint32_t i = 0; i < ms * 1000; i++)
            ; // Very rough busy-wait
        return;
    }

    uint64_t start_ticks = time_get_ticks();
    // Calculate target ticks: (ms / 1000) * pit_hz = (ms * pit_hz) / 1000
    uint64_t target_ticks = start_ticks + (uint64_t)ms * pit_hz / 1000;

    while (time_get_ticks() < target_ticks)
        __asm__ volatile("hlt"); /* hlt saves power between interrupts */
}

/* ---------------------------------------------------------------
 * Kernel entry point.
 * Your bootloader (e.g. GRUB via Multiboot) jumps here after
 * setting up a basic stack and loading the kernel image.
 * --------------------------------------------------------------- */
void kernel_main()
{
    /* 1. Clear the screen and say hello */
    vga_init();
    log_info("Kernel booting...");

    /* 2. Initialize the Global Descriptor Table (GDT) if not already done by bootloader.
     *    (Assuming GDT is handled before kernel_main or is minimalistic) */
    gdt_init();                           // Uncomment if your GDT requires C-level initialization here.
    kmalloc_init(0x00200000, 0x00100000); // 1MB heap starting at 2MB mark
    log_info("Heap initialized at 0x200000, size 1MB");
    /* 3. Initialize and remap the Programmable Interrupt Controller (PIC)
     *    This ensures hardware IRQs don't collide with CPU exceptions. */
    pic_init();

    /* 4. Initialize the Interrupt Descriptor Table (IDT).
     *    This sets up all CPU exception handlers and hardware IRQ stubs. */
    idt_init();
    log_info("Interrupt Descriptor Table (IDT) initialized.");

    /* 5. Configure the Programmable Interval Timer (PIT) to fire IRQ0.
     *    Each tick calls irq0_timer_handler -> time_set_ticks(ticks + 1). */
    uint32_t desired_pit_freq = 100; // 100 Hz
    pit_init(desired_pit_freq);
    log_info("PIT configured to %d Hz.", pit_get_frequency());

    /* 6. Enable interrupts - from this point the timer starts ticking and
     *    other hardware interrupts (like keyboard) can be processed. */
    __asm__ volatile("sti");
    log_info("Interrupts enabled globally. System tick counter is active.");

    /* 7. Show the tick counter incrementing so you can confirm it works */
    log_info("Watching system ticks for 5 seconds...");
    for (int i = 0; i < 5; i++)
    {
        sleep_ms(1000);
        log_info("  System ticks: %d", (int)time_get_ticks());
    }

    /* 8. Kernel idle loop - hlt saves power between interrupts. */
    log_info("Boot sequence complete. Entering kernel idle loop.");

    // Add this in kernel_main() before the idle loop:

    /* 9. Test dynamic memory allocation */
    log_info("Testing kmalloc...");

    // Test 1: basic allocation
    uint32_t *a = (uint32_t *)kmalloc(sizeof(uint32_t) * 4);
    if (a == NULL)
    {
        log_info("  [FAIL] kmalloc returned NULL!");
    }
    else
    {
        log_info("  [PASS] kmalloc returned address: 0x%x", (uint32_t)a);
    }

    // Test 2: write and read back
    a[0] = 0xDEADBEEF;
    a[1] = 0xCAFEBABE;
    if (a[0] == 0xDEADBEEF && a[1] == 0xCAFEBABE)
    {
        log_info("  [PASS] Write/read back correct: 0x%x 0x%x", a[0], a[1]);
    }
    else
    {
        log_info("  [FAIL] Memory corruption detected!");
    }

    // Test 3: two allocations don't overlap
    uint32_t *b = (uint32_t *)kmalloc(sizeof(uint32_t) * 4);
    if (b == NULL)
    {
        log_info("  [FAIL] second kmalloc returned NULL!");
    }
    else if (b == a)
    {
        log_info("  [FAIL] both allocs returned same address!");
    }
    else
    {
        log_info("  [PASS] Two allocs at different addresses: 0x%x and 0x%x", (uint32_t)a, (uint32_t)b);
    }

    // Test 4: free and reallocate
    kfree(a);
    uint32_t *c = (uint32_t *)kmalloc(sizeof(uint32_t) * 4);
    log_info("  After free, new alloc at: 0x%x (should reuse 0x%x)", (uint32_t)c, (uint32_t)a);

    run_all_tests(); // Run the comprehensive test suite for all kernel phases
    for (;;)
        __asm__ volatile("hlt");
}