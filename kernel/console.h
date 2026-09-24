#ifndef BYTEBANDIT_CONSOLE_H
#define BYTEBANDIT_CONSOLE_H

/* Initialize and print the headless ByteBandit command prompt. */
void console_init(void);

/* Poll COM1 and execute complete input lines. */
void console_poll(void);

#endif
