// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019, 2021 The Linux Foundation. All rights reserved.
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
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
#include <linux/panic_notifier.h>
#include <linux/input/qpnp-power-on.h>

#define RESET_EXTRA_SW_BOOT_REASON     BIT(7)
#define RESET_EXTRA_SW_REBOOT_REASON   BIT(0)

static void *restart_reason;
static int in_panic;

static bool force_warm_reboot;
#endif

struct qcom_reboot_reason {
	struct device *dev;
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	int reboot_notify_status;
#endif
	struct notifier_block reboot_nb;
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	struct notifier_block restart_nb;
#endif
	struct nvmem_cell *nvmem_cell;
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	struct notifier_block panic_nb;
#endif
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

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
static int moto_reboot_reason_panic(struct notifier_block *this,
		unsigned long event, void *ptr)
{
#if IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
	int ret = 0;
	in_panic = 1;
	__raw_writel(0x77665505, restart_reason);
	ret = qpnp_pon_store_extra_reset_info(RESET_EXTRA_PANIC_REASON,
            RESET_EXTRA_PANIC_REASON);
#endif
	return NOTIFY_OK;
}
#endif

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
#if IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
	u8 extra_reason = PON_RESTART_REASON_UNKNOWN;
#endif
	int ret = 0;
#endif

	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);
	struct poweroff_reason *reason;

	if (!cmd) {
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT) && IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
		pr_info("%s: cmd is NULL\n", __func__);
		__raw_writel(0x77665501, restart_reason);
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
#endif
		return NOTIFY_OK;
	}
	pr_info("%s: cmd is %s\n", __func__, cmd);

	for (reason = reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
			nvmem_cell_write(reboot->nvmem_cell,
					 &reason->pon_reason,
					 sizeof(reason->pon_reason));
			break;
		}
	}

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT) && IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
	if (force_warm_reboot)
		pr_info("Forcing a warm reset of the system\n");

	/* Hard reset the PMIC unless memory contents must be maintained. */
	if (force_warm_reboot || in_panic)
		qpnp_pon_system_pwr_off(PON_POWER_OFF_WARM_RESET);
	else
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
	if (!strncmp(cmd, "bootloader", 10)) {
		/* set reboot_bl flag in PMIC for cold reset */
		extra_reason = PON_RESTART_REASON_BOOTLOADER;
		__raw_writel(0x77665500, restart_reason);
		ret = qpnp_pon_store_extra_reset_info(RESET_EXTRA_REBOOT_BL_REASON,
				RESET_EXTRA_REBOOT_BL_REASON);
		/*
		* force cold reboot here to avoid unexpected
		* warm boot from bootloader.
		*/
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
	} else if (!strncmp(cmd, "recovery", 8)) {
		extra_reason = PON_RESTART_REASON_RECOVERY;
		__raw_writel(0x77665502, restart_reason);
	} else if (!strcmp(cmd, "rtc")) {
		extra_reason = PON_RESTART_REASON_RTC;
		__raw_writel(0x77665503, restart_reason);
	} else if (!strcmp(cmd, "dm-verity device corrupted")) {
		extra_reason = PON_RESTART_REASON_DMVERITY_CORRUPTED;
		__raw_writel(0x77665508, restart_reason);
	} else if (!strcmp(cmd, "dm-verity enforcing")) {
		extra_reason = PON_RESTART_REASON_DMVERITY_ENFORCE;
		__raw_writel(0x77665509, restart_reason);
	} else if (!strcmp(cmd, "keys clear")) {
		extra_reason = PON_RESTART_REASON_KEYS_CLEAR;
		__raw_writel(0x7766550a, restart_reason);
	} else if (!strncmp(cmd, "oem-", 4)) {
		unsigned long code;
		int ret;

		ret = kstrtoul(cmd + 4, 16, &code);
		if (!ret)
			__raw_writel(0x6f656d00 | (code & 0xff),
				     restart_reason);
	} else if (!strncmp(cmd, "post-wdt", 8)) {
		/* set  flag in PMIC to nofity BL post watchdog reboot */
		ret = qpnp_pon_store_extra_reset_info(RESET_EXTRA_POST_REBOOT_MASK,
		   RESET_EXTRA_POST_WDT_REASON);
		/* force cold reboot */
		ret = qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
	} else if (!strncmp(cmd, "post-pmicwdt", 12)) {
		/* set  flag in PMIC to nofity BL post pmic watchdog reboot */
		qpnp_pon_store_extra_reset_info(RESET_EXTRA_POST_REBOOT_MASK,
		   RESET_EXTRA_POST_PMICWDT_REASON);
		/* force cold reboot */
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
	} else if (!strncmp(cmd, "post-panic", 10)) {
		/* set  flag in PMIC to nofity BL post panic reboot */
		qpnp_pon_store_extra_reset_info(RESET_EXTRA_POST_REBOOT_MASK,
		   RESET_EXTRA_POST_PANIC_REASON);
		/* force cold reboot */
		qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
	} else {
	        __raw_writel(0x77665501, restart_reason);
        }
        qpnp_pon_set_restart_reason(
		(enum pon_restart_reason)extra_reason);

	if (in_panic == 1) {
		__raw_writel(0x77665505, restart_reason);
		qpnp_pon_store_extra_reset_info(RESET_EXTRA_PANIC_REASON,
			RESET_EXTRA_PANIC_REASON);
	}
#endif

	return NOTIFY_OK;
}

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
static int moto_reboot_reason_restart(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, restart_nb);

	if (reboot->reboot_notify_status)
		return NOTIFY_OK;
#if IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
	qpnp_pon_store_extra_reset_info(RESET_EXTRA_SW_REBOOT_REASON,
					RESET_EXTRA_SW_REBOOT_REASON);
#endif
	pr_warn("record sw reboot flag during restart\n");
	return NOTIFY_OK;
}
#endif

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	struct device_node *np;
	int ret;
#endif

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");

	if (IS_ERR(reboot->nvmem_cell))
		return PTR_ERR(reboot->nvmem_cell);

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	np = of_find_compatible_node(NULL, NULL,
				"qcom,msm-imem-restart_reason");
	if (!np) {
		pr_err("unable to find DT imem restart reason node\n");
	} else {
		restart_reason = of_iomap(np, 0);
		if (!restart_reason) {
			pr_err("unable to map imem restart reason offset\n");
			devm_kfree(&pdev->dev, reboot);
			ret = -ENOMEM;
			return ret;
		}
	}

	force_warm_reboot = of_property_read_bool((&pdev->dev)->of_node,
						"qcom,force-warm-reboot");

	reboot->panic_nb.notifier_call = moto_reboot_reason_panic;
	reboot->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &reboot->panic_nb);

	reboot->reboot_notify_status = 0;
#endif

	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	reboot->restart_nb.notifier_call = moto_reboot_reason_restart;
	reboot->restart_nb.priority = 255;
	register_restart_handler(&reboot->restart_nb);
#endif

	platform_set_drvdata(pdev, reboot);

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT) && IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
	ret = qpnp_pon_store_extra_reset_info(RESET_EXTRA_SW_BOOT_REASON,
					RESET_EXTRA_SW_BOOT_REASON);
	pr_info("update sw boot flag, ret = %d\n", ret);
#endif

	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &reboot->panic_nb);
#endif
	unregister_reboot_notifier(&reboot->reboot_nb);
#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT)
	unregister_restart_handler(&reboot->restart_nb);
#endif

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

MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL v2");

#if IS_ENABLED(CONFIG_MOTO_LEGACY_REBOOT_REASON_SUPPORT) && IS_ENABLED(CONFIG_INPUT_QPNP_POWER_ON)
MODULE_SOFTDEP("pre: qpnp-power-on");
#endif
