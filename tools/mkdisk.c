#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512u
#define PARTITION_START 2048u
#define PARTITION_TYPE 0xDAu
#define VFS_MAX_NODES 64u
#define VFS_PATH_LENGTH 96u
#define VFS_DIR_NAME_LENGTH 32u
#define VFS_FILE_CAPACITY 4096u
#define VFS_NODE_SECTORS 9u
#define VFS_SLOT_SECTORS (1u + VFS_MAX_NODES * VFS_NODE_SECTORS)
#define VFS_MAGIC 0x42594653u
#define VFS_VERSION 2u
#define VFS_COMMITTED 0x434f4d4du

static void put_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data,
                             uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

static int write_sector(FILE *image, uint32_t lba, const uint8_t *sector) {
    if (fseek(image, (long)lba * SECTOR_SIZE, SEEK_SET) != 0 ||
        fwrite(sector, 1, SECTOR_SIZE, image) != SECTOR_SIZE) {
        return 0;
    }
    return 1;
}

static void encode_node(uint8_t *sector, const char *path, const char *name,
                        uint32_t length, uint32_t mode, uint32_t parent,
                        uint8_t type) {
    memset(sector, 0, SECTOR_SIZE);
    sector[0] = 1;
    sector[1] = type;
    memcpy(sector + 4, &length, sizeof(length));
    memcpy(sector + 8, &mode, sizeof(mode));
    memcpy(sector + 24, path, strlen(path) + 1);
    memcpy(sector + 120, name, strlen(name) + 1);
    memcpy(sector + 20, &parent, sizeof(parent));
}

static int seed_smoke(FILE *image, const char *source_path) {
    uint8_t nodes[VFS_MAX_NODES][SECTOR_SIZE];
    uint8_t data[VFS_MAX_NODES][VFS_FILE_CAPACITY];
    uint8_t header[SECTOR_SIZE];
    uint8_t source[VFS_FILE_CAPACITY];
    FILE *source_file;
    size_t source_size;
    uint32_t checksum = 0xFFFFFFFFu;
    uint32_t base_lba = PARTITION_START + 2;

    source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        perror(source_path);
        return 0;
    }
    if (fseek(source_file, 0, SEEK_END) != 0 ||
        (source_size = (size_t)ftell(source_file)) > VFS_FILE_CAPACITY ||
        fseek(source_file, 0, SEEK_SET) != 0 ||
        fread(source, 1, source_size, source_file) != source_size) {
        fprintf(stderr, "unable to read smoke ELF (max %u bytes)\n",
                VFS_FILE_CAPACITY);
        fclose(source_file);
        return 0;
    }
    fclose(source_file);

    memset(nodes, 0, sizeof(nodes));
    memset(data, 0, sizeof(data));
    encode_node(nodes[0], "/", "/", 0, 0755u, 0, 2);
    encode_node(nodes[1], "/bin", "bin", 0, 0755u, 0, 2);
    encode_node(nodes[2], "/bin/smoke", "smoke", (uint32_t)source_size,
                0755u, 1, 1);
    memcpy(data[2], source, source_size);

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        uint32_t node_lba = base_lba + i * VFS_NODE_SECTORS;
        if (!write_sector(image, node_lba, nodes[i])) {
            return 0;
        }
        checksum = crc32_update(checksum, nodes[i], SECTOR_SIZE);
        for (uint32_t part = 0; part < VFS_FILE_CAPACITY / SECTOR_SIZE;
             part++) {
            if (!write_sector(image, node_lba + 1 + part,
                              data[i] + part * SECTOR_SIZE)) {
                return 0;
            }
            checksum = crc32_update(checksum,
                                    data[i] + part * SECTOR_SIZE, SECTOR_SIZE);
        }
    }

    checksum ^= 0xFFFFFFFFu;
    memset(header, 0, sizeof(header));
    put_le32(header, VFS_MAGIC);
    put_le32(header + 4, VFS_VERSION);
    put_le32(header + 8, 1);
    put_le32(header + 12, VFS_COMMITTED);
    put_le32(header + 16, checksum);
    return write_sector(image, PARTITION_START + 1, header);
}

int main(int argc, char **argv) {
    FILE *image;
    uint8_t sector[SECTOR_SIZE];
    long size;
    uint32_t sectors;
    uint32_t partition_sectors;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: %s disk-image [smoke-elf]\n", argv[0]);
        return EXIT_FAILURE;
    }

    image = fopen(argv[1], "rb+");
    if (image == NULL) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }
    if (fseek(image, 0, SEEK_END) != 0 || (size = ftell(image)) < 0 ||
        fseek(image, 0, SEEK_SET) != 0) {
        fprintf(stderr, "unable to inspect disk image\n");
        fclose(image);
        return EXIT_FAILURE;
    }

    sectors = (uint32_t)size / SECTOR_SIZE;
    if (sectors <= PARTITION_START) {
        fprintf(stderr, "disk image is too small for a partition\n");
        fclose(image);
        return EXIT_FAILURE;
    }
    partition_sectors = sectors - PARTITION_START;

    memset(sector, 0, sizeof(sector));
    sector[446 + 4] = PARTITION_TYPE;
    put_le32(sector + 446 + 8, PARTITION_START);
    put_le32(sector + 446 + 12, partition_sectors);
    sector[510] = 0x55;
    sector[511] = 0xAA;

    if (fwrite(sector, 1, sizeof(sector), image) != sizeof(sector)) {
        fprintf(stderr, "unable to write partition table\n");
        fclose(image);
        return EXIT_FAILURE;
    }
    if (argc == 3 && !seed_smoke(image, argv[2])) {
        fprintf(stderr, "unable to seed smoke ELF\n");
        fclose(image);
        return EXIT_FAILURE;
    }
    fclose(image);
    return EXIT_SUCCESS;
}
