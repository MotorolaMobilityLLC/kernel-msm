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

extern unsigned int system_rev;

static struct gpiomux_setting gpio_sensorhub_active = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_sensorhub_suspend = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi_act_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_spi_cs_act_config = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};
static struct gpiomux_setting gpio_spi_susp_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting gpio_func_i2c_config = {
	.func = GPIOMUX_FUNC_3,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gpio_gpio_i2c_config = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* for BT Uart TTYHS0 */
static struct gpiomux_setting gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_16MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct gpiomux_setting rx_gpio_uart_config = {
	.func = GPIOMUX_FUNC_2,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_OUT_HIGH,
};

static struct msm_gpiomux_config msm_sensorhub_configs[] __initdata = {
	{
		.gpio = 92,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sensorhub_active,
			[GPIOMUX_SUSPENDED] = &gpio_sensorhub_suspend,
		},
	},
	{
		.gpio = 106,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sensorhub_active,
			[GPIOMUX_SUSPENDED] = &gpio_sensorhub_suspend,
		},
	},
	{
		.gpio = 107,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sensorhub_active,
			[GPIOMUX_SUSPENDED] = &gpio_sensorhub_suspend,
		},
	},
	{
		.gpio = 108,
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_sensorhub_active,
			[GPIOMUX_SUSPENDED] = &gpio_sensorhub_suspend,
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
		.gpio = 25,		/* LCD Reset */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_rst_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_rst_sus_cfg,
		},
	},
	{
		.gpio = 109,		/* LCD Enable */
		.settings = {
			[GPIOMUX_ACTIVE]    = &lcd_rst_act_cfg,
			[GPIOMUX_SUSPENDED] = &lcd_rst_sus_cfg,
		},
	}
};

static struct msm_gpiomux_config msm_blsp_configs[] __initdata = {
	{
		.gpio      = 0,		/* BLSP1 QUP1 SPI_DATA_MOSI */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 1,		/* BLSP1 QUP1 SPI_DATA_MISO */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 2,		/* BLSP1 QUP1 SPI_CS_ETH */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_cs_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio      = 3,		/* BLSP1 QUP1 SPI_CLK */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_spi_act_config,
			[GPIOMUX_SUSPENDED] = &gpio_spi_susp_config,
		},
	},
	{
		.gpio	   = 4,		/* MUIC I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_gpio_i2c_config,
		},
	},
	{
		.gpio	   = 5,		/* MUIC I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_gpio_i2c_config,
		},
	},
	{
		.gpio	   = 8, 	/* UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio	   = 9, 	/* UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &rx_gpio_uart_config,
		},
	},
	{
		.gpio	   = 10,	/* FUEL I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_gpio_i2c_config,
		},
	},
	{
		.gpio	   = 11,	/* FUEL I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_gpio_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_gpio_i2c_config,
		},
	},
	{
		.gpio      = 14,	/* BLSP1 QUP4 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_func_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_func_i2c_config,
		},
	},
	{
		.gpio      = 15,	/* BLSP1 QUP4 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_func_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_func_i2c_config,
		},
	},
	{
		.gpio      = 18,		/* BLSP1 QUP5 I2C_SDA */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_func_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_func_i2c_config,
		},
	},
	{
		.gpio      = 19,		/* BLSP1 QUP5 I2C_SCL */
		.settings = {
			[GPIOMUX_ACTIVE] = &gpio_func_i2c_config,
			[GPIOMUX_SUSPENDED] = &gpio_func_i2c_config,
		},
	},

	/* for BT uart ttyHS0 */
	{
		.gpio      = 20,		/* BLSP6 UART TX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 21,		/* BLSP6 UART RX */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 22,		/* BLSP6 UART CTS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
	{
		.gpio      = 23,		/* BLSP6 UART RTS */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_uart_config,
		},
	},
};

static struct gpiomux_setting gpio_bcm4334w_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
		.dir = GPIOMUX_OUT_HIGH,
	},
	{
	  .func = GPIOMUX_FUNC_2,
	  .drv = GPIOMUX_DRV_8MA,
	  .pull = GPIOMUX_PULL_DOWN,
	},
	{
	  .func = GPIOMUX_FUNC_GPIO,
	  .drv = GPIOMUX_DRV_8MA,
	  .pull = GPIOMUX_PULL_DOWN,
	},
	{
	  .func = GPIOMUX_FUNC_GPIO,
	  .drv = GPIOMUX_DRV_2MA,
	  .pull = GPIOMUX_PULL_DOWN,
	  .dir = GPIOMUX_IN,
	},

};

static struct msm_gpiomux_config msm_bcm4334w_config[] __initdata = {
	{
		.gpio = 32,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[4],
		},
	},
	{
		.gpio = 39,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 40,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 41,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 42,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 43,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 44,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[5],
			[GPIOMUX_ACTIVE] = &gpio_bcm4334w_config[3],
		},
	},
	{
		.gpio = 47,	/* BT_EN */
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_bcm4334w_config[1],
			[GPIOMUX_SUSPENDED] = &gpio_bcm4334w_config[1],
		},
	},

};

static struct gpiomux_setting auxpcm_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting auxpcm_sus_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm_auxpcm_configs[] __initdata = {
	{
		.gpio = 63,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 64,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 65,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
	{
		.gpio = 66,
		.settings = {
			[GPIOMUX_SUSPENDED] = &auxpcm_sus_cfg,
			[GPIOMUX_ACTIVE] = &auxpcm_act_cfg,
		},
	},
};

static struct gpiomux_setting  tert_mi2s_act_cfg = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting  tert_mi2s_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8226_tertiary_mi2s_configs[] __initdata = {
	{
		.gpio	= 49,
		.settings = {
			[GPIOMUX_SUSPENDED] = &tert_mi2s_sus_cfg,
			[GPIOMUX_ACTIVE] = &tert_mi2s_act_cfg,
		},
	},
	{
		.gpio	= 50,
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
};

static struct gpiomux_setting gpio_tsp_config = {
	.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config msm_tsp_configs[] __initdata = {
	{
		.gpio = 17,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_tsp_config,
			[GPIOMUX_ACTIVE] = &gpio_tsp_config,
		},
	},
};

static struct gpiomux_setting gpio_max77826_config[] = {
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_NONE,
	},
	{
		.func = GPIOMUX_FUNC_GPIO,  /* IN-PU */
		.drv = GPIOMUX_DRV_2MA,
		.pull = GPIOMUX_PULL_UP,
	},
};

static struct msm_gpiomux_config msm_max77836_configs[] __initdata = {
	{
		.gpio = 37,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_max77826_config[0],
			[GPIOMUX_ACTIVE] = &gpio_max77826_config[0],
		},
	},
	{
		.gpio = 69,
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_max77826_config[0],
			[GPIOMUX_ACTIVE] = &gpio_max77826_config[0],
		},
	},
};

static struct gpiomux_setting gpio_hwchk_config = {
	.func = GPIOMUX_FUNC_GPIO,  /* IN-NP */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config sprat_rev01_hwchk_configs[] __initdata = {
	{
		.gpio = 14,	/* HW_CHK_BIT1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
	{
		.gpio = 15,	/* HW_CHK_BIT0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
	{
		.gpio = 91,	/* HW_CHK_BIT2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
};

static struct msm_gpiomux_config sprat_rev02_hwchk_configs[] __initdata = {
	{
		.gpio = 14,	/* HW_CHK_BIT1 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
	{
		.gpio = 15,	/* HW_CHK_BIT0 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
	{
		.gpio = 80,	/* HW_CHK_BIT3 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
	{
		.gpio = 91,	/* HW_CHK_BIT2 */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gpio_hwchk_config,
			[GPIOMUX_ACTIVE] = &gpio_hwchk_config,
		},
	},
};


static struct gpiomux_setting gpio_suspend_config = {
	.func = GPIOMUX_FUNC_GPIO,  /* IN-PD */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

#define GPIOMUX_SET_NC(n) \
	{ \
		.gpio	   = n, \
		.settings = { \
			[GPIOMUX_ACTIVE] = &gpio_suspend_config, \
			[GPIOMUX_SUSPENDED] = &gpio_suspend_config, \
		}, \
	}

static struct msm_gpiomux_config sprat_rev01_nc_configs[] __initdata = {
	GPIOMUX_SET_NC(6),
	GPIOMUX_SET_NC(7),
	GPIOMUX_SET_NC(12),
	GPIOMUX_SET_NC(13),
	GPIOMUX_SET_NC(16),
	GPIOMUX_SET_NC(26),
	GPIOMUX_SET_NC(27),
	GPIOMUX_SET_NC(28),
	GPIOMUX_SET_NC(29),
	GPIOMUX_SET_NC(30),
	GPIOMUX_SET_NC(33),
	GPIOMUX_SET_NC(34),
	GPIOMUX_SET_NC(35),
	GPIOMUX_SET_NC(38),
	GPIOMUX_SET_NC(45),
	GPIOMUX_SET_NC(46),
	GPIOMUX_SET_NC(52),
	GPIOMUX_SET_NC(53),
	GPIOMUX_SET_NC(54),
	GPIOMUX_SET_NC(55),
	GPIOMUX_SET_NC(56),
	GPIOMUX_SET_NC(57),
	GPIOMUX_SET_NC(58),
	GPIOMUX_SET_NC(59),
	GPIOMUX_SET_NC(60),
	GPIOMUX_SET_NC(62),
	GPIOMUX_SET_NC(67),
	GPIOMUX_SET_NC(68),
	GPIOMUX_SET_NC(70),
	GPIOMUX_SET_NC(71),
	GPIOMUX_SET_NC(73),
	GPIOMUX_SET_NC(75),
	GPIOMUX_SET_NC(76),
	GPIOMUX_SET_NC(77),
	GPIOMUX_SET_NC(78),
	GPIOMUX_SET_NC(79),
	GPIOMUX_SET_NC(80),
	GPIOMUX_SET_NC(81),
	GPIOMUX_SET_NC(82),
	GPIOMUX_SET_NC(83),
	GPIOMUX_SET_NC(84),
	GPIOMUX_SET_NC(85),
	GPIOMUX_SET_NC(86),
	GPIOMUX_SET_NC(87),
	GPIOMUX_SET_NC(88),
	GPIOMUX_SET_NC(89),
	GPIOMUX_SET_NC(90),
	GPIOMUX_SET_NC(93),
	GPIOMUX_SET_NC(94),
	GPIOMUX_SET_NC(95),
	GPIOMUX_SET_NC(96),
	GPIOMUX_SET_NC(97),
	GPIOMUX_SET_NC(98),
	GPIOMUX_SET_NC(99),
	GPIOMUX_SET_NC(100),
	GPIOMUX_SET_NC(101),
	GPIOMUX_SET_NC(102),
	GPIOMUX_SET_NC(103),
	GPIOMUX_SET_NC(104),
	GPIOMUX_SET_NC(105),
	GPIOMUX_SET_NC(110),
	GPIOMUX_SET_NC(111),
	GPIOMUX_SET_NC(112),
	GPIOMUX_SET_NC(114),
	GPIOMUX_SET_NC(115),
	GPIOMUX_SET_NC(116),
};

static struct msm_gpiomux_config sprat_rev02_nc_configs[] __initdata = {
	GPIOMUX_SET_NC(6),
	GPIOMUX_SET_NC(7),
	GPIOMUX_SET_NC(12),
	GPIOMUX_SET_NC(13),
	GPIOMUX_SET_NC(16),
	GPIOMUX_SET_NC(26),
	GPIOMUX_SET_NC(27),
	GPIOMUX_SET_NC(28),
	GPIOMUX_SET_NC(29),
	GPIOMUX_SET_NC(30),
	GPIOMUX_SET_NC(33),
	GPIOMUX_SET_NC(34),
	GPIOMUX_SET_NC(35),
	GPIOMUX_SET_NC(38),
	GPIOMUX_SET_NC(45),
	GPIOMUX_SET_NC(46),
	GPIOMUX_SET_NC(52),
	GPIOMUX_SET_NC(53),
	GPIOMUX_SET_NC(54),
	GPIOMUX_SET_NC(55),
	GPIOMUX_SET_NC(56),
	GPIOMUX_SET_NC(57),
	GPIOMUX_SET_NC(58),
	GPIOMUX_SET_NC(59),
	GPIOMUX_SET_NC(60),
	GPIOMUX_SET_NC(62),
	GPIOMUX_SET_NC(67),
	GPIOMUX_SET_NC(68),
	GPIOMUX_SET_NC(70),
	GPIOMUX_SET_NC(71),
	GPIOMUX_SET_NC(73),
	GPIOMUX_SET_NC(75),
	GPIOMUX_SET_NC(76),
	GPIOMUX_SET_NC(77),
	GPIOMUX_SET_NC(78),
	GPIOMUX_SET_NC(79),
	GPIOMUX_SET_NC(81),
	GPIOMUX_SET_NC(82),
	GPIOMUX_SET_NC(83),
	GPIOMUX_SET_NC(84),
	GPIOMUX_SET_NC(85),
	GPIOMUX_SET_NC(86),
	GPIOMUX_SET_NC(87),
	GPIOMUX_SET_NC(88),
	GPIOMUX_SET_NC(89),
	GPIOMUX_SET_NC(90),
	GPIOMUX_SET_NC(93),
	GPIOMUX_SET_NC(94),
	GPIOMUX_SET_NC(95),
	GPIOMUX_SET_NC(96),
	GPIOMUX_SET_NC(97),
	GPIOMUX_SET_NC(98),
	GPIOMUX_SET_NC(99),
	GPIOMUX_SET_NC(100),
	GPIOMUX_SET_NC(101),
	GPIOMUX_SET_NC(102),
	GPIOMUX_SET_NC(103),
	GPIOMUX_SET_NC(104),
	GPIOMUX_SET_NC(105),
	GPIOMUX_SET_NC(110),
	GPIOMUX_SET_NC(111),
	GPIOMUX_SET_NC(112),
	GPIOMUX_SET_NC(114),
	GPIOMUX_SET_NC(115),
	GPIOMUX_SET_NC(116),
};


void __init msm8226_init_gpiomux(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	/* blsp6 for bt uart ttyHS0 */
	msm_gpiomux_install(msm_blsp_configs, ARRAY_SIZE(msm_blsp_configs));

	msm_gpiomux_install(msm_sensorhub_configs,
			ARRAY_SIZE(msm_sensorhub_configs));

	msm_gpiomux_install(msm_bcm4334w_config,
			ARRAY_SIZE(msm_bcm4334w_config));

	msm_gpiomux_install_nowrite(msm_lcd_configs,
			ARRAY_SIZE(msm_lcd_configs));

	msm_gpiomux_install(msm_auxpcm_configs,
			ARRAY_SIZE(msm_auxpcm_configs));

	if (system_rev > 0) {
		msm_gpiomux_install(msm8226_tertiary_mi2s_configs,
				ARRAY_SIZE(msm8226_tertiary_mi2s_configs));
	}

	msm_gpiomux_install(msm_tsp_configs,
			ARRAY_SIZE(msm_tsp_configs));

	msm_gpiomux_install(msm_max77836_configs,
			ARRAY_SIZE(msm_max77836_configs));

	switch (system_rev) {
	case 1:
		msm_gpiomux_install(sprat_rev01_nc_configs,
				ARRAY_SIZE(sprat_rev01_nc_configs));
		msm_gpiomux_install(sprat_rev01_hwchk_configs,
				ARRAY_SIZE(sprat_rev01_hwchk_configs));
		break;
	case 2:
	case 4:
		msm_gpiomux_install(sprat_rev02_nc_configs,
				ARRAY_SIZE(sprat_rev01_nc_configs));
		msm_gpiomux_install(sprat_rev02_hwchk_configs,
				ARRAY_SIZE(sprat_rev02_hwchk_configs));
		break;
	default:
		break;
	}
}
