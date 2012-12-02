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
	{	.compatible = "qcom,msm8960-pinmux" },
	{ }
};

static int msm_pinmux_install(struct device_node *node)
{
	int rc;
	int gpio;
	struct device_node *child;
	struct gpiomux_setting s;
	enum msm_gpiomux_setting which;  /* active or suspended */


	rc = of_property_read_u32(node, "qcom,pin-num", &gpio);
	if (rc) {
		pr_err("Required device tree property missing: " \
			"no qcom,pin-num\n");
		return -ENODEV;
	}

	for_each_child_of_node(node, child) {
		which = GPIOMUX_SUSPENDED;
		s.func = GPIOMUX_FUNC_GPIO;
		s.drv = GPIOMUX_DRV_2MA;
		s.pull = GPIOMUX_PULL_NONE;
		s.dir = GPIOMUX_IN;

		pr_debug("\tpin %d\tname = %s\n", gpio, child->name);
		if (!strcmp("qcom,active", child->name))
			which = GPIOMUX_ACTIVE;

		of_property_read_u32(child, "qcom,func", &s.func);
		of_property_read_u32(child, "qcom,drv", &s.drv);
		of_property_read_u32(child, "qcom,pull", &s.pull);
		of_property_read_u32(child, "qcom,dir", &s.dir);

		rc = msm_gpiomux_write(gpio, which, &s, NULL);
		if (rc)
			pr_err("Failed to install GPIO%d, ret: %d\n", gpio, rc);
	}

	return rc;
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

	msm_gpiomux_init(NR_GPIO_IRQS);

	for_each_child_of_node(node, child) {
		if (msm_pinmux_install(child) == 0)
			count++;
	}

	pr_debug("installed %d PINMUX\n", count);

	return 0;
}

static int __devexit msm_pinmux_remove(struct platform_device *pdev)
{
	return -ENODEV;
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
