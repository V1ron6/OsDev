/*
 * panic.c - Kernel Panic Handler
 *
 * Implements safe system shutdown when a fatal error occurs.
 * Disables interrupts, prints diagnostic information to both
 * VGA and serial port, then halts the CPU.
 *
 * Serial output is preferred during boot/paging/interrupt setup
 * when VGA may be unavailable or unreliable.
 */

#include "panic.h"
#include "vga.h"
#include "serial.h"
#include "x86.h"
#include "stdarg.h"

/* Kernel panic handler - static to prevent recursive calls */
void kernel_panic(const char *message) {
    /* Disable interrupts - we're about to halt permanently */
    disable_interrupts();
    
    /* Output to serial (most reliable) */
    serial_puts("\n\n");
    serial_puts("======================================\n");
    serial_puts("         KERNEL PANIC DETECTED        \n");
    serial_puts("======================================\n\n");
    serial_puts("Message: ");
    serial_puts(message);
    serial_puts("\n\nSystem halted.\n");
    
    /* Attempt VGA output (may fail if we're in middle of video setup) */
    vga_clear(VGA_COLOR_RED);
    vga_puts("=== KERNEL PANIC ===\n\n");
    vga_puts(message);
    vga_puts("\n\nSee serial output for details.\nSystem halted.");
    
    /* Halt CPU - prevent further execution */
    while (1) {
        halt();
    }
}

/* Formatted panic output supporting basic format specifiers */
void panic_printf(const char *fmt, ...) {
    /* Disable interrupts */
    disable_interrupts();
    
    /* Output to serial first (more reliable) */
    serial_puts("\n\n");
    serial_puts("======================================\n");
    serial_puts("         KERNEL PANIC DETECTED        \n");
    serial_puts("======================================\n\n");
    
    /* Print formatted message via serial */
    va_list args;
    va_start(args, fmt);
    serial_printf(fmt, args);
    va_end(args);
    
    serial_puts("\n\nSystem halted.\n");
    
    /* Attempt VGA output (may be unavailable) */
    vga_clear(VGA_COLOR_RED);
    vga_puts("=== KERNEL PANIC ===\n\n");
    vga_puts("See serial output for details.\n");
    vga_puts("System halted.");
    
    /* Halt CPU */
    while (1) {
        halt();
    }
}
