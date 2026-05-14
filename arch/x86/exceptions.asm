; exceptions.asm - CPU Exception Handlers
;
; Implements ISR (Interrupt Service Routine) stubs for CPU exceptions.
; 
; ISR Stack Frame:
; When CPU enters an interrupt/exception handler, it creates a stack frame:
;   [ESP+12]: EFLAGS (flags)
;   [ESP+8]:  CS (code segment)
;   [ESP+4]:  EIP (return address)
;   [ESP+0]:  Error Code (or 0 if not provided by CPU)
;
; For exceptions that don't push error codes (0, 1, 3, 4, 5, 6, 7, 9, 11, 13, 14, 16, 19, 20),
; the ISR stub must push a dummy error code (0) to maintain consistent stack frame.
;
; Handler signature (C):
;   void exception_handler(uint32_t int_no, uint32_t err_code);
;
; Our ISR stubs conform to this by:
; 1. Pushing the interrupt number
; 2. Calling the common exception handler
; 3. The handler reads error code from stack
;

bits 32
section .text

; External: common exception handler in C
extern exception_handler

; _isr_stub - Default stub for unhandled interrupts
; Used as a placeholder; causes panic when triggered
global _isr_stub
_isr_stub:
    cli                 ; Disable further interrupts
    mov eax, 0xFFFFFFFF ; Invalid vector marker
    jmp exception_handler

; Macro to create exception handler stubs
; For exceptions WITH error code: STUB_WITH_CODE
; For exceptions WITHOUT error code: STUB_NO_CODE

; ISR for exceptions that provide error code
; Error code is already on stack, just push vector number
%macro STUB_WITH_CODE 1
  global _isr%1
  _isr%1:
    push dword %1  ; Push interrupt number
    jmp _exception_common
%endmacro

; ISR for exceptions that don't provide error code
; Must push dummy error code (0) first to align stack frame
%macro STUB_NO_CODE 1
  global _isr%1
  _isr%1:
    push dword 0   ; Dummy error code (stack alignment)
    push dword %1  ; Push interrupt number
    jmp _exception_common
%endmacro

; Create exception handlers (0-31)
; Only these are CPU exceptions; 32+ are software/hardware IRQs

; 0: Divide by Zero (no error code)
STUB_NO_CODE 0

; 1: Debug (no error code)
STUB_NO_CODE 1

; 2: NMI (no error code)
STUB_NO_CODE 2

; 3: Breakpoint (no error code)
STUB_NO_CODE 3

; 4: Overflow (no error code)
STUB_NO_CODE 4

; 5: Bound Range Exceeded (no error code)
STUB_NO_CODE 5

; 6: Invalid Opcode (no error code)
STUB_NO_CODE 6

; 7: Device Not Available (no error code)
STUB_NO_CODE 7

; 8: Double Fault (WITH error code)
STUB_WITH_CODE 8

; 9: Coprocessor Segment Overrun (no error code, obsolete)
STUB_NO_CODE 9

; 10: Invalid TSS (WITH error code)
STUB_WITH_CODE 10

; 11: Segment Not Present (WITH error code)
STUB_WITH_CODE 11

; 12: Stack Fault (WITH error code)
STUB_WITH_CODE 12

; 13: General Protection Fault (WITH error code)
STUB_WITH_CODE 13

; 14: Page Fault (WITH error code)
STUB_WITH_CODE 14

; 15: Reserved (no error code)
STUB_NO_CODE 15

; 16: FPU Floating Point Error (no error code)
STUB_NO_CODE 16

; 17: Alignment Check (WITH error code)
STUB_WITH_CODE 17

; 18: Machine Check (no error code)
STUB_NO_CODE 18

; 19: SIMD Floating Point (no error code)
STUB_NO_CODE 19

; 20: Virtualization Exception (no error code)
STUB_NO_CODE 20

; 21-31: Reserved (no error codes)
%assign i 21
%rep 11
  STUB_NO_CODE i
  %assign i i+1
%endrep

; Common exception handler
; At this point, stack looks like:
;   [ESP+4]: Error code (or dummy 0)
;   [ESP+0]: Interrupt number
;
; Call C handler with these two arguments
_exception_common:
    ; Preserve CPU state for debugging
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
    
    ; Calculate offset to arguments on stack
    ; Stack layout after pushes:
    ;   [ESP+48]: EFLAGS (from CPU)
    ;   [ESP+44]: CS (from CPU)
    ;   [ESP+40]: EIP (from CPU)
    ;   [ESP+36]: Error code (we pushed)
    ;   [ESP+32]: Interrupt number (we pushed)
    ;   [ESP+28-0]: Registers (we just pushed)
    
    mov eax, [esp + 32]  ; Interrupt number (first arg)
    mov edx, [esp + 36]  ; Error code (second arg)
    
    ; Call handler(int_no, err_code)
    push edx
    push eax
    call exception_handler
    add esp, 8
    
    ; Return from exception (normally won't reach here as handler panics)
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
    
    ; Remove error code and interrupt number from stack
    add esp, 8
    
    ; Return from interrupt
    iret
