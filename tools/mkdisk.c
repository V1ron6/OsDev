#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR_SIZE 512u
#define PARTITION_START 2048u
#define PARTITION_TYPE 0xDAu

static void put_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
}

int main(int argc, char **argv) {
    FILE *image;
    uint8_t sector[SECTOR_SIZE];
    long size;
    uint32_t sectors;
    uint32_t partition_sectors;

    if (argc != 2) {
        fprintf(stderr, "usage: %s disk-image\n", argv[0]);
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
    fclose(image);
    return EXIT_SUCCESS;
}
