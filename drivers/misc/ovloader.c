/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/limits.h>

struct platform_device *pdev;

static int load_overlay(const char *dtbo_name)
{
	const struct firmware *fw;
	struct device_node *overlay;
	struct of_overlay_info *ovinfo;
	int ovinfo_cnt;
	int ret;

	ret = request_firmware(&fw, dtbo_name, &pdev->dev);

	if (ret) {
		pr_err("ovloader: request_firmware on %s failed: %d\n",
		       dtbo_name, ret);
		return ret;
	}

	of_fdt_unflatten_tree((void *)fw->data, &overlay);

	if (!overlay) {
		pr_err("ovloader: unflatten tree failed\n");
		return -EINVAL;
	}

	of_node_set_flag(overlay, OF_DETACHED);

	ret = of_resolve(overlay);
	if (ret) {
		pr_err("ovloader: of_resolve failed: %d\n", ret);
		return ret;
	}

	ret = of_build_overlay_info(overlay, &ovinfo_cnt, &ovinfo);

	if (ret) {
		pr_err("ovloader: of_build_overlay_info failed: %d\n", ret);
		return ret;
	}

	ret = of_overlay(ovinfo_cnt, ovinfo);
	if (ret) {
		pr_err("ovloader: of_overlay failed: %d\n", ret);
		return ret;
	}

	pr_info("ovloader: successfully loaded overlay %s\n", dtbo_name);

	return ret;
}

static ssize_t ovloader_set_load(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	int ret;
	char *filename;
	char *newline;

	if (size < 1 || size > NAME_MAX + 1)
		return -EINVAL;

	filename = kstrndup(buf, size, GFP_KERNEL);

	if (!filename)
		return -ENOMEM;

	/* Drop trailing newline introduced by userspace */
	newline = strrchr(filename, '\n');
	if (newline)
		*newline = '\0';

	ret = load_overlay(filename);

	kfree(filename);

	if (ret)
		return ret;

	return size;
}

static DEVICE_ATTR(load, S_IWUSR, NULL, ovloader_set_load);

static int __init ovloader_init(void)
{
	int ret;

	pdev = platform_device_alloc("ovloader", 0);

	if (!pdev) {
		pr_err("Failed to allocate ovloader platform device\n");
		goto out;
	}

	ret = platform_device_add(pdev);

	if (ret) {
		pr_err("Failed to add ovloader platform device: %d\n", ret);
		goto out;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_load);

	if (ret) {
		pr_err("Failed to create ovloader sysfs attribute: %d\n", ret);
		goto out_device_free;
	}

	return 0;

out_device_free:
	platform_device_put(pdev);
out:
	return -ENODEV;
}

module_init(ovloader_init);
