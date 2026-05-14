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
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/heap.h"

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
    
        /* Stage 5: Initialize physical memory manager
         *
         * PMM tracks which 4KB frames are allocated/free.
         * Must be initialized before paging (which allocates page tables).
         * Reserves kernel, bootloader, VRAM, and BIOS regions.
         */
        vga_puts("[4/6] Initializing physical memory manager...\n");
        pmm_init();
        serial_printf("[PMM] Free frames: %u (%.1f MB available)\n",
                      pmm_get_free_frames(), 
                      (pmm_get_free_frames() * 4096) / (1024.0 * 1024.0));
        vga_puts("        [OK] PMM ready\n\n");
    
        /* Stage 6: Initialize paging infrastructure
         *
         * Sets up page tables for identity mapping (virt == phys).
         * After init, paging structures are ready but not yet enabled.
         * Pages are allocated from PMM for page tables.
         */
        vga_puts("[5/6] Initializing paging infrastructure...\n");
        paging_init();
        vga_puts("        [OK] Paging structures ready\n");
        vga_puts("[6/6] Enabling paging...\n");
        paging_enable();
        vga_puts("        [OK] Virtual memory active\n\n");
    
        /* Stage 7: Initialize kernel heap allocator
         *
         * After paging is enabled, we can allocate dynamic memory.
         * Heap allocator uses PMM to get frames for heap expansion.
         */
        vga_puts("[7/7] Initializing kernel heap...\n");
        heap_init();
        vga_puts("        [OK] Heap ready\n\n");
    
    /* Bootstrap complete - display status */
    vga_puts("Bootstrap complete\n\n");
    
    vga_puts("System Information:\n");
    vga_printf("  CR0: 0x%x (protected mode: %s)\n",
               read_cr0(), (read_cr0() & 0x1) ? "YES" : "NO");
    vga_puts("  GDT: installed\n");
    vga_puts("  IDT: 256 entries, 32 exceptions + 16 IRQs\n");
    vga_puts("  PIC: remapped (master 0x20-0x27, slave 0x28-0x2F)\n");
    vga_puts("  Serial: COM1 115200 baud\n");
    vga_puts("  VGA: 80x25 text mode\n\n");
        vga_printf("  CR3: 0x%x (page directory)\n", read_cr3());
        vga_printf("  Paging: %s\n", (read_cr0() & 0x80000000) ? "ENABLED" : "DISABLED");
        vga_printf("  Physical RAM: ~%.1f MB\n", 
                   (pmm_get_total_frames() * 4096) / (1024.0 * 1024.0));
        vga_printf("  Free memory: %.1f MB\n",
                   (pmm_get_free_frames() * 4096) / (1024.0 * 1024.0));
    
        uint32_t heap_used, heap_free;
        heap_get_stats(&heap_used, &heap_free);
        vga_printf("  Kernel heap: %u bytes used, %u free\n", heap_used, heap_free);
        vga_puts("\n");
    
    /* Test message to verify all outputs work */
    vga_puts(">>> System initialization test:\n");
    serial_puts("[KERNEL] Bootstrap sequence complete\n");
    serial_puts("[KERNEL] System is ready for hardware interrupts\n");
    serial_puts("[KERNEL] All IRQs currently masked (disabled by PIC)\n");
    serial_puts("[KERNEL] Virtual memory is active\n");
        serial_puts("[KERNEL] Kernel heap allocator initialized\n");
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
