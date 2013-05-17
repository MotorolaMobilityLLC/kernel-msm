/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_JDI.h"
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/board_asustek.h>

static struct msm_panel_info pinfo;

static struct mipi_dsi_phy_ctrl dsi_mode_phy_db = {
	/* 1920*1200, RGB888, 4 Lane 60 fps video mode */
	/* regulator */
	{0x03, 0x0a, 0x04, 0x00, 0x20},
	/* timing */
	{0x66, 0x26, 0x38, 0x00, 0x3E, 0xE6, 0x1E, 0x9B,
	0x3E, 0x03, 0x04, 0xa0},
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 0xBD, 0x30, 0xCA, 0x00, 0x10, 0xf, 0x62,
	0x70, 0x88, 0x99,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x0e, 0x01, 0x00, 0x01 },
};

static int __init mipi_JDI_1080P_pt_init(void)
{
	int ret;

	lcd_type type = asustek_get_lcd_type();

	if (type != 0)
		return 0;

	pr_info("%s+\n", __func__);

	pinfo.xres = 1200;
	pinfo.yres = 1920;
	pinfo.width = 95;
	pinfo.height = 151;

	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
	pinfo.lcdc.h_back_porch = 60;
	pinfo.lcdc.h_front_porch = 48;
	pinfo.lcdc.h_pulse_width = 32;
	pinfo.lcdc.v_back_porch = 6;
	pinfo.lcdc.v_front_porch = 3;
	pinfo.lcdc.v_pulse_width = 5;
	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;
	pinfo.lcdc.xres_pad = 0;
	pinfo.lcdc.yres_pad = 0;
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	/* only for command mode */
	pinfo.type = MIPI_CMD_PANEL;
	pinfo.lcd.vsync_enable = TRUE;
	pinfo.lcd.hw_vsync_mode = TRUE;
	pinfo.lcd.v_back_porch = 6;
	pinfo.lcd.v_front_porch = 3;
	pinfo.lcd.v_pulse_width = 5;
	pinfo.lcd.refx100 = 6032; /* adjust refx100 to prevent tearing */
	pinfo.clk_rate = 1000000000;

	pinfo.mipi.mode = DSI_CMD_MODE;
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo.mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo.mipi.interleave_max = 1;
	pinfo.mipi.insert_dcs_cmd = TRUE;
	pinfo.mipi.wr_mem_continue = 0x3c;
	pinfo.mipi.wr_mem_start = 0x2c;
	pinfo.mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;

	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;
	pinfo.mipi.tx_eot_append = TRUE;
	pinfo.mipi.t_clk_post = 0x04;
	pinfo.mipi.t_clk_pre = 0x1c;
	pinfo.mipi.stream = 0;	/* dma_p */
	pinfo.mipi.esc_byte_ratio = 9;	/*control TLPX: 75ns */
	pinfo.mipi.mdp_trigger = 0;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dsi_phy_db = &dsi_mode_phy_db;
	pinfo.mipi.frame_rate = 60;

	ret = mipi_JDI_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WUXGA);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

	pr_info("%s-\n", __func__);

	return ret;
}

module_init(mipi_JDI_1080P_pt_init);
