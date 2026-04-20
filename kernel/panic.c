#include "panic.h"
#include "log/log.h"
#include "drivers/vga/vga.h"
#include "cpu/isr.h" // For registers_t
#include <stdlib.h>
/*
 * kernel_panic
 * ────────────
 * This function is called when a fatal, unrecoverable error occurs
 * in the kernel. It attempts to provide as much diagnostic information
 * as possible before halting the system.
 *
 * Parameters:
 *   msg  — A descriptive message about the cause of the panic.
 *   regs — Optional pointer to a registers_t structure, containing the
 *          CPU state at the time of the interrupt/exception that led
 *          to the panic. Can be NULL if the panic is not interrupt-driven.
 */
void kernel_panic(const char *msg, registers_t *regs)
{
    // 1. Disable interrupts to prevent further execution and state changes
    __asm__ volatile("cli");

    // 2. Clear screen and set a panic-specific background color (e.g., red)
    vga_set_colors(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear_screen();
    vga_set_cursor(0, 0);

    // 3. Print the panic message
    log_fatal("KERNEL PANIC!");
    log_fatal("Message: %s", msg);

    // 4. If CPU state is available, print relevant register information
    if (regs != NULL)
    {
        log_fatal("  EIP: 0x%x, CS: 0x%x, EFLAGS: 0x%x", regs->eip, regs->cs, regs->eflags);
        log_fatal("  ESP: 0x%x, SS: 0x%x", regs->esp, regs->ss); // Note: user_esp is more accurate for user mode
        log_fatal("  EAX: 0x%x, EBX: 0x%x, ECX: 0x%x, EDX: 0x%x", regs->eax, regs->ebx, regs->ecx, regs->edx);
        log_fatal("  ESI: 0x%x, EDI: 0x%x, EBP: 0x%x", regs->esi, regs->edi, regs->ebp);
        log_fatal("  Interrupt No: 0x%x, Error Code: 0x%x", regs->int_no, regs->err_code);
    }
    else
    {
        log_fatal("  No CPU state available (panic not interrupt-driven).");
    }

    // 5. Halt the CPU indefinitely
    log_fatal("System Halted.");
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}