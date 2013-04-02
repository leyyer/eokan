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
#include "util.h"

#define EOKAN_SVCNAME TEXT("eokan_svc")
static	SERVICE_STATUS_HANDLE   gSvcStatusHandle;
static	SERVICE_STATUS			gSvcStatus;
static  HANDLE                  ghSvcStopEvent = NULL;

static int eokan_svc_install(void)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	TCHAR szPath[MAX_PATH];

	if(!GetModuleFileName( NULL, szPath, MAX_PATH )) {
		printf("Cannot install service (%lu)\n", GetLastError());
		return -1;
	}

	/* Get a handle to the SCM database. */
	schSCManager = OpenSCManager(
			NULL,                    /* local computer */
			NULL,                    /* ServicesActive database */
			SC_MANAGER_ALL_ACCESS);  /* full access rights */

	if (NULL == schSCManager) {
		printf("OpenSCManager failed (%lu)\n", GetLastError());
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
		printf("CreateService failed (%lu)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return -2;
	} else {
		printf("Service installed successfully\n");
	}

	/* start service now */
	if (!StartService(schService, 0, NULL)) {
		fprintf(stderr, "start service failed.\n");
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return 0;
}

static BOOL __stdcall stop_depends(SC_HANDLE schSCManager, SC_HANDLE schService)
{
	DWORD i;
	DWORD dwBytesNeeded;
	DWORD dwCount;
	LPENUM_SERVICE_STATUS   lpDependencies = NULL;
	ENUM_SERVICE_STATUS     ess;
	SC_HANDLE               hDepService;
	SERVICE_STATUS_PROCESS  ssp;

	DWORD dwStartTime = GetTickCount();
	DWORD dwTimeout = 30000; // 30-second time-out

	// Pass a zero-length buffer to get the required buffer size.
	if ( EnumDependentServices( schService, SERVICE_ACTIVE,
				lpDependencies, 0, &dwBytesNeeded, &dwCount ) ) {
		// If the Enum call succeeds, then there are no dependent
		// services, so do nothing.
		return TRUE;
	} else {
		if ( GetLastError() != ERROR_MORE_DATA )
			return FALSE; // Unexpected error

		// Allocate a buffer for the dependencies.
		lpDependencies = (LPENUM_SERVICE_STATUS) HeapAlloc(
				GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded );

		if ( !lpDependencies )
			return FALSE;

		// Enumerate the dependencies.
		if ( !EnumDependentServices( schService, SERVICE_ACTIVE,
					lpDependencies, dwBytesNeeded, &dwBytesNeeded,
					&dwCount ) )
			return FALSE;

		for ( i = 0; i < dwCount; i++ ) {
			ess = *(lpDependencies + i);
			// Open the service.
			hDepService = OpenService( schSCManager,
					ess.lpServiceName,
					SERVICE_STOP | SERVICE_QUERY_STATUS );

			if ( !hDepService )
				return FALSE;

			// Send a stop code.
			if ( !ControlService( hDepService,
						SERVICE_CONTROL_STOP,
						(LPSERVICE_STATUS) &ssp ) )
				return FALSE;

			// Wait for the service to stop.
			while ( ssp.dwCurrentState != SERVICE_STOPPED )
			{
				Sleep( ssp.dwWaitHint );
				if ( !QueryServiceStatusEx(
							hDepService,
							SC_STATUS_PROCESS_INFO,
							(LPBYTE)&ssp,
							sizeof(SERVICE_STATUS_PROCESS),
							&dwBytesNeeded ) )
					return FALSE;

				if ( ssp.dwCurrentState == SERVICE_STOPPED )
					break;

				if ( GetTickCount() - dwStartTime > dwTimeout )
					return FALSE;
			}
		}
	}
	if (lpDependencies)
		HeapFree(GetProcessHeap(), 0, lpDependencies );
	return TRUE;
}

static void __stdcall do_stop_eokansvc(SC_HANDLE schSCManager, SC_HANDLE schService)
{
    SERVICE_STATUS_PROCESS ssp;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;
    DWORD dwTimeout = 30000; // 30-second time-out
    DWORD dwWaitTime;

    // Make sure the service is not already stopped.
    if ( !QueryServiceStatusEx(
            schService,
            SC_STATUS_PROCESS_INFO,
            (LPBYTE)&ssp,
            sizeof(SERVICE_STATUS_PROCESS),
            &dwBytesNeeded ) )
    {
        printf("QueryServiceStatusEx failed (%lu)\n", GetLastError());
        goto stop_cleanup;
    }

    if ( ssp.dwCurrentState == SERVICE_STOPPED )
    {
        printf("Service is already stopped.\n");
        goto stop_cleanup;
    }

    // If a stop is pending, wait for it.

    while ( ssp.dwCurrentState == SERVICE_STOP_PENDING )
    {
        printf("Service stop pending...\n");

        // Do not wait longer than the wait hint. A good interval is
        // one-tenth of the wait hint but not less than 1 second
        // and not more than 10 seconds.

        dwWaitTime = ssp.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        if ( !QueryServiceStatusEx(
                 schService,
                 SC_STATUS_PROCESS_INFO,
                 (LPBYTE)&ssp,
                 sizeof(SERVICE_STATUS_PROCESS),
                 &dwBytesNeeded ) )
        {
            printf("QueryServiceStatusEx failed (%lu)\n", GetLastError());
            goto stop_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_STOPPED )
        {
            printf("Service stopped successfully.\n");
            goto stop_cleanup;
        }

        if ( GetTickCount() - dwStartTime > dwTimeout )
        {
            printf("Service stop timed out.\n");
            goto stop_cleanup;
        }
    }

    // If the service is running, dependencies must be stopped first.

    stop_depends(schSCManager, schService);

    // Send a stop code to the service.
    if ( !ControlService(
            schService,
            SERVICE_CONTROL_STOP,
            (LPSERVICE_STATUS) &ssp ) ) {
        printf( "ControlService failed (%lu)\n", GetLastError() );
        goto stop_cleanup;
    }

    // Wait for the service to stop.

    while ( ssp.dwCurrentState != SERVICE_STOPPED ) {
        Sleep( ssp.dwWaitHint );
        if ( !QueryServiceStatusEx(
                schService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ssp,
                sizeof(SERVICE_STATUS_PROCESS),
                &dwBytesNeeded ) ) {
            printf( "QueryServiceStatusEx failed (%lu)\n", GetLastError() );
            goto stop_cleanup;
        }

        if ( ssp.dwCurrentState == SERVICE_STOPPED )
            break;

        if ( GetTickCount() - dwStartTime > dwTimeout ) {
            printf( "Wait timed out\n" );
            goto stop_cleanup;
        }
    }
    printf("Service stopped successfully\n");
stop_cleanup:
	return;
}


static int __stdcall eokan_svc_remove(void)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	// Get a handle to the SCM database.
	schSCManager = OpenSCManager(
			NULL,                    // local computer
			NULL,                    // ServicesActive database
			SC_MANAGER_ALL_ACCESS);  // full access rights

	if (NULL == schSCManager) {
		printf("OpenSCManager failed (%lu)\n", GetLastError());
		return -1;
	}

	schService = OpenService(
			schSCManager,       // SCM database
			EOKAN_SVCNAME,      // name of service
			DELETE);            // need delete access

	if (schService == NULL){
		printf("OpenService failed (%lu)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return -2;
	}

	do_stop_eokansvc(schSCManager, schService);

	// Delete the service.
	if (! DeleteService(schService)) {
		printf("DeleteService failed (%lu)\n", GetLastError());
	}
	else printf("Service deleted successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return 0;
}

static void svc_report_event(LPTSTR szFunction)
{
	fwprintf(stderr, L"function %s failed.\n", szFunction);
}

static void report_svc_status(
		SERVICE_STATUS_HANDLE   gSvcStatusHandle,
		SERVICE_STATUS			*gSvcStatus,
		DWORD dwCurrentState,
		DWORD dwWin32ExitCode,
		DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.
	gSvcStatus->dwCurrentState = dwCurrentState;
	gSvcStatus->dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus->dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus->dwControlsAccepted = 0;
	else gSvcStatus->dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ( (dwCurrentState == SERVICE_RUNNING) ||
			(dwCurrentState == SERVICE_STOPPED) )
		gSvcStatus->dwCheckPoint = 0;
	else gSvcStatus->dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(gSvcStatusHandle, gSvcStatus );
}

static void WINAPI svcctl_handler( DWORD dwCtrl )
{
	// Handle the requested control code.

	switch(dwCtrl) {
		case SERVICE_CONTROL_STOP:
			report_svc_status(gSvcStatusHandle, &gSvcStatus, SERVICE_STOP_PENDING, NO_ERROR, 0);
			// Signal the service to stop.
			SetEvent(ghSvcStopEvent);
			report_svc_status(gSvcStatusHandle, &gSvcStatus, gSvcStatus.dwCurrentState, NO_ERROR, 0);
			return;
		case SERVICE_CONTROL_INTERROGATE:
			break;
		default:
			break;
	}
}

static int find_valid_drive(int from)
{
	DWORD dflag;
	int x, c;

	dflag = GetLogicalDrives();
	for (x = from - 'A'; x < 32 && (dflag & (1 << x)); ++x) {
		++x;
	}
	if (x >= 32) {
		return 0;
	}
	c = 'A' + x;
	printf("found: drive %c\n", c);
	return c;
}

static void create_eokan_process(const char *path, int part)
{
	char szPath[MAX_PATH];
	STARTUPINFOA si;
    PROCESS_INFORMATION pi;
	char cmdline[MAX_PATH*2];

    ZeroMemory(&si, sizeof(si) );

    si.cb = sizeof(si);

    ZeroMemory( &pi, sizeof(pi) );

	if(!GetModuleFileNameA( NULL, szPath, MAX_PATH )) {
		printf("can't find module (%lu)\n", GetLastError());
		return;
	}
	snprintf(cmdline, sizeof cmdline, "%s -p %d %s", szPath, part, path);
	if (!CreateProcessA(szPath,cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        printf( "create process failed (%lu).\n", GetLastError());
    }
}

static void mount_phydisk_parts(void)
{
	disk_descr_t dk;
	filesys_t    fs;
	part_descr_t partition;
	const char *disk_type = "physical";
	int n, k;
	char path[MAX_PATH];

	if (eokan_load(0) < 0)
		return;
	for (n = 0; n < 4; ++n) {
		snprintf(path, sizeof path, "\\\\.\\PhysicalDrive%d", n);
		dk = disk_open(disk_type, path, DISK_FLAG_READ);
		if (!dk)
			continue;
		for (k = 0; k < 16 ; ++k) {
			partition = disk_get_partition(dk,k+1);
			if (!partition)
				break;
			fs = vfs_mount(partition);
			if (fs) {
				vfs_umount(fs);
				create_eokan_process(path, k + 1);
			}
			part_close(partition);
		}
		disk_close(dk);
	}
	eokan_unload();
}

static void umount_phydisk_parts(void)
{
	DWORD dflag;
	int x;

	if (eokan_load(0) < 0)
		return;
	dflag = GetLogicalDrives();
	for (x = 'C' - 'A'; x < 32; ++x) {
		if (dflag & (1 << x)) {
			eokan_umount(x + 'A');
		}
	}
	eokan_unload();
}

static VOID WINAPI eokan_svc_main( DWORD dwArgc, LPTSTR *lpszArgv )
{
	// Register the handler function for the service
	gSvcStatusHandle = RegisterServiceCtrlHandler( EOKAN_SVCNAME, svcctl_handler);

	if( !gSvcStatusHandle ) {
		svc_report_event(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwServiceSpecificExitCode = 0;

	// Report initial status to the SCM
	report_svc_status(gSvcStatusHandle, &gSvcStatus, SERVICE_START_PENDING, NO_ERROR, 3000 );

	ghSvcStopEvent = CreateEvent( NULL,  TRUE, FALSE, NULL);

	if (ghSvcStopEvent == NULL) {
		report_svc_status(gSvcStatusHandle, &gSvcStatus, SERVICE_STOPPED, NO_ERROR, 0 );
		return;
	}

	mount_phydisk_parts();

	report_svc_status(gSvcStatusHandle, &gSvcStatus, SERVICE_RUNNING, NO_ERROR, 0 );

	/* Report running status when initialization is complete. */
	/* eokan main */
	while(1) {
		WaitForSingleObject(ghSvcStopEvent, INFINITE);
		break;
	}
	umount_phydisk_parts();
	report_svc_status(gSvcStatusHandle, &gSvcStatus, SERVICE_STOPPED, NO_ERROR, 0 );
}

static void print_usage()
{
	printf("usage: xokan [-h, -d] disk_path\n");
	printf("    -h, --help: print this message\n");
	printf("    -i, --install: install eokan_svc service.\n");
	printf("    -r, --remove: remove eokan_svc service.\n");
	printf("    -m, --mountpoint: specify mount point drive letter.\n");
	printf("    -u, --umount: umount a filesystem.\n");
	printf("    -s, --service: service mode (default this mode).\n");
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
	const char *disk_type = "physical";
	int iflag = 0, rflag = 0, uflag = 0, sflag = 0, mflag = 0;
	SERVICE_TABLE_ENTRY svc_dispatch_table[] = {
		{EOKAN_SVCNAME, eokan_svc_main},
		{NULL, NULL}
	};
	const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"disk", required_argument, NULL, 'd'},
		{"part", required_argument, NULL, 'p'},
		{"install", no_argument, NULL, 'i'},
		{"remove", no_argument, NULL, 'r'},
		{"umount", required_argument, NULL, 'u'},
		{"mountpoint", required_argument, NULL, 'm'},
		{"service", no_argument, NULL, 's'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "hird:p:u:sm:", long_options, NULL)) != -1) {
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
			case 'u':
				uflag = optarg[0];
				break;
			case 's':
				sflag = 1;
				break;
			case 'm':
				mflag = optarg[0];
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
	if (uflag) {
		int e;
		eokan_load(0);
		e = eokan_umount(uflag);
		eokan_unload();
		return e;
	}
	if (sflag || argc <= 0) {
		if (!StartServiceCtrlDispatcher(svc_dispatch_table)) {
			svc_report_event(TEXT("StartServiceCtrlDispatcher"));
		}
		return 0;
	}
	if (eokan_load(0) < 0) {
		return -1;
	}

	disk = disk_open(disk_type, argv[0], DISK_FLAG_READ);
	if (!disk) {
		printf("can't open disk: %s %s\n", disk_type, argv[0]);
		retval = -1;
		goto done;
	}
	partition = disk_get_partition(disk, part);
	if (!partition) {
		printf("can't find partition: %d\n", part);
		retval = -2;
		goto skip;
	}
	printf("parition: %d, offset: %I64u, length: %I64u\n", part, partition->off, partition->length);
	if ((fs = vfs_mount(partition)) == NULL) {
		retval = -3;
		part_close(partition);
		goto skip;
	}
	eokan_main(fs, mflag ? mflag : find_valid_drive('C'));
	vfs_umount(fs);
	part_close(partition);
skip:
	disk_close(disk);
done:
	eokan_unload();
	return retval;
}

