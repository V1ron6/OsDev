#ifndef _BYTEBANDIT_KERNEL_H
#define _BYTEBANDIT_KERNEL_H

#include "types.h"

/* Kernel version */
#define KERNEL_MAJOR 0
#define KERNEL_MINOR 1
#define KERNEL_PATCH 0

/* Forward declaration of main kernel function */
void kernel_main(void);

/* Panic and error handling */
void kernel_panic(const char *message);

#endif /* _BYTEBANDIT_KERNEL_H */
