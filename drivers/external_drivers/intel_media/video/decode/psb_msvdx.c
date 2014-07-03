/**************************************************************************
 * MSVDX I/O operations and IRQ handling
 *
 * Copyright (c) 2007 Intel Corporation, Hillsboro, OR, USA
 * Copyright (c) Imagination Technologies Limited, UK
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
 * Authors:
 *    Li Zeng <li.zeng@intel.com>
 *    Binglin Chen <binglin.chen@intel.com>
 *    Fei Jiang <fei.jiang@intel.com>
 *
 **************************************************************************/

#include <drm/drmP.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#include "vxd_drm.h"
#else
#include "psb_drm.h"
#include "psb_drv.h"
#include "psb_powermgmt.h"
#endif
#include "psb_msvdx.h"
#include "psb_msvdx_msg.h"
#include "psb_msvdx_reg.h"
#include "psb_msvdx_ec.h"

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/history_record.h>

#ifdef CONFIG_MDFD_GL3
#include "mdfld_gl3.h"
#endif

#ifndef list_first_entry
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)
#endif

static void psb_msvdx_fw_error_detected(struct drm_device *dev, uint32_t fence, uint32_t flags);
static struct psb_video_ctx* psb_msvdx_find_ctx(struct drm_psb_private *dev_priv, uint32_t fence);
static int psb_msvdx_send(struct drm_device *dev, void *cmd,
			  unsigned long cmd_size, struct psb_video_ctx* msvdx_ctx);
static void psb_msvdx_set_tile(struct drm_device *dev,
				unsigned long msvdx_tile);
static int psb_msvdx_protected_frame_finished(struct drm_psb_private *dev_priv, struct psb_video_ctx* pos, uint32_t fence);
int psb_msvdx_dequeue_send(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct psb_msvdx_cmd_queue *msvdx_cmd = NULL;
	int ret = 0;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	unsigned long irq_flags;

	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	if (list_empty(&msvdx_priv->msvdx_queue)) {
		PSB_DEBUG_GENERAL("MSVDXQUE: msvdx list empty.\n");
		msvdx_priv->msvdx_busy = 0;
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		return -EINVAL;
	}

	msvdx_cmd = list_first_entry(&msvdx_priv->msvdx_queue,
				     struct psb_msvdx_cmd_queue, head);
	list_del(&msvdx_cmd->head);
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);

#ifdef MERRIFIELD
#ifdef CONFIG_SLICE_HEADER_PARSING
	if(msvdx_cmd->msvdx_ctx->slice_extract_flag) {
		if (msvdx_cmd->msvdx_ctx->frame_boundary)
			power_island_get(OSPM_VIDEO_DEC_ISLAND);

		msvdx_cmd->msvdx_ctx->copy_cmd_send = 1;
		msvdx_cmd->msvdx_ctx->frame_end_seq = 0xffffffff;
		msvdx_cmd->msvdx_ctx->frame_boundary = msvdx_cmd->fence_flag >> 1;

		if (msvdx_cmd->msvdx_ctx->frame_boundary)
			msvdx_cmd->msvdx_ctx->frame_end_seq = msvdx_cmd->sequence & 0xffff;
	}else
#endif
	power_island_get(OSPM_VIDEO_DEC_ISLAND);
#endif

	PSB_DEBUG_GENERAL("MSVDXQUE: Queue has id %08x\n", msvdx_cmd->sequence);

	if (IS_MSVDX_MEM_TILE(dev) && drm_psb_msvdx_tiling)
		psb_msvdx_set_tile(dev, msvdx_cmd->msvdx_tile);

#ifdef CONFIG_VIDEO_MRFLD_EC
	/* Seperate update frame and backup cmds because if a batch of cmds
	 * doesn't have * host_be_opp message, no need to update frame info
	 * but still need to backup cmds.
	 * This case can happen if an batch of cmds is not the entire frame
	*/
	if (msvdx_cmd->host_be_opp_enabled) {
		psb_msvdx_update_frame_info(msvdx_priv, msvdx_cmd->tfile,
			msvdx_cmd->cmd + msvdx_cmd->deblock_cmd_offset);
	}
	psb_msvdx_backup_cmd(msvdx_priv, msvdx_cmd->tfile,
			msvdx_cmd->cmd,
			msvdx_cmd->cmd_size,
			msvdx_cmd->deblock_cmd_offset);
#endif
	ret = psb_msvdx_send(dev, msvdx_cmd->cmd, msvdx_cmd->cmd_size, msvdx_cmd->msvdx_ctx);
	if (ret) {
		DRM_ERROR("MSVDXQUE: psb_msvdx_send failed\n");
		ret = -EINVAL;
	}

	kfree(msvdx_cmd->cmd);
	kfree(msvdx_cmd);

	return ret;
}

void psb_msvdx_flush_cmd_queue(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct psb_msvdx_cmd_queue *msvdx_cmd;
	struct list_head *list, *next;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	unsigned long irq_flags;
	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	/*Flush the msvdx cmd queue and signal all fences in the queue */
	list_for_each_safe(list, next, &msvdx_priv->msvdx_queue) {
		msvdx_cmd =
			list_entry(list, struct psb_msvdx_cmd_queue, head);
		list_del(list);
		PSB_DEBUG_GENERAL("MSVDXQUE: flushing sequence:0x%08x\n",
				  msvdx_cmd->sequence);
		msvdx_priv->msvdx_current_sequence = msvdx_cmd->sequence;
		psb_fence_error(dev, PSB_ENGINE_DECODE,
				msvdx_cmd->sequence,
				_PSB_FENCE_TYPE_EXE, DRM_CMD_HANG);
		kfree(msvdx_cmd->cmd);
		kfree(msvdx_cmd);
	}
	msvdx_priv->msvdx_busy = 0;
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
}

static int psb_msvdx_map_command(struct drm_device *dev,
				 struct ttm_buffer_object *cmd_buffer,
				 unsigned long cmd_offset, unsigned long cmd_size,
				 void **msvdx_cmd, uint32_t sequence, int copy_cmd,
				 struct psb_video_ctx *msvdx_ctx)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int ret = 0;
	unsigned long cmd_page_offset = cmd_offset & ~PAGE_MASK;
	unsigned long cmd_size_remaining;
	struct ttm_bo_kmap_obj cmd_kmap;
	void *cmd, *cmd_copy, *cmd_start;
	bool is_iomem;
	union msg_header *header;

	/* command buffers may not exceed page boundary */
	if ((cmd_size > PAGE_SIZE) || (cmd_size + cmd_page_offset > PAGE_SIZE))
		return -EINVAL;

	ret = ttm_bo_kmap(cmd_buffer, cmd_offset >> PAGE_SHIFT, 1, &cmd_kmap);
	if (ret) {
		DRM_ERROR("MSVDXQUE:ret:%d\n", ret);
		return ret;
	}

	cmd_start = (void *)ttm_kmap_obj_virtual(&cmd_kmap, &is_iomem)
		    + cmd_page_offset;
	cmd = cmd_start;
	cmd_size_remaining = cmd_size;

	msvdx_priv->host_be_opp_enabled = 0;
	msvdx_priv->deblock_cmd_offset = PSB_MSVDX_INVALID_OFFSET;

	while (cmd_size_remaining > 0) {
		if (cmd_size_remaining < MTX_GENMSG_HEADER_SIZE) {
			ret = -EINVAL;
			goto out;
		}
		header = (union msg_header *)cmd;
		uint32_t cur_cmd_size = header->bits.msg_size;
		uint32_t cur_cmd_id = header->bits.msg_type;

		uint32_t mmu_ptd = 0, msvdx_mmu_invalid = 0;

		PSB_DEBUG_GENERAL("cmd start at %08x cur_cmd_size = %d"
				  " cur_cmd_id = %02x fence = %08x\n",
				  (uint32_t) cmd, cur_cmd_size, cur_cmd_id, sequence);
		if ((cur_cmd_size % sizeof(uint32_t))
		    || (cur_cmd_size > cmd_size_remaining)) {
			ret = -EINVAL;
			DRM_ERROR("MSVDX: ret:%d\n", ret);
			goto out;
		}

		switch (cur_cmd_id) {
		case MTX_MSGID_DECODE_FE: {
			if (sizeof(struct fw_decode_msg) > cmd_size_remaining) {
				/* Msg size is not correct */
				ret = -EINVAL;
				PSB_DEBUG_MSVDX("MSVDX: wrong msg size.\n");
				goto out;
			}
			struct fw_decode_msg *decode_msg =
					(struct fw_decode_msg *)cmd;
			decode_msg->header.bits.msg_fence = sequence;

			mmu_ptd = psb_get_default_pd_addr(dev_priv->mmu);
			msvdx_mmu_invalid = atomic_cmpxchg(&dev_priv->msvdx_mmu_invaldc,
							   1, 0);
			if (msvdx_mmu_invalid == 1) {
				decode_msg->flag_size.bits.flags |=
						FW_INVALIDATE_MMU;
#ifdef CONFIG_MDFD_GL3
				gl3_invalidate();
#endif
				PSB_DEBUG_GENERAL("MSVDX:Set MMU invalidate\n");
			}
			/*
			if (msvdx_mmu_invalid == 1)
				psb_mmu_pgtable_dump(dev);
			*/

			decode_msg->mmu_context.bits.mmu_ptd = mmu_ptd >> 8;
			PSB_DEBUG_MSVDX("MSVDX: MSGID_DECODE_FE:"
					  " - fence: %08x"
					  " - flags: %08x - buffer_size: %08x"
					  " - crtl_alloc_addr: %08x"
					  " - context: %08x - mmu_ptd: %08x"
					  " - operating_mode: %08x.\n",
					  decode_msg->header.bits.msg_fence,
					  decode_msg->flag_size.bits.flags,
					  decode_msg->flag_size.bits.buffer_size,
					  decode_msg->crtl_alloc_addr,
					  decode_msg->mmu_context.bits.context,
					  decode_msg->mmu_context.bits.mmu_ptd,
					  decode_msg->operating_mode);
			break;
		}

		case MTX_MSGID_HOST_BE_OPP_MFLD:
			msvdx_priv->host_be_opp_enabled = 1;
			msvdx_priv->deblock_cmd_offset =
					cmd_size - cmd_size_remaining;
		case MTX_MSGID_INTRA_OOLD_MFLD:
		case MTX_MSGID_DEBLOCK_MFLD: {
			if (sizeof(struct fw_deblock_msg) > cmd_size_remaining) {
				/* Msg size is not correct */
				ret = -EINVAL;
				PSB_DEBUG_MSVDX("MSVDX: wrong msg size.\n");
				goto out;
			}
			struct fw_deblock_msg *deblock_msg =
					(struct fw_deblock_msg *)cmd;
			mmu_ptd = psb_get_default_pd_addr(dev_priv->mmu);
			msvdx_mmu_invalid = atomic_cmpxchg(&dev_priv->msvdx_mmu_invaldc,
							   1, 0);
			if (msvdx_mmu_invalid == 1) {
				deblock_msg->flag_type.bits.flags |=
							FW_INVALIDATE_MMU;
				PSB_DEBUG_GENERAL("MSVDX:Set MMU invalidate\n");
			}

			/* patch to right cmd type */
			deblock_msg->header.bits.msg_type =
					cur_cmd_id -
					MTX_MSGID_DEBLOCK_MFLD +
					MTX_MSGID_DEBLOCK;

			deblock_msg->header.bits.msg_fence = (uint16_t)(sequence & 0xffff);
			deblock_msg->mmu_context.bits.mmu_ptd = (mmu_ptd >> 8);
			PSB_DEBUG_MSVDX("MSVDX: MSGID_DEBLOCK:"
				" - fence: %08x"
				" - flags: %08x - slice_field_type: %08x"
				" - operating_mode: %08x"
				" - context: %08x - mmu_ptd: %08x"
				" - frame_height_mb: %08x - pic_width_mb: %08x"
				" - address_a0: %08x - address_a1: %08x"
				" - mb_param_address: %08x"
				" - ext_stride_a: %08x"
				" - address_b0: %08x - address_b1: %08x"
				" - alt_output_flags_b: %08x.\n",
				deblock_msg->header.bits.msg_fence,
				deblock_msg->flag_type.bits.flags,
				deblock_msg->flag_type.bits.slice_field_type,
				deblock_msg->operating_mode,
				deblock_msg->mmu_context.bits.context,
				deblock_msg->mmu_context.bits.mmu_ptd,
				deblock_msg->pic_size.bits.frame_height_mb,
				deblock_msg->pic_size.bits.pic_width_mb,
				deblock_msg->address_a0,
				deblock_msg->address_a1,
				deblock_msg->mb_param_address,
				deblock_msg->ext_stride_a,
				deblock_msg->address_b0,
				deblock_msg->address_b1,
				deblock_msg->alt_output_flags_b);
			cmd += (sizeof(struct fw_deblock_msg) - cur_cmd_size);
			cmd_size_remaining -= (sizeof(struct fw_deblock_msg) -
						cur_cmd_size);
			break;
		}

#ifdef CONFIG_SLICE_HEADER_PARSING
		/* VA_MSGID_NALU_EXTRACT start */
		case MTX_MSGID_SLICE_HEADER_EXTRACT:
		case MTX_MSGID_MODULAR_SLICE_HEADER_EXTRACT: {
			struct fw_slice_header_extract_msg *extract_msg =
				(struct fw_slice_header_extract_msg *)cmd;

			PSB_DEBUG_MSVDX("send slice extract message.\n");

			extract_msg->header.bits.msg_fence = sequence;

			mmu_ptd = psb_get_default_pd_addr(dev_priv->mmu);
			msvdx_mmu_invalid = atomic_cmpxchg(&dev_priv->msvdx_mmu_invaldc,
							   1, 0);
			if (msvdx_mmu_invalid == 1) {
				extract_msg->flags.bits.flags |=
						FW_INVALIDATE_MMU;
#ifdef CONFIG_MDFD_GL3
				gl3_invalidate();
#endif
				PSB_DEBUG_GENERAL("MSVDX:Set MMU invalidate\n");
			}

			extract_msg->mmu_context.bits.mmu_ptd = mmu_ptd >> 8;

			PSB_DEBUG_MSVDX("Parse cmd msg size is %d,"
				"type is 0x%x, fence is %d, flags is 0x%x, context is 0x%x,"
				"mmu_ptd is 0x%x, src is 0x%x, src_size is %d, dst is 0x%x, dst_size is %d,"
				"flag_bitfield is 0x%x, pic_param0 is 0x%x\n",

			extract_msg->header.bits.msg_size,
			extract_msg->header.bits.msg_type,
			extract_msg->header.bits.msg_fence,
			extract_msg->flags.bits.flags,
			extract_msg->mmu_context.bits.context,
			extract_msg->mmu_context.bits.mmu_ptd,
			extract_msg->src,
			extract_msg->src_size,
			extract_msg->dst,
			extract_msg->src_size,
			extract_msg->flag_bitfield.value,
			extract_msg->pic_param0.value);
			break;
		}
		/* VA_MSGID_NALU_EXTRACT end */
#endif
		default:
			/* Msg not supported */
			ret = -EINVAL;
			PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);
			goto out;
		}

		cmd += cur_cmd_size;
		cmd_size_remaining -= cur_cmd_size;
		if (((sequence++) & 0xf) == 0xf) {
			ret = -EINVAL;
			PSB_DEBUG_GENERAL("MSVDX: too many cmds, abort\n");
			goto out;
		}
	}

	msvdx_priv->num_cmd = ((--sequence) & 0xf);

	if (copy_cmd) {
		PSB_DEBUG_GENERAL("MSVDXQUE:copying command\n");

		cmd_copy = kzalloc(cmd_size, GFP_KERNEL);
		if (cmd_copy == NULL) {
			ret = -ENOMEM;
			DRM_ERROR("MSVDX: fail to callc,ret=:%d\n", ret);
			goto out;
		}
		memcpy(cmd_copy, cmd_start, cmd_size);
		*msvdx_cmd = cmd_copy;
	} else {
		PSB_DEBUG_GENERAL("MSVDXQUE:did NOT copy command\n");
		if (IS_MSVDX_MEM_TILE(dev) && drm_psb_msvdx_tiling) {
			unsigned long msvdx_tile =
				((msvdx_priv->msvdx_ctx->ctx_type >> 16) & 0xff);
			psb_msvdx_set_tile(dev, msvdx_tile);
		}

#ifdef CONFIG_VIDEO_MRFLD_EC
		if (msvdx_priv->host_be_opp_enabled) {
			psb_msvdx_update_frame_info(msvdx_priv,
				msvdx_priv->tfile,
				cmd_start + msvdx_priv->deblock_cmd_offset);
		}
		psb_msvdx_backup_cmd(msvdx_priv, msvdx_priv->tfile,
				cmd_start,
				cmd_size,
				msvdx_priv->deblock_cmd_offset);
#endif
		ret = psb_msvdx_send(dev, cmd_start, cmd_size, msvdx_ctx);
		if (ret) {
			DRM_ERROR("MSVDXQUE: psb_msvdx_send failed\n");
			ret = -EINVAL;
		}
	}

out:
	ttm_bo_kunmap(&cmd_kmap);

	return ret;
}

int psb__submit_cmdbuf_copy(struct drm_device *dev,
			    struct ttm_buffer_object *cmd_buffer,
			    unsigned long cmd_offset, unsigned long cmd_size,
			    struct psb_video_ctx *msvdx_ctx, uint32_t fence_flag)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	struct psb_msvdx_cmd_queue *msvdx_cmd;
	uint32_t sequence =  (dev_priv->sequence[PSB_ENGINE_DECODE] << 4);
	unsigned long irq_flags;
	void *cmd = NULL;
	int ret;

	/* queue the command to be sent when the h/w is ready */
	PSB_DEBUG_GENERAL("MSVDXQUE: queueing sequence:%08x..\n",
			  sequence);
	msvdx_cmd = kzalloc(sizeof(struct psb_msvdx_cmd_queue),
			    GFP_KERNEL);
	if (msvdx_cmd == NULL) {
		DRM_ERROR("MSVDXQUE: Out of memory...\n");
		return -ENOMEM;
	}

	ret = psb_msvdx_map_command(dev, cmd_buffer, cmd_offset,
				    cmd_size, &cmd, sequence, 1, msvdx_ctx);
	if (ret) {
		DRM_ERROR("MSVDXQUE: Failed to extract cmd\n");
		kfree(msvdx_cmd
		     );
		return ret;
	}
	msvdx_cmd->cmd = cmd;
	msvdx_cmd->cmd_size = cmd_size;
	msvdx_cmd->sequence = sequence;

	msvdx_cmd->msvdx_tile =
		((msvdx_priv->msvdx_ctx->ctx_type >> 16) & 0xff);
	msvdx_cmd->deblock_cmd_offset =
		msvdx_priv->deblock_cmd_offset;
	msvdx_cmd->host_be_opp_enabled =
		msvdx_priv->host_be_opp_enabled;
	msvdx_cmd->tfile =
		msvdx_priv->tfile;
	msvdx_cmd->msvdx_ctx = msvdx_ctx;
#ifdef CONFIG_SLICE_HEADER_PARSING
	msvdx_cmd->fence_flag = fence_flag;
	msvdx_cmd->frame_boundary =  msvdx_cmd->fence_flag >> 1;
	msvdx_cmd->msvdx_ctx->copy_cmd_send = 0;
#endif
	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	list_add_tail(&msvdx_cmd->head, &msvdx_priv->msvdx_queue);
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
	if (!msvdx_priv->msvdx_busy) {
		msvdx_priv->msvdx_busy = 1;
		PSB_DEBUG_GENERAL("MSVDXQUE: Need immediate dequeue\n");
		psb_msvdx_dequeue_send(dev);
	}

	return ret;
}

int psb_submit_video_cmdbuf(struct drm_device *dev,
			    struct ttm_buffer_object *cmd_buffer,
			    unsigned long cmd_offset, unsigned long cmd_size,
			    struct psb_video_ctx *msvdx_ctx, uint32_t fence_flag)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t sequence =  (dev_priv->sequence[PSB_ENGINE_DECODE] << 4);
	unsigned long irq_flags;
	int ret = 0;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int offset = 0;

	if (!msvdx_priv->fw_b0_uploaded){
#ifdef MERRIFIELD
		if (IS_TNG_B0(dev) || IS_MOFD(dev))
			tng_securefw(dev, "msvdx", "VED", TNG_IMR5L_MSG_REGADDR);
		else {
			DRM_ERROR("VED secure fw: bad platform\n");
		}

		/*  change fw_b0_uploaded name */
		msvdx_priv->fw_b0_uploaded = 1;
#endif
	}

	if (!msvdx_ctx) {
		PSB_DEBUG_GENERAL("MSVDX: null ctx\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);

	msvdx_priv->msvdx_ctx = msvdx_ctx;
	msvdx_priv->last_msvdx_ctx = msvdx_priv->msvdx_ctx;

	PSB_DEBUG_PM("sequence is 0x%x, needs_reset is 0x%x.\n",
			sequence, msvdx_priv->msvdx_needs_reset);

#ifdef MERRIFIELD
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
	/* get power island when submit cmd to hardware */
#ifdef CONFIG_SLICE_HEADER_PARSING
	if (msvdx_ctx->slice_extract_flag) {
		if(msvdx_ctx->frame_boundary)
			if (!power_island_get(OSPM_VIDEO_DEC_ISLAND))
				return -EBUSY;
	}else{
		if (!power_island_get(OSPM_VIDEO_DEC_ISLAND))
			return -EBUSY;
	}
#else
	if (!power_island_get(OSPM_VIDEO_DEC_ISLAND))
		return -EBUSY;
#endif
	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
#endif

	if (msvdx_priv->msvdx_busy) {
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		ret = psb__submit_cmdbuf_copy(dev, cmd_buffer,
			    cmd_offset, cmd_size,
			    msvdx_ctx, fence_flag);

#ifdef MERRIFIELD
#ifdef CONFIG_SLICE_HEADER_PARSING
		if (msvdx_ctx->slice_extract_flag){
			if (msvdx_ctx->copy_cmd_send) {
				if(!msvdx_ctx->frame_boundary)
					power_island_put(OSPM_VIDEO_DEC_ISLAND);
			}
			else {
				if (msvdx_ctx->frame_boundary)
					power_island_put(OSPM_VIDEO_DEC_ISLAND);
			}
		}
		else{
			power_island_put(OSPM_VIDEO_DEC_ISLAND);
		}
#else
		power_island_put(OSPM_VIDEO_DEC_ISLAND);
#endif
#endif
		return ret;
	}

	if (msvdx_priv->msvdx_needs_reset) {
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		PSB_DEBUG_GENERAL("MSVDX: will reset msvdx\n");

		if (!msvdx_priv->fw_loaded_by_punit) {
			if (psb_msvdx_core_reset(dev_priv)) {
				ret = -EBUSY;
				DRM_ERROR("MSVDX: Reset failed\n");
				return ret;
			}
		}

		msvdx_priv->msvdx_needs_reset = 0;
		msvdx_priv->msvdx_busy = 0;

		if (msvdx_priv->fw_loaded_by_punit){
			ret = psb_msvdx_post_init(dev);
			if (ret) {
				ret = -EBUSY;
#ifdef MERRIFIELD
				power_island_put(OSPM_VIDEO_DEC_ISLAND);
#endif
				PSB_DEBUG_WARN("WARN: psb_msvdx_post_init failed.\n");
				return ret;
			}
		}
		else{
			if (psb_msvdx_init(dev)) {
				ret = -EBUSY;
				PSB_DEBUG_WARN("WARN: psb_msvdx_init failed.\n");
				return ret;
			}
		}

#ifdef CONFIG_VIDEO_MRFLD_EC
		/* restore the state when power up during EC */
		if (msvdx_priv->vec_ec_mem_saved) {
			for (offset = 0; offset < 4; ++offset)
				PSB_WMSVDX32(msvdx_priv->vec_ec_mem_data[offset],
					     0x2cb0 + offset * 4);

			PSB_WMSVDX32(msvdx_priv->vec_ec_mem_data[4],
				     0x2cc4);
			msvdx_priv->vec_ec_mem_saved = 0;
		}
#endif

		spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	}

	if (msvdx_priv->fw_loaded_by_punit && !msvdx_priv->rendec_init) {
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		PSB_DEBUG_GENERAL("MSVDX:setup msvdx.\n");
		ret = psb_msvdx_post_boot_init(dev);
		if (ret) {
			DRM_ERROR("MSVDX:fail to setup msvdx.\n");
			/* FIXME: find a proper return value */
			return -EFAULT;
		}
		msvdx_priv->rendec_init = 1;

		PSB_DEBUG_GENERAL("MSVDX: setup msvdx successfully\n");
		spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	}

	if (!msvdx_priv->fw_loaded_by_punit && !msvdx_priv->msvdx_fw_loaded) {
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		PSB_DEBUG_GENERAL("MSVDX:reload FW to MTX\n");
		ret = psb_setup_fw(dev);
		if (ret) {
			DRM_ERROR("MSVDX:fail to load FW\n");
			/* FIXME: find a proper return value */
			return -EFAULT;
		}
		msvdx_priv->msvdx_fw_loaded = 1;

		PSB_DEBUG_GENERAL("MSVDX: load firmware successfully\n");
		spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
	}

	msvdx_priv->msvdx_busy = 1;
#ifdef CONFIG_SLICE_HEADER_PARSING
	if (msvdx_ctx->slice_extract_flag){
		msvdx_ctx->frame_boundary = fence_flag >> 1;
		msvdx_ctx->frame_end_seq = 0xffffffff;
		if (msvdx_ctx->frame_boundary)
			msvdx_ctx->frame_end_seq = sequence & 0xffff;
	}
#endif
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
	PSB_DEBUG_GENERAL("MSVDX: commit command to HW,seq=0x%08x\n",
			  sequence);
	ret = psb_msvdx_map_command(dev, cmd_buffer, cmd_offset,
				    cmd_size, NULL, sequence, 0, msvdx_ctx);
	if (ret)
		DRM_ERROR("MSVDXQUE: Failed to extract cmd\n");

	return ret;
}

int psb_cmdbuf_video(struct drm_file *priv,
		     struct list_head *validate_list,
		     uint32_t fence_type,
		     struct drm_psb_cmdbuf_arg *arg,
		     struct ttm_buffer_object *cmd_buffer,
		     struct psb_ttm_fence_rep *fence_arg,
		     struct psb_video_ctx *msvdx_ctx)
{
	struct drm_device *dev = priv->minor->dev;
	struct ttm_fence_object *fence;
	int ret;

	/*
	 * Check this. Doesn't seem right. Have fencing done AFTER command
	 * submission and make sure drm_psb_idle idles the MSVDX completely.
	 */
	ret = psb_submit_video_cmdbuf(dev, cmd_buffer, arg->cmdbuf_offset,
					arg->cmdbuf_size, msvdx_ctx, arg->fence_flags);
	if (ret)
		return ret;


	/* DRM_ERROR("Intel: Fix video fencing!!\n"); */
	psb_fence_or_sync(priv, PSB_ENGINE_DECODE, fence_type,
			  arg->fence_flags, validate_list, fence_arg,
			  &fence);

	ttm_fence_object_unref(&fence);
	spin_lock(&cmd_buffer->bdev->fence_lock);
	if (cmd_buffer->sync_obj != NULL)
		ttm_fence_sync_obj_unref(&cmd_buffer->sync_obj);
	spin_unlock(&cmd_buffer->bdev->fence_lock);

	return 0;
}


static int psb_msvdx_send(struct drm_device *dev, void *cmd,
			  unsigned long cmd_size, struct psb_video_ctx *msvdx_ctx)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = psb_priv(dev);
	union msg_header *header;
	uint32_t cur_sequence = 0xffffffff;

	while (cmd_size > 0) {
		header = (union msg_header *)cmd;
		uint32_t cur_cmd_size = header->bits.msg_size;
		uint32_t cur_cmd_id = header->bits.msg_type;

		cur_sequence = ((struct fw_msg_header *)cmd)->header.bits.msg_fence;

		if (cur_sequence != 0xffffffff) {
			msvdx_ctx->cur_sequence = cur_sequence;
		}

		if (cur_cmd_size > cmd_size) {
			ret = -EINVAL;
			DRM_ERROR("MSVDX:cmd_size %lu cur_cmd_size %lu\n",
				  cmd_size, (unsigned long)cur_cmd_size);
			goto out;
		}

		/* Send the message to h/w */
		ret = psb_mtx_send(dev_priv, cmd);
		if (ret) {
			PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);
			goto out;
		}
		cmd += cur_cmd_size;
		cmd_size -= cur_cmd_size;
		if (cur_cmd_id == MTX_MSGID_HOST_BE_OPP ||
			cur_cmd_id == MTX_MSGID_DEBLOCK ||
			cur_cmd_id == MTX_MSGID_INTRA_OOLD) {
			cmd += (sizeof(struct fw_deblock_msg) - cur_cmd_size);
			cmd_size -=
				(sizeof(struct fw_deblock_msg) - cur_cmd_size);
		}
	}
out:
	PSB_DEBUG_GENERAL("MSVDX: ret:%d\n", ret);
	return ret;
}

int psb_mtx_send(struct drm_psb_private *dev_priv, const void *msg)
{
	static struct fw_padding_msg pad_msg;
	const uint32_t *p_msg = (uint32_t *) msg;
	uint32_t msg_num, words_free, ridx, widx, buf_size, buf_offset;
	int ret = 0;
	union msg_header *header;
	header = (union msg_header *)msg;

	PSB_DEBUG_GENERAL("MSVDX: psb_mtx_send\n");

	/* we need clocks enabled before we touch VEC local ram,
	 * but fw will take care of the clock after fw is loaded
	 */

	msg_num = (header->bits.msg_size + 3) / 4;

#if 0
	{
		int i;
		printk(KERN_DEBUG "MSVDX: psb_mtx_send is %dDW\n",
		       msg_num);

		for (i = 0; i < msg_num; i++)
			printk(KERN_DEBUG "0x%08x ", p_msg[i]);
		printk(KERN_DEBUG "\n");
	}
#endif
	buf_size = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_BUF_SIZE) & ((1 << 16) - 1);

	if (msg_num > buf_size) {
		ret = -EINVAL;
		DRM_ERROR("MSVDX: message exceed maximum,ret:%d\n", ret);
		goto out;
	}

	ridx = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_RD_INDEX);
	widx = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_WRT_INDEX);


	buf_size = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_BUF_SIZE) & ((1 << 16) - 1);
	/*0x2000 is VEC Local Ram offset*/
	buf_offset =
		(PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_BUF_SIZE) >> 16) + 0x2000;

	/* message would wrap, need to send a pad message */
	if (widx + msg_num > buf_size) {
		/* Shouldn't happen for a PAD message itself */
		if (header->bits.msg_type == MTX_MSGID_PADDING)
			DRM_INFO("MSVDX WARNING: should not wrap pad msg, "
				"buf_size is %d, widx is %d, msg_num is %d.\n",
				buf_size, widx, msg_num);

		/* if the read pointer is at zero then we must wait for it to
		 * change otherwise the write pointer will equal the read
		 * pointer,which should only happen when the buffer is empty
		 *
		 * This will only happens if we try to overfill the queue,
		 * queue management should make
		 * sure this never happens in the first place.
		 */
		if (0 == ridx) {
			ret = -EINVAL;
			DRM_ERROR("MSVDX: RIndex=0, ret:%d\n", ret);
			goto out;
		}

		/* Send a pad message */
		pad_msg.header.bits.msg_size = (buf_size - widx) << 2;
		pad_msg.header.bits.msg_type = MTX_MSGID_PADDING;
		psb_mtx_send(dev_priv, (void *)&pad_msg);
		widx = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_WRT_INDEX);
	}

	if (widx >= ridx)
		words_free = buf_size - (widx - ridx) - 1;
	else
		words_free = ridx - widx - 1;

	if (msg_num > words_free) {
		ret = -EINVAL;
		DRM_ERROR("MSVDX: msg_num > words_free, ret:%d\n", ret);
		goto out;
	}
	while (msg_num > 0) {
		PSB_WMSVDX32(*p_msg++, buf_offset + (widx << 2));
		msg_num--;
		widx++;
		if (buf_size == widx)
			widx = 0;
	}

	PSB_WMSVDX32(widx, MSVDX_COMMS_TO_MTX_WRT_INDEX);

	/* Make sure clocks are enabled before we kick
	 * but fw will take care of the clock after fw is loaded
	 */

	/* signal an interrupt to let the mtx know there is a new message */
	PSB_WMSVDX32(1, MTX_KICK_INPUT_OFFSET);

out:
	return ret;
}

/*
 * MSVDX MTX interrupt
 */

static void psb_msvdx_mtx_interrupt(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	static uint32_t buf[128]; /* message buffer */
	uint32_t ridx, widx, buf_size, buf_offset;
	uint32_t num, ofs; /* message num and offset */
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int i;
	union msg_header *header;
	int cmd_complete = 0;
#ifdef CONFIG_SLICE_HEADER_PARSING
	int frame_finished = 1;
#endif
	PSB_DEBUG_GENERAL("MSVDX:Got a MSVDX MTX interrupt\n");

	/* we need clocks enabled before we touch VEC local ram,
	 * but fw will take care of the clock after fw is loaded
	 */

loop: /* just for coding style check */
	ridx = PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_RD_INDEX);
	widx = PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_WRT_INDEX);

	/* Get out of here if nothing */
	if (ridx == widx)
		goto done;

	buf_size = PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_BUF_SIZE) & ((1 << 16) - 1);
	/*0x2000 is VEC Local Ram offset*/
	buf_offset =
		(PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_BUF_SIZE) >> 16) + 0x2000;

	ofs = 0;
	buf[ofs] = PSB_RMSVDX32(buf_offset + (ridx << 2));
	header = (union msg_header *)buf;

	/* round to nearest word */
	num = (header->bits.msg_size + 3) / 4;

	/* ASSERT(num <= sizeof(buf) / sizeof(uint32_t)); */

	if (++ridx >= buf_size)
		ridx = 0;

	for (ofs++; ofs < num; ofs++) {
		buf[ofs] = PSB_RMSVDX32(buf_offset + (ridx << 2));

		if (++ridx >= buf_size)
			ridx = 0;
	}

	/* Update the Read index */
	PSB_WMSVDX32(ridx, MSVDX_COMMS_TO_HOST_RD_INDEX);

	if (msvdx_priv->msvdx_needs_reset)
		goto loop;

	switch (header->bits.msg_type) {
	case MTX_MSGID_HW_PANIC: {
		/* For VXD385 firmware, fence value is not validate here */
		uint32_t diff = 0;
		uint32_t fence, last_mb;
		drm_psb_msvdx_frame_info_t *failed_frame = NULL;

		struct fw_panic_msg *panic_msg = (struct fw_panic_msg *)buf;

		cmd_complete = 1;

		PSB_DEBUG_WARN("MSVDX: MSGID_CMD_HW_PANIC:"
				  "Fault detected"
				  " - Fence: %08x"
				  " - fe_status mb: %08x"
				  " - be_status mb: %08x"
				  " - reserved2: %08x"
				  " - last mb: %08x"
				  " - resetting and ignoring error\n",
				  panic_msg->header.bits.msg_fence,
				  panic_msg->fe_status,
				  panic_msg->be_status,
				  panic_msg->mb.bits.reserved2,
				  panic_msg->mb.bits.last_mb);
		/* If bit 8 of MSVDX_INTERRUPT_STATUS is set the fault was caused in the DMAC.
		 * In this case you should check bits 20:22 of MSVDX_INTERRUPT_STATUS.
		 * If bit 20 is set there was a problem DMAing the buffer back to host.
		 * If bit 22 is set you'll need to get the value of MSVDX_DMAC_STREAM_STATUS (0x648).
		 * If bit 1 is set then there was an issue DMAing the bitstream or termination code for parsing.*/
		PSB_DEBUG_MSVDX("MSVDX: MSVDX_COMMS_ERROR_TRIG is 0x%x,"
				"MSVDX_INTERRUPT_STATUS is 0x%x,"
				"MSVDX_MMU_STATUS is 0x%x,"
				"MSVDX_DMAC_STREAM_STATUS is 0x%x.\n",
				PSB_RMSVDX32(MSVDX_COMMS_ERROR_TRIG),
				PSB_RMSVDX32(MSVDX_INTERRUPT_STATUS_OFFSET),
				PSB_RMSVDX32(MSVDX_MMU_STATUS_OFFSET),
				PSB_RMSVDX32(MSVDX_DMAC_STREAM_STATUS_OFFSET));

		fence = panic_msg->header.bits.msg_fence;
		last_mb = panic_msg->mb.bits.last_mb;

		if (msvdx_priv->fw_loaded_by_punit)
			msvdx_priv->msvdx_needs_reset |= MSVDX_RESET_NEEDS_REUPLOAD_FW |
				MSVDX_RESET_NEEDS_INIT_FW;
		else
			msvdx_priv->msvdx_needs_reset = 1;

		diff = msvdx_priv->msvdx_current_sequence
		       - dev_priv->sequence[PSB_ENGINE_DECODE];

		if (diff > 0x0FFFFFFF)
			msvdx_priv->msvdx_current_sequence++;

		PSB_DEBUG_WARN("MSVDX: Fence ID missing, "
				  "assuming %08x\n",
				  msvdx_priv->msvdx_current_sequence);

		psb_fence_error(dev, PSB_ENGINE_DECODE,
				msvdx_priv->msvdx_current_sequence,
				_PSB_FENCE_TYPE_EXE, DRM_CMD_FAILED);

		/* Flush the command queue */
		psb_msvdx_flush_cmd_queue(dev);
		if (msvdx_priv->host_be_opp_enabled) {
			/*get the frame_info struct for error concealment frame*/
			for (i = 0; i < MAX_DECODE_BUFFERS; i++) {
				/*by default the fence is 0, so there is problem here???*/
				if (msvdx_priv->frame_info[i].fence == fence) {
					failed_frame = &msvdx_priv->frame_info[i];
					break;
				}
			}
			if (!failed_frame) {
				DRM_ERROR("MSVDX: didn't find frame_info which matched the fence %d in failed/panic message\n", fence);
				goto done;
			}

			failed_frame->fw_status = 1; /* set ERROR flag */
		}
		msvdx_priv->decoding_err = 1;

		goto done;
	}

	case MTX_MSGID_COMPLETED: {
		uint32_t fence, flags;
		struct fw_completed_msg *completed_msg =
					(struct fw_completed_msg *)buf;
		struct psb_video_ctx *msvdx_ctx;

		PSB_DEBUG_GENERAL("MSVDX: MSGID_CMD_COMPLETED:"
			" - Fence: %08x - flags: %08x - vdebcr: %08x"
			" - first_mb : %d - last_mb: %d\n",
			completed_msg->header.bits.msg_fence,
			completed_msg->flags, completed_msg->vdebcr,
			completed_msg->mb.bits.start_mb, completed_msg->mb.bits.last_mb);

		cmd_complete = 1;

		flags = completed_msg->flags;
		fence = completed_msg->header.bits.msg_fence;

		msvdx_priv->msvdx_current_sequence = fence;

		if (IS_MRFLD(dev))
			psb_msvdx_fw_error_detected(dev, fence, flags);

		psb_fence_handler(dev, PSB_ENGINE_DECODE);

		msvdx_ctx = psb_msvdx_find_ctx(dev_priv, fence);
		if (unlikely(msvdx_ctx == NULL)) {
			PSB_DEBUG_GENERAL("abnormal complete msg\n");
			cmd_complete = 0;
		}
		else {
#ifdef CONFIG_SLICE_HEADER_PARSING
			msvdx_ctx->frame_end= psb_msvdx_protected_frame_finished(dev_priv, msvdx_ctx, fence);
#endif
		}

		if (flags & FW_VA_RENDER_HOST_INT) {
			/*Now send the next command from the msvdx cmd queue */
#ifndef CONFIG_DRM_VXD_BYT
			if (!(IS_MRFLD(dev)) ||
				drm_msvdx_pmpolicy == PSB_PMPOLICY_NOPM)
#endif
				psb_msvdx_dequeue_send(dev);
			goto done;
		}

		break;
	}

	case MTX_MSGID_CONTIGUITY_WARNING: {
		drm_psb_msvdx_decode_status_t *fault_region = NULL;
		struct psb_msvdx_ec_ctx *msvdx_ec_ctx = NULL;
		uint32_t reg_idx;
		int found = 0;

		struct fw_contiguity_msg *contiguity_msg =
					(struct fw_contiguity_msg *)buf;

		PSB_DEBUG_GENERAL("MSVDX: MSGID_CONTIGUITY_WARNING:");
		PSB_DEBUG_GENERAL(
			"- Fence: %08x - end_mb: %08x - begin_mb: %08x\n",
			contiguity_msg->header.bits.msg_fence,
			contiguity_msg->mb.bits.end_mb_num,
			contiguity_msg->mb.bits.begin_mb_num);

		/*get erro info*/
		uint32_t fence = contiguity_msg->header.bits.msg_fence;
		uint32_t start = contiguity_msg->mb.bits.begin_mb_num;
		uint32_t end = contiguity_msg->mb.bits.end_mb_num;

		/*get the frame_info struct for error concealment frame*/
		for (i = 0; i < PSB_MAX_EC_INSTANCE; i++)
			if (msvdx_priv->msvdx_ec_ctx[i]->fence ==
							(fence & (~0xf))) {
				msvdx_ec_ctx = msvdx_priv->msvdx_ec_ctx[i];
				found++;
			}
		/* psb_msvdx_mtx_message_dump(dev); */
		if (!msvdx_ec_ctx || !(msvdx_ec_ctx->tfile) || found > 1) {
			PSB_DEBUG_MSVDX(
			"no matched ctx: fence 0x%x, found %d, ctx 0x%08x\n",
				fence, found, msvdx_ec_ctx);
			goto done;
		}


		fault_region = &msvdx_ec_ctx->decode_status;
		if (start > end)
			start = end;
		if (start < PSB_MSVDX_EC_ROLLBACK)
			start = 0;
		else
			start -= PSB_MSVDX_EC_ROLLBACK;

		if (fault_region->num_region) {
			reg_idx = fault_region->num_region - 1;
			if ((start <= fault_region->mb_regions[reg_idx].end) &&
			    (end > fault_region->mb_regions[reg_idx].end)) {
				fault_region->mb_regions[reg_idx].end = end;
				if (msvdx_ec_ctx->cur_frame_info) {
					msvdx_ec_ctx->cur_frame_info->decode_status.mb_regions[reg_idx].end = end;
				}
			}
			else {
				reg_idx = fault_region->num_region++;
				if (unlikely(reg_idx >=
					MAX_SLICES_PER_PICTURE)) {
					PSB_DEBUG_MSVDX(
						"too many fault regions\n");
					break;
				}
				fault_region->mb_regions[reg_idx].start = start;
				fault_region->mb_regions[reg_idx].end = end;
				if (msvdx_ec_ctx->cur_frame_info) {
					msvdx_ec_ctx->cur_frame_info->decode_status.num_region = fault_region->num_region;
					msvdx_ec_ctx->cur_frame_info->decode_status.mb_regions[reg_idx].start = start;
					msvdx_ec_ctx->cur_frame_info->decode_status.mb_regions[reg_idx].end = end;
				}
			}
		} else {
			fault_region->num_region++;
			fault_region->mb_regions[0].start = start;
			fault_region->mb_regions[0].end = end;
			if (msvdx_ec_ctx->cur_frame_info) {
				msvdx_ec_ctx->cur_frame_info->decode_status.num_region = fault_region->num_region;
				msvdx_ec_ctx->cur_frame_info->decode_status.mb_regions[0].start = start;
				msvdx_ec_ctx->cur_frame_info->decode_status.mb_regions[0].end = end;
			}
		}

		break;

	}

	case MTX_MSGID_DEBLOCK_REQUIRED: {
		struct fw_deblock_required_msg *deblock_required_msg =
					(struct fw_deblock_required_msg *)buf;
		uint32_t fence;

		fence = deblock_required_msg->header.bits.msg_fence;
		PSB_DEBUG_GENERAL(
		    "MSVDX: MTX_MSGID_DEBLOCK_REQUIRED Fence=%08x\n", fence);


		struct psb_msvdx_ec_ctx *msvdx_ec_ctx = NULL;
		int found = 0;
		PSB_DEBUG_MSVDX("Get deblock required msg for ec\n");
		for (i = 0; i < PSB_MAX_EC_INSTANCE; i++)
			if (msvdx_priv->msvdx_ec_ctx[i]->fence
						== (fence & (~0xf))) {
				msvdx_ec_ctx =
					msvdx_priv->msvdx_ec_ctx[i];
				found++;
			}
		/* if found > 1, fence wrapping happens */
		if (!msvdx_ec_ctx ||
		    !(msvdx_ec_ctx->tfile) || found > 1) {
			PSB_DEBUG_MSVDX(
		"no matched ctx: fence 0x%x, found %d, ctx 0x%08x\n",
				fence, found, msvdx_ec_ctx);
			PSB_WMSVDX32(0, MSVDX_CMDS_END_SLICE_PICTURE_OFFSET + MSVDX_CMDS_BASE);
			PSB_WMSVDX32(1, MSVDX_CMDS_END_SLICE_PICTURE_OFFSET + MSVDX_CMDS_BASE);
			goto done;
		}

		msvdx_ec_ctx->cur_frame_info->fw_status = 1;
		msvdx_priv->cur_msvdx_ec_ctx = msvdx_ec_ctx;

		/*do error concealment with hw*/
		schedule_work(&msvdx_priv->ec_work);
		break;
	}
#ifdef CONFIG_SLICE_HEADER_PARSING
	/* extract done msg didn't return the msg id, which is not reasonable */
	case MTX_MSGID_SLICE_HEADER_EXTRACT_DONE: {
		struct fw_slice_header_extract_done_msg *extract_done_msg =
			(struct fw_slice_header_extract_done_msg *)buf;

		PSB_DEBUG_GENERAL("MSVDX:FW_VA_NALU_EXTRACT_DONE: "
			"extract msg size: %d, extract msg type is: %d.\n",
			extract_done_msg->header.bits.msg_size,
			extract_done_msg->header.bits.msg_type);
		break;
	}
#endif
	default:
		DRM_ERROR("ERROR: msvdx Unknown message from MTX, ID:0x%08x\n", header->bits.msg_type);
		goto done;
	}

done:
	PSB_DEBUG_GENERAL("MSVDX Interrupt: finish process a message\n");
	if (ridx != widx) {
		PSB_DEBUG_GENERAL("MSVDX Interrupt: there are more message to be read\n");
		goto loop;
	}

	if (drm_msvdx_pmpolicy == PSB_PMPOLICY_NOPM ||
			(IS_MDFLD(dev) && (msvdx_priv->msvdx_busy)) ||
			(IS_MRFLD(dev) && !cmd_complete)) {
		DRM_MEMORYBARRIER();	/* TBD check this... */
		return;
	}

	/* we get a frame/slice done, try to save some power */
	PSB_DEBUG_PM("MSVDX: schedule bottom half to\n"
		"suspend msvdx, current sequence is 0x%x.\n",
		msvdx_priv->msvdx_current_sequence);

	if (drm_msvdx_bottom_half == PSB_BOTTOM_HALF_WQ)
		schedule_delayed_work(
			&msvdx_priv->msvdx_suspend_wq, 0);
	else if (drm_msvdx_bottom_half == PSB_BOTTOM_HALF_TQ)
		tasklet_hi_schedule(&msvdx_priv->msvdx_tasklet);
	else
		PSB_DEBUG_PM("MSVDX: Unknown Bottom Half\n");

	DRM_MEMORYBARRIER();	/* TBD check this... */
}

/*
 * MSVDX interrupt.
 */
int psb_msvdx_interrupt(void *pvData)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct msvdx_private *msvdx_priv;
	uint32_t msvdx_stat;
	struct saved_history_record *precord = NULL;

	if (pvData == NULL) {
		DRM_ERROR("ERROR: msvdx %s, Invalid params\n", __func__);
		return -EINVAL;
	}

	dev = (struct drm_device *)pvData;

	dev_priv = psb_priv(dev);

	msvdx_priv = dev_priv->msvdx_private;
#ifndef CONFIG_DRM_VXD_BYT
	msvdx_priv->msvdx_hw_busy = REG_READ(0x20D0) & (0x1 << 9);
#endif
	msvdx_stat = PSB_RMSVDX32(MSVDX_INTERRUPT_STATUS_OFFSET);

	precord = get_new_history_record();
	if (precord) {
		precord->type = 4;
		precord->record_value.msvdx_stat = msvdx_stat;
	}

	/* driver only needs to handle mtx irq
	 * For MMU fault irq, there's always a HW PANIC generated
	 * if HW/FW is totally hang, the lockup function will handle
	 * the reseting
	 */
	if (msvdx_stat & MSVDX_INTERRUPT_STATUS_MMU_FAULT_IRQ_MASK) {
		/*Ideally we should we should never get to this */
		PSB_DEBUG_IRQ("MSVDX:MMU Fault:0x%x\n", msvdx_stat);

		/* Pause MMU */
		PSB_WMSVDX32(MSVDX_MMU_CONTROL0_MMU_PAUSE_MASK,
			     MSVDX_MMU_CONTROL0_OFFSET);
		DRM_WRITEMEMORYBARRIER();

		/* Clear this interupt bit only */
		PSB_WMSVDX32(MSVDX_INTERRUPT_STATUS_MMU_FAULT_IRQ_MASK,
			     MSVDX_INTERRUPT_CLEAR_OFFSET);
		PSB_RMSVDX32(MSVDX_INTERRUPT_CLEAR_OFFSET);
		DRM_READMEMORYBARRIER();

		msvdx_priv->msvdx_needs_reset = 1;
	} else if (msvdx_stat & MSVDX_INTERRUPT_STATUS_MTX_IRQ_MASK) {
		PSB_DEBUG_IRQ
			("MSVDX: msvdx_stat: 0x%x(MTX)\n", msvdx_stat);

		/* Clear all interupt bits */
		if (msvdx_priv->fw_loaded_by_punit)
			PSB_WMSVDX32(MSVDX_INTERRUPT_STATUS_MTX_IRQ_MASK,
				     MSVDX_INTERRUPT_CLEAR_OFFSET);
		else
			PSB_WMSVDX32(0xffff, MSVDX_INTERRUPT_CLEAR_OFFSET);

		PSB_RMSVDX32(MSVDX_INTERRUPT_CLEAR_OFFSET);
		DRM_READMEMORYBARRIER();

		psb_msvdx_mtx_interrupt(dev);
	}

	return 0;
}

#if 0
void psb_msvdx_lockup(struct drm_psb_private *dev_priv,
		      int *msvdx_lockup, int *msvdx_idle)
{
	int diff;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	*msvdx_lockup = 0;
	*msvdx_idle = 1;

	PSB_DEBUG_GENERAL("MSVDXTimer: current_sequence:%d "
			  "last_sequence:%d and last_submitted_sequence :%d\n",
			  msvdx_priv->msvdx_current_sequence,
			  msvdx_priv->msvdx_last_sequence,
			  dev_priv->sequence[PSB_ENGINE_DECODE]);

	diff = msvdx_priv->msvdx_current_sequence -
	       dev_priv->sequence[PSB_ENGINE_DECODE];

	if (diff > 0x0FFFFFFF) {
		if (msvdx_priv->msvdx_current_sequence ==
		    msvdx_priv->msvdx_last_sequence) {
			DRM_ERROR("MSVDXTimer:locked-up for sequence:%d\n",
				  msvdx_priv->msvdx_current_sequence);
			*msvdx_lockup = 1;
		} else {
			PSB_DEBUG_GENERAL("MSVDXTimer: "
					  "msvdx responded fine so far\n");
			msvdx_priv->msvdx_last_sequence =
				msvdx_priv->msvdx_current_sequence;
			*msvdx_idle = 0;
		}
	}
}
#endif

int psb_check_msvdx_idle(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	uint32_t loop, ret;

	if (msvdx_priv->fw_loaded_by_punit && msvdx_priv->rendec_init == 0)
		return 0;

	if (!msvdx_priv->fw_loaded_by_punit && msvdx_priv->msvdx_fw_loaded == 0)
		return 0;

	if (msvdx_priv->msvdx_busy) {
		PSB_DEBUG_PM("MSVDX: msvdx_busy was set, return busy.\n");
		return -EBUSY;
	}

	if (msvdx_priv->fw_loaded_by_punit) {
		if (!(PSB_RMSVDX32(MSVDX_COMMS_FW_STATUS) &
					MSVDX_FW_STATUS_HW_IDLE)) {
			PSB_DEBUG_PM("MSVDX_COMMS_SIGNATURE reg is 0x%x,\n"
				"MSVDX_COMMS_FW_STATUS reg is 0x%x,\n"
				"indicate hw is busy.\n",
				PSB_RMSVDX32(MSVDX_COMMS_SIGNATURE),
				PSB_RMSVDX32(MSVDX_COMMS_FW_STATUS));
			return -EBUSY;
		}
	}

	/* on some cores below 50502, there is one instance that
	 * read requests may not go to zero is in the case of a page fault,
	 * check core revision by reg MSVDX_CORE_REV, 385 core is 0x20001
	 * check if mmu page fault happend by reg MSVDX_INTERRUPT_STATUS,
	 * check was it a page table rather than a protection fault
	 * by reg MSVDX_MMU_STATUS, for such case,
	 * need call psb_msvdx_core_reset as the work around */
	if ((PSB_RMSVDX32(MSVDX_CORE_REV_OFFSET) < 0x00050502) &&
		(PSB_RMSVDX32(MSVDX_INTERRUPT_STATUS_OFFSET)
			& MSVDX_INTERRUPT_STATUS_MMU_FAULT_IRQ_MASK) &&
		(PSB_RMSVDX32(MSVDX_MMU_STATUS_OFFSET) & 1)) {
		PSB_DEBUG_WARN("mmu page fault, recover by core_reset.\n");
		return 0;
	}

	/* check MSVDX_MMU_MEM_REQ to confirm there's no memory requests */
	for (loop = 0; loop < 10; loop++)
		ret = psb_wait_for_register(dev_priv,
					MSVDX_MMU_MEM_REQ_OFFSET,
					0, 0xff, 100, 1);
	if (ret) {
		PSB_DEBUG_WARN("MSVDX: MSVDX_MMU_MEM_REQ reg is 0x%x,\n"
				"indicate mem busy, prevent power off vxd,"
				"MSVDX_COMMS_FW_STATUS reg is 0x%x,"
				"MSVDX_COMMS_ERROR_TRIG reg is 0x%x,",
				PSB_RMSVDX32(MSVDX_MMU_MEM_REQ_OFFSET),
				PSB_RMSVDX32(MSVDX_COMMS_FW_STATUS),
				PSB_RMSVDX32(MSVDX_COMMS_ERROR_TRIG));
#ifdef CONFIG_MDFD_GL3
		PSB_DEBUG_WARN("WARN: gl3 state is %d, 0 is off, 1 is on,\n"
				"gl3 MDFLD_GCL_CR_CTL2 reg is 0x%x,"
				"gl3 MDFLD_GCL_ERR_ADDR reg is 0x%x,"
				"gl3 MDFLD_GCL_ERR_STATUS reg is 0x%x,"
				"gl3 MDFLD_GCL_CR_ECO reg is 0x%x,"
				"gl3 MDFLD_GL3_CONTROL reg is 0x%x,"
				"gl3 MDFLD_GL3_USE_WRT_INVAL reg is 0x%x,"
				"gl3 MDFLD_GL3_STATUS reg is 0x%x.\n",
				psb_get_power_state(OSPM_GL3_CACHE_ISLAND),
				MDFLD_GL3_READ(MDFLD_GCL_CR_CTL2),
				MDFLD_GL3_READ(MDFLD_GCL_ERR_ADDR),
				MDFLD_GL3_READ(MDFLD_GCL_ERR_STATUS),
				MDFLD_GL3_READ(MDFLD_GCL_CR_ECO),
				MDFLD_GL3_READ(MDFLD_GL3_CONTROL),
				MDFLD_GL3_READ(MDFLD_GL3_USE_WRT_INVAL),
				MDFLD_GL3_READ(MDFLD_GL3_STATUS));
#endif
		return -EBUSY;
	}
	/*
		if (msvdx_priv->msvdx_hw_busy) {
			PSB_DEBUG_PM("MSVDX: %s, HW is busy\n", __func__);
			return -EBUSY;
		}
	*/
	return 0;
}

int psb_msvdx_save_context(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int offset;
	int need_sw_reset;

	need_sw_reset = msvdx_priv->msvdx_needs_reset &
			MSVDX_RESET_NEEDS_REUPLOAD_FW;

	if (msvdx_priv->fw_loaded_by_punit)
		msvdx_priv->msvdx_needs_reset = MSVDX_RESET_NEEDS_INIT_FW;
	else
		msvdx_priv->msvdx_needs_reset = 1;

#ifdef CONFIG_VIDEO_MRFLD_EC
	/* we should restore the state, if we power down/up during EC */
	for (offset = 0; offset < 4; ++offset)
		msvdx_priv->vec_ec_mem_data[offset] =
			PSB_RMSVDX32(0x2cb0 + offset * 4);

	msvdx_priv->vec_ec_mem_data[4] =
		PSB_RMSVDX32(0x2cc4);

	msvdx_priv->vec_ec_mem_saved = 1;
	PSB_DEBUG_MSVDX("ec last mb %d %d %d %d\n", msvdx_priv->vec_ec_mem_data[0],
				msvdx_priv->vec_ec_mem_data[1],
				msvdx_priv->vec_ec_mem_data[2],
				msvdx_priv->vec_ec_mem_data[3]);
	PSB_DEBUG_MSVDX("ec error state %d\n", msvdx_priv->vec_ec_mem_data[4]);
#endif

	if (need_sw_reset) {
		PSB_DEBUG_WARN("msvdx run into wrong state, soft reset msvdx before power down\n");
		PSB_WMSVDX32(MTX_SOFT_RESET_MTXRESET, MTX_SOFT_RESET_OFFSET);

		if (psb_msvdx_core_reset(dev_priv))
			PSB_DEBUG_WARN("failed to call psb_msvdx_core_reset.\n");

		if (msvdx_priv->fw_loaded_by_punit) {
			PSB_WMSVDX32(0, MTX_ENABLE_OFFSET);
			psb_msvdx_mtx_set_clocks(dev_priv->dev, 0);
		}
	}

	return 0;
}

int psb_msvdx_restore_context(struct drm_device *dev)
{
	return 0;
}

void psb_msvdx_check_reset_fw(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	unsigned long irq_flags;

	spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);

	/* handling fw upload here if required */
	/* power off first, then hw_begin will power up/upload FW correctly */
	if (msvdx_priv->msvdx_needs_reset & MSVDX_RESET_NEEDS_REUPLOAD_FW) {
		spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
		PSB_DEBUG_PM("MSVDX: force to power off msvdx due to decoding error.\n");
		ospm_apm_power_down_msvdx(dev, 1);
		spin_lock_irqsave(&msvdx_priv->msvdx_lock, irq_flags);
		msvdx_priv->msvdx_needs_reset &= ~MSVDX_RESET_NEEDS_REUPLOAD_FW;
	}
	spin_unlock_irqrestore(&msvdx_priv->msvdx_lock, irq_flags);
}

static void psb_msvdx_set_tile(struct drm_device *dev, unsigned long msvdx_tile)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	uint32_t cmd, msvdx_stride;
	uint32_t start = msvdx_priv->tile_region_start0;
	uint32_t end = msvdx_priv->tile_region_end0;
	msvdx_stride = (msvdx_tile & 0xf);
	/* Enable memory tiling */
	cmd = ((start >> 20) + (((end >> 20) - 1) << 12) +
				((0x8 | (msvdx_stride - 1)) << 24));
	if (msvdx_stride) {
		PSB_DEBUG_GENERAL("MSVDX: MMU Tiling register0 %08x\n", cmd);
		PSB_DEBUG_GENERAL("       Region 0x%08x-0x%08x\n",
					start, end);
		PSB_WMSVDX32(cmd, MSVDX_MMU_TILE_BASE0_OFFSET);
	}

	start = msvdx_priv->tile_region_start1;
	end = msvdx_priv->tile_region_end1;

	msvdx_stride = (msvdx_tile >> 4);
	/* Enable memory tiling */
	PSB_WMSVDX32(0, MSVDX_MMU_TILE_BASE1_OFFSET);
	cmd = ((start >> 20) + (((end >> 20) - 1) << 12) +
				((0x8 | (msvdx_stride - 1)) << 24));
	if (msvdx_stride) {
		PSB_DEBUG_GENERAL("MSVDX: MMU Tiling register1 %08x\n", cmd);
		PSB_DEBUG_GENERAL("       Region 0x%08x-0x%08x\n",
					start, end);
		PSB_WMSVDX32(cmd, MSVDX_MMU_TILE_BASE1_OFFSET);
	}
}

void psb_powerdown_msvdx(struct work_struct *work)
{
	struct msvdx_private *msvdx_priv =
		container_of(work, struct msvdx_private, msvdx_suspend_wq.work);

	PSB_DEBUG_PM("MSVDX: work queue is scheduled to power off msvdx.\n");
	ospm_apm_power_down_msvdx(msvdx_priv->dev, 0);
}

void msvdx_powerdown_tasklet(unsigned long data)
{
	struct drm_device *dev = (struct drm_device *)data;

	PSB_DEBUG_PM("MSVDX: tasklet is scheduled to power off msvdx.\n");
	ospm_apm_power_down_msvdx(dev, 0);
}

void psb_msvdx_mtx_set_clocks(struct drm_device *dev, uint32_t clock_state)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t old_clock_state = 0;
	/* PSB_DEBUG_MSVDX("SetClocks to %x.\n", clock_state); */
	old_clock_state = PSB_RMSVDX32(MSVDX_MAN_CLK_ENABLE_OFFSET);
	if (old_clock_state == clock_state)
		return;

	if (clock_state == 0) {
		/* Turn off clocks procedure */
		if (old_clock_state) {
			/* Turn off all the clocks except core */
			PSB_WMSVDX32(
				MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK,
				MSVDX_MAN_CLK_ENABLE_OFFSET);

			/* Make sure all the clocks are off except core */
			psb_wait_for_register(dev_priv,
				MSVDX_MAN_CLK_ENABLE_OFFSET,
				MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK,
				0xffffffff, 2000000, 5);

			/* Turn off core clock */
			PSB_WMSVDX32(0, MSVDX_MAN_CLK_ENABLE_OFFSET);
		}
	} else {
		uint32_t clocks_en = clock_state;

		/*Make sure that core clock is not accidentally turned off */
		clocks_en |= MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK;

		/* If all clocks were disable do the bring up procedure */
		if (old_clock_state == 0) {
			/* turn on core clock */
			PSB_WMSVDX32(
				MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK,
				MSVDX_MAN_CLK_ENABLE_OFFSET);

			/* Make sure core clock is on */
			psb_wait_for_register(dev_priv,
				MSVDX_MAN_CLK_ENABLE_OFFSET,
				MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK,
				0xffffffff, 2000000, 5);

			/* turn on the other clocks as well */
			PSB_WMSVDX32(clocks_en, MSVDX_MAN_CLK_ENABLE_OFFSET);

			/* Make sure that all they are on */
			psb_wait_for_register(dev_priv,
					MSVDX_MAN_CLK_ENABLE_OFFSET,
					clocks_en, 0xffffffff, 2000000, 5);
		} else {
			PSB_WMSVDX32(clocks_en, MSVDX_MAN_CLK_ENABLE_OFFSET);

			/* Make sure that they are on */
			psb_wait_for_register(dev_priv,
					MSVDX_MAN_CLK_ENABLE_OFFSET,
					clocks_en, 0xffffffff, 2000000, 5);
		}
	}
}

void psb_msvdx_clearirq(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	unsigned long mtx_int = 0;

	/* Clear MTX interrupt */
	REGIO_WRITE_FIELD_LITE(mtx_int, MSVDX_INTERRUPT_STATUS, MTX_IRQ,
			       1);
	PSB_WMSVDX32(mtx_int, MSVDX_INTERRUPT_CLEAR_OFFSET);
}

/* following two functions also works for CLV and MFLD */
/* PSB_INT_ENABLE_R is set in psb_irq_(un)install_islands */
void psb_msvdx_disableirq(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	/*uint32_t ier = dev_priv->vdc_irq_mask & (~_PSB_IRQ_MSVDX_FLAG); */

	unsigned long enables = 0;

	REGIO_WRITE_FIELD_LITE(enables, MSVDX_INTERRUPT_STATUS, MTX_IRQ,
			       0);
	PSB_WMSVDX32(enables, MSVDX_HOST_INTERRUPT_ENABLE_OFFSET);

	/* write in sysirq.c */
	/* PSB_WVDC32(ier, PSB_INT_ENABLE_R); /\* essential *\/ */
}

void psb_msvdx_enableirq(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	/* uint32_t ier = dev_priv->vdc_irq_mask | _PSB_IRQ_MSVDX_FLAG; */
	unsigned long enables = 0;

	/* Only enable the master core IRQ*/
	REGIO_WRITE_FIELD_LITE(enables, MSVDX_INTERRUPT_STATUS, MTX_IRQ,
			       1);
	PSB_WMSVDX32(enables, MSVDX_HOST_INTERRUPT_ENABLE_OFFSET);

	/* write in sysirq.c */
	/* PSB_WVDC32(ier, PSB_INT_ENABLE_R); /\* essential *\/ */
}

#ifdef CONFIG_SLICE_HEADER_PARSING
int psb_allocate_term_buf(struct drm_device *dev,
			    struct ttm_buffer_object **term_buf,
			    uint32_t *base_addr, unsigned long size)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_bo_device *bdev = &dev_priv->bdev;
	int ret;
	struct ttm_bo_kmap_obj tmp_kmap;
	bool is_iomem;
	unsigned char *addr;
	const unsigned char term_string[] = {
		0x0, 0x0, 0x1, 0xff, 0x65, 0x6e, 0x64, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	PSB_DEBUG_INIT("MSVDX: allocate termination buffer.\n");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_buffer_object_create(bdev, size,
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU |
				       TTM_PL_FLAG_NO_EVICT, 0, 0, 0,
				       NULL, term_buf);
#else
	ret = ttm_buffer_object_create(bdev, size,
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU |
				       TTM_PL_FLAG_NO_EVICT, 0, 0,
				       NULL, term_buf);
#endif
	if (ret) {
		DRM_ERROR("MSVDX:failed to allocate termination buffer.\n");
		*term_buf = NULL;
		return 1;
	}

	ret = ttm_bo_kmap(*term_buf, 0, (*term_buf)->num_pages, &tmp_kmap);
	if (ret) {
		PSB_DEBUG_GENERAL("ttm_bo_kmap failed ret: %d\n", ret);
		ttm_bo_unref(term_buf);
		*term_buf = NULL;
		return 1;
	}

	addr = (unsigned char *)ttm_kmap_obj_virtual(&tmp_kmap, &is_iomem);
	memcpy(addr, term_string, TERMINATION_SIZE);
	ttm_bo_kunmap(&tmp_kmap);

	*base_addr = (*term_buf)->offset;
	return 0;
}

static int psb_msvdx_protected_frame_finished(struct drm_psb_private *dev_priv, struct psb_video_ctx *pos, uint32_t fence)
{
	int is_protected = 0;

	if (unlikely(pos == NULL)) {
		return 1;
	}

	if (!pos->slice_extract_flag)
		return 1;

	PSB_DEBUG_GENERAL("end_frame_seq, 0x%08x, fence = 0x%x\n", pos->frame_end_seq, fence);
	if (pos->frame_end_seq == (fence & ~0xf))
		return 1;
	return 0;
}
#endif

static struct psb_video_ctx* psb_msvdx_find_ctx(struct drm_psb_private *dev_priv, uint32_t fence)
{
	struct psb_video_ctx *pos = NULL, *n = NULL;

	if (unlikely(dev_priv == NULL)) {
		return NULL;
	}

	spin_lock(&dev_priv->video_ctx_lock);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (pos->cur_sequence == fence) {
			spin_unlock(&dev_priv->video_ctx_lock);
			return pos;
		}
	}
	spin_unlock(&dev_priv->video_ctx_lock);

	return NULL;
}

static void psb_msvdx_fw_error_detected(struct drm_device *dev, uint32_t fence, uint32_t flags)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	struct psb_msvdx_ec_ctx *msvdx_ec_ctx = NULL;
	drm_psb_msvdx_frame_info_t *frame_info = NULL;
	int found = 0;
	int i;

	if (!(flags & FW_DEVA_ERROR_DETECTED))
		return;

	/*get the frame_info struct for error concealment frame*/
	for (i = 0; i < PSB_MAX_EC_INSTANCE; i++)
		if (msvdx_priv->msvdx_ec_ctx[i]->fence ==
						(fence & (~0xf))) {
			msvdx_ec_ctx = msvdx_priv->msvdx_ec_ctx[i];
			found++;
		}
	/* psb_msvdx_mtx_message_dump(dev); */
	if (!msvdx_ec_ctx || !(msvdx_ec_ctx->tfile) || found > 1) {
		PSB_DEBUG_MSVDX(
		"no matched ctx: fence 0x%x, found %d, ctx 0x%08x\n",
			fence, found, msvdx_ec_ctx);
		return;
	}

	if (msvdx_ec_ctx->cur_frame_info &&
		msvdx_ec_ctx->cur_frame_info->fence == (fence & (~0xf))) {
		frame_info = msvdx_ec_ctx->cur_frame_info;
	} else {
		if (msvdx_ec_ctx->cur_frame_info) {
			PSB_DEBUG_MSVDX(
			"cur_frame_info's fence(%x) doesn't match fence (%x)\n",
				msvdx_ec_ctx->cur_frame_info->fence, fence);
		} else	{
			PSB_DEBUG_MSVDX(
			"The pointer msvdx_ec_ctx->cur_frame_info is null\n");
		}
		return;
	}

	if (frame_info->decode_status.num_region) {
		PSB_DEBUG_MSVDX( "Error already recorded, no need to recorded again\n");
		return;
	}

	PSB_DEBUG_MSVDX( "record error as first fault region\n");
	frame_info->decode_status.num_region++;
	frame_info->decode_status.mb_regions[0].start = 0;
	frame_info->decode_status.mb_regions[0].end = 0;

	/*
	for (i = 0; i < MAX_DECODE_BUFFERS; i++) {
		if (msvdx_ec_ctx->frame_info[i].fence == (fence & (~0xf))) {
			break;
		}

	}
	*/
}
