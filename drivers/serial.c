/*
 * serial.c - COM1 Serial Port Driver
 *
 * Provides serial output for debugging and logging. COM1 is a critical
 * debugging resource that survives when VGA fails (e.g., during paging
 * setup or interrupt issues).
 *
 * Hardware:
 * - COM1 base address: 0x3F8
 * - Default baud rate: 115200
 * - Data format: 8 bits, no parity, 1 stop bit
 *
 * QEMU usage:
 *   qemu-system-i386 -serial stdio
 *
 * The serial port registers are I/O mapped:
 * - 0x3F8 + 0: Data register (RX/TX)
 * - 0x3F8 + 1: Interrupt enable register
 * - 0x3F8 + 2: FIFO control register
 * - 0x3F8 + 3: Line control register
 * - 0x3F8 + 4: Modem control register
 * - 0x3F8 + 5: Line status register (bit 6: transmit ready)
 */

#include "serial.h"
#include "x86.h"

/* COM1 base address and port offsets */
#define SERIAL_COM1_BASE  0x3F8

/* Register offsets from base address */
#define SERIAL_DATA       0  /* RX/TX data (when DLAB=0) */
#define SERIAL_INTR       1  /* Interrupt enable (when DLAB=0) */
#define SERIAL_FIFO       2  /* FIFO control */
#define SERIAL_LCRL       3  /* Line control register */
#define SERIAL_MCRL       4  /* Modem control register */
#define SERIAL_LSTAT      5  /* Line status register */
#define SERIAL_MSTAT      6  /* Modem status register */
#define SERIAL_SCRATCH    7  /* Scratch register */

/* Divisor latch registers (when DLAB=1) */
#define SERIAL_DLAB_LOW   0  /* Divisor latch low byte */
#define SERIAL_DLAB_HIGH  1  /* Divisor latch high byte */

/* Line control register bits */
#define SERIAL_LCRL_DLAB  0x80  /* Divisor latch access bit */
#define SERIAL_LCRL_8BIT  0x03  /* 8-bit word length */

/* Line status register bits */
#define SERIAL_LSTAT_TRDY 0x20  /* Transmit holding register empty */

/* Initialize COM1 serial port
 *
 * Configures the serial port for:
 * - 115200 baud
 * - 8 data bits
 * - No parity
 * - 1 stop bit
 *
 * Divisor calculation:
 *   For 115200 baud: divisor = 1
 *   (Clock 1.8432 MHz / (115200 * 16) = 1)
 */
void serial_init(void) {
    /* Disable all interrupts during initialization */
    outb(SERIAL_COM1_BASE + SERIAL_INTR, 0x00);
    
    /* Set DLAB to access divisor latch */
    outb(SERIAL_COM1_BASE + SERIAL_LCRL, SERIAL_LCRL_DLAB);
    
    /* Set divisor to 1 (115200 baud)
     *   Divisor = Clock frequency / (16 * baud rate)
     *           = 1843200 / (16 * 115200) = 1
     * Low byte: 0x01, High byte: 0x00
     */
    outb(SERIAL_COM1_BASE + SERIAL_DLAB_LOW, 0x01);
    outb(SERIAL_COM1_BASE + SERIAL_DLAB_HIGH, 0x00);
    
    /* Clear DLAB and set 8-bit data, no parity, 1 stop bit */
    outb(SERIAL_COM1_BASE + SERIAL_LCRL, SERIAL_LCRL_8BIT);
    
    /* Configure FIFO: enable FIFO, clear RX/TX, set trigger level */
    outb(SERIAL_COM1_BASE + SERIAL_FIFO, 0xC7);
    
    /* Configure modem: DTR + RTS (handshaking disabled for simplicity) */
    outb(SERIAL_COM1_BASE + SERIAL_MCRL, 0x0B);
    
    /* Clear any pending interrupts by reading status */
    inb(SERIAL_COM1_BASE + SERIAL_LSTAT);
    inb(SERIAL_COM1_BASE + SERIAL_MSTAT);
}

/* Wait until transmit buffer is ready
 *
 * The line status register's THRE bit (bit 5) indicates the transmit
 * holding register is empty and ready for new data.
 *
 * Polling here is acceptable because:
 * 1. Serial ports are fast (115200 baud = ~10 microseconds per byte)
 * 2. Interrupts may not be ready during early boot
 * 3. Panic/debug output takes priority over responsiveness
 *
 * Timeout would be added for production systems to prevent hangs.
 */
static void serial_wait_transmit(void) {
    while (!(inb(SERIAL_COM1_BASE + SERIAL_LSTAT) & SERIAL_LSTAT_TRDY)) {
        /* Spin until ready */
    }
}

/* Transmit single character
 *
 * @param c: character to send
 *
 * Waits for transmit buffer to be ready, then writes byte.
 * Simple implementation suitable for debugging and boot phase.
 */
void serial_putc(char c) {
    serial_wait_transmit();
    outb(SERIAL_COM1_BASE + SERIAL_DATA, (uint8_t)c);
}

/* Transmit null-terminated string
 *
 * @param str: pointer to null-terminated string
 *
 * Iterates through string and transmits each character.
 * Handles newlines (converts \n to \r\n for terminal compatibility).
 */
void serial_puts(const char *str) {
    if (!str) {
        return;
    }
    
    while (*str) {
        if (*str == '\n') {
            /* Send carriage return before newline for proper terminal display */
            serial_putc('\r');
        }
        serial_putc(*str);
        str++;
    }
}

/* Transmit formatted string (simplified printf-like)
 *
 * @param fmt: format string
 * @param ...: arguments
 *
 * Supports:
 *   %s - string
 *   %d - decimal integer
 *   %x - hexadecimal integer
 *   %% - literal %
 *
 * Note: This is a minimal implementation for debug output.
 * Full printf() is in kernel.c via vga_printf().
 */
#include <stdarg.h>

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                /* String argument */
                const char *str = va_arg(args, const char *);
                serial_puts(str);
            } else if (*fmt == 'd') {
                /* Decimal integer */
                int num = va_arg(args, int);
                if (num < 0) {
                    serial_putc('-');
                    num = -num;
                }
                
                /* Print digits (simple approach, no recursion) */
                char buffer[16];
                int idx = 0;
                if (num == 0) {
                    buffer[idx++] = '0';
                } else {
                    int temp = num;
                    while (temp > 0) {
                        buffer[idx++] = '0' + (temp % 10);
                        temp /= 10;
                    }
                }
                /* Print in reverse order */
                while (idx > 0) {
                    serial_putc(buffer[--idx]);
                }
            } else if (*fmt == 'x') {
                /* Hexadecimal integer */
                unsigned int num = va_arg(args, unsigned int);
                const char *hex = "0123456789ABCDEF";
                
                /* Handle 32-bit number */
                for (int i = 28; i >= 0; i -= 4) {
                    serial_putc(hex[(num >> i) & 0xF]);
                }
            } else if (*fmt == '%') {
                /* Literal % */
                serial_putc('%');
            }
            fmt++;
        } else if (*fmt == '\n') {
            /* Handle newlines */
            serial_putc('\r');
            serial_putc('\n');
            fmt++;
        } else {
            /* Regular character */
            serial_putc(*fmt);
            fmt++;
        }
    }
    
    va_end(args);
}
