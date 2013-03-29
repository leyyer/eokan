/*
 * Copyright (c) 2013, Renyi su <surenyi@gmail.com> * All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the distribution.

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

#ifndef __EXT4_COMMON__
#define __EXT4_COMMON__
#include "ext.h"
#include "ext4fs.h"
#if defined(CONFIG_EXT4_WRITE)
#include "ext4_jour.h"
#include "crc16.h"
#endif
#include "fs.h"
#define YES		1
#define NO		0
#define TRUE		1
#define FALSE		0
#define RECOVER	1
#define SCAN		0

#define S_IFLNK		0120000		/* symbolic link */
#define BLOCK_NO_ONE		1
#define SUPERBLOCK_SECTOR	2
#define SUPERBLOCK_SIZE	1024
#define F_FILE			1

static inline void *zalloc(size_t size)
{
	return calloc(1, size);
}

int ext4fs_read_inode(struct ext2_data *data, int ino,
		      struct ext2_inode *inode);
int ext4fs_read_file(struct ext2fs_node *node, int pos,
		unsigned int len, char *buf);
int ext4fs_find_file(const char *path, struct ext2fs_node *rootnode,
			struct ext2fs_node **foundnode, int expecttype);
typedef void (*dir_iterate_func_t)(void *, const char * name, struct xstat *, int is_directory);
int ext4fs_iterate_dir(struct ext2fs_node *dir, char *name,
			struct ext2fs_node **fnode, int *ftype, dir_iterate_func_t, void *);

#if defined(CONFIG_EXT4_WRITE)
uint32_t ext4fs_div_roundup(uint32_t size, uint32_t n);
int ext4fs_checksum_update(unsigned int i);
int ext4fs_get_parent_inode_num(const char *dirname, char *dname, int flags);
void ext4fs_update_parent_dentry(char *filename, int *p_ino, int file_type);
long int ext4fs_get_new_blk_no(void);
int ext4fs_get_new_inode_no(void);
void ext4fs_reset_block_bmap(long int blockno, unsigned char *buffer,
					int index);
int ext4fs_set_block_bmap(long int blockno, unsigned char *buffer, int index);
int ext4fs_set_inode_bmap(int inode_no, unsigned char *buffer, int index);
void ext4fs_reset_inode_bmap(int inode_no, unsigned char *buffer, int index);
int ext4fs_iget(int inode_no, struct ext2_inode *inode);
void ext4fs_allocate_blocks(struct ext2_inode *file_inode,
				unsigned int total_remaining_blocks,
				unsigned int *total_no_of_block);
void put_ext4(uint64_t off, void *buf, uint32_t size);
#endif
#endif
