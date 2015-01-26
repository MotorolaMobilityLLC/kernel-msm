/*
 * Copyright (C) 2014 Motorola Mobility LLC
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

struct bq5105x_ctrl_info {
	int num_gpios;
	struct gpio *list;
};

static struct bq5105x_ctrl_info *of_bq5105x_ctrl(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct bq5105x_ctrl_info *info;
	int i, gpio_count;
	enum of_gpio_flags flags;

	if (!np) {
		dev_err(&pdev->dev, "devtree data not found\n");
		return NULL;
	}

	gpio_count = of_gpio_count(np);

	if (gpio_count <= 0) {
		dev_err(&pdev->dev, "no GPIOS defined\n");
		return NULL;
	}

	/* Make sure number of GPIOs defined matches the supplied number of
	 * GPIO name strings.
	 */
	if (gpio_count != of_property_count_strings(np, "gpio-names")) {
		dev_err(&pdev->dev, "number of GPIOs and names mismatch\n");
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

static int bq5105x_ctrl_probe(struct platform_device *pdev)
{
	struct bq5105x_ctrl_info *info;
	int ret;
	int i;

	info = of_bq5105x_ctrl(pdev);
	if (!info) {
		dev_err(&pdev->dev, "failed to parse devtree node\n");
		return -ENODEV;
	}

	ret = gpio_request_array(info->list, info->num_gpios);
	if (ret) {
		dev_err(&pdev->dev, "failed to request GPIOs\n");
		return ret;
	}

	for (i = 0; i < info->num_gpios; i++) {
		ret = gpio_export(info->list[i].gpio, false);
		if (ret) {
			dev_err(&pdev->dev, "Failed to export GPIO %s: %d\n",
				info->list[i].label, info->list[i].gpio);
			goto fail;
		}

		ret = gpio_export_link(&pdev->dev, info->list[i].label,
					info->list[i].gpio);
		if (ret) {
			dev_err(&pdev->dev, "failed to link GPIO %s: %d\n",
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

static int bq5105x_ctrl_remove(struct platform_device *pdev)
{
	struct bq5105x_ctrl_info *info = platform_get_drvdata(pdev);

	gpio_free_array(info->list, info->num_gpios);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id bq5105x_ctrl_of_tbl[] = {
	{ .compatible = "ti,bq5105x-control", },
	{ },
};
MODULE_DEVICE_TABLE(of, bq5105x_ctrl_of_tbl);

static struct platform_driver bq5105x_ctrl_driver = {
	.probe = bq5105x_ctrl_probe,
	.remove = bq5105x_ctrl_remove,
	.driver = {
		.name = "bq5105x_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = bq5105x_ctrl_of_tbl,
	},
};

module_platform_driver(bq5105x_ctrl_driver);

MODULE_ALIAS("platform:bq5105x_ctrl");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("BQ5105x Control Driver");
MODULE_LICENSE("GPL");
