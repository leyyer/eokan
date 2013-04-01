/*
 * Copyright (c) 2013, Renyi su <surenyi@gmail.com> All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer. Redistributions in binary form must
 * reproduce the above copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the distribution.

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
#include <getopt.h>
#include "disk.h"
#include "fs.h"

#define EOKAN_SVCNAME TEXT("eokan_svc")

static int eokan_svc_install(void)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if(!GetModuleFileName( NULL, szPath, MAX_PATH )) {
        printf("Cannot install service (%d)\n", GetLastError());
        return -1;
    }

    /* Get a handle to the SCM database. */
    schSCManager = OpenSCManager(
        NULL,                    /* local computer */
        NULL,                    /* ServicesActive database */
        SC_MANAGER_ALL_ACCESS);  /* full access rights */

    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return -1;
    }

    /* Create the service */
    schService = CreateService(
        schSCManager,              // SCM database
        EOKAN_SVCNAME,             // name of service
        EOKAN_SVCNAME,             // service name to display
        SERVICE_ALL_ACCESS,        // desired access
        SERVICE_WIN32_OWN_PROCESS, // service type
        SERVICE_DEMAND_START,      // start type
        SERVICE_ERROR_NORMAL,      // error control type
        szPath,                    // path to service's binary
        NULL,                      // no load ordering group
        NULL,                      // no tag identifier
        NULL,                      // no dependencies
        NULL,                      // LocalSystem account
        NULL);                     // no password

    if (schService == NULL){
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return -2;
    }
    else printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
	return 0;
}

int __stdcall eokan_svc_remove(void)
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS ssStatus;

    // Get a handle to the SCM database.
    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database
        SC_MANAGER_ALL_ACCESS);  // full access rights

    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return -1;
    }

    schService = OpenService(
        schSCManager,       // SCM database
        EOKAN_SVCNAME,      // name of service
        DELETE);            // need delete access

    if (schService == NULL){
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return -2;
    }

    // Delete the service.
    if (! DeleteService(schService)) {
        printf("DeleteService failed (%d)\n", GetLastError());
    }
    else printf("Service deleted successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
	return 0;
}

static void print_usage()
{
	printf("usage: xokan [-h, -d] disk_path\n");
	printf("    -h, --help: print this message\n");
	printf("    -i, --install: install eokan_svc service.\n");
	printf("    -r, --remove: remove eokan_svc service.\n");
	printf("    -d, --disk: disk type [vmdk, physical]\n");
	printf("    -p, --part: disk partition number, 1, 2, 3 ...\n");
	printf("    disk_path: is vmdk file path or physical disk path. like:\n\t(\\\\.\\PhysicalDrive0 or \\\\.\\PhysicalDrive1, ...)\n");
}

int main(int argc, char *argv[])
{
	int c, retval = 0;
	disk_descr_t disk;
	filesys_t  fs;
	int part = 1;
	part_descr_t partition;
	const char *disk_type = "vmdk";
	int iflag = 0, rflag = 0;
	const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"disk", required_argument, NULL, 'd'},
		{"part", required_argument, NULL, 'p'},
		{"install", no_argument, NULL, 'i'},
		{"remove", no_argument, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "hird:p:", long_options, NULL)) != -1) {
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
			case 'i':
				iflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
		};
	}

	argv += optind;
	argc -= optind;
	/* Install the service */
	if (iflag) {
		return eokan_svc_install();
	}
	if (rflag) {
		return eokan_svc_remove();
	}
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
	if ((fs = vfs_mount(partition)) == NULL) {
		retval = -1;
		goto skip;
	}
	eokan_main(fs);
    vfs_umount(fs);
 skip:
	part_close(partition);
	disk_close(disk);
	return 0;
}

