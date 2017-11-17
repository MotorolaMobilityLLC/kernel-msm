/*
 * linux/sound/cs35l36.h -- Platform data for CS35L36
 *
 * Copyright (c) 2016 Cirrus Logic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CS35L36_H
#define __CS35L36_H

struct classh_cfg {

};

/* ASP TX Platform Data */
struct asp_tx_cfg {
	bool is_present;
	bool tx1, tx2, tx3, tx4, tx5, tx6;
	u8 tx1_slot;
	u8 tx1_src_select;
	u8 tx2_slot;
	u8 tx2_src_select;
	u8 tx3_slot;
	u8 tx3_src_select;
	u8 tx4_slot;
	u8 tx4_src_select;
	u8 tx5_slot;
	u8 tx5_src_select;
	u8 tx6_slot;
	u8 tx6_src_select;
};

/* INT/GPIO pin configuration */
struct irq_cfg {
	bool is_present;
	int irq_drv_sel;
	int irq_pol;
	int irq_gpio_sel;
	int irq_out_en;
	int irq_src_sel;
};

struct cs35l36_platform_data {
	bool sclk_frc;
	bool lrclk_frc;
	bool right_channel;
	bool dcm_mode;
	int ldm_mode_sel;
	bool amp_gain_zc;
	bool pdm_ldm_exit;
	bool pdm_ldm_enter;
	int boost_ind;
	int bst_vctl;
	int bst_vctl_sel;
	int bst_ipk;
	int temp_warn_thld;
	struct irq_cfg irq_config;
	struct asp_tx_cfg asp_tx_config;
};

#endif /* __CS35L36_H */
