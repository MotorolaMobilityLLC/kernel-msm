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
};

#endif /* __CS35L36_H */
