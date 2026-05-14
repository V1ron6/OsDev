/*
 * arch/x86/idt.c - Interrupt Descriptor Table Implementation
 *
 * Manages the IDT: creates, installs handlers, and configures interrupts.
 *
 * Memory layout:
 * - IDT size: 256 entries × 8 bytes = 2048 bytes
 * - Located after kernel BSS section (see kernel.ld)
 * - Loaded via LIDT with IDTR descriptor
 *
 * Gate descriptor format (8 bytes):
 * - Bits 0-15: Offset (low)
 * - Bits 16-31: Selector
 * - Bits 32-39: Reserved (0)
 * - Bits 40-47: Gate type + attributes
 * - Bits 48-63: Offset (high)
 *
 * Important:
 * - All 256 entries must be valid (no NULL entries)
 * - Missing handler will cause #UD (undefined opcode) or triple fault
 * - Interrupt gates clear IF flag; trap gates preserve it
 */

#include "idt.h"
#include "x86.h"
#include "serial.h"
#include "panic.h"

/* IDT storage (aligned to 8 bytes per entry requirement)
 *
 * Allocate in .bss for early initialization.
 * All 256 entries initialized to safe defaults.
 */
static struct idt_gate idt_table[IDT_ENTRIES] __attribute__((aligned(8)));

/* Default handler stub - for unimplemented interrupts
 *
 * This handler is used for any interrupt without a specific handler.
 * Prevents triple faults from missing handlers.
 *
 * External linkage allows assembly to reference it.
 */
extern void _isr_stub(void);

/* Load IDT into CPU
 *
 * Loads the IDT descriptor register (IDTR) via LIDT instruction.
 * This tells the CPU where the IDT is located and how large it is.
 *
 * @param idtr: pointer to IDTR descriptor
 *
 * The LIDT instruction is CPU-privileged (ring 0 only).
 * This function is called during kernel initialization.
 */
static void lidt(struct idtr_descriptor *idtr) {
    __asm__ volatile("lidt (%0)" : : "r"(idtr));
}

/* Initialize IDT
 *
 * Called early during kernel initialization.
 * Procedure:
 * 1. Clear all IDT entries to safe state
 * 2. Install exception handlers (vectors 0-31)
 * 3. Create IDTR descriptor
 * 4. Load IDTR into CPU
 *
 * After init, the IDT is ready but interrupts are not enabled.
 * Call enable_interrupts() to start receiving interrupts.
 *
 * Serial output:
 * - Logs initialization progress
 * - Useful for debugging IDT setup issues
 */
void idt_init(void) {
    serial_puts("[IDT] Initializing Interrupt Descriptor Table\n");
    
    /* Clear all IDT entries
     *
     * Each entry is an idt_gate structure (8 bytes).
     * Clearing to all-zeros creates invalid descriptors that would
     * cause #UD if triggered. Better than leaving uninitialized.
     */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_table[i].offset_low = 0;
        idt_table[i].offset_high = 0;
        idt_table[i].selector = 0;
        idt_table[i].reserved = 0;
        idt_table[i].type_attr = 0;  /* Not present, will cause #UD */
    }
    
    serial_puts("[IDT] Cleared 256 entries\n");
    
    /* Install default stub for all interrupts
     *
     * This is a minimal bootstrap approach. Each entry gets the same
     * stub handler that just disables interrupts and panics.
     * This prevents triple faults from missing handlers.
     *
     * Later, specific handlers are installed via idt_set_gate().
     */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint32_t)_isr_stub, IDT_GATE_INTR_32, 0);
    }
    
    serial_puts("[IDT] Installed default handler stubs\n");
    
    /* Create IDTR descriptor
     *
     * Size field = IDT size - 1 (in bytes)
     * Offset field = linear address of first IDT entry
     *
     * The IDTR is 6 bytes (48 bits):
     * - Bits 0-15: Size (IDT limit)
     * - Bits 16-47: Base address
     */
    struct idtr_descriptor idtr;
    idtr.size = (IDT_ENTRIES * sizeof(struct idt_gate)) - 1;
    idtr.offset = (uint32_t)&idt_table[0];
    
    serial_printf("[IDT] IDTR base: 0x%x, size: %d\n", idtr.offset, idtr.size);
    
    /* Load IDTR into CPU
     *
     * This tells the CPU where to find the IDT.
     * After this, any interrupt/exception will be routed to the
     * corresponding handler specified in the IDT.
     */
    lidt(&idtr);
    
    serial_puts("[IDT] IDTR loaded, IDT is ready for use\n");
}

/* Set IDT entry for a specific interrupt
 *
 * @param vector: interrupt number (0-255)
 * @param handler: address of handler function
 * @param gate_type: IDT_GATE_INTR_32 or IDT_GATE_TRAP_32
 * @param dpl: descriptor privilege level (0 = kernel, 3 = user)
 *
 * Converts handler address to gate descriptor and installs.
 *
 * The handler address is split:
 * - Bits 0-15: stored in offset_low
 * - Bits 16-31: stored in offset_high
 * - Selector: points to code segment (usually 0x0008 for kernel)
 *
 * Gate type flags:
 * - IDT_GATE_INTR_32 (0x0E): interrupt gate (clears IF on entry)
 * - IDT_GATE_TRAP_32 (0x0F): trap gate (preserves IF)
 * - Both require IDT_ATTR_PRESENT (0x80) to be valid
 */
void idt_set_gate(uint8_t vector, uint32_t handler, uint8_t gate_type, uint8_t dpl) {
    /* Bounds check - prevent corruption */
    if (vector >= IDT_ENTRIES) {
        serial_printf("[IDT] ERROR: vector %d out of range\n", vector);
        return;
    }
    
    struct idt_gate *gate = &idt_table[vector];
    
    /* Split handler address across offset_low and offset_high */
    gate->offset_low = (uint16_t)(handler & 0xFFFF);
    gate->offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    
    /* Code segment selector (from GDT)
     *
     * The kernel code segment is at offset 0x0008 in our GDT:
     * - GDT[0]: NULL (0x0000)
     * - GDT[1]: CODE (0x0008) ← kernel code
     * - GDT[2]: DATA (0x0010) ← kernel data
     *
     * This value is fixed for the bootstrap GDT.
     */
    gate->selector = 0x0008;
    
    /* Reserved field must be 0 for CPU compatibility */
    gate->reserved = 0;
    
    /* Gate type and privilege flags
     *
     * The type_attr byte format:
     * - Bit 7: P (present) = 1
     * - Bits 6-5: DPL (privilege level) = 00 for kernel, 11 for user
     * - Bit 4: reserved (0)
     * - Bits 3-0: gate type
     *
     * Combined formula:
     *   type_attr = IDT_ATTR_PRESENT | (dpl << 5) | gate_type
     *             = 0x80 | (dpl << 5) | gate_type
     */
    gate->type_attr = IDT_ATTR_PRESENT | ((dpl & 0x3) << 5) | (gate_type & 0x0F);
}

/* Return IDT base address for debugging */
void *idt_get_base(void) {
    return (void *)&idt_table[0];
}
