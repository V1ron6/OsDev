#include "panic.h"
#include "vga.h"
#include "x86.h"
#include "stdarg.h"

void kernel_panic(const char *message) {
    /* Disable interrupts - we're about to halt */
    disable_interrupts();
    
    /* Clear screen and print panic message */
    vga_clear(VGA_COLOR_RED);
    vga_puts("=== KERNEL PANIC ===\n\n");
    vga_puts(message);
    vga_puts("\n\nSystem halted.");
    
    /* Halt CPU */
    while (1) {
        halt();
    }
}

void panic_printf(const char *fmt, ...) {
    /* Disable interrupts */
    disable_interrupts();
    
    /* Clear screen */
    vga_clear(VGA_COLOR_RED);
    vga_puts("=== KERNEL PANIC ===\n\n");
    
    /* Print formatted message */
    va_list args;
    va_start(args, fmt);
    
    /* TODO: Implement proper vga_vprintf */
    vga_puts("(detailed panic info requires vprintf)\n");
    vga_puts(fmt);
    
    va_end(args);
    
    vga_puts("\n\nSystem halted.");
    
    /* Halt CPU */
    while (1) {
        halt();
    }
}
