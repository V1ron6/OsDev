/*
 * arch/x86/usermode.h - Ring 3 User Mode Transition
 *
 * Provides functions to safely transition from ring 0 (kernel) to ring 3 (user).
 *
 * The transition uses IRET (interrupt return) instruction:
 * - Push user code segment (with RPL=3)
 * - Push user code entry point (EIP)
 * - Push EFLAGS (with IF bit set for interrupts)
 * - Push user data segment (with RPL=3)
 * - Push user stack pointer
 * - Execute IRET to load all registers and switch privilege
 */

#ifndef _BYTEBANDIT_USERMODE_H
#define _BYTEBANDIT_USERMODE_H

#include "types.h"

/**
 * Jump to ring 3 (user mode) and never return to this function.
 *
 * @param entry_point  User code entry point (virtual address in user space)
 * @param user_stack   User stack pointer (top of user stack)
 * @param argc         Argument count (passed to user program in EAX)
 * @param argv         Argument vector pointer (passed to user program in EBX)
 *
 * This function:
 * 1. Disables interrupts (critical section)
 * 2. Prepares stack with IRET frame
 * 3. Executes IRET to transition to ring 3
 * 4. Never returns (user code now executing)
 *
 * The user program's stack will look like (before any code executes):
 *   [user_stack]  <- ESP points here, ready for program to use
 *
 * No actual function call is made; user code starts executing directly.
 */
__attribute__((noreturn))
void enter_user_mode(uint32_t entry_point, uint32_t user_stack,
                     uint32_t argc, uint32_t argv);

/**
 * Get the user code segment selector (ring 3).
 *
 * @return Segment selector for user code (with RPL=3).
 */
uint16_t get_user_code_selector(void);

/**
 * Get the user data segment selector (ring 3).
 *
 * @return Segment selector for user data (with RPL=3).
 */
uint16_t get_user_data_selector(void);

#endif /* _BYTEBANDIT_USERMODE_H */
