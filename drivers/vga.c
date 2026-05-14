#include "drivers/vga.h"
#include "libk/string.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t *const vga_buffer = (uint16_t *)VGA_MEMORY;
static size_t vga_row = 0;
static size_t vga_column = 0;
static uint8_t vga_color = 0x0F;

static uint16_t vga_entry(unsigned char character, uint8_t color)
{
    return (uint16_t)character | (uint16_t)color << 8;
}

static void vga_scroll(void)
{
    for (size_t row = 1; row < VGA_HEIGHT; ++row) {
        memcpy(&vga_buffer[(row - 1) * VGA_WIDTH],
               &vga_buffer[row * VGA_WIDTH],
               VGA_WIDTH * sizeof(uint16_t));
    }

    for (size_t column = 0; column < VGA_WIDTH; ++column) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + column] = vga_entry(' ', vga_color);
    }
}

static void vga_newline(void)
{
    vga_column = 0;
    vga_row++;

    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_init(void)
{
    vga_row = 0;
    vga_column = 0;
    vga_color = 0x0F;

    for (size_t row = 0; row < VGA_HEIGHT; ++row) {
        for (size_t column = 0; column < VGA_WIDTH; ++column) {
            vga_buffer[row * VGA_WIDTH + column] = vga_entry(' ', vga_color);
        }
    }
}

static void vga_putc(char character)
{
    if (character == '\n') {
        vga_newline();
        return;
    }

    if (character == '\r') {
        vga_column = 0;
        return;
    }

    vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(character, vga_color);
    vga_column++;

    if (vga_column >= VGA_WIDTH) {
        vga_newline();
    }
}

void vga_write(const char *text)
{
    for (size_t i = 0; text[i] != '\0'; ++i) {
        vga_putc(text[i]);
    }
}
