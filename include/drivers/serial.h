#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

#include <stdint.h>

int serial_init(void);
void serial_write(const char *text);
void serial_write_char(char c);

#endif
