/*
 * kernel/task.c - Task/Process Implementation
 *
 * Implements process abstractions (structure definitions, not scheduling yet).
 */

#include "kernel/task.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/constants.h"
#include "serial.h"
#include <string.h>

/* Global task counter for PID assignment */
static uint32_t next_pid = 1;

/* =========================================================================
 * TASK LIFECYCLE
 * ========================================================================= */

task_t *task_create(const char *name, uint32_t entry_point, uint32_t flags) {
    /* Allocate task structure from kernel heap */
    task_t *task = (task_t *)kcalloc(sizeof(task_t));
    if (task == NULL) {
        serial_puts("[ERROR] task_create: Out of memory\n");
        return NULL;
    }
    
    /* Initialize basic fields */
    task->pid = next_pid++;
    task->ppid = 0;  /* TODO: parent process ID */
    task->state = TASK_STATE_CREATED;
    task->priority = 20;  /* Default medium priority */
    task->flags = flags;
    task->entry_point = entry_point;
    task->cpu_time = 0;
    task->created_time = 0;  /* TODO: get current tick */
    
    /* Copy task name */
    strncpy(task->name, name ? name : "unnamed", sizeof(task->name) - 1);
    task->name[sizeof(task->name) - 1] = '\0';
    
    /* Initialize CPU context for new task
     * When scheduled, CPU will jump to entry_point
     */
    memset(&task->context, 0, sizeof(cpu_context_t));
    task->context.eip = entry_point;
    task->context.eflags = 0x00000202;  /* IF (interrupts enabled) + reserved bit */
    
    /* Allocate kernel stack (2 pages = 8KB for now) */
    uint32_t stack_frame1 = pmm_alloc_frame();
    uint32_t stack_frame2 = pmm_alloc_frame();
    
    if (stack_frame1 == 0 || stack_frame2 == 0) {
        serial_puts("[ERROR] task_create: Out of memory for stack\n");
        kfree(task);
        return NULL;
    }
    
    /* Map stack in kernel space (temporary location)
     * TODO: Use proper kernel memory region
     */
    uint32_t stack_virt = 0x00200000 + (task->pid * 8192);
    paging_map_page(stack_virt, stack_frame1, PAGE_KERNEL);
    paging_map_page(stack_virt + PAGE_SIZE, stack_frame2, PAGE_KERNEL);
    
    task->kernel_stack = stack_virt + (2 * PAGE_SIZE);  /* Top of stack */
    task->context.esp = task->kernel_stack;
    task->context.ebp = task->kernel_stack;
    
    /* For kernel tasks, use kernel page directory (identity mapping)
     * For user tasks, create separate page directory (not yet implemented)
     */
    if (flags & TASK_FLAG_USER_MODE) {
        /* TODO: Create user-mode page directory */
        task->page_dir = paging_get_page_dir();  /* Fallback to kernel */
    } else {
        task->page_dir = paging_get_page_dir();
    }
    
    serial_printf("[TASK] Created task %u: '%s' at 0x%x\n", 
                  task->pid, task->name, entry_point);
    
    return task;
}

void task_destroy(task_t *task) {
    if (task == NULL) {
        return;
    }
    
    serial_printf("[TASK] Destroying task %u: '%s'\n", task->pid, task->name);
    
    /* TODO: Free page directory */
    /* TODO: Free kernel stack */
    
    kfree(task);
}

void task_exit(task_t *task, int exit_code) {
    if (task == NULL) {
        return;
    }
    
    serial_printf("[TASK] Task %u exiting with code %d\n", task->pid, exit_code);
    
    task->state = TASK_STATE_DYING;
    /* TODO: Cleanup */
}

void task_dump(task_t *task) {
    if (task == NULL) {
        serial_puts("[TASK] NULL task\n");
        return;
    }
    
    serial_puts("\n");
    serial_puts("╔════════════════════════════════════════════════════╗\n");
    serial_puts("║                   TASK DUMP                        ║\n");
    serial_puts("╚════════════════════════════════════════════════════╝\n");
    
    serial_printf("PID:           %u\n", task->pid);
    serial_printf("Name:          %s\n", task->name);
    serial_printf("Parent PID:    %u\n", task->ppid);
    serial_printf("State:         %u\n", task->state);
    serial_printf("Priority:      %u\n", task->priority);
    serial_printf("Flags:         0x%x\n", task->flags);
    
    serial_puts("\nCPU Context:\n");
    serial_printf("  EAX:         0x%x\n", task->context.eax);
    serial_printf("  EBX:         0x%x\n", task->context.ebx);
    serial_printf("  ECX:         0x%x\n", task->context.ecx);
    serial_printf("  EDX:         0x%x\n", task->context.edx);
    serial_printf("  ESI:         0x%x\n", task->context.esi);
    serial_printf("  EDI:         0x%x\n", task->context.edi);
    serial_printf("  EBP:         0x%x\n", task->context.ebp);
    serial_printf("  ESP:         0x%x\n", task->context.esp);
    serial_printf("  EIP:         0x%x\n", task->context.eip);
    serial_printf("  EFLAGS:      0x%x\n", task->context.eflags);
    serial_printf("  CR3:         0x%x\n", task->context.cr3);
    
    serial_printf("\nKernel Stack: 0x%x\n", task->kernel_stack);
    serial_printf("Entry Point:  0x%x\n", task->entry_point);
    
    serial_puts("\n════════════════════════════════════════════════════\n\n");
}

void task_list_dump(void) {
    serial_puts("[TODO] task_list_dump() - task list not yet implemented\n");
}
