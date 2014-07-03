/*
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * Authors: Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"

#ifdef MERRIFIELD
#include "tng_topaz.h"
#else
#include "pnw_topaz.h"
#endif

#endif
#include "psb_msvdx.h"

#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

int psb_fence_emit_sequence(struct ttm_fence_device *fdev,
			    uint32_t fence_class,
			    uint32_t flags, uint32_t *sequence,
			    unsigned long *timeout_jiffies)
{
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	struct msvdx_private *msvdx_priv = NULL;
	uint32_t seq = 0;

	if (!dev_priv)
		return -EINVAL;

	if (fence_class >= PSB_NUM_ENGINES)
		return -EINVAL;

	msvdx_priv = dev_priv->msvdx_private;

	switch (fence_class) {
	case PSB_ENGINE_DECODE:
		spin_lock(&dev_priv->sequence_lock);
		seq = dev_priv->sequence[fence_class]++;
		/* cmds in one batch use different fence value */
		seq <<= 4;
		seq += msvdx_priv->num_cmd;
		spin_unlock(&dev_priv->sequence_lock);
		break;
#ifndef CONFIG_DRM_VXD_BYT
	case LNC_ENGINE_ENCODE:
		spin_lock(&dev_priv->sequence_lock);
		seq = dev_priv->sequence[fence_class]++;
		spin_unlock(&dev_priv->sequence_lock);
		break;
#ifdef SUPPORT_VSP
	case VSP_ENGINE_VPP:
		spin_lock(&dev_priv->sequence_lock);
		seq = dev_priv->sequence[fence_class]++;
		spin_unlock(&dev_priv->sequence_lock);
		break;
#endif
#endif
	default:
		DRM_ERROR("Unexpected fence class\n");
		return -EINVAL;
	}

	*sequence = seq;
	if (fence_class == PSB_ENGINE_DECODE)
		*timeout_jiffies = jiffies + DRM_HZ;
	else
		*timeout_jiffies = jiffies + DRM_HZ * 3;

	return 0;
}

static void psb_fence_poll(struct ttm_fence_device *fdev,
			   uint32_t fence_class, uint32_t waiting_types)
{
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	struct drm_device *dev = dev_priv->dev;
	uint32_t sequence = 0;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
#ifdef SUPPORT_VSP
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
#endif
	if (unlikely(!dev_priv))
		return;

	if (waiting_types == 0)
		return;

	switch (fence_class) {
	case PSB_ENGINE_DECODE:
		sequence = msvdx_priv->msvdx_current_sequence;
		break;
#ifndef CONFIG_DRM_VXD_BYT
	case LNC_ENGINE_ENCODE:
#ifdef MERRIFIELD
		if (IS_MRFLD(dev))
			sequence = *((uint32_t *)
				((struct tng_topaz_private *)dev_priv->
				topaz_private)->topaz_sync_addr);
#else
		if (IS_MDFLD(dev))
			sequence = *((uint32_t *)
				((struct pnw_topaz_private *)dev_priv->
				topaz_private)->topaz_sync_addr + 1);
#endif
		break;
	case VSP_ENGINE_VPP:
#ifdef SUPPORT_VSP
		sequence = vsp_priv->current_sequence;
		break;
#endif
#endif
	default:
		break;
	}

	PSB_DEBUG_GENERAL("Polling fence sequence, got 0x%08x\n", sequence);
	ttm_fence_handler(fdev, fence_class, sequence,
			  _PSB_FENCE_TYPE_EXE, 0);
}

static void psb_fence_lockup(struct ttm_fence_object *fence,
			     uint32_t fence_types)
{
	struct ttm_fence_device *fdev = fence->fdev;
	struct ttm_fence_class_manager *fc = ttm_fence_fc(fence);
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	struct drm_device *dev = (struct drm_device *)dev_priv->dev;

	if (fence->fence_class == LNC_ENGINE_ENCODE) {
#ifndef CONFIG_DRM_VXD_BYT
		DRM_ERROR("TOPAZ timeout (probable lockup) detected,  flush queued cmdbuf");

		write_lock(&fc->lock);
#ifdef MERRIFIELD
		if (IS_MRFLD(dev))
			tng_topaz_handle_timeout(fence->fdev);
#else
		if (IS_MDFLD(dev))
			pnw_topaz_handle_timeout(fence->fdev);
#endif
		ttm_fence_handler(fence->fdev, fence->fence_class,
				  fence->sequence, fence_types, -EBUSY);
		write_unlock(&fc->lock);
#endif
	} else if (fence->fence_class == PSB_ENGINE_DECODE) {
		struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

		PSB_DEBUG_WARN("MSVDX timeout (probable lockup) detected, flush queued cmdbuf");
#if (!defined(MERRIFIELD) && !defined(CONFIG_DRM_VXD_BYT))
		if (psb_get_power_state(OSPM_VIDEO_DEC_ISLAND) == 0)
			PSB_DEBUG_WARN("WARN: msvdx is power off in accident.\n");
#endif
		PSB_DEBUG_WARN("WARN: MSVDX_COMMS_FW_STATUS reg is 0x%x.\n",
				PSB_RMSVDX32(MSVDX_COMMS_FW_STATUS));
		psb_msvdx_flush_cmd_queue(dev);

		write_lock(&fc->lock);
		ttm_fence_handler(fence->fdev, fence->fence_class,
				  fence->sequence, fence_types, -EBUSY);
		write_unlock(&fc->lock);

		if (msvdx_priv->fw_loaded_by_punit)
			msvdx_priv->msvdx_needs_reset |= MSVDX_RESET_NEEDS_REUPLOAD_FW |
				MSVDX_RESET_NEEDS_INIT_FW;
		else
			msvdx_priv->msvdx_needs_reset = 1;
	} else if (fence->fence_class == VSP_ENGINE_VPP) {
#ifdef SUPPORT_VSP
		struct vsp_private *vsp_priv = dev_priv->vsp_private;

		if (vsp_fence_poll(dev) &&
		    fence->sequence <= vsp_priv->current_sequence) {
			DRM_ERROR("pass poll when timeout vsp sequence %x, current sequence %x\n", fence->sequence, vsp_priv->current_sequence);
			return;
		}

		DRM_ERROR("fence sequence is %x\n", fence->sequence);
		DRM_ERROR("VSP timeout (probable lockup) detected,"
			  " reset vsp\n");

		write_lock(&fc->lock);
		ttm_fence_handler(fence->fdev, fence->fence_class,
				  fence->sequence, fence_types,
				  -EBUSY);
		write_unlock(&fc->lock);

		psb_vsp_dump_info(dev_priv);
		vsp_priv->vsp_state = VSP_STATE_HANG;
#endif
	} else {
		DRM_ERROR("Unsupported fence class\n");
	}
}

static struct ttm_fence_driver psb_ttm_fence_driver = {
	.has_irq = NULL,
	.emit = psb_fence_emit_sequence,
	.flush = NULL,
	.poll = psb_fence_poll,
	.needed_flush = NULL,
	.wait = NULL,
	.signaled = NULL,
	.lockup = psb_fence_lockup,
};

int psb_ttm_fence_device_init(struct ttm_fence_device *fdev)
{
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	struct ttm_fence_class_init fci = {
		.wrap_diff = (1 << 30),
		.flush_diff = (1 << 29),
		.sequence_mask = 0xFFFFFFFF
	};

	return ttm_fence_device_init(PSB_NUM_ENGINES,
				     dev_priv->mem_global_ref.object,
				     fdev, &fci, 1,
				     &psb_ttm_fence_driver);
}

void psb_fence_handler(struct drm_device *dev, uint32_t fence_class)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_fence_device *fdev = &dev_priv->fdev;
	struct ttm_fence_class_manager *fc =
				&fdev->fence_class[fence_class];
	unsigned long irq_flags;

	write_lock_irqsave(&fc->lock, irq_flags);
	psb_fence_poll(fdev, fence_class, fc->waiting_types);
	write_unlock_irqrestore(&fc->lock, irq_flags);
}

void psb_fence_error(struct drm_device *dev,
		     uint32_t fence_class,
		     uint32_t sequence, uint32_t type, int error)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_fence_device *fdev = &dev_priv->fdev;
	unsigned long irq_flags;
	struct ttm_fence_class_manager *fc;

	if (fence_class >= PSB_NUM_ENGINES) {
		DRM_ERROR("fence_class %d is unsupported.\n", fence_class);
		return;
	}

	fc = &fdev->fence_class[fence_class];

	write_lock_irqsave(&fc->lock, irq_flags);
	ttm_fence_handler(fdev, fence_class, sequence, type, error);
	write_unlock_irqrestore(&fc->lock, irq_flags);
}
