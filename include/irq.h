/*
 * irq.h - Hardware Interrupt (IRQ) Handler Interface
 *
 * Provides mechanism for registering and handling hardware interrupts (0-15).
 * IRQs are remapped to CPU vectors 0x20-0x2F by the PIC.
 *
 * Typical usage:
 *   1. irq_init() - Initialize IRQ system and install stubs
 *   2. irq_handler_install(irq, handler) - Register handler
 *   3. irq_enable_irq(irq) - Unmask IRQ at PIC
 *   4. enable_interrupts() - Enable CPU interrupts
 */

#ifndef _BYTEBANDIT_IRQ_H
#define _BYTEBANDIT_IRQ_H

#include "types.h"

/* Hardware interrupt handler callback
 *
 * Called when hardware IRQ occurs.
 * The handler is responsible for:
 * - Reading data from the device
 * - Clearing device interrupt flag
 * - Calling pic_send_eoi() to re-enable PIC
 *
 * @param irq: IRQ number (0-15)
 */
typedef void (*irq_handler_t)(uint8_t irq);

/* Initialize IRQ system
 *
 * Must be called after:
 * - idt_init() (IDT must be ready)
 * - pic_init() (PIC must be remapped)
 *
 * Installs generic IRQ handler stub for all 16 hardware IRQs.
 * Does NOT enable interrupts or unmask IRQs yet.
 */
void irq_init(void);

/* Register IRQ handler
 *
 * @param irq: IRQ number (0-15)
 * @param handler: callback function, or NULL to remove handler
 *
 * Replaces any existing handler for this IRQ.
 * Handler is called when IRQ fires (if IRQ is unmasked at PIC).
 *
 * Handlers should be quick and not perform blocking operations.
 */
void irq_install_handler(uint8_t irq, irq_handler_t handler);

/* Get registered handler for IRQ */
irq_handler_t irq_get_handler(uint8_t irq);

/* Enable specific IRQ (unmask at PIC)
 *
 * Must be called before interrupts can be delivered for that IRQ.
 * Also requires enable_interrupts() to be called at CPU level.
 */
void irq_enable(uint8_t irq);

/* Disable specific IRQ (mask at PIC) */
void irq_disable(uint8_t irq);

/* Get IRQ enable/mask status */
uint16_t irq_get_status(void);

#endif
