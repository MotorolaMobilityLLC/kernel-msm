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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/time.h>
#include <linux/module.h>

#define PMDBG_IRQ_LIST_SIZE 3
#define PMDBG_UEVENT_ENV_BUFF 64
static unsigned int pmdbg_gpio_irq[PMDBG_IRQ_LIST_SIZE];
static unsigned int pmdbg_gic_irq[PMDBG_IRQ_LIST_SIZE];
static unsigned int pmdbg_pm8xxx_irq[PMDBG_IRQ_LIST_SIZE];

static int pmdbg_gpio_irq_index;
static int pmdbg_gic_irq_index;
static int pmdbg_pm8xxx_irq_index;

static int pmdbg_suspend_uah;
static int pmdbg_resume_uah;
static uint64_t pmdbg_suspend_time;
static uint64_t pmdbg_resume_time;

static void pmdbg_resume(struct early_suspend *h)
{
	int uah = 0;
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
	pr_info("pm_debug: wakeup uah=%d\n", uah);
}

static void pmdbg_suspend(struct early_suspend *h)
{
	int uah = 0;
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
	pr_info("pm_debug: sleep uah=%d\n", uah);
}
static struct early_suspend pmdbg_early_suspend_desc = {
#ifdef CONFIG_HAS_EARLYSUSPEND
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = pmdbg_suspend,
	.resume = pmdbg_resume,
#endif
};

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

void wakeup_source_pm8xxx_cleanup(void)
{
	pmdbg_pm8xxx_irq_index = 0;
}

int wakeup_source_pm8xxx_add_irq(unsigned int irq)
{
	if (pmdbg_pm8xxx_irq_index < PMDBG_IRQ_LIST_SIZE) {
		pmdbg_pm8xxx_irq[pmdbg_pm8xxx_irq_index] = irq;
		pmdbg_pm8xxx_irq_index++;
		return 0;
	} else {
		return -ENOBUFS;
	}
}

static int __devinit pmdbg_driver_probe(struct platform_device *pdev)
{
	pr_debug("pmdbg_driver_probe\n");
	return 0;
}

static int __devexit pmdbg_driver_remove(struct platform_device *pdev)
{
	pr_debug("pmdbg_driver_remove\n");
	return 0;
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

static int pmdbg_driver_suspend(struct device *dev)
{
	int uah = 0;
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
	pmdbg_suspend_uah = uah;
	pmdbg_suspend_time = pmdbg_gettimeofday_ms();
	pr_info("pm_debug: suspend uah=%d\n", uah);
	return 0;
}

static int pmdbg_driver_resume(struct device *dev)
{
	char *envp[8];
	char buf[7][PMDBG_UEVENT_ENV_BUFF];
	int env_index = 0;

	int uah = 0;
	if (pm8921_bms_get_cc_uah(&uah) < 0)
		uah = 0;
	pmdbg_resume_uah = uah;
	pmdbg_resume_time = pmdbg_gettimeofday_ms();
	pr_info("pm_debug: resume uah=%d\n", uah);

	pr_debug("pmdbg_driver_resume: send uevent\n");

	sprintf(buf[0], "suspend_uah=%d", pmdbg_suspend_uah);
	sprintf(buf[1], "suspend_time=%lld", pmdbg_suspend_time);
	sprintf(buf[2], "resume_uah=%d", pmdbg_resume_uah);
	sprintf(buf[3], "resume_time=%lld", pmdbg_resume_time);
	envp[0] = buf[0];
	envp[1] = buf[1];
	envp[2] = buf[2];
	envp[3] = buf[3];
	env_index = 4;

	if (pmdbg_gic_irq > 0) {
		wakeup_source_uevent_env(buf[env_index], "GIC",
					pmdbg_gic_irq,
					pmdbg_gic_irq_index);
		pmdbg_gic_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	if (pmdbg_gpio_irq > 0) {
		wakeup_source_uevent_env(buf[env_index], "GPIO",
					pmdbg_gpio_irq,
					pmdbg_gpio_irq_index);
		pmdbg_gpio_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	if (pmdbg_pm8xxx_irq_index > 0) {
		wakeup_source_uevent_env(buf[env_index], "PM8xxx",
					pmdbg_pm8xxx_irq,
					pmdbg_pm8xxx_irq_index);
		pmdbg_pm8xxx_irq_index = 0;
		envp[env_index] = buf[env_index];
		env_index++;
	}

	envp[env_index] = NULL;
	kobject_uevent_env(&dev->kobj, KOBJ_ONLINE, envp);
	return 0;
}

static struct platform_device pmdbg_device = {
	.name		= "pm_dbg",
	.id		= -1,
};

static const struct dev_pm_ops pmdbg_pm_ops = {
	.suspend	= pmdbg_driver_suspend,
	.resume		= pmdbg_driver_resume,
};

static struct platform_driver pmdbg_driver = {
	.probe		= pmdbg_driver_probe,
	.remove		= __devexit_p(pmdbg_driver_remove),
	.driver		= {
			.name	= "pm_dbg",
			.owner	= THIS_MODULE,
			.pm	= &pmdbg_pm_ops,
	},
};

static int __init pmdbg_init(void)
{
	int err;
	register_early_suspend(&pmdbg_early_suspend_desc);
	err = platform_device_register(&pmdbg_device);
	pr_debug("pmdbg_device register %d\n", err);
	err = platform_driver_register(&pmdbg_driver);
	pr_debug("pmdbg_driver register %d\n", err);

	wakeup_source_gpio_cleanup();
	wakeup_source_gic_cleanup();

	return 0;
}

static void  __exit pmdbg_exit(void)
{
	unregister_early_suspend(&pmdbg_early_suspend_desc);
	platform_driver_unregister(&pmdbg_driver);
	platform_device_unregister(&pmdbg_device);
}

core_initcall(pmdbg_init);
module_exit(pmdbg_exit);

MODULE_ALIAS("platform:pm_dbg");
MODULE_DESCRIPTION("PM Debug driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL v2");
