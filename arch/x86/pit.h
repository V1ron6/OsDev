/*
 * arch/x86/pit.h - Programmable Interval Timer (PIT) Driver
 *
 * 8253/8254 Timer for periodic interrupts.
 *
 * The PIT is used to generate regular clock interrupts (IRQ0)
 * which drive the task scheduler for preemptive multitasking.
 */

#ifndef ARCH_X86_PIT_H
#define ARCH_X86_PIT_H

#include "types.h"

/**
 * Initialize the PIT for periodic interrupts.
 *
 * Sets up timer to generate an interrupt every 10ms (100 Hz).
 * Must be called after IRQ system is initialized.
 *
 * Registers IRQ0 handler for task switching.
 */
void pit_init(void);

/**
 * Get the current timer tick count.
 *
 * @return Number of timer ticks since boot
 *
 * Incremented on each IRQ0 (PIT) interrupt.
 */
uint32_t pit_get_ticks(void);

/**
 * Convert ticks to milliseconds.
 *
 * @param ticks Timer ticks
 * @return Equivalent time in milliseconds (assuming 100 Hz timer)
 */
uint32_t pit_ticks_to_ms(uint32_t ticks);

/**
 * Sleep for the specified milliseconds.
 *
 * @param ms Milliseconds to sleep
 *
 * Yields CPU to scheduler, task will resume after timeout.
 * Not implemented yet (yields indefinitely).
 */
void pit_sleep_ms(uint32_t ms);

#endif /* ARCH_X86_PIT_H */
