/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/wakelock.h>

#include <asm/mach-types.h>

#include <mach/gpiomux.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <asm/system_info.h>

#define BT_LPM_ENABLE

#define BT_HOST_WAKE 48
#define BT_WAKE 50
#define BT_WAKE_EMUL 67
#define BT_WAKE_REV01 36
#define BT_EN 47

static struct rfkill *bt_rfkill;
static int cnt;

static int gpio_bt_host_wake = BT_HOST_WAKE;

int get_gpio_hwrev(int gpio)
{
	if(gpio == BT_WAKE) {
		if(system_rev == 0)
			return BT_WAKE;
		else if(system_rev == 3)
			return BT_WAKE_EMUL;
		else
			return BT_WAKE_REV01;
	}
	return gpio;
}

#ifdef BT_LPM_ENABLE
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= -1,
		.end	= -1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static void gpio_rev_init(void)
{
	pr_err("[BT] %s, gpio_bt_host_wake = %d", __func__, gpio_bt_host_wake);

	bluesleep_resources[0].start = get_gpio_hwrev(gpio_bt_host_wake);
	bluesleep_resources[0].end = get_gpio_hwrev(gpio_bt_host_wake);

	bluesleep_resources[1].start = get_gpio_hwrev(BT_WAKE);
	bluesleep_resources[1].end = get_gpio_hwrev(BT_WAKE);

	bluesleep_resources[2].start = gpio_to_irq(gpio_bt_host_wake);
	bluesleep_resources[2].end = gpio_to_irq(gpio_bt_host_wake);
}
#endif

static int bcm4334_bt_rfkill_set_power(void *data, bool blocked)
{
	int ret = -1;

	if (cnt < 1) {
		ret = gpio_request(BT_EN, "bt_en");
		if (ret) {
			pr_err("[BT] %s: gpio_request for host_wake is failed",
						__func__);
			gpio_free(BT_EN);
		}

		/* configure host_wake as input */
		gpio_tlmm_config(GPIO_CFG(gpio_bt_host_wake, 0, GPIO_CFG_INPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		ret = gpio_direction_input(gpio_bt_host_wake);
		if (ret < 0) {
			pr_err("[BT] %s: failed to configure ID for GPIO %d, error %d",
						__func__,
						gpio_bt_host_wake, ret);
			gpio_free(gpio_bt_host_wake);
		}
		cnt++;
	}
	pr_err("[BT] %s, gpio_bt_host_wake = %d", __func__, gpio_bt_host_wake);

	if (!blocked) {
		pr_err("[BT] Bluetooth Power On.\n");

		gpio_set_value(get_gpio_hwrev(BT_WAKE), 1);

		if (gpio_get_value(get_gpio_hwrev(gpio_bt_host_wake)) == 0)
			pr_err("[BT] BT_HOST_WAKE is low.\n");

		ret = gpio_direction_output(BT_EN, 1);
		if (ret)
			pr_err("[BT] failed to set BT_EN.\n");
	} else {
		pr_err("[BT] Bluetooth Power Off.\n");

		ret = gpio_direction_output(BT_EN, 0);
		if (ret)
			pr_err("[BT] failed to set BT_EN.\n");

		gpio_set_value(get_gpio_hwrev(BT_WAKE), 0);
	}
	return 0;
}

static struct platform_device bcm4334_bluetooth_platform_device = {
	.name		= "bcm4334_bluetooth",
	.id		= -1,
};

static struct platform_device *jf_bt_devs[] __initdata = {
	&bcm4334_bluetooth_platform_device,
};

static const struct rfkill_ops bcm4334_bt_rfkill_ops = {
	.set_block = bcm4334_bt_rfkill_set_power,
};

static int bcm4334_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;

	cnt = 0;

	bt_rfkill = rfkill_alloc("bcm4334 Bluetooth", &pdev->dev,
						RFKILL_TYPE_BLUETOOTH,
						&bcm4334_bt_rfkill_ops,
						NULL);

	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		return -ENOMEM;
	}


	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(get_gpio_hwrev(BT_WAKE));
		return rc;
	}


	rfkill_set_sw_state(bt_rfkill, true);

	return rc;
}

static int bcm4334_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(get_gpio_hwrev(BT_EN));
	gpio_free(get_gpio_hwrev(BT_WAKE));

	cnt = 0;
	return 0;
}

static struct platform_driver bcm4334_bluetooth_platform_driver = {
	.probe = bcm4334_bluetooth_probe,
	.remove = bcm4334_bluetooth_remove,
	.driver = {
		.name = "bcm4334_bluetooth",
		.owner = THIS_MODULE,
	},
};

static int __init bcm4334_bluetooth_init(void)
{
#ifdef BT_LPM_ENABLE
	gpio_rev_init();
	platform_device_register(&msm_bluesleep_device);
#endif
	platform_add_devices(jf_bt_devs, ARRAY_SIZE(jf_bt_devs));

	return platform_driver_register(&bcm4334_bluetooth_platform_driver);
}

static void __exit bcm4334_bluetooth_exit(void)
{
#ifdef BT_LPM_ENABLE
	platform_device_unregister(&msm_bluesleep_device);
#endif
	platform_driver_unregister(&bcm4334_bluetooth_platform_driver);
}

module_init(bcm4334_bluetooth_init);
module_exit(bcm4334_bluetooth_exit);

MODULE_ALIAS("platform:bcm4334");
MODULE_DESCRIPTION("bcm4334_bluetooth");
MODULE_LICENSE("GPL");
