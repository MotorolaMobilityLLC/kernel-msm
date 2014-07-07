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
 **************************************************************************/

/* include headers */
/* #define DRM_DEBUG_CODE 2 */
#include <drm/drmP.h>
#include <drm/drm.h>

#include "psb_drv.h"
#include "tng_topaz.h"
#include "psb_powermgmt.h"
#include "tng_topaz_hw_reg.h"
/*#include "private_data.h"*/

#include <linux/io.h>
#include <linux/delay.h>

#define TOPAZ_MAX_COMMAND_IN_QUEUE 0x1000
#define MASK_MTX_INT_ENAB 0x4000ff00

#define LOOP_COUNT 10000

#define MTX_PC		(0x05)

#define tng__max(a, b) ((a)> (b)) ? (a) : (b)
#define tng__min(a, b) ((a) < (b)) ? (a) : (b)

/*static uint32_t setv_cnt = 0;*/

enum MTX_MESSAGE_ID {
	MTX_MESSAGE_ACK,   /* !< (no data)\n Null command does nothing\n */
	MTX_MESSAGE_CODED, /* !< (no data)\n Null command does nothing\n */
} ;

/* static function define */
static int tng_topaz_deliver_command(
	struct drm_device *dev,
	struct drm_file *file_priv,
	struct ttm_buffer_object *cmd_buffer,
	uint32_t cmd_offset,
	uint32_t cmd_size,
	void **topaz_cmd, uint32_t sequence,
	int copy_cmd);

static int tng_topaz_send(
	struct drm_device *dev,
	struct drm_file *file_priv,
	void *cmd,
	uint32_t cmd_size,
	uint32_t sync_seq);

static int tng_topaz_save_command(
	struct drm_device *dev,
	struct drm_file *file_priv,
	void *cmd,
	uint32_t cmd_size,
	uint32_t sequence);

static void tng_topaz_getvideo(
	struct drm_device *dev,
	struct psb_video_ctx *video_ctx);

static void tng_topaz_setvideo(
	struct drm_device *dev,
	struct psb_video_ctx *video_ctx);

void mtx_start(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	mtx_set_target(dev_priv);
	MTX_WRITE32(MTX_CR_MTX_ENABLE, MASK_MTX_MTX_ENABLE);
}

void mtx_stop(struct drm_psb_private *dev_priv)
{
	mtx_set_target(dev_priv);
	MTX_WRITE32(MTX_CR_MTX_ENABLE, MASK_MTX_MTX_TOFF);
}

void mtx_kick(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	mtx_set_target(dev_priv);
	PSB_DEBUG_TOPAZ("TOPAZ: Kick MTX to start\n");
	MTX_WRITE32(MTX_CR_MTX_KICK, 1);
}

void tng_set_consumer(struct drm_device *dev, uint32_t consumer)
{
	unsigned int reg_val;
	struct drm_psb_private *dev_priv = dev->dev_private;

	MULTICORE_READ32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOMTX << 2), &reg_val);

	reg_val = F_INSERT(reg_val, consumer, WB_CONSUMER);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOMTX << 2), reg_val);
}

uint32_t tng_get_consumer(struct drm_device *dev)
{
	unsigned int reg_val;
	struct drm_psb_private *dev_priv = dev->dev_private;

	MULTICORE_READ32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOMTX << 2), &reg_val);

	return F_DECODE(reg_val, WB_CONSUMER);
}

void tng_set_producer(struct drm_device *dev, uint32_t producer)
{
	unsigned int reg_val;
	struct drm_psb_private *dev_priv = dev->dev_private;

	MULTICORE_READ32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOHOST << 2), &reg_val);

	reg_val = F_INSERT(reg_val, producer, WB_PRODUCER);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOHOST << 2), reg_val);
}

uint32_t tng_get_producer(struct drm_device *dev)
{
	unsigned int reg_val;
	struct drm_psb_private *dev_priv = dev->dev_private;

	MULTICORE_READ32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
		(MTX_SCRATCHREG_TOHOST << 2), &reg_val);

	return F_DECODE(reg_val, WB_PRODUCER);
}

uint32_t tng_wait_for_ctrl(struct drm_device *dev, uint32_t control)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int32_t ret = 0;
	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_ISEQUAL,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2),
		control, 0x80000000);
	if (ret)
		DRM_ERROR("Wait for register timeout");

	return ret;
}

uint32_t tng_serialize_enter(struct drm_device *dev)
{
	uint32_t reg_val;
	int32_t ret;
	struct drm_psb_private *dev_priv = dev->dev_private;
	/*
	* Poll for idle register to tell that both HW
	* and FW are idle (`FW_IDLE_STATUS_IDLE` state)
	*/
	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_ISEQUAL,
		MTX_SCRATCHREG_IDLE,
		F_ENCODE(FW_IDLE_STATUS_IDLE, FW_IDLE_REG_STATUS),
		MASK_FW_IDLE_REG_STATUS);
	if (ret)
		DRM_ERROR("Wait for register timeout");

	MULTICORE_READ32(MTX_SCRATCHREG_IDLE, &reg_val);

	return F_EXTRACT(reg_val, FW_IDLE_REG_RECEIVED_COMMANDS);
}

void tng_serialize_exit(struct drm_device *dev, uint32_t enter_token)
{
	int32_t ret;
	struct drm_psb_private *dev_priv = dev->dev_private;
	/*
	* Poll for idle register to tell that both HW
	* and FW are idle (`FW_IDLE_STATUS_IDLE` state)
	*/
	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
		MTX_SCRATCHREG_IDLE,
		F_ENCODE(enter_token, FW_IDLE_REG_RECEIVED_COMMANDS),
		MASK_FW_IDLE_REG_RECEIVED_COMMANDS);
	if (ret)
		DRM_ERROR("Wait for register timeout");
}

static void tng_topaz_Int_clear(
	struct drm_psb_private *dev_priv,
	uint32_t intClearMask)
{
	unsigned long irq_flags;
	struct tng_topaz_private *topaz_priv;

	topaz_priv = dev_priv->topaz_private;
	spin_lock_irqsave(&topaz_priv->topaz_lock, irq_flags);
	/* clear interrupt */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_INT_CLEAR,
		intClearMask);
	spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);
}

struct psb_video_ctx *get_ctx_from_fp(
	struct drm_device *dev, struct file *filp)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_video_ctx *pos;
	unsigned long irq_flags;
	int entrypoint;

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);

	list_for_each_entry(pos, &dev_priv->video_ctx, head) {
		if (pos->filp == filp) {
			entrypoint = pos->ctx_type & 0xff;
			if (entrypoint == VAEntrypointEncSlice ||
			    entrypoint == VAEntrypointEncPicture) {
				spin_unlock_irqrestore(
					&dev_priv->video_ctx_lock,
					irq_flags);
				return pos;
			}
		}
	}

	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);
	return NULL;
}

uint32_t get_ctx_cnt(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_video_ctx *pos, *n;
	int count = 0;
	int entrypoint;

	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		entrypoint = pos->ctx_type & 0xff;
		if (entrypoint == VAEntrypointEncSlice ||
		    entrypoint == VAEntrypointEncPicture)
			count++;
	}

	return count;
}

int32_t dispatch_wb_message_polling(
	struct drm_device *dev,
	struct drm_file *file_priv,
	int32_t sync_seq,
	unsigned char *command)
{
	struct psb_video_ctx *video_ctx;
	struct tng_topaz_private *topaz_priv;
	struct drm_psb_private *dev_priv;
	struct tng_topaz_cmd_header *cur_cmd_header;
	struct IMG_WRITEBACK_MSG *wb_msg;
	int32_t ret;

	dev_priv = (struct drm_psb_private *) dev->dev_private;
	if (!dev_priv) {
		DRM_ERROR("Failed to get dev_priv\n");
		return -1;
	}

	topaz_priv = dev_priv->topaz_private;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);

	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}

	topaz_priv->consumer = tng_get_consumer(dev);
	topaz_priv->producer = tng_get_producer(dev);

	/* Read and compare consumer and producer */
	if (topaz_priv->producer == topaz_priv->consumer) {
		PSB_DEBUG_TOPAZ("TOPAZ: producer = consumer = %d",
			topaz_priv->producer);
		PSB_DEBUG_TOPAZ("polling producer until change\n");
		/* if the same -> poll on Producer change */
		ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOHOST << 2),
			topaz_priv->consumer, MASK_WB_PRODUCER);
		if (ret) {
			wb_msg = (struct IMG_WRITEBACK_MSG *)
				video_ctx->wb_addr[topaz_priv->consumer];
			DRM_ERROR("Polling timeout, ui32CmdWord = %08x, " \
				"ui32Data = %08x, ui32ExtraData = %08x, " \
				"ui32WritebackVal = %08x, " \
				"ui32CodedBufferConsumed = %08x\n",
				wb_msg->ui32CmdWord, wb_msg->ui32Data,
				wb_msg->ui32ExtraData, wb_msg->ui32WritebackVal,
				wb_msg->ui32CodedBufferConsumed);

			return ret;
		}

		topaz_priv->producer = tng_get_producer(dev);
	}

	/* Dispatch new messages */
	do {
		PSB_DEBUG_TOPAZ("TOPAZ: Dispatch write back message, " \
			"producer = %d, consumer = %d\n",
			topaz_priv->producer, topaz_priv->consumer);

		ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOHOST << 2),
			topaz_priv->consumer, MASK_WB_PRODUCER);
		if (ret) {
			DRM_ERROR("Wait for register timeout");
			return ret;
		}

		topaz_priv->consumer++;
		if (topaz_priv->consumer == WB_FIFO_SIZE)
			topaz_priv->consumer = 0;

		tng_set_consumer(dev, topaz_priv->consumer);

	} while (topaz_priv->consumer != topaz_priv->producer);

	cur_cmd_header = (struct tng_topaz_cmd_header *)command;

	if (cur_cmd_header->id != MTX_CMDID_ENCODE_FRAME &&
	    cur_cmd_header->id != MTX_CMDID_ISSUEBUFF)
		return 0;

	PSB_DEBUG_TOPAZ("TOPAZ: Handle context saving/fence " \
		"handler/dequeue send on ENCODE_FRAME command\n");

	if (get_ctx_cnt(dev) > 1) {
		PSB_DEBUG_TOPAZ("TOPAZ: More than one context, " \
			"save current context\n");
		if (topaz_priv->cur_context->codec != IMG_CODEC_JPEG) {
			ret = tng_topaz_save_mtx_state(dev);
			if (ret) {
				DRM_ERROR("Failed to save mtx status");
				return ret;
			}
		}
	}

	*topaz_priv->topaz_sync_addr = sync_seq;
	(topaz_priv->cur_context)->handle_sequence_needed = false;
	psb_fence_handler(dev, LNC_ENGINE_ENCODE);

	topaz_priv->topaz_busy = 1;
	tng_topaz_dequeue_send(dev);

	return ret;
}

int32_t dispatch_wb_message_irq(struct drm_device *dev)
{
	struct tng_topaz_private *topaz_priv;
	struct drm_psb_private *dev_priv;
	/* uint32_t crMultiCoreIntStat;*/
	/* struct psb_video_ctx *video_ctx; */
	/* struct IMG_WRITEBACK_MSG *wb_msg; */
	int32_t ret;
	int32_t count = 0;

	dev_priv = (struct drm_psb_private *) dev->dev_private;
	if (!dev_priv) {
		DRM_ERROR("Failed to get dev_priv\n");
		return -1;
	}

	topaz_priv = dev_priv->topaz_private;

	topaz_priv->consumer = tng_get_consumer(dev);

	do {
		topaz_priv->producer = tng_get_producer(dev);
		count++;
	} while (topaz_priv->producer == topaz_priv->consumer &&
		 count < 300000);

	if (count == 300000) {
		DRM_ERROR("Waiting for IRQ timeout\n");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: Producer = %d, Consumer = %d\n",
		topaz_priv->producer, topaz_priv->consumer);

	do {
		ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOHOST << 2),
			topaz_priv->consumer, MASK_WB_PRODUCER);
		if (ret)
			return ret;

		topaz_priv->consumer++;
		if (topaz_priv->consumer == WB_FIFO_SIZE)
			topaz_priv->consumer = 0;

		/* Indicate buffer used by consmer is available */
		tng_set_consumer(dev, topaz_priv->consumer);
	} while (topaz_priv->consumer != topaz_priv->producer);

	return 0;
}

int32_t tng_wait_on_sync(
	struct drm_device *dev,
	int32_t sync_seq,
	unsigned long cmd_id)
{
	struct tng_topaz_private *topaz_priv;
	struct drm_psb_private *dev_priv;
	/* struct IMG_WRITEBACK_MSG *wb_msg; */
	/* struct tng_topaz_cmd_header *cur_cmd_header; */
	int32_t ret;
	int32_t count = 0;
	/* uint32_t crMultiCoreIntStat; */

	dev_priv = (struct drm_psb_private *) dev->dev_private;
	if (!dev_priv) {
		DRM_ERROR("Failed to get dev_priv\n");
		return -1;
	}

	topaz_priv = dev_priv->topaz_private;

	topaz_priv->consumer = tng_get_consumer(dev);
	topaz_priv->producer = tng_get_producer(dev);

#ifdef TOPAZHP_IRQ_ENABLED
	while (topaz_priv->producer == topaz_priv->consumer &&
		count < LOOP_COUNT * 1000) {
		topaz_priv->producer = tng_get_producer(dev);
		count++;
	}

	if (count == LOOP_COUNT * 1000) {
		DRM_ERROR("Sync cmd(%s) timeout\n", \
			cmd_to_string(cmd_id & (~MTX_CMDID_PRIORITY)));
		return -1;
	}
#else
	topaz_priv->producer = tng_get_producer(dev);

	/* Read and compare consumer and producer */
	if (topaz_priv->producer == topaz_priv->consumer) {
		PSB_DEBUG_TOPAZ("TOPAZ: producer = consumer = %d, " \
			"polling producer until change\n",
			topaz_priv->producer);
		/* if the same -> poll on Producer change */
		ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOHOST << 2),
			topaz_priv->consumer, MASK_WB_PRODUCER);
		if (ret) {

			DRM_ERROR("Polling timeout\n");

			return ret;
		}

		topaz_priv->producer = tng_get_producer(dev);
	}
#endif
	/* Dispatch new messages */
	do {
		PSB_DEBUG_TOPAZ("TOPAZ: Dispatch write back message, " \
			"producer = %d, consumer = %d\n",
			topaz_priv->producer, topaz_priv->consumer);
		/*
		ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_NOTEQUAL,
			TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOHOST << 2),
			topaz_priv->consumer, MASK_WB_PRODUCER);
		if (ret)
			return ret;
		*/

		topaz_priv->consumer++;
		if (topaz_priv->consumer == WB_FIFO_SIZE)
			topaz_priv->consumer = 0;

		/* Indicate buffer used by consmer is available */
		tng_set_consumer(dev, topaz_priv->consumer);
	} while (topaz_priv->consumer != topaz_priv->producer);

	/* When IRQ enabled, the belowing code will be called in ISR */
#ifdef TOPAZHP_IRQ_ENABLED
	return 0;
#endif

	if (cmd_id != MTX_CMDID_ENCODE_FRAME &&
	    cmd_id != MTX_CMDID_ISSUEBUFF)
		return 0;

	PSB_DEBUG_TOPAZ("TOPAZ: Handle context saving/fence " \
		"handler/dequeue send on ENCODE_FRAME command\n");

	if (get_ctx_cnt(dev) > 1) {
		PSB_DEBUG_TOPAZ("TOPAZ: More than one context, " \
			"save current context\n");
		if (topaz_priv->cur_context->codec != IMG_CODEC_JPEG) {
			ret = tng_topaz_save_mtx_state(dev);
			if (ret) {
				DRM_ERROR("Failed to save mtx status");
				return ret;
			}
		}
	}

	*topaz_priv->topaz_sync_addr = sync_seq;
	(topaz_priv->cur_context)->handle_sequence_needed = false;
	psb_fence_handler(dev, LNC_ENGINE_ENCODE);

	topaz_priv->topaz_busy = 1;
	tng_topaz_dequeue_send(dev);

	return ret;
}

bool tng_topaz_interrupt(void *pvData)
{
	struct drm_device *dev;
	/* struct drm_minor *minor; */
	struct tng_topaz_private *topaz_priv;
	struct drm_psb_private *dev_priv;
	uint32_t crMultiCoreIntStat;
	struct psb_video_ctx *video_ctx;
	struct IMG_WRITEBACK_MSG *wb_msg;
	unsigned long flags;

	if (pvData == NULL) {
		DRM_ERROR("Invalid params\n");
		return false;
	}
	dev = (struct drm_device *)pvData;
	/*
	minor = (struct drm_minor *)container_of(dev, struct drm_minor, dev);
	file_priv = (struct drm_file *)container_of(minor,
			struct drm_file, minor);
	*/
	/*
	if (!ospm_power_is_hw_on(OSPM_VIDEO_ENC_ISLAND)) {
		DRM_ERROR("ERROR: interrupt arrived but HW is power off\n");
		return false;
	}
	*/
	dev_priv = (struct drm_psb_private *) dev->dev_private;
	if (!dev_priv) {
		DRM_ERROR("Failed to get dev_priv\n");
		return false;
	}

	topaz_priv = dev_priv->topaz_private;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_INT_STAT,
			 &crMultiCoreIntStat);

	/* if interrupts enabled and fired */
	if ((crMultiCoreIntStat & MASK_TOPAZHP_TOP_CR_INT_STAT_MTX) ==
		MASK_TOPAZHP_TOP_CR_INT_STAT_MTX) {
		PSB_DEBUG_TOPAZ("TOPAZ: Get MTX interrupt , clear IRQ\n");
		tng_topaz_Int_clear(dev_priv, MASK_TOPAZHP_TOP_CR_INTCLR_MTX);
	} else
		return 0;

	topaz_priv->consumer = tng_get_consumer(dev);
	topaz_priv->producer = tng_get_producer(dev);

	spin_lock_irqsave(&(topaz_priv->ctx_spinlock), flags);
	/* Encode ctx has already been destroyed by user-space process */
	if (NULL == topaz_priv->irq_context) {
		PSB_DEBUG_TOPAZ("TOPAZ: ctx destroyed before ISR.\n");
		spin_unlock_irqrestore(&(topaz_priv->ctx_spinlock), flags);
		return true;
	}

	video_ctx = topaz_priv->irq_context;
	wb_msg = (struct IMG_WRITEBACK_MSG *)
		video_ctx->wb_addr[(topaz_priv->producer == 0) \
			? 31 \
			: topaz_priv->producer - 1];

	PSB_DEBUG_TOPAZ("TOPAZ: Dispatch write back message:\n");
	PSB_DEBUG_TOPAZ("producer = %d, consumer = %d\n",
		topaz_priv->producer, topaz_priv->consumer);

	while (topaz_priv->consumer != topaz_priv->producer) {
		topaz_priv->consumer++;
		if (topaz_priv->consumer == WB_FIFO_SIZE)
			topaz_priv->consumer = 0;
		tng_set_consumer(dev, topaz_priv->producer);
	};

	PSB_DEBUG_TOPAZ("TOPAZ: Context %p(%s):\n",
			video_ctx, codec_to_string(video_ctx->codec));
	PSB_DEBUG_TOPAZ("TOPAZ: frame %d, command %s IRQ\n",
			video_ctx->frame_count,
			cmd_to_string(wb_msg->ui32CmdWord));

	video_ctx->frame_count++;
#if 0
	if (video_ctx->codec == IMG_CODEC_JPEG) {
		if (wb_msg->ui32CmdWord != MTX_CMDID_NULL) {
			/* The LAST ISSUEBUF cmd means encoding complete */
			if (--topaz_priv->issuebuf_cmd_count) {
				PSB_DEBUG_TOPAZ("TOPAZ: JPEG ISSUEBUF cmd " \
					  "count left %d, return\n", \
					  topaz_priv->issuebuf_cmd_count);
			return true;
			}
		} else {
			return true;
		}
	}
#endif
	*topaz_priv->topaz_sync_addr = wb_msg->ui32WritebackVal;
	video_ctx->handle_sequence_needed = false;
	spin_unlock_irqrestore(&(topaz_priv->ctx_spinlock), flags);

	PSB_DEBUG_TOPAZ("TOPAZ: Set seq %08x, " \
		"dqueue cmd and schedule other work queue\n",
		wb_msg->ui32WritebackVal);
	psb_fence_handler(dev, LNC_ENGINE_ENCODE);

	/* Launch the task anyway */
	schedule_delayed_work(&topaz_priv->topaz_suspend_work, 0);

	return true;
}

static int tng_submit_encode_cmdbuf(struct drm_device *dev,
				    struct drm_file *file_priv,
				    struct ttm_buffer_object *cmd_buffer,
				    uint32_t cmd_offset, uint32_t cmd_size,
				    struct ttm_fence_object *fence)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int ret = 0;
	void *cmd;
	uint32_t sequence = dev_priv->sequence[LNC_ENGINE_ENCODE];
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx = NULL;

	PSB_DEBUG_TOPAZ("TOPAZ: command submit, topaz busy = %d\n",
		topaz_priv->topaz_busy);

	if (topaz_priv->topaz_fw_loaded == 0) {
		/* #.# load fw to driver */
		PSB_DEBUG_TOPAZ("TOPAZ:load /lib/firmware/topazhp_fw.bin\n");
		if (Is_Secure_Fw())
			ret = tng_securefw(dev, "topaz", "VEC",
					   TNG_IMR6L_MSG_REGADDR);
		else
			ret = tng_rawfw(dev, "topaz");
		if (ret) {
			/* FIXME: find a proper return value */
			DRM_ERROR("TOPAX:load firmware from storage failed\n");
			return -EFAULT;
		}
		topaz_priv->topaz_fw_loaded = 1;
	}

	/* # if topaz need to reset, reset it */
	if (topaz_priv->topaz_needs_reset) {
		/* #.# reset it */
		PSB_DEBUG_TOPAZ("TOPAZ: needs reset.\n");

		tng_topaz_reset(dev_priv);

		PSB_DEBUG_TOPAZ("TOPAZ: reset ok.\n");

		if (Is_Secure_Fw() == 0) {
			video_ctx = get_ctx_from_fp(dev, file_priv->filp);
			if (!video_ctx) {
				DRM_ERROR("Failed to get context from fp\n");
				return -1;
			}

			/* #.# upload firmware */
			ret = tng_topaz_setup_fw(dev, video_ctx,
						 topaz_priv->cur_codec);
			if (ret) {
				DRM_ERROR("TOPAZ: upload FW to HW failed\n");
				return ret;
			}
		}
	}

	if (!topaz_priv->topaz_busy) {
		/* # direct map topaz command if topaz is free */
		PSB_DEBUG_TOPAZ("TOPAZ:direct send command,sequence %08x\n",
				  sequence);

		ret = tng_topaz_deliver_command(dev, file_priv,
			cmd_buffer, cmd_offset, cmd_size, NULL, sequence, 0);

		if (ret) {
			DRM_ERROR("TOPAZ: failed to extract cmd...\n");
			return ret;
		}
	} else {
		PSB_DEBUG_TOPAZ("TOPAZ: queue command of sequence %08x\n",
				  sequence);
		cmd = NULL;

		ret = tng_topaz_deliver_command(dev, file_priv,
			cmd_buffer, cmd_offset, cmd_size, &cmd, sequence, 1);
		if (cmd == NULL || ret) {
			DRM_ERROR("TOPAZ: map command for save fialed\n");
			return ret;
		}

		ret = tng_topaz_save_command(dev, file_priv,
			cmd, cmd_size, sequence);
		if (ret)
			DRM_ERROR("TOPAZ: save command failed\n");
	}

	return ret;
}

static int tng_topaz_save_command(
	struct drm_device *dev,
	struct drm_file *file_priv,
	void *cmd,
	uint32_t cmd_size,
	uint32_t sequence)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_cmd_queue *topaz_cmd;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	PSB_DEBUG_TOPAZ("TOPAZ: queue command,sequence: %08x..\n",
		sequence);

	topaz_cmd = kzalloc(sizeof(struct tng_topaz_cmd_queue),
			    GFP_KERNEL);
	if (!topaz_cmd) {
		DRM_ERROR("TOPAZ: out of memory....\n");
		return -ENOMEM;
	}

	topaz_cmd->file_priv = file_priv;
	topaz_cmd->cmd = cmd;
	topaz_cmd->cmd_size = cmd_size;
	topaz_cmd->sequence = sequence;

	/* Avoid race condition with dequeue buffer in kernel task */
	mutex_lock(&topaz_priv->topaz_mutex);
	list_add_tail(&topaz_cmd->head, &topaz_priv->topaz_queue);
	mutex_unlock(&topaz_priv->topaz_mutex);

	if (!topaz_priv->topaz_busy) {
		/* topaz_priv->topaz_busy = 1; */
		PSB_DEBUG_TOPAZ("TOPAZ: need immediate dequeue...\n");
		tng_topaz_dequeue_send(dev);
		PSB_DEBUG_TOPAZ("TOPAZ: after dequeue command\n");
	}

	return 0;
}

int tng_cmdbuf_video(struct drm_file *file_priv,
		     struct list_head *validate_list,
		     uint32_t fence_type,
		     struct drm_psb_cmdbuf_arg *arg,
		     struct ttm_buffer_object *cmd_buffer,
		     struct psb_ttm_fence_rep *fence_arg)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct ttm_fence_object *fence = NULL;
	int32_t ret = 0;

	PSB_DEBUG_TOPAZ("TOPAZ : enter %s cmdsize: %d\n", __func__,
			  arg->cmdbuf_size);

	ret = tng_submit_encode_cmdbuf(dev, file_priv, cmd_buffer,
		arg->cmdbuf_offset, arg->cmdbuf_size, fence);
	if (ret)
		return ret;

	/* workaround for interrupt issue */
	psb_fence_or_sync(file_priv, LNC_ENGINE_ENCODE,
		fence_type, arg->fence_flags,
		validate_list, fence_arg, &fence);
	PSB_DEBUG_TOPAZ("TOPAZ : current fence 0x%08x\n",
		dev_priv->sequence[LNC_ENGINE_ENCODE]);
	if (fence)
		ttm_fence_object_unref(&fence);

	spin_lock(&cmd_buffer->bdev->fence_lock);
	if (cmd_buffer->sync_obj != NULL)
		ttm_fence_sync_obj_unref(&cmd_buffer->sync_obj);
	spin_unlock(&cmd_buffer->bdev->fence_lock);

	PSB_DEBUG_TOPAZ("TOPAZ exit %s\n", __func__);
	return ret;
}

#define SHIFT_MTX_CMDWORD_ID    (0)
#define MASK_MTX_CMDWORD_ID     (0xff << SHIFT_MTX_CMDWORD_ID)
#define SHIFT_MTX_CMDWORD_CORE  (8)
#define MASK_MTX_CMDWORD_CORE   (0xff << SHIFT_MTX_CMDWORD_CORE)
#define SHIFT_MTX_CMDWORD_COUNT (16)
#define MASK_MTX_CMDWORD_COUNT  (0xffff << SHIFT_MTX_CMDWORD_COUNT)

#define SHIFT_MTX_WBWORD_ID    (0)
#define MASK_MTX_WBWORD_ID     (0xff << SHIFT_MTX_WBWORD_ID)
#define SHIFT_MTX_WBWORD_CORE  (8)
#define MASK_MTX_WBWORD_CORE   (0xff << SHIFT_MTX_WBWORD_CORE)

#if 0
static int tng_error_dump_reg(struct drm_psb_private *dev_priv)
{
	uint32_t reg_val;
	PSB_DEBUG_TOPAZ("MULTICORE Registers:\n");
	MULTICORE_READ32(0x00, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_SRST %08x\n", reg_val);
	MULTICORE_READ32(0x04, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_INT_STAT %08x\n", reg_val);
	MULTICORE_READ32(0x08, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_MTX_INT_ENAB %08x\n", reg_val);
	MULTICORE_READ32(0x0C, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_HOST_INT_ENAB %08x\n", reg_val);
	MULTICORE_READ32(0x10, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_INT_CLEAR %08x\n", reg_val);
	MULTICORE_READ32(0x14, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_MAN_CLK_GATE %08x\n", reg_val);
	MULTICORE_READ32(0x18, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZ_MTX_C_RATIO %08x\n", reg_val);
	MULTICORE_READ32(0x1c, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_STATUS %08x\n", reg_val);
	MULTICORE_READ32(0x1c, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_STATUS %08x\n", reg_val);
	MULTICORE_READ32(0x20, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_MEM_REQ %08x\n", reg_val);
	MULTICORE_READ32(0x24, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_CONTROL0 %08x\n", reg_val);
	MULTICORE_READ32(0x28, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_CONTROL1 %08x\n", reg_val);
	MULTICORE_READ32(0x2c , &reg_val);
	PSB_DEBUG_TOPAZ("MMU_CONTROL2 %08x\n", reg_val);
	MULTICORE_READ32(0x30, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_DIR_LIST_BASE %08x\n", reg_val);
	MULTICORE_READ32(0x38, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_TILE %08x\n", reg_val);
	MULTICORE_READ32(0x44, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_DEBUG_MSTR %08x\n", reg_val);
	MULTICORE_READ32(0x48, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_DEBUG_SLV %08x\n", reg_val);
	MULTICORE_READ32(0x50, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_CORE_SEL_0 %08x\n", reg_val);
	MULTICORE_READ32(0x54, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_CORE_SEL_1 %08x\n", reg_val);
	MULTICORE_READ32(0x58, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_HW_CFG %08x\n", reg_val);
	MULTICORE_READ32(0x60, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_CMD_FIFO_WRITE %08x\n", reg_val);
	MULTICORE_READ32(0x64, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_CMD_FIFO_WRITE_SPACE %08x\n", reg_val);
	MULTICORE_READ32(0x70, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZ_CMD_FIFO_READ %08x\n", reg_val);
	MULTICORE_READ32(0x74, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZ_CMD_FIFO_READ_AVAILABLE %08x\n", reg_val);
	MULTICORE_READ32(0x78, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZ_CMD_FIFO_FLUSH %08x\n", reg_val);
	MULTICORE_READ32(0x80, &reg_val);
	PSB_DEBUG_TOPAZ("MMU_TILE_EXT %08x\n", reg_val);
	MULTICORE_READ32(0x100, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_1 %08x\n", reg_val);
	MULTICORE_READ32(0x104, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_2 %08x\n", reg_val);
	MULTICORE_READ32(0x108, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_3 %08x\n", reg_val);
	MULTICORE_READ32(0x110, &reg_val);
	PSB_DEBUG_TOPAZ("CYCLE_COUNTER %08x\n", reg_val);
	MULTICORE_READ32(0x114, &reg_val);
	PSB_DEBUG_TOPAZ("CYCLE_COUNTER_CTRL %08x\n", reg_val);
	MULTICORE_READ32(0x118, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_IDLE_PWR_MAN %08x\n", reg_val);
	MULTICORE_READ32(0x124, &reg_val);
	PSB_DEBUG_TOPAZ("DIRECT_BIAS_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x128, &reg_val);
	PSB_DEBUG_TOPAZ("INTRA_BIAS_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x12c, &reg_val);
	PSB_DEBUG_TOPAZ("INTER_BIAS_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x130, &reg_val);
	PSB_DEBUG_TOPAZ("INTRA_SCALE_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x134, &reg_val);
	PSB_DEBUG_TOPAZ("QPCB_QPCR_OFFSET %08x\n", reg_val);
	MULTICORE_READ32(0x140, &reg_val);
	PSB_DEBUG_TOPAZ("INTER_INTRA_SCALE_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x144, &reg_val);
	PSB_DEBUG_TOPAZ("SKIPPED_CODED_SCALE_TABLE %08x\n", reg_val);
	MULTICORE_READ32(0x148, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_ALPHA_COEFF_CORE0 %08x\n", reg_val);
	MULTICORE_READ32(0x14c, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_GAMMA_COEFF_CORE0 %08x\n", reg_val);
	MULTICORE_READ32(0x150, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_CUTOFF_CORE0 %08x\n", reg_val);
	MULTICORE_READ32(0x154, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_ALPHA_COEFF_CORE1 %08x\n", reg_val);
	MULTICORE_READ32(0x158, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_GAMMA_COEFF_CORE1 %08x\n", reg_val);
	MULTICORE_READ32(0x15c, &reg_val);
	PSB_DEBUG_TOPAZ("POLYNOM_CUTOFF_CORE1 %08x\n", reg_val);
	MULTICORE_READ32(0x300, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_4 %08x\n", reg_val);
	MULTICORE_READ32(0x304, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_5 %08x\n", reg_val);
	MULTICORE_READ32(0x308, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_6 %08x\n", reg_val);
	MULTICORE_READ32(0x30c, &reg_val);
	PSB_DEBUG_TOPAZ("FIRMWARE_REG_7 %08x\n", reg_val);
	MULTICORE_READ32(0x3b0, &reg_val);
	PSB_DEBUG_TOPAZ("MULTICORE_RSVD0 %08x\n", reg_val);

	PSB_DEBUG_TOPAZ("TopazHP Core Registers:\n");
	TOPAZCORE_READ32(0, 0x0, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_SRST %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x4, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_INTSTAT %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x8, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_MTX_INTENAB %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0xc, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_HOST_INTENAB %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x10, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_INTCLEAR %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x14, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_INT_COMB_SEL %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x18, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_BUSY %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x24, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_AUTO_CLOCK_GATING %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x28, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_MAN_CLOCK_GATING %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x30, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_RTM %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x34, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_RTM_VALUE %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x38, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_MB_PERFORMANCE_RESULT %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3c, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_MB_PERFORMANCE_MB_NUMBER %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x188, &reg_val);
	PSB_DEBUG_TOPAZ("FIELD_PARITY %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3d0, &reg_val);
	PSB_DEBUG_TOPAZ("WEIGHTED_PRED_CONTROL %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3d4, &reg_val);
	PSB_DEBUG_TOPAZ("WEIGHTED_PRED_COEFFS %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3e0, &reg_val);
	PSB_DEBUG_TOPAZ("WEIGHTED_PRED_INV_WEIGHT %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3f0, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_RSVD0 %08x\n", reg_val);
	TOPAZCORE_READ32(0, 0x3f4, &reg_val);
	PSB_DEBUG_TOPAZ("TOPAZHP_CRC_CLEAR %08x\n", reg_val);


	PSB_DEBUG_TOPAZ("MTX Registers:\n");
	MTX_READ32(0x00, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_ENABLE %08x\n", reg_val);
	MTX_READ32(0x08, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_STATUS %08x\n", reg_val);
	MTX_READ32(0x80, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_KICK %08x\n", reg_val);
	MTX_READ32(0x88, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_KICKI %08x\n", reg_val);
	MTX_READ32(0x90, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_FAULT0 %08x\n", reg_val);
	MTX_READ32(0xf8, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_REGISTER_READ_WRITE_DATA %08x\n", reg_val);
	MTX_READ32(0xfc, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_REGISTER_READ_WRITE_REQUEST %08x\n", reg_val);
	MTX_READ32(0x100, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_RAM_ACCESS_DATA_EXCHANGE %08x\n", reg_val);
	MTX_READ32(0x104, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_RAM_ACCESS_DATA_TRANSFER %08x\n", reg_val);
	MTX_READ32(0x108, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_RAM_ACCESS_CONTROL %08x\n", reg_val);
	MTX_READ32(0x10c, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_RAM_ACCESS_STATUS %08x\n", reg_val);
	MTX_READ32(0x200, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SOFT_RESET %08x\n", reg_val);
	MTX_READ32(0x340, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SYSC_CDMAC %08x\n", reg_val);
	MTX_READ32(0x344, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SYSC_CDMAA %08x\n", reg_val);
	MTX_READ32(0x348, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SYSC_CDMAS0 %08x\n", reg_val);
	MTX_READ32(0x34c, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SYSC_CDMAS1 %08x\n", reg_val);
	MTX_READ32(0x350, &reg_val);
	PSB_DEBUG_TOPAZ("MTX_SYSC_CDMAT %08x\n", reg_val);

	PSB_DEBUG_TOPAZ("DMA Registers:\n");
	DMAC_READ32(0x00, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_Setup_n %08x\n", reg_val);
	DMAC_READ32(0x04, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_Count_n %08x\n", reg_val);
	DMAC_READ32(0x08, &reg_val);
	PSB_DEBUG_TOPAZ(" DMA_Peripheral_param_n %08x\n", reg_val);
	DMAC_READ32(0x0C, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_IRQ_Stat_n %08x\n", reg_val);
	DMAC_READ32(0x10, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_2D_Mode_n %08x\n", reg_val);
	DMAC_READ32(0x14, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_Peripheral_addr_n %08x\n", reg_val);
	DMAC_READ32(0x18, &reg_val);
	PSB_DEBUG_TOPAZ("DMA_Per_hold %08x\n", reg_val);
	return 0;
}
#endif

int tng_topaz_deliver_command(struct drm_device *dev,
			      struct drm_file *file_priv,
			      struct ttm_buffer_object *cmd_buffer,
			      uint32_t cmd_offset, uint32_t cmd_size,
			      void **topaz_cmd, uint32_t sequence,
			      int copy_cmd)
{
	unsigned long cmd_page_offset = cmd_offset & ~PAGE_MASK;
	struct ttm_bo_kmap_obj cmd_kmap;
	bool is_iomem;
	int ret;
	unsigned char *cmd_start, *tmp;

	if (cmd_size > (cmd_buffer->num_pages << PAGE_SHIFT)) {
		DRM_ERROR("Command size %d is bigger than " \
			"command buffer size %d", cmd_size,
			(uint32_t)cmd_buffer->num_pages << PAGE_SHIFT);
		return -EINVAL;
	}

	ret = ttm_bo_kmap(cmd_buffer, cmd_offset >> PAGE_SHIFT, 2,
			  &cmd_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: drm_bo_kmap failed: %d\n", ret);
		return ret;
	}
	cmd_start = (unsigned char *) ttm_kmap_obj_virtual(&cmd_kmap,
		    &is_iomem) + cmd_page_offset;

	if (copy_cmd) {
		tmp = kzalloc(cmd_size, GFP_KERNEL);
		if (tmp == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		memcpy(tmp, cmd_start, cmd_size);
		*topaz_cmd = tmp;
	} else {
		PSB_DEBUG_TOPAZ("TOPAZ: directly send the command\n");
		ret = tng_topaz_send(dev, file_priv,
			cmd_start, cmd_size, sequence);
		if (ret) {
			DRM_ERROR("TOPAZ: commit commands failed.\n");
			ret = -EINVAL;
		}
	}

out:
	ttm_bo_kunmap(&cmd_kmap);

	return ret;
}

#if 0
static int32_t tng_topaz_wait_for_completion(struct drm_psb_private *dev_priv)
{
	int32_t ret = 0;

	mtx_set_target(dev_priv);

	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_ISEQUAL,
		REG_OFFSET_TOPAZ_MTX + MTX_CR_MTX_ENABLE,
		MASK_MTX_MTX_TOFF,
		MASK_MTX_MTX_TOFF | MASK_MTX_MTX_ENABLE);
	if (ret)
		DRM_ERROR("TOPAZ: Wait for MTX completion time out\n");

	return ret;
}
#endif

static int32_t tng_restore_bias_table(
	struct drm_device *dev,
	struct psb_video_ctx *video_ctx)
{
	/* bool is_iomem; */
	uint32_t i;
	uint32_t *p_command;
	uint32_t reg_id;
	uint32_t reg_off;
	uint32_t reg_val;
	uint32_t size;
	struct drm_psb_private *dev_priv = dev->dev_private;
	int32_t ret = 0;

	p_command = video_ctx->bias_reg;

	size = *p_command++;

	PSB_DEBUG_TOPAZ("TOPAZ: Restore BIAS %d regs of ctx %p(%s)\n",
			size - 1, video_ctx,
			codec_to_string(video_ctx->codec));
	for (i = 0; i < size; i++) {
		reg_id = *p_command;
		p_command++;
		reg_off = *p_command;
		p_command++;
		reg_val = *p_command;
		p_command++;

		switch (reg_id) {
		case TOPAZ_MULTICORE_REG:
			MULTICORE_WRITE32(reg_off, reg_val);
			break;
		case TOPAZ_CORE_REG:
			TOPAZCORE_WRITE32(0, reg_off, reg_val);
			break;
		case TOPAZ_VLC_REG:
			VLC_WRITE32(0, reg_off, reg_val);
			break;
		default:
			DRM_ERROR("Unknown reg space id: %08x\n", reg_id);
			/* ttm_bo_kunmap(&tmp_kmap); */
			return -1;
		}
	}

	/* ttm_bo_kunmap(&tmp_kmap); */

	return ret;
}

int32_t tng_topaz_restore_mtx_state(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	uint32_t *mtx_reg_state;
	int i, need_restore = 0;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;

	/* struct ttm_bo_kmap_obj tmp_kmap; */
	/* bool is_iomem; */
	int32_t ret = 0;
	uint32_t num_pipes;
	struct psb_video_ctx *video_ctx = topaz_priv->cur_context;

	/*if (!topaz_priv->topaz_mtx_saved)
		return -1;
	*/

	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if ((pos->ctx_type & 0xff) == VAEntrypointEncSlice ||
			(pos->ctx_type & 0xff) == VAEntrypointEncPicture)
			need_restore = 1;
	}

	if (0 == need_restore) {
		topaz_priv->topaz_mtx_saved = 0;
		PSB_DEBUG_TOPAZ("topaz: no vec context found. needn't" \
			" to restore mtx registers.\n");
		return ret;
	}

	if (topaz_priv->topaz_fw_loaded == 0) {
		PSB_DEBUG_TOPAZ("TOPAZ: needn't to restore context" \
			" without firmware uploaded\n");
		return ret;
	}

	/*TopazSC will be reset, no need to restore context.*/
	if (topaz_priv->topaz_needs_reset)
		return ret;
	/*There is no need to restore context for JPEG encoding*/
	/*
	if (TNG_IS_JPEG_ENC(topaz_priv->cur_codec)) {
		if (tng_topaz_setup_fw(dev, 0, topaz_priv->cur_codec))
			DRM_ERROR("TOPAZ: Setup JPEG firmware fails!\n");
		topaz_priv->topaz_mtx_saved = 0;
		return 0;
	}
	*/

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_HW_CFG, &num_pipes);
	num_pipes = num_pipes & MASK_TOPAZHP_TOP_CR_NUM_CORES_SUPPORTED;

	/* Clear registers used for Host-FW communications */
	MULTICORE_WRITE32(MTX_SCRATCHREG_IDLE, 0);
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_BOOTSTATUS << 2), 0);
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOHOST << 2), 0);
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_TOMTX  << 2), 0);

	mtx_set_target(dev_priv);

	/* write the topaz reset bits */
	/*1) Disable MTX by writing one to the MTX_TOFF
	field of the MTX_ENABLE register*/
	MTX_WRITE32(MTX_CR_MTX_ENABLE,
		    MASK_MTX_MTX_TOFF);

	/* 2) Software reset MTX only by writing 0x1
	then 0x0 to the MULTICORE_SRST register */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_SRST, 1);
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_SRST, 0);

	/* 3) Software reset MTX, cores, and IO by writing 0x7
	then 0x0 to the MULTICORE_SRST register */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_SRST,
		F_ENCODE(1, TOPAZHP_TOP_CR_IMG_TOPAZ_CORE_SOFT_RESET) |
		F_ENCODE(1, TOPAZHP_TOP_CR_IMG_TOPAZ_IO_SOFT_RESET) |
		F_ENCODE(1, TOPAZHP_TOP_CR_IMG_TOPAZ_MTX_SOFT_RESET));
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_SRST, 0);

	MTX_WRITE32(MTX_CR_MTX_SOFT_RESET, MASK_MTX_MTX_RESET);

	PSB_UDELAY(6);

	MTX_WRITE32(MTX_CR_MTX_SOFT_RESET, 0);

	PSB_DEBUG_TOPAZ("TOPAZ: Restore status of context %p(%s)\n",
			video_ctx, codec_to_string(video_ctx->codec));

	/* restore register */
	mtx_reg_state = (uint32_t *)(topaz_priv->topaz_mtx_reg_state[0]);

	/* Restore the MMU Control Registers */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_DIR_LIST_BASE(0),
			  *mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_TILE(0),
			  *mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_TILE(1),
			  *mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL2, *mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL1, *mtx_reg_state);
	mtx_reg_state++;

	/* we do not want to run in secre FW mode so write a place 
	 * holder to the FIFO that the firmware will know to ignore */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			  TOPAZHP_NON_SECURE_FW_MARKER);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL0, *mtx_reg_state);
	mtx_reg_state++;

	/* CARC registers */
	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_WRITE32(i, INTEL_JMCMP_CF_TOTAL,
			*mtx_reg_state);
		mtx_reg_state++;
	}

	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_WRITE32(i, TOPAZHP_CR_TOPAZHP_AUTO_CLOCK_GATING,
			*mtx_reg_state);
		mtx_reg_state++;
		TOPAZCORE_WRITE32(i, TOPAZHP_CR_TOPAZHP_MAN_CLOCK_GATING,
			*mtx_reg_state);
		mtx_reg_state++;
	}

	ret = mtx_upload_fw(dev, video_ctx->codec, video_ctx);
	if (ret) {
		DRM_ERROR("Failed to upload firmware for codec %s\n",
			codec_to_string(video_ctx->codec));
			/* tng_error_dump_reg(dev_priv); */
		return ret;
	}

	/* ttm_bo_kunmap(&tmp_kmap); */

	/* Turn on MTX */
	mtx_start(dev);

	/* Kick the MTX to get things running */
	mtx_kick(dev);

	/* topaz_priv->topaz_mtx_saved = 0; */
	PSB_DEBUG_TOPAZ("TOPAZ: Restore MTX status return\n");

	video_ctx->status &= ~MASK_TOPAZ_CONTEXT_SAVED;

#ifdef TOPAZHP_IRQ_ENABLED
	psb_irq_preinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	psb_irq_postinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);

	tng_topaz_enableirq(dev);
#endif
	tng_topaz_setvideo(dev, video_ctx);

	ret = tng_restore_bias_table(dev, video_ctx);
	if (ret) {
		DRM_ERROR("Failed to restore BIAS table");
		goto out;
	}
out:
	/* ttm_bo_kunmap(&tmp_kmap); */
	return ret;
}

int32_t tng_topaz_restore_mtx_state_b0(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	uint32_t *mtx_reg_state;
	int i, need_restore = 0;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;

	/* struct ttm_bo_kmap_obj tmp_kmap; */
	/* bool is_iomem; */
	int32_t ret = 0;
	uint32_t num_pipes;
	struct psb_video_ctx *video_ctx = topaz_priv->cur_context;

	/*if (!topaz_priv->topaz_mtx_saved)
		return -1;
	*/

	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if ((pos->ctx_type & 0xff) == VAEntrypointEncSlice ||
			(pos->ctx_type & 0xff) == VAEntrypointEncPicture)
			need_restore = 1;
	}

	if (0 == need_restore) {
		topaz_priv->topaz_mtx_saved = 0;
		PSB_DEBUG_TOPAZ("topaz: no vec context found. needn't" \
			" to restore mtx registers.\n");
		return ret;
	}

	if (topaz_priv->topaz_fw_loaded == 0) {
		PSB_DEBUG_TOPAZ("TOPAZ: needn't to restore context" \
			" without firmware uploaded\n");
		return ret;
	}

	if (topaz_priv->topaz_mtx_reg_state[0] == NULL) {
		PSB_DEBUG_TOPAZ("TOPAZ: try to restore context" \
			" without space allocated, return" \
			" directly without restore\n");
		ret = -1;
		return ret;
	}

	/*TopazSC will be reset, no need to restore context.*/
	if (topaz_priv->topaz_needs_reset)
		return ret;
	/*There is no need to restore context for JPEG encoding*/
	/*
	if (TNG_IS_JPEG_ENC(topaz_priv->cur_codec)) {
		if (tng_topaz_setup_fw(dev, 0, topaz_priv->cur_codec))
			DRM_ERROR("TOPAZ: Setup JPEG firmware fails!\n");
		topaz_priv->topaz_mtx_saved = 0;
		return 0;
	}
	*/
	ret = intel_mid_msgbus_read32(PUNIT_PORT, VEC_SS_PM0);
	PSB_DEBUG_TOPAZ("Read R(0x%08x) V(0x%08x)\n",
		VEC_SS_PM0, ret);
	PSB_DEBUG_TOPAZ("TOPAZ: Restore status of context %p(%s)\n",
			video_ctx, codec_to_string(video_ctx->codec));

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_HW_CFG, &num_pipes);
	num_pipes = num_pipes & MASK_TOPAZHP_TOP_CR_NUM_CORES_SUPPORTED;

	PSB_DEBUG_TOPAZ("TOPAZ: Restore status of pipe (%d)\n",
		num_pipes);
#ifdef MRFLD_B0_DEBUG
	/* repeat fw_run's steps */
	/* clock gating */
	reg_val = F_ENCODE(1, TOPAZHP_CR_TOPAZHP_VLC_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_DEB_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_IPE0_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_IPE1_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_SPE0_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_SPE1_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_H264COMP4X4_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_H264COMP8X8_AUTO_CLK_GATE)|
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_H264COMP16X16_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_JMCOMP_AUTO_CLK_GATE)|
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_PC_DM_AUTO_CLK_GATE) |
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_PC_DMS_AUTO_CLK_GATE)|
		F_ENCODE(1, TOPAZHP_CR_TOPAZHP_CABAC_AUTO_CLK_GATE);

	TOPAZCORE_WRITE32(0, TOPAZHP_CR_TOPAZHP_AUTO_CLOCK_GATING, reg_val);
#endif

	/* restore register */
	mtx_reg_state = (uint32_t *)(topaz_priv->topaz_mtx_reg_state[0]);

	/* Restore the MMU Control Registers */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_DIR_LIST_BASE(0),
		*mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: MMU_DIR_LIST_BASE(0) == 0x%08x\n",
		*mtx_reg_state);
#endif
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_TILE(0), *mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CR_MMU_TILE(0) == 0x%08x\n",
		*mtx_reg_state);
#endif
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_TILE(1), *mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CR_MMU_TILE(1) == 0x%08x\n",
		*mtx_reg_state);
#endif
	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL2, *mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CR_MMU_CONTROL2 == 0x%08x\n",
		*mtx_reg_state);
#endif

	mtx_reg_state++;

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL1, *mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CR_MMU_CONTROL1 == 0x%08x\n",
		*mtx_reg_state);
#endif
	mtx_reg_state++;

	/* we do not want to run in secre FW mode so write a place 
	 * holder to the FIFO that the firmware will know to ignore */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		*mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CMD_FIFO_WRITE == 0x%08x\n",
		*mtx_reg_state);
#endif
#ifdef MRFLD_B0_DEBUG
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		TOPAZHP_NON_SECURE_FW_MARKER);
	PSB_DEBUG_TOPAZ("TOPAZ Restore: CMD_FIFO_WRITE == 0x%08x\n",
		*mtx_reg_state);
#endif

	/* MULTICORE_WRITE32(TOPAZHP_TOP_CR_MMU_CONTROL0, *mtx_reg_state); */
	mtx_reg_state++;

	/* CARC registers */
	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_WRITE32(i, INTEL_JMCMP_CF_TOTAL, *mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
		PSB_DEBUG_TOPAZ("TOPAZ Restore: JMCMP_CF_TOTAL == 0x%08x\n",
			*mtx_reg_state);
#endif
		mtx_reg_state++;
	}

	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_WRITE32(i, TOPAZHP_CR_TOPAZHP_AUTO_CLOCK_GATING,
			*mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
		PSB_DEBUG_TOPAZ("TOPAZ Restore: AUTO_CLOCK_GATING == 0x%08x\n",
			*mtx_reg_state);
#endif
		mtx_reg_state++;
		TOPAZCORE_WRITE32(i, TOPAZHP_CR_TOPAZHP_MAN_CLOCK_GATING,
			*mtx_reg_state);
#ifdef MRFLD_B0_DEBUG
		PSB_DEBUG_TOPAZ("TOPAZ Restore: MAN_CLOCK_GATING == 0x%08x\n",
			*mtx_reg_state);
#endif
		mtx_reg_state++;
	}

	/* ttm_bo_kunmap(&tmp_kmap); */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CORE_SEL_0, 0);
	/* Turn on MTX */
	mtx_start(dev);
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CORE_SEL_0, 0);
	/* Kick the MTX to get things running */
	mtx_kick(dev);
	ret = tng_topaz_wait_for_register(
		dev_priv, CHECKFUNC_ISEQUAL,
		TOPAZHP_TOP_CR_FIRMWARE_REG_1 + (MTX_SCRATCHREG_BOOTSTATUS<<2),
		TOPAZHP_FW_BOOT_SIGNAL, 0xffffffff);
	if (ret) {
		DRM_ERROR("Restore Firmware failed\n");
		return ret;
	}

	tng_topaz_mmu_flushcache(dev_priv);

#ifdef TOPAZHP_IRQ_ENABLED
	psb_irq_preinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	psb_irq_postinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);

	tng_topaz_enableirq(dev);
#endif

	ret = tng_restore_bias_table(dev, video_ctx);
	if (ret) {
		DRM_ERROR("Failed to restore BIAS table");
		goto out;
	}

	tng_topaz_setvideo(dev, video_ctx);

	video_ctx->status &= ~MASK_TOPAZ_CONTEXT_SAVED;
	/* topaz_priv->topaz_mtx_saved = 0; */
	PSB_DEBUG_TOPAZ("TOPAZ: Restore MTX status return\n");
out:
	/* ttm_bo_kunmap(&tmp_kmap); */
	return ret;
}


static int  tng_poll_hw_inactive(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int32_t ret = 0;
	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_ISEQUAL,
		MTX_SCRATCHREG_IDLE,
		F_ENCODE(FW_IDLE_STATUS_IDLE, FW_IDLE_REG_STATUS),
		MASK_FW_IDLE_REG_STATUS);
	if (ret)
		DRM_ERROR("Wait for register timeout");

	return ret;
}

static int mtx_wait_for_completion(struct drm_psb_private *dev_priv)
{
	int32_t ret;

	mtx_set_target(dev_priv);

	ret = tng_topaz_wait_for_register(dev_priv, CHECKFUNC_ISEQUAL,
		REG_OFFSET_TOPAZ_MTX + MTX_CR_MTX_ENABLE,
		MASK_MTX_MTX_TOFF,
		MASK_MTX_MTX_TOFF | MASK_MTX_MTX_ENABLE);
	if (ret)
		DRM_ERROR("TOPAZ: Wait for MTX completion time out\n");

	return ret;
}

int tng_topaz_save_mtx_state(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	uint32_t *mtx_reg_state;
	int i, need_save = 0;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	/* struct ttm_bo_kmap_obj tmp_kmap; */
	struct psb_video_ctx *video_ctx;
	/* bool is_iomem; */
	uint32_t num_pipes;
	int32_t ret = 0;
	unsigned long irq_flags;

	PSB_DEBUG_TOPAZ("tng_topaz_save_mtx_state: start\n");

#ifdef TOPAZHP_IRQ_ENABLED
	spin_lock_irqsave(&topaz_priv->topaz_lock, irq_flags);
	spin_lock(&topaz_priv->ctx_spinlock);

	/* In case the context has been removed in
	 * tng_topaz_remove_ctx()
	*/
	video_ctx = topaz_priv->irq_context;
	if (!video_ctx || !video_ctx->wb_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: Context %p has "\
				"been released, bypass saving context\n",
			video_ctx);
		goto out;
	}
#else
	if (topaz_priv->cur_context) {
		video_ctx = topaz_priv->cur_context;
	} else {
		DRM_ERROR("Invalid context\n");
		return -1;
	}
#endif
	/* topaz_priv->topaz_mtx_saved = 0; */
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if ((pos->ctx_type & 0xff) == VAEntrypointEncSlice ||
			(pos->ctx_type & 0xff) == VAEntrypointEncPicture)
			need_save = 1;
	}

	if (0 == need_save) {
		PSB_DEBUG_TOPAZ("TOPAZ: vec context not found. No need" \
			" to save mtx registers.\n");
		goto out;
	}

	/*TopazSC will be reset, no need to save context.*/
	if (topaz_priv->topaz_needs_reset)
		goto out;

	if (topaz_priv->topaz_fw_loaded == 0) {
		PSB_DEBUG_TOPAZ("TOPAZ: No need to restore context since" \
			" firmware has not been uploaded.\n");
		ret = -1;
		goto out;
	}

	/*JPEG encoding needn't to save context*/
	if (TNG_IS_JPEG_ENC(topaz_priv->cur_codec)) {
		PSB_DEBUG_TOPAZ("TOPAZ: Bypass context saving for JPEG\n");
		topaz_priv->topaz_mtx_saved = 1;
		goto out;
	}

	tng_topaz_mmu_flushcache(dev_priv);

	tng_topaz_getvideo(dev, video_ctx);

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_HW_CFG, &num_pipes);
	num_pipes = num_pipes & MASK_TOPAZHP_TOP_CR_NUM_CORES_SUPPORTED;

	mtx_set_target(dev_priv);

	ret = tng_poll_hw_inactive(dev);
	if (ret)
		goto out;

	/* Turn off MTX */
	mtx_stop(dev_priv);
	ret = mtx_wait_for_completion(dev_priv);
	if (ret) {
		DRM_ERROR("Mtx wait for completion error");
		goto out;
	}

	mtx_reg_state = (uint32_t *)(topaz_priv->topaz_mtx_reg_state[0]);

	/* Save the MMU Control Registers */
	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_DIR_LIST_BASE(0), mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_TILE(0), mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_TILE(1), mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_CONTROL2, mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_CONTROL1, mtx_reg_state);
	mtx_reg_state++;

	MULTICORE_READ32(TOPAZHP_TOP_CR_MMU_CONTROL0, mtx_reg_state);
	mtx_reg_state++;

	/* CARC registers */
	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_READ32(i, INTEL_JMCMP_CF_TOTAL, mtx_reg_state);
		mtx_reg_state++;
	}

	for (i = 0; i < num_pipes; i++) {
		TOPAZCORE_READ32(i, TOPAZHP_CR_TOPAZHP_AUTO_CLOCK_GATING,
			mtx_reg_state);
		mtx_reg_state++;
		TOPAZCORE_READ32(i, TOPAZHP_CR_TOPAZHP_MAN_CLOCK_GATING,
			mtx_reg_state);
		mtx_reg_state++;
	}

	video_ctx->status |= MASK_TOPAZ_CONTEXT_SAVED;

out:
#ifdef TOPAZHP_IRQ_ENABLED
	spin_unlock(&topaz_priv->ctx_spinlock);
	spin_unlock_irqrestore(&topaz_priv->topaz_lock, irq_flags);
#endif
	/* topaz_priv->topaz_mtx_saved = 1; */
	PSB_DEBUG_TOPAZ("TOPAZ: Save MTX status return\n");
	/* ttm_bo_kunmap(&tmp_kmap); */
	return ret;
}

struct file *tng_get_context_fp(
	struct drm_psb_private *dev_priv,
	struct drm_file *file_priv)
{
	struct file *current_context = NULL;
	struct psb_video_ctx *pos, *n;

	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if (pos->filp == file_priv->filp)
			current_context = pos->filp;
	}

	return current_context;
}

static int tng_save_bias_table(
	struct drm_device *dev,
	struct drm_file *file_priv,
	const void *cmd)
{
	/* bool is_iomem; */
	uint32_t *reg_saving_ptr;
	uint32_t size;
	/* struct ttm_bo_kmap_obj tmp_kmap; */
	uint32_t *p_command;
	struct psb_video_ctx *video_ctx;
	int32_t ret = 0;

	p_command = (uint32_t *)cmd;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		ret = -1;
		goto out;
	}

	reg_saving_ptr = video_ctx->bias_reg;

	p_command++;
	/* Register count */
	size = *reg_saving_ptr = *p_command;
	PSB_DEBUG_TOPAZ("TOPAZ: Save BIAS table %d registers " \
			"for context %p\n", size, video_ctx);

	p_command++;
	reg_saving_ptr++;

	memcpy(reg_saving_ptr, p_command, size * 3 * 4);

	/* ttm_bo_kunmap(&tmp_kmap); */

out:
	return ret;
}

#if 0
/*
 * Check contxt status and assign ID for new context
 * Return -1 on error
 */
static int tng_check_context_status(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t reg_handle,
	uint32_t data_handle,
	uint32_t codec,
	uint32_t *buf_idx)
{
	return 0;
}

/*
 * If current context issued MTX_CMDID_SHUTDOWN command, mark the ctx_status,
 * clean related reg/data BO, write back BO. Return -1 on the last context.
 */
static int32_t tng_release_context(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t cur_codec)
{
	struct psb_video_ctx *video_ctx;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}

	if (cur_codec != IMG_CODEC_JPEG) {
		PSB_DEBUG_TOPAZ("TOPAZ: Free bias saving memory\n");
		if (video_ctx->bias_reg) {
			kfree(video_ctx->bias_reg);
			video_ctx->bias_reg = NULL;
		}

		video_ctx->status |= MASK_TOPAZ_FIRMWARE_EXIT;
	} else {
		PSB_DEBUG_TOPAZ("TOPAZ: JPEG bypass unmap " \
				"reg/data saving BO\n");
	}

	return 0;
}
#endif

int tng_topaz_kick_null_cmd(struct drm_device *dev,
			    uint32_t sync_seq)
{
	uint32_t cur_free_space;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

#ifdef TOPAZHP_SERIALIZED
	uint32_t serializeToken;
	serializeToken = tng_serialize_enter(dev);
#endif

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
			 &cur_free_space);

	cur_free_space = F_DECODE(cur_free_space,
			TOPAZHP_TOP_CR_CMD_FIFO_SPACE);

	while (cur_free_space < 4) {
		POLL_TOPAZ_FREE_FIFO_SPACE(4, 100, 10000, &cur_free_space);
		if (ret) {
			DRM_ERROR("TOPAZ : error ret %d\n", ret);
			return ret;
		}

		MULTICORE_READ32(
			TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
			&cur_free_space);

		cur_free_space = F_DECODE(cur_free_space,
				TOPAZHP_TOP_CR_CMD_FIFO_SPACE);
	}

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		MTX_CMDID_NULL | MTX_CMDID_WB_INTERRUPT);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		0);

	/* Write back address is always 0 */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		0);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
		sync_seq);

	PSB_DEBUG_TOPAZ("TOPAZ: Write to command FIFO:\n");
	PSB_DEBUG_TOPAZ("%08x, %08x, %08x, %08x\n",
		MTX_CMDID_NULL, 0, 0, 0);

	/* Notify ISR which context trigger interrupt */
#ifdef TOPAZHP_IRQ_ENABLED
	topaz_priv->irq_context = topaz_priv->cur_context;
#endif
	mtx_kick(dev);

#ifdef TOPAZHP_SERIALIZED
	tng_serialize_exit(dev, serializeToken);
#endif

	return ret;
}

int mtx_write_FIFO(
	struct drm_device *dev,
	struct tng_topaz_cmd_header *cmd_header,
	uint32_t param,
	uint32_t param_addr,
	uint32_t sync_seq)
{
	uint32_t cur_free_space;
	uint32_t wb_val;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;
	uint32_t cmdword = 0;
	struct psb_video_ctx *video_ctx = NULL;

	if (topaz_priv->cur_context) {
		video_ctx = topaz_priv->cur_context;
	} else {
		DRM_ERROR("Invalid video context\n");
		return -1;
	}

#ifdef TOPAZHP_SERIALIZED
	uint32_t serializeToken;
	serializeToken = tng_serialize_enter(dev);
#endif
	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
			 &cur_free_space);

	cur_free_space = F_DECODE(cur_free_space,
			TOPAZHP_TOP_CR_CMD_FIFO_SPACE);

	while (cur_free_space < 4) {
		POLL_TOPAZ_FREE_FIFO_SPACE(4, 100, 10000, &cur_free_space);
		if (ret) {
			DRM_ERROR("TOPAZ : error ret %d\n", ret);
			return ret;
		}

		MULTICORE_READ32(
			TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
			&cur_free_space);

		cur_free_space = F_DECODE(cur_free_space,
				TOPAZHP_TOP_CR_CMD_FIFO_SPACE);
	}

	cmdword = F_ENCODE(0, MTX_MSG_CORE) | cmd_header->id;

	if (cmd_header->id & MTX_CMDID_PRIORITY) {
		video_ctx->high_cmd_count++;
		cmdword |= F_ENCODE(1, MTX_MSG_PRIORITY) |
			   F_ENCODE(((video_ctx->low_cmd_count - 1) & 0xff) |
			   (video_ctx->high_cmd_count << 8), MTX_MSG_COUNT);

	} else {
		cmdword |= F_ENCODE(video_ctx->low_cmd_count & 0xff, MTX_MSG_COUNT);
	}

	cmd_header->val = cmdword;

	/* Trigger interrupt on MTX_CMDID_ENCODE_FRAME cmd */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
				  cmd_header->val);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			  param);

	/* Write back address is always 0 */
	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			  param_addr);

	if (cmd_header->id & MTX_CMDID_PRIORITY) {
		/* prepare Writeback value */
		wb_val = video_ctx->high_cmd_count << 24;
	} else {
		wb_val = video_ctx->low_cmd_count << 16;
		video_ctx->low_cmd_count++;
	}

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE,
			  wb_val);
	PSB_DEBUG_TOPAZ("TOPAZ: Write to command FIFO: " \
		"%08x, %08x, %08x, %08x\n",
		cmd_header->val, param, param_addr, wb_val);

	/* Notify ISR which context trigger interrupt */
#ifdef TOPAZHP_IRQ_ENABLED
	topaz_priv->irq_context = topaz_priv->cur_context;
#endif
	mtx_kick(dev);

#ifdef TOPAZHP_SERIALIZED
	tng_serialize_exit(dev, serializeToken);
#endif

	return ret;
}

static void tng_topaz_getvideo(
	struct drm_device *dev,
	struct psb_video_ctx *video_ctx)
{
	int ret;
	struct tng_topaz_cmd_header cmd_header;

	PSB_DEBUG_TOPAZ("TOPAZ: Issue MTX_CMDID_GETVIDEO command");
	PSB_DEBUG_TOPAZ("TOPAZ: to save context\n");
	cmd_header.id = MTX_CMDID_GETVIDEO;

	ret = mtx_write_FIFO(dev, &cmd_header,
		video_ctx->enc_ctx_param,
		video_ctx->enc_ctx_addr, 0);
	if (ret) {
		DRM_ERROR("Failed to write command to FIFO");
		goto out;
	}

	if ((video_ctx->codec == IMG_CODEC_H263_VBR) ||
		(video_ctx->codec == IMG_CODEC_H263_CBR))
		tng_wait_on_sync(dev, 0, cmd_header.id);
out:
    return;
}

static void tng_topaz_setvideo(
	struct drm_device *dev,
	struct psb_video_ctx *video_ctx)
{
	int ret;
	struct tng_topaz_cmd_header cmd_header;

	PSB_DEBUG_TOPAZ("TOPAZ: Issue MTX_CMDID_SETVIDEO command\n");
	PSB_DEBUG_TOPAZ("TOPAZ: to resotre context\n");
	cmd_header.id = MTX_CMDID_SETVIDEO;

	ret = mtx_write_FIFO(dev, &cmd_header,
			     video_ctx->enc_ctx_param,
			     video_ctx->enc_ctx_addr, 0);
	if (ret) {
		DRM_ERROR("Failed to write command to FIFO");
		goto out;
	}

	if ((video_ctx->codec == IMG_CODEC_H263_VBR) ||
		(video_ctx->codec == IMG_CODEC_H263_CBR))
		tng_wait_on_sync(dev, 0, cmd_header.id);
out:
    return;
}

static inline void tng_topaz_trace_ctx(
	char *words,
	struct psb_video_ctx *trace_ctx)
{
	if (words != NULL)
		PSB_DEBUG_TOPAZ("TOPAZ: %s:\n", words);
#ifdef MRFLD_B0_DEBUG
	if (trace_ctx != NULL)
		PSB_DEBUG_TOPAZ("%08x(%s), status %08x\n",
		trace_ctx, codec_to_string(trace_ctx->codec),
		trace_ctx->status);
#endif
	return ;
}

int tng_topaz_getvideo_setvideo(
	struct drm_device *dev,
	struct tng_topaz_private *topaz_priv,
	struct psb_video_ctx *old_video_ctx,
	struct psb_video_ctx *new_video_ctx,
	uint32_t codec)
{
	struct tng_topaz_cmd_header cmd_header;
	int ret = 1;

	PSB_DEBUG_TOPAZ("TOPAZ: Issue MTX_CMDID_GETVIDEO command");
	cmd_header.id = MTX_CMDID_GETVIDEO;

	ret = mtx_write_FIFO(dev, &cmd_header,
		old_video_ctx->enc_ctx_param,
		old_video_ctx->enc_ctx_addr, 0);
	if (ret) {
		DRM_ERROR("Failed to write command to FIFO");
		goto out;
	}

	/*tng_wait_on_sync(dev, 0, cmd_header.id);*/

	PSB_DEBUG_TOPAZ("TOPAZ: Issue MTX_CMDID_SETVIDEO command\n");
	cmd_header.id = MTX_CMDID_SETVIDEO;

	ret = mtx_write_FIFO(dev, &cmd_header,
		new_video_ctx->enc_ctx_param,
		new_video_ctx->enc_ctx_addr, 0);
	if (ret) {
		DRM_ERROR("Failed to write command to FIFO");
		goto out;
	}

	/*tng_wait_on_sync(dev, 0, cmd_header.id);*/
out:
	return ret;
}

static int tng_context_switch_secure(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t codec,
	uint32_t is_first_frame)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx;
	int32_t ret = 0;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: Frame (%d)\n", video_ctx->frame_count);

	if (TNG_IS_H264_ENC(codec)) {
		codec = IMG_CODEC_H264_ALL_RC;
		PSB_DEBUG_TOPAZ("TOPAZ: use RC_ALL for all H264 Codec\n");
		PSB_DEBUG_TOPAZ("TOPAZ: %s to %s\n",
				codec_to_string(codec),
				codec_to_string(video_ctx->codec));
	}

	PSB_DEBUG_TOPAZ("Incoming context is %p (%s, %08x)\n",
			video_ctx,
			codec_to_string(video_ctx->codec),
			video_ctx->status);

	/* Handle JPEG burst mode, save current context only if it's not JPEG */
	if (codec == IMG_CODEC_JPEG) {
		if (topaz_priv->cur_context &&
		    topaz_priv->cur_context->codec != IMG_CODEC_JPEG &&
		    !(topaz_priv->cur_context->status & MASK_TOPAZ_CONTEXT_SAVED)) {

			ret = tng_topaz_save_mtx_state(dev);
			if (ret) {
				DRM_ERROR("Failed to save mtx status");
				return ret;
			}
		}
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		return ret;
	}

	/* Continue doing other commands */
	if (is_first_frame) {
		PSB_DEBUG_TOPAZ("TOPAZ: First frame of ctx " \
				"%p(%s, %08x), continue\n",
				video_ctx, codec_to_string(codec),
				video_ctx->status);
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		return ret;
	}

	if (drm_topaz_pmpolicy == PSB_PMPOLICY_FORCE_PM) {
		ret = tng_topaz_power_off(dev);
		if (ret) {
			DRM_ERROR("TOPAZ: Failed to power off");
			return ret;
		}
	}

	if (is_island_on(OSPM_VIDEO_ENC_ISLAND)
		&& (topaz_priv->cur_context != video_ctx)) {
		if (TNG_IS_H264_ENC(topaz_priv->cur_context->codec)
			&& TNG_IS_H264_ENC(video_ctx->codec)) {
			PSB_DEBUG_TOPAZ("ctx switch without power up/off\n");
			ret = tng_topaz_getvideo_setvideo(
				dev, topaz_priv,
				topaz_priv->cur_context,
				video_ctx, codec);
			if (ret) {
				DRM_ERROR("Failed to context switch");
				return ret;
			}
			/* Context switch */
			topaz_priv->cur_context = video_ctx;
			topaz_priv->cur_codec = codec;
		} else {
			ret = tng_topaz_power_off(dev);
			if (ret) {
				DRM_ERROR("TOPAZ: Failed to power off");
				return ret;
			}
		}
	}

	if (!is_island_on(OSPM_VIDEO_ENC_ISLAND)) {
		tng_topaz_power_up(dev, codec);
		if (ret) {
			DRM_ERROR("TOPAZ: Failed to power up");
			return ret;
		}

		PSB_DEBUG_TOPAZ("Restore context %p(%s, %08x)",
				video_ctx,
				codec_to_string(video_ctx->codec),
				video_ctx->status);

		/* Context switch */
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		ret = tng_topaz_restore_mtx_state_b0(dev);
		if (ret) {
			DRM_ERROR("Failed to restore mtx status");
			return ret;
		}
	}

	return ret;
}

static int tng_context_switch(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t codec,
	uint32_t is_first_frame)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx;
	int32_t ret = 0;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: Frame (%d)\n", video_ctx->frame_count);
	PSB_DEBUG_TOPAZ("Incoming context is %p(%s, %08x)\n",
			video_ctx,
			codec_to_string(video_ctx->codec),
			video_ctx->status);

	/* Handle JPEG burst mode, save current context only if it's not JPEG */
	if (codec == IMG_CODEC_JPEG) {
		if (topaz_priv->cur_context &&
		    topaz_priv->cur_context->codec != IMG_CODEC_JPEG &&
		    !(topaz_priv->cur_context->
		      status &MASK_TOPAZ_CONTEXT_SAVED)) {

			ret = tng_topaz_save_mtx_state(dev);
			if (ret) {
				DRM_ERROR("Failed to save mtx status");
				return ret;
			}
		}
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		return ret;
	}

	/* Continue doing other commands */
	if (is_first_frame) {
		PSB_DEBUG_TOPAZ("First frame of ctx %p(%s, %08x), continue\n",
				video_ctx,
				codec_to_string(codec),
				video_ctx->status);
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		return ret;
	}

	if (topaz_priv->cur_context == video_ctx) {
		if (drm_topaz_pmpolicy == PSB_PMPOLICY_FORCE_PM) {
			ret = tng_topaz_power_off(dev);
			if (ret) {
				DRM_ERROR("TOPAZ: Failed to power off");
				return ret;
			}
		}
		if (video_ctx->status & MASK_TOPAZ_CONTEXT_SAVED) {
			PSB_DEBUG_TOPAZ("Same comtext %p(%s, %08x) " \
					"and already saved\n",
					video_ctx,
					codec_to_string(video_ctx->codec),
					video_ctx->status);

			if (Is_Secure_Fw()) {
				ret = tng_topaz_power_up(dev, codec);
				if (ret) {
					DRM_ERROR("TOPAZ: Failed to power up");
					return ret;
				}
			}

			/* Context switch */
			topaz_priv->cur_context = video_ctx;
			topaz_priv->cur_codec = codec;
			if (Is_Secure_Fw()) {
				ret = tng_topaz_restore_mtx_state_b0(dev);
				if (ret) {
					DRM_ERROR("Failed to restore mtx B0");
					return ret;
				}
			} else {
				ret = tng_topaz_restore_mtx_state(dev);
				if (ret) {
					DRM_ERROR("Failed to restore mtx");
					return ret;
				}
			}
		} else {
			PSB_DEBUG_TOPAZ("Same context %p(%s, %08x) " \
					"but not saved, continue\n",
					video_ctx,
					codec_to_string(video_ctx->codec),
					video_ctx->status);
			topaz_priv->cur_context = video_ctx;
			topaz_priv->cur_codec = codec;
			return ret;
		}
	} else {
		/* Current context already saved */
		if (topaz_priv->cur_context->status & \
		    MASK_TOPAZ_CONTEXT_SAVED) {
			PSB_DEBUG_TOPAZ("Different context and current context"\
					" %p(%s, %08x) already saved, "\
					"continue\n",
					topaz_priv->cur_context,
					codec_to_string(topaz_priv->
							cur_context->
							codec),
					topaz_priv->cur_context->status);
		} else {
			/* Save current context */
			PSB_DEBUG_TOPAZ("Different context and current context"\
					" %p(%s, %08x) not saved,"\
					" save it first",
					topaz_priv->cur_context,
					codec_to_string(topaz_priv->
							cur_context->codec),
					topaz_priv->cur_context->status);
			if (Is_Secure_Fw()) {
				ret = tng_topaz_power_off(dev);
				if (ret) {
					DRM_ERROR("TOPAZ: Failed to power off");
					return ret;
				}
			} else {
				if (topaz_priv->cur_context->codec !=
				    IMG_CODEC_JPEG) {
					ret = tng_topaz_save_mtx_state(dev);
					if (ret) {
						DRM_ERROR("Failed to save "\
							  "mtx status");
						return ret;
					}
				} else
					PSB_DEBUG_TOPAZ("TOPAZ: Bypass saving "\
							"JPEG context\n");
			}
		}

		if (Is_Secure_Fw()) {
			tng_topaz_power_up(dev, codec);
			if (ret) {
				DRM_ERROR("TOPAZ: Failed to power up");
				return ret;
			}
		}

		PSB_DEBUG_TOPAZ("Restore context %p(%s, %08x)",
				video_ctx,
				codec_to_string(video_ctx->codec),
				video_ctx->status);

		/* Context switch */
		topaz_priv->cur_context = video_ctx;
		topaz_priv->cur_codec = codec;
		if (Is_Secure_Fw()) {
			ret = tng_topaz_restore_mtx_state_b0(dev);
			if (ret) {
				DRM_ERROR("Failed to restore mtx status");
				return ret;
			}
		} else {
			if (topaz_priv->cur_context->codec != IMG_CODEC_JPEG) {
				ret = tng_topaz_restore_mtx_state(dev);
				if (ret) {
					DRM_ERROR("Failed to restore "\
						  "mtx status");
					return ret;
				}
			}
		}
	}

	return ret;
}

static uint16_t tng__rand(struct psb_video_ctx * video_ctx)
{
    uint16_t ret = 0;
    video_ctx->pseudo_rand_seed =  (uint32_t)((video_ctx->pseudo_rand_seed * 1103515245 + 12345) & 0xffffffff); //Using mask, just in case
    ret = (uint16_t)(video_ctx->pseudo_rand_seed / 65536) % 32768;
    return ret;
}

static int32_t tng_fill_input_control (
	struct drm_device *dev,
	struct drm_file *file_priv,
	int8_t slot_num,
	int16_t ir,
	int8_t init_qp,
	int8_t min_qp,
	int32_t buf_size,
	uint32_t pseudo_rand_seed)
{
	uint16_t default_param;
	uint16_t intra_param;
	bool refresh = false;
	uint32_t cur_index;
	uint32_t mb_x, mb_y;
	uint32_t mb_w, mb_h;
	uint16_t *p_input_buf;
	int8_t qp;
	int8_t max_qp = 31;
	uint16_t mb_param;
	int32_t ret;
	bool is_iomem;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx;

	mb_w = topaz_priv->frame_w / 16;
	mb_h = topaz_priv->frame_h / 16;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}
	video_ctx->pseudo_rand_seed = pseudo_rand_seed;

	PSB_DEBUG_TOPAZ("TOPAZ: Fill input control, cir=%d, initqp=%d, " \
			"minqp=%d, slot=%d, bufsize=%d, pseudo=%d, " \
			"mb_w=%d, mb_h=%d\n",
			ir, init_qp, min_qp, slot_num, buf_size,
			pseudo_rand_seed, mb_w, mb_h);

	ret = ttm_bo_reserve(video_ctx->cir_input_ctrl_bo,
			     true, true, false, 0);
	if (ret) {
		DRM_ERROR("TOPAZ: reserver failed.\n");
		return -1;
	}

	ret = ttm_bo_kmap(video_ctx->cir_input_ctrl_bo, 0,
			video_ctx->cir_input_ctrl_bo->num_pages,
			&video_ctx->cir_input_ctrl_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map cir input ctrl bo\n");
		ttm_bo_unref(&video_ctx->cir_input_ctrl_bo);
		return -1;
	}

	video_ctx->cir_input_ctrl_addr =
		(uint32_t *)(ttm_kmap_obj_virtual(
				     &video_ctx->
				     cir_input_ctrl_kmap,
				     &is_iomem) + slot_num * buf_size);

	if (ir > 0) {
		refresh = true;
	}

	p_input_buf = (uint16_t *)video_ctx->cir_input_ctrl_addr;

	cur_index = 0;

	for(mb_y = 0; mb_y < (uint32_t)(mb_h); mb_y++) {
		for(mb_x = 0; mb_x < mb_w; mb_x++) {
			mb_param = 0;

			qp = init_qp + ((tng__rand(video_ctx) % 6)-3);
			qp = tng__max(tng__min(qp, max_qp), min_qp);

			default_param = (qp << 10) | (3 << 7) | (3 << 4);
			intra_param = (qp << 10) | (0 << 7) | (0 << 4);

			mb_param = default_param;
			if (refresh) {
				if ((int32_t)cur_index >
				    video_ctx-> last_cir_index) {
					video_ctx->last_cir_index = cur_index;
					mb_param = intra_param;
					ir--;
					if(ir <= 0)
						refresh = false;
				}
			}
			p_input_buf[cur_index++] = mb_param;
		}
	}

	if (refresh) {
		video_ctx->last_cir_index = -1;
		while (ir) {
			qp = init_qp + ((tng__rand(video_ctx) % 6) - 3);
			qp = tng__max(tng__min(qp, max_qp), min_qp);
			intra_param = (qp << 10) |(0 << 7) |(0 << 4);
			p_input_buf[++video_ctx->last_cir_index] = intra_param;
			ir--;
		}
	}

	ttm_bo_unreserve(video_ctx->cir_input_ctrl_bo);
	ttm_bo_kunmap(&video_ctx->cir_input_ctrl_kmap);

	return 0;
}

static int32_t tng_update_air_send (
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint8_t slot_num,
	int16_t skip_count,
	int32_t air_per_frame,
	uint32_t buf_size,
	uint32_t frame_count)
{
	int32_t ret;
	bool is_iomem;
	uint16_t ui16IntraParam;
	uint32_t ui32CurrentCnt, ui32SentCnt;
	uint32_t ui32MBMaxSize;
	uint16_t *pui16MBParam;
	uint32_t ui32NewScanPos;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx;

	ui16IntraParam = (0 << 7) | (0 << 4);

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp\n");
		return -1;
	}

	if (skip_count >= 0)
		video_ctx->air_info.air_skip_cnt = skip_count;
	else /* Pseudorandom skip */
		video_ctx->air_info.air_skip_cnt = (frame_count & 0x7) + 1;

	if (frame_count < 1) {
		video_ctx->air_info.air_per_frame = air_per_frame;
		return 0;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: Update air send, slot_num=%d, skip_count=%d, "\
			"air_per_frame=%d, buf_size=%d, frame_count=%d\n", \
			slot_num, video_ctx->air_info.air_skip_cnt,
			video_ctx->air_info.air_per_frame,
			buf_size, frame_count);

	ret = ttm_bo_reserve(video_ctx->cir_input_ctrl_bo,
			     true, true, false, 0);
	if (ret) {
		DRM_ERROR("TOPAZ: reserver failed.\n");
		return -1;
	}

	ret = ttm_bo_kmap(video_ctx->cir_input_ctrl_bo, 0,
			video_ctx->cir_input_ctrl_bo->num_pages,
			&video_ctx->cir_input_ctrl_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map cir input ctrl bo\n");
		ttm_bo_unref(&video_ctx->cir_input_ctrl_bo);
		return -1;
	}

	video_ctx->cir_input_ctrl_addr =
		(uint32_t *)(ttm_kmap_obj_virtual(
				    &video_ctx->cir_input_ctrl_kmap,
				    &is_iomem) + slot_num * buf_size);

	/* get the buffer */
	pui16MBParam = (uint16_t *) video_ctx->cir_input_ctrl_addr;

	/* fill data */
	ui32MBMaxSize = (uint32_t)(topaz_priv->frame_w / 16) *
		(IMG_UINT32)(topaz_priv->frame_h / 16);

	ui32CurrentCnt = 0;
	ui32NewScanPos = (uint32_t) (video_ctx->air_info.air_scan_pos +
				     video_ctx->air_info.air_skip_cnt) %
		ui32MBMaxSize;
	ui32CurrentCnt = ui32SentCnt = 0;

	while (ui32CurrentCnt < ui32MBMaxSize &&
		((video_ctx->air_info.air_per_frame == 0) ||
		ui32SentCnt < (uint32_t) video_ctx->air_info.air_per_frame)) {

		uint16_t ui16MBParam;

		if (video_ctx->air_info.air_table[ui32NewScanPos] >= 0) {
			// Mark the entry as 'touched'
			video_ctx->air_info.air_table[ui32NewScanPos] =
				-1 - video_ctx->
				air_info.air_table[ui32NewScanPos];

			if (video_ctx->air_info.air_table[ui32NewScanPos] <
			    -1) {
				ui16MBParam = pui16MBParam[ui32NewScanPos] &
					(0xFF << 10);
				ui16MBParam |= ui16IntraParam;
				pui16MBParam[ui32NewScanPos] = ui16MBParam;
				video_ctx->air_info.air_table[ui32NewScanPos]++;
				ui32NewScanPos +=
					video_ctx->air_info.air_skip_cnt;
				ui32SentCnt++;
			}
			ui32CurrentCnt++;
		}

		ui32NewScanPos++;
		ui32NewScanPos = ui32NewScanPos % ui32MBMaxSize;
		if (ui32NewScanPos == video_ctx->air_info.air_scan_pos) {
			/* we have looped around */
			break;
		}
	}

	video_ctx->air_info.air_scan_pos = ui32NewScanPos;

	ttm_bo_unreserve(video_ctx->cir_input_ctrl_bo);
	ttm_bo_kunmap(&video_ctx->cir_input_ctrl_kmap);
	return 0;
}

static int32_t tng_air_buf_clear(
	struct drm_device *dev,
	struct drm_file *file_priv)
{
	struct psb_video_ctx *video_ctx;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp\n");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: Clear AIR buffer\n");
	memset(video_ctx->air_info.air_table, 0,
	       (topaz_priv->frame_h * topaz_priv->frame_w) >> 8);

	return 0;
}

static int32_t tng_update_air_calc(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint8_t slot_num,
	uint32_t buf_size,
	int32_t sad_threshold,
	uint8_t enable_sel_states_flag)
{
	int32_t ret;
	bool is_iomem;
	uint8_t *pSADPointer;
	uint8_t *pvSADBuffer;
	uint8_t ui8IsAlreadyIntra;
	uint32_t ui32MBFrameWidth;
	uint32_t ui32MBPictureHeight;
	uint16_t ui16IntraParam;
	uint32_t ui32MBx, ui32MBy;
	uint32_t ui32SADParam;
	uint32_t ui32tSAD_Threshold, ui32tSAD_ThresholdLo, ui32tSAD_ThresholdHi;
	uint32_t ui32MaxMBs, ui32NumMBsOverThreshold,
		ui32NumMBsOverLo, ui32NumMBsOverHi;
	struct psb_video_ctx *video_ctx;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	IMG_BEST_MULTIPASS_MB_PARAMS *psBestMB_Params;
	IMG_FIRST_STAGE_MB_PARAMS *psFirstMB_Params;
	uint8_t *pFirstPassOutBuf;
	uint8_t *pBestMBDecisionCtrlBuf;

	ui16IntraParam = (0 << 7) | (0 << 4);
	ui32NumMBsOverThreshold = ui32NumMBsOverLo = ui32NumMBsOverHi = 0;

	PSB_DEBUG_TOPAZ("TOPAZ: Update air calc, slot_num=%d, buf_size=%d, "\
			"sad_threshold=%d, enable_sel_states_flag=%08x\n", \
			slot_num, buf_size, sad_threshold,
			enable_sel_states_flag);

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp\n");
		return -1;
	}

	/* Map first pass out params */
	ret = ttm_bo_reserve(video_ctx->bufs_f_p_out_params_bo, true,
			     true, false, 0);
	if (ret) {
		DRM_ERROR("TOPAZ: reserver failed.\n");
		return -1;
	}

	ret = ttm_bo_kmap(video_ctx->bufs_f_p_out_params_bo, 0,
			video_ctx->bufs_f_p_out_params_bo->num_pages,
			&video_ctx->bufs_f_p_out_params_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map first pass out param bo\n");
		ttm_bo_unref(&video_ctx->bufs_f_p_out_params_bo);
		return -1;
	}

	video_ctx->bufs_f_p_out_params_addr =
		(uint32_t *)(ttm_kmap_obj_virtual(
				     &video_ctx->bufs_f_p_out_params_kmap,
				     &is_iomem) + slot_num * buf_size);

	pFirstPassOutBuf =
		(uint8_t *)video_ctx->bufs_f_p_out_params_addr;

	/* Map first pass out best multipass params */
	ret = ttm_bo_reserve(video_ctx->
			     bufs_f_p_out_best_mp_param_bo,
			     true, true, false, 0);
	if (ret) {
		DRM_ERROR("TOPAZ: reserver failed.\n");
		return -1;
	}

	ret = ttm_bo_kmap(video_ctx->
			  bufs_f_p_out_best_mp_param_bo, 0,
			  video_ctx->
			  bufs_f_p_out_best_mp_param_bo->
			  num_pages,
			  &video_ctx->
			  bufs_f_p_out_best_mp_param_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map first pass out best multipass "\
			  "param bo\n");
		ttm_bo_unref(&video_ctx->
			     bufs_f_p_out_best_mp_param_bo);
		return -1;
	}
	video_ctx->bufs_f_p_out_best_mp_param_addr =
		(uint32_t *)(ttm_kmap_obj_virtual(
				     &video_ctx->
				     bufs_f_p_out_best_mp_param_kmap,
				     &is_iomem) + slot_num * buf_size);

	pBestMBDecisionCtrlBuf = (uint8_t *)video_ctx->
		bufs_f_p_out_best_mp_param_addr;

	/* fill data */
	ui32MBFrameWidth  = (topaz_priv->frame_w / 16);
	ui32MBPictureHeight = (topaz_priv->frame_h / 16);

	/* get the SAD results buffer (either IPE0 and IPE1 results or,
	   preferably, the more accurate Best Multipass SAD results) */
	if (pBestMBDecisionCtrlBuf) {
		pvSADBuffer = pBestMBDecisionCtrlBuf;

		if (enable_sel_states_flag & ESF_MP_BEST_MOTION_VECTOR_STATS) {
			/* The actual Param structures (which contain SADs) are
			   located after the Multipass Motion Vector entries */
			pvSADBuffer +=
				(ui32MBPictureHeight * (ui32MBFrameWidth) *
				 sizeof(IMG_BEST_MULTIPASS_MB_PARAMS_IPMV));
		}
	} else {
		pvSADBuffer = pFirstPassOutBuf;
	}

	if (video_ctx->air_info.air_per_frame == 0)
		/* Default to ALL MB's in frame */
		ui32MaxMBs = ui32MBFrameWidth * ui32MBPictureHeight;
	else if (video_ctx->air_info.air_per_frame < 0)
		/* Default to 1% of MB's in frame (min 1) */
		video_ctx->air_info.air_per_frame = ui32MaxMBs =
			((ui32MBFrameWidth * ui32MBPictureHeight) + 99) / 100;
	else
		ui32MaxMBs = video_ctx->air_info.air_per_frame;

	pSADPointer = (uint8_t *)pvSADBuffer;

	video_ctx->air_info.sad_threshold = sad_threshold;
	if (video_ctx->air_info.sad_threshold >= 0)
		ui32tSAD_Threshold =
			(uint16_t)video_ctx->air_info.sad_threshold;
	else {
		// Running auto adjust threshold adjust mode
		if (video_ctx->air_info.sad_threshold == -1) {
			// This will occur only the first time
			if (pBestMBDecisionCtrlBuf) {
				/*Auto seed the threshold with the first value*/
				psBestMB_Params =
					(IMG_BEST_MULTIPASS_MB_PARAMS *)
					pSADPointer;
				ui32SADParam = psBestMB_Params->
					ui32SAD_Inter_MBInfo &
					IMG_BEST_MULTIPASS_SAD_MASK;
			} else {
				/*Auto seed the threshold with the first value*/
				psFirstMB_Params =
					(IMG_FIRST_STAGE_MB_PARAMS *)
					pSADPointer;
				ui32SADParam = (uint32_t) psFirstMB_Params->
					ui16Ipe0Sad;
			}
			/* Negative numbers indicate auto-adjusting threshold */
			video_ctx->air_info.sad_threshold = -1 - ui32SADParam;
		}
		ui32tSAD_Threshold =
			(int32_t) - (video_ctx->air_info.sad_threshold + 1);
	}

	ui32tSAD_ThresholdLo = ui32tSAD_Threshold / 2;
	ui32tSAD_ThresholdHi = ui32tSAD_Threshold + ui32tSAD_ThresholdLo;

	// This loop could be optimised to a single counter if necessary, retaining for clarity
	for (ui32MBy = 0; ui32MBy < ui32MBPictureHeight; ui32MBy++) {
		for( ui32MBx=0; ui32MBx<ui32MBFrameWidth; ui32MBx++) {
			psBestMB_Params = (IMG_BEST_MULTIPASS_MB_PARAMS *) pSADPointer;
			pSADPointer = (uint8_t *) &(psBestMB_Params[1]);
			//pSADPointer += sizeof(IMG_BEST_MULTIPASS_MB_PARAMS);

			// Turn all negative table values to positive (reset 'touched' state of a block that may have been set in APP_SendAIRInpCtrlBuf())
			if (video_ctx->air_info.air_table[ui32MBy *  ui32MBFrameWidth + ui32MBx] < 0)
				video_ctx->air_info.air_table[ui32MBy *  ui32MBFrameWidth + ui32MBx] = -1 - video_ctx->air_info.air_table[ui32MBy *  ui32MBFrameWidth + ui32MBx];

			// This will read the SAD value from the buffer (either IPE0 SAD or the superior Best multipass parameter structure SAD value)
			if (pBestMBDecisionCtrlBuf) {
				psBestMB_Params = (IMG_BEST_MULTIPASS_MB_PARAMS *) pSADPointer;
				ui32SADParam = psBestMB_Params->ui32SAD_Inter_MBInfo & IMG_BEST_MULTIPASS_SAD_MASK;
				if ((psBestMB_Params->ui32SAD_Intra_MBInfo & IMG_BEST_MULTIPASS_MB_TYPE_MASK) >> IMG_BEST_MULTIPASS_MB_TYPE_SHIFT == 1)
					ui8IsAlreadyIntra = 1;
				else
					ui8IsAlreadyIntra = 0;

				pSADPointer = (uint8_t *) &(psBestMB_Params[1]);
			} else {
				psFirstMB_Params = (IMG_FIRST_STAGE_MB_PARAMS *) pSADPointer;
				ui32SADParam = (uint32_t) psFirstMB_Params->ui16Ipe0Sad;
				ui32SADParam += (uint32_t) psFirstMB_Params->ui16Ipe1Sad;
				ui32SADParam /= 2;
				ui8IsAlreadyIntra = 0; // We don't have the information to determine this
				pSADPointer = (uint8_t *) &(psFirstMB_Params[1]);
			}

			if (ui32SADParam >= ui32tSAD_ThresholdLo) {
				ui32NumMBsOverLo++;

				if (ui32SADParam >= ui32tSAD_Threshold) {
				// if (!ui8IsAlreadyIntra) // Don't mark this block if it's just been encoded as an Intra block anyway
				// (results seem better without this condition anyway)
					video_ctx->air_info.air_table[ui32MBy *  ui32MBFrameWidth + ui32MBx]++;
					ui32NumMBsOverThreshold++;
					if (ui32SADParam >= ui32tSAD_ThresholdHi)
						ui32NumMBsOverHi++;
				}
			}
		}
		if ((uint32_t)(uintptr_t)pSADPointer % 64)
			pSADPointer = (uint8_t *)(uintptr_t)
				ALIGN_64(((uint32_t)(uintptr_t) pSADPointer));
	}

	// Test and process running adaptive threshold case
	if (video_ctx->air_info.sad_threshold < 0) {
		// Adjust our threshold (to indicate it's auto-adjustable store it as a negative value minus 1)
		if (ui32NumMBsOverLo <= ui32MaxMBs)
			video_ctx->air_info.sad_threshold = (int32_t) - ((int32_t)ui32tSAD_ThresholdLo) - 1;
		else {
			if (ui32NumMBsOverHi >= ui32MaxMBs)
				video_ctx->air_info.sad_threshold = (int32_t) - ((int32_t)ui32tSAD_ThresholdHi) - 1;
			else {
				if (ui32MaxMBs < ui32NumMBsOverThreshold) {
					video_ctx->air_info.sad_threshold = ((int32_t)ui32tSAD_ThresholdHi - (int32_t)ui32tSAD_Threshold);
					video_ctx->air_info.sad_threshold *= ((int32_t)ui32MaxMBs - (int32_t)ui32NumMBsOverThreshold);
					video_ctx->air_info.sad_threshold /= ((int32_t)ui32NumMBsOverHi - (int32_t)ui32NumMBsOverThreshold);
					video_ctx->air_info.sad_threshold += ui32tSAD_Threshold;
				} else {
                    			video_ctx->air_info.sad_threshold = ((int32_t)ui32tSAD_Threshold - (int32_t)ui32tSAD_ThresholdLo);
                    			video_ctx->air_info.sad_threshold *= ((int32_t)ui32MaxMBs - (int32_t)ui32NumMBsOverLo);
                    			video_ctx->air_info.sad_threshold /= ((int32_t)ui32NumMBsOverThreshold - (int32_t)ui32NumMBsOverLo);
                    			video_ctx->air_info.sad_threshold += ui32tSAD_ThresholdLo;
                		}
                		video_ctx->air_info.sad_threshold = -video_ctx->air_info.sad_threshold - 1;
			}
		}
	}

	ttm_bo_unreserve(video_ctx->bufs_f_p_out_params_bo);
	ttm_bo_kunmap(&video_ctx->bufs_f_p_out_params_kmap);
	ttm_bo_unreserve(video_ctx->bufs_f_p_out_best_mp_param_bo);
	ttm_bo_kunmap(&video_ctx->bufs_f_p_out_best_mp_param_kmap);

	return 0;
}

static int32_t tng_setup_WB_mem(
	struct drm_device *dev,
	struct drm_file *file_priv,
	const void *command)
{
	struct ttm_object_file *tfile = BCVideoGetPriv(file_priv)->tfile;
	struct psb_video_ctx *video_ctx;
	uint32_t i = 0;
	const uint32_t len = sizeof(struct IMG_WRITEBACK_MSG);
	int ret;
	bool is_iomem;
	uint32_t wb_handle;
	uint32_t mtx_ctx_handle;
	uint32_t cir_input_ctrl_handle;
	uint32_t bufs_f_p_out_params_handle;
	uint32_t bufs_f_p_out_best_mp_param_handle;
	uint8_t *ptmp = NULL;
	const uint8_t pas_val = (uint8_t) ~0x0;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		return -1;
	}

	wb_handle = *((uint32_t *)command + 3);
	PSB_DEBUG_TOPAZ("TOPAZ: Map write back memory from handle %08x\n",
		wb_handle);

	video_ctx->wb_bo = ttm_buffer_object_lookup(tfile, wb_handle);

        if (unlikely(video_ctx->wb_bo == NULL)) {
                DRM_ERROR("TOPAZ: Failed to lookup write back BO\n");
                return -1;
        }

	ret = ttm_bo_reserve(video_ctx->wb_bo , true, true, false, 0);
	if (ret) {
		DRM_ERROR("TOPAZ: reserver failed.\n");
		return -1;
	}

	ret = ttm_bo_kmap(video_ctx->wb_bo, 0,
			  video_ctx->wb_bo->num_pages,
			  &video_ctx->wb_bo_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map topaz write back BO\n");
		ttm_bo_unref(&video_ctx->wb_bo);
		return -1;
	}

	video_ctx->wb_addr[0] = (uint32_t *)ttm_kmap_obj_virtual(
				&video_ctx->wb_bo_kmap, &is_iomem);

	PSB_DEBUG_TOPAZ("TOPAZ: memset: val=0x%08x, len=%d\n", \
			pas_val, len);

	PSB_DEBUG_TOPAZ("TOPAZ: memset: wb_addr=%p, i=%d\n", \
			video_ctx->wb_addr[0], i);

	ptmp = (uint8_t *)(video_ctx->wb_addr[i++]);
	memset(ptmp, pas_val, len);
	while (i < WB_FIFO_SIZE) {
		video_ctx->wb_addr[i] = video_ctx->wb_addr[i-1] + 0x400;
		PSB_DEBUG_TOPAZ("TOPAZ: memset: wb_addr=%p, i=%d\n", \
				video_ctx->wb_addr[i], i);
		ptmp = (uint8_t *)(video_ctx->wb_addr[i++]);
		memset(ptmp, pas_val, len);
	} ;

	video_ctx->enc_ctx_param = *((uint32_t *)command + 1);
	video_ctx->enc_ctx_addr = *((uint32_t *)command + 2);

	PSB_DEBUG_TOPAZ("TOPAZ: GET/SETVIDEO data: %08x, address: %08x\n", \
			video_ctx->enc_ctx_param, video_ctx->enc_ctx_addr);

	if (video_ctx->codec == IMG_CODEC_JPEG)
		return 0;

	mtx_ctx_handle = *((uint32_t *)command + 4);
	PSB_DEBUG_TOPAZ("TOPAZ: Map IMG_MTX_VIDEO_CONTEXT buffer from handle %08x\n",
		mtx_ctx_handle);

	video_ctx->mtx_ctx_bo = ttm_buffer_object_lookup(tfile, mtx_ctx_handle);
        if (unlikely(video_ctx->mtx_ctx_bo == NULL)) {
                DRM_ERROR("TOPAZ: Failed to lookup IMG_MTX_VIDEO_CONTEXT handle\n");
                return -1;
        }

	/* If this cmd package is dequeued, must reserve it now */
	if (0 == atomic_read(&video_ctx->mtx_ctx_bo->reserved)) {
		PSB_DEBUG_TOPAZ("MTX context not reserved, reserve it now\n");
		ret = ttm_bo_reserve(video_ctx->mtx_ctx_bo, true, true, false, 0);
		if (ret) {
			DRM_ERROR("Reserve MTX context failed.\n");
			return -1;
		}
	}

	ret = ttm_bo_kmap(video_ctx->mtx_ctx_bo, 0,
			  video_ctx->mtx_ctx_bo->num_pages,
			  &video_ctx->mtx_ctx_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: Failed to map IMG_MTX_VIDEO_CONTEXT BO\n");
		ttm_bo_unref(&video_ctx->mtx_ctx_bo);
		return -1;
	}

	video_ctx->setv_addr = (uint32_t)(uintptr_t)ttm_kmap_obj_virtual(
				&video_ctx->mtx_ctx_kmap, &is_iomem);

	cir_input_ctrl_handle = *((uint32_t *)command + 5);

	video_ctx->cir_input_ctrl_bo = ttm_buffer_object_lookup(tfile, cir_input_ctrl_handle);
	if (unlikely(video_ctx->cir_input_ctrl_bo == NULL)) {
		DRM_ERROR("TOPAZ: Failed to lookup cir input ctrl handle\n");
		return -1;
	}

	bufs_f_p_out_params_handle = *((uint32_t *)command + 6);

	video_ctx->bufs_f_p_out_params_bo = ttm_buffer_object_lookup(tfile, bufs_f_p_out_params_handle);
	if (unlikely(video_ctx->bufs_f_p_out_params_bo == NULL)) {
		DRM_ERROR("TOPAZ: Failed to lookup air first pass out parameter handle\n");
		return -1;
	}
	video_ctx->bufs_f_p_out_params_addr = NULL;

	bufs_f_p_out_best_mp_param_handle = *((uint32_t *)command + 7);

	video_ctx->bufs_f_p_out_best_mp_param_bo = ttm_buffer_object_lookup(tfile, bufs_f_p_out_best_mp_param_handle);
	if (unlikely(video_ctx->bufs_f_p_out_best_mp_param_bo == NULL)) {
		DRM_ERROR("TOPAZ: Failed to lookup air first pass out best multipass parameter handle\n");
		return -1;
	}
	video_ctx->bufs_f_p_out_best_mp_param_addr = NULL;

	return ret;
}

static int tng_setup_new_context_secure(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t *cmd,
	uint32_t codec)
{
	struct psb_video_ctx *video_ctx;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

	topaz_priv->frame_h = (uint16_t)((*((uint32_t *) cmd + 1)) & 0xffff);
	topaz_priv->frame_w = (uint16_t)(((*((uint32_t *) cmd + 1))
				& 0xffff0000)  >> 16) ;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		ret = -1;
		goto out;
	}

	video_ctx->codec = codec;
	if (TNG_IS_H264_ENC(codec)) {
		video_ctx->codec = codec = IMG_CODEC_H264_ALL_RC;
		PSB_DEBUG_TOPAZ("TOPAZ: use RC_ALL for all H264 Codec\n");
		PSB_DEBUG_TOPAZ("TOPAZ: %s to %s\n", \
		codec_to_string(codec), codec_to_string(video_ctx->codec));
	}

	video_ctx->status = 0;
	video_ctx->frame_count = 0;
	video_ctx->bias_reg = NULL;
	video_ctx->handle_sequence_needed = false;
	video_ctx->high_cmd_count = 0;
	video_ctx->low_cmd_count = 0xa5a5a5a5 % MAX_TOPAZ_CMD_COUNT;
	video_ctx->last_cir_index = -1;
	video_ctx->air_info.air_scan_pos = 0;
	video_ctx->air_info.air_table = kzalloc((topaz_priv->frame_h * topaz_priv->frame_w) >> 8, GFP_KERNEL);
	if (!video_ctx->air_info.air_table) {
		DRM_ERROR("TOPAZ: Failed to alloc memory for AIR table\n");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: new context %p(%s)(cur context = %p)\n",
			video_ctx, codec_to_string(codec),
			topaz_priv->cur_context);

	if (Is_Secure_Fw() && drm_topaz_pmpolicy == PSB_PMPOLICY_NOPM) {
		PSB_DEBUG_TOPAZ("TOPAZ: new context, force a poweroff to reload firmware anyway\n");

		drm_topaz_pmpolicy = PSB_PMPOLICY_POWERDOWN; /* off NOPM policy */
		tng_topaz_power_off(dev);
		drm_topaz_pmpolicy = PSB_PMPOLICY_NOPM; /* reset back to NOPM */
	}

	if (topaz_priv->cur_context &&
		(topaz_priv->cur_context != video_ctx) &&
		is_island_on(OSPM_VIDEO_ENC_ISLAND)) {

		PSB_DEBUG_TOPAZ("Current context %p(%s, %08x)" \
				" not saved, save it first\n",
				topaz_priv->cur_context,
				codec_to_string(topaz_priv->cur_context->codec),
				topaz_priv->cur_context->status);

		ret = tng_topaz_power_off(dev);
		if (ret) {
			DRM_ERROR("TOPAZ: Failed");
			DRM_ERROR("to power off");
			goto out;
		}
	}

	if (video_ctx->codec != IMG_CODEC_JPEG) {
		video_ctx->bias_reg = (uint32_t *)kzalloc(2 * PAGE_SIZE, GFP_KERNEL);
		if (!video_ctx->bias_reg) {
			DRM_ERROR("Failed to kzalloc bias reg, OOM\n");
			ret = -1;
			goto out;
		}
	} else {
		video_ctx->bias_reg = NULL;
	}

	ret = tng_topaz_power_up(dev, codec);
	if (ret) {
		DRM_ERROR("TOPAZ: failed power up\n");
		goto out;
	}
	ret = tng_topaz_fw_run(dev, video_ctx, codec);
	if (ret) {
		DRM_ERROR("TOPAZ: upload FW to HW failed\n");
		goto out;
	}
out:
	return ret;
}

static int tng_setup_new_context(
	struct drm_device *dev,
	struct drm_file *file_priv,
	uint32_t *cmd,
	uint32_t codec)
{
	struct psb_video_ctx *video_ctx;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

	topaz_priv->frame_h = (uint16_t)((*((uint32_t *) cmd + 1)) & 0xffff);
	topaz_priv->frame_w = (uint16_t)(((*((uint32_t *) cmd + 1))
				& 0xffff0000)  >> 16) ;

	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (video_ctx == NULL) {
		DRM_ERROR("Failed to get video contex from filp");
		ret = -1;
		goto out;
	}

	video_ctx->codec = codec;
	video_ctx->status = 0;
	video_ctx->frame_count = 0;
	video_ctx->bias_reg = NULL;
	video_ctx->handle_sequence_needed = false;
	video_ctx->high_cmd_count = 0;
	video_ctx->low_cmd_count = 0xa5a5a5a5 % MAX_TOPAZ_CMD_COUNT;
	video_ctx->last_cir_index = -1;
	video_ctx->air_info.air_scan_pos = 0;
	video_ctx->air_info.air_table = (int8_t *)kzalloc((topaz_priv->frame_h * topaz_priv->frame_w) >> 8, GFP_KERNEL);
	if (!video_ctx->air_info.air_table) {
		DRM_ERROR("TOPAZ: Failed to alloc memory for AIR table\n");
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: new context %p(%s)(cur context = %p)\n",
			video_ctx, codec_to_string(codec),
			topaz_priv->cur_context);

	if (Is_Secure_Fw() && drm_topaz_pmpolicy == PSB_PMPOLICY_NOPM) {
		PSB_DEBUG_TOPAZ("TOPAZ: new context, force a poweroff to reload firmware anyway\n");

		drm_topaz_pmpolicy = PSB_PMPOLICY_POWERDOWN; /* off NOPM policy */
		tng_topaz_power_off(dev);
		drm_topaz_pmpolicy = PSB_PMPOLICY_NOPM; /* reset back to NOPM */
	}

	if (topaz_priv->cur_context &&
		topaz_priv->cur_context != video_ctx) {
		if (topaz_priv->cur_context->status & \
		    MASK_TOPAZ_CONTEXT_SAVED) {
			PSB_DEBUG_TOPAZ("Current context %p(%s, %08x)" \
					" already saved, continue\n",
					topaz_priv->cur_context,
					codec_to_string(topaz_priv->cur_context->codec),
					topaz_priv->cur_context->status);
		/* If the previous context not saved */
		} else {
			PSB_DEBUG_TOPAZ("Current context %p(%s, %08x)" \
					" not saved, save it first\n",
					topaz_priv->cur_context,
					codec_to_string(topaz_priv->cur_context->codec),
					topaz_priv->cur_context->status);

			if (Is_Secure_Fw()) {
				ret = tng_topaz_power_off(dev);
				if (ret) {
					DRM_ERROR("TOPAZ: Failed");
					DRM_ERROR("to power off");
					goto out;
				}
			} else {
				ret = tng_topaz_save_mtx_state(dev);
				if (ret) {
					DRM_ERROR("Failed to save");
					DRM_ERROR("mtx status");
					goto out;
				}
			}
		}
	}

	if (video_ctx->codec != IMG_CODEC_JPEG) {
		video_ctx->bias_reg = (uint32_t *)kzalloc(2 * PAGE_SIZE, GFP_KERNEL);
		if (!video_ctx->bias_reg) {
			DRM_ERROR("Failed to kzalloc bias reg, OOM\n");
			ret = -1;
			goto out;
		}
	} else {
		video_ctx->bias_reg = NULL;
	}

	if (Is_Secure_Fw()) {
		ret = tng_topaz_power_up(dev, codec);
		if (ret) {
			DRM_ERROR("TOPAZ: failed power up\n");
			goto out;
		}
		ret = tng_topaz_fw_run(dev, video_ctx, codec);
		if (ret) {
			DRM_ERROR("TOPAZ: upload FW to HW failed\n");
			goto out;
		}
	} else {
	/* Upload the new codec firmware */
		ret = tng_topaz_init_board(dev, video_ctx, codec);
		if (ret) {
			DRM_ERROR("TOPAZ: init board failed\n");
			/* tng_error_dump_reg(dev_priv, 0); */
			goto out;
		}

		/* Upload the new codec firmware */
		ret = tng_topaz_setup_fw(dev, video_ctx, codec);
		if (ret) {
			DRM_ERROR("TOPAZ: upload FW to HW failed\n");
			/* tng_error_dump_reg(dev_priv, 0); */
			goto out;
		}
	}
out:
	return ret;
}


static int32_t tng_check_bias_register(uint32_t reg_id, uint32_t reg_off)
{
	switch (reg_id) {
	case TOPAZ_MULTICORE_REG:
		if (reg_off < REG_MIN_TOPAZ_MULTICORE ||
			reg_off > REG_MAX_TOPAZ_MULTICORE) {
			DRM_ERROR("Invalid MULTICORE register %08x\n",
				reg_off);
			return -1;
		}
		break;
	case TOPAZ_CORE_REG:
		if (reg_off < REG_MIN_TOPAZ_CORE ||
			reg_off > REG_MAX_TOPAZ_CORE) {
			DRM_ERROR("Invalid CORE register %08x\n", reg_off);
			return -1;
		}
		break;
	case TOPAZ_VLC_REG:
		if (reg_off < REG_MIN_TOPAZ_VLC ||
			reg_off > REG_MAX_TOPAZ_VLC) {
			DRM_ERROR("Invalid VLC register %08x\n", reg_off);
			return -1;
		}
		break;
	default:
		DRM_ERROR("Unknown reg space id: %08x\n", reg_id);
		return -1;
	}

	return 0;
}

static int tng_topaz_set_bias(
	struct drm_device *dev,
	struct drm_file *file_priv,
	const uint32_t *command,
	uint32_t codec,
	uint32_t *cmd_size)
{
	uint32_t reg_id, reg_off, reg_val, reg_cnt;
	uint32_t *p_command;
	struct drm_psb_private *dev_priv;
	int ret = 0;

	dev_priv = dev->dev_private;
	p_command = (uint32_t *)command;
	p_command++;
	*cmd_size = *p_command;
	p_command++;

	PSB_DEBUG_TOPAZ("TOPAZ: Start to write %d Registers\n", *cmd_size);
	for (reg_cnt = 0; reg_cnt < *cmd_size; reg_cnt++) {
		/* Reg space ID */
		reg_id = *p_command;
		p_command++;
		/* Reg offset */
		reg_off = *p_command;
		p_command++;
		/* Reg value */
		reg_val = *p_command;
		p_command++;

		ret = tng_check_bias_register(reg_id, reg_off);
		if (ret) {
			DRM_ERROR("Failed in checking BIAS register");
			return ret;
		}

		switch (reg_id) {
		case TOPAZ_MULTICORE_REG:
			MULTICORE_WRITE32(reg_off, reg_val);
			break;
		case TOPAZ_CORE_REG:
			TOPAZCORE_WRITE32(0, reg_off, reg_val);
			break;
		case TOPAZ_VLC_REG:
			VLC_WRITE32(0, reg_off, reg_val);
			break;
		default:
			DRM_ERROR("Unknown reg space id: (%08x)\n", reg_id);
			return -1;
		}
	}

	p_command = (uint32_t *)command;

	/* For now, saving BIAS table no matter necessary or not */
	ret = tng_save_bias_table(dev, file_priv, p_command);
	if (ret) {
		DRM_ERROR("Failed to save BIAS table");
		return ret;
	}

	/* Update Globals */
	if (codec != IMG_CODEC_JPEG) {
		uint32_t ui32ToMtxReg = 0;

		MULTICORE_WRITE32(TOPAZHP_TOP_CR_FIRMWARE_REG_1 +
			(MTX_SCRATCHREG_TOMTX << 2), ui32ToMtxReg);
	}

	return ret;
}
/* #define MULTI_STREAM_TEST */

int
tng_topaz_send(
	struct drm_device *dev,
	struct drm_file *file_priv,
	void *cmd,
	uint32_t cmd_size_in,
	uint32_t sync_seq)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	unsigned char *command = (unsigned char *) cmd;
	struct tng_topaz_cmd_header *cur_cmd_header;
	uint32_t cur_cmd_id;
	uint32_t codec = 0;
	int32_t cur_cmd_size = 4;
	int32_t cmd_size = (int32_t)cmd_size_in;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;
	struct psb_video_ctx *video_ctx;

	if (Is_Secure_Fw() == 0) {
		ret = tng_topaz_power_up(dev, 1);
		if (ret) {
			DRM_ERROR("TOPAZ: Failed to power up");
			return ret;
		}
	}
	video_ctx = get_ctx_from_fp(dev, file_priv->filp);
	if (!video_ctx) {
		DRM_ERROR("Failed to get context from filp %p\n",
			  file_priv->filp);
		ret = -1;
		goto out;
	}

	topaz_priv->topaz_busy = 1;

	PSB_DEBUG_TOPAZ("TOPAZ : send the command in the buffer" \
		" one by one, cmdsize(%d), sequence(%08x)\n",
		cmd_size, sync_seq);

	if (is_island_on(OSPM_VIDEO_ENC_ISLAND)) {
		/* Must flush here in case of invalid cache data */
		tng_topaz_mmu_flushcache(dev_priv);
	}

	while (cmd_size > 0) {
		cur_cmd_header = (struct tng_topaz_cmd_header *) command;
		cur_cmd_id = cur_cmd_header->id;

		PSB_DEBUG_TOPAZ("TOPAZ : cmd is(%s)\n",
			cmd_to_string(cur_cmd_id & (~MTX_CMDID_PRIORITY)));
		PSB_DEBUG_TOPAZ("remaining cmd size is(%d)\n",
			cmd_size);

		switch (cur_cmd_id) {
		case MTX_CMDID_SW_NEW_CODEC:
			codec = (*((uint32_t *) cmd) & 0xFF00) >> 8;
			if (Is_Secure_Fw()) {
				ret = tng_setup_new_context_secure(dev,file_priv,
					(uint32_t *)command, codec);

			} else {
				ret = tng_setup_new_context(dev, file_priv,
					(uint32_t *)command, codec);
			}
			if (ret) {
				DRM_ERROR("Failed to setup new context");
				return ret;
			}
			cur_cmd_size = 2;
			break;
		case MTX_CMDID_SW_ENTER_LOWPOWER:
			PSB_DEBUG_TOPAZ("TOPAZ : Enter lowpower....\n");
			PSB_DEBUG_TOPAZ("XXX : implement it\n");
			cur_cmd_size = 1;
			break;

		case MTX_CMDID_SW_LEAVE_LOWPOWER:
			cur_cmd_size = 2;
			if (Is_Secure_Fw()) {
				ret = tng_context_switch_secure(dev, file_priv,
					*((uint32_t *)command + 1),
					((*((uint32_t *)command) & 0xFF00) >> 8));

			} else {
				ret = tng_context_switch(dev, file_priv,
					*((uint32_t *)command + 1),
					((*((uint32_t *)command) & 0xFF00) >> 8));
			}
			if (ret) {
				DRM_ERROR("Failed to switch context");
				return ret;
			}

			break;
		case MTX_CMDID_SW_WRITEREG:
			ret = tng_topaz_set_bias(dev, file_priv,
				(const uint32_t *)command, codec,
				&cur_cmd_size);
			if (ret) {
				DRM_ERROR("Failed to set BIAS table");
				return ret;
			}

			/*
			* cur_cmd_size if the register count here,
			* reg_id, reg_off and reg_val are stored in a 3 words
			* */
			cur_cmd_size *= 3;
			/* Header size, 2 words */
			cur_cmd_size += 2;
			break;

		case MTX_CMDID_SW_FILL_INPUT_CTRL:
			ret = tng_fill_input_control (dev, file_priv,
					*((uint32_t *)command + 1),//slot
					*((uint32_t *)command + 3),//ir
					*((uint32_t *)command + 4),//initqp
					*((uint32_t *)command + 5),//minqp
					*((uint32_t *)command + 6),//bufsize
					*((uint32_t *)command + 7));//seed
                        if (ret) {
                                DRM_ERROR("Failed to fill " \
                                        "input control buffer");
                                return ret;
                        }
			cur_cmd_size = 8;
			break;
		case MTX_CMDID_SW_UPDATE_AIR_SEND:
			ret = tng_update_air_send (dev, file_priv,
					*((uint32_t *)command + 1),//slot
					*((uint32_t *)command + 3),//air_skip_count
					*((uint32_t *)command + 4),//air_per_frame
					*((uint32_t *)command + 5),//bufsize
					*((uint32_t *)command + 6));//frame_count
                        if (ret) {
                                DRM_ERROR("Failed to update air " \
                                        "send");
                                return ret;
                        }
			cur_cmd_size = 7;
			break;
		case MTX_CMDID_SW_AIR_BUF_CLEAR:
			ret = tng_air_buf_clear(dev, file_priv);
                        if (ret) {
                                DRM_ERROR("Failed to clear " \
                                        "AIR buffer");
                                return ret;
                        }
			cur_cmd_size = 3;
			break;
		case MTX_CMDID_SW_UPDATE_AIR_CALC:
			ret = tng_update_air_calc(dev, file_priv,
					*((uint32_t *)command + 1),//slot
					*((uint32_t *)command + 3),//buf_size
					*((uint32_t *)command + 4),//sad_threashold
					*((uint32_t *)command + 5));//enable_seld_stats_flag
                        if (ret) {
                                DRM_ERROR("Failed to update " \
                                        "air calc");
                                return ret;
                        }
			cur_cmd_size = 6;
			break;
		case MTX_CMDID_PAD:
			/*Ignore this command, which is used to skip
			 * some commands in user space*/
			cur_cmd_size = 4;
			break;
		/* Ordinary commmand */
                case MTX_CMDID_SETUP_INTERFACE:
			if (video_ctx && video_ctx->wb_bo) {
				PSB_DEBUG_TOPAZ("TOPAZ: reset\n");
				if (Is_Secure_Fw()) {
					tng_topaz_power_off(dev);
					tng_topaz_power_up(dev, IMG_CODEC_JPEG);
					tng_topaz_fw_run(dev, video_ctx, IMG_CODEC_JPEG);
				} else {
					tng_topaz_reset(dev_priv);
					tng_topaz_setup_fw(dev, video_ctx, topaz_priv->cur_codec);
				}

				PSB_DEBUG_TOPAZ("TOPAZ: unref write back bo\n");
				ttm_bo_kunmap(&video_ctx->wb_bo_kmap);
				ttm_bo_unreserve(video_ctx->wb_bo);
				ttm_bo_unref(&video_ctx->wb_bo);
				video_ctx->wb_bo = NULL;
			}
                case MTX_CMDID_SETVIDEO:
                        ret = tng_setup_WB_mem(dev, file_priv,
                                (const void *)command);
                        if (ret) {
                                DRM_ERROR("Failed to setup " \
                                        "write back memory region");
                                return ret;
                        }

		case MTX_CMDID_DO_HEADER:
		case MTX_CMDID_ENCODE_FRAME:
		case MTX_CMDID_GETVIDEO:
		case MTX_CMDID_PICMGMT:
		case (MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY):

		case MTX_CMDID_START_FRAME:
		case MTX_CMDID_ENCODE_SLICE:
		case MTX_CMDID_END_FRAME:
		case MTX_CMDID_RC_UPDATE:
		case (MTX_CMDID_RC_UPDATE | MTX_CMDID_PRIORITY):
		case MTX_CMDID_PROVIDE_SOURCE_BUFFER:
		case MTX_CMDID_PROVIDE_REF_BUFFER:
		case MTX_CMDID_PROVIDE_CODED_BUFFER:
		case (MTX_CMDID_PROVIDE_SOURCE_BUFFER | MTX_CMDID_PRIORITY):
		case (MTX_CMDID_PROVIDE_REF_BUFFER | MTX_CMDID_PRIORITY):
		case (MTX_CMDID_PROVIDE_CODED_BUFFER | MTX_CMDID_PRIORITY):

		case MTX_CMDID_SETQUANT:
		case MTX_CMDID_ISSUEBUFF:
		case MTX_CMDID_SETUP:
		case MTX_CMDID_SHUTDOWN:
			/*
			if (cur_cmd_header->id == MTX_CMDID_SHUTDOWN) {
				cur_cmd_size = 4;
				PSB_DEBUG_TOPAZ("TOPAZ : Doesn't handle " \
					"SHUTDOWN command for now\n");
				break;
			}
			*/

			if (video_ctx) {
				video_ctx->cur_sequence = sync_seq;
				video_ctx->handle_sequence_needed = true;
			}

			/* Write command to FIFO */
			ret = mtx_write_FIFO(dev, cur_cmd_header,
				*((uint32_t *)(command) + 1),
				*((uint32_t *)(command) + 2), sync_seq);
			if (ret) {
				DRM_ERROR("Failed to write command to FIFO");
				goto out;
			}

			/*tng_wait_on_sync(dev, sync_seq, cur_cmd_id);*/

			/*
			for (m = 0; m < 1000; m++) {
				PSB_UDELAY(100);
			}
			PSB_UDELAY(6);
			*/

			/* Calculate command size */
			switch (cur_cmd_id) {
			case MTX_CMDID_SETVIDEO:
				cur_cmd_size =
					(video_ctx->codec == IMG_CODEC_JPEG) ?
					(6 + 1) : (7 + 1);
				break;
			case MTX_CMDID_SETUP_INTERFACE:
			case MTX_CMDID_SHUTDOWN:
				cur_cmd_size = 4;
				break;
			case (MTX_CMDID_PROVIDE_SOURCE_BUFFER |
				MTX_CMDID_PRIORITY):
			case (MTX_CMDID_PROVIDE_REF_BUFFER |
				MTX_CMDID_PRIORITY):
			case (MTX_CMDID_PROVIDE_CODED_BUFFER |
				MTX_CMDID_PRIORITY):
			case (MTX_CMDID_PICMGMT | MTX_CMDID_PRIORITY):
				cur_cmd_size = 3;
				break;
			default:
				cur_cmd_size = 3;
				break;
			}


			break;
		default:
			DRM_ERROR("TOPAZ: unsupported command id: %x\n",
				  cur_cmd_id);
			return -1;
		}

		/*cur_cmd_size indicate the number of words of
		current command*/
		command += cur_cmd_size * 4;
		cmd_size -= cur_cmd_size * 4;

		PSB_DEBUG_TOPAZ("TOPAZ : remaining cmd size is(%d)\n",
			cmd_size);

	}

	tng_topaz_kick_null_cmd(dev, sync_seq);
out:
	return ret;
}

int tng_topaz_remove_ctx(
	struct drm_psb_private *dev_priv,
	struct psb_video_ctx *video_ctx)
{
	struct tng_topaz_private *topaz_priv;
	/* struct psb_video_ctx *video_ctx; */
	struct tng_topaz_cmd_queue *entry, *next;
	unsigned long flags;

	topaz_priv = dev_priv->topaz_private;

	spin_lock_irqsave(&(topaz_priv->ctx_spinlock), flags);
	if (video_ctx == topaz_priv->irq_context)
		topaz_priv->irq_context = NULL;
	spin_unlock_irqrestore(&(topaz_priv->ctx_spinlock), flags);

	mutex_lock(&topaz_priv->topaz_mutex);

	/* topaz_priv->topaz_busy = 0; */
	/* video_ctx = NULL; */

	/* Disable ISR */
	/*if (TOPAZHP_IRQ_ENABLED) {
		PSB_DEBUG_TOPAZ("TOPAZ: Disalbe IRQ and " \
			"Wait for MTX completion\n");
		tng_topaz_disableirq(dev_priv);
	}*/

	/* Stop the MTX */
	/*
	mtx_stop(dev_priv);
	ret = mtx_wait_for_completion(dev_priv);
	if (ret) {
		DRM_ERROR("Mtx wait for completion error");
		return ret;
	}

	list_for_each_entry(pos, &dev_priv->video_ctx, head) {
		if (pos->filp == filp) {
			video_ctx = pos;
			break;
		}
	}
	*/

	if (video_ctx == NULL) {
		DRM_ERROR("Invalid video context\n");
		mutex_unlock(&topaz_priv->topaz_mutex);
		return -1;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: release context %p(%s)\n",
			video_ctx, codec_to_string(video_ctx->codec));

	if (video_ctx->bias_reg) {
		PSB_DEBUG_TOPAZ("TOPAZ: Free bias reg saving memory\n");
		kfree(video_ctx->bias_reg);
		video_ctx->bias_reg = NULL;
	}

	if (video_ctx->wb_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: unref write back bo\n");
		ttm_bo_kunmap(&video_ctx->wb_bo_kmap);
		ttm_bo_unreserve(video_ctx->wb_bo);
		ttm_bo_unref(&video_ctx->wb_bo);
		video_ctx->wb_bo = NULL;
	}

	if (video_ctx->mtx_ctx_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: unref setvideo bo\n");
		ttm_bo_kunmap(&video_ctx->mtx_ctx_kmap);
		/* unreserve if reserved in tng_setup_WB_mem() */
		if (0 != atomic_read(&video_ctx->mtx_ctx_bo->reserved)) {
			PSB_DEBUG_TOPAZ("MTX context reserved, "\
					"unreserve it now\n");
			ttm_bo_unreserve(video_ctx->mtx_ctx_bo);
		}
		ttm_bo_unref(&video_ctx->mtx_ctx_bo);
		video_ctx->mtx_ctx_bo = NULL;
	}

	if (video_ctx->cir_input_ctrl_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: unref cir input ctrl bo\n");
		ttm_bo_unref(&video_ctx->cir_input_ctrl_bo);
		video_ctx->cir_input_ctrl_bo = NULL;
	}

	if (video_ctx->bufs_f_p_out_best_mp_param_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: unref first pass out best "\
				"multipass param bo\n");
		ttm_bo_unref(&video_ctx->bufs_f_p_out_best_mp_param_bo);
		video_ctx->bufs_f_p_out_best_mp_param_bo = NULL;
	}

	if (video_ctx->bufs_f_p_out_params_bo) {
		PSB_DEBUG_TOPAZ("TOPAZ: unref first pass out param bo\n");
		ttm_bo_unref(&video_ctx->bufs_f_p_out_params_bo);
		video_ctx->bufs_f_p_out_params_bo = NULL;
	}

	if (video_ctx->air_info.air_table) {
		PSB_DEBUG_TOPAZ("TOPAZ: free air table\n");
		kfree(video_ctx->air_info.air_table);
		video_ctx->air_info.air_table = NULL;
	}

	video_ctx->status = 0;
	video_ctx->codec = 0;

	/* If shutdown before completion, need to handle the fence.*/
	if (video_ctx->handle_sequence_needed) {
		*topaz_priv->topaz_sync_addr = video_ctx->cur_sequence;
		psb_fence_handler(dev_priv->dev, LNC_ENGINE_ENCODE);
		video_ctx->handle_sequence_needed = false;
	}

	if (!list_empty(&topaz_priv->topaz_queue) &&
	    get_ctx_cnt(dev_priv->dev) == 0) {
		PSB_DEBUG_TOPAZ("TOPAZ: Flush all commands " \
			"the in queue\n");
		/* clear all the commands in queue */
		list_for_each_entry_safe(entry, next,
				 &topaz_priv->topaz_queue,
				 head) {
			list_del(&entry->head);
			kfree(entry->cmd);
			kfree(entry);
		}
	}

	if (get_ctx_cnt(dev_priv->dev) == 0) {
		PSB_DEBUG_TOPAZ("No more active VEC context\n");
		dev_priv->topaz_ctx = NULL;
	}

	mutex_unlock(&topaz_priv->topaz_mutex);

	return 0;
}

void tng_topaz_handle_sigint(
	struct drm_device *dev,
	struct file *filp)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *video_ctx;
	uint32_t count = 0;

	video_ctx = get_ctx_from_fp(dev, filp);
	if (video_ctx) {
		PSB_DEBUG_TOPAZ("TOPAZ: Prepare to handle CTRL + C\n");
	} else {
		PSB_DEBUG_TOPAZ("TOPAZ: Not VEC context or already released\n");
		return;
	}

	while (topaz_priv->topaz_busy == 1 &&
	       count++ < 20000)
		PSB_UDELAY(6);

	if (count == 20000)
		DRM_ERROR("Failed to handle sigint event\n");

	PSB_DEBUG_TOPAZ("TOPAZ: Start to handle CTRL + C\n");
	psb_remove_videoctx(dev_priv, filp);
}

int tng_topaz_dequeue_send(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_cmd_queue *topaz_cmd = NULL;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

	/* Avoid race condition with queue buffer when topaz_busy = 1 */
	mutex_lock(&topaz_priv->topaz_mutex);
	if (list_empty(&topaz_priv->topaz_queue)) {
		topaz_priv->topaz_busy = 0;
		PSB_DEBUG_TOPAZ("TOPAZ: empty command queue, " \
			"set topaz_busy = 0, directly return\n");
		mutex_unlock(&topaz_priv->topaz_mutex);
		return ret;
	}

	topaz_cmd = list_first_entry(&topaz_priv->topaz_queue,
			struct tng_topaz_cmd_queue, head);

	memset(topaz_priv->saved_queue, 0, sizeof(struct tng_topaz_cmd_queue));
	memset(topaz_priv->saved_cmd, 0, MAX_CMD_SIZE);

	topaz_priv->saved_queue->file_priv = topaz_cmd->file_priv;
	topaz_priv->saved_queue->cmd_size = topaz_cmd->cmd_size;
	topaz_priv->saved_queue->sequence = topaz_cmd->sequence;
	topaz_priv->saved_queue->head = topaz_cmd->head;

	memcpy(topaz_priv->saved_cmd, topaz_cmd->cmd, topaz_cmd->cmd_size);

	list_del(&topaz_cmd->head);
	kfree(topaz_cmd->cmd);
	kfree(topaz_cmd);
	mutex_unlock(&topaz_priv->topaz_mutex);

	PSB_DEBUG_TOPAZ("TOPAZ: dequeue command of sequence %08x " \
			"and send it to topaz\n", \
			topaz_priv->saved_queue->sequence);

	ret = tng_topaz_send(dev,
		topaz_priv->saved_queue->file_priv,
		topaz_priv->saved_cmd,
		topaz_priv->saved_queue->cmd_size,
		topaz_priv->saved_queue->sequence);
	if (ret) {
		DRM_ERROR("TOPAZ: tng_topaz_send failed.\n");
		ret = -EINVAL;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: dequeue command of sequence %08x " \
			"finished\n", topaz_priv->saved_queue->sequence);

	return ret;
}

int32_t tng_check_topaz_idle(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	uint32_t reg_val;

	if (dev_priv->topaz_ctx == NULL) {
		PSB_DEBUG_TOPAZ("TOPAZ: topaz context is null\n");
		return 0;
	}

	/*HW is stuck. Need to power off TopazSC to reset HW*/
	if (1 == topaz_priv->topaz_needs_reset) {
		PSB_DEBUG_TOPAZ("TOPAZ: Topaz needs reset\n");
		return 0;
	}

	if (topaz_priv->topaz_busy) {
		PSB_DEBUG_TOPAZ("TOPAZ: can't save, topaz_busy = %d\n", \
				   topaz_priv->topaz_busy);
		return -EBUSY;
	}

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_CMD_FIFO_WRITE_SPACE,
		&reg_val);
	reg_val &= MASK_TOPAZHP_TOP_CR_CMD_FIFO_SPACE;
	if (reg_val != 32) {
		PSB_DEBUG_TOPAZ("TOPAZ: HW is busy. Free words in command" \
				"FIFO is %d.\n",
				reg_val);
		return -EBUSY;
	}

	return 0; /* we think it is idle */
}


int32_t tng_video_get_core_num(struct drm_device *dev, uint64_t user_pointer)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

	ret = copy_to_user((void __user *)((unsigned long)user_pointer),
			   &topaz_priv->topaz_num_pipes,
			   sizeof(topaz_priv->topaz_num_pipes));

	if (ret)
		return -EFAULT;

	return ret;

}

int32_t tng_video_frameskip(struct drm_device *dev, uint64_t user_pointer)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	int32_t ret = 0;

	ret = copy_to_user((void __user *)((unsigned long)user_pointer),
		&topaz_priv->frame_skip, sizeof(topaz_priv->frame_skip));

	if (ret)
		return -EFAULT;

	return ret;
}

static void tng_topaz_flush_cmd_queue(struct tng_topaz_private *topaz_priv)
{
	struct tng_topaz_cmd_queue *entry, *next;

	/* remind to reset topaz */
	topaz_priv->topaz_needs_reset = 1;
	topaz_priv->topaz_busy = 0;

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

void tng_topaz_handle_timeout(struct ttm_fence_device *fdev)
{
	struct drm_psb_private *dev_priv =
		container_of(fdev, struct drm_psb_private, fdev);
	/*struct drm_device *dev =
		container_of((void *)dev_priv,
		struct drm_device, dev_private);*/
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	/* if (IS_MRST(dev))
		return  lnc_topaz_handle_timeout(fdev);
	*/
	DRM_ERROR("TOPAZ: current codec is %s\n",
			codec_to_string(topaz_priv->cur_codec));
	tng_topaz_flush_cmd_queue(topaz_priv);


	/* Power down TopazSC to reset HW*/
	/* schedule_delayed_work(&topaz_priv->topaz_suspend_wq, 0); */
}

void tng_topaz_enableirq(struct drm_device *dev)
{
#ifdef TOPAZHP_IRQ_ENABLED
	struct drm_psb_private *dev_priv = dev->dev_private;
	uint32_t crImgTopazIntenab;

	PSB_DEBUG_TOPAZ("TOPAZ: Enable TOPAZHP IRQ\n");

	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_HOST_INT_ENAB,
		&crImgTopazIntenab);

	crImgTopazIntenab |= (MASK_TOPAZHP_TOP_CR_MTX_INTEN_MTX |
		MASK_TOPAZHP_TOP_CR_HOST_TOPAZHP_MAS_INTEN);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_HOST_INT_ENAB,
		crImgTopazIntenab);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_MTX_INT_ENAB,
		MASK_MTX_INT_ENAB);
#else
	return 0;
#endif
}

void tng_topaz_disableirq(struct drm_device *dev)
{
	uint32_t crImgTopazIntenab;
	struct drm_psb_private *dev_priv = dev->dev_private;

	PSB_DEBUG_TOPAZ("TOPAZ: Disable TOPAZHP IRQ\n");
	MULTICORE_READ32(TOPAZHP_TOP_CR_MULTICORE_HOST_INT_ENAB,
		&crImgTopazIntenab);

	crImgTopazIntenab &= ~(MASK_TOPAZHP_TOP_CR_MTX_INTEN_MTX);

	MULTICORE_WRITE32(TOPAZHP_TOP_CR_MULTICORE_HOST_INT_ENAB,
		crImgTopazIntenab);
}

/* Disable VEC or GFX clock gating */
void tng_topaz_CG_disable(struct drm_device *dev)
{
	int reg_val;
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (drm_topaz_cgpolicy & PSB_CGPOLICY_GFXCG_DIS) {
		reg_val = ioread32(dev_priv->wrapper_reg + 0);
		PSB_DEBUG_TOPAZ("TOPAZ: GFX CG 0 = %08x, " \
			"disable GFX CG\n", reg_val);
		iowrite32(0x103, dev_priv->wrapper_reg + 0);
		reg_val = ioread32(dev_priv->wrapper_reg + 0);
		PSB_DEBUG_TOPAZ("TOPAZ: GFX CG 0 = %08x\n", reg_val);
	}

	if (drm_topaz_cgpolicy & PSB_CGPOLICY_VECCG_DIS) {
		reg_val = ioread32(dev_priv->vec_wrapper_reg + 0);
		PSB_DEBUG_TOPAZ("TOPAZ: VEC CG 0 = %08x, " \
			"disable VEC CG\n", reg_val);
		iowrite32(0x03, dev_priv->vec_wrapper_reg + 0);
		reg_val = ioread32(dev_priv->vec_wrapper_reg + 0);
		PSB_DEBUG_TOPAZ("TOPAZ: VEC CG 0 = %08x\n", reg_val);
	}
}

static int pm_cmd_freq_wait(u32 reg_freq)
{
	int tcount;
	u32 freq_val;

	for (tcount = 0; ; tcount++) {
		freq_val = intel_mid_msgbus_read32(PUNIT_PORT, reg_freq);
		if ((freq_val & IP_FREQ_VALID) == 0)
			break;
		if (tcount > 500) {
			DRM_ERROR("P-Unit freq request wait timeout");
			return -EBUSY;
		}
		udelay(1);
	}

	return 0;
}

static int pm_cmd_freq_set(u32 reg_freq, u32 freq_code)
{
	u32 freq_val;
	int rva;

	pm_cmd_freq_wait(reg_freq);

	freq_val = IP_FREQ_VALID | freq_code;
	intel_mid_msgbus_write32(PUNIT_PORT, reg_freq, freq_val);

	rva = pm_cmd_freq_wait(reg_freq);

	return rva;
}

int tng_topaz_set_vec_freq(u32 freq_code)
{
	return pm_cmd_freq_set(VEC_SS_PM1, freq_code);
}

#define PMU_ENC			0x1
/* Workaround to disable D0i3 */
bool power_island_get_dummy(struct drm_device *dev)
{
	int pm_ret = 0;
	unsigned long irqflags;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	if (atomic_read(&topaz_priv->vec_ref_count))
		goto out;

	pm_ret = pmu_nc_set_power_state(PMU_ENC, OSPM_ISLAND_UP, VEC_SS_PM0);
	if (pm_ret) {
		PSB_DEBUG_PM("power up vec failed\n");
		return false;
	}

	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);

	dev_priv->vdc_irq_mask |= _LNC_IRQ_TOPAZ_FLAG;
	PSB_WVDC32(~dev_priv->vdc_irq_mask, PSB_INT_MASK_R);
	PSB_WVDC32(dev_priv->vdc_irq_mask, PSB_INT_ENABLE_R);

	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

	atomic_inc(&topaz_priv->vec_ref_count);

	if (drm_topaz_cgpolicy != PSB_CGPOLICY_ON)
		tng_topaz_CG_disable(dev);
out:
	return true;
}

/* Workaround to disable D0i3 */
bool power_island_put_dummy(struct drm_device *dev)
{
	return true;
}

int tng_topaz_power_up(
	struct drm_device *dev,
	enum drm_tng_topaz_codec codec)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;
	u32 reg_val = 0;

	PSB_DEBUG_TOPAZ("TOPAZ: power up start\n");

	if (Is_Secure_Fw()) {
		if (is_island_on(OSPM_VIDEO_ENC_ISLAND)) {
			PSB_DEBUG_TOPAZ("TOPAZ: power is already up, end and return\n");
			return 0;
		}
		reg_val = intel_mid_msgbus_read32(PUNIT_PORT, VEC_SS_PM0);
		PSB_DEBUG_TOPAZ("TOPAZ: VECSSPM0 (0) = 0x%08x\n", reg_val);
		reg_val &= ~((u32)0x7c);
		reg_val |= ((u32)codec) << 2;
		intel_mid_msgbus_write32(PUNIT_PORT, VEC_SS_PM0, reg_val);
		PSB_DEBUG_TOPAZ("TOPAZ: VECSSPM0 (1) = 0x%08x\n", reg_val);
		/* udelay(2); */
	}

#ifdef MRFLD_B0_DEBUG
	reg_val = intel_mid_msgbus_read32(PUNIT_PORT, VEC_SS_PM0);
	PSB_DEBUG_TOPAZ("Read R(0x%08x) V(0x%08x)\n",
		VEC_SS_PM0, reg_val);
	reg_val &= ~((u32)0x3);

	intel_mid_msgbus_write32(PUNIT_PORT, VEC_SS_PM0, reg_val);
	PSB_DEBUG_TOPAZ("write R(0x%08x) V(0x%08x)\n",
		VEC_SS_PM0, reg_val);
	while (count != 0) {
		reg_val = intel_mid_msgbus_read32(PUNIT_PORT, DPC_SSC);
		PSB_DEBUG_TOPAZ("RR (%d), A(0x%08x) V(0x%08x)\n",
			count, DPC_SSC, reg_val);
		reg_val = intel_mid_msgbus_read32(PUNIT_PORT, VEC_SS_PM0);
		count -= 1;
		if (reg_val == (codec<<2))
			count = 0;
		PSB_DEBUG_TOPAZ("RR (%d), A(0x%08x) V(0x%08x)\n",
			count, VEC_SS_PM0, reg_val);
			udelay(20);
		if (count != 0) {
			reg_val &= ~((u32)0x3);
			intel_mid_msgbus_write32(PUNIT_PORT,
				VEC_SS_PM0, reg_val);
			PSB_DEBUG_TOPAZ("WW (%d), A(0x%08x) V(0x%08x)\n",
			count, VEC_SS_PM0, reg_val);
		}
	}
#endif

	if (drm_topaz_pmpolicy == PSB_PMPOLICY_NOPM)
		PSB_DEBUG_TOPAZ("Topaz: NOPM policy, but still need powerup here\n");

	/* Reduce D0i0 residency, original it is 0 set by GFX driver */
	topaz_priv->dev->pdev->d3_delay = 10;

	if (!power_island_get(OSPM_VIDEO_ENC_ISLAND)) {
		DRM_ERROR("Failed to power on ENC island\n");
		return -1;
	}

	/* Must flush here in case of invalid cache data */
	/* tng_topaz_mmu_flushcache(dev_priv); */
	PSB_DEBUG_TOPAZ("TOPAZ: power up end\n");
	return 0;
}

int tng_topaz_power_off(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct tng_topaz_private *topaz_priv = dev_priv->topaz_private;

	PSB_DEBUG_TOPAZ("TOPAZ: power off start\n");
	if (Is_Secure_Fw()) {
		if (!is_island_on(OSPM_VIDEO_ENC_ISLAND)) {
			PSB_DEBUG_TOPAZ("TOPAZ: power is already off\n");
			return 0;
		}
	}

	if (drm_topaz_pmpolicy == PSB_PMPOLICY_NOPM)
		PSB_DEBUG_TOPAZ("TOPAZ: skip power off since NOPM policy\n");
	else {
		power_island_put(OSPM_VIDEO_ENC_ISLAND);
		/* Restore delay to 0 */
		topaz_priv->dev->pdev->d3_delay = 0;
	}

	PSB_DEBUG_TOPAZ("TOPAZ: power off end\n");

	return 0;
}

int Is_Secure_Fw()
{
	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER &&
		intel_mid_soc_stepping() < 1)
		return 0;
	return 1;
}
