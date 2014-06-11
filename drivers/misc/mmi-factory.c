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
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/slab.h>


enum mmi_factory_device_list {
	HONEYFUFU = 0,
	KUNGPOW,
};

#define KP_KILL_INDEX 0
#define KP_CABLE_INDEX 1
#define KP_WARN_INDEX 2
#define KP_NUM_GPIOS 3

struct mmi_factory_info {
	int num_gpios;
	struct gpio *list;
	int factory_cable;
	enum mmi_factory_device_list dev;
	struct delayed_work warn_irq_work;
	struct delayed_work fac_cbl_irq_work;
	int warn_irq;
	int fac_cbl_irq;
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

static void warn_irq_w(struct work_struct *w)
{
	struct mmi_factory_info *info = container_of(w,
						     struct mmi_factory_info,
						     warn_irq_work.work);
	int warn_line = gpio_get_value(info->list[KP_WARN_INDEX].gpio);

	if (!warn_line) {
		pr_info("HW User Reset!\n");
		pr_info("2 sec to Reset.\n");
		kernel_restart(NULL);
		return;
	}
}

#define WARN_IRQ_DELAY	5 /* 5msec */
static irqreturn_t warn_irq_handler(int irq, void *data)
{
	struct mmi_factory_info *info = data;

	/*schedule delayed work for 5msec for line state to settle*/
	queue_delayed_work(system_nrt_wq, &info->warn_irq_work,
			   msecs_to_jiffies(WARN_IRQ_DELAY));

	return IRQ_HANDLED;
}

static void fac_cbl_irq_w(struct work_struct *w)
{
	struct mmi_factory_info *info = container_of(w,
						     struct mmi_factory_info,
						     fac_cbl_irq_work.work);
	int fac_cbl_line = gpio_get_value(info->list[KP_CABLE_INDEX].gpio);
	int fac_cbl_kill_line = gpio_get_value(info->list[KP_KILL_INDEX].gpio);

	if (fac_cbl_line) {
		pr_info("Factory Cable Attached!\n");
		info->factory_cable = 1;
	} else
		if (info->factory_cable) {
			pr_info("Factory Cable Detached!\n");
			if (fac_cbl_kill_line) {
				info->factory_cable = 0;
				pr_info("Factory Kill Disabled!\n");
			} else {
				pr_info("2 sec to power off.\n");
				kernel_power_off();
				return;
			}
		}
}

#define FAC_CBL_IRQ_DELAY	5 /* 5msec */
static irqreturn_t fac_cbl_irq_handler(int irq, void *data)
{
	struct mmi_factory_info *info = data;

	/*schedule delayed work for 5msec for line state to settle*/
	queue_delayed_work(system_nrt_wq, &info->fac_cbl_irq_work,
			   msecs_to_jiffies(FAC_CBL_IRQ_DELAY));

	return IRQ_HANDLED;
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

static enum mmi_factory_device_list hff_dev = HONEYFUFU;
static enum mmi_factory_device_list kp_dev = KUNGPOW;

static const struct of_device_id mmi_factory_of_tbl[] = {
	{ .compatible = "mmi,factory-support-msm8960", .data = &hff_dev},
	{ .compatible = "mmi,factory-support-kungpow", .data = &kp_dev},
	{},
};
MODULE_DEVICE_TABLE(of, mmi_factory_of_tbl);

static int mmi_factory_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct mmi_factory_info *info;
	int ret;
	int i;

	match = of_match_device(mmi_factory_of_tbl, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "No Match found\n");
		return -ENODEV;
	}

	if (match && match->compatible)
		dev_info(&pdev->dev, "Using %s\n", match->compatible);

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

	if (!mmi_factory_cable_present()) {
		dev_dbg(&pdev->dev, "factory cable not present\n");
	} else {
		pr_info("Factory Cable Attached at Power up!\n");
		info->factory_cable = 1;
	}

	if (match && match->data) {
		info->dev = *(enum mmi_factory_device_list *)(match->data);
	} else {
		dev_err(&pdev->dev, "failed to find device match\n");
		goto fail;
	}

	if ((info->dev == KUNGPOW) && (info->num_gpios == KP_NUM_GPIOS)) {
		/* Disable Kill if not powered up by a factory cable */
		if (!info->factory_cable)
			gpio_direction_output(info->list[KP_KILL_INDEX].gpio,
						1);

		info->warn_irq = gpio_to_irq(info->list[KP_WARN_INDEX].gpio);
		info->fac_cbl_irq =
			gpio_to_irq(info->list[KP_CABLE_INDEX].gpio);

		INIT_DELAYED_WORK(&info->warn_irq_work, warn_irq_w);
		INIT_DELAYED_WORK(&info->fac_cbl_irq_work, fac_cbl_irq_w);

		if (info->warn_irq) {
			ret = request_irq(info->warn_irq,
					  warn_irq_handler,
					  IRQF_TRIGGER_FALLING,
					  "mmi_factory_warn", info);
			if (ret) {
				dev_err(&pdev->dev,
					"request irq failed for Warn\n");
				goto fail;
			}
		} else {
			ret = -ENODEV;
			dev_err(&pdev->dev, "IRQ for Warn doesn't exist\n");
			goto fail;
		}

		if (info->fac_cbl_irq) {
			ret = request_irq(info->fac_cbl_irq,
					  fac_cbl_irq_handler,
					  IRQF_TRIGGER_RISING |
					  IRQF_TRIGGER_FALLING,
					  "mmi_factory_fac_cbl", info);
			if (ret) {
				dev_err(&pdev->dev,
					"irq failed for Factory Cable\n");
				goto remove_warn;
			}
		} else {
			ret = -ENODEV;
			dev_err(&pdev->dev,
				"IRQ for Factory Cable doesn't exist\n");
			goto remove_warn;
		}
	}

	platform_set_drvdata(pdev, info);

	return 0;

remove_warn:
	free_irq(info->warn_irq, info);
fail:
	gpio_free_array(info->list, info->num_gpios);
	return ret;
}

static int mmi_factory_remove(struct platform_device *pdev)
{
	struct mmi_factory_info *info = platform_get_drvdata(pdev);

	if (info) {
		gpio_free_array(info->list, info->num_gpios);

		cancel_delayed_work_sync(&info->warn_irq_work);
		cancel_delayed_work_sync(&info->fac_cbl_irq_work);
		if (info->fac_cbl_irq)
			free_irq(info->fac_cbl_irq, info);
		if (info->warn_irq)
			free_irq(info->warn_irq, info);
	}

	return 0;
}

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
