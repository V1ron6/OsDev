#include "ata.h"
#include "x86.h"
#include <string.h>

#define ATA_DATA       0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30
#define ATA_STATUS_ERR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_BSY 0x80
#define ATA_TIMEOUT    1000000u

static uint32_t ata_partition_start;

static bool ata_prepare(uint32_t lba, uint8_t command);

static bool ata_wait(uint8_t required) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0xFF || (status & ATA_STATUS_ERR) != 0) {
            return false;
        }
        if ((status & (ATA_STATUS_BSY | required)) == required) {
            return true;
        }
    }
    return false;
}

static void ata_select(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0u | ((lba >> 24) & 0x0Fu)));
    for (uint32_t i = 0; i < 4; i++) {
        (void)inb(ATA_STATUS);
    }
}

bool ata_present(void) {
    ata_select(0);
    return inb(ATA_STATUS) != 0xFF;
}

void ata_init(void) {
    uint8_t sector[ATA_SECTOR_SIZE];
    uint32_t start;

    ata_partition_start = 0;
    if (!ata_present()) {
        return;
    }
    if (!ata_prepare(0, ATA_CMD_READ)) {
        return;
    }
    for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
        ((uint16_t *)sector)[i] = inw(ATA_DATA);
    }
    if (!ata_wait(0) || sector[510] != 0x55 || sector[511] != 0xAA ||
        sector[446 + 4] == 0) {
        return;
    }
    memcpy(&start, sector + 446 + 8, sizeof(start));
    ata_partition_start = start;
}

static bool ata_prepare(uint32_t lba, uint8_t command) {
    ata_select(lba);
    if (!ata_wait(0)) {
        return false;
    }
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, command);
    return ata_wait(ATA_STATUS_DRQ);
}

bool ata_read_sector(uint32_t lba, void *buffer) {
    uint16_t *words = (uint16_t *)buffer;

    if (buffer == NULL || !ata_prepare(lba + ata_partition_start, ATA_CMD_READ)) {
        return false;
    }
    for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
        words[i] = inw(ATA_DATA);
    }
    return ata_wait(0);
}

bool ata_write_sector(uint32_t lba, const void *buffer) {
    const uint16_t *words = (const uint16_t *)buffer;

    if (buffer == NULL || !ata_prepare(lba + ata_partition_start, ATA_CMD_WRITE)) {
        return false;
    }
    for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 2; i++) {
        outw(ATA_DATA, words[i]);
    }
    return ata_wait(0);
}
