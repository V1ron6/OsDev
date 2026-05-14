#include "libk/format.h"

void hex32_to_str(uint32_t value, char *buffer)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 8; ++i) {
        uint32_t shift = 28U - (uint32_t)(i * 4);
        buffer[2 + i] = hex_digits[(value >> shift) & 0xF];
    }

    buffer[10] = '\0';
}
