# ByteBandit OS - Developer Quick Reference

## Essential Commands

### Build
```bash
make all              # Build everything (clean + bootloader + kernel)
make clean            # Remove build artifacts
make info             # Show build configuration
make disasm           # Generate kernel disassembly (build/kernel.asm)
```

### Run & Debug
```bash
make run              # Build and boot in QEMU (serial to stdio)
make debug            # Boot in QEMU with GDB server (-s -S)

# Custom QEMU
qemu-system-i386 -hda build/bytebandit.img -serial stdio
qemu-system-i386 -hda build/bytebandit.img -serial file:/tmp/serial.log

# GDB debugging
gdb build/kernel.elf
  target remote :1234
  break kernel_main
  continue
```

### Inspection
```bash
# View symbols
i686-elf-objdump -t build/kernel.elf | grep -E "(kernel_main|_start|__bss)"

# Disassembly
i686-elf-objdump -d build/kernel.elf | head -50

# Binary inspection
hexdump -C build/bootloader.bin
wc -c build/bootloader.bin   # Should be exactly 512

# Check section headers
i686-elf-objdump -h build/kernel.elf
```

## Key Files & Their Purposes

| File | Purpose | Key Points |
|------|---------|------------|
| boot/boot.asm | BIOS bootloader | 512 bytes, MBR, loads kernel, enables PM |
| kernel/entry.asm | Kernel entry point | Clears .bss, sets up stack, calls kernel_main() |
| kernel/main.c | Kernel initialization | First C code executed, sets up drivers |
| kernel/panic.c | Panic handler | Red screen, halts CPU |
| drivers/vga.c | VGA terminal driver | 80×25 text output, color, cursor |
| include/x86.h | Hardware abstractions | Port I/O, control regs, instructions |
| kernel.ld | Memory layout | Defines section placement, symbol boundaries |
| Makefile | Build system | Compiles, links, creates bootable image |

## Memory Map

```
Virtual Address | Physical | Description
================|==========|================
0x00000000      | 0x000000 | Null descriptor (segfaults here)
0x00007C00      | 0x007C00 | Bootloader (512 bytes) ← BIOS loads here
0x00010000      | 0x010000 | Kernel _start ← Bootloader jumps here
0x0008F000      | 0x08F000 | Kernel stack base (grows down)
0x00100000      | 0x100000 | Traditional paging boundary
```

## GDT Layout (Bootloader)

```
Offset | Selector | Descriptor | Purpose
=======|==========|============|==========
0x00   | N/A      | 0x0000...  | NULL (required)
0x08   | 0x0008   | 0x00CF9A.. | CODE (Ring 0, 32-bit, base=0, limit=4GB)
0x10   | 0x0010   | 0x00CF92.. | DATA (Ring 0, 32-bit, base=0, limit=4GB)
```

## Common Development Tasks

### Add a New Driver

1. Create header in `include/`
2. Create implementation in `drivers/`
3. Update `Makefile` KERNEL_C_SOURCES
4. `#include` in `kernel/main.c`
5. Call init function from `kernel_main()`

Example:
```bash
# Create serial driver
touch include/serial.h drivers/serial.c

# Add to Makefile:
# KERNEL_C_SOURCES += drivers/serial.c

# Implement
echo '#include "serial.h"
void serial_init(void) { ... }' > drivers/serial.c
```

### Debug Boot Issue

See [DEBUGGING.md](DEBUGGING.md) for detailed procedure:
1. Add serial output to bootloader
2. Verify GDT at runtime
3. Test minimal PM transition
4. Use GDB with QEMU

### Profile Code Size

```bash
# Bootloader size (must be ≤ 512)
wc -c build/bootloader.bin

# Kernel sections
i686-elf-objdump -h build/kernel.elf

# Total image size
wc -c build/bytebandit.iso

# Disassembly by section
make disasm && grep "Disassembly\|^0001" build/kernel.asm | head -30
```

### View Boot Sequence

```bash
# Boot and capture serial log
timeout 3 qemu-system-i386 -hda build/bytebandit.img -serial file:/tmp/boot.log || true
# Examine log
hexdump -C /tmp/boot.log | head -40
# (Look for SeaBIOS messages and execution path)
```

## Compiler & Linker Flags Explained

### GCC Flags
- `-ffreestanding`: Don't assume standard C library
- `-fno-builtin`: Don't use built-in functions (safer)
- `-Wall -Wextra -pedantic`: Strict warnings (catch bugs)
- `-std=c99`: C99 standard (not C11 or later)
- `-m32 -march=i386`: 32-bit, 386 baseline (maximum compatibility)

### Linker Flags
- `-T kernel.ld`: Use custom linker script for memory layout
- `-m elf_i386`: Generate 32-bit ELF (matching i686)

### NASM Flags (Bootloader)
- `-f bin`: Output raw binary (for bootloader)

### NASM Flags (Kernel Entry)
- `-f elf32`: Output 32-bit ELF object file (for linking with C code)

## Calling Convention (x86 32-bit cdecl)

When calling `int foo(int a, int b)`:
```
ESP before call    [return address]
                   [arg b] ← ESP+8  (caller passed this)
                   [arg a] ← ESP+4  (caller passed this)
Call foo()
ESP after call ← Now points to return address
Inside foo():
  mov eax, [esp+4]  → arg a
  mov eax, [esp+8]  → arg b
  mov [esp-4], eax  → local variable
  ret               → returns int in EAX
```

Caller cleanup: `add esp, 8` (2 args × 4 bytes)

## Interrupt/Exception Frame

When CPU exception occurs:
```
ESP →  [EFLAGS]
       [CS]
       [EIP] ← Where exception occurred
       [Error Code] (some exceptions only)
```

Handler receives these on stack.

## Testing Checklist

Before committing changes:

- [ ] `make clean && make all` builds with no errors
- [ ] Only ≤2 expected linker warnings
- [ ] All new C code compiles with `-Wall -Wextra`
- [ ] Bootloader is exactly 512 bytes
- [ ] Boot signature (0xAA55) at offset 510
- [ ] Binary loads without misalignment
- [ ] No hardcoded addresses (use linker symbols)
- [ ] All functions documented
- [ ] No stdlib function calls

## Troubleshooting

**Build error: "Bootloader exceeds 512 bytes"**
- Check current size: `wc -c build/bootloader.bin`
- Remove unnecessary code or optimize assembly
- Use `objdump` to find bloated sections

**Build error: "undefined reference to 'kernel_main'"**
- Verify `kernel/main.c` exists and has `void kernel_main(void) { ... }`
- Check `kernel.ld` ENTRY(_start) is correct
- Ensure entry.asm calls `kernel_main`

**QEMU hangs at boot**
- Check serial log: `qemu-system-i386 ... -serial file:/tmp/log.txt`
- Enable GDB: `make debug` and step through bootloader
- Verify binary layout: `hexdump -C build/bytebandit.iso | head -30`

**GDB "target remote" fails**
- Ensure QEMU started with `-s -S` flags
- Check connection: `netstat -an | grep 1234`
- Try `target remote localhost:1234` instead

## References

**Documentation**
- [ARCHITECTURE.md](ARCHITECTURE.md) - Design & decisions
- [DEBUGGING.md](DEBUGGING.md) - Boot issue diagnosis
- [README.md](README.md) - Getting started
- [PHASE1_REPORT.md](PHASE1_REPORT.md) - Completion summary

**External Resources**
- Intel IA-32 Manual: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html
- OSDev Wiki: https://wiki.osdev.org/
- NASM Manual: https://www.nasm.us/doc/
- System V ABI i386: https://refspecs.linuxbase.org/elf/i386-supplement.pdf

**Tools**
- NASM (assembler): `nasm -h`
- GCC: `gcc --help-variable=i686`
- Binutils: `objdump --help`, `objcopy --help`
- QEMU: `qemu-system-i386 -help | head -50`

## Editor Configuration

### Vim
```vim
" .vimrc additions for kernel development
set tabstop=4
set shiftwidth=4
set expandtab
set colorcolumn=80
syntax on

" Fold assembly comments
au FileType asm set commentstring=;%s
```

### VS Code
```json
{
  "[c]": {
    "editor.defaultFormatter": "xaver.clang-format",
    "editor.formatOnSave": true,
    "editor.formatOnPaste": true
  },
  "[asm]": {
    "editor.tabSize": 2
  }
}
```

---

**Keep this as a handy reference while developing!**

Last updated: Phase 1 completion
