/* ByteBandit OS Linker Script
 * 
 * Defines memory layout for the complete kernel image.
 * Structure:
 *   0x00000 - 0x7BFFF: (Reserved for BIOS/other)
 *   0x7C000 - 0x7DFFF: Bootloader (loaded by BIOS)
 *   0x10000 - 0x3FFFF: Kernel image (entry at 0x10000)
 *   0x90000+: Kernel stack, heap
 *
 * This linker script generates an ELF kernel that the bootloader
 * will load at 0x1000:0x0000 (linear address 0x10000).
 */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 0x10000 in physical memory */
    . = 0x10000;
    
    .text : {
        KEEP(*(.text._start))       /* _start must be first */
        KEEP(*(.text._start.*))
        *(.text .text.*)
        *(.rodata .rodata.*)
    } : text_seg
    
    .data ALIGN(0x1000) : {
        *(.data .data.*)
        *(.got.plt)
        *(.got)
    } : data_seg
    
    /* Uninitialized data - must be zeroed by kernel entry code */
    .bss ALIGN(0x1000) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } : data_seg
    
    /* Stack space: grows downward from 0x93000 */
    . = 0x8F000;
    .stack : {
        . += 0x4000;  /* 16 KB stack space */
        __kernel_stack_top = .;
    } : stack_seg
    
    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note)
        *(.note.*)
    }
}

PHDRS {
    text_seg PT_LOAD FLAGS(5); /* R+X */
    data_seg PT_LOAD FLAGS(6); /* R+W */
    stack_seg PT_LOAD FLAGS(6); /* R+W */
}
