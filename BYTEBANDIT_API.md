# ByteBandit System APIs

## Scope

The ByteBandit API is the headless operating-system interface for the current
x86 kernel. It is designed for serial-console operation first; the staged GUI
plan is documented in [DESKTOP_ROADMAP.md](DESKTOP_ROADMAP.md), but no GUI is
implemented in the current API version.

The public declarations are in [include/bb_api.h](include/bb_api.h).
User-space syscall wrappers are in [include/bb_syscall.h](include/bb_syscall.h).

## Version

The current API version is `0.3`. The version constants are `BB_API_MAJOR` and
`BB_API_MINOR`. The 0.3 update adds Linux-style identity tracking, ownership,
and VFS permission checks while keeping the existing console and task model
contracts stable.

## Status Codes

| Constant | Value | Meaning |
|---|---:|---|
| `BB_STATUS_OK` | `0` | Operation succeeded |
| `BB_STATUS_INVALID` | `-1` | Invalid argument |
| `BB_STATUS_NOT_FOUND` | `-2` | Requested object was not found |
| `BB_STATUS_NOSYS` | `-3` | Service is not implemented |
| `BB_STATUS_PERM` | `-4` | Permission denied |
| `BB_STATUS_AGAIN` | `-5` | Nonblocking operation has no data yet |

## Resource Model

ByteBandit uses Linux-style integer file descriptors for streams and files.
`0`, `1`, and `2` are standard input, console output, and error output.
Windows-style kernel objects use opaque `bb_handle_t` values with an explicit
`bb_object_type_t` and rights mask. Handles are reserved for processes,
threads, events, files, buffers, and future windows; applications must not
interpret their numeric values.

## Kernel API

### `bb_api_get_info(bb_system_info_t *info)`

Returns API version, PIT tick count, physical frame totals, and heap usage.
Returns `BB_STATUS_INVALID` for a null pointer.

### `bb_api_write(int fd, const char *buffer, uint32_t length)`

Writes bytes to the serial and VGA console. Valid descriptors are
`BB_CONSOLE_FD` (`1`) and `BB_ERROR_FD` (`2`). The return value is the number
of bytes written or a negative status code.

### `bb_api_task_list(void)`

Prints the registered task table, including PID, name, and state.

### `bb_api_registry_query(const char *path, char *buffer, uint32_t length)`

Reads a registry value from a kernel-managed key path, such as
`HKLM/System/Console` or `HKLM/System/ApiVersion`. Returns the value length on
success, `BB_STATUS_NOT_FOUND` when no key matches, and `BB_STATUS_INVALID`
when the input is malformed.

### `bb_api_registry_set(const char *path, const char *value)`

Creates or updates a registry entry. Values are kept in the RAM-backed registry
and are intended for runtime configuration, not persistent storage.

### `bb_api_vfs_list(void)`

Prints the current list of VFS entries, including file size and mode metadata.
This is the kernel-facing API equivalent of the shell `files` / `ls` commands.

### `bb_read(fd, buffer, length)`

The user syscall wrapper reads available bytes from `BB_STDIN_FD` without
blocking. Current input is COM1. It returns `BB_STATUS_AGAIN` when no byte is
available.

### `bb_api_print_version(void)`

Prints the API name and version to both console devices.

## User Syscall ABI

The version `0.3` user wrappers expose `bb_read`, `bb_write`, `bb_close`,
`bb_open`, `bb_getpid`, `bb_getuid`, `bb_setuid`, `bb_getgid`, `bb_setgid`,
`bb_uptime_ticks`, `bb_sleep_ms`, `bb_get_system_info`, `bb_registry_query`,
`bb_registry_set`, `bb_list_files`, `bb_chmod`, `bb_spawn`, `bb_wait`, and
`bb_exit`. File and process creation calls remain `BB_STATUS_NOSYS` until
those subsystems land. Application guidance and a complete example are in
[CLI_APP_DEVELOPMENT.md](CLI_APP_DEVELOPMENT.md).

## Serial Command Console

Run QEMU with `-serial stdio`, then type commands at the `bb>` prompt.

| Command | Purpose |
|---|---|
| `help` | List available commands |
| `version` | Print ByteBandit API version |
| `info` | Print ticks, physical memory, and heap statistics |
| `mem` | Alias for `info` |
| `free` | Print memory and heap statistics |
| `ls` / `files` | List VFS files |
| `stat PATH` | Show file metadata and ownership |
| `cat PATH` / `head PATH` | Read a VFS file |
| `touch PATH` | Create an empty RAM-backed file |
| `write PATH TEXT` | Replace a file with text |
| `cp SOURCE DEST` | Copy a file |
| `mkdir PATH` | Create a directory |
| `rm PATH` | Remove a file |
| `pwd` | Print the current directory (`/`) |
| `ps` / `tasks` | List registered tasks |
| `uptime` | Print kernel uptime |
| `uname` | Print kernel identity |
| `whoami` / `id` | Print current identity |
| `env` / `registry` | Print registry-backed system values |
| `mount` | Print mounted filesystem view |
| `echo TEXT` | Write text to both console devices |
| `clear` | Clear the VGA text display |
| `sync` | Commit the current filesystem state to the persistent volume |
| `sleep MS` | Pause the console task for milliseconds |
| `kill PID SIG` | Send a signal to a permitted task |
| `shutdown` / `poweroff` | Power off through the PC/QEMU power port |
| `reboot` / `restart` | Restart through the keyboard controller |
| `halt` | Disable interrupts and halt the CPU |
| `true` / `false` | Return shell-style success/failure output |
| `files` | List built-in VFS files |
| `registry` | List registry key/value entries |

Input is polled from COM1 between timer wakeups. The console currently has a
128-byte command line buffer and supports backspace, carriage return, and
printable ASCII input.

## User and Permission Model

ByteBandit now tracks minimal Linux-like user identities for tasks. Each task has
an effective UID/GID pair and a default root identity (UID 0, GID 0). Non-root
processes are denied access to VFS objects unless their UID/GID matches the
owner or group bits encoded in the node's mode. The permission check is the same
model used by Unix: owner/group/other triplets with read, write, and execute
bits.

The kernel exposes the identity through the syscall layer and the console's
`whoami` / `id` commands, which now print the current real/effective identity.
Filesystem path traversal requires execute access on each parent directory;
creating, deleting, or making directories also requires parent write access.
Only root may modify registry values or change another task's identity; signal
delivery is limited to root or tasks with the same effective UID.

## ByteFS Filesystem Foundation

The filesystem root is implemented as a lightweight inode-based filesystem
model called ByteFS. When an ATA hard disk is attached, ByteFS stores inode
metadata and fixed-size file payloads in two alternating volume slots inside
the ByteFS partition beginning at LBA 2048. Each save writes the inactive slot
and commits it with a generation
header last, allowing boot to select the newest complete slot after an
interrupted write. Each slot is also protected by a CRC32 over its metadata and
payload sectors. Without an ATA disk, it falls back to the volatile in-memory
backend so the boot ISO remains usable.

The current ByteFS contract is intentionally simple:
- directory entries are keyed by path
- each node has a parent, owner, group, mode, and size
- file data is stored in a fixed in-memory payload buffer
- `vfs_open`, `vfs_read`, `vfs_write`, `vfs_mkdir`, and `vfs_unlink` operate on
  the same inode table for all runtime filesystem actions

## VFS and Registry

The current VFS provides Linux-style read/write descriptors for:

- `/etc/motd`
- `/etc/system.conf`
- Runtime-created files under the RAM-backed root filesystem

The registry uses Windows-inspired hierarchical paths such as
`HKLM/System/Console`, but keeps Linux-like status codes and explicit API
boundaries. Initial values include console mode, graphics mode, and API
version. Registry values are persisted in `/etc/registry.db` when a writable
ByteFS partition is available, and fall back to defaults in volatile mode.

User applications use `bb_open`, `bb_read`, `bb_write`, and `bb_close` for VFS access, and
`bb_registry_query` or `bb_registry_set` for configuration access.

The public descriptor API is intentionally independent of the storage backend.
The standard build creates `output/bytebandit-data.img` with an MBR partition
table, and the standard QEMU run target attaches it as the writable hard disk.
Keep this file between boots
to preserve filesystem contents. Delete it only when a fresh ByteFS volume is
desired.

## Linux Command Coverage

The shell intentionally starts with native commands that can be supported by
the current kernel. Commands such as `grep`, `cp`, `mv`, `rm`, `mkdir`,
`chmod`, `kill`, `sh`, networking tools, and package tools require writable
storage, process launching, permissions, or network drivers and are planned
as later subsystems rather than being fake aliases.

## Future ABI Work

The following services are intentionally incomplete in version `0.3`:

- Partitioning and a richer persistent block filesystem
- Process creation and wait handles
- Registry persistence and access permissions
- Signals and wait queues
- Device discovery and permissions
- A keyboard driver

New services should receive explicit API versioning and documented status codes
before they become user-visible contracts.
