; ByteBandit OS Bootloader - Debug Version
; Minimal bootloader with serial output for debugging

[BITS 16]
[ORG 0x7C00]

boot_start:
    cli
    
    ; Initialize serial port for debugging
    mov dx, 0x03F8  ; COM1 port
    mov al, 0x80    ; Set DLAB to access baud rate
    add dl, 3       ; DLAB is bit 7 of offset 3
    mov al, 0x80
    out dx, al
    
    mov dl, 0xF8    ; LSB baud rate (divisor = 1 for 115200)
    mov al, 1
    out dx, al
    mov dl, 0xF9
    mov al, 0
    out dx, al
    
    ; Clear DLAB
    mov dl, 0xFB
    mov al, 0x03    ; 8 bits, 1 stop, no parity
    out dx, al
    
    ; Send test message to serial
    mov si, msg_hello
.send_loop:
    lodsb
    test al, al
    jz .done_send
    
    ; Send byte to serial
    mov dx, 0x03FD  ; Line Status Register
    mov cx, 1000
.wait:
    in al, dx
    test al, 0x20   ; TX Ready?
    jnz .send_char
    loop .wait
    
.send_char:
    mov al, [si-1]
    mov dx, 0x03F8
    out dx, al
    jmp .send_loop
    
.done_send:
    ; Hang
    cli
    hlt

msg_hello:
    db "ByteBandit Boot OK", 0x0D, 0x0A, 0

; Boot signature
align 512, db 0
dw 0xAA55
