#ifndef _BYTEBANDIT_EXCEPTIONS_H
#define _BYTEBANDIT_EXCEPTIONS_H

#include "types.h"

/*
 * exceptions.h - CPU Exception Handling Interface
 *
 * Provides infrastructure for catching and handling CPU exceptions
 * (interrupts 0-31 per x86 specification).
 */

/* CPU exception handler - called by ISR stubs
 *
 * @param int_no: exception number (0-31)
 * @param err_code: error code (0 if not provided by CPU)
 *
 * Never returns; always panics with diagnostic info.
 */
void exception_handler(uint32_t int_no, uint32_t err_code);

#endif
