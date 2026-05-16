/*
 * kernel/scheduler.c - Task Scheduler Implementation
 *
 * Simple round-robin task scheduler.
 */

#include "kernel/scheduler.h"
#include "kernel/task.h"
#include "mm/heap.h"
#include "serial.h"
#include "vga.h"

/* Ready queue - circular buffer of task pointers */
static task_t *ready_queue[MAX_TASKS];
static uint32_t queue_head = 0;
static uint32_t queue_tail = 0;
static uint32_t queue_size = 0;

/* Currently executing task */
static task_t *current_task = NULL;

/* Idle task (runs when nothing else is ready) */
static task_t *idle_task = NULL;

/* Scheduler statistics */
static uint32_t total_ticks = 0;
static uint32_t task_switch_count = 0;

/**
 * Idle task entry point (never terminates).
 *
 * This task runs when no other tasks are ready.
 * In a real OS, it would perform power management or background tasks.
 */
static void _idle_task_entry(void) {
    while (1) {
        /* Halt CPU until next interrupt */
        __asm__ volatile("hlt");
    }
}

void scheduler_init(void) {
    serial_puts("[SCHEDULER] Initializing task scheduler\n");
    
    /* Initialize queue as empty */
    queue_head = 0;
    queue_tail = 0;
    queue_size = 0;
    
    /* Create idle task (runs when nothing else is ready) */
    idle_task = task_create("idle", (uint32_t)_idle_task_entry, 0);
    if (!idle_task) {
        serial_puts("[ERROR] Failed to create idle task\n");
        return;
    }
    
    idle_task->priority = 0;  /* Lowest priority */
    current_task = idle_task;
    
    serial_printf("[SCHEDULER] Ready queue created (max %d tasks)\n", MAX_TASKS);
    serial_printf("[SCHEDULER] Idle task created (PID %u)\n", idle_task->pid);
}

void scheduler_enqueue(task_t *task) {
    if (!task) {
        serial_puts("[SCHEDULER] enqueue: NULL task\n");
        return;
    }
    
    if (queue_size >= MAX_TASKS) {
        serial_puts("[SCHEDULER] Ready queue full, cannot enqueue task\n");
        return;
    }
    
    /* Add to tail of queue */
    ready_queue[queue_tail] = task;
    queue_tail = (queue_tail + 1) % MAX_TASKS;
    queue_size++;
    
    /* Update task state */
    task->state = TASK_STATE_READY;
}

task_t *scheduler_dequeue(void) {
    if (queue_size == 0) {
        return NULL;
    }
    
    /* Remove from head of queue */
    task_t *task = ready_queue[queue_head];
    queue_head = (queue_head + 1) % MAX_TASKS;
    queue_size--;
    
    return task;
}

task_t *scheduler_current(void) {
    return current_task;
}

void scheduler_set_current(task_t *task) {
    if (task) {
        current_task = task;
        task->state = TASK_STATE_RUNNING;
    }
}

task_t *scheduler_select_next(void) {
    /* Try to get next task from ready queue */
    task_t *next = scheduler_dequeue();
    
    /* If no ready tasks, use idle task */
    if (!next) {
        next = idle_task;
    }
    
    return next;
}

void scheduler_block(task_t *task, const char *reason) {
    if (!task) {
        return;
    }
    
    serial_printf("[SCHEDULER] Task %u (%s) blocked: %s\n",
                  task->pid, task->name, reason);
    
    task->state = TASK_STATE_BLOCKED;
    
    /* If blocking current task, we'll switch on next interrupt */
    if (task == current_task) {
        current_task = NULL;  /* Force selection of next task */
    }
}

void scheduler_unblock(task_t *task) {
    if (!task) {
        return;
    }
    
    serial_printf("[SCHEDULER] Task %u (%s) unblocked\n", task->pid, task->name);
    scheduler_enqueue(task);
}

task_t *scheduler_get_idle_task(void) {
    return idle_task;
}

void scheduler_dump_state(void) {
    serial_puts("\n=== Scheduler State ===\n");
    serial_printf("Total ticks: %u\n", total_ticks);
    serial_printf("Task switches: %u\n", task_switch_count);
    serial_printf("Ready queue size: %u\n", queue_size);
    
    if (current_task) {
        serial_printf("Current task: PID %u (%s), state: %u\n",
                      current_task->pid, current_task->name, current_task->state);
    } else {
        serial_puts("Current task: NONE (idle)\n");
    }
    
    serial_puts("Ready queue contents:\n");
    uint32_t idx = queue_head;
    for (uint32_t i = 0; i < queue_size; i++) {
        task_t *task = ready_queue[idx];
        serial_printf("  [%u] PID %u (%s)\n", i, task->pid, task->name);
        idx = (idx + 1) % MAX_TASKS;
    }
    serial_puts("\n");
}

void scheduler_account_time(void) {
    if (current_task) {
        current_task->cpu_time++;
    }
    total_ticks++;
}
