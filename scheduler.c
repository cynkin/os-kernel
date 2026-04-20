#include "scheduler.h"

static pcb_t* current_process = NULL;
static pcb_t* ready_queue_head = NULL;

pcb_t* get_current_process(void) {
    return current_process;
}

void add_to_ready_queue(pcb_t* pcb) {
    if (ready_queue_head == NULL) {
        ready_queue_head = pcb;
        pcb->next = pcb; // Circular linked list: point to itself
    } else {
        // Find the end of the list and insert
        pcb_t* temp = ready_queue_head;
        while (temp->next != ready_queue_head) {
            temp = temp->next;
        }
        temp->next = pcb;
        pcb->next = ready_queue_head; // Complete the circle
    }
}

void schedule(void) {
    if (current_process == NULL) {
        current_process = ready_queue_head;
    } else {
        // Round Robin: Just move to the next PCB in the circle
        current_process->state = READY;
        current_process = current_process->next;
    }

    if (current_process != NULL) {
        current_process->state = RUNNING;
        
        // This is the Assembly function 
        // that actually swaps the hardware registers
        context_switch(current_process);
    }
}