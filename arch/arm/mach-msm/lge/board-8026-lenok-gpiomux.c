/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2014, LGE Inc.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <soc/qcom/socinfo.h>
#include <mach/board_lge.h>

static struct gpiomux_setting gpio_keys_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpio_keys_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_console_uart_tx_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_console_uart_rx_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_console_uart_disabled = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_console_uart_configs[] __initdata = {
	{
		.gpio      = 8,         /* BLSP1 QUP3 UART_TX */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_tx_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_tx_cfg,
		},
	},
	{
		.gpio      = 9,         /* BLSP1 QUP3 UART_RX */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_rx_cfg,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_rx_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_console_uart_disabled_configs[] __initdata = {
	{
		.gpio      = 8,         /* GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_disabled,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_disabled,
		},
	},
	{
		.gpio      = 9,         /* GPIO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_console_uart_disabled,
			[GPIOMUX_SUSPENDED] = &gpio_console_uart_disabled,
		},
	},
};

static struct msm_gpiomux_config msm_keypad_configs[] __initdata = {
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
	{
		.gpio = 107,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_keys_active,
			[GPIOMUX_SUSPENDED] = &gpio_keys_suspend,
		},
	},
};

static struct gpiomux_setting lcd_rst_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting lcd_rst_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_rst_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_rst_sus_cfg,
		},
	},
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 10,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 11,
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 18,		/* BLSP1 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 19,		/* BLSP1 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
};

static struct gpiomux_setting touch_gpio_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting touch_gpio_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static struct gpiomux_setting touch_gpio_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir  = GPIOMUX_IN,
};

static struct gpiomux_setting touch_gpio_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir  = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_touch_configs[] __initdata = {
	{
		.gpio = 16,           /* TOUCH_RESET */
		.settings = {
			[GPIOMUX_ACTIVE] = &touch_gpio_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_gpio_reset_sus_cfg,
		},
	},
	{
		.gpio = 17,           /* TOUCH_INT */
		.settings = {
			[GPIOMUX_ACTIVE] = &touch_gpio_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &touch_gpio_int_sus_cfg,
		},
	},
};

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct gpiomux_setting sdc3_clk_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting sdc3_cmd_data_0_3_actv_cfg = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting sdc3_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting sdc3_data_1_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm8226_sdc3_configs[] __initdata = {
	{
		/* DAT3 */
		.gpio      = 39,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT2 */
		.gpio      = 40,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* DAT1 */
		.gpio      = 41,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_data_1_suspend_cfg,
		},
	},
	{
		/* DAT0 */
		.gpio      = 42,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CMD */
		.gpio      = 43,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_cmd_data_0_3_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
	{
		/* CLK */
		.gpio      = 44,
		.settings = {
			[GPIOMUX_ACTIVE]    = &sdc3_clk_actv_cfg,
			[GPIOMUX_SUSPENDED] = &sdc3_suspend_cfg,
		},
	},
};

static void msm_gpiomux_sdc3_install(void)
{
	msm_gpiomux_install(msm8226_sdc3_configs,
			    ARRAY_SIZE(msm8226_sdc3_configs));
}
#else
static void msm_gpiomux_sdc3_install(void) {}
#endif /* CONFIG_MMC_MSM_SDC3_SUPPORT */

static struct gpiomux_setting fuel_gauge_i2c_sda_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting fuel_gauge_i2c_scl_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting fuel_gauge_int_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_fuel_gauge_configs[] __initdata = {
	{
		.gpio      = 2,			/* BLSP1 QUP1 I2C_DAT */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_i2c_sda_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_i2c_sda_config,
		},
	},
	{
		.gpio      = 3,			/* BLSP1 QUP1 I2C_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_i2c_scl_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_i2c_scl_config,
		},
	},
	{
		.gpio      = 31,		/* FUEL_GAUGE_INT_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_int_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_int_config,
		},
	},
};

static struct gpiomux_setting fuel_gauge_gpio_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct msm_gpiomux_config msm_fuel_gauge_configs_evb2[] __initdata = {
	{
		.gpio      = 2,			/* i2c-gpio */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_gpio_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_gpio_config,
		},
	},
	{
		.gpio      = 3,			/* i2c-gpio */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_gpio_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_gpio_config,
		},
	},
	{
		.gpio      = 31,		/* FUEL_GAUGE_INT_N */
		.settings = {
			[GPIOMUX_ACTIVE]    = &fuel_gauge_int_config,
			[GPIOMUX_SUSPENDED] = &fuel_gauge_int_config,
		},
	},
};

static struct gpiomux_setting bt_gpio_uart_active_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE, /* Should be PULL NONE */
};

static struct gpiomux_setting bt_gpio_uart_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE, /* PULL Configuration */
};

static struct gpiomux_setting bt_rfkill_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_rfkill_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv  = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_host_wakeup_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_host_wakeup_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting bt_wakeup_active_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_wakeup_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_pcm_active_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting bt_pcm_suspend_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config bt_msm_blsp_configs[] __initdata = {
	{
		.gpio = 12, /* BLSP1 UART4 TX */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 13, /* BLSP1 UART4 RX */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 14, /* BLSP1 UART4 CTS */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
	{
		.gpio = 15, /* BLSP1 UART4 RFR */
		.settings = {
			[GPIOMUX_ACTIVE] = &bt_gpio_uart_active_config ,
			[GPIOMUX_SUSPENDED] = &bt_gpio_uart_suspend_config ,
		},
	},
};

static struct msm_gpiomux_config bt_rfkill_configs[] = {
	{
		.gpio = 45,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_rfkill_active_config,
			[GPIOMUX_SUSPENDED] = &bt_rfkill_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_wakeup_configs[] __initdata = {
	{
		.gpio = 47,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_wakeup_active_config,
			[GPIOMUX_SUSPENDED] = &bt_wakeup_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_host_wakeup_configs[] __initdata = {
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_ACTIVE]    = &bt_host_wakeup_active_config,
			[GPIOMUX_SUSPENDED] = &bt_host_wakeup_suspend_config,
		},
	},
};

static struct msm_gpiomux_config bt_pcm_configs[] __initdata = {
	{
		.gpio = 49,	/* BT_PCM_CLK */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio = 50,	/* BT_PCM_SYNC */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio = 51,	/* BT_PCM_DIN */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	},
	{
		.gpio = 52,	/* BT_PCM_DOUT */
		.settings = {
			[GPIOMUX_ACTIVE]	= &bt_pcm_active_config,
			[GPIOMUX_SUSPENDED] = &bt_pcm_suspend_config,
		},
	}
};

#if defined(CONFIG_MSM_PWM_VIBRATOR)
static struct gpiomux_setting vibrator_suspend_cfg = {
       .func = GPIOMUX_FUNC_GPIO,
       .drv = GPIOMUX_DRV_2MA,
       .pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting vibrator_active_cfg_gpio33 = {
       .func = GPIOMUX_FUNC_3,
       .drv = GPIOMUX_DRV_2MA,
       .pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting vibrator_active_cfg_gpio62 = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config vibrator_pwm_config[] = {
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio33,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};

static struct msm_gpiomux_config vibrator_configs[] = {
	{
		.gpio = 62,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio62,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};
#endif

static void bluetooth_msm_gpiomux_install(void)
{
	/* UART */
	msm_gpiomux_install(bt_msm_blsp_configs,
			ARRAY_SIZE(bt_msm_blsp_configs));
	/* RFKILL */
	msm_gpiomux_install(bt_rfkill_configs, ARRAY_SIZE(bt_rfkill_configs));
	/* HOST WAKE-UP */
	msm_gpiomux_install(bt_host_wakeup_configs,
			ARRAY_SIZE(bt_host_wakeup_configs));
	/* BT WAKE-UP */
	msm_gpiomux_install(bt_wakeup_configs, ARRAY_SIZE(bt_wakeup_configs));
	/* PCM I/F */
	msm_gpiomux_install(bt_pcm_configs, ARRAY_SIZE(bt_pcm_configs));
}

void __init msm8226_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	if (lge_uart_console_enabled())
		msm_gpiomux_install(msm_console_uart_configs,
				ARRAY_SIZE(msm_console_uart_configs));
	else
		msm_gpiomux_install(msm_console_uart_disabled_configs,
				ARRAY_SIZE(msm_console_uart_disabled_configs));

	msm_gpiomux_install(msm_keypad_configs,
			ARRAY_SIZE(msm_keypad_configs));

	msm_gpiomux_install(msm_blsp_configs,
			ARRAY_SIZE(msm_blsp_configs));

	msm_gpiomux_install(msm_touch_configs,
			ARRAY_SIZE(msm_touch_configs));

	msm_gpiomux_install_nowrite(msm_lcd_configs,
			ARRAY_SIZE(msm_lcd_configs));

	msm_gpiomux_sdc3_install();

	if (lge_get_board_revno() == HW_REV_EVB2) {
		msm_gpiomux_install(msm_fuel_gauge_configs_evb2,
				ARRAY_SIZE(msm_fuel_gauge_configs_evb2));
	} else {
		msm_gpiomux_install(msm_fuel_gauge_configs,
				ARRAY_SIZE(msm_fuel_gauge_configs));
	}

#if defined(CONFIG_MSM_PWM_VIBRATOR)
	msm_gpiomux_install(vibrator_configs, ARRAY_SIZE(vibrator_configs));
	msm_gpiomux_install(vibrator_pwm_config,
			ARRAY_SIZE(vibrator_pwm_config));

#endif
	bluetooth_msm_gpiomux_install();
}
