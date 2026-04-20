#include "isr.h"
#include "../log/log.h"
#include "../time/time.h"
#include "../pic/pic.h"
#include "../kernel/panic.h" // Include kernel/panic.h for kernel_panic

/*
 * isr_unhandled_interrupt
 * ───────────────────────
 * A generic handler for interrupts that don't have a specific C routine
 * registered. This is where you might log unexpected interrupts.
 */
void isr_unhandled_interrupt(registers_t *regs)
{
    log_warn("Unhandled Interrupt (Vector: 0x%x, Error Code: 0x%x, EIP: 0x%x)",
             regs->int_no, regs->err_code, regs->eip);
    // For a real system, you might gracefully shut down the offending task
    // or log more context. For now, we'll just acknowledge and continue.
    // If it's a hardware IRQ, we must send EOI.
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        pic_send_eoi(regs->int_no - 32);
    }
}

/*
 * INT 0x00 - Divide By Zero Exception
 * ────────────────────────────────────
 * Fires when code executes DIV or IDIV with a zero divisor.
 * This is a critical error, usually unrecoverable.
 */
void isr_divide_by_zero(registers_t *regs)
{
    kernel_panic("EXCEPTION #0: Divide By Zero", regs);
}

/*
 * INT 0x06 - Invalid Opcode Exception
 * ───────────────────────────────────
 * Fires when the CPU encounters an instruction that is not valid.
 * This indicates corrupted code or a serious programming error.
 */
void isr_invalid_opcode(registers_t *regs)
{
    kernel_panic("EXCEPTION #6: Invalid Opcode", regs);
}

/*
 * INT 0x08 - Double Fault Exception
 * ─────────────────────────────────
 * Fires when the CPU encounters an exception while trying to
 * handle another exception. This typically indicates a severely
 * corrupted stack or an issue in an exception handler itself.
 */
void isr_double_fault(registers_t *regs)
{
    kernel_panic("EXCEPTION #8: Double Fault", regs);
}

/*
 * INT 0x0D - General Protection Fault Exception
 * ─────────────────────────────────────────────
 * Fires on segment violations, privilege violations, invalid memory accesses
 * (e.g., writing to a read-only segment), invalid TSS operations, etc.
 * The error code provides information about the cause of the fault.
 */
void isr_general_protection(registers_t *regs)
{
    // The previous logging provided specific details. kernel_panic will now
    // print general info and the registers. Specific details about GPF
    // error code flags should be handled within kernel_panic if desired.
    kernel_panic("EXCEPTION #13: General Protection Fault", regs);
}

/*
 * INT 0x0E - Page Fault Exception
 * ───────────────────────────────
 * Fires when the CPU tries to access a virtual address that has no valid
 * mapping in the page tables, or the access violates defined permissions.
 * CR2 register holds the linear (virtual) address that caused the fault.
 * The error code (pushed by CPU) describes the nature of the fault.
 */
void isr_page_fault(registers_t *regs)
{
    // The previous logging provided specific details. kernel_panic will now
    // print general info and the registers. Specific details about Page Fault
    // error code flags should be handled within kernel_panic if desired.
    kernel_panic("EXCEPTION #14: Page Fault", regs);
}

/*
 * IRQ0 -> INT 0x20 - PIT Timer Handler
 * ────────────────────────────────────
 * This function is called by the `isr_common_handler` every time
 * the Programmable Interval Timer (PIT) fires IRQ0.
 *
 * It increments a global tick counter, which is essential for timekeeping
 * and later for scheduling tasks in the OS.
 *
 * It MUST send an End-Of-Interrupt (EOI) signal to the PICs to acknowledge
 * the interrupt, otherwise the PIC will not send further interrupts.
 */
void irq0_timer_handler(registers_t *regs)
{
    (void)regs; // Parameter 'regs' is not used in this basic handler, so cast to void to suppress warnings.

    time_set_ticks(time_get_ticks() + 1); // Increment the global system tick counter

    // Send EOI to the Master PIC for IRQ0
    pic_send_eoi(0);

    // In a more advanced kernel, a scheduler might be invoked here
    // to switch to another task, or other time-based events could be processed.
}