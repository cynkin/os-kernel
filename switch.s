# This function is called by your C scheduler
# void context_switch(pcb_t* next_process)

.global context_switch
.intel_syntax noprefix
context_switch:
    # 1. Get the pointer to the next PCB from the stack
    mov eax, [esp + 4] 

    # 2. Load the saved registers from that PCB into the CPU
    #    pcb_t offsets: pid=0, state=4, eax=8, ebx=12, ecx=16, edx=20
    #                   esi=24, edi=28, esp=32, ebp=36, eip=40, eflags=44
    mov ebx, [eax + 12] # EBX
    mov ecx, [eax + 16] # ECX
    mov edx, [eax + 20] # EDX
    mov esi, [eax + 24] # ESI
    mov edi, [eax + 28] # EDI
    mov esp, [eax + 32] # ESP - restore the process's stack!
    mov ebp, [eax + 36] # EBP - restore the process's base pointer!

    # 3. The Big Jump: Load the Instruction Pointer
    push dword ptr [eax + 40] # Push the saved EIP
    ret                       # "Return" to the new process's code!
