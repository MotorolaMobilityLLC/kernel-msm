/**
 * file vsp.h
 * Author: Binglin Chen <binglin.chen@intel.com>
 *
 */

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

#ifndef _VSP_H_
#define _VSP_H_

#include "psb_drv.h"
#include "vsp_fw.h"

/* reg define */
#define SP1_SP_DMEM_IP 0x70000

/* processor */
#define SP0_SP_REG_BASE 0x000000
#define SP1_SP_REG_BASE 0x050000
#define VP0_SP_REG_BASE 0x080000
#define VP1_SP_REG_BASE 0x0C0000
#define MEA_SP_REG_BASE 0x100000

/* SP stat_ctrl */
#define SP_STAT_AND_CTRL_REG 0x0
#define SP_STAT_AND_CTRL_REG_RESET_FLAG           0
#define SP_STAT_AND_CTRL_REG_START_FLAG           1
#define SP_STAT_AND_CTRL_REG_BREAK_FLAG           2
#define SP_STAT_AND_CTRL_REG_RUN_FLAG             3
#define SP_STAT_AND_CTRL_REG_BROKEN_FLAG          4
#define SP_STAT_AND_CTRL_REG_READY_FLAG           5
#define SP_STAT_AND_CTRL_REG_SLEEP_FLAG           6
#define SP_STAT_AND_CTRL_REG_ICACHE_INVALID_FLAG  0xC
#define SP_STAT_AND_CTRL_REG_ICACHE_PREFETCH_FLAG 0xD

/* offsets of registers in processors */
#define VSP_STAT_CTRL_REG_OFFSET             0x00000
#define VSP_START_PC_REG_OFFSET              0x00004
#define VSP_ICACHE_BASE_REG_OFFSET           0x00010

#define SP_BASE_ADDR_REG (0x1 * 4)

#define SP_CFG_PMEM_MASTER 0x10

/* MMU */
#define MMU_INVALID         0x1B0000
#define MMU_TABLE_ADDR      0x1B0004

/* IRQ controller */
#define VSP_IRQ_REG_BASE 0x190000
#define VSP_IRQ_CTRL_IRQ_EDGE            0x0
#define VSP_IRQ_CTRL_IRQ_MASK            0x4
#define VSP_IRQ_CTRL_IRQ_STATUS          0x8
#define VSP_IRQ_CTRL_IRQ_CLR             0xC
#define VSP_IRQ_CTRL_IRQ_ENB             0x10
#define VSP_IRQ_CTRL_IRQ_LEVEL_PULSE     0x14

#define VSP_SP0_IRQ_SHIFT 0x7
#define VSP_SP1_IRQ_SHIFT 0x8

#define VSP_CONFIG_REG_SDRAM_BASE 0x1A0000
#define VSP_CONFIG_REG_START 0x8

#define VSP_FIRMWARE_MEM_ALIGNMENT 4096
/* #define VP8_ENC_DEBUG 1 */

#define MAX_VP8_CONTEXT_NUM 3
#define MAX_VPP_CONTEXT_NUM 1

static const unsigned int vsp_processor_base[] = {
				SP0_SP_REG_BASE,
				SP1_SP_REG_BASE,
				VP0_SP_REG_BASE,
				VP1_SP_REG_BASE,
				MEA_SP_REG_BASE
				};

/* help macro */
#define MM_WRITE32(base, offset, value)					\
	do {								\
		*((uint32_t *)((unsigned char *)(dev_priv->vsp_reg) \
				    + base + offset)) = value;		\
	} while (0)

#define MM_READ32(base, offset, pointer)				\
	do {								\
		*(pointer) =						\
			*((uint32_t *)((unsigned char *)		\
					    (dev_priv->vsp_reg)		\
						 + base + offset));	\
	} while (0)

#define SP1_DMEM_WRITE32(offset, value)		\
	MM_WRITE32(SP1_SP_DMEM_IP, offset, value)
#define SP1_DMEM_READ32(offset, pointer)	\
	MM_READ32(SP1_SP_DMEM_IP, offset, pointer)

#define SP_REG_WRITE32(offset, value, processor)			 \
	do {								 \
		MM_WRITE32(vsp_processor_base[processor], offset, value); \
	} while (0)

#define SP_REG_READ32(offset, pointer, processor)		\
	do {							\
		MM_READ32(vsp_processor_base[processor], offset, pointer); \
	} while (0)


#define SP0_REG_WRITE32(offset, value)		\
	MM_WRITE32(SP0_SP_REG_BASE, offset, value)
#define SP0_REG_READ32(offset, pointer)		\
	MM_READ32(SP0_SP_REG_BASE, offset, pointer)

#define SP1_REG_WRITE32(offset, value)		\
	MM_WRITE32(SP1_SP_REG_BASE, offset, value)
#define SP1_REG_READ32(offset, pointer)		\
	MM_READ32(SP1_SP_REG_BASE, offset, pointer)

#define CONFIG_REG_WRITE32(offset, value)			\
	MM_WRITE32(VSP_CONFIG_REG_SDRAM_BASE, ((offset) * 4), value)
#define CONFIG_REG_READ32(offset, pointer)			\
	MM_READ32(VSP_CONFIG_REG_SDRAM_BASE, ((offset) * 4), pointer)

#define PAGE_TABLE_SHIFT PAGE_SHIFT
#define INVALID_MMU MM_WRITE32(0, MMU_INVALID, 0x1)
#define SET_MMU_PTD(address)						\
	do {								\
		MM_WRITE32(0, MMU_TABLE_ADDR, address);			\
	} while (0)

#define VSP_SET_FLAG(val, offset) \
	((val) = ((val) | (0x1 << (offset))))
#define VSP_CLEAR_FLAG(val, offset) \
	((val) = ((val) & (~(0x1 << (offset)))))
#define VSP_READ_FLAG(val, offset) \
	(((val) & (0x1 << (offset))) >> (offset))
#define VSP_REVERT_FLAG(val, offset) \
	((val) = (val ^ (0x1 << (offset))))

#define IRQ_REG_WRITE32(offset, value)		\
	MM_WRITE32(VSP_IRQ_REG_BASE, offset, value)
#define IRQ_REG_READ32(offset, pointer)		\
	MM_READ32(VSP_IRQ_REG_BASE, offset, pointer)

#define VSP_NEW_PMSTATE(drm_dev, vsp_priv, new_state)			\
do {									\
	vsp_priv->pmstate = new_state;					\
	sysfs_notify_dirent(vsp_priv->sysfs_pmstate);			\
	PSB_DEBUG_PM("VSP: %s\n",					\
		(new_state == PSB_PMSTATE_POWERUP) ? "powerup"		\
		: ((new_state == PSB_PMSTATE_POWERDOWN) ? "powerdown"	\
		: "clockgated"));					\
} while (0)

extern int drm_vsp_pmpolicy;

/* The status of vsp hardware */
enum vsp_power_state {
	VSP_STATE_HANG = -1,
	VSP_STATE_DOWN = 0,
	VSP_STATE_SUSPEND,
	VSP_STATE_IDLE,
	VSP_STATE_ACTIVE
};

/* The status of firmware */
enum vsp_firmware_state {
	VSP_FW_NONE = 0,
	VSP_FW_LOADED
};

#define VSP_CONFIG_SIZE 16

enum vsp_irq_reg {
	VSP_IRQ_REG_EDGE   = 0,
	VSP_IRQ_REG_MASK   = 1,
	VSP_IRQ_REG_STATUS = 2,
	VSP_IRQ_REG_CLR    = 3,
	VSP_IRQ_REG_ENB    = 4,
	VSP_IRQ_REG_PULSE  = 5,
	VSP_IRQ_REG_SIZE
};

enum vsp_context_num {
	VSP_CONTEXT_NUM_VPP = 0,
	VSP_CONTEXT_NUM_VP8 = 1,
	VSP_CONTEXT_NUM_MAX = 3
};

enum vsp_fw_type {
	VSP_FW_TYPE_VPP,
	VSP_FW_TYPE_VP8
};

struct vsp_private {
	struct drm_device *dev;
	uint32_t current_sequence;

	int fw_loaded;
	int vsp_state;

	spinlock_t lock;

	unsigned int cmd_queue_size;
	unsigned int ack_queue_size;

	struct ttm_buffer_object *cmd_queue_bo;
	unsigned int cmd_queue_sz;
	struct ttm_bo_kmap_obj cmd_kmap;
	struct vss_command_t *cmd_queue;

	struct ttm_buffer_object *ack_queue_bo;
	unsigned int ack_queue_sz;
	struct ttm_bo_kmap_obj ack_kmap;
	struct vss_response_t *ack_queue;

	struct ttm_buffer_object *setting_bo;
	struct ttm_bo_kmap_obj setting_kmap;
	struct vsp_settings_t *setting;

	struct vsp_secure_boot_header boot_header;
	struct vsp_multi_app_blob_data ma_header;

	struct vsp_ctrl_reg *ctrl;

	unsigned int pmstate;
	struct sysfs_dirent *sysfs_pmstate;

	uint64_t vss_cc_acc;

	unsigned int saved_config_regs[VSP_CONFIG_SIZE];

	/* lock for vsp command */
	struct mutex vsp_mutex;

	/* pm suspend wq */
	struct delayed_work vsp_suspend_wq;

	/* irq tasklet */
	struct delayed_work vsp_irq_wq;

	/* the number of cmd will send to VSP */
	int vsp_cmd_num;

	/* save the address of vp8 cmd_buffer for now */
	struct VssVp8encPictureParameterBuffer *vp8_encode_frame_cmd;
	struct ttm_bo_kmap_obj vp8_encode_frame__kmap;

	/* For VP8 dual encoding */
	struct file *vp8_filp[4];
	int context_vp8_num;

	/* The context number of VPP */
	int context_vpp_num;

	/*
	 * to fix problem when CTRL+C vp8 encoding *
	 * save VssVp8encEncodeFrameCommand cmd numbers *
	 * */
	int vp8_cmd_num;

	struct vss_command_t seq_cmd;

	/* to save the last sequence */
	uint32_t last_sequence;

	/* VPP pnp usage */
	unsigned long cmd_submit_time;
	int acc_num_cmd;
	int force_flush_cmd;
	int delayed_burst_cnt;
	struct delayed_work vsp_cmd_submit_check_wq;

	/* Composer related */
	uint32_t compose_fence;
};

extern int vsp_init(struct drm_device *dev);
extern int vsp_deinit(struct drm_device *dev);

extern int vsp_reset(struct drm_psb_private *dev_priv);

extern int vsp_init_fw(struct drm_device *dev);
extern int vsp_setup_fw(struct drm_psb_private *dev_priv);

extern void vsp_enableirq(struct drm_device *dev);
extern void vsp_disableirq(struct drm_device *dev);

extern bool vsp_interrupt(void *pvData);

extern int vsp_cmdbuf_vpp(struct drm_file *priv,
			  struct list_head *validate_list,
			  uint32_t fence_type,
			  struct drm_psb_cmdbuf_arg *arg,
			  struct ttm_buffer_object *cmd_buffer,
			  struct psb_ttm_fence_rep *fence_arg);

extern bool vsp_fence_poll(struct drm_device *dev);

extern int vsp_new_context(struct drm_device *dev, struct file *filp, int ctx_type);
extern void vsp_rm_context(struct drm_device *dev, struct file *filp, int ctx_type);
extern uint32_t psb_get_default_pd_addr(struct psb_mmu_driver *driver);

extern int psb_vsp_save_context(struct drm_device *dev);
extern int psb_vsp_restore_context(struct drm_device *dev);
extern int psb_check_vsp_idle(struct drm_device *dev);

void vsp_init_function(struct drm_psb_private *dev_priv);
void vsp_continue_function(struct drm_psb_private *dev_priv);
int vsp_resume_function(struct drm_psb_private *dev_priv);

extern int psb_vsp_dump_info(struct drm_psb_private *dev_priv);

extern void psb_powerdown_vsp(struct work_struct *work);
extern void vsp_irq_task(struct work_struct *work);
extern void vsp_cmd_submit_check(struct work_struct *work);

static inline
unsigned int vsp_is_idle(struct drm_psb_private *dev_priv,
			 unsigned int processor)
{
	unsigned int reg, start_bit, idle_bit;

	SP_REG_READ32(SP_STAT_AND_CTRL_REG, &reg, processor);
	start_bit = VSP_READ_FLAG(reg, SP_STAT_AND_CTRL_REG_START_FLAG);
	idle_bit = VSP_READ_FLAG(reg, SP_STAT_AND_CTRL_REG_READY_FLAG);

	return !start_bit && idle_bit;
}

static inline
unsigned int vsp_is_sleeping(struct drm_psb_private *dev_priv,
			     unsigned int processor)
{
	unsigned int reg;

	SP_REG_READ32(SP_STAT_AND_CTRL_REG, &reg, processor);
	return VSP_READ_FLAG(reg, SP_STAT_AND_CTRL_REG_SLEEP_FLAG);
}
#endif	/* _VSP_H_ */
