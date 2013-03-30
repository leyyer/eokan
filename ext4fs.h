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

#ifndef __EXT4__
#define __EXT4__
#include "ext.h"

#define EXT4_EXTENTS_FL		0x00080000 /* Inode uses extents */
#define EXT4_EXT_MAGIC			0xf30a
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM	0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS	0x0040
#define EXT4_INDIRECT_BLOCKS		12

#define EXT4_BG_INODE_UNINIT		0x0001
#define EXT4_BG_BLOCK_UNINIT		0x0002
#define EXT4_BG_INODE_ZEROED		0x0004

/*
 * ext4_inode has i_block array (60 bytes total).
 * The first 12 bytes store ext4_extent_header;
 * the remainder stores an array of ext4_extent.
 */

/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct ext4_extent {
	uint32_t	ee_block;	/* first logical block extent covers */
	uint16_t	ee_len;		/* number of blocks covered by extent */
	uint16_t	ee_start_hi;	/* high 16 bits of physical block */
	uint32_t	ee_start_lo;	/* low 32 bits of physical block */
};

/*
 * This is index on-disk structure.
 * It's used at all the levels except the bottom.
 */
struct ext4_extent_idx {
	uint32_t	ei_block;	/* index covers logical blocks from 'block' */
	uint32_t	ei_leaf_lo;	/* pointer to the physical block of the next *
				 * level. leaf or next index could be there */
	uint16_t	ei_leaf_hi;	/* high 16 bits of physical block */
	uint16_t 	ei_unused;
};

/* Each block (leaves and indexes), even inode-stored has header. */
struct ext4_extent_header {
	uint16_t	eh_magic;	/* probably will support different formats */
	uint16_t	eh_entries;	/* number of valid entries */
	uint16_t	eh_max;		/* capacity of store in entries */
	uint16_t	eh_depth;	/* has tree real underlying blocks? */
	uint32_t	eh_generation;	/* generation of the tree */
};

struct part_descr;
struct ext_filesystem {
	/* Total Sector of partition */
	uint64_t total_sect;
	/* Block size  of partition */
	uint32_t blksz;
	/* Inode size of partition */
	uint32_t inodesz;
	/* Sectors per Block */
	uint32_t sect_perblk;
	/* Group Descriptor Block Number */
	uint32_t gdtable_blkno;
	/* Total block groups of partition */
	uint32_t no_blkgrp;
	/* No of blocks required for bgdtable */
	uint32_t no_blk_pergdt;
	/* Superblock */
	struct ext2_sblock *sb;
	/* Block group descritpor table */
	struct ext2_block_group *bgd;
	char *gdtable;

	/* Block Bitmap Related */
	unsigned char **blk_bmaps;
	long int curr_blkno;
	uint16_t first_pass_bbmap;

	/* Inode Bitmap Related */
	unsigned char **inode_bmaps;
	int curr_inode_no;
	uint16_t first_pass_ibmap;

	/* Journal Related */

	/* Partition Device Descriptor */
	struct part_descr *dev_desc;
	/* fs root */
	struct ext2_data *ext4fs_root;
};

extern struct ext2_data *ext4fs_root;
extern struct ext2fs_node *ext4fs_file;

#if defined(CONFIG_EXT4_WRITE)
extern struct ext2_inode *g_parent_inode;
extern int gd_index;
extern int gindex;

int ext4fs_init(void);
void ext4fs_deinit(void);
int ext4fs_filename_check(char *filename);
int ext4fs_write(const char *fname, unsigned char *buffer,
				unsigned long sizebytes);
#endif

struct part_descr;
struct ext_filesystem *get_fs(void);
int ext4fs_open(const char *filename);
int ext4fs_read(char *buf, unsigned len);
int ext4fs_mount(struct part_descr *);
void ext4fs_close(void);
int ext4fs_ls(const char *dirname);
void ext4fs_free_node(struct ext2fs_node *node, struct ext2fs_node *currroot);
long int read_allocated_block(struct ext2_inode *inode, int fileblock);
#endif
