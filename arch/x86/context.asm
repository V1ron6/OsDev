; arch/x86/context.asm - Context Switching Assembly
;
; Low-level CPU context save/restore for task switching.
;
; Context offsets:
;   0: EAX, 4: EBX, 8: ECX, 12: EDX
;   16: ESI, 20: EDI, 24: ESP, 28: EBP
;   32: EIP, 36: EFLAGS, 40: CR3

bits 32
section .text

global context_switch_asm
global context_save_asm
global context_restore_asm
global _context_switch_asm
global _context_save_asm
global _context_restore_asm

; void context_switch_asm(cpu_context_t *from, cpu_context_t *to)
;
; The interrupt-frame switch will be added when PIT preemption is wired.
context_switch_asm:
_context_switch_asm:
    ; The interrupt-frame switch will be added when PIT preemption is wired.
    ret

; void context_save_asm(cpu_context_t *ctx)
context_save_asm:
_context_save_asm:
    push edx
    mov edx, [esp + 8]      ; edx = ctx pointer
    
    ; Save general purpose registers.
    mov [edx + 0], eax
    mov [edx + 4], ebx
    mov [edx + 8], ecx
    mov ecx, [esp]
    mov [edx + 12], ecx
    mov [edx + 16], esi
    mov [edx + 20], edi
    
    ; Save frame and stack pointers.
    mov [edx + 28], ebp
    mov [edx + 24], esp
    
    ; Save EFLAGS and CR3.
    pushfd
    pop ecx
    mov [edx + 36], ecx
    mov eax, cr3
    mov [edx + 40], eax
    
    pop edx
    ret

; void context_restore_asm(cpu_context_t *ctx)
;
; EIP, ESP, and EFLAGS are restored by the interrupt return path.
context_restore_asm:
_context_restore_asm:
    mov edx, [esp + 4]      ; edx = ctx pointer
    
    ; Restore general purpose registers.
    mov ebx, [edx + 4]
    mov ecx, [edx + 8]
    mov eax, [edx + 0]
    mov esi, [edx + 16]
    mov edi, [edx + 20]
    
    ; Restore frame pointer and page directory.
    mov ebp, [edx + 28]
    mov ecx, [edx + 40]
    mov cr3, ecx
    
    ; Restore EDX last because it holds the context pointer.
    mov ecx, [edx + 12]
    mov eax, [edx + 0]
    mov edx, ecx

    ; ESP, EIP, and EFLAGS are restored by the interrupt return path.
    
    ret

