// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/smci_object.h>
#include <linux/of_platform.h>
#include <linux/mod_devicetable.h>

static struct smci_drv_ops smci_fun_ops = {0};

int32_t provide_smci_kernel_fun_ops(const struct smci_drv_ops *ops)
{
	if (!ops) {
		pr_err("ops is NULL\n");
		return -EINVAL;
	}
	smci_fun_ops = *ops;
	pr_debug("SMCI proxy Ready to be served\n");
	return 0;
}
EXPORT_SYMBOL_GPL(provide_smci_kernel_fun_ops);

int32_t smci_get_client_env_object(struct smci_object *client_env_obj)
{
	int32_t ret = -EAGAIN;

	/* If SMCI driver is up an environment object will be acquired */
	if (smci_fun_ops.smci_get_client_env_object) {
		ret = smci_fun_ops.smci_get_client_env_object(client_env_obj);
		if (ret != 0)
			pr_err("%s:  Unable to get clientEnv object\n", __func__);
	} else {
		pr_err_ratelimited("SMCI driver is not up yet\n");
	}
	return ret;
}
EXPORT_SYMBOL_GPL(smci_get_client_env_object);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SMCI proxy driver");
