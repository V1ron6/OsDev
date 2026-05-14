# ByteBandit OS

ByteBandit OS is a modular educational 32-bit x86 operating system built from scratch for BIOS systems. This repository focuses on correctness, observability, and incremental growth before advanced features.

## Current Stage

- 512-byte boot sector loads the kernel and transitions from real mode to protected mode.
- Minimal GDT is installed in the bootloader.
- Kernel entry clears `.bss`, sets a stack, and calls `kernel_main`.
- VGA text output and serial COM1 logging are available immediately.
- An IDT with CPU exception handlers (divide-by-zero, invalid opcode, GPF) is installed.
- Kernel panic halts safely with diagnostic output.

## Directory Layout

```
boot/        Bootloader and protected-mode transition
arch/x86/    GDT/IDT/ISR stubs and x86-specific helpers
kernel/      Kernel entry and initialization
drivers/     VGA/serial drivers
mm/          Memory management (future)
fs/          Filesystems (future)
libk/        Freestanding libc-style helpers
include/     Shared headers
build/       Build output (ignored)
```

## Build Requirements

- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy`
- `nasm`
- `qemu-system-i386`

## Build

```sh
make
```

The build produces `build/os.img` (1.44MB floppy image), `build/kernel.elf` (symbols for GDB), and `build/kernel.bin`.

## Run (QEMU)

```sh
make run
```

Serial logs are exposed through `-serial stdio`.

## Debug (GDB)

```sh
make debug
```

In another terminal:

```sh
i686-elf-gdb build/kernel.elf
target remote :1234
```

## Notes & Roadmap

- The current bootloader loads a flat binary kernel. A second-stage loader and ELF parsing are required before adding advanced subsystems.
- Next steps should include: expanded IDT coverage, PIC remapping, hardware IRQ handling, and a physical memory allocator.
