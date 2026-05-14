#ifndef _BYTEBANDIT_SERIAL_H
#define _BYTEBANDIT_SERIAL_H

/*
 * serial.h - Serial Port Driver Interface
 *
 * COM1 serial output for debugging, available early in boot
 * and before interrupt infrastructure is ready.
 */

/* Initialize COM1 (0x3F8) for 115200 baud, 8-N-1 */
void serial_init(void);

/* Send single character */
void serial_putc(char c);

/* Send null-terminated string */
void serial_puts(const char *str);

/* Send formatted string (printf-like) */
void serial_printf(const char *fmt, ...);

#endif
