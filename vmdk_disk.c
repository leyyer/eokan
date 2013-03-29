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

#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <dirent.h>
#include "disk.h"
#include "vixDiskLib.h"

struct vmdk_disk {
	struct disk_descr    disk;
	uint64_t             capacity;
	VixDiskLibConnection connection;
	VixDiskLibHandle     disk_handle;
	HINSTANCE            vix_lib;
	VixError (*VixDiskLib_Init)(uint32 , uint32 , VixDiskLibGenericLogFunc *, VixDiskLibGenericLogFunc *, VixDiskLibGenericLogFunc *, const char* );
	VixError (*VixDiskLib_Connect)(const VixDiskLibConnectParams *, VixDiskLibConnection *);
	VixError (*VixDiskLib_Open)(const VixDiskLibConnection , const char *, uint32 , VixDiskLibHandle *);
	VixError (*VixDiskLib_Read)(VixDiskLibHandle , VixDiskLibSectorType , VixDiskLibSectorType , uint8 *);
	VixError (*VixDiskLib_Write)(VixDiskLibHandle , VixDiskLibSectorType , VixDiskLibSectorType , const uint8 *);
	VixError (*VixDiskLib_Close)(VixDiskLibHandle diskHandle);
	VixError (*VixDiskLib_Disconnect)(VixDiskLibConnection connection);
	void (*VixDiskLib_Exit)(void);
	const char * (*VixDiskLib_GetTransportMode)(VixDiskLibHandle diskHandle);
	VixError (*VixDiskLib_GetInfo)(VixDiskLibHandle diskHandle, VixDiskLibInfo **info);
	void (*VixDiskLib_FreeInfo)(VixDiskLibInfo *info);
	const char * (*VixDiskLib_ListTransportModes)(void);
};

#define VIXDISKLIB_VERSION_MAJOR 5
#define VIXDISKLIB_VERSION_MINOR 1

static uint64_t vmdk_disk_capacity(disk_descr_t disk)
{
	struct vmdk_disk *vmdk = (struct vmdk_disk *)disk;
	return vmdk->capacity;
}

static int vmdk_disk_read(disk_descr_t disk, int64_t start, int64_t num, uint8_t *buf)
{
	VixError error;
	struct vmdk_disk *vmdk = (struct vmdk_disk *)disk;
	error = vmdk->VixDiskLib_Read(vmdk->disk_handle, start, num, buf);
	return error == VIX_OK ? 0 : -1;
}

static int vmdk_disk_write(disk_descr_t disk, int64_t start, int64_t num, const uint8_t *buf)
{
	VixError error;
	struct vmdk_disk *vmdk = (struct vmdk_disk *)disk;
	error = vmdk->VixDiskLib_Write(vmdk->disk_handle, start, num, buf);
	return error == VIX_OK ? 0 : -1;
}

static int vmdk_get_info(struct vmdk_disk *vmdk)
{
    VixDiskLibInfo *info = NULL;
    VixError error;

	printf("Transport mode \"%s\".\n", vmdk->VixDiskLib_GetTransportMode(vmdk->disk_handle));

    error = vmdk->VixDiskLib_GetInfo(vmdk->disk_handle, &info);
	if (error != VIX_OK) 
		return -1;

	vmdk->capacity = info->capacity;
    printf("Capacity          = %I64u sectors\n", (uint64_t)info->capacity);
    printf("number of links   = %d\n" ,info->numLinks);
    printf("adapter type      = ");
    switch (info->adapterType) {
    case VIXDISKLIB_ADAPTER_IDE:
       printf("IDE");
       break;
    case VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC:
       printf("BusLogic SCSI");
       break;
    case VIXDISKLIB_ADAPTER_SCSI_LSILOGIC:
       printf("LsiLogic SCSI");
       break;
    default:
       printf("unknown");
       break;
    }
    printf("\nBIOS geometry     = %u/%u/%u\n", info->biosGeo.cylinders, info->biosGeo.heads ,info->biosGeo.sectors);
    printf("physical geometry = %u/%u/%u\n",  info->physGeo.cylinders,  info->physGeo.heads, info->physGeo.sectors);
    vmdk->VixDiskLib_FreeInfo(info);
    printf("Transport modes supported by vixDiskLib: %s\n", vmdk->VixDiskLib_ListTransportModes());
	return 0;
}

static void   vmdk_disk_release(disk_descr_t disk)
{
	struct vmdk_disk *vmdk = (struct vmdk_disk *)disk;

	vmdk->VixDiskLib_Close(vmdk->disk_handle);
	vmdk->VixDiskLib_Disconnect(vmdk->connection);
	vmdk->VixDiskLib_Exit();
}

static int loadfunc(HINSTANCE hInstLib, void** pFunction, const char* funcName)
{
   *pFunction = GetProcAddress(hInstLib, funcName);
   if (*pFunction == NULL) {
      printf ("Failed to load %s . Error = %d\n" ,funcName, GetLastError());
	  return -1;
   }
   return 0;
}

#define LOAD_FUNC(handle, funcName)  \
   assert((loadfunc(handle->vix_lib, (void**)&(handle->funcName), #funcName)) == 0)

static int vmdk_disk_create(disk_descr_t disk, const char *path, uint32_t flags)
{
	VixError error;
    VixDiskLibConnectParams params = {0};
	uint32_t vflags = 0;
	struct vmdk_disk *vmdk = (struct vmdk_disk *)disk;

	if ((vmdk->vix_lib = LoadLibrary(L"vixDiskLib.dll")) == NULL) {
		fprintf(stderr, "Please install vmware vddk libraries.\n");
		return -1;
	}
	LOAD_FUNC(vmdk, VixDiskLib_Init);
	LOAD_FUNC(vmdk, VixDiskLib_Connect);
	LOAD_FUNC(vmdk, VixDiskLib_Open);
	LOAD_FUNC(vmdk, VixDiskLib_Read);
	LOAD_FUNC(vmdk, VixDiskLib_Write);
	LOAD_FUNC(vmdk, VixDiskLib_Close);
	LOAD_FUNC(vmdk, VixDiskLib_Disconnect);
	LOAD_FUNC(vmdk, VixDiskLib_Exit);
	LOAD_FUNC(vmdk, VixDiskLib_GetTransportMode);
	LOAD_FUNC(vmdk, VixDiskLib_GetInfo);
	LOAD_FUNC(vmdk, VixDiskLib_FreeInfo);
	LOAD_FUNC(vmdk, VixDiskLib_ListTransportModes);

	error = vmdk->VixDiskLib_Init(VIXDISKLIB_VERSION_MAJOR,
			VIXDISKLIB_VERSION_MINOR,
			NULL, NULL, NULL, /* Log, warn, panic */
			NULL);
	if (error != VIX_OK) {
		fprintf(stderr, "vixdisklib init failed.\n");
		return -1;
	}

	error = vmdk->VixDiskLib_Connect(&params, &vmdk->connection);
	if (error != VIX_OK) {
		vmdk->VixDiskLib_Exit();
		fprintf(stderr, "vixdisklib connect failed.\n");
		return -2;
	}

	if (flags & DISK_FLAG_READ) {
		vflags |= VIXDISKLIB_FLAG_OPEN_READ_ONLY;
	}
	if (flags & DISK_FLAG_WRITE) {
		vflags &= ~VIXDISKLIB_FLAG_OPEN_READ_ONLY;
	}
	error = vmdk->VixDiskLib_Open(vmdk->connection, path, vflags, &vmdk->disk_handle);
	if (error != VIX_OK) {
		vmdk->VixDiskLib_Disconnect(vmdk->connection);
		vmdk->VixDiskLib_Exit();
		fprintf(stderr, "vixdisklib open failed.\n");
		return -3;
	}
	vmdk_get_info(vmdk);
	disk->release = vmdk_disk_release;
	disk->read    = vmdk_disk_read;
	disk->write   = vmdk_disk_write;
	disk->capacity = vmdk_disk_capacity;
	return 0;
}

struct disk_probe_spec vmdk_disk_spec = {
	.name  = "vmdk",
	.size  = sizeof (struct vmdk_disk),
	.probe = vmdk_disk_create,
};

