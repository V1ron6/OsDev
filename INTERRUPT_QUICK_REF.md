# ByteBandit OS - Interrupt System Quick Reference

## Vector Map

```
CPU Vectors (x86-32):
  0x00-0x1F: CPU Exceptions (32 total)
  0x20-0x2F: Hardware IRQs (16 total, after PIC remapping)
  0x30-0xFF: Software/Custom (available)

Default PIC Mapping (DANGEROUS - before remapping):
  Master IRQ 0-7  → CPU vectors 0x08-0x0F (CONFLICTS with exceptions!)
  Slave IRQ 8-15  → CPU vectors 0x10-0x17

ByteBandit OS PIC Mapping (AFTER INIT):
  Master IRQ 0-7  → CPU vectors 0x20-0x27  ✓
  Slave IRQ 8-15  → CPU vectors 0x28-0x2F  ✓
```

## CPU Exceptions (0-31)

| Vector | Mnemonic | Error Code | Description |
|--------|----------|:----------:|-------------|
| 0x00 | #DE | No | Divide by Zero |
| 0x01 | #DB | No | Debug |
| 0x02 | NMI | No | Non-Maskable Interrupt |
| 0x03 | #BP | No | Breakpoint |
| 0x04 | #OF | No | Overflow |
| 0x05 | #BR | No | Bound Range Exceeded |
| 0x06 | #UD | No | **Invalid Opcode** ⚠️ |
| 0x07 | #NM | No | Device Not Available |
| 0x08 | #DF | Yes | **Double Fault** (CRITICAL) ⚠️ |
| 0x09 | (obsolete) | No | Coprocessor Segment |
| 0x0A | #TS | Yes | Invalid TSS |
| 0x0B | #NP | Yes | **Segment Not Present** ⚠️ |
| 0x0C | #SS | Yes | **Stack Fault** ⚠️ |
| 0x0D | #GP | Yes | **General Protection** ⚠️ |
| 0x0E | #PF | Yes | Page Fault (paging not yet enabled) |
| 0x0F | (reserved) | No | Reserved |
| 0x10 | #MF | No | Floating Point Error |
| 0x11 | #AC | Yes | Alignment Check |
| 0x12 | #MC | No | Machine Check |
| 0x13 | #XF | No | SIMD FP Exception |
| 0x14-0x1F | (reserved) | No | Reserved |

**Legend**: ⚠️ = Most likely to occur / easiest to trigger for testing

## Hardware IRQs (Master PIC)

| IRQ | Vector | Device | Status |
|-----|--------|--------|--------|
| 0 | 0x20 | PIT Timer | To implement |
| 1 | 0x21 | Keyboard | To implement |
| 2 | 0x22 | Slave PIC | Reserved (cascade) |
| 3 | 0x23 | COM2/4 | Future |
| 4 | 0x24 | COM1/3 | Future |
| 5 | 0x25 | Parallel Port | Future |
| 6 | 0x26 | Floppy | Future |
| 7 | 0x27 | Parallel Port | Future |

## Hardware IRQs (Slave PIC)

| IRQ | Vector | Device | Status |
|-----|--------|--------|--------|
| 8 | 0x28 | RTC | Future |
| 9 | 0x29 | (redirect IRQ2) | Future |
| 10 | 0x2A | (available) | Future |
| 11 | 0x2B | (available) | Future |
| 12 | 0x2C | PS/2 Mouse | Future |
| 13 | 0x2D | FPU | Future |
| 14 | 0x2E | IDE Primary | Future |
| 15 | 0x2F | IDE Secondary | Future |

## Key Functions

### Serial Debug Output
```c
#include "serial.h"

serial_init();
serial_puts("Hello\n");
serial_printf("Value: 0x%x\n", val);
```

### IDT Management
```c
#include "idt.h"

idt_init();  // Initialize all 256 entries
idt_set_gate(vector, handler_addr, IDT_GATE_INTR_32, 0);  // Install handler
```

### Exception Handling
```c
// Exceptions automatically routed to exception_handler() in isr.c
// No manual installation needed - already in IDT
```

### PIC Management
```c
#include "pic.h"

pic_init();           // Remap to 0x20-0x2F
pic_unmask_irq(1);    // Enable keyboard
pic_send_eoi(irq);    // End-of-interrupt
pic_mask_irq(irq);    // Disable IRQ
```

### IRQ Handler Registration
```c
#include "irq.h"

irq_init();                          // Install IRQ stubs
irq_install_handler(1, keyboard_handler);  // Register handler
irq_enable(1);                       // Unmask at PIC
enable_interrupts();                 // Enable at CPU (cli -> sti)
```

## Common Patterns

### Triggering an Exception (for testing)
```c
int x = 1 / 0;  // Triggers #DE (divide by zero)
int *p = NULL; *p = 0;  // Triggers #PF (page fault)
__asm__("ud2");  // Triggers #UD (invalid opcode)
```

### Simple IRQ Handler
```c
void timer_handler(uint8_t irq) {
    ticks++;  // Do work
    // irq_handler() in hwirq.c calls pic_send_eoi() automatically
}

// During init:
irq_install_handler(0, timer_handler);
irq_enable(0);
enable_interrupts();
```

### Safe Panic
```c
#include "panic.h"

if (error_condition) {
    panic_printf("Something bad: error=%d", error);
    // Never returns - disables interrupts, prints to serial/VGA, halts
}
```

## Stack Frame Details

### CPU-Provided Frame (on interrupt)
```
[ESP+12]: EFLAGS (flags register)
[ESP+8]:  CS     (code segment)
[ESP+4]:  EIP    (return address)
[ESP+0]:  Error Code (only some exceptions)
```

### With Error Code (8 bytes)
```
#8 Double Fault, #10-13 TSS/NP/SS/GP, #17 AC, #14 PF
```

### Without Error Code (4 bytes)
```
All others - stub must push dummy 0 for alignment
```

## Debug Checklist

### Boot Not Working?
- [ ] Bootloader loads (check BIOS output)
- [ ] Kernel entry at 0x10000 (check objdump)
- [ ] GDT installed (check CR0.PE)
- [ ] IDT installed (check IDTR with GDB)
- [ ] PIC remapped (check OC W2 register)
- [ ] Use GDB: `make debug` then `gdb build/kernel.elf`

### Exception Not Caught?
- [ ] IDT vector populated? (`info registers idtr` in GDB)
- [ ] Handler address correct?
- [ ] Interrupts enabled? (`enable_interrupts()`)
- [ ] Serial output for diagnostics

### IRQ Not Firing?
- [ ] PIC initialized? (`pic_init()` called)
- [ ] Handler installed? (`irq_install_handler()`)
- [ ] IRQ unmasked? (`irq_enable(n)`)
- [ ] CPU interrupts enabled? (`enable_interrupts()`)
- [ ] EOI sent? (automatic in our dispatcher)
- [ ] Device interrupt configured? (driver-specific)

## Code Organization

```
include/
  idt.h        - IDT interface
  isr.h        - Exception handler
  pic.h        - PIC management
  irq.h        - IRQ handler registration
  serial.h     - Debug output
  panic.h      - Crash handler

arch/x86/
  idt.c        - IDT implementation (256 entries)
  isr.c        - Exception dispatcher (0-31)
  exceptions.asm - Exception stubs (0-31)
  pic.c        - PIC driver (remapping + EOI)
  hwirq.c      - IRQ dispatcher (0-15)
  irq.asm      - IRQ stubs (0-15)

drivers/
  serial.c     - COM1 I/O (115200 baud)

kernel/
  main.c       - Bootstrap sequence
  panic.c      - Crash handler
```

## Essential Constants

```c
#define IDT_ENTRIES        256    // All possible vectors
#define IDT_GATE_INTR_32   0x0E   // 32-bit interrupt gate
#define IDT_GATE_TRAP_32   0x0F   // 32-bit trap gate
#define IDT_ATTR_PRESENT   0x80   // Descriptor present
#define IDT_ATTR_DPL_0     0x00   // Ring 0 (kernel)

#define PIC_MASTER_CMD     0x20   // Master command port
#define PIC_MASTER_DATA    0x21   // Master IRQ mask
#define PIC_SLAVE_CMD      0xA0   // Slave command port
#define PIC_SLAVE_DATA     0xA1   // Slave IRQ mask

#define OCW2_EOI           0x20   // Non-specific EOI
```

## Performance Notes

- Exception/IRQ dispatch: ~50-100 CPU cycles
- PIC EOI: 2-3 port I/O operations
- No interrupt nesting (IF cleared on entry)
- Serial output: ~100 microseconds per character

## Safety Principles

1. **Never leave entries uninitialized** - causes #UD or triple fault
2. **Always send EOI** - missing EOI hangs IRQ line
3. **Disable interrupts on entry** - interrupt gates do this
4. **Safe CPU halt** - use `cli; hlt; jmp .` to prevent CPU restart

---

**Reference**: x86 Intel 80386 Programmer's Reference Manual
**Last Updated**: May 14, 2026
