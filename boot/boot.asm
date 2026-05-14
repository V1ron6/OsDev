; ByteBandit OS Stage-1 Bootloader
; Loads the kernel, switches to protected mode, and jumps to the 32-bit entry.

[org 0x7C00]
[bits 16]

KERNEL_LOAD_SEG  equ 0x1000
KERNEL_LOAD_OFF  equ 0x0000
KERNEL_LOAD_ADDR equ 0x00010000

%ifndef KERNEL_SECTORS
%error KERNEL_SECTORS must be defined by the build system
%endif

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Initialize a safe real-mode stack away from the boot sector.
    mov ax, 0x9000
    mov ss, ax
    mov sp, 0x0000

    mov [boot_drive], dl

    ; Query BIOS disk geometry to avoid hardcoded CHS assumptions.
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov al, cl
    and al, 0x3F
    mov [spt], al

    mov al, dh
    inc al
    mov [heads], al

    call enable_a20

    ; Load the kernel from disk to 0x10000 (KERNEL_LOAD_SEG:KERNEL_LOAD_OFF).
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov si, KERNEL_SECTORS
    mov di, 1

load_loop:
    push bx
    mov ax, di
    call lba_to_chs
    pop bx

    mov ah, 0x02
    mov al, 0x01
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    add bx, 512
    inc di
    dec si
    jnz load_loop

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; Far jump flushes the pipeline and loads the 32-bit CS selector.
    jmp 0x08:protected_mode_entry

; ----------------------
; Real-mode helper logic
; ----------------------

enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

; AX = LBA (1-based). Returns CH/CL/DH for INT 13h.
; Clobbers AX, BX, CX, DX.

lba_to_chs:
    dec ax

    xor dx, dx
    mov bl, [spt]
    xor bh, bh
    div bx
    mov cx, dx

    xor dx, dx
    mov bl, [heads]
    xor bh, bh
    div bx

    mov dh, dl
    mov ch, al

    inc cl
    and cl, 0x3F

    mov al, ah
    and al, 0x03
    shl al, 6
    or cl, al
    ret


bios_print:
    mov ah, 0x0E
.print_loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .print_loop
.done:
    ret


disk_error:
    mov si, disk_error_msg
    call bios_print
    jmp $

; ----------------------
; Global Descriptor Table
; ----------------------

align 8

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ----------------------
; Protected mode entry
; ----------------------

[bits 32]
protected_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000

    jmp KERNEL_LOAD_ADDR

; ----------------------
; Data
; ----------------------

[bits 16]
boot_drive db 0
spt        db 0
heads      db 0


disk_error_msg db "Disk read failure", 0

; Boot signature

times 510 - ($ - $$) db 0
dw 0xAA55
