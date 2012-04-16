/*
 * Copyright (C) 2012 Motorola, Inc.
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
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_data/mmi-factory.h>

struct mmi_factory_info {
	int num_gpios;
	int gpios[1];
};

static int __devinit mmi_factory_probe(struct platform_device *pdev)
{
	int i;
	int gpio_count = 0; /* number of successfully registered gpios */
	int err = 0;
	struct mmi_factory_info *info;
	struct mmi_factory_platform_data *pdata = pdev->dev.platform_data;

	info = kzalloc(sizeof(struct mmi_factory_info) +
		(sizeof(int) * (pdata->num_gpios - 1)),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	for (i = 0; i < pdata->num_gpios; i++) {
		int gpio = pdata->gpios[i].number;

		err = gpio_request(gpio, pdata->gpios[i].name);
		if (err) {
			dev_err(&pdev->dev,
				"Failed to request GPIO (%s: %d).\n",
				pdata->gpios[i].name, gpio);
			continue;
		}

		if (pdata->gpios[i].direction == GPIOF_DIR_OUT)
			err = gpio_direction_output(gpio,
				pdata->gpios[i].value);
		else
			err = gpio_direction_input(gpio);

		if (err) {
			dev_err(&pdev->dev,
				"Failed to set GPIO (%s: %d) as %s.\n",
				pdata->gpios[i].name, gpio,
				(pdata->gpios[i].direction == GPIOF_DIR_OUT) ?
				"output" : "input");

				gpio_free(gpio);
				continue;
			}


		err = gpio_export(gpio, 1);
		if (err) {
			dev_err(&pdev->dev,
				"Failed to export GPIO (%s: %d)\n",
				pdata->gpios[i].name, gpio);

			gpio_free(gpio);
			continue;
		}

		err = gpio_export_link(&pdev->dev, pdata->gpios[i].name, gpio);
		if (err) {
			dev_err(&pdev->dev,
				"Failed to export GPIO (%s: %d).\n",
				pdata->gpios[i].name, gpio);

			gpio_unexport(gpio);
			gpio_free(gpio);
			continue;
		}

		/* got here successfully so save and up count of gpios */
		info->gpios[gpio_count] = gpio;
		gpio_count++;
	}

	if (gpio_count > 0) {
		info->num_gpios = pdata->num_gpios;
		platform_set_drvdata(pdev, info);
		err = 0;
	} else {
		kfree(info);
		err = -1;
	}

	return err;
}

static int __devexit mmi_factory_remove(struct platform_device *pdev)
{
	int i;
	struct mmi_factory_info *info = platform_get_drvdata(pdev);

	if (info) {
		for (i = 0; i < info->num_gpios; i++) {
			gpio_unexport(info->gpios[i]);
			gpio_free(info->gpios[i]);
		}

		kfree(info);
	}

	return 0;
}

static struct platform_driver mmi_factory_driver = {
	.probe = mmi_factory_probe,
	.remove = mmi_factory_remove,
	.driver = {
		.name = "mmi_factory",
		.owner = THIS_MODULE,
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
MODULE_AUTHOR("Jim Wylder <jwylder1@motorola.com>");
MODULE_DESCRIPTION("Motorola Mobility Factory Specific Controls");
MODULE_LICENSE("GPL");
