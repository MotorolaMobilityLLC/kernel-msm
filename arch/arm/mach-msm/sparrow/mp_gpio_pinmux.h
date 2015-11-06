#ifndef _PR_GPIO_PINMUX_H_
#define _PR_GPIO_PINMUX_H_

#include "gpio_pinmux_setting.h"

static struct msm_gpiomux_config pr_msm8226_gpio_configs[] __initdata= {
// ASUS_BSP BerylHou +++ "BT config"
	{
		.gpio      = 0,	/* BLSP1 BT Uart Tx */
		.settings = {
                        [GPIOMUX_ACTIVE] = &gpio_uart_config,
                        [GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 1,	/* BLSP1 BT Uart Rx */
		.settings = {
                        [GPIOMUX_ACTIVE] = &gpio_uart_config,
                        [GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 2,	/* BLSP1 BT Uart CTS */
		.settings = {
                        [GPIOMUX_ACTIVE] = &gpio_uart_config,
                        [GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 3,	/* BLSP1 BT Uart RTS */
		.settings = {
                        [GPIOMUX_ACTIVE] = &gpio_uart_config,
                        [GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
// ASUS_BSP BerylHou ---

// ASUS_BSP +++ Maggie_Lee "I2C"
	{
                .gpio = 6,		/* BLSP1 QUP2: I2C2 Sensors, I2C_DAT */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
        {
                .gpio = 7,		/* BLSP1 QUP2: I2C2 Sensors, I2C_CLK */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
	{
                .gpio = 10,		/* BLSP1 QUP3: I2C3 NFC, I2C_DAT */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
        {
                .gpio = 11,		/* BLSP1 QUP3: I2C3 NFC, I2C_CLK */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
// ASUS_BSP --- Maggie_Lee "I2C"
// ASUS_BSP +++ Cliff_Yu "TOUCH"
	{
		.gpio = 16,
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_rst,
			[GPIOMUX_SUSPENDED] = &touch_rst,
		},
	},
	{
		.gpio = 17,
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_int,
			[GPIOMUX_SUSPENDED] = &touch_int,
		},
	},
// ASUS_BSP --- Cliff_Yu "TOUCH"
// ASUS_BSP +++ Maggie_Lee "I2C"
        {
                .gpio = 18,	/* BLSP1 QUP5: I2C5 Touch, I2C_DAT */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
        {
                .gpio = 19,	/* BLSP1 QUP5: I2C5 Touch, I2C_CLK */
                .settings = {
                        [GPIOMUX_ACTIVE] = &gpio_i2c_config,
                        [GPIOMUX_SUSPENDED] = &gpio_i2c_config,
                },
        },
// ASUS_BSP --- Maggie_Lee "I2C"
        {
               .gpio = 24,             /* Tear Enable */
               .settings = {
                       [GPIOMUX_ACTIVE]    = &lcd_te_act_cfg,
                       [GPIOMUX_SUSPENDED] = &lcd_te_sus_cfg,
               },
        },
	{
		.gpio = 25,
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_rst_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_rst_sus_cfg,
		},
	},
// ASUS_BSP +++ Joe_Tsai "Wifi config SDIO interface"
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
// ASUS_BSP --- Joe_Tsai "Wifi config SDIO interface"
//ASUS_BSP BerylHou +++ "BT / host"
	{
		.gpio      = 48,	/* BT wake up host */
		.settings = {
                        [GPIOMUX_ACTIVE] = &bt_wakup_host_act_cfg,
                        [GPIOMUX_SUSPENDED] = &bt_wakup_host_sus_cfg,
		},
	},
//ASUS_BSP BerylHou ---
//ASUS_BSP +++ Ken_Cheng "MI2S for digital MIC"
// ASUS_BSP Ken_Cheng +++ I2S Speaker AMP
#ifdef CONFIG_SND_SOC_MSM8226_I2S_SPKR_AMP
	{
                .gpio = 49,           /* tertiary mi2s sck */
                .settings = {
                        [GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
                        [GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
                },
        },
        {
                .gpio = 50,           /* tertiary mi2s ws */
                .settings = {
                        [GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
                        [GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
                },
        },
        {
                .gpio = 52,           /* tertiary mi2s dout */
                .settings = {
                        [GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
                        [GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
                },
        },
#endif
// ASUS_BSP Ken_Cheng --- I2S Speaker AMP
	{
		.gpio = 60,           /* pri mi2s enable */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_en,
		},
	},
//ASUS_BSP BerylHou +++ "BT / host"
	{
		.gpio      = 61,	/* Host wake up BT */
		.settings = {
                        [GPIOMUX_ACTIVE] = &host_wakup_bt_act_cfg,
                        [GPIOMUX_SUSPENDED] = &host_wakup_bt_sus_cfg,
		},
	},
//ASUS_BSP BerylHou ---
	{
		.gpio = 63,           /* pri mi2s sck */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
		},
	},
	{
		.gpio = 64,           /* pri mi2s ws */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
		},
	},
	{
		.gpio = 65,           /* pri mi2s din */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
		},
	},
//ASUS_BSP --- Ken_Cheng "MI2S for digital MIC"
	{
		.gpio = 68,           /* secd mi2s out */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
		},
	},
	{
		.gpio = 69,           /* secd mi2s sync */
		.settings = {
			[GPIOMUX_SUSPENDED] = &pri_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &pri_mi2s_act_cfg,
		},
	},
	{
		.gpio = 70,           /* secd mi2s clk */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sec_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &sec_mi2s_act_cfg,
		},
	},
	{
		.gpio = 71,           /* secd mi2s in */
		.settings = {
			[GPIOMUX_SUSPENDED] = &sec_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &sec_mi2s_act_cfg,
		},
	},
//ASUS_BSP +++ Evan_Yeh "PNI sensors hub porting"
{
               .gpio = 111,
               .settings = {
                       [GPIOMUX_ACTIVE]    = &sensors_hub_int,
                       [GPIOMUX_SUSPENDED] = &sensors_hub_int,
               },
       },
//ASUS_BSP --- Evan_Yeh "PNI sensors hub porting"

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

	{
		.gpio      = 38,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 57,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 58,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 59,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 67,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 72,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 105,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
	{
		.gpio      = 108,		/* NC */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_nc_cfg,
		},
	},
};

#endif  /* _PR_GPIO_PINMUX_H_  */
