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

#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <soc/qcom/mmi_watchdog.h>

static int fire_softlockup_reset_set(void *data, u64 val)
{
	touch_hw_watchdog();

	printk(KERN_WARNING "Fire softlockup watchdog reset.\n");
	printk(KERN_WARNING "Please wait ...\n");
	preempt_disable();
	while (1)
	;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fire_softlockup_reset_fops,
	NULL, fire_softlockup_reset_set, "%llu\n");

static int watchdog_test_init(void)
{
	debugfs_create_file("fire_softlockup_reset", 0200,
		NULL, NULL, &fire_softlockup_reset_fops);
	return 0;
}

static void watchdog_test_exit(void)
{
}

module_init(watchdog_test_init);
module_exit(watchdog_test_exit);
MODULE_LICENSE("GPL");
