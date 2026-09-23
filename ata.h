/* ATA PIO driver: primary bus, LBA28, polled. Enough to identify
 * disks and copy the OS image to hard disk (see `install`). */
#ifndef ATA_H
#define ATA_H

#include "drivers.h"

typedef struct {
    int present;          /* IDENTIFY ok, ATA (not ATAPI) */
    int slave;            /* 0 master, 1 slave */
    char model[41];       /* byte-swapped, trimmed, NUL-terminated */
    u32 sectors;          /* LBA28 total (words 60-61) */
} ata_dev_t;

/* probe primary master+slave; 0 if at least one ATA disk found */
int ata_init(void);
/* info about drive sel (0/1); 0 ok, -1 absent */
int ata_info(int sel, ata_dev_t *out);
/* polled LBA28 sector I/O on drive sel; 0 ok, -1 error */
int ata_read(int sel, u32 lba, u8 *buf, u32 count);
int ata_write(int sel, u32 lba, const u8 *buf, u32 count);

#endif
