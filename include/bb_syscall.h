#ifndef BYTEBANDIT_SYSCALL_API_H
#define BYTEBANDIT_SYSCALL_API_H

#include <stdint.h>
#include "bb_api.h"

#define BB_SYS_EXIT    1
#define BB_SYS_READ    3
#define BB_SYS_WRITE   4
#define BB_SYS_OPEN    5
#define BB_SYS_CLOSE   6
#define BB_SYS_UNLINK  10
#define BB_SYS_MKDIR   39
#define BB_SYS_GETPID  20
#define BB_SYS_UPTIME  100
#define BB_SYS_SLEEP   101
#define BB_SYS_GETINFO 102
#define BB_SYS_SPAWN   110
#define BB_SYS_WAIT    111
#define BB_SYS_REG_QUERY 120
#define BB_SYS_REG_SET   121

static inline int32_t bb_syscall0(uint32_t number) {
    int32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return result;
}

static inline int32_t bb_syscall1(uint32_t number, uint32_t arg0) {
    int32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result)
                      : "a"(number), "b"(arg0) : "memory");
    return result;
}

static inline int32_t bb_syscall3(uint32_t number, uint32_t arg0,
                                  uint32_t arg1, uint32_t arg2) {
    int32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result)
                      : "a"(number), "b"(arg0), "c"(arg1), "d"(arg2)
                      : "memory");
    return result;
}

static inline int32_t bb_read(bb_fd_t fd, void *buffer, uint32_t length) {
    return bb_syscall3(BB_SYS_READ, (uint32_t)fd, (uint32_t)buffer, length);
}

static inline int32_t bb_write(int fd, const void *buffer, uint32_t length) {
    return bb_syscall3(BB_SYS_WRITE, (uint32_t)fd, (uint32_t)buffer, length);
}

static inline int32_t bb_close(bb_fd_t fd) {
    return bb_syscall1(BB_SYS_CLOSE, (uint32_t)fd);
}

static inline int32_t bb_open(const char *path, uint32_t flags,
                              uint32_t mode) {
    return bb_syscall3(BB_SYS_OPEN, (uint32_t)path, flags, mode);
}

static inline int32_t bb_mkdir(const char *path) {
    return bb_syscall1(BB_SYS_MKDIR, (uint32_t)path);
}

static inline int32_t bb_unlink(const char *path) {
    return bb_syscall1(BB_SYS_UNLINK, (uint32_t)path);
}

static inline bb_handle_t bb_spawn(const char *path, const char *const *argv) {
    return (bb_handle_t)bb_syscall3(BB_SYS_SPAWN, (uint32_t)path,
                                    (uint32_t)argv, 0);
}

static inline int32_t bb_wait(bb_handle_t process, int32_t *status) {
    return bb_syscall3(BB_SYS_WAIT, process, (uint32_t)status, 0);
}

static inline int32_t bb_registry_query(const char *path, char *buffer,
                                        uint32_t length) {
    return bb_syscall3(BB_SYS_REG_QUERY, (uint32_t)path,
                       (uint32_t)buffer, length);
}

static inline int32_t bb_registry_set(const char *path, const char *value) {
    return bb_syscall3(BB_SYS_REG_SET, (uint32_t)path,
                       (uint32_t)value, 0);
}

static inline uint32_t bb_getpid(void) {
    return (uint32_t)bb_syscall0(BB_SYS_GETPID);
}

static inline uint32_t bb_uptime_ticks(void) {
    return (uint32_t)bb_syscall0(BB_SYS_UPTIME);
}

static inline int32_t bb_sleep_ms(uint32_t milliseconds) {
    return bb_syscall1(BB_SYS_SLEEP, milliseconds);
}

static inline int32_t bb_get_system_info(bb_system_info_t *info) {
    return bb_syscall1(BB_SYS_GETINFO, (uint32_t)info);
}

static inline void bb_exit(int status) {
    (void)bb_syscall1(BB_SYS_EXIT, (uint32_t)status);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

#endif
