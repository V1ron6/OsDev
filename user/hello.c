#include <bb_syscall.h>

static const char message[] = "hello from a ByteBandit CLI app\n";

void app_main(void) {
    bb_write(BB_CONSOLE_FD, message, sizeof(message) - 1);
    bb_sleep_ms(1000);
    bb_exit(0);
}
