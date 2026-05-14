#include "libk/string.h"

void *memset(void *dest, int value, size_t count)
{
    unsigned char *ptr = (unsigned char *)dest;

    for (size_t i = 0; i < count; ++i) {
        ptr[i] = (unsigned char)value;
    }

    return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *dst = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    for (size_t i = 0; i < count; ++i) {
        dst[i] = source[i];
    }

    return dest;
}

size_t strlen(const char *str)
{
    size_t length = 0;

    while (str[length] != '\0') {
        ++length;
    }

    return length;
}
