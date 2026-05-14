#include "vga.h"
#include "x86.h"
#include "stdarg.h"

/* VGA text mode buffer */
static uint16_t *vga_buffer = (uint16_t *)VGA_BUFFER;

/* Current cursor position */
static uint8_t cursor_col = 0;
static uint8_t cursor_row = 0;

/* Current foreground and background colors */
static uint8_t current_fg = VGA_COLOR_WHITE;
static uint8_t current_bg = VGA_COLOR_BLACK;

/* Helper: Create a VGA character entry (character + color attributes) */
static inline uint16_t vga_entry(char c, uint8_t fg, uint8_t bg) {
    uint16_t attr = (bg << 4) | fg;
    return ((uint16_t)c) | (attr << 8);
}

/* Helper: Update hardware cursor position */
static void update_hardware_cursor(void) {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    
    /* Cursor high byte register */
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
    
    /* Cursor low byte register */
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}

void vga_init(void) {
    cursor_col = 0;
    cursor_row = 0;
    current_fg = VGA_COLOR_WHITE;
    current_bg = VGA_COLOR_BLACK;
    vga_clear(VGA_COLOR_BLACK);
}

void vga_clear(uint8_t bg_color) {
    uint16_t blank = vga_entry(' ', VGA_COLOR_WHITE, bg_color);
    
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    
    cursor_col = 0;
    cursor_row = 0;
    update_hardware_cursor();
}

void vga_set_cursor(uint8_t col, uint8_t row) {
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    
    cursor_col = col;
    cursor_row = row;
    update_hardware_cursor();
}

void vga_get_cursor(uint8_t *col, uint8_t *row) {
    *col = cursor_col;
    *row = cursor_row;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
    } else if (c >= 0x20 && c < 0x7F) {
        /* Printable character */
        size_t index = cursor_row * VGA_WIDTH + cursor_col;
        vga_buffer[index] = vga_entry(c, current_fg, current_bg);
        cursor_col++;
    }
    
    /* Handle line wrapping and scrolling */
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    
    /* Handle scrolling when cursor goes past last row */
    if (cursor_row >= VGA_HEIGHT) {
        /* Scroll display up one line */
        for (size_t i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        
        /* Clear last line */
        uint16_t blank = vga_entry(' ', current_fg, current_bg);
        for (size_t i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buffer[i] = blank;
        }
        
        cursor_row = VGA_HEIGHT - 1;
    }
    
    update_hardware_cursor();
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_printf(const char *fmt, ...) {
    /* Basic printf implementation - handle %s, %d, %x, %% */
    
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            
            if (*fmt == 's') {
                /* String argument */
                const char *s = va_arg(args, const char *);
                vga_puts(s);
            } else if (*fmt == 'd') {
                /* Decimal integer */
                int n = va_arg(args, int);
                
                if (n < 0) {
                    vga_putc('-');
                    n = -n;
                }
                
                /* Recursive digit printing */
                if (n >= 10) {
                    vga_printf("%d", n / 10);
                }
                vga_putc('0' + (n % 10));
                
            } else if (*fmt == 'x') {
                /* Hexadecimal */
                uint32_t n = va_arg(args, uint32_t);
                const char *hex = "0123456789ABCDEF";
                
                for (int i = 28; i >= 0; i -= 4) {
                    vga_putc(hex[(n >> i) & 0xF]);
                }
            } else if (*fmt == '%') {
                /* Escaped % */
                vga_putc('%');
            } else {
                /* Unknown format specifier - just print it */
                vga_putc('%');
                vga_putc(*fmt);
            }
        } else {
            vga_putc(*fmt);
        }
        
        fmt++;
    }
    
    va_end(args);
}
