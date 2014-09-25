/*
 *  Copyright (C) 2012 Motorola, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>

#define STILLMODE_CLIENT_DRIVER_NAME "m4sensorhub_stillmode"
#define STILLMODE_DEFAULT_TIMEOUT  600 /* 10 minutes */

static DEFINE_MUTEX(state_access);

enum m4_stillmode_type {
	MOTION,
	STILL,
};

struct stillmode_client {
	struct m4sensorhub_data *m4sensorhub;
	struct input_dev *input_dev;
	enum m4_stillmode_type state;
	struct work_struct queued_work;
	u16 timeout;
};

static struct stillmode_client *g_stillmode_data;

static int stillmode_set_timeout(struct stillmode_client *stillmode_client_data,
				 u16 timeout)
{
	int ret;

	ret = m4sensorhub_reg_write(stillmode_client_data->m4sensorhub,
				    M4SH_REG_POWER_STILLMODETIMEOUT,
				    (char *)&timeout, m4sh_no_mask);
	if (ret == m4sensorhub_reg_getsize(stillmode_client_data->m4sensorhub,
				M4SH_REG_POWER_STILLMODETIMEOUT)) {
		stillmode_client_data->timeout = timeout;
		ret = 0;
	} else
		ret = -EIO;

	return ret;

}

static void stillmode_set_state(struct stillmode_client *stillmode_client_data,
			       enum m4_stillmode_type state)
{
	mutex_lock(&state_access);
	if (stillmode_client_data->state == state) {
		mutex_unlock(&state_access);
		printk(KERN_WARNING "M4SH duplicate stillmode update (%s)\n",
			(state == STILL) ? "still" : "moving");
	} else {
		stillmode_client_data->state = state;
		mutex_unlock(&state_access);

		input_report_switch(stillmode_client_data->input_dev,
				    SW_STILL_MODE,
				   (stillmode_client_data->state == STILL));
		input_sync(stillmode_client_data->input_dev);
		printk(KERN_INFO "stillmode state changed to %s (%d)\n",
			(state == STILL) ? "still" : "moving", state);
	}
}

static int m4_stillmode_exit(void)
{
	struct stillmode_client *stillmode_client_data = g_stillmode_data;
	int ret = 0;

	KDEBUG(M4SH_INFO, "Resetting stillmode timer\n");

	/* writing timeout value to M4 resets its timer */
	ret = stillmode_set_timeout(stillmode_client_data,
				    stillmode_client_data->timeout);
	if (ret == 0) {
		if (stillmode_client_data->state == STILL)
			stillmode_set_state(stillmode_client_data, MOTION);
	} else
		KDEBUG(M4SH_ERROR, "M4SH Error setting timeout (%d)\n", ret);

	return ret;
}


int m4sensorhub_stillmode_exit(void)
{
	return m4_stillmode_exit();
}
EXPORT_SYMBOL_GPL(m4sensorhub_stillmode_exit);


static void m4sensorhub_stillmode_work(struct work_struct *work)
{
	m4sensorhub_stillmode_exit();
}

static void m4_handle_stillmode_irq(enum m4sensorhub_irqs int_event,
					void *stillmode_data)
{
	struct stillmode_client *stillmode_client_data = stillmode_data;
	enum m4_stillmode_type new_state;

	KDEBUG(M4SH_INFO, "%s() got irq %d (%s)\n", __func__, int_event,
	int_event == M4SH_IRQ_STILL_DETECTED ? "STILL_MODE" : "MOTION_MODE");

	switch (int_event) {
	case (M4SH_IRQ_STILL_DETECTED):
		new_state = STILL;
		break;
	case (M4SH_IRQ_MOTION_DETECTED):
		new_state = MOTION;
		break;
	default:
		printk(KERN_ERR "%s() Unexpected irq: %d\n",
			__func__, int_event);
		return;
		break;
	}

	stillmode_set_state(stillmode_client_data, new_state);
}

static ssize_t m4_stillmode_getstate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stillmode_client *stillmode_client_data =
						 platform_get_drvdata(pdev);

	return sprintf(buf, "%d \n", stillmode_client_data->state);
}

static ssize_t m4_stillmode_setstate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stillmode_client *stillmode_client_data =
						 platform_get_drvdata(pdev);
	long value;
	int ret = size;

	if (((strict_strtoul(buf, 10, &value)) < 0) ||
	     (value != MOTION)) {
		KDEBUG(M4SH_ERROR, "M4SH stillmode invalid value: %ld.  Only "
				   "%d is allowed\n", value, MOTION);
		return -EINVAL;
	}

	if (value != stillmode_client_data->state)
		return m4sensorhub_stillmode_exit();

	return ret;
}

static ssize_t m4_stillmode_get_timeout(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stillmode_client *stillmode_client_data =
						platform_get_drvdata(pdev);

	return sprintf(buf, "%d \n", stillmode_client_data->timeout);
}

static ssize_t m4_stillmode_set_timeout(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stillmode_client *stillmode_client_data =
						 platform_get_drvdata(pdev);
	long value;
	int ret;

	if (((strict_strtoul(buf, 10, &value)) < 0) ||
	     (value < 0) || (value > USHRT_MAX)) {
		KDEBUG(M4SH_ERROR, "M4SH stillmode invalid timeout: %ld\n",
			value);
		return -EINVAL;
	}

	KDEBUG(M4SH_DEBUG, "%s() setting timeout to %ld\n", __func__, value);

	ret = stillmode_set_timeout(stillmode_client_data, value);

	return ((ret == 0) ? size : ret);
}

static DEVICE_ATTR(state, 0664, m4_stillmode_getstate,
		   m4_stillmode_setstate);
static DEVICE_ATTR(timeout, 0664, m4_stillmode_get_timeout,
		   m4_stillmode_set_timeout);

static int stillmode_driver_init(struct init_calldata *p_arg)
{
	int ret;
	struct m4sensorhub_data *m4sensorhub = p_arg->p_m4sensorhub_data;

	ret = m4sensorhub_irq_register(m4sensorhub, M4SH_IRQ_STILL_DETECTED,
					m4_handle_stillmode_irq,
					g_stillmode_data, 1);

	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering still mode IRQ: %d\n",
		ret);
		return ret;
	}

	ret = m4sensorhub_irq_enable(m4sensorhub, M4SH_IRQ_STILL_DETECTED);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error enabling still mode int: %d\n",
			ret);
		goto unregister_still_irq;
	}

	ret = m4sensorhub_irq_register(m4sensorhub, M4SH_IRQ_MOTION_DETECTED,
					m4_handle_stillmode_irq,
					g_stillmode_data, 1);

	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering moving mode IRQ: %d\n",
			ret);
		goto disable_still_irq;
	}

	ret = m4sensorhub_irq_enable(m4sensorhub, M4SH_IRQ_MOTION_DETECTED);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error enabling moving mode int: %d\n",
			ret);
		goto unregister_moving_irq;
	}

	/* initialize timer on M4 */
	m4sensorhub_stillmode_exit();
	return ret;

unregister_moving_irq:
	m4sensorhub_irq_unregister(m4sensorhub, M4SH_IRQ_MOTION_DETECTED);
disable_still_irq:
	m4sensorhub_irq_disable(m4sensorhub, M4SH_IRQ_STILL_DETECTED);
unregister_still_irq:
	m4sensorhub_irq_unregister(m4sensorhub, M4SH_IRQ_STILL_DETECTED);
	return ret;
}

static int stillmode_client_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct stillmode_client *stillmode_client_data;
	struct m4sensorhub_data *m4sensorhub = m4sensorhub_client_get_drvdata();

	if (!m4sensorhub)
		return -EFAULT;

	stillmode_client_data = kzalloc(sizeof(*stillmode_client_data),
						GFP_KERNEL);
	if (!stillmode_client_data)
		return -ENOMEM;

	g_stillmode_data = stillmode_client_data;
	stillmode_client_data->m4sensorhub = m4sensorhub;
	platform_set_drvdata(pdev, stillmode_client_data);
	stillmode_client_data->state = MOTION;
	stillmode_client_data->timeout = STILLMODE_DEFAULT_TIMEOUT;

	stillmode_client_data->input_dev = input_allocate_device();
	if (!stillmode_client_data->input_dev) {
		ret = -ENOMEM;
		KDEBUG(M4SH_ERROR, "%s: input device allocate failed: %d\n",
			__func__, ret);
		goto free_memory;
	}

	stillmode_client_data->input_dev->name = STILLMODE_CLIENT_DRIVER_NAME;
	set_bit(EV_SW, stillmode_client_data->input_dev->evbit);
	set_bit(SW_STILL_MODE, stillmode_client_data->input_dev->swbit);

	if (input_register_device(stillmode_client_data->input_dev)) {
		KDEBUG(M4SH_ERROR, "%s: input device register failed\n",
			__func__);
		input_free_device(stillmode_client_data->input_dev);
		goto free_memory;
	}

	INIT_WORK(&stillmode_client_data->queued_work,
		m4sensorhub_stillmode_work);

	ret = m4sensorhub_register_initcall(stillmode_driver_init,
							stillmode_client_data);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Unable to register init function "
			"for stillmode client = %d\n", ret);
		goto destroy_wakelock;
	}

	if (device_create_file(&pdev->dev, &dev_attr_state)) {
		KDEBUG(M4SH_ERROR, "Error creating stillmode sys entry\n");
		ret = -1;
		goto unregister_initcall;
	}

	if (device_create_file(&pdev->dev, &dev_attr_timeout)) {
		KDEBUG(M4SH_ERROR, "Error creating timeout sys entry\n");
		ret = -1;
		goto remove_stillmode_sysfs;
	}

	KDEBUG(M4SH_INFO, "Initialized %s driver\n",
		STILLMODE_CLIENT_DRIVER_NAME);

	return 0;

remove_stillmode_sysfs:
	device_remove_file(&pdev->dev, &dev_attr_state);
unregister_initcall:
	m4sensorhub_unregister_initcall(stillmode_driver_init);
destroy_wakelock:
	input_unregister_device(stillmode_client_data->input_dev);
free_memory:
	platform_set_drvdata(pdev, NULL);
	stillmode_client_data->m4sensorhub = NULL;
	kfree(stillmode_client_data);
	g_stillmode_data = NULL;

	return ret;
}

static int __exit stillmode_client_remove(struct platform_device *pdev)
{
	struct stillmode_client *stillmode_client_data =
						platform_get_drvdata(pdev);
	struct m4sensorhub_data *m4sensorhub =
					stillmode_client_data->m4sensorhub;

	device_remove_file(&pdev->dev, &dev_attr_timeout);
	device_remove_file(&pdev->dev, &dev_attr_state);
	m4sensorhub_irq_disable(m4sensorhub, M4SH_IRQ_MOTION_DETECTED);
	m4sensorhub_irq_unregister(m4sensorhub, M4SH_IRQ_MOTION_DETECTED);
	m4sensorhub_irq_disable(m4sensorhub, M4SH_IRQ_STILL_DETECTED);
	m4sensorhub_irq_unregister(m4sensorhub, M4SH_IRQ_STILL_DETECTED);
	m4sensorhub_unregister_initcall(stillmode_driver_init);
	input_unregister_device(stillmode_client_data->input_dev);
	platform_set_drvdata(pdev, NULL);
	stillmode_client_data->m4sensorhub = NULL;
	kfree(stillmode_client_data);
	g_stillmode_data = NULL;

	return 0;
}

static void stillmode_client_shutdown(struct platform_device *pdev)
{
	return;
}
#ifdef CONFIG_PM
static int stillmode_client_suspend(struct platform_device *pdev,
				pm_message_t message)
{
	return 0;
}

static int stillmode_client_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define stillmode_client_suspend NULL
#define stillmode_client_resume  NULL
#endif


static struct of_device_id m4stillmode_match_tbl[] = {
	{ .compatible = "mot,m4stillmode" },
	{},
};

static struct platform_driver stillmode_client_driver = {
	.probe		= stillmode_client_probe,
	.remove		= __exit_p(stillmode_client_remove),
	.shutdown	= stillmode_client_shutdown,
	.suspend	= stillmode_client_suspend,
	.resume		= stillmode_client_resume,
	.driver		= {
		.name	= STILLMODE_CLIENT_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4stillmode_match_tbl),
	},
};

static int __init stillmode_client_init(void)
{
	return platform_driver_register(&stillmode_client_driver);
}

static void __exit stillmode_client_exit(void)
{
	platform_driver_unregister(&stillmode_client_driver);
}

module_init(stillmode_client_init);
module_exit(stillmode_client_exit);

MODULE_ALIAS("platform:stillmode_client");
MODULE_DESCRIPTION("M4 sensorhub still mode client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");

