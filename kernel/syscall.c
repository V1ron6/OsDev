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
#include "kernel/scheduler.h"
#include "arch/x86/pit.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "mm/constants.h"
#include "bb_api.h"
#include "fs/vfs.h"
#include "fs/registry.h"
#include <string.h>

static bool syscall_user_buffer_valid(const void *buffer, uint32_t count) {
    uint32_t start = (uint32_t)buffer;
    uint32_t end;

    if (buffer == NULL || count == 0 || start < 0x1000 ||
        start >= 0xC0000000 || count > 0xC0000000 - start) {
        return false;
    }

    end = start + count - 1;
    for (uint32_t page = start & 0xFFFFF000; page <= end; page += 0x1000) {
        uint32_t flags = paging_get_mapping_flags(page);
        if ((flags & (PAGE_PRESENT | PAGE_USER)) !=
            (PAGE_PRESENT | PAGE_USER)) {
            return false;
        }
        if (page > 0xFFFFEFFF) {
            break;
        }
    }

    return true;
}

static bool syscall_copy_user_string(const char *user_string, char *kernel_string,
                                     uint32_t capacity) {
    if (!syscall_user_buffer_valid(user_string, capacity)) {
        return false;
    }
    for (uint32_t i = 0; i < capacity; i++) {
        kernel_string[i] = user_string[i];
        if (kernel_string[i] == '\0') {
            return true;
        }
    }
    return false;
}

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

        case SYS_OPEN:
            result = (uint32_t)syscall_open((const char *)arg0, arg1);
            break;

        case SYS_CLOSE:
            result = (uint32_t)syscall_close((int32_t)arg0);
            break;

        case SYS_MKDIR:
            result = (uint32_t)syscall_mkdir((const char *)arg0);
            break;

        case SYS_UNLINK:
            result = (uint32_t)syscall_unlink((const char *)arg0);
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

        case BB_SYS_UPTIME:
            result = syscall_uptime();
            break;

        case BB_SYS_SLEEP:
            result = (uint32_t)syscall_sleep(arg0);
            break;

        case BB_SYS_GETINFO:
            result = (uint32_t)syscall_get_system_info((bb_system_info_t *)arg0);
            break;

        case BB_SYS_REG_QUERY:
            result = (uint32_t)syscall_registry_query((const char *)arg0,
                                                       (char *)arg1, arg2);
            break;

        case BB_SYS_REG_SET:
            result = (uint32_t)syscall_registry_set((const char *)arg0,
                                                     (const char *)arg1);
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
    task_t *current = scheduler_current();
    if (current) {
        current->state = TASK_STATE_DYING;
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
    if (!syscall_user_buffer_valid(buf, count)) {
        return -1;  /* EINVAL */
    }

    if (fd >= 3) {
        const char *data = (const char *)buf;
        char kernel_buffer[128];
        uint32_t offset = 0;
        while (offset < count) {
            uint32_t chunk = count - offset;
            if (chunk > sizeof(kernel_buffer)) {
                chunk = sizeof(kernel_buffer);
            }
            memcpy(kernel_buffer, data + offset, chunk);
            int32_t written = vfs_write(fd, kernel_buffer, chunk);
            if (written < 0) {
                return written;
            }
            offset += (uint32_t)written;
            if ((uint32_t)written < chunk) {
                break;
            }
        }
        return (int32_t)offset;
    }

    if (fd != 1 && fd != 2) {
        return -1;
    }
    
    /* Copy data to kernel-controlled buffer for safety
     *
     * This prevents user code from modifying buffer while we read it.
     */
    const char *data = (const char *)buf;
    char kernel_buffer[128];
    uint32_t offset = 0;

    while (offset < count) {
        uint32_t chunk = count - offset;
        if (chunk > sizeof(kernel_buffer)) {
            chunk = sizeof(kernel_buffer);
        }
        memcpy(kernel_buffer, data + offset, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            vga_putc(kernel_buffer[i]);
            serial_putc(kernel_buffer[i]);
        }
        offset += chunk;
    }

    return (int32_t)count;
}

int32_t syscall_read(int32_t fd, void *buf, uint32_t count) {
    uint8_t *output = (uint8_t *)buf;
    uint32_t read_count = 0;

    if (fd >= 3) {
        if (!syscall_user_buffer_valid(buf, count)) {
            return BB_STATUS_INVALID;
        }
        return vfs_read(fd, buf, count);
    }
    if (fd != BB_STDIN_FD || !syscall_user_buffer_valid(buf, count)) {
        return BB_STATUS_INVALID;
    }

    while (read_count < count && serial_received()) {
        output[read_count++] = (uint8_t)serial_getc();
    }

    return read_count == 0 ? BB_STATUS_AGAIN : (int32_t)read_count;
}

int32_t syscall_open(const char *path, uint32_t flags) {
    char kernel_path[64];
    if (!syscall_copy_user_string(path, kernel_path, sizeof(kernel_path))) {
        return BB_STATUS_INVALID;
    }
    return vfs_open(kernel_path, flags);
}

int32_t syscall_close(int32_t fd) {
    return vfs_close(fd);
}

int32_t syscall_mkdir(const char *path) {
    char kernel_path[64];
    if (!syscall_copy_user_string(path, kernel_path, sizeof(kernel_path))) {
        return BB_STATUS_INVALID;
    }
    return vfs_mkdir(kernel_path);
}

int32_t syscall_unlink(const char *path) {
    char kernel_path[64];
    if (!syscall_copy_user_string(path, kernel_path, sizeof(kernel_path))) {
        return BB_STATUS_INVALID;
    }
    return vfs_unlink(kernel_path);
}

uint32_t syscall_getpid(void) {
    task_t *current_task = scheduler_current();
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
    task_t *target = task_find(pid);
    if (target == NULL || sig == 0 || sig > 64) {
        return -1;
    }

    serial_printf("[SYSCALL] kill(%u, %u)\n", pid, sig);
    if (sig == 9 || sig == 15) {
        task_exit(target, -(int32_t)sig);
    }
    return 0;
}

uint32_t syscall_uptime(void) {
    return pit_get_ticks();
}

int32_t syscall_sleep(uint32_t milliseconds) {
    pit_sleep_ms(milliseconds);
    return BB_STATUS_OK;
}

int32_t syscall_get_system_info(bb_system_info_t *info) {
    bb_system_info_t kernel_info;

    if (!syscall_user_buffer_valid(info, sizeof(kernel_info)) ||
        bb_api_get_info(&kernel_info) != BB_STATUS_OK) {
        return BB_STATUS_INVALID;
    }

    memcpy(info, &kernel_info, sizeof(kernel_info));
    return BB_STATUS_OK;
}

int32_t syscall_registry_query(const char *path, char *buffer, uint32_t length) {
    char kernel_path[64];
    if (!syscall_copy_user_string(path, kernel_path, sizeof(kernel_path)) ||
        !syscall_user_buffer_valid(buffer, length)) {
        return BB_STATUS_INVALID;
    }
    return registry_query(kernel_path, buffer, length);
}

int32_t syscall_registry_set(const char *path, const char *value) {
    char kernel_path[64];
    char kernel_value[128];
    if (!syscall_copy_user_string(path, kernel_path, sizeof(kernel_path)) ||
        !syscall_copy_user_string(value, kernel_value, sizeof(kernel_value))) {
        return BB_STATUS_INVALID;
    }
    return registry_set(kernel_path, kernel_value);
}
