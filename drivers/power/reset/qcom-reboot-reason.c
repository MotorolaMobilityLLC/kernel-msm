// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>

#define RESET_EXTRA_SW_BOOT_REASON     BIT(7)
#define RESET_EXTRA_PANIC_REASON       BIT(3)
#define RESET_EXTRA_REBOOT_BL_REASON   BIT(2)
#define RESET_EXTRA_SW_REBOOT_REASON   BIT(0)

struct qcom_reboot_reason {
	struct device *dev;
	struct notifier_block reboot_nb;
	struct nvmem_cell *nvmem_cell;
	struct notifier_block panic_nb;
	struct nvmem_cell *nvmem_oem_cell;
};

struct poweroff_reason {
	const char *cmd;
	unsigned char pon_reason;
};

static struct poweroff_reason reasons[] = {
	{ "recovery",			0x01 },
	{ "bootloader",			0x02 },
	{ "rtc",			0x03 },
	{ "dm-verity device corrupted",	0x04 },
	{ "dm-verity enforcing",	0x05 },
	{ "keys clear",			0x06 },
	{}
};

static struct poweroff_reason extra_reasons[] = {
	{ "bootloader",		RESET_EXTRA_REBOOT_BL_REASON},
	{}
};

static int moto_reboot_reason_panic(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, panic_nb);
	unsigned char val = RESET_EXTRA_PANIC_REASON;

	nvmem_cell_write(reboot->nvmem_oem_cell, &val,
			sizeof(val));

	return NOTIFY_OK;
}

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);
	struct poweroff_reason *reason;
	unsigned char val = RESET_EXTRA_SW_REBOOT_REASON;

	nvmem_cell_write(reboot->nvmem_oem_cell, &val,
			sizeof(val));

	if (!cmd)
		return NOTIFY_OK;
	for (reason = reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
			nvmem_cell_write(reboot->nvmem_cell,
					 &reason->pon_reason,
					 sizeof(reason->pon_reason));
			break;
		}
	}

	for (reason = extra_reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
			nvmem_cell_write(reboot->nvmem_oem_cell,
					 &reason->pon_reason,
					 sizeof(reason->pon_reason));
			break;
		}
	}

	return NOTIFY_OK;
}

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;
	unsigned char val = RESET_EXTRA_SW_BOOT_REASON;
	int ret;

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");

	if (IS_ERR(reboot->nvmem_cell))
		return PTR_ERR(reboot->nvmem_cell);

	reboot->nvmem_oem_cell = nvmem_cell_get(reboot->dev, "extra_restart_reason");

	if (IS_ERR(reboot->nvmem_oem_cell))
		return PTR_ERR(reboot->nvmem_oem_cell);

	reboot->panic_nb.notifier_call = moto_reboot_reason_panic;
	reboot->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &reboot->panic_nb);

	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

	platform_set_drvdata(pdev, reboot);

	ret = nvmem_cell_write(reboot->nvmem_oem_cell, &val, sizeof(val));
	pr_err("update sw boot flag, ret = %d\n", ret);

	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &reboot->panic_nb);
	unregister_reboot_notifier(&reboot->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_reboot_reason_match[] = {
	{ .compatible = "qcom,reboot-reason", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_reboot_reason_match);

static struct platform_driver qcom_reboot_reason_driver = {
	.probe = qcom_reboot_reason_probe,
	.remove = qcom_reboot_reason_remove,
	.driver = {
		.name = "qcom-reboot-reason",
		.of_match_table = of_match_ptr(of_qcom_reboot_reason_match),
	},
};

module_platform_driver(qcom_reboot_reason_driver);

MODULE_INFO(depends, "nvmem_qcom_spmi_sdam,spmi_pmic_arb");
MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL v2");
