/*
 * arch/x86/context.c - Context Switching Implementation
 *
 * Stubs for context switching functions (to be implemented).
 * For now, these functions are not called - included for architecture completion.
 */

#include "arch/x86/context.h"
#include "serial.h"

void context_switch(task_t *from, task_t *to) {
    (void)from;
    (void)to;
    serial_puts("[TODO] context_switch() not implemented\n");
    /* Stub - would require assembly to actually switch */
}

void context_save(cpu_context_t *ctx) {
    (void)ctx;
    serial_puts("[TODO] context_save() not implemented\n");
}

void context_restore(cpu_context_t *ctx) {
    (void)ctx;
    serial_puts("[TODO] context_restore() not implemented\n");
}

void context_init_for_task(task_t *task, uint32_t entry, uint32_t stack) {
    (void)task;
    (void)entry;
    (void)stack;
    serial_puts("[TODO] context_init_for_task() not implemented\n");
}
