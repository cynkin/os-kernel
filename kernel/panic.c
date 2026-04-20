#include "drivers/vga/vga.h"
#include "lib/printf.h"

void panic(const char *msg)
{
    /* Disable interrupts immediately */
    __asm__ volatile("cli");

    /* Switch to bright red on black for maximum visibility */
    vga_set_color(0x4F); /* Red background, white text */

    printf("\n\n");
    printf("*** KERNEL PANIC ***\n");
    printf("%s\n", msg);
    printf("System halted.\n");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}