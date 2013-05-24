/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/peripheral-loader.h>
#include <mach/qdsp6v2/apr.h>
#include <linux/sysfs.h>

#define Q6_PIL_GET_DELAY_MS 100
#define BOOT_CMD 1


static ssize_t adsp_boot_store(struct kobject *kobj,\
	struct kobj_attribute *attr,\
	const char *buf, size_t count);

struct adsp_loader_private {
	void *pil_h;
	struct kobject *boot_adsp_obj;
	struct attribute_group *attr_group;
};

static struct kobj_attribute adsp_boot_attribute =
	__ATTR(boot, 0220, NULL, adsp_boot_store);
static struct attribute *attrs[] = {
	&adsp_boot_attribute.attr,
	NULL,
};

struct platform_device *adsp_private;

static void adsp_loader_do(struct platform_device *pdev)
{
	struct adsp_loader_private *priv = NULL;

	if (pdev) {
		priv = platform_get_drvdata(pdev);
	} else {
		pr_err("%s: Private data get failed\n", __func__);
		goto fail;
	}

	priv->pil_h = pil_get("q6");
	if (IS_ERR(priv->pil_h)) {
		pr_err("%s: pil get failed\n", __func__);
		goto fail;
	}

	msleep(Q6_PIL_GET_DELAY_MS);

	/* Set the state of the ADSP in APR driver */
	apr_set_q6_state(APR_SUBSYS_LOADED);

	pr_info("%s: Q6/ADSP image is loaded\n", __func__);
	return;
fail:
	pr_err("%s: Q6/ADSP image loading failed\n", __func__);
	return;
}


static ssize_t adsp_boot_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int boot = 0;
	sscanf(buf, "%du", &boot);
	if (boot == BOOT_CMD) {
		pr_info("%s:going to call adsp_loader_do", __func__);
		adsp_loader_do(adsp_private);
	}
	return count;
}

static int adsp_loader_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct adsp_loader_private *priv = NULL;
	adsp_private = NULL;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pr_err("memory alloc failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);

	priv->pil_h = NULL;
	priv->boot_adsp_obj = NULL;
	priv->attr_group = devm_kzalloc(&pdev->dev,
				sizeof(*(priv->attr_group)),
				GFP_KERNEL);
	if (!priv->attr_group) {
		pr_err("%s: malloc attr_group failed\n",
			__func__);
		goto error_return;
	}

	priv->attr_group->attrs = attrs;

	priv->boot_adsp_obj = kobject_create_and_add("boot_adsp", kernel_kobj);
	if (!priv->boot_adsp_obj) {
		pr_err("%s: sysfs create and add failed\n",
			__func__);
		goto error_return;
	}

	ret = sysfs_create_group(priv->boot_adsp_obj, priv->attr_group);
	if (ret) {
		pr_err("%s: sysfs create group failed %d\n", \
			__func__, ret);
		goto error_return;
	}

	adsp_private = pdev;

	return 0;
error_return:
	if (priv->attr_group) {
		devm_kfree(&pdev->dev, priv->attr_group);
		priv->attr_group = NULL;
	}

	if (priv->boot_adsp_obj) {
		kobject_del(priv->boot_adsp_obj);
		priv->boot_adsp_obj = NULL;
	}
	return ret;
}

static int adsp_loader_probe(struct platform_device *pdev)
{
	int ret = adsp_loader_init_sysfs(pdev);
	if (ret != 0) {
		pr_err("Error in initing sysfs\n");
		return ret;
	}

	return 0;
}

static int adsp_loader_remove(struct platform_device *pdev)
{
	struct adsp_loader_private *priv = NULL;
	priv = platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	if (priv->pil_h) {
		pil_put(priv->pil_h);
		priv->pil_h = NULL;
	}

	if (priv->boot_adsp_obj) {
		sysfs_remove_group(priv->boot_adsp_obj, priv->attr_group);
		kobject_del(priv->boot_adsp_obj);
		priv->boot_adsp_obj = NULL;
	}

	if (priv->attr_group) {
		devm_kfree(&pdev->dev, priv->attr_group);
		priv->attr_group = NULL;
	}

	devm_kfree(&pdev->dev, priv);
	adsp_private = NULL;

	return 0;
}


static const struct of_device_id adsp_loader_dt_match[] = {
	{ .compatible = "qcom,adsp-loader" },
	{ }
};
MODULE_DEVICE_TABLE(of, adsp_loader_dt_match);

static struct platform_driver adsp_loader_driver = {
	.driver = {
		.name = "adsp-loader",
		.owner = THIS_MODULE,
	},
	.probe = adsp_loader_probe,
	.remove = __devexit_p(adsp_loader_remove),
};

static int __init adsp_loader_init(void)
{
	return platform_driver_register(&adsp_loader_driver);
}
module_init(adsp_loader_init);

static void __exit adsp_loader_exit(void)
{
	platform_driver_unregister(&adsp_loader_driver);
}
module_exit(adsp_loader_exit);

MODULE_DESCRIPTION("ADSP Loader module");
MODULE_LICENSE("GPL v2");
