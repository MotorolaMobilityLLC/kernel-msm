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

#ifndef __MDFLD_DSI_DBI_H__
#define __MDFLD_DSI_DBI_H__

#include "mdfld_output.h"
#include "mdfld_dsi_output.h"

/*
 * DBI encoder which inherits from mdfld_dsi_encoder 
 */
struct mdfld_dsi_dbi_output {
	struct mdfld_dsi_encoder base;
	struct drm_display_mode *panel_fixed_mode;

	u8 last_cmd;
	u8 lane_count;
	u8 channel_num;

	struct drm_device *dev;

	/*DSR*/
	u32 dsr_idle_count;
	bool dsr_fb_update_done;

	/*mode setting flags*/
	u32 mode_flags;

	/*panel status*/
	bool dbi_panel_on;
	bool first_boot;
	struct panel_funcs *p_funcs;
};

struct mdfld_dbi_dsr_info {
	int dbi_output_num;
	struct mdfld_dsi_dbi_output *dbi_outputs[2];

	spinlock_t dsr_timer_lock;
	struct timer_list dsr_timer;
	u32 dsr_idle_count;
};

#define MDFLD_DSI_DBI_OUTPUT(dsi_encoder) \
	container_of(dsi_encoder, struct mdfld_dsi_dbi_output, base)

#define DBI_CB_TIMEOUT_COUNT	0xffff

/*DCS commands*/
#define enter_sleep_mode	0x10
#define exit_sleep_mode		0x11
#define set_display_off		0x28
#define	set_dispaly_on		0x29
#define set_column_address	0x2a
#define set_page_addr		0x2b
#define write_mem_start		0x2c

/*offsets*/
#define CMD_MEM_ADDR_OFFSET	0

#define CMD_DATA_SRC_SYSTEM_MEM	0
#define CMD_DATA_SRC_PIPE	1

/*export functions*/
extern void mdfld_dsi_dbi_exit_dsr(struct drm_device *dev,
		u32 update_src,
		void *p_surfaceAddr,
		bool check_hw_on_only);
extern void mdfld_dsi_dbi_enter_dsr(struct mdfld_dsi_dbi_output *dbi_output,
		int pipe);
extern int mdfld_dbi_dsr_init(struct drm_device *dev);
extern struct mdfld_dsi_encoder *mdfld_dsi_dbi_init(struct drm_device *dev,
		struct mdfld_dsi_connector *dsi_connector,
		struct panel_funcs *p_funcs);
extern void mdfld_reset_panel_handler_work(struct work_struct *work);
extern void mdfld_dbi_update_panel(struct drm_device *dev, int pipe);
extern int __dbi_power_on(struct mdfld_dsi_config *dsi_config);
extern int __dbi_power_off(struct mdfld_dsi_config *dsi_config);
uint32_t calculate_dbi_bw_ctrl(const uint32_t lane_count);
#endif /*__MDFLD_DSI_DBI_H__*/
