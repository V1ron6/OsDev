/*
 * kernel/scheduler.h - Task Scheduler
 *
 * Simple round-robin scheduler managing task execution.
 *
 * The scheduler:
 * - Maintains a ready queue of tasks
 * - Selects the next task to run on each timer interrupt
 * - Tracks CPU time per task
 * - Supports blocking/unblocking tasks
 */

#ifndef _BYTEBANDIT_SCHEDULER_H
#define _BYTEBANDIT_SCHEDULER_H

#include "types.h"
#include "kernel/task.h"

#define MAX_TASKS  256
#define DEFAULT_TIME_SLICE  10  /* ms per task (at 100Hz: 1 tick) */

/**
 * Initialize the scheduler.
 *
 * Creates empty ready queue, allocates idle task.
 * Must be called after memory subsystems are initialized.
 */
void scheduler_init(void);

/**
 * Add a task to the ready queue.
 *
 * @param task Task to add (must be in READY state)
 *
 * Task will be scheduled for execution on next timer interrupt.
 */
void scheduler_enqueue(task_t *task);

/**
 * Remove and return next task from ready queue.
 *
 * @return Next task to execute, or NULL if queue empty
 *
 * Returns tasks in FIFO order (round-robin).
 */
task_t *scheduler_dequeue(void);

/**
 * Get the currently executing task.
 *
 * @return Pointer to current task
 */
task_t *scheduler_current(void);

/**
 * Set the currently executing task.
 *
 * @param task New current task
 */
void scheduler_set_current(task_t *task);

/**
 * Select the next task to run.
 *
 * @return Task to run next (never NULL - returns idle task if queue empty)
 *
 * Called on each timer interrupt to decide which task gets the CPU next.
 */
task_t *scheduler_select_next(void);

/**
 * Block a task (remove from ready queue).
 *
 * @param task Task to block
 * @param reason Reason for blocking (for debugging)
 *
 * Task will not be scheduled until unblocked.
 */
void scheduler_block(task_t *task, const char *reason);

/**
 * Unblock a task (add to ready queue).
 *
 * @param task Task to unblock
 *
 * Task will be scheduled on next timer interrupt.
 */
void scheduler_unblock(task_t *task);

/**
 * Get the idle task (runs when no other tasks are ready).
 *
 * @return Pointer to idle task
 */
task_t *scheduler_get_idle_task(void);

/**
 * Dump scheduler state for debugging.
 *
 * Prints ready queue size, current task, etc.
 */
void scheduler_dump_state(void);

/**
 * Account for CPU time used by current task.
 *
 * Called on each timer interrupt to track task CPU usage.
 */
void scheduler_account_time(void);

#endif /* _BYTEBANDIT_SCHEDULER_H */
