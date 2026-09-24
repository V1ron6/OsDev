#include "fs/vfs.h"
#include "bb_api.h"
#include "serial.h"
#include <string.h>

#define VFS_FIRST_FD 3
#define VFS_MAX_FD 16
#define VFS_MAX_NODES 32
#define VFS_PATH_LENGTH 64
#define VFS_FILE_CAPACITY 4096

#define VFS_NODE_FILE 1
#define VFS_NODE_DIR  2

typedef struct {
    char path[VFS_PATH_LENGTH];
    uint8_t data[VFS_FILE_CAPACITY];
    uint32_t length;
    uint32_t mode;
    uint8_t type;
    bool used;
} vfs_node_t;

typedef struct {
    vfs_node_t *node;
    uint32_t offset;
    uint32_t flags;
} vfs_descriptor_t;

static vfs_node_t nodes[VFS_MAX_NODES];
static vfs_descriptor_t descriptors[VFS_MAX_FD];

static vfs_node_t *vfs_find(const char *path) {
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && strcmp(path, nodes[i].path) == 0) {
            return &nodes[i];
        }
    }
    return NULL;
}

static vfs_node_t *vfs_allocate(const char *path, uint8_t type) {
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            nodes[i].type = type;
            nodes[i].mode = type == VFS_NODE_DIR ? 0755 : 0644;
            nodes[i].length = 0;
            strncpy(nodes[i].path, path, VFS_PATH_LENGTH - 1);
            nodes[i].path[VFS_PATH_LENGTH - 1] = '\0';
            return &nodes[i];
        }
    }
    return NULL;
}

static bool vfs_valid_path(const char *path) {
    return path != NULL && path[0] == '/' && strlen(path) < VFS_PATH_LENGTH;
}

void vfs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    memset(descriptors, 0, sizeof(descriptors));

    vfs_node_t *motd = vfs_allocate("/etc/motd", VFS_NODE_FILE);
    vfs_node_t *config = vfs_allocate("/etc/system.conf", VFS_NODE_FILE);
    vfs_allocate("/etc", VFS_NODE_DIR);
    if (motd != NULL) {
        const char text[] = "ByteBandit OS\nA small Linux-first kernel with typed system handles.\n";
        memcpy(motd->data, text, sizeof(text) - 1);
        motd->length = sizeof(text) - 1;
    }
    if (config != NULL) {
        const char text[] = "api=0.1\nconsole=serial\ngraphics=disabled\n";
        memcpy(config->data, text, sizeof(text) - 1);
        config->length = sizeof(text) - 1;
    }
}

int32_t vfs_open(const char *path, uint32_t flags) {
    vfs_node_t *node;

    if (!vfs_valid_path(path) ||
        (flags & ~(BB_O_WRONLY | BB_O_RDWR | BB_O_CREAT | BB_O_TRUNC |
                   BB_O_APPEND)) != 0) {
        return BB_STATUS_INVALID;
    }

    node = vfs_find(path);
    if (node == NULL && (flags & BB_O_CREAT)) {
        node = vfs_allocate(path, VFS_NODE_FILE);
    }
    if (node == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    if (node->type == VFS_NODE_DIR && (flags & (BB_O_WRONLY | BB_O_RDWR))) {
        return BB_STATUS_INVALID;
    }
    if ((flags & BB_O_TRUNC) && node->type == VFS_NODE_FILE) {
        node->length = 0;
    }

    for (int32_t fd = VFS_FIRST_FD; fd < VFS_MAX_FD; fd++) {
        if (descriptors[fd].node == NULL) {
            descriptors[fd].node = node;
            descriptors[fd].flags = flags;
            descriptors[fd].offset = (flags & BB_O_APPEND) ? node->length : 0;
            return fd;
        }
    }
    return BB_STATUS_AGAIN;
}

int32_t vfs_read(int32_t fd, void *buffer, uint32_t length) {
    vfs_node_t *node;
    uint32_t available;
    uint32_t amount;

    if (fd < VFS_FIRST_FD || fd >= VFS_MAX_FD ||
        descriptors[fd].node == NULL || buffer == NULL ||
        (descriptors[fd].flags & BB_O_WRONLY) == BB_O_WRONLY) {
        return BB_STATUS_INVALID;
    }
    node = descriptors[fd].node;
    if (node->type != VFS_NODE_FILE) {
        return BB_STATUS_INVALID;
    }
    available = node->length - descriptors[fd].offset;
    amount = length < available ? length : available;
    memcpy(buffer, node->data + descriptors[fd].offset, amount);
    descriptors[fd].offset += amount;
    return (int32_t)amount;
}

int32_t vfs_write(int32_t fd, const void *buffer, uint32_t length) {
    vfs_node_t *node;
    uint32_t available;

    if (fd < VFS_FIRST_FD || fd >= VFS_MAX_FD ||
        descriptors[fd].node == NULL || buffer == NULL ||
        !(descriptors[fd].flags & (BB_O_WRONLY | BB_O_RDWR))) {
        return BB_STATUS_INVALID;
    }
    node = descriptors[fd].node;
    if (node->type != VFS_NODE_FILE || descriptors[fd].offset > VFS_FILE_CAPACITY) {
        return BB_STATUS_INVALID;
    }
    available = VFS_FILE_CAPACITY - descriptors[fd].offset;
    if (length > available) {
        length = available;
    }
    memcpy(node->data + descriptors[fd].offset, buffer, length);
    descriptors[fd].offset += length;
    if (descriptors[fd].offset > node->length) {
        node->length = descriptors[fd].offset;
    }
    return (int32_t)length;
}

int32_t vfs_close(int32_t fd) {
    if (fd < VFS_FIRST_FD || fd >= VFS_MAX_FD ||
        descriptors[fd].node == NULL) {
        return BB_STATUS_INVALID;
    }
    descriptors[fd].node = NULL;
    descriptors[fd].offset = 0;
    descriptors[fd].flags = 0;
    return BB_STATUS_OK;
}

int32_t vfs_mkdir(const char *path) {
    if (!vfs_valid_path(path) || vfs_find(path) != NULL) {
        return BB_STATUS_INVALID;
    }
    return vfs_allocate(path, VFS_NODE_DIR) == NULL ?
           BB_STATUS_AGAIN : BB_STATUS_OK;
}

int32_t vfs_unlink(const char *path) {
    vfs_node_t *node = vfs_find(path);
    if (node == NULL || node->type != VFS_NODE_FILE) {
        return BB_STATUS_NOT_FOUND;
    }
    for (int32_t fd = VFS_FIRST_FD; fd < VFS_MAX_FD; fd++) {
        if (descriptors[fd].node == node) {
            return BB_STATUS_AGAIN;
        }
    }
    memset(node, 0, sizeof(*node));
    return BB_STATUS_OK;
}

void vfs_list(void) {
    serial_puts("VFS nodes:\n");
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used) {
            serial_printf("  %s %s (%u bytes, mode %u)\n",
                          nodes[i].type == VFS_NODE_DIR ? "dir " : "file",
                          nodes[i].path, nodes[i].length, nodes[i].mode);
        }
    }
}
