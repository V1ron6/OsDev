/*
 * kernel/syscall.h - System Call Definitions
 *
 * Syscalls are the interface between user-space programs and the kernel.
 * User programs request kernel services via software interrupt 0x80.
 *
 * Syscall Convention (x86 32-bit):
 * - EAX: Syscall number
 * - EBX, ECX, EDX, ESI, EDI, EBP: Arguments (up to 6)
 * - Return value: EAX
 *
 * Kernel processes the request and returns via IRET.
 */

#ifndef _BYTEBANDIT_SYSCALL_H
#define _BYTEBANDIT_SYSCALL_H

#include "types.h"

/* Syscall numbers (matching Linux i386 ABI for reference) */
#define SYS_EXIT        1   /* Terminate process */
#define SYS_FORK        2   /* Create child process */
#define SYS_READ        3   /* Read from file descriptor */
#define SYS_WRITE       4   /* Write to file descriptor */
#define SYS_OPEN        5   /* Open file */
#define SYS_CLOSE       6   /* Close file descriptor */
#define SYS_WAITPID     7   /* Wait for child process */
#define SYS_CREAT       8   /* Create file */
#define SYS_LINK        9   /* Create link */
#define SYS_UNLINK      10  /* Remove link */
#define SYS_EXECVE      11  /* Execute program */
#define SYS_CHDIR       12  /* Change directory */
#define SYS_TIME        13  /* Get time */
#define SYS_MKNOD       14  /* Create device file */
#define SYS_CHMOD       15  /* Change file mode */
#define SYS_CHOWN       16  /* Change file owner */
#define SYS_LSEEK       19  /* Seek in file */
#define SYS_GETPID      20  /* Get process ID */
#define SYS_MOUNT       21  /* Mount filesystem */
#define SYS_UMOUNT      22  /* Unmount filesystem */
#define SYS_SETUID      23  /* Set user ID */
#define SYS_GETUID      24  /* Get user ID */
#define SYS_STIME       25  /* Set system time */
#define SYS_PTRACE      26  /* Process tracing */
#define SYS_ALARM       27  /* Schedule alarm */
#define SYS_FSTAT       28  /* Get file status */
#define SYS_PAUSE       29  /* Suspend process */
#define SYS_UTIME       30  /* Change file times */
#define SYS_ACCESS      33  /* Check file accessibility */
#define SYS_NICE        34  /* Change process priority */
#define SYS_SYNC        36  /* Flush filesystem caches */
#define SYS_KILL        37  /* Send signal to process */
#define SYS_RENAME      38  /* Rename file */
#define SYS_MKDIR       39  /* Create directory */
#define SYS_RMDIR       40  /* Remove directory */
#define SYS_DUP         41  /* Duplicate file descriptor */
#define SYS_PIPE        42  /* Create pipe */
#define SYS_TIMES       43  /* Get process times */
#define SYS_BRK         45  /* Change data segment size */
#define SYS_SIGNAL      48  /* Set signal handler (deprecated) */
#define SYS_GETPGRP     65  /* Get process group */
#define SYS_SETSID      66  /* Create new session */
#define SYS_SETREUID    70  /* Set real and effective user IDs */
#define SYS_SETREGID    71  /* Set real and effective group IDs */

/* Initial syscalls we implement */
#define SYSCALL_MAX 100

/**
 * Initialize syscall system.
 *
 * Installs int 0x80 handler in IDT for syscall processing.
 * Must be called after IDT is initialized.
 */
void syscall_init(void);

/**
 * Syscall handler dispatcher (called from assembly).
 *
 * @param int_no      Interrupt number (should be 0x80)
 * @param syscall_no  Syscall number (from user EAX)
 * @param arg0        First argument (from EBX)
 * @param arg1        Second argument (from ECX)
 * @param arg2        Third argument (from EDX)
 * @param arg3        Fourth argument (from ESI)
 * @param arg4        Fifth argument (from EDI)
 * @param arg5        Sixth argument (from EBP)
 *
 * @return Result to be placed in user EAX
 *
 * This function is called from syscall_entry.asm when a user program
 * executes "int 0x80". The assembly handler pushes arguments and calls
 * this dispatcher, which routes to the appropriate syscall handler.
 */
uint32_t syscall_handler(uint32_t int_no, uint32_t syscall_no,
                         uint32_t arg0, uint32_t arg1, uint32_t arg2,
                         uint32_t arg3, uint32_t arg4, uint32_t arg5);

/* Individual syscall implementations */

/**
 * SYS_EXIT - Terminate current process.
 *
 * @param exit_code Exit code (0=success, non-zero=error)
 * @return Never returns (process terminates)
 */
void syscall_exit(int32_t exit_code);

/**
 * SYS_WRITE - Write to file descriptor.
 *
 * @param fd    File descriptor (1=stdout, 2=stderr, etc.)
 * @param buf   User buffer address
 * @param count Number of bytes to write
 * @return Number of bytes written (or -1 on error)
 */
int32_t syscall_write(int32_t fd, const void *buf, uint32_t count);

/**
 * SYS_READ - Read from file descriptor.
 *
 * @param fd    File descriptor (0=stdin, etc.)
 * @param buf   User buffer address
 * @param count Number of bytes to read
 * @return Number of bytes read (or -1 on error)
 */
int32_t syscall_read(int32_t fd, void *buf, uint32_t count);

/**
 * SYS_GETPID - Get process ID.
 *
 * @return Process ID of current process
 */
uint32_t syscall_getpid(void);

/**
 * SYS_GETUID - Get user ID.
 *
 * @return User ID of current process
 */
uint32_t syscall_getuid(void);

/**
 * SYS_KILL - Send signal to process.
 *
 * @param pid    Target process ID
 * @param sig    Signal number
 * @return 0 on success, -1 on error
 */
int32_t syscall_kill(uint32_t pid, uint32_t sig);

#endif /* _BYTEBANDIT_SYSCALL_H */
