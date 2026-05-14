/*
 * arch/x86/context.h - CPU Context Switching Utilities
 *
 * Low-level context switching infrastructure.
 * Currently: Structure definitions and assembly stubs
 * Future: Actual context switch implementation
 *
 * Context switching flow:
 * 1. Save current task's registers to memory (cpu_context_t)
 * 2. Load scheduler's choice of next task's registers from memory
 * 3. Jump to next task's EIP
 * 4. Interrupt restores registers and continues execution
 */

#ifndef ARCH_X86_CONTEXT_H
#define ARCH_X86_CONTEXT_H

#include "kernel/task.h"

/**
 * context_switch() - Switch to a different task's CPU context
 *
 * Args:
 *   from - Current task to save (can be NULL)
 *   to   - New task to switch to
 *
 * Behavior:
 * 1. Saves current CPU state to from->context
 * 2. Loads to->context registers
 * 3. Jumps to next task's EIP
 *
 * Never returns to caller (jumps to new task instead).
 *
 * DANGEROUS: Must be called with interrupts disabled.
 * Stack must be carefully managed.
 *
 * Not yet implemented - placeholder.
 */
void context_switch(task_t *from, task_t *to);

/**
 * context_save() - Save current CPU context to structure
 *
 * Saves all general-purpose registers and control registers.
 * Useful for debugging, not typically called directly.
 */
void context_save(cpu_context_t *ctx);

/**
 * context_restore() - Restore CPU context from structure
 *
 * Restores all general-purpose registers and control registers.
 * After this call, EIP points to next instruction (resume point).
 */
void context_restore(cpu_context_t *ctx);

/**
 * context_init_for_task() - Initialize context for new task
 *
 * Sets up entry point, stack pointer, and flag to run task.
 * Called during task creation.
 *
 * Args:
 *   task    - Task to initialize for
 *   entry   - Entry point address
 *   stack   - Initial stack pointer
 */
void context_init_for_task(task_t *task, uint32_t entry, uint32_t stack);

#endif /* ARCH_X86_CONTEXT_H */
