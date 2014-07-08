/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * jim liu <jim.liu@intel.com>
 * Jackie Li<yaodong.li@intel.com>
 */

#ifndef __MDFLD_DSI_OUTPUT_H__
#define __MDFLD_DSI_OUTPUT_H__

#include <linux/backlight.h>
#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>

#include "psb_drv.h"
#include "psb_intel_display.h"
#include "psb_intel_reg.h"
#include "psb_powermgmt.h"

#include <asm/intel-mid.h>
#include "mdfld_output.h"

/*mdfld DSI controller registers*/
#define MIPIA_DEVICE_READY_REG				0xb000
#define MIPIA_INTR_STAT_REG				0xb004
#define MIPIA_INTR_EN_REG				0xb008
#define MIPIA_DSI_FUNC_PRG_REG				0xb00c
#define MIPIA_HS_TX_TIMEOUT_REG				0xb010
#define MIPIA_LP_RX_TIMEOUT_REG				0xb014
#define MIPIA_TURN_AROUND_TIMEOUT_REG			0xb018
#define MIPIA_DEVICE_RESET_TIMER_REG			0xb01c
#define MIPIA_DPI_RESOLUTION_REG			0xb020
#define MIPIA_DBI_FIFO_THROTTLE_REG			0xb024
#define MIPIA_HSYNC_COUNT_REG				0xb028
#define MIPIA_HBP_COUNT_REG				0xb02c
#define MIPIA_HFP_COUNT_REG				0xb030
#define MIPIA_HACTIVE_COUNT_REG				0xb034
#define MIPIA_VSYNC_COUNT_REG				0xb038
#define MIPIA_VBP_COUNT_REG				0xb03c
#define MIPIA_VFP_COUNT_REG				0xb040
#define MIPIA_HIGH_LOW_SWITCH_COUNT_REG			0xb044
#define MIPIA_DPI_CONTROL_REG				0xb048
#define MIPIA_DPI_DATA_REG				0xb04c
#define MIPIA_INIT_COUNT_REG				0xb050
#define MIPIA_MAX_RETURN_PACK_SIZE_REG			0xb054
#define MIPIA_VIDEO_MODE_FORMAT_REG			0xb058
#define MIPIA_EOT_DISABLE_REG				0xb05c
#define CLOCK_STOP					(0x1 << 1)
#define DSI_EOT_DISABLE_MASK                            (0xff)

#define MIPIA_LP_BYTECLK_REG				0xb060
#define MIPIA_LP_GEN_DATA_REG				0xb064
#define MIPIA_HS_GEN_DATA_REG				0xb068
#define MIPIA_LP_GEN_CTRL_REG				0xb06c
#define MIPIA_HS_GEN_CTRL_REG				0xb070
#define MIPIA_GEN_FIFO_STAT_REG				0xb074
#define MIPIA_HS_LS_DBI_ENABLE_REG			0xb078
#define MIPIA_DPHY_PARAM_REG				0xb080
#define MIPIA_DBI_BW_CTRL_REG				0xb084
#define MIPIA_CLK_LANE_SWITCH_TIME_CNT_REG		0xb088

#define DSI_DEVICE_READY				(0x1)
#define DSI_POWER_STATE_ULPS_ENTER			(0x2 << 1)
#define DSI_POWER_STATE_ULPS_EXIT			(0x1 << 1)
#define DSI_POWER_STATE_ULPS_MASK			(0x3 << 1)


#define DSI_ONE_DATA_LANE				(0x1)
#define DSI_TWO_DATA_LANE				(0x2)
#define DSI_THREE_DATA_LANE				(0X3)
#define DSI_FOUR_DATA_LANE				(0x4)
#define DSI_DPI_VIRT_CHANNEL_OFFSET			(0x3)
#define DSI_DBI_VIRT_CHANNEL_OFFSET			(0x5)
#define DSI_DPI_COLOR_FORMAT_RGB565			(0x01 << 7)
#define DSI_DPI_COLOR_FORMAT_RGB666			(0x02 << 7)
#define DSI_DPI_COLOR_FORMAT_RGB666_UNPACK		(0x03 << 7)
#define DSI_DPI_COLOR_FORMAT_RGB888			(0x04 << 7)
#define DSI_DBI_COLOR_FORMAT_OPTION2			(0x05 << 13)

#define DSI_INTR_STATE_RXSOTERROR			BIT0

#define DSI_INTR_STATE_SPL_PKG_SENT			BIT30
#define DSI_INTR_STATE_TE				BIT31

#define DSI_HS_TX_TIMEOUT_MASK				(0xffffff)

#define DSI_LP_RX_TIMEOUT_MASK				(0xffffff)

#define DSI_TURN_AROUND_TIMEOUT_MASK			(0x3f)

#define DSI_RESET_TIMER_MASK				(0xffff)

#define DSI_DBI_FIFO_WM_HALF				(0x0)
#define DSI_DBI_FIFO_WM_QUARTER				(0x1)
#define DSI_DBI_FIFO_WM_LOW				(0x2)

#define DSI_DPI_TIMING_MASK				(0xffff)

#define DSI_INIT_TIMER_MASK				(0xffff)

#define DSI_DBI_RETURN_PACK_SIZE_MASK			(0x3ff)

#define DSI_LP_BYTECLK_MASK				(0x0ffff)

#define DSI_HS_CTRL_GEN_SHORT_W0			(0x03)
#define DSI_HS_CTRL_GEN_SHORT_W1			(0x13)
#define DSI_HS_CTRL_GEN_SHORT_W2			(0x23)
#define DSI_HS_CTRL_GEN_R0				(0x04)
#define DSI_HS_CTRL_GEN_R1				(0x14)
#define DSI_HS_CTRL_GEN_R2				(0x24)
#define DSI_HS_CTRL_GEN_LONG_W				(0x29)
#define DSI_HS_CTRL_MCS_SHORT_W0			(0x05)
#define DSI_HS_CTRL_MCS_SHORT_W1			(0x15)
#define DSI_HS_CTRL_MCS_R0				(0x06)
#define DSI_HS_CTRL_MCS_LONG_W				(0x39)
#define DSI_HS_CTRL_VC_OFFSET				(0x06)
#define DSI_HS_CTRL_WC_OFFSET				(0x08)

#define	DSI_FIFO_GEN_HS_DATA_FULL			BIT0
#define DSI_FIFO_GEN_HS_DATA_HALF_EMPTY			BIT1
#define DSI_FIFO_GEN_HS_DATA_EMPTY			BIT2
#define DSI_FIFO_GEN_LP_DATA_FULL			BIT8
#define DSI_FIFO_GEN_LP_DATA_HALF_EMPTY			BIT9
#define DSI_FIFO_GEN_LP_DATA_EMPTY			BIT10
#define DSI_FIFO_GEN_HS_CTRL_FULL			BIT16
#define DSI_FIFO_GEN_HS_CTRL_HALF_EMPTY			BIT17
#define DSI_FIFO_GEN_HS_CTRL_EMPTY			BIT18
#define DSI_FIFO_GEN_LP_CTRL_FULL			BIT24
#define DSI_FIFO_GEN_LP_CTRL_HALF_EMPTY			BIT25
#define DSI_FIFO_GEN_LP_CTRL_EMPTY			BIT26
#define DSI_FIFO_DBI_EMPTY				BIT27
#define DSI_FIFO_DPI_EMPTY				BIT28

#define DSI_DBI_HS_LP_SWITCH_MASK			(0x1)

#define DSI_HS_LP_SWITCH_COUNTER_OFFSET			(0x0)
#define DSI_LP_HS_SWITCH_COUNTER_OFFSET			(0x16)

#define DSI_DPI_CTRL_HS_SHUTDOWN			(0x00000001)
#define DSI_DPI_CTRL_HS_TURN_ON				(0x00000002)

/*mdfld DSI adapter registers*/
#define MIPIA_CONTROL_REG				0xb104
#define MIPIA_DATA_ADD_REG				0xb108
#define MIPIA_DATA_LEN_REG				0xb10c
#define MIPIA_CMD_ADD_REG				0xb110
#define MIPIA_CMD_LEN_REG				0xb114

/*DSI data lane configuration*/
enum {
	MDFLD_DSI_DATA_LANE_4_0 = 0,
	MDFLD_DSI_DATA_LANE_3_1 = 1,
	MDFLD_DSI_DATA_LANE_2_2 = 2,
};

enum {
	RESET_FROM_BOOT_UP = 0,
	RESET_FROM_OSPM_RESUME,
} ;

enum {
	MDFLD_DSI_VIDEO_NON_BURST_MODE_SYNC_PULSE = 1,
	MDFLD_DSI_VIDEO_NON_BURST_MODE_SYNC_EVENTS = 2,
	MDFLD_DSI_VIDEO_BURST_MODE = 3,
};

#define DSI_DPI_COMPLETE_LAST_LINE			BIT2
#define DSI_DPI_DISABLE_BTA					BIT3

struct mdfld_dsi_connector_state {
	u32 mipi_ctrl_reg;
};

struct mdfld_dsi_encoder_state {

};

struct mdfld_dsi_connector {
	/*
	 * This is ugly, but I have to use connector in it! :-(
	 * FIXME: use drm_connector instead.
	 */
	struct psb_intel_output base;

	int pipe;
	void *private;
	void *pkg_sender;
	void *err_detector;

	/*connection status*/
	enum drm_connector_status status;
};

struct mdfld_dsi_encoder {
	struct drm_encoder base;
	void *private;
};

/*display controller hardware context on a pipe*/
struct mdfld_dsi_hw_context {
	u32 vgacntr;

	/*plane*/
	u32 dspcntr;
	u32 dspsize;
	u32 dspsurf;
	u32 dsppos;
	u32 dspstride;
	u32 dsplinoff;

	/*plane*/
	u32 sprite_dspcntr;
	u32 sprite_dspsize;
	u32 sprite_dspsurf;
	u32 sprite_dsppos;
	u32 sprite_dspstride;
	u32 sprite_dsplinoff;

	/*Drain Latency*/
	u32 ddl1;
	u32 ddl2;
	u32 ddl3;
	u32 ddl4;

	u32 dsparb;
	u32 dsparb2;

	/*overlay*/
	u32 ovaadd;
	u32 ovcadd;

	/* gamma and csc */
	u32 palette[256];
	u32 color_coef[6];

	/*pipe regs*/
	u32 htotal;
	u32 hblank;
	u32 hsync;
	u32 vtotal;
	u32 vblank;
	u32 vsync;
	u32 pipestat;

	u32 pipesrc;

	u32 dpll;
	u32 fp;
	u32 pipeconf;

	/*mipi port*/
	u32 mipi;

	/*DSI controller regs*/
	u32 device_ready;
	u32 intr_stat;
	u32 intr_en;
	u32 dsi_func_prg;
	u32 hs_tx_timeout;
	u32 lp_rx_timeout;
	u32 turn_around_timeout;
	u32 device_reset_timer;
	u32 dpi_resolution;
	u32 dbi_fifo_throttle;
	u32 hsync_count;
	u32 hbp_count;
	u32 hfp_count;
	u32 hactive_count;
	u32 vsync_count;
	u32 vbp_count;
	u32 vfp_count;
	u32 high_low_switch_count;
	u32 dpi_control;
	u32 dpi_data;
	u32 init_count;
	u32 max_return_pack_size;
	u32 video_mode_format;
	u32 eot_disable;
	u32 lp_byteclk;
	u32 lp_gen_data;
	u32 hs_gen_data;
	u32 lp_gen_ctrl;
	u32 hs_gen_ctrl;
	u32 gen_fifo_stat;
	u32 hs_ls_dbi_enable;
	u32 dphy_param;
	u32 dbi_bw_ctrl;
	u32 clk_lane_switch_time_cnt;

	/*MIPI adapter regs*/
	u32 mipi_control;
	u32 mipi_data_addr;
	u32 mipi_data_len;
	u32 mipi_cmd_addr;
	u32 mipi_cmd_len;

	/*panel status*/
	int panel_on;
	int backlight_level;

	u32 pll_bypass_mode;
	u32 cck_div;
	/*brightness*/
	int lastbrightnesslevel;

	/*dpst register values*/
	u32 histogram_intr_ctrl;
	u32 histogram_logic_ctrl;
	u32 aimg_enhance_bin;
	u32 lvds_port_ctrl;

};

struct mdfld_dsi_hw_registers {
	u32 vgacntr_reg;

	/*plane*/
	u32 dspcntr_reg;
	u32 dspsize_reg;
	u32 dspsurf_reg;
	u32 dsplinoff_reg;
	u32 dsppos_reg;
	u32 dspstride_reg;

	/*Drain Latency*/
	u32 ddl1_reg;
	u32 ddl2_reg;
	u32 ddl3_reg;
	u32 ddl4_reg;

	/*overlay*/
	u32 ovaadd_reg;
	u32 ovcadd_reg;

	/* csc */
	u32 color_coef_reg;

	/*pipe regs*/
	u32 htotal_reg;
	u32 hblank_reg;
	u32 hsync_reg;
	u32 vtotal_reg;
	u32 vblank_reg;
	u32 vsync_reg;
	u32 pipestat_reg;

	u32 pipesrc_reg;

	u32 dpll_reg;
	u32 fp_reg;
	u32 pipeconf_reg;
	u32 palette_reg;
	u32 gamma_red_max_reg;
	u32 gamma_green_max_reg;
	u32 gamma_blue_max_reg;

	/*mipi port*/
	u32 mipi_reg;

	/*DSI controller regs*/
	u32 device_ready_reg;
	u32 intr_stat_reg;
	u32 intr_en_reg;
	u32 dsi_func_prg_reg;
	u32 hs_tx_timeout_reg;
	u32 lp_rx_timeout_reg;
	u32 turn_around_timeout_reg;
	u32 device_reset_timer_reg;
	u32 dpi_resolution_reg;
	u32 dbi_fifo_throttle_reg;
	u32 hsync_count_reg;
	u32 hbp_count_reg;
	u32 hfp_count_reg;
	u32 hactive_count_reg;
	u32 vsync_count_reg;
	u32 vbp_count_reg;
	u32 vfp_count_reg;
	u32 high_low_switch_count_reg;
	u32 dpi_control_reg;
	u32 dpi_data_reg;
	u32 init_count_reg;
	u32 max_return_pack_size_reg;
	u32 video_mode_format_reg;
	u32 eot_disable_reg;
	u32 lp_byteclk_reg;
	u32 lp_gen_data_reg;
	u32 hs_gen_data_reg;
	u32 lp_gen_ctrl_reg;
	u32 hs_gen_ctrl_reg;
	u32 gen_fifo_stat_reg;
	u32 hs_ls_dbi_enable_reg;
	u32 dphy_param_reg;
	u32 dbi_bw_ctrl_reg;
	u32 clk_lane_switch_time_cnt_reg;

	/*MIPI adapter regs*/
	u32 mipi_control_reg;
	u32 mipi_data_addr_reg;
	u32 mipi_data_len_reg;
	u32 mipi_cmd_addr_reg;
	u32 mipi_cmd_len_reg;

	/*dpst registers*/
	u32 histogram_intr_ctrl_reg;
	u32 histogram_logic_ctrl_reg;
	u32 aimg_enhance_bin_reg;
	u32 lvds_port_ctrl_reg;
};

#define NO_GAMMA_CSC			0x0
#define ENABLE_GAMMA			(0x1 << 0)
#define ENABLE_CSC			(0x1 << 1)
#define ENABLE_GAMMA_CSC		(ENABLE_GAMMA | ENABLE_CSC)
/*
 * DSI config, consists of one DSI connector, two DSI encoders.
 * DRM will pick up on DSI encoder basing on differents configs.
 */
struct mdfld_dsi_config {
	struct drm_device *dev;
	struct drm_display_mode *fixed_mode;
	struct drm_display_mode *mode;

	struct mdfld_dsi_connector *connector;
	struct mdfld_dsi_encoder *encoders[DRM_CONNECTOR_MAX_ENCODER];
	struct mdfld_dsi_encoder *encoder;

	struct mdfld_dsi_hw_registers regs;

	/*DSI hw context*/
	struct mutex context_lock;
	struct mdfld_dsi_hw_context dsi_hw_context;

	int pipe;
	int changed;

	int drv_ic_inited;

	int bpp;
	mdfld_dsi_encoder_t type;
	int lane_count;
	/*mipi data lane config*/
	int lane_config;
	/*Virtual channel number for this encoder*/
	int channel_num;
	/*video mode configure*/
	int video_mode;
	int enable_gamma_csc;
	uint32_t s3d_format;

	/*dsr*/
	void *dsr;
};

#define MDFLD_DSI_CONNECTOR(psb_output) \
	(container_of(psb_output, struct mdfld_dsi_connector, base))

#define MDFLD_DSI_ENCODER(encoder) \
	(container_of(encoder, struct mdfld_dsi_encoder, base))

#define MDFLD_DSI_ENCODER_WITH_DRM_ENABLE(encoder) \
		(container_of((struct drm_encoder *) encoder, \
		struct mdfld_dsi_encoder, base))

static inline struct mdfld_dsi_config *
mdfld_dsi_get_config(struct mdfld_dsi_connector *connector)
{
	if (!connector)
		return NULL;

	return (struct mdfld_dsi_config *)connector->private;
}

static inline void *mdfld_dsi_get_pkg_sender(struct mdfld_dsi_config *config)
{
	struct mdfld_dsi_connector *dsi_connector;

	if (!config)
		return NULL;

	dsi_connector = config->connector;

	if (!dsi_connector)
		return NULL;

	return dsi_connector->pkg_sender;
}

static inline struct mdfld_dsi_config *
mdfld_dsi_encoder_get_config(struct mdfld_dsi_encoder *encoder)
{
	if (!encoder)
		return NULL;
	return (struct mdfld_dsi_config *)encoder->private;
}

static inline struct mdfld_dsi_connector *
mdfld_dsi_encoder_get_connector(struct mdfld_dsi_encoder *encoder)
{
	struct mdfld_dsi_config *config;

	if (!encoder)
		return NULL;

	config = mdfld_dsi_encoder_get_config(encoder);
	if (!config)
		return NULL;

	return config->connector;
}

static inline void *
mdfld_dsi_encoder_get_pkg_sender(struct mdfld_dsi_encoder *encoder)
{
	struct mdfld_dsi_config *dsi_config;

	dsi_config = mdfld_dsi_encoder_get_config(encoder);
	if (!dsi_config)
		return NULL;

	return mdfld_dsi_get_pkg_sender(dsi_config);
}

static inline int mdfld_dsi_encoder_get_pipe(struct mdfld_dsi_encoder *encoder)
{
	struct mdfld_dsi_connector *connector;

	if (!encoder)
		return -1;

	connector = mdfld_dsi_encoder_get_connector(encoder);
	if (!connector)
		return -1;

	return connector->pipe;
}

/*Export functions*/
extern void mdfld_dsi_gen_fifo_ready(struct drm_device *dev,
		u32 gen_fifo_stat_reg, u32 fifo_stat);
extern void mdfld_dsi_brightness_init(struct mdfld_dsi_config *dsi_config,
		int pipe);
extern void mdfld_dsi_brightness_control(struct drm_device *dev, int pipe,
		int level);
extern int mdfld_dsi_output_init(struct drm_device *dev,
		int pipe,
		struct mdfld_dsi_config *config,
		struct panel_funcs *p_funcs);
extern int mdfld_dsi_get_panel_status(struct mdfld_dsi_config *dsi_config,
		u8 dcs,
		u8 *data,
		u8 transmission,
		u32 len);
extern int mdfld_dsi_get_power_mode(struct mdfld_dsi_config *dsi_config,
		u8 *mode,
		u8 transmission);
extern void mdfld_dsi_set_drain_latency(struct drm_encoder *encoder,
		struct drm_display_mode *mode);

#endif /*__MDFLD_DSI_OUTPUT_H__*/
