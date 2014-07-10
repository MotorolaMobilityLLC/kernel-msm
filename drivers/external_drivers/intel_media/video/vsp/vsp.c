/**************************************************************************
 *
 * Copyright (c) 2007 Intel Corporation, Hillsboro, OR, USA
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
#include <linux/math64.h>

#include "psb_drv.h"
#include "psb_drm.h"
#include "vsp.h"
#include "ttm/ttm_execbuf_util.h"
#include "vsp_fw.h"
#include "pwr_mgmt.h"

#define PARTITIONS_MAX 9

#define REF_FRAME_LAST	0
#define REF_FRAME_ALT	1
#define REF_FRAME_GOLD	2

static int vsp_submit_cmdbuf(struct drm_device *dev,
			     struct file *filp,
			     unsigned char *cmd_start,
			     unsigned long cmd_size);
static int vsp_send_command(struct drm_device *dev,
			    struct file *filp,
			    unsigned char *cmd_start,
			    unsigned long cmd_size);
static int vsp_prehandle_command(struct drm_file *priv,
			    struct list_head *validate_list,
			    uint32_t fence_type,
			    struct drm_psb_cmdbuf_arg *arg,
			    unsigned char *cmd_start,
			    struct psb_ttm_fence_rep *fence_arg);
static int vsp_fence_vpp_surfaces(struct drm_file *priv,
			      struct list_head *validate_list,
			      uint32_t fence_type,
			      struct drm_psb_cmdbuf_arg *arg,
			      struct psb_ttm_fence_rep *fence_arg,
			      struct ttm_buffer_object *pic_param_bo);
static void handle_error_response(unsigned int error_type,
				unsigned int cmd_type);
static int vsp_fence_vp8enc_surfaces(struct drm_file *priv,
				struct list_head *validate_list,
				uint32_t fence_type,
				struct drm_psb_cmdbuf_arg *arg,
				struct psb_ttm_fence_rep *fence_arg,
				struct ttm_buffer_object *pic_param_bo);
static int vsp_fence_compose_surfaces(struct drm_file *priv,
				struct list_head *validate_list,
				uint32_t fence_type,
				struct drm_psb_cmdbuf_arg *arg,
				struct psb_ttm_fence_rep *fence_arg,
				struct ttm_buffer_object *pic_param_bo);

static inline void psb_clflush(void *addr)
{
	__asm__ __volatile__("wbinvd ");
}

int vsp_handle_response(struct drm_psb_private *dev_priv)
{
	struct vsp_private *vsp_priv = dev_priv->vsp_private;

	int ret = 0;
	unsigned int rd, wr;
	unsigned int idx;
	unsigned int msg_num;
	struct vss_response_t *msg;
	uint32_t sequence;
	uint32_t status;
	unsigned int cmd_rd, cmd_wr;

	idx = 0;
	sequence = vsp_priv->current_sequence;
	while (1) {
		rd = vsp_priv->ctrl->ack_rd;
		wr = vsp_priv->ctrl->ack_wr;
		msg_num = wr > rd ? wr - rd : wr == rd ? 0 :
			VSP_ACK_QUEUE_SIZE - (rd - wr);
		VSP_DEBUG("ack rd %d wr %d, msg_num %d, size %d\n",
			  rd, wr, msg_num, VSP_ACK_QUEUE_SIZE);

		if (msg_num == 0)
			break;
		else if ( msg_num < 0) {
			DRM_ERROR("invalid msg num, exit\n");
			break;
		}

		msg = vsp_priv->ack_queue + (idx + rd) % VSP_ACK_QUEUE_SIZE;
		VSP_DEBUG("ack[%d]->type = %x\n", idx, msg->type);

		switch (msg->type) {
		case VssErrorResponse:
			DRM_ERROR("error response:%.8x %.8x %.8x %.8x %.8x\n",
				  msg->context, msg->type, msg->buffer,
				  msg->size, msg->vss_cc);
			handle_error_response(msg->buffer & 0xFFFF,
					msg->buffer >> 16);

			cmd_rd = vsp_priv->ctrl->cmd_rd;
			cmd_wr = vsp_priv->ctrl->cmd_wr;

			if (msg->context != 0 && cmd_wr == cmd_rd) {
				vsp_priv->vp8_cmd_num = 0;
				sequence = vsp_priv->last_sequence;
			}

			ret = false;
			break;

		case VssEndOfSequenceResponse:
			PSB_DEBUG_GENERAL("end of the sequence received\n");
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;

			break;

		case VssOutputSurfaceReadyResponse:
			VSP_DEBUG("sequence %x is done!!\n", msg->buffer);
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			sequence = msg->buffer;

			break;

		case VssOutputSurfaceFreeResponse:
			VSP_DEBUG("sequence surface %x should be freed\n",
				  msg->buffer);
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			break;

		case VssOutputSurfaceCrcResponse:
			VSP_DEBUG("Crc of sequence %x is %x\n", msg->buffer,
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			break;

		case VssInputSurfaceReadyResponse:
			VSP_DEBUG("input surface ready\n");
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			break;

		case VssCommandBufferReadyResponse:
			VSP_DEBUG("command buffer ready\n");
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			break;

		case VssIdleResponse:
		{
			unsigned int cmd_rd, cmd_wr;

			VSP_DEBUG("VSP is idle\n");
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;

			cmd_rd = vsp_priv->ctrl->cmd_rd;
			cmd_wr = vsp_priv->ctrl->cmd_wr;
			VSP_DEBUG("cmd_rd=%d, cmd_wr=%d\n", cmd_rd, cmd_wr);

			if (vsp_priv->vsp_state == VSP_STATE_ACTIVE)
				vsp_priv->vsp_state = VSP_STATE_IDLE;
			break;
		}
		case VssVp8encSetSequenceParametersResponse:
			VSP_DEBUG("VSP clock cycles from pre response %x\n",
				  msg->vss_cc);
			vsp_priv->vss_cc_acc += msg->vss_cc;
			status = msg->buffer;
			switch (status) {
			case VssOK:
				VSP_DEBUG("vp8 sequence response received\n");
				break;
			default:
				VSP_DEBUG("Unknown VssStatus %x\n", status);
				DRM_ERROR("Invalid vp8 sequence response\n");
				break;
			}

			break;
		case VssVp8encEncodeFrameResponse:
		{
			sequence = msg->buffer;
			/* received VssVp8encEncodeFrameResponse indicates cmd has been handled */
			vsp_priv->vp8_cmd_num--;

			VSP_DEBUG("sequence %d\n", sequence);
			VSP_DEBUG("receive vp8 encoded frame response\n");

			break;
		}
		case VssWiDi_ComposeSetSequenceParametersResponse:
		{
			VSP_DEBUG("The Compose respose value is %d\n", msg->buffer);
			break;
		}
		case VssWiDi_ComposeFrameResponse:
		{
			VSP_DEBUG("Compose sequence %x is done!!\n", msg->buffer);
			sequence = msg->buffer;
			break;
		}
		default:
			DRM_ERROR("VSP: Unknown response type %x\n",
				  msg->type);
			DRM_ERROR("VSP: there're %d response remaining\n",
				  msg_num - idx - 1);
			ret = false;
			break;
		}

		vsp_priv->ctrl->ack_rd = (vsp_priv->ctrl->ack_rd + 1) %
			VSP_ACK_QUEUE_SIZE;
	}

	return sequence;
}

bool vsp_interrupt(void *pvData)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct vsp_private *vsp_priv;
	unsigned long status;
	bool ret = true;

	VSP_DEBUG("got vsp interrupt\n");

	if (pvData == NULL) {
		DRM_ERROR("VSP: vsp %s, Invalid params\n", __func__);
		return false;
	}

	dev = (struct drm_device *)pvData;
	dev_priv = (struct drm_psb_private *) dev->dev_private;
	vsp_priv = dev_priv->vsp_private;

	/* read interrupt status */
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_STATUS, &status);
	VSP_DEBUG("irq status %lx\n", status);

	/* clear interrupt status */
	if (!(status & (1 << VSP_SP0_IRQ_SHIFT))) {
		DRM_ERROR("VSP: invalid irq\n");
		return false;
	} else {
		IRQ_REG_WRITE32(VSP_IRQ_CTRL_IRQ_CLR, (1 << VSP_SP0_IRQ_SHIFT));
		/* we need to clear IIR VSP bit */
		PSB_WVDC32(_TNG_IRQ_VSP_FLAG, PSB_INT_IDENTITY_R);
		(void)PSB_RVDC32(PSB_INT_IDENTITY_R);
	}

	schedule_delayed_work(&vsp_priv->vsp_irq_wq, 0);

	VSP_DEBUG("will leave interrupt\n");
	return ret;
}

int vsp_cmdbuf_vpp(struct drm_file *priv,
		    struct list_head *validate_list,
		    uint32_t fence_type,
		    struct drm_psb_cmdbuf_arg *arg,
		    struct ttm_buffer_object *cmd_buffer,
		    struct psb_ttm_fence_rep *fence_arg)
{
	struct drm_device *dev = priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	int ret = 0;
	unsigned char *cmd_start;
	unsigned long cmd_page_offset = arg->cmdbuf_offset & ~PAGE_MASK;
	struct ttm_bo_kmap_obj cmd_kmap;
	bool is_iomem;
	struct file *filp = priv->filp;
	bool need_power_put = 0;

	memset(&cmd_kmap, 0, sizeof(cmd_kmap));
	vsp_priv->vsp_cmd_num = 1;

	/* check command buffer parameter */
	if ((arg->cmdbuf_offset > cmd_buffer->acc_size) ||
	    (arg->cmdbuf_size > cmd_buffer->acc_size) ||
	    (arg->cmdbuf_size + arg->cmdbuf_offset) > cmd_buffer->acc_size) {
		DRM_ERROR("VSP: the size of cmdbuf is invalid!");
		DRM_ERROR("VSP: offset=%x, size=%x,cmd_buffer size=%zx\n",
			  arg->cmdbuf_offset, arg->cmdbuf_size,
			  cmd_buffer->acc_size);
		vsp_priv->vsp_cmd_num = 0;
		ret = -EFAULT;
		goto out_err;
	}

	VSP_DEBUG("map command first\n");
	ret = ttm_bo_kmap(cmd_buffer, arg->cmdbuf_offset >> PAGE_SHIFT, 2,
			  &cmd_kmap);
	if (ret) {
		DRM_ERROR("VSP: ttm_bo_kmap failed: %d\n", ret);
		vsp_priv->vsp_cmd_num = 0;
		goto out_err;
	}

	cmd_start = (unsigned char *) ttm_kmap_obj_virtual(&cmd_kmap,
			&is_iomem) + cmd_page_offset;

	/* handle the Context and Fence command */
	VSP_DEBUG("handle Context and Fence commands\n");
	ret = vsp_prehandle_command(priv, validate_list, fence_type, arg,
			       cmd_start, fence_arg);
	if (ret)
		goto out;

	if (time_after(jiffies, vsp_priv->cmd_submit_time + HZ * 50 / 1000)) {
		VSP_DEBUG(" will be force to flush cmd due to jiffies\n");
		vsp_priv->force_flush_cmd = 1;
	}

	if (drm_vsp_vpp_batch_cmd == 0)
		vsp_priv->force_flush_cmd = 1;

	if (vsp_priv->vsp_state == VSP_STATE_IDLE)
		ospm_apm_power_down_vsp(dev);

	if (vsp_priv->acc_num_cmd >= 1 || vsp_priv->force_flush_cmd != 0
	    || vsp_priv->delayed_burst_cnt > 0) {
		if (power_island_get(OSPM_VIDEO_VPP_ISLAND) == false) {
			ret = -EBUSY;
			goto out_err1;
		}
		need_power_put = 1;
	}

	VSP_DEBUG("will submit command\n");
	ret = vsp_submit_cmdbuf(dev, filp, cmd_start, arg->cmdbuf_size);
	if (ret)
		goto out_err1;

out_err1:
	if (need_power_put)
		power_island_put(OSPM_VIDEO_VPP_ISLAND);
out:
	ttm_bo_kunmap(&cmd_kmap);

	spin_lock(&cmd_buffer->bdev->fence_lock);
	if (cmd_buffer->sync_obj != NULL)
		ttm_fence_sync_obj_unref(&cmd_buffer->sync_obj);
	spin_unlock(&cmd_buffer->bdev->fence_lock);

	vsp_priv->vsp_cmd_num = 0;
out_err:
	return ret;
}

int vsp_submit_cmdbuf(struct drm_device *dev,
		      struct file *filp,
		      unsigned char *cmd_start,
		      unsigned long cmd_size)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	int ret;

	/* If VSP timeout, don't send cmd to hardware anymore */
	if (vsp_priv->vsp_state == VSP_STATE_HANG) {
		DRM_ERROR("The VSP is hang abnormally!");
		return -EFAULT;
	}
	if (vsp_priv->acc_num_cmd >= 1 || vsp_priv->force_flush_cmd != 0
	    || vsp_priv->delayed_burst_cnt > 0) {
		/* consider to invalidate/flush MMU */
		if (vsp_priv->vsp_state == VSP_STATE_DOWN) {
			VSP_DEBUG("needs reset\n");

			if (vsp_reset(dev_priv)) {
				ret = -EBUSY;
				DRM_ERROR("VSP: failed to reset\n");
				return ret;
			}
		}

		if (vsp_priv->vsp_state == VSP_STATE_SUSPEND) {
			ret = vsp_resume_function(dev_priv);
			VSP_DEBUG("The VSP is on suspend, send resume!\n");
		}
	}

	/* submit command to HW */
	ret = vsp_send_command(dev, filp, cmd_start, cmd_size);
	if (ret != 0) {
		DRM_ERROR("VSP: failed to send command\n");
		return ret;
	}

#if 0
	/* If the VSP is in Suspend, need to send "Resume" */
	if (vsp_priv->vsp_state == VSP_STATE_SUSPEND) {
		ret = vsp_resume_function(dev_priv);
		VSP_DEBUG("The VSP is on suspend, send resume!\n");
	}
#endif
	return ret;
}

int vsp_send_command(struct drm_device *dev,
		     struct file *filp,
		     unsigned char *cmd_start,
		     unsigned long cmd_size)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	unsigned int rd, wr;
	unsigned int remaining_space;
	unsigned int cmd_idx, num_cmd = 0;
	struct vss_command_t *cur_cmd, *cur_cell_cmd;

	VSP_DEBUG("will send command here: cmd_start %p, cmd_size %ld\n",
		  cmd_start, cmd_size);

	cur_cmd = (struct vss_command_t *)cmd_start;


	/* if the VSP in suspend, update the saved config info */
	if (vsp_priv->vsp_state == VSP_STATE_SUSPEND) {
		VSP_DEBUG("In suspend, need update saved cmd_wr!\n");
		vsp_priv->ctrl = (struct vsp_ctrl_reg *)
				 &(vsp_priv->saved_config_regs[2]);
	}

	while (cmd_size) {
		rd = vsp_priv->ctrl->cmd_rd;
		wr = vsp_priv->ctrl->cmd_wr;

		remaining_space = rd >= wr + 1 ? rd - wr - 1 :
			VSP_CMD_QUEUE_SIZE - (wr + 1 - rd) ;
		remaining_space -= vsp_priv->acc_num_cmd;

		VSP_DEBUG("VSP: rd %d, wr %d, remaining_space %d, ",
			  rd, wr, remaining_space);
		VSP_DEBUG("cmd_size %ld sizeof(*cur_cmd) %zu\n",
			  cmd_size, sizeof(*cur_cmd));

		if (remaining_space < vsp_priv->vsp_cmd_num) {
			DRM_ERROR("no enough space for cmd queue\n");
			DRM_ERROR("VSP: rd %d, wr %d, remaining_space %d\n",
				  rd, wr, remaining_space);
			/* VP handle the data very slowly,
			* so we have to delay longer
			*/
#ifdef CONFIG_BOARD_MRFLD_VP
			udelay(1000);
#else
			udelay(10);
#endif
			continue;
		}

		for (cmd_idx = vsp_priv->acc_num_cmd; cmd_idx < remaining_space;) {
			VSP_DEBUG("current cmd type %x\n", cur_cmd->type);
			if (cur_cmd->type == VspFencePictureParamCommand) {
				VSP_DEBUG("skip VspFencePictureParamCommand");
				cur_cmd++;
				cmd_size -= sizeof(*cur_cmd);
				VSP_DEBUG("first cmd_size %ld\n", cmd_size);
				if (cmd_size == 0)
					goto out;
				else
					continue;
			} else if (cur_cmd->type == VspSetContextCommand ||
					cur_cmd->type == Vss_Sys_Ref_Frame_COMMAND) {
				VSP_DEBUG("skip Vss_Sys_Ref_Frame_COMMAND\n");
				cur_cmd++;

				cmd_size -= sizeof(*cur_cmd);
				if (cmd_size == 0)
					goto out;
				else
					continue;
			} else if (cur_cmd->type == VspFenceComposeCommand) {
				VSP_DEBUG("skip VspFenceComposeCommand\n");
				cur_cmd++;
				cmd_size -= sizeof(*cur_cmd);
				VSP_DEBUG("first cmd_size %ld\n", cmd_size);
				if (cmd_size == 0)
					goto out;
				else
					continue;
			} else if (cur_cmd->type == VssWiDi_ComposeFrameCommand) {
				/* save the fence value in buffer_id */
				cur_cmd->buffer_id = vsp_priv->compose_fence;
			}

			/* FIXME: could remove cmd_idx here */
			cur_cell_cmd = vsp_priv->cmd_queue +
				(wr + cmd_idx) % VSP_CMD_QUEUE_SIZE;
			++cmd_idx;

			memcpy(cur_cell_cmd, cur_cmd, sizeof(*cur_cmd));
			VSP_DEBUG("cmd: %.8x %.8x %.8x %.8x %.8x %.8x\n",
				cur_cell_cmd->context, cur_cell_cmd->type,
				cur_cell_cmd->buffer, cur_cell_cmd->size,
				cur_cell_cmd->buffer_id, cur_cell_cmd->irq);
			VSP_DEBUG("send %.8x cmd to VSP\n",
					cur_cell_cmd->type);

			num_cmd++;
			cur_cmd++;
			cmd_size -= sizeof(*cur_cmd);
			if (cmd_size == 0)
				goto out;
			else if (cmd_size < sizeof(*cur_cmd)) {
				DRM_ERROR("invalid command size %ld\n",
					  cmd_size);
				goto out;
			}
		}
	}
out:
	/* update write index */
	VSP_DEBUG("%d cmd will send to VSP!\n", num_cmd);
	if (vsp_priv->delayed_burst_cnt > 0)
		--vsp_priv->delayed_burst_cnt;

	vsp_priv->cmd_submit_time = jiffies;

	vsp_priv->acc_num_cmd += num_cmd;
	if (vsp_priv->acc_num_cmd > 1 || vsp_priv->force_flush_cmd != 0 ||
	    vsp_priv->delayed_burst_cnt > 0) {
		vsp_priv->ctrl->cmd_wr =
			(vsp_priv->ctrl->cmd_wr + vsp_priv->acc_num_cmd) %
			VSP_CMD_QUEUE_SIZE;
		vsp_priv->acc_num_cmd = 0;
		vsp_priv->force_flush_cmd = 0;
		cancel_delayed_work(&vsp_priv->vsp_cmd_submit_check_wq);
	} else {
		schedule_delayed_work(&vsp_priv->vsp_cmd_submit_check_wq, HZ);
	}

	return 0;
}

static int vsp_prehandle_command(struct drm_file *priv,
			    struct list_head *validate_list,
			    uint32_t fence_type,
			    struct drm_psb_cmdbuf_arg *arg,
			    unsigned char *cmd_start,
			    struct psb_ttm_fence_rep *fence_arg)
{
	struct ttm_object_file *tfile = BCVideoGetPriv(priv)->tfile;
	struct vss_command_t *cur_cmd;
	unsigned int cmd_size = arg->cmdbuf_size;
	int ret = 0;
	struct ttm_buffer_object *pic_param_bo = NULL;
	int pic_param_num, vsp_cmd_num = 0;
	struct ttm_validate_buffer *pos, *next;
	struct drm_device *dev = priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	struct ttm_buffer_object *pic_bo_vp8 = NULL;
	int vp8_pic_num = 0;
	struct ttm_buffer_object *compose_param_bo = NULL;
	int compose_param_num = 0;


	cur_cmd = (struct vss_command_t *)cmd_start;

	pic_param_num = 0;
	VSP_DEBUG("cmd size %d\n", cmd_size);
	while (cmd_size) {
		VSP_DEBUG("cmd type %x, buffer offset %x\n", cur_cmd->type,
			  cur_cmd->buffer);
		if (cur_cmd->type == VspFencePictureParamCommand) {
			pic_param_bo =
				ttm_buffer_object_lookup(tfile,
							 cur_cmd->buffer);
			if (pic_param_bo == NULL) {
				DRM_ERROR("VSP: failed to find %x bo\n",
					  cur_cmd->buffer);
				ret = -1;
				goto out;
			}
			pic_param_num++;
			VSP_DEBUG("find pic param buffer: id %x, offset %lx\n",
				  cur_cmd->buffer, pic_param_bo->offset);
			VSP_DEBUG("pic param placement %x bus.add %p\n",
				  pic_param_bo->mem.placement,
				  pic_param_bo->mem.bus.addr);
			if (pic_param_num > 1) {
				DRM_ERROR("pic_param_num invalid(%d)!\n",
					  pic_param_num);
				ret = -1;
				goto out;
			}
		} else if (cur_cmd->type == VspFenceComposeCommand) {
			compose_param_bo =
				ttm_buffer_object_lookup(tfile,
							 cur_cmd->buffer);
			if (compose_param_bo == NULL) {
				DRM_ERROR("VSP: failed to find %x bo\n",
					  cur_cmd->buffer);
				ret = -1;
				goto out;
			}
			compose_param_num++;
			VSP_DEBUG("find compose param buffer: id %x, offset %lx\n",
				  cur_cmd->buffer, compose_param_bo->offset);
			VSP_DEBUG("compose param placement %x bus.add %p\n",
				  compose_param_bo->mem.placement,
				  compose_param_bo->mem.bus.addr);
			if (compose_param_num > 1) {
				DRM_ERROR("compose_param_num invalid(%d)!\n",
					  compose_param_num);
				ret = -1;
				goto out;
			}
		} else if (cur_cmd->type == VspSetContextCommand) {
			VSP_DEBUG("set context and new vsp FRC context\n");
		} else if (cur_cmd->type == Vss_Sys_STATE_BUF_COMMAND) {
			VSP_DEBUG("set context and new vsp VP8 context\n");

			cur_cmd->context = VSP_API_GENERIC_CONTEXT_ID;
			cur_cmd->type = VssGenInitializeContext;
			if (priv->filp == vsp_priv->vp8_filp[0]) {
				cur_cmd->buffer = 1;
			} else if (priv->filp == vsp_priv->vp8_filp[1]) {
				cur_cmd->buffer = 2;
			} else if (priv->filp == vsp_priv->vp8_filp[2]) {
				cur_cmd->buffer = 3;
			} else {
				DRM_ERROR("got the wrong context_id and exit\n");
				return -1;
			}

			cur_cmd->size = VSP_APP_ID_VP8_ENC;
			cur_cmd->buffer_id = 0;
			cur_cmd->irq = 0;
			cur_cmd->reserved6 = 0;
			cur_cmd->reserved7 = 0;
		} else if (cur_cmd->type == VssGenInitializeContext) {
			/* Init them just get InitContext command */
			vsp_priv->force_flush_cmd = 0;
			vsp_priv->acc_num_cmd = 0;
			vsp_priv->delayed_burst_cnt = 90;

			vsp_cmd_num++;

		} else if (cur_cmd->type == VssGenDestroyContext) {
			vsp_cmd_num++;
		} else
			/* calculate the numbers of cmd send to VSP */
			vsp_cmd_num++;

		if (cur_cmd->type == VssVp8encEncodeFrameCommand) {
			/* calculate VssVp8encEncodeFrameCommand cmd numbers */
			vsp_priv->vp8_cmd_num++;

			/* set 1st VP8 process context_vp8_id=1 *
			 * set 2nd VP8 process context_vp8_id=2 *
			 * */
			if (priv->filp == vsp_priv->vp8_filp[0]) {
				cur_cmd->context = 1;
			} else if (priv->filp == vsp_priv->vp8_filp[1]) {
				cur_cmd->context = 2;
			} else if (priv->filp == vsp_priv->vp8_filp[2]) {
				cur_cmd->context = 3;
			} else {
				DRM_ERROR("got the wrong context_id and exit\n");
				return -1;
			}

			pic_bo_vp8 =
				ttm_buffer_object_lookup(tfile,
						cur_cmd->reserved7);

			if (pic_bo_vp8 == NULL) {
				DRM_ERROR("VSP: failed to find %x bo\n",
					cur_cmd->reserved7);
				ret = -1;
				goto out;
			}

			vp8_pic_num++;
			VSP_DEBUG("find pic param buffer: id %x, offset %lx\n",
				cur_cmd->reserved7, pic_bo_vp8->offset);
			VSP_DEBUG("pic param placement %x bus.add %p\n",
				pic_bo_vp8->mem.placement,
				pic_bo_vp8->mem.bus.addr);
			if (vp8_pic_num > 1) {
				DRM_ERROR("should be only 1 pic param cmd\n");
				ret = -1;
				goto out;
			}
		}

		if (cur_cmd->type == VssVp8encSetSequenceParametersCommand) {
			if (priv->filp == vsp_priv->vp8_filp[0]) {
				cur_cmd->context = 1;
			} else if (priv->filp == vsp_priv->vp8_filp[1]) {
				cur_cmd->context = 2;
			} else if (priv->filp == vsp_priv->vp8_filp[2]) {
				cur_cmd->context = 3;
			} else {
				DRM_ERROR("got the wrong context_id and exit\n");
				return -1;
			}

			memcpy(&vsp_priv->seq_cmd, cur_cmd, sizeof(struct vss_command_t));
		}

		/* for VP8, directly submit without delay */
		if (cur_cmd->context != 0)
			vsp_priv->force_flush_cmd = 1;

		cmd_size -= sizeof(*cur_cmd);
		cur_cmd++;
	}

	if (vsp_cmd_num)
		vsp_priv->vsp_cmd_num = vsp_cmd_num;

	if (pic_param_num > 0) {
		ret = vsp_fence_vpp_surfaces(priv, validate_list, fence_type, arg,
					 fence_arg, pic_param_bo);
	} else if (vp8_pic_num > 0) {
		ret = vsp_fence_vp8enc_surfaces(priv, validate_list,
					fence_type, arg,
					fence_arg, pic_bo_vp8);
	} else if (compose_param_num) {
		vsp_priv->force_flush_cmd = 1;
		ret = vsp_fence_compose_surfaces(priv, validate_list,
					fence_type, arg,
					fence_arg, compose_param_bo);
	} else {
		/* unreserve these buffer */
		list_for_each_entry_safe(pos, next, validate_list, head) {
			ttm_bo_unreserve(pos->bo);
		}

		vsp_priv->force_flush_cmd = 1;

		VSP_DEBUG("no fence for this command\n");
		goto out;
	}


	VSP_DEBUG("finished fencing\n");
out:

	return ret;
}

int vsp_fence_vpp_surfaces(struct drm_file *priv,
		       struct list_head *validate_list,
		       uint32_t fence_type,
		       struct drm_psb_cmdbuf_arg *arg,
		       struct psb_ttm_fence_rep *fence_arg,
		       struct ttm_buffer_object *pic_param_bo)
{
	struct ttm_bo_kmap_obj pic_param_kmap;
	struct psb_ttm_fence_rep local_fence_arg;
	bool is_iomem;
	int ret = 0;
	struct VssProcPictureParameterBuffer *pic_param;
	int output_surf_num;
	int idx;
	int found;
	uint32_t surf_handler;
	struct ttm_buffer_object *surf_bo;
	struct ttm_fence_object *fence = NULL;
	struct list_head surf_list, tmp_list;
	struct ttm_validate_buffer *pos, *next, *cur_valid_buf = NULL;
	struct ttm_object_file *tfile = BCVideoGetPriv(priv)->tfile;
	struct drm_device *dev = priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;

	INIT_LIST_HEAD(&surf_list);
	INIT_LIST_HEAD(&tmp_list);

	/* map pic param */
	ret = ttm_bo_kmap(pic_param_bo, 0, pic_param_bo->num_pages,
			  &pic_param_kmap);
	if (ret) {
		DRM_ERROR("VSP: ttm_bo_kmap failed: %d\n", ret);
		ttm_bo_unref(&pic_param_bo);
		goto out;
	}

	pic_param = (struct VssProcPictureParameterBuffer *)
		ttm_kmap_obj_virtual(&pic_param_kmap, &is_iomem);

	output_surf_num = pic_param->num_output_pictures;
	VSP_DEBUG("output surf num %d\n", output_surf_num);

	if (output_surf_num == 0)
		vsp_priv->force_flush_cmd = 1;

	/* create surface fence*/
	for (idx = 0; idx < output_surf_num - 1; ++idx) {
		found = 0;

		surf_handler = pic_param->output_picture[idx].surface_id;
		VSP_DEBUG("handling surface id %x\n", surf_handler);

		if (drm_vsp_single_int) {
			pic_param->output_picture[idx].irq = 0;
			continue;
		}

		surf_bo = ttm_buffer_object_lookup(tfile, surf_handler);
		if (surf_bo == NULL) {
			DRM_ERROR("VSP: failed to find %x surface\n",
				  surf_handler);
			ret = -1;
			goto out;
		}
		VSP_DEBUG("find target surf_bo %lx\n", surf_bo->offset);

		/* remove from original validate list */
		list_for_each_entry_safe(pos, next,
					 validate_list, head) {
			if (surf_bo->offset ==  pos->bo->offset) {
				cur_valid_buf = pos;
				list_del_init(&pos->head);
				found = 1;
				break;
			}
		}

		ttm_bo_unref(&surf_bo);

		BUG_ON(!list_empty(&surf_list));
		/* create fence */
		if (found == 1) {
			/* create right list */
			list_add_tail(&cur_valid_buf->head, &surf_list);
			psb_fence_or_sync(priv, VSP_ENGINE_VPP,
					  fence_type, arg->fence_flags,
					  &surf_list, &local_fence_arg,
					  &fence);
			list_del_init(&pos->head);
			/* reserve it */
			list_add_tail(&pos->head, &tmp_list);
		} else {
			DRM_ERROR("VSP: failed to find %d bo: %x\n",
				  idx, surf_handler);
			ret = -1;
			goto out;
		}

		/* assign sequence number
		 * FIXME: do we need fc lock for sequence read?
		 */
		if (fence) {
			VSP_DEBUG("fence sequence %x,pic_idx %d,surf %x\n",
				  fence->sequence, idx,
				  pic_param->output_picture[idx].surface_id);

			pic_param->output_picture[idx].surface_id =
				fence->sequence;
			ttm_fence_object_unref(&fence);
		}
	}

	/* just fence pic param if this is not end command */
	/* only send last output fence_arg back */
	psb_fence_or_sync(priv, VSP_ENGINE_VPP, fence_type,
			  arg->fence_flags, validate_list,
			  fence_arg, &fence);
	if (fence) {
		VSP_DEBUG("fence sequence %x at output pic %d\n",
			  fence->sequence, idx);
		pic_param->output_picture[idx].surface_id = fence->sequence;

		if (drm_vsp_single_int)
			for (idx = 0; idx < output_surf_num - 1; ++idx)
				pic_param->output_picture[idx].surface_id = 0;

		ttm_fence_object_unref(&fence);
	}

	/* add surface back into validate_list */
	list_for_each_entry_safe(pos, next, &tmp_list, head) {
		list_add_tail(&pos->head, validate_list);
	}
out:
	ttm_bo_kunmap(&pic_param_kmap);
	ttm_bo_unref(&pic_param_bo);

	return ret;
}

static int vsp_fence_vp8enc_surfaces(struct drm_file *priv,
				struct list_head *validate_list,
				uint32_t fence_type,
				struct drm_psb_cmdbuf_arg *arg,
				struct psb_ttm_fence_rep *fence_arg,
				struct ttm_buffer_object *pic_param_bo)
{
	bool is_iomem;
	int ret = 0;
	struct VssVp8encPictureParameterBuffer *pic_param;
	struct ttm_fence_object *fence = NULL;
	struct list_head surf_list;
	struct drm_device *dev = priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	struct ttm_bo_kmap_obj vp8_encode_frame__kmap;

	INIT_LIST_HEAD(&surf_list);

	/* map pic param */
	ret = ttm_bo_kmap(pic_param_bo, 0, pic_param_bo->num_pages,
			  &vp8_encode_frame__kmap);
	if (ret) {
		DRM_ERROR("VSP: ttm_bo_kmap failed: %d\n", ret);
		ttm_bo_unref(&pic_param_bo);
		goto out;
	}

	pic_param = (struct VssVp8encPictureParameterBuffer *)
		ttm_kmap_obj_virtual(
				&vp8_encode_frame__kmap,
				&is_iomem);

	VSP_DEBUG("save vp8 pic param address %p\n", pic_param);

	VSP_DEBUG("bo addr %p kernel addr %p surfaceid %x base %x base_uv %x\n",
			pic_param_bo,
			pic_param,
			pic_param->input_frame.surface_id,
			pic_param->input_frame.base,
			pic_param->input_frame.base_uv);

	VSP_DEBUG("pic_param->encoded_frame_base = %x\n",
			pic_param->encoded_frame_base);

	vsp_priv->vp8_encode_frame_cmd = (void *)pic_param;

	/* just fence pic param if this is not end command */
	/* only send last output fence_arg back */
	psb_fence_or_sync(priv, VSP_ENGINE_VPP, fence_type,
			  arg->fence_flags, validate_list,
			  fence_arg, &fence);
	if (fence) {
		VSP_DEBUG("vp8 fence sequence %x\n", fence->sequence);
		pic_param->input_frame.surface_id = fence->sequence;
		vsp_priv->last_sequence = fence->sequence;

		ttm_fence_object_unref(&fence);
	} else {
		VSP_DEBUG("NO fence?????\n");
	}

out:
#ifndef VP8_ENC_DEBUG
	ttm_bo_kunmap(&vp8_encode_frame__kmap);
	ttm_bo_unref(&pic_param_bo);
#endif
	return ret;
}

static int vsp_fence_compose_surfaces(struct drm_file *priv,
				struct list_head *validate_list,
				uint32_t fence_type,
				struct drm_psb_cmdbuf_arg *arg,
				struct psb_ttm_fence_rep *fence_arg,
				struct ttm_buffer_object *compose_param_bo)
{
	int ret = 0;
	struct ttm_fence_object *fence = NULL;
	struct drm_device *dev = priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;

	psb_fence_or_sync(priv, VSP_ENGINE_VPP, fence_type,
			  arg->fence_flags, validate_list,
			  fence_arg, &fence);
	if (fence) {
		VSP_DEBUG("compose fence sequence %x\n",
			  fence->sequence);
		vsp_priv->compose_fence = fence->sequence;

		ttm_fence_object_unref(&fence);
	} else {
		VSP_DEBUG("NO fence?????\n");
		ret = -1;
	}
	ttm_bo_unref(&compose_param_bo);
	return ret;
}




bool vsp_fence_poll(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	uint32_t sequence;
	unsigned long irq_flags;

	VSP_DEBUG("polling vsp msg\n");

	sequence = vsp_priv->current_sequence;

	spin_lock_irqsave(&vsp_priv->lock, irq_flags);

	/* handle the response message */
	sequence = vsp_handle_response(dev_priv);

	spin_unlock_irqrestore(&vsp_priv->lock, irq_flags);

	if (sequence != vsp_priv->current_sequence) {
		vsp_priv->current_sequence = sequence;
		psb_fence_handler(dev, VSP_ENGINE_VPP);
		return true;
	}

	return false;
}

int vsp_new_context(struct drm_device *dev, struct file *filp, int ctx_type)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	int ret = 0;

	dev_priv = dev->dev_private;
	if (dev_priv == NULL) {
		DRM_ERROR("VSP: drm driver is not initialized correctly\n");
		return -1;
	}

	vsp_priv = dev_priv->vsp_private;
	if (vsp_priv == NULL) {
		DRM_ERROR("VSP: vsp driver is not initialized correctly\n");
		return -1;
	}

	if (VAEntrypointEncSlice == ctx_type) {
		vsp_priv->context_vp8_num++;
		if (vsp_priv->context_vp8_num > MAX_VP8_CONTEXT_NUM) {
			DRM_ERROR("VSP: Only support 3 vp8 encoding!\n");
			/* store the 4th vp8 encoding fd for remove context use */
			vsp_priv->vp8_filp[3] = filp;
			return -1;
		}

		/* store the fd of 3 vp8 encoding processes */
		if (vsp_priv->vp8_filp[0] == NULL) {
			vsp_priv->vp8_filp[0] = filp;
		} else if (vsp_priv->vp8_filp[1] == NULL) {
			vsp_priv->vp8_filp[1] = filp;
		} else if (vsp_priv->vp8_filp[2] == NULL) {
			vsp_priv->vp8_filp[2] = filp;
		} else {
			DRM_ERROR("VSP: The current 3 vp8 contexts have not been removed\n");
		}
	} else if (ctx_type == VAEntrypointVideoProc) {
		vsp_priv->context_vpp_num++;
#ifdef MOOREFIELD
		if (vsp_priv->context_vpp_num > MAX_VPP_CONTEXT_NUM) {
			DRM_ERROR("VSP: Only support one VPP stream!\n");
			ret = -1;
		}
#endif
	} else {
		DRM_ERROR("VSP: couldn't support the context %x\n", ctx_type);
		ret = -1;
	}

	VSP_DEBUG("context_vp8_num %d, context_vpp_num %d\n",
			vsp_priv->context_vp8_num, vsp_priv->context_vpp_num);
	return ret;
}

void vsp_rm_context(struct drm_device *dev, struct file *filp, int ctx_type)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	bool ret = true;
	int count = 0;
	struct vss_command_t *cur_cmd;
	bool tmp = true;
	int i = 0;

	dev_priv = dev->dev_private;
	if (dev_priv == NULL) {
		DRM_ERROR("RM context, but dev_priv is NULL");
		return;
	}

	vsp_priv = dev_priv->vsp_private;
	if (vsp_priv == NULL) {
		DRM_ERROR("RM context, but vsp_priv is NULL");
		return;
	}

	if (vsp_priv->ctrl == NULL) {
		for (i = 0; i < MAX_VP8_CONTEXT_NUM + 1; i++) {
			if (filp == vsp_priv->vp8_filp[i])
				vsp_priv->vp8_filp[i] = NULL;
		}

		if (VAEntrypointEncSlice == ctx_type) {
			if (vsp_priv->context_vp8_num > 0)
				vsp_priv->context_vp8_num--;
		} else if (ctx_type == VAEntrypointVideoProc)
			if (vsp_priv->context_vpp_num > 0)
				vsp_priv->context_vpp_num--;
		return;
	}

	VSP_DEBUG("ctx_type=%d\n", ctx_type);

	if (VAEntrypointEncSlice == ctx_type && filp != vsp_priv->vp8_filp[3]) {
		vsp_priv->context_vp8_num--;
		mutex_lock(&vsp_priv->vsp_mutex);
		/* power on again to send VssGenDestroyContext to firmware */
		if (power_island_get(OSPM_VIDEO_VPP_ISLAND) == false) {
			tmp = -EBUSY;
		}
		if (vsp_priv->vsp_state == VSP_STATE_SUSPEND) {
			tmp = vsp_resume_function(dev_priv);
			VSP_DEBUG("The VSP is on suspend, send resume!\n");
		}

		VSP_DEBUG("VP8 send the last command here to destroy context buffer\n");
		/* Update cmd_wr for VP8 and FRC/VPP switch context case */
		if (vsp_priv->acc_num_cmd >= 1) {
			vsp_priv->ctrl->cmd_wr = (vsp_priv->ctrl->cmd_wr + 1) % VSP_CMD_QUEUE_SIZE;
			vsp_priv->acc_num_cmd = 0;
		}

		cur_cmd = vsp_priv->cmd_queue + vsp_priv->ctrl->cmd_wr % VSP_CMD_QUEUE_SIZE;

		cur_cmd->context = VSP_API_GENERIC_CONTEXT_ID;
		cur_cmd->type = VssGenDestroyContext;
		cur_cmd->size = 0;
		cur_cmd->buffer_id = 0;
		cur_cmd->irq = 0;
		cur_cmd->reserved6 = 0;
		cur_cmd->reserved7 = 0;

		/* judge which vp8 process should be remove context */
		for (i = 0; i< MAX_VP8_CONTEXT_NUM; i++) {
			/* context_id=1 for filp[0] */
			/* context_id=2 for filp[1] */
			/* context_id=3 for filp[2] */
			if (filp == vsp_priv->vp8_filp[i]) {
				cur_cmd->buffer = i + 1;
				vsp_priv->vp8_filp[i] = NULL;
			}
		}

		vsp_priv->ctrl->cmd_wr =
			(vsp_priv->ctrl->cmd_wr + 1) % VSP_CMD_QUEUE_SIZE;
		mutex_unlock(&vsp_priv->vsp_mutex);

		/* Wait all the cmd be finished */
		while (vsp_priv->vp8_cmd_num > 0 && count++ < 20000) {
			PSB_UDELAY(6);
		}

		if (count == 20000) {
			DRM_ERROR("Failed to handle sigint event\n");
		}
	} else if(VAEntrypointEncSlice == ctx_type && filp == vsp_priv->vp8_filp[3]) {
		/* driver support 3 vp8 encoding simultaneously at most */
		/* clear the 4th vp8 encoding fd */
		vsp_priv->context_vp8_num--;
		vsp_priv->vp8_filp[3] = NULL;
	} else if (ctx_type == VAEntrypointVideoProc)
		vsp_priv->context_vpp_num--;

	/* Return if there is any context is running */
	if (vsp_priv->context_vp8_num > 0 || vsp_priv->context_vpp_num > 0) {
		VSP_DEBUG("context_vp8_num %d, context_vpp_num %d\n",
			vsp_priv->context_vp8_num, vsp_priv->context_vpp_num);
		return;
	}

	vsp_priv->ctrl->entry_kind = vsp_exit;

	/* in case of power mode 0, HW always active,
	 * * in case got no response from FW, vsp_state=hang but could not be powered off,
	 * * force state to down */
	vsp_priv->vsp_state = VSP_STATE_DOWN;
	ospm_apm_power_down_vsp(dev);

	vsp_priv->vsp_state = VSP_STATE_DOWN;

	if (ret == false)
		PSB_DEBUG_PM("Couldn't power down VSP!");
	else
		PSB_DEBUG_PM("VSP: OK. Power down the HW!\n");

	/* FIXME: frequency should change */
	VSP_PERF("the total time spend on VSP is %llu ms\n",
		 div_u64(vsp_priv->vss_cc_acc, 200 * 1000));

	return;
}

int psb_vsp_save_context(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	int i;

	if (vsp_priv->fw_loaded == VSP_FW_NONE)
		return 0;

	/* save the VSP config registers */
	for (i = 2; i < VSP_CONFIG_SIZE; i++)
		CONFIG_REG_READ32(i, &(vsp_priv->saved_config_regs[i]));

	/* set VSP PM/entry status */
	vsp_priv->ctrl->entry_kind = vsp_entry_booted;
	vsp_priv->vsp_state = VSP_STATE_SUSPEND;

	return 0;
}

int psb_vsp_restore_context(struct drm_device *dev)
{
	/* restore the VSP info */
	return 0;
}

int psb_check_vsp_idle(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	int cmd_rd, cmd_wr;
	unsigned int reg, mode;

	if (vsp_priv->fw_loaded == 0
	    || vsp_priv->vsp_state == VSP_STATE_DOWN
	    || vsp_priv->vsp_state == VSP_STATE_SUSPEND)
		return 0;

	cmd_rd = vsp_priv->ctrl->cmd_rd;
	cmd_wr = vsp_priv->ctrl->cmd_wr;
	if ((cmd_rd != cmd_wr && vsp_priv->vsp_state != VSP_STATE_IDLE)
	    || vsp_priv->vsp_state == VSP_STATE_ACTIVE) {
		PSB_DEBUG_PM("VSP: there is command need to handle!\n");
		return -EBUSY;
	}

	/* make sure VSP system has really been idle
	 * vsp-api runs on sp0 or sp1, but we don't know which one when booting
	 * securely. So wait for both.
	 */
	if (!vsp_is_idle(dev_priv, vsp_sp0)) {
		PSB_DEBUG_PM("VSP: sp0 return busy!\n");
		goto out;
	}
	if (!vsp_is_idle(dev_priv, vsp_sp1)) {
		PSB_DEBUG_PM("VSP: sp1 return busy!\n");
		goto out;
	}

	if (!vsp_is_idle(dev_priv, vsp_vp0)) {
		PSB_DEBUG_PM("VSP: vp0 return busy!\n");
		goto out;
	}
	if (!vsp_is_idle(dev_priv, vsp_vp1)) {
		PSB_DEBUG_PM("VSP: vp1 return busy!\n");
		goto out;
	}

	return 0;
out:
	/* For suspend_and_hw_idle power-mode, sometimes hw couldn't handle
	 * the hw_idle signal correctly. So driver still need power off
	 * the VSP with error log to trace this situation.
	 */
	CONFIG_REG_READ32(1, &reg);
	mode = vsp_priv->ctrl->power_saving_mode;
	if (reg == 1 &&
	    mode == vsp_suspend_and_hw_idle_on_empty_queue) {
		PSB_DEBUG_PM("VSP core is active, but config_reg_d1 is 1\n");
		return 0;
	} else
		return -EBUSY;
}

/* The tasklet function to power down VSP */
void psb_powerdown_vsp(struct work_struct *work)
{
	struct vsp_private *vsp_priv =
		container_of(work, struct vsp_private, vsp_suspend_wq.work);
	bool ret;

	if (!vsp_priv)
		return;

	ret = power_island_put(OSPM_VIDEO_VPP_ISLAND);

	if (ret == false)
		PSB_DEBUG_PM("The VSP could NOT be powered off!\n");
	else
		PSB_DEBUG_PM("The VSP has been powered off!\n");

	return;
}

/* vsp irq tasklet function */
void vsp_irq_task(struct work_struct *work)
{
	struct vsp_private *vsp_priv =
		container_of(work, struct vsp_private, vsp_irq_wq.work);
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	uint32_t sequence;

	if (!vsp_priv)
		return;

	dev = vsp_priv->dev;
	dev_priv = dev->dev_private;

	mutex_lock(&vsp_priv->vsp_mutex);
	/* handle the response message */
	sequence = vsp_handle_response(dev_priv);

	/* handle fence info */
	if (sequence != vsp_priv->current_sequence) {
		vsp_priv->current_sequence = sequence;
		psb_fence_handler(dev, VSP_ENGINE_VPP);
	} else {
		VSP_DEBUG("will not handle fence for %x vs current %x\n",
			  sequence, vsp_priv->current_sequence);
	}

	if (vsp_priv->vsp_state == VSP_STATE_IDLE) {
		if (vsp_priv->ctrl->cmd_rd == vsp_priv->ctrl->cmd_wr)
			ospm_apm_power_down_vsp(dev);
		else {
			while (ospm_power_is_hw_on(OSPM_VIDEO_VPP_ISLAND))
				ospm_apm_power_down_vsp(dev);
			VSP_DEBUG("successfully power down VSP\n");
			power_island_get(OSPM_VIDEO_VPP_ISLAND);
			vsp_resume_function(dev_priv);
		}
	}
	mutex_unlock(&vsp_priv->vsp_mutex);

	return;
}

void vsp_cmd_submit_check(struct work_struct *work)
{
	struct vsp_private *vsp_priv =
		container_of(work, struct vsp_private, vsp_cmd_submit_check_wq.work);
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	uint32_t power_up_try_count;

	if (!vsp_priv)
		return;

	dev = vsp_priv->dev;
	dev_priv = dev->dev_private;

	mutex_lock(&vsp_priv->vsp_mutex);

	if (vsp_priv->acc_num_cmd > 0) {
		power_up_try_count = 10;
		while (power_up_try_count--)
			if (power_island_get(OSPM_VIDEO_VPP_ISLAND) == true)
				break;
		if (power_up_try_count <= 0) {
			DRM_ERROR("failed to send remaining command");
			goto out;
		}

		vsp_resume_function(dev_priv);

		vsp_priv->ctrl->cmd_wr =
			(vsp_priv->ctrl->cmd_wr + vsp_priv->acc_num_cmd) % VSP_CMD_QUEUE_SIZE;
		vsp_priv->acc_num_cmd = 0;
		vsp_priv->force_flush_cmd = 0;

		power_island_put(OSPM_VIDEO_VPP_ISLAND);
	}

out:
	mutex_unlock(&vsp_priv->vsp_mutex);
	return;
}

int psb_vsp_dump_info(struct drm_psb_private *dev_priv)
{
	struct vsp_private *vsp_priv = dev_priv->vsp_private;
	unsigned int reg, i;

	/* config info */
	for (i = 0; i < VSP_CONFIG_SIZE; i++) {
		CONFIG_REG_READ32(i, &reg);
		VSP_DEBUG("partition1_config_reg_d%d=%x\n", i, reg);
	}

	/* ma_header_reg */
	MM_READ32(vsp_priv->boot_header.ma_header_reg, 0, &reg);
	VSP_DEBUG("ma_header_reg:%x\n", reg);

	/* The setting-struct */
	VSP_DEBUG("setting addr:%lu\n", vsp_priv->setting_bo->offset);
	VSP_DEBUG("setting->command_queue_size:0x%x\n",
			vsp_priv->setting->command_queue_size);
	VSP_DEBUG("setting->command_queue_addr:%x\n",
			vsp_priv->setting->command_queue_addr);
	VSP_DEBUG("setting->response_queue_size:0x%x\n",
			vsp_priv->setting->response_queue_size);
	VSP_DEBUG("setting->response_queue_addr:%x\n",
			vsp_priv->setting->response_queue_addr);

	/* dump dma register */
	VSP_DEBUG("partition1_dma_external_ch[0..23]_pending_req_cnt\n");
	for (i=0; i <= 23; i++) {
		MM_READ32(0x150010, i * 0x20, &reg);
		if (reg != 0)
			VSP_DEBUG("partition1_dma_external_ch%d_pending_req_cnt = 0x%x\n",
					i, reg);
	}

	VSP_DEBUG("partition1_dma_external_dim[0..31]_pending_req_cnt\n");
	for (i=0; i <= 31; i++) {
		MM_READ32(0x151008, i * 0x20, &reg);
		if (reg != 0)
			VSP_DEBUG("partition1_dma_external_dim%d_pending_req_cnt = 0x%x\n",
					i, reg);
	}

	VSP_DEBUG("partition1_dma_internal_ch[0..7]_pending_req_cnt\n");
	for (i=0; i <= 7; i++) {
		MM_READ32(0x160010, i * 0x20, &reg);
		if (reg != 0)
			VSP_DEBUG("partition1_dma_internal_ch%d_pending_req_cnt = 0x%x\n",
					i, reg);
	}
	VSP_DEBUG("partition1_dma_internal_dim[0..7]_pending_req_cnt\n");
	for (i=0; i <= 7; i++) {
		MM_READ32(0x160408, i * 0x20, &reg);
		if (reg != 0)
			VSP_DEBUG("partition1_dma_internal_dim%d_pending_req_cnt = 0x%x\n",
					i, reg);
	}

	/* IRQ registers */
	for (i = 0; i < 6; i++) {
		MM_READ32(0x180000, i * 4, &reg);
		VSP_DEBUG("partition1_gp_ireg_IRQ%d:%x", i, reg);
	}
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_EDGE, &reg);
	VSP_DEBUG("partition1_irq_control_irq_edge:%x\n", reg);
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_MASK, &reg);
	VSP_DEBUG("partition1_irq_control_irq_mask:%x\n", reg);
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_STATUS, &reg);
	VSP_DEBUG("partition1_irq_control_irq_status:%x\n", reg);
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_CLR, &reg);
	VSP_DEBUG("partition1_irq_control_irq_clear:%x\n", reg);
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_ENB, &reg);
	VSP_DEBUG("partition1_irq_control_irq_enable:%x\n", reg);
	IRQ_REG_READ32(VSP_IRQ_CTRL_IRQ_LEVEL_PULSE, &reg);
	VSP_DEBUG("partition1_irq_control_irq_pulse:%x\n", reg);

	/* MMU table address */
	MM_READ32(MMU_TABLE_ADDR, 0x0, &reg);
	VSP_DEBUG("mmu_page_table_address:%x\n", reg);

	/* SP0 info */
	VSP_DEBUG("sp0_processor:%d\n", vsp_sp0);
	SP_REG_READ32(0x0, &reg, vsp_sp0);
	VSP_DEBUG("sp0_stat_and_ctrl:%x\n", reg);
	SP_REG_READ32(0x4, &reg, vsp_sp0);
	VSP_DEBUG("sp0_base_address:%x\n", reg);
	SP_REG_READ32(0x24, &reg, vsp_sp0);
	VSP_DEBUG("sp0_debug_pc:%x\n", reg);
	SP_REG_READ32(0x28, &reg, vsp_sp0);
	VSP_DEBUG("sp0_cfg_pmem_iam_op0:%x\n", reg);
	SP_REG_READ32(0x10, &reg, vsp_sp0);
	VSP_DEBUG("sp0_cfg_pmem_master:%x\n", reg);

	/* SP1 info */
	VSP_DEBUG("sp1_processor:%d\n", vsp_sp1);
	SP_REG_READ32(0x0, &reg, vsp_sp1);
	VSP_DEBUG("sp1_stat_and_ctrl:%x\n", reg);
	SP_REG_READ32(0x4, &reg, vsp_sp1);
	VSP_DEBUG("sp1_base_address:%x\n", reg);
	SP_REG_READ32(0x24, &reg, vsp_sp1);
	VSP_DEBUG("sp1_debug_pc:%x\n", reg);
	SP_REG_READ32(0x28, &reg, vsp_sp1);
	VSP_DEBUG("sp1_cfg_pmem_iam_op0:%x\n", reg);
	SP_REG_READ32(0x10, &reg, vsp_sp1);
	VSP_DEBUG("sp1_cfg_pmem_master:%x\n", reg);

	/* VP0 info */
	VSP_DEBUG("vp0_processor:%d\n", vsp_vp0);
	SP_REG_READ32(0x0, &reg, vsp_vp0);
	VSP_DEBUG("partition2_vp0_tile_vp_stat_and_ctrl:%x\n", reg);
	SP_REG_READ32(0x4, &reg, vsp_vp0);
	VSP_DEBUG("partition2_vp0_tile_vp_base_address:%x\n", reg);
	SP_REG_READ32(0x34, &reg, vsp_vp0);
	VSP_DEBUG("partition2_vp0_tile_vp_debug_pc:%x\n", reg);
	SP_REG_READ32(0x38, &reg, vsp_vp0);
	VSP_DEBUG("partition2_vp0_tile_vp_stall_stat_cfg_pmem_iam_op0:%x\n",
			reg);
	SP_REG_READ32(0x10, &reg, vsp_vp0);
	VSP_DEBUG("partition2_vp0_tile_vp_base_addr_MI_cfg_pmem_master:%x\n",
			reg);

	/* VP1 info */
	VSP_DEBUG("vp1_processor:%d\n", vsp_vp1);
	SP_REG_READ32(0x0, &reg, vsp_vp1);
	VSP_DEBUG("partition2_vp1_tile_vp_stat_and_ctrl:%x\n", reg);
	SP_REG_READ32(0x4, &reg, vsp_vp1);
	VSP_DEBUG("partition2_vp1_tile_vp_base_address:%x\n", reg);
	SP_REG_READ32(0x34, &reg, vsp_vp1);
	VSP_DEBUG("partition2_vp1_tile_vp_debug_pc:%x\n", reg);
	SP_REG_READ32(0x38, &reg, vsp_vp1);
	VSP_DEBUG("partition2_vp1_tile_vp_stall_stat_cfg_pmem_iam_op0:%x\n",
			reg);
	SP_REG_READ32(0x10, &reg, vsp_vp1);
	VSP_DEBUG("partition2_vp1_tile_vp_base_addr_MI_cfg_pmem_master:%x\n",
			reg);

	/* MEA info */
	VSP_DEBUG("mea_processor:%d\n", vsp_mea);
	SP_REG_READ32(0x0, &reg, vsp_mea);
	VSP_DEBUG("partition3_mea_tile_mea_stat_and_ctrl:%x\n", reg);
	SP_REG_READ32(0x4, &reg, vsp_mea);
	VSP_DEBUG("partition3_mea_tile_mea_base_address:%x\n", reg);
	SP_REG_READ32(0x2C, &reg, vsp_mea);
	VSP_DEBUG("partition3_mea_tile_mea_debug_pc:%x\n", reg);
	SP_REG_READ32(0x30, &reg, vsp_mea);
	VSP_DEBUG("partition3_mea_tile_mea_stall_stat_cfg_pmem_iam_op0:%x\n",
			reg);
	SP_REG_READ32(0x10, &reg, vsp_mea);
	VSP_DEBUG("partition3_mea_tile_mea_base_addr_MI_cfg_pmem_master:%x\n",
			reg);

	/* ECA info */
	VSP_DEBUG("ECA info\n");
	MM_READ32(0x30000, 0x0, &reg);
	VSP_DEBUG("partition1_sp0_tile_eca_stat_and_ctrl:%x\n", reg);
	MM_READ32(0x30000, 0x4, &reg);
	VSP_DEBUG("partition1_sp0_tile_eca_base_address:%x\n", reg);
	MM_READ32(0x30000, 0x2C, &reg);
	VSP_DEBUG("partition1_sp0_tile_eca_debug_pc:%x\n", reg);
	MM_READ32(0x30000, 0x30, &reg);
	VSP_DEBUG("partition1_sp0_tile_eca_stall_stat_cfg_pmem_loc_op0:%x\n",
			reg);

	/* WDT info */
	for (i = 0; i < 14; i++) {
		MM_READ32(0x170000, i * 4, &reg);
		VSP_DEBUG("partition1_wdt_reg%d:%x\n", i, reg);
	}

	/* command queue */
	VSP_DEBUG("command queue:\n");
	for (i = 0; i < VSP_CMD_QUEUE_SIZE; i++) {
		VSP_DEBUG("cmd[%d]:%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n", i,
			vsp_priv->cmd_queue[i].context,
			vsp_priv->cmd_queue[i].type,
			vsp_priv->cmd_queue[i].buffer,
			vsp_priv->cmd_queue[i].size,
			vsp_priv->cmd_queue[i].buffer_id,
			vsp_priv->cmd_queue[i].irq,
			vsp_priv->cmd_queue[i].reserved6,
			vsp_priv->cmd_queue[i].reserved7);
	}

	/* response queue */
	VSP_DEBUG("ack queue:\n");
	for (i = 0; i < VSP_ACK_QUEUE_SIZE; i++) {
		VSP_DEBUG("ack[%d]:%.8x %.8x %.8x %.8x %.8x %.8x %.8x %.8x\n", i,
			vsp_priv->ack_queue[i].context,
			vsp_priv->ack_queue[i].type,
			vsp_priv->ack_queue[i].buffer,
			vsp_priv->ack_queue[i].size,
			vsp_priv->ack_queue[i].vss_cc,
			vsp_priv->ack_queue[i].reserved5,
			vsp_priv->ack_queue[i].reserved6,
			vsp_priv->ack_queue[i].reserved7);
	}

	return 0;
}

void check_invalid_cmd_type(unsigned int cmd_type)
{
	switch (cmd_type) {
	case VssProcSharpenParameterCommand:
		DRM_ERROR("VSP: Sharpen parameter command is received ");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssProcDenoiseParameterCommand:
		DRM_ERROR("VSP: Denoise parameter command is received ");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssProcColorEnhancementParameterCommand:
		DRM_ERROR("VSP: color enhancer parameter command is received");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssProcFrcParameterCommand:
		DRM_ERROR("VSP: Frc parameter command is received ");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssProcPictureCommand:
		DRM_ERROR("VSP: Picture parameter command is received ");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssVp8encSetSequenceParametersCommand:
		DRM_ERROR("VSP: VP8 sequence parameter command is received\n");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssVp8encEncodeFrameCommand:
		DRM_ERROR("VSP: VP8 picture parameter command is received\n");
		DRM_ERROR("before pipeline command %x\n", cmd_type);
		break;

	case VssVp8encInit:
		DRM_ERROR("Firmware initialization failure\n");
		DRM_ERROR("state buffer size too small\n");
		break;

	default:
		DRM_ERROR("VSP: Unknown command type %x\n", cmd_type);
		break;
	}

	return;
}

void check_invalid_cmd_arg(unsigned int cmd_type)
{
	switch (cmd_type) {
	case VssProcDenoiseParameterCommand:
		DRM_ERROR("VSP: unsupport value for denoise parameter\n");
		break;
	default:
		DRM_ERROR("VSP: input frame resolution is different");
		DRM_ERROR("from previous command\n");
		break;
	}

	return;
}

void handle_error_response(unsigned int error_type, unsigned int cmd_type)
{

	switch (error_type) {
	case VssInvalidCommandType:
		check_invalid_cmd_type(cmd_type);
		DRM_ERROR("VSP: Invalid command\n");
		break;
	case VssInvalidCommandArgument:
		check_invalid_cmd_arg(cmd_type);
		DRM_ERROR("VSP: Invalid command\n");
		break;
	case VssInvalidProcPictureCommand:
		DRM_ERROR("VSP: wrong num of input/output\n");
		break;
	case VssInvalidDdrAddress:
		DRM_ERROR("VSP: DDR address isn't in allowed 1GB range\n");
		break;
	case VssInvalidSequenceParameters_VP8:
		check_invalid_cmd_type(cmd_type);
		DRM_ERROR("VSP: Invalid sequence parameter\n");
		break;
	case VssInvalidPictureParameters_VP8:
		check_invalid_cmd_type(cmd_type);
		DRM_ERROR("VSP: Invalid picture parameter\n");
		break;
	case VssInitFailure_VP8:
		check_invalid_cmd_type(cmd_type);
		DRM_ERROR("VSP: Init Failure\n");
		break;
	case VssCorruptFrame:
		DRM_ERROR("VSP: Coded Frame is corrupted\n");
		break;
	case VssCorruptFramecontinue_VP8:
		DRM_ERROR("VSP: not need to re-init context\n");
		break;
	case VssContextMustBeDestroyed_VP8:
		DRM_ERROR("VSP: context must be destroyed and new context is created\n");
		break;
	default:
		DRM_ERROR("VSP: Unknown error, code %x\n", error_type);
		break;
	}
}

