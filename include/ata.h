#ifndef BYTEBANDIT_ATA_H
#define BYTEBANDIT_ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512u

bool ata_present(void);
void ata_init(void);
bool ata_read_sector(uint32_t lba, void *buffer);
bool ata_write_sector(uint32_t lba, const void *buffer);

#endif
