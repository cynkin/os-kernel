[bits 32]

MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0x00000003
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top      ; SET UP THE STACK — this was missing!
    mov ebp, esp

    push ebx                ; Multiboot info pointer (GRUB puts in EBX)
    push eax                ; Multiboot magic (GRUB puts in EAX)

    call kernel_main

    cli
.hang:
    hlt
    jmp .hang