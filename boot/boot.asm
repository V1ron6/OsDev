; ByteBandit OS Bootloader
; Boot Sector: 512 bytes, BIOS entry point
; Execution: Real Mode → Protected Mode transition
; 
; BIOS guarantees:
;   - CPU in real mode
;   - DL contains boot drive number
;   - Bootloader loaded at 0x7C00
;
; Our responsibility:
;   1. Initialize stack safely
;   2. Enable A20 line (memory extension)
;   3. Load kernel from disk
;   4. Install GDT (Global Descriptor Table)
;   5. Enter protected mode
;   6. Jump to kernel entry point
;
; WARNING: Any error here will cause a triple fault (CPU reset).
; Debugging: Use serial port or test incrementally in QEMU.

[BITS 16]                               ; Real mode assembly (16-bit)
[ORG 0x7C00]                            ; Bootloader loaded here by BIOS

boot_start:
    ; Disable interrupts immediately - we manage CPU state now
    cli
    
    ; Set up stack: grows downward from 0x7C00
    ; We use space before bootloader
    mov ax, 0x0000
    mov ss, ax
    mov sp, 0x7C00
    
    ; Save boot drive number (BIOS sets DL)
    mov byte [boot_drive], dl
    
    ; Clear direction flag for string operations
    cld
    
    ; ===== ENABLE A20 LINE =====
    ; A20 controls memory addressing above 1 MB
    ; Without this, memory wraps at 1 MB (ancient 8086 compat)
    ; This must be done before entering protected mode
    
    call enable_a20
    
    ; ===== LOAD KERNEL FROM DISK =====
    ; Kernel image follows bootloader
    ; Load to 0x1000 (4 KB after bootloader)
    ; Calculate sectors needed: each sector is 512 bytes
    ; Kernel is about 3KB, so load 8 sectors to be safe
    
    mov ax, 0x1000
    mov es, ax                          ; Segment for disk read
    xor bx, bx                          ; Offset 0x0000
    
    mov ah, 0x02                        ; BIOS disk read function
    mov al, 8                           ; Read 8 sectors (4 KB)
    mov ch, 0                           ; Cylinder 0
    mov cl, 2                           ; Sector 2 (bootloader is sector 0-1)
    mov dh, 0                           ; Head 0
    mov dl, byte [boot_drive]           ; Drive number
    int 0x13
    
    jc disk_error                       ; Jump if error
    
    ; ===== INSTALL GDT =====
    ; GDT (Global Descriptor Table) defines memory segments
    ; Required for protected mode
    
    lgdt [gdt_descriptor]               ; Load GDT
    
    ; ===== ENTER PROTECTED MODE =====
    ; Set PE (Protection Enable) bit in CR0
    
    mov eax, cr0
    or eax, 0x00000001                  ; Set PE bit
    mov cr0, eax
    
    ; Flush instruction pipeline with far jump
    ; Jump to protected mode code at 0x1000:0x0000
    jmp 0x0008:pm_entry                 ; Code segment 0x0008, jump to pm_entry
    
disk_error:
    ; Disk read failed - halt
    mov ax, 0xB800                      ; VGA text buffer
    mov es, ax
    mov ax, 0x4F20                      ; White 'F' on black
    mov word [es:0], ax
    cli
    hlt

; ===== A20 ENABLE ROUTINE =====
; Enable address line 20 for >1 MB memory access
; Uses keyboard controller method (most reliable)
;
; The keyboard controller (i8042) has a control port that can enable A20
; This is the most compatible method for old BIOSes

enable_a20:
    ; Drain keyboard input buffer
    xor cx, cx
.drain_kbd_in:
    in al, 0x64                         ; Keyboard status port
    and al, 0x01                        ; Test input buffer full flag
    loopnz .drain_kbd_in
    
    ; Send "write output port" command
    mov al, 0xD1
    out 0x64, al
    
    ; Wait for keyboard controller ready
    xor cx, cx
.wait_kbd_out:
    in al, 0x64
    and al, 0x02                        ; Test output buffer full flag
    loopnz .wait_kbd_out
    
    ; Set A20 bit (0x02) in output port
    mov al, 0xDF
    out 0x60, al
    
    ; Small delay
    xor cx, cx
.delay:
    loop .delay
    
    ret

; ===== GLOBAL DESCRIPTOR TABLE =====
; Defines memory segments for protected mode
; Each entry is 8 bytes, 3 entries required

align 4
gdt:
    ; Entry 0: NULL descriptor (required, unused)
    dq 0x0000000000000000
    
    ; Entry 1: Code segment (base=0, limit=4GB, 32-bit)
    ; Flags: Present | Code | Read | 32-bit
    dq 0x00CF9A000000FFFF
    
    ; Entry 2: Data segment (base=0, limit=4GB, 32-bit)
    ; Flags: Present | Data | Write | 32-bit
    dq 0x00CF92000000FFFF

gdt_descriptor:
    dw gdt_descriptor - gdt - 1         ; GDT size - 1 (23 bytes)
    dd gdt                              ; GDT base address

; ===== PROTECTED MODE ENTRY =====
; Execution continues here after far jump to protected mode

[BITS 32]                               ; Protected mode assembly (32-bit)

pm_entry:
    ; Initialize all segment registers for flat memory model
    mov ax, 0x0010                      ; Data segment selector (entry 2 in GDT)
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov gs, ax
    mov fs, ax
    
    ; Set up stack in protected mode
    ; Use high address for safe stack space
    mov esp, 0x90000                    ; Stack grows downward from 0x90000
    
    ; Enable interrupts now that basic CPU is configured
    sti
    
    ; Jump to kernel entry point
    ; Kernel is loaded at 0x1000:0x0000 in memory
    jmp 0x10000

; ===== DATA SECTION =====
[BITS 16]

boot_drive:
    db 0x00

; ===== BOOT SIGNATURE =====
; BIOS validates bootloader with this signature at offset 510-511
; Must be exactly 512 bytes including signature

; Pad to offset 510 (512 - 2 for signature)
times 510 - ($ - $$) db 0

; Boot signature at offset 510-511
dw 0xAA55                               ; Boot signature (offset 510)
