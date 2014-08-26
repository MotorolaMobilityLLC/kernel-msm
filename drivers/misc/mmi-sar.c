/*
 * Copyright (C) 2014 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>

static BLOCKING_NOTIFIER_HEAD(sar_wifi_notifier_list);

static int wifi_max_tx_power = -1;

int register_notifier_by_sar(struct notifier_block *nb)
{
	int ret;
	ret = blocking_notifier_chain_register(
		&sar_wifi_notifier_list, nb);
	if (ret == 0 && wifi_max_tx_power > 0) {
		blocking_notifier_call_chain(
				&sar_wifi_notifier_list,
				0, &wifi_max_tx_power);
	}
	return ret;
}

int unregister_notifier_by_sar(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
			&sar_wifi_notifier_list, nb);
}

static ssize_t store_sar_wifi(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &wifi_max_tx_power);
	pr_info("%s: set wifi tx power limit = %d\n", __func__, wifi_max_tx_power);
	blocking_notifier_call_chain(&sar_wifi_notifier_list,
			0, &wifi_max_tx_power);
	return count;
}

static struct kobj_attribute mmi_sar_wifi =
	__ATTR(sar_wifi, 0220, (void *)NULL, (void *)store_sar_wifi);

static struct attribute *mmi_sar_attrs[] = {
	&mmi_sar_wifi.attr,
	NULL,
};

static struct attribute_group mmi_sar_attr_grp = {
	.attrs = mmi_sar_attrs,
};

static int mmi_sar_probe(struct platform_device *pdev)
{
	int ret;
	pr_debug("%s\n", __func__);
	ret = sysfs_create_group(&pdev->dev.kobj, &mmi_sar_attr_grp);
	return ret;
}

static int mmi_sar_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	sysfs_remove_group(&pdev->dev.kobj, &mmi_sar_attr_grp);
	return 0;
}

static const struct of_device_id mmi_sar_ctrl_match[] = {
	{ .compatible = "mmi,mmi_sar_ctrl", },
	{}
};
MODULE_DEVICE_TABLE(of, mmi_sar_ctrl_match);

static struct platform_driver mmi_sar_plat_driver = {
	.probe = mmi_sar_probe,
	.remove = mmi_sar_remove,
	.driver = {
		.name = "mmi_sar_ctrl",
		.owner = THIS_MODULE,
		.of_match_table = mmi_sar_ctrl_match,
	},
};

module_platform_driver(mmi_sar_plat_driver);

MODULE_ALIAS("platform:mmi_sar");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Motorola Mobility SAR Controls");
MODULE_LICENSE("GPL");

