#ifndef _WI500Q_SR2_GPIO_PINMUX_H_
#define _WI500Q_SR2_GPIO_PINMUX_H_

#include "wi500q_gpio_pinmux_setting.h"

static struct msm_gpiomux_config wi500q_sr2_msm8226_gpio_configs[] __initdata= {
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
// ASUS_BSP --- Maggie_Lee "I2C"
// ASUS_BSP +++ Cliff_Yu "TOUCH"
	{
		.gpio = 16,
		.settings = {
			[GPIOMUX_ACTIVE]    = &touch_reset,
			[GPIOMUX_SUSPENDED] = &touch_reset,
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
// ASUS_BSP +++ Maggie_Lee "ECG sensor porting"
	{
		.gpio = 20,
		.settings = {
			[GPIOMUX_ACTIVE] = &ecg_uart_act_cfg,
			[GPIOMUX_SUSPENDED] = &ecg_uart_sus_cfg,
		},
	},
	{
		.gpio = 21,
		.settings = {
			[GPIOMUX_ACTIVE] = &ecg_uart_act_cfg,
			[GPIOMUX_SUSPENDED] = &ecg_uart_sus_cfg,
		},
	},
// ASUS_BSP --- Maggie_Lee "ECG sensor porting"
	{
		.gpio = 24,		/* Tear Enable */
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
//ASUS_BSP +++ Maggie_Lee "Sensors Porting"
	{
		.gpio = 33,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gyro_int,
			[GPIOMUX_SUSPENDED] = &gyro_int,
		},
	},
	{
		.gpio = 35,
		.settings = {
			[GPIOMUX_ACTIVE]    = &mpu_int,
			[GPIOMUX_SUSPENDED] = &mpu_int,
		},
	},
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gyro_int,
			[GPIOMUX_SUSPENDED] = &gyro_int,
		},
	},
	//ASUS_BSP --- Maggie_Lee "Sensors Porting"
//ASUS_BSP BerylHou +++ "BT / host"
	{
		.gpio      = 48,	/* BT wake up host */
		.settings = {
                        [GPIOMUX_ACTIVE] = &bt_wakup_host_act_cfg,
                        [GPIOMUX_SUSPENDED] = &bt_wakup_host_sus_cfg,
		},
	},
//ASUS_BSP BerylHou ---
// ASUS_BSP +++ Maggie_Lee "ECG sensor porting"
	{
		.gpio      = 53,	/* ECG CS CPU*/
		.settings = {
                        [GPIOMUX_ACTIVE] = &ecg_act_cfg,
                        [GPIOMUX_SUSPENDED] = &ecg_sus_cfg,
		},
	},
	{
		.gpio      = 54,
		.settings = {
                        [GPIOMUX_ACTIVE] = &sensor_backup_int,
                        [GPIOMUX_SUSPENDED] = &sensor_backup_int,
		},
	},
	{
		.gpio      = 55,	/* ECG_RST CPU*/
		.settings = {
                        [GPIOMUX_ACTIVE] = &ecg_act_cfg,
                        [GPIOMUX_SUSPENDED] = &ecg_sus_cfg,
		},
	},
// ASUS_BSP --- Maggie_Lee "ECG sensor porting"
//ASUS_BSP +++ Ken_Cheng "MI2S for digital MIC"
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

#endif  /* _WI500Q_SR2_GPIO_PINMUX_H_  */
