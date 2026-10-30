; irq.asm - Hardware IRQ Handlers
;
; Implements ISR stubs for hardware interrupts (vectors 0x20-0x2F).
; These vectors are configured by the PIC during initialization.
;
; IRQ vector mapping (after PIC remapping):
;   Master PIC (IRQs 0-7):  vectors 0x20-0x27
;   Slave PIC (IRQs 8-15):  vectors 0x28-0x2F
;
; Each IRQ gets its own stub that:
; 1. Pushes the IRQ number
; 2. Calls the common IRQ handler
;
; The common handler:
; 1. Preserves CPU state
; 2. Calls registered C handler (if any), with the saved frame
; 3. Sends EOI to PIC
; 4. Restores CPU state
; 5. Returns via IRET
;

bits 32
section .text

; External: common IRQ handler in C
extern irq_handler

; Macro to create IRQ handler stub
; Each stub pushes IRQ number and jumps to common handler
%macro IRQ_STUB 1
  global _irq%1
  _irq%1:
    push dword 0        ; Dummy error code (for stack alignment with exceptions)
    push dword %1       ; Push IRQ number
    jmp _irq_common
%endmacro

; Generate stubs for all 16 hardware IRQs
%assign i 0
%rep 16
  IRQ_STUB i
  %assign i i+1
%endrep

; Common IRQ handler
; Stack layout when entering:
;   [ESP+36]: EFLAGS (from CPU)
;   [ESP+32]: CS (from CPU)
;   [ESP+28]: EIP (return address, from CPU)
;   [ESP+24]: Dummy error code (we pushed 0)
;   [ESP+20]: IRQ number (we pushed)
;   [ESP+16-0]: Saved registers (we'll push)
_irq_common:
    ; Preserve all registers
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    push ds
    push es
    push fs
    push gs
    
    ; Get IRQ number from stack (above eleven saved registers).
    mov eax, [esp + 44]
    
    ; Call C handler(irq_number, frame)
    push esp
    push eax
    call irq_handler
    add esp, 8

    ; A frame handler may return the next task context.  A non-zero result
    ; switches to that context and returns through a frame on its stack.
    test eax, eax
    jz .return_current

    mov edi, eax
    mov ecx, [edi + 40]       ; Load the next address space first.
    test ecx, ecx
    jz .keep_cr3
    mov cr3, ecx
  .keep_cr3:
    mov eax, [edi + 32]       ; EIP
    mov ecx, [edi + 36]       ; EFLAGS
    mov edx, [esp + 56]       ; Current CS (kernel tasks use the same CS).
    mov ebp, [edi + 24]       ; Next task's saved ESP.
    sub ebp, 12
    mov [ebp], eax
    mov [ebp + 4], edx
    mov [ebp + 8], ecx
    mov [esp + 64], ebp       ; Keep the target ESP in the unused kernel slot.
    mov edx, [edi + 20]
    mov [esp + 68], edx       ; Keep target EDI until the context pointer is free.

    mov eax, [edi + 0]
    mov ebx, [edi + 4]
    mov ecx, [edi + 8]
    mov edx, [edi + 12]
    mov ebp, [edi + 28]
    mov esi, [edi + 16]
    mov edi, [esp + 68]
    mov esp, [esp + 64]
    iret

  .return_current:
    
    ; Restore all registers
    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    
    ; Remove error code and IRQ number from stack
    add esp, 8
    
    ; Return from interrupt
    iret
