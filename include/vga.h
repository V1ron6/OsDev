#ifndef _BYTEBANDIT_VGA_H
#define _BYTEBANDIT_VGA_H

#include "types.h"

/* VGA text mode constants */
#define VGA_WIDTH       80
#define VGA_HEIGHT      25
#define VGA_BUFFER      0xB8000

/* VGA color attributes */
#define VGA_COLOR_BLACK         0x0
#define VGA_COLOR_BLUE          0x1
#define VGA_COLOR_GREEN         0x2
#define VGA_COLOR_CYAN          0x3
#define VGA_COLOR_RED           0x4
#define VGA_COLOR_MAGENTA       0x5
#define VGA_COLOR_BROWN         0x6
#define VGA_COLOR_LIGHT_GRAY    0x7
#define VGA_COLOR_DARK_GRAY     0x8
#define VGA_COLOR_LIGHT_BLUE    0x9
#define VGA_COLOR_LIGHT_GREEN   0xA
#define VGA_COLOR_LIGHT_CYAN    0xB
#define VGA_COLOR_LIGHT_RED     0xC
#define VGA_COLOR_LIGHT_MAGENTA 0xD
#define VGA_COLOR_YELLOW        0xE
#define VGA_COLOR_WHITE         0xF

/* Initialize VGA terminal */
void vga_init(void);

/* Clear screen with given background color */
void vga_clear(uint8_t bg_color);

/* Write a single character at current cursor position */
void vga_putc(char c);

/* Write a string */
void vga_puts(const char *str);

/* Write formatted output (basic printf-like) */
void vga_printf(const char *fmt, ...);

/* Set cursor position (column, row) */
void vga_set_cursor(uint8_t col, uint8_t row);

/* Get current cursor position */
void vga_get_cursor(uint8_t *col, uint8_t *row);

#endif /* _BYTEBANDIT_VGA_H */
