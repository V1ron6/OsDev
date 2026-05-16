/*
 * kernel/context.h - Context Switching Interface
 *
 * High-level interface for task context management.
 * Low-level assembly implementation in arch/x86/context.asm
 */

#ifndef _BYTEBANDIT_CONTEXT_H
#define _BYTEBANDIT_CONTEXT_H

#include "types.h"
#include "kernel/task.h"

/**
 * Initialize CPU context for a new task.
 *
 * @param task Target task to initialize
 * @param entry Entry point (instruction address)
 * @param stack_top Top of the task's kernel stack
 *
 * Sets up context so that when task is restored, it will jump to entry_point.
 * Used for creating new kernel tasks.
 * User tasks need additional setup (user stack, page directory, etc).
 */
void context_init_for_task(task_t *task, uint32_t entry, uint32_t stack_top);

/**
 * Perform a task context switch (low-level).
 *
 * @param from Current task context
 * @param to Target task context
 *
 * Saves current CPU state to 'from', restores 'to' state, and jumps to 'to'.
 * Must be called from assembly because it manipulates the stack and jumps.
 * Interrupts must be disabled during switch.
 *
 * Not typically called directly - use scheduler + interrupt handler instead.
 */
void context_switch_asm(cpu_context_t *from, cpu_context_t *to);

/**
 * Save current CPU context to a structure.
 *
 * @param ctx Pointer to context structure to save into
 *
 * Saves all registers that are needed for a task switch.
 * Usually called from interrupt handler before context_switch_asm.
 */
void context_save(cpu_context_t *ctx);

/**
 * Restore CPU context from a structure.
 *
 * @param ctx Pointer to context structure to restore from
 *
 * Loads registers from saved context.
 * Usually called by context_switch_asm or at task startup.
 */
void context_restore(cpu_context_t *ctx);

#endif /* _BYTEBANDIT_CONTEXT_H */
