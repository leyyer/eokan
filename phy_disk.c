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
#include "disk.h"
#include "util.h"

struct phy_disk {
	struct disk_descr    disk;
	uint64_t             capacity;
	HANDLE               hFile;
};

static uint64_t phy_disk_capacity(disk_descr_t disk)
{
	struct phy_disk *phy = (struct phy_disk *)disk;
	return phy->capacity;
}

static int __disk_seek(struct phy_disk *disk, int64_t offset /* sectors */)
{
	BOOL succ;
	LARGE_INTEGER off;
	off.QuadPart = offset * SECTOR_SIZE;
	succ = SetFilePointerEx(disk->hFile, off, NULL, FILE_BEGIN);
	return succ ? 0 : -1;
}

static int phy_disk_read(disk_descr_t disk, int64_t start, int64_t num, uint8_t *buf)
{
	BOOL succ;
	DWORD bytes = 0;
	struct phy_disk *phy = (struct phy_disk *)disk;

	__disk_seek(phy, start);
	num *= SECTOR_SIZE;
	succ = ReadFile(phy->hFile, buf, num, &bytes, NULL);
	if (!succ)
		return -1;
	return 0;
}

static int phy_disk_write(disk_descr_t disk, int64_t start, int64_t num, const uint8_t *buf)
{
	BOOL succ;
	DWORD bytes = 0;
	struct phy_disk *phy = (struct phy_disk *)disk;

	__disk_seek(phy, start);
	num *= SECTOR_SIZE;
	succ = WriteFile(phy->hFile, buf, num, &bytes, NULL);
	if (!succ)
		return -1;
	return 0;
}

static int phy_get_info(struct phy_disk *phy)
{
	BOOL succ;
	LARGE_INTEGER size;

	succ = GetFileSizeEx(phy->hFile, &size);
	if (!succ) {
		printf("can't get capacity size\n");
		return -1;
	}
	phy->capacity = size.QuadPart / SECTOR_SIZE;
	fprintf(stderr, "disk: %I64u sectors\n", phy->capacity);
	return 0;
}

static void   phy_disk_release(disk_descr_t disk)
{
	struct phy_disk *phy = (struct phy_disk *)disk;
	CloseHandle(phy->hFile);
}

static int phy_disk_create(disk_descr_t disk, const char *path, uint32_t flags)
{
	DWORD f0 = 0, f1 = 0;
	wchar_t xpath[MAX_PATH];
	struct phy_disk *phy = (struct phy_disk *)disk;

	if (flags & DISK_FLAG_READ) {
		f0 |= GENERIC_READ;
		f1 |= FILE_SHARE_READ;
	}
	if (flags & DISK_FLAG_WRITE) {
		f0 |= GENERIC_WRITE;
		f1 |= FILE_SHARE_WRITE;
	}

	utf8_to_utf16((const unsigned char *)path, strlen(path), xpath, MAX_PATH);
	phy->hFile= CreateFile(xpath, f0, f1, NULL, OPEN_EXISTING, 0, NULL);
	if (phy->hFile == INVALID_HANDLE_VALUE) {
		fwprintf(stderr, L"can't open device: %s [%d]", xpath, GetLastError());
		return -1;
	}
	phy_get_info(phy);
	disk->release  = phy_disk_release;
	disk->read     = phy_disk_read;
	disk->write    = phy_disk_write;
	disk->capacity = phy_disk_capacity;
	return 0;
}

struct disk_probe_spec phy_disk_spec = {
	.name  = "physical",
	.size  = sizeof (struct phy_disk),
	.probe = phy_disk_create,
};

