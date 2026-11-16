#ifndef _BYTEBANDIT_EXCEPTIONS_H
#define _BYTEBANDIT_EXCEPTIONS_H

#include "types.h"
#include "kernel/task.h"

/*
 * exceptions.h - CPU Exception Handling Interface
 *
 * Provides infrastructure for catching and handling CPU exceptions
 * (interrupts 0-31 per x86 specification).
 */

/* CPU exception handler - called by ISR stubs
 *
 * @param int_no: exception number (0-31)
 * @param err_code: error code (0 if not provided by CPU)
 *
 * User-mode faults terminate the current task and return a replacement
 * context; kernel-mode faults still panic with diagnostic info.
 */
typedef struct {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t int_no;
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t useresp;
    uint32_t userss;
} exception_frame_t;

cpu_context_t *exception_handler(uint32_t int_no, uint32_t err_code,
                                 exception_frame_t *frame);

#endif
