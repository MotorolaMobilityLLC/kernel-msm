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
#ifndef _FPGA_TOPAZ_H_
#define _FPGA_TOPAZ_H_

#include "psb_drv.h"
#include "tng_topaz_hw_reg.h"


#define TOPAZ_MTX_REG_SIZE (34 * 4 + 183 * 4)

/*Must be equal to IMG_CODEC_NUM*/
#define TNG_TOPAZ_CODEC_NUM_MAX (25)
#define TNG_TOPAZ_BIAS_TABLE_MAX_SIZE (2 * 1024)
/*#define TOPAZ_PDUMP*/

#define TOPAZHP_IRQ_ENABLED
#define TOPAZHP_PIPE_NUM 2

/* #define MRFLD_B0_DEBUG */

#define TNG_IS_H264_ENC(codec) \
	(codec == IMG_CODEC_H264_NO_RC || \
	codec == IMG_CODEC_H264_VBR  || \
	codec == IMG_CODEC_H264_VCM  || \
	codec == IMG_CODEC_H264_CBR  || \
	codec == IMG_CODEC_H264_LLRC || \
	codec == IMG_CODEC_H264_ALL_RC)

#define TNG_IS_H264MVC_ENC(codec) \
	(codec == IMG_CODEC_H264MVC_NO_RC || \
	codec == IMG_CODEC_H264MVC_CBR || \
	codec == IMG_CODEC_H264MVC_VBR)

#define TNG_IS_JPEG_ENC(codec) \
	(codec == IMG_CODEC_JPEG)

#define MASK_WB_HIGH_CMDID      0xFF000000
#define SHIFT_WB_HIGH_CMDID     24

#define MASK_WB_LOW_CMDID       0x00FFFF00
#define SHIFT_WB_LOW_CMDID      8

#define MASK_WB_SYNC_CNT        0x000000FF
#define SHIFT_WB_SYNC_CNT       0

#define MAX_CMD_SIZE		4096

#define SHIFT_MTX_MSG_PRIORITY          (7)
#define MASK_MTX_MSG_PRIORITY           (0x1 << SHIFT_MTX_MSG_PRIORITY)
#define SHIFT_MTX_MSG_CORE                      (8)
#define MASK_MTX_MSG_CORE                       (0x7f << SHIFT_MTX_MSG_CORE)
#define SHIFT_MTX_MSG_WB_INTERRUPT      (15)
#define MASK_MTX_MSG_WB_INTERRUPT       (0x1 << SHIFT_MTX_MSG_WB_INTERRUPT)
#define SHIFT_MTX_MSG_COUNT                     (16)
#define MASK_MTX_MSG_COUNT                      (0xffff << SHIFT_MTX_MSG_COUNT)

/*#define VERIFYFW*/
/*#define VERIFY_CONTEXT_SWITCH*/

enum TOPAZ_REG_ID {
	TOPAZ_MULTICORE_REG,
	TOPAZ_CORE_REG,
	TOPAZ_VLC_REG
};

extern int drm_topaz_pmpolicy;
extern int drm_topaz_cgpolicy;
extern int drm_topaz_cmdpolicy;

/* XXX: it's a copy of msvdx cmd queue. should have some change? */
struct tng_topaz_cmd_queue {
	struct drm_file *file_priv;
	struct list_head head;
	void *cmd;
	uint32_t cmd_size;
	uint32_t sequence;
};

#define SECURE_VRL_HEADER 728
#define SECURE_FIP_HEADER 296

struct tng_secure_fw {
	uint32_t codec_idx;
	uint32_t addr_data;
	uint32_t text_size;
	uint32_t data_size;
	uint32_t data_loca;

	struct ttm_buffer_object *text;
	struct ttm_buffer_object *data;
};


#define MAX_CONTEXT_CNT	2
#define MAX_TOPAZHP_CORES 4
#define MASK_TOPAZ_CONTEXT_SAVED	(0x1)
#define MASK_TOPAZ_FIRMWARE_EXIT	(0x1 << 1)
#define MASK_TOPAZ_FIRMWARE_ACTIVE	(0x1 << 2)

struct tng_topaz_private {
	unsigned int pmstate;
	struct sysfs_dirent *sysfs_pmstate;
	int frame_skip;

#ifdef VERIFYFW
	/* For verify firmware */
	struct ttm_buffer_object *text_mem;
	struct ttm_buffer_object *data_mem;
	uint32_t bo_text_items[10];
	uint32_t bo_data_items[10];
	uint32_t dma_text_items[10];
	uint32_t dma_data_items[10];
#endif
	uint32_t cur_codec;
	int topaz_needs_reset;
	void *topaz_mtx_reg_state[MAX_TOPAZHP_CORES];
	void *topaz_bias_table[MAX_TOPAZHP_CORES];
	uint32_t cur_mtx_data_size[MAX_TOPAZHP_CORES];
	struct ttm_buffer_object *topaz_mtx_data_mem[MAX_TOPAZHP_CORES];

	/*
	 *topaz command queue
	 */
	struct tng_topaz_cmd_queue *saved_queue;
	char *saved_cmd;
	spinlock_t topaz_lock;
	struct mutex topaz_mutex;
	struct list_head topaz_queue;
	atomic_t cmd_wq_free;
	atomic_t vec_ref_count;
	wait_queue_head_t cmd_wq;
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
	uint32_t core_id;
	uint32_t topaz_wb_received;
	uint32_t topaz_mtx_saved;

	/* firmware */
	struct tng_secure_fw topaz_fw[TNG_TOPAZ_CODEC_NUM_MAX];
	uint32_t topaz_hw_busy;

	uint32_t topaz_num_pipes;

	/*Before load firmware, need to set up
	jitter according to resolution*/
	/*The data of MTX_CMDID_SW_NEW_CODEC command
	contains width and length.*/
	uint16_t frame_w;
	uint16_t frame_h;

	/* For IRQ and Sync */
	uint32_t producer;
	uint32_t consumer;

	/* JPEG ISSUEBUF cmd count */
	uint32_t issuebuf_cmd_count;

	/* Context parameters */
	struct psb_video_ctx *cur_context;
	struct psb_video_ctx *irq_context;

	/* topaz suspend work queue */
	struct drm_device *dev;
	struct delayed_work topaz_suspend_work;
	uint32_t isr_enabled;
	uint32_t power_down_by_release;

	struct ttm_object_file *tfile;

	spinlock_t ctx_spinlock;
};

struct tng_topaz_cmd_header {
	union {
		struct {
			uint32_t id:8;
			uint32_t core:8;
			uint32_t low_cmd_count:8;
			uint32_t high_cmd_count:8;
		};
		uint32_t val;
	};
};

/* external function declare */
/*ISR of TopazSC*/
extern bool tng_topaz_interrupt(void *pvData);

/*topaz commad handling function*/
extern int tng_cmdbuf_video(struct drm_file *priv,
			    struct list_head *validate_list,
			    uint32_t fence_type,
			    struct drm_psb_cmdbuf_arg *arg,
			    struct ttm_buffer_object *cmd_buffer,
			    struct psb_ttm_fence_rep *fence_arg);

extern int tng_check_topaz_idle(struct drm_device *dev);

extern void tng_topaz_enableirq(struct drm_device *dev);

extern void tng_topaz_disableirq(struct drm_device *dev);

extern int tng_topaz_init(struct drm_device *dev);

extern int tng_topaz_uninit(struct drm_device *dev);

extern void tng_topaz_handle_timeout(struct ttm_fence_device *fdev);

extern int32_t mtx_write_core_reg(struct drm_psb_private *dev_priv,
				 uint32_t reg,
				 const uint32_t val);

extern int32_t mtx_read_core_reg(struct drm_psb_private *dev_priv,
				uint32_t reg,
				uint32_t *ret_val);

int tng_topaz_kick_null_cmd(struct drm_device *dev,
			    uint32_t sync_seq);

void tng_set_producer(struct drm_device *dev,
		      uint32_t consumer);

void tng_set_consumer(struct drm_device *dev,
		      uint32_t consumer);

uint32_t tng_get_producer(struct drm_device *dev);

uint32_t tng_get_consumer(struct drm_device *dev);

uint32_t tng_serialize(struct drm_device *dev);

uint32_t tng_wait_for_ctrl(struct drm_device *dev,
			   uint32_t control);

int mtx_upload_fw(struct drm_device *dev,
		  uint32_t codec,
		  struct psb_video_ctx *video_ctx);

int32_t mtx_dma_read(struct drm_device *dev,
		struct ttm_buffer_object *dst_bo,
		uint32_t src_ram_addr,
		uint32_t size);

void mtx_start(struct drm_device *dev);

void mtx_stop(struct drm_psb_private *dev_priv);

void mtx_kick(struct drm_device *dev);

int mtx_write_FIFO(struct drm_device *dev,
	struct tng_topaz_cmd_header *cmd_header,
	uint32_t param, uint32_t param_addr, uint32_t sync_seq);

int tng_topaz_remove_ctx(struct drm_psb_private *dev,
	struct psb_video_ctx *video_ctx);

extern int tng_topaz_save_mtx_state(struct drm_device *dev);

extern int tng_topaz_restore_mtx_state(struct drm_device *dev);
extern int tng_topaz_restore_mtx_state_b0(struct drm_device *dev);

int tng_topaz_dequeue_send(struct drm_device *dev);

uint32_t get_ctx_cnt(struct drm_device *dev);

struct psb_video_ctx *get_ctx_from_fp(
	struct drm_device *dev, struct file *filp);

void tng_topaz_handle_sigint(
	struct drm_device *dev,
	struct file *filp);

void tng_topaz_CG_disable(struct drm_device *dev);

int tng_topaz_set_vec_freq(u32 freq_code);

bool power_island_get_dummy(struct drm_device *dev);

bool power_island_put_dummy(struct drm_device *dev);

#define TNG_TOPAZ_NEW_PMSTATE(drm_dev, topaz_priv, new_state)		\
do { \
	topaz_priv->pmstate = new_state;				\
	sysfs_notify_dirent(topaz_priv->sysfs_pmstate);			\
	PSB_DEBUG_PM("TOPAZ: %s\n",					\
		(new_state == PSB_PMSTATE_POWERUP) ? "powerup" : "powerdown"); \
} while (0)

#endif	/* _FPGA_TOPAZ_H_ */
