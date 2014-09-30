/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/
#ifndef __DC_MRFLD_H__
#define __DC_MRFLD_H__

#include <drm/drmP.h>
#include <imgpixfmts_km.h>
#include "kerneldisplay.h"
#include "psb_drm.h"
#include "displayclass_interface.h"


enum {
	DC_PIPE_A,
	DC_PIPE_B,
	DC_PIPE_C,
	MAX_PIPE_NUM,
};

typedef enum {
	DCMrfldEX_BUFFER_SOURCE_ALLOC,
	DCMrfldEX_BUFFER_SOURCE_IMPORT,
	DCMrfldEX_BUFFER_SOURCE_SYSTEM,
} DCMrfldEX_BUFFER_SOURCE;

typedef enum {
	DC_MRFLD_FLIP_SURFACE,
	DC_MRFLD_FLIP_CONTEXT,
} DC_MRFLD_FLIP_OP;

/* ied session clean-up */
#define MAX_IED_SESSIONS    8
extern int sepapp_drm_playback(bool ied_status);

/* max count of plane contexts which share the same buffer*/
#define MAX_CONTEXT_COUNT   3

/* max index of a plane */
#define MAX_PLANE_INDEX     3

typedef struct {
	IMG_HANDLE hDisplayContext;
	IMG_PIXFMT ePixFormat;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT32 ui32ByteStride;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	/*physical sys page list*/
	IMG_SYS_PHYADDR *psSysAddr;
	/*physical device page list*/
	IMG_DEV_PHYADDR *psDevAddr;
	/*GTT offset*/
	IMG_DEV_VIRTADDR sDevVAddr;
	/*CPU virtual address*/
	IMG_CPU_VIRTADDR sCPUVAddr;
	IMG_BOOL bIsContiguous;
	IMG_BOOL bIsAllocated;
	IMG_UINT32 ui32OwnerTaskID;
	IMG_HANDLE hImport;
	IMG_UINT32 ui32RefCount;
	DCMrfldEX_BUFFER_SOURCE eSource;
	DC_MRFLD_FLIP_OP eFlipOp;
	IMG_UINT32 ui32ContextCount;
	/* This buffer */
	DC_MRFLD_SURF_CUSTOM sContext[MAX_CONTEXT_COUNT];
} DC_MRFLD_BUFFER;

struct plane_state {
	int type;
	int index;
	int attached_pipe;
	bool active;
	bool flip_active;
	bool disabled;
	u32 extra_power_island;
	bool powered_off;
};

/*Display Controller Device*/
typedef struct {
	IMG_HANDLE hSrvHandle;
	struct drm_device *psDrmDevice;
	DC_MRFLD_BUFFER *psSystemBuffer;
	PVRSRV_SURFACE_INFO sPrimInfo;
	DC_DISPLAY_INFO	sDisplayInfo;

	/*plane enabling*/
	IMG_UINT32 ui32ActiveOverlays;
	IMG_UINT32 ui32ActiveSprites;
	IMG_UINT32 ui32ActivePrimarys;

	IMG_UINT32 ui32ActivePlanes[DC_PLANE_MAX];
	IMG_UINT32 ui32SavedActivePlanes[DC_PLANE_MAX];

	/*mutex lock for flip queue*/
	struct mutex sFlipQueueLock;
	/*context configure queue*/
	struct list_head sFlipQueues[MAX_PIPE_NUM];
	IMG_BOOL bFlipEnabled[MAX_PIPE_NUM];

	/* lock for plane pipe mapping */
	struct mutex sMappingLock;
	/* plane - pipe mapping */
	IMG_UINT32 ui32PlanePipeMapping[DC_PLANE_MAX][MAX_PLANE_INDEX];
	IMG_UINT32 ui32ExtraPowerIslandsStatus;

	struct plane_state plane_states[DC_PLANE_MAX][MAX_PLANE_INDEX];

	/* Timer to retire blocked flips */
        struct timer_list sFlipTimer;
        struct work_struct flip_retire_work;

} DC_MRFLD_DEVICE;

typedef struct {
	DC_MRFLD_DEVICE *psDevice;
} DC_MRFLD_DISPLAY_CONTEXT;

struct flip_plane {
	struct list_head list;
	int type;
	int index;
	int attached_pipe;
	DC_MRFLD_BUFFER *flip_buf;
	DC_MRFLD_SURF_CUSTOM  *flip_ctx;
};

struct DC_MRFLD_PIPE_INFO {
	IMG_UINT32 uiSwapInterval;
	struct list_head flip_planes;
};

/*flip status*/
enum DC_MRFLD_FLIP_STATUS {
	DC_MRFLD_FLIP_ERROR = 0,
	DC_MRFLD_FLIP_QUEUED,
	DC_MRFLD_FLIP_DC_UPDATED,
	DC_MRFLD_FLIP_DISPLAYED,
};

typedef struct {
	struct list_head sFlips[MAX_PIPE_NUM];
	IMG_UINT32 eFlipStates[MAX_PIPE_NUM];
	IMG_UINT32 uiVblankCounters[MAX_PIPE_NUM];
	struct DC_MRFLD_PIPE_INFO asPipeInfo[MAX_PIPE_NUM];
	IMG_BOOL bActivePipes[MAX_PIPE_NUM];
	IMG_UINT32 uiNumBuffers;
	IMG_UINT32 uiRefCount;
	IMG_HANDLE hConfigData;
	IMG_UINT32 uiSwapInterval;
	IMG_UINT32 uiPowerIslands;
	DC_MRFLD_BUFFER *pasBuffers[0];
} DC_MRFLD_FLIP;

/*exported functions*/
PVRSRV_ERROR MerrifieldDCInit(struct drm_device * dev);
PVRSRV_ERROR MerrifieldDCDeinit(void);

#endif /* __DC_MRFLD_H__ */
