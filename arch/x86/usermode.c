/*
 * arch/x86/usermode.c - Ring 3 Transition Implementation
 *
 * Implements the transition from ring 0 (kernel) to ring 3 (user mode).
 */

#include "usermode.h"
#include "gdt.h"
#include "x86.h"
#include "serial.h"

/**
 * Ring 3 user mode transition via IRET.
 *
 * This is implemented in assembly to have precise control over:
 * - Stack layout for IRET
 * - Segment selector values
 * - EFLAGS configuration
 *
 * The assembly function:
 * 1. Receives kernel parameters on kernel stack
 * 2. Constructs IRET frame on kernel stack
 * 3. Sets up registers for user program
 * 4. Executes IRET to switch to ring 3
 */
extern void _usermode_iret(uint32_t entry_point, uint32_t user_stack,
                           uint32_t argc, uint32_t argv);

void enter_user_mode(uint32_t entry_point, uint32_t user_stack,
                     uint32_t argc, uint32_t argv) {
    serial_printf("[USERMODE] Transitioning to ring 3\n");
    serial_printf("[USERMODE] Entry point: 0x%08x\n", entry_point);
    serial_printf("[USERMODE] User stack: 0x%08x\n", user_stack);
    serial_printf("[USERMODE] Argc: %u, Argv: 0x%08x\n", argc, argv);
    
    /* Disable interrupts during transition - critical section */
    disable_interrupts();
    
    /* Call assembly function that builds IRET frame and switches mode
     *
     * This function never returns - control transfers to user code via IRET
     */
    _usermode_iret(entry_point, user_stack, argc, argv);
    
    /* Unreachable code */
    __builtin_unreachable();
}

uint16_t get_user_code_selector(void) {
    return USER_CODE_SEL;
}

uint16_t get_user_data_selector(void) {
    return USER_DATA_SEL;
}
