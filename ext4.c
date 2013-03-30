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
#include "ext.h"
#include "ext4.h"
#include "disk.h"
#include "fs.h"

struct filesys_spec {
	struct ext_filesystem extfs;
};

struct ext2fs_file_entry {
	struct file_entry  base;
	struct ext2fs_node *ext4fs_file;
};

struct ext_filesystem ext_fs;

struct ext_filesystem *get_fs(void)
{
	return &ext_fs;
}
static struct ext4_extent_header *ext4fs_get_extent_block(
	struct ext2_data *data, char *buf,
		struct ext4_extent_header *ext_block,
		uint32_t fileblock, int log2_blksz);

static void ext4fs_free_node(struct ext2fs_node *node, struct ext2fs_node *currroot)
{
	struct ext_filesystem *fs = get_fs();
	if ((node != &fs->ext4fs_root->diropen) && (node != currroot)) {
		free(node->indir1_block);
		free(node->indir2_block);
		free(node->indir3_block);
		free(node);
	}
}

long int read_allocated_block1(struct ext2fs_node *fsinode, int fileblock)
{
	long int blknr;
	int blksz;
	int log2_blksz;
	int status;
	long int rblock;
	long int perblock_parent;
	long int perblock_child;
	unsigned long long start;
	struct ext_filesystem *fs = get_fs();
	struct ext2_inode *inode = &fsinode->inode;

	/* get the blocksize of the filesystem */
	blksz = EXT2_BLOCK_SIZE(fs->ext4fs_root);
	log2_blksz = LOG2_EXT2_BLOCK_SIZE(fs->ext4fs_root);
	if (inode->flags & EXT4_EXTENTS_FL) {
		char *buf = zalloc(blksz);
		if (!buf)
			return -1;
		struct ext4_extent_header *ext_block;
		struct ext4_extent *extent;
		int i = -1;
		ext_block = ext4fs_get_extent_block(fs->ext4fs_root, buf,
						    (struct ext4_extent_header*)inode->b.blocks.dir_blocks,
						    fileblock, log2_blksz);
		if (!ext_block) {
			printf("invalid extent block\n");
			free(buf);
			return -2;
		}

		extent = (struct ext4_extent *)(ext_block + 1);

		do {
			i++;
			if (i >= ext_block->eh_entries)
				break;
		} while (fileblock >= extent[i].ee_block);
		if (--i >= 0) {
			fileblock -= extent[i].ee_block;
			if (fileblock >= extent[i].ee_len) {
				free(buf);
				return 0;
			}

			start = extent[i].ee_start_hi;
			start = (start << 32) + extent[i].ee_start_lo;
			free(buf);
			return fileblock + start;
		}

		printf("Extent Error\n");
		free(buf);
		return -1;
	}

	/* Direct blocks. */
	if (fileblock < INDIRECT_BLOCKS)
		blknr = (inode->b.blocks.dir_blocks[fileblock]);

	/* Indirect. */
	else if (fileblock < (INDIRECT_BLOCKS + (blksz / 4))) {
		if (fsinode->indir1_block == NULL) {
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** SI ext2fs read block (indir 1)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
			fsinode->indir1_blkno = -1;
		}
		if (blksz != fsinode->indir1_size) {
			free(fsinode->indir1_block);
			fsinode->indir1_block = NULL;
			fsinode->indir1_size = 0;
			fsinode->indir1_blkno = -1;
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** SI ext2fs read block (indir 1):"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
		}
		if (((inode->b.blocks.indir_block) <<
		     log2_blksz) != fsinode->indir1_blkno) {
			status = vfs_devread(fs->dev_desc,
					   (inode->b.blocks.
					    indir_block) << log2_blksz, 0,
					   blksz, (char *)fsinode->indir1_block);
			if (status == 0) {
				printf("** SI ext2fs read block (indir 1)"
					"failed. **\n");
				return 0;
			}
			fsinode->indir1_blkno =
				(inode->b.blocks.indir_block) << log2_blksz;
		}
		blknr = (fsinode->indir1_block[fileblock - INDIRECT_BLOCKS]);
	}
	/* Double indirect. */
	else if (fileblock < (INDIRECT_BLOCKS + (blksz / 4 *
					(blksz / 4 + 1)))) {

		long int perblock = blksz / 4;
		long int rblock = fileblock - (INDIRECT_BLOCKS + blksz / 4);

		if (fsinode->indir1_block == NULL) {
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** DI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
			fsinode->indir1_blkno = -1;
		}
		if (blksz != fsinode->indir1_size) {
			free(fsinode->indir1_block);
			fsinode->indir1_block = NULL;
			fsinode->indir1_size = 0;
			fsinode->indir1_blkno = -1;
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** DI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
		}
		if (((inode->b.blocks.double_indir_block) <<
		     log2_blksz) != fsinode->indir1_blkno) {
			status =
			    vfs_devread(fs->dev_desc,
					   (inode->b.blocks.
					    double_indir_block) << log2_blksz,
					   0, blksz,
					   (char *)fsinode->indir1_block);
			if (status == 0) {
				printf("** DI ext2fs read block (indir 2 1)"
					"failed. **\n");
				return -1;
			}
			fsinode->indir1_blkno =
			    (inode->b.blocks.double_indir_block) <<
			    log2_blksz;
		}

		if (fsinode->indir2_block == NULL) {
			fsinode->indir2_block = zalloc(blksz);
			if (fsinode->indir2_block == NULL) {
				printf("** DI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir2_size = blksz;
			fsinode->indir2_blkno = -1;
		}
		if (blksz != fsinode->indir2_size) {
			free(fsinode->indir2_block);
			fsinode->indir2_block = NULL;
			fsinode->indir2_size = 0;
			fsinode->indir2_blkno = -1;
			fsinode->indir2_block = zalloc(blksz);
			if (fsinode->indir2_block == NULL) {
				printf("** DI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir2_size = blksz;
		}
		if (((fsinode->indir1_block[rblock / perblock]) <<
		     log2_blksz) !=fsinode->indir2_blkno) {
			status = vfs_devread(fs->dev_desc,
						(fsinode->indir1_block
						 [rblock /
						  perblock]) << log2_blksz, 0,
						blksz,
						(char *)fsinode->indir2_block);
			if (status == 0) {
				printf("** DI ext2fs read block (indir 2 2)"
					"failed. **\n");
				return -1;
			}
			fsinode->indir2_blkno =(fsinode->indir1_block[rblock/perblock]) << log2_blksz;
		}
		blknr = fsinode->indir2_block[rblock % perblock];
	}
	/* Tripple indirect. */
	else {
		rblock = fileblock - (INDIRECT_BLOCKS + blksz / 4 +
				      (blksz / 4 * blksz / 4));
		perblock_child = blksz / 4;
		perblock_parent = ((blksz / 4) * (blksz / 4));

		if (fsinode->indir1_block == NULL) {
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** TI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
			fsinode->indir1_blkno = -1;
		}
		if (blksz != fsinode->indir1_size) {
			free(fsinode->indir1_block);
			fsinode->indir1_block = NULL;
			fsinode->indir1_size = 0;
			fsinode->indir1_blkno = -1;
			fsinode->indir1_block = zalloc(blksz);
			if (fsinode->indir1_block == NULL) {
				printf("** TI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir1_size = blksz;
		}
		if (((inode->b.blocks.triple_indir_block) <<
		     log2_blksz) != fsinode->indir1_blkno) {
			status = vfs_devread(fs->dev_desc,inode->b.blocks.triple_indir_block << log2_blksz, 0, blksz,
			     (char *)fsinode->indir1_block);
			if (status == 0) {
				printf("** TI ext2fs read block (indir 2 1)"
					"failed. **\n");
				return -1;
			}
			fsinode->indir1_blkno =
			    (inode->b.blocks.triple_indir_block) <<
			    log2_blksz;
		}

		if (fsinode->indir2_block == NULL) {
			fsinode->indir2_block = zalloc(blksz);
			if (fsinode->indir2_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir2_size = blksz;
			fsinode->indir2_blkno = -1;
		}
		if (blksz != fsinode->indir2_size) {
			free(fsinode->indir2_block);
			fsinode->indir2_block = NULL;
			fsinode->indir2_size = 0;
			fsinode->indir2_blkno = -1;
			fsinode->indir2_block = zalloc(blksz);
			if (fsinode->indir2_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir2_size = blksz;
		}
		if (((fsinode->indir1_block[rblock /
						       perblock_parent]) <<
		     log2_blksz)!= fsinode->indir2_blkno) {
			status = vfs_devread(fs->dev_desc,
						(fsinode->indir1_block
						 [rblock /
						  perblock_parent]) <<
						log2_blksz, 0, blksz,
						(char *)fsinode->indir2_block);
			if (status == 0) {
				printf("** TI ext2fs read block (indir 2 2)"
					"failed. **\n");
				return -1;
			}
			fsinode->indir2_blkno =
			    (fsinode->indir1_block[rblock /
							      perblock_parent])
			    << log2_blksz;
		}

		if (fsinode->indir3_block == NULL) {
			fsinode->indir3_block = zalloc(blksz);
			if (fsinode->indir3_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir3_size = blksz;
			fsinode->indir3_blkno = -1;
		}
		if (blksz != fsinode->indir3_size) {
			free(fsinode->indir3_block);
			fsinode->indir3_block = NULL;
			fsinode->indir3_size = 0;
			fsinode->indir3_blkno = -1;
			fsinode->indir3_block = zalloc(blksz);
			if (fsinode->indir3_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			fsinode->indir3_size = blksz;
		}
		if (((fsinode->indir2_block[rblock / perblock_child]) << log2_blksz) != fsinode->indir3_blkno) {
			status = vfs_devread(fs->dev_desc,
					   (fsinode->indir2_block
					    [(rblock / perblock_child)
					     % (blksz / 4)]) << log2_blksz, 0,
					   blksz, (char *)fsinode->indir3_block);
			if (status == 0) {
				printf("** TI ext2fs read block (indir 2 2)"
				       "failed. **\n");
				return -1;
			}
			fsinode->indir3_blkno =
			    (fsinode->indir2_block[(rblock /
							       perblock_child) %
							      (blksz /
							       4)]) <<
			    log2_blksz;
		}

		blknr = (fsinode->indir3_block[rblock % perblock_child]);
	}
#ifdef DEBUG
	printf("ext4fs_read_block %ld\n", blknr);
#endif
	return blknr;
}

/*
 * Taken from openmoko-kernel mailing list: By Andy green
 * Optimized read file API : collects and defers contiguous sector
 * reads into one potentially more efficient larger sequential read action
 */
static int ext4fs_read_file(struct ext_filesystem *fs, struct ext2fs_node *node,  int pos,
		unsigned int len, char *buf)
{
	int i;
	int blockcnt;
	int previous_block_number = -1;
	int delayed_start = 0;
	int delayed_extent = 0;
	int delayed_skipfirst = 0;
	int delayed_next = 0;
	char *delayed_buf = NULL;
	short status;
	int log2blocksize = LOG2_EXT2_BLOCK_SIZE(node->data);
	int blocksize = 1 << (log2blocksize + DISK_SECTOR_BITS);
	unsigned int filesize = node->inode.size;

	/* Adjust len so it we can't read past the end of the file. */
	if (len > filesize)
		len = filesize;

	blockcnt = ((len + pos) + blocksize - 1) / blocksize;

	for (i = pos / blocksize; i < blockcnt; i++) {
		int blknr;
		int blockoff = pos % blocksize;
		int blockend = blocksize;
		int skipfirst = 0;
		blknr = read_allocated_block1(node, i);
		if (blknr < 0)
			return -1;

		blknr = blknr << log2blocksize;

		/* Last block.  */
		if (i == blockcnt - 1) {
			blockend = (len + pos) % blocksize;

			/* The last portion is exactly blocksize. */
			if (!blockend)
				blockend = blocksize;
		}

		/* First block. */
		if (i == pos / blocksize) {
			skipfirst = blockoff;
			blockend -= skipfirst;
		}
		if (blknr) {
			int status;

			if (previous_block_number != -1) {
				if (delayed_next == blknr) {
					delayed_extent += blockend;
					delayed_next += blockend >> SECTOR_BITS;
				} else {	/* spill */
					status = vfs_devread(fs->dev_desc,delayed_start,
							delayed_skipfirst,
							delayed_extent,
							delayed_buf);
					if (status == 0)
						return -1;
					previous_block_number = blknr;
					delayed_start = blknr;
					delayed_extent = blockend;
					delayed_skipfirst = skipfirst;
					delayed_buf = buf;
					delayed_next = blknr +
						(blockend >> SECTOR_BITS);
				}
			} else {
				previous_block_number = blknr;
				delayed_start = blknr;
				delayed_extent = blockend;
				delayed_skipfirst = skipfirst;
				delayed_buf = buf;
				delayed_next = blknr +
					(blockend >> SECTOR_BITS);
			}
		} else {
			if (previous_block_number != -1) {
				/* spill */
				status = vfs_devread(fs->dev_desc,delayed_start,
						delayed_skipfirst,
						delayed_extent,
						delayed_buf);
				if (status == 0)
					return -1;
				previous_block_number = -1;
			}
			memset(buf, 0, blocksize - skipfirst);
		}
		buf += blocksize - skipfirst;
	}
	if (previous_block_number != -1) {
		/* spill */
		status = vfs_devread(fs->dev_desc,delayed_start,
				delayed_skipfirst, delayed_extent,
				delayed_buf);
		if (status == 0)
			return -1;
		previous_block_number = -1;
	}

	return len;
}

static struct ext4_extent_header *ext4fs_get_extent_block
	(struct ext2_data *data, char *buf,
		struct ext4_extent_header *ext_block,
		uint32_t fileblock, int log2_blksz)
{
	struct ext4_extent_idx *index;
	unsigned long long block;
	struct ext_filesystem *fs = get_fs();
	int i;

	while (1) {
		index = (struct ext4_extent_idx *)(ext_block + 1);

		if (ext_block->eh_magic != EXT4_EXT_MAGIC)
			return 0;

		if (ext_block->eh_depth == 0)
			return ext_block;
		i = -1;
		do {
			i++;
			if (i >= ext_block->eh_entries)
				break;
		} while (fileblock > index[i].ei_block);

		if (--i < 0)
			return 0;

		block = index[i].ei_leaf_hi;
		block = (block << 32) + index[i].ei_leaf_lo;

		if (vfs_devread(fs->dev_desc,block << log2_blksz, 0, fs->blksz, buf))
			ext_block = (struct ext4_extent_header *)buf;
		else
			return 0;
	}
}

static int ext4fs_blockgroup
	(struct ext2_data *data, int group, struct ext2_block_group *blkgrp)
{
	long int blkno;
	unsigned int blkoff, desc_per_blk;
	struct ext_filesystem *fs = get_fs();

	desc_per_blk = EXT2_BLOCK_SIZE(data) / sizeof(struct ext2_block_group);

	blkno = (data->sblock.first_data_block) + 1 +
			group / desc_per_blk;
	blkoff = (group % desc_per_blk) * sizeof(struct ext2_block_group);

#ifdef DEBUG
	printf("ext4fs read %d group descriptor (blkno %ld blkoff %u)\n",
	      group, blkno, blkoff);
#endif

	return vfs_devread(fs->dev_desc,blkno << LOG2_EXT2_BLOCK_SIZE(data),
			      blkoff, sizeof(struct ext2_block_group),
			      (char *)blkgrp);
}

int ext4fs_read_inode(struct ext_filesystem *fs, struct ext2_data *data, int ino, struct ext2_inode *inode)
{
	struct ext2_block_group blkgrp;
	struct ext2_sblock *sblock = &data->sblock;
	int inodes_per_block, status;
	long int blkno;
	unsigned int blkoff;

	/* It is easier to calculate if the first inode is 0. */
	ino--;
	status = ext4fs_blockgroup(data, ino /
				   (sblock->inodes_per_group), &blkgrp);
	if (status == 0)
		return 0;

	inodes_per_block = EXT2_BLOCK_SIZE(data) / fs->inodesz;
	blkno = (blkgrp.inode_table_id) +
	    (ino % (sblock->inodes_per_group)) / inodes_per_block;
	blkoff = (ino % inodes_per_block) * fs->inodesz;
	/* Read the inode. */
	status = vfs_devread(fs->dev_desc,blkno << LOG2_EXT2_BLOCK_SIZE(data), blkoff,
				sizeof(struct ext2_inode), (char *)inode);
	if (status == 0)
		return 0;

	return 1;
}

static int ext4fs_umount(struct filesys_spec *fs_descr)
{
	struct ext_filesystem *fs = &fs_descr->extfs;
	free(fs);
	return 0;
}


typedef int (*dir_iterate_func_t)(void *, const char *, struct xstat *, int is_dir);
static int ext4fs_iterate_dir(struct ext_filesystem *fs, struct ext2fs_node *dir, char *name,
				struct ext2fs_node **fnode, int *ftype, dir_iterate_func_t dir_func, void * user_data)
{
	unsigned int fpos = 0;
	int status, got = 0;
    struct xstat st;
	struct ext2fs_node *diro = (struct ext2fs_node *) dir;

#ifdef DEBUG
	if (name != NULL)
		printf("Iterate dir %s\n", name);
#endif /* of DEBUG */
	if (!diro->inode_read) {
		status = ext4fs_read_inode(fs, diro->data, diro->ino, &diro->inode);
		if (status == 0)
			return 0;
	}
	/* Search the file.  */
	while (!got && fpos < diro->inode.size) {
		struct ext2_dirent dirent;

		status = ext4fs_read_file(fs, diro, fpos,
					   sizeof(struct ext2_dirent),
					   (char *) &dirent);
		if (status < 1)
			return 0;

		if (dirent.namelen != 0) {
			char filename[dirent.namelen + 1];
			struct ext2fs_node *fdiro;
			int type = FILETYPE_UNKNOWN;

			status = ext4fs_read_file(fs, diro,
						  fpos +
						  sizeof(struct ext2_dirent),
						  dirent.namelen, filename);
			if (status < 1)
				return 0;

			fdiro = zalloc(sizeof(struct ext2fs_node));
			if (!fdiro)
				return 0;

			fdiro->data = diro->data;
			fdiro->ino  = dirent.inode;

			filename[dirent.namelen] = '\0';

			if (dirent.filetype != FILETYPE_UNKNOWN) {
				fdiro->inode_read = 0;

				if (dirent.filetype == FILETYPE_DIRECTORY)
					type = FILETYPE_DIRECTORY;
				else if (dirent.filetype == FILETYPE_SYMLINK)
					type = FILETYPE_SYMLINK;
				else if (dirent.filetype == FILETYPE_REG)
					type = FILETYPE_REG;
			} else {
				status = ext4fs_read_inode(fs, diro->data,

							   (dirent.inode),
							   &fdiro->inode);
				if (status == 0) {
					free(fdiro);
					return 0;
				}
				fdiro->inode_read = 1;

				if (((fdiro->inode.mode) &
				     FILETYPE_INO_MASK) ==
				    FILETYPE_INO_DIRECTORY) {
					type = FILETYPE_DIRECTORY;
				} else if (((fdiro->inode.mode)
					    & FILETYPE_INO_MASK) ==
					   FILETYPE_INO_SYMLINK) {
					type = FILETYPE_SYMLINK;
				} else if (((fdiro->inode.mode)
					    & FILETYPE_INO_MASK) ==
					   FILETYPE_INO_REG) {
					type = FILETYPE_REG;
				}
			}
#ifdef DEBUG
			printf("iterate >%s<\n", filename);
#endif /* of DEBUG */
			if ((name != NULL) && (fnode != NULL)
			    && (ftype != NULL)) {
				if (strcmp(filename, name) == 0) {
					*ftype = type;
					*fnode = fdiro;
					return 1;
				}
			} else {
				if (fdiro->inode_read == 0) {
					status = ext4fs_read_inode(fs, diro->data,
								 (
								 dirent.inode),
								 &fdiro->inode);
					if (status == 0) {
						free(fdiro);
						return 0;
					}
					fdiro->inode_read = 1;
				}
                if (dir_func) {
                    st.atime = fdiro->inode.atime;
                    st.ctime = fdiro->inode.ctime;
                    st.dtime = fdiro->inode.dtime;
                    st.mode  = fdiro->inode.mode;
                    st.size  = fdiro->inode.size;
                    st.size_high = fdiro->inode.dir_acl;
                    got = dir_func(user_data, filename, &st, type == FILETYPE_DIRECTORY ? 1: 0);
                }
			}
			free(fdiro);
		}
		fpos += dirent.direntlen;
	}
	return 0;
}

static char *ext4fs_read_symlink(struct ext_filesystem *fs, struct ext2fs_node *node)
{
	char *symlink;
	struct ext2fs_node *diro = node;
	int status;

	if (!diro->inode_read) {
		status = ext4fs_read_inode(fs, diro->data, diro->ino, &diro->inode);
		if (status == 0)
			return 0;
	}
	symlink = zalloc((diro->inode.size) + 1);
	if (!symlink)
		return 0;

	if ((diro->inode.size) <= 60) {
		strncpy(symlink, diro->inode.b.symlink,
			 (diro->inode.size));
	} else {
		status = ext4fs_read_file(fs, diro, 0,
					   (diro->inode.size),
					   symlink);
		if (status == 0) {
			free(symlink);
			return 0;
		}
	}
	symlink[(diro->inode.size)] = '\0';
	return symlink;
}

static int ext4fs_find_file1(const char *currpath,
			     struct ext2fs_node *currroot,
			     struct ext2fs_node **currfound, int *foundtype, int *symlinknest)
{
	char fpath[strlen(currpath) + 1];
	char *name = fpath;
	char *next;
	int status;
	int type = FILETYPE_DIRECTORY;
	struct ext2fs_node *currnode = currroot;
	struct ext2fs_node *oldnode = currroot;
	struct ext_filesystem *fs = get_fs();

	strncpy(fpath, currpath, strlen(currpath) + 1);

	/* Remove all leading slashes. */
	while (*name == '/')
		name++;

	if (!*name) {
		*currfound = currnode;
		return 1;
	}

	for (;;) {
		int found;

		/* Extract the actual part from the pathname. */
		next = strchr(name, '/');
		if (next) {
			/* Remove all leading slashes. */
			while (*next == '/')
				*(next++) = '\0';
		}

		if (type != FILETYPE_DIRECTORY) {
			ext4fs_free_node(currnode, currroot);
			return 0;
		}

		oldnode = currnode;

		/* Iterate over the directory. */
		found = ext4fs_iterate_dir(fs, currnode, name, &currnode, &type, NULL, NULL);
		if (found == 0)
			return 0;

		if (found == -1)
			break;

		/* Read in the symlink and follow it. */
		if (type == FILETYPE_SYMLINK) {
			char *symlink;

			/* Test if the symlink does not loop. */
			*symlinknest = *symlinknest + 1;
			if (*symlinknest == 8) {
				ext4fs_free_node(currnode, currroot);
				ext4fs_free_node(oldnode, currroot);
				return 0;
			}

			symlink = ext4fs_read_symlink(fs, currnode);
			ext4fs_free_node(currnode, currroot);

			if (!symlink) {
				ext4fs_free_node(oldnode, currroot);
				return 0;
			}

			printf("Got symlink >%s<\n", symlink);

			if (symlink[0] == '/') {
				ext4fs_free_node(oldnode, currroot);
				oldnode = &fs->ext4fs_root->diropen;
			}

			/* Lookup the node the symlink points to. */
			status = ext4fs_find_file1(symlink, oldnode,
						    &currnode, &type, symlinknest);

			free(symlink);

			if (status == 0) {
				ext4fs_free_node(oldnode, currroot);
				return 0;
			}
		}

		ext4fs_free_node(oldnode, currroot);

		/* Found the node! */
		if (!next || *next == '\0') {
			*currfound = currnode;
			*foundtype = type;
			return 1;
		}
		name = next;
	}
	return -1;
}

int ext4fs_find_file(const char *path, struct ext2fs_node *rootnode,
	struct ext2fs_node **foundnode, int expecttype)
{
	int status;
	int foundtype = FILETYPE_DIRECTORY;
	int symlinknest = 0;

	if (!path)
		return 0;

	status = ext4fs_find_file1(path, rootnode, foundnode, &foundtype, &symlinknest);
	if (status == 0)
		return 0;

	/* Check if the node that was found was of the expected type. */
	if ((expecttype == FILETYPE_REG) && (foundtype != expecttype))
		return 0;
	else if ((expecttype == FILETYPE_DIRECTORY)
		   && (foundtype != expecttype))
		return 0;

	return 1;
}

static int ext4fs_file_entry_read(struct file_entry *file, struct filesys_spec *fsys, char *buf, unsigned len)
{
	struct ext2fs_file_entry *filp = (struct ext2fs_file_entry *)file;

	if (!fsys || !filp)
		return 0;
	return ext4fs_read_file(&fsys->extfs, filp->ext4fs_file, 0, len, buf);
}

static int ext4fs_file_entry_close(struct file_entry *filp, struct filesys_spec *fsys)
{
	struct ext2fs_file_entry *file = (struct ext2fs_file_entry *)filp;
	struct ext_filesystem *fs = &fsys->extfs;

	ext4fs_free_node(file->ext4fs_file, &fs->ext4fs_root->diropen);
	free(file);
	return 0;
}

struct ext2fs_file_entry *
__alloc_ext2fs_entry(struct ext2fs_node * node)
{
	struct ext2fs_file_entry *filp = calloc(1, sizeof *filp);

	filp->base.read = ext4fs_file_entry_read;
	filp->base.close = ext4fs_file_entry_close;
	filp->ext4fs_file = node;
	return filp;
}

static struct file_entry *
ext4fs_open(struct filesys_spec *fsys, const char *filename)
{
	struct ext2fs_node *fdiro = NULL;
	int status;
	int len;
	struct ext2fs_file_entry *filp;
	struct ext_filesystem *fs = &fsys->extfs;

	status = ext4fs_find_file(filename, &fs->ext4fs_root->diropen, &fdiro,
				  FILETYPE_REG);
	if (status == 0)
		goto fail;

	if (!fdiro->inode_read) {
		status = ext4fs_read_inode(fs, fdiro->data, fdiro->ino,
				&fdiro->inode);
		if (status == 0)
			goto fail;
	}
	len = (fdiro->inode.size);
	filp = __alloc_ext2fs_entry(fdiro);

	return &filp->base;
fail:
	ext4fs_free_node(fdiro, &fs->ext4fs_root->diropen);

	return NULL;
}

static struct filesys_spec *ext4fs_mount(part_descr_t part)
{
	struct filesys_spec *fs_descr = NULL;
	struct ext2_data *data;
	int status;
	struct ext_filesystem *fs;

	fs_descr = zalloc(sizeof *fs_descr);
	if (fs_descr == NULL) {
		fprintf(stderr, "out of memory, mount failed.\n");
		return NULL;
	}
	fs = &fs_descr->extfs;
	data = fs->ext4fs_root;

	fs->dev_desc = part;

	/* Read the superblock. */
	status = vfs_devread(fs->dev_desc,1 * 2, 0, sizeof(struct ext2_sblock),
				(char *)&data->sblock);

	if (status == 0)
		goto fail;

	/* Make sure this is an ext2 filesystem. */
	if ((data->sblock.magic) != EXT2_MAGIC)
		goto fail;

	if ((data->sblock.revision_level == 0))
		fs->inodesz = 128;
	else
		fs->inodesz = (data->sblock.inode_size);

	printf("EXT2 rev %d, inode_size %d\n",(data->sblock.revision_level), fs->inodesz);

	data->diropen.data = data;
	data->diropen.ino = 2;
	data->diropen.inode_read = 1;
	data->inode = &data->diropen.inode;

	memcpy(get_fs(), fs, sizeof *fs);
	status = ext4fs_read_inode(fs, data, 2, data->inode);
	if (status == 0)
		goto fail;
	memcpy(get_fs(), fs, sizeof *fs);
	return fs_descr;
fail:
	printf("Failed to mount ext2 filesystem...\n");
	free(fs_descr);
	return NULL;
}


static int ext4fs_list_files(struct filesys_spec *fsys, const char *dirname, dir_iterate_func_t func, void *data)
{
	struct ext2fs_node *dirnode;
	int status;
	struct ext_filesystem *fs = &fsys->extfs;

	if (dirname == NULL)
		return -1;

	status = ext4fs_find_file(dirname, &fs->ext4fs_root->diropen, &dirnode,
			FILETYPE_DIRECTORY);
	if (status != 1) {
		printf("** Can not find directory. [%s] **\n", dirname);
		return -1;
	}
	ext4fs_iterate_dir(fs, dirnode, NULL, NULL, NULL, func, data);
	ext4fs_free_node(dirnode, &fs->ext4fs_root->diropen);

	return 0;
}

struct filesys_operations extfs_operations = {
	.mount  = ext4fs_mount,
	.dir_iterate = ext4fs_list_files,
	.umount = ext4fs_umount,
	.open = ext4fs_open,
};

