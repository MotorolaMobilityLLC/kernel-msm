/*************************************************************************/ /*!
@File
@Title          Device Memory Management internal utility functions
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Utility functions used internally by device memory management
                code.
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

#ifndef _DEVICEMEM_UTILS_H_
#define _DEVICEMEM_UTILS_H_

#include "devicemem.h"
#include "img_types.h"
#include "pvrsrv_error.h"
#include "pvr_debug.h"
#include "allocmem.h"
#include "ra.h"
#include "osfunc.h"
#include "lock.h"
#include "devicemem_mmap.h"
#include "devicemem_utils.h"

#define DEVMEM_HEAPNAME_MAXLENGTH 160


#if defined(DEVMEM_DEBUG) && defined(REFCOUNT_DEBUG)
#define DEVMEM_REFCOUNT_PRINT(fmt, ...) PVRSRVDebugPrintf(PVR_DBG_ERROR, __FILE__, __LINE__, fmt, __VA_ARGS__)
#else
#define DEVMEM_REFCOUNT_PRINT(fmt, ...)
#endif


/* If we need a "hMapping" but we don't have a server-side mapping, we
   poison the entry with this value so that it's easily recognised in
   the debugger.  Note that this is potentially a valid handle, but
   then so is IMG_NULL, which is no better, indeed worse, as it's not
   obvious in the debugger.  The value doesn't matter.  We _never_ use
   it (and because it's valid, we never assert it isn't this) but it's
   nice to have a value in the source code that we can grep for when
   things go wrong. */
#define LACK_OF_MAPPING_POISON ((IMG_HANDLE)0x6116dead)
#define LACK_OF_RESERVATION_POISON ((IMG_HANDLE)0x7117dead)

struct _DEVMEM_CONTEXT_ {
    /* Cookie of the device on which this memory context resides */
    IMG_HANDLE hDeviceNode;

    /* Number of heaps that have been created in this context
       (regardless of whether they have allocations) */
    IMG_UINT32 uiNumHeaps;

    /* Sometimes we need to talk to Kernel Services.  In order to do
       so, we need the connection handle */
    DEVMEM_BRIDGE_HANDLE hBridge;

    /*
      Each "DEVMEM_CONTEXT" has a counterpart in the server,
      which is responsible for handling the mapping into device MMU.
      We have a handle to that here.
    */
    IMG_HANDLE hDevMemServerContext;

    /* Number of automagically created heaps in this context,
       i.e. those that are born at context creation time from the
       chosen "heap config" or "blueprint" */
    IMG_UINT32 uiAutoHeapCount;

    /* pointer to array of such heaps */
    struct _DEVMEM_HEAP_ **ppsAutoHeapArray;

	/* Private data handle for device specific data */
	IMG_HANDLE hPrivData;
};

struct _DEVMEM_HEAP_ {
    /* Name of heap - for debug and lookup purposes. */
    IMG_CHAR *pszName;

    /* Number of live imports in the heap */
    IMG_UINT32 uiImportCount;

    /*
     * Base address of heap, required by clients due to some requesters
     * not being full range 
     */
    IMG_DEV_VIRTADDR sBaseAddress;

    /* This RA is for managing sub-allocations in virtual space.  Two
       more RA's will be used under the Hood for managing the coarser
       allocation of virtual space from the heap, and also for
       managing the physical backing storage. */
    RA_ARENA *psSubAllocRA;
    IMG_CHAR *pszSubAllocRAName;
    /*
      This RA is for the coarse allocation of virtual space from the heap
    */
    RA_ARENA *psQuantizedVMRA;
    IMG_CHAR *pszQuantizedVMRAName;

    /* We also need to store a copy of the quantum size in order to
       feed this down to the server */
    IMG_UINT32 uiLog2Quantum;

    /* The parent memory context for this heap */
    struct _DEVMEM_CONTEXT_ *psCtx;

	POS_LOCK hLock;							/*!< Lock to protect this structure */

    /*
      Each "DEVMEM_HEAP" has a counterpart in the server,
      which is responsible for handling the mapping into device MMU.
      We have a handle to that here.
    */
    IMG_HANDLE hDevMemServerHeap;
};


typedef struct _DEVMEM_DEVICE_IMPORT_ {
	DEVMEM_HEAP *psHeap;			/*!< Heap this import is bound to */
	IMG_DEV_VIRTADDR sDevVAddr;		/*!< Device virtual address of the import */
	IMG_UINT32 ui32RefCount;		/*!< Refcount of the device virtual address */
	IMG_HANDLE hReservation;		/*!< Device memory reservation handle */
	IMG_HANDLE hMapping;			/*!< Device mapping handle */
	IMG_BOOL bMapped;				/*!< This is import mapped? */
	POS_LOCK hLock;					/*!< Lock to protect the device import */
} DEVMEM_DEVICE_IMPORT;

typedef struct _DEVMEM_CPU_IMPORT_ {
	IMG_PVOID pvCPUVAddr;			/*!< CPU virtual address of the import */
	IMG_UINT32 ui32RefCount;		/*!< Refcount of the CPU virtual address */
	IMG_HANDLE hOSMMapData;			/*!< CPU mapping handle */
	POS_LOCK hLock;					/*!< Lock to protect the CPU import */
} DEVMEM_CPU_IMPORT;

typedef struct _DEVMEM_IMPORT_ {
    DEVMEM_BRIDGE_HANDLE hBridge;		/*!< Bridge connection for the server */
    IMG_DEVMEM_ALIGN_T uiAlign;			/*!< Alignment requirement */
	DEVMEM_SIZE_T uiSize;				/*!< Size of import */
    IMG_UINT32 ui32RefCount;			/*!< Refcount for this import */
    IMG_BOOL bExportable;				/*!< Is this import exportable? */
    IMG_HANDLE hPMR;					/*!< Handle to the PMR */
    DEVMEM_FLAGS_T uiFlags;				/*!< Flags for this import */
    POS_LOCK hLock;						/*!< Lock to protect the import */

	DEVMEM_DEVICE_IMPORT sDeviceImport;	/*!< Device specifics of the import */
	DEVMEM_CPU_IMPORT sCPUImport;		/*!< CPU specifics of the import */
} DEVMEM_IMPORT;

typedef struct _DEVMEM_DEVICE_MEMDESC_ {
	IMG_DEV_VIRTADDR sDevVAddr;		/*!< Device virtual address of the allocation */
	IMG_UINT32 ui32RefCount;		/*!< Refcount of the device virtual address */
	POS_LOCK hLock;					/*!< Lock to protect device memdesc */
} DEVMEM_DEVICE_MEMDESC;

typedef struct _DEVMEM_CPU_MEMDESC_ {
	IMG_PVOID pvCPUVAddr;			/*!< CPU virtual address of the import */
	IMG_UINT32 ui32RefCount;		/*!< Refcount of the device CPU address */
	POS_LOCK hLock;					/*!< Lock to protect CPU memdesc */
} DEVMEM_CPU_MEMDESC;

struct _DEVMEM_MEMDESC_ {
    DEVMEM_IMPORT *psImport;				/*!< Import this memdesc is on */
    IMG_DEVMEM_OFFSET_T uiOffset;			/*!< Offset into import where our allocation starts */
    IMG_UINT32 ui32RefCount;				/*!< Refcount of the memdesc */
    POS_LOCK hLock;							/*!< Lock to protect memdesc */

	DEVMEM_DEVICE_MEMDESC sDeviceMemDesc;	/*!< Device specifics of the memdesc */
	DEVMEM_CPU_MEMDESC sCPUMemDesc;		/*!< CPU specifics of the memdesc */

#if defined(PVR_RI_DEBUG)
    IMG_HANDLE hRIHandle;					/*!< Handle to RI information */
#endif
};

PVRSRV_ERROR _DevmemValidateParams(IMG_DEVMEM_SIZE_T uiSize,
								   IMG_DEVMEM_ALIGN_T uiAlign,
								   DEVMEM_FLAGS_T uiFlags);

PVRSRV_ERROR _DevmemImportStructAlloc(IMG_HANDLE hBridge,
									  IMG_BOOL bExportable,
									  DEVMEM_IMPORT **ppsImport);

IMG_VOID _DevmemImportStructFree(DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemImportStructInit(DEVMEM_IMPORT *psImport,
								 IMG_DEVMEM_SIZE_T uiSize,
								 IMG_DEVMEM_ALIGN_T uiAlign,
								 PVRSRV_MEMALLOCFLAGS_T uiMapFlags,
								 IMG_HANDLE hPMR);

PVRSRV_ERROR _DevmemImportStructDevMap(DEVMEM_HEAP *psHeap,
									   IMG_BOOL bMap,
									   DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemImportStructDevUnmap(DEVMEM_IMPORT *psImport);

PVRSRV_ERROR _DevmemImportStructCPUMap(DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemImportStructCPUUnmap(DEVMEM_IMPORT *psImport);



IMG_VOID _DevmemImportStructAcquire(DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemImportStructRelease(DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemImportDiscard(DEVMEM_IMPORT *psImport);

PVRSRV_ERROR _DevmemMemDescAlloc(DEVMEM_MEMDESC **ppsMemDesc);

IMG_VOID _DevmemMemDescInit(DEVMEM_MEMDESC *psMemDesc,
						  	IMG_DEVMEM_OFFSET_T uiOffset,
						  	DEVMEM_IMPORT *psImport);

IMG_VOID _DevmemMemDescAcquire(DEVMEM_MEMDESC *psMemDesc);

IMG_VOID _DevmemMemDescRelease(DEVMEM_MEMDESC *psMemDesc);

IMG_VOID _DevmemMemDescDiscard(DEVMEM_MEMDESC *psMemDesc);

#endif /* _DEVICEMEM_UTILS_H_ */
