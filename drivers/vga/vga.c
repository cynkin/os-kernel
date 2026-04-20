#include "vga.h"
#include "../../include/spinlock.h"
#include "../../lib/printf.h" // For vga_printf implementation
#include "stdlib.h"
static volatile uint16_t *VGA_MEMORY = (uint16_t *)0xB8000;

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color_byte = 0x0F; // Stores combined foreground and background color

static spinlock_t vga_lock;

static inline uint16_t vga_entry(char c, uint8_t color_byte)
{
    return (uint16_t)c | ((uint16_t)color_byte << 8);
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor()
{
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;

    outb(0x3D4, 14);
    outb(0x3D5, pos >> 8);

    outb(0x3D4, 15);
    outb(0x3D5, pos);
}

static void scroll()
{
    // Move all rows up by one
    for (int y = 1; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    // Clear the last row
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', current_color_byte);

    cursor_row = VGA_HEIGHT - 1; // Keep cursor on the last row
}

// Sets the foreground and background colors
void vga_set_colors(enum vga_color fg, enum vga_color bg)
{
    spinlock_acquire(&vga_lock);
    current_color_byte = fg | (bg << 4);
    spinlock_release(&vga_lock);
}

// Deprecated: Use vga_set_colors for explicit foreground/background control
// This function now just sets the foreground color, keeping the background as is.
void vga_set_color(uint8_t c)
{
    spinlock_acquire(&vga_lock);
    // Extract current background color and combine with new foreground
    current_color_byte = (c & 0x0F) | (current_color_byte & 0xF0);
    spinlock_release(&vga_lock);
}

void vga_clear()
{
    spinlock_acquire(&vga_lock);
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', current_color_byte);

    cursor_row = 0;
    cursor_col = 0;

    update_cursor();
    spinlock_release(&vga_lock);
}

static void handle_backspace()
{
    if (cursor_col > 0)
    {
        cursor_col--;
    }
    else if (cursor_row > 0)
    {
        cursor_row--;
        cursor_col = VGA_WIDTH - 1;
    }

    VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
        vga_entry(' ', current_color_byte);
}

static void handle_tab()
{
    // Align to next tab stop
    cursor_col = (cursor_col + TAB_WIDTH) & ~(TAB_WIDTH - 1);

    if (cursor_col >= VGA_WIDTH)
    {
        cursor_col = 0;
        cursor_row++;
    }
}

void vga_put_char(char c)
{
    spinlock_acquire(&vga_lock);
    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\b')
    {
        handle_backspace();
    }
    else if (c == '\t')
    {
        handle_tab();
    }
    else if (c == '\r')
    {
        cursor_col = 0;
    }
    else
    {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            vga_entry(c, current_color_byte);

        cursor_col++;

        if (cursor_col >= VGA_WIDTH)
        {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_HEIGHT)
        scroll();

    update_cursor();
    spinlock_release(&vga_lock);
}

// Refactored to call vga_put_char for each character
void vga_write(const char *str)
{
    spinlock_acquire(&vga_lock); // Acquire lock once for the entire string write
    while (*str)
    {
        // No need to call spinlock_acquire/release inside vga_put_char
        // if vga_put_char itself is thread-safe (which it will be via the lock)
        // For now, I'll directly implement the logic to avoid nested locks,
        // as vga_put_char currently acquires its own lock.
        // A better design would be for vga_put_char to assume the lock is held.

        // Reimplementing logic from vga_put_char here temporarily until refactor
        // allows vga_put_char to not acquire lock internally.
        if (*str == '\n')
        {
            cursor_col = 0;
            cursor_row++;
        }
        else if (*str == '\b')
        {
            handle_backspace();
        }
        else if (*str == '\t')
        {
            handle_tab();
        }
        else if (*str == '\r')
        {
            cursor_col = 0;
        }
        else
        {
            VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
                vga_entry(*str, current_color_byte);

            cursor_col++;

            if (cursor_col >= VGA_WIDTH)
            {
                cursor_col = 0;
                cursor_row++;
            }
        }

        if (cursor_row >= VGA_HEIGHT)
            scroll();

        str++;
    }
    update_cursor(); // Update cursor once after writing the entire string
    spinlock_release(&vga_lock);
}

// Implement vga_printf using the printf backend and vga_put_char
static int vga_printf_putc(char c, void *data)
{
    (void)data;      // Unused
    vga_put_char(c); // vga_put_char handles its own locking
    return 1;
}

void vga_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    tfp_format(NULL, vga_printf_putc, format, args);
    va_end(args);
}

void vga_init()
{
    spinlock_init(&vga_lock);
    current_color_byte = VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4); // Default to white on black
    vga_clear();
}