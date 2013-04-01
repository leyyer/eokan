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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "disk.h"

struct disk_dev {
	CRITICAL_SECTION lock;
	unsigned char disk_descr[1];
};

#define GET_DISKDEV(dk) ((struct disk_dev *)((unsigned char *)dk - offsetof(struct disk_dev, disk_descr)))
extern struct disk_probe_spec vmdk_disk_spec;
extern struct disk_probe_spec phy_disk_spec;

static struct disk_probe_spec *disks[] = {
	&phy_disk_spec,
	&vmdk_disk_spec,
	NULL
};

disk_descr_t disk_open(const char *type, const char *path, uint32_t flags)
{
	int x = 0;
	struct disk_probe_spec *dp;
	disk_descr_t disk = NULL;
	struct disk_dev *ddev;

	while (disks[x]) {
		dp = disks[x];
		if (strncasecmp(dp->name, type, strlen(type)) == 0) {
			ddev = calloc(1, sizeof *ddev + dp->size);
			if (ddev) {
				disk = (disk_descr_t)ddev->disk_descr;
				InitializeCriticalSection(&ddev->lock);
				if (dp->probe(disk, path, flags) < 0) {
					free(ddev);
					disk = NULL;
				}
			}
			break;
		}
		++x;
	}
	return disk;
}

void disk_close(disk_descr_t disk)
{
	struct disk_dev *ddk = GET_DISKDEV(disk);
	disk->release(disk);
	DeleteCriticalSection(&ddk->lock);
	free(ddk);
}

int disk_read(disk_descr_t disk, int64_t start, int64_t num, uint8_t *buf)
{
	int x;
	struct disk_dev *ddk = GET_DISKDEV(disk);
	EnterCriticalSection(&ddk->lock);
	x= disk->read(disk, start, num, buf);
	LeaveCriticalSection(&ddk->lock);
	return x;
}

int disk_write(disk_descr_t disk, int64_t start, int64_t num, const uint8_t *buf)
{
	int x;
	struct disk_dev *ddk = GET_DISKDEV(disk);
	EnterCriticalSection(&ddk->lock);
	x= disk->write(disk, start, num, buf);
	LeaveCriticalSection(&ddk->lock);
	return x;
}

static part_descr_t __alloc_partition(disk_descr_t disk, uint64_t off, uint64_t len)
{
	part_descr_t part;

	part = calloc(1, sizeof *part);
	part->disk = disk;
	part->off = off;
	part->length = len;

	return part;
}

static part_descr_t parse_logic_partition(disk_descr_t disk, uint32_t logic_off, uint32_t off, int *got, int no)
{
	part_descr_t part = NULL;
	unsigned char xbr[SECTOR_SIZE], *ep, status, type;
	uint32_t xoff, xlen;
	int x;

	if (disk_read(disk, logic_off + off, 1, xbr) < 0) {
		fprintf(stderr, "reading error.\n");
		goto xdone;
	}
	for (x = 0; x < 2; ++x) {
		ep = xbr + 16 * x + 446;
		status = *ep;
		type   = *(ep + 4);
		xoff    = *(uint32_t *)(ep + 8);
		xlen    = *(uint32_t *)(ep + 12);
		printf("xbr,part %d: status %d, type %d, off %u, len %u\n", x, status, type, xoff, xlen);
		if (type == 0x5 || type == 0xf) {
			part = parse_logic_partition(disk, logic_off, xoff, got, no);
			if (part)
				goto xdone;
		} else if (type > 0) {
			*got = *got + 1;
			printf("xbr: got %d, no: %d\n", *got, no);
			if (*got == no) {
				part = __alloc_partition(disk, logic_off + off + xoff, xlen);
				goto xdone;
			}
		}
	}
xdone:
	return part;
}

static part_descr_t disk_get_mbr_partition(disk_descr_t disk, int no)
{
	part_descr_t part = NULL;
	unsigned char mbr[SECTOR_SIZE], *ep, status, type;
	uint32_t off, len;
	int x, got=0;

	if (disk_read(disk, 0, 1, mbr) < 0) {
		goto done;
	}
	for (x = 0; x < 4; ++x ) {
		ep = mbr + 16 * x + 446;
		status = *ep;
		type   = *(ep + 4);
		off    = *(uint32_t *)(ep + 8);
		len    = *(uint32_t *)(ep + 12);
		printf("mbr,part %d: status %d, type %d, off %u, len %u\n", x, status, type, off, len);
		if (type == 0x5 || type == 0xf) {
			part = parse_logic_partition(disk, off, 0, &got, no);
			if (part)
				goto done;
		} else if (type > 0) {
			++got;
			printf("mbr: got %d, no: %d\n", got, no);
			if (got == no) {
				part = __alloc_partition(disk, off, len);
				goto done;
			}
		}
	}
done:
	return part;
}

part_descr_t disk_get_partition(disk_descr_t disk, int no)
{
	return disk_get_mbr_partition(disk, no);
}

void part_close(part_descr_t part)
{
	free(part);
}

int  part_read(part_descr_t part, int64_t start, int64_t num, uint8_t *buf)
{
	return disk_read(part->disk, start + part->off, num, buf);
}

int  part_write(part_descr_t part, int64_t start, int64_t num, const uint8_t *buf)
{
	return disk_write(part->disk, start + part->off, num, buf);
}

