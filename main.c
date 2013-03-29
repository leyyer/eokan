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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "disk.h"

static void print_usage()
{
	printf("usage: xokan [-h, -d] disk_path\n");
	printf("    -h, --help: print this message\n");
	printf("    -d, --disk: disk type [vmdk, physical]\n");
	printf("    -p, --part: disk partition number, 1, 2, 3 ...\n");
	printf("    disk_path: is vmdk file path or physical disk path. like:\n\t(\\\\.\\PhysicalDrive0 or \\\\.\\PhysicalDrive1, ...)\n");
}

int main(int argc, char *argv[])
{
	int c, retval = 0;
	disk_descr_t disk;
	int part = 1;
	part_descr_t partition;
	const char *disk_type = "vmdk";
	const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"disk", required_argument, NULL, 'd'},
		{"part", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0},
	};

	while ((c = getopt_long(argc, argv, "hd:p:", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				print_usage();
				exit(0);
			case 'd':
				disk_type = optarg;
				break;
			case 'p':
				part = atoi(optarg);
				break;
		};
	}

	argv += optind;
	argc -= optind;
	if (argc <= 0) {
		print_usage();
		exit(-1);
	}
	disk = disk_open(disk_type, argv[0], DISK_FLAG_READ);
	if (!disk) {
		printf("can't open disk: %s %s\n", disk_type, argv[0]);
		exit(-1);
	}
	partition = disk_get_partition(disk, part);
	if (!partition) {
		printf("can't find partition: %d\n", part);
		disk_close(disk);
		exit(-2);
	}
    printf("parition: %d, offset: %I64u, length: %I64u\n", part, partition->off, partition->length);
    if (ext4fs_mount(partition) < 0 ) {
        retval = -1;
        goto skip;
    }
    ext4fs_ls("/");
	eokan_main();
    ext4fs_close();
 skip:
	part_close(partition);
	disk_close(disk);
	return 0;
}

