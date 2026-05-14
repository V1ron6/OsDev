/*
 * arch/x86/exceptions.c - CPU Exception Handler
 *
 * Common exception handler for all CPU exceptions (vectors 0-31).
 * Routes exception information to diagnostic output (serial + VGA).
 *
 * Exception messages and error codes:
 *   0: Divide by Zero
 *   1: Debug
 *   3: Breakpoint
 *   4: Overflow
 *   5: Bound Range Exceeded
 *   6: Invalid Opcode
 *   8: Double Fault
 *   11: Segment Not Present
 *   12: Stack Fault
 *   13: General Protection Fault
 *   14: Page Fault
 */

#include "isr.h"
#include "serial.h"
#include "panic.h"
#include "x86.h"
#include "types.h"

/* Exception names for diagnostic output */
static const char *exception_names[] = {
    "Divide by Zero",           /* 0 */
    "Debug",                    /* 1 */
    "NMI",                      /* 2 */
    "Breakpoint",               /* 3 */
    "Overflow",                 /* 4 */
    "Bound Range Exceeded",     /* 5 */
    "Invalid Opcode",           /* 6 */
    "Device Not Available",     /* 7 */
    "Double Fault",             /* 8 */
    "Coprocessor Segment",      /* 9 */
    "Invalid TSS",              /* 10 */
    "Segment Not Present",      /* 11 */
    "Stack Fault",              /* 12 */
    "General Protection",       /* 13 */
    "Page Fault",               /* 14 */
    "Reserved",                 /* 15 */
    "FPU Error",                /* 16 */
    "Alignment Check",          /* 17 */
    "Machine Check",            /* 18 */
    "SIMD FP Error",            /* 19 */
    "Virtualization",           /* 20 */
    "Reserved",                 /* 21 */
    "Reserved",                 /* 22 */
    "Reserved",                 /* 23 */
    "Reserved",                 /* 24 */
    "Reserved",                 /* 25 */
    "Reserved",                 /* 26 */
    "Reserved",                 /* 27 */
    "Reserved",                 /* 28 */
    "Reserved",                 /* 29 */
    "Reserved",                 /* 30 */
    "Reserved"                  /* 31 */
};

/* CPU exception handler
 *
 * @param int_no: interrupt/exception number (0-31 for exceptions)
 * @param err_code: error code provided by CPU (or 0 if not applicable)
 *
 * Called from assembly ISR when a CPU exception occurs.
 * Handles both exceptions with and without error codes.
 *
 * Action:
 * 1. Log exception details to serial port
 * 2. Display on VGA terminal
 * 3. Trigger kernel panic
 *
 * Certain exceptions (like double fault) have special handling
 * to prevent infinite exception loops.
 */
void exception_handler(uint32_t int_no, uint32_t err_code) {
    /* Disable further interrupts immediately to prevent nesting */
    disable_interrupts();
    
    /* Get exception name or use default for unknown */
    const char *exc_name = "Unknown";
    if (int_no < 32) {
        exc_name = exception_names[int_no];
    }
    
    /* Log to serial port (primary debug output) */
    serial_puts("\n");
    serial_puts("========================================\n");
    serial_puts("        CPU EXCEPTION ENCOUNTERED       \n");
    serial_puts("========================================\n\n");
    
    serial_printf("Exception: %d (%s)\n", int_no, exc_name);
    serial_printf("Error Code: 0x%x\n", err_code);
    
    /* Additional diagnostic info for specific exceptions */
    switch (int_no) {
        case 8:  /* Double Fault */
            serial_puts("\nDouble Fault (critical error condition detected)\n");
            serial_puts("This usually indicates:\n");
            serial_puts("  - Invalid GDT/IDT descriptor\n");
            serial_puts("  - Stack corruption\n");
            serial_puts("  - Recursive exception loop\n");
            break;
            
        case 11: /* Segment Not Present */
            serial_puts("\nSegment Not Present\n");
            serial_printf("Selector index: %d (TI=%d, RPL=%d)\n",
                         (err_code >> 3) & 0x1FFF,
                         (err_code >> 2) & 0x1,
                         err_code & 0x3);
            break;
            
        case 12: /* Stack Fault */
            serial_puts("\nStack Segment Fault\n");
            serial_puts("Likely cause: stack overflow or invalid stack segment\n");
            break;
            
        case 13: /* General Protection Fault */
            serial_puts("\nGeneral Protection Fault\n");
            serial_printf("Selector index: %d (TI=%d, RPL=%d)\n",
                         (err_code >> 3) & 0x1FFF,
                         (err_code >> 2) & 0x1,
                         err_code & 0x3);
            serial_puts("Possible causes:\n");
            serial_puts("  - Invalid segment access\n");
            serial_puts("  - Privilege level violation\n");
            serial_puts("  - Undefined opcode (if not #6)\n");
            break;
            
        case 14: /* Page Fault */
            serial_puts("\nPage Fault (paging not yet enabled, this is expected)\n");
            break;
            
        default:
            break;
    }
    
    serial_puts("\nSystem will halt.\n");
    
    /* Panic with detailed message */
    panic_printf("CPU Exception: %d (%s), Error Code: 0x%x",
                 int_no, exc_name, err_code);
    
    /* Should not return from panic */
    while (1) {
        halt();
    }
}
