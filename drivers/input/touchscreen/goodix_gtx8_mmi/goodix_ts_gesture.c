/*
 * Goodix GTX5 Gesture Dirver
 *
 * Copyright (C) 2015 - 2016 Goodix, Inc.
 * Authors:  Wang Yafei <wangyafei@goodix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/i2c.h>
#include <linux/mods/wakelock.h>
#include "goodix_ts_core.h"

#define GSX_REG_GESTURE_DATA			0x4100
#define GSX_REG_GESTURE				0x6F68

#define GSX_GESTURE_CMD				0x08

#define QUERYBIT(longlong, bit) (!!(longlong[bit/8] & (1 << bit%8)))

#define GSX_KEY_DATA_LEN	37

#ifdef GOODIX_SENSOR_EN
static struct wake_lock gesture_wakelock;
static struct goodix_ts_core *goodix_core_data;
static int gsx_sensor_init(struct goodix_ts_core *data);

/*****************************************************************************
 * Static variables
 *****************************************************************************/
static struct sensors_classdev __maybe_unused sensors_touch_cdev = {

	.name = "dt-gesture",
	.vendor = "Goodix",
	.version = 1,
	.type = SENSOR_TYPE_MOTO_DOUBLE_TAP,
	.max_range = "5.0",
	.resolution = "5.0",
	.sensor_power = "1",
	.min_delay = -1,
	.max_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#endif
/*
 * struct gesture_module - gesture module data
 * @registered: module register state
 * @sysfs_node_created: sysfs node state
 * @gesture_type: store valied gesture type,each bit stand for a gesture
 * @gesture_data: gesture data
 * @gesture_ts_cmd: gesture command data
 */
struct gesture_module {
	atomic_t registered;
	unsigned int kobj_initialized;
	rwlock_t rwlock;
	unsigned char gesture_type[32];
	unsigned char gesture_data[GSX_KEY_DATA_LEN];
	struct goodix_ext_module module;
	struct goodix_ts_cmd cmd;
};

static struct gesture_module *gsx_gesture; /*allocated in gesture init module*/


/**
 * gsx_gesture_type_show - show valid gesture type
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to output buffer
 * Returns >=0 - succeed,< 0 - failed
 */
static ssize_t gsx_gesture_type_show(struct goodix_ext_module *module,
				char *buf)
{
	int count = 0, i, ret = 0;
	unsigned char *type;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	type = kzalloc(256, GFP_KERNEL);
	if (!type)
		return -ENOMEM;
	read_lock(&gsx_gesture->rwlock);
	for (i = 0; i < 256; i++) {
		if (QUERYBIT(gsx_gesture->gesture_type, i)) {
			type[count] = i;
			count++;
		}
	}
	type[count] = '\0';
	if (count > 0) {
		/* TODO 这里使用scnprintf需要确认一下是否有???*/
		ret = scnprintf(buf, PAGE_SIZE, "%s", type);
	}
	read_unlock(&gsx_gesture->rwlock);

	kfree(type);
	return ret;
}

/**
 * gsx_gesture_type_store - set vailed gesture
 *
 * @module: pointer to goodix_ext_module struct
 * @buf: pointer to valid gesture type
 * @count: length of buf
 * Returns >0 - valid gestures, < 0 - failed
 */
static ssize_t gsx_gesture_type_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	int i;

	if (count <= 0 || count > 256 || buf == NULL) {
		ts_err("Parameter error");
		return -EINVAL;
	}

	write_lock(&gsx_gesture->rwlock);
	memset(gsx_gesture->gesture_type, 0, 32);
	for (i = 0; i < count; i++)
		gsx_gesture->gesture_type[buf[i]/8] |= (0x1 << buf[i]%8);
	write_unlock(&gsx_gesture->rwlock);

	return count;
}

static ssize_t gsx_gesture_enable_show(struct goodix_ext_module *module,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&gsx_gesture->registered));
}

static ssize_t gsx_gesture_enable_store(struct goodix_ext_module *module,
		const char *buf, size_t count)
{
	unsigned int tmp;
	int ret;

	if (kstrtouint(buf, 0, &tmp) != 1) {
		ts_info("Parameter illegal");
		return -EINVAL;
	}
	ts_debug("Tmp value =%d", tmp);

	if (tmp == 1) {
		if (atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready registered");
			return count;
		}
		ret = goodix_register_ext_module(&gsx_gesture->module);
		if (!ret) {
			ts_info("Gesture module registered!");
			atomic_set(&gsx_gesture->registered, 1);
		} else {
			atomic_set(&gsx_gesture->registered, 0);
			ts_err("Gesture module register failed");
		}
	} else if (tmp == 0) {
		if (!atomic_read(&gsx_gesture->registered)) {
			ts_debug("Gesture module has aready unregistered");
			return count;
		}
		ts_debug("Start unregistered gesture module");
		ret = goodix_unregister_ext_module(&gsx_gesture->module);
		if (!ret) {
			atomic_set(&gsx_gesture->registered, 0);
			ts_info("Gesture module unregistered success");
		} else {
			atomic_set(&gsx_gesture->registered, 1);
			ts_info("Gesture module unregistered failed");
		}
	} else {
		ts_err("Parameter error!");
		return -EINVAL;
	}
	return count;
}

static ssize_t gsx_gesture_data_show(struct goodix_ext_module *module,
				char *buf)
{
	int count = GSX_KEY_DATA_LEN;

	if (atomic_read(&gsx_gesture->registered) != 1) {
		ts_info("Gesture module not register!");
		return -EPERM;
	}
	if (!buf || !gsx_gesture->gesture_data[0]) {
		ts_info("Parameter error!");
		return -EPERM;
	}
	read_lock(&gsx_gesture->rwlock);

	count = scnprintf(buf, PAGE_SIZE, "Previous gesture type:0x%x\n",
			  gsx_gesture->gesture_data[2]);
	read_unlock(&gsx_gesture->rwlock);

	return count;
}

const struct goodix_ext_attribute gesture_attrs[] = {
	__EXTMOD_ATTR(type, 0666, gsx_gesture_type_show,
		gsx_gesture_type_store),
	__EXTMOD_ATTR(enable, 0666, gsx_gesture_enable_show,
		gsx_gesture_enable_store),
	__EXTMOD_ATTR(data, 0444, gsx_gesture_data_show, NULL)
};

static int gsx_gesture_init(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	int i, ret;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
#ifdef GOODIX_SENSOR_EN
	static bool initialized_sensor;
#endif
	if (!core_data || !ts_dev->hw_ops->write || !ts_dev->hw_ops->read) {
		ts_err("Register gesture module failed, ts_core unsupported");
		goto exit_gesture_init;
	}

	gsx_gesture->cmd.cmd_reg = GSX_REG_GESTURE;
	gsx_gesture->cmd.length = 3;
	gsx_gesture->cmd.cmds[0] = GSX_GESTURE_CMD;
	gsx_gesture->cmd.cmds[1] = 0x0;
	gsx_gesture->cmd.cmds[2] = 0 - GSX_GESTURE_CMD;
	gsx_gesture->cmd.initialized = 1;

	memset(gsx_gesture->gesture_type, 0, 32);
	memset(gsx_gesture->gesture_data, 0xff, GSX_KEY_DATA_LEN);

	ts_debug("Set gesture type manually");
	memset(gsx_gesture->gesture_type, 0xff, 32);

#ifdef GOODIX_SENSOR_EN
	goodix_core_data = core_data;
	if (!initialized_sensor) {
		wake_lock_init(&gesture_wakelock,
			WAKE_LOCK_SUSPEND, "poll-wake-lock");
		if (!gsx_sensor_init(core_data))
			initialized_sensor = true;
	}
#endif

	if (gsx_gesture->kobj_initialized)
		goto exit_gesture_init;

	ret = kobject_init_and_add(&module->kobj, goodix_get_default_ktype(),
			&core_data->pdev->dev.kobj, "gesture");

	if (ret) {
		ts_err("Create gesture sysfs node error!");
		goto exit_gesture_init;
	}

	for (i = 0; i < (ARRAY_SIZE(gesture_attrs) / ARRAY_SIZE(gesture_attrs));
			i++) {
		if (sysfs_create_file(&module->kobj,
				&gesture_attrs[i].attr)) {
			ts_err("Create sysfs attr file error");
			kobject_put(&module->kobj);
			goto exit_gesture_init;
		}
	}
	gsx_gesture->kobj_initialized = 1;

exit_gesture_init:
	return 0;
}
static int gsx_gesture_exit(struct goodix_ts_core *core_data,
		struct goodix_ext_module *module)
{
	atomic_set(&gsx_gesture->registered, 0);
	return 0;
}

#define  NO_MAKE_4
#ifndef NO_MAKE_4
static void report_gesture_key(struct input_dev *dev, char keycode)
{
	switch (keycode) {
	case 0x11: /* click */
		input_report_key(dev, KEY_F7, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F7, 0);
		input_sync(dev);
		break;
	case 0x22: /* double click */
		input_report_key(dev, KEY_F6, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F6, 0);
		input_sync(dev);
		break;
	case 0xaa: /* up swip */
		input_report_key(dev, KEY_F2, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F2, 0);
		input_sync(dev);
		break;
	case 0xbb: /* right swip */
		input_report_key(dev, KEY_F5, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F5, 0);
		input_sync(dev);
		break;
	case 0xab: /* down swip */
		input_report_key(dev, KEY_F3, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F3, 0);
		input_sync(dev);
		break;
	case 0xba: /* left swip */
		input_report_key(dev, KEY_F4, 1);
		input_sync(dev);
		input_report_key(dev, KEY_F4, 0);
		input_sync(dev);
		break;
	default:
		break;
	}
}
#endif

/**
 * gsx_gesture_ist - Gesture Irq handle
 * This functions is excuted when interrupt happended and
 * ic in doze mode.
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCEL_IRQEVT  stop execute
 */
static int gsx_gesture_ist(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	unsigned char clear_reg = 0;
#ifdef GOODIX_SENSOR_EN
	static int report_cnt = 0;
#endif
	unsigned char checksum = 0, temp_data[GSX_KEY_DATA_LEN];
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_cmd *gesture_cmd = &gsx_gesture->cmd;

	if (atomic_read(&core_data->suspended) == 0)
		return EVT_CONTINUE;

		/* get ic gesture state*/
	ret = ts_dev->hw_ops->read_trans(core_data->ts_dev,
		GSX_REG_GESTURE_DATA,
		temp_data, sizeof(temp_data));
	if (ret < 0 || ((temp_data[0] & 0x20)  == 0)) {
		ts_debug("Read gesture data failed, ret=%d, temp_data[0]=0x%x",
			ret, temp_data[0]);
		goto gesture_ist_exit;
	}

	checksum = checksum_u8(temp_data, sizeof(temp_data));
	if (checksum != 0) {
		ts_err("Gesture data checksum error:0x%x", checksum);
		ts_info("Gesture data %*ph", (int)sizeof(temp_data), temp_data);
		goto re_send_ges_cmd;
	}

	ts_debug("Gesture data:");
	ts_debug("data[0-4]0x%x, 0x%x, 0x%x, 0x%x", temp_data[0], temp_data[1],
		 temp_data[2], temp_data[3]);

	write_lock(&gsx_gesture->rwlock);
	memcpy(gsx_gesture->gesture_data, temp_data, sizeof(temp_data));
	write_unlock(&gsx_gesture->rwlock);

	if (QUERYBIT(gsx_gesture->gesture_type, temp_data[2])) {
		/* do resume routine */
		ts_info("Gesture match success, resume IC");
#ifdef GOODIX_SENSOR_EN
		input_report_abs(core_data->sensor_pdata->input_sensor_dev,
				ABS_DISTANCE,
				++report_cnt);
		ts_info("input report: %d", report_cnt);
		if (report_cnt >= REPORT_MAX_COUNT) {
			report_cnt = 0;
		}
		input_sync(core_data->sensor_pdata->input_sensor_dev);
		wake_lock_timeout(&gesture_wakelock, msecs_to_jiffies(5000));
#else
		input_report_key(core_data->input_dev, KEY_POWER, 1);
		input_sync(core_data->input_dev);
		input_report_key(core_data->input_dev, KEY_POWER, 0);
		input_sync(core_data->input_dev);
#endif
		goto gesture_ist_exit;
	} else {
		ts_info("Unsupported gesture:%x", temp_data[2]);
	}

re_send_ges_cmd:
	if (ts_dev->hw_ops->send_cmd(core_data->ts_dev, gesture_cmd))
		ts_info("warning: failed re_send gesture cmd\n");
gesture_ist_exit:
	ts_dev->hw_ops->write_trans(core_data->ts_dev, GSX_REG_GESTURE_DATA,
			      &clear_reg, 1);
	return EVT_CANCEL_IRQEVT;
}

/**
 * gsx_gesture_before_suspend - execute gesture suspend routine
 * This functions is excuted to set ic into doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_IRQCANCLED  stop execute
 */
static int gsx_gesture_before_suspend(struct goodix_ts_core *core_data,
	struct goodix_ext_module *module)
{
	int ret;
	const struct goodix_ts_hw_ops *hw_ops = core_data->ts_dev->hw_ops;
	struct goodix_ts_cmd *gesture_cmd = &gsx_gesture->cmd;

	if (!gesture_cmd->initialized || hw_ops == NULL) {
		ts_err("Uninitialized doze command or hw_ops");
		return 0;
	}

	ts_info("Before suspend goodix_core_data->wakeable %d!",
		goodix_core_data->wakeable);
	if (goodix_core_data->wakeable) {
		msleep(GTP_60_DLY_MS);
		ret = hw_ops->send_cmd(core_data->ts_dev, gesture_cmd);
		if (ret != 0) {
			ts_err("Send doze command error");
			return 0;
		}

		ts_info("Set IC in doze mode");
		msleep(GTP_20_DLY_MS);
		atomic_set(&core_data->suspended, 1);
		return EVT_CANCEL_SUSPEND;
	} else {
		return 0;
	}
}

static int gsx_sensor_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int ret;
	struct goodix_ts_cmd *gesture_cmd = &gsx_gesture->cmd;
	struct goodix_ts_device *ts_dev = goodix_core_data->ts_dev;

	ts_info("Gesture set enable %d!", enable);
	mutex_lock(&goodix_core_data->state_mutex);
	if (enable == 1) {
		goodix_core_data->wakeable = true;
		//If touch still work in normal mode and screen on, skip enter into gesture mode.
		if (goodix_core_data->gtp_suspended == false
			&& goodix_core_data->screen_state == SCREEN_ON) {
			ts_info("Touch still in normal mode, skip to set to gesture mode");
			mutex_unlock(&goodix_core_data->state_mutex);
			return 0;
		}
		goodix_ts_start_resume(goodix_core_data);

		msleep(GTP_60_DLY_MS);

		ret = ts_dev->hw_ops->send_cmd(goodix_core_data->ts_dev,
			gesture_cmd);
		if (ret != 0) {
			ts_err("Send doze command error");
			mutex_unlock(&goodix_core_data->state_mutex);
			return 0;
		}

	msleep(GTP_20_DLY_MS);

#ifdef GTP_CHARGER
	//charger off
	if ((ts_dev->board_data->charger_detection_enable) &&
		(USB_DETECT_IN == ts_dev->board_data->charger_detection->usb_connected)) {
		goodix_set_charger_bit(USB_DETECT_OUT);
	}

	ts_dev->board_data->charger_send_flage_enable = false;
#endif

	ts_info("Set IC in doze mode");
	atomic_set(&goodix_core_data->suspended, 1);
	goodix_core_data->gtp_suspended = true;

	} else if (enable == 0) {
		goodix_core_data->wakeable = false;
		goodix_ts_start_resume(goodix_core_data);
	} else {
		ts_info("unknown enable symbol\n");
	}
	mutex_unlock(&goodix_core_data->state_mutex);
	return 0;
}

#ifdef GOODIX_SENSOR_EN
static int gsx_sensor_init(struct goodix_ts_core *data)
{
	struct goodix_sensor_platform_data *sensor_pdata;
	struct input_dev *sensor_input_dev;
	int err;

	sensor_input_dev = input_allocate_device();
	if (!sensor_input_dev) {
		ts_err("Failed to allocate device");
		goto exit;
	}

	sensor_pdata = devm_kzalloc(&sensor_input_dev->dev,
			sizeof(struct goodix_sensor_platform_data),
			GFP_KERNEL);
	if (!sensor_pdata) {
		ts_err("Failed to allocate memory");
		goto free_sensor_pdata;
	}
	data->sensor_pdata = sensor_pdata;

	__set_bit(EV_ABS, sensor_input_dev->evbit);
	__set_bit(EV_SYN, sensor_input_dev->evbit);
	input_set_abs_params(sensor_input_dev, ABS_DISTANCE,
			0, REPORT_MAX_COUNT, 0, 0);
	sensor_input_dev->name = "double-tap";
	data->sensor_pdata->input_sensor_dev = sensor_input_dev;

	err = input_register_device(sensor_input_dev);
	if (err) {
		ts_err("Unable to register device, err=%d", err);
		goto free_sensor_input_dev;
	}

	sensor_pdata->ps_cdev = sensors_touch_cdev;
	sensor_pdata->ps_cdev.sensors_enable = gsx_sensor_set_enable;
	sensor_pdata->data = data;

	err = sensors_classdev_register(&sensor_input_dev->dev,
				&sensor_pdata->ps_cdev);
	if (err)
		goto unregister_sensor_input_device;

	return 0;

unregister_sensor_input_device:
	input_unregister_device(data->sensor_pdata->input_sensor_dev);
free_sensor_input_dev:
	input_free_device(data->sensor_pdata->input_sensor_dev);
free_sensor_pdata:
	devm_kfree(&sensor_input_dev->dev, sensor_pdata);
	data->sensor_pdata = NULL;
exit:
	return 1;
}

int gsx_sensor_remove(struct goodix_ts_core *data)
{
	sensors_classdev_unregister(&data->sensor_pdata->ps_cdev);
	input_unregister_device(data->sensor_pdata->input_sensor_dev);
	devm_kfree(&data->sensor_pdata->input_sensor_dev->dev,
		data->sensor_pdata);
	data->sensor_pdata = NULL;
	return 0;
}
#endif

/**
 * gsx_gesture_before_resume - execute gesture resume routine
 * This functions is excuted to make ic out doze mode
 *
 * @core_data: pointer to touch core data
 * @module: pointer to goodix_ext_module struct
 * return: 0 goon execute, EVT_CANCLED  stop execute
 *
 *static int gsx_gesture_before_resume(struct goodix_ts_core *core_data,
 *			struct goodix_ext_module *module)
 *{
 *	struct goodix_ts_device *ts_dev = core_data->ts_dev;
 *
 *	ts_dev->hw_ops->reset(ts_dev);
 *	return 0;
 *}
 */

static struct goodix_ext_module_funcs gsx_gesture_funcs = {
	.irq_event = gsx_gesture_ist,
	.init = gsx_gesture_init,
	.exit = gsx_gesture_exit,
	.before_suspend = gsx_gesture_before_suspend
};

int __init goodix_gsx_gesture_init(void)
{
	/* initialize core_data->ts_dev->gesture_cmd*/
	int result;

	ts_info("gesture module init");
	gsx_gesture = kzalloc(sizeof(struct gesture_module), GFP_KERNEL);
	if (!gsx_gesture)
		result = -ENOMEM;
	gsx_gesture->module.funcs = &gsx_gesture_funcs;
	gsx_gesture->module.priority = EXTMOD_PRIO_GESTURE;
	gsx_gesture->module.name = "Goodix_gsx_gesture";
	gsx_gesture->module.priv_data = gsx_gesture;
	gsx_gesture->kobj_initialized = 0;
	atomic_set(&gsx_gesture->registered, 0);
	rwlock_init(&gsx_gesture->rwlock);
	result = goodix_register_ext_module(&(gsx_gesture->module));
	if (result == 0)
		atomic_set(&gsx_gesture->registered, 1);

	return result;
}

void __exit goodix_gsx_gesture_exit(void)
{
	ts_info("gesture module exit");
	if (gsx_gesture->kobj_initialized)
		kobject_put(&gsx_gesture->module.kobj);
	kfree(gsx_gesture);
	/*return;*/
}

MODULE_DESCRIPTION("Goodix gsx Touchscreen Gesture Module");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
