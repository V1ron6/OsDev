# Changelog

All notable ByteBandit OS development milestones are recorded here. Dates are
based on the repository history and the project progress reports.

## [Unreleased] - 2026-11-16

### Added

- Added `setup.sh` for idempotent Debian/Ubuntu dependency installation.
- Added validation and installation of the `i686-elf-*` toolchain wrappers used
  by the Makefile.
- Added this changelog to preserve the project history and current state.

### Current state

- The project builds a BIOS-bootable 32-bit x86 kernel image and runs it in
  QEMU.
- On 2026-11-16, `./setup.sh` installed the missing host dependencies and
  `make clean && make` completed successfully, producing the kernel, ISO, and
  persistent disk image. Existing compiler and linker warnings remain
  non-fatal.
- Added privilege-aware timer context metadata and interrupt return-frame
  construction for both ring-0 and ring-3 tasks.
- Added scheduler-aware syscall exit handoff so an exiting task selects a
  replacement context instead of halting in the syscall entry path.
- Added deferred task reaping on timer interrupts, including release of
  user-owned page frames while preserving the task's shared kernel stack.
- Added the console `exec PATH` command to load an ELF image from ByteFS and
  queue it as a user task.
- Updated user-buffer validation to inspect the active page directory during
  user syscalls.
- Implemented the public `bb_spawn(path, argv)` syscall. It loads a ByteBandit
  ELF image from ByteFS, inherits the caller identity, queues the child, and
  returns its PID as a process handle.
- Implemented blocking `bb_wait(process, status)`. Waiting parents enter the
  blocked state, child exit wakes them, and the completed child is reaped with
  its exit status.
- User-mode exceptions now terminate only the faulting task and transfer
  through the scheduler to a replacement context; kernel faults still panic.
- Added a minimal freestanding ring-3 smoke ELF and `make user-smoke` target
  for exercising the ELF launch and syscall-exit paths.
- Versioned ISO generation, persistent ByteFS storage, an operator shell,
  permissions, registry persistence, syscalls, user-mode scaffolding, task
  lifecycle tracking, and scheduler bookkeeping are implemented.
- The final Phase 4 runtime work remains: safe preemptive user-task switching,
  scheduler-aware syscall exit, address-space reclamation, page-fault handling,
  and complete spawn/wait behavior.

## 2026-11-07

### Changed

- Hardened task switching lifecycle handling.
- Updated timer task selection to maintain the TSS kernel stack pointer.
- Routed syscall exit through common task termination bookkeeping.
- Added guarded task reaping for post-switch cleanup.
- Preserved the selected task stack pointer through the IRQ restore path.
- Expanded development and Phase 4 progress reports with verification results,
  limitations, and the next implementation steps.

## 2026-10-11

### Added

- Added persistent ByteFS storage backed by an ATA disk image.
- Added MBR partition creation and kernel-side partition discovery.
- Added alternating committed volume slots with generation and CRC validation.
- Added persistent registry storage and Linux-style ownership and permission
  checks.
- Added the operator shell commands for filesystem, process, registry, identity,
  timing, and power-management workflows.
- Added the public ByteBandit API and CLI application development contracts.
- Added the staged desktop roadmap while keeping the kernel and filesystem
  model independent of presentation.

### Changed

- Added versioned ISO builds and a one-command QEMU launcher.
- Added persistent disk preservation across normal builds and explicit reset
  support.
- Extended the bootloader kernel-sector capacity and added segment rollover
  handling.

## 2026-05-16 – 2026-05-17

### Added

- Implemented the Phase 4 foundation: TSS setup, ring-3 transition scaffolding,
  the `int 0x80` syscall interface, task structures, and scheduler foundations.
- Added ELF loading support, user stack setup, process metadata, PIT timing,
  sleep support, and kernel-managed task lifecycle operations.

## 2026-05-14

### Added

- Completed the Phase 1 bootable kernel foundation with a BIOS bootloader,
  protected-mode transition, kernel entry, VGA output, serial logging, linker
  layout, and build documentation.
- Completed the Phase 2 interrupt foundation with IDT setup, CPU exception
  handlers, PIC configuration, and hardware IRQ support.
- Completed the Phase 3 memory-management foundation with physical allocation,
  paging, virtual-memory helpers, and heap support.

## Initial project

- Created the ByteBandit OS repository as an educational, modular 32-bit x86
  operating-system kernel project.
