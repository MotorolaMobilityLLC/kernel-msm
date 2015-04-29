/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define KS8851_IRQ_GPIO 115

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
static struct gpiomux_setting gpio_eth_config = {
	.pull = GPIOMUX_PULL_UP,
	.drv = GPIOMUX_DRV_2MA,
	.func = GPIOMUX_FUNC_GPIO,
};

#endif

static struct gpiomux_setting synaptics_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting synaptics_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting synaptics_reset_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting synaptics_reset_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting gpio_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
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

static struct gpiomux_setting lcd_te_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct gpiomux_setting lcd_te_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static struct msm_gpiomux_config msm_lcd_configs[] __initdata = {
	{
		.gpio = 25,		/* LCD Reset */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_rst_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_rst_sus_cfg,
		},
	}
};

static struct msm_gpiomux_config msm_lcd_te_configs[] __initdata = {
	{
		.gpio = 24,		/* Tear Enable */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_te_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_te_sus_cfg,
		},
	}
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
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

static struct msm_gpiomux_config msm_synaptics_configs[] __initdata = {
	{
		.gpio = 16,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_reset_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_reset_sus_cfg,
		},
	},
	{
		.gpio = 17,
		.settings = {
			[GPIOMUX_ACTIVE] = &synaptics_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &synaptics_int_sus_cfg,
		},
	},
};

static struct gpiomux_setting gpio_uart_config =
{
	.func = GPIOMUX_FUNC_2, //Please look @ GPIO function table for correct function
	.drv = GPIOMUX_DRV_8MA, //Drive Strength
	.pull = GPIOMUX_PULL_NONE, //Should be PULL NONE for UART
};

/*added gpio define and uart for MCU*/
static struct msm_gpiomux_config msm_uart1_configs[] __initdata =
{
	{
		.gpio = 0, //BLSP1 UART1 TX
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 1, //BLSP1 UART1 RX
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 2, //BLSP1 UART1 RTS
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 3, //BLSP1 UART1 CTS
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

static struct gpiomux_setting gpio_reset_config =
{
	.func = GPIOMUX_FUNC_GPIO, //Please look @ GPIO function table for correct function
	.drv = GPIOMUX_DRV_2MA, //Drive Strength
	.pull = GPIOMUX_PULL_UP, //Should be PULL_UP for UART
};

/*added gpio define and uart for MCU*/
static struct msm_gpiomux_config msm_mcureset_config[] __initdata =
{
	{
		.gpio = 87,/*mcu reset*/
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_reset_config,
			[GPIOMUX_SUSPENDED] = &gpio_reset_config,
		},
	},
};

/*gpio to indicate the sleep and wakeup status between mcu and AP*/
static struct gpiomux_setting gpio_low_power_status_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_low_power_status_config[] __initdata =
{
	{
		.gpio = 108, /*AP status*/
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_low_power_status_config,
			[GPIOMUX_SUSPENDED] = &gpio_low_power_status_config,
		},
	},
	{
		.gpio = 114, /*mcu status*/
		.settings =
		{
			[GPIOMUX_ACTIVE] = &gpio_low_power_status_config,
			[GPIOMUX_SUSPENDED] = &gpio_low_power_status_config,
		},
	},
};

/*uart for BT*/
static struct msm_gpiomux_config msm_uart3_configs[] __initdata =
{
	{
		.gpio = 12, //BLSP1 UART3 RX
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 13, //BLSP1 UART3 TX
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 14, //BLSP1 UART3 RTS
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio = 15, //BLSP1 UART3 CTS
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_uart_config,
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

/*speaker gpio*/
static struct gpiomux_setting gpio_speaker_i2c_act_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct gpiomux_setting gpio_speaker_i2c_sus_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};


static struct msm_gpiomux_config msm_speaker_configs[] __initdata =
{
	{
		.gpio      = 10,		/* i2c */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_speaker_i2c_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_speaker_i2c_sus_config,
		},
	},
	{
		.gpio      = 11,		/* i2c */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_speaker_i2c_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_speaker_i2c_sus_config,
		},
	},
};
/*motor gpio*/
static struct msm_gpiomux_config msm_motor_configs[] __initdata =
{
	{
		.gpio      = 6,		/* i2c */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
	{
		.gpio      = 7,		/* i2c */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config,
		},
	},
};

static struct gpiomux_setting gpio_motor_ctrl_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_motor_ctrl_configs[] __initdata =
{
	{
		.gpio      = 59,		/* trig*/
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_motor_ctrl_config,
			[GPIOMUX_SUSPENDED] = &gpio_motor_ctrl_config,
		},
	},
	{
		.gpio      = 60,		/* enable */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_motor_ctrl_config,
			[GPIOMUX_SUSPENDED] = &gpio_motor_ctrl_config,
		},
	},
};

static struct gpiomux_setting  tert_mi2s_act_cfg =
{
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting  tert_mi2s_sus_cfg =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8226_tert_mi2s_configs[] __initdata =
{
	{
		.gpio   = 49,       /* qua mi2s sck */
		.settings =
		{
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio   = 50,
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio = 51,
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio = 52,
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
};
/*mic I2S*/
static struct gpiomux_setting  quat_mi2s_act_cfg =
{
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting  quat_mi2s_sus_cfg =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config msm8226_quat_mi2s_configs[] __initdata =
{
	{
		.gpio   = 46,       /* qua mi2s sck */
		.settings =
		{
			[GPIOMUX_SUSPENDED] = &quat_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &quat_mi2s_act_cfg,
		},
	},
	{
		.gpio   = 47,
		.settings = {
			[GPIOMUX_SUSPENDED] = &quat_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &quat_mi2s_act_cfg,
		},
	},
	{
		.gpio = 48,
		.settings = {
			[GPIOMUX_SUSPENDED] = &quat_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &quat_mi2s_act_cfg,
		},
	}
};

/*force download*/
static struct gpiomux_setting  gpio_force_dload_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_UP,
};
static struct msm_gpiomux_config msm_force_dload_configs[] __initdata =
{
	{
		.gpio      = 58,		/* force dload */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_force_dload_config,
			[GPIOMUX_SUSPENDED] = &gpio_force_dload_config,
		},
	},
};

/*mcu boot*/
static struct gpiomux_setting  gpio_mcu_boot_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config msm_mcu_boot_configs[] __initdata =
{
	{
		.gpio      = 106,		/* boot1 */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_mcu_boot_config,
			[GPIOMUX_SUSPENDED] = &gpio_mcu_boot_config,
		},
	},
	{
		.gpio      = 111,		/* boot0 */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_mcu_boot_config,
			[GPIOMUX_SUSPENDED] = &gpio_mcu_boot_config,
		},
	},
};

/*wifi reg on*/
static struct gpiomux_setting  gpio_wifi_regon_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config msm_wifi_regon_configs[] __initdata =
{
	{
		.gpio      = 110,		/* wifi regon */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_wifi_regon_config,
			[GPIOMUX_SUSPENDED] = &gpio_wifi_regon_config,
		},
	},
};

/*LCD ID*/
static struct gpiomux_setting  gpio_lcd_id_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};
static struct msm_gpiomux_config msm_lcd_id_configs[] __initdata =
{
	{
		.gpio      = 37,		/* id1 */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_lcd_id_config,
			[GPIOMUX_SUSPENDED] = &gpio_lcd_id_config,
		},
	},
	{
		.gpio      = 38,		/* id0 */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_lcd_id_config,
			[GPIOMUX_SUSPENDED] = &gpio_lcd_id_config,
		},
	},
};

/*interrupt gpio to wake up AP*/
static struct gpiomux_setting  gpio_wakeup_ap_irq_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_IN,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_wakeup_ap_irq_configs[] __initdata =
{
	{
		.gpio      = 64,		/* BT wakeup AP */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_wakeup_ap_irq_config,
			[GPIOMUX_SUSPENDED] = &gpio_wakeup_ap_irq_config,
		},
	},
	{
		.gpio      = 65,		/* MCU wakeup AP */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_wakeup_ap_irq_config,
			[GPIOMUX_SUSPENDED] = &gpio_wakeup_ap_irq_config,
		},
	},
	{
		.gpio      = 66,		/* WLAN wakeup AP */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_wakeup_ap_irq_config,
			[GPIOMUX_SUSPENDED] = &gpio_wakeup_ap_irq_config,
		},
	},
};

/*interrupt gpio to wake up peripheral*/
static struct gpiomux_setting gpio_wakeup_peripheral_irq_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.dir = GPIOMUX_OUT_LOW,/*default need set LOW*/
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_wakeup_peripheral_irq_configs[] __initdata =
{
	{
		.gpio	= 62,		/* AP wakeup MCU*/
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_wakeup_peripheral_irq_config,
			[GPIOMUX_ACTIVE] = &gpio_wakeup_peripheral_irq_config,
		},
	},
	{
		.gpio	= 63,		/* AP wakeup BT*/
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_wakeup_peripheral_irq_config,
			[GPIOMUX_ACTIVE] = &gpio_wakeup_peripheral_irq_config,
		},
	},
};

/*BT reg on*/
static struct gpiomux_setting  gpio_bt_regon_config =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct msm_gpiomux_config msm_bt_regon_configs[] __initdata =
{
	{
		.gpio      = 67,		/* BT regon */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_bt_regon_config,
			[GPIOMUX_SUSPENDED] = &gpio_bt_regon_config,
		},
	},
};

/*BT I2S*/
static struct gpiomux_setting  bt_i2s_act1_cfg =
{
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting  bt_i2s_act0_cfg =
{
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting  bt_i2s_sus_cfg =
{
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8226_bt_i2s1_configs[] __initdata =
{
	{
		.gpio   = 68,       /* bt i2s data to ap */
		.settings =
		{
			[GPIOMUX_SUSPENDED] = &bt_i2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &bt_i2s_act1_cfg,
		},
	},
	{
		.gpio   = 69,       /* bt i2s ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &bt_i2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &bt_i2s_act1_cfg,
		},
	},
};

static struct msm_gpiomux_config msm8226_bt_i2s0_configs[] __initdata =
{
	{
		.gpio   = 70,       /* bt  i2s clk */
		.settings =
		{
			[GPIOMUX_SUSPENDED] = &bt_i2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &bt_i2s_act0_cfg,
		},
	},
	{
		.gpio   = 71,       /* bt i2s data from ap */
		.settings = {
			[GPIOMUX_SUSPENDED] = &bt_i2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &bt_i2s_act0_cfg,
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

void __init msm8226_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

#if defined(CONFIG_KS8851) || defined(CONFIG_KS8851_MODULE)
	msm_gpiomux_install(msm_eth_configs, ARRAY_SIZE(msm_eth_configs));
#endif

	msm_gpiomux_install(msm_blsp_configs,
			ARRAY_SIZE(msm_blsp_configs));

    msm_gpiomux_install(msm_synaptics_configs,
				ARRAY_SIZE(msm_synaptics_configs));

	msm_gpiomux_install_nowrite(msm_lcd_configs,
			ARRAY_SIZE(msm_lcd_configs));


	msm_gpiomux_install(msm_lcd_te_configs,
			ARRAY_SIZE(msm_lcd_te_configs));

	msm_gpiomux_sdc3_install();

	/*BT uart3*/
	msm_gpiomux_install(msm_uart3_configs, ARRAY_SIZE(msm_uart3_configs));

	/*MCU uart1*/
	msm_gpiomux_install(msm_uart1_configs, ARRAY_SIZE(msm_uart1_configs));

	/*speaker*/
	msm_gpiomux_install(msm_speaker_configs, ARRAY_SIZE(msm_speaker_configs));

	/*motor*/
	msm_gpiomux_install(msm_motor_configs, ARRAY_SIZE(msm_motor_configs));

	/*motor ctrl*/
	msm_gpiomux_install(msm_motor_ctrl_configs, ARRAY_SIZE(msm_motor_ctrl_configs));

	/*I2S for speaker*/
	msm_gpiomux_install(msm8226_tert_mi2s_configs, ARRAY_SIZE(msm8226_tert_mi2s_configs));
	/*I2S for MIC*/
	msm_gpiomux_install(msm8226_quat_mi2s_configs, ARRAY_SIZE(msm8226_quat_mi2s_configs));

	/*MCU reset*/
	msm_gpiomux_install(msm_mcureset_config, ARRAY_SIZE(msm_mcureset_config));

	/*AP and MCU low power status*/
	msm_gpiomux_install(msm_low_power_status_config, ARRAY_SIZE(msm_low_power_status_config));

	/*MCU boot*/
	msm_gpiomux_install(msm_mcu_boot_configs, ARRAY_SIZE(msm_mcu_boot_configs));

	/*force dload*/
	msm_gpiomux_install(msm_force_dload_configs, ARRAY_SIZE(msm_force_dload_configs));

	/*wifi reg on*/
	msm_gpiomux_install(msm_wifi_regon_configs, ARRAY_SIZE(msm_wifi_regon_configs));

	/*lcd id*/
	msm_gpiomux_install(msm_lcd_id_configs, ARRAY_SIZE(msm_lcd_id_configs));

	/*AP wakeup peripheral*/
	msm_gpiomux_install(msm_wakeup_peripheral_irq_configs, ARRAY_SIZE(msm_wakeup_peripheral_irq_configs));

	/*peripheral wakeup AP*/
	msm_gpiomux_install(msm_wakeup_ap_irq_configs, ARRAY_SIZE(msm_wakeup_ap_irq_configs));

	/*BT reg on*/
	msm_gpiomux_install(msm_bt_regon_configs, ARRAY_SIZE(msm_bt_regon_configs));

	/*BT i2s*/
	msm_gpiomux_install(msm8226_bt_i2s1_configs, ARRAY_SIZE(msm8226_bt_i2s1_configs));
	msm_gpiomux_install(msm8226_bt_i2s0_configs, ARRAY_SIZE(msm8226_bt_i2s0_configs));
}
