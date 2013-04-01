/*
 * Copyright (c) 2013, Renyi su <surenyi@gmail.com> * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __XOKAN_FS_H__
#define __XOKAN_FS_H__
struct part_descr;

struct xstat {
	uint16_t mode;
	uint32_t atime;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t dtime;
	uint32_t size;
	uint32_t size_high;
};

struct filesys_spec;
typedef struct file_entry * file_entry_t;
struct file_entry {
	int (*read)(struct file_entry *, struct filesys_spec*, int offset, char *buf, unsigned len);
	int (*stat)(struct file_entry *, struct filesys_spec *, struct xstat *st);
	int (*close)(struct file_entry *, struct filesys_spec *);
};

struct filesys_operations {
	struct filesys_spec *(*mount)(struct part_descr *);
	int (*dir_iterate)(struct filesys_spec *, const char *dir, int (*)(void *, const char *, struct xstat *, int is_dir), void *);
	int (*umount)(struct filesys_spec *);
	int (*label)(struct filesys_spec *, char *buf, int buflen);
	struct file_entry *(*open)(struct filesys_spec *, const char *file);
};

typedef struct filesys_descr *filesys_t;

int vfs_devread(struct part_descr *part_info, int sector, int byte_offset, int byte_len, char *buf);
filesys_t vfs_mount(struct part_descr* part);
int vfs_umount(filesys_t fsys);
int vfs_label(filesys_t, char *, int);
int vfs_dir_iterate(filesys_t, const char *dir, int (*)(void *, const char *, struct xstat *, int is_dir), void *);
file_entry_t vfs_open(filesys_t fs, const char *dir);
int vfs_file_read(file_entry_t filp, filesys_t fs, int offset,  char *buf, unsigned len);
int vfs_file_close(file_entry_t filp, filesys_t fs);
int vfs_file_stat(file_entry_t filp, filesys_t fs, struct xstat *);

#endif

