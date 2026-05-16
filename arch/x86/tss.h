/*
 * arch/x86/tss.h - Task State Segment (TSS) Management
 *
 * The TSS is a 104-byte structure used for:
 * - Storing kernel stack pointer (esp0/ss0) for privilege transitions
 * - Storing backup registers for task switching (not used in software switching)
 * - Storing segment registers for task switching
 *
 * When a user program (ring 3) triggers an interrupt:
 * 1. CPU loads kernel ESP and SS from TSS.esp0 and TSS.ss0
 * 2. CPU switches to kernel privilege level
 * 3. Interrupt handler runs in kernel mode
 * 4. Handler returns via IRET, CPU switches back to ring 3
 *
 * We use TSS *only* for privilege stack switching, not hardware task switching.
 */

#ifndef _BYTEBANDIT_TSS_H
#define _BYTEBANDIT_TSS_H

#include "types.h"

/**
 * Task State Segment (TSS) - 104 bytes for 32-bit protected mode.
 *
 * Layout matches Intel x86 TSS specification.
 * Fields are in the order required by x86 specification.
 */
typedef struct {
    /* Previous task link (only used with hardware task switch) */
    uint32_t previous_task_link;
    
    /* Ring 0 stack pointer and segment (used on privilege transitions) */
    uint32_t esp0;          /* Stack pointer for ring 0 */
    uint32_t ss0;           /* Stack segment for ring 0 */
    
    /* Ring 1 stack (not used in our system) */
    uint32_t esp1;
    uint32_t ss1;
    
    /* Ring 2 stack (not used in our system) */
    uint32_t esp2;
    uint32_t ss2;
    
    /* PDBR (page directory base register) - CR3 value
     * Only used if hardware task switching is enabled
     */
    uint32_t cr3;
    
    /* Task execution start point (not used with IRET) */
    uint32_t eip;
    
    /* EFLAGS (not used with IRET) */
    uint32_t eflags;
    
    /* General purpose registers (saved by CPU during task switch)
     * Not used in our software context switching
     */
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    
    /* Segment registers (not used in our system) */
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    
    /* LDT selector (we don't use LDTs) */
    uint32_t ldtr;
    
    /* Debug trap (T bit in EFLAGS) and I/O bitmap */
    uint16_t reserved;      /* I/O bitmap offset - set to 0 */
    uint16_t io_base;       /* I/O bitmap base - not used */
    
} __attribute__((packed)) tss_t;

/**
 * Initialize the TSS and load it into the GDT.
 *
 * This function:
 * - Allocates TSS structure from kernel heap
 * - Initializes all fields to safe values
 * - Sets esp0/ss0 to kernel stack pointers
 * - Registers TSS in GDT
 * - Loads TSS via LTR instruction
 *
 * Must be called after GDT is initialized.
 * Must be called after heap is initialized (uses kmalloc).
 */
void tss_init(void);

/**
 * Update the TSS kernel stack pointer.
 *
 * @param esp0 New kernel stack pointer (stack top)
 * @param ss0  Kernel stack segment (usually KERN_DATA_SEL)
 *
 * Called when switching tasks to update where the kernel
 * stack for the next user task will be.
 */
void tss_set_kernel_stack(uint32_t esp0, uint32_t ss0);

/**
 * Get the current TSS address.
 *
 * @return Physical address of the kernel's TSS structure.
 */
uint32_t tss_get_address(void);

/**
 * Get the current TSS structure.
 *
 * @return Pointer to the kernel's TSS structure.
 */
tss_t *tss_get_structure(void);

#endif /* _BYTEBANDIT_TSS_H */
