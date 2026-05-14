#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_write(const char *text);
void console_write_hex(uint32_t value);

#endif
