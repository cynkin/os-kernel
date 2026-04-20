; isr_stubs.asm
; ─────────────────────────────────────────────────────────────────────
; Assembly ISR entry points placed into the IDT.
;
; FLOW for every interrupt:
;   CPU detects interrupt
;     → looks up IDT[vector]
;     → jumps to our isr_stub (this file)
;     → stub builds interrupt_frame_t on stack
;     → jumps to isr_common_stub
;     → isr_common_stub calls isr_common_handler() in C
;     → returns here, restores state, iret
; ─────────────────────────────────────────────────────────────────────

[bits 32]
extern isr_common_handler   ; our C dispatcher in idt.c

; ─────────────────────────────────────────────────────────────────────
; isr_common_stub
; ─────────────────────────────────────────────────────────────────────
; Called by every individual stub after it pushes int_no (and possibly
; err_code). Builds the complete interrupt_frame_t on the stack, calls
; the C handler, then unwinds everything and returns with iret.
;
; Stack at entry to isr_common_stub (growing downward):
;
;   [previously pushed by CPU on interrupt:]
;   ss          (only if privilege change)
;   useresp     (only if privilege change)
;   eflags
;   cs
;   eip
;   [pushed by individual stub or CPU:]
;   err_code
;   int_no
;   [we push below:]
;   edi,esi,ebp,esp,ebx,edx,ecx,eax   ← pusha
;   ds                                 ← pushed manually
;   ← ESP now points here → this is interrupt_frame_t *
; ─────────────────────────────────────────────────────────────────────
isr_common_stub:
    pusha                   ; Push EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI (8 regs × 4B)

    mov  eax, ds             ; Save current data segment selector
    push eax                ; (push as 32-bit for alignment)

    mov  ax, 0x10           ; Switch DS/ES/FS/GS to kernel data segment
    mov  ds, ax             ; so C code can safely access kernel memory
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    push esp                ; Push ESP as argument — points to interrupt_frame_t
    call isr_common_handler ; Call our C dispatcher
    add  esp, 4             ; Pop the ESP argument (cdecl caller cleanup)

    pop  eax                ; Restore original data segment selector
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    popa                    ; Restore EDI,ESI,EBP,ESP,EBX,EDX,ECX,EAX

    add  esp, 8             ; Skip over err_code and int_no we pushed earlier

    iret                    ; Return from interrupt:
                            ;   pop EIP, CS, EFLAGS (and SS, ESP if privilege change)

; ─────────────────────────────────────────────────────────────────────
; MACROS for individual ISR stubs
; ─────────────────────────────────────────────────────────────────────

; ISR_NO_ERR: CPU does NOT push an error code for this vector.
; We push dummy 0 so interrupt_frame_t.err_code is always valid.
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    push dword 0    ; dummy error code — keeps frame layout uniform
    push dword %1   ; interrupt vector number
    jmp  isr_common_stub
%endmacro

; ISR_ERR: CPU DOES push an error code automatically before jumping here.
; We only push the vector number (error code is already on stack).
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1   ; interrupt vector number (err_code already on stack below)
    jmp  isr_common_stub
%endmacro

; IRQ: Hardware interrupt — no CPU error code, push dummy 0.
; First arg = IRQ line number (for the label), second = vector number.
%macro IRQ 2
global irq%1
irq%1:
    push dword 0    ; dummy error code
    push dword %2   ; vector number (32 + IRQ line)
    jmp  isr_common_stub
%endmacro

; ─────────────────────────────────────────────────────────────────────
; CPU EXCEPTION STUBS — Vectors 0–21
; Sourced from Intel SDM Vol.3 Table 6-1.
; Exceptions marked (*) have CPU-pushed error codes.
; ─────────────────────────────────────────────────────────────────────
ISR_NO_ERR  0    ; #DE  Divide-by-Zero
ISR_NO_ERR  1    ; #DB  Debug
ISR_NO_ERR  2    ;      Non-Maskable Interrupt
ISR_NO_ERR  3    ; #BP  Breakpoint
ISR_NO_ERR  4    ; #OF  Overflow
ISR_NO_ERR  5    ; #BR  Bound Range Exceeded
ISR_NO_ERR  6    ; #UD  Invalid Opcode
ISR_NO_ERR  7    ; #NM  Device Not Available
ISR_ERR     8    ; #DF  Double Fault             (*)  error code always 0
ISR_NO_ERR  9    ;      Coprocessor Overrun      (legacy, never fires on modern CPUs)
ISR_ERR    10    ; #TS  Invalid TSS              (*)
ISR_ERR    11    ; #NP  Segment Not Present      (*)
ISR_ERR    12    ; #SS  Stack-Segment Fault      (*)
ISR_ERR    13    ; #GP  General Protection Fault (*)  most common kernel fault
ISR_ERR    14    ; #PF  Page Fault               (*)  error code = access flags
ISR_NO_ERR 15    ;      Reserved
ISR_NO_ERR 16    ; #MF  x87 Floating-Point Exception
ISR_ERR    17    ; #AC  Alignment Check          (*)
ISR_NO_ERR 18    ; #MC  Machine Check
ISR_NO_ERR 19    ; #XM  SIMD Floating-Point Exception
ISR_NO_ERR 20    ; #VE  Virtualization Exception
ISR_NO_ERR 21    ;      Reserved

; ─────────────────────────────────────────────────────────────────────
; HARDWARE IRQ STUBS — Vectors 32–47
; Requires PIC to be remapped: IRQ0–7 → 32–39, IRQ8–15 → 40–47
; ─────────────────────────────────────────────────────────────────────
IRQ  0, 32    ; PIT Timer
IRQ  1, 33    ; PS/2 Keyboard
IRQ  2, 34    ; PIC2 Cascade (not a real device)
IRQ  3, 35    ; COM2 Serial Port
IRQ  4, 36    ; COM1 Serial Port
IRQ  5, 37    ; LPT2 Parallel Port
IRQ  6, 38    ; Floppy Disk Controller
IRQ  7, 39    ; LPT1 / Spurious IRQ
IRQ  8, 40    ; CMOS Real-Time Clock
IRQ  9, 41    ; Free (ACPI / legacy redirected IRQ2)
IRQ 10, 42    ; Free (NIC, USB, etc.)
IRQ 11, 43    ; Free (NIC, USB, etc.)
IRQ 12, 44    ; PS/2 Mouse
IRQ 13, 45    ; FPU / Math Coprocessor
IRQ 14, 46    ; Primary ATA/IDE Disk
IRQ 15, 47    ; Secondary ATA/IDE Disk