# ByteBandit OS - Phase 4: User Mode, Multitasking & Execution

**Planned Phase**: May 2026 onwards  
**Status**: Planning & Specification  
**Focus**: Ring 3 user mode, system calls, preemptive multitasking, ELF execution  

---

## Vision Statement

Transform ByteBandit OS from a **kernel-only execution environment** into a **multi-process operating system** where isolated user programs can run safely under the control of a preemptive scheduler.

By the end of Phase 4, the system will:
- ✅ Run isolated ring 3 user programs
- ✅ Support system calls for kernel services
- ✅ Schedule multiple processes preemptively
- ✅ Load and execute ELF executables
- ✅ Isolate process memory via separate page directories
- ✅ Survive user-space crashes safely
- ✅ Provide foundation for real multi-process execution

---

## Implementation Roadmap

### System 1: Task State Segment (TSS)

**Purpose**: Enable safe privilege transitions between ring 0 (kernel) and ring 3 (user).

**Requirements**:
- Define x86 TSS structure
- Allocate kernel TSS
- Create GDT TSS descriptor
- Load TSS with `ltr` instruction
- Configure kernel stack switching (esp0, ss0)

**Key Fields**:
```c
struct tss {
    uint32_t previous_task_link;
    uint32_t esp0;           /* Kernel stack pointer for ring 0 */
    uint32_t ss0;            /* Kernel stack segment (ring 0) */
    uint32_t esp1, ss1;      /* Ring 1 (not used) */
    uint32_t esp2, ss2;      /* Ring 2 (not used) */
    uint32_t cr3;            /* PDBR for task switching (not used) */
    uint32_t eip, eflags;    /* Not used (no hardware task switch) */
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldtr;
    uint16_t reserved, io_base;
};
```

**Files to Create**:
- `arch/x86/tss.h` - TSS structures and functions
- `arch/x86/tss.c` - TSS initialization and management

**Key Functions**:
- `tss_init()` - Initialize TSS, load into GDT
- `tss_set_kernel_stack(esp0, ss0)` - Update kernel stack for transitions
- `tss_get()` - Get current TSS address

**Design Notes**:
- Use TSS **strictly for privilege stack transitions** (not hardware task switching)
- When user code triggers interrupt, CPU automatically switches to esp0/ss0
- Essential for safe return from user mode to kernel mode

---

### System 2: Ring 3 User Mode Transition

**Purpose**: Safely enter ring 3 (user mode) from ring 0 (kernel mode).

**Requirements**:
- Create user-mode GDT segments
- Implement `enter_user_mode()` function
- Set up user stack
- Correct privilege levels in segment selectors
- Use IRET to transition

**GDT Segments Needed**:
```c
/* Ring 3 code segment (selector 0x1B) */
#define GDT_USER_CODE_SEL    0x1B

/* Ring 3 data segment (selector 0x23) */
#define GDT_USER_DATA_SEL    0x23
```

**Transition Mechanism** (IRET-based):

```asm
; Push to stack in this order (for iret):
; [ESP+0] = EIP (user code entry point)
; [ESP+4] = CS (user code segment with RPL=3)
; [ESP+8] = EFLAGS (with IF bit set for interrupts)
; [ESP+12] = ESP (user stack pointer)
; [ESP+16] = SS (user data segment with RPL=3)

iret  ; Pops EIP, CS, EFLAGS, ESP, SS and transitions to ring 3
```

**Files to Create**:
- `arch/x86/usermode.h` - User mode transition declarations
- `arch/x86/usermode.c` / `arch/x86/usermode.asm` - Transition implementation

**Key Functions**:
- `enter_user_mode(entry_point, user_stack)` - Jump to user code
- `setup_user_stack()` - Prepare stack for user execution

**Expected Challenges**:
- GPF faults if segment selectors incorrect
- Invalid privilege level transitions
- Stack corruption from bad ESP/SS values
- Triple fault from invalid CS selector

---

### System 3: System Call Interface

**Purpose**: Allow user programs to request kernel services.

**Initial Mechanism**: Software interrupt `int 0x80`
- When user code executes `int 0x80`, CPU:
  1. Loads TSS kernel stack (esp0/ss0)
  2. Calls exception handler (vector 0x80)
  3. Handler dispatches to syscall function
  4. Syscall validates arguments (userspace pointers)
  5. Performs kernel operation
  6. Returns to user code via IRET

**Syscall Convention** (x86 standard):
```c
/* System call number in EAX */
/* Arguments in EBX, ECX, EDX, ESI, EDI, EBP */
int result = syscall(SYS_WRITE, buf, count, ...);
```

**Initial Syscalls** (8-15):

| Number | Name | Purpose |
|--------|------|---------|
| 1 | SYS_EXIT | Terminate process |
| 4 | SYS_WRITE | Write to file descriptor |
| 20 | SYS_GETPID | Get process ID |
| 24 | SYS_KILL | Send signal (simplified) |
| 102 | SYS_SIGACTION | Register signal handler |

**Files to Create**:
- `kernel/syscall.h` - Syscall definitions
- `kernel/syscall.c` - Syscall dispatcher
- `arch/x86/syscall_entry.asm` - `int 0x80` handler

**Key Functions**:
- `syscall_init()` - Install int 0x80 handler
- `syscall_handler(int_no)` - Dispatcher (called from assembly)
- `syscall_exit(code)`, `syscall_write(fd, buf, count)` - Syscall implementations

**Safety Requirements** (CRITICAL):
- **Validate all userspace pointers**: Never directly dereference user memory
- **Copy data to kernel space first**: Use safe copy routines
- **Check argument bounds**: Size limits, fd validity
- **Disable interrupts**: During critical operations
- **Never trust user input**: Even EAX register value could be invalid

**Wrapper Library** (userspace):
```c
/* User code links against this for syscall wrappers */
long write(int fd, const void *buf, size_t count) {
    return syscall(SYS_WRITE, (long)buf, count, fd);
}

void exit(int code) {
    syscall(SYS_EXIT, code);
}
```

---

### System 4: Preemptive Multitasking

**Purpose**: Enable timer-driven task switching (preemption).

**Current State**: Tasks exist but don't run (cooperative only).

**Upgrade Path**:
1. PIT timer already fires at 100 Hz (every ~10ms)
2. Add task switch at timer interrupt
3. Save current task's registers
4. Load next task's registers
5. Return to next task via IRET

**Context Preservation Flow**:
```
Timer interrupt (IRQ0) fires
    ↓
CPU saves EIP/CS/EFLAGS/ESP/SS (automatic, to stack)
    ↓
IRQ0 handler (pit_handler) called
    ↓
Save remaining registers (EAX/EBX/ECX/EDX/ESI/EDI/EBP)
    ↓
Call schedule() to pick next task
    ↓
Call context_switch(from, to)
    ↓
Restore next task's registers
    ↓
IRET to next task's EIP
```

**Task Context Structure** (already defined in Phase 3):
```c
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp;
    uint32_t eip, eflags, cr3;
} cpu_context_t;
```

**Files to Create/Update**:
- `arch/x86/context.asm` - Low-level context switch assembly
- `kernel/scheduler.c/h` - Scheduling algorithm
- Update `arch/x86/pit.c` - Add task switching to timer handler

**Key Functions**:
- `schedule()` - Pick next runnable task
- `context_switch(from, to)` - Assembly routine to switch contexts
- `switch_to_task(task)` - Wrapper for context_switch

**Expected Challenges**:
- Corrupted stacks from bad ESP values
- Race conditions (disable interrupts during switch!)
- Invalid page directory (bad CR3 for user tasks)
- ISR context corruption (wrong register restore)
- Missed EOI to PIC (hangs system)

---

### System 5: Process Scheduler

**Purpose**: Determine which task runs next.

**Initial Algorithm**: Round-robin with priority
- Keep tasks in ready queue
- Give each task fixed time slice (quantum)
- Rotate through ready queue
- Skip blocked/terminated tasks

**Data Structures**:
```c
struct scheduler {
    task_t *ready_queue[MAX_TASKS];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_size;
    
    task_t *current_task;
    task_t *idle_task;
    
    uint32_t ticks;
    uint32_t task_ticks[MAX_TASKS];
};
```

**Task States**:
```c
TASK_READY    - Waiting for CPU
TASK_RUNNING  - Currently executing
TASK_BLOCKED  - Waiting for I/O or resource
TASK_DYING    - Exiting, cleanup in progress
TASK_DEAD     - Finished, waiting for reap
```

**Key Functions**:
- `scheduler_init()` - Create ready queue
- `schedule()` - Pick next task (called at every timer tick)
- `task_yield()` - Voluntary context switch
- `task_block(reason)` - Sleep/wait for resource
- `task_unblock(task)` - Wake blocked task
- `task_set_priority(task, priority)` - Change scheduling priority

**Queue Operations**:
```c
void enqueue_ready(task_t *task) {
    /* Add to back of queue */
}

task_t *dequeue_ready() {
    /* Remove from front of queue */
}

task_t *select_next_task() {
    if (ready_queue_empty())
        return idle_task;
    return dequeue_ready();
}
```

**Files to Create**:
- `kernel/scheduler.h` - Scheduler data structures
- `kernel/scheduler.c` - Scheduling algorithm

---

### System 6: ELF Executable Loading

**Purpose**: Load ELF32 user programs from memory into process address space.

**Current State**: ELF structures defined, parsing stubs in place.

**Loading Process**:
1. Validate ELF header (magic, 32-bit, x86)
2. Create new task/process
3. Allocate separate page directory
4. For each PT_LOAD program header:
   - Load segment from file
   - Map to virtual address in process space
   - Set permissions (R/W/X) from header flags
5. Set entry point (e_entry)
6. Queue task for execution

**Program Header Mapping**:
```c
typedef struct {
    uint32_t p_type;        /* PT_LOAD for loadable segments */
    uint32_t p_offset;      /* Offset in file */
    uint32_t p_vaddr;       /* Virtual address to load at */
    uint32_t p_filesz;      /* Size in file */
    uint32_t p_memsz;       /* Size in memory (may include .bss) */
    uint32_t p_flags;       /* PF_R, PF_W, PF_X */
    uint32_t p_align;       /* Alignment requirement */
} program_header_t;
```

**Typical Layout in User Space**:
```
0x08000000  .text segment (PT_LOAD, read-only, executable)
            ...
0x08100000  .data segment (PT_LOAD, read-write)
            .bss (zeroed, not in file)
            ...
0xBFFF0000  User stack (grows down)
0xBFFFFFFF  Stack top
```

**Files to Create/Update**:
- `fs/elf_loader.c` - Full ELF loading implementation
- Update `fs/elf.h/elf.c` - Add loading functions

**Key Functions**:
- `elf_load_executable(base, page_dir)` - Load ELF file into process
- `elf_parse_program_headers()` - Get loadable segments
- `elf_map_segment()` - Map single segment

**Expected Challenges**:
- Misaligned segments causing page faults
- Invalid entry points
- Segments outside user space boundary
- Wrong permission flags (execute non-executable)
- Stack collisions with heap

---

### System 7: User-Space Virtual Memory

**Purpose**: Give each process its own isolated address space.

**Current State**: Identity mapping only (virt == phys). All processes share same page directory.

**Upgrade Path**:
1. Create separate page directory per process
2. Map kernel regions (shared) in each directory
3. Map process-specific regions (isolated)
4. Switch CR3 during context switch

**Address Space Layout**:
```
0x00000000 - 0xBFFFFFFF   User space (process-private)
0xC0000000 - 0xFFFFFFFF   Kernel space (shared across all processes)
```

**Per-Process Regions**:
- Code (.text)
- Initialized data (.data)
- Uninitialized data (.bss)
- Heap (grows up)
- Stack (grows down)

**Shared Kernel Regions**:
- Kernel code/data
- IDT/GDT
- Page tables
- Kernel heap
- Device memory (VRAM, etc.)

**Page Directory Cloning**:
```c
page_directory_t *clone_kernel_pagedir() {
    /* Create new page directory */
    /* Copy kernel entries (0xC0000000+) from kernel pagedir */
    /* Leave user entries (0x00000000-0xBFFFFFFF) unmapped */
    /* Return new pagedir */
}
```

**Files to Create/Update**:
- `mm/process_memory.c/h` - Per-process address space management
- Update `mm/paging.c` - Support multiple page directories

**Key Functions**:
- `create_user_pagedir()` - Create process page directory
- `map_user_segment()` - Map process region into its pagedir
- `destroy_user_pagedir()` - Free process page directory and pages
- `switch_pagedir(pagedir)` - Load page directory into CR3

---

### System 8: Process Management

**Purpose**: Manage process lifecycle (create, execute, exit, wait).

**Process Operations**:

**fork() - Duplicate Process** (simplified):
```c
pid_t fork() {
    /* Create child task */
    /* Duplicate parent's address space */
    /* Copy parent's registers to child context */
    /* Return parent PID in parent, 0 in child */
}
```

**exec() - Execute Program**:
```c
int exec(const char *filename, char *argv[]) {
    /* Load ELF executable */
    /* Replace current process image */
    /* Keep same PID and parent*/
    /* Jump to program entry point */
}
```

**exit() - Terminate Process**:
```c
void exit(int code) {
    /* Mark task as DYING */
    /* Free resources (memory, open files) */
    /* Send SIGCHLD to parent */
    /* Never return */
}
```

**wait() - Wait for Child**:
```c
pid_t wait(int *status) {
    /* Block until child exits */
    /* Collect exit code */
    /* Free child task structure */
    /* Return child PID */
}
```

**Files to Create**:
- `kernel/process.c/h` - Process management
- `kernel/process_create.c` - Process creation helpers

**Key Functions**:
- `process_create_from_elf(filename)` - Create process from executable
- `process_fork(current)` - Duplicate process
- `process_exit(code)` - Terminate with exit code
- `process_wait(pid, status)` - Wait for child

---

### System 9: IPC Foundations

**Purpose**: Enable simple inter-process communication.

**Initial Mechanism**: Message queues (pipes come later).

**Message Queue Structure**:
```c
struct message {
    uint32_t sender_pid;
    uint32_t data[16];      /* Up to 64 bytes per message */
    uint32_t length;
};

struct message_queue {
    message_t messages[64];
    uint32_t head, tail;    /* Circular buffer */
    uint32_t count;
};
```

**Syscalls**:
- `send_message(pid, msg)` - Send message to process
- `recv_message(timeout)` - Receive message with timeout

**Safety**:
- Validate recipient PID
- Validate message size
- Block if queue full
- Timeout to prevent deadlock

**Files to Create**:
- `kernel/ipc.c/h` - Message queue implementation

---

### System 10: Userspace Runtime Support

**Purpose**: Provide minimal libc-like interface for user programs.

**Userspace Startup**:
```asm
; _start (entry point)
    mov eax, [esp + 4]      ; argc
    mov ebx, [esp + 8]      ; argv
    call main
    
    mov eax, SYS_EXIT       ; exit syscall
    mov ebx, [esp]          ; exit code
    int 0x80
```

**Syscall Wrappers** (C library):
```c
long write(int fd, const void *buf, size_t count) {
    return syscall(SYS_WRITE, buf, count, fd);
}

int printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    write(1, buf, n);
    return n;
}

void exit(int code) {
    syscall(SYS_EXIT, code);
    __builtin_unreachable();
}
```

**Files to Create**:
- `user/libc/libc.h` - Basic C library declarations
- `user/libc/syscall.c` - Syscall wrappers
- `user/libc/stdio.c` - printf, puts
- `user/libc/crt0.asm` - Startup code
- `user/linker.ld` - Linker script for user binaries

**Minimal Helpers**:
- `printf()` - Formatted output
- `puts()` - String output
- `exit()` - Terminate
- `malloc()` - Allocate (optional, initially)

---

## Architecture & Design

### GDT Structure (Updated)

```c
GDT Entry | Selector | Purpose
0         | 0x00     | NULL (required)
1         | 0x08     | Kernel code (ring 0)
2         | 0x10     | Kernel data (ring 0)
3         | 0x18     | User code (ring 3)
4         | 0x20     | User data (ring 3)
5         | 0x28     | TSS
```

### ISR Context (During System Call)

```
User program executes:  int 0x80

CPU saves automatically:
    EIP (user code location)
    CS (user code segment)
    EFLAGS
    ESP (user stack pointer)
    SS (user stack segment)

Then loads from TSS:
    CS = kernel code segment
    ESP = kernel stack (esp0)
    SS = kernel data segment

ISR handler then saves:
    All general purpose registers
    Segmentation registers
    (Context now at kernel privilege level)
```

### Context Switch Flow

```
Timer IRQ (every 10ms):
    Save user registers
    ↓
    schedule() picks next task
    ↓
    context_switch():
        Save current task's registers
        Switch page directory (CR3)
        Restore next task's registers
    ↓
    IRET to next task

If switching from user to user:
    - CR3 changes (different address space)
    - ESP/EBP change (different stack)
    - EIP changes (different code location)

If switching from user to kernel:
    - CR3 stays kernel
    - ESP/EBP change (different stack)
    - EIP changes (different code location)
```

---

## Testing Strategy

### Unit Tests (Before Integration)

1. **TSS Tests**:
   - Allocate TSS
   - Load into GDT
   - Verify kernel stack pointer
   - Trigger privilege transition (should not crash)

2. **System Call Tests**:
   - Execute `int 0x80` from kernel
   - Verify dispatcher called
   - Test parameter passing
   - Test return value

3. **Scheduler Tests**:
   - Create multiple tasks
   - Run scheduler
   - Verify round-robin order
   - Check ready queue integrity

4. **ELF Loader Tests**:
   - Load simple ELF executable
   - Verify segments mapped correctly
   - Check entry point set properly

5. **User Mode Tests**:
   - Jump to ring 3
   - Execute simple code
   - Verify fault isolation
   - Safe return to kernel

### Integration Tests

1. **Multi-Task Execution**:
   - Load multiple programs
   - Schedule them concurrently
   - Verify isolation
   - Test priority scheduling

2. **System Call Validation**:
   - Test write() syscall
   - Test exit() syscall
   - Test getpid() syscall

3. **Fault Handling**:
   - User program divide by zero
   - User program invalid opcode
   - User program page fault
   - Verify kernel survives

---

## Debugging Requirements

### Diagnostic Functions

```c
dump_task(task_t *task);           /* Print task info */
dump_scheduler_state();             /* Print ready queue */
dump_registers(cpu_context_t *ctx); /* Print saved registers */
dump_page_directory(uint32_t cr3);  /* Print page directory */
dump_user_memory(uint32_t addr);    /* Print user memory (safe) */
```

### On Crash Output

When user program crashes:
```
[FAULT] User program crashed
    Task ID: 3
    Task Name: user_test
    Exception: 13 (General Protection)
    Error Code: 0x0000
    EIP: 0x08000420 (user code)
    CR3: 0x00150000 (user page directory)
    Privilege: Ring 3 (user)
    Registers: EAX=0x00000000, EBX=0x08100000, ...
    
[KERNEL] Cleaning up task 3
[KERNEL] Resuming other tasks
```

---

## Expected Failure Modes

### Common Issues & Solutions

| Failure | Cause | Debug Method |
|---------|-------|--------------|
| Triple fault after `iret` | Invalid segment selector (bad GDT entry) | Check selector values vs GDT |
| GPF on user jump | Privilege level mismatch in selector | Verify RPL=3 in user segments |
| Kernel crash on user syscall | No TSS or wrong kernel stack | Check esp0/ss0 in TSS |
| User program hangs | Infinite loop or deadlock | Add breakpoint at timer handler |
| Memory corruption | Bad page directory switching | Verify CR3 loads correct pagedir |
| Lost interrupts | EOI not sent during context switch | Check PIC EOI in scheduler |
| Deadlock | All tasks blocked | Check task state machine |
| Invalid user pointer | Dereferencing user memory unsafely | Copy to kernel first, always |

---

## Phase 4 Success Criteria

✅ **Completion Checklist**:
- [ ] TSS implemented and loaded
- [ ] User mode transitions (iret to ring 3)
- [ ] System call interface (int 0x80)
- [ ] Preemptive scheduling working
- [ ] Scheduler queues functioning
- [ ] ELF executable loading (basic)
- [ ] Per-process page directories
- [ ] Process creation and termination
- [ ] Simple IPC (message queues)
- [ ] User programs run and terminate safely
- [ ] Faulting user processes don't crash kernel
- [ ] All diagnostics implemented

---

## File Organization

```
kernel/
├── main.c               (add phase 4 init)
├── scheduler/
│   ├── scheduler.c      (scheduling algorithm)
│   ├── scheduler.h
│   └── queue.c          (ready queue)
├── process/
│   ├── process.c        (process lifecycle)
│   ├── process.h
│   └── fork_exec.c      (fork/exec)
├── syscall/
│   ├── syscall.c        (dispatcher)
│   ├── syscall.h
│   └── handlers/        (individual syscalls)
└── ipc/
    ├── message_queue.c
    └── message_queue.h

arch/x86/
├── tss/
│   ├── tss.c
│   └── tss.h
├── userspace/
│   ├── usermode.c       (ring 3 transition)
│   ├── usermode.asm
│   └── usermode.h
└── context/
    ├── context.asm      (context_switch)
    └── context.c        (context management)

fs/
├── elf_loader.c         (full ELF loading)
└── elf_loader.h

user/
├── libc/
│   ├── libc.h
│   ├── syscall.c        (syscall wrappers)
│   ├── stdio.c
│   ├── stdlib.c
│   ├── crt0.asm         (startup)
│   └── linker.ld
└── programs/
    └── hello.c          (example user program)
```

---

## Timeline Estimate

| System | Effort | Timeline |
|--------|--------|----------|
| 1. TSS | Low | 2-3 hours |
| 2. User mode | Medium | 4-5 hours |
| 3. Syscalls | Medium | 4-5 hours |
| 4. Preemption | High | 6-8 hours |
| 5. Scheduler | High | 6-8 hours |
| 6. ELF loading | Medium | 4-5 hours |
| 7. Per-process VMM | Medium | 5-6 hours |
| 8. Process mgmt | Medium | 4-5 hours |
| 9. IPC | Low | 3-4 hours |
| 10. Userspace libs | Medium | 4-5 hours |
| **Total** | **High** | **~45-55 hours** |

---

## Prior Requirements Met by Phase 3

✅ Memory management (PMM, paging, heap)  
✅ Task structures and CPU context definitions  
✅ Exception handling ready for user faults  
✅ Interrupt system ready for preemption  
✅ Page table infrastructure for process isolation  
✅ ELF structure definitions  
✅ Serial/VGA debugging output  

---

## Success Metrics

At the end of Phase 4:

- **Stability**: Faulting user programs don't crash kernel
- **Isolation**: Users can't access kernel memory
- **Responsiveness**: Timer-driven preemption works
- **Functionality**: ELF programs can run and exit
- **Safety**: All user pointers validated
- **Debuggability**: Rich diagnostics on crashes

---

## Next Phase (Phase 5)

After Phase 4 completes, consider:
- File I/O and filesystem basics
- Process signals
- Process groups and sessions
- Device driver framework
- Memory-mapped I/O
- Advanced IPC (pipes, sockets)
- User authentication / permissions

---

## Conclusion

Phase 4 transforms ByteBandit OS from a kernel-only bootable environment into a true multi-process operating system with ring 3 user space isolation, preemptive scheduling, and system call support.

Success depends on:
- **Meticulous** attention to privilege level transitions
- **Extensive** testing of corner cases
- **Comprehensive** error checking (user pointers, bounds)
- **Safety-first** design (kernel integrity > performance)

The implementation must be **educational and debuggable** – every failure mode should be understandable and traceable via serial output and GDB.
