/*
 * arch/x86/pic.c - Programmable Interrupt Controller (8259A)
 *
 * Manages PIC remapping and End-Of-Interrupt (EOI) signaling.
 *
 * Architecture:
 * - Master PIC handles IRQs 0-7 (attached to CPU INT pin)
 * - Slave PIC handles IRQs 8-15 (cascaded to master IRQ2)
 * - Both connected via specific I/O port addresses
 *
 * Default vector mapping (conflicts with CPU exceptions):
 *   Master IRQs 0-7 → CPU vectors 8-15 (conflicts with exceptions)
 *   Slave IRQs 8-15 → CPU vectors 16-23
 *
 * We remap to:
 *   Master IRQs 0-7 → CPU vectors 0x20-0x27 (32-39)
 *   Slave IRQs 8-15 → CPU vectors 0x28-0x2F (40-47)
 */

#include "pic.h"
#include "x86.h"
#include "serial.h"

/* I/O Port addresses for PIC */
#define PIC_MASTER_CMD   0x20    /* Master command port */
#define PIC_MASTER_DATA  0x21    /* Master data port (IMR - interrupt mask) */
#define PIC_SLAVE_CMD    0xA0    /* Slave command port */
#define PIC_SLAVE_DATA   0xA1    /* Slave data port (IMR) */

/* ICW1 - Initialization Command Word 1 */
#define ICW1_ICW4        0x01    /* ICW4 needed */
#define ICW1_SINGLE      0x02    /* Single (not cascade) mode */
#define ICW1_INTERVAL4   0x04    /* Call address interval 4 (80x86) */
#define ICW1_LEVEL       0x08    /* Level triggered (vs edge) */
#define ICW1_INIT        0x10    /* Initialization */

/* ICW4 - Initialization Command Word 4 */
#define ICW4_8086        0x01    /* 8086 mode (vs MCS-80/85) */
#define ICW4_AUTO_EOI    0x02    /* Auto EOI (we use manual) */
#define ICW4_BUF_MASTER  0x04    /* Buffered mode, master */
#define ICW4_BUF_SLAVE   0x08    /* Buffered mode, slave */
#define ICW4_SFNM        0x10    /* Special fully nested mode */

/* OCW1 - Operational Command Word 1 (IMR) */
/* This is the interrupt mask register - bits 0-7 enable/disable IRQs */

/* OCW2 - Operational Command Word 2 (EOI) */
#define OCW2_EOI         0x20    /* Non-specific EOI */
#define OCW2_SPECIFIC    0x60    /* Specific EOI */
#define OCW2_ROTATE      0x80    /* Rotate in automatic EOI mode */

/* Current IRQ mask (remember what's disabled) */
static uint16_t pic_mask = 0xFFFF;  /* Initially all IRQs disabled */

/* Initialize PIC and remap interrupts
 *
 * Procedure:
 * 1. Send ICW1 (initialization) to both PICs
 * 2. Send ICW2 (vector mapping) to both PICs
 * 3. Send ICW3 (cascade configuration) to both PICs
 * 4. Send ICW4 (mode configuration) to both PICs
 * 5. Set initial interrupt mask (all disabled)
 *
 * Initialization command sequence:
 * - ICW1: Basic setup (0x11 = init + ICW4 needed)
 * - ICW2: Base vector address
 * - ICW3: Cascade/slave configuration
 * - ICW4: CPU mode (0x01 = 8086 mode)
 */
void pic_init(void) {
    serial_puts("[PIC] Initializing 8259A Programmable Interrupt Controller\n");
    
    /* Save current masks before we change them */
    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask = inb(PIC_SLAVE_DATA);
    
    serial_printf("[PIC] Current masks: master=0x%x, slave=0x%x\n",
                  master_mask, slave_mask);
    
    /* === ICW1: Start initialization sequence ===
     *
     * Send to both master and slave:
     * - Bit 4 (ICW1): Always set to 1 to start init
     * - Bit 3 (LTIM): 0 = edge triggered, 1 = level triggered
     * - Bit 2 (ADI): 0 = call address interval 4, 1 = call address interval 8
     * - Bit 1 (SNGL): 1 = single, 0 = cascade mode
     * - Bit 0 (IC4): 1 = ICW4 needed, 0 = not needed
     *
     * Value 0x11 = ICW1_INIT | ICW1_ICW4
     *   Bit 4 = 1 (INIT)
     *   Bit 0 = 1 (ICW4 needed)
     */
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);
    serial_puts("[PIC] Sent ICW1 (init) to both PICs\n");
    
    /* === ICW2: Set vector address base ===
     *
     * Tells PIC which CPU exception vectors to use for IRQs.
     *
     * Master PIC: 0x20 (vectors 0x20-0x27 for IRQs 0-7)
     * Slave PIC:  0x28 (vectors 0x28-0x2F for IRQs 8-15)
     *
     * This moves IRQs out of the CPU exception vector range (0-1F).
     */
    outb(PIC_MASTER_DATA, 0x20);  /* Master → vectors 0x20-0x27 */
    outb(PIC_SLAVE_DATA, 0x28);   /* Slave → vectors 0x28-0x2F */
    serial_puts("[PIC] Sent ICW2 (vector mapping): master=0x20, slave=0x28\n");
    
    /* === ICW3: Configure cascade mode ===
     *
     * Tells master and slave about each other (cascade).
     * - Slave PIC is connected to master PIC's IRQ line 2
     *
     * Master ICW3: 0x04 = IRQ2 has slave
     *   Bits 0-7: each bit set if IRQ has slave (only bit 2 set)
     *
     * Slave ICW3: 0x02 = slave is on master IRQ2
     *   Bits 0-7: slave's IRQ position (bit 2 = IRQ2)
     */
    outb(PIC_MASTER_DATA, 0x04);  /* Master: slave on IRQ2 (bit 2 set) */
    outb(PIC_SLAVE_DATA, 0x02);   /* Slave: I'm on IRQ2 */
    serial_puts("[PIC] Sent ICW3 (cascade): slave on master IRQ2\n");
    
    /* === ICW4: Set operating mode ===
     *
     * Configures 8086 mode, EOI behavior, etc.
     *
     * Value 0x01 = ICW4_8086
     *   Bit 0 = 1 (8086 mode)
     *   Other bits = 0 (manual EOI, no buffering, etc.)
     *
     * We use manual EOI mode because:
     * - More control over IRQ handling
     * - Required for proper interrupt priority
     * - Allows nested interrupts if needed
     */
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA, ICW4_8086);
    serial_puts("[PIC] Sent ICW4 (mode): 8086 mode, manual EOI\n");
    
    /* === Set initial interrupt masks ===
     *
     * Both PICs start with all IRQs masked (disabled).
     * This prevents random interrupts during initialization.
     * Drivers will unmask specific IRQs as needed.
     *
     * OCW1 register (Interrupt Mask Register):
     * - Each bit corresponds to one IRQ (0-7 on master, 0-7 on slave)
     * - Bit = 1: IRQ masked (disabled)
     * - Bit = 0: IRQ unmasked (enabled)
     *
     * 0xFF = all IRQs masked
     */
    outb(PIC_MASTER_DATA, 0xFF);  /* Mask all master IRQs */
    outb(PIC_SLAVE_DATA, 0xFF);   /* Mask all slave IRQs */
    pic_mask = 0xFFFF;
    
    serial_puts("[PIC] Initialized: all IRQs masked\n");
    serial_puts("[PIC] Master vectors: 0x20-0x27 (IRQs 0-7)\n");
    serial_puts("[PIC] Slave vectors: 0x28-0x2F (IRQs 8-15)\n");
    serial_puts("[PIC] Ready for interrupt configuration\n");
}

/* Send End-Of-Interrupt (EOI) to PIC
 *
 * Must be called at end of IRQ handler.
 * Without EOI, the IRQ line remains asserted and no more interrupts
 * on that level will be accepted.
 *
 * If IRQ 0-7 (master):
 *   Send EOI to master only
 *
 * If IRQ 8-15 (slave):
 *   Send EOI to both slave and master (because slave cascades to master)
 *
 * Using non-specific EOI (0x20) which services the highest priority IRQ.
 * Could use specific EOI (0x60 | irq_num) for more control.
 */
void pic_send_eoi(uint8_t irq) {
    /* Send EOI to slave if this is a slave IRQ */
    if (irq >= 8) {
        outb(PIC_SLAVE_CMD, OCW2_EOI);
    }
    
    /* Always send EOI to master (even for slave IRQs, since slave cascades to master IRQ2) */
    outb(PIC_MASTER_CMD, OCW2_EOI);
}

/* Mask (disable) specific IRQ
 *
 * @param irq: IRQ number (0-15)
 *
 * Sets the corresponding bit in the Interrupt Mask Register (OCW1).
 * Setting a bit to 1 disables that IRQ.
 */
void pic_mask_irq(uint8_t irq) {
    uint8_t port;
    uint8_t value;
    
    /* Determine which PIC and which bit */
    if (irq < 8) {
        port = PIC_MASTER_DATA;
        value = inb(port) | (1 << irq);
    } else {
        port = PIC_SLAVE_DATA;
        value = inb(port) | (1 << (irq - 8));
    }
    
    /* Update cached mask and write to hardware */
    pic_mask |= (1 << irq);
    outb(port, value);
}

/* Unmask (enable) specific IRQ
 *
 * @param irq: IRQ number (0-15)
 *
 * Clears the corresponding bit in the Interrupt Mask Register.
 * Setting a bit to 0 enables that IRQ.
 */
void pic_unmask_irq(uint8_t irq) {
    uint8_t port;
    uint8_t value;
    
    /* Determine which PIC and which bit */
    if (irq < 8) {
        port = PIC_MASTER_DATA;
        value = inb(port) & ~(1 << irq);
    } else {
        port = PIC_SLAVE_DATA;
        value = inb(port) & ~(1 << (irq - 8));
    }
    
    /* Update cached mask and write to hardware */
    pic_mask &= ~(1 << irq);
    outb(port, value);
}

/* Get current IRQ mask */
uint16_t pic_get_mask(void) {
    return pic_mask;
}
