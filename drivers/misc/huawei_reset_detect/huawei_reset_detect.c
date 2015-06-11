/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/list.h>
#include <linux/of_address.h>
#include <linux/huawei_reset_detect.h>

static void *reset_magic_addr = NULL;

static void raw_writel(unsigned long value, void *p_addr)
{
	if (NULL == p_addr) {
		return;
	}
	__raw_writel(value, p_addr);
	mb();
	return;
}

void set_reset_magic(int magic_number)
{
	/* no need to check reset_magic_addr here, raw_writel will check */
	raw_writel(magic_number, reset_magic_addr);
}
EXPORT_SYMBOL(set_reset_magic);

static int huawei_apanic_handler(struct notifier_block *this,
				 unsigned long event, void *ptr)
{
	pr_info(RESET_DETECT_TAG "huawei_apanic_handler enters \n");

#ifdef CONFIG_PREEMPT
	/* Ensure that cond_resched() won't try to preempt anybody */
	add_preempt_count(PREEMPT_ACTIVE);
#endif

	set_reset_magic(RESET_MAGIC_APANIC);

#ifdef CONFIG_PREEMPT
	sub_preempt_count(PREEMPT_ACTIVE);
#endif

	return NOTIFY_DONE;
}

static struct notifier_block huawei_apanic_event_nb = {
	.notifier_call = huawei_apanic_handler,
	.priority = INT_MAX,
};

static void register_huawei_apanic_notifier(void)
{
	int rc = 0;

	rc = atomic_notifier_chain_register(&panic_notifier_list,
					    &huawei_apanic_event_nb);

	pr_info(RESET_DETECT_TAG "Registerd panic notifier:%d\n", rc);

	return;
}

static void unregister_huawei_apanic_notifier(void)
{
	int rc = 0;

	rc = atomic_notifier_chain_unregister(&panic_notifier_list,
					    &huawei_apanic_event_nb);

	pr_info(RESET_DETECT_TAG "Unregisterd panic notifier:%d\n", rc);

	return;
}

static int __init reset_magic_address_get(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL,
				     "qcom,msm-imem-reset_magic_addr");
	if (!np) {
		pr_err(RESET_DETECT_TAG
		       "unable to find DT imem reset_magic_addr node\n");
		return -1;
	}

	reset_magic_addr = of_iomap(np, 0);
	if (!reset_magic_addr) {
		pr_err(RESET_DETECT_TAG
		       "unable to map imem reset_magic_addr offset\n");
		return -1;
	}

	pr_info(RESET_DETECT_TAG "reset_magic_addr=%p, value=0x%x\n",
		reset_magic_addr, __raw_readl(reset_magic_addr));


	return 0;
}

static int __init huawei_reset_detect_init(void)
{
	int ret = 0;

	/* get reset magic address for dt and iomap it */
	ret = reset_magic_address_get();
	if (ret) {
		return -1;
	}

	/* regitster the panic & reboot notifier */
	register_huawei_apanic_notifier();

	printk("Reset detect initialization was sucessful.\n");

	return 0;
}

static void __exit huawei_reset_detect_exit(void)
{
	iounmap(reset_magic_addr);
	reset_magic_addr = NULL;

	unregister_huawei_apanic_notifier();

	return;
}

module_init(huawei_reset_detect_init);
module_exit(huawei_reset_detect_exit);
MODULE_LICENSE(GPL);
