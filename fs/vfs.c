#include "fs/vfs.h"
#include "bb_api.h"
#include "serial.h"
#include "kernel/task.h"
#include "ata.h"
#include <string.h>

#define VFS_FIRST_FD 3
#define VFS_MAX_FD 16
#define VFS_MAX_NODES 64
#define VFS_PATH_LENGTH 96
#define VFS_DIR_NAME_LENGTH 32
#define VFS_FILE_CAPACITY 4096

#define VFS_NODE_FILE 1
#define VFS_NODE_DIR  2

#define VFS_DISK_MAGIC 0x42594653u
#define VFS_DISK_VERSION 2u
#define VFS_DISK_FIRST_LBA 1u
#define VFS_DISK_NODE_SECTORS 9u
#define VFS_DISK_SLOT_SECTORS (1u + VFS_MAX_NODES * VFS_DISK_NODE_SECTORS)
#define VFS_DISK_COMMITTED 0x434f4d4du

typedef struct {
    char path[VFS_PATH_LENGTH];
    char name[VFS_DIR_NAME_LENGTH];
    uint8_t data[VFS_FILE_CAPACITY];
    uint32_t length;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t parent;
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
static bool vfs_storage_ready;
static uint32_t vfs_active_slot;
static uint32_t vfs_generation;

static uint32_t vfs_slot_lba(uint32_t slot) {
    return VFS_DISK_FIRST_LBA + slot * VFS_DISK_SLOT_SECTORS;
}

static uint32_t vfs_node_lba(uint32_t slot, uint32_t index) {
    return vfs_slot_lba(slot) + 1 + index * VFS_DISK_NODE_SECTORS;
}

static uint32_t vfs_crc32_update(uint32_t crc, const uint8_t *data,
                                 uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

static void vfs_encode_node(uint8_t *sector, const vfs_node_t *node) {
    memset(sector, 0, ATA_SECTOR_SIZE);
    sector[0] = node->used ? 1 : 0;
    sector[1] = node->type;
    memcpy(sector + 4, &node->length, sizeof(node->length));
    memcpy(sector + 8, &node->mode, sizeof(node->mode));
    memcpy(sector + 12, &node->uid, sizeof(node->uid));
    memcpy(sector + 16, &node->gid, sizeof(node->gid));
    memcpy(sector + 20, &node->parent, sizeof(node->parent));
    memcpy(sector + 24, node->path, sizeof(node->path));
    memcpy(sector + 120, node->name, sizeof(node->name));
}

static void vfs_decode_node(vfs_node_t *node, const uint8_t *sector) {
    memset(node, 0, sizeof(*node));
    node->used = sector[0] != 0;
    node->type = sector[1];
    memcpy(&node->length, sector + 4, sizeof(node->length));
    memcpy(&node->mode, sector + 8, sizeof(node->mode));
    memcpy(&node->uid, sector + 12, sizeof(node->uid));
    memcpy(&node->gid, sector + 16, sizeof(node->gid));
    memcpy(&node->parent, sector + 20, sizeof(node->parent));
    memcpy(node->path, sector + 24, sizeof(node->path));
    memcpy(node->name, sector + 120, sizeof(node->name));
    node->path[VFS_PATH_LENGTH - 1] = '\0';
    node->name[VFS_DIR_NAME_LENGTH - 1] = '\0';
    if (node->length > VFS_FILE_CAPACITY) {
        node->length = VFS_FILE_CAPACITY;
    }
}

static bool vfs_read_slot_header(uint32_t slot, uint32_t *generation,
                                 uint32_t *checksum) {
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t magic;
    uint32_t version;
    uint32_t committed;

    if (!ata_read_sector(vfs_slot_lba(slot), sector)) {
        return false;
    }
    memcpy(&magic, sector, sizeof(magic));
    memcpy(&version, sector + 4, sizeof(version));
    memcpy(generation, sector + 8, sizeof(*generation));
    memcpy(&committed, sector + 12, sizeof(committed));
    memcpy(checksum, sector + 16, sizeof(*checksum));
    if (magic != VFS_DISK_MAGIC || version != VFS_DISK_VERSION ||
        committed != VFS_DISK_COMMITTED) {
        return false;
    }
    return true;
}

static bool vfs_load_slot(uint32_t slot, uint32_t *generation) {
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t expected_checksum;
    uint32_t checksum = 0xFFFFFFFFu;

    if (!vfs_read_slot_header(slot, generation, &expected_checksum)) {
        return false;
    }

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!ata_read_sector(vfs_node_lba(slot, i), sector)) {
            return false;
        }
        checksum = vfs_crc32_update(checksum, sector, ATA_SECTOR_SIZE);
        vfs_decode_node(&nodes[i], sector);
        memset(nodes[i].data, 0, sizeof(nodes[i].data));
        for (uint32_t part = 0; part < VFS_FILE_CAPACITY / ATA_SECTOR_SIZE; part++) {
            if (!ata_read_sector(vfs_node_lba(slot, i) + 1 + part,
                                 nodes[i].data + part * ATA_SECTOR_SIZE)) {
                return false;
            }
            checksum = vfs_crc32_update(checksum,
                                        nodes[i].data + part * ATA_SECTOR_SIZE,
                                        ATA_SECTOR_SIZE);
        }
    }
    checksum ^= 0xFFFFFFFFu;
    return nodes[0].used && nodes[0].type == VFS_NODE_DIR &&
           checksum == expected_checksum;
}

static bool vfs_load_from_disk(void) {
    uint32_t generation[2] = {0, 0};
    uint32_t checksum[2] = {0, 0};
    bool valid[2] = {false, false};
    uint32_t selected_slot;

    if (!vfs_storage_ready) {
        return false;
    }
    valid[0] = vfs_read_slot_header(0, &generation[0], &checksum[0]);
    valid[1] = vfs_read_slot_header(1, &generation[1], &checksum[1]);
    if (!valid[0] && !valid[1]) {
        return false;
    }
    selected_slot = valid[1] && (!valid[0] || generation[1] > generation[0]) ? 1 : 0;
    vfs_active_slot = selected_slot;
    vfs_generation = generation[selected_slot];
    if (!vfs_load_slot(selected_slot, &generation[selected_slot])) {
        return false;
    }
    return true;
}

static void vfs_save_to_disk(void) {
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t target_slot;
    uint32_t next_generation;
    uint32_t checksum = 0xFFFFFFFFu;

    if (!vfs_storage_ready) {
        return;
    }
    target_slot = vfs_active_slot ^ 1u;
    next_generation = vfs_generation + 1;

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        vfs_encode_node(sector, &nodes[i]);
        if (!ata_write_sector(vfs_node_lba(target_slot, i), sector)) {
            vfs_storage_ready = false;
            return;
        }
        checksum = vfs_crc32_update(checksum, sector, ATA_SECTOR_SIZE);
        for (uint32_t part = 0; part < VFS_FILE_CAPACITY / ATA_SECTOR_SIZE; part++) {
            checksum = vfs_crc32_update(checksum,
                                        nodes[i].data + part * ATA_SECTOR_SIZE,
                                        ATA_SECTOR_SIZE);
            if (!ata_write_sector(vfs_node_lba(target_slot, i) + 1 + part,
                                  nodes[i].data + part * ATA_SECTOR_SIZE)) {
                vfs_storage_ready = false;
                return;
            }
        }
    }

    checksum ^= 0xFFFFFFFFu;
    memset(sector, 0, sizeof(sector));
    memcpy(sector, &(uint32_t){VFS_DISK_MAGIC}, sizeof(uint32_t));
    memcpy(sector + 4, &(uint32_t){VFS_DISK_VERSION}, sizeof(uint32_t));
    memcpy(sector + 8, &next_generation, sizeof(next_generation));
    memcpy(sector + 12, &(uint32_t){VFS_DISK_COMMITTED}, sizeof(uint32_t));
    memcpy(sector + 16, &checksum, sizeof(checksum));
    if (!ata_write_sector(vfs_slot_lba(target_slot), sector)) {
        vfs_storage_ready = false;
        return;
    }
    vfs_active_slot = target_slot;
    vfs_generation = next_generation;
}

static bool vfs_valid_path(const char *path) {
    return path != NULL && path[0] == '/' && strlen(path) < VFS_PATH_LENGTH;
}

static vfs_node_t *vfs_find(const char *path) {
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && strcmp(path, nodes[i].path) == 0) {
            return &nodes[i];
        }
    }
    return NULL;
}

static vfs_node_t *vfs_parent_node(const char *path) {
    char temp[VFS_PATH_LENGTH];
    char *slash;

    if (path == NULL || *path == '\0' || strcmp(path, "/") == 0) {
        return NULL;
    }

    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    slash = strrchr(temp, '/');
    if (slash == NULL) {
        return NULL;
    }
    if (slash == temp) {
        return vfs_find("/");
    }
    *slash = '\0';
    return vfs_find(temp);
}

static vfs_node_t *vfs_allocate(const char *path, uint8_t type) {
    vfs_node_t *parent = vfs_parent_node(path);
    uint32_t parent_index;

    if (!vfs_valid_path(path)) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return NULL;
    }
    if (parent == NULL || !parent->used || parent->type != VFS_NODE_DIR) {
        return NULL;
    }
    parent_index = (uint32_t)(parent - nodes);

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            nodes[i].type = type;
            nodes[i].uid = task_get_current_uid();
            nodes[i].gid = task_get_current_gid();
            nodes[i].mode = type == VFS_NODE_DIR ? 0755u : 0644u;
            nodes[i].length = 0;
            nodes[i].parent = parent_index;
            memset(nodes[i].data, 0, sizeof(nodes[i].data));
            strncpy(nodes[i].path, path, VFS_PATH_LENGTH - 1);
            nodes[i].path[VFS_PATH_LENGTH - 1] = '\0';

            char *last = strrchr(nodes[i].path, '/');
            if (last != NULL && last[1] != '\0') {
                strncpy(nodes[i].name, last + 1, VFS_DIR_NAME_LENGTH - 1);
                nodes[i].name[VFS_DIR_NAME_LENGTH - 1] = '\0';
            } else {
                strncpy(nodes[i].name, "/", VFS_DIR_NAME_LENGTH - 1);
                nodes[i].name[VFS_DIR_NAME_LENGTH - 1] = '\0';
            }
            return &nodes[i];
        }
    }
    return NULL;
}

static bool vfs_node_has_access(const vfs_node_t *node, uint32_t requested_access) {
    if (node == NULL) {
        return false;
    }
    return task_current_has_access(node->uid, node->gid, node->mode,
                                   requested_access);
}

void vfs_init(void) {
    memset(nodes, 0, sizeof(nodes));
    memset(descriptors, 0, sizeof(descriptors));
    ata_init();
    vfs_storage_ready = ata_present();

    nodes[0].used = true;
    nodes[0].type = VFS_NODE_DIR;
    nodes[0].uid = 0;
    nodes[0].gid = 0;
    nodes[0].mode = 0755u;
    nodes[0].length = 0;
    nodes[0].parent = 0;
    strncpy(nodes[0].path, "/", VFS_PATH_LENGTH - 1);
    nodes[0].path[VFS_PATH_LENGTH - 1] = '\0';
    strncpy(nodes[0].name, "/", VFS_DIR_NAME_LENGTH - 1);
    nodes[0].name[VFS_DIR_NAME_LENGTH - 1] = '\0';

    vfs_node_t *etc = vfs_allocate("/etc", VFS_NODE_DIR);
    vfs_node_t *motd = vfs_allocate("/etc/motd", VFS_NODE_FILE);
    vfs_node_t *config = vfs_allocate("/etc/system.conf", VFS_NODE_FILE);

    if (etc != NULL) {
        etc->uid = 0;
        etc->gid = 0;
        etc->mode = 0755u;
    }

    if (motd != NULL) {
        const char text[] = "ByteBandit OS\nA small Linux-first kernel with typed system handles.\n";
        memcpy(motd->data, text, sizeof(text) - 1);
        motd->length = sizeof(text) - 1;
        motd->uid = 0;
        motd->gid = 0;
        motd->mode = 0644u;
    }

    if (config != NULL) {
        const char text[] = "api=0.3\nconsole=serial\ngraphics=disabled\n";
        memcpy(config->data, text, sizeof(text) - 1);
        config->length = sizeof(text) - 1;
        config->uid = 0;
        config->gid = 0;
        config->mode = 0644u;
    }

    if (vfs_load_from_disk()) {
        serial_puts("[VFS] Loaded persistent ByteFS volume\n");
    } else {
        serial_puts(vfs_storage_ready ?
                    "[VFS] Formatting persistent ByteFS volume\n" :
                    "[VFS] No ATA disk found; using volatile storage\n");
        vfs_save_to_disk();
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
    vfs_node_t *parent = vfs_parent_node(path);
    if (parent == NULL || !vfs_node_has_access(parent, VFS_ACCESS_EXEC)) {
        return BB_STATUS_PERM;
    }
    if (node == NULL && (flags & BB_O_CREAT)) {
        if (!vfs_node_has_access(parent, VFS_ACCESS_WRITE)) {
            return BB_STATUS_PERM;
        }
        node = vfs_allocate(path, VFS_NODE_FILE);
        if (node != NULL) {
            vfs_save_to_disk();
        }
    }
    if (node == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    if (node->type == VFS_NODE_DIR && (flags & (BB_O_WRONLY | BB_O_RDWR))) {
        return BB_STATUS_INVALID;
    }
    if (flags & BB_O_WRONLY || flags & BB_O_RDWR) {
        if (!vfs_node_has_access(node, VFS_ACCESS_WRITE)) {
            return BB_STATUS_PERM;
        }
    } else if (!vfs_node_has_access(node, VFS_ACCESS_READ)) {
        return BB_STATUS_PERM;
    }
    if ((flags & BB_O_TRUNC) && node->type == VFS_NODE_FILE) {
        node->length = 0;
        memset(node->data, 0, sizeof(node->data));
        vfs_save_to_disk();
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
    if (node->type != VFS_NODE_FILE || !vfs_node_has_access(node, VFS_ACCESS_READ)) {
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
    if (node->type != VFS_NODE_FILE ||
        !vfs_node_has_access(node, VFS_ACCESS_WRITE) ||
        descriptors[fd].offset > VFS_FILE_CAPACITY) {
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
    vfs_save_to_disk();
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
    vfs_node_t *parent;

    if (!vfs_valid_path(path) || vfs_find(path) != NULL) {
        return BB_STATUS_INVALID;
    }
    if (strcmp(path, "/") == 0) {
        return BB_STATUS_INVALID;
    }
    parent = vfs_parent_node(path);
    if (parent == NULL || !vfs_node_has_access(parent,
                                                VFS_ACCESS_WRITE | VFS_ACCESS_EXEC)) {
        return BB_STATUS_PERM;
    }
    if (vfs_allocate(path, VFS_NODE_DIR) == NULL) {
        return BB_STATUS_AGAIN;
    }
    vfs_save_to_disk();
    return BB_STATUS_OK;
}

int32_t vfs_unlink(const char *path) {
    vfs_node_t *node = vfs_find(path);
    vfs_node_t *parent;
    if (node == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    if (strcmp(path, "/") == 0 || node->type == VFS_NODE_DIR) {
        return BB_STATUS_INVALID;
    }
    parent = vfs_parent_node(path);
    if (parent == NULL || !vfs_node_has_access(parent,
                                                VFS_ACCESS_WRITE | VFS_ACCESS_EXEC)) {
        return BB_STATUS_PERM;
    }
    for (int32_t fd = VFS_FIRST_FD; fd < VFS_MAX_FD; fd++) {
        if (descriptors[fd].node == node) {
            return BB_STATUS_AGAIN;
        }
    }
    memset(node, 0, sizeof(*node));
    vfs_save_to_disk();
    return BB_STATUS_OK;
}

int32_t vfs_chmod(const char *path, uint32_t mode) {
    vfs_node_t *node = vfs_find(path);
    if (node == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    if (task_get_current_uid() != 0 && task_get_current_uid() != node->uid) {
        return BB_STATUS_PERM;
    }
    node->mode = mode & 07777;
    vfs_save_to_disk();
    return BB_STATUS_OK;
}

void vfs_list(void) {
    serial_puts("VFS nodes:\n");
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used) {
            serial_printf("  %s %s (%u bytes, mode %u, uid=%u gid=%u)\n",
                          nodes[i].type == VFS_NODE_DIR ? "dir " : "file",
                          nodes[i].path, nodes[i].length, nodes[i].mode,
                          nodes[i].uid, nodes[i].gid);
        }
    }
}

int32_t vfs_stat(const char *path) {
    vfs_node_t *node = vfs_find(path);

    if (node == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    serial_printf("%s: %s, size=%u, mode=%u, uid=%u, gid=%u, parent=%u\n",
                  node->path, node->type == VFS_NODE_DIR ? "directory" : "file",
                  node->length, node->mode, node->uid, node->gid, node->parent);
    return BB_STATUS_OK;
}

void vfs_sync(void) {
    vfs_save_to_disk();
}
