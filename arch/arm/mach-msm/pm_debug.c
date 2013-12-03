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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/fb.h>

static void stuck_wakelock_timeout(unsigned long data);
static void stuck_wakelock_wdset(void);
static void stuck_wakelock_wdclr(void);
static DEFINE_TIMER(stuck_wakelock_wd, stuck_wakelock_timeout, 0, 0);

/**
 *      stuck_wakelock_timeout - stuck wakelocks dump watchdog
 *      handler
 *
 *      Called after display off to dump the stuck wake locks
 *      clear in late resume.
 *
 */
static void stuck_wakelock_timeout(unsigned long data)
{
	pr_info("**** active wakelocks ****\n");
	active_wakeup_sources_stats_show();
	stuck_wakelock_wdset();
}

/**
 *      stuck_wakelock_wdset - Sets up stuck wakelocks dump
 *      watchdog timer.
 */
static void stuck_wakelock_wdset()
{
	mod_timer(&stuck_wakelock_wd, jiffies + (HZ * 600));
}

/**
 *      stuck_wakelock_wdclr - clears stuck wakelocks dump
 *      watchdog timer.
 *
 */
static void stuck_wakelock_wdclr(void)
{
	del_timer_sync(&stuck_wakelock_wd);
}

static int pmdbg_display_notify(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct fb_event *event = data;
	int *blank;

	if (action != FB_EVENT_BLANK)
		return NOTIFY_OK;

	if (!event || !event->data) {
		pr_info("%s(): fb event is invalid.\n", __func__);
		return NOTIFY_OK;
	}

	blank = event->data;
	pr_info("%s(): action = %lu, data = %d\n",
		__func__, action, *blank);

	if (*blank == FB_BLANK_UNBLANK)
		/* display on */
		stuck_wakelock_wdclr();
	else
		/* display off */
		stuck_wakelock_wdset();

	return NOTIFY_OK;
}

static int pmdbg_driver_probe(struct platform_device *pdev)
{
	struct notifier_block *notifier;

	pr_debug("pmdbg_driver_probe\n");
	notifier = kzalloc(sizeof(*notifier), GFP_KERNEL);
	if (!notifier) {
		pr_err("%s(): cannot allocate device data\n", __func__);
		return -ENOMEM;
	}
	notifier->notifier_call = pmdbg_display_notify;
	fb_register_client(notifier);
	platform_set_drvdata(pdev, notifier);

	return 0;
}

static int pmdbg_driver_remove(struct platform_device *pdev)
{
	struct notifier_block *notifier = platform_get_drvdata(pdev);

	pr_debug("pmdbg_driver_remove\n");
	stuck_wakelock_wdclr();
	fb_unregister_client(notifier);

	return 0;
}

static struct platform_device pmdbg_device = {
	.name		= "pm_dbg",
	.id		= -1,
};

static struct platform_driver pmdbg_driver = {
	.probe		= pmdbg_driver_probe,
	.remove		= __devexit_p(pmdbg_driver_remove),
	.driver		= {
			.name	= "pm_dbg",
			.owner	= THIS_MODULE,
	},
};

static int __init pmdbg_init(void)
{
	int err;
	err = platform_device_register(&pmdbg_device);
	pr_debug("pmdbg_device register %d\n", err);
	err = platform_driver_register(&pmdbg_driver);
	pr_debug("pmdbg_driver register %d\n", err);

	return 0;
}

static void  __exit pmdbg_exit(void)
{
	platform_driver_unregister(&pmdbg_driver);
	platform_device_unregister(&pmdbg_device);
}

core_initcall(pmdbg_init);
module_exit(pmdbg_exit);

MODULE_ALIAS("platform:pm_dbg");
MODULE_DESCRIPTION("PM Debug driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL v2");
