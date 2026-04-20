#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list args);

#define printk printf

#endif