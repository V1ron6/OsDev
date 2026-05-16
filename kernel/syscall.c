/*
 * kernel/syscall.c - System Call Implementation
 *
 * Implements syscall handler dispatch and initial syscall functions.
 */

#include "syscall.h"
#include "idt.h"
#include "x86.h"
#include "serial.h"
#include "vga.h"
#include "kernel/task.h"
#include "mm/heap.h"

/* Current process (stub - will be replaced by scheduler) */
static task_t *current_task = NULL;

void syscall_init(void) {
    serial_puts("[SYSCALL] Initializing syscall system\n");
    
    /* Install int 0x80 handler in IDT
     *
     * Vector 0x80 is reserved for software interrupts (syscalls).
     * This handler will be called when user code executes "int 0x80".
     */
    extern void syscall_entry(void);  /* Assembly function */
    idt_set_gate(0x80, (uint32_t)syscall_entry, 
                 IDT_GATE_INTR_32,  /* 32-bit interrupt gate */
                 3);                 /* DPL=3 for user mode access */
    
    serial_printf("[SYSCALL] Installed handler at vector 0x80 (ring 3 accessible)\n");
}

uint32_t syscall_handler(uint32_t int_no, uint32_t syscall_no,
                         uint32_t arg0, uint32_t arg1, uint32_t arg2,
                         uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    /* Dispatch to appropriate syscall handler
     *
     * Arguments from user code are in specific registers:
     * - EAX: syscall number
     * - EBX: arg0
     * - ECX: arg1
     * - EDX: arg2
     * - ESI: arg3
     * - EDI: arg4
     * - EBP: arg5
     *
     * Return value goes in EAX.
     */
    
    uint32_t result = 0;
    
    switch (syscall_no) {
        case SYS_EXIT:
            syscall_exit((int32_t)arg0);
            /* Never returns */
            break;
            
        case SYS_WRITE:
            result = (uint32_t)syscall_write((int32_t)arg0, (const void *)arg1, arg2);
            break;
            
        case SYS_READ:
            result = (uint32_t)syscall_read((int32_t)arg0, (void *)arg1, arg2);
            break;
            
        case SYS_GETPID:
            result = syscall_getpid();
            break;
            
        case SYS_GETUID:
            result = syscall_getuid();
            break;
            
        case SYS_KILL:
            result = (uint32_t)syscall_kill(arg0, arg1);
            break;
            
        default:
            serial_printf("[SYSCALL] Unknown syscall: %d\n", syscall_no);
            result = (uint32_t)-1;  /* ENOSYS */
            break;
    }
    
    return result;
}

void syscall_exit(int32_t exit_code) {
    serial_printf("[SYSCALL] exit(%d) - Process terminating\n", exit_code);
    
    /* Mark current task as dying */
    if (current_task) {
        current_task->state = TASK_STATE_DYING;
    }
    
    /* Halt the system (in future, will switch to next task)
     *
     * TODO: When scheduler is implemented, switch to next task
     * instead of halting.
     */
    while (1) {
        halt();
    }
}

int32_t syscall_write(int32_t fd, const void *buf, uint32_t count) {
    /* Validate user-space buffer
     *
     * In a real system, we would:
     * 1. Check if buf is in user-space (below 0xC0000000)
     * 2. Check if current task has access to this memory
     * 3. Verify all bytes of buf are accessible
     *
     * For now, we do a basic sanity check.
     */
    if (!buf || count == 0) {
        return -1;  /* EINVAL */
    }
    
    /* Copy data to kernel-controlled buffer for safety
     *
     * This prevents user code from modifying buffer while we read it.
     */
    const char *data = (const char *)buf;
    
    switch (fd) {
        case 1:  /* stdout */
            for (uint32_t i = 0; i < count; i++) {
                vga_putc(data[i]);
                serial_putc(data[i]);
            }
            return (int32_t)count;
            
        case 2:  /* stderr */
            for (uint32_t i = 0; i < count; i++) {
                vga_putc(data[i]);
                serial_putc(data[i]);
            }
            return (int32_t)count;
            
        default:
            serial_printf("[SYSCALL] write: Invalid FD %d\n", fd);
            return -1;  /* EBADF */
    }
}

int32_t syscall_read(int32_t fd, void *buf, uint32_t count) {
    /* TODO: Implement read from various file descriptors
     *
     * For now, return error since we have no input system.
     */
    (void)fd;
    (void)buf;
    (void)count;
    
    return -1;  /* ENOSYS */
}

uint32_t syscall_getpid(void) {
    if (current_task) {
        return current_task->pid;
    }
    return 0;  /* Kernel process ID */
}

uint32_t syscall_getuid(void) {
    /* TODO: Implement user ID tracking
     *
     * For now, all processes are "root" (UID 0).
     */
    return 0;
}

int32_t syscall_kill(uint32_t pid, uint32_t sig) {
    /* TODO: Implement signal delivery
     *
     * For now, just log and return error.
     */
    serial_printf("[SYSCALL] kill(%u, %u) - Not implemented\n", pid, sig);
    return -1;  /* ENOSYS */
}
