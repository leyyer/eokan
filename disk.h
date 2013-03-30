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


#ifndef __EOKAN_DISK__
#define __EOKAN_DISK__

#include <stdint.h>

#define SECTOR_SIZE (0x200)
#define SECTOR_BITS		9

typedef struct part_descr *part_descr_t;
typedef struct disk_descr *disk_descr_t;
struct disk_descr {
	uint64_t (*capacity)(disk_descr_t );
	int      (*read) (disk_descr_t, int64_t start, int64_t num, uint8_t *buf);
	int      (*write)(disk_descr_t, int64_t start, int64_t num, const uint8_t *buf);
	void     (*release)(disk_descr_t);
};

/* disk open flags */
#define DISK_FLAG_READ      (1<<0)
#define DISK_FLAG_WRITE     (1<<1)
struct disk_probe_spec {
	const char *name;
	size_t size;
	int    (*probe)(disk_descr_t, const char *path, uint32_t flags);
};

disk_descr_t disk_open(const char *type, const char *path, uint32_t flags);
void         disk_close(disk_descr_t disk);
int          disk_read(disk_descr_t, int64_t start, int64_t num, uint8_t *buf);
int          disk_write(disk_descr_t, int64_t start, int64_t num, const uint8_t *buf);
part_descr_t disk_get_partition(disk_descr_t, int no);

struct part_descr {
	disk_descr_t disk;
	uint64_t     off;
	uint64_t     length;
};

int  part_read(part_descr_t, int64_t start, int64_t num, uint8_t *buf);
int  part_write(part_descr_t, int64_t start, int64_t num, const uint8_t *buf);
void part_close(part_descr_t);
#endif

