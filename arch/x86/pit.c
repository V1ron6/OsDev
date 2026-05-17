/*
 * arch/x86/pit.c - Programmable Interval Timer (PIT) Driver
 *
 * 8253/8254 PIT for periodic interrupts at 100 Hz (10ms).
 * Used for task preemption and scheduling.
 *
 * PIT I/O Ports:
 *   0x40 - Counter 0 (tied to IRQ0)
 *   0x41 - Counter 1 (RAM refresh, not used)
 *   0x42 - Counter 2 (speaker, not used)
 *   0x43 - Control register
 *
 * To set frequency:
 * 1. Write control byte to 0x43 (counter select, access mode, operating mode)
 * 2. Write low byte of divisor to 0x40
 * 3. Write high byte of divisor to 0x40
 *
 * Divisor = 1193182 Hz / desired_frequency
 * For 100 Hz: divisor = 11931
 */

#include "arch/x86/pit.h"
#include "irq.h"
#include "kernel/scheduler.h"
#include "kernel/task.h"
#include "kernel/context.h"
#include "serial.h"
#include "x86.h"

/* I/O port definitions */
#define PIT_PORT_COUNTER0   0x40
#define PIT_PORT_COUNTER1   0x41
#define PIT_PORT_COUNTER2   0x42
#define PIT_PORT_CONTROL    0x43

/* Control register bits */
#define PIT_CTRL_COUNTER_SELECT_0   (0 << 6)
#define PIT_CTRL_COUNTER_SELECT_1   (1 << 6)
#define PIT_CTRL_COUNTER_SELECT_2   (2 << 6)

#define PIT_CTRL_ACCESS_MODE_LATCH  (0 << 4)
#define PIT_CTRL_ACCESS_MODE_LO     (1 << 4)
#define PIT_CTRL_ACCESS_MODE_HI     (2 << 4)
#define PIT_CTRL_ACCESS_MODE_BOTH   (3 << 4)

#define PIT_CTRL_OP_MODE_INTERRUPT  (0 << 1)  /* Mode 0: interrupt on terminal count */
#define PIT_CTRL_OP_MODE_ONESHOT    (1 << 1)  /* Mode 1: programmable one-shot */
#define PIT_CTRL_OP_MODE_RATE       (2 << 1)  /* Mode 2: rate generator (periodic) */
#define PIT_CTRL_OP_MODE_SQ_WAVE    (3 << 1)  /* Mode 3: square wave generator */

#define PIT_CTRL_BCD                1         /* Mode: 0=binary, 1=BCD */

/* PIT frequency settings */
#define PIT_BASE_FREQUENCY  1193182    /* Hz - base clock frequency */
#define PIT_DESIRED_FREQ    100        /* Hz - desired interrupt frequency */
#define PIT_DIVISOR         (PIT_BASE_FREQUENCY / PIT_DESIRED_FREQ)

/* Global timer state */
static volatile uint32_t pit_ticks = 0;

/**
 * PIT interrupt handler
 *
 * Called on each PIT interrupt (IRQ0, ~100 Hz).
 * Performs context switching to next task.
 *
 * Flow:
 * 1. Increment tick counter
 * 2. Account for CPU time
 * 3. If not yet initialized, just increment and return
 * 4. Save current task's context
 * 5. Select next task from scheduler
 * 6. Restore next task's context
 * 7. IRET to next task
 */
static void pit_handler(uint8_t irq) {
    (void)irq;  /* IRQ0, not needed */
    
    pit_ticks++;
    
    /* Before scheduler is initialized, just count ticks */
    if (scheduler_current() == NULL) {
        return;
    }
    
    /* Account for CPU time used by current task */
    scheduler_account_time();
    
    /* Get current and next task */
    task_t *current = scheduler_current();
    task_t *next = scheduler_select_next();
    
    if (!next) {
        /* No task to run (shouldn't happen) */
        return;
    }
    
    /* If switching tasks, perform context switch */
    if (current != next) {
        /* TODO: Context switch assembly
         * 
         * This is where we would:
         * 1. Save current->context from CPU registers
         * 2. Load next->context into CPU registers
         * 3. Update TSS.esp0 for privilege transitions
         * 4. Set CR3 for page directory switching
         * 5. IRET to next task
         *
         * For now, just mark task switch (actual context switch
         * requires assembly and interrupt frame manipulation).
         */
        serial_printf("[PIT] Task switch: %u -> %u\n", current->pid, next->pid);
        scheduler_set_current(next);
    }
}

void pit_init(void) {
    serial_puts("[PIT] Initializing Programmable Interval Timer\n");
    
    /* Install IRQ0 handler for task switching */
    irq_install_handler(0, pit_handler);
    
    /* Configure PIT counter 0 for mode 2 (rate generator, periodic) */
    uint8_t control = PIT_CTRL_COUNTER_SELECT_0 |
                      PIT_CTRL_ACCESS_MODE_BOTH |
                      PIT_CTRL_OP_MODE_RATE |
                      PIT_CTRL_BCD;  /* Binary mode (not BCD) */
    
    outb(PIT_PORT_CONTROL, control);
    
    /* Load divisor (little-endian: low byte first, then high byte) */
    outb(PIT_PORT_COUNTER0, (uint8_t)(PIT_DIVISOR & 0xFF));        /* Low byte */
    outb(PIT_PORT_COUNTER0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF)); /* High byte */
    
    serial_printf("[PIT] Configured for %u Hz (divisor %u)\n",
                  PIT_DESIRED_FREQ, PIT_DIVISOR);
    serial_printf("[PIT] Timer ticks every ~%u ms\n",
                  1000 / PIT_DESIRED_FREQ);
    
    /* Enable IRQ0 at PIC */
    irq_enable(0);
    serial_puts("[PIT] IRQ0 enabled\n");
}

uint32_t pit_get_ticks(void) {
    return pit_ticks;
}

uint32_t pit_ticks_to_ms(uint32_t ticks) {
    /* PIT runs at 100 Hz, so 1 tick = 10 ms */
    return ticks * 10;
}

void pit_sleep_ms(uint32_t ms) {
    (void)ms;
    /* TODO: Block current task, wake up after specified time */
}
