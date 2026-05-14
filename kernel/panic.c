#include "kernel/panic.h"
#include "kernel/console.h"

void panic(const char *message, const char *file, uint32_t line)
{
    __asm__ volatile("cli");

    console_write("\n=== KERNEL PANIC ===\n");
    console_write(message);
    console_write("\nLocation: ");
    console_write(file);
    console_write(":");
    console_write_hex(line);
    console_write("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
