/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
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

#ifndef SLIMPORT_DEVICE
#define SLIMPORT_DEVICE
#include <linux/pinctrl/consumer.h>

#define ANX_PINCTRL_STATE_DEFAULT "anx_default"
#define ANX_PINCTRL_STATE_SLEEP  "anx_sleep"


struct anx7816_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};


struct anx7816_platform_data {
	bool check_slimport_connection;
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	int gpio_v10_ctrl;
	int gpio_v33_ctrl;
	struct anx7816_pinctrl_res pin_res;
	spinlock_t lock;
	int external_ldo_control;
	int (*avdd_power)(unsigned int onoff);
	struct regulator *avdd_33;
	struct regulator *dvdd_10;
	struct regulator *vdd_18;
	struct clk *mclk;
	struct pinctrl *pinctrl;
	struct pinctrl_state *hdmi_pinctrl_active;
	struct pinctrl_state *hdmi_pinctrl_suspend;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct platform_device *hdmi_pdev;
#endif

};

unchar sp_get_rx_bw(void);
int slimport_read_edid_block(int block, uint8_t *edid_buf);
void anx7816_hpd_cb(bool connected);

#endif /* SLIMPORT_DEVICE */
