/*
 * idt.h - Interrupt Descriptor Table Interface
 *
 * The IDT stores interrupt and exception handler addresses.
 * The CPU uses the IDT to route interrupts/exceptions to handlers.
 *
 * Structure:
 * - 256 IDT entries (interrupts 0-255)
 * - Each entry: 8 bytes (gate descriptor)
 * - Loaded via IDTR (IDT Register) with LIDT instruction
 *
 * Gate types:
 * - Task gate: not used in protected mode (usually)
 * - Interrupt gate: clears interrupt flag (IF) on entry
 * - Trap gate: preserves interrupt flag
 *
 * Privilege levels:
 * - Ring 0 (kernel): CPL 0
 * - Ring 3 (user): CPL 3
 */

#ifndef _BYTEBANDIT_IDT_H
#define _BYTEBANDIT_IDT_H

#include "types.h"
#include "x86.h"

/* Number of IDT entries */
#define IDT_ENTRIES 256

/* IDT IDTR descriptor (for LIDT instruction) */
struct idtr_descriptor {
    uint16_t size;      /* IDT size - 1 (in bytes) */
    uint32_t offset;    /* Linear address of IDT base */
} __attribute__((packed));

/* Initialize IDT and install default handlers
 *
 * Must be called before enabling interrupts.
 * - Clears all 256 entries
 * - Installs exception handlers (0-31)
 * - Loads IDTR
 * - Does NOT enable interrupts (use enable_interrupts() separately)
 */
void idt_init(void);

/* Install interrupt/exception handler
 *
 * @param vector: interrupt number (0-255)
 * @param handler: pointer to handler function
 * @param gate_type: IDT_GATE_INTR_32 or IDT_GATE_TRAP_32
 * @param dpl: descriptor privilege level (0 for kernel, 3 for user)
 *
 * Handler signature should match ISR_HANDLER_FUNC:
 *   void handler(uint32_t int_no, uint32_t err_code);
 *
 * For exceptions without error codes, err_code will be 0.
 */
void idt_set_gate(uint8_t vector, uint32_t handler, uint8_t gate_type, uint8_t dpl);

/* Get the IDT base address
 *
 * Useful for debugging/inspection.
 * Returns: pointer to first IDT entry
 */
void *idt_get_base(void);

#endif
