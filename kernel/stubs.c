#include <stdint.h>

void log_debug(const char *msg) {}
void log_fatal(const char *msg) {}

void vga_clear_screen() {}
void vga_set_cursor(int x, int y) {}

void isr_handler(void *r) {}

int tfp_format(void *out, void (*putc)(char, void *), const char *fmt, ...)
{
    return 0;
}