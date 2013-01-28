/*
 * Simple softlockup watchdog regression test module
 *
 * Copyright (C) 2012 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <mach/mmi_watchdog.h>

struct completion wdt_timeout_complete;
static void wdt_timeout_work(struct work_struct *work)
{
	local_irq_disable();
	while (1) { }
	local_irq_enable();
	complete(&wdt_timeout_complete);
}
static DECLARE_WORK(wdt_timeout_work_struct, wdt_timeout_work);

static int fire_watchdog_reset_set(void *data, u64 val)
{
	init_completion(&wdt_timeout_complete);
	printk(KERN_WARNING "Fire hardware watchdog reset.\n");
	printk(KERN_WARNING "Please wait ...\n");
	schedule_work_on(0, &wdt_timeout_work_struct);
	wait_for_completion(&wdt_timeout_complete);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fire_watchdog_reset_fops,
	NULL, fire_watchdog_reset_set, "%llu\n");

static int fire_softlockup_reset_set(void *data, u64 val)
{
	touch_hw_watchdog();

	printk(KERN_WARNING "Fire softlockup watchdog reset.\n");
	printk(KERN_WARNING "Please wait ...\n");
	preempt_disable();
	while (1) { }

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fire_softlockup_reset_fops,
	NULL, fire_softlockup_reset_set, "%llu\n");

static int watchdog_test_init(void)
{
	debugfs_create_file("fire_softlockup_reset", 0200,
		NULL, NULL, &fire_softlockup_reset_fops);
	debugfs_create_file("fire_watchdog_reset", 0200,
		NULL, NULL, &fire_watchdog_reset_fops);
	return 0;
}

static void watchdog_test_exit(void)
{
}

module_init(watchdog_test_init);
module_exit(watchdog_test_exit);
MODULE_LICENSE("GPL");
