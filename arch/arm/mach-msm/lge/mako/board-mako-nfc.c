/* Copyright(c) 2012, LGE Inc.
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

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"

#include "board-mako.h"

#include <linux/nfc/bcm2079x.h>

#define NFC_GPIO_VEN            55
#define NFC_GPIO_IRQ            29
#define NFC_GPIO_FIRM           37

static struct bcm2079x_platform_data bcm2079x_pdata = {
	.irq_gpio = NFC_GPIO_IRQ,
	.en_gpio = NFC_GPIO_VEN,
	.wake_gpio= NFC_GPIO_FIRM,
};

static struct i2c_board_info i2c_bcm2079x[] __initdata = {
	{
		I2C_BOARD_INFO("bcm2079x-i2c", 0x1FA),
		.flags = I2C_CLIENT_TEN,
		.irq = MSM_GPIO_TO_INT(NFC_GPIO_IRQ),
		.platform_data = &bcm2079x_pdata,
	},
};

static void __init lge_add_i2c_bcm2079x_device(void)
{
	i2c_register_board_info(APQ_8064_GSBI1_QUP_I2C_BUS_ID,
			       i2c_bcm2079x,
			       ARRAY_SIZE(i2c_bcm2079x));
}

void __init lge_add_bcm2079x_device(void)
{
	lge_add_i2c_bcm2079x_device();
}
