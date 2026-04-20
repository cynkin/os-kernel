global context_switch
context_switch:
    mov eax, [esp + 4]

    mov ebx, [eax + 12]
    mov ecx, [eax + 16]
    mov edx, [eax + 20]

    push dword [eax + 32]
    ret