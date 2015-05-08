/*
 * Platform data for MAX98925
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98925_PDATA_H__
#define __SOUND_MAX98925_PDATA_H__

#define MAX98925L_I2C_ADDRESS	(0x62 >> 1)
#define MAX98925R_I2C_ADDRESS	(0x64 >> 1)

struct max98925_dsp_cfg {
	const char *name;
	u8 tx_dither_en;
	u8 rx_dither_en;
	u8 meas_dc_block_en;
	u8 rx_flt_mode;
};

/* codec platform data */
struct max98925_pdata {
	int irq;
	struct max98925_dsp_cfg *dsp_cfg;
	u8 clk_monitor_en;
	u8 rx_ch_en;
	u8 tx_ch_en;
	u8 tx_hiz_ch_en;
	u8 tx_ch_src;
	u8 auth_en;
	u8 wdog_time_out;
};

#endif
