/*************************************************************************/ /*!
@File           pvr_sync.h
@Title          Kernel driver for Android's sync mechanism
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

#ifndef _PVR_SYNC_H
#define _PVR_SYNC_H

#include "pvr_fd_sync_user.h"
#include "rgx_fwif_shared.h"

/* Services internal interface */
PVRSRV_ERROR PVRFDSyncDeviceInitKM(void);
void PVRFDSyncDeviceDeInitKM(void);

/* to keep track of the intermediate allocations done for the FD merge */
typedef struct _FDMERGE_DATA_
{
	PRGXFWIF_UFO_ADDR *pauiFenceUFOAddress;
	IMG_UINT32        *paui32FenceValue;
	PRGXFWIF_UFO_ADDR *pauiUpdateUFOAddress;
	IMG_UINT32        *paui32UpdateValue;
} FDMERGE_DATA;

IMG_INTERNAL PVRSRV_ERROR 
PVRFDSyncMergeFencesKM(IMG_UINT32        *pui32ClientFenceCountOut,
					   PRGXFWIF_UFO_ADDR **ppauiFenceUFOAddressOut,
					   IMG_UINT32        **ppaui32FenceValueOut,
					   IMG_UINT32        *pui32ClientUpdateCountOut,
					   PRGXFWIF_UFO_ADDR **ppauiUpdateUFOAddressOut,
					   IMG_UINT32        **ppaui32UpdateValueOut,
					   const IMG_CHAR*   pszName,
					   const IMG_BOOL    bUpdate,
					   const IMG_UINT32  ui32NumFDs,
					   const IMG_INT32   *paui32FDs,
					   FDMERGE_DATA      *psFDMergeData);

IMG_INTERNAL IMG_VOID
PVRFDSyncMergeFencesCleanupKM(FDMERGE_DATA *psFDMergeData);

PVRSRV_ERROR
PVRFDSyncNoHwUpdateFenceKM(IMG_INT32 i32FDFence);

#endif /* _PVR_SYNC_H */
