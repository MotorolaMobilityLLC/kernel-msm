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
#include "img_defs.h"
#include "servicesext.h"
#include "allocmem.h"
#include "kerneldisplay.h"
#include "mm.h"
#include "pvrsrv_error.h"
#include "display_callbacks.h"
#include "dc_server.h"
#include "dc_mrfld.h"
#include "pwr_mgmt.h"
#include "psb_drv.h"
#ifndef ENABLE_HW_REPEAT_FRAME
#include "dc_maxfifo.h"
#endif

#if !defined(SUPPORT_DRM)
#error "SUPPORT_DRM must be set"
#endif

#define KEEP_UNUSED_CODE 0

static DC_MRFLD_DEVICE *gpsDevice;

#define	 DRVNAME "Merrifield-DRM"

/* GPU asks for 32 pixels of width alignment */
#define DC_MRFLD_WIDTH_ALIGN 32
#define DC_MRFLD_WIDTH_ALIGN_MASK (DC_MRFLD_WIDTH_ALIGN - 1)

/*DC plane asks for 64 bytes alignment*/
#define DC_MRFLD_STRIDE_ALIGN 64
#define DC_MRFLD_STRIDE_ALIGN_MASK (DC_MRFLD_STRIDE_ALIGN - 1)

/* Timeout for Flip Watchdog */
#define FLIP_TIMEOUT (HZ/4)

struct power_off_req {
	struct delayed_work work;
	struct plane_state *pstate;
};

static IMG_PIXFMT DC_MRFLD_Supported_PixelFormats[] = {
	/*supported RGB formats*/
	IMG_PIXFMT_B8G8R8A8_UNORM,
	IMG_PIXFMT_B5G6R5_UNORM,

	/*supported YUV formats*/
	IMG_PIXFMT_YUV420_2PLANE,
};

#if 0
static uint32_t DC_MRFLD_PixelFormat_Mapping[] = {
	[IMG_PIXFMT_B5G6R5_UNORM] = (0x5 << 26),
	[IMG_PIXFMT_B8G8R8A8_UNORM] = (0x6 << 26),
};
#endif

#ifdef CONFIG_SUPPORT_MIPI
static uint32_t DC_ExtraPowerIslands[DC_PLANE_MAX][MAX_PLANE_INDEX] = {
	{ 0,              0,              0},
#ifdef CONFIG_MOOREFIELD
	{ 0,              0,              0},
#else
	{ OSPM_DISPLAY_C, 0,              0},
#endif
	{ 0,              OSPM_DISPLAY_C, 0},
	{ 0,              0,              0},
};
#else
static uint32_t DC_ExtraPowerIslands[DC_PLANE_MAX][MAX_PLANE_INDEX] = {
	{ 0,              0,              0},
	{ OSPM_DISPLAY_A, 0,              0},
	{ OSPM_DISPLAY_A, OSPM_DISPLAY_C, 0},
	{ 0,              0,              0},
};
#endif

static inline IMG_UINT32 _Align_To(IMG_UINT32 ulValue,
				IMG_UINT32 ulAlignment)
{
	return (ulValue + ulAlignment - 1) & ~(ulAlignment - 1);
}

static IMG_BOOL _Is_Valid_PixelFormat(IMG_PIXFMT ePixelFormat)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(DC_MRFLD_Supported_PixelFormats); i++) {
		if (ePixelFormat == DC_MRFLD_Supported_PixelFormats[i])
			return IMG_TRUE;
	}

	return IMG_FALSE;
}

#if KEEP_UNUSED_CODE
/*
 * NOTE: only use the 1st plane now.
 */
static IMG_BOOL _Is_Valid_DC_Buffer(DC_BUFFER_IMPORT_INFO *psBufferInfo)
{
	if (!psBufferInfo)
		return IMG_FALSE;

	/*common check*/
	if (!psBufferInfo->ui32BPP || !psBufferInfo->ui32ByteStride[0] ||
		!psBufferInfo->ui32Height[0] || !psBufferInfo->ui32Width[0])
		return IMG_FALSE;

	/*check format*/
	if (!_Is_Valid_PixelFormat(psBufferInfo->ePixFormat))
		return IMG_FALSE;

	/*check stride*/
	if (psBufferInfo->ui32ByteStride[0] & DC_MRFLD_STRIDE_ALIGN_MASK)
		return IMG_FALSE;

	return IMG_TRUE;
}
#endif /* if KEEP_UNUSED_CODE */


static IMG_BOOL _Is_Task_KThread(void)
{
	/* skip task from user space and work queue */
	if (((current->flags & PF_NO_SETAFFINITY) == 0)
	    && ((current->flags & PF_WQ_WORKER) == 0)
	    && ((current->flags & PF_KTHREAD) != 0))
		return IMG_TRUE;
	else
		return IMG_FALSE;
}

static void _Update_PlanePipeMapping(DC_MRFLD_DEVICE *psDevice,
					IMG_UINT32 uiType,
					IMG_UINT32 uiIndex,
					IMG_INT32 iPipe)
{
	mutex_lock(&psDevice->sMappingLock);

	psDevice->ui32PlanePipeMapping[uiType][uiIndex] = iPipe;

	mutex_unlock(&psDevice->sMappingLock);
}

#if 0
static IMG_BOOL _Enable_ExtraPowerIslands(DC_MRFLD_DEVICE *psDevice,
					IMG_UINT32 ui32ExtraPowerIslands)
{
	IMG_UINT32 ui32PowerIslands = 0;

	/*turn on extra power islands which were not turned on*/
	ui32PowerIslands = psDevice->ui32ExtraPowerIslandsStatus &
			ui32ExtraPowerIslands;

	ui32ExtraPowerIslands &= ~ui32PowerIslands;

	if (!ui32ExtraPowerIslands)
		return IMG_TRUE;

	if (!power_island_get(ui32ExtraPowerIslands)) {
		DRM_ERROR("Failed to turn on islands %x\n",
			ui32ExtraPowerIslands);
		return IMG_FALSE;

	}

	psDevice->ui32ExtraPowerIslandsStatus |= ui32ExtraPowerIslands;
	return IMG_TRUE;
}

static IMG_BOOL _Disable_ExtraPowerIslands(DC_MRFLD_DEVICE *psDevice,
					IMG_UINT32 ui32ExtraPowerIslands)
{
	IMG_UINT32 ui32PowerIslands = 0;
	IMG_UINT32 ui32ActivePlanes;
	IMG_UINT32 ui32ActiveExtraIslands = 0;
	int i, j;

	/*turn off extra power islands which were turned on*/
	ui32PowerIslands = psDevice->ui32ExtraPowerIslandsStatus &
				ui32ExtraPowerIslands;

	if (!ui32PowerIslands)
		return IMG_TRUE;

	/*don't turn off extra power islands used by other planes*/
	for (i = 1; i < DC_PLANE_MAX; i++) {
		for (j = 0; j < MAX_PLANE_INDEX; j++) {
			ui32ActivePlanes = gpsDevice->ui32ActivePlanes[i];

			/* don't need to power it off when it's active */
			if (ui32ActivePlanes & (1 << j)) {
				ui32ActiveExtraIslands =
					DC_ExtraPowerIslands[i][j];
				/*remove power islands needed by this plane*/
				ui32PowerIslands &= ~ui32ActiveExtraIslands;
			}
		}
	}

	if (ui32PowerIslands)
		power_island_put(ui32PowerIslands);

	psDevice->ui32ExtraPowerIslandsStatus &= ~ui32PowerIslands;
	return IMG_TRUE;
}

static void _Flip_To_Surface(DC_MRFLD_DEVICE *psDevice,
				IMG_UINT32 ulSurfAddr,
				IMG_PIXFMT eFormat,
				IMG_UINT32 ulStride,
				IMG_INT iPipe)
{
	struct drm_device *psDrmDev = psDevice->psDrmDevice;
	uint32_t format = DC_MRFLD_PixelFormat_Mapping[eFormat];
	DCCBFlipToSurface(psDrmDev, ulSurfAddr, format, ulStride, iPipe);
}
#endif

static void _Flip_Overlay(DC_MRFLD_DEVICE *psDevice,
			DC_MRFLD_OVERLAY_CONTEXT *psContext,
			IMG_INT iPipe)
{
	if ((iPipe && psContext->pipe) || (!iPipe && !psContext->pipe))
		DCCBFlipOverlay(psDevice->psDrmDevice, psContext);
}

static void _Flip_Sprite(DC_MRFLD_DEVICE *psDevice,
			DC_MRFLD_SPRITE_CONTEXT *psContext,
			IMG_INT iPipe)
{
	if ((iPipe && psContext->pipe) || (!iPipe && !psContext->pipe))
		DCCBFlipSprite(psDevice->psDrmDevice, psContext);
}

static void _Flip_Primary(DC_MRFLD_DEVICE *psDevice,
			DC_MRFLD_PRIMARY_CONTEXT *psContext,
			IMG_INT iPipe)
{
	if ((iPipe && psContext->pipe) || (!iPipe && !psContext->pipe))
		DCCBFlipPrimary(psDevice->psDrmDevice, psContext);
}

static void _Flip_Cursor(DC_MRFLD_DEVICE *psDevice,
			DC_MRFLD_CURSOR_CONTEXT *psContext,
			IMG_INT iPipe)
{
	if ((iPipe && psContext->pipe) || (!iPipe && !psContext->pipe))
		DCCBFlipCursor(psDevice->psDrmDevice, psContext);
}

static void _Setup_ZOrder(DC_MRFLD_DEVICE *psDevice,
			DC_MRFLD_DC_PLANE_ZORDER *psZorder,
			IMG_INT iPipe)
{
	DCCBSetupZorder(psDevice->psDrmDevice, psZorder, iPipe);
}

void display_power_work(struct work_struct *work)
{
	struct power_off_req *req = container_of(work,
				struct power_off_req, work.work);
	struct plane_state *pstate = req->pstate;

	mutex_lock(&gpsDevice->sFlipQueueLock);

	if (!pstate->active) {
		if (pstate->extra_power_island  && !pstate->powered_off) {
			DRM_DEBUG("power off island %#x for plane (%d %d)\n",
				 pstate->extra_power_island,
				 pstate->type, pstate->index);
			power_island_put(pstate->extra_power_island);
			pstate->powered_off = true;
		}
		pstate->disabled = true;
	}

	mutex_unlock(&gpsDevice->sFlipQueueLock);

	kfree(req);
}

static void free_flip(DC_MRFLD_FLIP *psFlip)
{
	int i;
	struct flip_plane *tmp, *plane;
	DC_MRFLD_BUFFER *psBuffer;

	for (i = 0; i < MAX_PIPE_NUM; i++) {
		list_for_each_entry_safe(plane, tmp,
				&psFlip->asPipeInfo[i].flip_planes, list) {
			list_del(&plane->list);
			kfree(plane);
		}
	}

	/* free buffers which allocated in configureCheck*/
	for (i = 0; i < psFlip->uiNumBuffers; i++) {
		psBuffer = psFlip->pasBuffers[i];
		if (!psBuffer) {
			continue;
		}
		OSFreeMem(psBuffer);
	}

	kfree(psFlip);
}

static void disable_plane(struct plane_state *pstate)
{
	int type = pstate->type;
	int index = pstate->index;
	int pipe = pstate->attached_pipe;
	u32 ctx = 0;
	struct drm_psb_private *dev_priv = gpsDevice->psDrmDevice->dev_private;

	DRM_DEBUG("disable plane (%d %d)\n", type, index);

	switch (type) {
	case DC_SPRITE_PLANE:
		DCCBSpriteEnable(gpsDevice->psDrmDevice, ctx, index, 0);
		break;
	case DC_OVERLAY_PLANE:
		/* TODO: disable overlay, need to allocat backbuf in kernel? */
		DCCBSetPipeToOvadd(&ctx, pipe);
		DCCBOverlayEnable(gpsDevice->psDrmDevice, ctx, index, 0);
		break;
	case DC_PRIMARY_PLANE:
		DCCBPrimaryEnable(gpsDevice->psDrmDevice, ctx, index, 0);
		break;
	case DC_CURSOR_PLANE:
		DCCBCursorDisable(gpsDevice->psDrmDevice, index);
		break;
	default:
		DRM_ERROR("unsupport plane type (%d %d)\n", type, index);
		return;
	}

	{
		struct power_off_req *req = kzalloc(sizeof(*req), GFP_KERNEL);

		if (!req) {
			DRM_ERROR("can't alloc power off req, plane (%d %d)\n",
				  type, index);
			return;
		}

		DRM_DEBUG("schedule power off island %#x for plane (%d %d)\n",
			  pstate->extra_power_island, type, index);

		INIT_DELAYED_WORK(&req->work, display_power_work);
		req->pstate = pstate;

		queue_delayed_work(dev_priv->power_wq, &req->work,
				msecs_to_jiffies(50));
	}

	pstate->active = false;
}

static IMG_BOOL enable_plane(struct flip_plane *plane)
{
	int type = plane->type;
	int index = plane->index;
	struct plane_state *pstate = &gpsDevice->plane_states[type][index];

	if (pstate->powered_off && pstate->extra_power_island) {
		DRM_DEBUG("power on island %#x for plane (%d %d)\n",
			  pstate->extra_power_island, type, index);

		if (!power_island_get(pstate->extra_power_island)) {
			DRM_ERROR("fail to power on island %#x"
				  " for plane (%d %d)\n",
				  pstate->extra_power_island, type, index);
			return IMG_FALSE;
		}
		pstate->powered_off = false;
	}

	pstate->active = true;
	pstate->disabled = false;
	pstate->flip_active = true;
	pstate->attached_pipe = plane->attached_pipe;
	return IMG_TRUE;
}

static void clear_plane_flip_state(int pipe)
{
	int i, j;
	struct plane_state *pstate;

	for (i = 1; i < DC_PLANE_MAX; i++) {
		for (j = 0; j < MAX_PLANE_INDEX; j++) {
			pstate = &gpsDevice->plane_states[i][j];
			if (pstate->attached_pipe != pipe)
				continue;

			pstate->flip_active = false;
		}
	}
}

static bool disable_unused_planes(int pipe)
{
	int i, j;
	struct plane_state *pstate;
	bool ret = false;

	for (i = 1; i < DC_PLANE_MAX; i++) {
		for (j = 0; j < MAX_PLANE_INDEX; j++) {
			pstate = &gpsDevice->plane_states[i][j];

			/* if already inactive or not on this pipe */
			if (!pstate->active || pstate->attached_pipe != pipe)
				continue;

			DRM_DEBUG("plane (%d %d) active:%d, flip_active:%d\n",
				  i, j, pstate->active, pstate->flip_active);

			if (pstate->active && !pstate->flip_active) {
				disable_plane(pstate);
				ret = true;
			}
		}
	}

	return ret;
}

static void disable_ied_session(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	/* Make sure overlay planes are in-active prior to turning off IED */
	if (dev_priv->ied_force_clean) {
		struct plane_state *oa_state =
			&gpsDevice->plane_states[DC_OVERLAY_PLANE][0];
		struct plane_state *oc_state =
			&gpsDevice->plane_states[DC_OVERLAY_PLANE][1];
		uint32_t ret = 0;
		if ((oa_state->active == false) &&
			(oc_state->active == false)) {
			uint8_t i = MAX_IED_SESSIONS;
			do {
				ret = sepapp_drm_playback(false);
				if (ret) {
					DRM_ERROR("sepapp_drm_playback failed\
						IED clean-up: 0x%x\n", ret);
					break;
				}
			} while (i--);
			dev_priv->ied_enabled = false;
			dev_priv->ied_force_clean = false;
		}
	}
}
static void free_flip_states_on_pipe(struct drm_device *psDrmDev, int pipe)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;
	IMG_UINT32 eFlipState;

	if (pipe != DC_PIPE_A && pipe != DC_PIPE_B)
		return;

	psFlipQueue = &gpsDevice->sFlipQueues[pipe];

	list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[pipe])
	{
		eFlipState = psFlip->eFlipStates[pipe];

		if (eFlipState == DC_MRFLD_FLIP_DC_UPDATED) {
			/* done with this flip item, disable vsync now*/
			DCCBDisableVSyncInterrupt(psDrmDev, pipe);

			if (pipe != DC_PIPE_B)
				DCCBDsrAllow(psDrmDev, pipe);
		}

		/*remove this entry from flip queue, decrease refCount*/
		list_del(&psFlip->sFlips[pipe]);

		if ((psFlip->uiRefCount>0) && !(--psFlip->uiRefCount)) {
			/*retire all buffers possessed by this flip*/
			DCDisplayConfigurationRetired(
					psFlip->hConfigData);
			/* free it */
			free_flip(psFlip);
			psFlip = NULL;
		}
	}

	return;
}

static void timer_flip_handler(struct work_struct *work)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;
	IMG_UINT32 eFlipState;
	int iPipe;
	bool bHasUpdatedFlip[MAX_PIPE_NUM] = { false };
	bool bHasDisplayedFlip[MAX_PIPE_NUM] = { false };
	bool bHasPendingCommand[MAX_PIPE_NUM] = { false };

	if (!gpsDevice)
		return;

	/* acquire flip queue mutex */
	mutex_lock(&gpsDevice->sFlipQueueLock);

	/* check flip queue state */
	for (iPipe=DC_PIPE_A; iPipe<=DC_PIPE_B; iPipe++) {
		psFlipQueue = &gpsDevice->sFlipQueues[iPipe];

		list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe])
		{
			eFlipState = psFlip->eFlipStates[iPipe];
			if (eFlipState == DC_MRFLD_FLIP_DC_UPDATED) {
				bHasUpdatedFlip[iPipe] = true;
			} else if (eFlipState == DC_MRFLD_FLIP_DISPLAYED) {
				bHasDisplayedFlip[iPipe] = true;
			}
		}

		/* check if there are pending scp cmds */
		psFlip = list_first_entry_or_null(psFlipQueue, DC_MRFLD_FLIP, sFlips[iPipe]);
		if (psFlip && DCDisplayHasPendingCommand(psFlip->hConfigData))
			bHasPendingCommand[iPipe] = true;

		if (bHasUpdatedFlip[iPipe]) {
			DRM_INFO("flip timer triggered, maybe vsync lost on pipe%d!\n", iPipe);
		} else if (bHasDisplayedFlip[iPipe] && bHasPendingCommand[iPipe]) {
			DRM_INFO("flip timer triggered, scp cmd pending on pipe%d!\n", iPipe);
		}

		/* free all flips whenever vysnc lost or scp cmd pending */
		if ((bHasUpdatedFlip[iPipe]) ||
		    (bHasDisplayedFlip[iPipe] && bHasPendingCommand[iPipe]))
			free_flip_states_on_pipe(gpsDevice->psDrmDevice, iPipe);

		if (list_empty_careful(psFlipQueue))
			INIT_LIST_HEAD(&gpsDevice->sFlipQueues[iPipe]);
	}

	mutex_unlock(&gpsDevice->sFlipQueueLock);

	return;
}

static void _Flip_Timer_Fn(unsigned long arg)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)arg;

        schedule_work(&psDevice->flip_retire_work);
}

static IMG_BOOL _Do_Flip(DC_MRFLD_FLIP *psFlip, int iPipe)
{
	struct intel_dc_plane_zorder *zorder = NULL;
	DC_MRFLD_BUFFER **pasBuffers;
	struct flip_plane *plane;
	IMG_UINT32 uiNumBuffers;
	IMG_BOOL bUpdated;
	unsigned long flags;

	if (!gpsDevice || !psFlip) {
		DRM_ERROR("%s: Invalid Flip\n", __func__);
		return IMG_FALSE;
	}

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B) {
		DRM_ERROR("%s: Invalid pipe %d\n", __func__, iPipe);
		return IMG_FALSE;
	}

	/* skip it if updated*/
	if (psFlip->eFlipStates[iPipe] == DC_MRFLD_FLIP_DC_UPDATED)
		return IMG_TRUE;

	pasBuffers = psFlip->pasBuffers;
	uiNumBuffers = psFlip->uiNumBuffers;

	if (!pasBuffers || !uiNumBuffers) {
		DRM_ERROR("%s: Invalid buffer list\n", __func__);
		return IMG_FALSE;
	}

	/*turn on required power islands*/
	if (!power_island_get(psFlip->uiPowerIslands))
		return IMG_FALSE;

	/* start update display controller hardware */
	bUpdated = IMG_FALSE;

	if (iPipe != DC_PIPE_B)
		DCCBDsrForbid(gpsDevice->psDrmDevice, iPipe);

	/*
	 * make sure vsync interrupt of this pipe is active before kicking
	 * off flip
	 */
	if (DCCBEnableVSyncInterrupt(gpsDevice->psDrmDevice, iPipe)) {
		DRM_ERROR("%s: failed to enable vsync on pipe %d\n",
				__func__, iPipe);
		if (iPipe != DC_PIPE_B)
			DCCBDsrAllow(gpsDevice->psDrmDevice, iPipe);
		goto err_out;
	}

	clear_plane_flip_state(iPipe);

	list_for_each_entry(plane,
			    &(psFlip->asPipeInfo[iPipe].flip_planes), list) {
		zorder = &plane->flip_ctx->zorder;

		enable_plane(plane);
	}

	/* Delay the Flip if we are close the Vblank interval since we
	 * do not want to update plane registers during the vblank period
	 */
	DCCBAvoidFlipInVblankInterval(gpsDevice->psDrmDevice, iPipe);
	local_irq_save(flags);

	list_for_each_entry(plane,
			    &(psFlip->asPipeInfo[iPipe].flip_planes), list) {
		int type = plane->type;
		int index = plane->index;
		struct plane_state *pstate =
			&gpsDevice->plane_states[type][index];

		if (!pstate->active)
			continue;

		switch (plane->type) {
		case DC_SPRITE_PLANE:
			if (!plane->flip_ctx->ctx.sp_ctx.surf)
				plane->flip_ctx->ctx.sp_ctx.surf =
					plane->flip_buf->sDevVAddr.uiAddr;
			_Flip_Sprite(gpsDevice,
					&plane->flip_ctx->ctx.sp_ctx, iPipe);
			break;
		case DC_PRIMARY_PLANE:
			if (!plane->flip_ctx->ctx.prim_ctx.surf)
				plane->flip_ctx->ctx.prim_ctx.surf =
					plane->flip_buf->sDevVAddr.uiAddr;
			_Flip_Primary(gpsDevice,
					&plane->flip_ctx->ctx.prim_ctx, iPipe);
			break;

		case DC_OVERLAY_PLANE:
			_Flip_Overlay(gpsDevice,
					&plane->flip_ctx->ctx.ov_ctx, iPipe);
			break;
		case DC_CURSOR_PLANE:
			_Flip_Cursor(gpsDevice,
					&plane->flip_ctx->ctx.cs_ctx, iPipe);
			break;
		}
	}

	local_irq_restore(flags);

	if (zorder)
		_Setup_ZOrder(gpsDevice, zorder, iPipe);

	disable_ied_session(gpsDevice->psDrmDevice);

	disable_unused_planes(iPipe);

	psFlip->eFlipStates[iPipe] = DC_MRFLD_FLIP_DC_UPDATED;
	psFlip->uiVblankCounters[iPipe] =
			drm_vblank_count(gpsDevice->psDrmDevice, iPipe);

	/*start Flip watch dog*/
	mod_timer(&gpsDevice->sFlipTimer, FLIP_TIMEOUT + jiffies);

	bUpdated = IMG_TRUE;

#ifndef ENABLE_HW_REPEAT_FRAME
	/* maxfifo is only enabled in mipi only mode */
	if (iPipe == DC_PIPE_A && !hdmi_state)
		maxfifo_timer_start(gpsDevice->psDrmDevice);
#endif
err_out:
	power_island_put(psFlip->uiPowerIslands);
	return bUpdated;
}

static DC_MRFLD_FLIP *_Next_Queued_Flip(int iPipe)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;
	DC_MRFLD_FLIP *psQueuedFlip = 0;

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B) {
		DRM_ERROR("%s: Invalid pipe %d\n", __func__, iPipe);
		return IMG_NULL;
	}

	psFlipQueue = &gpsDevice->sFlipQueues[iPipe];

	list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe])
	{
		if (psFlip->eFlipStates[iPipe] == DC_MRFLD_FLIP_QUEUED) {
			psQueuedFlip = psFlip;
			break;
		}
	}

	return psQueuedFlip;
}

static bool _Can_Flip(int iPipe)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;
	bool ret = true;
	int num_queued = 0;
	int num_updated = 0;

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B) {
		DRM_ERROR("%s: Invalid pipe %d\n", __func__, iPipe);
		return false;
	}

	psFlipQueue = &gpsDevice->sFlipQueues[iPipe];
	list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe])
	{
		if (psFlip->eFlipStates[iPipe] == DC_MRFLD_FLIP_QUEUED) {
			num_queued++;
			ret = false;
		}
		if (psFlip->eFlipStates[iPipe] == DC_MRFLD_FLIP_DC_UPDATED) {
			num_updated++;
			ret = false;
		}
	}

	if (num_queued > 2) {
		DRM_ERROR("num of queued buffers is %d\n", num_queued);
	}

	if (num_updated > 1) {
		DRM_ERROR("num of updated buffers is %d\n", num_updated);
	}
	return ret;
}

static void _Dispatch_Flip(DC_MRFLD_FLIP *psFlip)
{
	DC_MRFLD_SURF_CUSTOM *psSurfCustom;
	DC_MRFLD_BUFFER **pasBuffers;
	IMG_UINT32 uiNumBuffers;
	struct flip_plane *flip_plane;
	int type, index, pipe;
	int i, j;
	bool send_wms = false;

	if (!gpsDevice || !psFlip) {
		DRM_ERROR("%s: Invalid Flip\n", __func__);
		return;
	}

	pasBuffers = psFlip->pasBuffers;
	uiNumBuffers = psFlip->uiNumBuffers;

	if (!pasBuffers || !uiNumBuffers) {
		DRM_ERROR("%s: Invalid buffer list\n", __func__);
		return;
	}

	mutex_lock(&gpsDevice->sMappingLock);

	for (i = 0; i < uiNumBuffers; i++) {
		if (pasBuffers[i]->eFlipOp == DC_MRFLD_FLIP_SURFACE) {
			/* assign to pipe 0 by default*/
			psFlip->bActivePipes[DC_PIPE_A] = IMG_TRUE;
			continue;
		}

		if (pasBuffers[i]->eFlipOp != DC_MRFLD_FLIP_CONTEXT) {
			DRM_ERROR("%s: bad flip operation %d\n", __func__,
					pasBuffers[i]->eFlipOp);
			continue;
		}

		for (j = 0; j < pasBuffers[i]->ui32ContextCount; j++) {
			psSurfCustom = &pasBuffers[i]->sContext[j];

			type = psSurfCustom->type;
			index = -1;
			pipe = -1;

			switch (type) {
			case DC_SPRITE_PLANE:
				/*Flip sprite context*/
				index = psSurfCustom->ctx.sp_ctx.index;
				pipe = psSurfCustom->ctx.sp_ctx.pipe;
				break;
			case DC_PRIMARY_PLANE:
				index = psSurfCustom->ctx.prim_ctx.index;
				pipe = psSurfCustom->ctx.prim_ctx.pipe;
				break;
			case DC_OVERLAY_PLANE:
				index = psSurfCustom->ctx.ov_ctx.index;
				pipe = psSurfCustom->ctx.ov_ctx.pipe;
				break;
			case DC_CURSOR_PLANE:
				index = psSurfCustom->ctx.cs_ctx.index;
				pipe = psSurfCustom->ctx.cs_ctx.pipe;
				break;
			default:
				DRM_ERROR("Unknown plane type %d\n",
						psSurfCustom->type);
				break;
			}

			if (index < 0 || pipe < 0 || pipe >= MAX_PIPE_NUM) {
				DRM_ERROR("Invalid index = %d, pipe = %d\n",
						index, pipe);
				continue;
			}

			flip_plane = kzalloc(sizeof(*flip_plane), GFP_KERNEL);
			if (!flip_plane) {
				DRM_ERROR("fail to alloc flip plane\n");
				continue;
			}

			flip_plane->type = type;
			flip_plane->index = index;
			flip_plane->attached_pipe = pipe;
			flip_plane->flip_buf = pasBuffers[i];
			flip_plane->flip_ctx = psSurfCustom;

			list_add_tail(&flip_plane->list,
				      &psFlip->asPipeInfo[pipe].flip_planes);
			DRM_DEBUG("flip plane (%d %d) context %p to pipe %d\n",
					type, index, psSurfCustom, pipe);

			/* update flip active pipes */
			if (pipe)
				psFlip->bActivePipes[DC_PIPE_B] = IMG_TRUE;
			else
				psFlip->bActivePipes[DC_PIPE_A] = IMG_TRUE;

			/* check whether needs extra power island*/
			psFlip->uiPowerIslands |=
				DC_ExtraPowerIslands[type][index];

			/* update plane - pipe mapping*/
			gpsDevice->ui32PlanePipeMapping[type][index] = pipe;
		}
	}
	mutex_unlock(&gpsDevice->sMappingLock);

	mutex_lock(&gpsDevice->sFlipQueueLock);
	/* dispatch this flip*/
	for (i = 0; i < MAX_PIPE_NUM; i++) {
		if (psFlip->bActivePipes[i] && gpsDevice->bFlipEnabled[i]) {
			/* if pipe is not active */
			if (!DCCBIsPipeActive(gpsDevice->psDrmDevice, i))
				continue;

			/*turn on pipe power island based on active pipes*/
			if (i == 0)
				psFlip->uiPowerIslands |= OSPM_DISPLAY_A;
			else
				psFlip->uiPowerIslands |= OSPM_DISPLAY_B;

			psFlip->asPipeInfo[i].uiSwapInterval =
				psFlip->uiSwapInterval;

			/* if there's no pending queued flip, flip it*/
			if (_Can_Flip(i)) {
				/* don't queue it, if failed to update DC*/
				if (!_Do_Flip(psFlip,i))
					continue;
				else if (i != DC_PIPE_B)
					send_wms = true;
			}

			/*increase refCount*/
			psFlip->uiRefCount++;

			INIT_LIST_HEAD(&psFlip->sFlips[i]);
			list_add_tail(&psFlip->sFlips[i],
					&gpsDevice->sFlipQueues[i]);
		} else if (!psFlip->bActivePipes[i] && gpsDevice->bFlipEnabled[i]) {
			/* give a chance to disable planes on inactive pipe */
			clear_plane_flip_state(i);
			if (disable_unused_planes(i)) {
				if (i != DC_PIPE_B) {
					send_wms = true;
				}
			}

			/*
			 * NOTE:To inactive pipe, need to free the attached
			 * flipStates on FlipQueue; otherwise, as all pipes
			 * share same Flip item, other active pipes will be
			 * blocked by this pipe and no chance to get buffer
			 * retired.
			 */
			free_flip_states_on_pipe(gpsDevice->psDrmDevice, i);
		}
	}

	/* if failed to dispatch, skip this flip*/
	if (!psFlip->uiRefCount) {
		DCDisplayConfigurationRetired(psFlip->hConfigData);
		/* free it */
		free_flip(psFlip);
		psFlip = NULL;
	}

#ifdef CONFIG_SUPPORT_MIPI
	if (send_wms) {

		/* Ensure that *psFlip is not freed while lock is not held. */
		if (psFlip)
			psFlip->uiRefCount++;

		mutex_unlock(&gpsDevice->sFlipQueueLock);
		DCCBWaitForDbiFifoEmpty(gpsDevice->psDrmDevice, DC_PIPE_A);
		mutex_lock(&gpsDevice->sFlipQueueLock);

		if (psFlip != NULL && --psFlip->uiRefCount == 0) {
			DCDisplayConfigurationRetired(psFlip->hConfigData);
			/* free it */
			free_flip(psFlip);
			psFlip = NULL;
		}

		/* Issue "write_mem_start" for command mode panel. */
		DCCBUpdateDbiPanel(gpsDevice->psDrmDevice, DC_PIPE_A);
		if (psFlip)
			psFlip->uiVblankCounters[DC_PIPE_A] =
				drm_vblank_count(gpsDevice->psDrmDevice, DC_PIPE_A);
	}
#endif

	mutex_unlock(&gpsDevice->sFlipQueueLock);
}

static void _Queue_Flip(IMG_HANDLE hConfigData, IMG_HANDLE *ahBuffers,
			IMG_UINT32 uiNumBuffers, IMG_UINT32 ui32DisplayPeriod)
{
	IMG_UINT32 uiFlipSize;
	DC_MRFLD_BUFFER *psBuffer;
	DC_MRFLD_FLIP *psFlip;
	int i;

	uiFlipSize = sizeof(DC_MRFLD_FLIP);
	uiFlipSize += uiNumBuffers * sizeof(DC_MRFLD_BUFFER*);

	psFlip = kzalloc(uiFlipSize , GFP_KERNEL);
	if (!gpsDevice || !psFlip) {
		DRM_ERROR("Failed to allocate a flip\n");
		/*force it to complete*/
		DCDisplayConfigurationRetired(hConfigData);
		return;
	}

	/*set flip state as queued*/
	for (i = 0; i < MAX_PIPE_NUM; i++) {
		psFlip->eFlipStates[i] = DC_MRFLD_FLIP_QUEUED;
		psFlip->bActivePipes[i] = IMG_FALSE;
		INIT_LIST_HEAD(&psFlip->asPipeInfo[i].flip_planes);
	}

	/*update buffer number*/
	psFlip->uiNumBuffers = uiNumBuffers;

	/*initialize buffers*/
	for (i = 0; i < uiNumBuffers; i++) {
		psBuffer = ahBuffers[i];
		if (!psBuffer) {
			DRM_DEBUG("%s: buffer %d is empty!\n", __func__, i);
			continue;
		}
		psFlip->pasBuffers[i] = psBuffer;
	}

	psFlip->uiSwapInterval = ui32DisplayPeriod;

	psFlip->hConfigData = hConfigData;

	/*queue it to flip queue*/
	_Dispatch_Flip(psFlip);
}

static int _Vsync_ISR(struct drm_device *psDrmDev, int iPipe)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;
	DC_MRFLD_FLIP *psNextFlip;
	IMG_UINT32 eFlipState;
	IMG_UINT32 uiVblankCounter;
	IMG_BOOL bNewFlipUpdated = IMG_FALSE;

	if (!gpsDevice)
		return IMG_TRUE;

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B)
		return IMG_FALSE;

	/* acquire flip queue mutex */
	mutex_lock(&gpsDevice->sFlipQueueLock);

	psFlipQueue = &gpsDevice->sFlipQueues[iPipe];

	/*
	 * on vsync interrupt arrival:
	 * 1) surface composer would be unblocked to switch to a new buffer,
	 *    the displayed (old) buffer will be released at this point
	 * 2) we release the displayed buffer here to align with the surface
	 *    composer's buffer management mechanism.
	 * 3) in MOST cases, surface composer would release the displayed buffer
	 *    till it has been switched to consume a new buffer, so it is safe
	 *    to release the displayed buffer here. However, since there is
	 *    a work queue between surface composer and our kernel display class
	 *    driver, if the flip work wasn't scheduled on time and the released
	 *    displaying buffer was dequeued by a client and being rendered
	 *    then tearing might happen on the screen.
	 */
	uiVblankCounter	= drm_vblank_count(psDrmDev, iPipe);

	list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe])
	{
		/* Only when new flip updated and TE/VBLANK comes after
		 * that flip indicates the new flip takes effects on screen.
		 */
		eFlipState = psFlip->eFlipStates[iPipe];

		if ((eFlipState == DC_MRFLD_FLIP_DC_UPDATED) &&
		    (uiVblankCounter > psFlip->uiVblankCounters[iPipe])) {
			bNewFlipUpdated = IMG_TRUE;
			break;
		}
	}

	/* if new flip updated on pipe, give a chance to retire */
	if (bNewFlipUpdated) {
		list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe])
		{
			eFlipState = psFlip->eFlipStates[iPipe];
			if (eFlipState == DC_MRFLD_FLIP_DISPLAYED) {
				/*remove this entry from flip queue, decrease refCount*/
				list_del(&psFlip->sFlips[iPipe]);

				if (!(--psFlip->uiRefCount)) {
					/*retire all buffers possessed by this flip*/
					DCDisplayConfigurationRetired(
							psFlip->hConfigData);
					/* free it */
					free_flip(psFlip);
					psFlip = NULL;
				}
			} else if (eFlipState == DC_MRFLD_FLIP_DC_UPDATED) {

				psFlip->eFlipStates[iPipe] = DC_MRFLD_FLIP_DISPLAYED;

				/* done with this flip item, disable vsync now*/
				DCCBDisableVSyncInterrupt(psDrmDev, iPipe);

				if (iPipe != DC_PIPE_B)
					DCCBDsrAllow(psDrmDev, iPipe);
				break;
			}
		}
	}

	/*if flip queue isn't empty, flip the first queued flip*/
	psNextFlip = _Next_Queued_Flip(iPipe);
	if (psNextFlip) {
		/* failed to update DC, release it*/
		if (!_Do_Flip(psNextFlip, iPipe)) {

			/*remove this entry from flip queue, decrease refCount*/
			list_del(&psNextFlip->sFlips[iPipe]);

			if (!(--psNextFlip->uiRefCount)) {
				/*retire all buffers possessed by this flip*/
				DCDisplayConfigurationRetired(
						psNextFlip->hConfigData);
				/* free it */
				free_flip(psNextFlip);
				psNextFlip = NULL;
			}
		} else if (iPipe == DC_PIPE_A) {
			mutex_unlock(&gpsDevice->sFlipQueueLock);
			DCCBWaitForDbiFifoEmpty(gpsDevice->psDrmDevice,
						DC_PIPE_A);
			mutex_lock(&gpsDevice->sFlipQueueLock);

			DCCBUpdateDbiPanel(gpsDevice->psDrmDevice, iPipe);
		}
	}

	if (bNewFlipUpdated) {
		/*start Flip watch dog*/
		mod_timer(&gpsDevice->sFlipTimer, FLIP_TIMEOUT + jiffies);
	}

	if (list_empty_careful(psFlipQueue))
		INIT_LIST_HEAD(&gpsDevice->sFlipQueues[iPipe]);

	mutex_unlock(&gpsDevice->sFlipQueueLock);
	return IMG_TRUE;
}

/*----------------------------------------------------------------------------*/

static IMG_VOID DC_MRFLD_GetInfo(IMG_HANDLE hDeviceData,
				DC_DISPLAY_INFO *psDisplayInfo)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;

	DRM_DEBUG("%s\n", __func__);

	if (psDevice && psDisplayInfo)
		*psDisplayInfo = psDevice->sDisplayInfo;
}

static PVRSRV_ERROR DC_MRFLD_PanelQueryCount(IMG_HANDLE hDeviceData,
						IMG_UINT32 *ppui32NumPanels)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;
	if (!psDevice || !ppui32NumPanels)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	*ppui32NumPanels = 1;
	return PVRSRV_OK;
}

static PVRSRV_ERROR DC_MRFLD_PanelQuery(IMG_HANDLE hDeviceData,
				IMG_UINT32 ui32PanelsArraySize,
				IMG_UINT32 *pui32NumPanels,
				PVRSRV_PANEL_INFO *pasPanelInfo)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;

	if (!psDevice || !pui32NumPanels || !pasPanelInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	*pui32NumPanels = 1;

	pasPanelInfo[0].sSurfaceInfo = psDevice->sPrimInfo;
	/*TODO: export real panel info*/
	/*pasPanelInfo[0].ui32RefreshRate = 60;*/
	/*pasPanelInfo[0].ui32PhysicalWidthmm = 0;*/
	/*pasPanelInfo[0].ui32PhysicalHeightmm = 0;*/

	return PVRSRV_OK;
}

static PVRSRV_ERROR DC_MRFLD_FormatQuery(IMG_HANDLE hDeviceData,
					IMG_UINT32 ui32NumFormats,
					PVRSRV_SURFACE_FORMAT *pasFormat,
					IMG_UINT32 *pui32Supported)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;
	int i;

	if (!psDevice || !pasFormat || !pui32Supported)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	for (i = 0; i < ui32NumFormats; i++)
		pui32Supported[i] =
			_Is_Valid_PixelFormat(pasFormat[i].ePixFormat) ? 1 : 0;

	return PVRSRV_OK;
}

static PVRSRV_ERROR DC_MRFLD_DimQuery(IMG_HANDLE hDeviceData,
					IMG_UINT32 ui32NumDims,
					PVRSRV_SURFACE_DIMS *psDim,
					IMG_UINT32 *pui32Supported)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;
	int i;

	if (!psDevice || !psDim || !pui32Supported)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	for (i = 0; i < ui32NumDims; i++) {
		pui32Supported[i] = 0;

		if (psDim[i].ui32Width != psDevice->sPrimInfo.sDims.ui32Width)
			continue;
		if (psDim[i].ui32Height != psDevice->sPrimInfo.sDims.ui32Height)
			continue;

		pui32Supported[i] = 1;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR DC_MRFLD_BufferSystemAcquire(IMG_HANDLE hDeviceData,
					IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
					IMG_UINT32 *pui32PageCount,
					IMG_UINT32 *pui32PhysHeapID,
					IMG_UINT32 *pui32ByteStride,
					IMG_HANDLE *phSystemBuffer)
{
	DC_MRFLD_DEVICE *psDevice = (DC_MRFLD_DEVICE *)hDeviceData;
	IMG_UINT32 ulPagesNumber, ulBufferSize;

	if (!psDevice || !puiLog2PageSize || !pui32PageCount ||
		!pui32PhysHeapID || !pui32ByteStride || !phSystemBuffer)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	ulBufferSize = psDevice->psSystemBuffer->ui32BufferSize;
	ulPagesNumber = (ulBufferSize + (PAGE_SIZE - 1))  >> PAGE_SHIFT;

	*puiLog2PageSize = PAGE_SHIFT;
	*pui32PageCount = ulPagesNumber;
	*pui32PhysHeapID = 0;
	*pui32ByteStride = psDevice->psSystemBuffer->ui32ByteStride;
	*phSystemBuffer = (IMG_HANDLE)psDevice->psSystemBuffer;

	return PVRSRV_OK;
}

static IMG_VOID DC_MRFLD_BufferSystemRelease(IMG_HANDLE hSystemBuffer)
{
	/*TODO: do something here*/
}

static PVRSRV_ERROR DC_MRFLD_ContextCreate(IMG_HANDLE hDeviceData,
					IMG_HANDLE *hDisplayContext)
{
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext = NULL;
	DC_MRFLD_DEVICE *psDevice;
	PVRSRV_ERROR eRes = PVRSRV_OK;

	if (!hDisplayContext)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	psDevice = (DC_MRFLD_DEVICE *)hDeviceData;

	/*Allocate a new context*/
	psDisplayContext =
		kzalloc(sizeof(DC_MRFLD_DISPLAY_CONTEXT), GFP_KERNEL);
	if (!psDisplayContext) {
		DRM_ERROR("Failed to create display context\n");
		eRes = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto create_error;
	}

	psDisplayContext->psDevice = psDevice;
	*hDisplayContext = (IMG_HANDLE)psDisplayContext;
create_error:
	return eRes;
}

/* TODO: refine function name: buffers will be copied in ahBuffers */
static PVRSRV_ERROR DC_MRFLD_ContextConfigureCheck(
				IMG_HANDLE hDisplayContext,
				IMG_UINT32 ui32PipeCount,
				PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
				IMG_HANDLE *ahBuffers)
{
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext =
		(DC_MRFLD_DISPLAY_CONTEXT *)hDisplayContext;
	DC_MRFLD_DEVICE *psDevice;
	DC_MRFLD_SURF_CUSTOM *psSurfCustom;
	DC_MRFLD_BUFFER *psBuffer;
	int err;
	int i, j;

	if (!psDisplayContext || !pasSurfAttrib || !ahBuffers)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	psDevice = psDisplayContext->psDevice;

	/*TODO: handle ui32PipeCount = 0*/

	/* reset buffer context count*/
	for (i = 0; i < ui32PipeCount; i++) {
		if (!ahBuffers[i]) {
			continue;
		}
		psBuffer = OSAllocMem(sizeof(DC_MRFLD_BUFFER));
		if (psBuffer == IMG_NULL) {
			for (j = 0; j < i; j++) {
				if (ahBuffers[j]) {
					OSFreeMem(ahBuffers[j]);
				}
			}
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
		OSMemCopy(psBuffer, ahBuffers[i], sizeof(DC_MRFLD_BUFFER));
		psBuffer->ui32ContextCount = 0;
		ahBuffers[i] = psBuffer;
	}

	for (i = 0; i < ui32PipeCount; i++) {
		psBuffer = (DC_MRFLD_BUFFER *)ahBuffers[i];
		if (!psBuffer) {
			DRM_ERROR("%s: no buffer for layer %d\n", __func__, i);
			continue;
		}

		/*post flip*/
		if (!pasSurfAttrib[i].ui32Custom) {
			psBuffer->eFlipOp = DC_MRFLD_FLIP_SURFACE;
			continue;
		}

		/*check context count*/
		if (psBuffer->ui32ContextCount >= MAX_CONTEXT_COUNT) {
			DRM_ERROR("%s: plane context overflow\n", __func__);
			continue;
		}

		psSurfCustom =
			&psBuffer->sContext[psBuffer->ui32ContextCount++];

		/*copy the context from userspace*/
		err = copy_from_user(psSurfCustom,
				     (void *)(uintptr_t)
				     pasSurfAttrib[i].ui32Custom,
				     sizeof(DC_MRFLD_SURF_CUSTOM));
		if (err) {
			DRM_ERROR("Failed to copy plane context\n");
			continue;
		}

		psBuffer->eFlipOp = DC_MRFLD_FLIP_CONTEXT;
	}

	return PVRSRV_OK;
}

static IMG_VOID DC_MRFLD_ContextConfigure(IMG_HANDLE hDisplayContext,
				IMG_UINT32 ui32PipeCount,
				PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
				IMG_HANDLE *ahBuffers,
				IMG_UINT32 ui32DisplayPeriod,
				IMG_HANDLE hConfigData)
{
	DRM_DEBUG("%s\n", __func__);

	if (!ui32PipeCount) {
		/* Called from DCDisplayContextDestroy()
		 * Retire the current config  */
		DCDisplayConfigurationRetired(hConfigData);
		return;
	}

	/*queue this configure update*/
	_Queue_Flip(hConfigData, ahBuffers, ui32PipeCount, ui32DisplayPeriod);
}

static IMG_VOID DC_MRFLD_ContextDestroy(IMG_HANDLE hDisplayContext)
{
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext =
		(DC_MRFLD_DISPLAY_CONTEXT *)hDisplayContext;
	kfree(psDisplayContext);
}

/**
 *
 */
static PVRSRV_ERROR DC_MRFLD_BufferAlloc(IMG_HANDLE hDisplayContext,
					DC_BUFFER_CREATE_INFO *psCreateInfo,
					IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
					IMG_UINT32 *pui32PageCount,
					IMG_UINT32 *pui32PhysHeapID,
					IMG_UINT32 *pui32ByteStride,
					IMG_HANDLE *phBuffer)
{
	PVRSRV_ERROR eRes = PVRSRV_OK;
	DC_MRFLD_BUFFER *psBuffer = NULL;
	PVRSRV_SURFACE_INFO *psSurfInfo;
	IMG_UINT32 ulPagesNumber;
	IMG_UINT32 i, j;
	DC_MRFLD_DEVICE *psDevice;
	struct drm_device *psDrmDev;
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext =
		(DC_MRFLD_DISPLAY_CONTEXT *)hDisplayContext;

	if (!psDisplayContext || !psCreateInfo || !puiLog2PageSize ||
		!pui32PageCount || !pui32PhysHeapID || !pui32ByteStride ||
		!phBuffer)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	psBuffer = kzalloc(sizeof(DC_MRFLD_BUFFER), GFP_KERNEL);
	if (!psBuffer) {
		DRM_ERROR("Failed to create buffer\n");
		eRes = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto create_error;
	}

	psSurfInfo = &psCreateInfo->sSurface;

	/*
	 * As we're been asked to allocate this buffer we decide what it's
	 * stride should be.
	 */
	psBuffer->eSource = DCMrfldEX_BUFFER_SOURCE_ALLOC;
	psBuffer->hDisplayContext = hDisplayContext;

	/* Align 32 pixels of width alignment */
	psBuffer->ui32Width =
		_Align_To(psSurfInfo->sDims.ui32Width, DC_MRFLD_WIDTH_ALIGN_MASK);

	psBuffer->ui32ByteStride =
		psBuffer->ui32Width * psCreateInfo->ui32BPP;
	/*align stride*/
	psBuffer->ui32ByteStride =
		_Align_To(psBuffer->ui32ByteStride, DC_MRFLD_STRIDE_ALIGN);

	psBuffer->ui32Height = psSurfInfo->sDims.ui32Height;
	psBuffer->ui32BufferSize =
		psBuffer->ui32Height * psBuffer->ui32ByteStride;
	psBuffer->ePixFormat = psSurfInfo->sFormat.ePixFormat;

	/*
	 * Allocate display adressable memory. We only need physcial addresses
	 * at this stage.
	 * Note: This could be defered till the 1st map or acquire call.
	 * IMG uses pgprot_noncached(PAGE_KERNEL)
	 */
	psBuffer->sCPUVAddr = __vmalloc(psBuffer->ui32BufferSize,
			GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			 __pgprot((pgprot_val(PAGE_KERNEL) & ~_PAGE_CACHE_MASK)
			| _PAGE_CACHE_WC));
	/*FIXME: */
	//DCCBGetStolen(gpsDevice->psDrmDevice, &psBuffer->sCPUVAddr, &psBuffer->ui32BufferSize);
	if (psBuffer->sCPUVAddr == NULL) {
		DRM_ERROR("Failed to allocate buffer\n");
		eRes = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto alloc_error;
	}

	ulPagesNumber =
		(psBuffer->ui32BufferSize + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

	psBuffer->psSysAddr =
		kzalloc(ulPagesNumber * sizeof(IMG_SYS_PHYADDR), GFP_KERNEL);
	if (!psBuffer->psSysAddr) {
		DRM_ERROR("Failed to allocate phy array\n");
		eRes = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto phy_error;
	}

	i = 0; j = 0;
	for (i = 0; i < psBuffer->ui32BufferSize; i += PAGE_SIZE) {
#if defined(UNDER_WDDM)
		psBuffer->psSysAddr[j++].uiAddr =
			(IMG_UINTPTR_T)vmalloc_to_pfn(psBuffer->sCPUVAddr + i) << PAGE_SHIFT;
#else
		psBuffer->psSysAddr[j++].uiAddr =
			(IMG_UINT64)vmalloc_to_pfn(psBuffer->sCPUVAddr + i) << PAGE_SHIFT;
#endif
	}

	psBuffer->bIsAllocated = IMG_TRUE;
	psBuffer->bIsContiguous = IMG_FALSE;
	psBuffer->ui32OwnerTaskID = task_tgid_nr(current);

	psDevice = psDisplayContext->psDevice;
	psDrmDev = psDevice->psDrmDevice;

	ulPagesNumber =
		(psBuffer->ui32BufferSize + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

	/*map this buffer to gtt*/
	DCCBgttMapMemory(psDrmDev,
		(unsigned int)(uintptr_t)psBuffer,
		psBuffer->ui32OwnerTaskID,
		psBuffer->psSysAddr,
		ulPagesNumber,
		(unsigned int *)&psBuffer->sDevVAddr.uiAddr);

	psBuffer->sDevVAddr.uiAddr <<= PAGE_SHIFT;

	/*setup output params*/
	*pui32ByteStride = psBuffer->ui32ByteStride;
	*puiLog2PageSize = PAGE_SHIFT;
	*pui32PageCount = ulPagesNumber;
	*pui32PhysHeapID = 0;
	*phBuffer = psBuffer;

	DRM_DEBUG("%s: allocated buffer: %dx%d\n", __func__,
		psBuffer->ui32Width, psBuffer->ui32Height);

	return PVRSRV_OK;
phy_error:
	vfree(psBuffer->sCPUVAddr);
alloc_error:
	kfree(psBuffer);
create_error:
	return eRes;
}

static PVRSRV_ERROR DC_MRFLD_BufferImport(IMG_HANDLE hDisplayContext,
					IMG_UINT32 ui32NumPlanes,
					IMG_HANDLE **paphImport,
					DC_BUFFER_IMPORT_INFO *psSurfAttrib,
					IMG_HANDLE *phBuffer)
{
	DC_MRFLD_BUFFER *psBuffer;
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext =
		(DC_MRFLD_DISPLAY_CONTEXT *)hDisplayContext;
	DC_MRFLD_DEVICE *psDevice;

	if (!psDisplayContext || !ui32NumPlanes || !paphImport ||
		!psSurfAttrib || !phBuffer)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	psDevice = psDisplayContext->psDevice;

	/*NOTE: we are only using the first plane(buffer)*/
	DRM_DEBUG("%s: import surf format 0x%x, w %d, h %d, bpp %d," \
		"stride %d\n",
		__func__,
		psSurfAttrib->ePixFormat,
		psSurfAttrib->ui32Width[0],
		psSurfAttrib->ui32Height[0],
		psSurfAttrib->ui32BPP,
		psSurfAttrib->ui32ByteStride[0]);

	psBuffer = kzalloc(sizeof(DC_MRFLD_BUFFER), GFP_KERNEL);
	if (!psBuffer) {
		DRM_ERROR("Failed to create DC buffer\n");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/*initialize this buffer*/
	psBuffer->eSource = DCMrfldEX_BUFFER_SOURCE_IMPORT;
	psBuffer->hDisplayContext = hDisplayContext;
	psBuffer->ui32Width = psSurfAttrib->ui32Width[0];
	psBuffer->ePixFormat = psSurfAttrib->ePixFormat;
	psBuffer->ui32ByteStride = psSurfAttrib->ui32ByteStride[0];
	psBuffer->ui32Width = psSurfAttrib->ui32Width[0];
	psBuffer->ui32Height = psSurfAttrib->ui32Height[0];
	psBuffer->bIsAllocated = IMG_FALSE;
	psBuffer->bIsContiguous = IMG_FALSE;
	psBuffer->ui32OwnerTaskID = task_tgid_nr(current);

	psBuffer->hImport = paphImport[0];
	/*setup output param*/
	*phBuffer = psBuffer;

	return PVRSRV_OK;
}

static PVRSRV_ERROR DC_MRFLD_BufferAcquire(IMG_HANDLE hBuffer,
					IMG_DEV_PHYADDR *pasDevPAddr,
					IMG_PVOID *ppvLinAddr)
{
	DC_MRFLD_BUFFER *psBuffer = (DC_MRFLD_BUFFER *)hBuffer;
	IMG_UINT32 ulPages;
	int i;

	if (!psBuffer || !pasDevPAddr || !ppvLinAddr)
		return PVRSRV_ERROR_INVALID_PARAMS;

	DRM_DEBUG("%s\n", __func__);

	ulPages = (psBuffer->ui32BufferSize + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

	/*allocate new buffer for import buffer*/
	if (psBuffer->eSource == DCMrfldEX_BUFFER_SOURCE_ALLOC) {
		for (i = 0; i < ulPages; i++)
			pasDevPAddr[i].uiAddr = psBuffer->psSysAddr[i].uiAddr;
		*ppvLinAddr = psBuffer->sCPUVAddr;
	}
	return PVRSRV_OK;
}

static IMG_VOID DC_MRFLD_BufferRelease(IMG_HANDLE hBuffer)
{

}

static IMG_VOID DC_MRFLD_BufferFree(IMG_HANDLE hBuffer)
{
	DC_MRFLD_DISPLAY_CONTEXT *psDisplayContext;
	DC_MRFLD_BUFFER *psBuffer = (DC_MRFLD_BUFFER *)hBuffer;
	DC_MRFLD_DEVICE *psDevice;
	struct drm_device *psDrmDev;

	if (!psBuffer)
		return;

	DRM_DEBUG("%s\n", __func__);

	psDisplayContext =
		(DC_MRFLD_DISPLAY_CONTEXT *)psBuffer->hDisplayContext;
	if (!psDisplayContext)
		return;

	psDevice = psDisplayContext->psDevice;
	psDrmDev = psDevice->psDrmDevice;

	if (psBuffer->eSource == DCMrfldEX_BUFFER_SOURCE_SYSTEM)
		return;

	/*
	 * if it's buffer allocated by display device contineu to free
	 * the buffer pages
	 */
	if (psBuffer->eSource == DCMrfldEX_BUFFER_SOURCE_ALLOC) {
		/*make sure unmap this buffer from gtt*/
		DCCBgttUnmapMemory(psDrmDev, (unsigned int)
			(uintptr_t)psBuffer, psBuffer->ui32OwnerTaskID);
		kfree(psBuffer->psSysAddr);
		vfree(psBuffer->sCPUVAddr);
	}

	if (psBuffer->eSource == DCMrfldEX_BUFFER_SOURCE_IMPORT &&
	    _Is_Task_KThread()) {
		DRM_DEBUG("owner task id: %d\n", psBuffer->ui32OwnerTaskID);
		/* KThread is triggered to clean up gtt */
		DCCBgttCleanupMemoryOnTask(psDrmDev,
					psBuffer->ui32OwnerTaskID);
	}

	kfree(psBuffer);
}

static PVRSRV_ERROR DC_MRFLD_BufferMap(IMG_HANDLE hBuffer)
{
	return PVRSRV_OK;
}

static IMG_VOID DC_MRFLD_BufferUnmap(IMG_HANDLE hBuffer)
{

}

static DC_DEVICE_FUNCTIONS sDCFunctions = {
	.pfnGetInfo			= DC_MRFLD_GetInfo,
	.pfnPanelQueryCount		= DC_MRFLD_PanelQueryCount,
	.pfnPanelQuery			= DC_MRFLD_PanelQuery,
	.pfnFormatQuery			= DC_MRFLD_FormatQuery,
	.pfnDimQuery			= DC_MRFLD_DimQuery,
	.pfnSetBlank			= IMG_NULL,
	.pfnSetVSyncReporting		= IMG_NULL,
	.pfnLastVSyncQuery		= IMG_NULL,
	.pfnContextCreate		= DC_MRFLD_ContextCreate,
	.pfnContextDestroy		= DC_MRFLD_ContextDestroy,
	.pfnContextConfigure		= DC_MRFLD_ContextConfigure,
	.pfnContextConfigureCheck	= DC_MRFLD_ContextConfigureCheck,
	.pfnBufferAlloc			= DC_MRFLD_BufferAlloc,
	.pfnBufferAcquire		= DC_MRFLD_BufferAcquire,
	.pfnBufferRelease		= DC_MRFLD_BufferRelease,
	.pfnBufferFree			= DC_MRFLD_BufferFree,
	.pfnBufferImport		= DC_MRFLD_BufferImport,
	.pfnBufferMap			= DC_MRFLD_BufferMap,
	.pfnBufferUnmap			= DC_MRFLD_BufferUnmap,
	.pfnBufferSystemAcquire         = DC_MRFLD_BufferSystemAcquire,
	.pfnBufferSystemRelease         = DC_MRFLD_BufferSystemRelease,

};

static PVRSRV_ERROR _SystemBuffer_Init(DC_MRFLD_DEVICE *psDevice)
{
	struct drm_device *psDrmDev;
	struct psb_framebuffer *psPSBFb;
	IMG_UINT32 ulPagesNumber;
	int i;

	/*get fbDev*/
	psDrmDev = psDevice->psDrmDevice;
	DCCBGetFramebuffer(psDrmDev, &psPSBFb);
	if (!psPSBFb)
		return PVRSRV_ERROR_INVALID_PARAMS;

	/*allocate system buffer*/
	psDevice->psSystemBuffer =
		kzalloc(sizeof(DC_MRFLD_BUFFER), GFP_KERNEL);
	if (!psDevice->psSystemBuffer) {
		DRM_ERROR("Failed to allocate system buffer\n");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/*initilize system buffer*/
	psDevice->psSystemBuffer->bIsAllocated = IMG_FALSE;
	psDevice->psSystemBuffer->bIsContiguous = IMG_FALSE;
	psDevice->psSystemBuffer->eSource = DCMrfldEX_BUFFER_SOURCE_SYSTEM;
	psDevice->psSystemBuffer->hDisplayContext = 0;
	psDevice->psSystemBuffer->hImport = 0;
	psDevice->psSystemBuffer->sCPUVAddr = psPSBFb->vram_addr;
	psDevice->psSystemBuffer->sDevVAddr.uiAddr = 0;
	psDevice->psSystemBuffer->ui32BufferSize = psPSBFb->size;
	psDevice->psSystemBuffer->ui32ByteStride = psPSBFb->base.pitches[0];
	psDevice->psSystemBuffer->ui32Height = psPSBFb->base.height;
	psDevice->psSystemBuffer->ui32Width = psPSBFb->base.width;
	psDevice->psSystemBuffer->ui32OwnerTaskID = -1;
	psDevice->psSystemBuffer->ui32RefCount = 0;

	switch (psPSBFb->depth) {
	case 32:
	case 24:
		psDevice->psSystemBuffer->ePixFormat =
			IMG_PIXFMT_B8G8R8A8_UNORM;
		break;
	case 16:
		psDevice->psSystemBuffer->ePixFormat =
			IMG_PIXFMT_B5G6R5_UNORM;
		break;
	default:
		DRM_ERROR("Unsupported system buffer format\n");
	}

	ulPagesNumber = (psPSBFb->size + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
	psDevice->psSystemBuffer->psSysAddr =
		kzalloc(ulPagesNumber * sizeof(IMG_SYS_PHYADDR), GFP_KERNEL);
	if (!psDevice->psSystemBuffer->psSysAddr) {
		kfree(psDevice->psSystemBuffer);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	for (i = 0; i < ulPagesNumber; i++) {
		psDevice->psSystemBuffer->psSysAddr[i].uiAddr =
			psPSBFb->stolen_base + i * PAGE_SIZE;
	}

	DRM_DEBUG("%s: allocated system buffer %dx%d, format %d\n",
			__func__,
			psDevice->psSystemBuffer->ui32Width,
			psDevice->psSystemBuffer->ui32Height,
			psDevice->psSystemBuffer->ePixFormat);

	return PVRSRV_OK;
}

static IMG_VOID _SystemBuffer_Deinit(DC_MRFLD_DEVICE *psDevice)
{
	if (psDevice->psSystemBuffer) {
		kfree(psDevice->psSystemBuffer->psSysAddr);
		kfree(psDevice->psSystemBuffer);
	}
}

static PVRSRV_ERROR DC_MRFLD_init(struct drm_device *psDrmDev)
{
	PVRSRV_ERROR eRes = PVRSRV_OK;
	DC_MRFLD_DEVICE *psDevice;
	int i, j;

	if (!psDrmDev)
		return PVRSRV_ERROR_INVALID_PARAMS;

	/*create new display device*/
	psDevice = kzalloc(sizeof(DC_MRFLD_DEVICE), GFP_KERNEL);
	if (!psDevice) {
		DRM_ERROR("Failed to create display device\n");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/*init display device*/
	psDevice->psDrmDevice = psDrmDev;
	/*init system frame buffer*/
	eRes = _SystemBuffer_Init(psDevice);
	if (eRes != PVRSRV_OK)
		goto init_error;

	/*init primary surface info*/
	psDevice->sPrimInfo.sDims.ui32Width =
		psDevice->psSystemBuffer->ui32Width;
	psDevice->sPrimInfo.sDims.ui32Height =
		psDevice->psSystemBuffer->ui32Height;
	psDevice->sPrimInfo.sFormat.ePixFormat =
		psDevice->psSystemBuffer->ePixFormat;

	/*init display info*/
	strncpy(psDevice->sDisplayInfo.szDisplayName, DRVNAME, DC_NAME_SIZE);
	psDevice->sDisplayInfo.ui32MinDisplayPeriod = 0;
	psDevice->sDisplayInfo.ui32MaxDisplayPeriod = 5;
	psDevice->sDisplayInfo.ui32MaxPipes = DCCBGetPipeCount();

	/*init flip queue lock*/
	mutex_init(&psDevice->sFlipQueueLock);

	/*init flip queues*/
	for (i = 0; i < MAX_PIPE_NUM; i++) {
		INIT_LIST_HEAD(&psDevice->sFlipQueues[i]);
		psDevice->bFlipEnabled[i] = IMG_TRUE;
	}

	/*init plane pipe mapping lock */
	mutex_init(&psDevice->sMappingLock);

	/*init plane pipe mapping & plane state */
	for (i = 1; i < DC_PLANE_MAX; i++) {
		for (j = 0; j < MAX_PLANE_INDEX; j++) {
			struct plane_state *pstate =
				&psDevice->plane_states[i][j];

			pstate->type = i;
			pstate->index = j;
			pstate->attached_pipe = -1;
			pstate->active = false;
			pstate->disabled = true;
			pstate->extra_power_island =
				DC_ExtraPowerIslands[i][j];
			pstate->powered_off = true;

			psDevice->ui32PlanePipeMapping[i][j] = -1;
		}
	}

	/*unblank fbdev*/
	DCCBUnblankDisplay(psDevice->psDrmDevice);

	/*register display device*/
	eRes = DCRegisterDevice(&sDCFunctions,
				2,
				psDevice,
				&psDevice->hSrvHandle);
	if (eRes != PVRSRV_OK) {
		DRM_ERROR("Failed to register display device\n");
		goto reg_error;
	}

	/*init ISR*/
	DCCBInstallVSyncISR(psDrmDev, _Vsync_ISR);

	/*init flip timer*/
	psDevice->sFlipTimer.data = (unsigned long)psDevice;
	psDevice->sFlipTimer.function = _Flip_Timer_Fn;
	init_timer(&psDevice->sFlipTimer);
	INIT_WORK(&psDevice->flip_retire_work, timer_flip_handler);

	gpsDevice = psDevice;

	return PVRSRV_OK;
reg_error:
	_SystemBuffer_Deinit(psDevice);
init_error:
	kfree(psDevice);
	return eRes;
}

static PVRSRV_ERROR DC_MRFLD_exit(void)
{
	if (!gpsDevice)
		return PVRSRV_ERROR_INVALID_PARAMS;

	/*unregister display device*/
	DCUnregisterDevice(gpsDevice->hSrvHandle);

	/*destroy system frame buffer*/
	_SystemBuffer_Deinit(gpsDevice);

	/*free device*/
	kfree(gpsDevice);
	gpsDevice = 0;

	return PVRSRV_OK;
}

void DCLockMutex()
{
	if (!gpsDevice)
		return;

	mutex_lock(&gpsDevice->sFlipQueueLock);
}

void DCUnLockMutex()
{
	if (!gpsDevice)
		return;

	mutex_unlock(&gpsDevice->sFlipQueueLock);
}

int DCUpdateCursorPos(uint32_t pipe, uint32_t pos)
{
	int ret = 0;

	if (!gpsDevice || !gpsDevice->psDrmDevice)
		return -1;

	/* TODO: check flip queue state */
	/* if queue is not empty pending flip should be updated directly */
	DCLockMutex();
	ret = DCCBUpdateCursorPos(gpsDevice->psDrmDevice, (int)pipe, pos);
	DCUnLockMutex();

	return ret;
}

void DCAttachPipe(uint32_t iPipe)
{
	if (!gpsDevice)
		return;

	DRM_DEBUG("%s: pipe %d\n", __func__, iPipe);

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B)
		return;

	gpsDevice->bFlipEnabled[iPipe] = IMG_TRUE;
}

void DCUnAttachPipe(uint32_t iPipe)
{
	struct list_head *psFlipQueue;
	DC_MRFLD_FLIP *psFlip, *psTmp;

	if (!gpsDevice)
		return;

	DRM_DEBUG("%s: pipe %d\n", __func__, iPipe);

	if (iPipe != DC_PIPE_A && iPipe != DC_PIPE_B)
		return;

	psFlipQueue = &gpsDevice->sFlipQueues[iPipe];
	/* complete the flips*/
	list_for_each_entry_safe(psFlip, psTmp, psFlipQueue, sFlips[iPipe]) {

		if (psFlip->eFlipStates[iPipe] == DC_MRFLD_FLIP_DC_UPDATED) {
			/* Put pipe's vsync which has been enabled. */
			DCCBDisableVSyncInterrupt(gpsDevice->psDrmDevice, iPipe);

			if (iPipe != DC_PIPE_B)
				DCCBDsrAllow(gpsDevice->psDrmDevice, iPipe);
		}

		/*remove this entry from flip queue, decrease refCount*/
		list_del(&psFlip->sFlips[iPipe]);

		if (!(--psFlip->uiRefCount)) {
			/*retire all buffers possessed by this flip*/
			DCDisplayConfigurationRetired(
					psFlip->hConfigData);
			/* free it */
			free_flip(psFlip);
			psFlip = NULL;
		}
	}

	if (list_empty_careful(psFlipQueue))
		INIT_LIST_HEAD(&gpsDevice->sFlipQueues[iPipe]);

	gpsDevice->bFlipEnabled[iPipe] = IMG_FALSE;
}

/*TODO: merge with DCUnAttachPipe*/
void DC_MRFLD_onPowerOff(uint32_t iPipe)
{

	int i, j;
	struct plane_state *pstate;
	struct drm_psb_private *dev_priv;

	if (!gpsDevice)
		return;

	dev_priv = gpsDevice->psDrmDevice->dev_private;
	if (!dev_priv->um_start)
		return;

	for (i = 1; i < DC_PLANE_MAX; i++) {
		for (j = 0; j < MAX_PLANE_INDEX; j++) {
			pstate = &gpsDevice->plane_states[i][j];

			/* if already inactive or not on this pipe */
			if (!pstate->active || pstate->attached_pipe != iPipe)
				continue;

			disable_plane(pstate);

			/* turn off extra power island here */
			if (!pstate->powered_off &&
			    pstate->extra_power_island) {
				power_island_put(pstate->extra_power_island);
				pstate->powered_off = true;
			}

			/* set plane state to be correct in power off */
			pstate->disabled = true;
		}
	}
}

/*TODO: merge with DCAttachPipe*/
void DC_MRFLD_onPowerOn(uint32_t iPipe)
{
	/* we do nothing on ExtraPowerIsland during power on.
	 * It will be automatically turned on during flip.
	 */
	int j;
	struct plane_state *pstate;
	struct drm_psb_private *dev_priv;

	if (!gpsDevice)
		return;

	dev_priv = gpsDevice->psDrmDevice->dev_private;
	if (!dev_priv->um_start)
		return;

	/* keep primary on and flip to black screen */
	for (j = 0; j < MAX_PLANE_INDEX; j++) {
		pstate = &gpsDevice->plane_states[DC_PRIMARY_PLANE][j];

		/* primary plane is fixed to pipe */
		if (j != iPipe)
			continue;

		disable_plane(pstate);
	}

	for (j = 0; j < MAX_PLANE_INDEX; j++) {
		pstate = &gpsDevice->plane_states[DC_CURSOR_PLANE][j];

		/* primary plane is fixed to pipe */
		if (j != iPipe)
			continue;

		disable_plane(pstate);
	}
}

int DC_MRFLD_Enable_Plane(int type, int index, u32 ctx)
{
	int err = 0;
	IMG_INT32 *ui32ActivePlanes;
#if 0
	IMG_UINT32 uiExtraPowerIslands = 0;
#endif

	if (type <= DC_UNKNOWN_PLANE || type >= DC_PLANE_MAX) {
		DRM_ERROR("Invalid plane type %d\n", type);
		return -EINVAL;
	}

	if (index < 0 || index >= MAX_PLANE_INDEX) {
		DRM_ERROR("Invalid plane index %d\n", index);
		return -EINVAL;
	}

	/*acquire lock*/
	mutex_lock(&gpsDevice->sFlipQueueLock);

	ui32ActivePlanes = &gpsDevice->ui32ActivePlanes[type];

	/* add to active planes*/
	if (!(*ui32ActivePlanes & (1 << index))) {
		*ui32ActivePlanes |= (1 << index);

#if 0
		/* power on extra power islands if required */
		uiExtraPowerIslands = DC_ExtraPowerIslands[type][index];
		_Enable_ExtraPowerIslands(gpsDevice,
					uiExtraPowerIslands);
#endif
	}

	mutex_unlock(&gpsDevice->sFlipQueueLock);

	return err;
}

bool DC_MRFLD_Is_Plane_Disabled(int type, int index, u32 ctx)
{
	bool bDisabled;
	struct plane_state *pstate;

	/*acquire lock*/
	mutex_lock(&gpsDevice->sFlipQueueLock);

	pstate = &gpsDevice->plane_states[type][index];
	bDisabled = pstate->disabled;

	mutex_unlock(&gpsDevice->sFlipQueueLock);

	return bDisabled;
}

int DC_MRFLD_Disable_Plane(int type, int index, u32 ctx)
{
	int err = 0;
	IMG_INT32 *ui32ActivePlanes;
	IMG_UINT32 uiExtraPowerIslands = 0;

	if (type <= DC_UNKNOWN_PLANE || type >= DC_PLANE_MAX) {
		DRM_ERROR("Invalid plane type %d\n", type);
		return -EINVAL;
	}

	if (index < 0 || index >= MAX_PLANE_INDEX) {
		DRM_ERROR("Invalid plane index %d\n", index);
		return -EINVAL;
	}

	/*acquire lock*/
	mutex_lock(&gpsDevice->sFlipQueueLock);

	/*disable sprite & overlay plane*/
	switch (type) {
	case DC_SPRITE_PLANE:
		err = DCCBSpriteEnable(gpsDevice->psDrmDevice, ctx, index, 0);
		break;
	case DC_OVERLAY_PLANE:
		err = DCCBOverlayEnable(gpsDevice->psDrmDevice, ctx, index, 0);
		break;
	}

	ui32ActivePlanes = &gpsDevice->ui32ActivePlanes[type];

	/* remove from active planes*/
	if (!err && (*ui32ActivePlanes & (1 << index))) {
		*ui32ActivePlanes &= ~(1 << index);

		/* power off extra power islands if required */
		uiExtraPowerIslands = DC_ExtraPowerIslands[type][index];
#if 0
		if (uiExtraPowerIslands) {
			req = kzalloc(sizeof(*req), GFP_KERNEL);
			if (!req) {
				DRM_ERROR("fail to alloc power_off_req\n");
				goto out_mapping;
			}
			INIT_DELAYED_WORK(&req->work, display_power_work);

			req->power_off_islands = uiExtraPowerIslands;

			queue_delayed_work(dev_priv->power_wq,
					   &req->work, msecs_to_jiffies(32));
		}
	out_mapping:
#endif
		/* update plane pipe mapping */
		_Update_PlanePipeMapping(gpsDevice, type, index, -1);
	}

	mutex_unlock(&gpsDevice->sFlipQueueLock);

	if (type == DC_OVERLAY_PLANE && !err) {
		/* avoid big lock as it is a blocking call */
		// err = DCCBOverlayDisableAndWait(gpsDevice->psDrmDevice, ctx, index);
	}
	return err;
}

/*----------------------------------------------------------------------------*/
PVRSRV_ERROR MerrifieldDCInit(struct drm_device *psDrmDev)
{
	return DC_MRFLD_init(psDrmDev);
}

PVRSRV_ERROR MerrifieldDCDeinit(void)
{
	return DC_MRFLD_exit();
}
