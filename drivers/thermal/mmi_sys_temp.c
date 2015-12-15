/*
 * Copyright (C) 2015 Motorola Mobility LLC
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
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mmi_sys_temp.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/uaccess.h>

#define TEMP_NODE_SENSOR_NAMES "mmi,temperature-names"
#define DEFAULT_TEMPERATURE 0

struct mmi_sys_temp_sensor {
	struct thermal_zone_device *tz_dev;
	const char *name;
	unsigned long temp;
};

struct mmi_sys_temp_dev {
	struct platform_device *pdev;
	int num_sensors;
	struct mmi_sys_temp_sensor sensor[0];
};

static struct mmi_sys_temp_dev *sys_temp_dev;

static int mmi_sys_temp_ioctl_open(struct inode *node, struct file *file)
{
	return 0;
}

static int mmi_sys_temp_ioctl_release(struct inode *node, struct file *file)
{
	return 0;
}

static long mmi_sys_temp_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	int i;
	int ret = 0;
	struct mmi_sys_temp_ioctl request;

	if (!sys_temp_dev)
		return -EINVAL;

	if ((_IOC_TYPE(cmd) != MMI_SYS_TEMP_MAGIC_NUM) ||
	    (_IOC_NR(cmd) >= MMI_SYS_TEMP_MAX_NUM)) {
		return -ENOTTY;
	}

	switch (cmd) {
	case MMI_SYS_TEMP_SET_TEMP:
		ret = copy_from_user(&request, (void __user *)arg,
				     sizeof(struct mmi_sys_temp_ioctl));
		if (ret) {
			dev_err(&sys_temp_dev->pdev->dev,
				"failed to copy_from_user\n");
			return -EACCES;
		}

		dev_dbg(&sys_temp_dev->pdev->dev, "name=%s, temperature=%d\n",
			request.name, request.temperature);

		for (i = 0; i < sys_temp_dev->num_sensors; i++) {
			if (!strncasecmp(sys_temp_dev->sensor[i].name,
					 request.name,
					 MMI_SYS_TEMP_NAME_LENGTH)) {
				sys_temp_dev->sensor[i].temp =
					request.temperature;
				break;
			}
		}
		if (i >= sys_temp_dev->num_sensors) {
			dev_dbg(&sys_temp_dev->pdev->dev,
				"name %s not supported\n",
			request.name);
			ret = -EBADR;
		}
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long mmi_sys_temp_compat_ioctl(struct file *file,
				      unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return mmi_sys_temp_ioctl(file, cmd, arg);
}
#endif	/* CONFIG_COMPAT */

static const struct file_operations mmi_sys_temp_fops = {
	.owner = THIS_MODULE,
	.open = mmi_sys_temp_ioctl_open,
	.unlocked_ioctl = mmi_sys_temp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmi_sys_temp_compat_ioctl,
#endif  /* CONFIG_COMPAT */
	.release = mmi_sys_temp_ioctl_release,
};

static struct miscdevice mmi_sys_temp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mmi_sys_temp",
	.fops = &mmi_sys_temp_fops,
};

static int mmi_sys_temp_get(struct thermal_zone_device *thermal,
			    unsigned long *temp)
{
	struct mmi_sys_temp_sensor *sensor = thermal->devdata;

	if (!sensor)
		return -EINVAL;

	*temp = sensor->temp;
	return 0;
}

static struct thermal_zone_device_ops mmi_sys_temp_ops = {
	.get_temp = mmi_sys_temp_get,
};

static int mmi_sys_temp_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct device_node *node;
	int num_sensors;
	int num_registered = 0;

	if (!pdev)
		return -ENODEV;

	node = pdev->dev.of_node;
	if (!node) {
		dev_err(&pdev->dev, "bad of_node\n");
		return -ENODEV;
	}

	num_sensors = of_property_count_strings(node, TEMP_NODE_SENSOR_NAMES);
	if (num_sensors <= 0) {
		dev_err(&pdev->dev,
			"bad number of sensors: %d\n", num_sensors);
		return -EINVAL;
	}

	sys_temp_dev = devm_kzalloc(&pdev->dev, sizeof(struct mmi_sys_temp_dev)
				    + (num_sensors *
				       sizeof(struct mmi_sys_temp_sensor)),
				    GFP_KERNEL);
	if (!sys_temp_dev) {
		dev_err(&pdev->dev,
			"Unable to alloc memory for sys_temp_dev\n");
		return -ENOMEM;
	}

	sys_temp_dev->pdev = pdev;
	sys_temp_dev->num_sensors = num_sensors;

	for (i = 0; i < num_sensors; i++) {
		ret = of_property_read_string_index(node,
						TEMP_NODE_SENSOR_NAMES, i,
						&sys_temp_dev->sensor[i].name);
		if (ret) {
			dev_err(&pdev->dev, "Unable to read of_prop string\n");
			goto err_thermal_unreg;
		}

		sys_temp_dev->sensor[i].temp = DEFAULT_TEMPERATURE;
		sys_temp_dev->sensor[i].tz_dev =
		   thermal_zone_device_register(sys_temp_dev->sensor[i].name,
						0, 0,
						&sys_temp_dev->sensor[i],
						&mmi_sys_temp_ops,
						NULL, 0, 0);
		if (IS_ERR(sys_temp_dev->sensor[i].tz_dev)) {
			dev_err(&pdev->dev,
				"thermal_zone_device_register() failed.\n");
			ret = -ENODEV;
			goto err_thermal_unreg;
		}
		num_registered = i + 1;
	}

	platform_set_drvdata(pdev, sys_temp_dev);

	ret = misc_register(&mmi_sys_temp_misc);
	if (ret) {
		dev_err(&pdev->dev, "Error registering device %d\n", ret);
		goto err_thermal_unreg;
	}

	return 0;

err_thermal_unreg:
	for (i = 0; i < num_registered; i++)
		thermal_zone_device_unregister(sys_temp_dev->sensor[i].tz_dev);

	devm_kfree(&pdev->dev, sys_temp_dev);
	return ret;
}

static int mmi_sys_temp_remove(struct platform_device *pdev)
{
	int i;
	struct mmi_sys_temp_dev *dev =  platform_get_drvdata(pdev);

	for (i = 0; i < dev->num_sensors; i++)
		thermal_zone_device_unregister(dev->sensor[i].tz_dev);

	misc_deregister(&mmi_sys_temp_misc);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, sys_temp_dev);
	return 0;
}

static const struct of_device_id mmi_sys_temp_match_table[] = {
	{.compatible = "mmi,sys-temp"},
	{},
};
MODULE_DEVICE_TABLE(of, mmi_sys_temp_match_table);

static struct platform_driver mmi_sys_temp_driver = {
	.probe = mmi_sys_temp_probe,
	.remove = mmi_sys_temp_remove,
	.driver = {
		.name = "mmi_sys_temp",
		.owner = THIS_MODULE,
		.of_match_table = mmi_sys_temp_match_table,
	},
};

static int __init mmi_sys_temp_init(void)
{
	return platform_driver_register(&mmi_sys_temp_driver);
}

static void __exit mmi_sys_temp_exit(void)
{
	platform_driver_unregister(&mmi_sys_temp_driver);
}

module_init(mmi_sys_temp_init);
module_exit(mmi_sys_temp_exit);

MODULE_ALIAS("platform:mmi_sys_temp");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility System Temperatures");
MODULE_LICENSE("GPL");
