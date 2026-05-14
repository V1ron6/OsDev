/*
 * kernel/task.h - Process/Task Structures
 *
 * Defines task/process abstractions for ByteBandit OS.
 * Currently: Structure definitions only
 * Future: Task scheduling, process creation, multitasking
 *
 * A task (or process) represents an independent execution context.
 * Each task has:
 * - CPU registers (for context switching)
 * - Memory (page directory for virtual address space)
 * - Stack (for function calls)
 * - State (running, ready, blocked, etc.)
 */

#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * TASK STATE ENUMERATION
 * ========================================================================= */

typedef enum {
    TASK_STATE_CREATED  = 0,  /* Task created but not yet run */
    TASK_STATE_READY    = 1,  /* Ready to run, waiting for CPU */
    TASK_STATE_RUNNING  = 2,  /* Currently executing */
    TASK_STATE_BLOCKED  = 3,  /* Waiting for resource (I/O, semaphore, etc.) */
    TASK_STATE_DYING    = 4,  /* Exiting, cleanup in progress */
    TASK_STATE_DEAD     = 5,  /* Task terminated */
} task_state_t;

/* =========================================================================
 * CPU CONTEXT - SAVED REGISTERS
 * ========================================================================= */

/**
 * struct cpu_context
 *
 * Registers saved during context switch.
 * Used to restore execution state when switching to a task.
 *
 * x86 register usage (System V ABI):
 *   EAX, ECX, EDX - Scratch (caller-saved)
 *   EBX, ESP, EBP, ESI, EDI - Preserved (callee-saved)
 *   Flags (EFLAGS) - Interrupt enable, etc.
 *   EIP - Instruction pointer (next instruction)
 */
typedef struct {
    uint32_t eax;      /* General purpose */
    uint32_t ebx;      /* Preserved */
    uint32_t ecx;      /* General purpose */
    uint32_t edx;      /* General purpose */
    uint32_t esi;      /* Preserved */
    uint32_t edi;      /* Preserved */
    uint32_t esp;      /* Stack pointer */
    uint32_t ebp;      /* Base pointer (frame pointer) */
    uint32_t eip;      /* Instruction pointer (next to execute) */
    uint32_t eflags;   /* CPU flags (IF, CF, ZF, etc.) */
    uint32_t cr3;      /* Page directory (if multitasking) */
} cpu_context_t;

/* =========================================================================
 * TASK STRUCTURE
 * ========================================================================= */

/**
 * struct task
 *
 * Represents a single process/task in the system.
 *
 * Fields:
 *   pid            - Process ID (unique identifier)
 *   ppid           - Parent process ID (for process tree)
 *   state          - Current state (running, ready, blocked, etc.)
 *   priority       - Scheduling priority (0=low, 31=high)
 *   context        - Saved CPU context for context switching
 *   page_dir       - Physical address of page directory (CR3 value)
 *   kernel_stack   - Top of kernel stack for this task
 *   entry_point    - Address where task starts executing
 *   flags          - Task flags (user/kernel mode, etc.)
 *   cpu_time       - Total CPU time used (in timer ticks)
 *   created_time   - When task was created (in timer ticks)
 *   name           - Human-readable task name
 */
typedef struct {
    uint32_t pid;              /* Process ID */
    uint32_t ppid;             /* Parent process ID */
    task_state_t state;        /* Current state */
    uint32_t priority;         /* Scheduling priority */
    cpu_context_t context;     /* Saved CPU registers */
    uint32_t page_dir;         /* CR3 value (page directory address) */
    uint32_t kernel_stack;     /* Kernel stack pointer */
    uint32_t entry_point;      /* Entry point address */
    uint32_t flags;            /* Task flags */
    uint32_t cpu_time;         /* Total CPU time (ticks) */
    uint32_t created_time;     /* Creation time (ticks) */
    char name[32];             /* Task name for debugging */
} task_t;

/* Task flags */
#define TASK_FLAG_USER_MODE    0x0001  /* User mode (else kernel) */
#define TASK_FLAG_PRIVILEGED   0x0002  /* Has elevated privileges */

/* =========================================================================
 * TASK LIFECYCLE
 * ========================================================================= */

/**
 * task_create() - Create a new task
 *
 * Args:
 *   name        - Human-readable name (max 31 chars)
 *   entry_point - Address where task starts executing
 *   flags       - Task flags (user/kernel mode, etc.)
 *
 * Returns: Pointer to new task_t, or NULL if allocation failed
 *
 * Note: Task is created but not scheduled yet. Call task_spawn() to start.
 */
task_t *task_create(const char *name, uint32_t entry_point, uint32_t flags);

/**
 * task_destroy() - Destroy and free a task
 *
 * Args:
 *   task - Task to destroy
 *
 * Actions:
 * - Free page directory and memory
 * - Free kernel stack
 * - Free task structure itself
 */
void task_destroy(task_t *task);

/**
 * task_exit() - Mark task as exiting
 *
 * Called when a task finishes executing.
 * Initiates cleanup (but doesn't free yet).
 */
void task_exit(task_t *task, int exit_code);

/* =========================================================================
 * DEBUGGING
 * ========================================================================= */

/**
 * task_dump() - Print task information for debugging
 */
void task_dump(task_t *task);

/**
 * task_list_dump() - Print all tasks
 * (When task list is implemented)
 */
void task_list_dump(void);

#endif /* KERNEL_TASK_H */
