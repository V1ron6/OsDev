/*
 * arch/x86/irq.c - Hardware Interrupt Handler
 *
 * Routes hardware IRQs to registered device handlers.
 * Manages IRQ dispatch, EOI, and handler registration.
 *
 * Flow:
 * 1. CPU receives interrupt on vector 0x20-0x2F (mapped from IRQ 0-15 by PIC)
 * 2. CPU calls corresponding IRQ stub from irq.asm
 * 3. Stub calls irq_handler(irq_number)
 * 4. irq_handler() dispatches to registered handler
 * 5. irq_handler() sends EOI to PIC
 * 6. stub returns via IRET
 */

#include "irq.h"
#include "pic.h"
#include "idt.h"
#include "x86.h"
#include "serial.h"
#include "panic.h"

/* Forward declare IRQ stubs from assembly */
extern void _irq0(void);
extern void _irq1(void);
extern void _irq2(void);
extern void _irq3(void);
extern void _irq4(void);
extern void _irq5(void);
extern void _irq6(void);
extern void _irq7(void);
extern void _irq8(void);
extern void _irq9(void);
extern void _irq10(void);
extern void _irq11(void);
extern void _irq12(void);
extern void _irq13(void);
extern void _irq14(void);
extern void _irq15(void);

/* Array of IRQ stub addresses */
static void (*irq_stubs[16])(void) = {
    _irq0, _irq1, _irq2, _irq3, _irq4, _irq5, _irq6, _irq7,
    _irq8, _irq9, _irq10, _irq11, _irq12, _irq13, _irq14, _irq15
};

/* Registered IRQ handlers
 *
 * Drivers register handlers for the IRQs they're interested in.
 * NULL = no handler installed
 */
static irq_handler_t irq_handlers[16] = {NULL};

/* Initialize IRQ system
 *
 * Installs IRQ stubs in the IDT at vectors 0x20-0x2F.
 * Does not enable interrupts or unmask IRQs.
 *
 * Requirements:
 * - IDT must be initialized (idt_init already called)
 * - PIC must be remapped (pic_init already called)
 */
void irq_init(void) {
    serial_puts("[IRQ] Initializing Hardware Interrupt (IRQ) System\n");
    
    /* Install each IRQ stub into the IDT
     *
     * IRQs 0-7 → vectors 0x20-0x27 (master PIC)
     * IRQs 8-15 → vectors 0x28-0x2F (slave PIC)
     *
     * Use interrupt gates (not trap gates) because we want interrupts
     * to be disabled during handler execution (standard IRQ behavior).
     */
    for (int i = 0; i < 16; i++) {
        uint8_t vector = 0x20 + i;  /* Vector = 0x20 + IRQ number */
        idt_set_gate(vector, (uint32_t)irq_stubs[i], IDT_GATE_INTR_32, 0);
    }
    
    serial_printf("[IRQ] Installed 16 IRQ stubs: vectors 0x20-0x2F (IRQs 0-15)\n");
    serial_puts("[IRQ] All IRQs currently masked (disabled)\n");
    serial_puts("[IRQ] Ready for handler installation\n");
}

/* Install IRQ handler
 *
 * @param irq: IRQ number (0-15)
 * @param handler: callback function, or NULL to uninstall
 *
 * Replaces any existing handler.
 * Handler is called when IRQ fires and is unmasked.
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler) {
    if (irq >= 16) {
        serial_printf("[IRQ] ERROR: Invalid IRQ %d (must be 0-15)\n", irq);
        return;
    }
    
    irq_handlers[irq] = handler;
    
    if (handler) {
        serial_printf("[IRQ] Handler installed for IRQ %d\n", irq);
    } else {
        serial_printf("[IRQ] Handler removed for IRQ %d\n", irq);
    }
}

/* Get registered handler */
irq_handler_t irq_get_handler(uint8_t irq) {
    if (irq >= 16) {
        return NULL;
    }
    return irq_handlers[irq];
}

/* Enable specific IRQ
 *
 * Unmasks the IRQ at the PIC so it can be delivered to CPU.
 * Still requires enable_interrupts() at CPU level.
 */
void irq_enable(uint8_t irq) {
    if (irq >= 16) {
        serial_printf("[IRQ] ERROR: Invalid IRQ %d\n", irq);
        return;
    }
    pic_unmask_irq(irq);
    serial_printf("[IRQ] Enabled IRQ %d\n", irq);
}

/* Disable specific IRQ
 *
 * Masks the IRQ at the PIC to prevent delivery.
 */
void irq_disable(uint8_t irq) {
    if (irq >= 16) {
        serial_printf("[IRQ] ERROR: Invalid IRQ %d\n", irq);
        return;
    }
    pic_mask_irq(irq);
    serial_printf("[IRQ] Disabled IRQ %d\n", irq);
}

/* Get IRQ status */
uint16_t irq_get_status(void) {
    return pic_get_mask();
}

/* Common IRQ handler - called by assembly stubs
 *
 * @param irq: IRQ number (0-15)
 *
 * This function:
 * 1. Calls the registered handler (if any)
 * 2. Sends EOI to PIC
 * 3. Returns (stub performs IRET)
 *
 * Called with interrupts disabled (interrupt gate behavior).
 * Should execute quickly to minimize latency.
 */
void irq_handler(uint8_t irq) {
    /* Validate IRQ number */
    if (irq >= 16) {
        serial_printf("[IRQ] ERROR: Invalid IRQ in handler: %d\n", irq);
        pic_send_eoi(irq);  /* Send EOI anyway to avoid hung IRQ */
        return;
    }
    
    /* Call registered handler if one exists */
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](irq);
    } else {
        /* No handler - this is unexpected but not fatal
         *
         * In a real system, this might indicate:
         * - Device interrupt fired but driver isn't loaded
         * - Spurious interrupt from device
         * - Hardware misconfiguration
         *
         * Log it for debugging but continue.
         */
        serial_printf("[IRQ] Unhandled IRQ %d\n", irq);
    }
    
    /* Send End-Of-Interrupt to PIC
     *
     * Without EOI, the IRQ line remains held and no new interrupts
     * on the same line will be accepted.
     */
    pic_send_eoi(irq);
}
