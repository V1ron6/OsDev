; kernel/syscall_entry.asm - System Call Entry Point
;
; Handles int 0x80 software interrupt for syscalls.
; When user code executes "int 0x80":
; 1. CPU automatically switches to kernel stack (via TSS)
; 2. This handler saves remaining registers
; 3. Handler calls C function to dispatch syscall
; 4. Result returned in EAX via IRET

[BITS 32]

; Import the C dispatcher function
extern syscall_handler
extern syscall_select_exit_context

; Export entry point for IDT
global syscall_entry

; Syscall entry point (called via int 0x80)
; At entry, CPU has already:
; - Saved EIP, CS, EFLAGS, ESP, SS on kernel stack
; - Switched to kernel privilege (ring 0)
; - Loaded kernel stack pointer from TSS

syscall_entry:
    ; Save user registers
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    
    ; EAX still contains syscall number (don't clobber yet)
    ; Extract syscall number
    mov eax, eax        ; EAX = syscall number (no-op, just for clarity)
    
    ; Call syscall_handler(int_no, syscall_no, arg0, arg1, arg2, arg3, arg4, arg5)
    ; Arguments:
    ;   int_no    = 0x80 (from IDT, not used but consistent)
    ;   syscall_no = EAX
    ;   arg0      = EBX
    ;   arg1      = ECX
    ;   arg2      = EDX
    ;   arg3      = ESI
    ;   arg4      = EDI
    ;   arg5      = EBP
    
    ; Push arguments in reverse order (x86 cdecl: right to left)
    push ebp            ; arg5
    push edi            ; arg4
    push esi            ; arg3
    push edx            ; arg2
    push ecx            ; arg1
    push ebx            ; arg0
    push eax            ; syscall_no (from EAX)
    push dword 0x80     ; int_no
    
    call syscall_handler
    
    ; EAX now contains return value
    ; Discard arguments from stack
    add esp, 32         ; 8 args × 4 bytes

    ; Preserve the syscall result while checking whether exit() terminated
    ; the current task.  A non-zero context switches without returning to it.
    push eax
    call syscall_select_exit_context
    add esp, 4
    test eax, eax
    jnz .switch_task
    
    ; Restore user registers
    ; Note: EAX not restored (contains return value)
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp
    
    ; Return to user mode via IRET
    ; IRET pops EIP, CS, EFLAGS and returns to user code
    iret

.switch_task:
    mov edi, eax
    mov ecx, [edi + 40]       ; Load the next address space first.
    test ecx, ecx
    jz .keep_cr3
    mov cr3, ecx
.keep_cr3:
    mov eax, [edi + 32]       ; EIP
    mov ecx, [edi + 36]       ; EFLAGS
    mov edx, [edi + 44]       ; CS
    test dl, 3
    jz .build_kernel_frame

    mov ebp, [edi + 24]       ; Next task's saved kernel ESP.
    sub ebp, 20
    mov [ebp], eax
    mov [ebp + 4], edx
    mov [ebp + 8], ecx
    mov eax, [edi + 48]
    mov [ebp + 12], eax
    mov eax, [edi + 52]
    mov [ebp + 16], eax
    jmp .frame_ready

.build_kernel_frame:
    mov ebp, [edi + 24]
    sub ebp, 12
    mov [ebp], eax
    mov [ebp + 4], edx
    mov [ebp + 8], ecx

.frame_ready:
    mov [esp + 24], ebp       ; Preserve target ESP across register restore.
    mov edx, [edi + 20]
    mov [esp + 28], edx       ; Preserve target EDI until the context is free.

    mov eax, [edi + 0]
    mov ebx, [edi + 4]
    mov ecx, [edi + 8]
    mov edx, [edi + 12]
    mov ebp, [edi + 28]
    mov esi, [edi + 16]
    mov edi, [esp + 28]
    mov esp, [esp + 24]
    iret
