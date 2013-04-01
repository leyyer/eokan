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

#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <shellapi.h>
#include "dokan.h"
#include "disk.h"
#include "util.h"
#include "fs.h"

static BOOL g_UseStdErr;
static BOOL g_DebugMode;

static void DbgPrint(LPCWSTR format, ...)
{
	if (g_DebugMode) {
		WCHAR buffer[512];
		va_list argp;
		va_start(argp, format);
		vswprintf_s(buffer, sizeof(buffer)/sizeof(WCHAR), format, argp);
		va_end(argp);
		if (g_UseStdErr) {
			fwprintf(stderr, buffer);
		} else {
			OutputDebugStringW(buffer);
		}
	}
}

static int GetFilePath(char *file, int size, LPCWSTR name)
{
	int nby;
	char *ep, *endp;

	nby =  utf16_to_utf8(name, wcslen(name), file, size);
	ep = file;
	endp = ep + size;

	while (ep < endp) {
		if (*ep == '\\')
			*ep = '/';
		++ep;
	}
	return nby;
}

static int __CreateFile(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	char filePath[MAX_PATH];
	filesys_t fs = (filesys_t)DokanFileInfo->DokanOptions->GlobalContext;
	file_entry_t filp = NULL;

	DbgPrint(L"CreateFile : %s\n", FileName);

	if (CreationDisposition == CREATE_NEW)
		DbgPrint(L"\tCREATE_NEW\n");
	if (CreationDisposition == OPEN_ALWAYS)
		DbgPrint(L"\tOPEN_ALWAYS\n");
	if (CreationDisposition == CREATE_ALWAYS)
		DbgPrint(L"\tCREATE_ALWAYS\n");
	if (CreationDisposition == OPEN_EXISTING)
		DbgPrint(L"\tOPEN_EXISTING\n");
	if (CreationDisposition == TRUNCATE_EXISTING)
		DbgPrint(L"\tTRUNCATE_EXISTING\n");

	DbgPrint(L"\tShareMode = 0x%x\n", ShareMode);

	if (!DokanFileInfo->IsDirectory) {
		GetFilePath(filePath, MAX_PATH, FileName);
		filp = vfs_open(fs, filePath);
	}
	// save the file handle in Context
	DokanFileInfo->Context = (ULONG64)filp;
	return 0;
}

static int __CreateDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];

	DbgPrint(L"CreateDirectory : %s\n", FileName);
	return 0;
}

static int __OpenDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	char FilePath[MAX_PATH] = {0};
	DWORD attr;

	DbgPrint(L"OpenDirectory : %s\n", FileName);
	utf16_to_utf8(FileName, wcslen(FileName), FilePath, sizeof FilePath);
	DokanFileInfo->Context = (ULONG64)0;

	return 0;
}

static int __CloseFile(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	filesys_t fs = (filesys_t) DokanFileInfo->DokanOptions->GlobalContext;
	file_entry_t filp = (file_entry_t)DokanFileInfo->Context;
	if (filp) {
		vfs_file_close(filp, fs);
		DokanFileInfo->Context = 0;
	}
	return 0;
}


static int __Cleanup(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	return 0;
}


static int __ReadFile(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	int nrd = 0;
	filesys_t fs = (filesys_t)DokanFileInfo->DokanOptions->GlobalContext;
	file_entry_t filp = (file_entry_t)DokanFileInfo->Context;

	if (filp) {
		nrd = vfs_file_read(filp, fs, Offset, Buffer, BufferLength);
		*ReadLength = nrd;
	}

	DbgPrint(L"ReadFile : %s\n", FileName);

	return 0;
}


static int __WriteFile(
	LPCWSTR		FileName,
	LPCVOID		Buffer,
	DWORD		NumberOfBytesToWrite,
	LPDWORD		NumberOfBytesWritten,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	ULONG	offset = (ULONG)Offset;
	BOOL	opened = FALSE;

	DbgPrint(L"WriteFile : %s, offset %I64d, length %d\n", FileName, Offset, NumberOfBytesToWrite);

	return 0;
}


static int __FlushFileBuffers(
	LPCWSTR		FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	DbgPrint(L"FlushFileBuffers : %s\n", FileName);

	return 0;
}
#if 0
typedef struct _BY_HANDLE_FILE_INFORMATION {
  DWORD    dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD    dwVolumeSerialNumber;
  DWORD    nFileSizeHigh;
  DWORD    nFileSizeLow;
  DWORD    nNumberOfLinks;
  DWORD    nFileIndexHigh;
  DWORD    nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION;
#endif

static int __GetFileInformation(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	filesys_t fs = (filesys_t)DokanFileInfo->DokanOptions->GlobalContext;
	file_entry_t filp = (file_entry_t)DokanFileInfo->Context;
	struct xstat stbuf;

	if (filp) {
		memset(&stbuf, 0, sizeof stbuf);
		vfs_file_stat(filp, fs, &stbuf);
		HandleFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		HandleFileInformation->nFileSizeLow = stbuf.size;
		HandleFileInformation->nFileSizeHigh = stbuf.size_high;
	}

	DbgPrint(L"GetFileInfo : %s\n", FileName);
	return 0;
}

struct find_cb_data {
	PFillFindData fill_find;
	PDOKAN_FILE_INFO finfo;
};

static int list_all_files(void *data, const char *name, struct xstat *st, int is_dir)
{
	char path[MAX_PATH] = {0}, *dp;
	const char *sp;
	WIN32_FIND_DATAW    dw;
	struct find_cb_data *fdp = data;

	sp = name;
	dp = path;

	while (*sp) {
		if (*sp == '/') {
			*dp = '\\';
		} else {
			*dp = *sp;
		}
		++sp;
		++dp;
	}
	memset(&dw, 0, sizeof dw);
	dw.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	if (is_dir) {
		dw.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
	}
	utf8_to_utf16(path, strlen(path), dw.cFileName, MAX_PATH);
/*	wprintf(L"list_files: %s\n", dw.cFileName); */
	dw.nFileSizeHigh = st->size_high;
	dw.nFileSizeLow  = st->size;
	dw.ftLastAccessTime.dwLowDateTime = st->atime;
	dw.ftLastWriteTime.dwLowDateTime = st->mtime;
	dw.ftCreationTime.dwLowDateTime = st->ctime;
	fdp->fill_find(&dw, fdp->finfo);
	return 0;
}

static int __FindFiles(LPCWSTR	FileName, PFillFindData	FillFindData /* function pointer */, PDOKAN_FILE_INFO	DokanFileInfo)
{
	char filePath[MAX_PATH] = {0};
	struct find_cb_data find_data;
	filesys_t fs = (filesys_t)DokanFileInfo->DokanOptions->GlobalContext;

	memset(&find_data, 0, sizeof find_data);

	GetFilePath(filePath, sizeof filePath, FileName);

	//DbgPrint(L"FindFiles :%s -> %s\n", FileName, filePath);
	find_data.fill_find = FillFindData;
	find_data.finfo = DokanFileInfo;

	vfs_dir_iterate(fs, filePath, list_all_files, &find_data);
	return 0;
}

static int __DeleteFile( LPCWSTR FileName, PDOKAN_FILE_INFO	DokanFileInfo)
{
	char	filePath[MAX_PATH * 2];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	utf16_to_utf8(FileName, wcslen(FileName), filePath, sizeof filePath);

	DbgPrint(L"DeleteFile %s\n", filePath);

	return 0;
}

static int __DeleteDirectory( LPCWSTR FileName, PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"DeleteDirectory %s\n", FileName);
	return -1;
}


static int __MoveFile(
	LPCWSTR				FileName, // existing file name
	LPCWSTR				NewFileName,
	BOOL				ReplaceIfExisting,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"MoveFile %s\n", FileName);
	return 0;
}


static int __LockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"LockFile %s\n", FileName);
	return 0;
}

static int __SetEndOfFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"SetEndOfFile %s, %I64d\n", FileName, ByteOffset);
	return 0;
}


static int __SetAllocationSize(LPCWSTR	FileName,
	LONGLONG			AllocSize,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"SetAllocationSize %s, %I64d\n", FileName, AllocSize);
	return 0;
}


static int __SetFileAttributes(
	LPCWSTR				FileName,
	DWORD				FileAttributes,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	return 0;
}


static int __SetFileTime(
	LPCWSTR				FileName,
	CONST FILETIME*		CreationTime,
	CONST FILETIME*		LastAccessTime,
	CONST FILETIME*		LastWriteTime,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"SetFileTime %s\n", FileName);
	return 0;
}


static int __UnlockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"UnlockFile %s\n", FileName);
	return 0;
}


static int __GetFileSecurity(
	LPCWSTR					FileName,
	PSECURITY_INFORMATION	SecurityInformation,
	PSECURITY_DESCRIPTOR	SecurityDescriptor,
	ULONG				BufferLength,
	PULONG				LengthNeeded,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"GetFileSecurity %s\n", FileName);
	return 0;
}


static int __SetFileSecurity(
	LPCWSTR					FileName,
	PSECURITY_INFORMATION	SecurityInformation,
	PSECURITY_DESCRIPTOR	SecurityDescriptor,
	ULONG				SecurityDescriptorLength,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	return 0;
}

static int __GetVolumeInformation(
	LPWSTR		VolumeNameBuffer,
	DWORD		VolumeNameSize,
	LPDWORD		VolumeSerialNumber,
	LPDWORD		MaximumComponentLength,
	LPDWORD		FileSystemFlags,
	LPWSTR		FileSystemNameBuffer,
	DWORD		FileSystemNameSize,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	char vname[MAX_PATH] = {0}, *ep = vname;
	filesys_t fs = (filesys_t)DokanFileInfo->DokanOptions->GlobalContext;

	if (vfs_label(fs, vname, MAX_PATH) <= 0) {
		ep = "linuxfs";
	}
	utf8_to_utf16(ep, strlen(ep), VolumeNameBuffer, VolumeNameSize);
	*VolumeSerialNumber = 0x19821215;
	*MaximumComponentLength = 256;
	*FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH |
						FILE_CASE_PRESERVED_NAMES |
						FILE_SUPPORTS_REMOTE_STORAGE |
						FILE_UNICODE_ON_DISK |
						FILE_PERSISTENT_ACLS;
	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize / sizeof(WCHAR), L"EXT2/3/4");

	return 0;
}

static int __Unmount(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	DbgPrint(L"Unmount\n");
	return 0;
}


int eokan_main(filesys_t fs)
{
	int status;
	WCHAR wmount_point[MAX_PATH];
	char mount_point[16] = " :";
	int ch = 'C';
	DOKAN_OPERATIONS doperations;
	PDOKAN_OPERATIONS dokanOperations = &doperations;
	DOKAN_OPTIONS doptions;
	PDOKAN_OPTIONS dokanOptions = &doptions;
	HINSTANCE libdokan;
	int DOKANAPI (*dokan_main)(PDOKAN_OPTIONS	DokanOptions, PDOKAN_OPERATIONS DokanOperations);

	libdokan = LoadLibrary(L"dokan.dll");
	if (libdokan == NULL) {
		fwprintf(stderr, L"Please install dokan library. please see:\n\thttp://dokan-dev.net/en/");
		return -1;
	}
	dokan_main = (void *)GetProcAddress(libdokan, "DokanMain");
	if (dokan_main == NULL) {
		fwprintf(stderr, L"can't load DokanMain\n");
		FreeLibrary(libdokan);
		return -1;
	}

	g_DebugMode = FALSE;
	g_UseStdErr = FALSE;

	memset(dokanOptions, 0, sizeof *dokanOptions);
	dokanOptions->Version = DOKAN_VERSION;
	dokanOptions->ThreadCount = 1;

	if (g_DebugMode) {
		dokanOptions->Options |= DOKAN_OPTION_DEBUG;
	}
	if (g_UseStdErr) {
		dokanOptions->Options |= DOKAN_OPTION_STDERR;
	}

	dokanOptions->Options |= DOKAN_OPTION_KEEP_ALIVE;

	dokanOptions->GlobalContext = (ULONG64)fs;

	memset(dokanOperations, 0, sizeof *dokanOperations);
	dokanOperations->CreateFile            = __CreateFile;
	dokanOperations->OpenDirectory         = __OpenDirectory;
	dokanOperations->CreateDirectory       = __CreateDirectory;
	dokanOperations->Cleanup               = __Cleanup;
	dokanOperations->CloseFile             = __CloseFile;
	dokanOperations->ReadFile              = __ReadFile;
	dokanOperations->WriteFile             = __WriteFile;
	dokanOperations->FlushFileBuffers      = __FlushFileBuffers;
	dokanOperations->GetFileInformation    = __GetFileInformation;
	dokanOperations->FindFiles             = __FindFiles;
	dokanOperations->SetFileAttributes     = __SetFileAttributes;
	dokanOperations->SetFileTime           = __SetFileTime;
	dokanOperations->DeleteFile            = __DeleteFile;
	dokanOperations->DeleteDirectory       = __DeleteDirectory;
	dokanOperations->MoveFile              = __MoveFile;
	dokanOperations->SetEndOfFile          = __SetEndOfFile;
	dokanOperations->SetAllocationSize     = __SetAllocationSize;
	dokanOperations->LockFile              = __LockFile;
	dokanOperations->UnlockFile            = __UnlockFile;
	dokanOperations->GetFileSecurity       = __GetFileSecurity;
	dokanOperations->SetFileSecurity       = __SetFileSecurity;
	dokanOperations->GetVolumeInformation  = __GetVolumeInformation;
	dokanOperations->Unmount               = __Unmount;
	dokanOperations->GetDiskFreeSpace      = NULL;
	dokanOperations->FindFilesWithPattern  = NULL;
	dokanOptions->MountPoint = wmount_point;
retry:
	ch += 1;
	if (ch > 'Z') {
		goto skip;
	}
	mount_point[0] = ch;
	utf8_to_utf16(mount_point, strlen(mount_point), wmount_point, MAX_PATH);
	status = dokan_main(dokanOptions, dokanOperations);
	if (status == DOKAN_MOUNT_ERROR) {
		goto retry;
	}
skip:
	switch (status) {
	case DOKAN_SUCCESS:
		fprintf(stderr, "Success\n");
		break;
	case DOKAN_ERROR:
		fprintf(stderr, "Error\n");
		break;
	case DOKAN_DRIVE_LETTER_ERROR:
		fprintf(stderr, "Bad Drive letter\n");
		break;
	case DOKAN_DRIVER_INSTALL_ERROR:
		fprintf(stderr, "Can't install driver\n");
		break;
	case DOKAN_START_ERROR:
		fprintf(stderr, "Driver something wrong\n");
		break;
	case DOKAN_MOUNT_ERROR:
		fprintf(stderr, "Can't assign a drive letter\n");
		break;
	case DOKAN_MOUNT_POINT_ERROR:
		fprintf(stderr, "Mount point error\n");
		break;
	default:
		fprintf(stderr, "Unknown error: %d\n", status);
		break;
	}
	FreeLibrary(libdokan);
	return 0;
}

