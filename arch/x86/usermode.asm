; arch/x86/usermode.asm - Ring 3 User Mode Transition
;
; Implements the low-level IRET instruction for transitioning from ring 0 to ring 3.
; This code is called from enter_user_mode() in usermode.c.

[BITS 32]

; Selectors from GDT (defined in arch/x86/gdt.h)
USER_CODE_SEL equ 0x1B    ; GDT index 3, TI=0, RPL=3
USER_DATA_SEL equ 0x23    ; GDT index 4, TI=0, RPL=3

; Assembly implementation for IRET transition
global _usermode_iret

; Parameters (x86 cdecl calling convention):
;   [esp+4]  = entry_point (user code address)
;   [esp+8]  = user_stack (top of user stack)
;   [esp+12] = argc
;   [esp+16] = argv

_usermode_iret:
    mov ebp, esp
    
    ; Save parameters in registers that won't be overwritten
    mov eax, [ebp+4]    ; entry_point -> EAX (will be EIP)
    mov ebx, [ebp+8]    ; user_stack -> EBX (will become ESP)
    mov ecx, [ebp+12]   ; argc -> ECX
    mov edx, [ebp+16]   ; argv -> EDX
    
    ; Build IRET frame on current kernel stack
    ; IRET pops in this order: EIP, CS, EFLAGS, ESP, SS
    ; So we push in reverse: SS, ESP, EFLAGS, CS, EIP
    
    ; Make space for IRET frame (5 dwords = 20 bytes)
    sub esp, 20
    
    ; Fill IRET frame (now [esp] points to EIP field)
    mov [esp+0], eax            ; EIP = entry_point
    mov [esp+4], dword USER_CODE_SEL  ; CS = user code selector
    
    ; EFLAGS with IF bit set (interrupts enabled)
    pushfd
    pop dword [esp+8]
    or dword [esp+8], 0x200      ; Set IF bit (bit 9)
    
    mov [esp+12], ebx           ; ESP = user_stack
    mov [esp+16], dword USER_DATA_SEL  ; SS = user data selector
    
    ; Set up user data registers
    mov ax, USER_DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up user argument registers (before final register setup)
    mov eax, ecx                ; EAX = argc
    mov ebx, edx                ; EBX = argv
    
    ; Clear other registers
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    
    ; Execute IRET to switch to ring 3
    ; This is the point of no return - control transfers to user code
    iret

