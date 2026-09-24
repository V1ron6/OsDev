# ByteBandit System APIs

## Scope

The ByteBandit API is the headless operating-system interface for the current
x86 kernel. It is designed for serial-console operation first; no GUI is
required or provided.

The public declarations are in [include/bb_api.h](include/bb_api.h).
User-space syscall wrappers are in [include/bb_syscall.h](include/bb_syscall.h).

## Version

The current API version is `0.1`. The version constants are `BB_API_MAJOR` and
`BB_API_MINOR`.

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

### `bb_read(fd, buffer, length)`

The user syscall wrapper reads available bytes from `BB_STDIN_FD` without
blocking. Current input is COM1. It returns `BB_STATUS_AGAIN` when no byte is
available.

### `bb_api_print_version(void)`

Prints the API name and version to both console devices.

## User Syscall ABI

The version `0.1` user wrappers expose `bb_read`, `bb_write`, `bb_close`,
`bb_open`, `bb_getpid`, `bb_uptime_ticks`, `bb_sleep_ms`,
`bb_get_system_info`, `bb_spawn`, `bb_wait`, and `bb_exit`. File and process
creation calls currently return `BB_STATUS_NOSYS` until their subsystems land.
Application guidance and a complete example are in
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
| `true` / `false` | Return shell-style success/failure output |
| `files` | List built-in VFS files |
| `registry` | List registry key/value entries |

Input is polled from COM1 between timer wakeups. The console currently has a
128-byte command line buffer and supports backspace, carriage return, and
printable ASCII input.

## VFS and Registry

The current VFS provides Linux-style read/write descriptors for:

- `/etc/motd`
- `/etc/system.conf`
- Runtime-created files under the RAM-backed root filesystem

The registry uses Windows-inspired hierarchical paths such as
`HKLM/System/Console`, but keeps Linux-like status codes and explicit API
boundaries. Initial values include console mode, graphics mode, and API
version. Registry values currently live in memory and reset at boot.

User applications use `bb_open`, `bb_read`, `bb_write`, and `bb_close` for VFS access, and
`bb_registry_query` or `bb_registry_set` for configuration access.

The current filesystem is volatile: contents are lost at reboot. The public
descriptor API is intentionally independent of the storage backend, so a later
ATA/IDE driver and persistent filesystem can replace the RAM backend.

## Linux Command Coverage

The shell intentionally starts with native commands that can be supported by
the current kernel. Commands such as `grep`, `cp`, `mv`, `rm`, `mkdir`,
`chmod`, `kill`, `sh`, networking tools, and package tools require writable
storage, process launching, permissions, or network drivers and are planned
as later subsystems rather than being fake aliases.

## Future ABI Work

The following services are intentionally incomplete in version `0.1`:

- Writable files and persistent block storage
- Process creation and wait handles
- Registry persistence and access permissions
- Signals and wait queues
- Device discovery and permissions
- A keyboard driver

New services should receive explicit API versioning and documented status codes
before they become user-visible contracts.
