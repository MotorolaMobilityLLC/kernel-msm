/*************************************************************************/ /*!
@File
@Title          Resource Allocator
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

@Description
 Implements generic resource allocation. The resource
 allocator was originally intended to manage address spaces.  In
 practice the resource allocator is generic and can manage arbitrary
 sets of integers.

 Resources are allocated from arenas. Arena's can be created with an
 initial span of resources. Further resources spans can be added to
 arenas. A call back mechanism allows an arena to request further
 resource spans on demand.

 Each arena maintains an ordered list of resource segments each
 described by a boundary tag. Each boundary tag describes a segment
 of resources which are either 'free', available for allocation, or
 'busy' currently allocated. Adjacent 'free' segments are always
 coallesced to avoid fragmentation.

 For allocation, all 'free' segments are kept on lists of 'free'
 segments in a table index by pvr_log2(segment size). ie Each table index
 n holds 'free' segments in the size range 2**(n-1) -> 2**n.

 Allocation policy is based on an *almost* best fit
 stratedy. Choosing any segment from the appropriate table entry
 guarantees that we choose a segment which is with a power of 2 of
 the size we are allocating.

 Allocated segments are inserted into a self scaling hash table which
 maps the base resource of the span to the relevant boundary
 tag. This allows the code to get back to the bounary tag without
 exporting explicit boundary tag references through the API.

 Each arena has an associated quantum size, all allocations from the
 arena are made in multiples of the basic quantum.

 On resource exhaustion in an arena, a callback if provided will be
 used to request further resources. Resouces spans allocated by the
 callback mechanism are delimited by special boundary tag markers of
 zero span, 'span' markers. Span markers are never coallesced. Span
 markers are used to detect when an imported span is completely free
 and can be deallocated by the callback mechanism.
*/ /**************************************************************************/

/* Issues:
 * - flags, flags are passed into the resource allocator but are not currently used.
 * - determination, of import size, is currently braindead.
 * - debug code should be moved out to own module and #ifdef'd
 */

#include "img_types.h"
#include "pvr_debug.h"
#include "pvrsrv_error.h"


#include "hash.h"
#include "ra.h"

#include "osfunc.h"
#include "allocmem.h"
#include "lock.h"

/* The initial, and minimum size of the live address -> boundary tag
   structure hash table. The value 64 is a fairly arbitrary
   choice. The hash table resizes on demand so the value choosen is
   not critical. */
#define MINIMUM_HASH_SIZE (64)

#if defined(VALIDATE_ARENA_TEST)

/* This test validates the doubly linked ordered list of boundary tags, by
checking that adjacent members of the list have compatible eResourceSpan
and eResourceType values. */

typedef enum RESOURCE_DESCRIPTOR_TAG {

	RESOURCE_SPAN_LIVE				= 10,
	RESOURCE_SPAN_FREE,
	IMPORTED_RESOURCE_SPAN_START,
	IMPORTED_RESOURCE_SPAN_LIVE,
	IMPORTED_RESOURCE_SPAN_FREE,
	IMPORTED_RESOURCE_SPAN_END,

} RESOURCE_DESCRIPTOR;

typedef enum RESOURCE_TYPE_TAG {

	IMPORTED_RESOURCE_TYPE		= 20,
	NON_IMPORTED_RESOURCE_TYPE

} RESOURCE_TYPE;


static IMG_UINT32 ui32BoundaryTagID = 0;

IMG_UINT32 ValidateArena(RA_ARENA *pArena);
#endif

/* We store private data in the BTs per "import".  We shouldn't care
   about initialising it at all, but doing so may help to track bugs,
   so we want to write a greppable value to it.  NB: NULL is useless,
   since it is a valid handle.  So is this, potentially, but as long
   as we never compare with it in code, we should be safe */
#define RA_UNINITED_PRIV ((IMG_HANDLE)(IMG_UINTPTR_T)0xdeade023U)

/* boundary tags, used to describe a resource segment */
struct _BT_
{
	enum bt_type
	{
		btt_span,				/* span markers */
		btt_free,				/* free resource segment */
		btt_live				/* allocated resource segment */
	} type;

	/* The base resource and extent of this segment */
	RA_BASE_T base;
	RA_LENGTH_T uSize;

	/* doubly linked ordered list of all segments within the arena */
	struct _BT_ *pNextSegment;
	struct _BT_ *pPrevSegment;
	/* doubly linked un-ordered list of free segments. */
	struct _BT_ *pNextFree;
	struct _BT_ *pPrevFree;
	/* a user reference associated with this span, user references are
	 * currently only provided in the callback mechanism */
    IMG_HANDLE hPriv;

    /* Flags to match on this span */
    IMG_UINT32 uFlags;

#if defined(VALIDATE_ARENA_TEST)
	RESOURCE_DESCRIPTOR eResourceSpan;
	RESOURCE_TYPE		eResourceType;

	/* This variable provides a reference (used in debug messages) to incompatible
	boundary tags within the doubly linked ordered list. */
	IMG_UINT32			ui32BoundaryTagID;
#endif
};
typedef struct _BT_ BT;


/* resource allocation arena */
struct _RA_ARENA_
{
	/* arena name for diagnostics output */
	IMG_CHAR *name;

	/* allocations within this arena are quantum sized */
	RA_LENGTH_T uQuantum;

	/* import interface, if provided */
	IMG_BOOL (*pImportAlloc)(RA_PERARENA_HANDLE h,
							 RA_LENGTH_T uSize,
							 IMG_UINT32 uFlags,
							 RA_BASE_T *pBase,
							 RA_LENGTH_T *pActualSize,
                             RA_PERISPAN_HANDLE *phPriv);
	IMG_VOID (*pImportFree) (RA_PERARENA_HANDLE,
                             RA_BASE_T,
                             RA_PERISPAN_HANDLE hPriv);

	/* arbitrary handle provided by arena owner to be passed into the
	 * import alloc and free hooks */
	IMG_VOID *pImportHandle;

    /* The BM-specific check-free-space stuff has been removed - so if
       anyone wants to reintroduce it, please do so by supplying a
       suitable callback: */
    IMG_VOID (*pfnPreAllocCheck)(IMG_VOID);

	/* head of list of free boundary tags for indexed by pvr_log2 of the
	   boundary tag size */
#define FREE_TABLE_LIMIT 40

	/* power-of-two table of free lists */
	BT *aHeadFree [FREE_TABLE_LIMIT];

	/* resource ordered segment list */
	BT *pHeadSegment;
	BT *pTailSegment;

	/* segment address to boundary tag hash table */
	HASH_TABLE *pSegmentHash;

	/* Lock for this arena */
	POS_LOCK hLock;
};
/* #define ENABLE_RA_DUMP	1 */
#if defined(ENABLE_RA_DUMP)
IMG_VOID RA_Dump (RA_ARENA *pArena);
#endif
/* #define ENABLE_RA_LIST_ASSERTS	1 */
#if defined(ENABLE_RA_LIST_ASSERTS)
#define RA_LIST_ASSERT(cond)    PVR_ASSERT(cond)
#else
#define RA_LIST_ASSERT(cond)
#endif

/*************************************************************************/ /*!
@Function       _RequestAllocFail
@Description    Default callback allocator used if no callback is
                specified, always fails to allocate further resources to the
                arena.
@Input          _h - callback handle
@Input          _uSize - requested allocation size
@Output         _pActualSize - actual allocation size
@Input          _pRef - user reference
@Input          _uflags - allocation flags
@Input          _pBase - receives allocated base
@Return         IMG_FALSE, this function always fails to allocate.
*/ /**************************************************************************/
static IMG_BOOL
_RequestAllocFail (RA_PERARENA_HANDLE _h,
                   RA_LENGTH_T _uSize,
                   IMG_UINT32 _uFlags,
                   RA_BASE_T *_pBase,
                   RA_LENGTH_T *_pActualSize,
                   RA_PERISPAN_HANDLE *_phPriv)
{
	PVR_UNREFERENCED_PARAMETER (_h);
	PVR_UNREFERENCED_PARAMETER (_uSize);
	PVR_UNREFERENCED_PARAMETER (_pActualSize);
	PVR_UNREFERENCED_PARAMETER (_phPriv);
	PVR_UNREFERENCED_PARAMETER (_uFlags);
	PVR_UNREFERENCED_PARAMETER (_pBase);

	return IMG_FALSE;
}

/*************************************************************************/ /*!
@Function       pvr_log2
@Description    Computes the floor of the log base 2 of a unsigned integer
@Input          n       Unsigned integer
@Return         Floor(Log2(n))
*/ /**************************************************************************/
static IMG_UINT32
pvr_log2 (RA_LENGTH_T n)
{
	IMG_UINT32 l = 0;
	n>>=1;
	while (n>0)
	{
		n>>=1;
		l++;
	}
	return l;
}

/*************************************************************************/ /*!
@Function       _IsInSegmentList
@Description    Tests if a BT is in the segment list.
@Input          pArena           The arena.
@Input          pBT              The boundary tag to look for.
@Return         IMG_FALSE  BT was not in the arena's segment list.
                IMG_TRUE   BT was in the arena's segment list.
*/ /**************************************************************************/
static IMG_BOOL
_IsInSegmentList (RA_ARENA *pArena,
                  BT *pBT)
{
	BT*  pBTScan;

	PVR_ASSERT (pArena != IMG_NULL);
	PVR_ASSERT (pBT != IMG_NULL);

	/* Walk the segment list until we see the BT pointer... */
	pBTScan = pArena->pHeadSegment;
	while (pBTScan != IMG_NULL  &&  pBTScan != pBT)
	{
		pBTScan = pBTScan->pNextSegment;
	}

	/* Test if we found it and then return */
	return (pBTScan == pBT);
}

#if defined(ENABLE_RA_LIST_ASSERTS)
/*************************************************************************/ /*!
@Function       _IsInFreeList
@Description    Tests if a BT is in the free list.
@Input          pArena           The arena.
@Input          pBT              The boundary tag to look for.
@Return         IMG_FALSE  BT was not in the arena's free list.
                IMG_TRUE   BT was in the arena's free list.
*/ /**************************************************************************/
static IMG_BOOL
_IsInFreeList (RA_ARENA *pArena,
               BT *pBT)
{
	BT*  pBTScan;
	IMG_UINT32  uIndex;
	
	PVR_ASSERT (pArena != IMG_NULL);
	PVR_ASSERT (pBT != IMG_NULL);

	/* Look for the free list that holds BTs of this size... */
	uIndex  = pvr_log2 (pBT->uSize);
	PVR_ASSERT (uIndex < FREE_TABLE_LIMIT);

	/* Walk the free list until we see the BT pointer... */
	pBTScan = pArena->aHeadFree[uIndex];
	while (pBTScan != IMG_NULL  &&  pBTScan != pBT)
	{
		pBTScan = pBTScan->pNextFree;
	}

	/* Test if we found it and then return */
	return (pBTScan == pBT);
}
#endif

/*************************************************************************/ /*!
@Function       _SegmentListInsertAfter
@Description    Insert a boundary tag into an arena segment list after a
                specified boundary tag.
@Input          pArena           The arena.
@Input          pInsertionPoint  The insertion point.
@Input          pBT              The boundary tag to insert.
@Return         PVRSRV_ERROR
*/ /**************************************************************************/
static PVRSRV_ERROR
_SegmentListInsertAfter (RA_ARENA *pArena,
						 BT *pInsertionPoint,
						 BT *pBT)
{
	PVR_ASSERT (pArena != IMG_NULL);
	PVR_ASSERT (pInsertionPoint != IMG_NULL);
	RA_LIST_ASSERT (!_IsInSegmentList(pArena, pBT));

	if ((pInsertionPoint == IMG_NULL) || (pArena == IMG_NULL))
	{
		PVR_DPF ((PVR_DBG_ERROR,"_SegmentListInsertAfter: invalid parameters"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	pBT->pNextSegment = pInsertionPoint->pNextSegment;
	pBT->pPrevSegment = pInsertionPoint;
	if (pInsertionPoint->pNextSegment == IMG_NULL)
		pArena->pTailSegment = pBT;
	else
		pInsertionPoint->pNextSegment->pPrevSegment = pBT;
	pInsertionPoint->pNextSegment = pBT;

	return PVRSRV_OK;
}

/*************************************************************************/ /*!
@Function       _SegmentListInsert
@Description    Insert a boundary tag into an arena segment list at the
                appropriate point.
@Input          pArena    The arena.
@Input          pBT       The boundary tag to insert.
@Return         Error code
*/ /**************************************************************************/
static PVRSRV_ERROR
_SegmentListInsert (RA_ARENA *pArena, BT *pBT)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	RA_LIST_ASSERT (!_IsInSegmentList(pArena, pBT));

	/* insert into the segment chain */
	if (pArena->pHeadSegment == IMG_NULL)
	{
		pArena->pHeadSegment = pArena->pTailSegment = pBT;
		pBT->pNextSegment = pBT->pPrevSegment = IMG_NULL;
	}
	else
	{
		BT *pBTScan;

		if (pBT->base < pArena->pHeadSegment->base)
		{
			/* The base address of pBT is less than the base address of the boundary tag
			at the head of the list - so insert this boundary tag at the head. */
			pBT->pNextSegment = pArena->pHeadSegment;
			pArena->pHeadSegment->pPrevSegment = pBT;
			pArena->pHeadSegment = pBT;
			pBT->pPrevSegment = IMG_NULL;
		}
		else
		{

			/* The base address of pBT is greater than or equal to that of the boundary tag
			at the head of the list. Search for the insertion point: pBT must be inserted
			before the first boundary tag with a greater base value - or at the end of the list.
			*/
			pBTScan = pArena->pHeadSegment;

			while ((pBTScan->pNextSegment != IMG_NULL)  && (pBT->base >= pBTScan->pNextSegment->base))
			{
				pBTScan = pBTScan->pNextSegment;
			}

			eError = _SegmentListInsertAfter (pArena, pBTScan, pBT);
			if (eError != PVRSRV_OK)
			{
				return eError;
			}
		}
	}
	return eError;
}

/*************************************************************************/ /*!
@Function       _SegmentListRemove
@Description    Remove a boundary tag from an arena segment list.
@Input          pArena    The arena.
@Input          pBT       The boundary tag to remove.
*/ /**************************************************************************/
static IMG_VOID
_SegmentListRemove (RA_ARENA *pArena, BT *pBT)
{
	RA_LIST_ASSERT (_IsInSegmentList(pArena, pBT));
	
	if (pBT->pPrevSegment == IMG_NULL)
		pArena->pHeadSegment = pBT->pNextSegment;
	else
		pBT->pPrevSegment->pNextSegment = pBT->pNextSegment;

	if (pBT->pNextSegment == IMG_NULL)
		pArena->pTailSegment = pBT->pPrevSegment;
	else
		pBT->pNextSegment->pPrevSegment = pBT->pPrevSegment;
}

/*************************************************************************/ /*!
@Function       _SegmentSplit
@Description    Split a segment into two, maintain the arena segment list. The
                boundary tag should not be in the free table. Neither the
                original or the new neighbour bounary tag will be in the free
                table.
@Input          pArena    The arena.
@Input          pBT       The boundary tag to split.
@Input          uSize     The required segment size of boundary tag after
                          splitting.
@Return         New neighbour boundary tag.
*/ /**************************************************************************/
static BT *
_SegmentSplit (RA_ARENA *pArena, BT *pBT, RA_LENGTH_T uSize)
{
	BT *pNeighbour;

	PVR_ASSERT (pArena != IMG_NULL);
	RA_LIST_ASSERT (_IsInSegmentList(pArena, pBT));
	RA_LIST_ASSERT (!_IsInFreeList(pArena, pBT));

	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"_SegmentSplit: invalid parameter - pArena"));
		return IMG_NULL;
	}

    pNeighbour = OSAllocMem(sizeof(BT));
    if (pNeighbour == IMG_NULL)
    {
        return IMG_NULL;
    }

	OSMemSet(pNeighbour, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pNeighbour->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pNeighbour->pPrevSegment = pBT;
	pNeighbour->pNextSegment = pBT->pNextSegment;
	if (pBT->pNextSegment == IMG_NULL)
		pArena->pTailSegment = pNeighbour;
	else
		pBT->pNextSegment->pPrevSegment = pNeighbour;
	pBT->pNextSegment = pNeighbour;

	pNeighbour->type = btt_free;
	pNeighbour->uSize = pBT->uSize - uSize;
	pNeighbour->base = pBT->base + uSize;
	pNeighbour->hPriv = pBT->hPriv;
    pNeighbour->uFlags = pBT->uFlags;
	pBT->uSize = uSize;

#if defined(VALIDATE_ARENA_TEST)
	if (pNeighbour->pPrevSegment->eResourceType == IMPORTED_RESOURCE_TYPE)
	{
		pNeighbour->eResourceType = IMPORTED_RESOURCE_TYPE;
		pNeighbour->eResourceSpan = IMPORTED_RESOURCE_SPAN_FREE;
	}
	else if (pNeighbour->pPrevSegment->eResourceType == NON_IMPORTED_RESOURCE_TYPE)
	{
		pNeighbour->eResourceType = NON_IMPORTED_RESOURCE_TYPE;
		pNeighbour->eResourceSpan = RESOURCE_SPAN_FREE;
	}
	else
	{
		PVR_DPF ((PVR_DBG_ERROR,"_SegmentSplit: pNeighbour->pPrevSegment->eResourceType unrecognized"));
		PVR_DBG_BREAK;
	}
#endif

	RA_LIST_ASSERT (_IsInSegmentList(pArena, pBT));
	RA_LIST_ASSERT (_IsInSegmentList(pArena, pNeighbour));
	RA_LIST_ASSERT (!_IsInFreeList(pArena, pBT));
	RA_LIST_ASSERT (!_IsInFreeList(pArena, pNeighbour));

	return pNeighbour;
}

/*************************************************************************/ /*!
@Function       _FreeListInsert
@Description    Insert a boundary tag into an arena free table.
@Input          pArena    The arena.
@Input          pBT       The boundary tag.
*/ /**************************************************************************/
static IMG_VOID
_FreeListInsert (RA_ARENA *pArena, BT *pBT)
{
	IMG_UINT32 uIndex;
	uIndex = pvr_log2 (pBT->uSize);

	PVR_ASSERT (uIndex < FREE_TABLE_LIMIT);
	RA_LIST_ASSERT (!_IsInFreeList(pArena, pBT));

	pBT->type = btt_free;
	pBT->pNextFree = pArena->aHeadFree [uIndex];
	pBT->pPrevFree = IMG_NULL;
	if (pArena->aHeadFree[uIndex] != IMG_NULL)
		pArena->aHeadFree[uIndex]->pPrevFree = pBT;
	pArena->aHeadFree [uIndex] = pBT;
}

/*************************************************************************/ /*!
@Function       _FreeListRemove
@Description    Remove a boundary tag from an arena free table.
@Input          pArena    The arena.
@Input          pBT       The boundary tag.
*/ /**************************************************************************/
static IMG_VOID
_FreeListRemove (RA_ARENA *pArena, BT *pBT)
{
	IMG_UINT32 uIndex;
	uIndex = pvr_log2 (pBT->uSize);

	PVR_ASSERT (uIndex < FREE_TABLE_LIMIT);
	RA_LIST_ASSERT (_IsInFreeList(pArena, pBT));

	if (pBT->pNextFree != IMG_NULL)
		pBT->pNextFree->pPrevFree = pBT->pPrevFree;
	if (pBT->pPrevFree == IMG_NULL)
		pArena->aHeadFree[uIndex] = pBT->pNextFree;
	else
		pBT->pPrevFree->pNextFree = pBT->pNextFree;
}

/*************************************************************************/ /*!
@Function       _BuildSpanMarker
@Description    Construct a span marker boundary tag.
@Input          pArena   The arena to contain span marker
@Input          base     The base of the bounary tag.
@Return         Span marker boundary tag.
*/ /**************************************************************************/
static BT *
_BuildSpanMarker (RA_BASE_T base, RA_LENGTH_T uSize)
{
	BT *pBT;

    pBT = OSAllocMem(sizeof(BT));
    if (pBT == IMG_NULL)
    {
        return IMG_NULL;
    }

	OSMemSet(pBT, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pBT->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pBT->type = btt_span;
	pBT->base = base;
	pBT->uSize = uSize;
	pBT->hPriv = RA_UNINITED_PRIV;

	return pBT;
}

/*************************************************************************/ /*!
@Function       _BuildBT
@Description    Construct a boundary tag for a free segment.
@Input          base     The base of the resource segment.
@Input          uSize    The extent of the resouce segment.
@Return         Boundary tag
*/ /**************************************************************************/
static BT *
_BuildBT (RA_BASE_T base,
          RA_LENGTH_T uSize,
          RA_FLAGS_T uFlags
          )
{
	BT *pBT;

	pBT = OSAllocMem(sizeof(BT));
    if (pBT == IMG_NULL)
	{
		return IMG_NULL;
	}

	OSMemSet(pBT, 0, sizeof(BT));

#if defined(VALIDATE_ARENA_TEST)
	pBT->ui32BoundaryTagID = ++ui32BoundaryTagID;
#endif

	pBT->type = btt_free;
	pBT->base = base;
	pBT->uSize = uSize;
    pBT->uFlags = uFlags;

	return pBT;
}

/*************************************************************************/ /*!
@Function       _InsertResource
@Description    Add a free resource segment to an arena.
@Input          pArena    The arena.
@Input          base      The base of the resource segment.
@Input          uSize     The extent of the resource segment.
@Return         New bucket pointer
                IMG_NULL on failure
*/ /**************************************************************************/
static BT *
_InsertResource (RA_ARENA *pArena,
                 RA_BASE_T base,
                 RA_LENGTH_T uSize,
                 RA_FLAGS_T uFlags
                 )
{
	BT *pBT;
	PVR_ASSERT (pArena!=IMG_NULL);
	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"_InsertResource: invalid parameter - pArena"));
		return IMG_NULL;
	}

	pBT = _BuildBT (base, uSize, uFlags);
	if (pBT != IMG_NULL)
	{

#if defined(VALIDATE_ARENA_TEST)
		pBT->eResourceSpan = RESOURCE_SPAN_FREE;
		pBT->eResourceType = NON_IMPORTED_RESOURCE_TYPE;
#endif

		if (_SegmentListInsert (pArena, pBT) != PVRSRV_OK)
		{
			PVR_DPF ((PVR_DBG_ERROR,"_InsertResource: call to _SegmentListInsert failed"));
			return IMG_NULL;
		}
		_FreeListInsert (pArena, pBT);
	}
	return pBT;
}

/*************************************************************************/ /*!
@Function       _InsertResourceSpan
@Description    Add a free resource span to an arena, complete with span markers.
@Input          pArena    The arena.
@Input          base      The base of the resource segment.
@Input          uSize     The extent of the resource segment.
@Return         The boundary tag representing the free resource segment,
                or IMG_NULL on failure.
*/ /**************************************************************************/
static BT *
_InsertResourceSpan (RA_ARENA *pArena,
                     RA_BASE_T base,
                     RA_LENGTH_T uSize,
                     RA_FLAGS_T uFlags)
{
	PVRSRV_ERROR eError;
	BT *pSpanStart;
	BT *pSpanEnd;
	BT *pBT;

	PVR_ASSERT (pArena != IMG_NULL);
	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"_InsertResourceSpan: invalid parameter - pArena"));
		return IMG_NULL;
	}

	PVR_DPF ((PVR_DBG_MESSAGE, "RA_InsertResourceSpan: arena='%s', base=" RA_BASE_FMTSPEC ", size=" RA_LENGTH_FMTSPEC, pArena->name, base, uSize));

	pSpanStart = _BuildSpanMarker (base, uSize);
	if (pSpanStart == IMG_NULL)
	{
		goto fail_start;
	}

#if defined(VALIDATE_ARENA_TEST)
	pSpanStart->eResourceSpan = IMPORTED_RESOURCE_SPAN_START;
	pSpanStart->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	pSpanEnd = _BuildSpanMarker (base + uSize, 0);
	if (pSpanEnd == IMG_NULL)
	{
		goto fail_end;
	}

#if defined(VALIDATE_ARENA_TEST)
	pSpanEnd->eResourceSpan = IMPORTED_RESOURCE_SPAN_END;
	pSpanEnd->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	pBT = _BuildBT (base, uSize, uFlags);
	if (pBT == IMG_NULL)
	{
		goto fail_bt;
	}

#if defined(VALIDATE_ARENA_TEST)
	pBT->eResourceSpan = IMPORTED_RESOURCE_SPAN_FREE;
	pBT->eResourceType = IMPORTED_RESOURCE_TYPE;
#endif

	eError = _SegmentListInsert (pArena, pSpanStart);
	if (eError != PVRSRV_OK)
	{
		goto fail_SegListInsert;
	}

	eError = _SegmentListInsertAfter (pArena, pSpanStart, pBT);
	if (eError != PVRSRV_OK)
	{
		goto fail_SegListInsert;
	}

	_FreeListInsert (pArena, pBT);

	eError = _SegmentListInsertAfter (pArena, pBT, pSpanEnd);
	if (eError != PVRSRV_OK)
	{
		goto fail_SegListInsert;
	}

	return pBT;

  fail_SegListInsert:
    OSFreeMem(pBT);
	/*not nulling pointer, out of scope*/
  fail_bt:
	OSFreeMem(pSpanEnd);
	/*not nulling pointer, out of scope*/
  fail_end:
	OSFreeMem(pSpanStart);
	/*not nulling pointer, out of scope*/
  fail_start:
	return IMG_NULL;
}


/*************************************************************************/ /*!
@Function       _RemoveResourceSpan
@Description    Frees a resource span from an arena, removing also the span
                markers and returning the imported span via the callback.
@Input          pArena     The arena.
@Input          pBT        The boundary tag to free.
@Return         IMG_FALSE failure - span was still in use
                IMG_TRUE  success - span was removed and returned
*/ /**************************************************************************/
static IMG_BOOL
_RemoveResourceSpan (RA_ARENA *pArena, BT *pBT)
{
	PVR_ASSERT (pArena!=IMG_NULL);
	PVR_ASSERT (pBT!=IMG_NULL);

	if (pBT->pNextSegment!=IMG_NULL && pBT->pNextSegment->type == btt_span
		&& pBT->pPrevSegment!=IMG_NULL && pBT->pPrevSegment->type == btt_span)
	{
		BT *next = pBT->pNextSegment;
		BT *prev = pBT->pPrevSegment;

		_SegmentListRemove (pArena, next);
		_SegmentListRemove (pArena, prev);
		_SegmentListRemove (pArena, pBT);
		pArena->pImportFree (pArena->pImportHandle, pBT->base, pBT->hPriv);
		OSFreeMem(next);
		/*not nulling original pointer, already overwritten*/
		OSFreeMem(prev);
		/*not nulling original pointer, already overwritten*/
		OSFreeMem(pBT);
		/*not nulling pointer, copy on stack*/
		
		return IMG_TRUE;
	}

	return IMG_FALSE;
}


/*************************************************************************/ /*!
@Function       _FreeBT
@Description    Free a boundary tag taking care of the segment list and the
                boundary tag free table.
@Input          pArena     The arena.
@Input          pBT        The boundary tag to free.
*/ /**************************************************************************/
static IMG_VOID
_FreeBT (RA_ARENA *pArena, BT *pBT)
{
	BT *pNeighbour;

	PVR_ASSERT (pArena!=IMG_NULL);
	PVR_ASSERT (pBT!=IMG_NULL);

	if ((pArena == IMG_NULL) || (pBT == IMG_NULL))
	{
		PVR_DPF ((PVR_DBG_ERROR,"_FreeBT: invalid parameter"));
		return;
	}

	/* try and coalesce with left neighbour */
	pNeighbour = pBT->pPrevSegment;
	if (pNeighbour!=IMG_NULL
		&& pNeighbour->type == btt_free
		&& pNeighbour->base + pNeighbour->uSize == pBT->base)
	{
		_FreeListRemove (pArena, pNeighbour);
		_SegmentListRemove (pArena, pNeighbour);
		pBT->base = pNeighbour->base;
		pBT->uSize += pNeighbour->uSize;
        OSFreeMem(pNeighbour);
		/*not nulling original pointer, already overwritten*/
	}

	/* try to coalesce with right neighbour */
	pNeighbour = pBT->pNextSegment;
	if (pNeighbour!=IMG_NULL
		&& pNeighbour->type == btt_free
		&& pBT->base + pBT->uSize == pNeighbour->base)
	{
		_FreeListRemove (pArena, pNeighbour);
		_SegmentListRemove (pArena, pNeighbour);
		pBT->uSize += pNeighbour->uSize;
		OSFreeMem(pNeighbour);
		/*not nulling original pointer, already overwritten*/
	}

	if (_RemoveResourceSpan(pArena, pBT) == IMG_FALSE)
	{
		_FreeListInsert (pArena, pBT);
	}
}


/*************************************************************************/ /*!
@Function       _AttemptAllocAligned
@Description    Attempt an allocation from an arena.
@Input          pArena       The arena.
@Input          uSize        The requested allocation size.
@Output         ppsMapping   The user references associated with
                             the allocated segment.
@Input          flags        Allocation flags
@Input          uAlignment   Required uAlignment, or 0. 
                             Must be a power of 2 if not 0
@Output         base         Allocated resource base
@Return         IMG_FALSE failure
                IMG_TRUE success
*/ /**************************************************************************/
static IMG_BOOL
_AttemptAllocAligned (RA_ARENA *pArena,
					  RA_LENGTH_T uSize,
					  IMG_UINT32 uFlags,
					  RA_LENGTH_T uAlignment,
					  RA_BASE_T *base,
                      RA_PERISPAN_HANDLE *phPriv) // is this the "per-import" private data? FIXME: check
{
	IMG_UINT32 uIndex;
	PVR_ASSERT (pArena!=IMG_NULL);
	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"_AttemptAllocAligned: invalid parameter - pArena"));
		return IMG_FALSE;
	}

	/* search for a near fit free boundary tag, start looking at the
	   pvr_log2 free table for our required size and work on up the
	   table. */
	uIndex = pvr_log2 (uSize);

#if 0
	/* If the size required is exactly 2**n then use the n bucket, because
	   we know that every free block in that bucket is larger than 2**n,
	   otherwise start at then next bucket up. */
	if (1u<<uIndex < uSize)
		uIndex++;
#endif

	while (uIndex < FREE_TABLE_LIMIT && pArena->aHeadFree[uIndex]==IMG_NULL)
		uIndex++;

	while (uIndex < FREE_TABLE_LIMIT)
	{
		if (pArena->aHeadFree[uIndex]!=IMG_NULL)
		{
			/* we have a cached free boundary tag */
			BT *pBT;

			pBT = pArena->aHeadFree [uIndex];
			while (pBT!=IMG_NULL)
			{
				RA_BASE_T aligned_base;

				if (uAlignment>1)
					aligned_base = (pBT->base + uAlignment - 1) & ~(uAlignment - 1);
				else
					aligned_base = pBT->base;
				PVR_DPF ((PVR_DBG_MESSAGE,
						  "RA_AttemptAllocAligned: pBT-base=" RA_BASE_FMTSPEC " "
						  "pBT-size=" RA_LENGTH_FMTSPEC " "
                          "alignedbase=" RA_BASE_FMTSPEC " "
                          "size=" RA_LENGTH_FMTSPEC,
                          pBT->base, pBT->uSize, aligned_base, uSize));

				if (pBT->base + pBT->uSize >= aligned_base + uSize)
				{
                    /* FIXME: do we need a "bCheckFlags"?  I think it's
                       ok to say that caller would just supply 0 for
                       such RAs, and 0 == 0, so all is good */
					if(/*!pArena->bCheckFlags ||*/ pBT->uFlags == uFlags)
					{
						_FreeListRemove (pArena, pBT);

						PVR_ASSERT (pBT->type == btt_free);

						/* with uAlignment we might need to discard the front of this segment */
						if (aligned_base > pBT->base)
						{
							BT *pNeighbour;
							pNeighbour = _SegmentSplit (pArena, pBT, (RA_LENGTH_T)(aligned_base - pBT->base));
							/* partition the buffer, create a new boundary tag */
							if (pNeighbour==IMG_NULL)
							{
								PVR_DPF ((PVR_DBG_ERROR,"_AttemptAllocAligned: Front split failed"));
								/* Put pBT back in the list */
								_FreeListInsert (pArena, pBT);
								return IMG_FALSE;
							}

							_FreeListInsert (pArena, pBT);
							pBT = pNeighbour;
						}

						/* the segment might be too big, if so, discard the back of the segment */
						if (pBT->uSize > uSize)
						{
							BT *pNeighbour;
							pNeighbour = _SegmentSplit (pArena, pBT, uSize);
							/* partition the buffer, create a new boundary tag */
							if (pNeighbour==IMG_NULL)
							{
								PVR_DPF ((PVR_DBG_ERROR,"_AttemptAllocAligned: Back split failed"));
								/* Put pBT back in the list */
								_FreeListInsert (pArena, pBT);
								return IMG_FALSE;
							}

							_FreeListInsert (pArena, pNeighbour);
						}

						pBT->type = btt_live;

#if defined(VALIDATE_ARENA_TEST)
						if (pBT->eResourceType == IMPORTED_RESOURCE_TYPE)
						{
							pBT->eResourceSpan = IMPORTED_RESOURCE_SPAN_LIVE;
						}
						else if (pBT->eResourceType == NON_IMPORTED_RESOURCE_TYPE)
						{
							pBT->eResourceSpan = RESOURCE_SPAN_LIVE;
						}
						else
						{
							PVR_DPF ((PVR_DBG_ERROR,"_AttemptAllocAligned ERROR: pBT->eResourceType unrecognized"));
							PVR_DBG_BREAK;
						}
#endif
						if (!HASH_Insert_Extended (pArena->pSegmentHash, &pBT->base, (IMG_UINTPTR_T)pBT))
						{
							_FreeBT (pArena, pBT);
							return IMG_FALSE;
						}

						if (phPriv != IMG_NULL)
							*phPriv = pBT->hPriv;

						*base = pBT->base;

						return IMG_TRUE;
					}
					else
					{
						PVR_DPF ((PVR_DBG_MESSAGE,
								"AttemptAllocAligned: mismatch in flags. Import has %x, request was %x", pBT->uFlags, uFlags));

					}
				}
				pBT = pBT->pNextFree;
			}

		}
		uIndex++;
	}

	return IMG_FALSE;
}



/*************************************************************************/ /*!
@Function       RA_Create
@Description    To create a resource arena.
@Input          name          The name of the arena for diagnostic purposes.
@Input          base          The base of an initial resource span or 0.
@Input          uSize         The size of an initial resource span or 0.
@Input          ulog2Quantum  The arena allocation quantum.
@Input          alloc         A resource allocation callback or 0.
@Input          free          A resource de-allocation callback or 0.
@Input          pImportHandle Handle passed to alloc and free or 0.
@Return         arena handle, or IMG_NULL.
*/ /**************************************************************************/
IMG_INTERNAL RA_ARENA *
RA_Create (IMG_CHAR *name,
		   RA_BASE_T base,
		   RA_LENGTH_T uSize,
           RA_FLAGS_T uFlags,
           RA_PERISPAN_HANDLE hPriv,
		   RA_LOG2QUANTUM_T uLog2Quantum,
		   IMG_BOOL (*imp_alloc)(RA_PERARENA_HANDLE h, 
                                 RA_LENGTH_T uSize,
                                 RA_FLAGS_T _flags, 
                                 /* returned data */
                                 RA_BASE_T *pBase,
                                 RA_LENGTH_T *pActualSize,
                                 RA_PERISPAN_HANDLE *phPriv),
		   IMG_VOID (*imp_free) (RA_PERARENA_HANDLE,
                                 RA_BASE_T,
                                 RA_PERISPAN_HANDLE),
		   RA_PERARENA_HANDLE pImportHandle)
{
	RA_ARENA *pArena;
	BT *pBT;
	IMG_INT i;
	PVRSRV_ERROR eError;

	PVR_DPF ((PVR_DBG_MESSAGE, "RA_Create: name='%s', base=" RA_BASE_FMTSPEC ", "
              "uSize=" RA_LENGTH_FMTSPEC, name, base, uSize));

	pArena = OSAllocMem(sizeof (*pArena));
    if (pArena == IMG_NULL)
	{
		goto arena_fail;
	}

	eError = OSLockCreate(&pArena->hLock, LOCK_TYPE_PASSIVE);
	if (eError != PVRSRV_OK)
	{
		goto lock_fail;
	}

	pArena->name = name;
	pArena->pImportAlloc = (imp_alloc!=IMG_NULL) ? imp_alloc : &_RequestAllocFail;
	pArena->pImportFree = imp_free;
	pArena->pImportHandle = pImportHandle;
	for (i=0; i<FREE_TABLE_LIMIT; i++)
		pArena->aHeadFree[i] = IMG_NULL;
	pArena->pHeadSegment = IMG_NULL;
	pArena->pTailSegment = IMG_NULL;
	pArena->uQuantum = (IMG_UINT64) (1 << uLog2Quantum);
    pArena->pfnPreAllocCheck = IMG_NULL;

	pArena->pSegmentHash = HASH_Create_Extended(MINIMUM_HASH_SIZE, sizeof(RA_BASE_T), HASH_Func_Default, HASH_Key_Comp_Default);

	if (pArena->pSegmentHash==IMG_NULL)
	{
		goto hash_fail;
	}
	if (uSize>0)
	{
		uSize = (RA_LENGTH_T) ((uSize + (RA_LENGTH_T) (1 << uLog2Quantum) - 1) & ~((RA_LENGTH_T)(1 << uLog2Quantum) - 1));
		pBT = _InsertResource (pArena, base, uSize, uFlags);
		if (pBT == IMG_NULL)
		{
			goto insert_fail;
		}
        pBT->hPriv = hPriv;
	}
	return pArena;

insert_fail:
	HASH_Delete (pArena->pSegmentHash);
hash_fail:
	OSLockDestroy(pArena->hLock);
lock_fail:
	OSFreeMem(pArena);
	/*not nulling pointer, out of scope*/
arena_fail:
	return IMG_NULL;
}

/*************************************************************************/ /*!
@Function       RA_Delete
@Description    To delete a resource arena. All resources allocated from
                the arena must be freed before deleting the arena.
@Input          pArena        The arena to delete.
*/ /**************************************************************************/
IMG_INTERNAL IMG_VOID
RA_Delete (RA_ARENA *pArena)
{
	IMG_UINT32 uIndex;

	PVR_ASSERT(pArena != IMG_NULL);

	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"RA_Delete: invalid parameter - pArena"));
		return;
	}

	PVR_DPF ((PVR_DBG_MESSAGE,
			  "RA_Delete: name='%s'", pArena->name));

	for (uIndex=0; uIndex<FREE_TABLE_LIMIT; uIndex++)
		pArena->aHeadFree[uIndex] = IMG_NULL;

	while (pArena->pHeadSegment != IMG_NULL)
	{
		BT *pBT = pArena->pHeadSegment;

		if (pBT->type != btt_free)
		{
			PVR_DPF ((PVR_DBG_ERROR, "RA_Delete: allocations still exist in the arena that is being destroyed"));
			PVR_DPF ((PVR_DBG_ERROR, "Likely Cause: client drivers not freeing alocations before destroying devmemcontext"));
			PVR_DPF ((PVR_DBG_ERROR, "RA_Delete: base = " RA_BASE_FMTSPEC " "
                      "size=" RA_LENGTH_FMTSPEC, pBT->base, pBT->uSize));
		}

		_SegmentListRemove (pArena, pBT);
		OSFreeMem(pBT);
		/*not nulling original pointer, it has changed*/
	}
	HASH_Delete (pArena->pSegmentHash);
	OSLockDestroy(pArena->hLock);
	OSFreeMem(pArena);
	/*not nulling pointer, copy on stack*/
}

/*************************************************************************/ /*!
@Function       RA_Add
@Description    To add a resource span to an arena. The span must not
                overlapp with any span previously added to the arena.
@Input          pArena     The arena to add a span into.
@Input          base       The base of the span.
@Input          uSize      The extent of the span.
@Return         IMG_TRUE - Success
                IMG_FALSE - failure
*/ /**************************************************************************/
IMG_INTERNAL IMG_BOOL
RA_Add (RA_ARENA *pArena, RA_BASE_T base, RA_LENGTH_T uSize, RA_FLAGS_T uFlags)
{
	IMG_BOOL bRet;
	PVR_ASSERT (pArena != IMG_NULL);

	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"RA_Add: invalid parameter - pArena"));
		return IMG_FALSE;
	}

	OSLockAcquire(pArena->hLock);
	PVR_DPF ((PVR_DBG_MESSAGE, "RA_Add: name='%s', "
              "base=" RA_BASE_FMTSPEC ", size=" RA_LENGTH_FMTSPEC,
              pArena->name, base, uSize));

	uSize = (uSize + pArena->uQuantum - 1) & ~(pArena->uQuantum - 1);
	bRet = (IMG_BOOL)(_InsertResource (pArena, base, uSize, uFlags) != IMG_NULL);
	OSLockRelease(pArena->hLock);
	return bRet;
}

/*************************************************************************/ /*!
@Function       RA_Alloc
@Description    To allocate resource from an arena.
@Input          pArena         The arena
@Input          uRequestSize   The size of resource segment requested.
@Output         pActualSize    The actual size of resource segment
                               allocated, typcially rounded up by quantum.
@Output         ppsMapping     The user reference associated with allocated resource span.
@Input          uFlags         Flags influencing allocation policy.
@Input          uAlignment     The uAlignment constraint required for the
                               allocated segment, use 0 if uAlignment not required, otherwise
                               must be a power of 2.
@Output         base           Allocated base resource
@Return         IMG_TRUE - success
                IMG_FALSE - failure
*/ /**************************************************************************/
IMG_INTERNAL IMG_BOOL
RA_Alloc (RA_ARENA *pArena,
		  RA_LENGTH_T uRequestSize,
		  RA_FLAGS_T uFlags,
		  RA_LENGTH_T uAlignment,
		  RA_BASE_T *base,
		  RA_LENGTH_T *pActualSize,
          RA_PERISPAN_HANDLE *phPriv)
{
	IMG_BOOL bResult;
	RA_LENGTH_T uSize = uRequestSize;

	PVR_ASSERT (pArena!=IMG_NULL);

	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"RA_Alloc: invalid parameter - pArena"));
		return IMG_FALSE;
	}

	OSLockAcquire(pArena->hLock);
#if defined(VALIDATE_ARENA_TEST)
	ValidateArena(pArena);
#endif

    if (pArena->pfnPreAllocCheck)
    {
        pArena->pfnPreAllocCheck(); // FIXME: per-RA private data?
    }

	if (pActualSize != IMG_NULL)
	{
		*pActualSize = uSize;
	}

	/* Must be a power of 2 or 0 */
	PVR_ASSERT((uAlignment == 0) || (uAlignment & (uAlignment - 1)) == 0);

	PVR_DPF ((PVR_DBG_MESSAGE,
			  "RA_Alloc: arena='%s', size=" RA_LENGTH_FMTSPEC "(" RA_LENGTH_FMTSPEC "), "
              "alignment=" RA_ALIGN_FMTSPEC, pArena->name, uSize, uRequestSize, uAlignment));

	/* if allocation failed then we might have an import source which
	   can provide more resource, else we will have to fail the
	   allocation to the caller. */
	bResult = _AttemptAllocAligned (pArena, uSize, uFlags, uAlignment, base, phPriv);
	if (!bResult)
	{
        IMG_HANDLE hPriv;
		RA_BASE_T import_base;
		RA_LENGTH_T uImportSize = uSize;

		/*
			Ensure that we allocate sufficient space to meet the uAlignment
			constraint
		 */
		if (uAlignment > pArena->uQuantum)
		{
			uImportSize += (uAlignment - pArena->uQuantum);
		}

		/* ensure that we import according to the quanta of this arena */
		uImportSize = (uImportSize + pArena->uQuantum - 1) & ~(pArena->uQuantum - 1);

		bResult =
			pArena->pImportAlloc (pArena->pImportHandle, uImportSize, uFlags,
                                  &import_base, &uImportSize, &hPriv);
		if (bResult)
		{
			BT *pBT;
			pBT = _InsertResourceSpan (pArena, import_base, uImportSize, uFlags);
			/* successfully import more resource, create a span to
			   represent it and retry the allocation attempt */
			if (pBT == IMG_NULL)
			{
				/* insufficient resources to insert the newly acquired span,
				   so free it back again */
				pArena->pImportFree(pArena->pImportHandle, import_base, hPriv);

				PVR_DPF ((PVR_DBG_MESSAGE, "RA_Alloc: name='%s', "
                          "size=" RA_LENGTH_FMTSPEC " failed!", pArena->name, uSize));
				/* RA_Dump (arena); */
				OSLockRelease(pArena->hLock);
				return IMG_FALSE;
			}


            pBT->hPriv = hPriv;

			bResult = _AttemptAllocAligned(pArena, uSize, uFlags, uAlignment, base, phPriv);
			if (!bResult)
			{
				PVR_DPF ((PVR_DBG_ERROR,
						  "RA_Alloc: name='%s' second alloc failed!",
						  pArena->name));

				/*
				  On failure of _AttemptAllocAligned() depending on the exact point
				  of failure, the imported segment may have been used and freed, or
				  left untouched. If the later, we need to return it.
				*/
				if (_IsInSegmentList(pArena, pBT))
				{
					_FreeListRemove(pArena, pBT);
					if (_RemoveResourceSpan(pArena, pBT) == IMG_FALSE)
					{
						_FreeListInsert (pArena, pBT);
					}
				}
			}
			else
			{
				/* Check if the new allocation was in the span we just added... */
				if (*base < import_base  ||  *base > (import_base + uImportSize))
				{
					PVR_DPF ((PVR_DBG_ERROR,
							  "RA_Alloc: name='%s' alloc did not occur in the imported span!",
							  pArena->name));

					/*
					  Remove the imported span which should not be in use (if it is then
					  that is okay, but essentially no span should exist that is not used).
					*/
					_FreeListRemove(pArena, pBT);
					if (_RemoveResourceSpan(pArena, pBT) == IMG_FALSE)
					{
						_FreeListInsert (pArena, pBT);
					}
				}
			}
		}
	}

	PVR_DPF ((PVR_DBG_MESSAGE, "RA_Alloc: name='%s', size=" RA_LENGTH_FMTSPEC ", "
              "*base=" RA_BASE_FMTSPEC " = %d",pArena->name, uSize, *base, bResult));

	/*  RA_Dump (pArena);
		ra_stats (pArena);
	*/

#if defined(VALIDATE_ARENA_TEST)
	ValidateArena(pArena);
#endif

	OSLockRelease(pArena->hLock);
	return bResult;
}


#if defined(VALIDATE_ARENA_TEST)

/*************************************************************************/ /*!
@Function       ValidateArena
@Description    Validate an arena by checking that adjacent members of the
                double linked ordered list are compatible. PVR_DBG_BREAK and
                PVR_DPF messages are used when an error is detected.
                NOTE: A DEBUG build is required for PVR_DBG_BREAK and PVR_DPF
                to operate.
@Input          pArena    The arena
@Return         0
*/ /**************************************************************************/
IMG_UINT32 ValidateArena(RA_ARENA *pArena)
{
	BT* pSegment;
	RESOURCE_DESCRIPTOR eNextSpan;

	pSegment = pArena->pHeadSegment;

	if (pSegment == IMG_NULL)
	{
		return 0;
	}

	if (pSegment->eResourceType == IMPORTED_RESOURCE_TYPE)
	{
		PVR_ASSERT(pSegment->eResourceSpan == IMPORTED_RESOURCE_SPAN_START);

		while (pSegment->pNextSegment)
		{
			eNextSpan = pSegment->pNextSegment->eResourceSpan;

			switch (pSegment->eResourceSpan)
			{
				case IMPORTED_RESOURCE_SPAN_LIVE:

					if (!((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE) ||
						  (eNextSpan == IMPORTED_RESOURCE_SPAN_FREE) ||
						  (eNextSpan == IMPORTED_RESOURCE_SPAN_END)))
					{
						/* error - next span must be live, free or end */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;

				case IMPORTED_RESOURCE_SPAN_FREE:

					if (!((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE) ||
						  (eNextSpan == IMPORTED_RESOURCE_SPAN_END)))
					{
						/* error - next span must be live or end */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;

				case IMPORTED_RESOURCE_SPAN_END:

					if ((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE) ||
						(eNextSpan == IMPORTED_RESOURCE_SPAN_FREE) ||
						(eNextSpan == IMPORTED_RESOURCE_SPAN_END))
					{
						/* error - next span cannot be live, free or end */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;


				case IMPORTED_RESOURCE_SPAN_START:

					if (!((eNextSpan == IMPORTED_RESOURCE_SPAN_LIVE) ||
						  (eNextSpan == IMPORTED_RESOURCE_SPAN_FREE)))
					{
						/* error - next span must be live or free */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;

				default:
					PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

					PVR_DBG_BREAK;
				break;
			}
			pSegment = pSegment->pNextSegment;
		}
	}
	else if (pSegment->eResourceType == NON_IMPORTED_RESOURCE_TYPE)
	{
		PVR_ASSERT((pSegment->eResourceSpan == RESOURCE_SPAN_FREE) || (pSegment->eResourceSpan == RESOURCE_SPAN_LIVE));

		while (pSegment->pNextSegment)
		{
			eNextSpan = pSegment->pNextSegment->eResourceSpan;

			switch (pSegment->eResourceSpan)
			{
				case RESOURCE_SPAN_LIVE:

					if (!((eNextSpan == RESOURCE_SPAN_FREE) ||
						  (eNextSpan == RESOURCE_SPAN_LIVE)))
					{
						/* error - next span must be free or live */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;

				case RESOURCE_SPAN_FREE:

					if (!((eNextSpan == RESOURCE_SPAN_FREE) ||
						  (eNextSpan == RESOURCE_SPAN_LIVE)))
					{
						/* error - next span must be free or live */
						PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

						PVR_DBG_BREAK;
					}
				break;

				default:
					PVR_DPF((PVR_DBG_ERROR, "ValidateArena ERROR: adjacent boundary tags %d (base=" RA_BASE_FMTSPEC ") and %d (base=" RA_BASE_FMTSPEC ") are incompatible (arena: %s)",
								pSegment->ui32BoundaryTagID, pSegment->base, pSegment->pNextSegment->ui32BoundaryTagID, pSegment->pNextSegment->base, pArena->name));

					PVR_DBG_BREAK;
				break;
			}
			pSegment = pSegment->pNextSegment;
		}

	}
	else
	{
		PVR_DPF ((PVR_DBG_ERROR,"ValidateArena ERROR: pSegment->eResourceType unrecognized"));

		PVR_DBG_BREAK;
	}

	return 0;
}

#endif


/*************************************************************************/ /*!
@Function       RA_Free
@Description    To free a resource segment.
@Input          pArena     The arena the segment was originally allocated from.
@Input          base       The base of the resource span to free.
*/ /**************************************************************************/
IMG_INTERNAL IMG_VOID
RA_Free (RA_ARENA *pArena, RA_BASE_T base)
{
	BT *pBT;

	PVR_ASSERT (pArena != IMG_NULL);

	if (pArena == IMG_NULL)
	{
		PVR_DPF ((PVR_DBG_ERROR,"RA_Free: invalid parameter - pArena"));
		return;
	}

	OSLockAcquire(pArena->hLock);

	PVR_DPF ((PVR_DBG_MESSAGE, "RA_Free: name='%s', base=" RA_BASE_FMTSPEC, pArena->name, base));

	pBT = (BT *) HASH_Remove_Extended (pArena->pSegmentHash, &base);
	PVR_ASSERT (pBT != IMG_NULL);

	if (pBT)
	{
		PVR_ASSERT (pBT->base == base);
		_FreeBT (pArena, pBT);
	}
	OSLockRelease(pArena->hLock);
}

#if defined(ENABLE_RA_DUMP)
static IMG_CHAR *
_BTType (IMG_INT eType)
{
	switch (eType)
	{
	case btt_span: return "span";
	case btt_free: return "free";
	case btt_live: return "live";
	}
	return "junk";
}

/*************************************************************************/ /*!
@Function       RA_Dump
@Description    To dump a readable description of an arena. Diagnostic only.
@Input          pArena      The arena to dump.
*/ /**************************************************************************/
IMG_VOID
RA_Dump (RA_ARENA *pArena)
{
	BT *pBT;
	PVR_ASSERT (pArena != IMG_NULL);

	OSLockAcquire(pArena->hLock);

	PVR_DPF ((PVR_DBG_MESSAGE,"Arena '%s':", pArena->name));
	PVR_DPF ((PVR_DBG_MESSAGE,"  alloc=%p free=%p handle=%p quantum=" RA_LENGTH_FMTSPEC,
			 pArena->pImportAlloc, pArena->pImportFree, pArena->pImportHandle,
			 pArena->uQuantum));
	PVR_DPF ((PVR_DBG_MESSAGE,"  segment Chain:"));
	if (pArena->pHeadSegment != IMG_NULL &&
	    pArena->pHeadSegment->pPrevSegment != IMG_NULL)
		PVR_DPF ((PVR_DBG_MESSAGE,"  error: head boundary tag has invalid pPrevSegment"));
	if (pArena->pTailSegment != IMG_NULL &&
	    pArena->pTailSegment->pNextSegment != IMG_NULL)
		PVR_DPF ((PVR_DBG_MESSAGE,"  error: tail boundary tag has invalid pNextSegment"));

	for (pBT=pArena->pHeadSegment; pBT!=IMG_NULL; pBT=pBT->pNextSegment)
	{
		PVR_DPF ((PVR_DBG_MESSAGE,"\tbase=" RA_BASE_FMTSPEC " size=" RA_LENGTH_FMTSPEC " type=%s",
				 pBT->base, pBT->uSize, _BTType (pBT->type)));
	}

#ifdef HASH_TRACE
	HASH_Dump (pArena->pSegmentHash);
#endif
	OSLockRelease(pArena->hLock);
}
#endif /* #if defined(ENABLE_RA_DUMP) */

/******************************************************************************
 End of file (ra.c)
******************************************************************************/




