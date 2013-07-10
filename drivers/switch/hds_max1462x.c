/*
 *  drivers/switch/hds_max1462x.c
 *
 *  MAX1462x 3.5 PI Headset detection driver using max1462x.
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Copyright (C) 2009 ~ 2010 LGE, Inc.
 * Author: Lee SungYoung <lsy@lge.com>
 *
 * Copyright (C) 2010 LGE, Inc.
 * Author: Kim Eun Hye <ehgrace.kim@lge.com>
 *
 * Copyright (C) 2011 LGE, Inc.
 * Author: Yoon Gi Souk <gisouk.yoon@lge.com>
 *
 * Copyright (C) 2012 LGE, Inc.
 * Author: Park Gyu Hwa <gyuhwa.park@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Interface is following;
 * source file is
 * android/frameworks/base/services/java/com/android/server/HeadsetObserver.java
 * HEADSET_UEVENT_MATCH = "DEVPATH=/sys/devices/virtual/switch/h2w"
 * HEADSET_STATE_PATH = /sys/class/switch/h2w/state
 * HEADSET_NAME_PATH = /sys/class/switch/h2w/name
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/platform_data/hds_max1462x.h>
#include <linux/jiffies.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/qpnp/qpnp-adc.h>
#include <mach/gpiomux.h>

#define HOOK_MIN		0
#define HOOK_MAX		150000
#define VUP_MIN			150000
#define VUP_MAX			400000
#define VDOWN_MIN		400000
#define VDOWN_MAX		600000
#define ADC_PORT_NUM    	P_MUX6_1_1

#define JACK_NONE               0
#define JACK_HEADPHONE_3_POLE   1
#define JACK_HEADPSET_4_POLE    2

/* TODO */
/* 1. coding for additional excetion case in probe function */
/* 2. additional sleep related/excetional case  */


static struct workqueue_struct *local_max1462x_workqueue;

static struct wake_lock ear_hook_wake_lock;

struct ear_3button_info_table {
	unsigned int ADC_HEADSET_BUTTON;
	int PERMISS_REANGE_MAX;
	int PERMISS_REANGE_MIN;
	int PRESS_OR_NOT;
};

static struct ear_3button_info_table max1462x_ear_3button_type_data[]={
	{KEY_MEDIA, HOOK_MAX, HOOK_MIN, 0},
	{KEY_VOLUMEUP, VUP_MAX, VUP_MIN, 0},
	{KEY_VOLUMEDOWN, VDOWN_MAX, VDOWN_MIN, 0}
};

struct max1462x_hsd_info {
	/* function devices provided by this driver */
	struct switch_dev sdev;
	struct input_dev *input;

	/* mutex */
	struct mutex mutex_lock;

	/* h/w configuration : initilized by platform data */

	/* MODE : high, low, high-z */
	unsigned int gpio_mic_en;

	/* SWD : to detect 3pole or 4pole to detect among hook,
	 * volum up or down key */
	unsigned int gpio_key;

	/* DET : to detect jack inserted or not */
	unsigned int gpio_detect;

	unsigned int latency_for_key;

	unsigned int key_code;	/* KEY_MEDIA, KEY_VOLUMEUP or KEY_VOLUMEDOWN */

	/* irqs */
	unsigned int irq_detect;	/* detect */
	unsigned int irq_key;		/* key */

	/* internal states */
	atomic_t irq_key_enabled;
	atomic_t is_3_pole_or_not;
	atomic_t btn_state;

	/* work for detect_work */
	struct work_struct work;
	struct delayed_work work_for_key_pressed;
	struct delayed_work work_for_key_released;

	unsigned char *pdev_name;
};

enum {
	NO_DEVICE   = 0,
	MAX1642X_HEADSET = (1 << 0),
	MAX1642X_HEADSET_NO_MIC = (1 << 1),
};

static ssize_t max1462x_hsd_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
		case NO_DEVICE:
			return sprintf(buf, "No Device");
		case MAX1642X_HEADSET:
			return sprintf(buf, "Headset");
		case MAX1642X_HEADSET_NO_MIC:
			return sprintf(buf, "Headset");
		default:
			break;
	}
	return -EINVAL;
}

static ssize_t max1462x_hsd_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static void max1462x_button_pressed(struct work_struct *work)
{
	struct delayed_work *dwork =
		container_of(work, struct delayed_work, work);
	struct max1462x_hsd_info *hi =
		container_of(dwork, struct max1462x_hsd_info,
				work_for_key_pressed);
	struct qpnp_vadc_result result;
	int acc_read_value = 0;
	int i, rc;
	struct ear_3button_info_table *table;
	int table_size = ARRAY_SIZE(max1462x_ear_3button_type_data);

	if (gpio_get_value(hi->gpio_detect)) {
		pr_err("%s: hs_pressed but jack plugged out already! "
		       "ignore event\n",
			__func__);
		return;
	}

	rc = qpnp_vadc_read(ADC_PORT_NUM, &result);
	if (rc < 0) {
		if (rc == -ETIMEDOUT)
			pr_err("%s: adc read timeout\n",
					__func__);
		else
			pr_err("%s: adc read error - %d\n",
					__func__, rc);
	} else {
		acc_read_value = (int)result.physical;
		pr_debug("%s: acc_read_value - %d\n", __func__, acc_read_value);
	}

	pr_debug("%s: hs_pressed!\n", __func__);
	for (i = 0; i < table_size; i++) {
		table = &max1462x_ear_3button_type_data[i];
		/*
		* include min value '=' for 1 button earjack (ADC value= 0)
		 */
		if (acc_read_value > table->PERMISS_REANGE_MAX ||
		    acc_read_value < table->PERMISS_REANGE_MIN)
			continue;

		atomic_set(&hi->btn_state, 1);
		switch (table->ADC_HEADSET_BUTTON) {
			case  KEY_MEDIA :
				input_report_key(hi->input, KEY_MEDIA, 1);
				pr_info("hs_pressed: KEY_MEDIA\n");
				break;
			case KEY_VOLUMEUP :
				input_report_key(hi->input, KEY_VOLUMEUP, 1);
				pr_info("hs_pressed: KEY_VOLUMEUP\n");
				break;
			case KEY_VOLUMEDOWN :
				input_report_key(hi->input, KEY_VOLUMEDOWN, 1);
				pr_info("hs_pressed: KEY_VOLUMDOWN\n");
				break;
			default:
				pr_info("hs_pressed: UNDEFINED\n");
				break;
		}
		table->PRESS_OR_NOT = 1;
		input_sync(hi->input);
		break;
	}
}

static void max1462x_button_released(struct work_struct *work)
{
	struct delayed_work *dwork =
		container_of(work, struct delayed_work, work);
	struct max1462x_hsd_info *hi =
		container_of(dwork, struct max1462x_hsd_info,
				work_for_key_released);
	struct ear_3button_info_table *table;
	int table_size = ARRAY_SIZE(max1462x_ear_3button_type_data);
	int i;

	if (gpio_get_value(hi->gpio_detect)){
		pr_warn("%s: hs_released but jack plugged out already! "
			"ignore event\n",
			__func__);
		return;
	}

	pr_debug("%s: hs_released!\n", __func__);
	for (i = 0; i < table_size; i++) {
		table = &max1462x_ear_3button_type_data[i];
		if (!table->PRESS_OR_NOT)
			continue;

		atomic_set(&hi->btn_state, 0);
		switch (table->ADC_HEADSET_BUTTON) {
			case  KEY_MEDIA :
				input_report_key(hi->input, KEY_MEDIA, 0);
				pr_info("hs_released: KEY_MEDIA\n");
				break;
			case KEY_VOLUMEUP :
				input_report_key(hi->input, KEY_VOLUMEUP, 0);
				pr_info("hs_released: KEY_VOLUMEUP\n");
				break;
			case KEY_VOLUMEDOWN :
				input_report_key(hi->input, KEY_VOLUMEDOWN, 0);
				pr_info("hs_released: KEY_VOLUMEDOWN\n");
				break;
			default:
				pr_info("hs_released: UNDEFINED\n");
				break;
		}
		table->PRESS_OR_NOT = 0;
		input_sync(hi->input);
		break;
	}
}

static void max1462x_insert_headset(struct max1462x_hsd_info *hi)
{
	int earjack_type;

	pr_info("%s\n", __func__);

        /* If you reduce the delay time, it will cause problems. */
	msleep(40);

	/* Issue : Recognized 3-pole after reboot from 4-pole earjack plug. */

	/* check if 3-pole or 4-pole
	 * 1. read gpio_key
	 * 2. check if 3-pole or 4-pole
	 * 3-1. NOT regiter irq with gpio_key if 3-pole. complete.
	 * 3-2. regiter irq with gpio_key if 4-pole
	 * 4. read MPP6 and decide a pressed key when interrupt occurs
	 */
	earjack_type = gpio_get_value(hi->gpio_key);
	if (earjack_type == 1) {
		pr_debug("%s: 4 polarity earjack\n", __func__);
		atomic_set(&hi->is_3_pole_or_not, 0);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, MAX1642X_HEADSET);
		input_report_switch(hi->input, SW_HEADPHONE_INSERT, 1);
		input_report_switch(hi->input, SW_MICROPHONE_INSERT, 1);
		input_sync(hi->input);
		mutex_unlock(&hi->mutex_lock);

		if (!atomic_read(&hi->irq_key_enabled)) {
			pr_debug("%s: irq_key_enabled = FALSE, key IRQ = %d\n",
				__func__, hi->irq_key);
			enable_irq(hi->irq_key);
			atomic_set(&hi->irq_key_enabled, true);
                        enable_irq_wake(hi->irq_key);
                }
	} else {
		pr_debug("%s; 3 polarity earjack\n", __func__);
		atomic_set(&hi->is_3_pole_or_not, 1);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, MAX1642X_HEADSET_NO_MIC);
		input_report_switch(hi->input, SW_HEADPHONE_INSERT, 1);
		input_sync(hi->input);
		mutex_unlock(&hi->mutex_lock);

		if (atomic_read(&hi->irq_key_enabled)) {
			pr_debug("%s: irq_key_enabled = TRUE, key IRQ = %d\n",
				__func__, hi->irq_key);
			disable_irq(hi->irq_key);
			atomic_set(&hi->irq_key_enabled, false);
                        disable_irq_wake(hi->irq_key);
                }
	}
}

static void max1462x_remove_headset(struct max1462x_hsd_info *hi)
{
	int earjack_type = JACK_NONE;

	pr_info("%s\n", __func__);

	if (atomic_read(&hi->is_3_pole_or_not))
		earjack_type = JACK_HEADPHONE_3_POLE;
	else
		earjack_type = JACK_HEADPSET_4_POLE;

	atomic_set(&hi->is_3_pole_or_not, 1);

	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, NO_DEVICE);
	input_report_switch(hi->input, SW_HEADPHONE_INSERT, 0);
	if (earjack_type == JACK_HEADPSET_4_POLE)
		input_report_switch(hi->input, SW_MICROPHONE_INSERT, 0);
	input_sync(hi->input);
	mutex_unlock(&hi->mutex_lock);

	if (atomic_read(&hi->irq_key_enabled)) {
		disable_irq(hi->irq_key);
		atomic_set(&hi->irq_key_enabled, false);
                disable_irq_wake(hi->irq_key);
	}

	if (atomic_read(&hi->btn_state)) {
		queue_delayed_work(local_max1462x_workqueue,
			&(hi->work_for_key_released), hi->latency_for_key);
	}
}

static void max1462x_detect_work(struct work_struct *work)
{
	int state;
	struct max1462x_hsd_info *hi =
        container_of(work, struct max1462x_hsd_info, work);

	pr_debug("%s\n", __func__);

	state = gpio_get_value(hi->gpio_detect);

	if (state == 1) {
		if (switch_get_state(&hi->sdev) != NO_DEVICE)
			max1462x_remove_headset(hi);
		 else
			pr_debug("%s: err_invalid_state state = %d\n",
				__func__, state);
	} else {
		if (switch_get_state(&hi->sdev) == NO_DEVICE)
			max1462x_insert_headset(hi);
		else
			pr_debug("%s: err_invalid_state state = %d\n",
				__func__, state);
	}
}

static irqreturn_t max1462x_earjack_det_irq_handler(int irq, void *dev_id)
{
	struct max1462x_hsd_info *hi = (struct max1462x_hsd_info *) dev_id;

	pr_debug("%s\n", __func__);

	wake_lock_timeout(&ear_hook_wake_lock, 2 * HZ);
	queue_work(local_max1462x_workqueue, &(hi->work));
	return IRQ_HANDLED;
}

static irqreturn_t max1462x_button_irq_handler(int irq, void *dev_id)
{
	struct max1462x_hsd_info *hi = (struct max1462x_hsd_info *) dev_id;
	int value;

	pr_debug("%s\n", __func__);

	wake_lock_timeout(&ear_hook_wake_lock, 2 * HZ);
	value = gpio_get_value(hi->gpio_key);
	pr_debug("gpio_get_value(hi->gpio_key) : %d\n", value);

	if (value)
		queue_delayed_work(local_max1462x_workqueue,
			&(hi->work_for_key_released), hi->latency_for_key);
	else
		queue_delayed_work(local_max1462x_workqueue,
			&(hi->work_for_key_pressed), hi->latency_for_key);

	return IRQ_HANDLED;
}

static void max1462x_parse_dt(struct device *dev, struct max1462x_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	pdata->gpio_detect = of_get_named_gpio_flags(np,
			"max1462x,gpio_detect", 0, NULL);
	pdata->gpio_key = of_get_named_gpio_flags(np,
			"max1462x,gpio_key", 0, NULL);
	pdata->gpio_mic_en = of_get_named_gpio_flags(np,
			"max1462x,gpio_mic_en", 0, NULL);
	pdata->key_code = 0;
	pdata->switch_name = "h2w";
	pdata->keypad_name = "hs_detect";
}

static int max1462x_hsd_probe(struct platform_device *pdev)
{
	struct max1462x_platform_data *pdata = pdev->dev.platform_data;
	struct max1462x_hsd_info *hi;
	struct qpnp_vadc_result result;
	int acc_read_value = 0;
	int i;
	int adc_read_count = 3;
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct max1462x_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;
		max1462x_parse_dt(&pdev->dev, pdata);
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			pr_err("%s: No pdata\n", __func__);
			return -ENODEV;
		}
	}

	local_max1462x_workqueue = create_workqueue("max1462x");
	if (!local_max1462x_workqueue) {
		pr_err("%s: Failed to create_workqueue\n", __func__);
		return -ENOMEM;
	}

	wake_lock_init(&ear_hook_wake_lock, WAKE_LOCK_SUSPEND, "ear_hook");

	hi = kzalloc(sizeof(struct max1462x_hsd_info), GFP_KERNEL);
	if (!hi) {
		pr_err("%s: Failed to allloate memory for device info\n",
				__func__);
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	hi->key_code = pdata->key_code;

	platform_set_drvdata(pdev, hi);

	atomic_set(&hi->btn_state, 0);
	atomic_set(&hi->is_3_pole_or_not, 1);

	hi->gpio_mic_en = pdata->gpio_mic_en;
	hi->gpio_detect = pdata->gpio_detect;
	hi->gpio_key = pdata->gpio_key;

	hi->latency_for_key = msecs_to_jiffies(50); /* convert ms to jiffies */
	mutex_init(&hi->mutex_lock);
	INIT_WORK(&hi->work, max1462x_detect_work);
	INIT_DELAYED_WORK(&hi->work_for_key_pressed, max1462x_button_pressed);
	INIT_DELAYED_WORK(&hi->work_for_key_released, max1462x_button_released);

	/* init gpio_mic_en & set default value */
	ret = gpio_request_one(hi->gpio_mic_en, GPIOF_OUT_INIT_HIGH,
			"gpio_mic_en");
	if (ret < 0) {
		pr_err("%s: Failed to configure gpio%d(gpio_mic_en)n",
			__func__, hi->gpio_mic_en);
		goto err_gpio_mic_en;
	}
	pr_debug("gpio_get_value_cansleep(hi->gpio_mic_en)=%d\n",
		gpio_get_value_cansleep(hi->gpio_mic_en));

	/* init gpio_detect */
	ret = gpio_request_one(hi->gpio_detect, GPIOF_IN, "gpio_detect");
	if (ret < 0) {
		pr_err("%s: Failed to configure gpio%d(gpio_det)\n",
			__func__, hi->gpio_detect);
		goto err_gpio_detect;
	}

	/* init gpio_key */
	ret = gpio_request_one(hi->gpio_key, GPIOF_IN, "gpio_key");
	if (ret < 0) {
		pr_err("%s: Failed to configure gpio%d(gpio_key)\n",
			__func__, hi->gpio_key);
		goto err_gpio_key;
	}

	ret = gpio_to_irq(hi->gpio_detect);
	if (ret < 0) {
		pr_err("%s: Failed to get interrupt number\n", __func__);
		goto err_irq_detect;
	}
	hi->irq_detect = ret;
	pr_debug("%s: hi->irq_detect = %d\n", __func__, hi->irq_detect);

	ret = request_threaded_irq(hi->irq_detect, NULL,
				max1462x_earjack_det_irq_handler,
				IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
				pdev->name, hi);
	if (ret < 0) {
		pr_err("%s: failed to request button irq\n", __func__);
		goto err_irq_detect_request;
	}

	ret = enable_irq_wake(hi->irq_detect);
	if (ret < 0) {
		pr_err("%s: Failed to set gpio_detect interrupt wake\n",
			__func__);
		goto err_irq_detect_wake;
	}

	/* initialize irq of gpio_key */
	ret = gpio_to_irq(hi->gpio_key);
	if (ret < 0) {
		pr_err("%s: Failed to get interrupt number\n", __func__);
		goto err_irq_key;
	}
	hi->irq_key = ret;
	pr_debug("%s: hi->irq_key = %d\n", __func__, hi->irq_key);

	ret = request_threaded_irq(hi->irq_key, NULL,
			max1462x_button_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			pdev->name, hi);
	if (ret < 0) {
		pr_err("%s: failed to request button irq\n", __func__);
		goto err_irq_key_request;
	}

        disable_irq(hi->irq_key);
        atomic_set(&hi->irq_key_enabled, false);

        /* initialize switch device */
	hi->sdev.name = pdata->switch_name;
	hi->sdev.print_state = max1462x_hsd_print_state;
	hi->sdev.print_name = max1462x_hsd_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0) {
		pr_err("%s: Failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	/* initialize input device */
	hi->input = input_allocate_device();
	if (!hi->input) {
		pr_err("%s: Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device;
	}

	hi->input->name = pdata->keypad_name;
	hi->input->id.vendor    = 0x0001;
	hi->input->id.product   = 1;
	hi->input->id.version   = 1;

	/* headset tx noise */
	for (i = 0; i < adc_read_count; i++) {
		ret = qpnp_vadc_read(ADC_PORT_NUM, &result);
		if (ret < 0) {
			if (ret == -ETIMEDOUT)
				pr_warn("%s: warning: adc read timeout \n",
						__func__);
			else
				pr_warn("%s: warning: adc read error - %d\n",
						__func__, ret);
		} else {
			acc_read_value = (int)result.physical;
			pr_info("%s: acc_read_value - %d\n", __func__,
				acc_read_value);
			break;
		}
	}

	set_bit(EV_SYN, hi->input->evbit);
	set_bit(EV_KEY, hi->input->evbit);
	set_bit(EV_SW, hi->input->evbit);
	set_bit(hi->key_code, hi->input->keybit);
	set_bit(SW_HEADPHONE_INSERT, hi->input->swbit);
	set_bit(SW_MICROPHONE_INSERT, hi->input->swbit);

	input_set_capability(hi->input, EV_KEY, KEY_MEDIA);
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(hi->input, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(hi->input, EV_SW, SW_HEADPHONE_INSERT);
	input_set_capability(hi->input, EV_SW, SW_MICROPHONE_INSERT);

	ret = input_register_device(hi->input);
	if (ret < 0) {
		pr_err("%s: Failed to register input device\n", __func__);
		goto err_input_register_device;
	}

	/* to detect in initialization with eacjack insertion */
	if (!(gpio_get_value(hi->gpio_detect)))
		queue_work(local_max1462x_workqueue, &(hi->work));

	return ret;

err_input_register_device:
	input_free_device(hi->input);
err_input_allocate_device:
	switch_dev_unregister(&hi->sdev);
err_switch_dev_register:
        free_irq(hi->irq_key, hi);
err_irq_key_request:
err_irq_key:
err_irq_detect_wake:
	free_irq(hi->irq_detect, hi);
err_irq_detect_request:
err_irq_detect:
	gpio_free(hi->gpio_key);
err_gpio_key:
	gpio_free(hi->gpio_detect);
err_gpio_detect:
	gpio_free(hi->gpio_mic_en);
err_gpio_mic_en:
	mutex_destroy(&hi->mutex_lock);
	kfree(hi);
err_kzalloc:
	destroy_workqueue(local_max1462x_workqueue);
	local_max1462x_workqueue = NULL;
	return ret;
}

static int max1462x_hsd_remove(struct platform_device *pdev)
{
	struct max1462x_hsd_info *hi = platform_get_drvdata(pdev);

	if (switch_get_state(&hi->sdev))
		max1462x_remove_headset(hi);

	if (local_max1462x_workqueue) {
		destroy_workqueue(local_max1462x_workqueue);
		local_max1462x_workqueue = NULL;
	}

	input_unregister_device(hi->input);
	switch_dev_unregister(&hi->sdev);

	free_irq(hi->irq_key, hi);
	free_irq(hi->irq_detect, hi);

	gpio_free(hi->gpio_detect);
	gpio_free(hi->gpio_key);
	gpio_free(hi->gpio_mic_en);

	mutex_destroy(&hi->mutex_lock);

	wake_lock_destroy(&ear_hook_wake_lock);

	kfree(hi);

	return 0;
}

static struct of_device_id max1462x_match_table[] = {
	{ .compatible = "maxim,max1462x",},
	{},
};

static struct platform_driver max1462x_hsd_driver = {
	.probe          = max1462x_hsd_probe,
	.remove         = max1462x_hsd_remove,
	.driver         = {
		.name           = "max1462x",
		.owner          = THIS_MODULE,
		.of_match_table = max1462x_match_table,
	},
};

static int __init max1462x_hsd_init(void)
{
	int ret;

	ret = platform_driver_register(&max1462x_hsd_driver);
	if (ret < 0)
		pr_err("%s: Fail to register platform driver\n", __func__);

	return ret;
}

static void __exit max1462x_hsd_exit(void)
{
	platform_driver_unregister(&max1462x_hsd_driver);
}

late_initcall_sync(max1462x_hsd_init);
module_exit(max1462x_hsd_exit);

MODULE_DESCRIPTION("MAX1642X Headset detection driver (max1462x)");
MODULE_LICENSE("GPL");
