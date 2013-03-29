#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "util.h"
#include "ext4fs.h"
#include "ext.h"

unsigned long part_offset;

part_descr_t part_info;

void ext4fs_set_blk_dev(void *rbdd, part_descr_t info)
{
	part_info = info;
	part_offset = info->off;
	get_fs()->total_sect = info->length;
	get_fs()->dev_desc = info;
}

int ext4fs_devread(int sector, int byte_offset, int byte_len, char *buf)
{
	unsigned block_len;
	char sec_buf[SECTOR_SIZE];

	/* Check partition boundaries */
	if ((sector < 0) || ((sector + ((byte_offset + byte_len - 1) >> SECTOR_BITS)) >=
		part_info->length)) {
		printf("%s read outside partition %d\n", __func__, sector);
		return 0;
	}

	/* Get the read to the beginning of a partition */
	sector += byte_offset >> SECTOR_BITS;
	byte_offset &= SECTOR_SIZE - 1;
#ifdef DEBUG
	printf(" <%d, %d, %d>\n", sector, byte_offset, byte_len);
#endif
	if (part_info == NULL) {
		printf("** Invalid Block Device Descriptor (NULL)\n");
		return 0;
	}

	if (byte_offset != 0) {
		/* read first part which isn't aligned with start of sector */
		if (part_read(part_info, sector, 1, (unsigned char *) sec_buf) < 0 ) {
			printf(" ** ext2fs_devread() read error **\n");
			return 0;
		}
		memcpy(buf, sec_buf + byte_offset,
			MIN(SECTOR_SIZE - byte_offset, byte_len));
		buf += MIN(SECTOR_SIZE - byte_offset, byte_len);
		byte_len -= MIN(SECTOR_SIZE - byte_offset, byte_len);
		sector++;
	}

	if (byte_len == 0)
		return 1;

	/* read sector aligned part */
	block_len = byte_len & ~(SECTOR_SIZE - 1);

	if (block_len == 0) {
		uint8_t p[SECTOR_SIZE];

		block_len = SECTOR_SIZE;
		part_read(part_info, sector, 1, (unsigned char *)p);
		memcpy(buf, p, byte_len);
		return 1;
	}

	if (part_read(part_info, sector,
					       block_len / SECTOR_SIZE,
					       (unsigned char *) buf) < 0) {
		printf(" ** %s read error - block\n", __func__);
		return 0;
	}
	block_len = byte_len & ~(SECTOR_SIZE - 1);
	buf += block_len;
	byte_len -= block_len;
	sector += block_len / SECTOR_SIZE;

	if (byte_len != 0) {
		/* read rest of data which are not in whole sector */
		if (part_read(part_info, sector, 1, (unsigned char *) sec_buf) < 0) {
			printf("* %s read error - last part\n", __func__);
			return 0;
		}
		memcpy(buf, sec_buf, byte_len);
	}
	return 1;
}
