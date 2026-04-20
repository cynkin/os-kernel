#include "idt.h"
#include <string.h>     /* memset */
#include "../cpu/isr.h" // Include isr.h for isr_unhandled_interrupt and other handlers

extern void idt_flush(uint32_t);
/*
 * idt[] — the 256-entry table that lives in memory.
 * The CPU indexes into this on every interrupt.
 * 256 entries × 8 bytes = 2048 bytes total.
 */
static idt_entry_t idt[IDT_ENTRY_COUNT];

/* IDTR — the 6-byte pointer we load with lidt */
static idt_ptr_t idt_ptr;

/*
 * isr_handlers[] — table of C-level handler function pointers.
 * Index = interrupt vector number.
 * Drivers or subsystems call idt_register_handler() to fill these in.
 * NULL means "no C handler registered" — isr_common_handler will note it.
 */
static isr_handler_t isr_handlers[IDT_ENTRY_COUNT];

/*
 * idt_set_entry()
 * ───────────────
 * Write one gate descriptor into idt[vector].
 *
 * Parameters:
 *   vector   — 0–255, the interrupt number this gate handles
 *   handler  — 32-bit address of the ASM ISR stub
 *   selector — GDT code selector (0x08 = kernel code, always)
 *   type_attr— gate type + DPL + present bit (IDT_KERNEL_INTERRUPT etc.)
 *
 * The handler address is split across offset_low and offset_high
 * because of the same fragmented hardware format as the GDT base.
 */
void idt_set_entry(uint8_t vector, uint32_t handler,
                   uint16_t selector, uint8_t type_attr)
{
    idt_entry_t *e = &idt[vector];

    e->offset_low = handler & 0xFFFF;          /* lower 16 bits of handler address */
    e->offset_high = (handler >> 16) & 0xFFFF; /* upper 16 bits of handler address */
    e->selector = selector;                    /* must be kernel code segment 0x08 */
    e->zero = 0;                               /* reserved — always 0              */
    e->type_attr = type_attr;                  /* P | DPL | gate type              */
}

/*
 * idt_register_handler()
 * ──────────────────────
 * Register a C function to be called when 'vector' fires.
 * Called by device drivers AFTER idt_init().
 *
 * Example:
 *   idt_register_handler(33, keyboard_handler);  // IRQ1 = keyboard
 *   idt_register_handler(14, page_fault_handler); // #PF
 */
void idt_register_handler(uint8_t vector, isr_handler_t fn)
{
    isr_handlers[vector] = fn;
}

/*
 * isr_common_handler()
 * ─────────────────────
 * Called by isr_common_stub (in assembly) for EVERY interrupt.
 * 'frame' points to the interrupt_frame_t built on the kernel stack.
 *
 * Dispatches to the registered C handler for frame->int_no.
 * If none is registered, this is where you'd print a kernel panic
 * or log the unhandled exception.
 *
 * This function is called from assembly — mark it global so the
 * linker can find it.
 */
void isr_common_handler(interrupt_frame_t *frame)
{
    if (isr_handlers[frame->int_no] != NULL) // Use NULL instead of 0 for clarity
    {
        /* Call the registered C handler, passing the full CPU state */
        isr_handlers[frame->int_no](frame);
    }
    else
    {
        /*
         * No specific handler was registered for this interrupt.
         * Call the generic unhandled interrupt handler.
         */
        isr_unhandled_interrupt(frame);
    }
}

/*
 * idt_init()
 * ──────────
 * 1. Zero all 256 IDT entries and the handler table.
 * 2. Install ASM stubs for all CPU exceptions (0–21).
 * 3. Install ASM stubs for all hardware IRQs (32–47).
 * 4. Load IDTR via idt_flush().
 *
 * NOTE: Before calling idt_init(), you should remap the 8259 PIC
 * so that IRQ0–15 map to vectors 32–47 (not 8–15 which overlap
 * CPU exceptions). That is done in pic_init() / pic_remap().
 */
void idt_init(void)
{
    /* Zero everything — unset entries are "not present" (P=0) */
    memset(idt, 0, sizeof(idt));
    memset(isr_handlers, 0, sizeof(isr_handlers));

    /* Build IDTR: size = (256 × 8) − 1 = 2047 */
    idt_ptr.limit = (uint16_t)(sizeof(idt_entry_t) * IDT_ENTRY_COUNT - 1);
    idt_ptr.base = (uint32_t)&idt;

    /*
     * ── CPU EXCEPTION GATES (vectors 0–21) ───────────────────────────
     *
     * All use selector 0x08 (kernel code) and IDT_KERNEL_INTERRUPT,
     * EXCEPT debug exceptions which use IDT_KERNEL_TRAP so that IF
     * is not cleared (debugger needs interrupts enabled).
     *
     * Vectors with (*) have a CPU-pushed error code on the stack.
     * Our ISR_NO_ERR macro pushes a dummy 0 for the rest to keep
     * interrupt_frame_t layout uniform.
     */
    idt_set_entry(0, (uint32_t)isr0, 0x08, IDT_KERNEL_INTERRUPT);   /* #DE  Divide-by-Zero            */
    idt_set_entry(1, (uint32_t)isr1, 0x08, IDT_KERNEL_TRAP);        /* #DB  Debug (trap gate)         */
    idt_set_entry(2, (uint32_t)isr2, 0x08, IDT_KERNEL_INTERRUPT);   /* NMI                            */
    idt_set_entry(3, (uint32_t)isr3, 0x08, IDT_KERNEL_TRAP);        /* #BP  Breakpoint (trap gate)    */
    idt_set_entry(4, (uint32_t)isr4, 0x08, IDT_KERNEL_INTERRUPT);   /* #OF  Overflow                  */
    idt_set_entry(5, (uint32_t)isr5, 0x08, IDT_KERNEL_INTERRUPT);   /* #BR  Bound Range               */
    idt_set_entry(6, (uint32_t)isr6, 0x08, IDT_KERNEL_INTERRUPT);   /* #UD  Invalid Opcode            */
    idt_set_entry(7, (uint32_t)isr7, 0x08, IDT_KERNEL_INTERRUPT);   /* #NM  Device Not Available      */
    idt_set_entry(8, (uint32_t)isr8, 0x08, IDT_KERNEL_INTERRUPT);   /* #DF  Double Fault        (*)   */
    idt_set_entry(9, (uint32_t)isr9, 0x08, IDT_KERNEL_INTERRUPT);   /* Coprocessor Overrun (legacy)   */
    idt_set_entry(10, (uint32_t)isr10, 0x08, IDT_KERNEL_INTERRUPT); /* #TS  Invalid TSS         (*)   */
    idt_set_entry(11, (uint32_t)isr11, 0x08, IDT_KERNEL_INTERRUPT); /* #NP  Seg Not Present     (*)   */
    idt_set_entry(12, (uint32_t)isr12, 0x08, IDT_KERNEL_INTERRUPT); /* #SS  Stack-Segment Fault (*)   */
    idt_set_entry(13, (uint32_t)isr13, 0x08, IDT_KERNEL_INTERRUPT); /* #GP  General Protection  (*)   */
    idt_set_entry(14, (uint32_t)isr14, 0x08, IDT_KERNEL_INTERRUPT); /* #PF  Page Fault          (*)   */
    idt_set_entry(15, (uint32_t)isr15, 0x08, IDT_KERNEL_INTERRUPT); /* Reserved                       */
    idt_set_entry(16, (uint32_t)isr16, 0x08, IDT_KERNEL_INTERRUPT); /* #MF  x87 FP Exception          */
    idt_set_entry(17, (uint32_t)isr17, 0x08, IDT_KERNEL_INTERRUPT); /* #AC  Alignment Check     (*)   */
    idt_set_entry(18, (uint32_t)isr18, 0x08, IDT_KERNEL_INTERRUPT); /* #MC  Machine Check             */
    idt_set_entry(19, (uint32_t)isr19, 0x08, IDT_KERNEL_INTERRUPT); /* #XM  SIMD FP Exception         */
    idt_set_entry(20, (uint32_t)isr20, 0x08, IDT_KERNEL_INTERRUPT); /* #VE  Virtualization            */
    idt_set_entry(21, (uint32_t)isr21, 0x08, IDT_KERNEL_INTERRUPT); /* Reserved                       */

    /*
     * ── HARDWARE IRQ GATES (vectors 32–47) ───────────────────────────
     *
     * These map to physical IRQ lines from the 8259A PIC.
     * They only work correctly AFTER pic_remap(0x20, 0x28)
     * which remaps IRQ0–7 → 0x20 (32) and IRQ8–15 → 0x28 (40).
     */
    idt_set_entry(32, (uint32_t)irq0, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ0  PIT Timer          */
    idt_set_entry(33, (uint32_t)irq1, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ1  Keyboard           */
    idt_set_entry(34, (uint32_t)irq2, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ2  PIC Cascade        */
    idt_set_entry(35, (uint32_t)irq3, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ3  COM2               */
    idt_set_entry(36, (uint32_t)irq4, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ4  COM1               */
    idt_set_entry(37, (uint32_t)irq5, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ5  LPT2               */
    idt_set_entry(38, (uint32_t)irq6, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ6  Floppy             */
    idt_set_entry(39, (uint32_t)irq7, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ7  LPT1 / Spurious    */
    idt_set_entry(40, (uint32_t)irq8, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ8  RTC                */
    idt_set_entry(41, (uint32_t)irq9, 0x08, IDT_KERNEL_INTERRUPT);  /* IRQ9  Free               */
    idt_set_entry(42, (uint32_t)irq10, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ10 Free               */
    idt_set_entry(43, (uint32_t)irq11, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ11 Free               */
    idt_set_entry(44, (uint32_t)irq12, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ12 PS/2 Mouse         */
    idt_set_entry(45, (uint32_t)irq13, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ13 FPU                */
    idt_set_entry(46, (uint32_t)irq14, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ14 Primary ATA        */
    idt_set_entry(47, (uint32_t)irq15, 0x08, IDT_KERNEL_INTERRUPT); /* IRQ15 Secondary ATA      */

    // Register C-level handlers for CPU exceptions
    idt_register_handler(0, isr_divide_by_zero);
    idt_register_handler(6, isr_invalid_opcode); // Register the new Invalid Opcode handler
    idt_register_handler(8, isr_double_fault);
    idt_register_handler(13, isr_general_protection);
    idt_register_handler(14, isr_page_fault);

    // Register C-level handler for PIT Timer IRQ
    idt_register_handler(32, irq0_timer_handler);

    // For all other interrupt vectors that haven't been explicitly assigned a handler,
    // register the generic unhandled interrupt handler. This ensures every interrupt
    // vector has a C-level handler function.
    for (int i = 0; i < IDT_ENTRY_COUNT; i++)
    {
        if (isr_handlers[i] == NULL)
        {
            isr_handlers[i] = isr_unhandled_interrupt;
        }
    }

    /* Load IDTR and enable interrupts (sti is called inside idt_flush) */
    idt_flush((uint32_t)&idt_ptr);
}