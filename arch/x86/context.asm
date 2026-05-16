/*
 * arch/x86/context.asm - Context Switching Assembly
 *
 * Low-level CPU context save/restore for task switching.
 *
 * Context is saved to/restored from cpu_context_t structure:
 *   Offset  0-3:  EAX
 *   Offset  4-7:  EBX
 *   Offset  8-11: ECX
 *   Offset 12-15: EDX
 *   Offset 16-19: ESI
 *   Offset 20-23: EDI
 *   Offset 24-27: ESP
 *   Offset 28-31: EBP
 *   Offset 32-35: EIP
 *   Offset 36-39: EFLAGS
 *   Offset 40-43: CR3
 */

.section .text

.global context_switch_asm
.global context_save_asm
.global context_restore_asm
.global _context_switch_asm
.global _context_save_asm
.global _context_restore_asm

/*
 * void context_switch_asm(cpu_context_t *from, cpu_context_t *to)
 *
 * Switch context from one task to another.
 * Saves current CPU state to 'from' context, restores 'to' context.
 * Does NOT return - jumps to 'to' task's EIP.
 *
 * WARNING: Interrupts must be disabled. Stack and other state are fragile.
 * Only call from carefully controlled contexts (interrupt handler).
 *
 * Parameters (x86 cdecl):
 *   [esp+4] = from pointer
 *   [esp+8] = to pointer
 */
context_switch_asm:
_context_switch_asm:
    /* For now, this is a stub that just returns to caller.
     * Real implementation would:
     * 1. Save EBX, ECX, ESI, EDI, EBP, ESP to 'from'
     * 2. Load those registers from 'to'
     * 3. Update TSS.esp0 if privilege level changes
     * 4. Load CR3 from 'to' (page directory switch)
     * 5. Jump to 'to'->eip (or set up to IRET)
     *
     * For Phase 4.4, we'll implement proper context switching.
     * For now, return 0 so the system doesn't crash.
     */
    ret

/*
 * void context_save_asm(cpu_context_t *ctx)
 *
 * Save current CPU registers to context structure.
 *
 * Parameter:
 *   [esp+4] = pointer to cpu_context_t
 */
context_save_asm:
_context_save_asm:
    mov 4(%esp), %eax       /* eax = ctx pointer */
    
    /* Save general purpose registers */
    mov %ebx, 4(%eax)       /* EBX */
    mov %ecx, 8(%eax)       /* ECX */
    mov %edx, 12(%eax)      /* EDX */
    mov %esi, 16(%eax)      /* ESI */
    mov %edi, 20(%eax)      /* EDI */
    
    /* Save frame and stack pointers */
    mov %ebp, 28(%eax)      /* EBP */
    mov %esp, 24(%eax)      /* ESP */
    
    /* Save EFLAGS */
    pushf
    pop %ecx
    mov %ecx, 36(%eax)      /* EFLAGS */
    
    ret

/*
 * void context_restore_asm(cpu_context_t *ctx)
 *
 * Restore CPU registers from context structure.
 * Does NOT restore EIP, ESP, or EFLAGS (caller must handle with IRET).
 *
 * Parameter:
 *   [esp+4] = pointer to cpu_context_t
 */
context_restore_asm:
_context_restore_asm:
    mov 4(%esp), %eax       /* eax = ctx pointer */
    
    /* Restore general purpose registers */
    mov 4(%eax), %ebx       /* EBX */
    mov 8(%eax), %ecx       /* ECX */
    mov 12(%eax), %edx      /* EDX */
    mov 16(%eax), %esi      /* ESI */
    mov 20(%eax), %edi      /* EDI */
    
    /* Restore frame pointer */
    mov 28(%eax), %ebp      /* EBP */
    
    /* Don't restore ESP (caller uses this for return) */
    /* Don't restore EIP (caller uses this for return address) */
    /* Don't restore EFLAGS (interrupts managed elsewhere) */
    
    ret

