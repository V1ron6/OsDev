#include "bb_api.h"
#include "serial.h"
#include "vga.h"
#include "arch/x86/pit.h"
#include "kernel/task.h"
#include "mm/heap.h"
#include "mm/pmm.h"

int bb_api_get_info(bb_system_info_t *info) {
    uint32_t heap_used;
    uint32_t heap_free;

    if (info == NULL) {
        return BB_STATUS_INVALID;
    }

    heap_get_stats(&heap_used, &heap_free);
    info->api_major = BB_API_MAJOR;
    info->api_minor = BB_API_MINOR;
    info->ticks = pit_get_ticks();
    info->total_frames = pmm_get_total_frames();
    info->free_frames = pmm_get_free_frames();
    info->heap_used = heap_used;
    info->heap_free = heap_free;
    return BB_STATUS_OK;
}

int bb_api_write(int fd, const char *buffer, uint32_t length) {
    if ((fd != BB_CONSOLE_FD && fd != BB_ERROR_FD) ||
        buffer == NULL || length == 0) {
        return BB_STATUS_INVALID;
    }

    for (uint32_t i = 0; i < length; i++) {
        serial_putc(buffer[i]);
        vga_putc(buffer[i]);
    }
    return (int)length;
}

int bb_api_task_list(void) {
    task_list_dump();
    return BB_STATUS_OK;
}

void bb_api_print_version(void) {
    serial_printf("%s v%u.%u\n", BB_API_NAME, BB_API_MAJOR, BB_API_MINOR);
    vga_printf("%s v%u.%u\n", BB_API_NAME, BB_API_MAJOR, BB_API_MINOR);
}
