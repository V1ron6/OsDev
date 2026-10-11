# ByteBandit OS Development Progress

**Updated**: 2026-10-11
**Current ISO**: `output/bytebandit-v035.iso`
**Current phase**: Bootable kernel foundation with persistent storage and operator shell

## Completed

### Build and boot workflow

- Added versioned ISO builds through `builder.sh`.
- Added `run.sh` for one-command QEMU execution.
- `run.sh` automatically selects the newest versioned ISO.
- Added `--no-build` and `--reset-disk` launcher options.
- Added a stable writable disk image at `output/bytebandit-data.img`.
- QEMU boots from the ISO while using the hard disk for persistent data.
- Extended the bootloader from 128 to 200 kernel sectors.
- Added segment rollover handling for kernels larger than 64 KiB.

### Persistent ByteFS storage

- Replaced the flat VFS model with an inode-style tree.
- Added root and directory nodes with parent relationships.
- Added path, owner, group, mode, type, length, and payload metadata.
- Added ATA PIO sector reads and writes.
- Added MBR partition creation with ByteFS beginning at LBA 2048.
- Added kernel-side partition discovery with raw-disk fallback.
- Added two alternating persistent volume slots.
- Added generation-based committed-slot selection.
- Added CRC32 validation for metadata and file payload sectors.
- Added persistence for file create, write, truncate, mkdir, unlink, and chmod.
- Added `sync` to explicitly commit the current filesystem state.

### Registry and permissions

- Added persistent registry storage in `/etc/registry.db`.
- Registry falls back to defaults when no writable disk is present.
- Added Linux-style owner/group/other VFS permission checks.
- Path traversal requires directory execute access.
- Create, delete, and mkdir require parent write and execute access.
- Only root may change UID/GID through syscalls.
- Only root may modify registry values.
- `kill` is restricted to root or same-effective-UID targets.

### Operator shell

Implemented and documented:

- `help`, `version`, `info`, `mem`, `free`
- `ls`, `files`, `stat`, `cat`, `head`
- `touch`, `write`, `cp`, `mkdir`, `rm`, `sync`
- `registry`, `env`, `mount`
- `ps`, `tasks`, `kill`, `sleep`, `uptime`
- `whoami`, `id`, `uname`, `pwd`
- `echo`, `clear`, `true`, `false`
- `shutdown`, `poweroff`, `reboot`, `restart`, `halt`

## Verification completed

- `builder.sh` successfully produced versioned ISO images through `v035`.
- `./run.sh --no-build` reaches the `bb>` prompt.
- Persistent ByteFS loads successfully after reboot.
- `shutdown` exits QEMU cleanly.
- `stat /etc/motd` reports correct metadata.
- `sync` reports successful filesystem synchronization.
- `bash -n run.sh builder.sh` passes.
- `git diff --check` passes.
- Touched kernel files report no language-server errors.

Expected non-fatal build warnings remain in older code, including unused syscall
parameters, signedness checks, and the missing GNU-stack assembler note.

## Current limitations

- The filesystem uses fixed-size inode slots and fixed-size file payloads.
- The volume has no journaling or transactional recovery beyond alternating slots.
- There is no full directory-entry index or efficient path lookup.
- Process creation, user ELF execution, preemptive context switching, and wait
  semantics are incomplete.
- `syscall_exit()` still halts instead of safely switching to another task.
- Signals are only partially represented; full signal delivery is not implemented.
- Keyboard input is still serial-based.
- Registry permissions are root-only rather than per-key ownership metadata.
- No GUI or graphics subsystem exists yet.

## Next course of action

1. **Stabilize process runtime**
   - Complete timer-driven context switching.
   - Save and restore registers, `ESP`, `EIP`, and `CR3` safely.
   - Make task exit return to the scheduler instead of halting.
   - Reclaim task stacks, page directories, and user frames.

2. **Complete user process APIs**
   - Implement reliable `spawn`, `wait`, and `exit` behavior.
   - Add per-process descriptor tables instead of the current global descriptor table.
   - Finish ring-3 `IRET` entry and return paths.
   - Add controlled page-fault and general-protection-fault handling.

3. **Strengthen filesystem behavior**
   - Add directory-entry indexing and proper tree traversal.
   - Add file size/stat APIs to the user syscall ABI.
   - Add volume validation tools and clearer corruption diagnostics.
   - Consider a journal or copy-on-write metadata transaction layer.

4. **Expand hardware support**
   - Add a keyboard driver while retaining serial input for debugging.
   - Add a block-device abstraction above ATA.
   - Add a simple device registry and mount model.

5. **Only after the kernel is stable: graphical shell**
   - Follow the staged graphics, input, window-server, compositor, desktop-shell, and application plan in `DESKTOP_ROADMAP.md`.
   - Keep macOS-inspired presentation separate from Linux-like process, filesystem, permissions, and scripting behavior.

## Resume commands

```bash
cd /workspaces/OsDev
./builder.sh
./run.sh
```

For a fast reboot without rebuilding:

```bash
./run.sh --no-build
```

To intentionally reset persistent storage:

```bash
./run.sh --reset-disk
```

The public contracts and subsystem details remain documented in:

- `BYTEBANDIT_API.md`
- `CLI_APP_DEVELOPMENT.md`
- `DEBUGGING.md`
- `ARCHITECTURE.md`
- `PHASE4_PROGRESS.md`
- `DESKTOP_ROADMAP.md`
