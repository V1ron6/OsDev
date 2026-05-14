; ByteBandit OS Bootloader - Simple Test Version
; Minimal bootloader without disk loading
; This version just switches to protected mode and jumps to 0x10000

[BITS 16]
[ORG 0x7C00]

boot_start:
    cli
    cld
    
    ; Clear registers
    xor ax, ax
    xor dx, dx
    xor si, si
    xor di, di
    
    ; Set up stack
    mov ss, ax
    mov sp, 0x7C00
    
    ; ===== ENABLE A20 =====
    ; Fast A20 method
    in al, 0x92
    or al, 0x02
    out 0x92, al
    
    ; ===== INSTALL GDT =====
    lgdt [gdt_descriptor]
    
    ; ===== ENTER PROTECTED MODE =====
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
    
    ; Flush pipeline
    jmp 0x0008:pm_entry

[BITS 32]

pm_entry:
    ; Initialize segment registers
    mov eax, 0x0010
    mov ds, eax
    mov es, eax
    mov ss, eax
    mov gs, eax
    mov fs, eax
    
    ; Set up stack for kernel
    mov esp, 0x90000
    
    ; Jump to kernel
    jmp 0x10000

; ===== GDT =====
[BITS 16]

align 4
gdt:
    dq 0x0000000000000000              ; Null descriptor
    dq 0x00CF9A000000FFFF              ; Code (base=0, limit=4GB)
    dq 0x00CF92000000FFFF              ; Data (base=0, limit=4GB)

gdt_descriptor:
    dw gdt_descriptor - gdt - 1
    dd gdt

; ===== BOOT SIGNATURE =====
times 510 - ($ - $$) db 0
dw 0xAA55
