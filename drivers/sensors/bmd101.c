/*
 * Copyright (c) 2014, ASUStek All rights reserved.
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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/sysfs.h>
#include <linux/syscalls.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/sensors.h>
#include <linux/workqueue.h>
#include <mach/gpiomux.h>
#ifdef CONFIG_ASUS_UTILITY
#include <linux/notifier.h>
#include <linux/asus_utility.h>
#endif

#define ECG_MAJOR_NUM 0x71
#define IOCTL_SET_WELLNESS_ENABLE _IOR(ECG_MAJOR_NUM, 0, int *)
#define IOCTL_GET_WELLNESS_ENABLE _IOR(ECG_MAJOR_NUM, 1, int *)

#define bmd101_uart			"/dev/ttyHSL1"
#define bmd101_regulator		"8226_l28"
//Debug Masks +++
#define NO_DEBUG       0
#define DEBUG_POWER     1
#define DEBUG_INFO  2
#define DEBUG_VERBOSE 5
#define DEBUG_RAW      8
#define DEBUG_TRACE   10

static int debug = DEBUG_INFO;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activate debugging output");

#define sensor_debug(level, ...) \
		if (debug >= (level)) \
			pr_info(__VA_ARGS__);
//Debug Masks ---

#define BMD101_CS_GPIO		53
#define BMD101_RST_GPIO		55

#define BMD101_timeout_delay_default            300
#define SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT		-1
#define SENSOR_HEART_RATE_CONFIDENCE_UNRELIABLE		0
#define SENSOR_HEART_RATE_CONFIDENCE_LOW			1
#define SENSOR_HEART_RATE_CONFIDENCE_MEDIUM			2
#define SENSOR_HEART_RATE_CONFIDENCE_HIGH			3

#define LOW_POWER_MODE_EXIT		1
#define LOW_POWER_MODE_ENTER		0

#define LSQ_THRESHOLD	3333

#define HR_ENABLED	1
#define MOOD_ENABLED	2

static struct regulator *pm8921_l28;
static struct sensors_classdev bmd101_cdev = {
	.name = "bmd101",
	.vendor = "NeuroSky",
	.version = 3,		//v1: sensor always on, v2: sensor controlled by HAL, default off, /proc/wellness has ioctl, v3: NS SDK moved to HAL
	.enabled = 0,
	.sensors_enable = NULL,
};
struct bmd101_data {
	struct sensors_classdev cdev;
	struct input_dev *hr_dev;			/* Pointer to input device */
	struct input_dev *mood_dev;
	int enabled_function;
	int hw_enabled;
	int bpm;
	int lsq;
	int ssq;
	int mood;
	int count;
	int accuracy;
	int timeout_delay;
	int low_power_mode;
    unsigned int wellness_on;
	struct mutex lock;
	struct delayed_work timeout_work;
	struct delayed_work suspend_resume_work;
	struct workqueue_struct *bmd101_work_queue;
};
static struct bmd101_data *sensor_data;

static struct gpiomux_setting gpio_act_cfg;
static struct gpiomux_setting gpio_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
	.dir = GPIOMUX_IN,
};

static int bmd101_data_report_hr(int data, int accuracy)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: bpm(%d) accuracy(%d)\n", __func__, data, accuracy);
	input_report_abs(sensor_data->hr_dev, ABS_MISC, data);
	input_event(sensor_data->hr_dev, EV_MSC, MSC_SCAN, accuracy);
	input_sync(sensor_data->hr_dev);

	return 0;
}

static int bmd101_data_report_mood(int data, int accuracy)
{
    sensor_debug(DEBUG_INFO, "[bmd101] %s: mood(%d) accuracy(%d)\n", __func__, data, accuracy);
    input_report_abs(sensor_data->mood_dev, ABS_MISC, data);
    input_event(sensor_data->mood_dev, EV_MSC, MSC_SCAN, accuracy);
    input_sync(sensor_data->mood_dev);

    return 0;
}

static void bmd101_hw_enable(int enable) {
	int rc;

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: sensor hw currently %s turning sensor hw %s\n", __func__, sensor_data->hw_enabled? "on":"off", enable ? "on":"off");

	if (enable && !sensor_data->hw_enabled) {
		rc = regulator_enable(pm8921_l28);
    		if (rc) {
			pr_err("[bmd101] %s: regulator_enable of 8921_l28 failed(%d)\n", __func__, rc);
			goto reg_put_LDO28;
    		}
		msleep(100);

		gpio_direction_output(BMD101_CS_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_CS_GPIO, 1);
		msleep(100);

		gpio_direction_output(BMD101_RST_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_RST_GPIO, 1);
		msleep(100);

		sensor_data->hw_enabled = 1;
		
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: gpio %d and %d pulled hig, sensor hw(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, sensor_data->hw_enabled);
	}
	else if (!enable && sensor_data->hw_enabled) {
		gpio_direction_output(BMD101_RST_GPIO, 0);
		msleep(100);
		gpio_direction_output(BMD101_CS_GPIO, 0);
		msleep(100);
		rc = regulator_disable(pm8921_l28);
    		if (rc) {
			pr_err("%s: regulator_enable of 8921_l28 failed(%d)\n", __func__, rc);
			goto reg_put_LDO28;
    		}

		sensor_data->hw_enabled = 0;
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: gpio %d and %d pulled low, sensor hw(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, sensor_data->hw_enabled);
	}
	else
		pr_err("[bmd101] %s : sensor hw is already %s\n", __func__, enable ? "enabled":"disabled");

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);

	return;

	reg_put_LDO28:
	regulator_put(pm8921_l28);

	return;
	
}

static void bmd101_data_filter(int data)
{
	if(sensor_data->ssq == -1)
            sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT;
	else if(sensor_data->ssq == 5 && sensor_data->lsq > LSQ_THRESHOLD)
	    sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_HIGH;
	else if(sensor_data->enabled_function==HR_ENABLED &&sensor_data->bpm < 0)
	    sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_LOW;
	else
	    sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_UNRELIABLE;

	if(sensor_data->enabled_function == HR_ENABLED && sensor_data->accuracy > SENSOR_HEART_RATE_CONFIDENCE_LOW) {
		sensor_data->bpm = data;
		sensor_debug(DEBUG_INFO, "[bmd101] %s: bpm(%d) LSQ(%d) SSQ(%d)", __func__, sensor_data->bpm, sensor_data->lsq, sensor_data->ssq);
		bmd101_data_report_hr(sensor_data->bpm, sensor_data->accuracy);
	}
	else if(sensor_data->enabled_function == MOOD_ENABLED) {
		sensor_data->mood = data;
		sensor_debug(DEBUG_INFO, "[bmd101] %s: mood(%d) LSQ(%d) SSQ(%d)", __func__, sensor_data->mood, sensor_data->lsq, sensor_data->ssq);
		bmd101_data_report_mood(sensor_data->mood, sensor_data->accuracy);
	}

	return;
}

static int bmd101_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: sensor currently %s, turning sensor %s\n", __func__, sensor_data->cdev.enabled? "on":"off", enable ? "on":"off");
	if (!sensor_data->cdev.enabled) {
		bmd101_hw_enable(enable);
		sensor_data->timeout_delay = BMD101_timeout_delay_default;
		sensor_data->cdev.enabled = 1;
	}
	else if (enable==0 && sensor_data->cdev.enabled) {
		if(delayed_work_pending(&sensor_data->timeout_work)) {
			printk("[bmd101] timeout cancelled.\n");
			cancel_delayed_work(&sensor_data->timeout_work);
		}
		bmd101_hw_enable(enable);
		sensor_data->cdev.enabled = 0;
		sensor_data->count = 0;
		sensor_data->timeout_delay = BMD101_timeout_delay_default;
		sensor_data->enabled_function = 0;

		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: bmd101_enable(%d)\n", __func__, sensor_data->cdev.enabled);
	}
	else
		pr_err("[bmd101] %s : sensor is already %s\n", __func__, enable ? "enabled":"disabled");

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);

	return 0;
}

static void bmd101_suspend_resume_work(struct work_struct *work) {
	if(sensor_data->low_power_mode) {
		sensor_data->count = 0;
		mutex_unlock(&sensor_data->lock);
	}

	sensor_debug(DEBUG_INFO, "[bmd101] %s low power mode\n", sensor_data->low_power_mode?"enter":"exit");
}

static void bmd101_timeout_work(struct work_struct *work) {
	sensor_debug(DEBUG_INFO, "[bmd101] %s: no contact detected. TIMEOUT!! \n", __func__);
	bmd101_data_report_hr(-1, -1);
	sensor_data->count = 0;
}

static int bmd101_show_bpm (struct device *dev, struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] Heart_Beat = %d\n",sensor_data->bpm);
	return sprintf(buf, "%d\n", sensor_data->bpm);
}

static ssize_t bmd101_store_bpm(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	long val;

	if((kstrtol(buf, 10, &val)) != 0)
		return -EINVAL;
        sensor_debug(DEBUG_VERBOSE, "[bmd101] %s (%d)\n", __func__, (int)val);
	bmd101_data_filter((int)val);

	return size;
}
static DEVICE_ATTR(bpm, S_IWUSR | S_IRUGO, bmd101_show_bpm, bmd101_store_bpm);

static int bmd101_show_mood(struct device *dev, struct device_attribute *attr, char *buf)
{
    sensor_debug(DEBUG_VERBOSE, "[bmd101] Relaxation = %d\n",sensor_data->mood);
    return sprintf(buf, "%d\n", sensor_data->mood);
}

static ssize_t bmd101_store_mood(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    long val;
    if((kstrtol(buf, 10, &val)) != 0)
        return -EINVAL;
    sensor_debug(DEBUG_INFO, "[bmd101] %s (%d)\n", __func__, (int)val);
	bmd101_data_filter((int)val);
    return size;
}
static DEVICE_ATTR(mood, S_IWUSR | S_IRUGO, bmd101_show_mood, bmd101_store_mood);

static int bmd101_show_function(struct device *dev, struct device_attribute *attr, char *buf)
{
    sensor_debug(DEBUG_VERBOSE, "[bmd101] function = %d\n",sensor_data->enabled_function);
    return sprintf(buf, "%d\n", sensor_data->enabled_function);
}

static ssize_t bmd101_store_function(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    long val;

    if((kstrtol(buf, 10, &val)) != 0)
        return -EINVAL;
	sensor_data->enabled_function = (int)val;
    sensor_debug(DEBUG_VERBOSE, "[bmd101] %s (%d)\n", __func__, (int)val);
    return size;
}
static DEVICE_ATTR(function, S_IWUSR | S_IRUGO, bmd101_show_function, bmd101_store_function);


static int bmd101_show_lsq (struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", sensor_data->lsq);
}

static ssize_t bmd101_store_lsq(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	long val;

    if ((kstrtol(buf, 10, &val) != 0))
        return -EINVAL;
	sensor_data->lsq = (int)val;
    return size;
}
static DEVICE_ATTR(lsq, S_IWUSR | S_IRUGO, bmd101_show_lsq, bmd101_store_lsq);

static int bmd101_show_ssq (struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", sensor_data->ssq);
}

static ssize_t bmd101_store_ssq(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    long val;

    if ((kstrtol(buf, 10, &val) != 0))
        return -EINVAL;
    sensor_data->ssq = (int)val;
    return size;
}
static DEVICE_ATTR(ssq, S_IWUSR | S_IRUGO, bmd101_show_ssq, bmd101_store_ssq);

#ifndef ASUS_USER_BUILD
static int bmd101_show_hw_enable (struct device *dev, struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] sensor hw_enabled = %d\n",sensor_data->hw_enabled);
	return sprintf(buf, "%d\n", sensor_data->hw_enabled);
}

static ssize_t bmd101_store_hw_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;

	if ((kstrtoul(buf, 10, &val) != 0) ||(val > 1))
		return -EINVAL;

	sensor_debug(DEBUG_INFO, "[bmd101] %s (%d)\n", __func__, (int)val);

	bmd101_hw_enable((int)val);

	return size;
}
static DEVICE_ATTR(hw_enable, S_IWUSR | S_IRUGO, bmd101_show_hw_enable, bmd101_store_hw_enable);
#endif

static struct attribute *bmd101_attributes[] = {
	&dev_attr_bpm.attr,
	&dev_attr_lsq.attr,
	&dev_attr_ssq.attr,
    &dev_attr_mood.attr,
	&dev_attr_function.attr,
	#ifndef ASUS_USER_BUILD
	&dev_attr_hw_enable.attr,		//sensor hw enable/disable interface
	#endif
	NULL
};

static const struct attribute_group bmd101_attr_group = {
        .attrs = bmd101_attributes,
};

static int bmd101_proc_show_wellness(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sensor_data->cdev.version);
	return 0;
}

static int bmd101_proc_open_wellness(struct inode *inode, struct file *filp)
{
	return single_open(filp, bmd101_proc_show_wellness, NULL);
}

static long bmd101_proc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *data;
	int value = 0;
	int ret = 0;

	data = (void __user *) arg;
	ret = copy_from_user(&value, data, sizeof(int));
	if(data==NULL) {
		pr_err("[bmd101][ioctl] %s: null data!!\n", __func__);
		return -EFAULT;
	}
	switch (cmd) {
		case IOCTL_SET_WELLNESS_ENABLE:
			sensor_debug(DEBUG_INFO, "[bmd101] %s setting wellness (%d)\n", __func__, value);
			sensor_data->wellness_on = value;
			break;
		case IOCTL_GET_WELLNESS_ENABLE:
			sensor_debug(DEBUG_INFO, "[bmd101] %s getting wellness (%d)\n", __func__, value);
			ret = __put_user(sensor_data->wellness_on, (int __user *)arg);
			break;
		default:
			sensor_debug(DEBUG_INFO, "[bmd101] %s, unknown cmd(0x%x)\n", __func__, cmd);
			break;
	}

	return ret;
}

static const struct file_operations proc_bmd101_operations = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = bmd101_proc_ioctl,
	.read		= seq_read,
	.open		= bmd101_proc_open_wellness,
	.release	= single_release,
};

#ifdef CONFIG_ASUS_UTILITY
static int bmd101_fb_notifier_callback(struct notifier_block *this, unsigned long code, void *data)
{
	int err = 0;

	printk(KERN_DEBUG "[PF]%s +\n", __func__);
	switch (code) {
		case LOW_POWER_MODE_ENTER:
			sensor_data->low_power_mode = 1;
			break;
		case LOW_POWER_MODE_EXIT:
			sensor_data->low_power_mode = 0;
			break;
		default:
			err = -1;
			break;
	}

	if (err < 0)
		pr_err("[BMD101] ERROR!! unknown low power mode call back code (%lu)\n", code);
	else
		queue_delayed_work(sensor_data->bmd101_work_queue, &sensor_data->suspend_resume_work, 0);

	printk(KERN_DEBUG "[PF]%s -\n", __func__);
	return 0;
}

static struct notifier_block display_mode_notifier = {
        .notifier_call =    bmd101_fb_notifier_callback,
};
#endif

static int bmd101_input_init(void)
{
	int ret = 0;
	struct input_dev *bmd101_hr_dev = NULL;
	struct input_dev *bmd101_mood_dev = NULL;

	bmd101_hr_dev = input_allocate_device();
	if (!bmd101_hr_dev) {
		printk("[bmd101]: Failed to allocate input_data device\n");
		return -ENOMEM;
	}

	bmd101_hr_dev->name = "ASUS ECG HR";
	bmd101_hr_dev->id.bustype = BUS_HOST;
	input_set_capability(bmd101_hr_dev, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, bmd101_hr_dev->evbit);
	__set_bit(ABS_MISC, bmd101_hr_dev->absbit);
	input_set_abs_params(bmd101_hr_dev, ABS_MISC, 0, 1048576, 0, 0);
	input_set_capability(bmd101_hr_dev, EV_MSC, MSC_SCAN);
	__set_bit(MSC_SCAN, bmd101_hr_dev->mscbit);
	input_set_drvdata(bmd101_hr_dev, sensor_data);
	ret = input_register_device(bmd101_hr_dev);
	if (ret < 0) {
		input_free_device(bmd101_hr_dev);
		return ret;
	}
	sensor_data->hr_dev = bmd101_hr_dev;

        bmd101_mood_dev = input_allocate_device();
        if (!bmd101_mood_dev) {
                printk("[bmd101]: Failed to allocate input_data device\n");
                return -ENOMEM;
        }

        bmd101_mood_dev->name = "ASUS ECG MOOD";
        bmd101_mood_dev->id.bustype = BUS_HOST;
        input_set_capability(bmd101_mood_dev, EV_ABS, ABS_MISC);
        __set_bit(EV_ABS, bmd101_mood_dev->evbit);
        __set_bit(ABS_MISC, bmd101_mood_dev->absbit);
        input_set_abs_params(bmd101_mood_dev, ABS_MISC, 0, 1048576, 0, 0);
        input_set_capability(bmd101_mood_dev, EV_MSC, MSC_SCAN);
        __set_bit(MSC_SCAN, bmd101_mood_dev->mscbit);
        input_set_drvdata(bmd101_mood_dev, sensor_data);
        ret = input_register_device(bmd101_mood_dev);
        if (ret < 0) {
                input_free_device(bmd101_mood_dev);
                return ret;
        }
        sensor_data->mood_dev = bmd101_mood_dev;

	return 0;
}

static int bmd101_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);

	sensor_data = kzalloc(sizeof(struct bmd101_data),GFP_KERNEL);
	if(!sensor_data)
	{
		pr_err("[BMD101] %s: failed to allocate bmd101_data\n", __func__);
		goto allocate_data_fail;
	}
	sensor_data->bpm = 0;
	sensor_data->lsq = 0;
	sensor_data->ssq = 0;
	sensor_data->mood = 0;
	sensor_data->accuracy = 0;
	sensor_data->enabled_function = 0;
	sensor_data->hw_enabled = 0;
	sensor_data->wellness_on = 0;
	sensor_data->timeout_delay = BMD101_timeout_delay_default;
	sensor_data->cdev = bmd101_cdev;
	sensor_data->cdev.enabled = 0;
	sensor_data->cdev.sensors_enable = bmd101_enable_set;
	mutex_init(&sensor_data->lock);
	
   	sensor_data->bmd101_work_queue = create_workqueue("BMD101_wq");
	INIT_DELAYED_WORK(&sensor_data->timeout_work, bmd101_timeout_work);
	INIT_DELAYED_WORK(&sensor_data->suspend_resume_work, bmd101_suspend_resume_work);
	
	ret = sensors_classdev_register(&pdev->dev, &sensor_data->cdev);
	if (ret) {
		pr_err("[BMD101] %s: class device create failed: %d\n", __func__, ret);
		goto classdev_register_fail;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &bmd101_attr_group);
	if (ret)
		goto classdev_register_fail;

	proc_create("wellness", 0, NULL, &proc_bmd101_operations);

	ret = bmd101_input_init();
	if (ret < 0) {
		pr_err("[BMD101] %s: init input device failed: %d\n", __func__, ret);
		goto input_init_fail;
	}
	
	#ifdef CONFIG_ASUS_UTILITY
	register_mode_notifier(&display_mode_notifier);
	#endif

	pm8921_l28 = regulator_get(&pdev->dev, bmd101_regulator);
	if (IS_ERR(pm8921_l28)) {
        	pr_err("[bmd101] %s: regulator get of 8921_l28 failed (%ld)\n", __func__, PTR_ERR(pm8921_l28));
		ret = PTR_ERR(pm8921_l28);
		goto regulator_get_fail;
	}
	
    	ret = regulator_set_voltage(pm8921_l28, 2850000, 2850000);
    	if (ret) {
		pr_err("[bmd101] %s: regulator_set_voltage of 8921_l28 failed(%d)\n", __func__, ret);
		goto reg_put_LDO28;
    	}

    	ret = gpio_request(BMD101_RST_GPIO, "BMD101_RST");
    	if (ret) {
		pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_RST_GPIO);
		goto gpio_rst_fail;
    	}
    	ret = gpio_request(BMD101_CS_GPIO, "BMD101_CS");
    	if (ret) {
		pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_CS_GPIO);
		goto gpio_cs_fail;
    	}

    	msm_gpiomux_write(BMD101_CS_GPIO, GPIOMUX_ACTIVE, &gpio_act_cfg, &gpio_sus_cfg);
    	msm_gpiomux_write(BMD101_RST_GPIO, GPIOMUX_ACTIVE, &gpio_act_cfg, &gpio_sus_cfg);

	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;

	gpio_cs_fail:
	gpio_free(BMD101_CS_GPIO);
	gpio_rst_fail:
	gpio_free(BMD101_RST_GPIO);
	reg_put_LDO28:
	regulator_put(pm8921_l28);
	regulator_get_fail:
	sensors_classdev_unregister(&sensor_data->cdev);
	input_init_fail:
	classdev_register_fail:
	kfree(sensor_data);
	allocate_data_fail:

	return ret;

}

static int bmd101_remove(struct platform_device *pdev)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	#ifdef CONFIG_ASUS_UTILITY
	unregister_mode_notifier(&display_mode_notifier);
	#endif
	gpio_free(BMD101_RST_GPIO);
	gpio_free(BMD101_CS_GPIO);

	return 0;
}

static struct of_device_id bmd101_match_table[] = {
	{ .compatible = "neurosky, bmd101",},
	{},
};

static struct platform_driver bmd101_platform_driver = {
	.probe = bmd101_probe,
	.remove = bmd101_remove,
	.driver = {
		.name = "bmd101",
		.owner = THIS_MODULE,
		.of_match_table = bmd101_match_table,
	},
};

static int __init bmd101_init(void)
{
	return platform_driver_register(&bmd101_platform_driver);
}

static void __exit bmd101_exit(void)
{
	return platform_driver_unregister(&bmd101_platform_driver);
}

module_init(bmd101_init);
module_exit(bmd101_exit);

MODULE_DESCRIPTION("Driver for ECG sensor");
MODULE_LICENSE("GPL v2");
