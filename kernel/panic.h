#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include "cpu/isr.h"       // For registers_t
#include "drivers/vga/vga.h" // For VGA_COLOR definitions

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
void kernel_panic(const char *msg, registers_t *regs);

#endif // KERNEL_PANIC_H
