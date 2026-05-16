/*
 * arch/x86/context.c - Context Switching Implementation
 *
 * Low-level CPU context management for task switching.
 *
 * When a task is first created, context_init_for_task() sets up a context
 * that points to the task's entry point. When the task is switched to,
 * the CPU registers are restored from this context, causing execution to
 * begin at the entry point.
 *
 * For actual context switching during interrupts, the low-level assembly
 * (context_switch_asm) saves current registers and loads next task's registers.
 */

#include "kernel/context.h"
#include "kernel/task.h"
#include "serial.h"

/**
 * Initialize CPU context for a new kernel task.
 *
 * Sets up the context structure so that when this task is resumed,
 * execution will jump to entry_point with ESP pointing to stack_top.
 *
 * Layout:
 * - EIP points to entry_point
 * - ESP points to stack_top
 * - EBP points to stack_top (frame pointer = stack pointer initially)
 * - Other registers zeroed or uninitialized
 * - EFLAGS has IF set (interrupts enabled)
 */
void context_init_for_task(task_t *task, uint32_t entry, uint32_t stack_top) {
    if (!task) {
        serial_puts("[CONTEXT] context_init_for_task: NULL task\n");
        return;
    }
    
    /* Zero the context */
    __builtin_memset(&task->context, 0, sizeof(cpu_context_t));
    
    /* Set entry point and stack */
    task->context.eip = entry;      /* Jump here when task starts */
    task->context.esp = stack_top;  /* Stack pointer */
    task->context.ebp = stack_top;  /* Frame pointer */
    
    /* Set flags: interrupts enabled (IF bit = 1), carry clear */
    task->context.eflags = 0x00000202;  /* IF=1, reserved bit 1 = 1 */
    
    /* CR3 will be set later for user tasks (per-process page directories) */
    task->context.cr3 = 0;  /* Use current kernel page directory */
    
    serial_printf("[CONTEXT] Task %u context initialized: entry=%p, stack=%p\n",
                  task->pid, (void *)entry, (void *)stack_top);
}

/**
 * Save current CPU registers to a context structure.
 *
 * This is a placeholder - actual implementation would need inline assembly
 * to read current CPU state. Typically called from interrupt handler
 * immediately after interrupt frame is created.
 */
void context_save(cpu_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    /* TODO: Use inline assembly to save current registers:
     *   movl %eax, offset_eax(%rdi)
     *   movl %ebx, offset_ebx(%rdi)
     *   ... etc for all registers
     *   pushf / popl for EFLAGS
     *   movl %cr3, %eax / movl %eax, offset_cr3(%rdi)
     */
}

/**
 * Restore CPU registers from a context structure.
 *
 * This is a placeholder - actual implementation would need inline assembly
 * to write to CPU registers. Typically called when resuming a task.
 */
void context_restore(cpu_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    /* TODO: Use inline assembly to restore registers from ctx:
     *   movl offset_eax(%rdi), %eax
     *   movl offset_ebx(%rdi), %ebx
     *   ... etc
     *   movl offset_cr3(%rdi), %eax
     *   movl %eax, %cr3  (flushes TLB)
     */
}
