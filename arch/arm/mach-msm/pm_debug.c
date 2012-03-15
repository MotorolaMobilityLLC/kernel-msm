/* Copyright (c) 2012, Motorola Mobility Inc. All rights reserved.
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
#include <linux/suspend.h>
#include <linux/earlysuspend.h>
#ifdef CONFIG_PM8921_BMS
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#endif

static struct notifier_block pmdbg_suspend_notifier;

static void pmdbg_resume(struct early_suspend *h)
{
	int uah = 0;
#ifdef CONFIG_PM8921_BMS
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
#endif
	pr_info("pm_debug: wakeup uah=%d\n", uah);
}

static void pmdbg_suspend(struct early_suspend *h)
{
	int uah = 0;
#ifdef CONFIG_PM8921_BMS
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
#endif
	pr_info("pm_debug: sleep uah=%d\n", uah);
}
static struct early_suspend pmdbg_early_suspend_desc = {
#ifdef CONFIG_HAS_EARLYSUSPEND
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = pmdbg_suspend,
	.resume = pmdbg_resume,
#endif
};

static int
pmdbg_suspend_notifier_call(struct notifier_block *bl, unsigned long state,
			void *unused)
{
	int uah = 0;
#ifdef CONFIG_PM8921_BMS
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
#endif
	switch (state) {
	case PM_SUSPEND_PREPARE:
		pr_info("pm_debug: suspend uah=%d\n", uah);
		break;
	case PM_POST_SUSPEND:
		pr_info("pm_debug: resume uah=%d\n", uah);
		break;
	}
	return NOTIFY_DONE;
}

static int __init pmdbg_init(void)
{
	pmdbg_suspend_notifier.notifier_call = pmdbg_suspend_notifier_call;
	register_pm_notifier(&pmdbg_suspend_notifier);
	register_early_suspend(&pmdbg_early_suspend_desc);
	return 0;
}

static void  __exit pmdbg_exit(void)
{
	unregister_early_suspend(&pmdbg_early_suspend_desc);
	unregister_pm_notifier(&pmdbg_suspend_notifier);
}

core_initcall(pmdbg_init);
module_exit(pmdbg_exit);
