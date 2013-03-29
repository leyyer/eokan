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

#ifndef __EXT4_JRNL__
#define __EXT4_JRNL__

#define EXT2_JOURNAL_INO		8	/* Journal inode */
#define EXT2_JOURNAL_SUPERBLOCK	0	/* Journal  Superblock number */

#define JBD2_FEATURE_COMPAT_CHECKSUM	0x00000001
#define EXT3_JOURNAL_MAGIC_NUMBER	0xc03b3998U
#define TRANSACTION_RUNNING		1
#define TRANSACTION_COMPLETE		0
#define EXT3_FEATURE_INCOMPAT_RECOVER	0x0004	/* Needs recovery */
#define EXT3_JOURNAL_DESCRIPTOR_BLOCK	1
#define EXT3_JOURNAL_COMMIT_BLOCK	2
#define EXT3_JOURNAL_SUPERBLOCK_V1	3
#define EXT3_JOURNAL_SUPERBLOCK_V2	4
#define EXT3_JOURNAL_REVOKE_BLOCK	5
#define EXT3_JOURNAL_FLAG_ESCAPE	1
#define EXT3_JOURNAL_FLAG_SAME_UUID	2
#define EXT3_JOURNAL_FLAG_DELETED	4
#define EXT3_JOURNAL_FLAG_LAST_TAG	8

/* Maximum entries in 1 journal transaction */
#define MAX_JOURNAL_ENTRIES 100
struct journal_log {
	char *buf;
	int blknr;
};

struct dirty_blocks {
	char *buf;
	int blknr;
};

/* Standard header for all descriptor blocks: */
struct journal_header_t {
	uint32_t h_magic;
	uint32_t h_blocktype;
	uint32_t h_sequence;
};

/* The journal superblock.  All fields are in big-endian byte order. */
struct journal_superblock_t {
	/* 0x0000 */
	struct journal_header_t s_header;

	/* Static information describing the journal */
	uint32_t s_blocksize;	/* journal device blocksize */
	uint32_t s_maxlen;		/* total blocks in journal file */
	uint32_t s_first;		/* first block of log information */

	/* Dynamic information describing the current state of the log */
	uint32_t s_sequence;	/* first commit ID expected in log */
	uint32_t s_start;		/* blocknr of start of log */

	/* Error value, as set by journal_abort(). */
	int32_t s_errno;

	/* Remaining fields are only valid in a version-2 superblock */
	uint32_t s_feature_compat;	/* compatible feature set */
	uint32_t s_feature_incompat;	/* incompatible feature set */
	uint32_t s_feature_ro_compat;	/* readonly-compatible feature set */
	/* 0x0030 */
	uint8_t s_uuid[16];	/* 128-bit uuid for journal */

	/* 0x0040 */
	uint32_t s_nr_users;	/* Nr of filesystems sharing log */

	uint32_t s_dynsuper;	/* Blocknr of dynamic superblock copy */

	/* 0x0048 */
	uint32_t s_max_transaction;	/* Limit of journal blocks per trans. */
	uint32_t s_max_trans_data;	/* Limit of data blocks per trans. */

	/* 0x0050 */
	uint32_t s_padding[44];

	/* 0x0100 */
	uint8_t s_users[16 * 48];	/* ids of all fs'es sharing the log */
	/* 0x0400 */
} ;

struct ext3_journal_block_tag {
	uint32_t block;
	uint32_t flags;
};

struct journal_revoke_header_t {
	struct journal_header_t r_header;
	int r_count;		/* Count of bytes used in the block */
};

struct revoke_blk_list {
	char *content;		/* revoke block itself */
	struct revoke_blk_list *next;
};

extern struct ext2_data *ext4fs_root;

int ext4fs_init_journal(void);
int ext4fs_log_gdt(char *gd_table);
int ext4fs_check_journal_state(int recovery_flag);
int ext4fs_log_journal(char *journal_buffer, long int blknr);
int ext4fs_put_metadata(char *metadata_buffer, long int blknr);
void ext4fs_update_journal(void);
void ext4fs_dump_metadata(void);
void ext4fs_push_revoke_blk(char *buffer);
void ext4fs_free_journal(void);
void ext4fs_free_revoke_blks(void);
#endif
