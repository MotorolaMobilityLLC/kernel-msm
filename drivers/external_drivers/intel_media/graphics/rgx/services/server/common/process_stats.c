/*************************************************************************/ /*!
@File
@Title          Process based statistics
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Manages a collection of statistics based around a process
                and referenced via OS agnostic methods.
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

#include <stddef.h>

#include "img_defs.h"
#include "img_types.h"
#include "pvr_debug.h"
#include "lock.h"
#include "allocmem.h"
#include "osfunc.h"
#include "lists.h"
#include "process_stats.h"
#include "ri_server.h"


/*
 *  Maximum history of process statistics that will be kept.
 */
#define MAX_DEAD_LIST_PROCESSES  (10)


/*
 * Definition of all process based statistics and the strings used to
 * format them.
 */
typedef enum
{
    /* Stats that are per process... */
    PVRSRV_PROCESS_STAT_TYPE_CONNECTIONS,
    PVRSRV_PROCESS_STAT_TYPE_MAX_CONNECTIONS,

    PVRSRV_PROCESS_STAT_TYPE_RC_OOMS,
    PVRSRV_PROCESS_STAT_TYPE_RC_PRS,
    PVRSRV_PROCESS_STAT_TYPE_RC_GROWS,
    PVRSRV_PROCESS_STAT_TYPE_RC_PUSH_GROWS,
    PVRSRV_PROCESS_STAT_TYPE_RC_TA_STORES,
    PVRSRV_PROCESS_STAT_TYPE_RC_3D_STORES,
    PVRSRV_PROCESS_STAT_TYPE_RC_SH_STORES,
    PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_APP,
    PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_FW,
    PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_APP,
    PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_FW,
    PVRSRV_PROCESS_STAT_TYPE_FREELIST_PAGES_INIT,
    PVRSRV_PROCESS_STAT_TYPE_FREELIST_MAX_PAGES,

#if defined(PVRSRV_ENABLE_MEMORY_STATS)
    PVRSRV_PROCESS_STAT_TYPE_KMALLOC,
    PVRSRV_PROCESS_STAT_TYPE_MAX_KMALLOC,
    PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES,
    PVRSRV_PROCESS_STAT_TYPE_MAX_ALLOC_PAGES,
    PVRSRV_PROCESS_STAT_TYPE_IOREMAP,
    PVRSRV_PROCESS_STAT_TYPE_MAX_IOREMAP,
    PVRSRV_PROCESS_STAT_TYPE_VMAP,
    PVRSRV_PROCESS_STAT_TYPE_MAX_VMAP,
#endif
	
	/* Must be the last enum...*/
	PVRSRV_PROCESS_STAT_TYPE_COUNT
} PVRSRV_PROCESS_STAT_TYPE;

static IMG_CHAR*  pszProcessStatFmt[PVRSRV_PROCESS_STAT_TYPE_COUNT] = {
	"Connections                     %10d\n", /* PVRSRV_STAT_TYPE_CONNECTIONS */
	"ConnectionsMax                  %10d\n", /* PVRSRV_STAT_TYPE_MAXCONNECTIONS */

    "RenderContextOutOfMemoryEvents  %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_OOMS */
    "RenderContextPartialRenders     %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_PRS */
    "RenderContextGrows              %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_GROWS */
    "RenderContextPushGrows          %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_PUSH_GROWS */
    "RenderContextTAStores           %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_TA_STORES */
    "RenderContext3DStores           %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_3D_STORES */
    "RenderContextSHStores           %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_RC_SH_STORES */
    "ZSBufferRequestsByApp           %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_APP */
    "ZSBufferRequestsByFirmware      %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_FW */
    "FreeListGrowRequestsByApp       %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_APP */
    "FreeListGrowRequestsByFirmware  %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_FW */
    "FreeListInitialPages            %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_FREELIST_PAGES_INIT */
    "FreeListMaxPages                %10d\n", /* PVRSRV_PROCESS_STAT_TYPE_FREELIST_MAX_PAGES */

#if defined(PVRSRV_ENABLE_MEMORY_STATS)
    "MemoryUsageKMalloc              %10d\n", /* PVRSRV_STAT_TYPE_KMALLOC */
    "MemoryUsageKMallocMax           %10d\n", /* PVRSRV_STAT_TYPE_MAX_KMALLOC */
    "MemoryUsageAllocPageMemory      %10d\n", /* PVRSRV_STAT_TYPE_ALLOC_PAGES */
    "MemoryUsageAllocPageMemoryMax   %10d\n", /* PVRSRV_STAT_TYPE_MAX_ALLOC_PAGES */
    "MemoryUsageIORemap              %10d\n", /* PVRSRV_STAT_TYPE_IOREMAP */
    "MemoryUsageIORemapMax           %10d\n", /* PVRSRV_STAT_TYPE_MAX_IOREMAP */
    "MemoryUsageVMap                 %10d\n", /* PVRSRV_STAT_TYPE_VMAP */
    "MemoryUsageVMapMax              %10d\n"  /* PVRSRV_STAT_TYPE_MAX_VMAP */
#endif
};


/*
 *  Macro for updating maximum stat values.
 */
#define UPDATE_MAX_VALUE(a,b)  do { if ((b) > (a)) {(a) = (b);} } while(0)

/*
 * Structures for holding statistics...
 */
typedef enum
{
	PVRSRV_STAT_STRUCTURE_PROCESS = 1,
	PVRSRV_STAT_STRUCTURE_RENDER_CONTEXT = 2,
	PVRSRV_STAT_STRUCTURE_MEMORY = 3,
	PVRSRV_STAT_STRUCTURE_RIMEMORY = 4
} PVRSRV_STAT_STRUCTURE_TYPE;

#define MAX_PROC_NAME_LENGTH   (32)

typedef struct _PVRSRV_PROCESS_STATS_ {
	/* Structure type (must be first!) */
	PVRSRV_STAT_STRUCTURE_TYPE        eStructureType;

	/* Linked list pointers */
	struct _PVRSRV_PROCESS_STATS_*    psNext;
	struct _PVRSRV_PROCESS_STATS_*    psPrev;

	/* OS level process ID */
	IMG_PID                           pid;
	IMG_UINT32                        ui32RefCount;

	/* Folder name used to store the statistic */
	IMG_CHAR				          szFolderName[MAX_PROC_NAME_LENGTH];

	/* OS specific data */
	IMG_PVOID                         pvOSPidFolderData;
	IMG_PVOID                         pvOSPidEntryData;

	/* Stats... */
	IMG_INT32                         i32StatValue[PVRSRV_PROCESS_STAT_TYPE_COUNT];

	/* Other statistics structures */
	struct _PVRSRV_RENDER_STATS_*     psRenderLiveList;
	struct _PVRSRV_RENDER_STATS_*     psRenderDeadList;

	struct _PVRSRV_MEMORY_STATS_*     psMemoryStats;
	struct _PVRSRV_RI_MEMORY_STATS_*  psRIMemoryStats;
} PVRSRV_PROCESS_STATS;

typedef struct _PVRSRV_RENDER_STATS_ {
	/* Structure type (must be first!) */
	PVRSRV_STAT_STRUCTURE_TYPE     eStructureType;

	/* Linked list pointers */
	struct _PVRSRV_RENDER_STATS_*  psNext;
	struct _PVRSRV_RENDER_STATS_*  psPrev;

	/* OS specific data */
	IMG_PVOID                      pvOSData;

	/* Stats... */
	IMG_INT32                      i32StatValue[4];
} PVRSRV_RENDER_STATS;

typedef struct _PVRSRV_MEM_ALLOC_REC_
{
    PVRSRV_MEM_ALLOC_TYPE  eAllocType;
    IMG_VOID               *pvCpuVAddr;
    IMG_CPU_PHYADDR        sCpuPAddr;
	IMG_SIZE_T			   uiBytes;
    IMG_PVOID              pvPrivateData;

    struct _PVRSRV_MEM_ALLOC_REC_  *psNext;
	struct _PVRSRV_MEM_ALLOC_REC_  **ppsThis;
} PVRSRV_MEM_ALLOC_REC;

typedef struct _PVRSRV_MEMORY_STATS_ {
	/* Structure type (must be first!) */
	PVRSRV_STAT_STRUCTURE_TYPE  eStructureType;

	/* OS specific data */
	IMG_PVOID                   pvOSMemEntryData;

    /* Cached position in the linked list when obtaining all items... */
    IMG_UINT32                  ui32LastStatNumberRequested;
	PVRSRV_MEM_ALLOC_REC        *psLastStatMemoryRecordFound;

	/* Stats... */
	PVRSRV_MEM_ALLOC_REC        *psMemoryRecords;
} PVRSRV_MEMORY_STATS;

typedef struct _PVRSRV_RI_MEMORY_STATS_ {
	/* Structure type (must be first!) */
	PVRSRV_STAT_STRUCTURE_TYPE  eStructureType;

	/* OS level process ID */
	IMG_PID                   	pid;
	
	/* Handle used when querying the RI server... */
	IMG_HANDLE                 *pRIHandle;

	/* OS specific data */
	IMG_PVOID                   pvOSRIMemEntryData;
} PVRSRV_RI_MEMORY_STATS;

#if defined(PVRSRV_ENABLE_MEMORY_STATS)
static IMPLEMENT_LIST_INSERT(PVRSRV_MEM_ALLOC_REC)
static IMPLEMENT_LIST_REMOVE(PVRSRV_MEM_ALLOC_REC)
#endif


/*
 *  Global Boolean to flag when the statistics are ready to monitor
 *  memory allocations.
 */
static  IMG_BOOL  bProcessStatsInitialised = IMG_FALSE;

/*
 * Linked lists for process stats. Live stats are for processes which are still running
 * and the dead list holds those that have exited.
 */
static PVRSRV_PROCESS_STATS*  psLiveList = IMG_NULL;
static PVRSRV_PROCESS_STATS*  psDeadList = IMG_NULL;

POS_LOCK  psLinkedListLock = IMG_NULL;


/*
 * Pointer to OS folder to hold PID folders.
 */
IMG_CHAR*  pszOSPidFolderName = "pid";
IMG_PVOID  pvOSPidFolder      = IMG_NULL;


/*************************************************************************/ /*!
@Function       _RemoveRenderStatsFromList
@Description    Detaches a process from either the live or dead list.
@Input          psProcessStats  Process to remove the stats from.
@Input          psRenderStats   Render stats to remove.
*/ /**************************************************************************/
static IMG_VOID
_RemoveRenderStatsFromList(PVRSRV_PROCESS_STATS* psProcessStats,
                           PVRSRV_RENDER_STATS* psRenderStats)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);
	PVR_ASSERT(psRenderStats != IMG_NULL);

	/* Remove the item from the linked lists... */
	if (psProcessStats->psRenderLiveList == psRenderStats)
	{
		psProcessStats->psRenderLiveList = psRenderStats->psNext;

		if (psProcessStats->psRenderLiveList != IMG_NULL)
		{
			psProcessStats->psRenderLiveList->psPrev = IMG_NULL;
		}
	}
	else if (psProcessStats->psRenderDeadList == psRenderStats)
	{
		psProcessStats->psRenderDeadList = psRenderStats->psNext;

		if (psProcessStats->psRenderDeadList != IMG_NULL)
		{
			psProcessStats->psRenderDeadList->psPrev = IMG_NULL;
		}
	}
	else
	{
		PVRSRV_RENDER_STATS*  psNext = psRenderStats->psNext;
		PVRSRV_RENDER_STATS*  psPrev = psRenderStats->psPrev;

		if (psRenderStats->psNext != IMG_NULL)
		{
			psRenderStats->psNext->psPrev = psPrev;
		}
		if (psRenderStats->psPrev != IMG_NULL)
		{
			psRenderStats->psPrev->psNext = psNext;
		}
	}

	/* Reset the pointers in this cell, as it is not attached to anything */
	psRenderStats->psNext = IMG_NULL;
	psRenderStats->psPrev = IMG_NULL;
} /* _RemoveRenderStatsFromList */


/*************************************************************************/ /*!
@Function       _DestoryRenderStat
@Description    Frees memory and resources held by a render statistic.
@Input          psRenderStats  Render stats to destroy.
*/ /**************************************************************************/
static IMG_VOID
_DestoryRenderStat(PVRSRV_RENDER_STATS* psRenderStats)
{
	PVR_ASSERT(psRenderStats != IMG_NULL);

	/* Remove the statistic from the OS... */
	OSRemoveStatisticEntry(psRenderStats->pvOSData);

	/* Free the memory... */
	OSFreeMem(psRenderStats);
} /* _DestoryRenderStat */


/*************************************************************************/ /*!
@Function       _FindProcessStatsInLiveList
@Description    Searches the Live Process List for a statistics structure that
                matches the PID given.
@Input          pid  Process to search for.
@Return         Pointer to stats structure for the process.
*/ /**************************************************************************/
static PVRSRV_PROCESS_STATS*
_FindProcessStatsInLiveList(IMG_PID pid)
{
	PVRSRV_PROCESS_STATS*  psProcessStats = psLiveList;

	while (psProcessStats != IMG_NULL)
	{
		if (psProcessStats->pid == pid)
		{
			return psProcessStats;
		}

		psProcessStats = psProcessStats->psNext;
	}

	return IMG_NULL;
} /* _FindProcessStatsInLiveList */


/*************************************************************************/ /*!
@Function       _FindProcessStatsInDeadList
@Description    Searches the Dead Process List for a statistics structure that
                matches the PID given.
@Input          pid  Process to search for.
@Return         Pointer to stats structure for the process.
*/ /**************************************************************************/
static PVRSRV_PROCESS_STATS*
_FindProcessStatsInDeadList(IMG_PID pid)
{
	PVRSRV_PROCESS_STATS*  psProcessStats = psDeadList;

	while (psProcessStats != IMG_NULL)
	{
		if (psProcessStats->pid == pid)
		{
			return psProcessStats;
		}

		psProcessStats = psProcessStats->psNext;
	}

	return IMG_NULL;
} /* _FindProcessStatsInDeadList */


/*************************************************************************/ /*!
@Function       _FindProcessStats
@Description    Searches the Live and Dead Process Lists for a statistics
                structure that matches the PID given.
@Input          pid  Process to search for.
@Return         Pointer to stats structure for the process.
*/ /**************************************************************************/
static PVRSRV_PROCESS_STATS*
_FindProcessStats(IMG_PID pid)
{
	PVRSRV_PROCESS_STATS*  psProcessStats = _FindProcessStatsInLiveList(pid);

	if (psProcessStats == IMG_NULL)
	{
		psProcessStats = _FindProcessStatsInDeadList(pid);
	}

	return psProcessStats;
} /* _FindProcessStats */


/*************************************************************************/ /*!
@Function       _AddProcessStatsToFrontOfLiveList
@Description    Add a statistic to the live list head.
@Input          psProcessStats  Process stats to add.
*/ /**************************************************************************/
static IMG_VOID
_AddProcessStatsToFrontOfLiveList(PVRSRV_PROCESS_STATS* psProcessStats)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);

	if (psLiveList != IMG_NULL)
	{
		psLiveList->psPrev     = psProcessStats;
		psProcessStats->psNext = psLiveList;
	}

	psLiveList = psProcessStats;
} /* _AddProcessStatsToFrontOfLiveList */


/*************************************************************************/ /*!
@Function       _AddProcessStatsToFrontOfDeadList
@Description    Add a statistic to the dead list head.
@Input          psProcessStats  Process stats to add.
*/ /**************************************************************************/
static IMG_VOID
_AddProcessStatsToFrontOfDeadList(PVRSRV_PROCESS_STATS* psProcessStats)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);

	if (psDeadList != IMG_NULL)
	{
		psDeadList->psPrev     = psProcessStats;
		psProcessStats->psNext = psDeadList;
	}

	psDeadList = psProcessStats;
} /* _AddProcessStatsToFrontOfDeadList */


/*************************************************************************/ /*!
@Function       _RemoveProcessStatsFromList
@Description    Detaches a process from either the live or dead list.
@Input          psProcessStats  Process stats to remove.
*/ /**************************************************************************/
static IMG_VOID
_RemoveProcessStatsFromList(PVRSRV_PROCESS_STATS* psProcessStats)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);

	/* Remove the item from the linked lists... */
	if (psLiveList == psProcessStats)
	{
		psLiveList = psProcessStats->psNext;

		if (psLiveList != IMG_NULL)
		{
			psLiveList->psPrev = IMG_NULL;
		}
	}
	else if (psDeadList == psProcessStats)
	{
		psDeadList = psProcessStats->psNext;

		if (psDeadList != IMG_NULL)
		{
			psDeadList->psPrev = IMG_NULL;
		}
	}
	else
	{
		PVRSRV_PROCESS_STATS*  psNext = psProcessStats->psNext;
		PVRSRV_PROCESS_STATS*  psPrev = psProcessStats->psPrev;

		if (psProcessStats->psNext != IMG_NULL)
		{
			psProcessStats->psNext->psPrev = psPrev;
		}
		if (psProcessStats->psPrev != IMG_NULL)
		{
			psProcessStats->psPrev->psNext = psNext;
		}
	}

	/* Reset the pointers in this cell, as it is not attached to anything */
	psProcessStats->psNext = IMG_NULL;
	psProcessStats->psPrev = IMG_NULL;
} /* _RemoveProcessStatsFromList */


/*************************************************************************/ /*!
@Function       _DestoryProcessStat
@Description    Frees memory and resources held by a render statistic.
@Input          psProcessStats  Process stats to destroy.
*/ /**************************************************************************/
static IMG_VOID
_DestoryProcessStat(PVRSRV_PROCESS_STATS* psProcessStats)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);

	/* Remove this statistic from the OS... */
#if defined(PVR_RI_DEBUG)
	OSRemoveStatisticEntry(psProcessStats->psRIMemoryStats->pvOSRIMemEntryData);
#endif
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	OSRemoveStatisticEntry(psProcessStats->psMemoryStats->pvOSMemEntryData);
#endif
	OSRemoveStatisticEntry(psProcessStats->pvOSPidEntryData);
	OSRemoveStatisticFolder(psProcessStats->pvOSPidFolderData);

	/* Free the live and dead render statistic lists... */
	while (psProcessStats->psRenderLiveList != IMG_NULL)
	{
		PVRSRV_RENDER_STATS*  psRenderStats = psProcessStats->psRenderLiveList;

		_RemoveRenderStatsFromList(psProcessStats, psRenderStats);
		_DestoryRenderStat(psRenderStats);
	}

	while (psProcessStats->psRenderDeadList != IMG_NULL)
	{
		PVRSRV_RENDER_STATS*  psRenderStats = psProcessStats->psRenderDeadList;

		_RemoveRenderStatsFromList(psProcessStats, psRenderStats);
		_DestoryRenderStat(psRenderStats);
	}

	/* Free the memory statistics... */
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	while (psProcessStats->psMemoryStats->psMemoryRecords)
	{
		List_PVRSRV_MEM_ALLOC_REC_Remove(psProcessStats->psMemoryStats->psMemoryRecords);
	}
	OSFreeMem(psProcessStats->psMemoryStats);
#endif

	/* Free the memory... */
	OSFreeMem(psProcessStats);
} /* _DestoryProcessStat */


/*************************************************************************/ /*!
@Function       _CompressMemoryUsage
@Description    Reduces memory usage by deleting old statistics data.
                This function requires that the list lock is not held!
*/ /**************************************************************************/
static IMG_VOID
_CompressMemoryUsage(IMG_VOID)
{
	PVRSRV_PROCESS_STATS*  psProcessStats;
	PVRSRV_PROCESS_STATS*  psProcessStatsToBeFreed;
	IMG_UINT32  ui32ItemsRemaining;

	/*
	 *  We hold the lock whilst checking the list, but we'll release it
	 *  before freeing memory (as that will require the lock too)!
	 */
    OSLockAcquire(psLinkedListLock);

	/* Check that the dead list is not bigger than the max size... */
	psProcessStats          = psDeadList;
	psProcessStatsToBeFreed = IMG_NULL;
	ui32ItemsRemaining      = MAX_DEAD_LIST_PROCESSES;

	while (psProcessStats != IMG_NULL  &&  ui32ItemsRemaining > 0)
    {
		ui32ItemsRemaining--;
		if (ui32ItemsRemaining == 0)
		{
			/* This is the last allowed process, cut the linked list here! */
			psProcessStatsToBeFreed = psProcessStats->psNext;
			psProcessStats->psNext  = IMG_NULL;
		}
		else
		{
			psProcessStats = psProcessStats->psNext;
		}
	}

	OSLockRelease(psLinkedListLock);

	/* Any processes stats remaining will need to be destroyed... */
	while (psProcessStatsToBeFreed != IMG_NULL)
    {
		PVRSRV_PROCESS_STATS*  psNextProcessStats = psProcessStatsToBeFreed->psNext;

		psProcessStatsToBeFreed->psNext = IMG_NULL;
		_DestoryProcessStat(psProcessStatsToBeFreed);

		psProcessStatsToBeFreed = psNextProcessStats;
	}
} /* _CompressMemoryUsage */


static IMG_VOID
_MoveProcessFromLiveListToDeadList(PVRSRV_PROCESS_STATS* psProcessStats)
{
	/* Take the element out of the live list and append to the dead list... */
	_RemoveProcessStatsFromList(psProcessStats);
	_AddProcessStatsToFrontOfDeadList(psProcessStats);
} /* _MoveProcessFromLiveListToDeadList */


/*************************************************************************/ /*!
@Function       PVRSRVStatsInitialise
@Description    Entry point for initialising the statistics module.
@Return         Standard PVRSRV_ERROR error code.
*/ /**************************************************************************/
PVRSRV_ERROR
PVRSRVStatsInitialise(IMG_VOID)
{
    PVRSRV_ERROR error;

    PVR_ASSERT(psLiveList == IMG_NULL);
    PVR_ASSERT(psDeadList == IMG_NULL);
    PVR_ASSERT(psLinkedListLock == IMG_NULL);
	PVR_ASSERT(bProcessStatsInitialised == IMG_FALSE);

	/* We need a lock to protect the linked lists... */
    error = OSLockCreate(&psLinkedListLock, LOCK_TYPE_NONE);
    
    /* Create a pid folder for putting the PID files in... */
    pvOSPidFolder = OSCreateStatisticFolder(pszOSPidFolderName, IMG_NULL);

	/* Flag that we are ready to start monitoring memory allocations. */
	bProcessStatsInitialised = IMG_TRUE;
	
	return error;
} /* PVRSRVStatsInitialise */


/*************************************************************************/ /*!
@Function       PVRSRVStatsDestroy
@Description    Method for destroying the statistics module data.
*/ /**************************************************************************/
IMG_VOID
PVRSRVStatsDestroy(IMG_VOID)
{
	PVR_ASSERT(bProcessStatsInitialised == IMG_TRUE);

	/* Stop monitoring memory allocations... */
	bProcessStatsInitialised = IMG_FALSE;

	/* Destroy the lock... */
	if (psLinkedListLock != IMG_NULL)
	{
		OSLockDestroy(psLinkedListLock);
		psLinkedListLock = IMG_NULL;
	}

	/* Free the live and dead lists... */
    while (psLiveList != IMG_NULL)
    {
		PVRSRV_PROCESS_STATS*  psProcessStats = psLiveList;

		_RemoveProcessStatsFromList(psProcessStats);
		_DestoryProcessStat(psProcessStats);
	}

    while (psDeadList != IMG_NULL)
    {
		PVRSRV_PROCESS_STATS*  psProcessStats = psDeadList;

		_RemoveProcessStatsFromList(psProcessStats);
		_DestoryProcessStat(psProcessStats);
	}

	/* Remove the OS folder used by the PID folders... */
    OSRemoveStatisticFolder(pvOSPidFolder);
    pvOSPidFolder = IMG_NULL;
} /* PVRSRVStatsDestroy */


/*************************************************************************/ /*!
@Function       PVRSRVStatsRegisterProcess
@Description    Register a process into the list statistics list.
@Output         phProcessStats  Handle to the process to be used to deregister.
@Return         Standard PVRSRV_ERROR error code.
*/ /**************************************************************************/
PVRSRV_ERROR
PVRSRVStatsRegisterProcess(IMG_HANDLE* phProcessStats)
{
    PVRSRV_PROCESS_STATS*  psProcessStats;
    IMG_PID                currentPid = OSGetCurrentProcessIDKM();

    PVR_ASSERT(phProcessStats != IMG_NULL);

    /* Check the PID has not already moved to the dead list... */
	OSLockAcquire(psLinkedListLock);
	psProcessStats = _FindProcessStatsInDeadList(currentPid);
    if (psProcessStats != IMG_NULL)
    {
		/* Move it back onto the live list! */
		_RemoveProcessStatsFromList(psProcessStats);
		_AddProcessStatsToFrontOfLiveList(psProcessStats);
	}
	else
	{
		/* Check the PID is not already registered in the live list... */
		psProcessStats = _FindProcessStatsInLiveList(currentPid);
	}

	/* If the PID is on the live list then just increment the ref count and return... */
    if (psProcessStats != IMG_NULL)
    {
		psProcessStats->ui32RefCount++;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_CONNECTIONS] = psProcessStats->ui32RefCount;
		UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_CONNECTIONS],
		                 psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_CONNECTIONS]);
		OSLockRelease(psLinkedListLock);

		*phProcessStats = psProcessStats;
		return PVRSRV_OK;
	}
	OSLockRelease(psLinkedListLock);

	/* Allocate a new node structure and initialise it... */
	psProcessStats = OSAllocMem(sizeof(PVRSRV_PROCESS_STATS));
	if (psProcessStats == IMG_NULL)
	{
		*phProcessStats = 0;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSMemSet(psProcessStats, 0, sizeof(PVRSRV_PROCESS_STATS));

	psProcessStats->eStructureType  = PVRSRV_STAT_STRUCTURE_PROCESS;
	psProcessStats->pid             = currentPid;
	psProcessStats->ui32RefCount    = 1;

	psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_CONNECTIONS]     = 1;
	psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_CONNECTIONS] = 1;

#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	psProcessStats->psMemoryStats = OSAllocMem(sizeof(PVRSRV_MEMORY_STATS));
	if (psProcessStats->psMemoryStats == IMG_NULL)
	{
		OSFreeMem(psProcessStats);
		*phProcessStats = 0;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSMemSet(psProcessStats->psMemoryStats, 0, sizeof(PVRSRV_MEMORY_STATS));
	psProcessStats->psMemoryStats->eStructureType = PVRSRV_STAT_STRUCTURE_MEMORY;
#endif

#if defined(PVR_RI_DEBUG)
	psProcessStats->psRIMemoryStats = OSAllocMem(sizeof(PVRSRV_RI_MEMORY_STATS));
	if (psProcessStats->psRIMemoryStats == IMG_NULL)
	{
		OSFreeMem(psProcessStats->psMemoryStats);
		OSFreeMem(psProcessStats);
		*phProcessStats = 0;
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSMemSet(psProcessStats->psRIMemoryStats, 0, sizeof(PVRSRV_RI_MEMORY_STATS));
	psProcessStats->psRIMemoryStats->eStructureType = PVRSRV_STAT_STRUCTURE_RIMEMORY;
	psProcessStats->psRIMemoryStats->pid            = currentPid;
#endif

	/* Add it to the live list... */
    OSLockAcquire(psLinkedListLock);
	_AddProcessStatsToFrontOfLiveList(psProcessStats);
	OSLockRelease(psLinkedListLock);

	/* Create the process stat in the OS... */
	OSSNPrintf(psProcessStats->szFolderName, sizeof(psProcessStats->szFolderName),
	           "%d", currentPid);

	psProcessStats->pvOSPidFolderData = OSCreateStatisticFolder(psProcessStats->szFolderName, pvOSPidFolder);
	psProcessStats->pvOSPidEntryData  = OSCreateStatisticEntry("process_stats",
	                                                           psProcessStats->pvOSPidFolderData,
	                                                           PVRSRVStatsObtainElement,
	                                                           (IMG_PVOID) psProcessStats);

#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	psProcessStats->psMemoryStats->pvOSMemEntryData = OSCreateStatisticEntry("mem_area",
	                                                           psProcessStats->pvOSPidFolderData,
	                                                           PVRSRVStatsObtainElement,
	                                                           (IMG_PVOID) psProcessStats->psMemoryStats);
#endif

#if defined(PVR_RI_DEBUG)
	psProcessStats->psRIMemoryStats->pvOSRIMemEntryData = OSCreateStatisticEntry("ri_mem_area",
	                                                           psProcessStats->pvOSPidFolderData,
	                                                           PVRSRVStatsObtainElement,
	                                                           (IMG_PVOID) psProcessStats->psRIMemoryStats);
#endif

	/* Done */
	*phProcessStats = (IMG_HANDLE) psProcessStats;

	return PVRSRV_OK;
} /* PVRSRVStatsRegisterProcess */


/*************************************************************************/ /*!
@Function       PVRSRVStatsDeregisterProcess
@Input          hProcessStats  Handle to the process returned when registered.
@Description    Method for destroying the statistics module data.
*/ /**************************************************************************/
IMG_VOID
PVRSRVStatsDeregisterProcess(IMG_HANDLE hProcessStats)
{
	if (hProcessStats != 0)
	{
		PVRSRV_PROCESS_STATS*  psProcessStats = (PVRSRV_PROCESS_STATS*) hProcessStats;

		/* Lower the reference count, if zero then move it to the dead list */
		OSLockAcquire(psLinkedListLock);
		if (psProcessStats->ui32RefCount > 0)
		{
			psProcessStats->ui32RefCount--;
			psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_CONNECTIONS] = psProcessStats->ui32RefCount;

			if (psProcessStats->ui32RefCount == 0)
			{
				_MoveProcessFromLiveListToDeadList(psProcessStats);
			}
		}
		OSLockRelease(psLinkedListLock);

		/* Check if the dead list needs to be reduced */
		_CompressMemoryUsage();
	}
} /* PVRSRVStatsDeregisterProcess */


IMG_VOID
PVRSRVStatsAddMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE eAllocType,
                             IMG_VOID *pvCpuVAddr,
                             IMG_CPU_PHYADDR sCpuPAddr,
                             IMG_SIZE_T uiBytes,
                             IMG_PVOID pvPrivateData)
{
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	IMG_PID                currentPid = OSGetCurrentProcessIDKM();
    PVRSRV_MEM_ALLOC_REC*  psRecord   = IMG_NULL;
    PVRSRV_PROCESS_STATS*  psProcessStats;
    PVRSRV_MEMORY_STATS*   psMemoryStats;

    /* Don't do anything if we are not initialised or we are shutting down! */
    if (!bProcessStatsInitialised)
    {
		return;
	}

	/*
	 *  To prevent a recursive loop, we don't store the memory allocations
	 *  which come from our own OSAllocMem() as that would just call
	 *  OSAllocMem() again!!! Instead we just update the usage counter.
	 */
	if (eAllocType != PVRSRV_MEM_ALLOC_TYPE_KMALLOC  ||
	    uiBytes != sizeof(PVRSRV_MEM_ALLOC_REC))
	{
		/* Allocate the memory record... */
		psRecord = OSAllocMem(sizeof(PVRSRV_MEM_ALLOC_REC));
		if (psRecord == IMG_NULL)
		{
			return;
		}

		OSMemSet(psRecord, 0, sizeof(PVRSRV_MEM_ALLOC_REC));
		psRecord->eAllocType       = eAllocType;
		psRecord->pvCpuVAddr       = pvCpuVAddr;
		psRecord->sCpuPAddr.uiAddr = sCpuPAddr.uiAddr;
		psRecord->uiBytes          = uiBytes;
		psRecord->pvPrivateData    = pvPrivateData;
	}

	/* Lock while we find the correct process... */
	OSLockAcquire(psLinkedListLock);

    psProcessStats = _FindProcessStats(currentPid);
    if (psProcessStats == IMG_NULL)
    {
		OSLockRelease(psLinkedListLock);
		if (psRecord != IMG_NULL)
		{
			OSFreeMem(psRecord);
		}
		return;
	}
	psMemoryStats = psProcessStats->psMemoryStats;

	/* Insert the memory record... */
	if (psRecord != IMG_NULL)
	{
		psMemoryStats->ui32LastStatNumberRequested = 0;
		psMemoryStats->psLastStatMemoryRecordFound = IMG_NULL;
		List_PVRSRV_MEM_ALLOC_REC_Insert(&psMemoryStats->psMemoryRecords, psRecord);
	}

	/* Update the memory watermarks... */
	switch (eAllocType)
	{
		case PVRSRV_MEM_ALLOC_TYPE_KMALLOC:
		{
			psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] += uiBytes;
			UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_KMALLOC],
							 psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC]);
		}
		break;
		
		case PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES:
		{
			psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES] += uiBytes;
			UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_ALLOC_PAGES],
							 psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES]);
		}
		break;
		
		case PVRSRV_MEM_ALLOC_TYPE_IOREMAP:
		{
			psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_IOREMAP] += uiBytes;
			UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_IOREMAP],
							 psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_IOREMAP]);
		}
		break;
		
		case PVRSRV_MEM_ALLOC_TYPE_VMAP:
		{
			psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_VMAP] += uiBytes;
			UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_MAX_VMAP],
							 psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_VMAP]);
		}
		break;
		
		default:
		{
			PVR_ASSERT(0);
		}
		break;
	}

	OSLockRelease(psLinkedListLock);
#else
PVR_UNREFERENCED_PARAMETER(eAllocType);
PVR_UNREFERENCED_PARAMETER(pvCpuVAddr);
PVR_UNREFERENCED_PARAMETER(sCpuPAddr);
PVR_UNREFERENCED_PARAMETER(uiBytes);
PVR_UNREFERENCED_PARAMETER(pvPrivateData);
#endif
} /* PVRSRVStatsAddMemAllocRecord */


IMG_VOID
PVRSRVStatsRemoveMemAllocRecord(PVRSRV_MEM_ALLOC_TYPE eAllocType,
                                IMG_VOID *pvCpuVAddr)
{
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	IMG_PID                currentPid = OSGetCurrentProcessIDKM();
	PVRSRV_MEM_ALLOC_REC*  psRecord   = IMG_NULL;
    PVRSRV_PROCESS_STATS*  psProcessStats;

    /* Don't do anything if we are not initialised or we are shutting down! */
    if (!bProcessStatsInitialised)
    {
		return;
	}

	/* Lock while we find the correct process and remove this record... */
	OSLockAcquire(psLinkedListLock);

    psProcessStats = _FindProcessStats(currentPid);
    if (psProcessStats != IMG_NULL)
    {
		PVRSRV_MEMORY_STATS*   psMemoryStats = psProcessStats->psMemoryStats;

		psRecord = psMemoryStats->psMemoryRecords;
		while (psRecord != IMG_NULL)
		{
			if (psRecord->pvCpuVAddr == pvCpuVAddr  &&  psRecord->eAllocType == eAllocType)
			{
				/* Update the watermark and remove this record...*/
				switch (eAllocType)
				{
					case PVRSRV_MEM_ALLOC_TYPE_KMALLOC:
					{
						if (psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] >= psRecord->uiBytes)
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] -= psRecord->uiBytes;
						}
						else
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] = 0;
						}
					}
					break;
					
					case PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES:
					{
						if (psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES] >= psRecord->uiBytes)
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES] -= psRecord->uiBytes;
						}
						else
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ALLOC_PAGES] = 0;
						}
					}
					break;
					
					case PVRSRV_MEM_ALLOC_TYPE_IOREMAP:
					{
						if (psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_IOREMAP] >= psRecord->uiBytes)
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_IOREMAP] -= psRecord->uiBytes;
						}
						else
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_IOREMAP] = 0;
						}
					}
					break;
					
					case PVRSRV_MEM_ALLOC_TYPE_VMAP:
					{
						if (psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_VMAP] >= psRecord->uiBytes)
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_VMAP] -= psRecord->uiBytes;
						}
						else
						{
							psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_VMAP] = 0;
						}
					}
					break;
					
					default:
					{
						PVR_ASSERT(0);
					}
					break;
				}

				psMemoryStats->ui32LastStatNumberRequested = 0;
				psMemoryStats->psLastStatMemoryRecordFound = IMG_NULL;
				List_PVRSRV_MEM_ALLOC_REC_Remove(psRecord);
				break;
			}

			psRecord = psRecord->psNext;
		}
		
		if (psRecord == IMG_NULL  &&
		    eAllocType == PVRSRV_MEM_ALLOC_TYPE_KMALLOC)
		{
			/* Most likely this is one of our own data structures... */
			if (psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] >= sizeof(PVRSRV_MEM_ALLOC_REC))
			{
				psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] -= sizeof(PVRSRV_MEM_ALLOC_REC);
			}
			else
			{
				psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_KMALLOC] = 0;
			}
		}
	}

	OSLockRelease(psLinkedListLock);

	/*
	 * Free the record outside the lock so we don't deadlock and so we
	 * reduce the time the lock is held.
	 */
	if (psRecord != IMG_NULL)
	{
		OSFreeMem(psRecord);
	}
#else
PVR_UNREFERENCED_PARAMETER(eAllocType);
PVR_UNREFERENCED_PARAMETER(pvCpuVAddr);
#endif
} /* PVRSRVStatsRemoveMemAllocRecord */


IMG_VOID
PVRSRVStatsUpdateRenderContextStats(IMG_UINT32 ui32TotalNumPartialRenders,
                                    IMG_UINT32 ui32TotalNumOutOfMemory,
                                    IMG_UINT32 ui32NumTAStores,
                                    IMG_UINT32 ui32Num3DStores,
                                    IMG_UINT32 ui32NumSHStores)
{
	IMG_PID                currentPid = OSGetCurrentProcessIDKM();
    PVRSRV_PROCESS_STATS*  psProcessStats;

    /* Don't do anything if we are not initialised or we are shutting down! */
    if (!bProcessStatsInitialised)
    {
		return;
	}

	/* Lock while we find the correct process and update the record... */
	OSLockAcquire(psLinkedListLock);

    psProcessStats = _FindProcessStats(currentPid);
    if (psProcessStats != IMG_NULL)
    {
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_RC_PRS]       += ui32TotalNumPartialRenders;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_RC_OOMS]      += ui32TotalNumOutOfMemory;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_RC_TA_STORES] += ui32NumTAStores;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_RC_3D_STORES] += ui32Num3DStores;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_RC_SH_STORES] += ui32NumSHStores;
	}

	OSLockRelease(psLinkedListLock);
} /* PVRSRVStatsUpdateRenderContextStats */


IMG_VOID
PVRSRVStatsUpdateZSBufferStats(IMG_UINT32 ui32NumReqByApp,
                               IMG_UINT32 ui32NumReqByFW)
{
	IMG_PID                currentPid = OSGetCurrentProcessIDKM();
    PVRSRV_PROCESS_STATS*  psProcessStats;

    /* Don't do anything if we are not initialised or we are shutting down! */
    if (!bProcessStatsInitialised)
    {
		return;
	}

	/* Lock while we find the correct process and update the record... */
	OSLockAcquire(psLinkedListLock);

    psProcessStats = _FindProcessStats(currentPid);
    if (psProcessStats != IMG_NULL)
    {
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_APP] += ui32NumReqByApp;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_ZSBUFFER_REQS_BY_FW]  += ui32NumReqByFW;
	}

	OSLockRelease(psLinkedListLock);
} /* PVRSRVStatsUpdateZSBufferStats */


IMG_VOID
PVRSRVStatsUpdateFreelistStats(IMG_UINT32 ui32NumGrowReqByApp,
                               IMG_UINT32 ui32NumGrowReqByFW,
                               IMG_UINT32 ui32InitFLPages,
                               IMG_UINT32 ui32NumHighPages)
{
	IMG_PID                currentPid = OSGetCurrentProcessIDKM();
    PVRSRV_PROCESS_STATS*  psProcessStats;

    /* Don't do anything if we are not initialised or we are shutting down! */
    if (!bProcessStatsInitialised)
    {
		return;
	}

	/* Lock while we find the correct process and update the record... */
	OSLockAcquire(psLinkedListLock);

    psProcessStats = _FindProcessStats(currentPid);
    if (psProcessStats != IMG_NULL)
    {
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_APP] += ui32NumGrowReqByApp;
		psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_FREELIST_GROW_REQS_BY_FW]  += ui32NumGrowReqByFW;
		UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_FREELIST_PAGES_INIT],
		                 ui32InitFLPages);
		UPDATE_MAX_VALUE(psProcessStats->i32StatValue[PVRSRV_PROCESS_STAT_TYPE_FREELIST_MAX_PAGES],
		                 ui32NumHighPages);
	}

	OSLockRelease(psLinkedListLock);
} /* PVRSRVStatsUpdateFreelistStats */


/*************************************************************************/ /*!
@Function       _ObtainProcessStatistic
@Description    Returns a specific statistic number for a process.
@Input          psProcessStats  Process statistics to examine.
@Input          ui32StatNumber  Index of the statistic to return.
@Output         pi32StatData    Statistic value.
@Output         peStatistic     Statistic type being returned.
@Output         pszStatFmtText  Pointer to formatting string for the stat.
@Return         IMG_TRUE if a statistic is returned, IMG_FALSE if no statistic
                is available (and this also implies there are no more to get).
*/ /**************************************************************************/
static IMG_BOOL
_ObtainProcessStatistic(PVRSRV_PROCESS_STATS* psProcessStats,
                        IMG_UINT32 ui32StatNumber,
                        IMG_INT32* pi32StatData,
                        IMG_CHAR** ppszStatFmtText)
{
	PVR_ASSERT(psProcessStats != IMG_NULL);
	PVR_ASSERT(pi32StatData != IMG_NULL);

	if (ui32StatNumber < PVRSRV_PROCESS_STAT_TYPE_COUNT)
	{
		*pi32StatData = psProcessStats->i32StatValue[ui32StatNumber];
		
		if (ppszStatFmtText != IMG_NULL)
		{
			*ppszStatFmtText = pszProcessStatFmt[ui32StatNumber];
		}

		return IMG_TRUE;
	}

	return IMG_FALSE;
} /* _ObtainProcessStatistic */


#if defined(PVRSRV_ENABLE_MEMORY_STATS)
/*************************************************************************/ /*!
@Function       _ObtainMemoryStatistic
@Description    Returns a specific statistic number for a memory area.
@Input          psMemoryStats   Memory statistics to examine.
@Input          ui32StatNumber  Index of the statistic to return.
@Output         pi32StatData    Statistic value.
@Output         pszStatFmtText  Pointer to formatting string for the stat.
@Return         IMG_TRUE if a statistic is returned, IMG_FALSE if no statistic
                is available (and this also implies there are no more to get).
*/ /**************************************************************************/
static IMG_BOOL
_ObtainMemoryStatistic(PVRSRV_MEMORY_STATS* psMemoryStats,
                       IMG_UINT32 ui32StatNumber,
                       IMG_INT32* pi32StatData,
                       IMG_CHAR** ppszStatFmtText)
{
	PVRSRV_MEM_ALLOC_REC  *psRecord;
	IMG_BOOL              found = IMG_FALSE;
	IMG_UINT32            ui32ItemNumber, ui32VAddrFields, ui32PAddrFields;

	PVR_ASSERT(psMemoryStats != IMG_NULL);
	PVR_ASSERT(pi32StatData != IMG_NULL);

	/*
	 *  Displaying the record is achieved by passing back 32bit chunks of data and
	 *  appropriate formatting for it. Therefore the ui32StatNumber is divided by the
	 *  number of items to be displayed.
	 *
	 *  sCpuPAddr.uiAddr is assumed to be a multiple of 32bits.
	 */
	ui32VAddrFields = sizeof(IMG_VOID*)/sizeof(IMG_UINT32);
	ui32PAddrFields = sizeof(IMG_CPU_PHYADDR)/sizeof(IMG_UINT32);
	ui32ItemNumber  = ui32StatNumber % (2 + ui32VAddrFields + ui32PAddrFields);
	ui32StatNumber  = ui32StatNumber / (2 + ui32VAddrFields + ui32PAddrFields);
	
	/* Write the header... */
	if (ui32StatNumber == 0)
	{
		if (ui32ItemNumber == 0)
		{
			*pi32StatData    = 0;
			*ppszStatFmtText = "Type         ";
		}
		else if (ui32ItemNumber-1 < ui32VAddrFields)
		{
			*pi32StatData    = 0;
			*ppszStatFmtText = (ui32ItemNumber-1 == 0 ? "VAddress  " : "        ");
		}
		else if (ui32ItemNumber-1-ui32VAddrFields < ui32PAddrFields)
		{
			*pi32StatData    = 0;
			*ppszStatFmtText = (ui32ItemNumber-1-ui32VAddrFields == 0 ? "PAddress  " : "        ");
		}
		else if (ui32ItemNumber == ui32PAddrFields+2)
		{
			*pi32StatData    = 0;
			*ppszStatFmtText = "Size(bytes)\n";
		}
		
		return IMG_TRUE;
	}

	/* The lock has to be held whilst moving through the memory list... */
	OSLockAcquire(psLinkedListLock);
	
	/* Check if we have a cached position... */
	if (psMemoryStats->ui32LastStatNumberRequested == ui32StatNumber  &&
		psMemoryStats->psLastStatMemoryRecordFound != IMG_NULL)
	{
		psRecord = psMemoryStats->psLastStatMemoryRecordFound;
	}
	else if (psMemoryStats->ui32LastStatNumberRequested == ui32StatNumber-1  &&
	         psMemoryStats->psLastStatMemoryRecordFound != IMG_NULL)
	{
		psRecord = psMemoryStats->psLastStatMemoryRecordFound->psNext;
	}
	else
	{
		psRecord = psMemoryStats->psMemoryRecords;
		while (psRecord != IMG_NULL  &&  ui32StatNumber > 1)
		{
			psRecord = psRecord->psNext;
			ui32StatNumber--;
		}
	}

	psMemoryStats->ui32LastStatNumberRequested = ui32StatNumber;
	psMemoryStats->psLastStatMemoryRecordFound = psRecord;
	
	/* Was a record found? */
	if (psRecord != IMG_NULL)
	{
		if (ui32ItemNumber == 0)
		{
			*pi32StatData    = 0;
			switch (psRecord->eAllocType)
			{
				case PVRSRV_MEM_ALLOC_TYPE_KMALLOC:      *ppszStatFmtText = "KMALLOC      "; break;
				case PVRSRV_MEM_ALLOC_TYPE_ALLOC_PAGES:  *ppszStatFmtText = "ALLOC_PAGES  "; break;
				case PVRSRV_MEM_ALLOC_TYPE_IOREMAP:      *ppszStatFmtText = "IOREMAP      "; break;
				case PVRSRV_MEM_ALLOC_TYPE_VMAP:         *ppszStatFmtText = "VMAP         "; break;
				default:                                 *ppszStatFmtText = "INVALID      "; break;
			}
		}
		else if (ui32ItemNumber-1 < ui32VAddrFields)
		{
			*pi32StatData    = *(((IMG_UINT32*) &psRecord->pvCpuVAddr) + ui32VAddrFields - (ui32ItemNumber-1) - 1);
			*ppszStatFmtText = (ui32ItemNumber-1 == ui32VAddrFields-1 ? "%08x  " : "%08x");
		}
		else if (ui32ItemNumber-1-ui32VAddrFields < ui32PAddrFields)
		{
			*pi32StatData    = *(((IMG_UINT32*) &psRecord->sCpuPAddr.uiAddr) + ui32PAddrFields - (ui32ItemNumber-1-ui32VAddrFields) - 1);
			*ppszStatFmtText = (ui32ItemNumber-1-ui32VAddrFields == ui32PAddrFields-1 ? "%08x  " : "%08x");
		}
		else if (ui32ItemNumber == ui32PAddrFields+2)
		{
			*pi32StatData    = psRecord->uiBytes;
			*ppszStatFmtText = "%u\n";
		}

		found = IMG_TRUE;
	}

	OSLockRelease(psLinkedListLock);

	return found;
} /* _ObtainMemoryStatistic */
#endif


#if defined(PVR_RI_DEBUG)
/*************************************************************************/ /*!
@Function       _ObtainRIMemoryStatistic
@Description    Returns a specific RI MEMDESC entry.
@Input          psMemoryStats   Memory statistics to examine.
@Input          ui32StatNumber  Index of the statistic to return.
@Output         pi32StatData    Statistic value.
@Output         pszStatFmtText  Pointer to formatting string for the stat.
@Return         IMG_TRUE if a statistic is returned, IMG_FALSE if no statistic
                is available (and this also implies there are no more to get).
*/ /**************************************************************************/
static IMG_BOOL
_ObtainRIMemoryStatistic(PVRSRV_RI_MEMORY_STATS* psRIMemoryStats,
                         IMG_UINT32 ui32StatNumber,
                         IMG_INT32* pi32StatData,
                         IMG_CHAR** ppszStatFmtText)
{
	PVR_ASSERT(psRIMemoryStats != IMG_NULL);

	PVR_UNREFERENCED_PARAMETER(pi32StatData);

	/*
	 * If this request is for the first item then set the handle to null.
	 * If it is not the first request, then the handle should not be null
	 * unless this is the OS's way of double checking that the previous
	 * request, which returned IMG_FALSE was indeed the last request.
	 */
	if (ui32StatNumber == 0)
	{
		psRIMemoryStats->pRIHandle = IMG_NULL;
	}
	else if (psRIMemoryStats->pRIHandle == IMG_NULL)
	{
		return IMG_FALSE;
	}
	
	return RIGetListEntryKM(psRIMemoryStats->pid,
	                        &psRIMemoryStats->pRIHandle,
	                        ppszStatFmtText);
} /* _ObtainRIMemoryStatistic */
#endif


/*************************************************************************/ /*!
@Function       PVRSRVStatsObtainElement
@Description    Returns a specific statistic number for a process. Clients are
                expected to iterate through the statistics by requesting stat
                0,1,2,3,4,etc. and stopping when FALSE is returned.
@Input          pvStatPtr       Pointer to statistics structure.
@Input          ui32StatNumber  Index of the statistic to return.
@Output         pi32StatData    Statistic value.
@Output         pszStatFmtText  Pointer to formatting string for the stat.
@Return         IMG_TRUE if a statistic is returned, IMG_FALSE if no statistic
                is available (and this also implies there are no more to get).
*/ /**************************************************************************/
IMG_BOOL
PVRSRVStatsObtainElement(IMG_PVOID pvStatPtr, IMG_UINT32 ui32StatNumber,
                         IMG_INT32* pi32StatData, IMG_CHAR** ppszStatFmtText)
{
	PVRSRV_STAT_STRUCTURE_TYPE*  peStructureType = (PVRSRV_STAT_STRUCTURE_TYPE*) pvStatPtr;

	if (peStructureType == IMG_NULL  ||  pi32StatData == IMG_NULL)
	{
		return IMG_FALSE;
	}

	if (*peStructureType == PVRSRV_STAT_STRUCTURE_PROCESS)
	{
		PVRSRV_PROCESS_STATS*  psProcessStats = (PVRSRV_PROCESS_STATS*) pvStatPtr;

		return _ObtainProcessStatistic(psProcessStats, ui32StatNumber,
									   pi32StatData, ppszStatFmtText);
	}
#if defined(PVRSRV_ENABLE_MEMORY_STATS)
	else if (*peStructureType == PVRSRV_STAT_STRUCTURE_MEMORY)
	{
		PVRSRV_MEMORY_STATS*  psMemoryStats = (PVRSRV_MEMORY_STATS*) pvStatPtr;

		return _ObtainMemoryStatistic(psMemoryStats, ui32StatNumber,
									  pi32StatData, ppszStatFmtText);
	}
#endif
#if defined(PVR_RI_DEBUG)
	else if (*peStructureType == PVRSRV_STAT_STRUCTURE_RIMEMORY)
	{
		PVRSRV_RI_MEMORY_STATS*  psRIMemoryStats = (PVRSRV_RI_MEMORY_STATS*) pvStatPtr;

		return _ObtainRIMemoryStatistic(psRIMemoryStats, ui32StatNumber,
									    pi32StatData, ppszStatFmtText);
	}
#endif

	/* Stat type not handled probably indicates bad pointers... */
	PVR_ASSERT(IMG_FALSE);

	return IMG_FALSE;
} /* PVRSRVStatsObtainElement */


