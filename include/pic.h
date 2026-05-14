/*
 * pic.h - Programmable Interrupt Controller Interface
 *
 * The 8259A PIC remaps hardware IRQs (0-7 from master, 8-15 from slave)
 * into CPU exception vectors.
 *
 * By default, PIC maps IRQs to CPU exception vectors 8-15 (conflicts with
 * CPU exceptions). We remap to vectors 0x20-0x27 (master) and 0x28-0x2F (slave).
 *
 * Hardware configuration:
 * - Master PIC: I/O ports 0x20 (command), 0x21 (data)
 * - Slave PIC: I/O ports 0xA0 (command), 0xA1 (data)
 * - Slave connected to master IRQ line 2
 */

#ifndef _BYTEBANDIT_PIC_H
#define _BYTEBANDIT_PIC_H

#include "types.h"

/* Initialize and remap PIC
 *
 * Must be called before enabling hardware interrupts.
 * Remaps:
 *   - Master PIC IRQs 0-7 to vectors 0x20-0x27
 *   - Slave PIC IRQs 8-15 to vectors 0x28-0x2F
 *
 * This moves hardware IRQs out of CPU exception vector range (0-31).
 */
void pic_init(void);

/* Send End Of Interrupt (EOI) to PIC
 *
 * Must be called at end of IRQ handler to re-enable PIC.
 * @param irq: IRQ number (0-15), or 0xFF to skip EOI
 *
 * The PIC holds the IRQ line asserted until EOI is received.
 * Without EOI, further IRQs on same line won't be handled.
 */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) specific IRQ
 *
 * @param irq: IRQ number (0-15)
 *
 * Prevents the IRQ from being signaled to the CPU.
 * Useful for disabling specific hardware interrupts.
 */
void pic_mask_irq(uint8_t irq);

/* Unmask (enable) specific IRQ
 *
 * @param irq: IRQ number (0-15)
 *
 * Re-enables interrupt delivery for masked IRQ.
 */
void pic_unmask_irq(uint8_t irq);

/* Get IRQ mask status
 *
 * Returns: 16-bit mask (bit set = IRQ disabled)
 */
uint16_t pic_get_mask(void);

#endif
