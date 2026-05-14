#ifndef _BYTEBANDIT_TYPES_H
#define _BYTEBANDIT_TYPES_H

/* Fundamental integer types for freestanding environment */
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;

/* Common types */
typedef uint32_t size_t;
typedef int32_t ssize_t;
typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

/* Boolean type */
typedef int bool;
#define true 1
#define false 0

/* NULL pointer */
#define NULL ((void *)0)

#endif /* _BYTEBANDIT_TYPES_H */
