#include "vga.h"
#include "../include/spinlock.h"

static volatile uint16_t *VGA_MEMORY = (uint16_t *)0xB8000;

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t color = 0x0F;

static spinlock_t vga_lock;

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
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
    if (cursor_row < VGA_HEIGHT)
        return;

    for (int y = 1; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', color);

    cursor_row = VGA_HEIGHT - 1;
}

void vga_set_color(uint8_t c)
{
    spinlock_acquire(&vga_lock);
    color = c;
    spinlock_release(&vga_lock);
}

void vga_clear()
{
    spinlock_acquire(&vga_lock);
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', color);

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
        vga_entry(' ', color);
}

static void handle_tab()
{
    cursor_col = (cursor_col + TAB_WIDTH) & ~(TAB_WIDTH - 1);

    if (cursor_col >= VGA_WIDTH)
    {
        cursor_col = 0;
        cursor_row++;
    }
}

static void vga_put_char_unlocked(char c)
{
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
            vga_entry(c, color);

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
}

void vga_put_char(char c)
{
    spinlock_acquire(&vga_lock);
    vga_put_char_unlocked(c);
    spinlock_release(&vga_lock);
}

void vga_write(const char *str)
{
    spinlock_acquire(&vga_lock);

    while (*str)
        vga_put_char_unlocked(*str++);

    spinlock_release(&vga_lock);
}

void vga_init()
{
    spinlock_init(&vga_lock);
    color = 0x0F;
    vga_clear();
}