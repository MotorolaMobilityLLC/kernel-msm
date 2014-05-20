/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>

#include "../staging/android/timed_output.h"

/* default timeout */
#define VIB_DEFAULT_TIMEOUT 10000

struct msm_vib {
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct workqueue_struct *queue;

	int state;
	int timeout;
	struct mutex lock;
	const char *supply_name;
	int voltage_level;
	struct regulator *vib_en_ldo;
};

static void set_vibrator(struct regulator *vib_en_ldo, int on)
{
	int rc;
	static int previous_state = 0;

	pr_info("[VIB] %s, on=%d\n", __func__, on);

	if ( previous_state == on) {
		pr_info("[VIB] %s,keep current state %d\n", __func__, previous_state);
		return;
	}

	if (on) {
		rc = regulator_enable(vib_en_ldo);
		if (rc) {
			pr_err("[VIB] unable to enable vib ldo %d\n", rc);
			previous_state = rc;
		}
		else
			previous_state = on;
	} else {
		rc = regulator_disable(vib_en_ldo);
		if (rc) {
			pr_err("[VIB] unable to disable vib ldo %d\n", rc);
			previous_state = rc;
		}
		else
			previous_state = on;

	}
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct msm_vib *vib = container_of(dev, struct msm_vib,
					 timed_dev);

	if ((value > 0) && (value <10)) {
		pr_info("[VIB] not effective value %d / %d \n" , value, vib->state);
		return;
	}

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (value == 0) {
		pr_info("[VIB] OFF\n");
		vib->state = 0;
	} else {
		pr_info("[VIB] ON, Duration : %d msec\n" , value);
		vib->state = 1;

		if (value == 0x7fffffff) {
			pr_info("[VIB] No Use Timer %d\n", value);
		} else {
			value = (value > vib->timeout ?
					vib->timeout : value);
				hrtimer_start(&vib->vib_timer,
					ktime_set(value / 1000,
					(value % 1000) * 1000000),
					HRTIMER_MODE_REL);
		}
	}
	queue_work(vib->queue, &vib->work);
	mutex_unlock(&vib->lock);
}

static void msm_vibrator_update(struct work_struct *work)
{
	struct msm_vib *vib = container_of(work, struct msm_vib,
					 work);

		set_vibrator(vib->vib_en_ldo, vib->state);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct msm_vib *vib = container_of(dev, struct msm_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct msm_vib *vib = container_of(timer, struct msm_vib,
							 vib_timer);

	vib->state = 0;
	queue_work(vib->queue, &vib->work);

	return HRTIMER_NORESTART;
}

static int vibrator_hw_init(struct msm_vib *vib)
{
	int ret = 0;

	vib->vib_en_ldo = regulator_get(NULL, vib->supply_name);
	if (IS_ERR(vib->vib_en_ldo)) {
		vib->vib_en_ldo = NULL;
		pr_err("[VIB] ldo error\n");
		return -EINVAL;
	}

	ret = regulator_set_voltage(vib->vib_en_ldo, 2700000, 2700000);
	if (ret) {
		pr_err("[VIB] unable to set voltage for ldo1\n");
		return ret;
	}

	return ret;

}

static int msm_vibrator_probe(struct platform_device *pdev)
{
	struct msm_vib *vib;
	int rc = 0;

	pr_info("[VIB] %s\n", __func__);

	vib = devm_kzalloc(&pdev->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)	{
		pr_err("%s : Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	rc = of_property_read_string(pdev->dev.of_node,
			"samsung,supply-name", &vib->supply_name);
	if (rc < 0) {
		pr_err("[%s]: supply-name\n", __func__);
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
			"samsung,voltage-level", &vib->voltage_level);
	if (rc < 0) {
		pr_err("[%s]: voltage-level\n", __func__);
		return rc;
	}

	pr_info("%s, %s, %d\n", __func__, vib->supply_name, vib->voltage_level);

	rc = vibrator_hw_init(vib);
	if (rc < 0) {
		pr_err("[%s]: hw init fail\n", __func__);
		return rc;
	}

	vib->timeout = VIB_DEFAULT_TIMEOUT;

	INIT_WORK(&vib->work, msm_vibrator_update);
	mutex_init(&vib->lock);

	vib->queue = create_singlethread_workqueue("msm_vibrator");
	if (!vib->queue) {
		pr_err("%s(): can't create workqueue\n", __func__);
		return -ENOMEM;
	}

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = vibrator_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = vibrator_get_time;
	vib->timed_dev.enable = vibrator_enable;

	dev_set_drvdata(&pdev->dev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0) {
		pr_err("[VIB] timed_output_dev_register fail (rc=%d)\n", rc);
		goto err_read_vib;
	}

	return 0;

err_read_vib:
	destroy_workqueue(vib->queue);
	return rc;
}

static int msm_vibrator_remove(struct platform_device *pdev)
{
	struct msm_vib *vib = dev_get_drvdata(&pdev->dev);

	destroy_workqueue(vib->queue);
	mutex_destroy(&vib->lock);

	return 0;
}

static const struct of_device_id vib_motor_match[] = {
	{	.compatible = "vibrator",
	},
	{}
};

static struct platform_driver msm_vibrator_platdrv = {
	.driver = {
		.name = "msm_vibrator",
		.owner = THIS_MODULE,
		.of_match_table = vib_motor_match,
	},
	.probe = msm_vibrator_probe,
	.remove = msm_vibrator_remove,
};

static int __init msm_timed_vibrator_init(void)
{
	pr_info("[VIB] %s\n", __func__);

	return platform_driver_register(&msm_vibrator_platdrv);
}

void __exit msm_timed_vibrator_exit(void)
{
	platform_driver_unregister(&msm_vibrator_platdrv);
}
module_init(msm_timed_vibrator_init);
module_exit(msm_timed_vibrator_exit);

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_LICENSE("GPL v2");
