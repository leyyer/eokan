/*
 * Copyright (c) 2013, Renyi su <surenyi@gmail.com> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer. Redistributions in binary form must
 * reproduce the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "util.h"
#include "fs.h"

struct filesys_descr {
	struct filesys_operations *fs_ops;
	struct filesys_spec       *fs_data;
};

int vfs_devread(part_descr_t part_info, int sector, int byte_offset, int byte_len, char *buf)
{
	unsigned block_len;
	unsigned char sec_buf[SECTOR_SIZE];

	/* Check partition boundaries */
	if ((sector < 0) || ((sector + ((byte_offset + byte_len - 1) >> SECTOR_BITS)) >= part_info->length)) {
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
		if (part_read(part_info, sector, 1, sec_buf) < 0 ) {
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
		part_read(part_info, sector, 1, p);
		memcpy(buf, p, byte_len);
		return 1;
	}

	if (part_read(part_info, sector, block_len / SECTOR_SIZE, buf) < 0) {
		printf(" ** %s read error - block\n", __func__);
		return 0;
	}
	block_len = byte_len & ~(SECTOR_SIZE - 1);
	buf += block_len;
	byte_len -= block_len;
	sector += block_len / SECTOR_SIZE;

	if (byte_len != 0) {
		/* read rest of data which are not in whole sector */
		if (part_read(part_info, sector, 1, sec_buf) < 0) {
			printf("* %s read error - last part\n", __func__);
			return 0;
		}
		memcpy(buf, sec_buf, byte_len);
	}
	return 1;
}

extern struct filesys_operations extfs_operations;
static struct filesys_operations * allfs[] = {
	&extfs_operations,
	NULL,
};

filesys_t vfs_mount(part_descr_t part)
{
	struct filesys_operations *fop;
	struct filesys_spec *fs;
	filesys_t fsys;
	int x = 0;

	while (allfs[x]) {
		fop = allfs[x];
		if ((fs = fop->mount(part)) != NULL) {
			fsys = calloc(1, sizeof *fsys);
			fsys->fs_ops = fop;
			fsys->fs_data = fs;
			return fsys;
		}
		++x;
	}
	fprintf(stderr, "can't find support file system.\n");
	return NULL;
}

int vfs_dir_iterate(filesys_t fs, const char *dir, int (*foreach)(void *, const char *, struct xstat *, int is_dir), void *user_data)
{
	return fs->fs_ops->dir_iterate(fs->fs_data, dir, foreach, user_data);
}

int vfs_umount(filesys_t fsys)
{
	int x;
	x = fsys->fs_ops->umount(fsys->fs_data);
	free(fsys);
	return x;
}

int vfs_label(filesys_t fsys, char *buf, int size)
{
	return fsys->fs_ops->label(fsys->fs_data, buf, size);
}

file_entry_t vfs_open(filesys_t fs, const char *dir)
{
	return fs->fs_ops->open(fs->fs_data, dir);
}

int vfs_file_read(file_entry_t filp, filesys_t fs, int offset, char *buf, unsigned len)
{
	return filp->read(filp, fs->fs_data, offset,  buf, len);
}

int vfs_file_close(file_entry_t filp, filesys_t fs)
{
	return filp->close(filp, fs->fs_data);
}

int vfs_file_stat(file_entry_t filp, filesys_t fs, struct xstat *st)
{
	return filp->stat(filp, fs->fs_data, st);
}

