#include "kernel/console.h"
#include "arch/x86/idt.h"

void kernel_main(void)
{
    console_init();
    console_write("ByteBandit OS: kernel_main()\n");

    idt_init();
    console_write("IDT initialized.\n");

    console_write("Kernel idle.\n");

    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
