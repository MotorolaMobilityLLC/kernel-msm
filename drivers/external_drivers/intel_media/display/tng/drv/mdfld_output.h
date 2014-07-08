/*
 * Copyright (c)  2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicensen
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
 * Thomas Eaton <thomas.g.eaton@intel.com>
 * Scott Rowe <scott.m.rowe@intel.com>
*/



#ifndef MDFLD_OUTPUT_H
#define MDFLD_OUTPUT_H

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>

#include "psb_drv.h"

#ifdef CONFIG_SUPPORT_MIPI

#define TPO_PANEL_WIDTH		84
#define TPO_PANEL_HEIGHT	46
#define TMD_PANEL_WIDTH		53 /* PR3 */
#define TMD_PANEL_HEIGHT	89 /* PR3 */
#define PYR_PANEL_WIDTH		53
#define PYR_PANEL_HEIGHT	95
#define PANEL_4DOT3_WIDTH	53
#define PANEL_4DOT3_HEIGHT	95
#define AUO_PANEL_WIDTH		54
#define AUO_PANEL_HEIGHT	96
#define PANEL_3DOT47_WIDTH	49
#define PANEL_3DOT47_HEIGHT	73

struct mdfld_dsi_config;

/*DSI panel connection status*/
enum {
	MDFLD_DSI_PANEL_CONNECTED = 0,
	MDFLD_DSI_PANEL_DISCONNECTED,
};

enum {
	MDFLD_DSI_PANEL_POWER_ON = 0,
	MDFLD_DSI_PANEL_POWER_OFF,
};

struct panel_info {
	u32 width_mm;
	u32 height_mm;

	bool panel_180_rotation;
	/*other infos*/
};

/**
 *Panel specific callbacks.
 *
 *@get_config_mode: return the fixed display mode of panel.
 *@update_fb: command mode panel only. update on-panel framebuffer.
 *@get_panel_info: return panel information. such as physical size.
 *@reset: panel hard reset.
 *@drv_ic_init: initialize panel driver IC and additional HW initialization.
 *@detect: return the panel physical connection status.
 *@dsi_controller_init: Initialize MIPI IP for this panel.
 *@power_on: turn on panel. e.g. send a TURN_ON special packet.
 *@power_off: turn off panel. e.g. send a SHUT_DOWN special packet.
 *
 *When adding a new panel, the driver code should implement these callbacks
 *according to corresponding panel specs. DPI and DBI implementation will
 *call these callbacks to take the specific actions for the new panel.
 */
struct panel_funcs {
	struct drm_display_mode *(*get_config_mode)(void);
	void (*dsi_controller_init)(struct mdfld_dsi_config *dsi_config);
	void (*get_panel_info)(int, struct panel_info *);
	int (*reset)(struct mdfld_dsi_config *dsi_config);
	int (*exit_deep_standby)(struct mdfld_dsi_config *dsi_config);
	int (*detect)(struct mdfld_dsi_config *dsi_config);
	int (*power_on)(struct mdfld_dsi_config *dsi_config);
	int (*power_off)(struct mdfld_dsi_config *dsi_config);
	int (*set_brightness)(struct mdfld_dsi_config *dsi_config, int level);
	int (*drv_ic_init)(struct mdfld_dsi_config *dsi_config);
	int (*drv_set_panel_mode)(struct mdfld_dsi_config *dsi_config);
};

struct intel_mid_panel_list {
	enum panel_type p_type;
	int encoder_type;
	void (*panel_init)(struct drm_device *, struct panel_funcs *);
};

extern enum panel_type get_panel_type(struct drm_device *dev, int pipe);
extern bool is_dual_dsi(struct drm_device *dev);
extern bool is_dual_panel(struct drm_device *dev);
extern mdfld_dsi_encoder_t is_panel_vid_or_cmd(struct drm_device *dev);
#endif

extern void mdfld_output_init(struct drm_device *dev);

#endif
