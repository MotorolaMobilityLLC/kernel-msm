/**
 * file pnw_topaz.c
 * TOPAZ I/O operations and IRQ handling
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
 *      Shengquan(Austin) Yuan <shengquan.yuan@intel.com>
 *      Elaine Wang <elaine.wang@intel.com>
 *      Li Zeng <li.zeng@intel.com>
 **************************************************************************/

/* include headers */
/* #define DRM_DEBUG_CODE 2 */
#include <drm/drmP.h>

#include "psb_drv.h"
#include "psb_drm.h"
#include "pnw_topaz.h"
#include "psb_powermgmt.h"
#include "pnw_topaz_hw_reg.h"

#include <linux/io.h>
#include <linux/delay.h>

#define TOPAZ_MAX_COMMAND_IN_QUEUE 0x1000
/* #define SYNC_FOR_EACH_COMMAND */

#define PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size, cur_cmd_size, cur_cmd_id)\
	do {\
		if ((cmd_size) < (cur_cmd_size)) {\
			DRM_ERROR("%s L%d cmd size(%d) of cmd id(%x)"\
					" is not correct\n",\
					__func__, __LINE__, cur_cmd_size,\
					cur_cmd_id);\
			return -EINVAL;\
		} \
	} while (0)

#define PNW_TOPAZ_CHECK_CORE_ID(core_id)\
	do {\
		if ((core_id) >= TOPAZSC_NUM_CORES) {\
			DRM_ERROR("%s L%d core_id(%d)"\
					" is not correct\n",\
					__func__, __LINE__,\
					core_id);\
			return -EINVAL;\
		} \
	} while (0)



/* static function define */
static int pnw_topaz_deliver_command(struct drm_device *dev,
				     struct ttm_buffer_object *cmd_buffer,
				     u32 cmd_offset,
				     u32 cmd_size,
				     void **topaz_cmd, uint32_t sequence,
				     int copy_cmd);
static int pnw_topaz_send(struct drm_device *dev, unsigned char *cmd,
			  u32 cmd_size, uint32_t sync_seq);
static int pnw_topaz_dequeue_send(struct drm_device *dev);
static int pnw_topaz_save_command(struct drm_device *dev, void *cmd,
				  u32 cmd_size, uint32_t sequence);

static void topaz_mtx_kick(struct drm_psb_private *dev_priv, uint32_t core_id,
			   uint32_t kick_count);

IMG_BOOL pnw_topaz_interrupt(IMG_VOID *pvData)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	uint32_t clr_flag;
	struct pnw_topaz_private *topaz_priv;
	uint32_t topaz_stat;
	uint32_t cur_seq, cmd_id;

	PSB_DEBUG_IRQ("Got an TopazSC interrupt\n");

	if (pvData == IMG_NULL) {
		DRM_ERROR("ERROR: TOPAZ %s, Invalid params\n", __func__);
		return IMG_FALSE;
	}

	dev = (struct drm_device *)pvData;

	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		DRM_ERROR("ERROR: interrupt arrived but HW is power off\n");
		return IMG_FALSE;
	}

	dev_priv = (struct drm_psb_private *) dev->dev_private;
	topaz_priv = dev_priv->topaz_private;

	/*TODO : check if topaz is busy*/
	topaz_priv->topaz_hw_busy = REG_READ(0x20D0) & (0x1 << 11);

	TOPAZ_READ32(TOPAZ_CR_IMG_TOPAZ_INTSTAT, &topaz_stat, 0);
	clr_flag = pnw_topaz_queryirq(dev);

	pnw_topaz_clearirq(dev, clr_flag);

	TOPAZ_MTX_WB_READ32(topaz_priv->topaz_sync_addr,
			    0, MTX_WRITEBACK_CMDWORD, &cmd_id);
	cmd_id = (cmd_id & 0x7f); /* CMD ID */
	if (cmd_id != MTX_CMDID_NULL)
		return IMG_TRUE;

	TOPAZ_MTX_WB_READ32(topaz_priv->topaz_sync_addr,
			    0, MTX_WRITEBACK_VALUE, &cur_seq);

	PSB_DEBUG_TOPAZ("TOPAZ:Got SYNC IRQ,sync seq:0x%08x (MTX)"
		      " vs 0x%08x(fence)\n",
		      cur_seq, dev_priv->sequence[LNC_ENGINE_ENCODE]);

	psb_fence_handler(dev, LNC_ENGINE_ENCODE);

	topaz_priv->topaz_busy = 1;
	pnw_topaz_dequeue_send(dev);

	if (drm_topaz_pmpolicy != PSB_PMPOLICY_NOPM \
			&& topaz_priv->topaz_busy == 0) {
		PSB_DEBUG_IRQ("TOPAZ:Schedule a work to power down Topaz\n");
		schedule_delayed_work(&topaz_priv->topaz_suspend_wq, 0);
	}

	return IMG_TRUE;
}

/* #define PSB_DEBUG_GENERAL DRM_ERROR */
static int pnw_submit_encode_cmdbuf(struct drm_device *dev,
				    struct ttm_buffer_object *cmd_buffer,
				    u32 cmd_offset, u32 cmd_size,
				    struct ttm_fence_object *fence)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned long irq_flags;
	int ret = 0;
	void *cmd;
	uint32_t sequence = dev_priv->sequence[LNC_ENGINE_ENCODE];
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	PSB_DEBUG_GENERAL("TOPAZ: command submit\n");

	PSB_DEBUG_GENERAL("TOPAZ: topaz busy = %d\n", topaz_priv->topaz_busy);

	if (dev_priv->last_topaz_ctx != dev_priv->topaz_ctx) {
		/* todo: save current context into dev_priv->last_topaz_ctx
		 * and reload dev_priv->topaz_ctx context
		 */
		PSB_DEBUG_GENERAL("TOPAZ: context switch\n");
		dev_priv->last_topaz_ctx = dev_priv->topaz_ctx;
	}

	if (topaz_priv->topaz_fw_loaded == 0) {
		/* #.# load fw to driver */
		PSB_DEBUG_INIT("TOPAZ: load /lib/firmware/topaz_fwsc.bin\n");
		ret = pnw_topaz_init_fw(dev);
		if (ret != 0) {
			/* FIXME: find a proper return value */
			DRM_ERROR("TOPAX:load /lib/firmware/topaz_fwsc.bin"
				  " fails, ensure udevd is configured"
				  " correctly!\n");
			return -EFAULT;
		}
		topaz_priv->topaz_fw_loaded = 1;
	}

	/* # schedule watchdog */
	/* psb_schedule_watchdog(dev_priv); */

	/* # spin lock irq save [msvdx_lock] */
	spin_lock_irqsave(&topaz_priv->topaz_lock, irq_flags);

	/* # if topaz need to reset, reset it */
	if (topaz_priv->topaz_needs_reset) {
		/* #.# reset it */
		spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);
		DRM_ERROR("TOPAZ: needs reset.\n");
		return -EFAULT;
	}

	if (!topaz_priv->topaz_busy) {
		/* # direct map topaz command if topaz is free */
		PSB_DEBUG_GENERAL("TOPAZ:direct send command,sequence %08x\n",
				  sequence);

		topaz_priv->topaz_busy = 1;
		spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);

		ret = pnw_topaz_deliver_command(dev, cmd_buffer, cmd_offset,
						cmd_size, NULL, sequence, 0);

		if (ret) {
			DRM_ERROR("TOPAZ: failed to extract cmd...\n");
			return ret;
		}
	} else {
		PSB_DEBUG_GENERAL("TOPAZ: queue command,sequence %08x \n",
				  sequence);
		cmd = NULL;

		spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);

		ret = pnw_topaz_deliver_command(dev, cmd_buffer, cmd_offset,
						cmd_size, &cmd, sequence, 1);
		if (cmd == NULL || ret) {
			DRM_ERROR("TOPAZ: map command for save fialed\n");
			return ret;
		}

		ret = pnw_topaz_save_command(dev, cmd, cmd_size, sequence);
		if (ret) {
			kfree(cmd);
			DRM_ERROR("TOPAZ: save command failed\n");
		}
	}

	return ret;
}

static int pnw_topaz_save_command(struct drm_device *dev, void *cmd,
				  u32 cmd_size, uint32_t sequence)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_cmd_queue *topaz_cmd;
	unsigned long irq_flags;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	PSB_DEBUG_GENERAL("TOPAZ: queue command,sequence: %08x..\n",
			  sequence);

	topaz_cmd = kzalloc(sizeof(struct pnw_topaz_cmd_queue),
			    GFP_KERNEL);
	if (topaz_cmd == NULL) {
		DRM_ERROR("TOPAZ: out of memory....\n");
		return -ENOMEM;
	}

	topaz_cmd->cmd = cmd;
	topaz_cmd->cmd_size = cmd_size;
	topaz_cmd->sequence = sequence;

	spin_lock_irqsave(&topaz_priv->topaz_lock, irq_flags);
	list_add_tail(&topaz_cmd->head, &topaz_priv->topaz_queue);
	if (!topaz_priv->topaz_busy) {
		/* topaz_priv->topaz_busy = 1; */
		PSB_DEBUG_GENERAL("TOPAZ: need immediate dequeue...\n");
		pnw_topaz_dequeue_send(dev);
		PSB_DEBUG_GENERAL("TOPAZ: after dequeue command\n");
	}

	spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);

	return 0;
}


int pnw_cmdbuf_video(struct drm_file *priv,
		     struct list_head *validate_list,
		     uint32_t fence_type,
		     struct drm_psb_cmdbuf_arg *arg,
		     struct ttm_buffer_object *cmd_buffer,
		     struct psb_ttm_fence_rep *fence_arg)
{
	struct drm_device *dev = priv->minor->dev;
	struct ttm_fence_object *fence = NULL;
	int ret;

	PSB_DEBUG_GENERAL("TOPAZ : enter %s cmdsize: %d\n", __func__,
			  arg->cmdbuf_size);

	ret = pnw_submit_encode_cmdbuf(dev, cmd_buffer, arg->cmdbuf_offset,
				       arg->cmdbuf_size, fence);
	if (ret)
		return ret;

	/* workaround for interrupt issue */
	psb_fence_or_sync(priv, LNC_ENGINE_ENCODE, fence_type, arg->fence_flags,
			  validate_list, fence_arg, &fence);

	if (fence)
		ttm_fence_object_unref(&fence);

	spin_lock(&cmd_buffer->bdev->fence_lock);
	if (cmd_buffer->sync_obj != NULL)
		ttm_fence_sync_obj_unref(&cmd_buffer->sync_obj);
	spin_unlock(&cmd_buffer->bdev->fence_lock);

	PSB_DEBUG_GENERAL("TOPAZ exit %s\n", __func__);
	return 0;
}

int pnw_wait_on_sync(struct drm_psb_private *dev_priv,
		     uint32_t sync_seq,
		     uint32_t *sync_p)
{
	int count = 10000;
	if (sync_p == NULL) {
		DRM_ERROR("TOPAZ: pnw_wait_on_sync invalid memory address\n ");
		return -1;
	}

	while (count && (sync_seq != *sync_p)) {
		PSB_UDELAY(100);/* experimental value */
		--count;
	}
	if ((count == 0) && (sync_seq != *sync_p)) {
		DRM_ERROR("TOPAZ: wait sycn timeout (0x%08x),actual 0x%08x\n",
			  sync_seq, *sync_p);
		return -EBUSY;
	}
	PSB_DEBUG_GENERAL("TOPAZ: SYNC done, seq=0x%08x\n", *sync_p);
	return 0;
}

int pnw_topaz_deliver_command(struct drm_device *dev,
			      struct ttm_buffer_object *cmd_buffer,
			      u32 cmd_offset, u32 cmd_size,
			      void **topaz_cmd, uint32_t sequence,
			      int copy_cmd)
{
	unsigned long cmd_page_offset = cmd_offset & ~PAGE_MASK;
	struct ttm_bo_kmap_obj cmd_kmap;
	bool is_iomem;
	int ret;
	unsigned char *cmd_start, *tmp;
	u16 num_pages;

	num_pages = ((cmd_buffer->num_pages < PNW_MAX_CMD_BUF_PAGE_NUM) ?
		       cmd_buffer->num_pages : PNW_MAX_CMD_BUF_PAGE_NUM);
	if (cmd_size > (num_pages << PAGE_SHIFT) ||
			cmd_offset > (num_pages << PAGE_SHIFT) ||
			(cmd_size + cmd_offset) > (num_pages << PAGE_SHIFT)
			|| (cmd_size == 0)) {
		DRM_ERROR("TOPAZ: %s invalid cmd_size(%d) or cmd_offset(%d)",
				__func__, cmd_size, cmd_offset);
		return -EINVAL;
	}
	ret = ttm_bo_kmap(cmd_buffer, cmd_offset >> PAGE_SHIFT, num_pages,
			  &cmd_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: drm_bo_kmap failed: %d\n", ret);
		return ret;
	}
	cmd_start = (unsigned char *) ttm_kmap_obj_virtual(&cmd_kmap,
			&is_iomem) + cmd_page_offset;

	if (copy_cmd) {
		PSB_DEBUG_GENERAL("TOPAZ: queue commands\n");
		tmp = kzalloc(cmd_size, GFP_KERNEL);
		if (tmp == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(tmp, cmd_start, cmd_size);
		*topaz_cmd = tmp;
	} else {
		PSB_DEBUG_GENERAL("TOPAZ: directly send the command\n");
		ret = pnw_topaz_send(dev, cmd_start, cmd_size, sequence);
		if (ret) {
			DRM_ERROR("TOPAZ: commit commands failed.\n");
			ret = -EINVAL;
		}
	}

out:
	PSB_DEBUG_GENERAL("TOPAZ:cmd_size(%d), sequence(%d)"
			  " copy_cmd(%d)\n",
			  cmd_size, sequence, copy_cmd);

	ttm_bo_kunmap(&cmd_kmap);

	return ret;
}


int pnw_topaz_kick_null_cmd(struct drm_psb_private *dev_priv,
			    uint32_t core_id,
			    uint32_t wb_offset,
			    uint32_t sync_seq,
			    uint8_t irq_enable)
{
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	uint32_t cur_free_space;
	struct topaz_cmd_header cur_cmd_header;
	int ret;

	POLL_TOPAZ_FREE_FIFO_SPACE(PNW_TOPAZ_WORDS_PER_CMDSET,
			PNW_TOPAZ_POLL_DELAY,
			PNW_TOPAZ_POLL_RETRY,
			&cur_free_space);
	if (ret) {
		DRM_ERROR("TOPAZ: error -- ret(%d)\n", ret);
		return ret;
	}

	cur_cmd_header.core = core_id;
	cur_cmd_header.seq = sync_seq,
		       cur_cmd_header.enable_interrupt = ((irq_enable == 0) ? 0 : 1);
	cur_cmd_header.id = MTX_CMDID_NULL;

	topaz_priv->topaz_cmd_count %= MAX_TOPAZ_CMD_COUNT;
	PSB_DEBUG_GENERAL("TOPAZ: free FIFO space %d\n",
			  cur_free_space);
	PSB_DEBUG_GENERAL("TOPAZ: write 4 words to FIFO:"
			  "0x%08x,0x%08x,0x%08x,0x%08x\n",
			  cur_cmd_header.val,
			  0,
			  wb_offset,
			  cur_cmd_header.seq);

	TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
				cur_cmd_header.val);
	TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
				0);
	TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
				wb_offset);
	TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
				sync_seq);

	PSB_DEBUG_GENERAL("TOPAZ: Write back value for NULL CMD is %d\n",
			  sync_seq);

	topaz_mtx_kick(dev_priv, 0, 1);

	return 0;
}


static void pnw_topaz_save_bias_table(struct pnw_topaz_private *topaz_priv,
	const void *cmd, int byte_size, int core)
{
	PSB_DEBUG_GENERAL("TOPAZ: Save BIAS table(size %d) for core %d\n",
			byte_size, core);

	if (byte_size > PNW_TOPAZ_BIAS_TABLE_MAX_SIZE) {
		DRM_ERROR("Invalid BIAS table size %d!\n", byte_size);
		return;
	}

	if (core > (topaz_priv->topaz_num_cores - 1)) {
		DRM_ERROR("Invalid core id %d\n", core);
		return;
	}

	if (topaz_priv->topaz_bias_table[core] == NULL) {
		topaz_priv->topaz_bias_table[core] =
			kmalloc(PNW_TOPAZ_BIAS_TABLE_MAX_SIZE,
					GFP_KERNEL);
		if (NULL == topaz_priv->topaz_bias_table[core]) {
			DRM_ERROR("Run out of memory!\n");
			return;
		}
	}

	memcpy(topaz_priv->topaz_bias_table[core],
			cmd, byte_size);
	return;
}

static inline int pnw_topaz_write_reg(struct drm_psb_private *dev_priv,
		u32 *p_command, u32 reg_cnt, u8 core_id)
{
	u32 reg_off, reg_val;
	for (; reg_cnt > 0; reg_cnt--) {
		reg_off = *p_command;
		p_command++;
		reg_val = *p_command;
		p_command++;
		if (reg_off > TOPAZ_BIASREG_MAX(core_id) ||
				reg_off < TOPAZ_BIASREG_MIN(core_id)) {
			DRM_ERROR("TOPAZ: Ignore write (0x%08x)" \
					" to register 0x%08x\n",
					reg_val, reg_off);
			return -EINVAL;
		} else
			MM_WRITE32(0, reg_off, reg_val);
	}
	return 0;
}


int
pnw_topaz_send(struct drm_device *dev, unsigned char *cmd,
	       u32 cmd_size, uint32_t sync_seq)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int ret = 0;
	struct topaz_cmd_header *cur_cmd_header;
	uint32_t cur_cmd_size = 4, cur_cmd_id, cur_free_space = 0;
	uint32_t codec;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	uint32_t reg_cnt;
	uint32_t *p_command;
	uint32_t tmp;

	PSB_DEBUG_TOPAZ("TOPAZ: send encoding commands(seq 0x%08x) to HW\n",
		sync_seq);

	tmp = atomic_cmpxchg(&dev_priv->topaz_mmu_invaldc, 1, 0);
	if (tmp)
		pnw_topaz_mmu_flushcache(dev_priv);


	/* Command header(struct topaz_cmd_header) is 32 bit */
	while (cmd_size >= sizeof(struct topaz_cmd_header)) {
		cur_cmd_header = (struct topaz_cmd_header *) cmd;
		PNW_TOPAZ_CHECK_CORE_ID(cur_cmd_header->core);
		cur_cmd_id = cur_cmd_header->id;
		PSB_DEBUG_GENERAL("TOPAZ: %s:\n", cmd_to_string(cur_cmd_id));

		switch (cur_cmd_id) {
		case MTX_CMDID_SW_NEW_CODEC:
			cur_cmd_size = sizeof(struct topaz_cmd_header)
				+ TOPAZ_NEW_CODEC_CMD_BYTES;
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			p_command = (uint32_t *)
				(cmd + sizeof(struct topaz_cmd_header));
			codec = *p_command;

			if (codec >= PNW_TOPAZ_CODEC_NUM_MAX) {
				DRM_ERROR("%s unknown video codec %d\n",
						__func__, codec);
				return -EINVAL;
			}

			p_command++;
			topaz_priv->frame_h =
				(u16)((*p_command) & 0xffff) ;
			topaz_priv->frame_w =
				 (u16)((*p_command >> 16) & 0xffff);
			PSB_DEBUG_GENERAL("TOPAZ: setup new codec %s (%d),"
					  " width %d, height %d\n",
					  codec_to_string(codec), codec,
					  topaz_priv->frame_w,
					  topaz_priv->frame_h);
			if (pnw_topaz_setup_fw(dev, codec)) {
				DRM_ERROR("TOPAZ: upload FW to HW failed\n");
				return -EBUSY;
			}
			topaz_priv->topaz_cur_codec = codec;
			break;
#ifdef CONFIG_VIDEO_MRFLD
		case MTX_CMDID_SW_ENTER_LOWPOWER:
			PSB_DEBUG_GENERAL("TOPAZ: enter lowpower....\n");
			cur_cmd_size = sizeof(struct topaz_cmd_header)
				+ TOPAZ_POWER_CMD_BYTES;
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			break;

		case MTX_CMDID_SW_LEAVE_LOWPOWER:
			PSB_DEBUG_GENERAL("TOPAZ: leave lowpower...\n");
			cur_cmd_size = sizeof(struct topaz_cmd_header)
				+ TOPAZ_POWER_CMD_BYTES;
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			break;
#endif
		case MTX_CMDID_SW_WRITEREG:
			p_command = (uint32_t *)
				(cmd + sizeof(struct topaz_cmd_header));
			cur_cmd_size = sizeof(struct topaz_cmd_header)
				+ sizeof(u32);
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			reg_cnt = *p_command;
			p_command++;
			PNW_TOPAZ_CHECK_CMD_SIZE(TOPAZ_WRITEREG_MAX_SETS,
					reg_cnt, cur_cmd_id);
			/* Reg_off and reg_val are stored in a pair of words*/
			cur_cmd_size += (reg_cnt *
					TOPAZ_WRITEREG_BYTES_PER_SET);
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			if ((drm_topaz_pmpolicy != PSB_PMPOLICY_NOPM) &&
				(!PNW_IS_JPEG_ENC(topaz_priv->topaz_cur_codec)))
				pnw_topaz_save_bias_table(topaz_priv,
					(const void *)cmd,
					cur_cmd_size,
					cur_cmd_header->core);

			PSB_DEBUG_GENERAL("TOPAZ: Start to write" \
					" %d Registers\n", reg_cnt);

			ret = pnw_topaz_write_reg(dev_priv,
				p_command,
				reg_cnt,
				cur_cmd_header->core);
			break;
		case MTX_CMDID_PAD:
			/* Ignore this command, which is used to skip
			 * some commands in user space */
			cur_cmd_size = sizeof(struct topaz_cmd_header);
			cur_cmd_size += TOPAZ_COMMON_CMD_BYTES;
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			break;
		/* ordinary commmand */
		case MTX_CMDID_START_PIC:
		case MTX_CMDID_DO_HEADER:
		case MTX_CMDID_ENCODE_SLICE:
		case MTX_CMDID_END_PIC:
		case MTX_CMDID_SETQUANT:
		case MTX_CMDID_RESET_ENCODE:
		case MTX_CMDID_ISSUEBUFF:
		case MTX_CMDID_SETUP:
		case MTX_CMDID_NULL:
			cur_cmd_header->seq = topaz_priv->topaz_cmd_count++;
			cur_cmd_header->enable_interrupt = 0;
			cur_cmd_size = sizeof(struct topaz_cmd_header);
			cur_cmd_size += TOPAZ_COMMON_CMD_BYTES;
			PNW_TOPAZ_CHECK_CMD_SIZE(cmd_size,
					cur_cmd_size, cur_cmd_id);
			if (cur_free_space < cur_cmd_size) {
				POLL_TOPAZ_FREE_FIFO_SPACE(
						PNW_TOPAZ_WORDS_PER_CMDSET,
						PNW_TOPAZ_POLL_DELAY,
						PNW_TOPAZ_POLL_RETRY,
						&cur_free_space);

				if (ret) {
					DRM_ERROR("TOPAZ: error -- ret(%d)\n",
						  ret);
					goto out;
				}
			}
			p_command = (uint32_t *)
				(cmd + sizeof(struct topaz_cmd_header));
			PSB_DEBUG_GENERAL("TOPAZ: free FIFO space %d\n",
					  cur_free_space);
			PSB_DEBUG_GENERAL("TOPAZ: write 4 words to FIFO:"
					  "0x%08x,0x%08x,0x%08x,0x%08x\n",
					  cur_cmd_header->val,
					  p_command[0],
					  TOPAZ_MTX_WB_OFFSET(
						  topaz_priv->topaz_wb_offset,
						  cur_cmd_header->core),
					  cur_cmd_header->seq);

			TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
						cur_cmd_header->val);
			TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
						p_command[0]);
			TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
						TOPAZ_MTX_WB_OFFSET(
							topaz_priv->topaz_wb_offset,
							cur_cmd_header->core));
			TOPAZ_MULTICORE_WRITE32(TOPAZSC_CR_MULTICORE_CMD_FIFO_0,
						cur_cmd_header->seq);

			cur_free_space -= 4;
			topaz_priv->topaz_cmd_count %= MAX_TOPAZ_CMD_COUNT;
			topaz_mtx_kick(dev_priv, 0, 1);
#ifdef SYNC_FOR_EACH_COMMAND
			pnw_wait_on_sync(dev_priv, cur_cmd_header->seq,
					 topaz_priv->topaz_mtx_wb +
					 cur_cmd_header->core *
					 MTX_WRITEBACK_DATASIZE_ROUND + 1);
#endif
			break;
		default:
			DRM_ERROR("TOPAZ: unsupported command id: %x\n",
				  cur_cmd_id);
			goto out;
		}

		cmd += cur_cmd_size;
		cmd_size -= cur_cmd_size;
	}
#if PNW_TOPAZ_NO_IRQ
	PSB_DEBUG_GENERAL("reset NULL writeback to 0xffffffff,"
			  "topaz_priv->topaz_sync_addr=0x%p\n",
			  topaz_priv->topaz_sync_addr);

	*((uint32_t *)topaz_priv->topaz_sync_addr + MTX_WRITEBACK_VALUE) = ~0;
	pnw_topaz_kick_null_cmd(dev_priv, 0,
				topaz_priv->topaz_sync_offset, sync_seq, 0);

	if (0 != pnw_wait_on_sync(dev_priv, sync_seq,
				  topaz_priv->topaz_sync_addr + MTX_WRITEBACK_VALUE)) {
		DRM_ERROR("TOPAZSC: Polling the writeback of last command"
			  " failed!\n");
		topaz_read_core_reg(dev_priv,  0, 0x5, &reg_val);
		DRM_ERROR("TOPAZSC: PC pointer of core 0 is %x\n", reg_val);
		topaz_read_core_reg(dev_priv, 1, 0x5, &reg_val);
		DRM_ERROR("TOPAZSC: PC pointer of core 1 is %x\n", reg_val);
		TOPAZ_MULTICORE_READ32(TOPAZSC_CR_MULTICORE_CMD_FIFO_1,
				       &reg_val);
		reg_val &= MASK_TOPAZSC_CR_CMD_FIFO_SPACE;
		DRM_ERROR("TOPAZSC: Free words in command FIFO %d\n", reg_val);
		DRM_ERROR("TOPAZSC: Last writeback of core 0 %d\n",
			  *(topaz_priv->topaz_mtx_wb +  1));
		DRM_ERROR("TOPAZSC: Last writeback of core 1 %d\n",
			  *(topaz_priv->topaz_mtx_wb +
			    MTX_WRITEBACK_DATASIZE_ROUND + 1));
	}

	PSB_DEBUG_GENERAL("Kicked command with sequence 0x%08x,"
			  " and polling it, got 0x%08x\n",
			  sync_seq,
			  *(topaz_priv->topaz_sync_addr + MTX_WRITEBACK_VALUE));
	PSB_DEBUG_GENERAL("Can handle unfence here, but let fence"
			  " polling do it\n");
	topaz_priv->topaz_busy = 0;
#else
	PSB_DEBUG_GENERAL("Kick command with sequence %x\n", sync_seq);
	topaz_priv->topaz_busy = 1; /* This may be reset in topaz_setup_fw*/
	pnw_topaz_kick_null_cmd(dev_priv, 0,
				topaz_priv->topaz_sync_offset,
				sync_seq, 1);
#endif
out:
	return ret;
}

int pnw_topaz_dequeue_send(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_cmd_queue *topaz_cmd = NULL;
	int ret;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	PSB_DEBUG_GENERAL("TOPAZ: dequeue command and send it to topaz\n");

	if (list_empty(&topaz_priv->topaz_queue)) {
		topaz_priv->topaz_busy = 0;
		return 0;
	}

	topaz_cmd = list_first_entry(&topaz_priv->topaz_queue,
		struct pnw_topaz_cmd_queue, head);
	if (dev_priv->topaz_ctx) {
		topaz_priv->topaz_busy = 1;

		PSB_DEBUG_GENERAL("TOPAZ: queue has id %08x\n",
			topaz_cmd->sequence);
		ret = pnw_topaz_send(dev, topaz_cmd->cmd, topaz_cmd->cmd_size,
			topaz_cmd->sequence);
		if (ret) {
			DRM_ERROR("TOPAZ: pnw_topaz_send failed.\n");
			ret = -EINVAL;
		}
	} else {
		/* Since context has been removed, we should discard the
		   encoding commands in the queue and release fence */
		PSB_DEBUG_TOPAZ("TOPAZ: Context has been removed!\n");
		TOPAZ_MTX_WB_WRITE32(topaz_priv->topaz_sync_addr,
			0, MTX_WRITEBACK_VALUE,
			topaz_cmd->sequence);
		topaz_priv->topaz_busy = 0;
		psb_fence_handler(dev, LNC_ENGINE_ENCODE);
		ret = -EINVAL;
	}

	list_del(&topaz_cmd->head);
	kfree(topaz_cmd->cmd);
	kfree(topaz_cmd);

	return ret;
}

void topaz_mtx_kick(struct drm_psb_private *dev_priv, uint32_t core_id, uint32_t kick_count)
{
	PSB_DEBUG_GENERAL("TOPAZ: kick core(%d) mtx count(%d).\n",
			  core_id, kick_count);
	topaz_set_mtx_target(dev_priv, core_id, 0);
	MTX_WRITE32(MTX_CR_MTX_KICK, kick_count, core_id);
	return;
}

int pnw_check_topaz_idle(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	/*HW is stuck. Need to power off TopazSC to reset HW*/
	if (topaz_priv->topaz_needs_reset)
		return 0;

	/* Even if topaz_ctx is null, which means there is no more encoding
	    commands, return busy here to avoid runtime PM power down Topaz,
	    but let Topaz suspend(D0i3) task will power down Topaz */
	if (topaz_priv->topaz_busy)
		return -EBUSY;

	if (dev_priv->topaz_ctx == NULL)
		return 0;

	if (topaz_priv->topaz_hw_busy) {
		PSB_DEBUG_PM("TOPAZ: %s, HW is busy\n", __func__);
		return -EBUSY;
	}

	return 0; /* we think it is idle */
}



void pnw_topaz_flush_cmd_queue(struct pnw_topaz_private *topaz_priv)
{
	struct pnw_topaz_cmd_queue *entry, *next;

	if (list_empty(&topaz_priv->topaz_queue))
		return;

	/* flush all command in queue */
	list_for_each_entry_safe(entry, next,
				 &topaz_priv->topaz_queue,
				 head) {
		list_del(&entry->head);
		kfree(entry->cmd);
		kfree(entry);
	}

	return;
}

void pnw_topaz_handle_timeout(struct ttm_fence_device *fdev)
{
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	u32 cur_seq;

	TOPAZ_MTX_WB_READ32(topaz_priv->topaz_sync_addr,
			    0, MTX_WRITEBACK_VALUE, &cur_seq);

	DRM_ERROR("TOPAZ:last sync seq:0x%08x (MTX)" \
		      " vs 0x%08x(fence)\n",
		      cur_seq, dev_priv->sequence[LNC_ENGINE_ENCODE]);

	DRM_ERROR("TOPAZ: current codec is %s\n",
			codec_to_string(topaz_priv->topaz_cur_codec));
	pnw_topaz_flush_cmd_queue(topaz_priv);
	topaz_priv->topaz_needs_reset = 1;
	topaz_priv->topaz_busy = 0;
}


void pnw_topaz_enableirq(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	/* uint32_t ier = dev_priv->vdc_irq_mask | _PNW_IRQ_TOPAZ_FLAG; */

	PSB_DEBUG_IRQ("TOPAZ: enable IRQ\n");

	/* Only enable the master core IRQ*/
	TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_INTENAB,
		      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_MAS_INTEN) |
		      /* F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTEN_MVEA) | */
		      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTEN_MMU_FAULT) |
		      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTEN_MTX) |
		      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTEN_MTX_HALT),
		      0);

	/* write in sysirq.c */
	/* PSB_WVDC32(ier, PSB_INT_ENABLE_R); /\* essential *\/ */
}

void pnw_topaz_disableirq(struct drm_device *dev)
{

	struct drm_psb_private *dev_priv = dev->dev_private;
	/*uint32_t ier = dev_priv->vdc_irq_mask & (~_PNW_IRQ_TOPAZ_FLAG); */

	PSB_DEBUG_IRQ("TOPAZ: disable IRQ\n");

	TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_INTENAB, 0, 0);

	/* write in sysirq.c */
	/* PSB_WVDC32(ier, PSB_INT_ENABLE_R); /\* essential *\/ */
}

void psb_powerdown_topaz(struct work_struct *work)
{
	struct pnw_topaz_private *topaz_priv =
		container_of(work, struct pnw_topaz_private,
					topaz_suspend_wq.work);
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)topaz_priv->dev->dev_private;

	if (!dev_priv->topaz_disabled)
		ospm_apm_power_down_topaz(topaz_priv->dev);
}

