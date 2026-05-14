#include "kernel.h"
#include "vga.h"
#include "x86.h"
#include "stdarg.h"

void kernel_main(void) {
    /* Initialize debugging first */
    vga_init();
    
    vga_puts("ByteBandit OS v0.1.0\n");
    vga_puts("Kernel started successfully.\n");
    vga_printf("CPU in protected mode: 0x%x\n", read_cr0());
    
    vga_puts("\nBootstrap sequence:\n");
    vga_puts("  [OK] Real mode to protected mode transition\n");
    vga_puts("  [OK] Kernel loaded at 0x10000\n");
    vga_puts("  [OK] Stack initialized\n");
    vga_puts("  [OK] .bss cleared\n");
    vga_puts("  [OK] VGA terminal active\n");
    vga_puts("\nNextsteps:\n");
    vga_puts("  - Implement GDT directly\n");
    vga_puts("  - Set up IDT infrastructure\n");
    vga_puts("  - CPU exception handlers\n");
    
    /* Test panic (commented out for now) */
    /* kernel_panic("Test panic message"); */
    
    /* Hang indefinitely */
    vga_puts("\n[Kernel halting - waiting for interrupts]\n");
    disable_interrupts();
    while (1) {
        halt();
    }
}
