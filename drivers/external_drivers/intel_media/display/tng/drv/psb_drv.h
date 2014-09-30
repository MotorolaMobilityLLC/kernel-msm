/**************************************************************************
 * Copyright (c) 2007-2008, Intel Corporation.
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

#ifndef _PSB_DRV_H_
#define _PSB_DRV_H_

#include <linux/version.h>
#include <linux/panel_psb_drv.h>

#include <drm/drmP.h>
#include "psb_drm.h"
#include "psb_reg.h"

#include "psb_intel_drv.h"
#include "psb_hotplug.h"
#ifdef CONFIG_SUPPORT_MIPI
#include "psb_dpst.h"
#endif
#include "psb_gtt.h"
#include "ospm/pwr_mgmt.h"
#include "ttm/ttm_object.h"
#include "psb_ttm_fence_driver.h"
#include "psb_ttm_userobj_api.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_lock.h"
#include "psb_video_drv.h"
#include "mdfld_hdmi_audio_if.h"

#include "bufferclass_interface.h"
#include "pvrsrv_interface.h"
#include "displayclass_interface.h"
#include "display_callbacks.h"
#include <linux/wakelock.h>
#include <linux/pm_qos.h>

/*  Name changed with kernel 3.10 gen graphics patches. */
#if !defined DRM_MODE_ENCODER_DSI
#define DRM_MODE_ENCODER_DSI DRM_MODE_ENCODER_MIPI
#endif
#if !defined DRM_MODE_CONNECTOR_DSI
#define DRM_MODE_CONNECTOR_DSI DRM_MODE_CONNECTOR_MIPI
#endif

/*Append new drm mode definition here, align with libdrm definition*/
#define DRM_MODE_SCALE_NO_SCALE   4

extern int drm_psb_debug;
extern int drm_psb_dsr;
extern int drm_psb_enable_pr2_cabc;
extern int gfxrtdelay;
extern int drm_psb_te_timer_delay;
extern int drm_psb_enable_gamma;
extern int drm_psb_enable_color_conversion;
extern int drm_psb_set_gamma_success;
extern int drm_psb_set_gamma_pending;
extern int drm_psb_set_gamma_pipe;
extern int gamma_setting_save[256];
extern int csc_setting_save[6];
extern u32 DISP_PLANEB_STATUS;

extern struct ttm_bo_driver psb_ttm_bo_driver;

enum {
	CHIP_PSB_8108 = 0,
	CHIP_PSB_8109 = 1,
	CHIP_MRST_4100 = 2,
	CHIP_MDFLD_0130 = 3,
	CHIP_MRFLD_1180 = 4,
	CHIP_MRFLD_1480 = 5
};

enum enum_ports {
	PORT_A = 0,
	PORT_C = 1
} ;

#define PCI_ID_TOPAZ_DISABLED 0x4101

/*
 *Hardware bugfixes
 */

#define FIX_TG_16
#define FIX_TG_2D_CLOCKGATE
#define OSPM_STAT

#define DRIVER_NAME "pvrsrvkm"
#define DRIVER_DESC "drm driver for the Intel GMA500"
#define DRIVER_AUTHOR "Intel Corporation"
#define OSPM_PROC_ENTRY "ospm"
#define RTPM_PROC_ENTRY "rtpm"
#define DSR_PROC_ENTRY "dsr"
#define GFXPM_PROC_ENTRY "mrfld_gfx_pm"
#define VSPPM_PROC_ENTRY "mrfld_vsp_pm"
#define VEDPM_PROC_ENTRY "mrfld_ved_pm"
#define VECPM_PROC_ENTRY "mrfld_vec_pm"
#define BLC_PROC_ENTRY "mrst_blc"
#define DISPLAY_PROC_ENTRY "display_status"
#define PANEL_PROC_ENTRY "panel_status"
#define HDMI_PROC_ENTRY "hdmi_power"
#define GPIO_PROC_ENTRY "hdmi_gpio_control"
#define CSC_PROC_ENTRY "csc_control"


#define PSB_DRM_DRIVER_DATE "2009-03-10"
#define PSB_DRM_DRIVER_MAJOR 8
#define PSB_DRM_DRIVER_MINOR 1
#define PSB_DRM_DRIVER_PATCHLEVEL 0

#define PSB_VDC_OFFSET		 0x00000000
#define PSB_VDC_SIZE		 0x00080000
#define PSB_MMIO_RESOURCE	 0
#define PSB_GATT_RESOURCE	 2
#define PSB_GTT_RESOURCE	 3
#define PSB_GMCH_CTRL		 0x52
#define PSB_BSM			 0x5C

#define RGX_OFFSET		 0x00100000	/* GFX-IP IOSF-SB */
#define RGX_SIZE		 0x00010000

#define GFX_WRAPPER_OFFSET	 0x00160000	/* GFX Wrapper */
#define GFX_WRAPPER_SIZE	 0x00001000

#define VED_WRAPPER_OFFSET	 0x00161000	/* VED Wrapper */
#define VED_WRAPPER_SIZE	 0x00001000

#define VEC_WRAPPER_OFFSET	 0x00162000	/* VEC Wrapper */
#define VEC_WRAPPER_SIZE	 0x00001000

#define MRFLD_MSAC		 0x60
#define _APERTURE_SIZE_POS	 28
#define _APERTURE_SIZE_MASK	 ((1<<28) | (1<<29))
#define _1G_APERTURE_SIZE	 (1<<30)
#define _512M_APERTURE_SIZE      (1<<29)
#define _256M_APERTURE_SIZE      (1<<28)
#define _1G_APERTURE 		 0x0
#define _512M_APERTURE 		 0x1
#define _256M_APERTURE 		 0x2
#define MRFLD_BGSM		 0x70
#define _PSB_GMCH_ENABLED	 0x4
#define PSB_PGETBL_CTL		 0x2020
#define _PSB_PGETBL_ENABLED	 0x00000001
#define PSB_SGX_2D_SLAVE_PORT	 0x4000

/*
 *Flags for external memory type field.
 */
#define CONFIG_VIDEO_MRFLD  1
#define CONFIG_VIDEO_MRFLD_VP  1

#ifdef CONFIG_VIDEO_MRFLD
#define MRST_MSVDX_OFFSET	0x120000	/*MSVDX Base offset */
#else
#define MRST_MSVDX_OFFSET	0x90000	/*MSVDX Base offset */
#endif
#define PSB_MSVDX_OFFSET	0x50000	/*MSVDX Base offset */
/* MSVDX MMIO region is 0x50000 - 0x57fff ==> 32KB */
#define PSB_MSVDX_SIZE		0x10000

#define PSB_IED_DRM_CNTL_STATUS		0x2208

#define TNG_VSP_OFFSET		0x800000
#define TNG_VSP_SIZE		0x400000

#define LNC_TOPAZ_OFFSET	0xA0000
#define PNW_TOPAZ_OFFSET	0xC0000
#define TNG_TOPAZ_OFFSET	0x140000
#define LNC_TOPAZ_SIZE		0x10000
#define PNW_TOPAZ_SIZE		0x30000	/* PNW VXE285 has two cores */
#define TNG_TOPAZ_SIZE		0x20000
#define PSB_MMU_CACHED_MEMORY	  0x0001	/* Bind to MMU only */
#define PSB_MMU_RO_MEMORY	  0x0002	/* MMU RO memory */
#define PSB_MMU_WO_MEMORY	  0x0004	/* MMU WO memory */

/*
 *PTE's and PDE's
 */

#define PSB_PDE_MASK		  0x003FFFFF
#define PSB_PDE_SHIFT		  22
#define PSB_PTE_SHIFT		  12

#define PSB_PTE_VALID		  0x0001	/* PTE / PDE valid */
#define PSB_PTE_WO		  0x0002	/* Write only */
#define PSB_PTE_RO		  0x0004	/* Read only */
#define PSB_PTE_CACHED		  0x0008	/* CPU cache coherent */

/*
 * VSP PTE's and PDE's
 */

#define VSP_PDE_MASK		  0x003FFFFF
#define VSP_PDE_SHIFT		  24
#define VSP_PTE_SHIFT		  8

/* PTE / PDE valid */
#define VSP_PTE_VALID		  (0x1 << VSP_PDE_SHIFT)

/*
 *VDC registers and bits
 */
#define PSB_MSVDX_CLOCKGATING	  0x2064
#define PSB_TOPAZ_CLOCKGATING	  0x2068
#define PSB_HWSTAM		  0x2098
#define PSB_INSTPM		  0x20C0
#define PSB_INT_IDENTITY_R        0x20A4
#define _MDFLD_PIPEC_EVENT_FLAG   (1<<2)
#define _MDFLD_PIPEC_VBLANK_FLAG  (1<<3)
#define _PSB_DPST_PIPEB_FLAG      (1<<4)
#define _MDFLD_PIPEB_EVENT_FLAG   (1<<4)
#define _PSB_VSYNC_PIPEB_FLAG	  (1<<5)
#define _PSB_DPST_PIPEA_FLAG      (1<<6)
#define _PSB_PIPEA_EVENT_FLAG     (1<<6)
#define _PSB_VSYNC_PIPEA_FLAG	  (1<<7)
#define _MDFLD_MIPIA_FLAG	  (1<<16)
#define _MDFLD_MIPIC_FLAG	  (1<<17)
#define _PSB_IRQ_SGX_FLAG	  (1<<18)
#define _PSB_IRQ_MSVDX_FLAG	  (1<<19)
#define _LNC_IRQ_TOPAZ_FLAG	  (1<<20)
#define _TNG_IRQ_VSP_FLAG	  (1<<21)

/* This flag includes all the display IRQ bits excepts the vblank irqs. */
#define _MDFLD_DISP_ALL_IRQ_FLAG (_MDFLD_PIPEC_EVENT_FLAG | _MDFLD_PIPEB_EVENT_FLAG | \
        _PSB_PIPEA_EVENT_FLAG | _PSB_VSYNC_PIPEA_FLAG | _MDFLD_MIPIA_FLAG | _MDFLD_MIPIC_FLAG)
#define PSB_INT_IDENTITY_R	  0x20A4
#define PSB_INT_MASK_R		  0x20A8
#define PSB_INT_ENABLE_R	  0x20A0

#define _PSB_MMU_ER_MASK      0x0001FF00
#define _PSB_MMU_ER_HOST      (1 << 16)
#define GPIOA			0x5010
#define GPIOB			0x5014
#define GPIOC			0x5018
#define GPIOD			0x501c
#define GPIOE			0x5020
#define GPIOF			0x5024
#define GPIOG			0x5028
#define GPIOH			0x502c
#define GPIO_CLOCK_DIR_MASK		(1 << 0)
#define GPIO_CLOCK_DIR_IN		(0 << 1)
#define GPIO_CLOCK_DIR_OUT		(1 << 1)
#define GPIO_CLOCK_VAL_MASK		(1 << 2)
#define GPIO_CLOCK_VAL_OUT		(1 << 3)
#define GPIO_CLOCK_VAL_IN		(1 << 4)
#define GPIO_CLOCK_PULLUP_DISABLE	(1 << 5)
#define GPIO_DATA_DIR_MASK		(1 << 8)
#define GPIO_DATA_DIR_IN		(0 << 9)
#define GPIO_DATA_DIR_OUT		(1 << 9)
#define GPIO_DATA_VAL_MASK		(1 << 10)
#define GPIO_DATA_VAL_OUT		(1 << 11)
#define GPIO_DATA_VAL_IN		(1 << 12)
#define GPIO_DATA_PULLUP_DISABLE	(1 << 13)

#define VCLK_DIVISOR_VGA0   0x6000
#define VCLK_DIVISOR_VGA1   0x6004
#define VCLK_POST_DIV	    0x6010

#define PSB_COMM_2D (PSB_ENGINE_2D << 4)
#define PSB_COMM_3D (PSB_ENGINE_3D << 4)
#define PSB_COMM_TA (PSB_ENGINE_TA << 4)
#define PSB_COMM_HP (PSB_ENGINE_HP << 4)
#define PSB_COMM_USER_IRQ (1024 >> 2)
#define PSB_COMM_USER_IRQ_LOST (PSB_COMM_USER_IRQ + 1)
#define PSB_COMM_FW (2048 >> 2)

#define PSB_UIRQ_VISTEST	       1
#define PSB_UIRQ_OOM_REPLY	       2
#define PSB_UIRQ_FIRE_TA_REPLY	       3
#define PSB_UIRQ_FIRE_RASTER_REPLY     4

#define PSB_2D_SIZE (256*1024*1024)
#define PSB_MAX_RELOC_PAGES 1024

#define PSB_LOW_REG_OFFS 0x0204
#define PSB_HIGH_REG_OFFS 0x0600

#define PSB_NUM_VBLANKS 2

#define PSB_2D_SIZE (256*1024*1024)
#define PSB_MAX_RELOC_PAGES 1024

#define PSB_LOW_REG_OFFS 0x0204
#define PSB_HIGH_REG_OFFS 0x0600

#define PSB_NUM_VBLANKS 2
#define PSB_WATCHDOG_DELAY (DRM_HZ * 2)
#define PSB_LID_DELAY (DRM_HZ / 10)

#define MDFLD_PNW_A0 0x00
#define MDFLD_PNW_B0 0x04
#define MDFLD_PNW_C0 0x08

#define MDFLD_DSR_2D_3D_0 	BIT0
#define MDFLD_DSR_2D_3D_2 	BIT1
#define MDFLD_DSR_CURSOR_0 	BIT2
#define MDFLD_DSR_CURSOR_2	BIT3
#define MDFLD_DSR_OVERLAY_0 	BIT4
#define MDFLD_DSR_OVERLAY_2 	BIT5
#define MDFLD_DSR_MIPI_CONTROL	BIT6
#define MDFLD_DSR_DAMAGE_MASK_0	(BIT0 | BIT2 | BIT4)
#define MDFLD_DSR_DAMAGE_MASK_2	(BIT1 | BIT3 | BIT5)
#define MDFLD_DSR_2D_3D 	(MDFLD_DSR_2D_3D_0 | MDFLD_DSR_2D_3D_2)

#define MDFLD_DSR_RR 45
#define MDFLD_DPU_ENABLE BIT31
#define MDFLD_DSR_FULLSCREEN BIT30
#define MDFLD_DSR_DELAY (DRM_HZ / MDFLD_DSR_RR)

#define PSB_PWR_STATE_ON		1
#define PSB_PWR_STATE_OFF		2

#define PSB_PMPOLICY_NOPM		0
#define PSB_PMPOLICY_CLOCKGATING	1
#define PSB_PMPOLICY_POWERDOWN		2
#define PSB_PMPOLICY_HWIDLE		3
#define PSB_PMPOLICY_SUSPEND_HWIDLE	4
#define PSB_PMPOLICY_FORCE_PM		5


#define PSB_CGPOLICY_ON		0
#define PSB_CGPOLICY_GFXCG_DIS		1
#define PSB_CGPOLICY_VECCG_DIS		2
#define PSB_CGPOLICY_VEDCG_DIS		3

#define PSB_CMDPOLICY_PARALLEL		1

#define PSB_BOTTOM_HALF_WQ              1
#define PSB_BOTTOM_HALF_TQ              2

#define PSB_PMSTATE_POWERUP		0
#define PSB_PMSTATE_CLOCKGATED		1
#define PSB_PMSTATE_POWERDOWN		2
#define PSB_PCIx_MSI_ADDR_LOC		0x94
#define PSB_PCIx_MSI_DATA_LOC		0x98

#define MDFLD_PLANE_MAX_WIDTH		2048
#define MDFLD_PLANE_MAX_HEIGHT		2048

#define IS_POULSBO(dev) 0

#define IS_MDFLD(dev) (((dev)->pci_device & 0xfff8) == 0x0130)
#define IS_CTP(dev) (((dev->pci_device & 0xffff) == 0x08c0) ||	\
		((dev->pci_device & 0xffff) == 0x08c7) ||  \
		((dev->pci_device & 0xffff) == 0x08c8))
#define IS_MRFLD(dev) (((dev)->pci_device & 0xfff8) == 0x1180 || ((dev)->pci_device & 0xfff8) == 0x1480)

#define IS_TNG_A0(dev) ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) && (intel_mid_soc_stepping() == 0))

#if defined(CONFIG_DRM_CTP)
#define IS_TNG_B0(dev)		0
#elif defined(CONFIG_DRM_VXD_BYT)
#define IS_TNG_B0(dev)		0
#else
#define IS_TNG_B0(dev) ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) && (intel_mid_soc_stepping() == 1))
#endif

#define IS_ANN(dev) (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE)

#define IS_ANN_A0(dev) ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE) && (intel_mid_soc_stepping() == 0))

#define IS_ANN_B0(dev) ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE) && (intel_mid_soc_stepping() == 1))

#define IS_MOFD(dev) (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE)

#define IS_MID(dev) (IS_MDFLD(dev) || IS_MRFLD(dev))
#define IS_FLDS(dev) (IS_MDFLD(dev) || IS_MRFLD(dev))
#define IS_MSVDX(dev) (IS_MDFLD(dev) || IS_MRFLD(dev))
#define IS_TOPAZ(dev) (IS_MDFLD(dev) || IS_MRFLD(dev))

#define IS_MSVDX_MEM_TILE(dev) ((IS_MRFLD(dev)) || (IS_CTP(dev)))


/*
 *User options.
 */

struct drm_psb_uopt {
	int pad;		/*keep it here in case we use it in future */
};

#define MODE_SETTING_IN_CRTC 	0x1
#define MODE_SETTING_IN_ENCODER 0x2
#define MODE_SETTING_ON_GOING 	0x3
#define MODE_SETTING_IN_DSR 	0x4
#define MODE_SETTING_ENCODER_DONE 0x8
#define GCT_R10_HEADER_SIZE	16
#define GCT_R10_DISPLAY_DESC_SIZE	28

struct psb_context;
struct psb_validate_buffer;
struct psb_video_ctx;

/*  enum mdfld_dsi_encoder_t is required by mdfld_output.h */

typedef enum {
	MDFLD_DSI_ENCODER_DBI = 0,
	MDFLD_DSI_ENCODER_DPI,
} mdfld_dsi_encoder_t;

typedef enum {
	MDFLD_PIPE_A = 0,
	MDFLD_PIPE_B,
	MDFLD_PIPE_C,
	MDFLD_PIPE_MAX,
} mdfld_pipe_num;

struct drm_psb_private {
	/*
	 * DSI info.
	 */
	void *dbi_dsr_info;
#ifdef CONFIG_MID_DSI_DPU
	void *dbi_dpu_info;
#endif
	/* QOS */
	struct pm_qos_request s0ix_qos;

	struct mdfld_dsi_config *dsi_configs[2];

	struct workqueue_struct *vsync_wq;
	struct workqueue_struct *power_wq;

	struct work_struct te_work;
	int te_pipe;
	struct work_struct reset_panel_work;

	struct work_struct vsync_event_work;
	int vsync_pipe;

	struct mutex vsync_lock;
	atomic_t *vblank_count;
	bool vsync_enabled[3];

	bool pipea_dpi_underrun_count;
	bool pipec_dpi_underrun_count;

	/*
	 *TTM Glue.
	 */

	struct drm_global_reference mem_global_ref;
	struct ttm_bo_global_ref bo_global_ref;
	int has_global;

	struct drm_device *dev;
	struct ttm_object_device *tdev;
	struct ttm_fence_device fdev;
	struct ttm_bo_device bdev;
	struct ttm_lock ttm_lock;
	struct vm_operations_struct *ttm_vm_ops;
	int has_fence_device;
	int has_bo_device;

	unsigned long chipset;

	struct drm_psb_dev_info_arg dev_info;
	struct drm_psb_uopt uopt;

	struct psb_gtt *pg;

	/*GTT Memory manager */
	struct psb_gtt_mm *gtt_mm;

	struct page *scratch_page;
	uint32_t sequence[PSB_NUM_ENGINES];
	uint32_t last_sequence[PSB_NUM_ENGINES];
	uint32_t last_submitted_seq[PSB_NUM_ENGINES];

	struct psb_mmu_driver *mmu;
	struct psb_mmu_pd *pf_pd;

	/* VSP MMU */
	struct psb_mmu_driver *vsp_mmu;
	struct psb_mmu_pd *vsp_pf_pd;

	uint8_t *rgx_reg;
	uint8_t *wrapper_reg;
	uint8_t *ved_wrapper_reg;
	uint8_t *vec_wrapper_reg;
	uint8_t *vdc_reg;

	/* IMG video context */
	struct list_head video_ctx;
	spinlock_t video_ctx_lock;
	/* Current video context */
	struct psb_video_ctx *topaz_ctx;
	/* previous vieo context */
	struct psb_video_ctx *last_topaz_ctx;

	/*
	 *MSVDX
	 */
	uint8_t *msvdx_reg;
	atomic_t msvdx_mmu_invaldc;
	void *msvdx_private;

	/*
	 *TOPAZ
	 */
	uint8_t *topaz_reg;
	void *topaz_private;
	uint8_t topaz_disabled;
	uint32_t video_device_fuse;
	atomic_t topaz_mmu_invaldc;

	/*
	 * VSP
	 */
	uint8_t *vsp_reg;
	atomic_t vsp_mmu_invaldc;
	void *vsp_private;

	/*
	 *Fencing / irq.
	 */

	uint32_t vdc_irq_mask;
	uint32_t pipestat[PSB_NUM_PIPE];
	bool vblanksEnabledForFlips;

	spinlock_t irqmask_lock;
	spinlock_t sequence_lock;

	/*
	 *Modesetting
	 */
	struct psb_intel_mode_device mode_dev;

	struct drm_crtc *plane_to_crtc_mapping[PSB_NUM_PIPE];
	struct drm_crtc *pipe_to_crtc_mapping[PSB_NUM_PIPE];
	uint32_t num_pipe;

	/*
	 *Memory managers
	 */
	int have_tt;
	int have_mem_mmu;
	int have_mem_mmu_tiling;
	struct mutex temp_mem;

	/*
	 *Relocation buffer mapping.
	 */

	spinlock_t reloc_lock;
	unsigned int rel_mapped_pages;
	wait_queue_head_t rel_mapped_queue;

	/*
	 *SAREA
	 */
	struct drm_psb_sarea *sarea_priv;

	/*
	 *OSPM info
	 */
	uint8_t panel_desc;
	bool early_suspended;
	struct wake_lock ospm_wake_lock;

	/*
	 *MAXFIFO/ S0i1-Display info
	 */
	void * dc_maxfifo_info;


	/*
	 * Sizes info
	 */

	struct drm_psb_sizes_arg sizes;

	uint32_t fuse_reg_value;

	/* vbt (gct) header information */
	struct mrst_vbt vbt_data;
	/* info that is stored from the gct */
	struct gct_ioctl_arg gct_data;
	enum panel_type panel_id;

	/* pci revision id for B0:D2:F0 */
	uint8_t platform_rev_id;

	/* Feature bits from the VBIOS */
	unsigned int int_tv_support:1;
	unsigned int lvds_dither:1;
	unsigned int lvds_vbt:1;
	unsigned int int_crt_support:1;
	unsigned int lvds_use_ssc:1;
	int lvds_ssc_freq;
	bool is_lvds_on;

	/* MRST private date start */
	unsigned int core_freq;
	uint32_t iLVDS_enable;

	/* pipe config register value */
	uint32_t pipeconf;
	uint32_t pipeconf1;
	uint32_t pipeconf2;

	/* plane control register value */
	uint32_t dspcntr;
	uint32_t dspcntr1;
	uint32_t dspcntr2;

	/* MRST_DSI private date start */
	struct work_struct dsi_work;

	/*
	 *MRST DSI info
	 */

	/* The DPI panel power on */
	bool dpi_panel_on;

	/* The DBI panel power on */
	bool dbi_panel_on;

	/* The DPI display */
	bool dpi;

	enum mipi_panel_type panel_make;

	/* Set if MIPI encoder wants to control plane/pipe */
	bool dsi_plane_pipe_control;

	/* status */
	uint32_t videoModeFormat:2;
	uint32_t laneCount:3;
	uint32_t channelNumber:2;
	uint32_t status_reserved:25;

	/* dual display - DPI & DBI */
	bool dual_display;

	/* HS or LP transmission */
	bool lp_transmission;

	/* configuration phase */
	bool config_phase;

	/* first boot phase */
	bool first_boot;

	bool hdmi_first_boot;

	bool is_mipi_on;

	/* DSI clock */
	uint32_t RRate;
	uint32_t DDR_Clock;
	uint32_t DDR_Clock_Calculated;
	uint32_t ClockBits;

	/* DBI Buffer pointer */
	u32 DBI_CB_phys;
	u8 *p_DBI_commandBuffer;
	uint32_t DBI_CB_pointer;
	u8 *p_DBI_dataBuffer_orig;
	u8 *p_DBI_dataBuffer;
	uint32_t DBI_DB_pointer;

	/* DSI panel spec */
	uint32_t pixelClock;
	uint32_t HsyncWidth;
	uint32_t HbackPorch;
	uint32_t HfrontPorch;
	uint32_t HactiveArea;
	uint32_t VsyncWidth;
	uint32_t VbackPorch;
	uint32_t VfrontPorch;
	uint32_t VactiveArea;
	uint32_t bpp:5;
	uint32_t Reserved:27;
	/* MRST_DSI private date end */

	/* MDFLD_DSI private date start */
	/* dual display - DPI & DBI */
	bool dual_mipi;
	uint32_t ksel;
	uint32_t mipi_lane_config;
	uint32_t mipi_ctrl_display;
	/*
	 *MRST DSI info
	 */
	/* The DPI panel power on */
	bool dpi_panel_on2;

	/* The DBI panel power on */
	bool dbi_panel_on2;

	/* The DPI display */
	bool dpi2;

	/* status */
	uint32_t videoModeFormat2:2;
	uint32_t laneCount2:3;
	uint32_t channelNumber2:2;
	uint32_t status_reserved2:25;

	/* HS or LP transmission */
	bool lp_transmission2;

	/* configuration phase */
	bool config_phase2;

	/* DSI clock */
	uint32_t RRate2;
	uint32_t DDR_Clock2;
	uint32_t DDR_Clock_Calculated2;
	uint32_t ClockBits2;

	/* DBI Buffer pointer */
	u32 DBI_CB_phys2;
	u8 *p_DBI_commandBuffer2;
	uint32_t DBI_CB_pointer2;
	u8 *p_DBI_dataBuffer_orig2;
	u8 *p_DBI_dataBuffer2;

	/* DSI panel spec */
	uint32_t pixelClock2;
	uint32_t HsyncWidth2;
	uint32_t HbackPorch2;
	uint32_t HfrontPorch2;
	uint32_t HactiveArea2;
	uint32_t VsyncWidth2;
	uint32_t VbackPorch2;
	uint32_t VfrontPorch2;
	uint32_t VactiveArea2;
	uint32_t bpp2:5;
	uint32_t Reserved2:27;
	struct mdfld_dsi_dbi_output *dbi_output;
	struct mdfld_dsi_dbi_output *dbi_output2;
	/* MDFLD_DSI private date end */

	/* wait queue for write_mem_status complete (EOF interrupt) */
	wait_queue_head_t eof_wait;

	/*
	 *Register state
	 */
	uint32_t saveDSPACNTR;
	uint32_t saveDSPBCNTR;
	uint32_t savePIPEACONF;
	uint32_t savePIPEBCONF;
	uint32_t savePIPEASRC;
	uint32_t savePIPEBSRC;
	uint32_t saveFPA0;
	uint32_t saveFPA1;
	uint32_t saveDPLL_A;
	uint32_t saveDPLL_A_MD;
	uint32_t saveHTOTAL_A;
	uint32_t saveHBLANK_A;
	uint32_t saveHSYNC_A;
	uint32_t saveVTOTAL_A;
	uint32_t saveVBLANK_A;
	uint32_t saveVSYNC_A;
	uint32_t saveDSPASTRIDE;
	uint32_t saveDSPASIZE;
	uint32_t saveDSPAPOS;
	uint32_t saveDSPABASE;
	uint32_t saveDSPASURF;
	uint32_t saveDSPASTATUS;
	uint32_t saveFPB0;
	uint32_t saveFPB1;
	uint32_t saveDPLL_B;
	uint32_t saveDPLL_B_MD;
	uint32_t saveHTOTAL_B;
	uint32_t saveHBLANK_B;
	uint32_t saveHSYNC_B;
	uint32_t saveVTOTAL_B;
	uint32_t saveVBLANK_B;
	uint32_t saveVSYNC_B;
	uint32_t saveDSPBSTRIDE;
	uint32_t saveDSPBSIZE;
	uint32_t saveDSPBPOS;
	uint32_t saveDSPBBASE;
	uint32_t saveDSPBSURF;
	uint32_t saveDSPBSTATUS;
	uint32_t saveVCLK_DIVISOR_VGA0;
	uint32_t saveVCLK_DIVISOR_VGA1;
	uint32_t saveVCLK_POST_DIV;
	uint32_t saveVGACNTRL;
	uint32_t saveADPA;
	uint32_t saveLVDS;
	uint32_t saveDVOA;
	uint32_t saveDVOB;
	uint32_t saveDVOC;
	uint32_t savePP_ON;
	uint32_t savePP_OFF;
	uint32_t savePP_CONTROL;
	uint32_t savePP_CYCLE;
	uint32_t savePFIT_CONTROL;
	uint32_t savePaletteA[256];
	uint32_t savePaletteB[256];
	uint32_t saveBLC_PWM_CTL2;
	uint32_t saveBLC_PWM_CTL;
	uint32_t saveCLOCKGATING;
	uint32_t saveDSPARB;
	uint32_t saveDSPATILEOFF;
	uint32_t saveDSPBTILEOFF;
	uint32_t saveDSPAADDR;
	uint32_t saveDSPBADDR;
	uint32_t savePFIT_AUTO_RATIOS;
	uint32_t savePFIT_PGM_RATIOS;
	uint32_t savePP_ON_DELAYS;
	uint32_t savePP_OFF_DELAYS;
	uint32_t savePP_DIVISOR;
	uint32_t saveBSM;
	uint32_t saveBGSM;
	uint32_t saveVBT;
	uint32_t saveBCLRPAT_A;
	uint32_t saveBCLRPAT_B;
	uint32_t saveDSPALINOFF;
	uint32_t saveDSPBLINOFF;
	uint32_t savePERF_MODE;
	uint32_t saveDSPFW1;
	uint32_t saveDSPFW2;
	uint32_t saveDSPFW3;
	uint32_t saveDSPFW4;
	uint32_t saveDSPFW5;
	uint32_t saveDSPFW6;
	uint32_t saveCHICKENBIT;
	uint32_t saveDSPACURSOR_CTRL;
	uint32_t saveDSPBCURSOR_CTRL;
	uint32_t saveDSPACURSOR_BASE;
	uint32_t saveDSPBCURSOR_BASE;
	uint32_t saveDSPACURSOR_POS;
	uint32_t saveDSPBCURSOR_POS;
	uint32_t save_palette_a[256];
	uint32_t save_palette_b[256];
	uint32_t saveOV_OVADD;
	uint32_t saveOV_OGAMC0;
	uint32_t saveOV_OGAMC1;
	uint32_t saveOV_OGAMC2;
	uint32_t saveOV_OGAMC3;
	uint32_t saveOV_OGAMC4;
	uint32_t saveOV_OGAMC5;
	uint32_t saveOVC_OVADD;
	uint32_t saveOVC_OGAMC0;
	uint32_t saveOVC_OGAMC1;
	uint32_t saveOVC_OGAMC2;
	uint32_t saveOVC_OGAMC3;
	uint32_t saveOVC_OGAMC4;
	uint32_t saveOVC_OGAMC5;

	/*
	 * extra MDFLD Register state
	 */
	uint32_t saveHDMIPHYMISCCTL;
	uint32_t saveHDMIB_CONTROL;
	uint32_t saveDSPCCNTR;
	uint32_t savePIPECCONF;
	uint32_t savePIPECSRC;
	uint32_t saveHTOTAL_C;
	uint32_t saveHBLANK_C;
	uint32_t saveHSYNC_C;
	uint32_t saveVTOTAL_C;
	uint32_t saveVBLANK_C;
	uint32_t saveVSYNC_C;
	uint32_t saveDSPCSTRIDE;
	uint32_t saveDSPCSIZE;
	uint32_t saveDSPCPOS;
	uint32_t saveDSPCSURF;
	uint32_t saveDSPCSTATUS;
	uint32_t saveDSPCLINOFF;
	uint32_t saveDSPCTILEOFF;
	uint32_t saveDSPCCURSOR_CTRL;
	uint32_t saveDSPCCURSOR_BASE;
	uint32_t saveDSPCCURSOR_POS;
	uint32_t save_palette_c[256];
	uint32_t saveOV_OVADD_C;
	uint32_t saveOV_OGAMC0_C;
	uint32_t saveOV_OGAMC1_C;
	uint32_t saveOV_OGAMC2_C;
	uint32_t saveOV_OGAMC3_C;
	uint32_t saveOV_OGAMC4_C;
	uint32_t saveOV_OGAMC5_C;

	/* DSI reg save */
	uint32_t saveDEVICE_READY_REG;
	uint32_t saveINTR_EN_REG;
	uint32_t saveDSI_FUNC_PRG_REG;
	uint32_t saveHS_TX_TIMEOUT_REG;
	uint32_t saveLP_RX_TIMEOUT_REG;
	uint32_t saveTURN_AROUND_TIMEOUT_REG;
	uint32_t saveDEVICE_RESET_REG;
	uint32_t saveDPI_RESOLUTION_REG;
	uint32_t saveHORIZ_SYNC_PAD_COUNT_REG;
	uint32_t saveHORIZ_BACK_PORCH_COUNT_REG;
	uint32_t saveHORIZ_FRONT_PORCH_COUNT_REG;
	uint32_t saveHORIZ_ACTIVE_AREA_COUNT_REG;
	uint32_t saveVERT_SYNC_PAD_COUNT_REG;
	uint32_t saveVERT_BACK_PORCH_COUNT_REG;
	uint32_t saveVERT_FRONT_PORCH_COUNT_REG;
	uint32_t saveHIGH_LOW_SWITCH_COUNT_REG;
	uint32_t saveINIT_COUNT_REG;
	uint32_t saveMAX_RET_PAK_REG;
	uint32_t saveVIDEO_FMT_REG;
	uint32_t saveEOT_DISABLE_REG;
	uint32_t saveLP_BYTECLK_REG;
	uint32_t saveHS_LS_DBI_ENABLE_REG;
	uint32_t saveTXCLKESC_REG;
	uint32_t saveDPHY_PARAM_REG;
	uint32_t saveMIPI_CONTROL_REG;
	uint32_t saveMIPI;
	uint32_t saveMIPI_C;
	void (*init_drvIC) (struct drm_device * dev);
	void (*dsi_prePowerState) (struct drm_device * dev);
	void (*dsi_postPowerState) (struct drm_device * dev);

	/* RGX IRQ handler function and data */
	int (*pfn_rgxIrqHandler) (void * prgx_irqData);
	void * prgx_irqData;

	/* DPST Register Save */
	uint32_t saveHISTOGRAM_INT_CONTROL_REG;
	uint32_t saveHISTOGRAM_LOGIC_CONTROL_REG;
	// SH START DPST
	struct drm_connector *dpst_connector;
	// SH END DPST
	uint32_t savePWM_CONTROL_LOGIC;

	/* MSI reg save */

	uint32_t msi_addr;
	uint32_t msi_data;

	/*
	 *Scheduling.
	 */

	struct mutex reset_mutex;
	struct mutex cmdbuf_mutex;
	/*uint32_t ta_mem_pages;
	   struct psb_ta_mem *ta_mem;
	   int force_ta_mem_load; */
	atomic_t val_seq;

	/*
	 *TODO: change this to be per drm-context.
	 */

	struct psb_context decode_context;
	struct psb_context encode_context;
#ifdef SUPPORT_VSP
	struct psb_context vsp_context;
#endif
	/*
	 * LID-Switch
	 */
	spinlock_t lid_lock;
	struct timer_list lid_timer;
	u32 *lid_state;
	u32 lid_last_state;

	/*
	 *Watchdog
	 */

	spinlock_t watchdog_lock;
	struct timer_list watchdog_timer;
	struct work_struct watchdog_wq;
	struct work_struct msvdx_watchdog_wq;
	struct work_struct topaz_watchdog_wq;
	struct work_struct hdmi_hotplug_wq;
	struct work_struct hdmi_audio_wq;
	struct work_struct hdmi_audio_underrun_wq;
	struct work_struct hdmi_audio_bufferdone_wq;
	struct tasklet_struct hdmi_audio_bufferdone_tasklet;
	atomic_t hotplug_wq_done;
	int timer_available;

#ifdef OSPM_STAT
	unsigned char graphics_state;
	unsigned long gfx_on_time;
	unsigned long gfx_off_time;
	unsigned long gfx_last_mode_change;
	unsigned long gfx_on_cnt;
	unsigned long gfx_off_cnt;
#endif

	/*to be removed later */
	/*int dri_page_flipping;
	   int current_page;
	   int pipe_active[3];
	   int saved_start[2];
	   int saved_offset[2];
	   int saved_stride[2];

	   int flip_start[2];
	   int flip_offset[2];
	   int flip_stride[2]; */

	/*
	 * Used for modifying backlight from
	 * xrandr -- consider removing and using HAL instead
	 */
	struct drm_property *backlight_property;
	uint32_t blc_adj1;
	uint32_t blc_adj2;

	/*
	 * DPST and Hotplug state
	 */

	struct dpst_state *psb_dpst_state;
	struct hotplug_state *psb_hotplug_state;
	pfn_vsync_handler psb_vsync_handler;

	struct mutex dsr_mutex;
	bool b_dsr_enable_config;
	bool b_dsr_enable;
	bool dsr_fb_update_done_0;
	bool dsr_fb_update_done_2;
	uint32_t dsr_fb_update;
	uint32_t dsr_idle_count;
	bool b_is_in_idle;
	void (*exit_idle) (struct drm_device * dev, u32 update_src,
			   void *p_surfaceAddr, bool check_hw_on_only);

	bool b_async_flip_enable;
	/*
	 * DSR TIMER
	 */
	spinlock_t dsr_lock;
	struct timer_list dsr_timer;

	bool dsi_device_ready;
	bool hdmi_done_reading_edid;
	bool um_start;

	uint32_t tmds_clock_khz;
	had_event_call_back mdfld_had_event_callbacks;
	struct snd_intel_had_interface *had_interface;
	void *had_pvt_data;

	uint32_t hdmi_audio_interrupt_mask;

	struct mdfld_dsi_encoder *encoder0;
	struct mdfld_dsi_encoder *encoder2;
	mdfld_dsi_encoder_t mipi_encoder_type;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	/*psb fb dev */
	void *fbdev;
#endif
	uint32_t cur_pipe;

	struct mutex dpms_mutex;

	/* read register value through sysfs. */
	int count;
	char buf[256];

	/*
	 * HDMI config data
	*/
	void *hdmi_priv;

	/* indicate whether IED session is active */
	/* Maximum one active IED session at any given time */
	bool ied_enabled;
	bool ied_force_clean;
	/* indicate which source sets ied_enabled flag */
	struct file *ied_context;

#define DRM_PSB_HDMI_FLIP_ARRAY_SIZE 4
	void *flip_array[DRM_PSB_HDMI_FLIP_ARRAY_SIZE];
	unsigned int addr_array[DRM_PSB_HDMI_FLIP_ARRAY_SIZE];
	unsigned int flip_valid_size;
	unsigned int flip_head;
	unsigned int flip_tail;
	unsigned int flip_inited;
	unsigned int head_fliped;
	spinlock_t flip_lock;

	/*hdmi connected status */
	bool bhdmiconnected;
	bool dpms_on_off;
	struct workqueue_struct *hpd_detect;
/*	check it later
*	pfn_screen_event_handler pvr_screen_event_handler;
*/
	struct timer_list hdmi_timer;
	struct timer_list maxfifo_timer;
	/* fix Lock screen flip in resume issue */
	unsigned long init_screen_start;
	unsigned long init_screen_offset;
	unsigned long init_screen_size;
	unsigned long init_screen_stride;

	/* gamma and csc setting lock*/
	struct mutex gamma_csc_lock;
	/* overlay setting lock*/
	struct mutex overlay_lock;
	uint32_t overlay_wait;
	uint32_t overlay_fliped;
	int brightness_adjusted;

	/*
	 * S3D state 
	 */
	uint32_t cur_s3d_state;
	/* whether is DVI port */
	bool bDVIport;

	struct pci_dev *pci_root;
	bool bUseHFPLL;
	bool bRereadZero;
	bool panel_180_rotation;
};

struct psb_mmu_driver;

extern int drm_crtc_probe_output_modes(struct drm_device *dev, int, int);
extern int drm_pick_crtcs(struct drm_device *dev);
extern int mdfld_intel_crtc_set_gamma(struct drm_device *dev,
					struct gamma_setting *setting_data);

extern int mdfld_intel_crtc_set_color_conversion(struct drm_device *dev,
                                        struct csc_setting *setting_data);

static inline struct drm_psb_private *psb_priv(struct drm_device *dev)
{
	return (struct drm_psb_private *)dev->dev_private;
}

/*
 *psb_irq.c
 */

extern irqreturn_t psb_irq_handler(DRM_IRQ_ARGS);
extern int psb_irq_enable_dpst(struct drm_device *dev);
extern int psb_irq_disable_dpst(struct drm_device *dev);
extern int psb_dpst_diet_save(struct drm_device *dev);
extern int psb_dpst_diet_restore(struct drm_device *dev);
extern void psb_irq_preinstall(struct drm_device *dev);
extern int psb_irq_postinstall(struct drm_device *dev);
extern void psb_irq_uninstall(struct drm_device *dev);
extern void psb_irq_preinstall_islands(struct drm_device *dev, int hw_islands);
extern int psb_irq_postinstall_islands(struct drm_device *dev, int hw_islands);
extern void psb_irq_turn_on_dpst(struct drm_device *dev);
extern void psb_irq_turn_off_dpst(struct drm_device *dev);

extern void psb_irq_uninstall_islands(struct drm_device *dev, int hw_islands);
extern int psb_vblank_wait2(struct drm_device *dev, unsigned int *sequence);
extern int psb_vblank_wait(struct drm_device *dev, unsigned int *sequence);
extern int psb_enable_vblank(struct drm_device *dev, int crtc);
extern void psb_disable_vblank(struct drm_device *dev, int crtc);
void psb_enable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);

void psb_disable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);

void mid_enable_pipe_event(struct drm_psb_private *dev_priv, int pipe);

extern u32 psb_get_vblank_counter(struct drm_device *dev, int crtc);
extern int intel_get_vblank_timestamp(struct drm_device *dev, int pipe,
		int *max_error, struct timeval *vblank_time, unsigned flags);
extern int intel_get_crtc_scanoutpos(struct drm_device *dev, int pipe,
		int *vpos, int *hpos);
extern int mdfld_enable_te(struct drm_device *dev, int pipe);
extern int mdfld_recover_te(struct drm_device *dev, int pipe);
extern void mdfld_disable_te(struct drm_device *dev, int pipe);
extern int mrfl_enable_repeat_frame_intr(struct drm_device *dev,
					int idle_frame_count);
extern void mrfl_disable_repeat_frame_intr(struct drm_device *dev);
extern int mdfld_irq_enable_hdmi_audio(struct drm_device *dev);
extern int mdfld_irq_disable_hdmi_audio(struct drm_device *dev);
extern void psb_te_timer_func(unsigned long data);
extern void mdfld_te_handler_work(struct work_struct *te_work);
extern void mdfld_vsync_event_work(struct work_struct *work);

#ifdef CONFIG_SUPPORT_HDMI
void hdmi_do_audio_underrun_wq(struct work_struct *work);
void hdmi_do_audio_bufferdone_wq(struct work_struct *work);
void hdmi_audio_bufferdone_tasklet_func(unsigned long data);
#endif
extern u32 intel_vblank_count(struct drm_device *dev, int pipe);

/*
 *psb_fb.c
 */
extern int psbfb_probed(struct drm_device *dev);
extern int psbfb_remove(struct drm_device *dev, struct drm_framebuffer *fb);
extern int psbfb_kms_off_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
extern int psbfb_kms_on_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
extern void *psbfb_vdc_reg(struct drm_device *dev);

extern void psb_print_pagefault(struct drm_psb_private *dev_priv);
extern void mdfld_dsr_timer_init(struct drm_psb_private *dev_priv);
extern void mdfld_dsr_timer_takedown(struct drm_psb_private *dev_priv);

extern int mid_irq_enable_hdmi_audio(struct drm_device *dev);
extern int mid_irq_disable_hdmi_audio(struct drm_device *dev);

/* modesetting */
extern void psb_modeset_init(struct drm_device *dev);
extern void psb_modeset_cleanup(struct drm_device *dev);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
/*fbdev*/
extern int psb_fbdev_init(struct drm_device *dev);
#endif

/* psb_bl.c */
extern int psb_brightness;
int psb_backlight_init(struct drm_device *dev);
void psb_backlight_exit(void);
int psb_set_brightness(struct backlight_device *bd);
int psb_get_brightness(struct backlight_device *bd);
struct backlight_device *psb_get_backlight_device(void);

/*
 *Debug print bits setting
 */
#define PSB_D_GENERAL (1 << 0)
#define PSB_D_INIT    (1 << 1)
#define PSB_D_IRQ     (1 << 2)
#define PSB_D_ENTRY   (1 << 3)
/* debug the get H/V BP/FP count */
#define PSB_D_HV      (1 << 4)
#define PSB_D_DBI_BF  (1 << 5)
#define PSB_D_PM      (1 << 6)
#define PSB_D_RENDER  (1 << 7)
#define PSB_D_REG     (1 << 8)
#define PSB_D_MSVDX   (1 << 9)
#define PSB_D_TOPAZ   (1 << 10)
#define VSP_D_LOG   (1 << 11)
#define VSP_D_PERF   (1 << 12)
#define PSB_D_WARN    (1 << 13)
#define PSB_D_MIPI    (1 << 14)
#define PSB_D_BL    (1 << 15)
#define PSB_D_DPST    (1 << 16)

#ifndef DRM_DEBUG_CODE
/* To enable debug printout, set drm_psb_debug in psb_drv.c
 * to any combination of above print flags.
 */
#define DRM_DEBUG_CODE 2
#endif

extern int drm_psb_debug;
extern int drm_psb_no_fb;
extern int drm_idle_check_interval;
extern int drm_topaz_sbuswa;

#define PSB_DEBUG_GENERAL(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_GENERAL, _fmt, ##_arg)
#define PSB_DEBUG_INIT(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_INIT, _fmt, ##_arg)
#define PSB_DEBUG_IRQ(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_IRQ, _fmt, ##_arg)
#define PSB_DEBUG_ENTRY(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_ENTRY, _fmt, ##_arg)
#define PSB_DEBUG_HV(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_HV, _fmt, ##_arg)
#define PSB_DEBUG_DBI_BF(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_DBI_BF, _fmt, ##_arg)
#define PSB_DEBUG_PM(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_PM, _fmt, ##_arg)
#define PSB_DEBUG_RENDER(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_RENDER, _fmt, ##_arg)
#define PSB_DEBUG_REG(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_REG, _fmt, ##_arg)
#define PSB_DEBUG_MSVDX(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_MSVDX, _fmt, ##_arg)
#define PSB_DEBUG_TOPAZ(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_TOPAZ, _fmt, ##_arg)
#define VSP_DEBUG(_fmt, _arg...) \
	PSB_DEBUG(VSP_D_LOG, "VSP: "_fmt, ##_arg)
#define VSP_PERF(_fmt, _arg...) \
	PSB_DEBUG(VSP_D_PERF, "VSP PERFORMANCE: "_fmt, ##_arg)
/* force to print WARN msg */
#define PSB_DEBUG_WARN(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_WARN, _fmt, ##_arg)
#define PSB_DEBUG_MIPI(_fmt, _arg...) \
	PSB_DEBUG(PSB_D_MIPI, _fmt, ##_arg)
#define PSB_DEBUG_BL(_fmt, _arg...) \
        PSB_DEBUG(PSB_D_BL, _fmt, ##_arg)
#define PSB_DEBUG_DPST(_fmt, _arg...) \
        PSB_DEBUG(PSB_D_DPST, _fmt, ##_arg)

#if DRM_DEBUG_CODE
#define PSB_DEBUG(_flag, _fmt, _arg...)					\
	do {								\
		if (unlikely((_flag) & drm_psb_debug))			\
			printk(KERN_INFO				\
			       "[psb:0x%02x:%s] " _fmt , _flag,		\
			       __func__ , ##_arg);			\
	} while (0)
#else
#define PSB_DEBUG(_fmt, _arg...)     do { } while (0)
#endif

/*
 *Utilities
 */
#define DRM_DRIVER_PRIVATE_T struct drm_psb_private

#define MAX_READ_COUNT		0x3
static inline uint32_t REGISTER_READ(struct drm_device *dev, uint32_t reg)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	int i = 0;
	int reg_val = ioread32(dev_priv->vdc_reg + (reg));

	/* we might need to re-read registers if video mode panel */
	if (dev_priv->bRereadZero) {
		if (!reg_val) {
			for (i = 0; i < MAX_READ_COUNT; i++) {
				reg_val = ioread32(dev_priv->vdc_reg + (reg));
				if (reg_val)
					break;
			}
		}

		if (i == MAX_READ_COUNT) {
			PSB_DEBUG_REG("Register (reg = 0x%x) read failure.\n",
					reg);
		}
	}

	PSB_DEBUG_REG("reg = 0x%x. reg_val = 0x%x. \n", reg, reg_val);
	return reg_val;
}

#define REG_READ(reg)	       REGISTER_READ(dev, (reg))


static inline void REGISTER_WRITE(struct drm_device *dev, uint32_t reg,
				  uint32_t val)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	if ((reg < 0x70084 || reg > 0x70088) && (reg < 0xa000 || reg > 0xa3ff))
		PSB_DEBUG_REG("reg = 0x%x, val = 0x%x.\n", reg, val);
	iowrite32((val), dev_priv->vdc_reg + (reg));
}

#define REG_WRITE(reg, val)	REGISTER_WRITE(dev, (reg), (val))

static inline void REGISTER_WRITE16(struct drm_device *dev,
				    uint32_t reg, uint32_t val)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	PSB_DEBUG_REG("reg = 0x%x, val = 0x%x. \n", reg, val);

	iowrite16((val), dev_priv->vdc_reg + (reg));
}

#define REG_WRITE16(reg, val)	  REGISTER_WRITE16(dev, (reg), (val))

static inline void REGISTER_WRITE8(struct drm_device *dev,
				   uint32_t reg, uint32_t val)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	PSB_DEBUG_REG("reg = 0x%x, val = 0x%x. \n", reg, val);

	iowrite8((val), dev_priv->vdc_reg + (reg));
}

#define REG_WRITE8(reg, val)	 REGISTER_WRITE8(dev, (reg), (val))

#define PSB_ALIGN_TO(_val, _align) \
  (((_val) + ((_align) - 1)) & ~((_align) - 1))

#define PSB_WVDC32(_val, _offs)		REG_WRITE(_offs, _val)
#define PSB_RVDC32(_offs)		REG_READ(_offs)

static inline uint32_t RGX_REGISTER_READ(struct drm_device *dev, uint32_t reg)
{
       struct drm_psb_private *dev_priv = dev->dev_private;
       int reg_val = ioread32(dev_priv->rgx_reg + (reg));
       PSB_DEBUG_REG("rgx reg = 0x%x. reg_val = 0x%x. \n", reg, reg_val);
       return reg_val;
}

#define RGX_REG_READ(reg)             RGX_REGISTER_READ(dev, (reg))
static inline void RGX_REGISTER_WRITE(struct drm_device *dev, uint32_t reg,
						uint32_t val)
{
       struct drm_psb_private *dev_priv = dev->dev_private;
       PSB_DEBUG_REG("rgx reg = 0x%x, val = 0x%x. \n", reg, val);

       iowrite32((val), dev_priv->rgx_reg + (reg));
}

#define RGX_REG_WRITE(reg, val)        RGX_REGISTER_WRITE(dev, (reg), (val))

static inline uint32_t WRAPPER_REGISTER_READ(struct drm_device *dev, uint32_t reg)
{
       struct drm_psb_private *dev_priv = dev->dev_private;
       int reg_val = ioread32(dev_priv->wrapper_reg + (reg));
       PSB_DEBUG_REG("wrapper reg = 0x%x. reg_val = 0x%x. \n", reg, reg_val);
       return reg_val;
}

#define WRAPPER_REG_READ(reg)             WRAPPER_REGISTER_READ(dev, (reg))
static inline void WRAPPER_REGISTER_WRITE(struct drm_device *dev, uint32_t reg,
						uint32_t val)
{
       struct drm_psb_private *dev_priv = dev->dev_private;
       PSB_DEBUG_REG("wrapper reg = 0x%x, val = 0x%x. \n", reg, val);

       iowrite32((val), dev_priv->wrapper_reg + (reg));
}

#define WRAPPER_REG_WRITE(reg, val)        WRAPPER_REGISTER_WRITE(dev, (reg), (val))

#define PSB_ALPL(_val, _base)			\
  (((_val) >> (_base ## _ALIGNSHIFT)) << (_base ## _SHIFT))
#define PSB_ALPLM(_val, _base)			\
  ((((_val) >> (_base ## _ALIGNSHIFT)) << (_base ## _SHIFT)) & (_base ## _MASK))

extern int drm_psb_ospm;
extern int drm_psb_cpurelax;
extern int drm_psb_udelaydivider;
extern int drm_psb_gl3_enable;
extern int drm_psb_topaz_clockgating;
extern int drm_vsp_burst;
extern int drm_vsp_force_up_freq;
extern int drm_vsp_force_down_freq;
extern int drm_vsp_single_int;
extern int drm_vec_force_up_freq;
extern int drm_vec_force_down_freq;

extern int drm_decode_flag;

#define PSB_DEFAULT_HDMI_FB_WIDTH  1920
#define PSB_DEFAULT_HDMI_FB_HEIGHT 1080

extern char HDMI_EDID[20];
extern int hdmi_state;
extern int drm_vsp_vpp_batch_cmd;
extern int drm_video_sepkey;

/*
 * set cpu_relax = 1 in sysfs to use cpu_relax instead of udelay bysy loop
 * set udelay_divider to reduce the udelay values,e.g.= 10, reduce 10 times
 */
#define PSB_UDELAY(usec)                        \
do {                                            \
	if (drm_psb_cpurelax == 0)              \
		DRM_UDELAY(usec / drm_psb_udelaydivider);   \
	else                                    \
		cpu_relax();                    \
} while (0)

#endif
