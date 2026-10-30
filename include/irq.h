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

/* Register frame built by irq.asm before entering C code. */
typedef struct {
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t irq;
	uint32_t error_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t useresp;
	uint32_t userss;
} irq_frame_t;

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

/* Optional handler that may inspect or modify the interrupt return frame. */
typedef uint32_t (*irq_frame_handler_t)(uint8_t irq, irq_frame_t *frame);

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

/* Register a frame-aware handler for scheduler or low-level IRQ work. */
void irq_install_frame_handler(uint8_t irq, irq_frame_handler_t handler);

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
