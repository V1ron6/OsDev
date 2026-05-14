#include "kernel/console.h"
#include "drivers/serial.h"
#include "drivers/vga.h"
#include "libk/format.h"

void console_init(void)
{
    vga_init();
    serial_init();
}

void console_write(const char *text)
{
    vga_write(text);
    serial_write(text);
}

void console_write_hex(uint32_t value)
{
    char buffer[11];

    hex32_to_str(value, buffer);
    console_write(buffer);
}
