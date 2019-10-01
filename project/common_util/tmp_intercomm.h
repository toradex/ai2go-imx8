// Copyright (c) 2019 Toradex
//
#ifndef __TMP_INTERCOMM_FILE_H__
#define __TMP_INTERCOMM_FILE_H__

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fuse.h>
#include <sys/syscall.h>
#include <pthread.h>

#define INTERCOMM_DIR "/tmp/"

static int tmp_intercomm_argc = 4;
static char* tmp_intercomm_argv[] = {
	"tmp_intercomm",
	"-d",
	"-s",
	INTERCOMM_DIR
};

typedef struct tmp_intercomm_device_struct
{
	char filePath[256];
	char fileName[120];
	char strVal[120];
	int intVal;
} tmp_intercomm_device;

typedef struct tmp_intercomm_interface_struct
{
	char mountPoint[120];
	char appName[120];
	int devices_count;
	tmp_intercomm_device* devices[20];
	struct fuse_operations operations;
} tmp_intercomm_interface;

extern tmp_intercomm_interface* tmp_intercomm_global_interface;

tmp_intercomm_interface* tmp_intercomm_create_interface(const char *name);
void tmp_intercomm_mount(tmp_intercomm_interface *interface);
void tmp_intercomm_mount_global();
void tmp_intercomm_implement_ops_global();
void tmp_intercomm_write_int(tmp_intercomm_device* device, int value);
void tmp_intercomm_write_str(tmp_intercomm_device* device,
	const char* value);
tmp_intercomm_device* tmp_intercomm_create_device (
	tmp_intercomm_interface *interface, const char* name);

#endif  // __TMP_INTERCOMM_FILE_H__
