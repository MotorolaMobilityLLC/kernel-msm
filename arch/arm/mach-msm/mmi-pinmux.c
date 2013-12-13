/* Copyright (c) 2012, Motorola Mobility LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <asm/mach-types.h>
#include <mach/gpiomux.h>
#include <mach/irqs.h>

static struct of_device_id msm_pinmux_dt_match[] = {
	{	.compatible = "qcom,msm8x26-pinmux" },
	{	.compatible = "qcom,msm8x10-pinmux" },
	{ }
};

static int msm_pinmux_install(struct device_node *node)
{
	int rc;
	int gpio;
	int count = 0;
	int i;
	struct device_node *child;
	struct gpiomux_setting s[GPIOMUX_NSETTINGS];
	struct msm_gpiomux_config c;
	enum msm_gpiomux_setting which;  /* active or suspended */


	rc = of_property_read_u32(node, "qcom,pin-num", &gpio);
	if (rc) {
		pr_err("device tree property missing: no qcom,pin-num\n");
		return -ENODEV;
	}

	for (i = 0; i < GPIOMUX_NSETTINGS; i++) {
		s[i].func = GPIOMUX_FUNC_GPIO;
		s[i].drv = GPIOMUX_DRV_2MA;
		s[i].pull = GPIOMUX_PULL_NONE;
		s[i].dir = GPIOMUX_IN;
	}

	for_each_child_of_node(node, child) {
		which = GPIOMUX_SUSPENDED;

		pr_debug("\tpin %d\tname = %s\n", gpio, child->name);

		if (!strcmp("qcom,active", child->name))
			which = GPIOMUX_ACTIVE;

		of_property_read_u32(child, "qcom,func", &s[which].func);
		of_property_read_u32(child, "qcom,drv", &s[which].drv);
		of_property_read_u32(child, "qcom,pull", &s[which].pull);
		of_property_read_u32(child, "qcom,dir", &s[which].dir);

		count++;
	}

	if (!count) {
		pr_err("gpio %d badly formatted: no children\n", gpio);
		return -EINVAL;
	}

	if (count != GPIOMUX_NSETTINGS) {
		pr_debug("%s: gpio %d dt entry count %d.\n", __func__, gpio,
				count);
		return msm_gpiomux_write(gpio, which, &s[which], NULL);
	}

	c.gpio = gpio;
	for (i = 0; i < GPIOMUX_NSETTINGS; i++)
		c.settings[i] = &s[i];

	if (of_property_read_bool(node, "qcom,pin-keep-state"))
		msm_gpiomux_install_nowrite(&c, 1);
	else
		msm_gpiomux_install(&c, 1);

	return 0;
}

static int __devinit msm_pinmux_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	int count = 0;

	if (!node) {
		pr_warn("devtree node not present!\n");
		return -ENODEV;
	}

	msm_gpiomux_init_dt();

	for_each_child_of_node(node, child) {
		if (msm_pinmux_install(child) == 0)
			count++;
	}

	pr_debug("installed %d PINMUX\n", count);

	return count ? 0 : -ENODEV;
}

static int __devexit msm_pinmux_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver msm_pinmux_driver = {
	.probe = msm_pinmux_probe,
	.remove = __devexit_p(msm_pinmux_remove),
	.driver = {
		.name = "msm_pinmux",
		.owner = THIS_MODULE,
		.of_match_table = msm_pinmux_dt_match,
	},
};

static int __init msm_pinmux_init(void)
{
	return platform_driver_register(&msm_pinmux_driver);
}

static void __exit msm_pinmux_exit(void)
{
	platform_driver_unregister(&msm_pinmux_driver);
}

postcore_initcall(msm_pinmux_init);
module_exit(msm_pinmux_exit);

MODULE_DESCRIPTION("QCOM MSM device tree pinmux installer");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
