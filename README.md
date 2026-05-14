# ByteBandit OS - Getting Started

## Overview

ByteBandit OS is a modular, educational x86 operating system kernel built from scratch. It focuses on correctness, maintainability, and clear architecture over feature completeness.

**Current Phase**: Bootstrap infrastructure complete. Kernel execution in QEMU being debugged.

## Quick Start

### Prerequisites

Linux system (Ubuntu 24.04 tested) with:
```bash
sudo apt install -y nasm gcc build-essential gcc-multilib qemu-system-x86
```

The project provides cross-compiler wrappers for i686-elf toolchain.

### Build the OS

```bash
# Build everything (bootloader + kernel + disk image)
make all

# Build and display info
make info

# Clean build artifacts
make clean
```

### Run in QEMU

```bash
# Boot OS (serial output to console)
make run

# Note: Currently boots to debugging stage (triple fault investigation)
```

### Expected Output (When Boot Works)

```
ByteBandit OS v0.1.0
Kernel started successfully.
CPU in protected mode: 0x10000011

Bootstrap sequence:
  [OK] Real mode to protected mode transition
  [OK] Kernel loaded at 0x10000
  [OK] Stack initialized
  [OK] .bss cleared
  [OK] VGA terminal active

Next steps:
  - Implement GDT directly
  - Set up IDT infrastructure
  - CPU exception handlers

[Kernel halting - waiting for interrupts]
```

## Project Structure

```
boot/               Bootloader (512 bytes, BIOS-bootable)
kernel/             Kernel core (entry point, main, panic handling)
drivers/            Hardware drivers (VGA terminal, serial)
arch/x86/           x86-specific code (GDT, IDT, paging - future)
mm/                 Memory management (future)
fs/                 Filesystem code (future)
libk/               Freestanding C library helpers
include/            Header files
build/              Build artifacts (generated)
kernel.ld           Linker script (memory layout)
Makefile            Build system
ARCHITECTURE.md     Detailed architecture documentation
DEBUGGING.md        Boot issue diagnosis and debugging steps
```

## Key Concepts

### Real Mode → Protected Mode Transition

The bootloader:
1. Starts in real mode (BIOS environment)
2. Loads GDT with flat memory model (code + data segments)
3. Sets PE bit in CR0 to enable protected mode
4. Performs far jump to flush CPU pipeline
5. Initializes segment registers for flat addressing
6. Jumps to kernel entry point at 0x10000

### Memory Layout

```
0x00007C00 - 0x00007DFF:  Bootloader (512 bytes)
0x00010000 - 0x0002FFFF:  Kernel code/data
0x0008F000 - 0x00092FFF:  Kernel stack (16 KB)
```

### Calling Conventions

32-bit x86 cdecl:
- Arguments passed on stack (right-to-left)
- Return value in EAX
- Caller cleans up stack
- All registers except ESP, EBP volatile

## Build System Details

### Makefile Targets

| Target | Purpose |
|--------|---------|
| `all` | Build bootloader, kernel, disk image |
| `run` | Build and boot in QEMU |
| `debug` | Boot in QEMU with GDB server (-s -S) |
| `disasm` | Generate kernel disassembly |
| `clean` | Remove build artifacts |
| `info` | Show build configuration |

### Compilation Flags

**C Compiler** (i686-elf-gcc → gcc -m32):
- `-ffreestanding`: No standard library
- `-fno-builtin`: No built-in functions  
- `-std=c99`: C99 standard (not C11+)
- `-Wall -Wextra -pedantic`: Strict warnings
- `-m32 -march=i386`: 32-bit i386 baseline

**Linker** (i686-elf-ld → ld -m elf_i386):
- Uses `kernel.ld` for memory layout
- `-T kernel.ld`: Load custom linker script
- Forces `_start` to be first section

**Assembler** (nasm):
- `-f bin`: Bootloader (raw binary)
- `-f elf32`: Kernel entry (ELF object)

## Development Workflow

### Adding a New Module

1. **Create source file**:
   ```bash
   touch drivers/my_driver.c
   echo '#include "my_driver.h"' > include/my_driver.h
   ```

2. **Update Makefile**:
   ```makefile
   # Add to KERNEL_C_SOURCES:
   KERNEL_C_SOURCES += drivers/my_driver.c
   ```

3. **Build and test**:
   ```bash
   make clean && make all
   make run  # Test in QEMU
   ```

### Debugging with GDB

```bash
# Terminal 1: Start QEMU with GDB server
make debug

# Terminal 2: GDB session
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Serial Logging

Serial port COM1 (0x3F8) is configured but not heavily used yet.

To add logging:
```c
#include "x86.h"
outb(0x3F8, 'A');  // Write character to COM1
```

## Common Tasks

### View Boot Log
```bash
timeout 3 qemu-system-i386 -hda build/bytebandit.img -serial file:/tmp/log.txt
cat /tmp/log.txt
```

### Generate Kernel Disassembly
```bash
make disasm
less build/kernel.asm
```

### Verify Boot Signature
```bash
hexdump -C build/bootloader.bin | tail -3
# Last line should show: ... 55 aa (boot signature)
```

### Check Symbol Table
```bash
i686-elf-objdump -t build/kernel.elf | grep -E "(_start|kernel_main|__bss)"
```

## Known Limitations

1. **Boot Issue**: Triple fault during protected mode transition (under investigation)
2. **No Interrupts**: IDT/IRQ handling not yet implemented
3. **No Paging**: Direct memory access only, no virtual memory
4. **No Multitasking**: Single-threaded kernel only
5. **Limited Drivers**: Only VGA terminal (no keyboard, disk, network, etc.)
6. **No Filesystem**: No disk access beyond bootloader

## Next Milestones

### Phase 2: Interrupt Foundation
- IDT setup and CPU exception handlers
- PIC configuration and hardware IRQ handling
- Timer and keyboard input support

### Phase 3: Memory Management  
- Physical frame allocator
- Paging infrastructure
- Higher-half kernel mapping

### Phase 4: Advanced Features
- ELF loader for modular kernels
- Multitasking/scheduling
- User mode and syscalls

## Contributing

This project follows kernel architecture best practices:
- Each subsystem is modular and testable
- Low-level code has detailed comments explaining WHY hardware operations are necessary
- Assembly is avoided unless absolutely necessary
- Memory safety is paramount (no unsafe pointer casts without explicit justification)
- Debugging infrastructure is prioritized

When adding features:
1. Ensure it compiles cleanly with no warnings
2. Document hardware behavior and interrupt implications
3. Add diagnostic output for failures
4. Test in isolation before integrating

## References

- **Intel IA-32 Architecture Manual**: [ARK.INTEL.COM](https://ark.intel.com)
- **OSDev.org**: Community OS development resource
- **System V ABI i386**: Standard calling conventions
- **NASM Manual**: x86 assembly syntax

## Troubleshooting

### Build Errors

**Error**: `i686-elf-gcc: command not found`
```bash
# Verify cross-compiler setup
which i686-elf-gcc
# Should show: /usr/local/bin/i686-elf-gcc

# If missing, the Makefile will fail
# Verify toolchain installation
gcc -m32 --version  # Should work if gcc-multilib installed
```

**Error**: `kernel.ld: No such file or directory`
```bash
# Ensure you're in the project root
pwd  # Should be /workspaces/OsDev
ls kernel.ld  # Should exist
```

### Runtime Issues

**QEMU hangs during boot**:
- See [DEBUGGING.md](DEBUGGING.md) for triple fault investigation
- Enable serial logging to see where execution stops
- Use `make debug` with GDB to step through bootloader

**VGA output not showing**:
- VGA terminal writes to 0xB8000 (text mode buffer)
- In `-nographic` mode, output won't show unless captured to file
- Use serial output for headless debugging

## License

Check [LICENSE](LICENSE) file for project licensing.

---

**Status**: Phase 1 (Bootstrap) substantially complete | Phase 2 (Interrupts) in planning

**Last Updated**: May 14, 2026

**Maintainer**: OS Development Team
