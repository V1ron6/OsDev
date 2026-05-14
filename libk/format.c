#include "libk/format.h"

void hex32_to_str(uint32_t value, char *buffer)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    enum { HEX_NIBBLES = 8, HEX_SHIFT_START = (HEX_NIBBLES - 1) * 4 };

    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < HEX_NIBBLES; ++i) {
        int shift = HEX_SHIFT_START - (i * 4);
        buffer[2 + i] = hex_digits[(value >> (uint32_t)shift) & 0xF];
    }

    buffer[10] = '\0';
}
