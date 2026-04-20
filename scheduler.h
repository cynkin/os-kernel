#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"

void add_to_ready_queue(pcb_t* pcb);
void schedule(void);
pcb_t* get_current_process(void);

#endif
