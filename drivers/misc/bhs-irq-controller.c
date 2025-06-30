// SPDX-License-Identifier:  GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

static int smp2p_adsp_irq = -1;
static int smp2p_wpss_irq = -1;
static int smp2p_modem_irq = -1;
static int smp2p_cdsp_irq = -1;
static int glink_modem_irq = -1;

static DEFINE_MUTEX(bhs_irq_mutex);

static ssize_t bhs_ipcc_irqs_store(struct kobject *kobj,
		  struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret = kstrtoint(buf, 10, &val);

	if (ret)
		return ret;

	if (val != 0 && val != 1)
		return -EINVAL;

	mutex_lock(&bhs_irq_mutex);
	if (val == 1) {
		/* Disable IRQs */
		if (smp2p_adsp_irq > 0)
			disable_irq_nosync(smp2p_adsp_irq);
		if (smp2p_wpss_irq > 0)
			disable_irq_nosync(smp2p_wpss_irq);
		if (smp2p_modem_irq > 0)
			disable_irq_nosync(smp2p_modem_irq);
		if (smp2p_cdsp_irq > 0)
			disable_irq_nosync(smp2p_cdsp_irq);
		if (glink_modem_irq > 0)
			disable_irq_nosync(glink_modem_irq);
		pr_info("BHS Disabled irqs: %d %d %d %d %d\n", smp2p_adsp_irq,
			smp2p_wpss_irq, smp2p_modem_irq, smp2p_cdsp_irq, glink_modem_irq);
	} else {
		/* Enable IRQs */
		if (smp2p_adsp_irq > 0)
			enable_irq(smp2p_adsp_irq);
		if (smp2p_wpss_irq > 0)
			enable_irq(smp2p_wpss_irq);
		if (smp2p_modem_irq > 0)
			enable_irq(smp2p_modem_irq);
		if (smp2p_cdsp_irq > 0)
			enable_irq(smp2p_cdsp_irq);
		if (glink_modem_irq > 0)
			enable_irq(glink_modem_irq);
		pr_info("BHS Enabled irqs: %d %d %d %d %d\n", smp2p_adsp_irq,
			smp2p_wpss_irq, smp2p_modem_irq, smp2p_cdsp_irq, glink_modem_irq);
	}
	mutex_unlock(&bhs_irq_mutex);

	return count;
}

static struct kobj_attribute bhs_ipcc_irqs_attr =
	__ATTR(bhs_ipcc_irqs, 0220, NULL, bhs_ipcc_irqs_store);

static int parse_irqs_from_dt(void)
{
	struct device_node *np;
	u32 remote_pid;
	int irq;
	int found_irqs = 0;

	for_each_compatible_node(np, NULL, "qcom,smp2p") {
		if (!of_property_read_u32(np, "qcom,remote-pid", &remote_pid)) {
			irq = of_irq_get(np, 0);
			if (irq > 0) {
				switch (remote_pid) {
				case 1:
					smp2p_modem_irq = irq;
					found_irqs++;
					break;
				case 2:
					smp2p_adsp_irq = irq;
					found_irqs++;
					break;
				case 5:
					smp2p_cdsp_irq = irq;
					found_irqs++;
					break;
				case 13:
					smp2p_wpss_irq = irq;
					found_irqs++;
					break;
				}
			}
		}
	}
	struct device_node *rproc_np, *glink_np;

	for_each_compatible_node(rproc_np, NULL, "qcom,volcano-modem-pas") {
		glink_np = of_get_child_by_name(rproc_np, "glink-edge");
		if (!glink_np)
			continue;
		if (!of_property_read_u32(glink_np, "qcom,remote-pid", &remote_pid)) {
			if (remote_pid == 1) {
				irq = of_irq_get(glink_np, 0);
				if (irq > 0) {
					glink_modem_irq = irq;
					found_irqs++;
				}
			}
		}
		of_node_put(glink_np);
	}

	return found_irqs > 0 ? 0 : -ENODEV;
}

static struct kobject *irq_kobj;

static int __init bhs_irq_module_init(void)
{
	int ret;

	ret = parse_irqs_from_dt();
	if (ret) {
		pr_err("Failed to find any BHS IRQs in device tree\n");
		return ret;
	}

	irq_kobj = kobject_create_and_add("bhs_irq_ctrl", kernel_kobj);
	if (!irq_kobj) {
		pr_err("Failed to create kobject 'bhs_irq_ctrl'\n");
		return -ENOMEM;
	}
	ret = sysfs_create_file(irq_kobj, &bhs_ipcc_irqs_attr.attr);
	if (ret) {
		pr_err("Failed to create sysfs file for bhs_ipcc_irqs_attr\n");
		kobject_put(irq_kobj);
		irq_kobj = NULL; // Set to NULL to avoid double-free in module_exit
		return ret;
	}
	return 0;
}

static void __exit bhs_irq_module_exit(void)
{
	if (irq_kobj) { // Check if irq_kobj is valid
		sysfs_remove_file(irq_kobj, &bhs_ipcc_irqs_attr.attr);
		kobject_put(irq_kobj);
		irq_kobj = NULL;
	}
}

module_init(bhs_irq_module_init);
module_exit(bhs_irq_module_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standalone IPCC IRQ control module for BHS SMP2P and GLINK");
