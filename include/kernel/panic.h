#ifndef KERNEL_PANIC_H
#define KERNEL_PANIC_H

#include <stdint.h>

void panic(const char *message, const char *file, uint32_t line);

#define PANIC(message) panic((message), __FILE__, __LINE__)

#endif
