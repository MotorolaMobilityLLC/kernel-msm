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

#define BMD101_filter_array          10
#define BMD101_setup_time		5
#define BMD101_timeout_delay_default            300
#define SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT		-1
#define SENSOR_HEART_RATE_CONFIDENCE_UNRELIABLE		0
#define SENSOR_HEART_RATE_CONFIDENCE_LOW			1
#define SENSOR_HEART_RATE_CONFIDENCE_MEDIUM			2
#define SENSOR_HEART_RATE_CONFIDENCE_HIGH			3

#define LOW_POWER_MODE_EXIT		1
#define LOW_POWER_MODE_ENTER		0

static struct regulator *pm8921_l28;
struct bpm_array{
	int bpm;
	int weight;
	int freq;
};
static struct sensors_classdev bmd101_cdev = {
	.name = "bmd101",
	.vendor = "NeuroSky",
	.version = 2,				//v1: sensor always on, v2: sensor controlled by HAL, default off, /proc/wellness has ioctl
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

static int bmd101_data_report(int data, int accuracy)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: bpm(%d) accuracy(%d)\n", __func__, data, accuracy);
	input_report_abs(sensor_data->input_dev, ABS_MISC, data);
	input_event(sensor_data->input_dev, EV_MSC, MSC_SCAN, accuracy);
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

static void sortArray(struct bpm_array *array) {
	static int x;
	static int y;

	for(x=0; x<BMD101_filter_array; x++) {
		for(y=0; y<BMD101_filter_array-1; y++) {
			if(array[y].bpm>array[y+1].bpm) {
				int temp = array[y+1].bpm;
				array[y+1].bpm = array[y].bpm;
				array[y].bpm = temp;
				temp = array[y+1].weight;
				array[y+1].weight = array[y].weight;
				array[y].weight = temp;
			}
		}
	}
}

static int findMode(struct bpm_array *array) {
	int i, maxCount, modeValue;
	int maxFreq, maxWeight;

	maxFreq = 0;
	maxCount = 0;
	modeValue = 0;

	for (i = 0; i < BMD101_filter_array; i++) {
		if (array[i].bpm > maxCount)
			maxCount = array[i].bpm;
		else {
			if (array[i-1].freq > 0)
				array[i].freq = array[i-1].freq+1;
			else
				array[i].freq++;
		}

		if(array[i].freq> maxFreq) {
			maxFreq = array[i].freq;
			modeValue = array[i].bpm;
		}
		else if (array[i].freq == maxFreq) {
			if (array[i].weight > maxWeight) {
				maxWeight = array[i].weight;
				modeValue = array[i].bpm;
			}
		}
		sensor_debug(DEBUG_VERBOSE, "[bmd101] %s: bpm(%d) freq(%d) weight(%d)\n", __func__, array[i].bpm, array[i].freq, array[i].weight);
	}

	if(maxFreq==0)
		return -1;
	else
		return modeValue;
}

static void bmd101_data_filter(int data)
{
	static struct bpm_array input[BMD101_filter_array];
	static int setup_complete = 0;
	int i=0;

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
				for(i=0; i<BMD101_filter_array; i++) {
					input[i].bpm = 0;
					input[i].weight = 0;
					input[i].freq = 0;
				}
			}
			else
				sensor_data->count++;
			sensor_data->bpm = 0;
			mutex_unlock(&sensor_data->lock);
			return;
		}

		sensor_debug(DEBUG_INFO, "[bmd101] %s: (%d) %s count=%d timeout_delay=%d\n", __func__, data, sensor_data->status<2?"DROP":"COLLECT", sensor_data->count, sensor_data->timeout_delay);
		if(sensor_data->count < BMD101_filter_array) {
			input[sensor_data->count].bpm = data;
			input[sensor_data->count].weight = sensor_data->count;
			sensor_data->count++;
			sensor_data->bpm = data;
			sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_LOW;
		}
		else {
			setup_complete = 0;
			sensor_data->count = 0;
			sortArray(input);
			sensor_data->bpm = findMode(input);
			if(sensor_data->bpm == -1) {
				sensor_debug(DEBUG_INFO, "[bmd101] %s: bad contact, request user retest\n", __func__);
				sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_NO_CONTACT;
			}
			else
				sensor_data->accuracy = SENSOR_HEART_RATE_CONFIDENCE_HIGH;
		}

		bmd101_data_report(sensor_data->bpm, sensor_data->accuracy);
	}
	else {
		sensor_data->count = 0;
		setup_complete = 0;
		queue_delayed_work(sensor_data->bmd101_work_queue, &sensor_data->timeout_work, sensor_data->timeout_delay);		//queue timeout delay first time: @3s in between measurements: @1s
	}
	mutex_unlock(&sensor_data->lock);

	return;
}

static int bmd101_enable_set(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	sensor_debug(DEBUG_INFO, "[bmd101] %s: sensor currently %s, turning sensor %s\n", __func__, sensor_data->cdev.enabled? "on":"off", enable ? "on":"off");
	if (enable && !sensor_data->cdev.enabled) {
		bmd101_hw_enable(enable);
		sensor_data->timeout_delay = BMD101_timeout_delay_default;
		sensor_data->cdev.enabled = 1;
	}
	else if (!enable && sensor_data->cdev.enabled) {
		if(delayed_work_pending(&sensor_data->timeout_work)) {
			printk("[bmd101] timeout cancelled.\n");
			cancel_delayed_work(&sensor_data->timeout_work);
		}
		bmd101_hw_enable(enable);
		sensor_data->cdev.enabled = 0;
		sensor_data->status = 0;
		sensor_data->count = 0;
		sensor_data->timeout_delay = BMD101_timeout_delay_default;

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
#ifndef ASUS_USER_BUILD
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
#endif
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

#ifndef ASUS_USER_BUILD
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
#endif

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
	input_set_capability(bmd101_dev, EV_MSC, MSC_SCAN);
	__set_bit(MSC_SCAN, bmd101_dev->mscbit);
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
		pr_err("[BMD101] %s: failed to allocate bmd101_data\n", __func__);
		goto allocate_data_fail;
	}
	sensor_data->esd = 0;
	sensor_data->bpm = 0;
	sensor_data->accuracy = 0;
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
