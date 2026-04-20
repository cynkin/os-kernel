#include "printf.h"
#include "../drivers/vga/vga.h"
#include <stdarg.h>
#include <stdint.h>

static void print_number(int num)
{
    char buf[32];
    int i = 0;
    unsigned int unum = (unsigned int)num;

    if (num == 0)
    {
        vga_put_char('0');
        return;
    }

    if (num < 0)
    {
        vga_put_char('-');
        unum = ~unum + 1;
    }

    while (unum > 0)
    {
        buf[i++] = '0' + (unum % 10);
        unum /= 10;
    }

    for (int j = i - 1; j >= 0; j--)
        vga_put_char(buf[j]);
}

static void print_hex(unsigned int num)
{
    char hex[] = "0123456789ABCDEF";
    int started = 0;

    for (int i = 28; i >= 0; i -= 4)
    {
        int digit = (num >> i) & 0xF;

        if (digit != 0 || started || i == 0)
        {
            started = 1;
            vga_put_char(hex[digit]);
        }
    }
}

static void print_ptr(uintptr_t ptr)
{
    vga_write("0x");

    char hex[] = "0123456789ABCDEF";
    int started = 0;

    for (int i = (sizeof(ptr) * 8) - 4; i >= 0; i -= 4)
    {
        int digit = (ptr >> i) & 0xF;

        if (digit != 0 || started || i == 0)
        {
            started = 1;
            vga_put_char(hex[digit]);
        }
    }
}

static int handle_ansi(const char **fmt)
{
    const char *p = *fmt + 1;

    if (*p != '[')
        return 0;

    p++;

    int code = 0;

    while (*p >= '0' && *p <= '9')
    {
        code = code * 10 + (*p - '0');
        p++;
    }

    if (*p != 'm')
        return 0;

    switch (code)
    {
    case 30:
        vga_set_color(0x00);
        break;
    case 31:
        vga_set_color(0x04);
        break;
    case 32:
        vga_set_color(0x02);
        break;
    case 33:
        vga_set_color(0x06);
        break;
    case 34:
        vga_set_color(0x01);
        break;
    case 35:
        vga_set_color(0x05);
        break;
    case 36:
        vga_set_color(0x03);
        break;
    case 37:
        vga_set_color(0x07);
        break;
    case 0:
        vga_set_color(0x0F);
        break;
    }

    *fmt = p;
    return 1;
}

void vprintf(const char *fmt, va_list args)
{
    while (*fmt)
    {
        if (*fmt == '\033')
        {
            if (handle_ansi(&fmt))
            {
                fmt++;
                continue;
            }
            vga_put_char(*fmt);
        }
        else if (*fmt == '%')
        {
            fmt++;

            switch (*fmt)
            {
            case 'd':
                print_number(va_arg(args, int));
                break;

            case 'x':
                print_hex(va_arg(args, unsigned int));
                break;

            case 'p':
                print_ptr((uintptr_t)va_arg(args, void *));
                break;

            case 's':
                vga_write(va_arg(args, char *));
                break;

            case 'c':
                vga_put_char((char)va_arg(args, int));
                break;

            default:
                vga_put_char(*fmt);
            }
        }
        else
        {
            vga_put_char(*fmt);
        }

        fmt++;
    }
}

void printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}