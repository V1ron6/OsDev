[bits 32]

extern isr_handler

%macro ISR_NOERR 1
    global isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
    global isr%1
isr%1:
    cli
    push dword %1
    jmp isr_common
%endmacro

isr_common:
    pushad
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popad

    add esp, 8
    iretd

ISR_NOERR 0
ISR_NOERR 6
ISR_ERR 13
