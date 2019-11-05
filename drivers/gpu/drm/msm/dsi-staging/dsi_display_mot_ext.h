/*
 * Copyright (c) 2015-2019, The Linux Foundation.All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DSI_DISPLAY_MOT_EXT_H_
#define _DSI_DISPLAY_MOT_EXT_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "dsi_defs.h"
#include "dsi_ctrl.h"
#include "dsi_phy.h"
#include "dsi_panel.h"
#include "sde_connector.h"
#include "sde_motUtil.h"
#include "dsi_display.h"
#include <linux/alarmtimer.h>

#define LCD_PARA_LINE_LEN 2*1024

enum dsi_display_early_power_state {
	DSI_EARLY_POWER_IDLE = 0,
	DSI_EARLY_POWER_BEGIN,
	DSI_EARLY_POWER_PREPARED,
	DSI_EARLY_POWER_INITIALIZED,
	DSI_EARLY_POWER_FORBIDDEN,
	DSI_EARLY_POWER_STATE_NUM
};

struct dsi_mot_ext_feature {
	struct dsi_display *display;
	struct workqueue_struct *early_power_workq;
	struct work_struct early_on_work;
	struct delayed_work early_off_work;
	enum dsi_display_early_power_state early_power_state;
	struct wakeup_source early_wake_src;
};

/**
 * dsi_display_ext_init() - Moto early power init
 * @display:          Handle to display.
 *
 * Prepare for early power up sequences for the primary panel.
 *
 * Return: error code.
 */
int dsi_display_ext_init(struct dsi_display *display);

#endif /* _DSI_DISPLAY_MOT_EXT_H_ */
