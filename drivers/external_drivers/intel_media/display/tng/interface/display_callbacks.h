/*****************************************************************************
 *
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************/

#ifndef __DC_CALLBACKS_H__
#define __DC_CALLBACKS_H__

#include <drm/drmP.h>
#include <displayclass_interface.h>
#include "img_types.h"
#include "psb_drm.h"

struct psb_framebuffer;

typedef int (*pfn_vsync_handler) (struct drm_device *dev, int pipe);

void DCCBGetFramebuffer(struct drm_device *dev, struct psb_framebuffer **);
int DCChangeFrameBuffer(struct drm_device *dev,
			struct psb_framebuffer *psbfb);
int DCCBEnableVSyncInterrupt(struct drm_device *dev, int pipe);
void DCCBDisableVSyncInterrupt(struct drm_device *dev, int pipe);
void DCCBInstallVSyncISR(struct drm_device *dev,
			 pfn_vsync_handler pVsyncHandler);
void DCCBUninstallVSyncISR(struct drm_device *dev);
void DCCBFlipToSurface(struct drm_device *dev,
				unsigned long uiAddr,
				unsigned long uiFormat,
				unsigned long uiStride,
		       unsigned int pipeflag);
void DCCBFlipOverlay(struct drm_device *dev,
			struct intel_dc_overlay_ctx *ctx);
void DCCBFlipSprite(struct drm_device *dev,
			struct intel_dc_sprite_ctx *ctx);
void DCCBFlipPrimary(struct drm_device *dev,
			struct intel_dc_primary_ctx *ctx);
void DCCBFlipCursor(struct drm_device *dev,
			struct intel_dc_cursor_ctx *ctx);
void DCCBSetupZorder(struct drm_device *dev,
			struct intel_dc_plane_zorder *zorder,
			int pipe);
void DCCBWaitForDbiFifoEmpty(struct drm_device *dev, int pipe);
void DCCBAvoidFlipInVblankInterval(struct drm_device *dev, int pipe);
int DCCBUpdateDbiPanel(struct drm_device *dev, int pipe);
int DCCBOverlayDisableAndWait(struct drm_device *dev, u32 ctx,
			int index);
int DCCBOverlayEnable(struct drm_device *dev, u32 ctx,
			int index, int enabled);
int DCCBSpriteEnable(struct drm_device *dev, u32 ctx,
			int index, int enabled);
int DCCBPrimaryEnable(struct drm_device *dev, u32 ctx,
			int index, int enabled);
int DCCBCursorDisable(struct drm_device *dev, int index);
void DCCBFlipDSRCb(struct drm_device *dev);
void DCCBUnblankDisplay(struct drm_device *dev);
int DCCBgttMapMemory(struct drm_device *dev,
		     unsigned int hHandle,
		     unsigned int ui32TaskId,
		     IMG_SYS_PHYADDR *pPages,
		     unsigned int ui32PagesNum, unsigned int *ui32Offset);
int DCCBgttUnmapMemory(struct drm_device *dev,
		       unsigned int hHandle, unsigned int ui32TaskId);
int DCCBgttCleanupMemoryOnTask(struct drm_device *dev,
				unsigned int ui32TaskId);
bool DCChangeSwapChainProperty(unsigned long *psSwapChainGTTOffset,
			int pipe);
u32 DCCBGetPipeCount(void);
void DCCBSetPipeToOvadd(u32 *ovadd, int pipe);
bool DCCBIsSuspended(struct drm_device *dev);
int DCCBIsPipeActive(struct drm_device *dev, int pipe);

void DCCBDsrForbid(struct drm_device *dev, int pipe);
void DCCBDsrAllow(struct drm_device *dev, int pipe);
int DCCBUpdateCursorPos(struct drm_device *dev, int pipe, uint32_t pos);
#endif				/* __DC_CALLBACKS_H__ */
