/*
 * Copyright (c) 2009, Intel Corporation.
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
 */

#ifndef __INTEL_DRV_H__
#define __INTEL_DRV_H__

#include <linux/i2c.h>
//#include <linux/i2c-id.h>
#include <linux/i2c-algo-bit.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <linux/gpio.h>

#define KEEP_UNUSED_CODE_S3D 0

/* Switch - don't change before PO */
#define MDFLD_GET_SYNC_BURST 0	/* Consider BURST_MODE when calcaulation H/V sync counts */
/* MDFLD FEATURE SWITCHES*/
/* MDFLD MIPI panels only one of them can be set to 1 */

/* MDFLD KSEL only one of them can be set to 1 */
#define KSEL_CRYSTAL_19_ENABLED 1
#define KSEL_BYPASS_19_ENABLED 0
#define KSEL_BYPASS_25_ENABLED 0
#define KSEL_BYPASS_83_100_ENABLE 0

#define KSEL_CRYSTAL_19 1
#define KSEL_BYPASS_19 5
#define KSEL_BYPASS_25 6
#define KSEL_BYPASS_83_100 7
/*
 * MOORESTOWN defines
 */
#define DELAY_TIME1 2000	/* 1000 = 1ms */

/*
 * Display related stuff
 */

/* store information about an Ixxx DVO */
/* The i830->i865 use multiple DVOs with multiple i2cs */
/* the i915, i945 have a single sDVO i2c bus - which is different */
#define MAX_OUTPUTS 6
/* maximum connectors per crtcs in the mode set */
#define INTELFB_CONN_LIMIT 4

#define INTEL_I2C_BUS_DVO 1
#define INTEL_I2C_BUS_SDVO 2

/* these are outputs from the chip - integrated only
 * external chips are via DVO or SDVO output */
#define INTEL_OUTPUT_UNUSED 0
#define INTEL_OUTPUT_ANALOG 1
#define INTEL_OUTPUT_DVO 2
#define INTEL_OUTPUT_SDVO 3
#define INTEL_OUTPUT_LVDS 4
#define INTEL_OUTPUT_TVOUT 5
#define INTEL_OUTPUT_HDMI 6
#define INTEL_OUTPUT_MIPI 7
#define INTEL_OUTPUT_MIPI2 8

#define INTEL_DVO_CHIP_NONE 0
#define INTEL_DVO_CHIP_LVDS 1
#define INTEL_DVO_CHIP_TMDS 2
#define INTEL_DVO_CHIP_TVOUT 4

enum s3d_structure {
	S3D_FRAME_PACKING = 0x0,
	S3D_FIELD_ALTERNATIVE = 0x1,
	S3D_LINE_ALTERNATIVE = 0x2,
	S3D_SIDE_BY_SIDE_FULL = 0x3,
	S3D_L_DEPTH = 0x4,
	S3D_L_DEPTH_GRAPHICS = 0x5,
	S3D_TOP_AND_BOTTOM = 0x6,
	S3D_SIDE_BY_SIDE_HALF = 0x8,
	S3D_SIDE_BY_SIDE_HALF_QUINCUNX = 0xF,
	S3D_LINE_ALTERNATIVE_HALF = 0x10,
	S3D_PIXEL_INTERLEAVING_HALF = 0x11,
	S3D_PIXEL_INTERLEAVING = 0x12,
	S3D_DISABLED = 0xff
};

/**
 * These define the S3D format in bit 
*/
#define S3D_FRAME_PACKING_BIT (1 << S3D_FRAME_PACKING)
#define S3D_FIELD_ALTERNATIVE_BIT (1 << S3D_FIELD_ALTERNATIVE)
#define S3D_LINE_ALTERNATIVE_BIT (1 << S3D_LINE_ALTERNATIVE)
#define S3D_SIDE_BY_SIDE_FULL_BIT (1 << S3D_SIDE_BY_SIDE_FULL)
#define S3D_L_DEPTH_BIT (1 << S3D_L_DEPTH)
#define S3D_L_DEPTH_GRAPHICS_BIT (1 << S3D_L_DEPTH_GRAPHICS)
#define S3D_TOP_AND_BOTTOM_BIT (1 << S3D_TOP_AND_BOTTOM)
#define S3D_SIDE_BY_SIDE_HALF_BIT (1 << S3D_SIDE_BY_SIDE_HALF)
#define S3D_SIDE_BY_SIDE_HALF_QUINCUNX_BIT (1 << S3D_SIDE_BY_SIDE_HALF_QUINCUNX)
#define S3D_PIXEL_INTERLEAVING_BIT (1 << S3D_PIXEL_INTERLEAVING)

#define S3D_STATE_MASK		(1 << 0)
#define S3D_STATE_ENALBLED	(1 << 0)
#define S3D_L_FRAME_ONLY	(1 << 1)

#if KEEP_UNUSED_CODE_S3D
struct mrfld_s3d_flip {
	unsigned long uiAddr_l;
	unsigned long uiAddr_r;
	unsigned long s3d_format;
	unsigned long pitch_l;
	unsigned long pitch_r;
	unsigned long s3d_state;
};
#endif /* if KEEP_UNUSED_CODE_S3D */

enum mipi_panel_type {
	NSC_800X480 = 1,
	LGE_480X1024 = 2,
	TPO_864X480 = 3
};

/**
 * Hold information useally put on the device driver privates here,
 * since it needs to be shared across multiple of devices drivers privates.
*/
struct psb_intel_mode_device {

	/*
	 * Abstracted memory manager operations
	 */
	void *(*bo_from_handle) (struct drm_device * dev,
				 struct drm_file * file_priv,
				 unsigned int handle);
	 size_t(*bo_size) (struct drm_device * dev, void *bo);
	 size_t(*bo_offset) (struct drm_device * dev, void *bo);
	int (*bo_pin_for_scanout) (struct drm_device * dev, void *bo);
	int (*bo_unpin_for_scanout) (struct drm_device * dev, void *bo);

	/*
	 * Cursor
	 */
	int cursor_needs_physical;

	/*
	 * LVDS info
	 */
	int backlight_duty_cycle;	/* restore backlight to this value */
	bool panel_wants_dither;
	struct drm_display_mode *panel_fixed_mode;
	struct drm_display_mode *panel_fixed_mode2;
	struct drm_display_mode *vbt_mode;	/* if any */

	uint32_t saveBLC_PWM_CTL;
};

struct psb_intel_i2c_chan {
	/* for getting at dev. private (mmio etc.) */
	struct drm_device *drm_dev;
	u32 reg;		/* GPIO reg */
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo;
	u8 slave_addr;
};

struct psb_intel_output {
	struct drm_connector base;

	struct drm_encoder enc;
	int type;
	struct i2c_adapter *hdmi_i2c_adapter;	/* for control functions */
	struct psb_intel_i2c_chan *i2c_bus;	/* for control functions */
	struct psb_intel_i2c_chan *ddc_bus;	/* for DDC only stuff */
	bool load_detect_temp;
	void *dev_priv;

	struct psb_intel_mode_device *mode_dev;

};

struct psb_intel_crtc_state {
	uint32_t saveDSPCNTR;
	uint32_t savePIPECONF;
	uint32_t savePIPESRC;
	uint32_t saveDPLL;
	uint32_t saveFP0;
	uint32_t saveFP1;
	uint32_t saveHTOTAL;
	uint32_t saveHBLANK;
	uint32_t saveHSYNC;
	uint32_t saveVTOTAL;
	uint32_t saveVBLANK;
	uint32_t saveVSYNC;
	uint32_t saveDSPSTRIDE;
	uint32_t saveDSPSIZE;
	uint32_t saveDSPPOS;
	uint32_t saveDSPBASE;
	uint32_t savePalette[256];
};

struct psb_intel_crtc {
	struct drm_crtc base;
	int pipe;
	int plane;
	uint32_t cursor_addr;
	u8 lut_r[256], lut_g[256], lut_b[256];
	u8 lut_adj[256];
	struct psb_intel_framebuffer *fbdev_fb;
	/* a mode_set for fbdev users on this crtc */
	struct drm_mode_set mode_set;

	/* current bo we scanout from */
	void *scanout_bo;

	/* current bo we cursor from */
	void *cursor_bo;

	struct drm_display_mode saved_mode;
	struct drm_display_mode saved_adjusted_mode;

	struct psb_intel_mode_device *mode_dev;

	/*crtc mode setting flags */
	u32 mode_flags;
	u32 scaling_type;
/*FIXME: Workaround to avoid MRST block.*/
#ifndef CONFIG_X86_MRST
	/* Saved Crtc HW states */
	struct psb_intel_crtc_state *crtc_state;
#endif
};

#define to_psb_intel_crtc(x)	\
		container_of(x, struct psb_intel_crtc, base)
#define to_psb_intel_output(x)	\
		container_of(x, struct psb_intel_output, base)
#define enc_to_psb_intel_output(x)	\
		container_of(x, struct psb_intel_output, enc)
#define to_psb_intel_framebuffer(x)	\
		container_of(x, struct psb_intel_framebuffer, base)

void mrst_init_TPO_MIPI(struct drm_device *dev);
void aava_koski_dsi_init(struct drm_device *dev,
			 struct psb_intel_mode_device *mode_dev);

extern void psb_intel_crtc_init(struct drm_device *dev, int pipe,
				struct psb_intel_mode_device *mode_dev);
extern void psb_intel_crt_init(struct drm_device *dev);
extern void mrst_wait_for_INTR_PKT_SENT(struct drm_device *dev);

extern void psb_intel_crtc_load_lut(struct drm_crtc *crtc);
extern void psb_intel_encoder_prepare(struct drm_encoder *encoder);
extern void psb_intel_encoder_commit(struct drm_encoder *encoder);

extern struct drm_encoder *psb_intel_best_encoder(struct drm_connector
						  *connector);

extern void psb_intel_wait_for_vblank(struct drm_device *dev);
extern int psb_intel_get_pipe_from_crtc_id(struct drm_device *dev, void *data,
					   struct drm_file *file_priv);
extern struct drm_crtc *psb_intel_get_crtc_from_pipe(struct drm_device *dev,
						     int pipe);
extern struct drm_framebuffer *psb_intel_framebuffer_create(struct drm_device
							    *dev, struct
							    drm_mode_fb_cmd2
							    *mode_cmd,
							    void *mm_private);
extern void mdfld_disable_crtc(struct drm_device *dev, int pipe);

extern void mdfld_dbi_update_panel(struct drm_device *dev, int pipe);
extern void mdfld_dsi_brightness_control(struct drm_device *dev, int pipe,
					 int level);
extern void mdfld_dsi_gen_fifo_ready(struct drm_device *dev,
				     u32 gen_fifo_stat_reg, u32 fifo_stat);
extern void mdfld_dsi_dbi_CB_ready(struct drm_device *dev,
				   u32 mipi_command_address_reg,
				   u32 gen_fifo_stat_reg);
extern void mdfldWaitForPipeDisable(struct drm_device *dev, int pipe);
extern void mdfldWaitForPipeEnable(struct drm_device *dev, int pipe);
extern uint8_t blc_pol;
extern uint8_t blc_freq;

extern void mrfld_setup_pll(struct drm_device *dev, int pipe, int clk);

#if KEEP_UNUSED_CODE_S3D
extern int mrfld_s3d_flip_surf_addr(struct drm_device *dev, int pipe, struct
				    mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_line_interleave_half(struct drm_device *dev, int pipe, struct
					     mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_line_interleave_half(struct drm_device *dev, int pipe, struct
					       mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_line_interleave(struct drm_device *dev, int pipe, struct
					mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_line_interleave(struct drm_device *dev, int pipe, struct
					  mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_frame_packing(struct drm_device *dev, int pipe, struct
				      mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_frame_packing(struct drm_device *dev, int pipe, struct
					mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_top_and_bottom(struct drm_device *dev, int pipe, struct
				       mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_top_and_bottom(struct drm_device *dev, int pipe, struct
					 mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_full_side_by_side(struct drm_device *dev, int pipe, struct
					  mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_full_side_by_side(struct drm_device *dev, int pipe, struct
					    mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_half_side_by_side(struct drm_device *dev, int pipe, struct
					  mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_half_side_by_side(struct drm_device *dev, int pipe, struct
					    mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_pixel_interleaving_full(struct drm_device *dev,
						int pipe, struct
						mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_pixel_interleaving_full(struct drm_device *dev,
						  int pipe, struct
						  mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_to_pixel_interleaving_half(struct drm_device *dev,
						int pipe, struct
						mrfld_s3d_flip *ps3d_flip);
extern int mrfld_s3d_from_pixel_interleaving_half(struct drm_device *dev,
						  int pipe, struct
						  mrfld_s3d_flip *ps3d_flip);
#endif /* if KEEP_UNUSED_CODE_S3D */

#endif				/* __INTEL_DRV_H__ */
