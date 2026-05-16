# ByteBandit OS Build System
# Compiles bootloader, kernel, and creates bootable image
# Usage: make all | make clean | make run

# Toolchain configuration
CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm
OBJCOPY = i686-elf-objcopy
OBJDUMP = i686-elf-objdump

# Compiler flags
CFLAGS = -ffreestanding -fno-builtin -Wall -Wextra -pedantic
CFLAGS += -std=c99 -I. -Iinclude -m32 -march=i386

# Assembly flags
ASFLAGS = -f elf32

# Linker flags
LDFLAGS = -T kernel.ld -m elf_i386

# Directories
BUILD = build
BOOT = boot
KERNEL = kernel
DRIVERS = drivers
ARCH = arch/x86

# Target outputs
BOOTLOADER = $(BUILD)/bootloader.bin
KERNEL_ELF = $(BUILD)/kernel.elf
KERNEL_BIN = $(BUILD)/kernel.bin
OS_IMAGE = $(BUILD)/bytebandit.iso

# Source files
KERNEL_ASM_SOURCES = \
    $(KERNEL)/entry.asm \
    $(ARCH)/exceptions.asm \
    $(ARCH)/irq.asm \
    $(ARCH)/usermode.asm

KERNEL_C_SOURCES = \
    $(KERNEL)/main.c \
    $(KERNEL)/panic.c \
    $(DRIVERS)/vga.c \
    $(DRIVERS)/serial.c \
    $(ARCH)/idt.c \
    $(ARCH)/isr.c \
    $(ARCH)/pic.c \
	$(ARCH)/hwirq.c \
    $(ARCH)/gdt.c \
    $(ARCH)/tss.c \
    $(ARCH)/context.c \
    $(ARCH)/usermode.c \
    mm/pmm.c \
    mm/paging.c \
    mm/vmm.c \
    mm/heap.c \
    $(KERNEL)/task.c \
    fs/elf.c \
    libk/string.c

KERNEL_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C_SOURCES))
KERNEL_OBJ += $(patsubst %.asm,$(BUILD)/%.o,$(KERNEL_ASM_SOURCES))

# Default target
all: $(OS_IMAGE)

# Build bootloader
$(BOOTLOADER): $(BOOT)/boot.asm
	@mkdir -p $(BUILD)
	@echo "[ASM] Building bootloader..."
	$(AS) -f bin -o $@ $<
	@BOOTSIZE=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null); \
	if [ $$BOOTSIZE -le 512 ]; then \
		echo "  Bootloader size: $$BOOTSIZE bytes (OK)"; \
	else \
		echo "  ERROR: Bootloader exceeds 512 bytes! Size: $$BOOTSIZE"; exit 1; \
	fi

# Compile C sources to objects
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble kernel entry
$(BUILD)/$(KERNEL)/entry.o: $(KERNEL)/entry.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS) -o $@ $<

# Assemble CPU exceptions
$(BUILD)/$(ARCH)/exceptions.o: $(ARCH)/exceptions.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS) -o $@ $<

# Assemble hardware IRQs
$(BUILD)/$(ARCH)/irq.o: $(ARCH)/irq.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS) -o $@ $<

# Assemble user mode transition
$(BUILD)/$(ARCH)/usermode.o: $(ARCH)/usermode.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS) -o $@ $<

# Link kernel ELF
$(KERNEL_ELF): $(KERNEL_OBJ)
	@echo "[LD] Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/$(KERNEL)/entry.o $(filter-out $(BUILD)/$(KERNEL)/entry.o,$^)

# Extract kernel binary
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "[OBJCOPY] Extracting kernel binary..."
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data -j .bss $< $@

# Create bootable ISO (for now, just concatenate bootloader + kernel)
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL_BIN)
	@echo "[BUILD] Creating OS image..."
	@mkdir -p $(BUILD)
	cat $(BOOTLOADER) $(KERNEL_BIN) > $@
	@SIZE=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null); \
	echo "  Image size: $$SIZE bytes"

# Run in QEMU
run: $(OS_IMAGE)
	@echo "[QEMU] Starting emulator..."
	qemu-system-i386 -drive file=$(OS_IMAGE),format=raw,if=floppy -serial stdio

# Run with GDB debugging
debug: $(OS_IMAGE)
	@echo "[QEMU] Starting with GDB debugging..."
	qemu-system-i386 -drive file=$(OS_IMAGE),format=raw,if=floppy -serial stdio -s -S

# Generate disassembly
disasm: $(KERNEL_ELF)
	@echo "[OBJDUMP] Generating disassembly..."
	$(OBJDUMP) -d -S $(KERNEL_ELF) > $(BUILD)/kernel.asm
	@echo "  Saved to $(BUILD)/kernel.asm"

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing build artifacts..."
	rm -rf $(BUILD)

# Print build info
info:
	@echo "ByteBandit OS Build System"
	@echo "  Bootloader: $(BOOTLOADER)"
	@echo "  Kernel ELF: $(KERNEL_ELF)"
	@echo "  Kernel BIN: $(KERNEL_BIN)"
	@echo "  OS Image: $(OS_IMAGE)"
	@echo ""
	@echo "Targets:"
	@echo "  make all    - Build complete OS image"
	@echo "  make run    - Build and run in QEMU"
	@echo "  make debug  - Run with GDB debugging enabled"
	@echo "  make disasm - Generate kernel disassembly"
	@echo "  make clean  - Remove build artifacts"

.PHONY: all run debug disasm clean info
