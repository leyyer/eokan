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
#include <string.h>
#include <stdlib.h>
#include "ext.h"
#include "ext4fs.h"
#include "ext4.h"
#include "disk.h"

#if defined(CONFIG_EXT4_WRITE)
struct ext2_inode *g_parent_inode;

uint32_t ext4fs_div_roundup(uint32_t size, uint32_t n)
{
	uint32_t res = size / n;
	if (res * n != size)
		res++;

	return res;
}

void put_ext4(uint64_t off, void *buf, uint32_t size)
{
	uint64_t startblock;
	uint64_t remainder;
	unsigned char *temp_ptr = NULL;
	unsigned char sec_buf[SECTOR_SIZE];
	struct ext_filesystem *fs = get_fs();

	startblock = off / (uint64_t)SECTOR_SIZE;
	startblock += fs->dev_desc->off;
	remainder = off % (uint64_t)SECTOR_SIZE;
	remainder &= SECTOR_SIZE - 1;

	if (fs->dev_desc == NULL)
		return;

	if ((startblock + (size / SECTOR_SIZE)) >
	    (fs->dev_desc->off + fs->total_sect)) {
		printf("part_offset is %lu\n", fs->dev_desc->off);
		printf("total_sector is %llu\n", fs->total_sect);
		printf("error: overflow occurs\n");
		return;
	}

	if (remainder) {
		part_read(fs->dev_desc, startblock, 1, sec_buf);
		temp_ptr = sec_buf;
		memcpy((temp_ptr + remainder), (unsigned char *)buf, size);
		part_write(fs->dev_desc, startblock, 1, sec_buf);
	} else {
		if (size / SECTOR_SIZE != 0) {
			part_write(fs->dev_desc, startblock, size / SECTOR_SIZE,
				   	(unsigned long *)buf);
		} else {
			part_read(fs->dev_desc, startblock, 1, sec_buf);
			temp_ptr = sec_buf;
			memcpy(temp_ptr, buf, size);
			part_write(fs->dev_desc, startblock, 1, (unsigned long *)sec_buf);
		}
	}
}

static int _get_new_inode_no(unsigned char *buffer)
{
	struct ext_filesystem *fs = get_fs();
	unsigned char input;
	int operand, status;
	int count = 1;
	int j = 0;

	/* get the blocksize of the filesystem */
	unsigned char *ptr = buffer;
	while (*ptr == 255) {
		ptr++;
		count += 8;
		if (count > ext4fs_root->sblock.inodes_per_group)
			return -1;
	}

	for (j = 0; j < fs->blksz; j++) {
		input = *ptr;
		int i = 0;
		while (i <= 7) {
			operand = 1 << i;
			status = input & operand;
			if (status) {
				i++;
				count++;
			} else {
				*ptr |= operand;
				return count;
			}
		}
		ptr = ptr + 1;
	}

	return -1;
}

static int _get_new_blk_no(unsigned char *buffer)
{
	unsigned char input;
	int operand, status;
	int count = 0;
	int j = 0;
	unsigned char *ptr = buffer;
	struct ext_filesystem *fs = get_fs();

	if (fs->blksz != 1024)
		count = 0;
	else
		count = 1;

	while (*ptr == 255) {
		ptr++;
		count += 8;
		if (count == (fs->blksz * 8))
			return -1;
	}

	for (j = 0; j < fs->blksz; j++) {
		input = *ptr;
		int i = 0;
		while (i <= 7) {
			operand = 1 << i;
			status = input & operand;
			if (status) {
				i++;
				count++;
			} else {
				*ptr |= operand;
				return count;
			}
		}
		ptr = ptr + 1;
	}

	return -1;
}

int ext4fs_set_block_bmap(long int blockno, unsigned char *buffer, int index)
{
	int i, remainder, status;
	unsigned char *ptr = buffer;
	unsigned char operand;
	struct ext_filesystem *fs = get_fs();
	i = blockno / 8;
	remainder = blockno % 8;
	int blocksize = EXT2_BLOCK_SIZE(fs->ext4fs_root);

	i = i - (index * blocksize);
	if (blocksize != 1024) {
		ptr = ptr + i;
		operand = 1 << remainder;
		status = *ptr & operand;
		if (status)
			return -1;

		*ptr = *ptr | operand;
		return 0;
	} else {
		if (remainder == 0) {
			ptr = ptr + i - 1;
			operand = (1 << 7);
		} else {
			ptr = ptr + i;
			operand = (1 << (remainder - 1));
		}
		status = *ptr & operand;
		if (status)
			return -1;

		*ptr = *ptr | operand;
		return 0;
	}
}

void ext4fs_reset_block_bmap(long int blockno, unsigned char *buffer, int index)
{
	int i, remainder, status;
	unsigned char *ptr = buffer;
	unsigned char operand;
	struct ext_filesystem *fs = get_fs();
	i = blockno / 8;
	remainder = blockno % 8;
	int blocksize = EXT2_BLOCK_SIZE(fs->ext4fs_root);

	i = i - (index * blocksize);
	if (blocksize != 1024) {
		ptr = ptr + i;
		operand = (1 << remainder);
		status = *ptr & operand;
		if (status)
			*ptr = *ptr & ~(operand);
	} else {
		if (remainder == 0) {
			ptr = ptr + i - 1;
			operand = (1 << 7);
		} else {
			ptr = ptr + i;
			operand = (1 << (remainder - 1));
		}
		status = *ptr & operand;
		if (status)
			*ptr = *ptr & ~(operand);
	}
}

int ext4fs_set_inode_bmap(int inode_no, unsigned char *buffer, int index)
{
	int i, remainder, status;
	unsigned char *ptr = buffer;
	unsigned char operand;
	struct ext_filesystem *fs = get_fs();

	inode_no -= (index * fs->ext4fs_root->sblock.inodes_per_group);
	i = inode_no / 8;
	remainder = inode_no % 8;
	if (remainder == 0) {
		ptr = ptr + i - 1;
		operand = (1 << 7);
	} else {
		ptr = ptr + i;
		operand = (1 << (remainder - 1));
	}
	status = *ptr & operand;
	if (status)
		return -1;

	*ptr = *ptr | operand;

	return 0;
}

void ext4fs_reset_inode_bmap(int inode_no, unsigned char *buffer, int index)
{
	int i, remainder, status;
	unsigned char *ptr = buffer;
	unsigned char operand;
	struct ext_filesystem *fs = get_fs();

	inode_no -= (index * fs->ext4fs_root->sblock.inodes_per_group);
	i = inode_no / 8;
	remainder = inode_no % 8;
	if (remainder == 0) {
		ptr = ptr + i - 1;
		operand = (1 << 7);
	} else {
		ptr = ptr + i;
		operand = (1 << (remainder - 1));
	}
	status = *ptr & operand;
	if (status)
		*ptr = *ptr & ~(operand);
}

int ext4fs_checksum_update(unsigned int i)
{
	struct ext2_block_group *desc;
	struct ext_filesystem *fs = get_fs();
	__u16 crc = 0;

	desc = (struct ext2_block_group *)&fs->bgd[i];
	if (fs->sb->feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM) {
		int offset = offsetof(struct ext2_block_group, bg_checksum);

		crc = ext2fs_crc16(~0, fs->sb->unique_id,
				   sizeof(fs->sb->unique_id));
		crc = ext2fs_crc16(crc, &i, sizeof(i));
		crc = ext2fs_crc16(crc, desc, offset);
		offset += sizeof(desc->bg_checksum);	/* skip checksum */
		assert(offset == sizeof(*desc));
	}

	return crc;
}

static int check_void_in_dentry(struct ext2_dirent *dir, char *filename)
{
	int dentry_length;
	int sizeof_void_space;
	int new_entry_byte_reqd;
	short padding_factor = 0;

	if (dir->namelen % 4 != 0)
		padding_factor = 4 - (dir->namelen % 4);

	dentry_length = sizeof(struct ext2_dirent) +
			dir->namelen + padding_factor;
	sizeof_void_space = dir->direntlen - dentry_length;
	if (sizeof_void_space == 0)
		return 0;

	padding_factor = 0;
	if (strlen(filename) % 4 != 0)
		padding_factor = 4 - (strlen(filename) % 4);

	new_entry_byte_reqd = strlen(filename) +
	    sizeof(struct ext2_dirent) + padding_factor;
	if (sizeof_void_space >= new_entry_byte_reqd) {
		dir->direntlen = dentry_length;
		return sizeof_void_space;
	}

	return 0;
}

void ext4fs_update_parent_dentry(char *filename, int *p_ino, int file_type)
{
	unsigned int *zero_buffer = NULL;
	char *root_first_block_buffer = NULL;
	int direct_blk_idx;
	long int root_blknr;
	long int first_block_no_of_root = 0;
	long int previous_blknr = -1;
	int totalbytes = 0;
	short int padding_factor = 0;
	unsigned int new_entry_byte_reqd;
	unsigned int last_entry_dirlen;
	int sizeof_void_space = 0;
	int templength = 0;
	int inodeno;
	int status;
	struct ext_filesystem *fs = get_fs();
	/* directory entry */
	struct ext2_dirent *dir;
	char *temp_dir = NULL;

	zero_buffer = zalloc(fs->blksz);
	if (!zero_buffer) {
		printf("No Memory\n");
		return;
	}
	root_first_block_buffer = zalloc(fs->blksz);
	if (!root_first_block_buffer) {
		free(zero_buffer);
		printf("No Memory\n");
		return;
	}
restart:

	/* read the block no allocated to a file */
	for (direct_blk_idx = 0; direct_blk_idx < INDIRECT_BLOCKS;
	     direct_blk_idx++) {
		root_blknr = read_allocated_block(g_parent_inode,
						  direct_blk_idx);
		if (root_blknr == 0) {
			first_block_no_of_root = previous_blknr;
			break;
		}
		previous_blknr = root_blknr;
	}

	status = vfs_devread(fs->dev_desc, first_block_no_of_root
				* fs->sect_perblk,
				0, fs->blksz, root_first_block_buffer);
	if (status == 0)
		goto fail;

	if (ext4fs_log_journal(root_first_block_buffer, first_block_no_of_root))
		goto fail;
	dir = (struct ext2_dirent *)root_first_block_buffer;
	totalbytes = 0;
	while (dir->direntlen > 0) {
		/*
		 * blocksize-totalbytes because last directory length
		 * i.e. dir->direntlen is free availble space in the
		 * block that means  it is a last entry of directory
		 * entry
		 */

		/* traversing the each directory entry */
		if (fs->blksz - totalbytes == dir->direntlen) {
			if (strlen(filename) % 4 != 0)
				padding_factor = 4 - (strlen(filename) % 4);

			new_entry_byte_reqd = strlen(filename) +
			    sizeof(struct ext2_dirent) + padding_factor;
			padding_factor = 0;
			/*
			 * update last directory entry length to its
			 * length because we are creating new directory
			 * entry
			 */
			if (dir->namelen % 4 != 0)
				padding_factor = 4 - (dir->namelen % 4);

			last_entry_dirlen = dir->namelen +
			    sizeof(struct ext2_dirent) + padding_factor;
			if ((fs->blksz - totalbytes - last_entry_dirlen) <
				new_entry_byte_reqd) {
				printf("1st Block Full:Allocate new block\n");

				if (direct_blk_idx == INDIRECT_BLOCKS - 1) {
					printf("Directory exceeds limit\n");
					goto fail;
				}
				g_parent_inode->b.blocks.dir_blocks
				    [direct_blk_idx] = ext4fs_get_new_blk_no();
				if (g_parent_inode->b.blocks.dir_blocks
					[direct_blk_idx] == -1) {
					printf("no block left to assign\n");
					goto fail;
				}
				put_ext4(((uint64_t)
					  (g_parent_inode->b.
					   blocks.dir_blocks[direct_blk_idx] *
					   fs->blksz)), zero_buffer, fs->blksz);
				g_parent_inode->size =
				    g_parent_inode->size + fs->blksz;
				g_parent_inode->blockcnt =
				    g_parent_inode->blockcnt + fs->sect_perblk;
				if (ext4fs_put_metadata
				    (root_first_block_buffer,
				     first_block_no_of_root))
					goto fail;
				goto restart;
			}
			dir->direntlen = last_entry_dirlen;
			break;
		}

		templength = dir->direntlen;
		totalbytes = totalbytes + templength;
		sizeof_void_space = check_void_in_dentry(dir, filename);
		if (sizeof_void_space)
			break;

		dir = (struct ext2_dirent *)((char *)dir + templength);
	}

	/* make a pointer ready for creating next directory entry */
	templength = dir->direntlen;
	totalbytes = totalbytes + templength;
	dir = (struct ext2_dirent *)((char *)dir + templength);

	/* get the next available inode number */
	inodeno = ext4fs_get_new_inode_no();
	if (inodeno == -1) {
		printf("no inode left to assign\n");
		goto fail;
	}
	dir->inode = inodeno;
	if (sizeof_void_space)
		dir->direntlen = sizeof_void_space;
	else
		dir->direntlen = fs->blksz - totalbytes;

	dir->namelen = strlen(filename);
	dir->filetype = FILETYPE_REG;	/* regular file */
	temp_dir = (char *)dir;
	temp_dir = temp_dir + sizeof(struct ext2_dirent);
	memcpy(temp_dir, filename, strlen(filename));

	*p_ino = inodeno;

	/* update or write  the 1st block of root inode */
	if (ext4fs_put_metadata(root_first_block_buffer,
				first_block_no_of_root))
		goto fail;

fail:
	free(zero_buffer);
	free(root_first_block_buffer);
}

static int search_dir(struct ext2_inode *parent_inode, char *dirname)
{
	int status;
	int inodeno;
	int totalbytes;
	int templength;
	int direct_blk_idx;
	long int blknr;
	int found = 0;
	char *ptr = NULL;
	unsigned char *block_buffer = NULL;
	struct ext2_dirent *dir = NULL;
	struct ext2_dirent *previous_dir = NULL;
	struct ext_filesystem *fs = get_fs();

	/* read the block no allocated to a file */
	for (direct_blk_idx = 0; direct_blk_idx < INDIRECT_BLOCKS;
		direct_blk_idx++) {
		blknr = read_allocated_block(parent_inode, direct_blk_idx);
		if (blknr == 0)
			goto fail;

		/* read the blocks of parenet inode */
		block_buffer = zalloc(fs->blksz);
		if (!block_buffer)
			goto fail;

		status = vfs_devread(fs->dev_desc, blknr * fs->sect_perblk,
					0, fs->blksz, (char *)block_buffer);
		if (status == 0)
			goto fail;

		dir = (struct ext2_dirent *)block_buffer;
		ptr = (char *)dir;
		totalbytes = 0;
		while (dir->direntlen >= 0) {
			/*
			 * blocksize-totalbytes because last directory
			 * length i.e.,*dir->direntlen is free availble
			 * space in the block that means
			 * it is a last entry of directory entry
			 */
			if (strlen(dirname) == dir->namelen) {
				if (strncmp(dirname, ptr +
					sizeof(struct ext2_dirent),
					dir->namelen) == 0) {
					previous_dir->direntlen +=
							dir->direntlen;
					inodeno = dir->inode;
					dir->inode = 0;
					found = 1;
					break;
				}
			}

			if (fs->blksz - totalbytes == dir->direntlen)
				break;

			/* traversing the each directory entry */
			templength = dir->direntlen;
			totalbytes = totalbytes + templength;
			previous_dir = dir;
			dir = (struct ext2_dirent *)((char *)dir + templength);
			ptr = (char *)dir;
		}

		if (found == 1) {
			free(block_buffer);
			block_buffer = NULL;
			return inodeno;
		}

		free(block_buffer);
		block_buffer = NULL;
	}

fail:
	free(block_buffer);

	return -1;
}

static int find_dir_depth(char *dirname)
{
	char *token = strtok(dirname, "/");
	int count = 0;
	while (token != NULL) {
		token = strtok(NULL, "/");
		count++;
	}
	return count + 1 + 1;
	/*
	 * for example  for string /home/temp
	 * depth=home(1)+temp(1)+1 extra for NULL;
	 * so count is 4;
	 */
}

static int parse_path(char **arr, char *dirname)
{
	char *token = strtok(dirname, "/");
	int i = 0;

	/* add root */
	arr[i] = zalloc(strlen("/") + 1);
	if (!arr[i])
		return -ENOMEM;

	arr[i++] = "/";

	/* add each path entry after root */
	while (token != NULL) {
		arr[i] = zalloc(strlen(token) + 1);
		if (!arr[i])
			return -ENOMEM;
		memcpy(arr[i++], token, strlen(token));
		token = strtok(NULL, "/");
	}
	arr[i] = NULL;

	return 0;
}

int ext4fs_iget(int inode_no, struct ext2_inode *inode)
{
	struct ext_filesystem * fs = get_fs();
	if (ext4fs_read_inode(fs, fs->ext4fs_root, inode_no, inode) == 0)
		return -1;

	return 0;
}

/*
 * Function: ext4fs_get_parent_inode_num
 * Return Value: inode Number of the parent directory of  file/Directory to be
 * created
 * dirname : Input parmater, input path name of the file/directory to be created
 * dname : Output parameter, to be filled with the name of the directory
 * extracted from dirname
 */
int ext4fs_get_parent_inode_num(const char *dirname, char *dname, int flags)
{
	int i;
	int depth = 0;
	int matched_inode_no;
	int result_inode_no = -1;
	char **ptr = NULL;
	char *depth_dirname = NULL;
	char *parse_dirname = NULL;
	struct ext2_inode *parent_inode = NULL;
	struct ext2_inode *first_inode = NULL;
	struct ext2_inode temp_inode;
	struct ext_filesystem *fs = get_fs();

	if (*dirname != '/') {
		printf("Please supply Absolute path\n");
		return -1;
	}

	/* TODO: input validation make equivalent to linux */
	depth_dirname = zalloc(strlen(dirname) + 1);
	if (!depth_dirname)
		return -ENOMEM;

	memcpy(depth_dirname, dirname, strlen(dirname));
	depth = find_dir_depth(depth_dirname);
	parse_dirname = zalloc(strlen(dirname) + 1);
	if (!parse_dirname)
		goto fail;
	memcpy(parse_dirname, dirname, strlen(dirname));

	/* allocate memory for each directory level */
	ptr = zalloc((depth) * sizeof(char *));
	if (!ptr)
		goto fail;
	if (parse_path(ptr, parse_dirname))
		goto fail;
	parent_inode = zalloc(sizeof(struct ext2_inode));
	if (!parent_inode)
		goto fail;
	first_inode = zalloc(sizeof(struct ext2_inode));
	if (!first_inode)
		goto fail;
	memcpy(parent_inode, fs->ext4fs_root->inode, sizeof(struct ext2_inode));
	memcpy(first_inode, parent_inode, sizeof(struct ext2_inode));
	if (flags & F_FILE)
		result_inode_no = EXT2_ROOT_INO;
	for (i = 1; i < depth; i++) {
		matched_inode_no = search_dir(parent_inode, ptr[i]);
		if (matched_inode_no == -1) {
			if (ptr[i + 1] == NULL && i == 1) {
				result_inode_no = EXT2_ROOT_INO;
				goto end;
			} else {
				if (ptr[i + 1] == NULL)
					break;
				printf("Invalid path\n");
				result_inode_no = -1;
				goto fail;
			}
		} else {
			if (ptr[i + 1] != NULL) {
				memset(parent_inode, '\0',
				       sizeof(struct ext2_inode));
				if (ext4fs_iget(matched_inode_no,
						parent_inode)) {
					result_inode_no = -1;
					goto fail;
				}
				result_inode_no = matched_inode_no;
			} else {
				break;
			}
		}
	}

end:
	if (i == 1)
		matched_inode_no = search_dir(first_inode, ptr[i]);
	else
		matched_inode_no = search_dir(parent_inode, ptr[i]);

	if (matched_inode_no != -1) {
		ext4fs_iget(matched_inode_no, &temp_inode);
		if (temp_inode.mode & S_IFDIR) {
			printf("It is a Directory\n");
			result_inode_no = -1;
			goto fail;
		}
	}

	if (strlen(ptr[i]) > 256) {
		result_inode_no = -1;
		goto fail;
	}
	memcpy(dname, ptr[i], strlen(ptr[i]));

fail:
	free(depth_dirname);
	free(parse_dirname);
	free(ptr);
	free(parent_inode);
	free(first_inode);

	return result_inode_no;
}

static int check_filename(char *filename, unsigned int blknr)
{
	unsigned int first_block_no_of_root;
	int totalbytes = 0;
	int templength = 0;
	int status, inodeno;
	int found = 0;
	char *root_first_block_buffer = NULL;
	char *root_first_block_addr = NULL;
	struct ext2_dirent *dir = NULL;
	struct ext2_dirent *previous_dir = NULL;
	char *ptr = NULL;
	struct ext_filesystem *fs = get_fs();

	/* get the first block of root */
	first_block_no_of_root = blknr;
	root_first_block_buffer = zalloc(fs->blksz);
	if (!root_first_block_buffer)
		return -ENOMEM;
	root_first_block_addr = root_first_block_buffer;
	status = vfs_devread(fs->dev_desc, first_block_no_of_root *
				fs->sect_perblk, 0,
				fs->blksz, root_first_block_buffer);
	if (status == 0)
		goto fail;

	if (ext4fs_log_journal(root_first_block_buffer, first_block_no_of_root))
		goto fail;
	dir = (struct ext2_dirent *)root_first_block_buffer;
	ptr = (char *)dir;
	totalbytes = 0;
	while (dir->direntlen >= 0) {
		/*
		 * blocksize-totalbytes because last
		 * directory length i.e., *dir->direntlen
		 * is free availble space in the block that
		 * means it is a last entry of directory entry
		 */
		if (strlen(filename) == dir->namelen) {
			if (strncmp(filename, ptr + sizeof(struct ext2_dirent),
				dir->namelen) == 0) {
				printf("file found deleting\n");
				previous_dir->direntlen += dir->direntlen;
				inodeno = dir->inode;
				dir->inode = 0;
				found = 1;
				break;
			}
		}

		if (fs->blksz - totalbytes == dir->direntlen)
			break;

		/* traversing the each directory entry */
		templength = dir->direntlen;
		totalbytes = totalbytes + templength;
		previous_dir = dir;
		dir = (struct ext2_dirent *)((char *)dir + templength);
		ptr = (char *)dir;
	}


	if (found == 1) {
		if (ext4fs_put_metadata(root_first_block_addr,
					first_block_no_of_root))
			goto fail;
		return inodeno;
	}
fail:
	free(root_first_block_buffer);

	return -1;
}

int ext4fs_filename_check(char *filename)
{
	short direct_blk_idx = 0;
	long int blknr = -1;
	int inodeno = -1;

	/* read the block no allocated to a file */
	for (direct_blk_idx = 0; direct_blk_idx < INDIRECT_BLOCKS;
		direct_blk_idx++) {
		blknr = read_allocated_block(g_parent_inode, direct_blk_idx);
		if (blknr == 0)
			break;
		inodeno = check_filename(filename, blknr);
		if (inodeno != -1)
			return inodeno;
	}

	return -1;
}

long int ext4fs_get_new_blk_no(void)
{
	short i;
	short status;
	int remainder;
	unsigned int bg_idx;
	static int prev_bg_bitmap_index = -1;
	struct ext_filesystem *fs = get_fs();
	unsigned int blk_per_grp = fs->ext4fs_root->sblock.blocks_per_group;
	char *journal_buffer = zalloc(fs->blksz);
	char *zero_buffer = zalloc(fs->blksz);
	if (!journal_buffer || !zero_buffer)
		goto fail;
	struct ext2_block_group *bgd = (struct ext2_block_group *)fs->gdtable;

	if (fs->first_pass_bbmap == 0) {
		for (i = 0; i < fs->no_blkgrp; i++) {
			if (bgd[i].free_blocks) {
				if (bgd[i].bg_flags & EXT4_BG_BLOCK_UNINIT) {
					put_ext4(((uint64_t) (bgd[i].block_id *
							      fs->blksz)),
						 zero_buffer, fs->blksz);
					bgd[i].bg_flags =
					    bgd[i].
					    bg_flags & ~EXT4_BG_BLOCK_UNINIT;
					memcpy(fs->blk_bmaps[i], zero_buffer,
					       fs->blksz);
				}
				fs->curr_blkno =
				    _get_new_blk_no(fs->blk_bmaps[i]);
				if (fs->curr_blkno == -1)
					/* if block bitmap is completely fill */
					continue;
				fs->curr_blkno = fs->curr_blkno +
						(i * fs->blksz * 8);
				fs->first_pass_bbmap++;
				bgd[i].free_blocks--;
				fs->sb->free_blocks--;
				status = vfs_devread(fs->dev_desc, bgd[i].block_id *
							fs->sect_perblk, 0,
							fs->blksz,
							journal_buffer);
				if (status == 0)
					goto fail;
				if (ext4fs_log_journal(journal_buffer,
							bgd[i].block_id))
					goto fail;
				goto success;
			} else {
				printf("no space left on block group %d\n", i);
			}
		}

		goto fail;
	} else {
restart:
		fs->curr_blkno++;
		/* get the blockbitmap index respective to blockno */
		if (fs->blksz != 1024) {
			bg_idx = fs->curr_blkno / blk_per_grp;
		} else {
			bg_idx = fs->curr_blkno / blk_per_grp;
			remainder = fs->curr_blkno % blk_per_grp;
			if (!remainder)
				bg_idx--;
		}

		/*
		 * To skip completely filled block group bitmaps
		 * Optimize the block allocation
		 */
		if (bg_idx >= fs->no_blkgrp)
			goto fail;

		if (bgd[bg_idx].free_blocks == 0) {
			printf("block group %u is full. Skipping\n", bg_idx);
			fs->curr_blkno = fs->curr_blkno + blk_per_grp;
			fs->curr_blkno--;
			goto restart;
		}

		if (bgd[bg_idx].bg_flags & EXT4_BG_BLOCK_UNINIT) {
			memset(zero_buffer, '\0', fs->blksz);
			put_ext4(((uint64_t) (bgd[bg_idx].block_id *
					fs->blksz)), zero_buffer, fs->blksz);
			memcpy(fs->blk_bmaps[bg_idx], zero_buffer, fs->blksz);
			bgd[bg_idx].bg_flags = bgd[bg_idx].bg_flags &
						~EXT4_BG_BLOCK_UNINIT;
		}

		if (ext4fs_set_block_bmap(fs->curr_blkno, fs->blk_bmaps[bg_idx],
				   bg_idx) != 0) {
			printf("going for restart for the block no %ld %u\n",
			      fs->curr_blkno, bg_idx);
			goto restart;
		}

		/* journal backup */
		if (prev_bg_bitmap_index != bg_idx) {
			memset(journal_buffer, '\0', fs->blksz);
			status = vfs_devread(fs->dev_desc, bgd[bg_idx].block_id
						* fs->sect_perblk,
						0, fs->blksz, journal_buffer);
			if (status == 0)
				goto fail;
			if (ext4fs_log_journal(journal_buffer,
						bgd[bg_idx].block_id))
				goto fail;

			prev_bg_bitmap_index = bg_idx;
		}
		bgd[bg_idx].free_blocks--;
		fs->sb->free_blocks--;
		goto success;
	}
success:
	free(journal_buffer);
	free(zero_buffer);

	return fs->curr_blkno;
fail:
	free(journal_buffer);
	free(zero_buffer);

	return -1;
}

int ext4fs_get_new_inode_no(void)
{
	short i;
	short status;
	unsigned int ibmap_idx;
	static int prev_inode_bitmap_index = -1;
	struct ext_filesystem *fs = get_fs();
	unsigned int inodes_per_grp = fs->ext4fs_root->sblock.inodes_per_group;
	char *journal_buffer = zalloc(fs->blksz);
	char *zero_buffer = zalloc(fs->blksz);
	if (!journal_buffer || !zero_buffer)
		goto fail;
	struct ext2_block_group *bgd = (struct ext2_block_group *)fs->gdtable;

	if (fs->first_pass_ibmap == 0) {
		for (i = 0; i < fs->no_blkgrp; i++) {
			if (bgd[i].free_inodes) {
				if (bgd[i].bg_itable_unused !=
						bgd[i].free_inodes)
					bgd[i].bg_itable_unused =
						bgd[i].free_inodes;
				if (bgd[i].bg_flags & EXT4_BG_INODE_UNINIT) {
					put_ext4(((uint64_t)
						  (bgd[i].inode_id *
							fs->blksz)),
						 zero_buffer, fs->blksz);
					bgd[i].bg_flags = bgd[i].bg_flags &
							~EXT4_BG_INODE_UNINIT;
					memcpy(fs->inode_bmaps[i],
					       zero_buffer, fs->blksz);
				}
				fs->curr_inode_no =
				    _get_new_inode_no(fs->inode_bmaps[i]);
				if (fs->curr_inode_no == -1)
					/* if block bitmap is completely fill */
					continue;
				fs->curr_inode_no = fs->curr_inode_no +
							(i * inodes_per_grp);
				fs->first_pass_ibmap++;
				bgd[i].free_inodes--;
				bgd[i].bg_itable_unused--;
				fs->sb->free_inodes--;
				status = vfs_devread(fs->dev_desc, bgd[i].inode_id *
							fs->sect_perblk, 0,
							fs->blksz,
							journal_buffer);
				if (status == 0)
					goto fail;
				if (ext4fs_log_journal(journal_buffer,
							bgd[i].inode_id))
					goto fail;
				goto success;
			} else
				printf("no inode left on block group %d\n", i);
		}
		goto fail;
	} else {
restart:
		fs->curr_inode_no++;
		/* get the blockbitmap index respective to blockno */
		ibmap_idx = fs->curr_inode_no / inodes_per_grp;
		if (bgd[ibmap_idx].bg_flags & EXT4_BG_INODE_UNINIT) {
			memset(zero_buffer, '\0', fs->blksz);
			put_ext4(((uint64_t) (bgd[ibmap_idx].inode_id *
					      fs->blksz)), zero_buffer,
				 fs->blksz);
			bgd[ibmap_idx].bg_flags =
			    bgd[ibmap_idx].bg_flags & ~EXT4_BG_INODE_UNINIT;
			memcpy(fs->inode_bmaps[ibmap_idx], zero_buffer,
				fs->blksz);
		}

		if (ext4fs_set_inode_bmap(fs->curr_inode_no,
					  fs->inode_bmaps[ibmap_idx],
					  ibmap_idx) != 0) {
			printf("going for restart for the block no %d %u\n",
			      fs->curr_inode_no, ibmap_idx);
			goto restart;
		}

		/* journal backup */
		if (prev_inode_bitmap_index != ibmap_idx) {
			memset(journal_buffer, '\0', fs->blksz);
			status = vfs_devread(fs->dev_desc, bgd[ibmap_idx].inode_id
						* fs->sect_perblk,
						0, fs->blksz, journal_buffer);
			if (status == 0)
				goto fail;
			if (ext4fs_log_journal(journal_buffer,
						bgd[ibmap_idx].inode_id))
				goto fail;
			prev_inode_bitmap_index = ibmap_idx;
		}
		if (bgd[ibmap_idx].bg_itable_unused !=
				bgd[ibmap_idx].free_inodes)
			bgd[ibmap_idx].bg_itable_unused =
					bgd[ibmap_idx].free_inodes;
		bgd[ibmap_idx].free_inodes--;
		bgd[ibmap_idx].bg_itable_unused--;
		fs->sb->free_inodes--;
		goto success;
	}

success:
	free(journal_buffer);
	free(zero_buffer);

	return fs->curr_inode_no;
fail:
	free(journal_buffer);
	free(zero_buffer);

	return -1;

}


static void alloc_single_indirect_block(struct ext2_inode *file_inode,
					unsigned int *total_remaining_blocks,
					unsigned int *no_blks_reqd)
{
	short i;
	short status;
	long int actual_block_no;
	long int si_blockno;
	/* si :single indirect */
	unsigned int *si_buffer = NULL;
	unsigned int *si_start_addr = NULL;
	struct ext_filesystem *fs = get_fs();

	if (*total_remaining_blocks != 0) {
		si_buffer = zalloc(fs->blksz);
		if (!si_buffer) {
			printf("No Memory\n");
			return;
		}
		si_start_addr = si_buffer;
		si_blockno = ext4fs_get_new_blk_no();
		if (si_blockno == -1) {
			printf("no block left to assign\n");
			goto fail;
		}
		(*no_blks_reqd)++;
		printf("SIPB %ld: %u\n", si_blockno, *total_remaining_blocks);

		status = vfs_devread(fs->dev_desc, si_blockno * fs->sect_perblk,
					0, fs->blksz, (char *)si_buffer);
		memset(si_buffer, '\0', fs->blksz);
		if (status == 0)
			goto fail;

		for (i = 0; i < (fs->blksz / sizeof(int)); i++) {
			actual_block_no = ext4fs_get_new_blk_no();
			if (actual_block_no == -1) {
				printf("no block left to assign\n");
				goto fail;
			}
			*si_buffer = actual_block_no;
			printf("SIAB %u: %u\n", *si_buffer,
				*total_remaining_blocks);

			si_buffer++;
			(*total_remaining_blocks)--;
			if (*total_remaining_blocks == 0)
				break;
		}

		/* write the block to disk */
		put_ext4(((uint64_t) (si_blockno * fs->blksz)),
			 si_start_addr, fs->blksz);
		file_inode->b.blocks.indir_block = si_blockno;
	}
fail:
	free(si_start_addr);
}

static void alloc_double_indirect_block(struct ext2_inode *file_inode,
					unsigned int *total_remaining_blocks,
					unsigned int *no_blks_reqd)
{
	short i;
	short j;
	short status;
	long int actual_block_no;
	/* di:double indirect */
	long int di_blockno_parent;
	long int di_blockno_child;
	unsigned int *di_parent_buffer = NULL;
	unsigned int *di_child_buff = NULL;
	unsigned int *di_block_start_addr = NULL;
	unsigned int *di_child_buff_start = NULL;
	struct ext_filesystem *fs = get_fs();

	if (*total_remaining_blocks != 0) {
		/* double indirect parent block connecting to inode */
		di_blockno_parent = ext4fs_get_new_blk_no();
		if (di_blockno_parent == -1) {
			printf("no block left to assign\n");
			goto fail;
		}
		di_parent_buffer = zalloc(fs->blksz);
		if (!di_parent_buffer)
			goto fail;

		di_block_start_addr = di_parent_buffer;
		(*no_blks_reqd)++;
		printf("DIPB %ld: %u\n", di_blockno_parent,
		      *total_remaining_blocks);

		status = vfs_devread(fs->dev_desc,di_blockno_parent *
					fs->sect_perblk, 0,
					fs->blksz, (char *)di_parent_buffer);

		if (!status) {
			printf("%s: Device read error!\n", __func__);
			goto fail;
		}
		memset(di_parent_buffer, '\0', fs->blksz);

		/*
		 * start:for each double indirect parent
		 * block create one more block
		 */
		for (i = 0; i < (fs->blksz / sizeof(int)); i++) {
			di_blockno_child = ext4fs_get_new_blk_no();
			if (di_blockno_child == -1) {
				printf("no block left to assign\n");
				goto fail;
			}
			di_child_buff = zalloc(fs->blksz);
			if (!di_child_buff)
				goto fail;

			di_child_buff_start = di_child_buff;
			*di_parent_buffer = di_blockno_child;
			di_parent_buffer++;
			(*no_blks_reqd)++;
			printf("DICB %ld: %u\n", di_blockno_child,
			      *total_remaining_blocks);

			status = vfs_devread(fs->dev_desc,di_blockno_child *
						fs->sect_perblk, 0,
						fs->blksz,
						(char *)di_child_buff);

			if (!status) {
				printf("%s: Device read error!\n", __func__);
				goto fail;
			}
			memset(di_child_buff, '\0', fs->blksz);
			/* filling of actual datablocks for each child */
			for (j = 0; j < (fs->blksz / sizeof(int)); j++) {
				actual_block_no = ext4fs_get_new_blk_no();
				if (actual_block_no == -1) {
					printf("no block left to assign\n");
					goto fail;
				}
				*di_child_buff = actual_block_no;
				printf("DIAB %ld: %u\n", actual_block_no,
				      *total_remaining_blocks);

				di_child_buff++;
				(*total_remaining_blocks)--;
				if (*total_remaining_blocks == 0)
					break;
			}
			/* write the block  table */
			put_ext4(((uint64_t) (di_blockno_child * fs->blksz)),
				 di_child_buff_start, fs->blksz);
			free(di_child_buff_start);
			di_child_buff_start = NULL;

			if (*total_remaining_blocks == 0)
				break;
		}
		put_ext4(((uint64_t) (di_blockno_parent * fs->blksz)),
			 di_block_start_addr, fs->blksz);
		file_inode->b.blocks.double_indir_block = di_blockno_parent;
	}
fail:
	free(di_block_start_addr);
}

static void alloc_triple_indirect_block(struct ext2_inode *file_inode,
					unsigned int *total_remaining_blocks,
					unsigned int *no_blks_reqd)
{
	short i;
	short j;
	short k;
	long int actual_block_no;
	/* ti: Triple Indirect */
	long int ti_gp_blockno;
	long int ti_parent_blockno;
	long int ti_child_blockno;
	unsigned int *ti_gp_buff = NULL;
	unsigned int *ti_parent_buff = NULL;
	unsigned int *ti_child_buff = NULL;
	unsigned int *ti_gp_buff_start_addr = NULL;
	unsigned int *ti_pbuff_start_addr = NULL;
	unsigned int *ti_cbuff_start_addr = NULL;
	struct ext_filesystem *fs = get_fs();
	if (*total_remaining_blocks != 0) {
		/* triple indirect grand parent block connecting to inode */
		ti_gp_blockno = ext4fs_get_new_blk_no();
		if (ti_gp_blockno == -1) {
			printf("no block left to assign\n");
			goto fail;
		}
		ti_gp_buff = zalloc(fs->blksz);
		if (!ti_gp_buff)
			goto fail;

		ti_gp_buff_start_addr = ti_gp_buff;
		(*no_blks_reqd)++;
		printf("TIGPB %ld: %u\n", ti_gp_blockno,
		      *total_remaining_blocks);

		/* for each 4 byte grand parent entry create one more block */
		for (i = 0; i < (fs->blksz / sizeof(int)); i++) {
			ti_parent_blockno = ext4fs_get_new_blk_no();
			if (ti_parent_blockno == -1) {
				printf("no block left to assign\n");
				goto fail;
			}
			ti_parent_buff = zalloc(fs->blksz);
			if (!ti_parent_buff)
				goto fail;

			ti_pbuff_start_addr = ti_parent_buff;
			*ti_gp_buff = ti_parent_blockno;
			ti_gp_buff++;
			(*no_blks_reqd)++;
			printf("TIPB %ld: %u\n", ti_parent_blockno,
			      *total_remaining_blocks);

			/* for each 4 byte entry parent create one more block */
			for (j = 0; j < (fs->blksz / sizeof(int)); j++) {
				ti_child_blockno = ext4fs_get_new_blk_no();
				if (ti_child_blockno == -1) {
					printf("no block left assign\n");
					goto fail;
				}
				ti_child_buff = zalloc(fs->blksz);
				if (!ti_child_buff)
					goto fail;

				ti_cbuff_start_addr = ti_child_buff;
				*ti_parent_buff = ti_child_blockno;
				ti_parent_buff++;
				(*no_blks_reqd)++;
				printf("TICB %ld: %u\n", ti_parent_blockno,
				      *total_remaining_blocks);

				/* fill actual datablocks for each child */
				for (k = 0; k < (fs->blksz / sizeof(int));
					k++) {
					actual_block_no =
					    ext4fs_get_new_blk_no();
					if (actual_block_no == -1) {
						printf("no block left\n");
						goto fail;
					}
					*ti_child_buff = actual_block_no;
					printf("TIAB %ld: %u\n", actual_block_no,
					      *total_remaining_blocks);

					ti_child_buff++;
					(*total_remaining_blocks)--;
					if (*total_remaining_blocks == 0)
						break;
				}
				/* write the child block */
				put_ext4(((uint64_t) (ti_child_blockno *
						      fs->blksz)),
					 ti_cbuff_start_addr, fs->blksz);
				free(ti_cbuff_start_addr);

				if (*total_remaining_blocks == 0)
					break;
			}
			/* write the parent block */
			put_ext4(((uint64_t) (ti_parent_blockno * fs->blksz)),
				 ti_pbuff_start_addr, fs->blksz);
			free(ti_pbuff_start_addr);

			if (*total_remaining_blocks == 0)
				break;
		}
		/* write the grand parent block */
		put_ext4(((uint64_t) (ti_gp_blockno * fs->blksz)),
			 ti_gp_buff_start_addr, fs->blksz);
		file_inode->b.blocks.triple_indir_block = ti_gp_blockno;
	}
fail:
	free(ti_gp_buff_start_addr);
}

void ext4fs_allocate_blocks(struct ext2_inode *file_inode,
				unsigned int total_remaining_blocks,
				unsigned int *total_no_of_block)
{
	short i;
	long int direct_blockno;
	unsigned int no_blks_reqd = 0;

	/* allocation of direct blocks */
	for (i = 0; i < INDIRECT_BLOCKS; i++) {
		direct_blockno = ext4fs_get_new_blk_no();
		if (direct_blockno == -1) {
			printf("no block left to assign\n");
			return;
		}
		file_inode->b.blocks.dir_blocks[i] = direct_blockno;
		printf("DB %ld: %u\n", direct_blockno, total_remaining_blocks);

		total_remaining_blocks--;
		if (total_remaining_blocks == 0)
			break;
	}

	alloc_single_indirect_block(file_inode, &total_remaining_blocks,
				    &no_blks_reqd);
	alloc_double_indirect_block(file_inode, &total_remaining_blocks,
				    &no_blks_reqd);
	alloc_triple_indirect_block(file_inode, &total_remaining_blocks,
				    &no_blks_reqd);
	*total_no_of_block += no_blks_reqd;
}

static void ext4fs_update(void)
{
	short i;
	ext4fs_update_journal();
	struct ext_filesystem *fs = get_fs();

	/* update  super block */
	put_ext4((uint64_t)(SUPERBLOCK_SIZE),
			(struct ext2_sblock *)fs->sb, (uint32_t)SUPERBLOCK_SIZE);

	/* update block groups */
	for (i = 0; i < fs->no_blkgrp; i++) {
		fs->bgd[i].bg_checksum = ext4fs_checksum_update(i);
		put_ext4((uint64_t)(fs->bgd[i].block_id * fs->blksz),
				fs->blk_bmaps[i], fs->blksz);
	}

	/* update inode table groups */
	for (i = 0; i < fs->no_blkgrp; i++) {
		put_ext4((uint64_t) (fs->bgd[i].inode_id * fs->blksz),
				fs->inode_bmaps[i], fs->blksz);
	}

	/* update the block group descriptor table */
	put_ext4((uint64_t)(fs->gdtable_blkno * fs->blksz),
			(struct ext2_block_group *)fs->gdtable,
			(fs->blksz * fs->no_blk_pergdt));

	ext4fs_dump_metadata();

	gindex = 0;
	gd_index = 0;
}

int ext4fs_get_bgdtable(void)
{
	int status;
	int grp_desc_size;
	struct ext_filesystem *fs = get_fs();
	grp_desc_size = sizeof(struct ext2_block_group);
	fs->no_blk_pergdt = (fs->no_blkgrp * grp_desc_size) / fs->blksz;
	if ((fs->no_blkgrp * grp_desc_size) % fs->blksz)
		fs->no_blk_pergdt++;

	/* allocate memory for gdtable */
	fs->gdtable = zalloc(fs->blksz * fs->no_blk_pergdt);
	if (!fs->gdtable)
		return -ENOMEM;
	/* read the group descriptor table */
	status = vfs_devread(fs->dev_desc,fs->gdtable_blkno * fs->sect_perblk, 0,
			fs->blksz * fs->no_blk_pergdt, fs->gdtable);
	if (status == 0)
		goto fail;

	if (ext4fs_log_gdt(fs->gdtable)) {
		printf("Error in ext4fs_log_gdt\n");
		return -1;
	}

	return 0;
fail:
	free(fs->gdtable);
	fs->gdtable = NULL;

	return -1;
}

static void delete_single_indirect_block(struct ext2_inode *inode)
{
	struct ext2_block_group *bgd = NULL;
	static int prev_bg_bmap_idx = -1;
	long int blknr;
	int remainder;
	int bg_idx;
	int status;
	struct ext_filesystem *fs = get_fs();
	unsigned int blk_per_grp = fs->ext4fs_root->sblock.blocks_per_group;
	char *journal_buffer = zalloc(fs->blksz);
	if (!journal_buffer) {
		printf("No memory\n");
		return;
	}
	/* get  block group descriptor table */
	bgd = (struct ext2_block_group *)fs->gdtable;

	/* deleting the single indirect block associated with inode */
	if (inode->b.blocks.indir_block != 0) {
		debug("SIPB releasing %u\n", inode->b.blocks.indir_block);
		blknr = inode->b.blocks.indir_block;
		if (fs->blksz != 1024) {
			bg_idx = blknr / blk_per_grp;
		} else {
			bg_idx = blknr / blk_per_grp;
			remainder = blknr % blk_per_grp;
			if (!remainder)
				bg_idx--;
		}
		ext4fs_reset_block_bmap(blknr, fs->blk_bmaps[bg_idx], bg_idx);
		bgd[bg_idx].free_blocks++;
		fs->sb->free_blocks++;
		/* journal backup */
		if (prev_bg_bmap_idx != bg_idx) {
			status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id *
						fs->sect_perblk, 0, fs->blksz,
						journal_buffer);
			if (status == 0)
				goto fail;
			if (ext4fs_log_journal
					(journal_buffer, bgd[bg_idx].block_id))
				goto fail;
			prev_bg_bmap_idx = bg_idx;
		}
	}
fail:
	free(journal_buffer);
}

static void delete_double_indirect_block(struct ext2_inode *inode)
{
	int i;
	short status;
	static int prev_bg_bmap_idx = -1;
	long int blknr;
	int remainder;
	int bg_idx;
	struct ext_filesystem *fs = get_fs();
	unsigned int blk_per_grp = fs->ext4fs_root->sblock.blocks_per_group;
	unsigned int *di_buffer = NULL;
	unsigned int *DIB_start_addr = NULL;
	struct ext2_block_group *bgd = NULL;
	char *journal_buffer = zalloc(fs->blksz);
	if (!journal_buffer) {
		printf("No memory\n");
		return;
	}
	/* get the block group descriptor table */
	bgd = (struct ext2_block_group *)fs->gdtable;

	if (inode->b.blocks.double_indir_block != 0) {
		di_buffer = zalloc(fs->blksz);
		if (!di_buffer) {
			printf("No memory\n");
			return;
		}
		DIB_start_addr = (unsigned int *)di_buffer;
		blknr = inode->b.blocks.double_indir_block;
		status = vfs_devread(fs->dev_desc,blknr * fs->sect_perblk, 0, fs->blksz,
				(char *)di_buffer);
		for (i = 0; i < fs->blksz / sizeof(int); i++) {
			if (*di_buffer == 0)
				break;

			debug("DICB releasing %u\n", *di_buffer);
			if (fs->blksz != 1024) {
				bg_idx = (*di_buffer) / blk_per_grp;
			} else {
				bg_idx = (*di_buffer) / blk_per_grp;
				remainder = (*di_buffer) % blk_per_grp;
				if (!remainder)
					bg_idx--;
			}
			ext4fs_reset_block_bmap(*di_buffer,
					fs->blk_bmaps[bg_idx], bg_idx);
			di_buffer++;
			bgd[bg_idx].free_blocks++;
			fs->sb->free_blocks++;
			/* journal backup */
			if (prev_bg_bmap_idx != bg_idx) {
				status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id
						* fs->sect_perblk, 0,
						fs->blksz,
						journal_buffer);
				if (status == 0)
					goto fail;

				if (ext4fs_log_journal(journal_buffer,
							bgd[bg_idx].block_id))
					goto fail;
				prev_bg_bmap_idx = bg_idx;
			}
		}

		/* removing the parent double indirect block */
		blknr = inode->b.blocks.double_indir_block;
		if (fs->blksz != 1024) {
			bg_idx = blknr / blk_per_grp;
		} else {
			bg_idx = blknr / blk_per_grp;
			remainder = blknr % blk_per_grp;
			if (!remainder)
				bg_idx--;
		}
		ext4fs_reset_block_bmap(blknr, fs->blk_bmaps[bg_idx], bg_idx);
		bgd[bg_idx].free_blocks++;
		fs->sb->free_blocks++;
		/* journal backup */
		if (prev_bg_bmap_idx != bg_idx) {
			memset(journal_buffer, '\0', fs->blksz);
			status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id *
					fs->sect_perblk, 0, fs->blksz,
					journal_buffer);
			if (status == 0)
				goto fail;

			if (ext4fs_log_journal(journal_buffer,
						bgd[bg_idx].block_id))
				goto fail;
			prev_bg_bmap_idx = bg_idx;
		}
		debug("DIPB releasing %ld\n", blknr);
	}
fail:
	free(DIB_start_addr);
	free(journal_buffer);
}

static void delete_triple_indirect_block(struct ext2_inode *inode)
{
	int i, j;
	short status;
	static int prev_bg_bmap_idx = -1;
	long int blknr;
	int remainder;
	int bg_idx;
	struct ext_filesystem *fs = get_fs();
	unsigned int blk_per_grp = fs->ext4fs_root->sblock.blocks_per_group;
	unsigned int *tigp_buffer = NULL;
	unsigned int *tib_start_addr = NULL;
	unsigned int *tip_buffer = NULL;
	unsigned int *tipb_start_addr = NULL;
	struct ext2_block_group *bgd = NULL;

	char *journal_buffer = zalloc(fs->blksz);
	if (!journal_buffer) {
		printf("No memory\n");
		return;
	}
	/* get block group descriptor table */
	bgd = (struct ext2_block_group *)fs->gdtable;

	if (inode->b.blocks.triple_indir_block != 0) {
		tigp_buffer = zalloc(fs->blksz);
		if (!tigp_buffer) {
			printf("No memory\n");
			return;
		}
		tib_start_addr = (unsigned int *)tigp_buffer;
		blknr = inode->b.blocks.triple_indir_block;
		status = vfs_devread(fs->dev_desc,blknr * fs->sect_perblk, 0, fs->blksz,
				(char *)tigp_buffer);
		for (i = 0; i < fs->blksz / sizeof(int); i++) {
			if (*tigp_buffer == 0)
				break;
			debug("tigp buffer releasing %u\n", *tigp_buffer);

			tip_buffer = zalloc(fs->blksz);
			if (!tip_buffer)
				goto fail;
			tipb_start_addr = (unsigned int *)tip_buffer;
			status = vfs_devread(fs->dev_desc,(*tigp_buffer) *
					fs->sect_perblk, 0, fs->blksz,
					(char *)tip_buffer);
			for (j = 0; j < fs->blksz / sizeof(int); j++) {
				if (*tip_buffer == 0)
					break;
				if (fs->blksz != 1024) {
					bg_idx = (*tip_buffer) / blk_per_grp;
				} else {
					bg_idx = (*tip_buffer) / blk_per_grp;

					remainder = (*tip_buffer) % blk_per_grp;
					if (!remainder)
						bg_idx--;
				}

				ext4fs_reset_block_bmap(*tip_buffer,
						fs->blk_bmaps[bg_idx],
						bg_idx);

				tip_buffer++;
				bgd[bg_idx].free_blocks++;
				fs->sb->free_blocks++;
				/* journal backup */
				if (prev_bg_bmap_idx != bg_idx) {
					status = vfs_devread(fs->dev_desc,
								bgd[bg_idx].block_id *
								fs->sect_perblk, 0,
								fs->blksz,
								journal_buffer);
					if (status == 0)
						goto fail;

					if (ext4fs_log_journal(journal_buffer,
								bgd[bg_idx].
								block_id))
						goto fail;
					prev_bg_bmap_idx = bg_idx;
				}
			}
			free(tipb_start_addr);
			tipb_start_addr = NULL;

			/*
			 * removing the grand parent blocks
			 * which is connected to inode
			 */
			if (fs->blksz != 1024) {
				bg_idx = (*tigp_buffer) / blk_per_grp;
			} else {
				bg_idx = (*tigp_buffer) / blk_per_grp;

				remainder = (*tigp_buffer) % blk_per_grp;
				if (!remainder)
					bg_idx--;
			}
			ext4fs_reset_block_bmap(*tigp_buffer,
					fs->blk_bmaps[bg_idx], bg_idx);

			tigp_buffer++;
			bgd[bg_idx].free_blocks++;
			fs->sb->free_blocks++;
			/* journal backup */
			if (prev_bg_bmap_idx != bg_idx) {
				memset(journal_buffer, '\0', fs->blksz);
				status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id *
							fs->sect_perblk, 0,
							fs->blksz, journal_buffer);
				if (status == 0)
					goto fail;

				if (ext4fs_log_journal(journal_buffer,
							bgd[bg_idx].block_id))
					goto fail;
				prev_bg_bmap_idx = bg_idx;
			}
		}

		/* removing the grand parent triple indirect block */
		blknr = inode->b.blocks.triple_indir_block;
		if (fs->blksz != 1024) {
			bg_idx = blknr / blk_per_grp;
		} else {
			bg_idx = blknr / blk_per_grp;
			remainder = blknr % blk_per_grp;
			if (!remainder)
				bg_idx--;
		}
		ext4fs_reset_block_bmap(blknr, fs->blk_bmaps[bg_idx], bg_idx);
		bgd[bg_idx].free_blocks++;
		fs->sb->free_blocks++;
		/* journal backup */
		if (prev_bg_bmap_idx != bg_idx) {
			memset(journal_buffer, '\0', fs->blksz);
			status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id *
					fs->sect_perblk, 0, fs->blksz,
					journal_buffer);
			if (status == 0)
				goto fail;

			if (ext4fs_log_journal(journal_buffer,
						bgd[bg_idx].block_id))
				goto fail;
			prev_bg_bmap_idx = bg_idx;
		}
		debug("tigp buffer itself releasing %ld\n", blknr);
	}
fail:
	free(tib_start_addr);
	free(tipb_start_addr);
	free(journal_buffer);
}

static int ext4fs_delete_file(int inodeno)
{
	struct ext2_inode inode;
	short status;
	int i;
	int remainder;
	long int blknr;
	int bg_idx;
	int ibmap_idx;
	char *read_buffer = NULL;
	char *start_block_address = NULL;
	unsigned int no_blocks;

	static int prev_bg_bmap_idx = -1;
	unsigned int inodes_per_block;
	long int blkno;
	unsigned int blkoff;
	struct ext_filesystem *fs = get_fs();
	unsigned int blk_per_grp = fs->ext4fs_root->sblock.blocks_per_group;
	unsigned int inode_per_grp = fs->ext4fs_root->sblock.inodes_per_group;
	struct ext2_inode *inode_buffer = NULL;
	struct ext2_block_group *bgd = NULL;
	char *journal_buffer = zalloc(fs->blksz);
	if (!journal_buffer)
		return -ENOMEM;
	/* get the block group descriptor table */
	bgd = (struct ext2_block_group *)fs->gdtable;
	status = ext4fs_read_inode(fs, fs->ext4fs_root, inodeno, &inode);
	if (status == 0)
		goto fail;

	/* read the block no allocated to a file */
	no_blocks = inode.size / fs->blksz;
	if (inode.size % fs->blksz)
		no_blocks++;

	if (le32_to_cpu(inode.flags) & EXT4_EXTENTS_FL) {
		struct ext2fs_node *node_inode =
			zalloc(sizeof(struct ext2fs_node));
		if (!node_inode)
			goto fail;
		node_inode->data = fs->ext4fs_root;
		node_inode->ino = inodeno;
		node_inode->inode_read = 0;
		memcpy(&(node_inode->inode), &inode, sizeof(struct ext2_inode));

		for (i = 0; i < no_blocks; i++) {
			blknr = read_allocated_block(&(node_inode->inode), i);
			if (fs->blksz != 1024) {
				bg_idx = blknr / blk_per_grp;
			} else {
				bg_idx = blknr / blk_per_grp;
				remainder = blknr % blk_per_grp;
				if (!remainder)
					bg_idx--;
			}
			ext4fs_reset_block_bmap(blknr, fs->blk_bmaps[bg_idx],
					bg_idx);
			debug("EXT4_EXTENTS Block releasing %ld: %d\n",
					blknr, bg_idx);

			bgd[bg_idx].free_blocks++;
			fs->sb->free_blocks++;

			/* journal backup */
			if (prev_bg_bmap_idx != bg_idx) {
				status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id *
							fs->sect_perblk, 0,
							fs->blksz, journal_buffer);
				if (status == 0)
					goto fail;
				if (ext4fs_log_journal(journal_buffer,
							bgd[bg_idx].block_id))
					goto fail;
				prev_bg_bmap_idx = bg_idx;
			}
		}
		if (node_inode) {
			free(node_inode);
			node_inode = NULL;
		}
	} else {

		delete_single_indirect_block(&inode);
		delete_double_indirect_block(&inode);
		delete_triple_indirect_block(&inode);

		/* read the block no allocated to a file */
		no_blocks = inode.size / fs->blksz;
		if (inode.size % fs->blksz)
			no_blocks++;
		for (i = 0; i < no_blocks; i++) {
			blknr = read_allocated_block(&inode, i);
			if (fs->blksz != 1024) {
				bg_idx = blknr / blk_per_grp;
			} else {
				bg_idx = blknr / blk_per_grp;
				remainder = blknr % blk_per_grp;
				if (!remainder)
					bg_idx--;
			}
			ext4fs_reset_block_bmap(blknr, fs->blk_bmaps[bg_idx],
					bg_idx);
			debug("ActualB releasing %ld: %d\n", blknr, bg_idx);

			bgd[bg_idx].free_blocks++;
			fs->sb->free_blocks++;
			/* journal backup */
			if (prev_bg_bmap_idx != bg_idx) {
				memset(journal_buffer, '\0', fs->blksz);
				status = vfs_devread(fs->dev_desc,bgd[bg_idx].block_id
						* fs->sect_perblk,
						0, fs->blksz,
						journal_buffer);
				if (status == 0)
					goto fail;
				if (ext4fs_log_journal(journal_buffer,
							bgd[bg_idx].block_id))
					goto fail;
				prev_bg_bmap_idx = bg_idx;
			}
		}
	}

	/* from the inode no to blockno */
	inodes_per_block = fs->blksz / fs->inodesz;
	ibmap_idx = inodeno / inode_per_grp;

	/* get the block no */
	inodeno--;
	blkno = bgd[ibmap_idx].inode_table_id +
		(inodeno % inode_per_grp) / inodes_per_block;

	/* get the offset of the inode */
	blkoff = ((inodeno) % inodes_per_block) * fs->inodesz;

	/* read the block no containing the inode */
	read_buffer = zalloc(fs->blksz);
	if (!read_buffer)
		goto fail;
	start_block_address = read_buffer;
	status = vfs_devread(fs->dev_desc,blkno * fs->sect_perblk,
			0, fs->blksz, read_buffer);
	if (status == 0)
		goto fail;

	if (ext4fs_log_journal(read_buffer, blkno))
		goto fail;

	read_buffer = read_buffer + blkoff;
	inode_buffer = (struct ext2_inode *)read_buffer;
	memset(inode_buffer, '\0', sizeof(struct ext2_inode));

	/* write the inode to original position in inode table */
	if (ext4fs_put_metadata(start_block_address, blkno))
		goto fail;

	/* update the respective inode bitmaps */
	inodeno++;
	ext4fs_reset_inode_bmap(inodeno, fs->inode_bmaps[ibmap_idx], ibmap_idx);
	bgd[ibmap_idx].free_inodes++;
	fs->sb->free_inodes++;
	/* journal backup */
	memset(journal_buffer, '\0', fs->blksz);
	status = vfs_devread(fs->dev_desc,bgd[ibmap_idx].inode_id *
			fs->sect_perblk, 0, fs->blksz, journal_buffer);
	if (status == 0)
		goto fail;
	if (ext4fs_log_journal(journal_buffer, bgd[ibmap_idx].inode_id))
		goto fail;

	ext4fs_update();
	ext4fs_deinit();

	if (ext4fs_init() != 0) {
		printf("error in File System init\n");
		goto fail;
	}

	free(start_block_address);
	free(journal_buffer);

	return 0;
fail:
	free(start_block_address);
	free(journal_buffer);

	return -1;
}

int ext4fs_init(void)
{
	short status;
	int i;
	unsigned int real_free_blocks = 0;
	struct ext_filesystem *fs = get_fs();

	/* populate fs */
	fs->blksz = EXT2_BLOCK_SIZE(fs->ext4fs_root);
	fs->inodesz = INODE_SIZE_FILESYSTEM(fs->ext4fs_root);
	fs->sect_perblk = fs->blksz / SECTOR_SIZE;

	/* get the superblock */
	fs->sb = zalloc(SUPERBLOCK_SIZE);
	if (!fs->sb)
		return -1;
	if (!vfs_devread(fs->dev_desc,SUPERBLOCK_SECTOR, 0, SUPERBLOCK_SIZE,
				(char *)fs->sb))
		goto fail;

	/* init journal */
	if (ext4fs_init_journal())
		goto fail;

	/* get total no of blockgroups */
	fs->no_blkgrp = (uint32_t)ext4fs_div_roundup(
			(fs->ext4fs_root->sblock.total_blocks -
			 fs->ext4fs_root->sblock.first_data_block),
			fs->ext4fs_root->sblock.blocks_per_group);

	/* get the block group descriptor table */
	fs->gdtable_blkno = ((EXT2_MIN_BLOCK_SIZE == fs->blksz) + 1);
	if (ext4fs_get_bgdtable() == -1) {
		printf("Error in getting the block group descriptor table\n");
		goto fail;
	}
	fs->bgd = (struct ext2_block_group *)fs->gdtable;

	/* load all the available bitmap block of the partition */
	fs->blk_bmaps = zalloc(fs->no_blkgrp * sizeof(char *));
	if (!fs->blk_bmaps)
		goto fail;
	for (i = 0; i < fs->no_blkgrp; i++) {
		fs->blk_bmaps[i] = zalloc(fs->blksz);
		if (!fs->blk_bmaps[i])
			goto fail;
	}

	for (i = 0; i < fs->no_blkgrp; i++) {
		status = vfs_devread(fs->dev_desc,fs->bgd[i].block_id * fs->sect_perblk, 0,
					fs->blksz, (char *)fs->blk_bmaps[i]);
		if (status == 0)
			goto fail;
	}

	/* load all the available inode bitmap of the partition */
	fs->inode_bmaps = zalloc(fs->no_blkgrp * sizeof(unsigned char *));
	if (!fs->inode_bmaps)
		goto fail;
	for (i = 0; i < fs->no_blkgrp; i++) {
		fs->inode_bmaps[i] = zalloc(fs->blksz);
		if (!fs->inode_bmaps[i])
			goto fail;
	}

	for (i = 0; i < fs->no_blkgrp; i++) {
		status =vfs_devread(fs->dev_desc,fs->bgd[i].inode_id * fs->sect_perblk,
				0, fs->blksz,
				(char *)fs->inode_bmaps[i]);
		if (status == 0)
			goto fail;
	}

	/*
	 * check filesystem consistency with free blocks of file system
	 * some time we observed that superblock freeblocks does not match
	 * with the  blockgroups freeblocks when improper
	 * reboot of a linux kernel
	 */
	for (i = 0; i < fs->no_blkgrp; i++)
		real_free_blocks = real_free_blocks + fs->bgd[i].free_blocks;
	if (real_free_blocks != fs->sb->free_blocks)
		fs->sb->free_blocks = real_free_blocks;

	return 0;
fail:
	ext4fs_deinit();

	return -1;
}

void ext4fs_deinit(void)
{
	int i;
	struct ext2_inode inode_journal;
	struct journal_superblock_t *jsb;
	long int blknr;
	struct ext_filesystem *fs = get_fs();

	/* free journal */
	char *temp_buff = zalloc(fs->blksz);
	if (temp_buff) {
		ext4fs_read_inode(fs, fs->ext4fs_root, EXT2_JOURNAL_INO,
				&inode_journal);
		blknr = read_allocated_block(&inode_journal,
				EXT2_JOURNAL_SUPERBLOCK);
		vfs_devread(fs->dev_desc,blknr * fs->sect_perblk, 0, fs->blksz,
				temp_buff);
		jsb = (struct journal_superblock_t *)temp_buff;
		jsb->s_start = cpu_to_be32(0);
		put_ext4((uint64_t) (blknr * fs->blksz),
				(struct journal_superblock_t *)temp_buff, fs->blksz);
		free(temp_buff);
	}
	ext4fs_free_journal();

	/* get the superblock */
	vfs_devread(fs->dev_desc,SUPERBLOCK_SECTOR, 0, SUPERBLOCK_SIZE, (char *)fs->sb);
	fs->sb->feature_incompat &= ~EXT3_FEATURE_INCOMPAT_RECOVER;
	put_ext4((uint64_t)(SUPERBLOCK_SIZE),
			(struct ext2_sblock *)fs->sb, (uint32_t)SUPERBLOCK_SIZE);
	free(fs->sb);
	fs->sb = NULL;

	if (fs->blk_bmaps) {
		for (i = 0; i < fs->no_blkgrp; i++) {
			free(fs->blk_bmaps[i]);
			fs->blk_bmaps[i] = NULL;
		}
		free(fs->blk_bmaps);
		fs->blk_bmaps = NULL;
	}

	if (fs->inode_bmaps) {
		for (i = 0; i < fs->no_blkgrp; i++) {
			free(fs->inode_bmaps[i]);
			fs->inode_bmaps[i] = NULL;
		}
		free(fs->inode_bmaps);
		fs->inode_bmaps = NULL;
	}


	free(fs->gdtable);
	fs->gdtable = NULL;
	fs->bgd = NULL;
	/*
	 * reinitiliazed the global inode and
	 * block bitmap first execution check variables
	 */
	fs->first_pass_ibmap = 0;
	fs->first_pass_bbmap = 0;
	fs->curr_inode_no = 0;
	fs->curr_blkno = 0;
}

static int ext4fs_write_file(struct ext2_inode *file_inode,
		int pos, unsigned int len, char *buf)
{
	int i;
	int blockcnt;
	struct ext_filesystem *fs = get_fs();
	int log2blocksize = LOG2_EXT2_BLOCK_SIZE(fs->ext4fs_root);
	unsigned int filesize = file_inode->size;
	int previous_block_number = -1;
	int delayed_start = 0;
	int delayed_extent = 0;
	int delayed_next = 0;
	char *delayed_buf = NULL;

	/* Adjust len so it we can't read past the end of the file. */
	if (len > filesize)
		len = filesize;

	blockcnt = ((len + pos) + fs->blksz - 1) / fs->blksz;

	for (i = pos / fs->blksz; i < blockcnt; i++) {
		long int blknr;
		int blockend = fs->blksz;
		int skipfirst = 0;
		blknr = read_allocated_block1(file_inode, i);
		if (blknr < 0)
			return -1;

		blknr = blknr << log2blocksize;

		if (blknr) {
			if (previous_block_number != -1) {
				if (delayed_next == blknr) {
					delayed_extent += blockend;
					delayed_next += blockend >> SECTOR_BITS;
				} else {	/* spill */
					put_ext4((uint64_t) (delayed_start *
								SECTOR_SIZE),
							delayed_buf,
							(uint32_t) delayed_extent);
					previous_block_number = blknr;
					delayed_start = blknr;
					delayed_extent = blockend;
					delayed_buf = buf;
					delayed_next = blknr +
						(blockend >> SECTOR_BITS);
				}
			} else {
				previous_block_number = blknr;
				delayed_start = blknr;
				delayed_extent = blockend;
				delayed_buf = buf;
				delayed_next = blknr +
					(blockend >> SECTOR_BITS);
			}
		} else {
			if (previous_block_number != -1) {
				/* spill */
				put_ext4((uint64_t) (delayed_start *
							SECTOR_SIZE), delayed_buf,
						(uint32_t) delayed_extent);
				previous_block_number = -1;
			}
			memset(buf, 0, fs->blksz - skipfirst);
		}
		buf += fs->blksz - skipfirst;
	}
	if (previous_block_number != -1) {
		/* spill */
		put_ext4((uint64_t) (delayed_start * SECTOR_SIZE),
				delayed_buf, (uint32_t) delayed_extent);
		previous_block_number = -1;
	}

	return len;
}

int ext4fs_write(const char *fname, unsigned char *buffer,
		unsigned long sizebytes)
{
	int ret = 0;
	struct ext2_inode *file_inode = NULL;
	unsigned char *inode_buffer = NULL;
	int parent_inodeno;
	int inodeno;
	time_t timestamp = 0;

	uint64_t bytes_reqd_for_file;
	unsigned int blks_reqd_for_file;
	unsigned int blocks_remaining;
	int existing_file_inodeno;
	char *temp_ptr = NULL;
	long int itable_blkno;
	long int parent_itable_blkno;
	long int blkoff;
	struct ext_filesystem *fs = get_fs();
	struct ext2_sblock *sblock = &(fs->ext4fs_root->sblock);
	unsigned int inodes_per_block;
	unsigned int ibmap_idx;
	ALLOC_CACHE_ALIGN_BUFFER(char, filename, 256);
	memset(filename, 0x00, sizeof(filename));

	g_parent_inode = zalloc(sizeof(struct ext2_inode));
	if (!g_parent_inode)
		goto fail;

	if (ext4fs_init() != 0) {
		printf("error in File System init\n");
		return -1;
	}
	inodes_per_block = fs->blksz / fs->inodesz;
	parent_inodeno = ext4fs_get_parent_inode_num(fname, filename, F_FILE);
	if (parent_inodeno == -1)
		goto fail;
	if (ext4fs_iget(parent_inodeno, g_parent_inode))
		goto fail;
	/* check if the filename is already present in root */
	existing_file_inodeno = ext4fs_filename_check(filename);
	if (existing_file_inodeno != -1) {
		ret = ext4fs_delete_file(existing_file_inodeno);
		fs->first_pass_bbmap = 0;
		fs->curr_blkno = 0;

		fs->first_pass_ibmap = 0;
		fs->curr_inode_no = 0;
		if (ret)
			goto fail;
	}
	/* calucalate how many blocks required */
	bytes_reqd_for_file = sizebytes;
	blks_reqd_for_file = lldiv(bytes_reqd_for_file, fs->blksz);
	if (do_div(bytes_reqd_for_file, fs->blksz) != 0) {
		blks_reqd_for_file++;
		debug("total bytes for a file %u\n", blks_reqd_for_file);
	}
	blocks_remaining = blks_reqd_for_file;
	/* test for available space in partition */
	if (fs->sb->free_blocks < blks_reqd_for_file) {
		printf("Not enough space on partition !!!\n");
		goto fail;
	}

	ext4fs_update_parent_dentry(filename, &inodeno, FILETYPE_REG);
	/* prepare file inode */
	inode_buffer = zalloc(fs->inodesz);
	if (!inode_buffer)
		goto fail;
	file_inode = (struct ext2_inode *)inode_buffer;
	file_inode->mode = S_IFREG | S_IRWXU |
		S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH;
	/* ToDo: Update correct time */
	file_inode->mtime = timestamp;
	file_inode->atime = timestamp;
	file_inode->ctime = timestamp;
	file_inode->nlinks = 1;
	file_inode->size = sizebytes;

	/* Allocate data blocks */
	ext4fs_allocate_blocks(file_inode, blocks_remaining,
			&blks_reqd_for_file);
	file_inode->blockcnt = (blks_reqd_for_file * fs->blksz) / SECTOR_SIZE;

	temp_ptr = zalloc(fs->blksz);
	if (!temp_ptr)
		goto fail;
	ibmap_idx = inodeno / fs->ext4fs_root->sblock.inodes_per_group;
	inodeno--;
	itable_blkno = fs->bgd[ibmap_idx].inode_table_id +
		(inodeno % sblock->inodes_per_group) /
		inodes_per_block;
	blkoff = (inodeno % inodes_per_block) * fs->inodesz;
	vfs_devread(fs->dev_desc,itable_blkno * fs->sect_perblk, 0, fs->blksz, temp_ptr);
	if (ext4fs_log_journal(temp_ptr, itable_blkno))
		goto fail;

	memcpy(temp_ptr + blkoff, inode_buffer, fs->inodesz);
	if (ext4fs_put_metadata(temp_ptr, itable_blkno))
		goto fail;
	/* copy the file content into data blocks */
	if (ext4fs_write_file(file_inode, 0, sizebytes, (char *)buffer) == -1) {
		printf("Error in copying content\n");
		goto fail;
	}
	ibmap_idx = parent_inodeno / fs->ext4fs_root->sblock.inodes_per_group;
	parent_inodeno--;
	parent_itable_blkno = fs->bgd[ibmap_idx].inode_table_id +
		(parent_inodeno %
		 (sblock->inodes_per_group)) / inodes_per_block;
	blkoff = (parent_inodeno % inodes_per_block) * fs->inodesz;
	if (parent_itable_blkno != itable_blkno) {
		memset(temp_ptr, '\0', fs->blksz);
		vfs_devread(fs->dev_desc,parent_itable_blkno * fs->sect_perblk,
				0, fs->blksz, temp_ptr);
		if (ext4fs_log_journal(temp_ptr, parent_itable_blkno))
			goto fail;

		memcpy(temp_ptr + blkoff, g_parent_inode,
				sizeof(struct ext2_inode));
		if (ext4fs_put_metadata(temp_ptr, parent_itable_blkno))
			goto fail;
		free(temp_ptr);
	} else {
		/*
		 * If parent and child fall in same inode table block
		 * both should be kept in 1 buffer
		 */
		memcpy(temp_ptr + blkoff, g_parent_inode,
				sizeof(struct ext2_inode));
		gd_index--;
		if (ext4fs_put_metadata(temp_ptr, itable_blkno))
			goto fail;
		free(temp_ptr);
	}
	ext4fs_update();
	ext4fs_deinit();

	fs->first_pass_bbmap = 0;
	fs->curr_blkno = 0;
	fs->first_pass_ibmap = 0;
	fs->curr_inode_no = 0;
	free(inode_buffer);
	free(g_parent_inode);
	g_parent_inode = NULL;

	return 0;
fail:
	ext4fs_deinit();
	free(inode_buffer);
	free(g_parent_inode);
	g_parent_inode = NULL;

	return -1;
}
#endif
