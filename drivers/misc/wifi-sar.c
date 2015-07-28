/*
 * Copyright (C) 2015 HUAWEI DEVICE
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

static BLOCKING_NOTIFIER_HEAD(wifi_sar_notifier_list);

static int wifi_tx_power_limit = -1;

static int notify_wifi_sar_change(const char *val,
				const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	if (wifi_tx_power_limit)
	{
		pr_info("%s: set wifi tx power limit = %d\n", __func__, wifi_tx_power_limit);
		blocking_notifier_call_chain(&wifi_sar_notifier_list,
			0, &wifi_tx_power_limit);
	}

	return 0;
}

static struct kernel_param_ops wifi_sar_ops = {
	.set = notify_wifi_sar_change,
};

module_param_cb(wifi_tx_power_limit, &wifi_sar_ops, &wifi_tx_power_limit, 0220);

int register_notifier_by_sar(struct notifier_block *nb)
{
	int ret;

	ret = blocking_notifier_chain_register(
		&wifi_sar_notifier_list, nb);
	if (ret == 0 && wifi_tx_power_limit > 0)
	{
		blocking_notifier_call_chain(
				&wifi_sar_notifier_list,
				0, &wifi_tx_power_limit);
	}
	return ret;
}

int unregister_notifier_by_sar(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
			&wifi_sar_notifier_list, nb);
}

static int __init wifi_sar_module_init(void)
{
	pr_info("wifi sar module init;\n");
	return 0;
}

static void __exit wifi_sar_module_exit(void)
{
	pr_info("wifi sar module exit\n");
}

module_init(wifi_sar_module_init);
module_exit(wifi_sar_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Wifi sar control driver");
