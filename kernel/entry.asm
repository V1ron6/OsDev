; ByteBandit OS Kernel Entry Point
; Execution starts here after bootloader transfers control
; Assumptions:
;   - CPU in protected mode
;   - Paging disabled
;   - Interrupts disabled
;   - Stack set up at 0x90000
;   - GDT loaded and valid
;
; Responsibilities:
;   1. Clear .bss section (uninitialized data)
;   2. Set up kernel stack frame
;   3. Call kernel_main() 
;   4. Halt if kernel_main returns

[BITS 32]
[GLOBAL _start]

; ===== EXTERN SYMBOLS =====
; Provided by linker script
extern __bss_start
extern __bss_end
extern __kernel_stack_top
extern kernel_main

; Put _start in its own section that will be placed first
SECTION .text._start

_start:
    ; ===== CLEAR .bss SECTION =====
    ; .bss contains zero-initialized global/static variables
    ; We must zero it manually in freestanding environment
    
    lea ecx, [__bss_end]                ; End address
    lea eax, [__bss_start]              ; Start address
    sub ecx, eax                        ; Calculate size
    shr ecx, 2                          ; Convert to dwords
    
    mov edi, __bss_start                ; Destination
    xor eax, eax                        ; Value: 0
    rep stosd                           ; Clear memory
    
    ; ===== INITIALIZE STACK =====
    ; Stack is already set up by bootloader, but we ensure alignment
    ; x86 stack must be 16-byte aligned for safety with SSE/ABI compliance
    
    mov esp, __kernel_stack_top         ; Set to stack top (grows downward)
    and esp, 0xFFFFFFF0                 ; Align to 16 bytes
    
    ; ===== CALL KERNEL MAIN =====
    ; Standard x86 calling convention: arguments on stack, return in EAX
    ; kernel_main(void) takes no arguments
    
    call kernel_main
    
    ; ===== HALT IF KERNEL_MAIN RETURNS =====
    ; kernel_main should never return in a real kernel
    ; If it does, something is very wrong
    
    cli                                 ; Disable interrupts
    hlt                                 ; Halt CPU
    
    ; Infinite loop in case hlt doesn't work
.halt:
    jmp .halt

