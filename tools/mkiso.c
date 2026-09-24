#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISO_SECTOR_SIZE 2048
#define BOOT_IMAGE_SECTORS 2880
#define BOOT_CATALOG_SECTOR 20
#define BOOT_IMAGE_SECTOR 21
#define ROOT_DIRECTORY_SECTOR 742
#define ISO_SECTORS 744

static void put_le16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
}

static void put_be16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)value;
}

static void put_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8);
    buffer[2] = (uint8_t)(value >> 16);
    buffer[3] = (uint8_t)(value >> 24);
}

static void put_be32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)value;
}

static void put_both16(uint8_t *buffer, uint16_t value) {
    put_le16(buffer, value);
    put_be16(buffer + 2, value);
}

static void put_both32(uint8_t *buffer, uint32_t value) {
    put_le32(buffer, value);
    put_be32(buffer + 4, value);
}

static int write_sector(FILE *image, uint32_t sector, const void *data) {
    if (fseek(image, (long)sector * ISO_SECTOR_SIZE, SEEK_SET) != 0) {
        return 0;
    }
    return fwrite(data, 1, ISO_SECTOR_SIZE, image) == ISO_SECTOR_SIZE;
}

static void make_root_record(uint8_t *record) {
    memset(record, 0, 34);
    record[0] = 34;
    record[1] = 0;
    put_both32(record + 2, ROOT_DIRECTORY_SECTOR);
    put_both32(record + 10, ISO_SECTOR_SIZE);
    record[25] = 2;
    put_both16(record + 28, 1);
    record[32] = 1;
    record[33] = 0;
}

static void make_primary_volume_descriptor(uint8_t *descriptor) {
    memset(descriptor, 0, ISO_SECTOR_SIZE);
    descriptor[0] = 1;
    memcpy(descriptor + 1, "CD001", 5);
    descriptor[6] = 1;
    memcpy(descriptor + 8, "BYTEBANDIT", 10);
    memcpy(descriptor + 40, "BYTEBANDIT_OS", 13);
    put_both32(descriptor + 80, ISO_SECTORS);
    put_both16(descriptor + 120, 1);
    put_both16(descriptor + 124, 1);
    put_both16(descriptor + 128, ISO_SECTOR_SIZE);
    put_le32(descriptor + 140, 0);
    put_be32(descriptor + 144, 0);
    make_root_record(descriptor + 156);
    memcpy(descriptor + 813, "BYTEBANDIT OS", 13);
}

static void make_boot_record(uint8_t *record) {
    memset(record, 0, ISO_SECTOR_SIZE);
    record[0] = 0;
    memcpy(record + 1, "CD001", 5);
    record[6] = 1;
    memcpy(record + 7, "EL TORITO SPECIFICATION", 23);
    put_le32(record + 71, BOOT_CATALOG_SECTOR);
}

static void make_boot_catalog(uint8_t *catalog) {
    uint16_t checksum = 0;

    memset(catalog, 0, ISO_SECTOR_SIZE);
    catalog[0] = 1;
    catalog[1] = 0;
    memcpy(catalog + 4, "BYTEBANDIT EL TORITO", 20);
    for (uint32_t i = 0; i < 28; i += 2) {
        checksum = (uint16_t)(checksum + catalog[i] +
                              ((uint16_t)catalog[i + 1] << 8));
    }
    put_le16(catalog + 28, (uint16_t)(0 - checksum));
    catalog[30] = 0x55;
    catalog[31] = 0xAA;

    catalog[32] = 0x88;
    catalog[33] = 0x02;
    put_le16(catalog + 38, 1);
    put_le32(catalog + 40, BOOT_IMAGE_SECTOR);
    catalog[64] = 0x91;
    catalog[65] = 0x00;
}

int main(int argc, char **argv) {
    FILE *boot = NULL;
    FILE *image = NULL;
    uint8_t sector[ISO_SECTOR_SIZE];
    uint8_t *boot_image = NULL;
    size_t boot_size;
    int result = EXIT_FAILURE;

    if (argc != 3) {
        fprintf(stderr, "usage: %s boot-image output-iso\n", argv[0]);
        return EXIT_FAILURE;
    }

    boot = fopen(argv[1], "rb");
    if (boot == NULL) {
        perror(argv[1]);
        goto done;
    }
    if (fseek(boot, 0, SEEK_END) != 0 ||
        (boot_size = (size_t)ftell(boot)) > BOOT_IMAGE_SECTORS * 512u ||
        fseek(boot, 0, SEEK_SET) != 0) {
        fprintf(stderr, "boot image must be at most 1.44 MB\n");
        goto done;
    }

    boot_image = calloc(BOOT_IMAGE_SECTORS, 512);
    if (boot_image == NULL ||
        fread(boot_image, 1, boot_size, boot) != boot_size) {
        fprintf(stderr, "unable to read boot image\n");
        goto done;
    }

    image = fopen(argv[2], "wb+");
    if (image == NULL) {
        perror(argv[2]);
        goto done;
    }
    if (fseek(image, (long)ISO_SECTORS * ISO_SECTOR_SIZE - 1, SEEK_SET) != 0 ||
        fputc(0, image) == EOF) {
        fprintf(stderr, "unable to size output ISO\n");
        goto done;
    }

    memset(sector, 0, sizeof(sector));
    if (!write_sector(image, 16, sector)) {
        goto done;
    }
    make_primary_volume_descriptor(sector);
    if (!write_sector(image, 16, sector)) {
        goto done;
    }
    make_boot_record(sector);
    if (!write_sector(image, 17, sector)) {
        goto done;
    }
    memset(sector, 0, sizeof(sector));
    sector[0] = 0xFF;
    memcpy(sector + 1, "CD001", 5);
    sector[6] = 1;
    if (!write_sector(image, 18, sector)) {
        goto done;
    }
    make_boot_catalog(sector);
    if (!write_sector(image, BOOT_CATALOG_SECTOR, sector) ||
        fseek(image, (long)BOOT_IMAGE_SECTOR * ISO_SECTOR_SIZE, SEEK_SET) != 0 ||
        fwrite(boot_image, 512, BOOT_IMAGE_SECTORS, image) != BOOT_IMAGE_SECTORS) {
        goto done;
    }
    memset(sector, 0, sizeof(sector));
    if (!write_sector(image, ROOT_DIRECTORY_SECTOR, sector)) {
        goto done;
    }

    result = EXIT_SUCCESS;

done:
    free(boot_image);
    if (boot != NULL) {
        fclose(boot);
    }
    if (image != NULL) {
        fclose(image);
    }
    return result;
}
