# ByteBandit OS - Phase 4 Progress

**Updated**: 2026-10-29
**Status**: Active implementation with core kernel/user-mode scaffolding in place; scheduler queue lifecycle hardening is complete; preemptive user-task launch remains the final milestone

## Scope

Phase 4 moves the OS from a kernel-only execution model toward a multi-process runtime. The implementation is aligned with the public contract described in [BYTEBANDIT_API.md](BYTEBANDIT_API.md) and [CLI_APP_DEVELOPMENT.md](CLI_APP_DEVELOPMENT.md): version `0.1` system info, `int 0x80` syscall dispatch, RAM-backed filesystem access, and kernel-managed task metadata.

## Current API / ABI Coverage

| Layer | Status | Evidence |
|---|---|---|
| API version and status codes | Implemented | `include/bb_api.h` defines `BB_API_MAJOR`, `BB_API_MINOR`, and the `BB_STATUS_*` values |
| Kernel API info functions | Implemented | `kernel/bb_api.c` implements `bb_api_get_info()` and `bb_api_print_version()` |
| Syscall ABI | Implemented | `kernel/syscall.c` installs the `0x80` handler and dispatches the system calls |
| User wrappers | Implemented | `include/bb_syscall.h` exposes `bb_read`, `bb_write`, `bb_getpid`, `bb_sleep_ms`, `bb_get_system_info`, and registry calls |
| Console shell | Implemented | `kernel/console.c` handles `help`, `version`, `info`, `ps`, `free`, `mount`, `echo`, and related commands |
| VFS and registry | Implemented | `fs/vfs.c` and `fs/registry.c` provide RAM-backed file and registry operations with inode metadata and ownership checks |
| Task registry and lifecycle | Implemented | `kernel/task.c` tracks task objects, PIDs, states, and ELF-backed task creation |
| Scheduler loop | Implemented | `kernel/scheduler.c` contains the ready queue and round-robin selection logic |

## Implemented

- TSS allocation and GDT registration for kernel stack transitions
- Ring 3-compatible GDT descriptors and user-mode transition scaffolding
- `int 0x80` syscall installation and dispatcher logic
- User buffer validation before copy-in/copy-out operations
- Per-process page directory creation and separate user-space mappings
- Private user stack layout below `0xC0000000`
- ELF32 validation, section lookup, PT_LOAD processing, and ELF-backed task creation
- Task registry with PID assignment, state tracking, and task destruction cleanup
- Scheduler ready queue using round-robin selection and current-task tracking
- Scheduler queue lifecycle hardening: duplicate enqueue prevention, terminated-task rejection, explicit removal, and retained task exit codes
- Explicit `task_spawn()` boundary for moving created tasks into the ready queue; scheduler selection now skips terminated entries
- PIT tick/time accounting and sleep support through the API layer
- RAM-backed VFS and registry support for console and file metadata workflows
- Inode-backed ByteFS foundation with parent links, directory semantics, and owner/group metadata
- ATA-backed ByteFS persistence with alternating committed volume slots
- MBR partition creation and kernel-side partition discovery at LBA 2048
- Registry persistence through the ByteFS data partition
- Parent-directory write/execute enforcement for VFS mutations and traversal
- Root/owner enforcement for registry writes, identity changes, and signals
- Basic console power commands: shutdown, reboot, poweroff, restart, and halt
- Operator utilities for stat, sync, sleep, and permission-aware kill
- `bb_api_get_info()` and `bb_system_info_t` reporting for kernel stats

## Verified Build State

The project currently builds successfully with the repo's real toolchain. Fresh verification:

```bash
cd /workspaces/OsDev && make clean && make
```

Result: the kernel, ISO image, and boot artifacts were produced successfully, and `output/bytebandit.iso` was generated. The build still emits warnings, but there were no fatal compiler or linker errors during the full build.

## Remaining Work

The largest remaining gap is the transition from a runnable kernel skeleton into a truly safe multi-process runtime. The concrete items still open are:

- Complete the timer interrupt → scheduler context switch path
- Restore saved `CR3`, `ESP`, and register state during preemption
- Reconcile user and kernel page directories correctly during task switches
- Remove terminated tasks from all queues and reclaim scheduler metadata safely
- Complete user-mode `IRET` return flow for ELF tasks without crashing the kernel
- Handle page faults and general protection faults in a controlled way
- Fully implement user process creation and waiting semantics (`bb_spawn`, `bb_wait`, `bb_open`, `bb_close`)
- Add address-space teardown and frame ownership tracking for task exit
- Add richer persistent block storage and filesystem directory indexing while preserving the inode API contract

## Risk Areas

The current work is stable enough to build, but the final Phase 4 milestone is still blocked by privilege transition correctness. The critical technical risk is not the build graph; it is the correctness of ring-3 context switching, page-directory restoration, and exception recovery. These are the points that determine whether a user ELF task can actually run and survive beyond a single entry.

## Build Validation

Use the repo's standard build path after each major subsystem change:

```bash
make clean && make
```

This preserves the production build flow defined in [Makefile](Makefile) and generates the bootable image in `output/`.
