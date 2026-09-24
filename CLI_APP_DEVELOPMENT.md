# ByteBandit CLI Application Development

This document describes the first application contract for ByteBandit. The
current system is text-only and communicates with applications through the
`int 0x80` syscall ABI. GUI applications are not supported yet, but future GUI
code can reuse the process, timing, and system-information calls.

## Public Header

Application code should include:

```c
#include <bb_syscall.h>
```

The header is freestanding and provides inline wrappers. It does not depend on
host libc or a desktop operating system.

## Available Calls

| Wrapper | Service | Result |
|---|---|---|
| `bb_read(fd, buffer, length)` | Nonblocking standard input | Bytes read or `BB_STATUS_AGAIN` |
| `bb_write(fd, buffer, length)` | Write console bytes | Bytes written or negative status |
| `bb_open(path, flags, mode)` | Open a filesystem object | `BB_STATUS_NOSYS` currently |
| `bb_close(fd)` | Close a descriptor | `BB_STATUS_NOSYS` currently |
| `bb_getpid()` | Get current process ID | PID |
| `bb_uptime_ticks()` | Read 100 Hz timer ticks | Tick count |
| `bb_sleep_ms(milliseconds)` | Wait using kernel timer | Status code |
| `bb_get_system_info(&info)` | Read memory/API information | Status code |
| `bb_spawn(path, argv)` | Start a process | `BB_STATUS_NOSYS` currently |
| `bb_wait(process, &status)` | Wait for a process handle | `BB_STATUS_NOSYS` currently |
| `bb_exit(status)` | Terminate the process | Does not return |

The first built-in file paths are `/etc/motd` and `/etc/system.conf`. Files can
be created and modified in the RAM-backed filesystem, but contents are lost at
reboot until a block device and persistent filesystem are available.

Registry paths use a Windows-like hierarchy, for example
`HKLM/System/Console`, while returning the same Linux-style negative status
codes as the rest of the ABI.

Use `BB_CONSOLE_FD` for normal output and `BB_ERROR_FD` for error output.
Use `BB_STDIN_FD` for input. Reads are intentionally nonblocking so a CLI
event loop can do work while waiting for input.

## Example CLI Application

```c
#include <bb_syscall.h>

static const char message[] = "hello from a ByteBandit app\n";

void app_main(void) {
    bb_write(BB_CONSOLE_FD, message, sizeof(message) - 1);
    bb_sleep_ms(1000);
    bb_exit(0);
}
```

An event-loop style input fragment looks like this:

```c
char buffer[64];
int result = bb_read(BB_STDIN_FD, buffer, sizeof(buffer));
if (result > 0) {
  bb_write(BB_CONSOLE_FD, buffer, (uint32_t)result);
}
```

Applications must be compiled as 32-bit freestanding ELF executables and must
provide their own startup entry point until the ByteBandit C runtime is added.
The current kernel has ELF segment loading support, but a complete user-process
launch path and preemptive context switch are still being completed.

The native shell currently provides Linux-like `ls`, `cat`, `head`, `pwd`,
`ps`, `free`, `uptime`, `uname`, `whoami`, `id`, `env`, `mount`, `true`, and
`false` commands. These are kernel commands while user-process launching is
being completed.

## ABI Rules

- Arguments use the i386 cdecl register convention documented in
  [BYTEBANDIT_API.md](BYTEBANDIT_API.md).
- User pointers are validated by the kernel before supported pointer-bearing
  calls copy data.
- Applications must not access kernel addresses or hardware ports directly.
- Negative results are ByteBandit status/error values.
- API version `0.1` is exposed through `bb_system_info_t`.

## Future GUI Compatibility

The syscall layer intentionally does not assume VGA. Future graphics APIs can
be added as separate versioned services for framebuffers, windows, input, and
buffers. Existing process, memory, timer, and exit calls should remain usable
by both CLI and GUI applications.
