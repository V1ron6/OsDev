ARCH        := i386
CC          := i686-elf-gcc
LD          := i686-elf-ld
AS          := nasm
OBJCOPY     := i686-elf-objcopy

CFLAGS      := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -Werror \
               -fno-builtin -fno-stack-protector -fno-pic -m32 -Iinclude
LDFLAGS     := -T linker.ld -nostdlib

KERNEL_C_SOURCES := \
kernel/kernel.c \
kernel/console.c \
kernel/panic.c \
drivers/vga.c \
drivers/serial.c \
arch/x86/idt.c \
arch/x86/isr.c \
libk/string.c \
libk/format.c

KERNEL_ASM_SOURCES := \
kernel/entry.asm \
arch/x86/isr.asm

KERNEL_OBJECTS := $(patsubst %.c,build/%.o,$(KERNEL_C_SOURCES)) \
$(patsubst %.asm,build/%.o,$(KERNEL_ASM_SOURCES))

.PHONY: all clean run debug

all: build/os.img

build/os.img: build/boot.bin build/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=build/boot.bin of=$@ conv=notrunc status=none
	dd if=build/kernel.bin of=$@ seek=1 conv=notrunc status=none

build/kernel.elf: $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@

build/boot.bin: boot/boot.asm build/kernel.bin
	@kernel_size=$$(stat -c %s build/kernel.bin); \
	kernel_sectors=$$((($$kernel_size + 511) / 512)); \
	$(AS) -f bin -DKERNEL_SECTORS=$$kernel_sectors -o $@ $<

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

run: build/os.img
	qemu-system-i386 -drive format=raw,file=build/os.img -serial stdio

debug: build/os.img
	qemu-system-i386 -drive format=raw,file=build/os.img -serial stdio -s -S

clean:
	rm -rf build
