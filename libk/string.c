/*
 * libk/string.c - Basic string and memory functions
 *
 * Implements common C library functions for freestanding environment.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * memset() - Fill memory block with a byte value
 *
 * Args:
 *   s - Pointer to memory block
 *   c - Byte value to fill with
 *   n - Number of bytes to fill
 *
 * Returns: Pointer to s
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    unsigned char value = (unsigned char)c;
    
    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
    
    return s;
}

/**
 * memcpy() - Copy memory block
 *
 * Args:
 *   dest - Destination buffer
 *   src  - Source buffer
 *   n    - Number of bytes to copy
 *
 * Returns: Pointer to dest
 *
 * Note: Does not handle overlapping regions (use memmove for that)
 */
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    
    return dest;
}

/**
 * memmove() - Move memory block (handles overlap)
 *
 * Args:
 *   dest - Destination buffer
 *   src  - Source buffer
 *   n    - Number of bytes to move
 *
 * Returns: Pointer to dest
 *
 * Unlike memcpy, this correctly handles overlapping regions.
 */
void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    /* If source is before destination, copy forward to avoid overlap issues */
    if (s < d) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    
    return dest;
}

/**
 * strlen() - Get string length
 *
 * Args:
 *   s - Null-terminated string
 *
 * Returns: Length of string (not including null terminator)
 */
size_t strlen(const char *s) {
    size_t length = 0;
    while (s[length] != '\0') {
        length++;
    }
    return length;
}

/**
 * strncpy() - Copy string with length limit
 *
 * Args:
 *   dest - Destination buffer
 *   src  - Source string
 *   n    - Maximum bytes to copy
 *
 * Returns: Pointer to dest
 *
 * Note: If src is longer than n, dest will NOT be null-terminated!
 * This follows standard C library behavior.
 */
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    
    /* Copy up to n characters from src */
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    /* Null-pad the rest if src ended before n bytes */
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return dest;
}

/**
 * strcmp() - Compare two strings
 *
 * Args:
 *   s1 - First string
 *   s2 - Second string
 *
 * Returns: 
 *   < 0 if s1 < s2
 *   0 if s1 == s2
 *   > 0 if s1 > s2
 */
int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/**
 * strncmp() - Compare strings with length limit
 *
 * Args:
 *   s1 - First string
 *   s2 - Second string
 *   n  - Maximum bytes to compare
 *
 * Returns: < 0, 0, or > 0 (see strcmp)
 */
int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}
