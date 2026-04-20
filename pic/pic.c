#include "pic.h"
#include "../log/log.h" // For logging

/* Master PIC: command=0x20, data=0x21 */
/* Slave  PIC: command=0xA0, data=0xA1 */
#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

/* End-of-interrupt command */
#define PIC_EOI 0x20

/* ICW1: start init sequence, expect ICW4 */
#define ICW1_INIT 0x11

/* ICW4: 8086 mode */
#define ICW4_8086 0x01

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Small delay by doing a dummy write to port 0x80 */
static inline void io_wait()
{
    // Writing to an unused I/O port, typically 0x80, is a common way
    // to introduce a small delay, ensuring that PIC operations
    // have enough time to complete before the next instruction.
    outb(0x80, 0x00);
}

/*
 * pic_init - Remap the two 8259 PIC chips and initialize their states.
 *
 * By default, the master PIC fires IRQ0-7 as INT 0x08-0x0F.
 * Those slots are already used by CPU exceptions (double fault,
 * GPF, etc.), so every hardware interrupt would look like a CPU
 * crash. We remap:
 *   Master IRQ0-7  -> INT 0x20-0x27 (vectors 32-39)
 *   Slave  IRQ8-15 -> INT 0x28-0x2F (vectors 40-47)
 *
 * This function also saves and restores the IRQ masks to ensure
 * that only explicitly unmasked IRQs will be delivered after remapping.
 */
void pic_init()
{
    log_info("Initializing 8259 PICs...");

    /* Save current masks so we can restore them after remapping.
     * This preserves the enabled/disabled state of IRQs. */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    log_debug("  Saved Master PIC mask: 0x%x, Slave PIC mask: 0x%x", mask1, mask2);

    // Start initialization sequence (ICW1)
    log_debug("  Sending ICW1 to Master PIC (0x%x) and Slave PIC (0x%x)", ICW1_INIT, ICW1_INIT);
    outb(PIC1_CMD, ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT);
    io_wait();

    // Set vector offsets (ICW2)
    log_debug("  Setting Master PIC offset to 0x20 (vector 32)");
    outb(PIC1_DATA, 0x20); // Master PIC IRQs (0-7) -> INT 0x20-0x27
    io_wait();
    log_debug("  Setting Slave PIC offset to 0x28 (vector 40)");
    outb(PIC2_DATA, 0x28); // Slave PIC IRQs (8-15) -> INT 0x28-0x2F
    io_wait();

    // Configure cascade (ICW3)
    log_debug("  Configuring Master PIC cascade (slave on IRQ2)");
    outb(PIC1_DATA, 0x04); // Tell Master PIC there is a Slave PIC at IRQ2 (0000 0100)
    io_wait();
    log_debug("  Configuring Slave PIC identity (cascade identity = 2)");
    outb(PIC2_DATA, 0x02); // Tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    // Set 8086 mode (ICW4)
    log_debug("  Setting 8086 mode for both PICs (0x%x)", ICW4_8086);
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks. This is important as some drivers might
     * have set specific IRQ masks before PIC remapping. */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
    log_debug("  Restored Master PIC mask: 0x%x, Slave PIC mask: 0x%x", mask1, mask2);

    log_info("8259 PICs initialized and remapped.");
}

/*
 * pic_send_eoi - Signal end-of-interrupt to the PIC(s).
 *
 * This MUST be called at the end of every hardware IRQ handler
 * to acknowledge the interrupt. Without EOI, the PIC will not
 * generate further interrupts on that line.
 *
 * Parameters:
 *   irq — The original IRQ line number (0-15).
 */
void pic_send_eoi(uint8_t irq)
{
    // If the IRQ is from the Slave PIC (IRQs 8-15), we must send EOI to it first.
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    // Always send EOI to the Master PIC, as it's involved in all IRQs.
    outb(PIC1_CMD, PIC_EOI);
}

/*
 * pic_mask_irq - Mask (disable) a single IRQ line.
 *
 * Parameters:
 *   irq — The original IRQ line number (0-15).
 */
void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8) { // IRQ belongs to Master PIC
        port = PIC1_DATA;
    } else {       // IRQ belongs to Slave PIC
        port = PIC2_DATA;
        irq -= 8; // Adjust IRQ number for Slave PIC's internal mask register
    }
    value = inb(port) | (1 << irq); // Set the bit corresponding to the IRQ
    outb(port, value);
    log_debug("  Masked IRQ%d. PIC mask for port 0x%x: 0x%x", irq, port, value);
}

/*
 * pic_unmask_irq - Unmask (enable) a single IRQ line.
 *
 * Parameters:
 *   irq — The original IRQ line number (0-15).
 */
void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8) { // IRQ belongs to Master PIC
        port = PIC1_DATA;
    } else {       // IRQ belongs to Slave PIC
        port = PIC2_DATA;
        irq -= 8; // Adjust IRQ number for Slave PIC's internal mask register
    }
    value = inb(port) & ~(1 << irq); // Clear the bit corresponding to the IRQ
    outb(port, value);
    log_debug("  Unmasked IRQ%d. PIC mask for port 0x%x: 0x%x", irq, port, value);
}

/*
 * pic_mask_all - Masks all IRQ lines on both PICs.
 * This effectively disables all hardware interrupts.
 */
void pic_mask_all() {
    outb(PIC1_DATA, 0xFF); // Mask all IRQs on Master PIC
    outb(PIC2_DATA, 0xFF); // Mask all IRQs on Slave PIC
    log_info("All IRQs on PICs masked.");
}

/*
 * pic_unmask_all - Unmasks all IRQ lines on both PICs.
 * This effectively enables all hardware interrupts.
 * Use with caution, as unexpected interrupts can occur.
 */
void pic_unmask_all() {
    outb(PIC1_DATA, 0x00); // Unmask all IRQs on Master PIC
    outb(PIC2_DATA, 0x00); // Unmask all IRQs on Slave PIC
    log_info("All IRQs on PICs unmasked (use with caution).");
}
