// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * QCOM TSC PTP on Virtual Machine: Linux driver for Time Stamp Counter Hardware.
 *
 */

#define pr_fmt(fmt) "qcom_tsc_vm: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/platform_device.h>

#define TSCSS_TSC_READ_CNTCV_LO			0x0
#define TSCSS_TSC_READ_CNTCV_HI			0x4

#define NSEC_SHFT				32
#define NSEC					1000000000ULL

struct qcom_ptp_tsc_vm {
	struct	device *dev;
	void __iomem *baseaddr;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info  ptp_clock_info;
	bool tsc_nsec_update;
	spinlock_t reg_lock;
};

static void qcom_vm_tod_read(struct qcom_ptp_tsc_vm *timer, struct timespec64 *ts)
{
	u64 temp, final;
	u32 sec, nsec;

	sec = readl_relaxed(timer->baseaddr + TSCSS_TSC_READ_CNTCV_HI);
	nsec = readl_relaxed(timer->baseaddr + TSCSS_TSC_READ_CNTCV_LO);

	pr_debug("sec %lld nsec %ld\n", sec, nsec);

	if (timer->tsc_nsec_update) {
		temp = sec;
		final = (temp << NSEC_SHFT) | nsec;
		sec = div_u64_rem(final, NSEC, &nsec);
		pr_debug("tsc_nsec_update: %d, sec %lld, nsec %ld\n",
					timer->tsc_nsec_update, sec, nsec);
	}

	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}

static int qcom_ptp_vm_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	unsigned long flags;
	struct qcom_ptp_tsc_vm *timer = container_of(ptp, struct qcom_ptp_tsc_vm,
								ptp_clock_info);

	spin_lock_irqsave(&timer->reg_lock, flags);
	qcom_vm_tod_read(timer, ts);
	spin_unlock_irqrestore(&timer->reg_lock, flags);
	return 0;
}

static struct ptp_clock_info qcom_ptp_clock_info = {
	.owner    = THIS_MODULE,
	.name     = "QCOM TSC VM",
	.gettime64  = qcom_ptp_vm_gettime,
};

static int qcom_ptp_tsc_vm_remove(struct platform_device *pdev)
{
	struct qcom_ptp_tsc_vm *timer = platform_get_drvdata(pdev);

	if (timer->ptp_clock) {
		ptp_clock_unregister(timer->ptp_clock);
		timer->ptp_clock = NULL;
	}

	return 0;
}

static int qcom_ptp_tsc_vm_probe(struct platform_device *pdev)
{
	struct qcom_ptp_tsc_vm *timer;
	struct resource *r_mem;
	int ret;

	timer = devm_kzalloc(&pdev->dev, sizeof(*timer), GFP_KERNEL);
	if (!timer)
		return -ENOMEM;

	timer->dev = &pdev->dev;

	r_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tsc");
	if (!r_mem) {
		dev_err(&pdev->dev, "no IO resource defined\n");
		return -ENXIO;
	}

	timer->baseaddr = devm_ioremap_resource(&pdev->dev, r_mem);
	if (IS_ERR(timer->baseaddr))
		return PTR_ERR(timer->baseaddr);

	spin_lock_init(&timer->reg_lock);

	timer->tsc_nsec_update = of_property_read_bool(pdev->dev.of_node,
							"qcom,tsc-nsec-update");

	timer->ptp_clock_info = qcom_ptp_clock_info;

	timer->ptp_clock = ptp_clock_register(&timer->ptp_clock_info, &pdev->dev);
	if (IS_ERR(timer->ptp_clock)) {
		ret = PTR_ERR(timer->ptp_clock);
		dev_err(&pdev->dev, "Failed to register ptp clock\n");
		goto out;
	}

	pr_info("TSC PTP clock registered in VM\n");

	platform_set_drvdata(pdev, timer);

	return 0;
out:
	timer->ptp_clock = NULL;
	return ret;
}

static const struct of_device_id tsc_vm_of_match[] = {
	{ .compatible = "qcom,tsc-vm", },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, tsc_vm_of_match);

static struct platform_driver qcom_ptp_tsc_vm_driver = {
	.probe  = qcom_ptp_tsc_vm_probe,
	.remove = qcom_ptp_tsc_vm_remove,
	.driver = {
		.name = "qcom_ptp_tsc_vm",
		.of_match_table = tsc_vm_of_match,
	},
};

module_platform_driver(qcom_ptp_tsc_vm_driver);

MODULE_DESCRIPTION("PTP QCOM TSC VM driver");
MODULE_LICENSE("GPL");
