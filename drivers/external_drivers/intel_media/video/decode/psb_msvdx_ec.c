/**
 * file psb_msvdx_ec.c
 * MSVDX error concealment I/O operations
 *
 */

/**************************************************************************
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
 *
 **************************************************************************/

#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#endif

#include "psb_msvdx.h"
#include "psb_msvdx_msg.h"
#include "psb_msvdx_reg.h"
#include "psb_msvdx_ec.h"

#define MAX_SIZE_IN_MB		(4096 / 16)

static inline int psb_msvdx_cmd_port_write(struct drm_psb_private *dev_priv,
			uint32_t offset, uint32_t value, uint32_t *cmd_space)
{
	uint32_t max_attempts = 0xff;
	uint32_t attempts = 0;

	max_attempts = 0xff;
	while (*cmd_space == 0) {
		*cmd_space = PSB_RMSVDX32(
			MSVDX_CORE_CR_MSVDX_COMMAND_SPACE_OFFSET +
			MSVDX_CORE_BASE);
		if (*cmd_space)
			break;
		PSB_UDELAY(2);
		attempts++;
		if (attempts > max_attempts) {
			printk(KERN_ERR "MSVDX: poll cmd space timeout\n");
			return -1;
		}
	}

	PSB_WMSVDX32(value, offset + MSVDX_CMDS_BASE);
	(*cmd_space)--;
	/*
	 *printk(KERN_DEBUG "MSVDX: poll cmd space attempts %d\n", attempts);
	*/
	return 0;
}

#define PSB_CMDPORT_WRITE(_dev_priv_, _offset_, _cmd_, _cmd_space_)	\
	do {								\
		ret = psb_msvdx_cmd_port_write(_dev_priv_,		\
				 _offset_, _cmd_, &_cmd_space_);	\
		if (ret) {						\
			printk(KERN_DEBUG "write cmd fail, abort\n");	\
			goto ec_done;					\
		}							\
	} while (0);

#define PSB_CMDPORT_WRITE_FAST(_dev_priv_, _offset_, _cmd_, _cmd_space_)  \
	psb_msvdx_cmd_port_write(_dev_priv_,				\
				 _offset_, _cmd_, &_cmd_space_);	\

void psb_msvdx_do_concealment(struct work_struct *work)
{
	struct msvdx_private *msvdx_priv =
		container_of(work, struct msvdx_private, ec_work);
	struct drm_psb_private *dev_priv = NULL;
	struct psb_msvdx_ec_ctx *msvdx_ec_ctx = msvdx_priv->cur_msvdx_ec_ctx;
	drm_psb_msvdx_decode_status_t *fault_region = NULL;
	struct fw_deblock_msg *deblock_msg =
		(struct fw_deblock_msg *)(msvdx_ec_ctx->unfenced_cmd +
		msvdx_ec_ctx->deblock_cmd_offset);
	uint32_t width_in_mb, height_in_mb, cmd;
	int conceal_above_row = 0, loop, mb_loop;
	uint32_t cmd_space = 0;
	int ret = 0;

#ifdef CONFIG_VIDEO_MRFLD
	if (!power_island_get(OSPM_VIDEO_DEC_ISLAND)) {
#else
	if (!ospm_power_using_video_begin(OSPM_VIDEO_DEC_ISLAND)) {
#endif
		printk(KERN_ERR, "MSVDX: fail to power on ved for ec\n");
		return;
	}

	dev_priv = msvdx_priv->dev_priv;
	fault_region = &msvdx_ec_ctx->decode_status;

	/* Concealment should be done in time,
	 * otherwise panic msg will be signaled in msvdx
	 */
	preempt_disable();

	if (msvdx_ec_ctx->deblock_cmd_offset == PSB_MSVDX_INVALID_OFFSET) {
		printk(KERN_ERR "invalid msg offset, abort concealment\n");
		goto ec_done;
	}

	if (fault_region->num_region == 0) {
		PSB_DEBUG_MSVDX("no fault region\n");
		goto ec_done;
	}


	width_in_mb = deblock_msg->pic_size.bits.pic_width_mb;
	height_in_mb = deblock_msg->pic_size.bits.frame_height_mb;

	{
		int i;
		for (i = 0; i < fault_region->num_region; i++)
			PSB_DEBUG_MSVDX("[region %d] is %d to %d\n",
					 i,
					 fault_region->mb_regions[i].start,
					 fault_region->mb_regions[i].end);
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
		PSB_DEBUG_MSVDX("deblock addr_c0 is	0x%08x\n",
					deblock_msg->address_c0);
		PSB_DEBUG_MSVDX("deblock addr_c1 is	0x%08x\n",
					deblock_msg->address_c1);
	}

	if (unlikely(!width_in_mb || !height_in_mb ||
		width_in_mb > MAX_SIZE_IN_MB ||
		height_in_mb > MAX_SIZE_IN_MB)) {
		PSB_DEBUG_MSVDX("wrong pic size\n");
		goto ec_done;
	}

	cmd = 0;
	REGIO_WRITE_FIELD_LITE(cmd, MSVDX_CMDS,
			       DISPLAY_PICTURE_SIZE_DISPLAY_PICTURE_HEIGHT,
			       (height_in_mb * 16) - 1);
	REGIO_WRITE_FIELD_LITE(cmd, MSVDX_CMDS,
			       DISPLAY_PICTURE_SIZE_DISPLAY_PICTURE_WIDTH,
			       (width_in_mb * 16) - 1);
	PSB_CMDPORT_WRITE(dev_priv,
				 MSVDX_CMDS_DISPLAY_PICTURE_SIZE_OFFSET,
				 cmd, cmd_space);

	cmd = 0;
	REGIO_WRITE_FIELD_LITE(cmd, MSVDX_CMDS,
			       CODED_PICTURE_SIZE_CODED_PICTURE_HEIGHT,
			       (height_in_mb * 16) - 1);
	REGIO_WRITE_FIELD_LITE(cmd, MSVDX_CMDS,
			       CODED_PICTURE_SIZE_CODED_PICTURE_WIDTH,
			       (width_in_mb * 16) - 1);
	PSB_CMDPORT_WRITE(dev_priv,
				 MSVDX_CMDS_CODED_PICTURE_SIZE_OFFSET,
				 cmd, cmd_space);

	cmd = deblock_msg->operating_mode;
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_OPERATING_MODE,
			  CHROMA_FORMAT, 1);
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_OPERATING_MODE,
			  ASYNC_MODE, 1);
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_OPERATING_MODE,
			  CODEC_MODE, 3);
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_OPERATING_MODE,
			  CODEC_PROFILE, 1);
	PSB_CMDPORT_WRITE(dev_priv,
				 MSVDX_CMDS_OPERATING_MODE_OFFSET,
				 cmd, cmd_space);

	/* dest frame address */
	PSB_CMDPORT_WRITE(dev_priv,
		MSVDX_CMDS_LUMA_RECONSTRUCTED_PICTURE_BASE_ADDRESSES_OFFSET,
				 deblock_msg->address_a0,
				 cmd_space);

	PSB_CMDPORT_WRITE(dev_priv,
		MSVDX_CMDS_CHROMA_RECONSTRUCTED_PICTURE_BASE_ADDRESSES_OFFSET,
				 deblock_msg->address_a1,
				 cmd_space);

	/* conceal frame address */
	PSB_CMDPORT_WRITE(dev_priv,
		MSVDX_CMDS_REFERENCE_PICTURE_BASE_ADDRESSES_OFFSET,
				 deblock_msg->address_b0,
				 cmd_space);
	PSB_CMDPORT_WRITE(dev_priv,
		MSVDX_CMDS_REFERENCE_PICTURE_BASE_ADDRESSES_OFFSET + 4,
				 deblock_msg->address_b1,
				 cmd_space);
	cmd = 0;
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_SLICE_PARAMS, SLICE_FIELD_TYPE, 2);
	REGIO_WRITE_FIELD(cmd, MSVDX_CMDS_SLICE_PARAMS, SLICE_CODE_TYPE, 1);

	PSB_CMDPORT_WRITE(dev_priv,
				 MSVDX_CMDS_SLICE_PARAMS_OFFSET,
				 cmd, cmd_space);

	cmd = deblock_msg->alt_output_flags_b;
	if ((cmd & 3) != 0) {
		PSB_DEBUG_MSVDX("MSVDX: conceal to rotate surface\n");
	} else {
		PSB_CMDPORT_WRITE(dev_priv,
			MSVDX_CMDS_ALTERNATIVE_OUTPUT_PICTURE_ROTATION_OFFSET,
					 cmd, cmd_space);

		PSB_CMDPORT_WRITE(dev_priv,
			MSVDX_CMDS_VC1_LUMA_RANGE_MAPPING_BASE_ADDRESS_OFFSET,
				 0, cmd_space);

		PSB_CMDPORT_WRITE(dev_priv,
			MSVDX_CMDS_VC1_CHROMA_RANGE_MAPPING_BASE_ADDRESS_OFFSET,
				 0, cmd_space);

		PSB_CMDPORT_WRITE(dev_priv,
			MSVDX_CMDS_VC1_RANGE_MAPPING_FLAGS_OFFSET,
				 0, cmd_space);
	}

	cmd = deblock_msg->ext_stride_a;
	PSB_CMDPORT_WRITE(dev_priv,
			  MSVDX_CMDS_EXTENDED_ROW_STRIDE_OFFSET,
			  cmd, cmd_space);

	for (loop = 0; loop < fault_region->num_region; loop++) {

		uint32_t start = fault_region->mb_regions[loop].start;
		uint32_t end = fault_region->mb_regions[loop].end;
		uint32_t x, y;

		PSB_DEBUG_MSVDX("MSVDX: region(%d) is %d~%d\n",
			loop, start, end);

		if (conceal_above_row)
			start -= width_in_mb;
		if (end > (width_in_mb * height_in_mb - 1))
			end = (width_in_mb * height_in_mb - 1);
		if (start > end)
			start = 0;

		PSB_DEBUG_MSVDX("MSVDX: modify region(%d) is %d~%d\n",
			loop, start, end);

		x = start % width_in_mb;
		y = start / width_in_mb;

		for (mb_loop = start; mb_loop <= end; mb_loop++, x++) {
			if (x >= width_in_mb) {
				x = 0;
				y++;
			}

			/* PSB_DEBUG_MSVDX("MSVDX: concleament (%d,%d)\n",
				x, y); */
			if ((x == 0) && (mb_loop != start))
				PSB_CMDPORT_WRITE_FAST(dev_priv,
					MSVDX_CMDS_END_SLICE_PICTURE_OFFSET,
					0, cmd_space);
			cmd = 0;
			REGIO_WRITE_FIELD_LITE(cmd,
					       MSVDX_CMDS_MACROBLOCK_NUMBER,
					       MB_CODE_TYPE, 1);
			REGIO_WRITE_FIELD_LITE(cmd,
					       MSVDX_CMDS_MACROBLOCK_NUMBER,
					       MB_NO_X, x);
			REGIO_WRITE_FIELD_LITE(cmd,
					       MSVDX_CMDS_MACROBLOCK_NUMBER,
					       MB_NO_Y, y);
			PSB_CMDPORT_WRITE_FAST(dev_priv,
				MSVDX_CMDS_MACROBLOCK_NUMBER_OFFSET,
				cmd, cmd_space);
			PSB_CMDPORT_WRITE_FAST(dev_priv,
				MSVDX_CMDS_MACROBLOCK_RESIDUAL_FORMAT_OFFSET,
				0, cmd_space);
			cmd = 0;
			REGIO_WRITE_FIELD_LITE(cmd,
					MSVDX_CMDS_INTER_BLOCK_PREDICTION,
					       REF_INDEX_A_VALID, 1);
			REGIO_WRITE_FIELD_LITE(cmd,
					MSVDX_CMDS_INTER_BLOCK_PREDICTION,
					       INTER_PRED_BLOCK_SIZE, 0);
			REGIO_WRITE_FIELD_LITE(cmd,
					MSVDX_CMDS_INTER_BLOCK_PREDICTION,
					       REF_INDEX_A, 0);
			REGIO_WRITE_FIELD_LITE(cmd,
				MSVDX_CMDS_INTER_BLOCK_PREDICTION,
				REF_INDEX_B, 0);
			PSB_CMDPORT_WRITE_FAST(dev_priv,
				MSVDX_CMDS_INTER_BLOCK_PREDICTION_OFFSET,
				cmd, cmd_space);
			PSB_CMDPORT_WRITE_FAST(dev_priv,
				MSVDX_CMDS_MOTION_VECTOR_OFFSET,
				0, cmd_space);
		}

		PSB_CMDPORT_WRITE(dev_priv,
			MSVDX_CMDS_END_SLICE_PICTURE_OFFSET,
			0, cmd_space);
	}

ec_done:
	/* try to unblock rendec */
	ret = PSB_CMDPORT_WRITE_FAST(dev_priv,
		MSVDX_CMDS_END_SLICE_PICTURE_OFFSET,
		1, cmd_space);

	fault_region->num_region = 0;

	preempt_enable();

#ifdef CONFIG_VIDEO_MRFLD
	power_island_put(OSPM_VIDEO_DEC_ISLAND);
#else
	ospm_power_using_video_end(OSPM_VIDEO_DEC_ISLAND);
#endif
	printk(KERN_DEBUG "MSVDX: EC done, unlock msvdx ret %d\n",
	       ret);

	return;
}

struct psb_msvdx_ec_ctx *psb_msvdx_find_ec_ctx(
			struct msvdx_private *msvdx_priv,
			struct ttm_object_file *tfile,
			void *cmd)
{
	int i, free_idx;
	struct psb_msvdx_ec_ctx *ec_ctx = NULL;
	struct fw_deblock_msg *deblock_msg = (struct fw_deblock_msg *)cmd;

	free_idx = -1;
	for (i = 0; i < PSB_MAX_EC_INSTANCE; i++) {
		if (msvdx_priv->msvdx_ec_ctx[i]->tfile == tfile)
			break;
		else if (free_idx < 0 &&
			 msvdx_priv->msvdx_ec_ctx[i]->tfile == NULL)
			free_idx = i;
	}

	if (i < PSB_MAX_EC_INSTANCE)
		ec_ctx = msvdx_priv->msvdx_ec_ctx[i];
	else if (free_idx >= 0 && cmd) {
		PSB_DEBUG_MSVDX("acquire ec ctx idx %d for tfile 8x%08x\n",
				free_idx, tfile);
		ec_ctx = msvdx_priv->msvdx_ec_ctx[free_idx];
		memset(ec_ctx, 0, sizeof(*ec_ctx));
		ec_ctx->tfile = tfile;
		ec_ctx->context_id = deblock_msg->mmu_context.bits.context;
	} else {
		PSB_DEBUG_MSVDX("Available ec ctx is not found\n");
	}

	return ec_ctx;
}

void psb_msvdx_update_frame_info(struct msvdx_private *msvdx_priv,
					struct ttm_object_file *tfile,
					void *cmd)
{

	int i, free_idx;
	drm_psb_msvdx_frame_info_t *frame_info;
	struct fw_deblock_msg *deblock_msg = (struct fw_deblock_msg *)cmd;
	uint32_t buffer_handle = deblock_msg->mb_param_address;

	struct psb_msvdx_ec_ctx *ec_ctx;

	PSB_DEBUG_MSVDX(
		"update frame info (handle 0x%08x) for error concealment\n",
		buffer_handle);

	ec_ctx = psb_msvdx_find_ec_ctx(msvdx_priv, tfile, cmd);

	if (!ec_ctx)
		return;

	free_idx = -1;
	for (i = 0; i < MAX_DECODE_BUFFERS; i++) {
		if (buffer_handle == ec_ctx->frame_info[i].handle)
			break;
		if (free_idx < 0 && ec_ctx->frame_info[i].handle == NULL)
			free_idx = i;
	}

	if (i < MAX_DECODE_BUFFERS)
		frame_info = &ec_ctx->frame_info[i];
	else if (free_idx >= 0) {
		PSB_DEBUG_MSVDX("acquire frame_info solt idx %d\n", free_idx);
		frame_info = &ec_ctx->frame_info[free_idx];
	} else {
		PSB_DEBUG_MSVDX("%d solts occupied, abort update frame_info\n",
				MAX_DECODE_BUFFERS);
		return;
	}

	frame_info->fw_status = 0;
	frame_info->handle = buffer_handle;
	frame_info->fence = (deblock_msg->header.bits.msg_fence & (~0xf));
	frame_info->decode_status.num_region = 0;
	ec_ctx->cur_frame_info = frame_info;
}

void psb_msvdx_backup_cmd(struct msvdx_private *msvdx_priv,
				struct ttm_object_file *tfile,
				void *cmd,
				uint32_t cmd_size,
				uint32_t deblock_cmd_offset)
{
	struct fw_deblock_msg *deblock_msg = NULL;

	struct psb_msvdx_ec_ctx *ec_ctx;
	union msg_header *header;

	PSB_DEBUG_MSVDX("backup cmd for ved error concealment\n");

	ec_ctx = psb_msvdx_find_ec_ctx(msvdx_priv, tfile, NULL);

	if (!ec_ctx) {
		PSB_DEBUG_MSVDX("this is not a ec ctx, abort backup cmd\n");
		return;
	}


	if (deblock_cmd_offset != PSB_MSVDX_INVALID_OFFSET)
		deblock_msg = (struct fw_deblock_msg *)(cmd + deblock_cmd_offset);

	if (deblock_msg &&
	    ec_ctx->context_id != deblock_msg->mmu_context.bits.context) {
		PSB_DEBUG_MSVDX("backup cmd but find mis-match context id\n");
		return;
	}

	ec_ctx->cmd_size = cmd_size;
	ec_ctx->deblock_cmd_offset = deblock_cmd_offset;
	memcpy(ec_ctx->unfenced_cmd, cmd, cmd_size);
	ec_ctx->fence = PSB_MSVDX_INVALID_FENCE;
	header = (union msg_header *)ec_ctx->unfenced_cmd;
	if (cmd_size)
		ec_ctx->fence = header->bits.msg_fence;
	ec_ctx->fence &= (~0xf);
	PSB_DEBUG_MSVDX("backup cmd for ved: fence 0x%08x, cmd_size %d\n",
				ec_ctx->fence, cmd_size);
}

void psb_msvdx_mtx_message_dump(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	int i, buf_size, buf_offset;
	buf_size = PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_BUF_SIZE) & ((1 << 16) - 1);
	buf_offset =
		(PSB_RMSVDX32(MSVDX_COMMS_TO_HOST_BUF_SIZE) >> 16) + 0x2000;

	printk(KERN_DEBUG "Dump to HOST message buffer (offset:size)%04x:%04x\n",
	       buf_offset, buf_size);
	for (i = 0; i < buf_size; i += 4) {
		uint32_t reg1, reg2, reg3, reg4;
		reg1 = PSB_RMSVDX32(buf_offset + i * 4);
		reg2 = PSB_RMSVDX32(buf_offset + i * 4 + 4);
		reg3 = PSB_RMSVDX32(buf_offset + i * 4 + 8);
		reg4 = PSB_RMSVDX32(buf_offset + i * 4 + 12);
		printk(KERN_DEBUG "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		       (buf_offset + i * 4), reg1, reg2, reg3, reg4);
	}

	buf_size = PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_BUF_SIZE) & ((1 << 16) - 1);
	buf_offset = (PSB_RMSVDX32(MSVDX_COMMS_TO_MTX_BUF_SIZE) >> 16) + 0x2000;

	printk(KERN_DEBUG "Dump to MTX message buffer (offset:size)%04x:%04x\n",
	       buf_offset, buf_size);
	for (i = 0; i < buf_size; i += 4) {
		uint32_t reg1, reg2, reg3, reg4;
		reg1 = PSB_RMSVDX32(buf_offset + i * 4);
		reg2 = PSB_RMSVDX32(buf_offset + i * 4 + 4);
		reg3 = PSB_RMSVDX32(buf_offset + i * 4 + 8);
		reg4 = PSB_RMSVDX32(buf_offset + i * 4 + 12);
		printk(KERN_DEBUG "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		       (buf_offset + i * 4), reg1, reg2, reg3, reg4);
	}

	buf_size = 12;
	buf_offset = 0xFD0 + 0x2000;

	printk(KERN_DEBUG "Comm header (offset:size)%04x:%04x\n",
	       buf_offset, buf_size);
	for (i = 0; i < buf_size; i += 4) {
		uint32_t reg1, reg2, reg3, reg4;
		reg1 = PSB_RMSVDX32(buf_offset + i * 4);
		reg2 = PSB_RMSVDX32(buf_offset + i * 4 + 4);
		reg3 = PSB_RMSVDX32(buf_offset + i * 4 + 8);
		reg4 = PSB_RMSVDX32(buf_offset + i * 4 + 12);
		printk(KERN_DEBUG "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
		       (buf_offset + i * 4), reg1, reg2, reg3, reg4);
	}

	printk(KERN_DEBUG "Error status 0x2cc4: 0x%08x\n",
	       PSB_RMSVDX32(0x2cc4));
}
