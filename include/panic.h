#ifndef _BYTEBANDIT_PANIC_H
#define _BYTEBANDIT_PANIC_H

#include "types.h"

/* Print panic message and halt kernel */
void kernel_panic(const char *message);

/* Print panic with format string */
void panic_printf(const char *fmt, ...);

#endif /* _BYTEBANDIT_PANIC_H */
