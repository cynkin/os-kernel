; idt_flush.asm
; ─────────────────────────────────────────────────────────────────────
; Loads our IDT pointer into the IDTR hardware register,
; then enables hardware interrupts with 'sti'.
;
; CALLING CONVENTION (cdecl):
;   void idt_flush(uint32_t idt_ptr_address);
;   Argument on stack at [esp+4].
; ─────────────────────────────────────────────────────────────────────

[bits 32]
global idt_flush

idt_flush:
    mov  eax, [esp+4]   ; EAX = address of idt_ptr_t
    lidt [eax]          ; Load 6 bytes into IDTR:
                        ;   IDTR.limit ← idt_ptr.limit
                        ;   IDTR.base  ← idt_ptr.base
    ret