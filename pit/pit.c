#include "pit.h"
#include "../pic/pic.h"
#include "../log/log.h" // For logging

/*
 * The PIT has a fixed input clock of 1,193,182 Hz.
 * We tell it a divisor; it fires IRQ0 every (divisor) clock ticks.
 *
 *   divisor = 1,193,182 / desired_hz
 *
 * Example: 1,193,182 / 100 = 11931  -> IRQ0 fires 100 times per second.
 */
#define PIT_BASE_FREQ 1193182
#define PIT_MIN_FREQ 19        // Smallest divisor is 1, so max freq is PIT_BASE_FREQ. Min freq is ~18.2 Hz (divisor 0xFFFF)
#define PIT_MAX_FREQ 1193182   // Max theoretical frequency (divisor 1)

/* PIT I/O ports */
#define PIT_DATA_CH0 0x40 /* Channel 0 data port (read/write) */
#define PIT_CMD 0x43      /* Mode/command register (write only) */

/*
 * Command byte breakdown (we send 0x36):
 *   Bits 7-6: 00  = channel 0
 *   Bits 5-4: 11  = lo byte then hi byte
 *   Bits 3-1: 011 = mode 3 (square wave generator)
 *   Bit  0:   0   = binary counting
 */
#define PIT_CMD_BINARY_MODE3 0x36

static uint32_t current_pit_hz = 0; // Store the currently configured frequency

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pit_init(uint32_t hz)
{
    if (hz == 0) {
        log_error("PIT: Attempted to set frequency to 0 Hz. Aborting initialization.");
        return;
    }
    if (hz < PIT_MIN_FREQ) {
        log_warn("PIT: Requested frequency %d Hz is very low (min recommended %d Hz).", hz, PIT_MIN_FREQ);
    }
    if (hz > PIT_MAX_FREQ) {
        log_warn("PIT: Requested frequency %d Hz exceeds maximum theoretical frequency %d Hz. Capping.", hz, PIT_MAX_FREQ);
        hz = PIT_MAX_FREQ;
    }

    uint32_t divisor = PIT_BASE_FREQ / hz;
    if (divisor == 0) { // Ensure divisor is at least 1
        divisor = 1;
    }
    
    // Recalculate hz based on actual divisor for accuracy
    current_pit_hz = PIT_BASE_FREQ / divisor;

    log_info("PIT: Initializing with desired frequency %d Hz (actual %d Hz, divisor %d).", hz, current_pit_hz, divisor);

    /* Set operating mode */
    log_debug("PIT: Writing command byte 0x%x to port 0x%x (Channel 0, Mode 3, LSB/MSB access).", PIT_CMD_BINARY_MODE3, PIT_CMD);
    outb(PIT_CMD, PIT_CMD_BINARY_MODE3);

    /* Send divisor as two bytes: low byte first, then high byte */
    log_debug("PIT: Writing divisor LSB (0x%x) to port 0x%x.", (uint8_t)(divisor & 0xFF), PIT_DATA_CH0);
    outb(PIT_DATA_CH0, (uint8_t)(divisor & 0xFF));
    log_debug("PIT: Writing divisor MSB (0x%x) to port 0x%x.", (uint8_t)((divisor >> 8) & 0xFF), PIT_DATA_CH0);
    outb(PIT_DATA_CH0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Unmask IRQ0 so the PIC lets timer interrupts through */
    log_debug("PIT: Unmasking IRQ0 on PIC to allow timer interrupts.");
    pic_unmask_irq(0);

    log_info("PIT: Timer initialized and IRQ0 unmasked.");
}

/*
 * pit_get_frequency - Returns the currently configured PIT frequency in Hz.
 * If pit_init() has not been called, this will return 0.
 */
uint32_t pit_get_frequency() {
    return current_pit_hz;
}