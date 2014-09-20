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

#define BMD101_filter_array          10
#define BMD101_setup_time		5
#define SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT		-1
#define SENSOR_HEART_RATE_CONFIDENCE_UNRELIABLE		0
#define SENSOR_HEART_RATE_CONFIDENCE_LOW			1
#define SENSOR_HEART_RATE_CONFIDENCE_MEDIUM			2
#define SENSOR_HEART_RATE_CONFIDENCE_HIGH			3

static struct regulator *pm8921_l28;
static struct sensors_classdev bmd101_cdev = {
	.name = "bmd101",
	.vendor = "NeuroSky",
	.version = 1,
	.enabled = 0,
	.sensors_enable = NULL,
};
struct bmd101_data {
	struct sensors_classdev cdev;
	struct input_dev		*input_dev;			/* Pointer to input device */
	int hw_enabled;
	unsigned int  bpm;
	int esd;
	int status;
	int count;
	int accuracy;
	int timeout_delay;
	struct mutex lock;
	struct delayed_work timeout_work;
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

static int bmd101_data_report(int data, int accuracy)
{
	int report_data;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: bpm(%d) accuracy(%d)\n", __func__, data, accuracy);
	switch(accuracy) {
		case SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT:
			report_data = -1;
			break;
		case SENSOR_HEART_RATE_CONFIDENCE_LOW:
			report_data = data + 1000;
			break;
		case SENSOR_HEART_RATE_CONFIDENCE_MEDIUM:
			report_data = data + 2000;
			break;
		case SENSOR_HEART_RATE_CONFIDENCE_HIGH:
			report_data = data;
			break;
		default:
			report_data = 0;
	}
	input_report_abs(sensor_data->input_dev, ABS_MISC, report_data);
	input_sync(sensor_data->input_dev);

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

              rc = gpio_request(BMD101_RST_GPIO, "BMD101_RST");
              if (rc) {
	            pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_RST_GPIO);
	            goto gpio_rst_fail;
	       }
              rc = gpio_request(BMD101_CS_GPIO, "BMD101_CS");
              if (rc) {
	            pr_err("[bmd101] %s: Failed to request gpio %d\n", __func__, BMD101_CS_GPIO);
	            goto gpio_cs_fail;
              }

              msm_gpiomux_write(BMD101_CS_GPIO, GPIOMUX_ACTIVE, &gpio_act_cfg, &gpio_sus_cfg);
              msm_gpiomux_write(BMD101_RST_GPIO, GPIOMUX_ACTIVE, &gpio_act_cfg, &gpio_sus_cfg);

		gpio_direction_output(BMD101_CS_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_CS_GPIO, 1);
		msleep(100);

		gpio_direction_output(BMD101_RST_GPIO, 0);
		usleep(300);
		gpio_direction_output(BMD101_RST_GPIO, 1);
		msleep(100);

		sensor_data->hw_enabled = 1;
		
		sensor_debug(DEBUG_INFO, "[bmd101] %s: gpio %d and %d pulled hig, sensor hw(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, sensor_data->hw_enabled);
	}
	else if (!enable && sensor_data->hw_enabled) {
              msm_gpiomux_write(BMD101_CS_GPIO, GPIOMUX_ACTIVE, &gpio_sus_cfg, &gpio_act_cfg);
              msm_gpiomux_write(BMD101_RST_GPIO, GPIOMUX_ACTIVE, &gpio_sus_cfg, &gpio_act_cfg);

		rc = regulator_disable(pm8921_l28);
    		if (rc) {
			pr_err("%s: regulator_enable of 8921_l28 failed(%d)\n", __func__, rc);
			goto reg_put_LDO28;
    		}

		gpio_free(BMD101_RST_GPIO);
		gpio_free(BMD101_CS_GPIO);

		sensor_data->hw_enabled = 0;
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: gpio %d and %d pulled low, sensor hw(%d)\n", __func__, BMD101_CS_GPIO, BMD101_RST_GPIO, sensor_data->hw_enabled);
	}
	else
		pr_err("[bmd101] %s : sensor hw is already %s\n", __func__, enable ? "enabled":"disabled");

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);

	return;

	gpio_rst_fail:
	gpio_free(BMD101_RST_GPIO);
	gpio_cs_fail:
	gpio_free(BMD101_CS_GPIO);
	reg_put_LDO28:
	regulator_put(pm8921_l28);

	return;
	
}

static void sortArray(int *nums) {
	static int x;
	static int y;

	for(x=0; x<BMD101_filter_array; x++) {
		for(y=0; y<BMD101_filter_array-1; y++) {
			if(nums[y]>nums[y+1]) {
				int temp = nums[y+1];
				nums[y+1] = nums[y];
				nums[y] = temp;
			}
		}
	}
}

static int findMode(int *nums) {
	int i, maxCount, modeValue;
	int freq[BMD101_filter_array] = {0};
	int temp;

	temp = 0;
	maxCount = 0;
	modeValue = 0;

	for (i = 0; i < BMD101_filter_array; i++) {
		if (nums[i] > maxCount)
			maxCount = nums[i];
		else {
			if (freq[i-1] > 0)
				freq[i] = freq[i-1]+1;
			else
				freq[i]++;
		}

		if(freq[i] > temp) {
			temp = freq[i];
			modeValue = nums[i];
		}
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: bpm(%d) freq(%d)\n", __func__, nums[i], freq[i]);
	}

	if(temp==0)
		return -1;
	else
		return modeValue;
}

static void bmd101_data_filter(int data)
{
	static int bpm[BMD101_filter_array];
	static int setup_complete = 0;

	mutex_lock(&sensor_data->lock);
	if(sensor_data->status >= 2) {
		sensor_data->timeout_delay = 0;
		if(delayed_work_pending(&sensor_data->timeout_work)) {
			sensor_debug(DEBUG_INFO, "[bmd101] timeout cancelled.\n");
			cancel_delayed_work_sync(&sensor_data->timeout_work);
		}

		if((sensor_data->count <= BMD101_setup_time) && !setup_complete) {
			if(sensor_data->count == BMD101_setup_time) {
				sensor_data->count = 0;
				setup_complete = 1;
			}
			else
				sensor_data->count++;
			sensor_data->bpm = 0;
			mutex_unlock(&sensor_data->lock);
			return;
		}

		sensor_debug(DEBUG_INFO, "[bmd101] %s: (%d) %s count=%d timeout_delay=%d\n", __func__, data, sensor_data->status<2?"DROP":"COLLECT", sensor_data->count, sensor_data->timeout_delay);
		if(sensor_data->count < BMD101_filter_array) {
			bpm[sensor_data->count++] = data;
			sensor_data->bpm = data;
			sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_LOW;
		}
		else {
			setup_complete = 0;
			sensor_data->count = 0;
			sortArray(bpm);
			sensor_data->bpm = findMode(bpm);
			if(sensor_data->bpm == -1) {
				sensor_debug(DEBUG_INFO, "[bmd101] %s: bad contact, request user retest\n", __func__);
				sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT;
			}
			else
				sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_HIGH;
		}

		bmd101_data_report(sensor_data->bpm, sensor_data->accuracy);
	}
	else
		queue_delayed_work(sensor_data->bmd101_work_queue, &sensor_data->timeout_work, sensor_data->timeout_delay);		//queue timeout delay first time: @3s in between measurements: @1s
	mutex_unlock(&sensor_data->lock);

	return;
}

static int bmd101_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: sensor currently %s, turning sensor %s\n", __func__, sensor_data->cdev.enabled? "on":"off", enable ? "on":"off");
	if (enable && !sensor_data->cdev.enabled) {
		if (!sensor_data->hw_enabled)
			bmd101_hw_enable(1);

		sensor_data->timeout_delay = 300;
		sensor_data->cdev.enabled = 1;
	}
	else if (!enable && sensor_data->cdev.enabled) {
		if(delayed_work_pending(&sensor_data->timeout_work)) {
			printk("[bmd101] timeout cancelled.\n");
			cancel_delayed_work(&sensor_data->timeout_work);
		}
			
		sensor_data->cdev.enabled = 0;
		sensor_data->status = 0;
		sensor_data->count = 0;
		sensor_data->timeout_delay = 300;

		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: bmd101_enable(%d)\n", __func__, sensor_data->cdev.enabled);
	}
	else
		pr_err("[bmd101] %s : sensor is already %s\n", __func__, enable ? "enabled":"disabled");

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s ---\n", __func__);

	return 0;
}

static void bmd101_timeout_work(struct work_struct *work) {
	sensor_debug(DEBUG_INFO, "[bmd101] %s: no contact detected. TIMEOUT!! \n", __func__);
	bmd101_data_report(-1, -1);
	sensor_data->count = 0;
}

//ASUS_BSP +++ Maggie_Lee "Add ATD interface"
static ssize_t sensors_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", sensor_data->status);
}

static ssize_t sensors_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 2))
		return -EINVAL;

	sensor_data->status = val;

	return size;
}
static DEVICE_ATTR(status, S_IWUSR | S_IRUGO, sensors_status_show, sensors_status_store);
//ASUS_BSP --- Maggie_Lee "Add ATD interface"

//ASUS_BSP +++ Maggie_Lee "Add ESD interface"
static ssize_t bmd101_show_esd(struct device *dev, struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] ESD = %d\n",sensor_data->esd);
	return sprintf(buf, "%d\n", sensor_data->esd);
}

static ssize_t bmd101_store_esd(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 1))
		return -EINVAL;

	sensor_debug(DEBUG_VERBOSE, "[als_P01] %s (%d)\n", __func__, (int)val);
	sensor_data->esd = val;

	return size;
}
static DEVICE_ATTR(esd, S_IWUSR | S_IRUGO, bmd101_show_esd, bmd101_store_esd);
//ASUS_BSP --- Maggie_Lee "Add ESD interface"

static int bmd101_show_bpm (struct device *dev, struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] Heart_Beat = %d\n",sensor_data->bpm);
	return sprintf(buf, "%d\n", sensor_data->bpm);
}

static ssize_t bmd101_store_bpm(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;

	if ((strict_strtoul(buf, 10, &val) < 0))
		return -EINVAL;

	sensor_debug(DEBUG_VERBOSE, "[bmd101] %s (%d)\n", __func__, (int)val);

	bmd101_data_filter((int)val);

	return size;
}
static DEVICE_ATTR(bpm, S_IWUSR | S_IRUGO, bmd101_show_bpm, bmd101_store_bpm);

static int bmd101_show_hw_enable (struct device *dev, struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_VERBOSE, "[bmd101] sensor hw_enabled = %d\n",sensor_data->hw_enabled);
	return sprintf(buf, "%d\n", sensor_data->hw_enabled);
}

static ssize_t bmd101_store_hw_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;

	if ((strict_strtoul(buf, 10, &val) < 0) ||(val > 1))
		return -EINVAL;

	sensor_debug(DEBUG_INFO, "[bmd101] %s (%d)\n", __func__, (int)val);

	bmd101_hw_enable((int)val);

	return size;
}
static DEVICE_ATTR(hw_enable, S_IWUSR | S_IRUGO, bmd101_show_hw_enable, bmd101_store_hw_enable);

static struct attribute *bmd101_attributes[] = {
	&dev_attr_status.attr,			//atd interface
	&dev_attr_bpm.attr,
	#ifndef ASUS_USER_BUILD
	&dev_attr_esd.attr,				//esd interface
	&dev_attr_hw_enable.attr,		//sensor hw enable/disable interface
	#endif

	NULL
};

static const struct attribute_group bmd101_attr_group = {
	.attrs = bmd101_attributes,
};

void notify_ecg_sensor_lowpowermode(int low)
{
	if(low)
		bmd101_hw_enable(0);
	else
		bmd101_hw_enable(1);
	sensor_debug(DEBUG_INFO, "[bmd101] %s low power mode\n", low?"enter":"exit");
}

#ifdef CONFIG_ASUS_UTILITY
static int bmd101_fb_notifier_callback(struct notifier_block *this, unsigned long code, void *data)
{
	switch (code) {
		case 0:
			notify_ecg_sensor_lowpowermode(1);
			sensor_data->count = 0;
			mutex_unlock(&sensor_data->lock);
			break;
		case 1:
			notify_ecg_sensor_lowpowermode(0);
			break;
		default:
			break;
	}

	return 0;
}

static struct notifier_block display_mode_notifier = {
        .notifier_call =    bmd101_fb_notifier_callback,
};
#endif

static int bmd101_input_init(void)
{
	int ret = 0;
	struct input_dev *bmd101_dev = NULL;

	bmd101_dev = input_allocate_device();
	if (!bmd101_dev) {
		printk("[bmd101]: Failed to allocate input_data device\n");
		return -ENOMEM;
	}

	bmd101_dev->name = "ASUS ECG";
	bmd101_dev->id.bustype = BUS_HOST;
	input_set_capability(bmd101_dev, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, bmd101_dev->evbit);
	__set_bit(ABS_MISC, bmd101_dev->absbit);
	input_set_abs_params(bmd101_dev, ABS_MISC, 0, 1048576, 0, 0);
	input_set_drvdata(bmd101_dev, sensor_data);
	ret = input_register_device(bmd101_dev);
	if (ret < 0) {
		input_free_device(bmd101_dev);
		return ret;
	}
	sensor_data->input_dev = bmd101_dev;

	return 0;
}

static int bmd101_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);

	sensor_data = kzalloc(sizeof(struct bmd101_data),GFP_KERNEL);
	if(!sensor_data)
	{
		printk(KERN_ERR "%s: failed to allocate bmd101_data\n", __func__);
		return -ENOMEM;
	}
	sensor_data->esd = 0;
	sensor_data->bpm = 0;
	sensor_data->accuracy = 0;
	sensor_data->hw_enabled = 0;
	sensor_data->timeout_delay = 300;
	sensor_data->cdev = bmd101_cdev;
	sensor_data->cdev.enabled = 0;
	sensor_data->cdev.sensors_enable = bmd101_enable_set;
	mutex_init(&sensor_data->lock);
	
   	sensor_data->bmd101_work_queue = create_workqueue("BMD101_wq");
	INIT_DELAYED_WORK(&sensor_data->timeout_work, bmd101_timeout_work);
	
	ret = sensors_classdev_register(&pdev->dev, &sensor_data->cdev);
	if (ret) {
		pr_err("[BMD101] class device create failed: %d\n", ret);
		goto classdev_register_fail;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &bmd101_attr_group);
	if (ret)
		goto classdev_register_fail;

	ret = bmd101_input_init();
	if (ret < 0) {
		pr_err("[BMD101] init input device failed: %d\n", ret);
		goto input_init_fail;
	}
	
	#ifdef CONFIG_ASUS_UTILITY
	register_mode_notifier(&display_mode_notifier);
	#endif

	pm8921_l28 = regulator_get(&pdev->dev, bmd101_regulator);
	if (IS_ERR(pm8921_l28)) {
        	pr_err("%s: regulator get of 8921_l28 failed (%ld)\n", __func__, PTR_ERR(pm8921_l28));
		ret = PTR_ERR(pm8921_l28);
		goto regulator_get_fail;
	}
	
    	ret = regulator_set_voltage(pm8921_l28, 2850000, 2850000);
    	if (ret) {
		pr_err("%s: regulator_set_voltage of 8921_l28 failed(%d)\n", __func__, ret);
		goto reg_put_LDO28;
    	}

	sensor_debug(DEBUG_INFO, "[bmd101] %s: ---\n", __func__);

	return 0;

	reg_put_LDO28:
	regulator_put(pm8921_l28);
	regulator_get_fail:
	sensors_classdev_unregister(&sensor_data->cdev);
	input_init_fail:
	classdev_register_fail:
	kfree(sensor_data);

	return ret;

}

static int bmd101_remove(struct platform_device *pdev)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: +++\n", __func__);
	#ifdef CONFIG_ASUS_UTILITY
	unregister_mode_notifier(&display_mode_notifier);
	#endif

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
