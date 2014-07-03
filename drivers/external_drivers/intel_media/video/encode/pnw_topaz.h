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

#ifndef _PNW_TOPAZ_H_
#define _PNW_TOPAZ_H_

#include "psb_drv.h"
#include "img_types.h"

#define PNW_TOPAZ_NO_IRQ 0
#define TOPAZ_MTX_REG_SIZE (34 * 4 + 183 * 4)
#define MAX_TOPAZ_CORES 2

/*Must be equal to IMG_CODEC_NUM*/
#define PNW_TOPAZ_CODEC_NUM_MAX (11)
#define PNW_TOPAZ_BIAS_TABLE_MAX_SIZE (2 * 1024)
/* #define TOPAZ_PDUMP */

/* Max command buffer size */
#define PNW_MAX_CMD_BUF_PAGE_NUM (2)
/* One cmd set contains 4 words */
#define PNW_TOPAZ_WORDS_PER_CMDSET (4)
#define PNW_TOPAZ_POLL_DELAY (100)
#define PNW_TOPAZ_POLL_RETRY (10000)

#define TOPAZ_NEW_CODEC_CMD_BYTES (4 * 2)
#define TOPAZ_COMMON_CMD_BYTES (4 * 3)
#define TOPAZ_POWER_CMD_BYTES (0)
/* Every WRITEREG command set contain 2 words.
   The first word indicates register offset.
   The second word indicates register value */
#define TOPAZ_WRITEREG_BYTES_PER_SET (4 * 2)
#define TOPAZ_WRITEREG_MAX_SETS \
	(PNW_TOPAZ_BIAS_TABLE_MAX_SIZE / TOPAZ_WRITEREG_BYTES_PER_SET)

/* in words */
#define TOPAZ_CMD_FIFO_SIZE (32)

#define PNW_IS_H264_ENC(codec) \
	(codec == IMG_CODEC_H264_VBR || \
	 codec == IMG_CODEC_H264_VCM || \
	 codec == IMG_CODEC_H264_CBR || \
	 codec == IMG_CODEC_H264_NO_RC)

#define PNW_IS_JPEG_ENC(codec) \
	(codec == IMG_CODEC_JPEG)

#define PNW_IS_MPEG4_ENC(codec) \
	(codec == IMG_CODEC_MPEG4_VBR || \
	 codec == IMG_CODEC_MPEG4_CBR || \
	 codec == IMG_CODEC_MPEG4_NO_RC)

#define PNW_IS_H263_ENC(codec) \
	(codec == IMG_CODEC_H263_VBR || \
	 codec == IMG_CODEC_H263_CBR || \
	 codec == IMG_CODEC_H263_NO_RC)

extern int drm_topaz_pmpolicy;

/* XXX: it's a copy of msvdx cmd queue. should have some change? */
struct pnw_topaz_cmd_queue {
	struct list_head head;
	void *cmd;
	unsigned long cmd_size;
	uint32_t sequence;
};

/* define structure */
/* firmware file's info head */
struct topazsc_fwinfo {
	unsigned int ver:16;
	unsigned int codec:16;

	unsigned int text_size;
	unsigned int data_size;
	unsigned int data_location;
};

/* firmware data array define  */
struct pnw_topaz_codec_fw {
	uint32_t ver;
	uint32_t codec;

	uint32_t text_size;
	uint32_t data_size;
	uint32_t data_location;

	struct ttm_buffer_object *text;
	struct ttm_buffer_object *data;
};

struct pnw_topaz_private {
	struct drm_device *dev;
	unsigned int pmstate;
	struct sysfs_dirent *sysfs_pmstate;

	/*Save content of MTX register, whole RAM and BIAS table*/
	void *topaz_mtx_reg_state[MAX_TOPAZ_CORES];
	struct ttm_buffer_object *topaz_mtx_data_mem[MAX_TOPAZ_CORES];
	uint32_t topaz_cur_codec;
	uint32_t cur_mtx_data_size[MAX_TOPAZ_CORES];
	int topaz_needs_reset;
	void *topaz_bias_table[MAX_TOPAZ_CORES];

	/*
	 *topaz command queue
	 */
	spinlock_t topaz_lock;
	struct list_head topaz_queue;
	int topaz_busy;		/* 0 means topaz is free */
	int topaz_fw_loaded;

	uint32_t stored_initial_qp;
	uint32_t topaz_dash_access_ctrl;

	struct ttm_buffer_object *topaz_bo; /* 4K->2K/2K for writeback/sync */
	struct ttm_bo_kmap_obj topaz_bo_kmap;
	uint32_t *topaz_mtx_wb;
	uint32_t topaz_wb_offset;
	uint32_t *topaz_sync_addr;
	uint32_t topaz_sync_offset;
	uint32_t topaz_cmd_count;
	uint32_t topaz_mtx_saved;


	/* firmware */
	struct pnw_topaz_codec_fw topaz_fw[PNW_TOPAZ_CODEC_NUM_MAX * 2];

	uint32_t topaz_hw_busy;

	uint32_t topaz_num_cores;

	/*Before load firmware, need to set up jitter according to resolution*/
	/*The data of MTX_CMDID_SW_NEW_CODEC command contains width and length*/
	uint16_t frame_w;
	uint16_t frame_h;
	/* topaz suspend work queue */
	struct delayed_work topaz_suspend_wq;
	uint32_t pm_gating_count;
};

/* external function declare */
/*ISR of TopazSC*/
extern IMG_BOOL pnw_topaz_interrupt(IMG_VOID *pvData);

/*topaz commad handling function*/
extern int pnw_cmdbuf_video(struct drm_file *priv,
			    struct list_head *validate_list,
			    uint32_t fence_type,
			    struct drm_psb_cmdbuf_arg *arg,
			    struct ttm_buffer_object *cmd_buffer,
			    struct psb_ttm_fence_rep *fence_arg);
extern int pnw_wait_topaz_idle(struct drm_device *dev);
extern int pnw_check_topaz_idle(struct drm_device *dev);
extern int pnw_topaz_restore_mtx_state(struct drm_device *dev);
extern void pnw_topaz_enableirq(struct drm_device *dev);
extern void pnw_topaz_disableirq(struct drm_device *dev);

extern int pnw_topaz_init(struct drm_device *dev);
extern int pnw_topaz_uninit(struct drm_device *dev);
extern void pnw_topaz_handle_timeout(struct ttm_fence_device *fdev);

extern int pnw_topaz_save_mtx_state(struct drm_device *dev);

#define PNW_TOPAZ_START_CTX (0x1)
#define PNW_TOPAZ_END_CTX (0x1<<1)
extern void pnw_reset_fw_status(struct drm_device *dev, u32 flag);

extern void topaz_write_core_reg(struct drm_psb_private *dev_priv,
				 uint32_t core,
				 uint32_t reg,
				 const uint32_t val);
extern void topaz_read_core_reg(struct drm_psb_private *dev_priv,
				uint32_t core,
				uint32_t reg,
				uint32_t *ret_val);
extern void psb_powerdown_topaz(struct work_struct *work);

extern void pnw_topaz_flush_cmd_queue(struct pnw_topaz_private *topaz_priv);
#define PNW_TOPAZ_NEW_PMSTATE(drm_dev, topaz_priv, new_state)		\
do { \
	topaz_priv->pmstate = new_state;				\
	if (new_state == PSB_PMSTATE_POWERDOWN)				\
		topaz_priv->pm_gating_count++;				\
	sysfs_notify_dirent(topaz_priv->sysfs_pmstate);			\
	PSB_DEBUG_PM("TOPAZ: %s, power gating count 0x%08x\n",		\
	(new_state == PSB_PMSTATE_POWERUP) ? "powerup" : "powerdown",	\
		topaz_priv->pm_gating_count); \
} while (0)

#endif	/* _PNW_TOPAZ_H_ */
