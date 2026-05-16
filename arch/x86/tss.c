/*
 * arch/x86/tss.c - Task State Segment Implementation
 *
 * Manages TSS allocation, initialization, and updates.
 */

#include "tss.h"
#include "gdt.h"
#include "mm/heap.h"
#include "serial.h"
#include "vga.h"

/* Global pointer to the kernel's TSS */
static tss_t *kernel_tss = NULL;

void tss_init(void) {
    serial_puts("[TSS] Initializing Task State Segment\n");
    
    /* Allocate TSS from kernel heap */
    kernel_tss = (tss_t *)kmalloc(sizeof(tss_t));
    if (!kernel_tss) {
        serial_puts("[ERROR] Failed to allocate TSS from heap\n");
        return;
    }
    
    /* Zero the entire TSS */
    uint8_t *tss_bytes = (uint8_t *)kernel_tss;
    for (int i = 0; i < sizeof(tss_t); i++) {
        tss_bytes[i] = 0;
    }
    
    /* Set kernel stack segment selector for ring 0
     *
     * When a user program triggers an interrupt, the CPU will load this
     * ESP and SS to switch to kernel mode on the kernel stack.
     */
    kernel_tss->ss0 = 0x10;  /* Kernel data segment selector (KERN_DATA_SEL) */
    
    /* ESP0 will be set by scheduler when switching tasks
     * Initially, set to a safe kernel stack location
     */
    kernel_tss->esp0 = 0x93000;  /* Kernel stack base (grows down) */
    
    /* All other fields left as 0 for now
     * - We don't use hardware task switching
     * - We handle context manually in software
     */
    
    /* Register TSS in GDT and get the descriptor set up */
    gdt_set_tss_descriptor((uint32_t)kernel_tss);
    
    /* Load TSS into task register via LTR instruction
     *
     * After this, the CPU knows where to find the TSS when privilege
     * transitions occur (user code calling int, user code triggering exception, etc.)
     */
    uint16_t tss_selector = (5 << 3);  /* GDT_INDEX_TSS << 3, TI and RPL are 0 */
    __asm__ volatile("ltr %0" : : "r"(tss_selector));
    
    serial_printf("[TSS] Allocated at 0x%08x, kernel stack at 0x%08x\n",
                  (uint32_t)kernel_tss, kernel_tss->esp0);
}

void tss_set_kernel_stack(uint32_t esp0, uint32_t ss0) {
    if (!kernel_tss) {
        serial_puts("[WARNING] tss_set_kernel_stack: TSS not initialized\n");
        return;
    }
    
    kernel_tss->esp0 = esp0;
    kernel_tss->ss0 = ss0;
}

uint32_t tss_get_address(void) {
    return (uint32_t)kernel_tss;
}

tss_t *tss_get_structure(void) {
    return kernel_tss;
}
