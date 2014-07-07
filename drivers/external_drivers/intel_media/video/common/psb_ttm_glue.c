/**************************************************************************
 * Copyright (c) 2008, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics Inc.  Cedar Park, TX., USA.
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
 **************************************************************************/


#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#ifndef MERRIFIELD
#include "pnw_topaz.h"
#else
#include "tng_topaz.h"
#endif
/*IMG Headers*/
#include "private_data.h"
#endif
#include "psb_video_drv.h"
#include "psb_ttm_userobj_api.h"
#include <linux/io.h>
#include <asm/intel-mid.h>
#include "psb_msvdx.h"

#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

static int ied_enabled;

static void ann_rm_workaround_ctx(struct drm_psb_private *dev_priv, uint64_t ctx_type);
static void ann_add_workaround_ctx(struct drm_psb_private *dev_priv, uint64_t ctx_type);

#ifdef MERRIFIELD
struct psb_fpriv *psb_fpriv(struct drm_file *file_priv)
{
	return (struct psb_fpriv *) BCVideoGetPriv(file_priv);
}
#endif

int psb_fence_signaled_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	return ttm_fence_signaled_ioctl(psb_fpriv(file_priv)->tfile, data);
}

int psb_fence_finish_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	return ttm_fence_finish_ioctl(psb_fpriv(file_priv)->tfile, data);
}

int psb_fence_unref_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return ttm_fence_unref_ioctl(psb_fpriv(file_priv)->tfile, data);
}

int psb_pl_waitidle_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	return ttm_pl_waitidle_ioctl(psb_fpriv(file_priv)->tfile, data);
}

int psb_pl_setstatus_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	return ttm_pl_setstatus_ioctl(psb_fpriv(file_priv)->tfile,
				      &psb_priv(dev)->ttm_lock, data);
}

int psb_pl_synccpu_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	return ttm_pl_synccpu_ioctl(psb_fpriv(file_priv)->tfile, data);
}

int psb_pl_unref_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	return ttm_pl_unref_ioctl(psb_fpriv(file_priv)->tfile, data);

}

int psb_pl_reference_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	return  ttm_pl_reference_ioctl(psb_fpriv(file_priv)->tfile, data);

}

int psb_pl_create_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	return ttm_pl_create_ioctl(psb_fpriv(file_priv)->tfile,
				   &dev_priv->bdev, &dev_priv->ttm_lock, data);
}

int psb_pl_ub_create_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);

	return ttm_pl_ub_create_ioctl(psb_fpriv(file_priv)->tfile,
				      &dev_priv->bdev, &dev_priv->ttm_lock, data);
}

/**
 * psb_ttm_fault - Wrapper around the ttm fault method.
 *
 * @vma: The struct vm_area_struct as in the vm fault() method.
 * @vmf: The struct vm_fault as in the vm fault() method.
 *
 * Since ttm_fault() will reserve buffers while faulting,
 * we need to take the ttm read lock around it, as this driver
 * relies on the ttm_lock in write mode to exclude all threads from
 * reserving and thus validating buffers in aperture- and memory shortage
 * situations.
 */
int psb_ttm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct ttm_buffer_object *bo = (struct ttm_buffer_object *)
				       vma->vm_private_data;
	struct drm_psb_private *dev_priv =
		container_of(bo->bdev, struct drm_psb_private, bdev);
	int ret;

	ret = ttm_read_lock(&dev_priv->ttm_lock, true);
	if (unlikely(ret != 0))
		return ret;

	ret = dev_priv->ttm_vm_ops->fault(vma, vmf);

	ttm_read_unlock(&dev_priv->ttm_lock);
	return ret;
}

/*
ssize_t psb_ttm_write(struct file *filp, const char __user *buf,
		      size_t count, loff_t *f_pos)
{
	struct drm_file *file_priv = (struct drm_file *)filp->private_data;
	struct drm_psb_private *dev_priv = psb_priv(file_priv->minor->dev);

	return ttm_bo_io(&dev_priv->bdev, filp, buf, NULL, count, f_pos, 1);
}

ssize_t psb_ttm_read(struct file *filp, char __user *buf,
		     size_t count, loff_t *f_pos)
{
	struct drm_file *file_priv = (struct drm_file *)filp->private_data;
	struct drm_psb_private *dev_priv = psb_priv(file_priv->minor->dev);

	return ttm_bo_io(&dev_priv->bdev, filp, NULL, buf, count, f_pos, 1);
}
*/

static int psb_ttm_mem_global_init(struct drm_global_reference *ref)
{
	return ttm_mem_global_init(ref->object);
}

static void psb_ttm_mem_global_release(struct drm_global_reference *ref)
{
	ttm_mem_global_release(ref->object);
}

int psb_ttm_global_init(struct drm_psb_private *dev_priv)
{
	struct drm_global_reference *global_ref;
	struct drm_global_reference *global;
	int ret;

	global_ref = &dev_priv->mem_global_ref;
	global_ref->global_type = DRM_GLOBAL_TTM_MEM;
	global_ref->size = sizeof(struct ttm_mem_global);
	global_ref->init = &psb_ttm_mem_global_init;
	global_ref->release = &psb_ttm_mem_global_release;

	ret = drm_global_item_ref(global_ref);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed referencing a global TTM memory object.\n");
		return ret;
	}

	dev_priv->bo_global_ref.mem_glob = dev_priv->mem_global_ref.object;
	global = &dev_priv->bo_global_ref.ref;
	global->global_type = DRM_GLOBAL_TTM_BO;
	global->size = sizeof(struct ttm_bo_global);
	global->init = &ttm_bo_global_init;
	global->release = &ttm_bo_global_release;
	ret = drm_global_item_ref((struct drm_global_reference *)global);
	if (ret != 0) {
		DRM_ERROR("Failed setting up TTM BO subsystem.\n");
		drm_global_item_unref((struct drm_global_reference *)global_ref);
		return ret;
	}

	return 0;
}

void psb_ttm_global_release(struct drm_psb_private *dev_priv)
{
	drm_global_item_unref(&dev_priv->mem_global_ref);
}

int psb_getpageaddrs_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_getpageaddrs_arg *arg = data;
	struct ttm_buffer_object *bo;
	struct ttm_tt *ttm;
	struct page **tt_pages;
	unsigned long i, num_pages;
	unsigned long *p;

	bo = ttm_buffer_object_lookup(psb_fpriv(file_priv)->tfile,
				      arg->handle);
	if (unlikely(bo == NULL)) {
		printk(KERN_ERR
		       "Could not find buffer object for getpageaddrs.\n");
		return -EINVAL;
	}
	arg->gtt_offset = bo->offset;
	ttm = bo->ttm;
	num_pages = ttm->num_pages;
	p = kzalloc(num_pages * sizeof(unsigned long), GFP_KERNEL);
	if (unlikely(p == NULL))
		return -ENOMEM;

	tt_pages = ttm->pages;

	for (i = 0; i < num_pages; i++)
		p[i] = (unsigned long)page_to_phys(tt_pages[i]);

	if (copy_to_user((void __user *)((unsigned long)arg->page_addrs),
			 p, sizeof(unsigned long) * num_pages)) {
		return -EFAULT;
	}

	ttm_bo_unref(&bo);
	kfree(p);
	return 0;
}

void psb_remove_videoctx(struct drm_psb_private *dev_priv, struct file *filp)
{
	struct psb_video_ctx *pos, *n;
	struct psb_video_ctx *found_ctx = NULL;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int ctx_type;
	unsigned long irq_flags;

	/* iterate to query all ctx to if there is DRM running*/
	ied_enabled = 0;

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (pos->filp == filp) {
			found_ctx = pos;
			list_del(&pos->head);
		} else {
			if (pos->ctx_type & VA_RT_FORMAT_PROTECTED)
				ied_enabled = 1;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	if (found_ctx) {
		PSB_DEBUG_PM("Video:remove context profile %llu,"
				  " entrypoint %llu\n",
				  (found_ctx->ctx_type >> 8) & 0xff,
				  (found_ctx->ctx_type & 0xff));
		if (IS_ANN(dev))
			ann_rm_workaround_ctx(dev_priv, found_ctx->ctx_type);
#ifndef CONFIG_DRM_VXD_BYT
		/* if current ctx points to it, set to NULL */
		if ((VAEntrypointEncSlice ==
				(found_ctx->ctx_type & 0xff)
			|| VAEntrypointEncPicture ==
				(found_ctx->ctx_type & 0xff))
			&& VAProfileVP8Version0_3 !=
				((found_ctx->ctx_type >> 8) & 0xff)) {
#ifdef MERRIFIELD
			tng_topaz_remove_ctx(dev_priv,
				found_ctx);
#else
			if (dev_priv->topaz_ctx == found_ctx) {
				pnw_reset_fw_status(dev_priv->dev,
					PNW_TOPAZ_END_CTX);
				dev_priv->topaz_ctx = NULL;
			} else {
				PSB_DEBUG_PM("Remove a inactive "\
						"encoding context.\n");
			}
#endif
			if (dev_priv->last_topaz_ctx == found_ctx)
				dev_priv->last_topaz_ctx = NULL;
#ifdef SUPPORT_VSP
		} else if (
			(VAEntrypointVideoProc ==
					(found_ctx->ctx_type & 0xff)
				&& 0xff ==
					((found_ctx->ctx_type >> 8) & 0xff))
			|| (VAEntrypointEncSlice ==
					(found_ctx->ctx_type & 0xff)
				&& VAProfileVP8Version0_3 ==
					((found_ctx->ctx_type >> 8) & 0xff))
			) {
			ctx_type = found_ctx->ctx_type & 0xff;
			PSB_DEBUG_PM("Remove vsp context.\n");
			vsp_rm_context(dev_priv->dev, filp, ctx_type);
#endif
		} else
#endif
		{
			mutex_lock(&msvdx_priv->msvdx_mutex);
			if (msvdx_priv->msvdx_ctx == found_ctx)
				msvdx_priv->msvdx_ctx = NULL;
			if (msvdx_priv->last_msvdx_ctx == found_ctx)
				msvdx_priv->last_msvdx_ctx = NULL;
			mutex_unlock(&msvdx_priv->msvdx_mutex);
		}

		kfree(found_ctx);
		#if (defined CONFIG_GFX_RTPM) && (!defined MERRIFIELD)
		psb_ospm_post_power_down();
		#endif
	}
}

static struct psb_video_ctx *psb_find_videoctx(struct drm_psb_private *dev_priv,
						struct file *filp)
{
	struct psb_video_ctx *pos, *n;
	unsigned long irq_flags;

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (pos->filp == filp) {
			spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);
			return pos;
		}
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);
	return NULL;
}

static int psb_entrypoint_number(struct drm_psb_private *dev_priv,
		uint32_t entry_type)
{
	struct psb_video_ctx *pos, *n;
	int count = 0;
	unsigned long irq_flags;

	entry_type &= 0xff;

	if (entry_type < VAEntrypointVLD ||
			entry_type > VAEntrypointEncPicture) {
		DRM_ERROR("Invalide entrypoint value %d.\n", entry_type);
		return -EINVAL;
	}

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (entry_type == (pos->ctx_type & 0xff))
			count++;
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	PSB_DEBUG_GENERAL("There are %d active entrypoint %d.\n",
			count, entry_type);
	return count;
}

int psb_video_getparam(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_lnc_video_getparam_arg *arg = data;
	int ret = 0;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	drm_psb_msvdx_frame_info_t *current_frame = NULL;
	uint32_t handle, i;
	uint32_t device_info = 0;
	uint64_t ctx_type = 0;
	struct psb_video_ctx *video_ctx = NULL;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	unsigned long irq_flags;
	struct file *filp = file_priv->filp;
#if (!defined(MERRIFIELD) && !defined(CONFIG_DRM_VXD_BYT))
	uint32_t imr_info[2];
#endif
#ifdef SUPPORT_VSP
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
#endif
#ifdef CONFIG_VIDEO_MRFLD
	struct ttm_object_file *tfile = psb_fpriv(file_priv)->tfile;
	struct psb_msvdx_ec_ctx *ec_ctx = NULL;
#endif

	switch (arg->key) {
#if (!defined(MERRIFIELD) && !defined(CONFIG_DRM_VXD_BYT))
	case LNC_VIDEO_GETPARAM_IMR_INFO:
		imr_info[0] = dev_priv->imr_region_start;
		imr_info[1] = dev_priv->imr_region_size;
		ret = copy_to_user((void __user *)((unsigned long)arg->value),
				   &imr_info[0],
				   sizeof(imr_info));
		break;
#endif

	case LNC_VIDEO_DEVICE_INFO:
#ifdef CONFIG_DRM_VXD_BYT
		device_info = (0xffff & dev->pci_device) << 16;
#else
		device_info = 0xffff & dev_priv->video_device_fuse;
		device_info |= (0xffff & dev->pci_device) << 16;
#endif
		ret = copy_to_user((void __user *)((unsigned long)arg->value),
				   &device_info, sizeof(device_info));
		break;

	case IMG_VIDEO_NEW_CONTEXT:
		/* add video decode/encode context */
		ret = copy_from_user(&ctx_type, (void __user *)((unsigned long)arg->value),
				     sizeof(ctx_type));
		if (ret)
			break;

		video_ctx = kzalloc(sizeof(struct psb_video_ctx), GFP_KERNEL);
		if (video_ctx == NULL) {
			ret = -ENOMEM;
			break;
		}
		INIT_LIST_HEAD(&video_ctx->head);
		video_ctx->ctx_type = ctx_type;
		video_ctx->cur_sequence = 0xffffffff;
		if (IS_ANN(dev))
			ann_add_workaround_ctx(dev_priv, ctx_type);
#ifdef CONFIG_SLICE_HEADER_PARSING
		video_ctx->frame_end_seq = 0xffffffff;
		if (ctx_type & VA_RT_FORMAT_PROTECTED) {
			video_ctx->slice_extract_flag = 1;
			video_ctx->frame_boundary = 1;
			video_ctx->frame_end_seq = 0xffffffff;
		}
#endif
		video_ctx->filp = file_priv->filp;
		spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
		list_add(&video_ctx->head, &dev_priv->video_ctx);
		spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);
#ifndef CONFIG_DRM_VXD_BYT
#ifndef MERRIFIELD
		if (IS_MDFLD(dev_priv->dev) &&
				(VAEntrypointEncSlice ==
				 (ctx_type & 0xff)))
			pnw_reset_fw_status(dev_priv->dev,
				PNW_TOPAZ_START_CTX);
#endif

#ifdef SUPPORT_VSP
		if ((VAEntrypointVideoProc == (ctx_type & 0xff)
				&& 0xff == ((ctx_type >> 8) & 0xff))
			|| (VAEntrypointEncSlice == (ctx_type & 0xff)
				&& VAProfileVP8Version0_3 ==
					((ctx_type >> 8) & 0xff))) {
			ret = vsp_new_context(dev, filp, ctx_type & 0xff);
			if (ret)
				break;

			if (unlikely(vsp_priv->fw_loaded == 0)) {
				ret = tng_securefw(dev, "vsp", "VSP", TNG_IMR11L_MSG_REGADDR);
				if (ret != 0) {
					DRM_ERROR("VSP: failed to init firmware\n");
					break;
				}
			}
		}
#endif
#endif
		PSB_DEBUG_INIT("Video:add ctx profile %llu, entry %llu.\n",
					((ctx_type >> 8) & 0xff),
					(ctx_type & 0xff));
		PSB_DEBUG_INIT("Video:add context protected %llu.\n",
					(ctx_type & VA_RT_FORMAT_PROTECTED));
		if (ctx_type & VA_RT_FORMAT_PROTECTED)
			ied_enabled = 1;
		break;
	case IMG_VIDEO_RM_CONTEXT:
		psb_remove_videoctx(dev_priv, file_priv->filp);
		break;
	case IMG_VIDEO_UPDATE_CONTEXT:
		ret = copy_from_user(&ctx_type,
				(void __user *)((unsigned long)arg->value),
				sizeof(ctx_type));
		if (ret)
			break;
		video_ctx = psb_find_videoctx(dev_priv, file_priv->filp);
		if (video_ctx) {
			PSB_DEBUG_GENERAL(
				"Video: update video ctx old value %llu\n",
				video_ctx->ctx_type);
			video_ctx->ctx_type = ctx_type;
			PSB_DEBUG_GENERAL(
				"Video: update video ctx new value %llu\n",
				video_ctx->ctx_type);
		} else
			PSB_DEBUG_GENERAL(
				"Video:fail to find context profile %llu, entrypoint %llu",
				(ctx_type >> 8), (ctx_type & 0xff));
		break;
	case IMG_VIDEO_DECODE_STATUS:
#ifdef CONFIG_VIDEO_MRFLD
		if (msvdx_priv->host_be_opp_enabled) {
			/*get the right frame_info struct for current surface*/
			ret = copy_from_user(&handle,
					     (void __user *)((unsigned long)arg->arg), 4);
			if (ret)
				break;

			for (i = 0; i < MAX_DECODE_BUFFERS; i++) {
				if (msvdx_priv->frame_info[i].handle == handle) {
					current_frame = &msvdx_priv->frame_info[i];
					break;
				}
			}
			if (!current_frame) {
				DRM_ERROR("MSVDX: didn't find frame_info which matched the surface_id. \n");
				ret = -EFAULT;
				break;
			}
			ret = copy_to_user((void __user *)((unsigned long)arg->value),
					   &current_frame->fw_status, sizeof(current_frame->fw_status));
		} else
#endif
		{
			ret = copy_to_user((void __user *)((unsigned long)arg->value),
					   &msvdx_priv->decoding_err, sizeof(msvdx_priv->decoding_err));
		}
		break;
#ifdef CONFIG_VIDEO_MRFLD
	case IMG_VIDEO_MB_ERROR:
		/*get the right frame_info struct for current surface*/
		ret = copy_from_user(&handle,
			(void __user *)((unsigned long)arg->arg), 4);
		if (ret)
			break;

		PSB_DEBUG_GENERAL(
			"query surface (handle 0x%08x) decode error\n",
			handle);

		if (msvdx_priv->msvdx_ec_ctx[0] == NULL) {
			PSB_DEBUG_GENERAL(
				"Video: ec contexts are initilized\n");
			return -EFAULT;
		}

		for (i = 0; i < PSB_MAX_EC_INSTANCE; i++)
			if (msvdx_priv->msvdx_ec_ctx[i]->tfile == tfile)
				ec_ctx = msvdx_priv->msvdx_ec_ctx[i];

		if (!ec_ctx) {
			PSB_DEBUG_GENERAL(
				"Video: no ec context found\n");
			return -EFAULT;
		}

		if (ec_ctx->cur_frame_info &&
			ec_ctx->cur_frame_info->handle == handle) {
			ret = copy_to_user(
				(void __user *)((unsigned long)arg->value),
				&(ec_ctx->cur_frame_info->decode_status),
				sizeof(drm_psb_msvdx_decode_status_t));
			PSB_DEBUG_GENERAL(
			"surface is cur_frame, fault region num is %d\n",
			ec_ctx->cur_frame_info->decode_status.num_region);
			break;
		}
		for (i = 0; i < MAX_DECODE_BUFFERS; i++)
			if (ec_ctx->frame_info[i].handle == handle) {
				ret = copy_to_user(
				(void __user *)((unsigned long)arg->value),
				&(ec_ctx->frame_info[i].decode_status),
				sizeof(drm_psb_msvdx_decode_status_t));
				PSB_DEBUG_GENERAL(
					"find surface with index %d, \
					fault region num is %d \n",
			i, ec_ctx->frame_info[i].decode_status.num_region);
				break;
			}

		if (i >= MAX_DECODE_BUFFERS)
			PSB_DEBUG_GENERAL(
			    "could not find handle 0x%08x in ctx\n", handle);

		break;
#endif

	case IMG_VIDEO_SET_DISPLAYING_FRAME:
		ret = copy_from_user(&msvdx_priv->displaying_frame,
				(void __user *)((unsigned long)arg->value),
				sizeof(msvdx_priv->displaying_frame));
		break;
	case IMG_VIDEO_GET_DISPLAYING_FRAME:
		ret = copy_to_user((void __user *)((unsigned long)arg->value),
				&msvdx_priv->displaying_frame,
				sizeof(msvdx_priv->displaying_frame));
		break;

#ifndef CONFIG_DRM_VXD_BYT
	case IMG_VIDEO_GET_HDMI_STATE:
		ret = copy_to_user((void __user *)((unsigned long)arg->value),
				&hdmi_state,
				sizeof(hdmi_state));
		break;
	case IMG_VIDEO_SET_HDMI_STATE:
		if (!hdmi_state) {
			PSB_DEBUG_ENTRY(
				"wait 100ms for kernel hdmi pipe ready.\n");
			msleep(100);
		}
		if (dev_priv->bhdmiconnected)
			hdmi_state = (int)arg->value;
		else
			PSB_DEBUG_ENTRY(
				"skip hdmi_state setting, for unplugged.\n");

		PSB_DEBUG_ENTRY("%s, set hdmi_state = %d\n",
				 __func__, hdmi_state);
		break;
#endif
	case PNW_VIDEO_QUERY_ENTRY:
		ret = copy_from_user(&handle,
				(void __user *)((unsigned long)arg->arg),
				sizeof(handle));
		if (ret)
			break;
		/*Return the number of active entries*/
		i = psb_entrypoint_number(dev_priv, handle);
		ret = copy_to_user((void __user *)
				((unsigned long)arg->value),
				&i, sizeof(i));
		break;
#if (!defined(MERRIFIELD) && !defined(CONFIG_DRM_VXD_BYT))
	case IMG_VIDEO_IED_STATE:
		if (IS_MDFLD(dev)) {
			int enabled = dev_priv->ied_enabled ? 1 : 0;
			ret = copy_to_user((void __user *)
				((unsigned long)arg->value),
				&enabled, sizeof(enabled));
		} else {
			DRM_ERROR("IMG_VIDEO_IED_EANBLE error.\n");
			return -EFAULT;
		}
		break;
#endif
	default:
		ret = -EFAULT;
		break;
	}

	if (ret) {
		DRM_ERROR("%s: failed to call sub-ioctl 0x%llu",
			__func__, arg->key);
		return -EFAULT;
	}

	return 0;
}

/* SLC bug: there is green corruption for vc1 decode if width is not 64 aligned
 * Recode the number of VC1 ctx whose width is not 64 aligned
 */
static void ann_add_workaround_ctx(struct drm_psb_private *dev_priv, uint64_t ctx_type)
{

	struct msvdx_private *msvdx_priv;
	int profile = (ctx_type >> 8) & 0xff;

	if (profile != VAProfileVC1Simple &&
	    profile != VAProfileVC1Main &&
	    profile != VAProfileVC1Advanced)
		return;

	if (unlikely(!dev_priv))
		return;

	msvdx_priv = dev_priv->msvdx_private;
	if (unlikely(!msvdx_priv))
		return;

	PSB_DEBUG_GENERAL(
	    "add vc1 ctx, ctx_type is 0x%llx\n",
		ctx_type);

	/* ctx_type >> 32 is width_in_mb */
	if ((ctx_type >> 32) % 4) {
		atomic_inc(&msvdx_priv->vc1_workaround_ctx);
		PSB_DEBUG_GENERAL("add workaround ctx %p in ctx\n", msvdx_priv);
	}
}

static void ann_rm_workaround_ctx(struct drm_psb_private *dev_priv, uint64_t ctx_type)
{

	struct msvdx_private *msvdx_priv;
	int profile = (ctx_type >> 8) & 0xff;

	if (profile != VAProfileVC1Simple &&
	    profile != VAProfileVC1Main &&
	    profile != VAProfileVC1Advanced)
		return;

	if (unlikely(!dev_priv))
		return;

	msvdx_priv = dev_priv->msvdx_private;
	if (unlikely(!msvdx_priv))
		return;

	PSB_DEBUG_GENERAL(
	    "rm vc1 ctx, ctx_type is 0x%llx\n",
		ctx_type);
	/* ctx_type >> 32 is width_in_mb */
	if ((ctx_type >> 32) % 4) {
		atomic_dec(&msvdx_priv->vc1_workaround_ctx);
		PSB_DEBUG_GENERAL("dec workaround ctx %p in ctx\n", msvdx_priv);
	}
}
