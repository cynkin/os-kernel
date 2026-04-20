#include "log.h"
#include "printf.h"
#include "time.h"
#include <stdarg.h>
#include <stdint.h>
#include "../lib/printf.h"

static void print_timestamp()
{
    uint64_t t = time_get_ticks();

    printf("[");
    printf("%d", (int)t);
    printf("] ");
}

void log_info(const char *fmt, ...)
{
    va_list args;

    print_timestamp();
    printf("\033[32m[INFO] ");

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\033[0m\n");
}

void log_warn(const char *fmt, ...)
{
    va_list args;

    print_timestamp();
    printf("\033[33m[WARN] ");

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\033[0m\n");
}

void log_error(const char *fmt, ...)
{
    va_list args;

    print_timestamp();
    printf("\033[31m[ERROR] ");

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\033[0m\n");
}