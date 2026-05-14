/*
 * kernel/main.c - Kernel Entry Point
 *
 * Initializes core kernel subsystems in proper order:
 * 1. VGA terminal (output device)
 * 2. Serial port (debugging/logging)
 * 3. Interrupt descriptor table (interrupt handling)
 *
 * After initialization, kernel waits for interrupts or panics
 * on critical errors.
 */

#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "idt.h"
#include "pic.h"
#include "irq.h"
#include "x86.h"
#include "stdarg.h"

void kernel_main(void) {
    /* Stage 1: Initialize VGA for visual feedback */
    vga_init();
    vga_puts("=== ByteBandit OS Bootstrap ===\n\n");
    
    /* Stage 2: Initialize serial port for reliable debugging
     *
     * Serial output is essential for:
     * - Early boot diagnostics
     * - Debugging paging/interrupt setup
     * - Logging when VGA is unavailable
     */
    vga_puts("[1/5] Initializing serial port...\n");
    serial_init();
    serial_puts("[SERIAL] COM1 initialized at 115200 baud\n");
    vga_puts("        [OK] Serial port ready\n\n");
    
    /* Stage 3: Initialize interrupt system
     *
     * IDT must be set up before PIC, so we know where to install IRQ handlers.
     */
    vga_puts("[2/5] Initializing IDT and exception handlers...\n");
    idt_init();
    vga_puts("        [OK] IDT installed, 256 entries ready\n\n");
    
    /* Stage 4: Remap PIC to non-conflicting vectors
     *
     * PIC must be remapped before IRQ system can install handlers.
     * This moves hardware IRQs from vectors 8-15 (conflicts with exceptions)
     * to vectors 0x20-0x2F (standard x86 location).
     */
    vga_puts("[3/5] Initializing PIC (remapping interrupts)...\n");
    pic_init();
    vga_puts("        [OK] PIC remapped to vectors 0x20-0x2F\n\n");
    
    /* Stage 5: Initialize hardware IRQ handler infrastructure
     *
     * Installs IRQ stubs in IDT for all 16 hardware interrupts.
     * Does not enable any specific IRQs yet (drivers will do that).
     */
    vga_puts("[4/5] Initializing IRQ handler system...\n");
    irq_init();
    vga_puts("        [OK] 16 IRQ handlers ready\n\n");
    
    /* Bootstrap complete - display status */
    vga_puts("[5/5] Bootstrap complete\n\n");
    
    vga_puts("System Information:\n");
    vga_printf("  CR0: 0x%x (protected mode: %s)\n",
               read_cr0(), (read_cr0() & 0x1) ? "YES" : "NO");
    vga_puts("  GDT: installed\n");
    vga_puts("  IDT: 256 entries, 32 exceptions + 16 IRQs\n");
    vga_puts("  PIC: remapped (master 0x20-0x27, slave 0x28-0x2F)\n");
    vga_puts("  Serial: COM1 115200 baud\n");
    vga_puts("  VGA: 80x25 text mode\n\n");
    
    /* Test message to verify all outputs work */
    vga_puts(">>> System initialization test:\n");
    serial_puts("[KERNEL] Bootstrap sequence complete\n");
    serial_puts("[KERNEL] System is ready for hardware interrupts\n");
    serial_puts("[KERNEL] All IRQs currently masked (disabled by PIC)\n");
    vga_puts("    Messages sent to serial port\n\n");
    
    vga_puts("Ready for driver initialization and interrupt handling.\n");
    vga_puts("(Waiting indefinitely - CPU halted with interrupts disabled)\n\n");
    
    /* Hang indefinitely with interrupts disabled (for now)
     *
     * Once drivers are ready, they will:
     * 1. Call irq_install_handler() to register their handler
     * 2. Call irq_enable() to unmask their IRQ at PIC
     * 3. Call enable_interrupts() to allow CPU to receive them
     *
     * Without this, the system would be frozen since no device drivers
     * are running to provide input or generate events.
     */
    disable_interrupts();
    while (1) {
        halt();
    }
}
