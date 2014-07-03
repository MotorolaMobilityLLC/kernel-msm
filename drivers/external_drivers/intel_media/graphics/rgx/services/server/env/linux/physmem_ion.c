/*************************************************************************/ /*!
@File           physmem_ion.c
@Title          Ion memory allocator
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Part of the memory management. This module is responsible for
                implementing the function callbacks for ion memory.
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

#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"
#include "pvrsrv_memallocflags.h"

#include "allocmem.h"
#include "osfunc.h"
#include "pvrsrv.h"
#include "physmem_lma.h"
#include "pdump_physmem.h"
#include "pmr.h"
#include "pmr_impl.h"
#include "physmem_ion.h"
#include "ion_sys.h"
#include "hash.h"

/* Includes required to get the connection data */
#include "connection_server.h"
#include "env_connection.h"

#if defined(PVR_RI_DEBUG)
#include "ri_server.h"
#endif

#include <linux/err.h>
#include <linux/slab.h>
#include PVR_ANDROID_ION_HEADER
#include <linux/scatterlist.h>

typedef struct _PMR_ION_DATA_ {
	struct ion_client *psIonClient;
	struct ion_handle *psIonHandle;
	IMG_UINT32 ui32PageCount;
	IMG_DEVMEM_SIZE_T uiSize;
	IMG_DEV_PHYADDR *pasDevPhysAddr;
	PHYS_HEAP *psPhysHeap;
	IMG_BOOL bPoisonOnFree;
	ENV_ION_CONNECTION_DATA *psIonData;
	/*
	  for pdump...
	*/
	IMG_BOOL bPDumpMalloced;
	IMG_HANDLE hPDumpAllocInfo;
} PMR_ION_DATA;

/* Start size of the g_psIonBufferHash hash table */
#define ION_BUFFER_HASH_SIZE 20
HASH_TABLE *g_psIonBufferHash = IMG_NULL;
IMG_UINT32 g_ui32HashRefCount = 0;

#if defined(PVR_ANDROID_ION_USE_SG_LENGTH)
#define pvr_sg_length(sg) ((sg)->length)
#else
#define pvr_sg_length(sg) sg_dma_len(sg)
#endif

/*****************************************************************************
 *                       Ion specific functions                              *
 *****************************************************************************/

/*
	Obtain a list of physical pages from the ion
	handle.
*/
static
PVRSRV_ERROR IonPhysAddrAcquire(PMR_ION_DATA *psPrivData,
							   int fd)
{
	struct sg_table *table;
	struct scatterlist *sg;
	IMG_CPU_PHYADDR sCpuPhysAddr;
	IMG_DEV_PHYADDR *pasDevPhysAddr = NULL;
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32PageCount = 0;
	IMG_UINT32 i;

	table = ion_sg_table(psPrivData->psIonClient, psPrivData->psIonHandle);
	if (!table)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto exitFailMap;
	}

	/*
		We do a two pass process, 1st workout how many pages there
		are, 2nd fill in the data.
	*/
	for_each_sg(table->sgl, sg, table->nents, i)
	{
		ui32PageCount += PAGE_ALIGN(pvr_sg_length(sg)) / PAGE_SIZE;
	}

	if (WARN_ON(!ui32PageCount))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to import ion buffer with no pages",
				 __func__));
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto exitFailMap;
	}

	pasDevPhysAddr = kmalloc(sizeof(IMG_DEV_PHYADDR)*ui32PageCount, GFP_KERNEL);
	if (!pasDevPhysAddr)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto exitFailAlloc;
	}

	ui32PageCount = 0;

	for_each_sg(table->sgl, sg, table->nents, i)
	{
		IMG_UINT32 j;

		for (j = 0; j < pvr_sg_length(sg); j += PAGE_SIZE)
		{
			/* Pass 2: Get the page data */
			sCpuPhysAddr.uiAddr = sg_phys(sg);

			pasDevPhysAddr[ui32PageCount] =
				IonCPUPhysToDevPhys(sCpuPhysAddr, j);
			ui32PageCount++;
		}
	}

	psPrivData->pasDevPhysAddr = pasDevPhysAddr;
	psPrivData->ui32PageCount = ui32PageCount;
	psPrivData->uiSize = (IMG_DEVMEM_SIZE_T)ui32PageCount * PAGE_SIZE;

	return PVRSRV_OK;

exitFailAlloc:
exitFailMap:
	PVR_ASSERT(eError!= PVRSRV_OK);
	return eError;
}

static
IMG_VOID IonPhysAddrRelease(PMR_ION_DATA *psPrivData)
{
	kfree(psPrivData->pasDevPhysAddr);
}

static IMG_BOOL _IonKeyCompare(IMG_SIZE_T uKeySize, IMG_VOID *pKey1, IMG_VOID *pKey2)
{
	IMG_DEV_PHYADDR *psKey1 = pKey1;
	IMG_DEV_PHYADDR *psKey2 = pKey2;
	PVR_ASSERT(uKeySize == sizeof(IMG_DEV_PHYADDR));
	
	return psKey1->uiAddr == psKey2->uiAddr;
}

/*****************************************************************************
 *                       PMR callback functions                              *
 *****************************************************************************/


static IMG_VOID
_Poison(IMG_PVOID pvKernAddr,
		IMG_DEVMEM_SIZE_T uiBufferSize,
        const IMG_CHAR *pacPoisonData,
        IMG_SIZE_T uiPoisonSize)
{
    IMG_UINT32 uiSrcByteIndex = 0;
    IMG_DEVMEM_SIZE_T uiDestByteIndex;
    IMG_CHAR *pcDest;

	pcDest = pvKernAddr;

	for(uiDestByteIndex=0; uiDestByteIndex<uiBufferSize; uiDestByteIndex++)
	{
		pcDest[uiDestByteIndex] = pacPoisonData[uiSrcByteIndex];
		uiSrcByteIndex++;
		if (uiSrcByteIndex == uiPoisonSize)
		{
			uiSrcByteIndex = 0;
		}
	}
}

static const IMG_CHAR _AllocPoison[] = "^PoIsOn";
static const IMG_UINT32 _AllocPoisonSize = 7;
static const IMG_CHAR _FreePoison[] = "<DEAD-BEEF>";
static const IMG_UINT32 _FreePoisonSize = 11;

static PVRSRV_ERROR
PMRFinalizeIon(PMR_IMPL_PRIVDATA pvPriv)
{
	PMR_ION_DATA *psPrivData = IMG_NULL;

	psPrivData = pvPriv;

	HASH_Remove_Extended(g_psIonBufferHash, &psPrivData->pasDevPhysAddr[0]);
	g_ui32HashRefCount--;
	if (g_ui32HashRefCount == 0)
	{
		HASH_Delete(g_psIonBufferHash);
		g_psIonBufferHash = IMG_NULL;
	}

	if (psPrivData->bPDumpMalloced)
	{
		PDumpPMRFree(psPrivData->hPDumpAllocInfo);
	}

	if (psPrivData->bPoisonOnFree)
	{
		IMG_PVOID pvKernAddr;

		pvKernAddr = ion_map_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);

		if (IS_ERR_OR_NULL(pvKernAddr))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to poison allocation before free", __FUNCTION__));
			PVR_ASSERT(IMG_FALSE);
		}

		_Poison(pvKernAddr,
				psPrivData->uiSize,
				_FreePoison,
				_FreePoisonSize);

		ion_unmap_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);
	}

	IonPhysAddrRelease(psPrivData);
	ion_free(psPrivData->psIonClient, psPrivData->psIonHandle);
	EnvDataIonClientRelease(psPrivData->psIonData);
	PhysHeapRelease(psPrivData->psPhysHeap);
	kfree(psPrivData);

	return PVRSRV_OK;
}

/*
	Lock and unlock function for physical address
	don't do anything for as we acquire the physical
	address at create time.
*/
static PVRSRV_ERROR
PMRLockPhysAddressesIon(PMR_IMPL_PRIVDATA pvPriv,
						   IMG_UINT32 uiLog2DevPageSize)
{
	PVR_UNREFERENCED_PARAMETER(pvPriv);
	PVR_UNREFERENCED_PARAMETER(uiLog2DevPageSize);

	return PVRSRV_OK;

}

static PVRSRV_ERROR
PMRUnlockPhysAddressesIon(PMR_IMPL_PRIVDATA pvPriv)
{
	PVR_UNREFERENCED_PARAMETER(pvPriv);

	return PVRSRV_OK;
}

static PVRSRV_ERROR
PMRDevPhysAddrIon(PMR_IMPL_PRIVDATA pvPriv,
				  IMG_DEVMEM_OFFSET_T uiOffset,
				  IMG_DEV_PHYADDR *psDevPAddr)
{
	PMR_ION_DATA *psPrivData = pvPriv;
    IMG_UINT32 ui32PageCount;
    IMG_UINT32 ui32PageIndex;
    IMG_UINT32 ui32InPageOffset;

    ui32PageCount = psPrivData->ui32PageCount;

    ui32PageIndex = uiOffset >> PAGE_SHIFT;
    ui32InPageOffset = uiOffset - ((IMG_DEVMEM_OFFSET_T)ui32PageIndex << PAGE_SHIFT);
    PVR_ASSERT(ui32PageIndex < ui32PageCount);
    PVR_ASSERT(ui32InPageOffset < PAGE_SIZE);

    psDevPAddr->uiAddr = psPrivData->pasDevPhysAddr[ui32PageIndex].uiAddr + ui32InPageOffset;

	return PVRSRV_OK;
}

static PVRSRV_ERROR
PMRAcquireKernelMappingDataIon(PMR_IMPL_PRIVDATA pvPriv,
							   IMG_SIZE_T uiOffset,
							   IMG_SIZE_T uiSize,
							   IMG_VOID **ppvKernelAddressOut,
							   IMG_HANDLE *phHandleOut,
							   PMR_FLAGS_T ulFlags)
{
	PMR_ION_DATA *psPrivData = pvPriv;
	IMG_PVOID pvKernAddr;
	PVRSRV_ERROR eError;

	pvKernAddr = ion_map_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);

	if (IS_ERR_OR_NULL(pvKernAddr))
	{
		eError = PVRSRV_ERROR_PMR_NO_KERNEL_MAPPING;
		goto fail;
	}

	*ppvKernelAddressOut = pvKernAddr;

	return PVRSRV_OK;

	/*
	  error exit paths follow
	*/

fail:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

static IMG_VOID PMRReleaseKernelMappingDataIon(PMR_IMPL_PRIVDATA pvPriv,
											   IMG_HANDLE hHandle)
{
	PMR_ION_DATA *psPrivData = pvPriv;
	PVR_UNREFERENCED_PARAMETER(hHandle);

	ion_unmap_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);
}

static PMR_IMPL_FUNCTAB _sPMRIonFuncTab = {
	/* pfnLockPhysAddresses */
	&PMRLockPhysAddressesIon,
	/* pfnUnlockPhysAddresses */
	&PMRUnlockPhysAddressesIon,
	/* pfnDevPhysAddr */
	&PMRDevPhysAddrIon,
	/* pfnPDumpSymbolicAddr */
	IMG_NULL,
	/* pfnAcquireKernelMappingData */
	&PMRAcquireKernelMappingDataIon,
	/* pfnReleaseKernelMappingData */
	&PMRReleaseKernelMappingDataIon,
	/* pfnReadBytes */
	IMG_NULL,
	/* pfnWriteBytes */
	IMG_NULL,
	/* pfnFinalize */
	&PMRFinalizeIon
};

/*****************************************************************************
 *                       Public facing interface                             *
 *****************************************************************************/

PVRSRV_ERROR
PhysmemImportIon(CONNECTION_DATA *psConnection,
				 IMG_INT fd,
				 PVRSRV_MEMALLOCFLAGS_T uiFlags,
				 PMR **ppsPMRPtr,
				 IMG_DEVMEM_SIZE_T *puiSize,
				 IMG_DEVMEM_ALIGN_T *puiAlign)
{
	PMR_ION_DATA *psPrivData = IMG_NULL;
	IMG_HANDLE hPDumpAllocInfo = IMG_NULL;
	ENV_CONNECTION_DATA *psEnvConnectionData;
	PMR *psPMR = IMG_NULL;
	PMR_FLAGS_T uiPMRFlags;
    IMG_BOOL bZero;
    IMG_BOOL bPoisonOnAlloc;
    IMG_BOOL bPoisonOnFree;
	IMG_BOOL bMappingTable = IMG_TRUE;
	PVRSRV_ERROR eError;

    if (uiFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC)
    {
        bZero = IMG_TRUE;
    }
    else
    {
        bZero = IMG_FALSE;
    }

    if (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC)
    {
        bPoisonOnAlloc = IMG_TRUE;
    }
    else
    {
        bPoisonOnAlloc = IMG_FALSE;
    }

    if (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_FREE)
    {
        bPoisonOnFree = IMG_TRUE;
    }
    else
    {
        bPoisonOnFree = IMG_FALSE;
    }

    if ((uiFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC) &&
        (uiFlags & PVRSRV_MEMALLOCFLAG_POISON_ON_ALLOC))
    {
        /* Zero on Alloc and Poison on Alloc are mutually exclusive */
        eError = PVRSRV_ERROR_INVALID_PARAMS;
        goto fail_params;
    }

	if (!psConnection)
	{
		eError = PVRSRV_ERROR_INVALID_PARAMS;
		goto fail_params;
	}

	psPrivData = kmalloc(sizeof(*psPrivData), GFP_KERNEL);
	if (psPrivData == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_privalloc;
	}

	/*
		Get the physical heap for this PMR
		
		Note:
		While we have no way to determine the type of the buffer
		we just assume that all Ion buffers are from the same
		physical heap.
	*/
	eError = PhysHeapAcquire(IonPhysHeapID(), &psPrivData->psPhysHeap);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed PhysHeapAcquire", __func__));
		goto fail_physheap;
	}

	/* Get the ion client from this connection */
	psEnvConnectionData = PVRSRVConnectionPrivateData(psConnection);
	psPrivData->psIonData = psEnvConnectionData->psIonData;
	psPrivData->psIonClient = EnvDataIonClientAcquire(psEnvConnectionData);

	/* Get the buffer handle */
	psPrivData->psIonHandle = ion_import_dma_buf(psPrivData->psIonClient, fd);
	if (psPrivData->psIonHandle == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: ion_import_dma_buf failed", __func__));
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto fail_ionimport;
	}

	/*
		Note:

		We could defer the import until lock address time but we
		do it here as then we can detect any errors at import time.
		Also we need to know the ion buffer size here and there seems
		to be no other way to find that other then map the buffer for dma.
	*/
	eError = IonPhysAddrAcquire(psPrivData,
							   fd);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: IonPhysAddrAcquire failed", __func__));
		goto fail_acquire;
	}

	if (g_psIonBufferHash == IMG_NULL)
	{
		/*
			As different processes may import the same ION buffer we need to
			create a hash table so we don't generate a duplicate PMR but
			rather just take a reference on an existing one.
		*/
		g_psIonBufferHash = HASH_Create_Extended(ION_BUFFER_HASH_SIZE, sizeof(psPrivData->pasDevPhysAddr[0]), HASH_Func_Default, _IonKeyCompare);
		if (g_psIonBufferHash == IMG_NULL)
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}
	else
	{
		/*
			We have a hash table so check if have already seen this
			this ion buffer before
		*/
		psPMR = (PMR *) HASH_Retrieve_Extended(g_psIonBufferHash, &psPrivData->pasDevPhysAddr[0]);
		if (psPMR != IMG_NULL)
		{
			/*
				We already know about this ion buffer but we had to do a bunch
				for work to determine that so here we have to undo it
			*/
			IonPhysAddrRelease(psPrivData);
			ion_free(psPrivData->psIonClient, psPrivData->psIonHandle);
			EnvDataIonClientRelease(psPrivData->psIonData);
			PhysHeapRelease(psPrivData->psPhysHeap);
			kfree(psPrivData);
			
			/* Reuse the PMR we already created */
			PMRRefPMR(psPMR);

			*ppsPMRPtr = psPMR;
			psPrivData = PMRGetPrivateDataHack(psPMR, &_sPMRIonFuncTab);
			*puiSize = psPrivData->uiSize;
			*puiAlign = PAGE_SIZE;
			return PVRSRV_OK;
		}
	}

	if (bZero || bPoisonOnAlloc)
	{
		IMG_PVOID pvKernAddr;

		pvKernAddr = ion_map_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);

		if (IS_ERR_OR_NULL(pvKernAddr))
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: ion_map_kernel failed", __func__));
			eError = PVRSRV_ERROR_PMR_NO_KERNEL_MAPPING;
			goto fail_kernelmap;
		}

		if (bZero)
		{
			memset(pvKernAddr, 0, psPrivData->uiSize);
		}
		else
		{
			_Poison(pvKernAddr,
					psPrivData->uiSize,
					_AllocPoison,
					_AllocPoisonSize);
		}

		ion_unmap_kernel(psPrivData->psIonClient, psPrivData->psIonHandle);
	}

	psPrivData->bPoisonOnFree = bPoisonOnFree;

	uiPMRFlags = (PMR_FLAGS_T)(uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK);
	/* check no significant bits were lost in cast due to different
	   bit widths for flags */
	PVR_ASSERT(uiPMRFlags == (uiFlags & PVRSRV_MEMALLOCFLAGS_PMRFLAGSMASK));

	eError = PMRCreatePMR(psPrivData->psPhysHeap,
						  psPrivData->uiSize,
                          psPrivData->uiSize,
                          1,
                          1,
                          &bMappingTable,
						  PAGE_SHIFT,
						  uiPMRFlags,
						  "PMRION",
						  &_sPMRIonFuncTab,
						  psPrivData,
						  &psPMR,
						  &hPDumpAllocInfo,
						  IMG_FALSE);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to create PMR", __func__));
		goto fail_pmrcreate;
	}

#if defined(PVR_RI_DEBUG)
	{
		eError = RIWritePMREntryKM (psPMR,
									"ION",
									psPrivData->uiSize);
	}
#endif

	psPrivData->hPDumpAllocInfo = hPDumpAllocInfo;
	psPrivData->bPDumpMalloced = IMG_TRUE;

	/* First time we've seen this ion buffer so store it in the hash table */
	HASH_Insert_Extended(g_psIonBufferHash, &psPrivData->pasDevPhysAddr[0], (IMG_UINTPTR_T) psPMR);
	g_ui32HashRefCount++;

	*ppsPMRPtr = psPMR;
	*puiSize = psPrivData->uiSize;
	*puiAlign = PAGE_SIZE;
	return PVRSRV_OK;

fail_pmrcreate:
fail_kernelmap:
	IonPhysAddrRelease(psPrivData);
fail_acquire:
	ion_free(psPrivData->psIonClient, psPrivData->psIonHandle);
fail_ionimport:
	EnvDataIonClientRelease(psPrivData->psIonData);
	PhysHeapRelease(psPrivData->psPhysHeap);
fail_physheap:
	kfree(psPrivData);

fail_privalloc:
fail_params:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}
