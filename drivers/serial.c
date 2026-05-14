#include "drivers/serial.h"
#include "arch/x86/port.h"
#include <stddef.h>

#define COM1 0x3F8

static int serial_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

int serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);

    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);

    if (inb(COM1 + 0) != 0xAE) {
        outb(COM1 + 4, 0x0F);
        return -1;
    }

    outb(COM1 + 4, 0x0F);
    return 0;
}

void serial_write_char(char c)
{
    while (!serial_transmit_empty()) {
        io_wait();
    }

    outb(COM1, (uint8_t)c);
}

void serial_write(const char *text)
{
    for (size_t i = 0; text[i] != '\0'; ++i) {
        serial_write_char(text[i]);
    }
}
