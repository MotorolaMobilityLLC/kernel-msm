/* Copyright (c) 2016, Motorola Mobility, LLC.
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
#ifndef _MOD_DISPLAY_H_
#define _MOD_DISPLAY_H_

#ifdef __KERNEL__

#include <linux/types.h>

#define MOD_DISPLAY_NAME	"mod_display"

enum mod_display_type {
	MOD_DISPLAY_TYPE_INVALID,
	MOD_DISPLAY_TYPE_DSI,
	MOD_DISPLAY_TYPE_DP,
};

enum mod_display_config_type {
	MOD_CONFIG_INVALID,
	MOD_CONFIG_EDID_1_3,
	MOD_CONFIG_DSI_CONF,
	MOD_CONFIG_EDID_DOWNSTREAM,
};

struct mod_display_panel_config {
	u8 display_type;
	u8 config_type;
	u32 config_size;
	char config_buf[0];
};

enum mod_display_dsi_config_mode {
	MOD_DISPLAY_DSI_CONFIG_MODE_VIDEO   = 0x00,
	MOD_DISPLAY_DSI_CONFIG_MODE_COMMAND = 0x01,
};

enum mod_display_dsi_config_swap {
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_RGB = 0x00,
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_RBG = 0x01,
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_BGR = 0x02,
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_BRG = 0x03,
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_GRB = 0x04,
	MOD_DISPLAY_DSI_CONFIG_SWAP_RGB_TO_GBR = 0x05,
};

enum mod_display_dsi_config_continuous_clock {
	MOD_DISPLAY_DSI_CONFIG_CONTINUOUS_CLOCK_DISABLED = 0x00,
	MOD_DISPLAY_DSI_CONFIG_CONTINUOUS_CLOCK_ENABLED  = 0x01,
};

enum mod_display_dsi_config_eot_mode {
	MOD_DISPLAY_DSI_CONFIG_EOT_MODE_NONE   = 0x00,
	MOD_DISPLAY_DSI_CONFIG_EOT_MODE_APPEND = 0x01,
};

enum mod_display_dsi_config_vsync_mode {
	MOD_DISPLAY_DSI_CONFIG_VSYNC_MODE_NONE = 0x00,
	MOD_DISPLAY_DSI_CONFIG_VSYNC_MODE_GPIO = 0x01,
	MOD_DISPLAY_DSI_CONFIG_VSYNC_MODE_DCS  = 0x02,
};

enum mod_display_dsi_config_traffic_mode {
	MOD_DISPLAY_DSI_CONFIG_TRAFFIC_MODE_NON_BURST_SYNC_PULSE = 0x00,
	MOD_DISPLAY_DSI_CONFIG_TRAFFIC_MODE_NON_BURST_SYNC_EVENT = 0x01,
	MOD_DISPLAY_DSI_CONFIG_TRAFFIC_MODE_BURST                = 0x02,
};

enum mod_display_dsi_config_pixel_packing {
	MOD_DISPLAY_DSI_CONFIG_PIXEL_PACKING_UNPACKED = 0x00,
};

struct mod_display_dsi_config {
	u16 manufacturer_id;
	u8 mode;
	u8 num_lanes;

	u16 width;
	u16 height;

	u16 physical_width_dim;
	u16 physical_length_dim;

	u8 framerate;
	u8 bpp;
	u16 reserved0;

	u64 clockrate;

	u16 t_clk_pre;
	u16 t_clk_post;

	u8 continuous_clock;
	u8 eot_mode;
	u8 vsync_mode;
	/*1 :non_burst_sync_pulse 2: non_burst_sync_event 3: burst_mode */
	u8 traffic_mode;

	u8 virtual_channel_id; /* 0: default */
	/* 1: rgb_swap_rgb 2: rgb_swap_rbg 3: rgb_swap_brg 4: rgb_swap_grb
	   5: rgb_swap_gbr */
	u8 color_order;
	u8 pixel_packing;
	u8 reserved1;

	u16 horizontal_front_porch;
	u16 horizontal_pulse_width;
	u16 horizontal_sync_skew;
	u16 horizontal_back_porch;
	u16 horizontal_left_border;
	u16 horizontal_right_border;

	u16 vertical_front_porch;
	u16 vertical_pulse_width;
	u16 vertical_back_porch;
	u16 vertical_top_border;
	u16 vertical_bottom_border;
	u16 reserved2;
};

struct mod_display_downstream_config {
	u32 max_link_bandwidth_khz;
};

enum mod_display_state {
	MOD_DISPLAY_OFF,
	MOD_DISPLAY_ON,
};

#ifdef CONFIG_MOD_DISPLAY

int mod_display_get_display_config(
	struct mod_display_panel_config **display_config);
int mod_display_set_display_config(u8 index);
int mod_display_get_display_state(u8 *state);
int mod_display_set_display_state(u8 state);

#else

static inline int mod_display_get_display_config(
	struct mod_display_panel_config **display_config)
	{ return 0; };
static inline int mod_display_set_display_config(u8 index)
	{ return 0; };
static inline int mod_display_get_display_state(u8 *state)
	{ return 0; };
static inline int mod_display_set_display_state(u8 state)
	{ return 0; };

#endif /* CONFIG_MOD_DISPLAY */

#endif /* __KERNEL__ */

#endif /* _MOD_DISPLAY_H_ */
