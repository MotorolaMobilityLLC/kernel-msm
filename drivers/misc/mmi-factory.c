/*
 * Copyright (C) 2012-2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct mmi_factory_info {
	int num_gpios;
	struct gpio *list;
};

/* The driver should only be present when booting with the environment variable
 * indicating factory-cable is present.
 */
static bool mmi_factory_cable_present(void)
{
	struct device_node *np;
	bool fact_cable;

	np = of_find_node_by_path("/chosen");
	fact_cable = of_property_read_bool(np, "mmi,factory-cable");
	of_node_put(np);

	if (!np || !fact_cable)
		return false;

	return true;
}

static struct mmi_factory_info *mmi_parse_of(struct platform_device *pdev)
{
	struct mmi_factory_info *info;
	int gpio_count;
	struct device_node *np = pdev->dev.of_node;
	int i;
	enum of_gpio_flags flags;

	if (!np) {
		dev_err(&pdev->dev, "No OF DT node found.\n");
		return NULL;
	}

	gpio_count = of_gpio_count(np);

	if (!gpio_count) {
		dev_err(&pdev->dev, "No GPIOS defined.\n");
		return NULL;
	}

	/* Make sure number of GPIOs defined matches the supplied number of
	 * GPIO name strings.
	 */
	if (gpio_count != of_property_count_strings(np, "gpio-names")) {
		dev_err(&pdev->dev, "GPIO info and name mismatch\n");
		return NULL;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return NULL;

	info->list = devm_kzalloc(&pdev->dev,
				sizeof(struct gpio) * gpio_count,
				GFP_KERNEL);
	if (!info->list)
		return NULL;

	info->num_gpios = gpio_count;
	for (i = 0; i < gpio_count; i++) {
		info->list[i].gpio = of_get_gpio_flags(np, i, &flags);
		info->list[i].flags = flags;
		of_property_read_string_index(np, "gpio-names", i,
						&info->list[i].label);
		dev_dbg(&pdev->dev, "GPIO: %d  FLAGS: %ld  LABEL: %s\n",
			info->list[i].gpio, info->list[i].flags,
			info->list[i].label);
	}

	return info;
}

static int mmi_factory_probe(struct platform_device *pdev)
{
	struct mmi_factory_info *info;
	int ret;
	int i;

	if (!mmi_factory_cable_present()) {
		dev_dbg(&pdev->dev, "factory cable not present\n");
		return -ENODEV;
	}

	info = mmi_parse_of(pdev);
	if (!info) {
		dev_err(&pdev->dev, "failed to parse node\n");
		return -ENODEV;
	}

	ret = gpio_request_array(info->list, info->num_gpios);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIOs\n");
		return ret;
	}

	for (i = 0; i < info->num_gpios; i++) {
		ret = gpio_export(info->list[i].gpio, 1);
		if (ret) {
			dev_err(&pdev->dev, "Failed to export GPIO %s: %d\n",
				info->list[i].label, info->list[i].gpio);
			goto fail;
		}

		ret = gpio_export_link(&pdev->dev, info->list[i].label,
					info->list[i].gpio);
		if (ret) {
			dev_err(&pdev->dev, "Failed to link GPIO %s: %d\n",
				info->list[i].label, info->list[i].gpio);
			goto fail;
		}
	}

	platform_set_drvdata(pdev, info);

	return 0;

fail:
	gpio_free_array(info->list, info->num_gpios);
	return ret;
}

static int mmi_factory_remove(struct platform_device *pdev)
{
	struct mmi_factory_info *info = platform_get_drvdata(pdev);

	if (info)
		gpio_free_array(info->list, info->num_gpios);

	return 0;
}

static const struct of_device_id mmi_factory_of_tbl[] = {
	{ .compatible = "mmi,factory-support-msm8960", },
	{ },
};
MODULE_DEVICE_TABLE(of, mmi_factory_of_tbl);

static struct platform_driver mmi_factory_driver = {
	.probe = mmi_factory_probe,
	.remove = mmi_factory_remove,
	.driver = {
		.name = "mmi_factory",
		.owner = THIS_MODULE,
		.of_match_table = mmi_factory_of_tbl,
	},
};

static int __init mmi_factory_init(void)
{
	return platform_driver_register(&mmi_factory_driver);
}

static void __exit mmi_factory_exit(void)
{
	platform_driver_unregister(&mmi_factory_driver);
}

module_init(mmi_factory_init);
module_exit(mmi_factory_exit);

MODULE_ALIAS("platform:mmi_factory");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility Factory Specific Controls");
MODULE_LICENSE("GPL");
