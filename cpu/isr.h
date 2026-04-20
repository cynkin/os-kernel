#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * registers_t
 * ───────────
 * This structure represents the CPU's state (registers) saved on the
 * kernel stack when an interrupt or exception occurs.
 * The layout must precisely match the push order in isr_stubs.asm.
 */
typedef struct
{
    /* Saved by our assembly stub (pushed in reverse order by PUSHA) */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    /* Pushed by our assembly stub: interrupt number and error code
     * (a dummy 0 is pushed for exceptions that don't have a CPU-pushed error code) */
    uint32_t int_no, err_code;

    /* Pushed automatically by the CPU on interrupt/exception */
    uint32_t eip, cs, eflags, user_esp, ss;
} registers_t;

/*
 * C-level Interrupt Service Routines (ISRs)
 * ─────────────────────────────────────────
 * These functions are the entry points for specific CPU exceptions and
 * hardware interrupts, called from the common assembly dispatcher.
 */
void isr_unhandled_interrupt(registers_t *regs); /* Generic handler for unhandled interrupts */
void isr_divide_by_zero(registers_t *regs);      /* INT 0x00 - Divide By Zero Exception */
void isr_invalid_opcode(registers_t *regs);      /* INT 0x06 - Invalid Opcode Exception */
void isr_double_fault(registers_t *regs);        /* INT 0x08 - Double Fault Exception */
void isr_general_protection(registers_t *regs);  /* INT 0x0D - General Protection Fault Exception */
void isr_page_fault(registers_t *regs);          /* INT 0x0E - Page Fault Exception */

/*
 * C-level Interrupt Request (IRQ) Handlers
 * ────────────────────────────────────────
 * These are specific handlers for hardware interrupts.
 */
void irq0_timer_handler(registers_t *regs);      /* IRQ0 -> INT 0x20 - PIT Timer */

#endif