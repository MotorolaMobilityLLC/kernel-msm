/* Copyright (c) 2014, ASUSTek Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>


/* Charger LED Control Registr Definition */
#define CHRLEDCTRL_REG 0x53
/* Charger LED State Maching */
#define CHRLEDFSM_REG 0x54
/* Charger LED PWM Register Definiition */
#define CHRLEDPWM_REG 0x55

static struct rpmsg_instance *pmic_instance;

void led_init(void)
{
	/* LED is software controll and enabled */
	intel_scu_ipc_iowrite8(CHRLEDCTRL_REG, 3);

	/* LED lighting effect: Breathing varies Duty Cycle */
	intel_scu_ipc_iowrite8(CHRLEDFSM_REG, 6);

	/* LED PWM: 40/256. There are a total of 256 duty cycle steps */
	intel_scu_ipc_iowrite8(CHRLEDPWM_REG, 40);
}

static int fugu_led_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic rpmsg device\n");

	/* Allocate rpmsg instance for pmic*/
	ret = alloc_rpmsg_instance(rpdev, &pmic_instance);
	if (!pmic_instance) {
		dev_err(&rpdev->dev, "kzalloc pmic instance failed\n");
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(pmic_instance);

	/* Initialize led */
	led_init();
out:
	return ret;
}

static void fugu_led_remove(struct rpmsg_channel *rpdev)
{
	free_rpmsg_instance(rpdev, &pmic_instance);
	dev_info(&rpdev->dev, "Removed pmic rpmsg device\n");
}

static void fugu_led_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id fugu_led_id_table[] = {
	{ .name	= "rpmsg_fugu_led" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, fugu_led_id_table);

static struct rpmsg_driver fugu_led = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= fugu_led_id_table,
	.probe		= fugu_led_probe,
	.callback	= fugu_led_cb,
	.remove		= fugu_led_remove,
};

static int __init fugu_led_init(void)
{
	return register_rpmsg_driver(&fugu_led);
}

module_init(fugu_led_init);

static void __exit fugu_led_exit(void)
{
	return unregister_rpmsg_driver(&fugu_led);
}
module_exit(fugu_led_exit);

MODULE_AUTHOR("Alan Lu<alan_lu@asus.com>");
MODULE_DESCRIPTION("Fugu Led Driver");
MODULE_LICENSE("GPL v2");
