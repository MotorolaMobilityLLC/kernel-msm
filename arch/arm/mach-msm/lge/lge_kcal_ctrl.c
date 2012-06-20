/* arch/arm/mach-msm/lge/lge_kcal_ctrl.c
 *
 * Interface to calibrate display color temperature.
 *
 * Copyright (C) 2012 LGE
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <mach/board_lge.h>

static struct kcal_platform_data *kcal_pdata;
static int last_status_kcal_ctrl;

static ssize_t kcal_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int kcal_r = 0;
	int kcal_g = 0;
	int kcal_b = 0;

	if (!count)
		return -EINVAL;

	sscanf(buf, "%d %d %d", &kcal_r, &kcal_g, &kcal_b);
	kcal_pdata->set_values(kcal_r, kcal_g, kcal_b);
	return count;
}

static ssize_t kcal_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	int kcal_r = 0;
	int kcal_g = 0;
	int kcal_b = 0;

	kcal_pdata->get_values(&kcal_r, &kcal_g, &kcal_b);

	return sprintf(buf, "%d %d %d\n", kcal_r, kcal_g, kcal_b);
}

static ssize_t kcal_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = 0;
	if (!count)
		return last_status_kcal_ctrl = -EINVAL;

	sscanf(buf, "%d", &cmd);

	if(cmd != 1)
		return last_status_kcal_ctrl = -EINVAL;

	last_status_kcal_ctrl = kcal_pdata->refresh_display();

	if(last_status_kcal_ctrl)
		return -EINVAL;
	else
		return count;
}

static ssize_t kcal_ctrl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if(last_status_kcal_ctrl)
		return sprintf(buf, "NG\n");
	else
		return sprintf(buf, "OK\n");
}

static DEVICE_ATTR(kcal, 0644, kcal_show, kcal_store);
static DEVICE_ATTR(kcal_ctrl, 0644, kcal_ctrl_show, kcal_ctrl_store);

static int kcal_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0;
	kcal_pdata = pdev->dev.platform_data;

	if(!kcal_pdata->set_values || !kcal_pdata->get_values ||
					!kcal_pdata->refresh_display) {
		printk(KERN_ERR "kcal function not registered\n");
		return -1;
	}

	rc = device_create_file(&pdev->dev, &dev_attr_kcal);
	if(rc !=0)
		return -1;
	rc = device_create_file(&pdev->dev, &dev_attr_kcal_ctrl);
	if(rc !=0)
		return -1;

	return 0;
}
static struct platform_driver this_driver = {
	.probe  = kcal_ctrl_probe,
	.driver = {
		.name   = "kcal_ctrl",
	},
};

int __init kcal_ctrl_init(void)
{
	printk(KERN_INFO "kcal_ctrl_init \n");
	return platform_driver_register(&this_driver);
}

device_initcall(kcal_ctrl_init);

MODULE_DESCRIPTION("LGE KCAL driver");
MODULE_LICENSE("GPL v2");
