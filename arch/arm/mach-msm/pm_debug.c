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
#include <linux/suspend.h>
#include <linux/power_supply.h>

#define PMDBG_IRQ_LIST_SIZE 3
#define PMDBG_UEVENT_ENV_BUFF 64
static unsigned int pmdbg_gpio_irq[PMDBG_IRQ_LIST_SIZE];
static unsigned int pmdbg_gic_irq[PMDBG_IRQ_LIST_SIZE];
static unsigned int pmdbg_pmic_irq[PMDBG_IRQ_LIST_SIZE];

static int pmdbg_gpio_irq_index;
static int pmdbg_gic_irq_index;
static int pmdbg_pmic_irq_index;

static uint64_t pmdbg_suspend_time;
static uint64_t pmdbg_resume_time;

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

void wakeup_source_gpio_cleanup(void)
{
	pmdbg_gpio_irq_index = 0;
}

int wakeup_source_gpio_add_irq(unsigned int irq)
{
	if (pmdbg_gpio_irq_index < PMDBG_IRQ_LIST_SIZE) {
		pmdbg_gpio_irq[pmdbg_gpio_irq_index] = irq;
		pmdbg_gpio_irq_index++;
		return 0;
	} else {
		return -ENOBUFS;
	}
}

void wakeup_source_gic_cleanup(void)
{
	pmdbg_gic_irq_index = 0;
}

int wakeup_source_gic_add_irq(unsigned int irq)
{
	if (pmdbg_gic_irq_index < PMDBG_IRQ_LIST_SIZE) {
		pmdbg_gic_irq[pmdbg_gic_irq_index] = irq;
		pmdbg_gic_irq_index++;
		return 0;
	} else {
		return -ENOBUFS;
	}
}

void wakeup_source_pmic_cleanup(void)
{
	pmdbg_pmic_irq_index = 0;
}

int wakeup_source_pmic_add_irq(unsigned int irq)
{
	if (pmdbg_pmic_irq_index < PMDBG_IRQ_LIST_SIZE) {
		pmdbg_pmic_irq[pmdbg_pmic_irq_index] = irq;
		pmdbg_pmic_irq_index++;
		return 0;
	} else {
		return -ENOBUFS;
	}
}

void wakeup_source_uevent_env(char *buf, char *tag, unsigned int irqs[],
				int index)
{
	int i, len;

	if (index > 0) {
		if (index > PMDBG_IRQ_LIST_SIZE)
			index = PMDBG_IRQ_LIST_SIZE;
		len = 0;
		for (i = 0; i < index; i++) {
			if (i == 0)
				len += snprintf(buf,
					PMDBG_UEVENT_ENV_BUFF - len,
					"%s=%d", tag, irqs[i]);
			else
				len += snprintf(buf + len,
					PMDBG_UEVENT_ENV_BUFF - len,
					",%d", irqs[i]);
			if (len == PMDBG_UEVENT_ENV_BUFF)
				break;
		}
	}
}

static uint64_t pmdbg_gettimeofday_ms(void)
{
	uint64_t timeofday = 0;
	struct timespec time;

	getnstimeofday(&time);
	timeofday = time.tv_nsec;
	do_div(timeofday, NSEC_PER_MSEC);
	timeofday = (uint64_t)time.tv_sec * MSEC_PER_SEC + timeofday;
	return timeofday;
}

int pmdbg_suspend(struct device *dev)
{
	pmdbg_suspend_time = pmdbg_gettimeofday_ms();
	return 0;
}

int pmdbg_resume(struct device *dev)
{
	char *envp[6];
	char buf[5][PMDBG_UEVENT_ENV_BUFF];
	int env_index = 0;

	if (pmdbg_suspend_time == 0)
		return 0;

	pmdbg_resume_time = pmdbg_gettimeofday_ms();

	snprintf(buf[0], 33, "suspend_time=%019lld", pmdbg_suspend_time);
	snprintf(buf[1], 32, "resume_time=%019lld", pmdbg_resume_time);
	envp[0] = buf[0];
	envp[1] = buf[1];
	env_index = 2;

	if (pmdbg_gic_irq_index > 0) {
		wakeup_source_uevent_env(buf[env_index], "GIC",
					pmdbg_gic_irq,
					pmdbg_gic_irq_index);
		pmdbg_gic_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	if (pmdbg_gpio_irq_index > 0) {
		wakeup_source_uevent_env(buf[env_index], "GPIO",
					pmdbg_gpio_irq,
					pmdbg_gpio_irq_index);
		pmdbg_gpio_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	if (pmdbg_pmic_irq_index > 0) {
		wakeup_source_uevent_env(buf[env_index], "PMIC",
					pmdbg_pmic_irq,
					pmdbg_pmic_irq_index);
		pmdbg_pmic_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	envp[env_index] = NULL;
	kobject_uevent_env(&dev->kobj, KOBJ_ONLINE, envp);
	return 0;
}

static const struct dev_pm_ops pm_debug_pm_ops = {
	.suspend	= pmdbg_suspend,
	.resume		= pmdbg_resume,
};

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
			.pm	= &pm_debug_pm_ops,
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
