#ifndef DRIVERS_VGA_H
#define DRIVERS_VGA_H

#include <stdint.h>

void vga_init(void);
void vga_write(const char *text);

#endif
