/*
 * android vibrator driver
 *
 * Copyright (C) 2009-2012 LGE, Inc.
 *
 * Author: Jinkyu Choi <jinkyu@lge.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/android_vibrator.h>
#include <linux/mutex.h>
#include "../staging/android/timed_output.h"

#define ANDROID_VIBRATOR_USE_WORKQUEUE

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
static struct workqueue_struct *vibrator_workqueue;
#endif

struct timed_vibrator_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	spinlock_t lock;
	int max_timeout;
	atomic_t vib_status; /* 1:on,0:off */
	atomic_t gain;    /* default max gain */
	atomic_t pwm;
	atomic_t ms_time; /* vibrator duration */
	struct android_vibrator_platform_data *pdata;
	struct work_struct work_vibrator_off;
	struct work_struct work_vibrator_on;
};

#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
static inline void vibrator_work_on(struct work_struct *work)
{
	queue_work(vibrator_workqueue, work);
}

static inline void vibrator_work_off(struct work_struct *work)
{
	if (!work_pending(work))
		queue_work(vibrator_workqueue, work);
}
#else
static inline void vibrator_work_on(struct work_struct *work)
{
	schedule_work(work);
}

static inline void vibrator_work_off(struct work_struct *work)
{
	if (!work_pending(work))
		schedule_work(work);
}
#endif

static int android_vibrator_force_set(struct timed_vibrator_data *vib,
		int intensity, int pwm)
{
	struct android_vibrator_platform_data *pdata = vib->pdata;
	int vib_duration_ms = 0;

	pr_debug("%s: intensity : %d\n", __func__, intensity);

	if (pdata->vibe_warmup_delay > 0) {
		if (atomic_read(&vib->vib_status))
			msleep(pdata->vibe_warmup_delay);
	}

	/* TODO: control the gain of vibrator */
	if (intensity == 0) {
		pdata->ic_enable_set(0);
		pdata->pwm_set(0, 0, pwm);
		/* should be checked for vibrator response time */
		pdata->power_set(0);
		atomic_set(&vib->vib_status, 0);
	} else {
		if (work_pending(&vib->work_vibrator_off))
			cancel_work_sync(&vib->work_vibrator_off);
		hrtimer_cancel(&vib->timer);

		vib_duration_ms = atomic_read(&vib->ms_time);
		/* should be checked for vibrator response time */
		pdata->power_set(1);
		pdata->pwm_set(1, intensity, pwm);
		pdata->ic_enable_set(1);
		atomic_set(&vib->vib_status, 1);

		hrtimer_start(&vib->timer,
				ns_to_ktime((u64)vib_duration_ms * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
	}

	return 0;
}

static void android_vibrator_on(struct work_struct *work)
{
	struct timed_vibrator_data *vib =
		container_of(work, struct timed_vibrator_data,
				work_vibrator_on);
	int gain = atomic_read(&vib->gain);
	int pwm = atomic_read(&vib->pwm);
	/* suspend /resume logging test */
	pr_debug("%s: gain = %d pwm = %d\n", __func__,
			gain, pwm);

	android_vibrator_force_set(vib, gain, pwm);
}

static void android_vibrator_off(struct work_struct *work)
{
	struct timed_vibrator_data *vib =
		container_of(work, struct timed_vibrator_data,
				work_vibrator_off);

	pr_debug("%s\n", __func__);
	android_vibrator_force_set(vib, 0, vib->pdata->vibe_n_value);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct timed_vibrator_data *vib =
		container_of(timer, struct timed_vibrator_data, timer);
	vibrator_work_off(&vib->work_vibrator_off);
	return HRTIMER_NORESTART;
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct timed_vibrator_data *vib =
		container_of(dev, struct timed_vibrator_data, dev);

	if (hrtimer_active(&vib->timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_enable(struct timed_output_dev *dev, int ms_time)
{
	struct timed_vibrator_data *vib =
		container_of(dev, struct timed_vibrator_data, dev);
	unsigned long	flags;

	pr_debug("%s: ms_time %d \n", __func__, ms_time);
	spin_lock_irqsave(&vib->lock, flags);

	if (ms_time > 0) {
		if (ms_time > vib->max_timeout)
			ms_time = vib->max_timeout;

		atomic_set(&vib->ms_time, ms_time);

		vibrator_work_on(&vib->work_vibrator_on);
	} else {
		vibrator_work_off(&vib->work_vibrator_off);
	}
	spin_unlock_irqrestore(&vib->lock, flags);
}

static ssize_t vibrator_amp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ =
		(struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib =
		container_of(dev_, struct timed_vibrator_data, dev);
	int gain = atomic_read(&(vib->gain));

	return sprintf(buf, "%d\n", gain);
}

static ssize_t vibrator_amp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ =
		(struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib =
		container_of(dev_, struct timed_vibrator_data, dev);

	int gain;
	sscanf(buf, "%d", &gain);
	if (gain > 100)
		gain = 100;
	else if (gain < -100)
		gain = -100;
	atomic_set(&vib->gain, gain);

	return size;
}

static ssize_t vibrator_pwm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *dev_ =
		(struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib =
		container_of(dev_, struct timed_vibrator_data, dev);
	int gain = atomic_read(&(vib->pwm));

	gain = 4800/gain;

	return sprintf(buf, "%d\n", gain);
}


static ssize_t vibrator_pwm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct timed_output_dev *dev_ =
		(struct timed_output_dev *)dev_get_drvdata(dev);
	struct timed_vibrator_data *vib =
		container_of(dev_, struct timed_vibrator_data, dev);

	int gain;
	sscanf(buf, "%d", &gain);

	gain = 4800/gain;

	atomic_set(&vib->pwm, gain);

	return size;
}

static struct device_attribute android_vibrator_device_attrs[] = {
	__ATTR(amp, S_IRUGO | S_IWUSR, vibrator_amp_show, vibrator_amp_store),
	__ATTR(pwm, S_IRUGO | S_IWUSR, vibrator_pwm_show, vibrator_pwm_store),
};

struct timed_vibrator_data android_vibrator_data = {
	.dev.name = "vibrator",
	.dev.enable = vibrator_enable,
	.dev.get_time = vibrator_get_time,
	.max_timeout = 30000, /* max time for vibrator enable 30 sec. */
};

static int android_vibrator_probe(struct platform_device *pdev)
{

	int i, ret = 0;
	struct timed_vibrator_data *vib = &android_vibrator_data;
	vib->pdata = pdev->dev.platform_data;

	if (!vib->pdata) {
		pr_err("%s: no platform data\n", __func__);
		return -ENODEV;
	}

	if (vib->pdata->vibrator_init) {
		ret = vib->pdata->vibrator_init();
		if (ret < 0) {
			pr_err("%s: failed to vibrator init\n", __func__);
			return ret;
		}
	}

	platform_set_drvdata(pdev, &android_vibrator_data);

	if (vib->pdata->amp > 100)
		vib->pdata->amp = 100;
	else if (vib->pdata->amp < -100)
		vib->pdata->amp = -100;
	atomic_set(&vib->gain, vib->pdata->amp); /* max value is 100 */
	atomic_set(&vib->pwm, vib->pdata->vibe_n_value);
	atomic_set(&vib->vib_status, 0);
	pr_info("android_vibrator: default amplitude %d \n",
			vib->pdata->amp);

	INIT_WORK(&vib->work_vibrator_off, android_vibrator_off);
	INIT_WORK(&vib->work_vibrator_on, android_vibrator_on);
	hrtimer_init(&vib->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->timer.function = vibrator_timer_func;
	spin_lock_init(&vib->lock);

	ret = timed_output_dev_register(&vib->dev);
	if (ret < 0) {
		pr_err("%s: failed to register timed output device\n",
				__func__);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(android_vibrator_device_attrs); i++) {
		ret = device_create_file(vib->dev.dev,
				&android_vibrator_device_attrs[i]);
		if (ret < 0) {
			pr_err("%s: failed to create sysfs\n", __func__);
			goto err_sysfs;
		}
	}

	pr_info("android vibrator probed\n");

	return 0;

err_sysfs:
	for (; i >= 0; i--) {
		device_remove_file(vib->dev.dev,
				&android_vibrator_device_attrs[i]);
	}

	timed_output_dev_unregister(&vib->dev);
	return ret;
}

static int android_vibrator_remove(struct platform_device *pdev)
{
	struct timed_vibrator_data *vib =
		(struct timed_vibrator_data *)platform_get_drvdata(pdev);
	int i;

	vibrator_work_off(&vib->work_vibrator_off);
	for (i = ARRAY_SIZE(android_vibrator_device_attrs); i >= 0; i--) {
		device_remove_file(vib->dev.dev,
				&android_vibrator_device_attrs[i]);
	}

	timed_output_dev_unregister(&vib->dev);

	return 0;
}

static struct platform_driver android_vibrator_driver = {
	.probe = android_vibrator_probe,
	.remove = android_vibrator_remove,
	.driver = {
		.name = "android-vibrator",
	},
};

static int __init android_vibrator_init(void)
{
#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
	vibrator_workqueue = create_workqueue("vibrator");
	if (!vibrator_workqueue) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}
#endif
	return platform_driver_register(&android_vibrator_driver);
}

static void __exit android_vibrator_exit(void)
{
#ifdef ANDROID_VIBRATOR_USE_WORKQUEUE
	if (vibrator_workqueue)
		destroy_workqueue(vibrator_workqueue);
	vibrator_workqueue = NULL;
#endif
	platform_driver_unregister(&android_vibrator_driver);
}

late_initcall_sync(android_vibrator_init); /* to let init lately */
module_exit(android_vibrator_exit);

MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("Android Vibrator Driver");
MODULE_LICENSE("GPL");
