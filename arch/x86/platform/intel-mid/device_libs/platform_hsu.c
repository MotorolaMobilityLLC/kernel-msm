/*
 * platform_hsu.c: hsu platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <asm/setup.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_hsu.h>

#include "platform_hsu.h"

#define TNG_CLOCK_CTL 0xFF00B830
#define TNG_CLOCK_SC  0xFF00B868

#define VLV_HSU_CLOCK	0x0800
#define VLV_HSU_RESET	0x0804
#define VLV_HSU_OVF_IRQ	0x0820	/* Overflow interrupt related */

static unsigned int clock;
static struct hsu_port_pin_cfg *hsu_port_gpio_mux;
static struct hsu_port_cfg *platform_hsu_info;

static struct
hsu_port_pin_cfg hsu_port_pin_cfgs[][hsu_pid_max][hsu_port_max] = {
	[hsu_pnw] = {
		[hsu_pid_def] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.wake_gpio = 13,
				.rx_gpio = 96+26,
				.rx_alt = 1,
				.tx_gpio = 96+27,
				.tx_alt = 1,
				.cts_gpio = 96+28,
				.cts_alt = 1,
				.rts_gpio = 96+29,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_MODEM_PORT,
				.wake_gpio = 64,
				.rx_gpio = 64,
				.rx_alt = 1,
				.tx_gpio = 65,
				.tx_alt = 1,
				.cts_gpio = 68,
				.cts_alt = 1,
				.rts_gpio = 66,
				.rts_alt = 2,
			},
			[hsu_port2] = {
				.id = 2,
				.name = HSU_GPS_PORT,
			},
			[hsu_port_share] = {
				.id = 1,
				.name = HSU_DEBUG_PORT,
				.wake_gpio = 96+30,
				.rx_gpio = 96+30,
				.rx_alt = 1,
				.tx_gpio = 96+31,
				.tx_alt = 1,
			},
		},
	},
	[hsu_clv] = {
		[hsu_pid_rhb] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.wake_gpio = 42,
				.rx_gpio = 96+26,
				.rx_alt = 1,
				.tx_gpio = 96+27,
				.tx_alt = 1,
				.cts_gpio = 96+28,
				.cts_alt = 1,
				.rts_gpio = 96+29,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_MODEM_PORT,
				.wake_gpio = 64,
				.rx_gpio = 64,
				.rx_alt = 1,
				.tx_gpio = 65,
				.tx_alt = 1,
				.cts_gpio = 68,
				.cts_alt = 1,
				.rts_gpio = 66,
				.rts_alt = 2,
			},
			[hsu_port2] = {
				.id = 2,
				.name = HSU_DEBUG_PORT,
				.wake_gpio = 67,
				.rx_gpio = 67,
				.rx_alt = 1,
			},
			[hsu_port_share] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 96+30,
				.rx_gpio = 96+30,
				.rx_alt = 1,
				.tx_gpio = 96+31,
				.tx_alt = 1,
				.cts_gpio = 96+33,
				.cts_alt = 1,
				.rts_gpio = 96+32,
				.rts_alt = 2,
			},
		},
		[hsu_pid_vtb_pro] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.rx_gpio = 96+26,
				.rx_alt = 1,
				.tx_gpio = 96+27,
				.tx_alt = 1,
				.cts_gpio = 96+28,
				.cts_alt = 1,
				.rts_gpio = 96+29,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_MODEM_PORT,
				.wake_gpio = 96+30,
				.rx_gpio = 96+30,
				.rx_alt = 1,
				.tx_gpio = 96+31,
				.tx_alt = 1,
				.cts_gpio = 96+33,
				.cts_alt = 1,
				.rts_gpio = 96+32,
				.rts_alt = 2,
			},
			[hsu_port2] = {
				.id = 2,
				.name = HSU_DEBUG_PORT,
				.wake_gpio = 67,
				.rx_gpio = 67,
				.rx_alt = 1,
			},
			[hsu_port_share] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 64,
				.rx_gpio = 64,
				.rx_alt = 1,
				.tx_gpio = 65,
				.tx_alt = 1,
				.cts_gpio = 68,
				.cts_alt = 1,
				.rts_gpio = 66,
				.rts_alt = 2,
			},
		},
		[hsu_pid_vtb_eng] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.rx_gpio = 96+26,
				.rx_alt = 1,
				.tx_gpio = 96+27,
				.tx_alt = 1,
				.cts_gpio = 96+28,
				.cts_alt = 1,
				.rts_gpio = 96+29,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_MODEM_PORT,
				.wake_gpio = 64,
				.rx_gpio = 64,
				.rx_alt = 1,
				.tx_gpio = 65,
				.tx_alt = 1,
				.cts_gpio = 68,
				.cts_alt = 1,
				.rts_gpio = 66,
				.rts_alt = 2,
			},
			[hsu_port2] = {
				.id = 2,
				.name = HSU_DEBUG_PORT,
				.wake_gpio = 67,
				.rx_gpio = 67,
				.rx_alt = 1,
			},
			[hsu_port_share] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 96+30,
				.rx_gpio = 96+30,
				.rx_alt = 1,
				.tx_gpio = 96+31,
				.tx_alt = 1,
				.cts_gpio = 96+33,
				.cts_alt = 1,
				.rts_gpio = 96+32,
				.rts_alt = 2,
			},
		},
	},
	[hsu_tng] = {
		[hsu_pid_def] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.rx_gpio = 126,
				.rx_alt = 1,
				.tx_gpio = 127,
				.tx_alt = 1,
				.cts_gpio = 124,
				.cts_alt = 1,
				.rts_gpio = 125,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 130,
				.rx_gpio = 130,
				.rx_alt = 1,
				.cts_gpio = 128,
				.cts_alt = 1,
				.rts_gpio = 129,
				.rts_alt = 1,
			},
			[hsu_port2] = {
				.id = 2,
				.name = HSU_DEBUG_PORT,
				.wake_gpio = 134,
				.rx_gpio = 134,
				.rx_alt = 1,
				.cts_gpio = 132,
				.cts_alt = 1,
				.rts_gpio = 133,
				.rts_alt = 1,
			},
		},
	},
	[hsu_vlv2] = {
		[hsu_pid_def] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.rts_gpio = 72,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 74,
				.rx_gpio = 74,
				.rx_alt = 1,
				.rts_gpio = 76,
				.rts_alt = 1,
			},
		},
	},
	[hsu_chv] = {
		[hsu_pid_def] = {
			[hsu_port0] = {
				.id = 0,
				.name = HSU_BT_PORT,
				.rts_gpio = 0,
				.rts_alt = 1,
			},
			[hsu_port1] = {
				.id = 1,
				.name = HSU_GPS_PORT,
				.wake_gpio = 0,
				.wake_src = hsu_rxd,
				.rx_gpio = 0,
				.rx_alt = 1,
				.rts_gpio = 0,
				.rts_alt = 1,
			},
		},
	},

};

static struct hsu_port_cfg hsu_port_cfgs[][hsu_port_max] = {
	[hsu_pnw] = {
		[hsu_port0] = {
			.type = bt_port,
			.hw_ip = hsu_intel,
			.index = 0,
			.name = HSU_BT_PORT,
			.idle = 20,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
		},
		[hsu_port1] = {
			.type = modem_port,
			.hw_ip = hsu_intel,
			.index = 1,
			.name = HSU_MODEM_PORT,
			.idle = 100,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.has_alt = 1,
			.alt = hsu_port_share,
			.force_suspend = 0,
		},
		[hsu_port2] = {
			.type = gps_port,
			.hw_ip = hsu_intel,
			.index = 2,
			.name = HSU_GPS_PORT,
			.idle = 40,
			.preamble = 1,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_suspend_post = intel_mid_hsu_suspend_post,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
		},
		[hsu_port_share] = {
			.type = debug_port,
			.hw_ip = hsu_intel,
			.index = 3,
			.name = HSU_DEBUG_PORT,
			.idle = 2000,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.has_alt = 1,
			.alt = hsu_port1,
			.force_suspend = 1,
		},
	},
	[hsu_clv] = {
		[hsu_port0] = {
			.type = bt_port,
			.hw_ip = hsu_intel,
			.index = 0,
			.name = HSU_BT_PORT,
			.idle = 20,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
		},
		[hsu_port1] = {
			.type = modem_port,
			.hw_ip = hsu_intel,
			.index = 1,
			.name = HSU_MODEM_PORT,
			.idle = 100,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.has_alt = 1,
			.alt = hsu_port_share,
			.force_suspend = 0,
		},
		[hsu_port2] = {
			.type = debug_port,
			.hw_ip = hsu_intel,
			.index = 2,
			.name = HSU_DEBUG_PORT,
			.idle = 2000,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
		},
		[hsu_port_share] = {
			.type = gps_port,
			.hw_ip = hsu_intel,
			.index = 3,
			.name = HSU_GPS_PORT,
			.idle = 40,
			.preamble = 1,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_suspend_post = intel_mid_hsu_suspend_post,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.has_alt = 1,
			.alt = hsu_port1,
			.force_suspend = 1,
		},
	},
	[hsu_tng] = {
		[hsu_port0] = {
			.type = bt_port,
			.hw_ip = hsu_intel,
			.index = 0,
			.name = HSU_BT_PORT,
			.idle = 20,
			.hw_ctrl_cts = 1,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.hw_context_save = 1,
		},
		[hsu_port1] = {
			.type = gps_port,
			.hw_ip = hsu_intel,
			.index = 1,
			.name = HSU_GPS_PORT,
			.idle = 40,
			.preamble = 1,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_suspend_post = intel_mid_hsu_suspend_post,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.hw_context_save = 1,
		},
		[hsu_port2] = {
			.type = debug_port,
			.hw_ip = hsu_intel,
			.index = 2,
			.name = HSU_DEBUG_PORT,
			.idle = 2000,
			.hw_init = intel_mid_hsu_init,
			.hw_set_alt = intel_mid_hsu_switch,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_get_clk = intel_mid_hsu_get_clk,
			.hw_context_save = 1,
		},
	},
	[hsu_vlv2] = {
		[hsu_port0] = {
			.type = bt_port,
			.hw_ip = hsu_dw,
			.index = 0,
			.name = HSU_BT_PORT,
			.idle = 100,
			.hw_reset = intel_mid_hsu_reset,
			.set_clk = intel_mid_hsu_set_clk,
			.hw_ctrl_cts = 1,
			.hw_init = intel_mid_hsu_init,
			/* Trust FW has set it correctly */
			.hw_set_alt = NULL,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_context_save = 1,
		},
		[hsu_port1] = {
			.type = gps_port,
			.hw_ip = hsu_dw,
			.index = 1,
			.name = HSU_GPS_PORT,
			.idle = 40,
			.preamble = 1,
			.hw_reset = intel_mid_hsu_reset,
			.set_clk = intel_mid_hsu_set_clk,
			.hw_ctrl_cts = 1,
			.hw_init = intel_mid_hsu_init,
			/* Trust FW has set it correctly */
			.hw_set_alt = NULL,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_suspend_post = intel_mid_hsu_suspend_post,
			.hw_resume = intel_mid_hsu_resume,
			.hw_context_save = 1,
		},
	},
	[hsu_chv] = {
		[hsu_port0] = {
			.type = bt_port,
			.hw_ip = hsu_dw,
			.index = 0,
			.name = HSU_BT_PORT,
			.idle = 100,
			.hw_reset = intel_mid_hsu_reset,
			.set_clk = intel_mid_hsu_set_clk,
			.hw_ctrl_cts = 1,
			.hw_init = intel_mid_hsu_init,
			/* Trust FW has set it correctly */
			.hw_set_alt = NULL,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_resume = intel_mid_hsu_resume,
			.hw_context_save = 1,
		},
		[hsu_port1] = {
			.type = gps_port,
			.hw_ip = hsu_dw,
			.index = 1,
			.name = HSU_GPS_PORT,
			.idle = 40,
			.preamble = 1,
			.hw_reset = intel_mid_hsu_reset,
			.set_clk = intel_mid_hsu_set_clk,
			.hw_ctrl_cts = 1,
			.hw_init = intel_mid_hsu_init,
			/* Trust FW has set it correctly */
			.hw_set_alt = NULL,
			.hw_set_rts = intel_mid_hsu_rts,
			.hw_suspend = intel_mid_hsu_suspend,
			.hw_suspend_post = intel_mid_hsu_suspend_post,
			.hw_resume = intel_mid_hsu_resume,
			.hw_context_save = 1,
		},
	},

};

static struct hsu_func2port hsu_port_func_id_tlb[][hsu_port_func_max] = {
	[hsu_pnw] = {
		[0] = {
			.func = 0,
			.port = hsu_port0,
		},
		[1] = {
			.func = 1,
			.port = hsu_port1,
		},
		[2] = {
			.func = 2,
			.port = hsu_port2,
		},
		[3] = {
			.func = -1,
			.port = -1,
		},
	},
	[hsu_clv] = {
		[0] = {
			.func = 0,
			.port = hsu_port0,
		},
		[1] = {
			.func = 1,
			.port = hsu_port1,
		},
		[2] = {
			.func = 2,
			.port = hsu_port2,
		},
		[3] = {
			.func = -1,
			.port = -1,
		},
	},
	[hsu_tng] = {
		[0] = {
			.func = 0,
			.port = -1,
		},
		[1] = {
			.func = 1,
			.port = hsu_port0,
		},
		[2] = {
			.func = 2,
			.port = hsu_port1,
		},
		[3] = {
			.func = 3,
			.port = hsu_port2,
		},
	},
	[hsu_vlv2] = {
		[0] = {
			.func = 3,
			.port = hsu_port0,
		},
		[1] = {
			.func = 4,
			.port = hsu_port1,
		},
		[2] = {
			.func = -1,
			.port = -1,
		},
		[3] = {
			.func = -1,
			.port = -1,
		},
	},
};

static void hsu_port_enable(int port)
{
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	if (info->rx_gpio) {
		lnw_gpio_set_alt(info->rx_gpio, info->rx_alt);
		gpio_direction_input(info->rx_gpio);
	}
	if (info->tx_gpio) {
		gpio_direction_output(info->tx_gpio, 1);
		lnw_gpio_set_alt(info->tx_gpio, info->tx_alt);
		usleep_range(10, 10);
		gpio_direction_output(info->tx_gpio, 0);

	}
	if (info->cts_gpio) {
		lnw_gpio_set_alt(info->cts_gpio, info->cts_alt);
		gpio_direction_input(info->cts_gpio);
	}
	if (info->rts_gpio) {
		gpio_direction_output(info->rts_gpio, 0);
		lnw_gpio_set_alt(info->rts_gpio, info->rts_alt);
	}
}

static void hsu_port_disable(int port)
{
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	if (info->rx_gpio) {
		lnw_gpio_set_alt(info->rx_gpio, LNW_GPIO);
		gpio_direction_input(info->rx_gpio);
	}
	if (info->tx_gpio) {
		gpio_direction_output(info->tx_gpio, 1);
		lnw_gpio_set_alt(info->tx_gpio, LNW_GPIO);
		usleep_range(10, 10);
		gpio_direction_input(info->tx_gpio);
	}
	if (info->cts_gpio) {
		lnw_gpio_set_alt(info->cts_gpio, LNW_GPIO);
		gpio_direction_input(info->cts_gpio);
	}
	if (info->rts_gpio) {
		lnw_gpio_set_alt(info->rts_gpio, LNW_GPIO);
		gpio_direction_input(info->rts_gpio);
	}
}

void intel_mid_hsu_suspend(int port, struct device *dev, irq_handler_t wake_isr)
{
	int ret;
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	info->dev = dev;
	info->wake_isr = wake_isr;

	if (info->wake_gpio) {
		lnw_gpio_set_alt(info->wake_gpio, LNW_GPIO);
		gpio_direction_input(info->wake_gpio);
		udelay(10);
		ret = request_irq(gpio_to_irq(info->wake_gpio), info->wake_isr,
				IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING,
				info->name, info->dev);
		if (ret)
			dev_err(info->dev, "failed to register wakeup irq\n");
	}
}

void intel_mid_hsu_resume(int port, struct device *dev)
{
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	if (info->wake_gpio)
		free_irq(gpio_to_irq(info->wake_gpio), info->dev);

	if (info->rx_gpio) {
		lnw_gpio_set_alt(info->rx_gpio, info->rx_alt);
		gpio_direction_input(info->rx_gpio);
	}
	if (info->tx_gpio) {
		gpio_direction_output(info->tx_gpio, 1);
		lnw_gpio_set_alt(info->tx_gpio, info->tx_alt);
		usleep_range(10, 10);
		gpio_direction_output(info->tx_gpio, 0);

	}
	if (info->cts_gpio) {
		lnw_gpio_set_alt(info->cts_gpio, info->cts_alt);
		gpio_direction_input(info->cts_gpio);
	}
}

void intel_mid_hsu_switch(int port)
{
	int i;
	struct hsu_port_pin_cfg *tmp;
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	for (i = 0; i < hsu_port_max; i++) {
		tmp = hsu_port_gpio_mux + i;
		if (tmp != info && tmp->id == info->id)
			hsu_port_disable(i);
	}
	hsu_port_enable(port);
}

void intel_mid_hsu_rts(int port, int value)
{
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	if (!info->rts_gpio)
		return;

	if (value) {
		gpio_direction_output(info->rts_gpio, 1);
		lnw_gpio_set_alt(info->rts_gpio, LNW_GPIO);
	} else
		lnw_gpio_set_alt(info->rts_gpio, info->rts_alt);
}

void intel_mid_hsu_suspend_post(int port)
{
	struct hsu_port_pin_cfg *info = hsu_port_gpio_mux + port;

	if (info->rts_gpio && info->wake_gpio
		&& info->wake_gpio == info->rx_gpio) {
		gpio_direction_output(info->rts_gpio, 0);
		lnw_gpio_set_alt(info->rts_gpio, LNW_GPIO);
	}
}

void intel_mid_hsu_set_clk(unsigned int m, unsigned int n,
				void __iomem *addr)
{
	unsigned int param, update_bit;

	switch (boot_cpu_data.x86_model) {
	/* valleyview*/
	case 0x37:
	/* cherryview */
	case 0x4C:
		update_bit = 1 << 31;
		param = (m << 1) | (n << 16) | 0x1;

		writel(param, addr + VLV_HSU_CLOCK);
		writel((param | update_bit), addr + VLV_HSU_CLOCK);
		writel(param, addr + VLV_HSU_CLOCK);
		break;
	default:
		break;
	}
}

void intel_mid_hsu_reset(void __iomem *addr)
{
	switch (boot_cpu_data.x86_model) {
	/* valleyview*/
	case 0x37:
	/* cherryview */
	case 0x4C:
		writel(0, addr + VLV_HSU_RESET);
		writel(3, addr + VLV_HSU_RESET);
		/* Disable the tx overflow IRQ */
		writel(2, addr + VLV_HSU_OVF_IRQ);
		break;
	default:
		break;
	}
}

unsigned int intel_mid_hsu_get_clk(void)
{
	return clock;
}

int intel_mid_hsu_func_to_port(unsigned int func)
{
	int i;
	struct hsu_func2port *tbl = NULL;

	switch (boot_cpu_data.x86_model) {
	/* penwell */
	case 0x27:
		tbl = &hsu_port_func_id_tlb[hsu_pnw][0];
		break;
	/* cloverview */
	case 0x35:
		tbl = &hsu_port_func_id_tlb[hsu_clv][0];
		break;
	/* tangier */
	case 0x3C:
	case 0x4A:
		tbl = &hsu_port_func_id_tlb[hsu_tng][0];
		break;
	/* valleyview*/
	case 0x37:
		tbl = &hsu_port_func_id_tlb[hsu_vlv2][0];
		break;
	/* anniedale */
	case 0x5A:
		/* anniedale same config as tangier */
		tbl = &hsu_port_func_id_tlb[hsu_tng][0];
		break;
	/* cherryview */
	case 0x4C:
	default:
		return -1;
	}

	for (i = 0; i < hsu_port_func_max; i++) {
		if (tbl->func == func)
			return tbl->port;
		tbl++;
	}

	return -1;
}

int intel_mid_hsu_init(struct device *dev, int port)
{
	struct hsu_port_cfg *port_cfg = platform_hsu_info + port;
	struct hsu_port_pin_cfg *info;

	if (port >= hsu_port_max)
		return -ENODEV;

	port_cfg->dev = dev;

	info = hsu_port_gpio_mux + port;
	if (info->wake_gpio)
		gpio_request(info->wake_gpio, "hsu");
	if (info->rx_gpio)
		gpio_request(info->rx_gpio, "hsu");
	if (info->tx_gpio)
		gpio_request(info->tx_gpio, "hsu");
	if (info->cts_gpio)
		gpio_request(info->cts_gpio, "hsu");
	if (info->rts_gpio)
		gpio_request(info->rts_gpio, "hsu");

	return 1;
}

static void hsu_platform_clk(enum intel_mid_cpu_type cpu_type, ulong plat)
{
	void __iomem *clkctl, *clksc;
	u32 clk_src, clk_div;

	switch (boot_cpu_data.x86_model) {
	/* penwell */
	case 0x27:
	/* cloverview */
	case 0x35:
		clock = 50000;
		break;
	/* tangier */
	case 0x3C:
	case 0x4A:
	/* anniedale */
	case 0x5A:
		clock = 100000;
		clkctl = ioremap_nocache(TNG_CLOCK_CTL, 4);
		if (!clkctl) {
			pr_err("tng scu clk ctl ioremap error\n");
			break;
		}

		clksc = ioremap_nocache(TNG_CLOCK_SC, 4);
		if (!clksc) {
			pr_err("tng scu clk sc ioremap error\n");
			iounmap(clkctl);
			break;
		}

		clk_src = readl(clkctl);
		clk_div = readl(clksc);

		if (clk_src & (1 << 16))
			/* source SCU fabric 100M */
			clock = clock / ((clk_div & 0x7) + 1);
		else {
			if (clk_src & (1 << 31))
				/* source OSCX2 38.4M */
				clock = 38400;
			else
				/* source OSC clock 19.2M */
				clock = 19200;
		}

		iounmap(clkctl);
		iounmap(clksc);
		break;
	/* valleyview*/
	case 0x37:
	/* cherryview */
	case 0x4C:
	default:
		clock = 100000;
		break;
	}

	pr_info("hsu core clock %u M\n", clock / 1000);
}

int intel_mid_hsu_plat_init(int port, ulong plat, struct device *dev)
{
#ifdef CONFIG_ACPI
	struct acpi_gpio_info info;
	struct hsu_port_pin_cfg *pin_cfg = NULL;
	int gpio = -1;

	switch (plat) {
	case hsu_chv:
		pin_cfg = &hsu_port_pin_cfgs[plat][hsu_pid_def][port];

		if (!pin_cfg->rx_gpio) {
			gpio = acpi_get_gpio_by_index(dev, rxd_acpi_idx, &info);
			if (gpio >= 0)
				pin_cfg->rx_gpio = gpio;
		}

		if (!pin_cfg->tx_gpio) {
			gpio = acpi_get_gpio_by_index(dev, txd_acpi_idx, &info);
			if (gpio >= 0)
				pin_cfg->tx_gpio = gpio;
		}

		if (!pin_cfg->rts_gpio) {
			gpio = acpi_get_gpio_by_index(dev, rts_acpi_idx, &info);
			if (gpio >= 0)
				pin_cfg->rts_gpio = gpio;
		}

		if (!pin_cfg->cts_gpio) {
			gpio = acpi_get_gpio_by_index(dev, cts_acpi_idx, &info);
			if (gpio >= 0)
				pin_cfg->cts_gpio = gpio;
		}
		break;
	default:
		return 0;
	}

	if (pin_cfg) {
		switch (pin_cfg->wake_src) {
		case hsu_rxd:
			pin_cfg->wake_gpio = pin_cfg->rx_gpio;
			break;
		default:
			break;
		}
	}
#endif
	return 0;
}

static __init int hsu_dev_platform_data(void)
{
	switch (boot_cpu_data.x86_model) {
	/* penwell */
	case 0x27:
		platform_hsu_info = &hsu_port_cfgs[hsu_pnw][0];
		hsu_port_gpio_mux = &hsu_port_pin_cfgs[hsu_pnw][hsu_pid_def][0];
		break;
	/* cloverview */
	case 0x35:
		platform_hsu_info = &hsu_port_cfgs[hsu_clv][0];
		if (INTEL_MID_BOARD(2, PHONE, CLVTP, VB, PRO))
			hsu_port_gpio_mux =
				&hsu_port_pin_cfgs[hsu_clv][hsu_pid_vtb_pro][0];
		else if (INTEL_MID_BOARD(2, PHONE, CLVTP, VB, ENG))
			hsu_port_gpio_mux =
				&hsu_port_pin_cfgs[hsu_clv][hsu_pid_vtb_eng][0];
		else
			hsu_port_gpio_mux =
				&hsu_port_pin_cfgs[hsu_clv][hsu_pid_rhb][0];
		break;
	/* tangier */
	case 0x3C:
	case 0x4A:
		platform_hsu_info = &hsu_port_cfgs[hsu_tng][0];
		hsu_port_gpio_mux = &hsu_port_pin_cfgs[hsu_tng][hsu_pid_def][0];
		break;
	/* valleyview*/
	case 0x37:
		platform_hsu_info = &hsu_port_cfgs[hsu_vlv2][0];
		hsu_port_gpio_mux =
			&hsu_port_pin_cfgs[hsu_vlv2][hsu_pid_def][0];
		break;
	/* anniedale */
	case 0x5A:
		/* anniedale same config as tangier */
		platform_hsu_info = &hsu_port_cfgs[hsu_tng][0];
		hsu_port_gpio_mux = &hsu_port_pin_cfgs[hsu_tng][hsu_pid_def][0];
		break;
	/* cherryview */
	case 0x4C:
		platform_hsu_info = &hsu_port_cfgs[hsu_chv][0];
		hsu_port_gpio_mux =
			&hsu_port_pin_cfgs[hsu_chv][hsu_pid_def][0];
		break;
	default:
		pr_err("HSU: cpu%x no platform config!\n", boot_cpu_data.x86_model);
		return -ENODEV;
	}

	if (platform_hsu_info == NULL)
		return -ENODEV;

	if (hsu_port_gpio_mux == NULL)
		return -ENODEV;

	hsu_register_board_info(platform_hsu_info);
	hsu_platform_clk(intel_mid_identify_cpu(), 0);

	return 0;
}

fs_initcall(hsu_dev_platform_data);
