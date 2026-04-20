#include "pcb.h"
#include "kmalloc.h"
#include "lib/memory.h"

#define PROCESS_STACK_SIZE 4096  /* 4 KB stack per process */

static uint32_t next_pid = 1;

pcb_t* create_process(uint32_t entry_point) {

    pcb_t* new_pcb = (pcb_t*)kmalloc(sizeof(pcb_t));
    
    if (new_pcb == NULL) return NULL;

    /* Zero the entire PCB */
    memset(new_pcb, 0, sizeof(pcb_t));

    new_pcb->pid = next_pid++;
    new_pcb->state = READY;
    
    /* Allocate a real kernel stack for this process */
    uint8_t* stack = (uint8_t*)kmalloc(PROCESS_STACK_SIZE);
    if (stack == NULL) {
        kfree(new_pcb);
        return NULL;
    }

    /* Stack grows downward on x86: ESP starts at the TOP */
    new_pcb->esp = (uint32_t)(stack + PROCESS_STACK_SIZE);
    new_pcb->ebp = new_pcb->esp;

    /* Set the instruction pointer so the CPU knows where to start */
    new_pcb->eip = entry_point;
    
    new_pcb->eflags = 0x202; /* Standard "interrupts enabled" flag for x86 */
    
    new_pcb->next = NULL;
    
    return new_pcb;
}