#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_init();
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);
void pic_mask_all();   // New: Masks all IRQs
void pic_unmask_all(); // New: Unmasks all IRQs

#endif