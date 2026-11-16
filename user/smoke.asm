; Minimal ByteBandit ring-3 workload.
; It exits through int 0x80 so the scheduler's syscall handoff is exercised.

bits 32

%define USER_BASE 0x08000000

org USER_BASE

ehdr:
    db 0x7F, "ELF"
    db 1, 1, 1, 0
    times 8 db 0
    dw 2
    dw 3
    dd 1
    dd USER_BASE + code - $$
    dd 52
    dd 0
    dd 0
    dw 52
    dw 32
    dw 1
    dw 0
    dw 0
    dw 0

phdr:
    dd 1
    dd 0
    dd USER_BASE
    dd USER_BASE
    dd file_end - $$
    dd file_end - $$
    dd 5
    dd 0x1000

code:
    mov eax, 1
    xor ebx, ebx
    int 0x80
    hlt
    jmp $

file_end:
