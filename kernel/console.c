#include "kernel/console.h"
#include "bb_api.h"
#include "serial.h"
#include "vga.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "arch/x86/pit.h"
#include "fs/vfs.h"
#include "fs/registry.h"
#include "kernel/task.h"
#include "kernel/syscall.h"
#include "x86.h"
#include <string.h>

#define CONSOLE_LINE_LENGTH 128

static char console_line[CONSOLE_LINE_LENGTH];
static uint32_t console_length;

static void console_puts(const char *text) {
    serial_puts(text);
    vga_puts(text);
}

static void console_prompt(void) {
    console_puts("bb> ");
}

static void console_print_line(const char *text) {
    serial_puts(text);
    vga_puts(text);
    serial_puts("\n");
    vga_puts("\n");
}

static void console_cat(const char *path) {
    char buffer[128];
    int32_t fd = vfs_open(path ? path : "/etc/motd", BB_O_RDONLY);
    if (fd < 0) {
        console_puts("cat: file not found\n");
        return;
    }

    int32_t amount;
    while ((amount = vfs_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[amount] = '\0';
        console_puts(buffer);
    }
    vfs_close(fd);
}

static void console_touch(const char *path) {
    int32_t fd = path == NULL ? BB_STATUS_INVALID :
                 vfs_open(path, BB_O_CREAT | BB_O_RDWR);
    if (fd < 0) {
        console_puts("touch: unable to create file\n");
        return;
    }
    vfs_close(fd);
}

static void console_write_file(const char *path, const char *text) {
    int32_t fd;
    if (path == NULL || text == NULL) {
        console_puts("write: usage write PATH TEXT\n");
        return;
    }
    fd = vfs_open(path, BB_O_CREAT | BB_O_TRUNC | BB_O_RDWR);
    if (fd < 0 || vfs_write(fd, text, strlen(text)) < 0) {
        console_puts("write: unable to write file\n");
    }
    if (fd >= 0) {
        vfs_close(fd);
    }
}

static void console_copy_file(const char *source, const char *destination) {
    char buffer[128];
    int32_t source_fd;
    int32_t destination_fd;
    int32_t amount;

    if (source == NULL || destination == NULL) {
        console_puts("cp: usage cp SOURCE DEST\n");
        return;
    }
    source_fd = vfs_open(source, BB_O_RDONLY);
    destination_fd = vfs_open(destination, BB_O_CREAT | BB_O_TRUNC | BB_O_RDWR);
    if (source_fd < 0 || destination_fd < 0) {
        console_puts("cp: unable to open file\n");
        if (source_fd >= 0) vfs_close(source_fd);
        if (destination_fd >= 0) vfs_close(destination_fd);
        return;
    }
    while ((amount = vfs_read(source_fd, buffer, sizeof(buffer))) > 0) {
        if (vfs_write(destination_fd, buffer, (uint32_t)amount) != amount) {
            console_puts("cp: write failed\n");
            break;
        }
    }
    vfs_close(source_fd);
    vfs_close(destination_fd);
}

static void console_uname(void) {
    console_print_line("ByteBandit bytebandit 0.1.0 i386 GNU/Linux-compatible");
}

static void console_uptime(void) {
    serial_printf("%u ticks (%u seconds)\n", pit_get_ticks(),
                  pit_ticks_to_ms(pit_get_ticks()) / 1000);
    vga_printf("%u ticks (%u seconds)\n", pit_get_ticks(),
               pit_ticks_to_ms(pit_get_ticks()) / 1000);
}

static void console_free(void) {
    bb_system_info_t info;
    if (bb_api_get_info(&info) == BB_STATUS_OK) {
        serial_printf("Mem: %u frames total, %u frames free\n",
                      info.total_frames, info.free_frames);
        serial_printf("Heap: %u bytes used, %u bytes free\n",
                      info.heap_used, info.heap_free);
        vga_printf("Mem: %u frames total, %u frames free\n",
                   info.total_frames, info.free_frames);
        vga_printf("Heap: %u bytes used, %u bytes free\n",
                   info.heap_used, info.heap_free);
    }
}

static void console_info(void) {
    bb_system_info_t info;
    if (bb_api_get_info(&info) != BB_STATUS_OK) {
        console_puts("error: system information unavailable\n");
        return;
    }
    serial_printf("API %u.%u, ticks %u\n", info.api_major, info.api_minor,
                  info.ticks);
    serial_printf("frames: %u total, %u free\n",
                  info.total_frames, info.free_frames);
    serial_printf("heap: %u used, %u free\n",
                  info.heap_used, info.heap_free);
    vga_printf("API %u.%u, ticks %u\n", info.api_major, info.api_minor,
               info.ticks);
    vga_printf("frames: %u total, %u free\n",
               info.total_frames, info.free_frames);
    vga_printf("heap: %u used, %u free\n",
               info.heap_used, info.heap_free);
}

static uint32_t console_parse_uint(const char *text) {
    uint32_t value = 0;
    if (text == NULL || *text == '\0') {
        return 0;
    }
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint32_t)(*text - '0');
        text++;
    }
    return value;
}

static void console_kill(const char *pid_text, const char *signal_text) {
    uint32_t pid = console_parse_uint(pid_text);
    uint32_t signal = console_parse_uint(signal_text);
    int32_t result = syscall_kill(pid, signal);

    if (result != BB_STATUS_OK) {
        console_puts("kill: permission denied or invalid target\n");
    }
}

static void console_halt(void) {
    console_puts("System halted.\n");
    disable_interrupts();
    while (1) {
        halt();
    }
}

static void console_shutdown(void) {
    console_puts("Shutting down.\n");
    disable_interrupts();
    outw(0x0604, 0x2000);
    while (1) {
        halt();
    }
}

static void console_reboot(void) {
    console_puts("Restarting.\n");
    disable_interrupts();
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(0x0064) & 0x02) == 0) {
            break;
        }
    }
    outb(0x0064, 0xFE);
    while (1) {
        halt();
    }
}

static void console_execute(void) {
    char *argument = NULL;
    char *second_argument = NULL;

    if (console_length == 0) {
        return;
    }
    console_line[console_length] = '\0';

    for (uint32_t i = 0; i < console_length; i++) {
        if (console_line[i] == ' ') {
            console_line[i] = '\0';
            argument = console_line + i + 1;
            break;
        }
    }
    if (argument != NULL) {
        for (uint32_t i = 0; argument[i] != '\0'; i++) {
            if (argument[i] == ' ') {
                argument[i] = '\0';
                second_argument = argument + i + 1;
                break;
            }
        }
    }

    if (strcmp(console_line, "help") == 0) {
        console_puts("commands: cat clear cp echo env files free help id ls mkdir\n");
        console_puts("          kill mount mem poweroff ps pwd registry reboot\n");
        console_puts("          restart rm shutdown sleep stat sync tasks touch\n");
        console_puts("          true uname uptime whoami write halt\n");
    } else if (strcmp(console_line, "version") == 0) {
        bb_api_print_version();
    } else if (strcmp(console_line, "info") == 0) {
        console_info();
    } else if (strcmp(console_line, "tasks") == 0) {
        bb_api_task_list();
    } else if (strcmp(console_line, "mem") == 0) {
        console_free();
    } else if (strcmp(console_line, "free") == 0) {
        console_free();
    } else if (strcmp(console_line, "files") == 0) {
        vfs_list();
    } else if (strcmp(console_line, "stat") == 0) {
        if (argument == NULL || vfs_stat(argument) != BB_STATUS_OK) {
            console_puts("stat: file not found\n");
        }
    } else if (strcmp(console_line, "sync") == 0) {
        vfs_sync();
        console_print_line("filesystem synchronized");
    } else if (strcmp(console_line, "registry") == 0) {
        registry_list();
    } else if (strcmp(console_line, "ls") == 0) {
        vfs_list();
    } else if (strcmp(console_line, "touch") == 0) {
        console_touch(argument);
    } else if (strcmp(console_line, "mkdir") == 0) {
        if (argument == NULL || vfs_mkdir(argument) != BB_STATUS_OK) {
            console_puts("mkdir: unable to create directory\n");
        }
    } else if (strcmp(console_line, "rm") == 0) {
        if (argument == NULL || vfs_unlink(argument) != BB_STATUS_OK) {
            console_puts("rm: unable to remove file\n");
        }
    } else if (strcmp(console_line, "write") == 0) {
        console_write_file(argument, second_argument);
    } else if (strcmp(console_line, "cp") == 0) {
        console_copy_file(argument, second_argument);
    } else if (strcmp(console_line, "cat") == 0 ||
               strcmp(console_line, "head") == 0) {
        console_cat(argument);
    } else if (strcmp(console_line, "pwd") == 0) {
        console_print_line("/");
    } else if (strcmp(console_line, "uname") == 0) {
        console_uname();
    } else if (strcmp(console_line, "uptime") == 0) {
        console_uptime();
    } else if (strcmp(console_line, "sleep") == 0) {
        pit_sleep_ms(console_parse_uint(argument));
    } else if (strcmp(console_line, "ps") == 0 ||
               strcmp(console_line, "tasks") == 0) {
        bb_api_task_list();
    } else if (strcmp(console_line, "whoami") == 0) {
        if (task_get_current_uid() == 0) {
            console_print_line("root");
        } else {
            serial_printf("user%u\n", task_get_current_uid());
            vga_printf("user%u\n", task_get_current_uid());
        }
    } else if (strcmp(console_line, "id") == 0) {
        serial_printf("uid=%u gid=%u groups=%u\n",
                      task_get_current_uid(), task_get_current_gid(),
                      task_get_current_gid());
        vga_printf("uid=%u gid=%u groups=%u\n",
                   task_get_current_uid(), task_get_current_gid(),
                   task_get_current_gid());
    } else if (strcmp(console_line, "env") == 0) {
        registry_list();
    } else if (strcmp(console_line, "mount") == 0) {
        console_print_line("rootfs on / type bytebanditfs (rw,ram)");
    } else if (strcmp(console_line, "true") == 0) {
        return;
    } else if (strcmp(console_line, "false") == 0) {
        console_print_line("false: command returned status 1");
    } else if (strcmp(console_line, "echo") == 0) {
        if (argument != NULL) {
            bb_api_write(BB_CONSOLE_FD, argument, strlen(argument));
        }
        console_puts("\n");
    } else if (strcmp(console_line, "clear") == 0) {
        vga_clear(VGA_COLOR_BLACK);
        console_puts("\n");
    } else if (strcmp(console_line, "kill") == 0) {
        console_kill(argument, second_argument);
    } else if (strcmp(console_line, "shutdown") == 0 ||
               strcmp(console_line, "poweroff") == 0) {
        console_shutdown();
    } else if (strcmp(console_line, "reboot") == 0 ||
               strcmp(console_line, "restart") == 0) {
        console_reboot();
    } else if (strcmp(console_line, "halt") == 0) {
        console_halt();
    } else {
        console_puts("unknown command; type 'help'\n");
    }
}

void console_init(void) {
    console_length = 0;
    console_puts("\nByteBandit command console ready.\n");
    console_puts("Type 'help' for commands.\n");
    console_prompt();
}

void console_poll(void) {
    while (serial_received()) {
        char character = serial_getc();
        if (character == '\r' || character == '\n') {
            console_puts("\n");
            console_execute();
            console_length = 0;
            console_prompt();
        } else if (character == '\b' || character == 0x7F) {
            if (console_length > 0) {
                console_length--;
                serial_puts("\b \b");
                vga_puts("\b \b");
            }
        } else if (character >= 0x20 && character <= 0x7E &&
                   console_length < CONSOLE_LINE_LENGTH - 1) {
            console_line[console_length++] = character;
            serial_putc(character);
            vga_putc(character);
        }
    }
}
