# ByteBandit OS - Debugging Guide

## Current Boot Issue: Triple Fault During Protected Mode Transition

### Symptoms
- QEMU: "Booting from Hard Disk..." then system reboots
- Serial log shows SeaBIOS restarting in a loop
- Kernel never executes

### Root Cause Analysis

The bootloader is executing (confirmed by BIOS output), but a **triple fault** occurs during or after the protected mode transition. A triple fault triggers a system reset.

### Possible Issues

#### 1. **GDT Address Mismatch**
**Problem**: GDT descriptor loaded with real-mode address, but CPU in protected mode expects linear addresses.

**Check**:
```nasm
; In boot.asm:
lgdt [gdt_descriptor]      ; Loads real-mode address of GDT
; After entering PM, GDT still at the same physical address, so this should work...
```

**Investigation**:
- The GDT is in the bootloader itself (around offset 0x7C00 + GDT_OFFSET)
- GDT descriptor specifies base address as `dd gdt`
- In real mode, this computes correctly
- Once in protected mode, linear = physical for identity mapping, so should still work

**Potential Fix**:
Verify GDT descriptor base address is correct for physical address.

#### 2. **Invalid Segment Selector in Far Jump**
**Problem**: `jmp 0x0008:pm_entry` assumes code selector is at offset 0x0008 in GDT.

**Check GDT Layout**:
```
Offset 0x00: NULL descriptor (8 bytes)
Offset 0x08: CODE descriptor (8 bytes) ← Selector 0x0008 ✓
Offset 0x10: DATA descriptor (8 bytes) ← Selector 0x0010 ✓
```

**Investigation**:
- GDT entries are correct
- Code selector (0x0008) points to entry 1 (offset 0x08)
- Access byte: 0x9A (Present, Ring 0, Code, Execute+Read)
- Granularity byte: 0xCF (Page granular, 32-bit, Limit high=F)

Seems correct, but verify the encoding matches Intel specs.

#### 3. **Invalid Memory Address in pm_entry**
**Problem**: After far jump, addresses become invalid.

**Check**:
```asm
pm_entry:
    mov ax, 0x0010          ; Load data selector
    mov ds, ax              ; This should work...
```

The label `pm_entry` is in the bootloader code. After `jmp 0x0008:pm_entry`, execution should continue at this label.

**Potential Issue**: The `pm_entry` label location might be address 0xXXXX in real mode coordinates, but the CPU expects a linear address.

**Solution**: Use explicit far jump with absolute address:
```asm
jmp 0x0008:0x7C00+<offset>  ; Explicit linear address
```

Or use `org` directive to ensure correct addresses.

#### 4. **Stack Corruption**
**Problem**: Stack pointer (SP) points to invalid memory after mode switch.

**Current Setup**:
```asm
mov ss, ax         ; Set SS to data selector (0x0010)
mov sp, 0x7C00     ; Stack at bootloader location
```

Then in protected mode:
```asm
mov esp, 0x90000   ; Stack moved to higher address
```

This could be problematic if accessed between the SS change and ESP change.

#### 5. **Interrupts Enabled Inadvertently**
**Problem**: EFLAGS.IF bit set, causing interrupt before IDT setup.

**Check**:
```asm
cli                ; Disable interrupts at start
...
jmp 0x0008:pm_entry ; No sti before this
```

Looks correct, but verify CLI actually disables interrupts in real mode.

### Debugging Strategy

#### Step 1: Add Serial Output to Bootloader
Add debug messages at key points:
```asm
call serial_puts   ; "Entering PM..."
mov eax, cr0
or eax, 0x00000001
mov cr0, eax       ; Set PE bit
call serial_puts   ; "CR0.PE set..."
jmp 0x0008:pm_entry
call serial_puts   ; "PM_ENTRY reached..." (if execution continues)
```

#### Step 2: Minimal Protected Mode Test
Create a bootloader that:
1. Enters protected mode
2. Does NOT jump far (no pipeline flush)
3. Just hangs

If this works, the issue is in the far jump. If not, it's earlier.

#### Step 3: GDB Remote Debugging

```bash
# Terminal 1: Start QEMU with GDB server
qemu-system-i386 -hda build/bytebandit.img -s -S -nographic

# Terminal 2: Start GDB
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break *0x7c00              # Breakpoint at bootloader start
(gdb) continue
(gdb) x/i $eip                   # Examine instruction at EIP
```

#### Step 4: Memory Validation
Check actual memory contents after loading:
```
QEMU Monitor: info memory
Check: Physical 0x7C00 has bootloader code
Check: Physical 0x10000 has kernel (_start code)
```

#### Step 5: Register Inspection
At the triple fault:
- EIP should be pointing somewhere valid
- CR0.PE should be 1 (protected mode)
- CS should be valid (0x0008)
- GDTR should point to valid GDT location

### Verification Checklist

- [ ] GDT is at correct physical address (0x7C00 + offset)
- [ ] GDT descriptor base address matches GDT location
- [ ] GDT entries have correct access bytes
- [ ] GDT entries have correct granularity bytes
- [ ] Code selector (0x0008) points to correct GDT entry
- [ ] Data selector (0x0010) points to correct GDT entry
- [ ] CR0.PE is set BEFORE far jump
- [ ] Pipeline is flushed with far jump
- [ ] pm_entry is at correct address for far jump
- [ ] SP/SS transition doesn't corrupt stack
- [ ] No interrupts enabled before IDT setup
- [ ] Memory at 0x10000 contains valid kernel code

### Quick Fixes to Try

#### Option 1: Explicit Address in Far Jump
```asm
jmp 0x0008:0x7c6e  ; Use computed absolute address instead of label
```

#### Option 2: Clear EFLAGS Before Far Jump
```asm
pushf
pop eax
and eax, 0xFFFFF8FF  ; Clear TF, IF, DF, OF
push eax
popf
```

#### Option 3: Simplify GDT
Ensure GDT entries are 100% correct:
```asm
; NULL descriptor
dq 0x0000000000000000

; Code segment (Ring 0)
; Base=0, Limit=0xFFFFF, G=1(4KB), D=1(32-bit)
; P=1, DPL=0, S=1(code/data), Type=1010 (R+E)
dq 0x00CF9A000000FFFF

; Data segment (Ring 0)  
; Base=0, Limit=0xFFFFF, G=1(4KB), D=1(32-bit)
; P=1, DPL=0, S=1(code/data), Type=0010 (R+W)
dq 0x00CF92000000FFFF
```

Verify each byte:
- `00CF9A000000FFFF`
- `00CF92000000FFFF`

#### Option 4: Don't Load from Disk Initially
Embed kernel directly in bootloader or at fixed offset to eliminate disk loading as failure source.

### Serial Debugging Output

If you add serial output, use this format for easy grepping:
```asm
mov ax, '[BP'   ; Bootstrap marker
mov dx, 0x3F8   ; COM1
out dx, al
```

Then: `cat /tmp/serial.log | grep \[BP`

### Additional Resources

- **Intel IA-32 Manual Vol. 3A**: Protected mode transition
- **OSDev.org**: Bootloader pitfalls
- **QEMU Docs**: Debugging with GDB

---

## Workaround: Test Without Disk Loading

Create `boot_direct.asm` that:
1. Doesn't load from disk
2. Kernel code is statically placed in bootloader
3. Just jumps directly to kernel code in protected mode

This isolates whether issue is disk-related or boot-related.

---

**Next Step**: Enable serial debugging in bootloader and capture output at each step.
