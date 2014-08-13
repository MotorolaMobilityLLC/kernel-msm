/*************************************************************************/ /*!
@File           pvr_fd_sync_user.h
@Title          Userspace definitions to use the kernel sync driver
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef _PVR_FD_SYNC_USER_H_
#define _PVR_FD_SYNC_USER_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#include "img_types.h"
#include "pvrsrv_error.h"
#include "util_android.h"

#include "sync_external.h"

#define PVR_SYNC_MAX_QUERY_FENCE_POINTS 15

#define PVR_SYNC_IOC_MAGIC 'W'

#define PVR_SYNC_IOC_CREATE_FENCE \
	_IOWR(PVR_SYNC_IOC_MAGIC, 0, struct PVR_SYNC_CREATE_FENCE_IOCTL_DATA)

#define PVR_SYNC_IOC_ENABLE_FENCING \
	_IOWR(PVR_SYNC_IOC_MAGIC, 1, struct PVR_SYNC_ENABLE_FENCING_IOCTL_DATA)

#define PVR_SYNC_IOC_DEBUG_FENCE \
	_IOWR(PVR_SYNC_IOC_MAGIC, 2, struct PVR_SYNC_DEBUG_FENCE_IOCTL_DATA)

#define PVRSYNC_MODNAME "pvr_sync"

struct PVR_SYNC_CREATE_FENCE_IOCTL_DATA
{
	/* Input */
	IMG_CHAR					szName[32];

	/* Output */
	int							iFenceFd;
	int                         bIdleFence;
}
__attribute__((packed, aligned(8)));

struct PVR_SYNC_ENABLE_FENCING_IOCTL_DATA
{
	/* Input */
	int bFencingEnabled;
}
__attribute__((packed, aligned(8)));

typedef struct
{
	IMG_UINT32					ui32FWAddr;
	IMG_UINT32					ui32Flags;
	IMG_UINT32					ui32FenceValue;
	IMG_UINT32					ui32UpdateValue;
}
__attribute__((packed, aligned(8))) PVR_SYNC_POINT_DATA;

typedef struct
{
	IMG_CHAR					szParentName[32];
	IMG_UINT32					ui32Id;
	IMG_UINT32					ui32CurrOp;
	IMG_UINT32					ui32NextOp;
	PVR_SYNC_POINT_DATA			sData;
}
__attribute__((packed, aligned(8))) PVR_SYNC_DEBUG_SYNC_DATA;

struct PVR_SYNC_DEBUG_FENCE_IOCTL_DATA
{
	/* Input */
	int							iFenceFd;

	/* Output */
	IMG_CHAR					szName[32];
	IMG_INT32					i32Status;
	IMG_UINT32					ui32NumSyncs;
	PVR_SYNC_DEBUG_SYNC_DATA	aPts[PVR_SYNC_MAX_QUERY_FENCE_POINTS];
}
__attribute__((packed, aligned(8)));

PVRSRV_ERROR PVRFDSyncOpen(int *piSyncFd);
PVRSRV_ERROR PVRFDSyncClose(int iSyncFd);

PVRSRV_ERROR PVRFDSyncWaitFence(int iFenceFd);
PVRSRV_ERROR PVRFDSyncCheckFence(int iFenceFd);

PVRSRV_ERROR PVRFDSyncMergeFences(const char *pcszName,
								  int iFenceFd1,
								  int iFenceFd2,
								  int *piNewFenceFd);

PVRSRV_ERROR PVRFDSyncCreateFence(int iSyncFd,
								  const char *pcszName,
								  int *piFenceFd,
								  int *pbIdleFence);

PVRSRV_ERROR PVRFDSyncEnableFencing(int iSyncFd,
									int bFencingEnabled);

PVRSRV_ERROR PVRFDSyncQueryFence(int iSyncFd,
								 int iFenceFd,
								 struct PVR_SYNC_DEBUG_FENCE_IOCTL_DATA *psData);

PVRSRV_ERROR IMG_FORMAT_PRINTF(4, 5) PVRFDSyncDumpFence(int iSyncFd,
														int iFenceFd,
														const char *pcszModule,
														const char *pcszFmt, ...);

#endif /* _PVR_FD_SYNC_USER_H_ */
