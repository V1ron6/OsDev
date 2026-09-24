#ifndef BYTEBANDIT_API_H
#define BYTEBANDIT_API_H

#include "types.h"

#define BB_API_MAJOR 0
#define BB_API_MINOR 1
#define BB_API_NAME "ByteBandit System API"

#define BB_STATUS_OK       0
#define BB_STATUS_INVALID  -1
#define BB_STATUS_NOT_FOUND -2
#define BB_STATUS_NOSYS    -3
#define BB_STATUS_PERM     -4
#define BB_STATUS_AGAIN    -5

#define BB_STDIN_FD   0
#define BB_CONSOLE_FD 1
#define BB_ERROR_FD   2

#define BB_O_RDONLY 0x0000
#define BB_O_WRONLY 0x0001
#define BB_O_RDWR   0x0002
#define BB_O_CREAT  0x0040
#define BB_O_TRUNC  0x0200
#define BB_O_APPEND 0x0400

typedef int32_t bb_fd_t;
typedef uint32_t bb_handle_t;

#define BB_INVALID_HANDLE 0

typedef enum {
    BB_OBJECT_NONE = 0,
    BB_OBJECT_PROCESS,
    BB_OBJECT_THREAD,
    BB_OBJECT_FILE,
    BB_OBJECT_EVENT,
    BB_OBJECT_WINDOW,
    BB_OBJECT_BUFFER
} bb_object_type_t;

#define BB_RIGHT_READ   0x0001
#define BB_RIGHT_WRITE  0x0002
#define BB_RIGHT_WAIT   0x0004
#define BB_RIGHT_SIGNAL 0x0008

typedef struct {
    bb_handle_t value;
    bb_object_type_t type;
    uint32_t rights;
} bb_handle_info_t;

typedef struct {
    uint32_t api_major;
    uint32_t api_minor;
    uint32_t ticks;
    uint32_t total_frames;
    uint32_t free_frames;
    uint32_t heap_used;
    uint32_t heap_free;
} bb_system_info_t;

/* Kernel-facing ByteBandit system API. */
int bb_api_get_info(bb_system_info_t *info);
int bb_api_write(int fd, const char *buffer, uint32_t length);
int bb_api_task_list(void);
void bb_api_print_version(void);

#endif
