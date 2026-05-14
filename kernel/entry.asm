; Kernel entry point for ByteBandit OS

[bits 32]

global _start

extern kernel_main
extern __bss_start
extern __bss_end
extern __stack_top

_start:
    mov esp, __stack_top
    xor ebp, ebp

    ; Clear the .bss section for deterministic startup state.
    cld
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb

    call kernel_main

    cli
.hang:
    hlt
    jmp .hang
