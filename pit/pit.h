#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/*
 * pit_init - Configure the PIT (Intel 8253/8254) to fire IRQ0
 * at the given frequency in Hz.
 *
 * Call this AFTER pic_init() and AFTER your IDT has irq0_stub
 * installed at vector 0x20, then call sti to unmask interrupts.
 *
 * Typical value: pit_init(100)  -> 100 ticks per second
 */
void pit_init(uint32_t hz);
uint32_t pit_get_frequency(); // New: Returns the currently configured PIT frequency

#endif