ENTRY(_start)

SECTIONS
{
    . = 0x00010000;

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
        . = ALIGN(16);
        __stack_bottom = .;
        . = . + 16384;
        __stack_top = .;
    }
}
