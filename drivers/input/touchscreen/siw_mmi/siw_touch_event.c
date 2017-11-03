/*
 * siw_touch_event.c - SiW touch event driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include "siw_touch.h"
#include "siw_touch_event.h"


#define __siw_input_report_key(_idev, _code, _value)	\
do {	\
	if (t_dbg_flag & DBG_FLAG_SKIP_IEVENT) {	\
		t_dev_dbg_event(&_idev->dev,	\
		"skip input report: c %d, v %d\n", _code, _value);	\
	} else {	\
		input_report_key(_idev, _code, _value);	\
	}	\
} while (0)

#define siw_input_report_key(_idev, _code, _value)	\
	do {	\
		__siw_input_report_key(_idev, _code, _value);	\
		siwmon_submit_evt(&_idev->dev, "EV_KEY", EV_KEY,	\
		#_code, _code, _value, 0);	\
	} while (0)


#define __siw_input_report_abs(_idev, _code, _value)	\
	do {	\
		if (t_dbg_flag & DBG_FLAG_SKIP_IEVENT) {	\
			t_dev_dbg_event(&_idev->dev,	\
			"skip input report: c %d, v %d\n",	\
			_code, _value);	\
		} else {	\
			input_report_abs(_idev, _code, _value);	\
		}	\
	} while (0)

#define siw_input_report_abs(_idev, _code, _value)	\
	do {	\
		__siw_input_report_abs(_idev, _code, _value);	\
		siwmon_submit_evt(&_idev->dev, "EV_ABS", EV_ABS,	\
		#_code, _code, _value, 0);	\
	} while (0)

static void siw_touch_report_cancel_event(struct siw_ts *ts)
{
	struct input_dev *input = ts->input;
	u16 old_mask = ts->old_mask;
	int i = 0;

	if (!input) {
		t_dev_err(ts->dev, "no input device (cancel)\n");
		return;
	}

	for (i = 0; i < touch_max_finger(ts); i++) {
		if (old_mask & (1 << i)) {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			siw_input_report_key(input, BTN_TOUCH, 1);
			siw_input_report_key(input, BTN_TOOL_FINGER, 1);
			siw_input_report_abs(input, ABS_MT_PRESSURE, 255);
			t_dev_info(&input->dev, "finger canceled <%d> (%4d, %4d, %4d)\n",
						i,
						ts->tdata[i].x,
						ts->tdata[i].y,
						ts->tdata[i].pressure);
		}
	}

	input_sync(input);
}

void siw_touch_report_event(void *ts_data)
{
	struct siw_ts *ts = ts_data;
	struct input_dev *input = ts->input;
	struct device *idev = NULL;
	struct touch_data *tdata = NULL;
	u16 old_mask = ts->old_mask;
	u16 new_mask = ts->new_mask;
	u16 press_mask = 0;
	u16 release_mask = 0;
	u16 change_mask = 0;
	int i;

	if (!input) {
		t_dev_err(ts->dev, "no input device (report)\n");
		return;
	}

	idev = &input->dev;

	change_mask = old_mask ^ new_mask;
	press_mask = new_mask & change_mask;
	release_mask = old_mask & change_mask;

	t_dev_dbg_abs(idev,
			"mask [new: %04x, old: %04x]\n",
			new_mask, old_mask);
	t_dev_dbg_abs(idev,
			"mask [change: %04x, press: %04x, release: %04x]\n",
			change_mask, press_mask, release_mask);

	/* Palm state - Report Pressure value 255 */
	if (ts->is_palm) {
		siw_touch_report_cancel_event(ts);
		ts->is_palm = 0;
	}

	tdata = ts->tdata;
	for (i = 0; i < touch_max_finger(ts); i++, tdata++) {
		if (new_mask & (1 << i)) {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			siw_input_report_key(input, BTN_TOUCH, 1);
			siw_input_report_key(input, BTN_TOOL_FINGER, 1);
			siw_input_report_abs(input, ABS_MT_TRACKING_ID,
				tdata->id);
			siw_input_report_abs(input, ABS_MT_POSITION_X,
				tdata->x);
			siw_input_report_abs(input, ABS_MT_POSITION_Y,
				tdata->y);
			siw_input_report_abs(input, ABS_MT_PRESSURE,
				tdata->pressure);
			siw_input_report_abs(input, ABS_MT_WIDTH_MAJOR,
				tdata->width_major);
			siw_input_report_abs(input, ABS_MT_WIDTH_MINOR,
				tdata->width_minor);
			siw_input_report_abs(input, ABS_MT_ORIENTATION,
				tdata->orientation);

			if (press_mask & (1 << i)) {
				t_dev_dbg_button(idev,
					"%d finger press <%d>(%4d, %4d, %4d)\n",
						ts->tcount,
						i,
						ts->tdata[i].x,
						ts->tdata[i].y,
						ts->tdata[i].pressure);
			}

			continue;
		}

		if (release_mask & (1 << i)) {
			input_mt_slot(input, i);
			input_mt_report_slot_state(input,
				MT_TOOL_FINGER, false);
			t_dev_dbg_button(idev,
				"finger release <%d>(%4d, %4d, %4d)\n",
					i,
					ts->tdata[i].x,
					ts->tdata[i].y,
					ts->tdata[i].pressure);
		}
	}

	if (!ts->tcount) {
		siw_input_report_key(input, BTN_TOUCH, 0);
		siw_input_report_key(input, BTN_TOOL_FINGER, 0);
	}

	ts->old_mask = new_mask;

	input_sync(input);
}

void siw_touch_report_all_event(void *ts_data)
{
	struct siw_ts *ts = ts_data;

	ts->is_palm = 1;
	if (ts->old_mask) {
		ts->new_mask = 0;
		siw_touch_report_event(ts);
		ts->tcount = 0;
		memset(ts->tdata, 0,
			sizeof(struct touch_data) * touch_max_finger(ts));
	}
	ts->is_palm = 0;
}

#if defined(__SIW_SUPPORT_UEVENT)
static void siw_touch_uevent_release(struct device *dev)
{
	if (dev->platform_data)
		dev->platform_data = NULL;
}

static struct siw_touch_uevent_ctrl siw_uevent_ctrl_default = {
	.str = {
		[TOUCH_UEVENT_KNOCK] = {
			"TOUCH_GESTURE_WAKEUP=WAKEUP", NULL},
		[TOUCH_UEVENT_PASSWD] = {
			"TOUCH_GESTURE_WAKEUP=PASSWORD", NULL},
		[TOUCH_UEVENT_SWIPE_RIGHT] = {
			"TOUCH_GESTURE_WAKEUP=SWIPE_RIGHT", NULL},
		[TOUCH_UEVENT_SWIPE_LEFT] = {
			"TOUCH_GESTURE_WAKEUP=SWIPE_LEFT", NULL},
		[TOUCH_UEVENT_GESTURE_C] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_C", NULL},
		[TOUCH_UEVENT_GESTURE_W] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_W", NULL},
		[TOUCH_UEVENT_GESTURE_V] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_V", NULL},
		[TOUCH_UEVENT_GESTURE_O] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_O", NULL},
		[TOUCH_UEVENT_GESTURE_S] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_S", NULL},
		[TOUCH_UEVENT_GESTURE_E] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_E", NULL},
		[TOUCH_UEVENT_GESTURE_M] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_M", NULL},
		[TOUCH_UEVENT_GESTURE_Z] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_Z", NULL},
		[TOUCH_UEVENT_GESTURE_DIR_RIGHT] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_DIR_RIGHT", NULL},
		[TOUCH_UEVENT_GESTURE_DIR_DOWN] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_DIR_DOWN", NULL},
		[TOUCH_UEVENT_GESTURE_DIR_LEFT] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_DIR_LEFT", NULL},
		[TOUCH_UEVENT_GESTURE_DIR_UP] = {
			"TOUCH_GESTURE_WAKEUP=GESTURE_DIR_UP", NULL},
	},
	.flags = 0,
};

#define siw_kobject_uevent_env(_idev, _type, _kobj, _action, _envp_ext)	\
	do {	\
		kobject_uevent_env(_kobj, _action, _envp_ext);	\
		siwmon_submit_evt(&_idev->dev, "@UEVENT", _type,	\
		#_action, _action, 0, 0);	\
	} while (0)

void siw_touch_send_uevent(void *ts_data, int type)
{
	struct siw_ts *ts = ts_data;
	struct input_dev *input = ts->input;
	struct device *idev = NULL;
	struct siw_touch_uevent_ctrl *uevent_ctrl = touch_uevent_ctrl(ts);
	char *str = NULL;

	if (!input) {
		t_dev_err(ts->dev, "no input device (uevent)\n");
		return;
	}

	idev = &ts->input->dev;

	if (uevent_ctrl == NULL)
		uevent_ctrl = &siw_uevent_ctrl_default;

	if (type >= TOUCH_UEVENT_MAX) {
		t_dev_err(idev, "Invalid event type, %d\n", type);
		return;
	}

	if ((uevent_ctrl->str[type] == NULL) ||
		(uevent_ctrl->str[type][0] == NULL)) {
		t_dev_err(idev, "empty event str, %d\n", type);
		return;
	}

	str = uevent_ctrl->str[type][0];

	if (t_dbg_flag & DBG_FLAG_SKIP_UEVENT) {
		t_dev_dbg_event(idev, "skip uevent, %s\n", str);
		return;
	}

	if (atomic_read(&ts->state.uevent) == UEVENT_IDLE) {
	#if defined(__SIW_SUPPORT_WAKE_LOCK)
		wake_lock_timeout(&ts->lpwg_wake_lock,
						msecs_to_jiffies(3000));
	#endif

		atomic_set(&ts->state.uevent, UEVENT_BUSY);

		siw_kobject_uevent_env(input, type, &ts->udev.kobj,
					KOBJ_CHANGE, uevent_ctrl->str[type]);

		t_dev_info(ts->dev, "%s\n", str);
		siw_touch_report_all_event(ts);

		/*
		 * Echo reply to lpwg_data(_store_lpwg_data) is required to
		 * clear(UEVENT_IDLE) state.event
		 */
	}
}

int siw_touch_init_uevent(void *ts_data)
{
	struct siw_ts *ts = ts_data;
	char *drv_name = touch_drv_name(ts);
	int ret = 0;

	if (drv_name) {
		ts->ubus.name = drv_name;
		ts->ubus.dev_name = drv_name;
	} else {
		ts->ubus.name = SIW_TOUCH_NAME;
		ts->ubus.dev_name = SIW_TOUCH_NAME;
	}

	memset((void *)&ts->udev, 0, sizeof(ts->udev));
	ts->udev.bus = &ts->ubus;
	ts->udev.release = siw_touch_uevent_release;

	ret = subsys_system_register(&ts->ubus, NULL);
	if (ret < 0) {
		t_dev_err(ts->dev, "bus is not registered, %d\n", ret);
		goto out_subsys;
	}

	ret = device_register(&ts->udev);
	if (ret < 0) {
		t_dev_err(ts->dev, "device is not registered, %d\n", ret);
		goto out_dev;
	}

	return 0;

out_dev:
	bus_unregister(&ts->ubus);

out_subsys:

	return ret;
}

void siw_touch_free_uevent(void *ts_data)
{
	struct siw_ts *ts = ts_data;

	device_unregister(&ts->udev);

	bus_unregister(&ts->ubus);
}
#else	/* __SIW_SUPPORT_UEVENT */
void siw_touch_send_uevent(void *ts_data, int type)
{

}
int siw_touch_init_uevent(void *ts_data)
{
	return 0;
}

void siw_touch_free_uevent(void *ts_data)
{

}
#endif	/* __SIW_SUPPORT_UEVENT */

#define SIW_TOUCH_PHYS_NAME_SIZE	128

int siw_touch_init_input(void *ts_data)
{
	struct siw_ts *ts = ts_data;
	struct device *dev = ts->dev;
	struct touch_device_caps *caps = &ts->caps;
	struct input_dev *input = NULL;
	char *input_name = NULL;
	char *phys_name = NULL;
	int ret = 0;

	if (ts->input) {
		t_dev_err(dev, "input device has been already allocated!\n");
		return -EINVAL;
	}

	phys_name = touch_kzalloc(dev, SIW_TOUCH_PHYS_NAME_SIZE, GFP_KERNEL);
	if (!phys_name) {
		t_dev_err(dev, "failed to allocate memory for phys_name\n");
		ret = -ENOMEM;
		goto out_phys;
	}

	input = input_allocate_device();
	if (!input) {
		t_dev_err(dev, "failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto out_input;
	}

	input_name = touch_idrv_name(ts);
	input_name = (input_name) ? input_name : SIW_TOUCH_INPUT;

	snprintf(phys_name, SIW_TOUCH_PHYS_NAME_SIZE,
			"%s/%s - %s",
			dev_name(dev->parent),
			dev_name(dev),
			input_name);

	if (touch_flags(ts) & TOUCH_USE_INPUT_PARENT)
		input->dev.parent = dev;
	else {
		/*
		 * To fix sysfs location
		 * With no parent, sysfs folder is created at
		 * '/sys/devices/virtual/input/{input_name}'
		 */
		input->dev.parent = NULL;
	}

	input->name = (const char *)input_name;
	input->phys = (const char *)phys_name;
	memcpy((void *)&input->id, (void *)&ts->i_id, sizeof(input->id));

	set_bit(EV_SYN, input->evbit);
	set_bit(EV_ABS, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_TOUCH, input->keybit);
	set_bit(BTN_TOOL_FINGER, input->keybit);
	set_bit(INPUT_PROP_DIRECT, input->propbit);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0,
				caps->max_x, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0,
				caps->max_y, 0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0,
				caps->max_pressure, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0,
				caps->max_width, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MINOR, 0,
				caps->max_width, 0, 0);
	input_set_abs_params(input, ABS_MT_ORIENTATION, 0,
				caps->max_orientation, 0, 0);

	/* Initialize multi-touch slot */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 6, 0))
	ret = input_mt_init_slots(input, caps->max_id);
#else
	ret = input_mt_init_slots(input, caps->max_id, 0);
#endif
	if (ret < 0) {
		t_dev_err(dev, "failed to initialize input device, %d\n", ret);
		goto out_slot;
	}

	ret = input_register_device(input);
	if (ret < 0) {
		t_dev_err(dev, "failed to register input device, %d\n", ret);
		goto out_reg;
	}

	input_set_drvdata(input, ts);
	ts->input = input;

	t_dev_info(&input->dev, "input device[%s] registered (%d, %d, %d, %d, %d, %d, %d)\n",
			input->phys,
			caps->max_x,
			caps->max_y,
			caps->max_pressure,
			caps->max_width,
			caps->max_width,
			caps->max_orientation,
			caps->max_id);

	return 0;

out_reg:
	input_mt_destroy_slots(input);

out_slot:
	input_free_device(input);

out_input:
	touch_kfree(dev, phys_name);

out_phys:

	return ret;
}

void siw_touch_free_input(void *ts_data)
{
	struct siw_ts *ts = ts_data;
	struct device *dev = ts->dev;
	struct input_dev *input = ts->input;

	if (input) {
		char *phys_name = (char *)input->phys;

		ts->input = NULL;

		t_dev_info(&input->dev, "input device[%s] released\n",
					input->phys);

		input_unregister_device(input);
		input_mt_destroy_slots(input);
		input_free_device(input);

		touch_kfree(dev, phys_name);
	}
}



