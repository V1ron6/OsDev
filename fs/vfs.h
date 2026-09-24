#ifndef BYTEBANDIT_VFS_H
#define BYTEBANDIT_VFS_H

#include "types.h"

void vfs_init(void);
int32_t vfs_open(const char *path, uint32_t flags);
int32_t vfs_read(int32_t fd, void *buffer, uint32_t length);
int32_t vfs_write(int32_t fd, const void *buffer, uint32_t length);
int32_t vfs_close(int32_t fd);
int32_t vfs_mkdir(const char *path);
int32_t vfs_unlink(const char *path);
void vfs_list(void);

#endif
