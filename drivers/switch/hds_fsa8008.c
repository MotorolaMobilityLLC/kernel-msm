/* drivers/switch/hds_fsa8008.c
 *
 *  LGE 3.5 PI Headset detection driver using fsa8008.
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * Copyright (C) 2009-2012 LGE, Inc.
 * Lee SungYoung <lsy@lge.com>
 * Kim Eun Hye <ehgrace.kim@lge.com>
 * Yoon Gi Souk <gisouk.yoon@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Interface is following:
 * android:frameworks/base/services/java/com/android/server/HeadsetObserver.java
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
#include <linux/platform_data/hds_fsa8008.h>

#define FSA8008_USE_WORK_QUEUE
#define FSA8008_KEY_LATENCY_TIME	200 /* in ms */
#define FSA8008_DEBOUNCE_TIME		500 /* in ms */
#define FSA8008_WAKELOCK_TIMEOUT	(2*HZ)

#define HSD_DEBUG_PRINT

#ifdef HSD_DEBUG_PRINT
#define HSD_DBG(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __func__, ##args)
#else
#define HSD_DBG(fmt, args...) do {} while (0)
#endif

#ifdef FSA8008_USE_WORK_QUEUE
static struct workqueue_struct *local_fsa8008_workqueue;
#endif

static struct wake_lock ear_hook_wake_lock;

struct hsd_info {
	/* function devices provided by this driver */
	struct switch_dev sdev;
	struct input_dev *input;
	/* mutex */
	struct mutex mutex_lock;
	/* h/w configuration : initilized by platform data */
	unsigned int gpio_detect; /* DET : to detect jack inserted or not */
	unsigned int gpio_detect_can_wakeup;
	unsigned int gpio_mic_en; /* EN : to enable mic */
	unsigned int gpio_mic_bias_en; /* EN : to enable mic bias */
	unsigned int gpio_jpole;  /* JPOLE : 3pole or 4pole */
	unsigned int gpio_key;    /* S/E button */

	/* callback function which is initialized while probing */
	void (*set_headset_mic_bias)(int enable);
	void (*set_uart_console)(int enable);

	unsigned int latency_for_detection;
	unsigned int latency_for_key;

	unsigned int key_code;

	/* irqs */
	unsigned int irq_detect;
	unsigned int irq_key;
	/* internal states */
	atomic_t irq_key_enabled;
	atomic_t is_3_pole_or_not;
	atomic_t btn_state;
	int saved_detect;
	/* work for detect_work */
	struct delayed_work work;
	struct delayed_work work_for_key_pressed;
	struct delayed_work work_for_key_released;
};

enum {
	NO_DEVICE   = 0,
	HEADSET_WITH_MIC = (1 << 0),
	HEADSET_NO_MIC = (1 << 1),
};
enum {
	HEADSET_INSERT = 0,
	HEADSET_REMOVE = 1,
};

enum {
	HEADSET_4POLE = 0,
	HEADSET_3POLE = 1,
};

static ssize_t hsd_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case HEADSET_WITH_MIC:
		return sprintf(buf, "Headset\n");
	case HEADSET_NO_MIC:
		return sprintf(buf, "Headset_no_mic\n");

	}
	return -EINVAL;
}

static ssize_t hsd_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static void button_pressed(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work, struct delayed_work, work);
	struct hsd_info *hi = container_of(dwork, struct hsd_info, work_for_key_pressed);


	if (gpio_get_value_cansleep(hi->gpio_detect) &&
			(switch_get_state(&hi->sdev)== HEADSET_WITH_MIC)) {
		pr_warn("%s: ear jack was plugged out already!"
			"just ignore the event.\n", __func__);
		return;
	}

	HSD_DBG("button_pressed \n");

	atomic_set(&hi->btn_state, 1);
	input_report_key(hi->input, hi->key_code, 1);
	input_sync(hi->input);
}

static void button_released(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(
			work, struct delayed_work, work);
	struct hsd_info *hi = container_of(
			dwork, struct hsd_info, work_for_key_released);

	if (gpio_get_value_cansleep(hi->gpio_detect) &&
			(switch_get_state(&hi->sdev)== HEADSET_WITH_MIC)){
		pr_warn("%s: ear jack was plugged out already!"
			"just ignore the event.\n", __func__);
		return;
	}

	HSD_DBG("button_released \n");

	atomic_set(&hi->btn_state, 0);
	input_report_key(hi->input, hi->key_code, 0);
	input_sync(hi->input);
}

static void insert_headset(struct hsd_info *hi)
{
	int earjack_type;

	HSD_DBG("insert_headset");

	if (hi->set_headset_mic_bias)
		hi->set_headset_mic_bias(1);

	gpio_set_value_cansleep(hi->gpio_mic_en, 1);

	msleep(hi->latency_for_detection);

	earjack_type = gpio_get_value_cansleep(hi->gpio_jpole);

	if (earjack_type == HEADSET_3POLE) {
		HSD_DBG("3 polarity earjack");

		atomic_set(&hi->is_3_pole_or_not, 1);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, HEADSET_NO_MIC);
		mutex_unlock(&hi->mutex_lock);

		gpio_set_value_cansleep(hi->gpio_mic_en, 0);
		if (hi->set_headset_mic_bias)
			hi->set_headset_mic_bias(0);
		if (hi->set_uart_console)
			hi->set_uart_console(0);

		input_report_switch(hi->input, SW_HEADPHONE_INSERT, 1);
		input_sync(hi->input);
	} else {
		HSD_DBG("4 polarity earjack");

		atomic_set(&hi->is_3_pole_or_not, 0);

		mutex_lock(&hi->mutex_lock);
		switch_set_state(&hi->sdev, HEADSET_WITH_MIC);
		mutex_unlock(&hi->mutex_lock);

		if (!atomic_read(&hi->irq_key_enabled)) {
			HSD_DBG("enable_irq - irq_key");
			enable_irq(hi->irq_key);

			atomic_set(&hi->irq_key_enabled, 1);
		}
		if (hi->set_uart_console)
			hi->set_uart_console(0);
		input_report_switch(hi->input, SW_HEADPHONE_INSERT, 1);
		input_report_switch(hi->input, SW_MICROPHONE_INSERT, 1);
		input_sync(hi->input);
	}
}

static void remove_headset(struct hsd_info *hi)
{
	int has_mic = switch_get_state(&hi->sdev);

	HSD_DBG("remove_headset");

	gpio_set_value_cansleep(hi->gpio_mic_en, 0);
	if (hi->set_headset_mic_bias)
		hi->set_headset_mic_bias(0);

	atomic_set(&hi->is_3_pole_or_not, 1);
	mutex_lock(&hi->mutex_lock);
	switch_set_state(&hi->sdev, NO_DEVICE);
	mutex_unlock(&hi->mutex_lock);

	if (atomic_read(&hi->irq_key_enabled)) {
		disable_irq(hi->irq_key);
		atomic_set(&hi->irq_key_enabled, 0);
	}

	if (atomic_read(&hi->btn_state))
#ifdef	FSA8008_USE_WORK_QUEUE
	queue_delayed_work(local_fsa8008_workqueue,
			&(hi->work_for_key_released), hi->latency_for_key );
#else
	schedule_delayed_work(&(hi->work_for_key_released),
			hi->latency_for_key );
#endif
	input_report_switch(hi->input, SW_HEADPHONE_INSERT, 0);
	if (has_mic == HEADSET_WITH_MIC)
		input_report_switch(hi->input, SW_MICROPHONE_INSERT, 0);
	input_sync(hi->input);
}

static void detect_work(struct work_struct *work)
{
	int state;
	struct delayed_work *dwork = container_of(
			work, struct delayed_work, work);
	struct hsd_info *hi = container_of(dwork, struct hsd_info, work);

	state = gpio_get_value_cansleep(hi->gpio_detect);

	if (state == HEADSET_REMOVE) {
		if (switch_get_state(&hi->sdev) != NO_DEVICE) {
			remove_headset(hi);
		} else {
			HSD_DBG("err_invalid_state state = %d\n", state);
		}
	} else {

		if (switch_get_state(&hi->sdev) == NO_DEVICE) {
			insert_headset(hi);
		} else {
			HSD_DBG("err_invalid_state state = %d\n", state);
		}
	}

}

static void schedule_detect_work(struct hsd_info *hi)
{
	wake_lock_timeout(&ear_hook_wake_lock, FSA8008_WAKELOCK_TIMEOUT);

#ifdef FSA8008_USE_WORK_QUEUE
	queue_delayed_work(local_fsa8008_workqueue, &(hi->work),
			msecs_to_jiffies(FSA8008_DEBOUNCE_TIME));
#else
	schedule_delayed_work(&(hi->work),
			msecs_to_jiffies(FSA8008_DEBOUNCE_TIME));
#endif
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct hsd_info *hi = (struct hsd_info *) dev_id;

	HSD_DBG("gpio_irq_handler");

	schedule_detect_work(hi);

	return IRQ_HANDLED;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	struct hsd_info *hi = (struct hsd_info *) dev_id;
	int value;

	HSD_DBG("button_irq_handler");

	wake_lock_timeout(&ear_hook_wake_lock, FSA8008_WAKELOCK_TIMEOUT);

	value = gpio_get_value_cansleep(hi->gpio_key);

#ifdef	FSA8008_USE_WORK_QUEUE
	if (value)
		queue_delayed_work(local_fsa8008_workqueue,
				&(hi->work_for_key_pressed),
				hi->latency_for_key );
	else
		queue_delayed_work(local_fsa8008_workqueue,
				&(hi->work_for_key_released),
				hi->latency_for_key );
#else
	if (value)
		schedule_delayed_work(&(hi->work_for_key_pressed),
				hi->latency_for_key );
	else
		schedule_delayed_work(&(hi->work_for_key_released),
				hi->latency_for_key );
#endif

	return IRQ_HANDLED;
}

static int hsd_gpio_init(struct hsd_info *hi)
{
	int ret;

	/* initialize gpio_detect */
	ret = gpio_request_one(hi->gpio_detect, GPIOF_IN, "gpio_detect");
	if (ret < 0) {
		pr_err("%s: Failed to gpio_request gpio%d (gpio_detect)\n",
				__func__, hi->gpio_detect);
		goto error_01;
	}

	/* initialize gpio_jpole */
	ret = gpio_request_one(hi->gpio_jpole, GPIOF_IN, "gpio_jpole");
	if (ret < 0) {
		pr_err("%s: Failed to gpio_request gpio%d (gpio_jpole)\n",
				__func__, hi->gpio_jpole);
		goto error_02;
	}

	/* initialize gpio_key */
	ret = gpio_request_one(hi->gpio_key, GPIOF_IN, "gpio_key");
	if (ret < 0) {
		pr_err("%s: Failed to gpio_request gpio%d (gpio_key)\n",
				__func__, hi->gpio_key);
		goto error_03;
	}

	/* initialize gpio_mic_en */
	ret = gpio_request_one(hi->gpio_mic_en, GPIOF_OUT_INIT_LOW,
			"gpio_mic_en");
	if (ret < 0) {
		pr_err("%s: Failed to gpio_request gpio%d (gpio_mic_en)\n",
				__func__, hi->gpio_mic_en);
		goto error_04;
	}

	/* initialize gpio_mic_bias_en */
	if (gpio_is_valid(hi->gpio_mic_bias_en)) {
		ret = gpio_request_one(hi->gpio_mic_bias_en,
				GPIOF_OUT_INIT_LOW, "gpio_mic_bias_en");
		if (ret < 0) {
			pr_err("%s: Failed to gpio_request gpio%d "
			       "(gpio_mic_bias_en)\n",
					__func__, hi->gpio_mic_en);
			goto error_05;
		}
	}

	return 0;

error_05:
	gpio_free(hi->gpio_mic_en);
error_04:
	gpio_free(hi->gpio_key);
error_03:
	gpio_free(hi->gpio_jpole);
error_02:
	gpio_free(hi->gpio_detect);
error_01:
	return ret;
}

static void hsd_gpio_free(struct hsd_info *hi)
{
	if (gpio_is_valid(hi->gpio_mic_bias_en))
		gpio_free(hi->gpio_mic_bias_en);
	gpio_free(hi->gpio_mic_en);
	gpio_free(hi->gpio_key);
	gpio_free(hi->gpio_jpole);
	gpio_free(hi->gpio_detect);
}

static int hsd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fsa8008_platform_data *pdata = pdev->dev.platform_data;
	struct hsd_info *hi;

	pr_info("fsa8008 probe\n");

	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	hi = kzalloc(sizeof(struct hsd_info), GFP_KERNEL);
	if (NULL == hi) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	hi->key_code = pdata->key_code;

	platform_set_drvdata(pdev, hi);

	atomic_set(&hi->btn_state, 0);
	atomic_set(&hi->is_3_pole_or_not, 1);

	hi->gpio_detect = pdata->gpio_detect;
	hi->gpio_detect_can_wakeup = pdata->gpio_detect_can_wakeup;
	hi->gpio_mic_en = pdata->gpio_mic_en;
	hi->gpio_mic_bias_en = pdata->gpio_mic_bias_en;
	hi->gpio_jpole = pdata->gpio_jpole;
	hi->gpio_key = pdata->gpio_key;
	hi->set_headset_mic_bias = pdata->set_headset_mic_bias;
	hi->set_uart_console = pdata->set_uart_console;
	hi->latency_for_detection = pdata->latency_for_detection;
	hi->latency_for_key = msecs_to_jiffies(FSA8008_KEY_LATENCY_TIME);

	mutex_init(&hi->mutex_lock);

	INIT_DELAYED_WORK(&hi->work, detect_work);
	INIT_DELAYED_WORK(&hi->work_for_key_pressed, button_pressed);
	INIT_DELAYED_WORK(&hi->work_for_key_released, button_released);

	if (hsd_gpio_init(hi) < 0)
		goto error_01;

	/* initialize irq of gpio_jpole */
	hi->irq_detect = gpio_to_irq(hi->gpio_detect);
	HSD_DBG("hi->irq_detect = %d\n", hi->irq_detect);
	if (hi->irq_detect < 0) {
		pr_err("%s: Failed to get interrupt number\n", __func__);
		ret = hi->irq_detect;
		goto error_02;
	}

	ret = request_threaded_irq(hi->irq_detect, NULL, gpio_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			pdev->name, hi);
	if (ret) {
		pr_err("%s: failed to request button irq\n", __func__);
		goto error_02;
	}

	if (hi->gpio_detect_can_wakeup) {
		ret = irq_set_irq_wake(hi->irq_detect, 1);
		if (ret < 0) {
			pr_err("%s: Failed to set irq_detect interrupt wake\n",
					__func__);
			goto error_03;
		}
	}

	/* initialize irq of gpio_key */
	hi->irq_key = gpio_to_irq(hi->gpio_key);
	HSD_DBG("hi->irq_key = %d\n", hi->irq_key);
	if (hi->irq_key < 0) {
		pr_err("%s: Failed to get interrupt number\n", __func__);
		ret = hi->irq_key;
		goto error_03;
	}

	ret = request_threaded_irq(hi->irq_key, NULL, button_irq_handler,
			IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING,
			pdev->name, hi);
	if (ret) {
		pr_err("%s: failed to request button irq\n", __func__);
		goto error_03;
	}

	disable_irq(hi->irq_key);

	ret = irq_set_irq_wake(hi->irq_key, 1);
	if (ret < 0) {
		pr_err("%s: Failed to set irq_key interrupt wake\n", __func__);
		goto error_04;
	}

	/* initialize switch device */
	hi->sdev.name = pdata->switch_name;
	hi->sdev.print_state = hsd_print_state;
	hi->sdev.print_name = hsd_print_name;

	ret = switch_dev_register(&hi->sdev);
	if (ret < 0) {
		pr_err("%s: Failed to register switch device\n", __func__);
		goto error_04;
	}

	/* initialize input device */
	hi->input = input_allocate_device();
	if (!hi->input) {
		pr_err("%s: Failed to allocate input device\n", __func__);
		ret = -ENOMEM;
		goto error_05;
	}

	hi->input->name = pdata->keypad_name;

	hi->input->id.vendor    = 0x0001;
	hi->input->id.product   = 1;
	hi->input->id.version   = 1;

	set_bit(EV_SYN, hi->input->evbit);
	set_bit(EV_KEY, hi->input->evbit);
	set_bit(EV_SW, hi->input->evbit);
	set_bit(hi->key_code, hi->input->keybit);
	set_bit(SW_HEADPHONE_INSERT, hi->input->swbit);
	set_bit(SW_MICROPHONE_INSERT, hi->input->swbit);

	ret = input_register_device(hi->input);
	if (ret) {
		pr_err("%s: Failed to register input device\n", __func__);
		goto error_06;
	}

	if (!gpio_get_value_cansleep(hi->gpio_detect)) {
#ifdef FSA8008_USE_WORK_QUEUE
		/* to detect in initialization with eacjack insertion */
		queue_delayed_work(local_fsa8008_workqueue, &(hi->work), 0);
#else
		/* to detect in initialization with eacjack insertion */
		schedule_delayed_work(&(hi->work), 0);
#endif
	}

	return ret;

error_06:
	input_free_device(hi->input);
error_05:
	switch_dev_unregister(&hi->sdev);
error_04:
	free_irq(hi->irq_key, 0);
error_03:
	free_irq(hi->irq_detect, 0);
error_02:
	hsd_gpio_free(hi);
error_01:
	mutex_destroy(&hi->mutex_lock);
	kfree(hi);

	return ret;
}

static int hsd_remove(struct platform_device *pdev)
{
	struct hsd_info *hi = (struct hsd_info *)platform_get_drvdata(pdev);

	if (switch_get_state(&hi->sdev))
		remove_headset(hi);

	input_unregister_device(hi->input);
	switch_dev_unregister(&hi->sdev);

	free_irq(hi->irq_key, 0);
	free_irq(hi->irq_detect, 0);

	hsd_gpio_free(hi);

	mutex_destroy(&hi->mutex_lock);
	kfree(hi);

	return 0;
}

static int hsd_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hsd_info *hi = platform_get_drvdata(pdev);

	if (!hi->gpio_detect_can_wakeup) {
		disable_irq(hi->irq_detect);
		hi->saved_detect = gpio_get_value(hi->gpio_detect);
	}

	return 0;
}

static int hsd_resume(struct device *dev)
{

	struct platform_device *pdev = to_platform_device(dev);
	struct hsd_info *hi = platform_get_drvdata(pdev);
	int detect = 0;

	detect = gpio_get_value(hi->gpio_detect);
	if (HEADSET_INSERT == detect)
		if (hi->set_uart_console)
			hi->set_uart_console(0);

	if (!hi->gpio_detect_can_wakeup) {
		enable_irq(hi->irq_detect);
		if (hi->saved_detect != detect)
			schedule_detect_work(hi);
	}

	return 0;
}

static const struct dev_pm_ops hsd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hsd_suspend, hsd_resume)
};

static struct platform_driver hsd_driver = {
	.probe          = hsd_probe,
	.remove         = hsd_remove,
	.driver         = {
		.name   = "fsa8008",
		.owner  = THIS_MODULE,
		.pm = &hsd_pm_ops,
	},
};

static int __init hsd_init(void)
{
	int ret;

	pr_info("fsa8008 init\n");

	wake_lock_init(&ear_hook_wake_lock, WAKE_LOCK_SUSPEND, "ear_hook");

#ifdef FSA8008_USE_WORK_QUEUE
	local_fsa8008_workqueue = create_workqueue("fsa8008");
	if (!local_fsa8008_workqueue) {
		pr_err("%s: out of memory\n", __func__);
		ret = -ENOMEM;
		goto err_workqueue;
	}
#endif

	ret = platform_driver_register(&hsd_driver);
	if (ret < 0) {
		pr_err("%s: Fail to register platform driver\n", __func__);
		goto err_platform_driver_register;
	}

	return 0;

err_platform_driver_register:
#ifdef FSA8008_USE_WORK_QUEUE
	if (local_fsa8008_workqueue)
		destroy_workqueue(local_fsa8008_workqueue);
	local_fsa8008_workqueue = NULL;
#endif
err_workqueue:
	wake_lock_destroy(&ear_hook_wake_lock);

	return ret;
}

static void __exit hsd_exit(void)
{
#ifdef FSA8008_USE_WORK_QUEUE
	if (local_fsa8008_workqueue)
		destroy_workqueue(local_fsa8008_workqueue);
	local_fsa8008_workqueue = NULL;
#endif

	platform_driver_unregister(&hsd_driver);
	wake_lock_destroy(&ear_hook_wake_lock);
}

/* to make init after pmicxxxx module */
late_initcall_sync(hsd_init);
module_exit(hsd_exit);

MODULE_AUTHOR("Yoon Gi Souk <gisouk.yoon@lge.com>");
MODULE_DESCRIPTION("FSA8008 Headset detection driver");
MODULE_LICENSE("GPL");
