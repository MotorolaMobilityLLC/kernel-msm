/*
 *  Headset device detection driver.
 *
 * Copyright (C) 2011 ASUSTek Corporation.
 *
 * Authors:
 *  Jason Cheng <jason4_cheng@asus.com>
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
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <sound/soc.h>
#include <linux/mfd/wcd9xxx/wcd9310_registers.h>
#include "../codecs/wcd9310.h"
#include "../../../arch/arm/mach-msm/board-8960.h"
#include <asm/mach-types.h>
#include <mach/board_asustek.h>

MODULE_DESCRIPTION("Headset detection driver");
MODULE_LICENSE("GPL");


/*----------------------------------------------------------------------------
** FUNCTION DECLARATION
**----------------------------------------------------------------------------*/
static int	__init	headset_init(void);
static void	__exit	headset_exit(void);
static irqreturn_t   	detect_irq_handler(int irq, void *dev_id);
static void 		detection_work(struct work_struct *work);
static int          	jack_config_gpio(void);
static int btn_config_gpio(void);
static void button_press_work(struct work_struct *work);
static void button_release_work(struct work_struct *work);
static void	set_hs_micbias(int status);

/*----------------------------------------------------------------------------
** GLOBAL VARIABLES
**----------------------------------------------------------------------------*/
#define DB_DET_GPIO	(85)
#define JACK_GPIO	(45)
#define HS_HOOK_DET	(62)
#define BTN_SLEEP	(250)
#define ON	(1)
#define OFF	(0)

enum{
	NO_DEVICE = 0,
	HEADSET_WITH_MIC = 1,
	HEADSET_WITHOUT_MIC = 2,
};

struct headset_data {
	struct switch_dev sdev;
	struct input_dev *input;
	unsigned hp_det_gpio;
	unsigned hook_det_gpio;
	unsigned int hp_det_irq;
	unsigned int hook_det_irq;
	struct hrtimer btn_timer;
	ktime_t debouncing_time;
	ktime_t btn_debouncing_time;
	bool btn_ignore;
	bool btn_pressed;
};

extern struct snd_soc_codec *wcd9310_codec;
static struct headset_data *hs_data;
static struct workqueue_struct *g_detection_work_queue;
static struct workqueue_struct *g_button_release_work_queue;
static struct workqueue_struct *g_button_press_work_queue;
static DECLARE_DELAYED_WORK(g_detection_work, detection_work);
static struct work_struct g_button_press_works[2];
static struct work_struct g_button_release_works[2];

struct work_struct headset_work;
struct work_struct lineout_work;

static void set_hs_micbias(int status)
{
	struct snd_soc_dapm_widget *w = NULL;

	if (wcd9310_codec == NULL) {
		printk(KERN_INFO "%s: wcd9310_codec = NULL\n", __func__);
		return;
	}

	 w = snd_soc_get_codec_widget(wcd9310_codec->card,
					wcd9310_codec, "LDO_H");

	if (status) {
		snd_soc_update_bits(wcd9310_codec, TABLA_A_MICB_CFILT_2_VAL,
				0xFC, (0x28 << 2));
		snd_soc_update_bits(wcd9310_codec, TABLA_A_MICB_CFILT_2_CTL,
				0xC0, 0xC0);
		snd_soc_update_bits(wcd9310_codec, TABLA_A_MICB_2_INT_RBIAS,
				0xFF, 0x00);
		snd_soc_update_bits(wcd9310_codec, TABLA_A_MICB_2_CTL,
				0x80, 0x80);
		/* Enable  Bandgap Reference */
		snd_soc_update_bits(wcd9310_codec, TABLA_A_BIAS_CENTRAL_BG_CTL,
				0x0F, 0x05);
		snd_soc_update_bits(wcd9310_codec, TABLA_A_LDO_H_MODE_1,
				0xff, 0xdf);

	} else {
		snd_soc_update_bits(wcd9310_codec, TABLA_A_MICB_2_CTL,
				0xC0, 0x00);
		/* Keep LDO_H power on while using */
		if (w != NULL && w->power)
			printk(KERN_INFO "%s widget LDO_H power on. Keep it\n",
			__func__);
		else
			snd_soc_update_bits(wcd9310_codec,
				TABLA_A_LDO_H_MODE_1, 0xff, 0x6d);

		if (tabla_check_bandgap_status(wcd9310_codec) == 0) {
			/* Disable Bandgap Reference power */
			printk(KERN_INFO "%s badgap status: OFF power down bandgap\n",
				__func__);
			snd_soc_write(wcd9310_codec,
				TABLA_A_BIAS_CENTRAL_BG_CTL, 0x50);
		}
	}

	return;
}
static ssize_t headset_name_show(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(&hs_data->sdev)) {
	case NO_DEVICE:{
		return sprintf(buf, "%s\n", "No Device");
		}
	case HEADSET_WITH_MIC:{
		return sprintf(buf, "%s\n", "HEADSET");
		}
	case HEADSET_WITHOUT_MIC:{
		return sprintf(buf, "%s\n", "HEADPHONE");
		}
	}
	return -EINVAL;
}

static ssize_t headset_state_show(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(&hs_data->sdev)) {
	case NO_DEVICE:
		return sprintf(buf, "%d\n", 0);
	case HEADSET_WITH_MIC:
		return sprintf(buf, "%d\n", 1);
	case HEADSET_WITHOUT_MIC:
		return sprintf(buf, "%d\n", 2);
	}
	return -EINVAL;
}

static void insert_headset(void)
{
	if (gpio_get_value(DB_DET_GPIO) == 0) {
		printk(KERN_INFO "%s: debug board\n", __func__);
	} else if (gpio_get_value(HS_HOOK_DET)) {
		printk(KERN_INFO "%s: headphone\n", __func__);
		switch_set_state(&hs_data->sdev, HEADSET_WITHOUT_MIC);
		tabla_set_h2w_status(wcd9310_codec, H2W_HEADPHONE);
	} else {
		printk(KERN_INFO "%s: headset\n", __func__);
		switch_set_state(&hs_data->sdev, HEADSET_WITH_MIC);
		tabla_set_h2w_status(wcd9310_codec, H2W_HEADSET);
		hs_data->btn_pressed = false;
		hs_data->btn_ignore = false;
		irq_set_irq_type(hs_data->hook_det_irq, IRQF_TRIGGER_HIGH);
		enable_irq(hs_data->hook_det_irq);
	}
	hs_data->debouncing_time = ktime_set(0, 100000000);  /* 100 ms */
}

static void remove_headset(void)
{
	if (switch_get_state(&hs_data->sdev) == HEADSET_WITH_MIC) {
		hs_data->btn_pressed = false;
		disable_irq_nosync(hs_data->hook_det_irq);
	}
	tabla_set_h2w_status(wcd9310_codec, H2W_NONE);
	switch_set_state(&hs_data->sdev, NO_DEVICE);
	hs_data->debouncing_time = ktime_set(0, 100000000);  /* 100 ms */
	set_hs_micbias(OFF);
}

static void detection_work(struct work_struct *work)
{
	unsigned long irq_flags;
	int cable_in1;
	int mic_in = 0;
	/* Disable headset interrupt while detecting.*/
	local_irq_save(irq_flags);
	disable_irq(hs_data->hp_det_irq);
	local_irq_restore(irq_flags);

	irq_set_irq_type(hs_data->hook_det_irq, IRQF_TRIGGER_NONE);

	set_hs_micbias(ON);

	if (switch_get_state(&hs_data->sdev) != NO_DEVICE) {
		/* Delay for pin stable when being removed. */
		msleep(110);
	} else {
		/* Delay for pin stable when plugged. */
		msleep(1000);
	}

	/* Restore IRQs */
	local_irq_save(irq_flags);
	enable_irq(hs_data->hp_det_irq);
	local_irq_restore(irq_flags);

	if (gpio_get_value(JACK_GPIO) != 0) {
		/* Headset not plugged in */
		remove_headset();
		goto closed_micbias;
	}

	cable_in1 = gpio_get_value(JACK_GPIO);
	mic_in  = gpio_get_value(HS_HOOK_DET);
	if (cable_in1 == 0) {
		printk(KERN_INFO "HOOK_GPIO value: %d\n", mic_in);
		if (switch_get_state(&hs_data->sdev) == NO_DEVICE)
			insert_headset();
		if (mic_in == 1)
			goto closed_micbias;
	} else{
		printk(KERN_INFO "HEADSET: Jack-in GPIO is low, but not a headset\n");
		goto closed_micbias;
	}
	return;

closed_micbias:
	set_hs_micbias(OFF);
	return;
}

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;

	do {
		value1 = gpio_get_value(hs_data->hook_det_gpio);
		irq_set_irq_type(hs_data->hook_det_irq,
				value1 ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		value2 = gpio_get_value(hs_data->hook_det_gpio);
	} while (value1 != value2 && retry_limit-- > 0);

	hrtimer_start(&hs_data->btn_timer,
		hs_data->btn_debouncing_time, HRTIMER_MODE_REL);
	return IRQ_HANDLED;
}

static void button_release_work(struct work_struct *work)
{
	msleep(BTN_SLEEP);
	if (hs_data->btn_ignore)
		return;
	pr_debug("release event\n");
	input_report_key(hs_data->input, KEY_MEDIA, 0);
	input_sync(hs_data->input);
}
static void button_press_work(struct work_struct *work)
{
	msleep(BTN_SLEEP);
	if (hs_data->btn_ignore)
		return;
	pr_debug("press event\n");
	input_report_key(hs_data->input, KEY_MEDIA, 1);
	input_sync(hs_data->input);
}
static enum hrtimer_restart button_event_timer_func(struct hrtimer *data)
{
	int res;
	if (hs_data->btn_ignore)
		return HRTIMER_NORESTART;

	if (gpio_get_value(hs_data->hook_det_gpio)) {
		res = queue_work(g_button_press_work_queue,
				g_button_press_works);
		if (res == 0)
			queue_work(g_button_press_work_queue,
					g_button_press_works + 1);
		hs_data->btn_pressed = true;
	} else if (hs_data->btn_pressed) {
		res = queue_work(g_button_release_work_queue,
				g_button_release_works);
		if (res == 0)
			queue_work(g_button_release_work_queue,
					g_button_release_works + 1);
		hs_data->btn_pressed = false;
	} else {
		pr_debug("%s: extra event.\n", __func__);
	}
	return HRTIMER_NORESTART;
}



/**********************************************************
**  Function: Headset Hook Key Detection interrupt handler
**  Parameter: irq
**  Return value: IRQ_HANDLED
**  High: Hook button pressed
************************************************************/
static int btn_config_gpio()
{
	int ret;

	pr_debug("HEADSET: Config Headset Button detection gpio\n");

	ret = gpio_request(hs_data->hook_det_gpio, "HS_HOOK_INT");
	if (ret) {
		pr_err("%s: Failed to request gpio %d\n", __func__,
		hs_data->hook_det_gpio);
	}

	ret = gpio_direction_input(hs_data->hook_det_gpio);
	hs_data->hook_det_irq = MSM_GPIO_TO_INT(hs_data->hook_det_gpio);
	ret = request_irq(hs_data->hook_det_irq, button_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_NO_SUSPEND, "h2w_button", NULL);
	disable_irq(hs_data->hook_det_irq);

	return ret;
}


/**********************************************************
**  Function: Jack detection-in gpio configuration function
**  Parameter: none
**  Return value: if sucess, then returns 0
**
************************************************************/
static int jack_config_gpio()
{
	int ret;

	ret = gpio_request(DB_DET_GPIO, "DB_DET");

	if (ret) {
		pr_err("%s: Error requesting GPIO %d\n", __func__, DB_DET_GPIO);
		gpio_free(DB_DET_GPIO);
	} else
		gpio_direction_input(DB_DET_GPIO);

	ret = gpio_request(JACK_GPIO, "JACK_IN_DET");
	if (ret) {
		pr_err("%s: Error requesting GPIO %d\n", __func__, JACK_GPIO);
		gpio_free(JACK_GPIO);
	} else
		gpio_direction_input(JACK_GPIO);

	hs_data->hp_det_irq = MSM_GPIO_TO_INT(JACK_GPIO);
	ret = request_irq(hs_data->hp_det_irq, detect_irq_handler,
			IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,
			"h2w_detect", NULL);

	ret = irq_set_irq_wake(hs_data->hp_det_irq, 1);
	set_hs_micbias(ON);
	msleep(50);
	if (gpio_get_value(JACK_GPIO) == 0) {
		insert_headset();
	} else {
		switch_set_state(&hs_data->sdev, NO_DEVICE);
		remove_headset();
	}

	return 0;
}

static irqreturn_t detect_irq_handler(int irq, void *dev_id)
{
	int value1, value2;
	int retry_limit = 10;
	do {
		value1 = gpio_get_value(JACK_GPIO);
		irq_set_irq_type(hs_data->hp_det_irq, value1 ?
				IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
		value2 = gpio_get_value(JACK_GPIO);
	}while (value1 != value2 && retry_limit-- > 0);

	if ((switch_get_state(&hs_data->sdev) == NO_DEVICE) != (value2 == 1)) {
		hs_data->btn_ignore = true;
		queue_delayed_work(g_detection_work_queue, &g_detection_work,
				msecs_to_jiffies(100));
	}

	return IRQ_HANDLED;
}

/**********************************************************
**  Function: Headset driver init function
**  Parameter: none
**  Return value: none
**
************************************************************/
static int __init headset_init(void)
{
	int ret = 0;

	printk(KERN_INFO "%s+ #####\n", __func__);

	hs_data = kzalloc(sizeof(struct headset_data), GFP_KERNEL);
	if (!hs_data)
		return -ENOMEM;
	hs_data->hook_det_gpio = HS_HOOK_DET;
	hs_data->hp_det_gpio = JACK_GPIO;
	hs_data->debouncing_time = ktime_set(0, 100000000);  /* 100 ms */
	hs_data->btn_debouncing_time = ktime_set(0, 10000000);
	hs_data->sdev.name = "h2w";
	hs_data->sdev.print_name = headset_name_show;
	hs_data->sdev.print_state = headset_state_show;
	hs_data->btn_ignore = false;
	hs_data->btn_pressed = false;

	ret = switch_dev_register(&hs_data->sdev);
	if (ret < 0) {
		printk(KERN_ERR "Headset: Failed to register driver\n");
		goto err_switch_dev_register;
	}

	g_detection_work_queue = create_workqueue("detection");
	g_button_press_work_queue = create_workqueue("button_press");
	g_button_release_work_queue = create_workqueue("button_release");

	INIT_WORK(&g_button_press_works[0], button_press_work);
	INIT_WORK(&g_button_press_works[1], button_press_work);
	INIT_WORK(&g_button_release_works[0], button_release_work);
	INIT_WORK(&g_button_release_works[1], button_release_work);

	hrtimer_init(&hs_data->btn_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hs_data->btn_timer.function = button_event_timer_func;

	hs_data->input = input_allocate_device();

	if (!hs_data->input) {
		printk(KERN_ERR "Headset: Failed to request input device\n");
		ret = -ENOMEM;
		goto err_input_dev_request;
	}

	hs_data->input->name = "h2w button";
	set_bit(EV_SYN, hs_data->input->evbit);
	set_bit(EV_KEY, hs_data->input->evbit);
	set_bit(KEY_MEDIA, hs_data->input->keybit);

	ret = input_register_device(hs_data->input);
	if (ret < 0) {
		printk(KERN_ERR "Headset: Failed to register input device\n");
		goto err_input_dev_register;
	}

	ret = btn_config_gpio();
	if (ret < 0)
		printk(KERN_ERR "Headst: Failed to config button gpio\n");

	printk(KERN_INFO "HEADSET: Headset detection mode\n");
	jack_config_gpio();/*Config jack detection GPIO*/

	printk(KERN_INFO "%s- #####\n", __func__);
	return 0;

err_input_dev_register:
	input_free_device(hs_data->input);
err_input_dev_request:
	switch_dev_unregister(&hs_data->sdev);
err_switch_dev_register:
	kfree(hs_data);

	return ret;
}

/**********************************************************
**  Function: Headset driver exit function
**  Parameter: none
**  Return value: none
**
************************************************************/
static void __exit headset_exit(void)
{
	printk(KERN_INFO "HEADSET: Headset exit\n");
	if (switch_get_state(&hs_data->sdev))
		remove_headset();
	gpio_free(JACK_GPIO);
	gpio_free(DB_DET_GPIO);
	gpio_free(HS_HOOK_DET);
	free_irq(hs_data->hp_det_irq, 0);
	free_irq(hs_data->hook_det_irq, 0);
	destroy_workqueue(g_detection_work_queue);
	destroy_workqueue(g_button_press_work_queue);
	destroy_workqueue(g_button_release_work_queue);
	switch_dev_unregister(&hs_data->sdev);
	input_unregister_device(hs_data->input);
	kfree(hs_data);
}

module_init(headset_init);
module_exit(headset_exit);
