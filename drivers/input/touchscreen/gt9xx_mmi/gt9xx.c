/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2010 - 2016 Goodix. Ltd.
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
 *
 * Version: 2.4.0.1
 * Release Date: 2016/10/26
 */

#include <linux/irq.h>
#include <linux/platform_device.h>
#include "gt9xx.h"

#include <linux/input/mt.h>

#define GOODIX_VTG_MIN_UV	2600000
#define GOODIX_VTG_MAX_UV	3300000
#define GOODIX_I2C_VTG_MIN_UV	1800000
#define GOODIX_I2C_VTG_MAX_UV	1800000

#define GOODIX_COORDS_ARR_SIZE	4
#define PROP_NAME_SIZE	24
#define CONFIG_PROC_LEN	3000

static const char *goodix_ts_name = "goodix-ts";
static const char *goodix_input_phys = "input/ts";
struct i2c_client *i2c_connect_client;

static const u16 touch_key_array[] = GTP_KEY_TAB;
#define GTP_MAX_KEY_NUM  (ARRAY_SIZE(touch_key_array))

#if GTP_DEBUG_ON
static const int  key_codes[] =	{
	KEY_HOME, KEY_BACK, KEY_MENU, KEY_HOMEPAGE,
	KEY_F1, KEY_F2, KEY_F3};
static const char * const key_names[] = {
	"Key_Home", "Key_Back", "Key_Menu", "Key_Homepage",
	"KEY_F1", "KEY_F2", "KEY_F3"};
#endif

enum doze {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};

enum {
	GD_MOD_CHARGER,
	GD_MOD_MAX
};

static enum doze doze_status = DOZE_DISABLED;

static ssize_t gt91xx_config_read_proc(struct file *, char __user *,
		size_t, loff_t *);
static ssize_t gt91xx_config_write_proc(struct file *, const char __user *,
		size_t, loff_t *);
static struct proc_dir_entry *gt91xx_config_proc;
static const struct file_operations config_proc_ops = {
	.owner = THIS_MODULE,
	.read = gt91xx_config_read_proc,
	.write = gt91xx_config_write_proc,
};

static s8 gtp_i2c_test(struct i2c_client *client);
static s8 gtp_enter_doze(struct goodix_ts_data *ts);
static int gtp_register_ps_notifier(struct goodix_ts_data *ts);
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id);

static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue =
		NULL;

static int gtp_unregister_powermanger(struct goodix_ts_data *ts);
static void gtp_esd_check_func(struct work_struct *);
static s32 gtp_init_ext_watchdog(struct i2c_client *client);

#if defined(CONFIG_FB)
static int gtp_fb_notifier_callback(struct notifier_block *noti,
		unsigned long event, void *data);
static void fb_notify_resume_work(struct work_struct *work);
#endif

/*******************************************************
* Function:
*	Read data from the i2c slave device.
* Input:
*	client: i2c device.
*	buf[0~1]: read start address.
*	buf[2~len-1]: read data buffer.
*	len: GTP_ADDR_LENGTH + read bytes count
* Output:
*	numbers of i2c_msgs to transfer:
*		2: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	s32 retries = 0;
	struct i2c_msg msgs[2];
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];

	while (retries < RETRY_MAX_TIMES) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}

	if (retries >= RETRY_MAX_TIMES) {
		/*  reset chip would quit doze mode */
		if (ts->pdata->slide_wakeup)
			if (doze_status == DOZE_ENABLED)
				return ret;
		dev_err(&client->dev,
		"I2C Read: 0x%04X, %d bytes failed, errcode: %d! Process reset.",
		(((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
		gtp_reset_guitar(client, 10);
	}

	return ret;
}


/*******************************************************
* Function:
*	Write data to the i2c slave device.
* Input:
*	client: i2c device.
*	buf[0~1]: write start address.
*	buf[2~len-1]: data buffer
*	len: GTP_ADDR_LENGTH + write bytes count
* Output:
*	numbers of i2c_msgs to transfer:
*		1: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	s32 retries = 0;
	struct i2c_msg msg;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;

	while (retries < RETRY_MAX_TIMES) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}

	if (retries >= RETRY_MAX_TIMES) {
		if (ts->pdata->slide_wakeup)
			if (doze_status == DOZE_ENABLED)
				return ret;
		dev_err(&client->dev,
		"I2C Write: 0x%04X, %d bytes failed, errcode: %d! Process reset.",
		(((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
		gtp_reset_guitar(client, 10);
	}

	return ret;
}


/*******************************************************
* Function:
*	i2c read twice, compare the results
* Input:
*	client:	i2c device
*	addr: operate address
*	rxbuf: read data to store, if compare successful
*	len: bytes to read
* Output:
*	FAIL: read failed
*	SUCCESS: read successful
*********************************************************/
s32 gtp_i2c_read_dbl_check(struct i2c_client *client,
		u16 addr, u8 *rxbuf, int len)
{
	u8 buf[16] = {0};
	u8 confirm_buf[16] = {0};
	u8 retry = 0;

	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8)(addr >> 8);
		buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8)(addr >> 8);
		confirm_buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len+2)) {
			memcpy(rxbuf, confirm_buf+2, len);
			return SUCCESS;
		}
	}
	dev_err(&client->dev,
			"I2C read 0x%04X, %d bytes, double check failed!",
			addr, len);

	return FAIL;
}

/*******************************************************
* Function:
*	Send config.
* Input:
*	client: i2c device.
* Output:
*	result of i2c write operation.
*		1: succeed, otherwise: failed
*********************************************************/

s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 2;
	s32 retry = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pnl_init_error) {
		dev_info(&ts->client->dev,
				"Error occurred in init_panel, no config sent");
		return 0;
	}

	if (ts->pdata->driver_send_cfg) {
		dev_info(&ts->client->dev, "Driver send config.");
		for (retry = 0; retry < 5; retry++) {
			ret = gtp_i2c_write(client, ts->pdata->config,
				GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
			if (ret > 0)
				break;
		}
	}

	return ret;
}

/*******************************************************
* Function:
*	Control irq enable or disable.
* Input:
*	ts: goodix i2c_client private data
*	enbale: enbale var.
* Output:
*	result of request_threaded_irq operation.
*		1: succeed, otherwise: failed
*********************************************************/
int gtp_irq_control_enable(struct goodix_ts_data *ts, bool enable)
{
	int retval = 0;

	if (enable) {
		if (ts->irq_enabled)
			return retval;

		/* You can select */
		/* IRQF_TRIGGER_RISING, */
		/* IRQF_TRIGGER_FALLING, */
		/* IRQF_TRIGGER_LOW, */
		/* IRQF_TRIGGER_HIGH */
		retval = request_threaded_irq(ts->client->irq, NULL,
				goodix_ts_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				ts->client->name,
				ts);

		if (retval < 0) {
			dev_err(&ts->client->dev,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		dev_dbg(&ts->client->dev,
				"%s: Started irq thread\n", __func__);
		ts->irq_enabled = true;
	} else {
		if (ts->irq_enabled) {
			disable_irq(ts->client->irq);
			free_irq(ts->client->irq, ts);
			ts->irq_enabled = false;
			dev_dbg(&ts->client->dev,
					"%s: Stopped irq thread\n", __func__);
		}
	}

	return retval;
}

/*******************************************************
* Function:
*	Report touch point event
*Input:
*	ts: goodix i2c_client private data
*	id: trackId
*	x: input x coordinate
*	y: input y coordinate
*	w: input pressure
* Output:
*	None.
*********************************************************/
static void gtp_touch_down(struct goodix_ts_data *ts, s32 id,
	s32 x, s32 y, s32 w)
{
	if (ts->pdata->change_x2y)
		GTP_SWAP(x, y);

	/* readjust coordinate to adapt screen */
	/* pannel_minx/y, x/y_min is always set to 0 */
	/* conversion relation : x = x * (TOUCH_MAX_X / DISPLAY_MAX_X) */
	if (ts->pdata->coordinate_scale) {
		x = x * ts->pdata->panel_maxx / ts->pdata->x_max;
		y = y * ts->pdata->panel_maxy / ts->pdata->y_max;
	}

	if (ts->pdata->ics_slot_report) {
		input_mt_slot(ts->input_dev, id);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
		if (id == PEN_TRACK_ID)
			input_mt_report_slot_state(ts->input_dev,
				MT_TOOL_PEN, true);
		else
			input_mt_report_slot_state(ts->input_dev,
				MT_TOOL_FINGER, true);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	} else {
		if (id & MASK_BIT_8) {/* pen */
			id = PEN_TRACK_ID;
			input_report_abs(ts->input_dev, ABS_MT_TOOL_TYPE, 1);
		} else {/* finger */
			id = id & 0x0F;
			input_report_abs(ts->input_dev, ABS_MT_TOOL_TYPE, 0);
		}
		input_report_key(ts->input_dev, BTN_TOUCH, 1);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

		input_mt_sync(ts->input_dev);
	}

	dev_dbg(&ts->client->dev, "ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

/*******************************************************
* Function:
*	Report touch release event
* Input:
*	ts: goodix i2c_client private data
* Output:
*	None.
*********************************************************/
static void gtp_touch_up(struct goodix_ts_data *ts, s32 id)
{
	if (ts->pdata->ics_slot_report) {
		input_mt_slot(ts->input_dev, id);
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
		dev_dbg(&ts->client->dev, "Touch id[%2d] release!", id);
	} else {
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->input_dev);
	}
}

/*******************************************************
* Function:
*	Pen device init.
* Input:
*	ts: goodix i2c_client private data
* Output:
*	None.
*********************************************************/
static void gtp_pen_init(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	dev_info(&ts->client->dev, "Request input device for pen/stylus.");

	ts->pen_dev = input_allocate_device();
	if (ts->pen_dev == NULL) {
		dev_err(&ts->client->dev,
				"Failed to allocate input device for pen/stylus.");
		return;
	}

	ts->pen_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) |
		BIT_MASK(EV_ABS);
	ts->pen_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	set_bit(BTN_TOOL_PEN, ts->pen_dev->keybit);

	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max,
		 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max,
		 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	ts->pen_dev->name = "goodix-pen";
	ts->pen_dev->id.bustype = BUS_I2C;

	ret = input_register_device(ts->pen_dev);
	if (ret) {
		dev_err(&ts->client->dev,
				"Register %s input device failed",
				ts->pen_dev->name);
		return;
	}
}

/*******************************************************
* Function:
*	Pen down report.
* Input:
*	x: coordinate x.
*	y: coordinate y.
*	id: track id.
* Output:
*	None.
*********************************************************/
static void gtp_pen_down(s32 x, s32 y, s32 w, s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	if (ts->pdata->change_x2y)
		GTP_SWAP(x, y);

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 1);
	if (ts->pdata->ics_slot_report) {
		input_mt_slot(ts->pen_dev, id);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
		input_report_key(ts->pen_dev, BTN_TOUCH, 0);
	} else {
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
		input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
		input_report_key(ts->pen_dev, BTN_TOUCH, 0);
		input_mt_sync(ts->pen_dev);
	}
	dev_dbg(&ts->client->dev, "(%d)(%d, %d)[%d]", id, x, y, w);
}

/*******************************************************
* Function:
*	Pen up report.
* Input:
*	id: track id.
* Output:
*	None.
*********************************************************/
static void gtp_pen_up(s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 0);

	if (ts->pdata->ics_slot_report) {
		input_mt_slot(ts->pen_dev, id);
		/*  input_report_abs(ts->pen_dev, ABS_MT_TRACKING_ID, -1); */
		input_report_key(ts->pen_dev, BTN_TOUCH, 0);
	} else {
		input_report_key(ts->pen_dev, BTN_TOUCH, 0);
	}
}

/*******************************************************
* Function:
*	Goodix touchscreen sensor report function
* Input:
*	ts: goodix tp private data
* Output:
*	None.
 *********************************************************/
static void goodix_ts_sensor_report(struct goodix_ts_data *ts)
{
	u8	end_cmd[3] = {GTP_READ_COOR_ADDR >> 8,
		GTP_READ_COOR_ADDR & 0xFF, 0};
	u8	point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {
		GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
	u8	touch_num = 0;
	u8	finger = 0;
	u8	pen_active = 0;
	u8	dev_active = 0;
	u8	key_value = 0;
	u8	*coor_data = NULL;
	u8	doze_buf[3] = {GTP_REG_DOZE_BUF >> 8, GTP_REG_DOZE_BUF & 0xFF};
	s32	input_x = 0;
	s32	input_y = 0;
	s32	input_w = 0;
	s32	id = 0;
	s32	i = 0;
	s32	ret = -1;
	static	u16 pre_touch = (u16)0;
	static	u8 pre_key = (u8)0;
	static	u8 pre_pen = (u8)0;
	static	u8 pre_finger = (u8)0;

	GTP_DEBUG_FUNC();
	if (ts->enter_update)
		return;
	if (ts->pdata->slide_wakeup && DOZE_ENABLED == doze_status) {
		ret = gtp_i2c_read(i2c_connect_client, doze_buf, 3);
		dev_dbg(&ts->client->dev, "0x814B = 0x%02X", doze_buf[2]);
		if (ret > 0) {
			if ((doze_buf[2] == 'a') ||
					(doze_buf[2] == 'b') ||
					(doze_buf[2] == 'c') ||
					(doze_buf[2] == 'd') ||
					(doze_buf[2] == 'e') ||
					(doze_buf[2] == 'g') ||
					(doze_buf[2] == 'h') ||
					(doze_buf[2] == 'm') ||
					(doze_buf[2] == 'o') ||
					(doze_buf[2] == 'q') ||
					(doze_buf[2] == 's') ||
					(doze_buf[2] == 'v') ||
					(doze_buf[2] == 'w') ||
					(doze_buf[2] == 'y') ||
					(doze_buf[2] == 'z') ||
					(doze_buf[2] == 0x5E) ||
					(doze_buf[2] == 0x3E)/* ^ */
			   ) {
				if (doze_buf[2] != 0x5E)
					dev_info(&ts->client->dev,
					"Wakeup by gesture(%c), light up the screen!",
					doze_buf[2]);
				else
					dev_info(&ts->client->dev,
					"Wakeup by gesture(^), light up the screen!");
				doze_status = DOZE_WAKEUP;
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);
				/*  clear 0x814B */
				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else if ((doze_buf[2] == 0xAA) ||
					(doze_buf[2] == 0xBB) ||
					(doze_buf[2] == 0xAB) ||
					(doze_buf[2] == 0xBA)) {
				char *direction[4] = {
					"Right", "Down", "Up", "Left"};
				u8 type = ((doze_buf[2] & 0x0F) - 0x0A) +
				(((doze_buf[2] >> 4) & 0x0F) - 0x0A) * 2;

				dev_info(&ts->client->dev,
					"%s slide to light up the screen!",
					direction[type]);
				doze_status = DOZE_WAKEUP;
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);
				/*  clear 0x814B */
				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else if (doze_buf[2] == 0xCC) {
				dev_info(&ts->client->dev,
					"Double click to light up the screen!");
				doze_status = DOZE_WAKEUP;
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);
				/*  clear 0x814B */
				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else {
				/*  clear 0x814B */
				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
				gtp_enter_doze(ts);
			}
		}
		return;
	}

	ret = gtp_i2c_read(ts->client, point_data, 12);
	if (ret < 0) {
		dev_err(&ts->client->dev,
				"I2C transfer error. errno:%d\n ", ret);
		return;
	}

	finger = point_data[GTP_ADDR_LENGTH];

	if (finger == 0x00)
		return;

	if ((finger & MASK_BIT_8) == 0)
		goto exit_sensor_report;

	touch_num = finger & 0x0f;
	if (touch_num > GTP_MAX_TOUCH)
		goto exit_sensor_report;

	if (touch_num > 1) {
		u8 buf[8 * GTP_MAX_TOUCH] = {
			(GTP_READ_COOR_ADDR + 10) >> 8,
			(GTP_READ_COOR_ADDR + 10)
				& 0xff};

		ret = gtp_i2c_read(ts->client, buf, 2 + 8 * (touch_num - 1));
		memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
	}

	if (ts->pdata->have_touch_key) {
	key_value = point_data[3 + 8 * touch_num];

	if (key_value || pre_key) {
		for (i = 0; i < GTP_MAX_KEY_NUM; i++) {
#if GTP_DEBUG_ON
			for (ret = 0; ret < GTP_MAX_KEY_NUM; ++ret) {
				if (key_codes[ret] == touch_key_array[i]) {
					dev_dbg(&ts->client->dev, "Key: %s %s",
					key_names[ret],
					(key_value & (0x01 << i)) ?
					"Down" : "Up");
					break;
				}
			}
#endif
			input_report_key(ts->input_dev, touch_key_array[i],
					key_value & (0x01<<i));
		}
		/* 0x20_UPKEY 0X10_DOWNKEY 0X40_ALLKEYDOWN */

		if ((pre_key == 0x20) || (key_value == 0x20)
				|| (pre_key == 0x10) || (key_value == 0x10)
				|| (key_value == 0x40) || (pre_key == 0x40)) {
			/* do nothing */
		} else {
			touch_num = 0;
		}
		dev_active = 1;
	}
	}
	pre_key = key_value;

	dev_dbg(&ts->client->dev,
			"pre_touch:%02x, finger:%02x.", pre_touch, finger);

	if (ts->pdata->ics_slot_report) {
	if (pre_touch || touch_num) {
		s32 pos = 0;
		u16 touch_index = 0;
		u8 report_num = 0;

		coor_data = &point_data[3];

		if (touch_num) {
			id = coor_data[pos];

			if (ts->pdata->with_pen) {
				id = coor_data[pos];
				input_x  = coor_data[pos + 1] |
						(coor_data[pos + 2]
						<< 8);
				input_y  = coor_data[pos + 3] |
						(coor_data[pos + 4]
						<< 8);
				input_w  = coor_data[pos + 5] |
						(coor_data[pos + 6]
						<< 8);
				if (id & MASK_BIT_8) {
					if (!input_w) {
						dev_dbg(&ts->client->dev,
						"Pen touch DOWN(Slot)!");
						/* hover */
						if (pre_finger) {
							dev_active = 1;
							pre_finger = 0;
							for (i = 0;
							i < GTP_MAX_TOUCH; i++)
								gtp_touch_up(
									ts, i);
						}

						gtp_pen_down(input_x, input_y,
								input_w, 0);
						pre_pen = 1;
						pre_touch = 1;
						pen_active = 1;
					} else {
						if (pre_pen) {
							dev_dbg(
							&ts->client->dev,
							"Pen touch UP(Slot)!");
							gtp_pen_up(0);
							pen_active = 1;
							pre_pen = 0;
						}
						id = PEN_TRACK_ID;
					}
				}
			} else {
				if (id & MASK_BIT_8)
					id = PEN_TRACK_ID;
			}
			touch_index |= (0x01<<id);
		} else {
			if (pre_pen) {
				dev_dbg(&ts->client->dev, "Pen touch UP(Slot)!");
				gtp_pen_up(0);
				pen_active = 1;
				pre_pen = 0;
			}
		}

		dev_dbg(&ts->client->dev,
				"id = %d,touch_index = 0x%x, pre_touch = 0x%x\n",
				id, touch_index, pre_touch);
		for (i = 0; i < GTP_MAX_TOUCH; i++) {
			if (ts->pdata->with_pen) {
				if (pre_pen)
					break;
			}

			if ((touch_index & (0x01<<i))) {
				dev_dbg(&ts->client->dev,
						"Devices touch Down(Slot)!");
				pre_finger = 1;
				input_x  = coor_data[pos + 1] |
					(coor_data[pos + 2] << 8);
				input_y  = coor_data[pos + 3] |
					(coor_data[pos + 4] << 8);
				input_w  = coor_data[pos + 5] |
					(coor_data[pos + 6] << 8);

				gtp_touch_down(ts, id, input_x,
					input_y, input_w);
				pre_touch |= 0x01 << i;

				report_num++;
				if (report_num < touch_num) {
					pos += 8;
					id = coor_data[pos] & 0x0F;
					touch_index |= (0x01<<id);
				}
			} else {
				dev_dbg(&ts->client->dev,
						"Devices touch Up(Slot)!");
				gtp_touch_up(ts, i);
				pre_touch &= ~(0x01 << i);
			}
			dev_active = 1;
		}
		pre_touch = touch_num;
	}
	} else {
	if (touch_num) {
		for (i = 0; i < touch_num; i++) {
			coor_data = &point_data[i * 8 + 3];
			input_x  = coor_data[1] | (coor_data[2] << 8);
			input_y  = coor_data[3] | (coor_data[4] << 8);
			input_w  = coor_data[5] | (coor_data[6] << 8);
			id = coor_data[0];
			if (ts->pdata->with_pen && (id & MASK_BIT_8) &&
				!input_w) {
				dev_dbg(&ts->client->dev,
						"Pen touch DOWN:(%d)(%d, %d)[%d]",
						id, input_x, input_y, input_w);
				gtp_pen_down(input_x, input_y, input_w, 0);
				pre_pen = 1;
				pen_active = 1;
				if (pre_finger) {
					gtp_touch_up(ts, 0);
					dev_active = 1;
					pre_finger = 0;
				}
			} else {
				if (ts->pdata->with_pen) {
					if (pre_pen) {
						dev_dbg(&ts->client->dev,
							"Pen touch UP!");
						gtp_pen_up(0);
						pre_pen = 0;
						pen_active = 1;
					}
				}

				dev_dbg(&ts->client->dev,
						" (%d)(%d, %d)[%d]", id,
						input_x, input_y, input_w);
				dev_active = 1;
				pre_finger = 1;
				gtp_touch_down(ts, id, input_x, input_y,
						input_w);
			}
		}
	} else if (pre_touch) {
		if (ts->pdata->with_pen) {
			dev_dbg(&ts->client->dev,
				"@@@@ touch UP ,pre_pen is %d,pre_finger is %d!",
				pre_pen, pre_finger);
			if (pre_pen) {
				dev_dbg(&ts->client->dev, "Pen touch UP!");
				gtp_pen_up(0);
				pre_pen = 0;
				pen_active = 1;
			}
		}

		if (pre_finger) {
			dev_dbg(&ts->client->dev, "Touch Release!");
			dev_active = 1;
			pre_finger = 0;
			gtp_touch_up(ts, 0);
		}
	}

	pre_touch = touch_num;
	}

	if (ts->pdata->with_pen) {
		if (pen_active) {
			pen_active = 0;
			input_sync(ts->pen_dev);
		}
	}
	if (dev_active) {
		dev_active = 0;
		input_sync(ts->input_dev);
	}

exit_sensor_report:
	if (!ts->gtp_rawdiff_mode) {
		ret = gtp_i2c_write(ts->client, end_cmd, 3);
		if (ret < 0)
			dev_info(&ts->client->dev, "I2C write end_cmd error!");
	}
}

/*******************************************************
* Function:
*	Timer interrupt service routine for polling mode.
* Input:
*	timer: timer struct pointer
* Output:
*	Timer work mode.
* HRTIMER_NORESTART:
*	no restart mode
 *********************************************************/
static enum hrtimer_restart goodix_ts_timer_handler(struct hrtimer *timer)
{
	struct goodix_ts_data *ts = container_of(timer,
			struct goodix_ts_data, timer);

	GTP_DEBUG_FUNC();

	goodix_ts_sensor_report(ts);
	hrtimer_start(&ts->timer, ktime_set(0,
				(GTP_POLL_TIME+6)*1000000), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

/*******************************************************
* Function:
*	External interrupt service routine for interrupt mode.
* Input:
*	irq: interrupt number.
*	dev_id: private data pointer
* Output:
*	Handle Result.
* IRQ_HANDLED:
*	interrupt handled successfully
 *********************************************************/
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	GTP_DEBUG_FUNC();

	goodix_ts_sensor_report(ts);

	return IRQ_HANDLED;
}

/*******************************************************
* Function:
*	Synchronization.
* Input:
*	ts: goodix tp private data
*	ms: synchronization time in millisecond.
* Output:
*	None.
 *******************************************************/
void gtp_int_sync(struct goodix_ts_data *ts, s32 ms)
{
	msleep(ms);
	GTP_GPIO_AS_INT(ts->pdata->irq_gpio);
}


/*******************************************************
* Function:
*	Reset chip.
* Input:
*	client:	i2c device.
*	ms: reset time in millisecond
* Output:
*	None.
 *******************************************************/
void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();
	dev_info(&ts->client->dev, "Guitar reset");

	GTP_GPIO_OUTPUT(ts->pdata->rst_gpio, 0);
	usleep_range(ms*1000, ms*1000 + 1000);	/*  T2: > 10ms */

	usleep_range(2000, 3000);		/*  T3: > 100us (2ms)*/
	GTP_GPIO_OUTPUT(ts->pdata->rst_gpio, 1);

	usleep_range(6000, 7000);		/*  T4: > 5ms */

	GTP_GPIO_AS_INPUT(ts->pdata->rst_gpio);
	/*  end select I2C slave addr */

	gtp_int_sync(ts, 50);
	if (ts->pdata->esd_protect)
		gtp_init_ext_watchdog(client);
}

/*******************************************************
* Function:
*	Enter doze mode for sliding wakeup.
* Input:
*	ts: goodix tp private data
* Output:
*	1: succeed, otherwise failed
 *******************************************************/
static s8 gtp_enter_doze(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8),
		(u8)GTP_REG_SLEEP, 8};

	GTP_DEBUG_FUNC();

	dev_dbg(&ts->client->dev, "Entering gesture mode.");
	while (retry++ < 5) {
		i2c_control_buf[0] = (u8)(GTP_REG_COMMAND_CHECK >> 8);
		i2c_control_buf[1] = (u8)GTP_REG_COMMAND_CHECK;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret < 0) {
			dev_dbg(&ts->client->dev,
					"failed to set doze flag into 0x8046, %d",
					retry);
			continue;
		}
		i2c_control_buf[0] = (u8)(GTP_REG_SLEEP >> 8);
		i2c_control_buf[1] = (u8)GTP_REG_SLEEP;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			doze_status = DOZE_ENABLED;
			dev_info(&ts->client->dev, "Gesture mode enabled.");
			return ret;
		}
		usleep_range(10000, 11000);
	}
	dev_err(&ts->client->dev, "GTP send gesture cmd failed.");

	return ret;
}

/*******************************************************
* Function:
*	Enter sleep mode.
* Input:
*	ts: private data.
* Output:
*	Executive outcomes.
*		1: succeed, otherwise failed.
 *******************************************************/
static s8 gtp_enter_sleep(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8),
		(u8)GTP_REG_SLEEP, 5};

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			dev_info(&ts->client->dev, "GTP enter sleep!");

			return ret;
		}
		usleep_range(10000, 11000);
	}
	dev_err(&ts->client->dev, "GTP send sleep cmd failed.");

	return ret;
}

/*******************************************************
* Function:
*	Wakeup from sleep.
* Input:
*	ts: private data.
* Output:
*	Executive outcomes.
*		0: succeed, otherwise: failed.
 *******************************************************/
static s8 gtp_wakeup_sleep(struct goodix_ts_data *ts)
{
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

	if (ts->pdata->wakeup_with_reset) {
		while (retry++ < 5) {
			gtp_reset_guitar(ts->client, 20);

			dev_info(&ts->client->dev, "GTP wakeup sleep.");
			return 1;
		}
	} else {
		while (retry++ < 10) {
			if (ts->pdata->slide_wakeup) {
				if (doze_status != DOZE_WAKEUP)
					dev_info(&ts->client->dev,
							"Powerkey wakeup.");
				else
					dev_info(&ts->client->dev,
							"Gesture wakeup.");

				doze_status = DOZE_DISABLED;
				gtp_irq_control_enable(ts, false);
				gtp_reset_guitar(ts->client, 10);
				gtp_irq_control_enable(ts, true);
			}

			ret = gtp_i2c_test(ts->client);
			if (ret > 0) {
				dev_info(&ts->client->dev,
						"GTP wakeup sleep.");

				if (!ts->pdata->slide_wakeup) {
					gtp_int_sync(ts, 25);
					if (ts->pdata->esd_protect)
						gtp_init_ext_watchdog(
							ts->client);
				}

				return ret;
			}
			gtp_reset_guitar(ts->client, 20);
		}
	}

	dev_err(&ts->client->dev, "GTP wakeup sleep failed.");

	return ret;
}

/*******************************************************
* Function:
*	Initialize gtp.
* Input:
*	ts: goodix private data
* Output:
*	Executive outcomes.
*		0: succeed, otherwise: failed
 *******************************************************/
static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = -1;
	s32 i = 0;

	u8 check_sum = 0;
	u8 opr_buf[16] = {0};
	u8 sensor_id = 0;
	u8 drv_cfg_version = 0;
	u8 flash_cfg_version = 0;

	/* if defined CONFIG_OF, parse config data from dtsi
	 *	else parse config data form header file.
	 */
#ifndef	CONFIG_OF
	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;

	u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1,
		cfg_info_group2, cfg_info_group3,
		cfg_info_group4, cfg_info_group5};
	u8 cfg_info_len[] = {CFG_GROUP_LEN(cfg_info_group0),
		CFG_GROUP_LEN(cfg_info_group1),
		CFG_GROUP_LEN(cfg_info_group2),
		CFG_GROUP_LEN(cfg_info_group3),
		CFG_GROUP_LEN(cfg_info_group4),
		CFG_GROUP_LEN(cfg_info_group5)};

	if (ts->pdata->driver_send_cfg) {
		dev_dbg(&ts->client->dev,
			"Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
			cfg_info_len[0], cfg_info_len[1], cfg_info_len[2],
			cfg_info_len[3], cfg_info_len[4], cfg_info_len[5]);
	}
#endif
	if (ts->pdata->driver_send_cfg) {
	/* check firmware */
	ret = gtp_i2c_read_dbl_check(ts->client, 0x41E4, opr_buf, 1);
	if (ret == SUCCESS) {
		if (opr_buf[0] != 0xBE) {
			ts->fw_error = 1;
			dev_err(&ts->client->dev,
					"Firmware error, no config sent!");
			return -EPERM;
		}
	}
	/* read sensor id */
	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_SENSOR_ID,
			&sensor_id, 1);
	if (ret == SUCCESS) {
		if (sensor_id >= 0x06) {
			dev_err(&ts->client->dev,
					"Invalid sensor_id(0x%02X), No Config Sent!",
					sensor_id);

			ts->pnl_init_error = 1;
			return -EPERM;
		}
	} else {
		dev_err(&ts->client->dev,
				"Failed to get sensor_id, No config sent!");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
	dev_info(&ts->client->dev, "Sensor_ID: %d", sensor_id);

	/* parse config data*/
#ifdef CONFIG_OF
	dev_dbg(&ts->client->dev, "Get config data from device tree.");
	ret = gtp_parse_dt_cfg(&ts->client->dev,
			&ts->pdata->config[GTP_ADDR_LENGTH],
			&ts->gtp_cfg_len, sensor_id);
	if (ret < 0) {
		dev_err(&ts->client->dev,
				"Failed to parse config data form device tree.");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
#else
	dev_dbg(&ts->client->dev, "Get config data from header file.");
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
			(!cfg_info_len[3]) && (!cfg_info_len[4]) &&
			(!cfg_info_len[5])) {
		sensor_id = 0;
	}
	ts->gtp_cfg_len = cfg_info_len[sensor_id];
	memset(&ts->pdata->config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&ts->pdata->config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id],
			ts->gtp_cfg_len);
#endif

	dev_info(&ts->client->dev, "Config group%d used,length: %d",
			sensor_id, ts->gtp_cfg_len);

	if (ts->gtp_cfg_len < GTP_CONFIG_MIN_LENGTH) {
		dev_err(&ts->client->dev,
		"Config Group%d is INVALID CONFIG GROUP(Len: %d)! NO Config Sent! You need to check you header file CFG_GROUP section!"
		, sensor_id, ts->gtp_cfg_len);
		ts->pnl_init_error = 1;
		return -EPERM;
	}

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA,
			&opr_buf[0], 1);
	if (ret == SUCCESS) {
		dev_dbg(&ts->client->dev,
			"Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X",
			ts->pdata->config[GTP_ADDR_LENGTH],
			ts->pdata->config[GTP_ADDR_LENGTH],
			opr_buf[0], opr_buf[0]);
		flash_cfg_version = opr_buf[0];
		drv_cfg_version = ts->pdata->config[GTP_ADDR_LENGTH];

		if (flash_cfg_version < 90 &&
				flash_cfg_version > drv_cfg_version)
			ts->pdata->config[GTP_ADDR_LENGTH] = 0x00;

	} else {
		dev_err(&ts->client->dev,
				"Failed to get ic config version!No config sent!");
		return -EPERM;
	}

#if GTP_CUSTOM_CFG
	ts->pdata->config[RESOLUTION_LOC] = (u8)GTP_MAX_WIDTH;
	ts->pdata->config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
	ts->pdata->config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	ts->pdata->config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);

	if (GTP_INT_TRIGGER == 0)  /* RISING */{
		ts->pdata->config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1)	/* FALLING */{
		ts->pdata->config[TRIGGER_LOC] |= 0x01;
	}
#endif	/*  GTP_CUSTOM_CFG */

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++)
		check_sum += ts->pdata->config[i];

	ts->pdata->config[ts->gtp_cfg_len] = (~check_sum) + 1;
	} else {
		/*  driver not send config */
		dev_info(&ts->client->dev, "driver not send config");
		ts->gtp_cfg_len = GTP_CONFIG_MAX_LENGTH;
		ret = gtp_i2c_read(ts->client,
				ts->pdata->config, ts->gtp_cfg_len +
				GTP_ADDR_LENGTH);
		if (ret < 0) {
			dev_err(&ts->client->dev,
			"Read Config Failed, Using Default Resolution & INT Trigger!");
			ts->abs_x_max = GTP_MAX_WIDTH;
			ts->abs_y_max = GTP_MAX_HEIGHT;
			ts->int_trigger_type = GTP_INT_TRIGGER;
		}
	} /* GTP_DRIVER_SEND_CFG */

	if ((ts->abs_x_max == 0) && (ts->abs_y_max == 0)) {
		dev_info(&ts->client->dev,
				"Set trigger %d from IC config",
				(ts->pdata->config[TRIGGER_LOC]) & 0x03);
		ts->abs_x_max = (ts->pdata->config[RESOLUTION_LOC + 1] << 8) +
			ts->pdata->config[RESOLUTION_LOC];
		ts->abs_y_max = (ts->pdata->config[RESOLUTION_LOC + 3] << 8) +
			ts->pdata->config[RESOLUTION_LOC + 2];
		ts->int_trigger_type = (ts->pdata->config[TRIGGER_LOC]) & 0x03;
	}

	if (ts->pdata->driver_send_cfg) {
		ret = gtp_send_cfg(ts->client);
		if (ret < 0)
			dev_err(&ts->client->dev, "Send config error.");
		if (flash_cfg_version < 90 &&
				flash_cfg_version > drv_cfg_version) {
			check_sum = 0;
			ts->pdata->config[GTP_ADDR_LENGTH] = drv_cfg_version;
			for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++)
				check_sum += ts->pdata->config[i];

			ts->pdata->config[ts->gtp_cfg_len] = (~check_sum) + 1;
		}
	}

	dev_info(&ts->client->dev, "X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x",
			ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);

	usleep_range(10000, 11000); /* 10 ms */

	return 0;
}

static ssize_t gt91xx_config_read_proc(struct file *file,
		char __user *page, size_t size, loff_t *ppos)
{
	int i;
	unsigned char *read_buf = NULL;
	int num_read_chars = 0;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {
		(u8)(GTP_REG_CONFIG_DATA >> 8),
		(u8)GTP_REG_CONFIG_DATA};
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	if (*ppos)
		return 0;

	read_buf = kmalloc(CONFIG_PROC_LEN, GFP_KERNEL);
	if (read_buf == NULL)
		return -ENOMEM;

	num_read_chars += snprintf(read_buf, 50,
		"==== GT9XX config init value====\n");
	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		num_read_chars += snprintf(&read_buf[num_read_chars],
			10, "0x%02X ", ts->pdata->config[i + 2]);

		if (i % 8 == 7)
			num_read_chars +=
				snprintf(&read_buf[num_read_chars], 10, "\n");
	}

	num_read_chars += snprintf(&read_buf[num_read_chars], 10, "\n");

	num_read_chars += snprintf(&read_buf[num_read_chars], 50,
		"==== GT9XX config real value====\n");
	gtp_i2c_read(i2c_connect_client, temp_data, GTP_CONFIG_MAX_LENGTH + 2);
	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		num_read_chars += snprintf(&read_buf[num_read_chars], 10,
		"0x%02X ", temp_data[i+2]);

		if (i % 8 == 7)
			num_read_chars +=
				snprintf(&read_buf[num_read_chars], 10, "\n");
	}

	if (copy_to_user(page, read_buf, num_read_chars)) {
		kfree(read_buf);
		return -EFAULT;
	}
	kfree(read_buf);

	*ppos += num_read_chars;

	return num_read_chars;
}

static ssize_t gt91xx_config_write_proc(struct file *filp,
		const char __user *buffer, size_t count, loff_t *off)
{
	s32 ret = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	dev_dbg(&ts->client->dev, "write count %zd\n", count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		dev_err(&ts->client->dev, "size not match [%d:%zd]\n",
				GTP_CONFIG_MAX_LENGTH, count);
		return -EFAULT;
	}

	if (copy_from_user(&ts->pdata->config[2], buffer, count)) {
		dev_err(&ts->client->dev, "copy from user fail\n");
		return -EFAULT;
	}

	ret = gtp_send_cfg(i2c_connect_client);

	if (ret < 0)
		dev_err(&ts->client->dev, "send config failed.");

	return count;
}

/* Attribute: path (RO) */
static ssize_t path_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);
	const char *path;
	ssize_t blen;

	path = kobject_get_path(&data->client->dev.kobj, GFP_KERNEL);
	blen = scnprintf(buf, PAGE_SIZE, "%s\n", path ? path : "na");
	kfree(path);
	return blen;
}

/* Attribute: vendor (RO) */
static ssize_t vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "goodix");
}

static struct device_attribute touchscreen_attributes[] = {
	__ATTR_RO(path),
	__ATTR_RO(vendor),
	__ATTR_NULL
};

#define TSDEV_MINOR_BASE 128
#define TSDEV_MINOR_MAX 32
static int goodix_ts_sysfs_class(void *_data, bool create)
{
	struct goodix_ts_data *data = _data;
	struct device_attribute *attrs = touchscreen_attributes;
	static struct class *touchscreen_class;
	static struct device *ts_class_dev;
	static int minor;
	int i, error = 0;

	if (create) {
		minor = input_get_new_minor(data->client->addr,
				1, false);
		if (minor < 0)
			minor = input_get_new_minor(TSDEV_MINOR_BASE,
					TSDEV_MINOR_MAX, true);

		dev_info(&data->client->dev, "assigned minor %d\n", minor);

		touchscreen_class = class_create(THIS_MODULE, "touchscreen");
		if (IS_ERR(touchscreen_class)) {
			error = PTR_ERR(touchscreen_class);
			touchscreen_class = NULL;
			return error;
		}

		ts_class_dev = device_create(touchscreen_class, NULL,
				MKDEV(INPUT_MAJOR, minor),
				data, "gt9xx");
		if (IS_ERR(ts_class_dev)) {
			error = PTR_ERR(ts_class_dev);
			ts_class_dev = NULL;
			return error;
		}

		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			error = device_create_file(ts_class_dev, &attrs[i]);
			if (error)
				break;
		}

		if (error)
			goto device_destroy;
	} else {
		if (!touchscreen_class || !ts_class_dev)
			return -ENODEV;

		for (i = 0; attrs[i].attr.name != NULL; ++i)
			device_remove_file(ts_class_dev, &attrs[i]);

		device_unregister(ts_class_dev);
		class_unregister(touchscreen_class);
	}

	return 0;

device_destroy:
	for (--i; i >= 0; --i)
		device_remove_file(ts_class_dev, &attrs[i]);
	device_destroy(touchscreen_class, MKDEV(INPUT_MAJOR, minor));
	ts_class_dev = NULL;
	class_unregister(touchscreen_class);
	dev_err(&data->client->dev, "error creating touchscreen class\n");

	return -ENODEV;
}

static ssize_t gtp_poweron_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);
	bool val;

	mutex_lock(&data->input_dev->mutex);
	val = data->gtp_suspended;
	mutex_unlock(&data->input_dev->mutex);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			val == false);
}

static DEVICE_ATTR(poweron, S_IRUGO,
		gtp_poweron_show,
		NULL);

#define FW_NAME_MAX_LEN	50
static ssize_t gtp_doreflash_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	char update_file_name[FW_NAME_MAX_LEN];
	int retval;

	if (count > FW_NAME_MAX_LEN) {
		dev_info(&ts->client->dev, "FW filename is too long");
		retval = -EINVAL;
		goto exit;
	}

	if (ts->gtp_suspended) {
		dev_info(&ts->client->dev,
			"Can't start fw upgrade. Device is in suspend state.");
		retval = -EBUSY;
		goto exit;
	}

	if (ts->fw_loading) {
		dev_info(&ts->client->dev, "In FW flashing state, try again later");
		retval = -EINVAL;
		goto exit;
	}

	strlcpy(update_file_name, buf, count);

	mutex_lock(&ts->input_dev->mutex);
	gtp_irq_control_enable(ts, false);
	ts->fw_loading = true;
	/*if (config_enabled(CONFIG_GT9XX_TOUCHPANEL_UPDATE)) {*/
	retval = gup_update_proc(update_file_name);
	if (retval == FAIL)
		dev_err(&ts->client->dev,
				"Fail to update GTP firmware.\n");
	else {
		retval = gtp_read_version(ts->client,
				&ts->version_info, ts->product_id);
		if (retval < 0)
			dev_err(&ts->client->dev, "Update version failed.");
	}
	/*}*/
	ts->fw_loading = false;
	gtp_irq_control_enable(ts, true);
	mutex_unlock(&ts->input_dev->mutex);

	return count;

exit:
	return retval;
}

static DEVICE_ATTR(doreflash, (S_IWUSR | S_IWGRP),
		NULL,
		gtp_doreflash_store);

static ssize_t gtp_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			(data->fw_loading) ? 1 : 0);
}

static DEVICE_ATTR(flashprog, S_IRUGO,
		gtp_flashprog_show,
		NULL);

static ssize_t gtp_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	if (data->product_id[3] == 0x00) {
		return scnprintf(buf, PAGE_SIZE, "GT%c%c%c\n",
				data->product_id[0], data->product_id[1],
				data->product_id[2]);
	} else {
		return scnprintf(buf, PAGE_SIZE, "GT%c%c%c%c\n",
			data->product_id[0], data->product_id[1],
			data->product_id[2], data->product_id[3]);
	}
}

static DEVICE_ATTR(productinfo, S_IRUGO,
		gtp_productinfo_show,
		NULL);

static ssize_t gtp_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%04x\n",
			data->version_info);
}

static DEVICE_ATTR(buildid, S_IRUGO,
		gtp_buildid_show,
		NULL);

static ssize_t gtp_forcereflash_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	unsigned int input;

	if (kstrtouint(buf, 10, &input) != 0)
		return -EINVAL;

	ts->force_update = (input == 0) ? false : true;

	return size;
}

static DEVICE_ATTR(forcereflash, (S_IWUSR | S_IWGRP),
		NULL,
		gtp_forcereflash_store);

static ssize_t gtp_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		dev_err(dev, "Failed to convert value\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
		/* Disable irq */
		if (data->irq_enabled)
			gtp_irq_control_enable(data, false);
		break;
	case 1:
		/* Enable irq */
		if (!data->irq_enabled)
			gtp_irq_control_enable(data, true);
		break;
	default:
		dev_err(dev, "Invalid value\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t gtp_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			data->irq_enabled ? "ENABLED" : "DISABLED");
}

static DEVICE_ATTR(drv_irq, (S_IRUGO | S_IWUSR | S_IWGRP),
		gtp_drv_irq_show,
		gtp_drv_irq_store);

static ssize_t gtp_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	if ('1' != buf[0]) {
		dev_err(dev, "Invalid argument for reset\n");
		return -EINVAL;
	}

	mutex_lock(&data->input_dev->mutex);
	if (data->irq_enabled) {
		gtp_irq_control_enable(data, false);
		data->irq_enabled = false;
	}
	gtp_reset_guitar(data->client, 20);
	msleep(GTP_100_DLY_MS);
	gtp_irq_control_enable(data, true);
	data->irq_enabled  = true;
	mutex_unlock(&data->input_dev->mutex);

	return count;
}

static DEVICE_ATTR(reset, (S_IWUSR | S_IWGRP),
		NULL,
		gtp_reset_store);

static ssize_t gtp_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = i2c_get_clientdata(to_i2c_client(dev));

	switch (gpio_get_value(data->pdata->irq_gpio)) {
	case 0:
		return scnprintf(buf, PAGE_SIZE, "Low\n");
	case 1:
		return scnprintf(buf, PAGE_SIZE, "High\n");
	default:
		dev_err(dev, "Failed to get GPIO for irq %d\n",
				data->client->irq);
		return scnprintf(buf, PAGE_SIZE, "Unknown\n");
	}
}

static DEVICE_ATTR(hw_irqstat, S_IRUGO,
		gtp_hw_irqstat_show,
		NULL);

/*******************************************************
* Function:
*	Read chip version.
* Input:
*	client: i2c device
*	version: buffer to keep ic firmware version
* Output:
*	read operation return.
*		2: succeed, otherwise: failed
 *******************************************************/
s32 gtp_read_version(struct i2c_client *client, u16 *version, u8 *product_id)
{
	s32 ret = -1;
	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

	GTP_DEBUG_FUNC();

	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "GTP read version failed");
		return ret;
	}

	if (version)
		*version = (buf[7] << 8) | buf[6];

	/* product id */
	memcpy(product_id, buf+GTP_ADDR_LENGTH, 4);

	if (buf[5] == 0x00) {
		dev_info(&client->dev, "IC Version: %c%c%c_%02x%02x",
				buf[2], buf[3], buf[4], buf[7], buf[6]);
	} else {
		dev_info(&client->dev, "IC Version: %c%c%c%c_%02x%02x",
				buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
	}

	return ret;
}

/*******************************************************
* Function:
*	I2c test Function.
* Input:
*	client: i2c client.
* Output:
*	Executive outcomes.
*		2: succeed, otherwise failed.
 *******************************************************/
static s8 gtp_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

	while (retry++ < 5) {
		ret = gtp_i2c_read(client, test, 3);
		if (ret > 0)
			return ret;

		dev_err(&client->dev, "GTP i2c test failed time %d.", retry);
		usleep_range(10000, 11000); /* 10 ms */
	}

	return ret;
}

/*******************************************************
* Function:
*	Request gpio(INT & RST) ports.
* Input:
*	ts: private data.
* Output:
*	Executive outcomes.
*		>= 0: succeed, < 0: failed
 *******************************************************/
static s8 gtp_request_io_port(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();

	ret = GTP_GPIO_REQUEST(ts->pdata->irq_gpio, "GTP INT IRQ");
	if (ret < 0) {
		dev_err(&ts->client->dev, "Failed to request GPIO:%d, ERRNO:%d",
				(s32)ts->pdata->irq_gpio, ret);
		ret = -ENODEV;
	} else {
		GTP_GPIO_AS_INT(ts->pdata->irq_gpio);
		ts->client->irq = gpio_to_irq(ts->pdata->irq_gpio);
	}

	ret = GTP_GPIO_REQUEST(ts->pdata->rst_gpio, "GTP RST PORT");
	if (ret < 0) {
		dev_err(&ts->client->dev, "Failed to request GPIO:%d, ERRNO:%d",
				(s32)ts->pdata->rst_gpio, ret);
		ret = -ENODEV;
	}

	GTP_GPIO_AS_INPUT(ts->pdata->rst_gpio);

	gtp_reset_guitar(ts->client, 20);

	if (ret < 0) {
		GTP_GPIO_FREE(ts->pdata->rst_gpio);
		GTP_GPIO_FREE(ts->pdata->irq_gpio);
	}

	return ret;
}

/*******************************************************
* Function:
*	Request interrupt.
* Input:
*	ts: private data.
* Output:
*	Executive outcomes.
*		0: succeed, -1: failed.
 *******************************************************/
static s8 gtp_request_irq(struct goodix_ts_data *ts)
{
	s32 ret = -1;

	GTP_DEBUG_FUNC();
	dev_dbg(&ts->client->dev, "INT trigger type:%x", ts->int_trigger_type);

	ret = gtp_irq_control_enable(ts, true);
	if (ret) {
		dev_err(&ts->client->dev, "Request IRQ failed!ERRNO:%d.", ret);
		GTP_GPIO_AS_INPUT(ts->pdata->irq_gpio);
		GTP_GPIO_FREE(ts->pdata->irq_gpio);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		ts->use_irq = false;
		ret = -EPERM;
	} else {
		gtp_irq_control_enable(ts, false);
		ts->use_irq = true;
		ret = 0;
	}

	return ret;
}

/*******************************************************
* Function:
*	Request input device Function.
* Input:
*	ts: private data.
* Output:
*	Executive outcomes.
*		0: succeed, otherwise: failed.
 *******************************************************/
static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	u8 index = 0;

	GTP_DEBUG_FUNC();

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		dev_err(&ts->client->dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY)
		| BIT_MASK(EV_ABS);
	if (ts->pdata->ics_slot_report) {
		/*  in case of "out of memory" */
		input_mt_init_slots(ts->input_dev, 16, 0);
	} else {
		ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] =
						BIT_MASK(BTN_TOUCH);
	}
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	if (ts->pdata->have_touch_key) {
		for (index = 0; index < GTP_MAX_KEY_NUM; index++)
			input_set_capability(ts->input_dev, EV_KEY,
					touch_key_array[index]);
	}

	if (ts->pdata->slide_wakeup)
		input_set_capability(ts->input_dev, EV_KEY, KEY_POWER);

	if (ts->pdata->change_x2y)
		GTP_SWAP(ts->abs_x_max, ts->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,
		ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
		ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_TOOL_TYPE,
		0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 1023, 0, 0);

	ts->input_dev->name = goodix_ts_name;
	ts->input_dev->phys = goodix_input_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;

	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&ts->client->dev, "Register %s input device failed",
				ts->input_dev->name);
		return -ENODEV;
	}

	if (ts->pdata->with_pen)
		gtp_pen_init(ts);

	return 0;
}

/*
 * Devices Tree support
*/
#ifdef CONFIG_OF
/* ASCII names order MUST match enum */
static const char * const ascii_names[] = {"charger", "na"};

static int goodix_modifier_name2id(const char *name)
{
	int i, len2cmp, chosen = -1;

	for (i = 0; i < GD_MOD_MAX; i++) {
		len2cmp = min_t(int, strlen(name), strlen(ascii_names[i]));
		if (!strncmp(name, ascii_names[i], len2cmp)) {
			chosen = i;
			break;
		}
	}
	return chosen;
}

static void goodix_dt_parse_modifier(struct goodix_ts_data *data,
		struct device_node *parent, struct config_modifier *config,
		const char *modifier_name, bool active)
{
	struct device *dev = &data->client->dev;
	char node_name[64];
	struct device_node *np_config;

	scnprintf(node_name, 63, "%s-%s", modifier_name,
		active ? "active" : "suspended");
	np_config = of_find_node_by_name(parent, node_name);
	if (!np_config) {
		dev_dbg(dev, "%s: node does not exist\n", node_name);
		return;
	}

	of_node_put(np_config);
}

static int goodix_dt_parse_modifiers(struct goodix_ts_data *data)
{
	struct device *dev = &data->client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_mod;
	int i, num_names, ret = 0;
	char node_name[64];
	static const char **modifiers_names;

	sema_init(&data->modifiers.list_sema, 1);
	INIT_LIST_HEAD(&data->modifiers.mod_head);
	data->modifiers.mods_num = 0;

	num_names = of_property_count_strings(np, "config_modifier-names");
	if (num_names < 0) {
		dev_err(dev, "Cannot parse config_modifier-names: %d\n",
			num_names);
		return -ENODEV;
	}

	modifiers_names = devm_kzalloc(dev,
			sizeof(*modifiers_names) * num_names, GFP_KERNEL);
	if (!modifiers_names)
		return -ENOMEM;

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(np, "config_modifier-names",
			i, &modifiers_names[i]);
		if (ret < 0) {
			dev_err(dev, "Cannot parse modifier-names: %d\n", ret);
			return ret;
		}
	}

	data->modifiers.mods_num = num_names;

	for (i = 0; i < num_names; i++) {
		int id;
		struct config_modifier *cm, *config;

		scnprintf(node_name, 63, "config_modifier-%s",
				modifiers_names[i]);
		np_mod = of_find_node_by_name(np, node_name);
		if (!np_mod) {
			dev_warn(dev, "cannot find modifier node %s\n",
				node_name);
			continue;
		}

		/* check for duplicate nodes in devtree */
		id = goodix_modifier_name2id(modifiers_names[i]);
		dev_info(dev, "processing modifier %s[%d]\n", node_name, id);
		list_for_each_entry(cm, &data->modifiers.mod_head, link) {
			if (cm->id == id) {
				dev_err(dev, "duplicate modifier node %s\n",
					node_name);
				return -EFAULT;
			}
		}
		/* allocate modifier's structure */
		config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
		if (!config)
			return -ENOMEM;

		list_add_tail(&config->link, &data->modifiers.mod_head);
		config->name = modifiers_names[i];
		config->id = id;

		if (of_property_read_bool(np_mod, "enable-notification")) {
			switch (id) {
			case GD_MOD_CHARGER:
				pr_notice("using charger detection\n");
				data->charger_detection_enabled = true;
				break;
			default:
				pr_notice("no notification found\n");
				break;
			}
		} else {
			config->effective = true;
			dev_dbg(dev, "modifier %s enabled unconditionally\n",
					node_name);
		}

		dev_dbg(dev, "processing modifier %s[%d]\n",
			node_name, config->id);

		goodix_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], true);
		goodix_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], false);

		of_node_put(np_mod);
	}

	return 0;
}

static int goodix_ts_get_dt_coords(struct device *dev, char *name,
		struct goodix_ts_platform_data *pdata)
{
	struct property *prop;
	struct device_node *np = dev->of_node;
	int rc;
	u32 coords[GOODIX_COORDS_ARR_SIZE];

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	rc = of_property_read_u32_array(np, name, coords,
			GOODIX_COORDS_ARR_SIZE);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "goodix,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "goodix,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

/*******************************************************
* Function:
*	parse platform information form devices tree.
* Input:
*	dev: device that this driver attached.
*	pdata: private plateform data pointer.
* Output:
*	Executive outcomes.
*		0: succeed, otherwise: failed.
 *******************************************************/
static int gtp_parse_dt(struct device *dev,
		struct goodix_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_BUTTONS];

	rc = goodix_ts_get_dt_coords(dev, "goodix,panel-coords", pdata);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Failed to parse goodix,panel-coords.");
		return rc;
	}

	rc = goodix_ts_get_dt_coords(dev, "goodix,display-coords", pdata);
	if (rc) {
		dev_err(dev, "Failed to parse goodix,display-coords.");
		return rc;
	}

	pdata->force_update = of_property_read_bool(np,
			"goodix,force-update");

	pdata->i2c_pull_up = of_property_read_bool(np,
			"goodix,i2c-pull-up");

	pdata->have_touch_key = of_property_read_bool(np,
			"goodix,have-touch-key");

	pdata->driver_send_cfg = of_property_read_bool(np,
			"goodix,driver-send-cfg");

	pdata->change_x2y = of_property_read_bool(np,
			"goodix,change-x2y");

	pdata->with_pen = of_property_read_bool(np,
			"goodix,with-pen");

	pdata->slide_wakeup = of_property_read_bool(np,
			"goodix,slide-wakeup");

	pdata->dbl_clk_wakeup = of_property_read_bool(np,
			"goodix,dbl-clk-wakeup");

	pdata->auto_update = of_property_read_bool(np,
			"goodix,auto-update");

	pdata->auto_update_cfg = of_property_read_bool(np,
			"goodix,auto-update-cfg");

	pdata->esd_protect = of_property_read_bool(np,
			"goodix,esd-protect");

	pdata->wakeup_with_reset = of_property_read_bool(np,
			"goodix,wakeup-with-reset");

	pdata->ics_slot_report = of_property_read_bool(np,
			"goodix,ics-slot-report");

	pdata->create_wr_node = of_property_read_bool(np,
			"goodix,create-wr-node");

	pdata->resume_in_workqueue = of_property_read_bool(np,
			"goodix,resume-in-workqueue");

	pdata->coordinate_scale = of_property_read_bool(np,
			"goodix,coordinate-scale");

	/* reset, irq gpio info */
	pdata->irq_gpio = of_get_named_gpio_flags(np, "goodix,irq-gpio",
			0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0) {
		dev_err(dev, "Failed to parse goodix,irq-gpio.");
		return pdata->irq_gpio;
	}

	pdata->rst_gpio = of_get_named_gpio_flags(np, "goodix,rst-gpio",
			0, &pdata->rst_gpio_flags);
	if (pdata->rst_gpio < 0) {
		dev_err(dev, "Failed to parse goodix,rst-gpio.");
		return pdata->rst_gpio;
	}

	prop = of_find_property(np, "goodix,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
				"goodix,button-map", button_map,
				num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
		pdata->num_button = num_buttons;
		memcpy(pdata->button_map, button_map,
			pdata->num_button * sizeof(u32));
	}

	return 0;
}

/*******************************************************
* Function:
*	parse config data from devices tree.
* Input:
*	dev: device that this driver attached.
*	cfg: pointer of the config array.
*	cfg_len: pointer of the config length.
*	sid: sensor id.
* Output:
*	Executive outcomes.
*		0-succeed, -1-faileds.
 *******************************************************/
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	char cfg_name[18];
	int ret;

	snprintf(cfg_name, sizeof(cfg_name), "goodix,cfg-group%d", sid);
	prop = of_find_property(np, cfg_name, cfg_len);
	if (!prop || !prop->value || *cfg_len == 0 ||
		*cfg_len > GTP_CONFIG_MAX_LENGTH) {
		ret = -EPERM;/* failed */
	} else {
		memcpy(cfg, prop->value, *cfg_len);
		ret = 0;
	}

	return ret;
}

#endif

/*******************************************************
* Function:
*	Turn device power ON.
* Input:
*	ts: driver private data.
* Output:
*	Returns zero on success, else an error.
 *******************************************************/
static int goodix_power_on(struct goodix_ts_data *ts)
{
	int ret;

	if (ts->power_on) {
		dev_info(&ts->client->dev,
				"Device already power on\n");
		return 0;
	}

	if (!IS_ERR(ts->vdd_ana)) {
		ret = regulator_set_voltage(ts->vdd_ana, GOODIX_VTG_MIN_UV,
					   GOODIX_VTG_MAX_UV);
		if (ret) {
			dev_err(&ts->client->dev,
					"Regulator set_vtg failed vdd ret=%d\n",
					ret);
			goto err_set_vtg_vdd;
		}

		ret = regulator_enable(ts->vdd_ana);
		if (ret) {
			dev_err(&ts->client->dev,
					"Regulator vdd enable failed ret=%d\n",
					ret);
			goto err_enable_vdd;
		}
	}

	if (!IS_ERR(ts->vcc_i2c)) {
		ret = regulator_set_voltage(ts->vcc_i2c, GOODIX_I2C_VTG_MIN_UV,
					   GOODIX_I2C_VTG_MAX_UV);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator set_vtg failed vcc_i2c ret=%d\n",
				ret);
			goto err_set_vtg_vcc_i2c;
		}

		ret = regulator_enable(ts->vcc_i2c);
		if (ret) {
			dev_err(&ts->client->dev,
					"Regulator vcc_i2c enable failed ret=%d\n",
					ret);
			regulator_disable(ts->vdd_ana);
			goto err_enable_vcc_i2c;
			}
	}

	ts->power_on = true;
	return 0;

err_enable_vcc_i2c:
	if (!IS_ERR(ts->vcc_i2c))
		regulator_set_voltage(ts->vcc_i2c, 0, GOODIX_I2C_VTG_MAX_UV);
err_set_vtg_vcc_i2c:
	if (!IS_ERR(ts->vdd_ana))
		regulator_disable(ts->vdd_ana);
err_enable_vdd:
	if (!IS_ERR(ts->vdd_ana))
		regulator_set_voltage(ts->vdd_ana, 0, GOODIX_VTG_MAX_UV);
err_set_vtg_vdd:
	ts->power_on = false;

	return ret;
}

/*******************************************************
* Function:
*	Turn device power OFF.
* Input:
*	ts: driver private data.
* Output:
*	Returns zero on success, else an error.
 *******************************************************/
static int goodix_power_off(struct goodix_ts_data *ts)
{
	int ret;

	if (!ts->power_on) {
		dev_info(&ts->client->dev,
				"Device already power off\n");
		return 0;
	}

	if (!IS_ERR(ts->vcc_i2c)) {
		ret = regulator_set_voltage(ts->vcc_i2c, 0,
			GOODIX_I2C_VTG_MAX_UV);
		if (ret < 0)
			dev_err(&ts->client->dev,
				"Regulator vcc_i2c set_vtg failed ret=%d\n",
				ret);
		ret = regulator_disable(ts->vcc_i2c);
		if (ret)
			dev_err(&ts->client->dev,
				"Regulator vcc_i2c disable failed ret=%d\n",
				ret);
	}

	if (!IS_ERR(ts->vdd_ana)) {
		ret = regulator_set_voltage(ts->vdd_ana, 0, GOODIX_VTG_MAX_UV);
		if (ret < 0)
			dev_err(&ts->client->dev,
					"Regulator vdd set_vtg failed ret=%d\n",
					ret);
		ret = regulator_disable(ts->vdd_ana);
		if (ret)
			dev_err(&ts->client->dev,
					"Regulator vdd disable failed ret=%d\n",
					ret);
	}

	ts->power_on = false;

	return 0;
}

/*******************************************************
* Function:
*	Initialize device power.
* Input:
*	ts: driver private data.
* Output:
*	Returns zero on success, else an error.
 *******************************************************/
static int goodix_power_init(struct goodix_ts_data *ts)
{
	int ret;

	ts->vdd_ana = regulator_get(&ts->client->dev, "vdd_ana");
	if (IS_ERR(ts->vdd_ana)) {
		ret = PTR_ERR(ts->vdd_ana);
		dev_info(&ts->client->dev,
				"Regulator get failed vdd ret=%d\n", ret);
	}

	ts->vcc_i2c = regulator_get_optional(&ts->client->dev, "vcc_i2c");
	if (IS_ERR(ts->vcc_i2c)) {
		ret = PTR_ERR(ts->vcc_i2c);
		dev_info(&ts->client->dev,
				"Regulator get failed vcc_i2c ret=%d\n", ret);
	}

	return 0;
}

/*******************************************************
* Function:
*	Deinitialize device power.
* Input:
*	ts: driver private data.
* Output:
*	Returns zero on success, else an error.
 *******************************************************/
static int goodix_power_deinit(struct goodix_ts_data *ts)
{
	regulator_put(ts->vdd_ana);
	regulator_put(ts->vcc_i2c);

	return 0;
}

/*******************************************************
* Function:
*	Touch screen shutdown.
* Input:
*	client: i2c client.
* Output:
*	None.
 *******************************************************/
void goodix_ts_shutdown(struct i2c_client *client)
{
	struct goodix_ts_data *data = i2c_get_clientdata(client);
	int ret;

	if (!data->init_done)
		return;

	gtp_irq_control_enable(data, false);

	GTP_GPIO_OUTPUT(data->pdata->rst_gpio, 0);
	msleep(GTP_20_DLY_MS);
	GTP_GPIO_FREE(data->pdata->rst_gpio);

	if (data->use_irq)
		GTP_GPIO_FREE(data->pdata->irq_gpio);

	ret = goodix_power_off(data);
	if (ret) {
		dev_err(&client->dev, "GTP power off failed.");
		return;
	}
}

/*******************************************************
* Function:
*	I2c probe.
* Input:
*	client: i2c device struct.
*	id: device id.
* Output:
*	Executive outcomes.
*		0: succeed.
*******************************************************/
static int goodix_ts_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	s32 ret = -1;
	struct goodix_ts_data *ts;
	struct goodix_ts_platform_data *pdata;

	GTP_DEBUG_FUNC();

	/* do NOT remove these logs */
	dev_info(&client->dev,
			"GTP Driver Version: %s", GTP_DRIVER_VERSION);
	dev_info(&client->dev,
			"GTP I2C Address: 0x%02x", client->addr);

	pdata = devm_kzalloc(&client->dev,
			sizeof(struct goodix_ts_platform_data), GFP_KERNEL);

	if (!pdata) {
		dev_err(&client->dev, "GTP invalid pdata\n");
		return -EINVAL;
	}

	i2c_connect_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

	ts->init_done = false;

#ifdef CONFIG_OF	/* device tree support */
	if (client->dev.of_node) {
		ret = gtp_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "parse dts failed!");
			return ret;
		}
	}
#else			/* use gpio defined in gt9xx.h */
	pdata->rst_gpio = GTP_RST_PORT;
	pdata->irq_gpio = GTP_INT_PORT;
	pdata->have_touch_key = true;
	pdata->with_pen = true;
	pdata->slide_wakeup = true;
	pdata->force_update = false;
	pdata->auto_update = true;
	pdata->auto_update_cfg = false;
	pdata->ics_slot_report = false;
	pdata->create_wr_node = true;
	pdata->wakeup_with_reset = true;
	pdata->esd_protect = false;
#endif

	ts->client = client;
	ts->pdata = pdata;
	spin_lock_init(&ts->irq_lock);		/*  2.6.39 later */
	/*  ts->irq_lock = SPIN_LOCK_UNLOCKED;	// 2.6.39 & before */
	if (pdata->esd_protect) {
		/*  HZ: clock ticks in 1 second generated by system */
		ts->clk_tick_cnt = 2 * HZ;
		dev_dbg(&ts->client->dev,
				"Clock ticks for an esd cycle: %d",
				ts->clk_tick_cnt);
		spin_lock_init(&ts->esd_lock);
		/*  ts->esd_lock = SPIN_LOCK_UNLOCKED; */
	}
	i2c_set_clientdata(client, ts);
	ts->gtp_rawdiff_mode = 0;
	ts->irq_enabled = false;
	ts->gtp_suspended = true;
	ts->power_on = false;
	ts->patching_enabled = true;

	ret = goodix_dt_parse_modifiers(ts);
	if (ret)
		ts->patching_enabled = false;

	ret = goodix_power_init(ts);
	if (ret) {
		dev_err(&client->dev, "GTP power init failed.");
		ret = -EINVAL;
		goto exit_free_client_data;
	}

	ret = goodix_power_on(ts);
	if (ret) {
		dev_err(&client->dev, "GTP power on failed");
		ret = -EINVAL;
		goto exit_deinit_power;
	}

	ret = gtp_request_io_port(ts);
	if (ret < 0) {
		dev_err(&client->dev, "GTP request IO port failed.");
		goto exit_power_off;
	}

	ret = gtp_i2c_test(client);
	if (ret < 0) {
		dev_err(&client->dev, "I2C communication ERROR!");
		goto exit_free_io_port;
	}

	dev_info(&client->dev, "I2C Addr is %x", client->addr);

	if (pdata->force_update)
		ts->force_update = true;

	ret = gtp_read_version(client, &ts->version_info, ts->product_id);
	if (ret < 0) {
		dev_err(&client->dev, "Read version failed.");
		goto exit_free_io_port;
	}

	ts->pdata->config[0] = GTP_REG_CONFIG_DATA >> 8;
	ts->pdata->config[1] = GTP_REG_CONFIG_DATA & 0xff;
	ret = gtp_init_panel(ts);
	if (ret < 0) {
		dev_err(&client->dev, "GTP init panel failed.");
		ts->abs_x_max = GTP_MAX_WIDTH;
		ts->abs_y_max = GTP_MAX_HEIGHT;
		ts->int_trigger_type = GTP_INT_TRIGGER;
	}

	if (pdata->esd_protect)
		gtp_esd_switch(client, SWITCH_ON);

	if (ts->pdata->auto_update) {
		ret = gup_init_update_proc(ts);
		if (ret < 0)
			dev_err(&client->dev, "Create update thread error.");
	}

	ret = gtp_request_input_dev(ts);
	if (ret < 0) {
		dev_err(&client->dev, "GTP request input dev failed");
		goto exit_free_io_port;
	}

	mutex_init(&ts->lock);

	ret = gtp_request_irq(ts);
	if (ret < 0)
		dev_info(&client->dev, "GTP works in polling mode.");
	else
		dev_info(&client->dev, "GTP works in interrupt mode.");

	if (ts->use_irq) {
		gtp_irq_control_enable(ts, true);
		if (ts->pdata->slide_wakeup)
			enable_irq_wake(client->irq);
	}

	/* register suspend and resume function */
#if defined(CONFIG_FB)
	INIT_WORK(&ts->fb_notify_work, fb_notify_resume_work);
	ts->notifier.notifier_call = gtp_fb_notifier_callback;
	ret = fb_register_client(&ts->notifier);
	if (ret) {
		dev_err(&client->dev,
				"Unable to register fb_notifier: %d\n", ret);
		goto exit_unreg_input_dev;
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	/* register charger function */
	if (ts->charger_detection_enabled) {
		ret = gtp_register_ps_notifier(ts);
		if (ret)
			goto exit_fb_notifier;
	}

	/*  Create proc file system */
	gt91xx_config_proc = NULL;
	gt91xx_config_proc = proc_create(
		GT91XX_CONFIG_PROC_FILE, 0664, NULL, &config_proc_ops);
	if (gt91xx_config_proc == NULL) {
		dev_err(&ts->client->dev, "create_proc_entry %s failed\n",
				GT91XX_CONFIG_PROC_FILE);
		goto exit_free_ps_notifier;
	} else
		dev_info(&client->dev, "create proc entry %s success",
				GT91XX_CONFIG_PROC_FILE);

	/*  Create sys file system */
	ret  = goodix_ts_sysfs_class(ts, true);
	if (ret < 0) {
		dev_err(&client->dev, "create sys class failed\n");
		goto exit_free_config_proc;
	} else {
		dev_info(&client->dev, "create sys class success\n");
	}

	/* poweron file */
	ret = device_create_file(&client->dev, &dev_attr_poweron);
	if (ret < 0) {
		dev_err(&client->dev, "create sys poweron failed\n");
		goto exit_free_sys_class;
	} else {
		dev_dbg(&client->dev, "create sys poweron success\n");
	}

	/* doreflash file */
	ret = device_create_file(&client->dev, &dev_attr_doreflash);
	if (ret < 0) {
		dev_err(&client->dev, "create sys doreflash failed\n");
		goto exit_free_poweron;
	} else {
		dev_dbg(&client->dev, "create sys doreflash success\n");
	}

	/* flashprog file */
	ret = device_create_file(&client->dev, &dev_attr_flashprog);
	if (ret < 0) {
		dev_err(&client->dev, "create sys flashprog failed\n");
		goto exit_free_doreflash;
	} else {
		dev_dbg(&client->dev, "create sys flashprog success\n");
	}

	/* productinfo file */
	ret = device_create_file(&client->dev, &dev_attr_productinfo);
	if (ret < 0) {
		dev_err(&client->dev, "create sys productinfo failed\n");
		goto exit_free_flashprog;
	} else {
		dev_dbg(&client->dev, "create sys productinfo success\n");
	}

	/* buildid file */
	ret = device_create_file(&client->dev, &dev_attr_buildid);
	if (ret < 0) {
		dev_err(&client->dev, "create sys buildid failed\n");
		goto exit_free_productinfo;
	} else {
		dev_dbg(&client->dev, "create sys buildid success\n");
	}

	/* forcereflash file */
	ret = device_create_file(&client->dev, &dev_attr_forcereflash);
	if (ret < 0) {
		dev_err(&client->dev, "create sys forcereflash failed\n");
		goto exit_free_buildid;
	} else {
		dev_dbg(&client->dev, "create sys forcereflash success\n");
	}

	/* drv_irq file */
	ret = device_create_file(&client->dev, &dev_attr_drv_irq);
	if (ret < 0) {
		dev_err(&client->dev, "create sys drv_irq failed\n");
		goto exit_free_forcereflash;
	} else {
		dev_dbg(&client->dev, "create sys drv_irq success\n");
	}

	/* reset file */
	ret = device_create_file(&client->dev, &dev_attr_reset);
	if (ret < 0) {
		dev_err(&client->dev, "create sys reset failed\n");
		goto exit_free_drv_irq;
	} else {
		dev_dbg(&client->dev, "create sys reset success\n");
	}

	/* hw_irqstat file */
	ret = device_create_file(&client->dev, &dev_attr_hw_irqstat);
	if (ret < 0) {
		dev_err(&client->dev, "create sys hw_irqstat failed\n");
		goto exit_free_reset;
	} else {
		dev_dbg(&client->dev, "create sys hw_irqstat  success\n");
	}

	if (pdata->create_wr_node)
		init_wr_node(client);
	/* probe init finished */
	ts->init_done = true;
	return 0;

exit_free_reset:
	device_remove_file(&client->dev, &dev_attr_reset);
exit_free_drv_irq:
	device_remove_file(&client->dev, &dev_attr_drv_irq);
exit_free_forcereflash:
	device_remove_file(&client->dev, &dev_attr_forcereflash);
exit_free_buildid:
	device_remove_file(&client->dev, &dev_attr_buildid);
exit_free_productinfo:
	device_remove_file(&client->dev, &dev_attr_productinfo);
exit_free_flashprog:
	device_remove_file(&client->dev, &dev_attr_flashprog);
exit_free_doreflash:
	device_remove_file(&client->dev, &dev_attr_doreflash);
exit_free_poweron:
	device_remove_file(&client->dev, &dev_attr_poweron);
exit_free_sys_class:
	goodix_ts_sysfs_class(ts, false);
exit_free_config_proc:
	remove_proc_entry(GT91XX_CONFIG_PROC_FILE, gt91xx_config_proc);
exit_free_ps_notifier:
	gtp_unregister_powermanger(ts);
exit_fb_notifier:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts->notifier))
		dev_err(&client->dev,
				"Error occurred while unregistering fb_notifer.\n");
#endif
exit_unreg_input_dev:
	input_unregister_device(ts->input_dev);
exit_free_io_port:
	if (gpio_is_valid(ts->pdata->rst_gpio))
		gpio_free(ts->pdata->rst_gpio);
	if (gpio_is_valid(ts->pdata->irq_gpio))
		gpio_free(ts->pdata->irq_gpio);
exit_power_off:
	goodix_power_off(ts);
exit_deinit_power:
	goodix_power_deinit(ts);
exit_free_client_data:
	kfree(ts);
	i2c_set_clientdata(client, NULL);

	return ret;
}


/*******************************************************
* Function:
*	Goodix touchscreen driver release function.
* Input:
*	client: i2c device struct.
* Output:
*	Executive outcomes. 0---succeed.
*******************************************************/
static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

	gtp_unregister_powermanger(ts);

	if (ts->charger_detection_enabled)
		power_supply_unreg_notifier(&ts->ps_notif);
#if defined(CONFIG_FB)
	if (fb_unregister_client(&ts->notifier))
		dev_err(&client->dev,
				"Error occurred while unregistering fb_notifer.\n");
#endif

	remove_proc_entry(GT91XX_CONFIG_PROC_FILE, gt91xx_config_proc);
	goodix_ts_sysfs_class(ts, false);
	device_remove_file(&client->dev, &dev_attr_poweron);
	device_remove_file(&client->dev, &dev_attr_productinfo);
	device_remove_file(&client->dev, &dev_attr_forcereflash);
	device_remove_file(&client->dev, &dev_attr_flashprog);
	device_remove_file(&client->dev, &dev_attr_doreflash);
	device_remove_file(&client->dev, &dev_attr_buildid);
	device_remove_file(&client->dev, &dev_attr_drv_irq);
	device_remove_file(&client->dev, &dev_attr_reset);

	mutex_destroy(&ts->lock);

	if (ts->pdata->create_wr_node)
		uninit_wr_node();

	if (ts->pdata->esd_protect)
		destroy_workqueue(gtp_esd_check_workqueue);

	if (ts) {
		if (ts->use_irq) {
			GTP_GPIO_AS_INPUT(ts->pdata->irq_gpio);
			GTP_GPIO_FREE(ts->pdata->irq_gpio);
			free_irq(client->irq, ts);
		} else {
			hrtimer_cancel(&ts->timer);
		}
	}

	if (gpio_is_valid(ts->pdata->rst_gpio))
		gpio_free(ts->pdata->rst_gpio);

	if (gpio_is_valid(ts->pdata->irq_gpio))
		gpio_free(ts->pdata->irq_gpio);

	goodix_power_off(ts);
	goodix_power_deinit(ts);

	dev_info(&client->dev, "GTP driver removing...");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

/*******************************************************
* Function:
*	Release all finger.
* Input:
*	ts: driver private data.
* Output:
*	None.
*******************************************************/
static void gtp_release_all_finger(struct goodix_ts_data *ts)
{
	int i;

	for (i = 0; i < GTP_MAX_TOUCH; i++)
		gtp_touch_up(ts, i);
	input_sync(ts->input_dev);
}

/*******************************************************
* Function:
*	Suspend function.
* Input:
*	ts: driver private data.
* Output:
*	None.
*******************************************************/
static void goodix_ts_suspend(struct goodix_ts_data *ts)
{
	s8 ret = -1;

	GTP_DEBUG_FUNC();

	if (ts->fw_loading) {
		dev_info(&ts->client->dev,
				"Fw upgrade in progress, can't go to suspend.");
		return;
	}

	if (ts->enter_update)
		return;

	if (ts->gtp_suspended) {
		dev_info(&ts->client->dev, "Already in suspend state\n");
		return;
	}

	dev_info(&ts->client->dev, "Goodix touch suspend.");
	mutex_lock(&ts->lock);

	if (ts->pdata->esd_protect)
		gtp_esd_switch(ts->client, SWITCH_OFF);

	if (ts->pdata->slide_wakeup)
		ret = gtp_enter_doze(ts);
	else {
		if (ts->use_irq)
			gtp_irq_control_enable(ts, false);
		else
			hrtimer_cancel(&ts->timer);

		/* release the finger touch */
		gtp_release_all_finger(ts);

		ret = gtp_enter_sleep(ts);
	}

	if (ret < 0)
		dev_err(&ts->client->dev, "GTP early suspend failed.");
	/*  to avoid waking up while not sleeping */
	/*	delay 48 + 10ms to ensure reliability */
	msleep(GTP_58_DLY_MS);

	ts->gtp_suspended = true;
	mutex_unlock(&ts->lock);
}

/*******************************************************
* Function:
*	Resume function.
* Input:
*	ts: driver private data.
* Output:
*	None.
* *****************************************************/
static void goodix_ts_resume(struct goodix_ts_data *ts)
{
	s8 ret = -1;

	GTP_DEBUG_FUNC();
	if (ts->enter_update)
		return;

	if (!ts->gtp_suspended) {
		dev_dbg(&ts->client->dev, "Already in awake state\n");
		return;
	}

	dev_info(&ts->client->dev, "Goodix touch resume.");
	mutex_lock(&ts->lock);

	if (ts->irq_enabled)
		gtp_irq_control_enable(ts, false);

	ret = gtp_wakeup_sleep(ts);
	if (ret < 0)
		dev_err(&ts->client->dev, "GTP later resume failed.");
	{
		gtp_send_cfg(ts->client);
	}

	if (ts->pdata->slide_wakeup)
		doze_status = DOZE_DISABLED;

	if (ts->use_irq)
		gtp_irq_control_enable(ts, true);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	mutex_unlock(&ts->lock);
	ts->gtp_suspended = false;

	if (ts->charger_detection_enabled)
		goodix_resume_ps_chg_cm_state(ts);

	if (ts->pdata->esd_protect)
		gtp_esd_switch(ts->client, SWITCH_ON);
}

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct goodix_ts_data *ts =
		container_of(work, struct goodix_ts_data, fb_notify_work);
	goodix_ts_resume(ts);
}

/* frame buffer notifier block control the suspend/resume procedure */
static int gtp_fb_notifier_callback(struct notifier_block *noti,
		unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct goodix_ts_data *ts = container_of(noti,
			struct goodix_ts_data, notifier);
	int *blank;

	if ((event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK) &&
			ev_data && ev_data->data && ts) {
		blank = ev_data->data;
		dev_dbg(&ts->client->dev, "fb notification: event = %lu blank = %d\n",
				event, *blank);
		if (event == FB_EARLY_EVENT_BLANK) {
			if (*blank != FB_BLANK_POWERDOWN)
				return 0;
			dev_dbg(&ts->client->dev, "ts_suspend");
			if (ts->pdata->resume_in_workqueue)
				flush_work(&ts->fb_notify_work);
			goodix_ts_suspend(ts);
		} else if (*blank == FB_BLANK_UNBLANK ||
				(*blank == FB_BLANK_NORMAL &&
				 ts->gtp_suspended)) {
			dev_dbg(&ts->client->dev, "ts_resume");
			if (ts->pdata->resume_in_workqueue)
				schedule_work(&ts->fb_notify_work);
			else
				goodix_ts_resume(ts);
		}
	}

	return 0;
}
#elif defined(CONFIG_PM)
/* bus control the suspend/resume procedure */
static int gtp_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		dev_dbg(&ts->client->dev, "Suspend by i2c pm.");
		goodix_ts_suspend(ts);
	}

	return 0;
}
static int gtp_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		dev_dbg(&ts->client->dev, "Resume by i2c pm.");
		goodix_ts_resume(ts);
	}

	return 0;
}

static const struct dev_pm_ops gtp_pm_ops = {
	.suspend = gtp_pm_suspend,
	.resume  = gtp_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void gtp_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h,
			struct goodix_ts_data, early_suspend);

	if (ts) {
		dev_dbg(&ts->client->dev, "Suspend by earlysuspend module.");
		goodix_ts_suspend(ts);
	}
}
static void gtp_late_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h,
			struct goodix_ts_data, early_suspend);

	if (ts) {
		dev_dbg(&ts->client->dev, "Resume by earlysuspend module.");
		goodix_ts_resume(ts);
	}
}
#endif

static int gtp_unregister_powermanger(struct goodix_ts_data *ts)
{
#if defined(CONFIG_FB)
		fb_unregister_client(&ts->notifier);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts->early_suspend);
#endif

	return 0;
}

static int goodix_set_charger_state(struct goodix_ts_data *data,
					bool enable)
{
	int retval;
	struct device *dev = &data->client->dev;
	u8 enter_charger_cmd[3] = {GTP_REG_SLEEP >> 8,
			GTP_REG_SLEEP & 0xFF, 6};
	u8 exit_charger_cmd[3] = {GTP_REG_SLEEP >> 8,
			GTP_REG_SLEEP & 0xFF, 7};

	/* disable IRQ to set CHARGER_STATE reg */
	disable_irq(data->client->irq);
	if (enable) {
		retval = gtp_i2c_write(data->client,
			enter_charger_cmd, 3);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "set chg state\n");
	} else {
		retval = gtp_i2c_write(data->client,
			exit_charger_cmd, 3);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "unset chg state\n");
	}
	enable_irq(data->client->irq);

	return (retval > 0) ? 0 : retval;
}

static int ps_get_state(struct goodix_ts_data *data,
			struct power_supply *psy, bool *present)
{
	struct device *dev = &data->client->dev;
	union power_supply_propval pval = {0};
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&pval);
	if (retval) {
		dev_err(dev, "%s psy get property failed\n", psy->desc->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	dev_dbg(dev, "%s is %s\n", psy->desc->name,
			(*present) ? "present" : "not present");
	return 0;
}

static void goodix_update_ps_chg_cm_state(
				struct goodix_ts_data *data, bool present)
{
	struct config_modifier *cm = NULL;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == GD_MOD_CHARGER) {
			cm->effective = present;
			break;
		}
	}
	up(&data->modifiers.list_sema);
}

void goodix_resume_ps_chg_cm_state(struct goodix_ts_data *data)
{
	struct config_modifier *cm = NULL;
	bool is_active = false;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == GD_MOD_CHARGER) {
			is_active = cm->effective;
			break;
		}
	}
	up(&data->modifiers.list_sema);

	goodix_set_charger_state(data, is_active);
}

static void ps_notify_callback_work(struct work_struct *work)
{
	struct goodix_ts_data *goodix_data =
		container_of(work, struct goodix_ts_data, ps_notify_work);
	bool present = goodix_data->ps_is_present;
	struct device *dev = &goodix_data->client->dev;
	int retval;

	/* enable IC if it in suspend state */
	if (goodix_data->gtp_suspended || goodix_data->enter_update) {
		dev_dbg(dev, "charger resumes tp ic\n");
		goto update_cm_state;
	}

	retval = goodix_set_charger_state(goodix_data, present);
	if (retval) {
		dev_err(dev, "set charger state failed rc=%d\n", retval);
		return;
	}

update_cm_state:
	goodix_update_ps_chg_cm_state(goodix_data, present);
}

static int ps_notify_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct goodix_ts_data *goodix_data =
		container_of(self, struct goodix_ts_data, ps_notif);
	struct power_supply *psy = data;
	struct device *dev = NULL;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED ||
		event == PSY_EVENT_PROP_CHANGED)
		&& psy && psy->desc->get_property &&
		psy->desc->name &&
		!strncmp(psy->desc->name, "usb", sizeof("usb"))
		&& goodix_data) {
		dev = &goodix_data->client->dev;
		dev_dbg(dev, "ps notification: event = %lu\n", event);
		retval = ps_get_state(goodix_data, psy, &present);
		if (retval) {
			dev_err(dev, "psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (goodix_data->ps_is_present == present) {
				dev_info(dev, "ps present state not change\n");
				return 0;
			}
		}
		goodix_data->ps_is_present = present;
		schedule_work(&goodix_data->ps_notify_work);
	}

	return 0;
}

static int gtp_register_ps_notifier(struct goodix_ts_data *ts)
{
	struct power_supply *psy = NULL;
	int err = 0;

	INIT_WORK(&ts->ps_notify_work, ps_notify_callback_work);
	ts->ps_notif.notifier_call = ps_notify_callback;
	err = power_supply_reg_notifier(&ts->ps_notif);
	if (err) {
		dev_err(&ts->client->dev,
				"Unable to register ps_notifier: %d\n", err);
		return err;
	}

	/* if power supply supplier registered brfore TP */
	/* ps_notify_callback will not receive PSY_EVENT_PROP_ADDED */
	/* event, and will cause miss to set TP into charger state. */
	/* So check PS state in probe */
	psy = power_supply_get_by_name("usb");
	if (psy) {
		err = ps_get_state(ts, psy, &ts->ps_is_present);
		if (err) {
			dev_err(&ts->client->dev,
					"psy get property failed rc=%d\n",
					err);
			return err;
		}
		goodix_update_ps_chg_cm_state(ts,
			ts->ps_is_present);
	}
	return 0;
}

s32 gtp_i2c_read_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];

	while (retries < RETRY_MAX_TIMES) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if (retries >= RETRY_MAX_TIMES)
		dev_err(&client->dev,
			"I2C Read: 0x%04X, %d bytes failed, errcode: %d!",
			(((u16)(buf[0] << 8)) | buf[1]), len-2, ret);

	return ret;
}

s32 gtp_i2c_write_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;
	/* msg.scl_rate = 300 * 1000;	// for Rockchip, etc */

	while (retries < RETRY_MAX_TIMES) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	if (retries >= RETRY_MAX_TIMES)
		dev_err(&client->dev,
			"I2C Write: 0x%04X, %d bytes failed, errcode: %d!",
			(((u16)(buf[0] << 8)) | buf[1]), len-2, ret);

	return ret;
}
/*******************************************************
* Function:
*	switch on & off esd delayed work
* Input:
*	client: i2c device
*	on: SWITCH_ON / SWITCH_OFF
* Output:
*	None.
*********************************************************/
void gtp_esd_switch(struct i2c_client *client, s32 on)
{
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(client);
	spin_lock(&ts->esd_lock);

	if (on == SWITCH_ON)	{ /*  switch on esd */
		if (!ts->esd_running) {
			ts->esd_running = 1;
			spin_unlock(&ts->esd_lock);
			dev_info(&ts->client->dev, "Esd started");
			queue_delayed_work(gtp_esd_check_workqueue,
					&gtp_esd_check_work, ts->clk_tick_cnt);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	} else { /* switch off esd */
		if (ts->esd_running) {
			ts->esd_running = 0;
			spin_unlock(&ts->esd_lock);
			dev_info(&ts->client->dev, "Esd cancelled");
			cancel_delayed_work_sync(&gtp_esd_check_work);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	}
}

/*******************************************************
* Function:
*	Initialize external watchdog for esd protect
* Input:
*	client: i2c device.
* Output:
*	result of i2c write operation.
*		1: succeed, otherwise: failed
*********************************************************/
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
	u8 opr_buffer[3] = {(u8)(GTP_REG_ESD_CHECK >> 8),
		(u8)GTP_REG_ESD_CHECK, (u8)GTP_ESD_CHECK_VALUE};

	dev_dbg(&client->dev, "[Esd]Init external watchdog");

	return gtp_i2c_write_no_rst(client, opr_buffer, 3);
}

/*******************************************************
* Function:
*	Esd protect function.
*	External watchdog added by meta, 2013/03/07
* Input:
*	work: delayed work
* Output:
*	None.
*******************************************************/
static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i;
	s32 ret = -1;
	u8 esd_buf[5] = {(u8)(GTP_REG_COMMAND >> 8),
		(u8)GTP_REG_COMMAND};
	struct goodix_ts_data *ts = NULL;

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);

	if (ts->gtp_suspended || ts->enter_update) {
		dev_info(&ts->client->dev, "Esd suspended!");
		return;
	}

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read_no_rst(ts->client, esd_buf, 4);

		dev_dbg(&ts->client->dev,
				"[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X",
				esd_buf[2], esd_buf[3]);

		if ((ret < 0)) {
			/*  IIC communication problem */
			continue;
		} else {
			if ((esd_buf[2] == (u8)GTP_ESD_CHECK_VALUE)
				|| (esd_buf[3] != (u8)GTP_ESD_CHECK_VALUE)) {
				/*  IC works abnormally.. */
				u8 chk_buf[4] = {(u8)(GTP_REG_COMMAND >> 8),
					(u8)GTP_REG_COMMAND};

				gtp_i2c_read_no_rst(ts->client, chk_buf, 4);

				dev_dbg(&ts->client->dev,
				"[Check]0x8040 = 0x%02X, 0x8041 = 0x%02X",
				chk_buf[2], chk_buf[3]);

				if ((chk_buf[2] == (u8)GTP_ESD_CHECK_VALUE)
				|| (chk_buf[3] != (u8)GTP_ESD_CHECK_VALUE)) {
					i = 3;
					break;
				} else {
					continue;
				}
			} else {
				/*  IC works normally, */
				/*  Write 0x8040 0xAA, feed the dog */
				esd_buf[2] = (u8)GTP_ESD_CHECK_VALUE;
				gtp_i2c_write_no_rst(ts->client, esd_buf, 3);
				break;
			}
		}
	}

	if (i >= 3) {
		dev_err(&ts->client->dev,
				"IC working abnormally! Process reset guitar.");
		esd_buf[0] = 0x42;
		esd_buf[1] = 0x26;
		esd_buf[2] = 0x01;
		esd_buf[3] = 0x01;
		esd_buf[4] = 0x01;
		gtp_i2c_write_no_rst(ts->client, esd_buf, 5);
		msleep(GTP_50_DLY_MS);
#ifdef CONFIG_OF
		goodix_power_off(ts);
		msleep(GTP_20_DLY_MS);
		goodix_power_on(ts);
		msleep(GTP_20_DLY_MS);
#endif
		gtp_reset_guitar(ts->client, 50);
		msleep(GTP_50_DLY_MS);
		gtp_send_cfg(ts->client);
	}

	if (!ts->gtp_suspended)
		queue_delayed_work(gtp_esd_check_workqueue,
		&gtp_esd_check_work, ts->clk_tick_cnt);
	else
		dev_info(&ts->client->dev, "Esd suspended!");
}

#ifdef CONFIG_OF
static const struct of_device_id goodix_match_table[] = {
	{.compatible = "goodix,gt9xx-mmi",},
	{ },
};
#endif

static const struct i2c_device_id goodix_ts_id[] = {
	{ GTP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe		= goodix_ts_probe,
	.remove		= goodix_ts_remove,
	.id_table	= goodix_ts_id,
	.shutdown	= goodix_ts_shutdown,
	.driver = {
		.name	  = GTP_I2C_NAME,
		.owner	  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = goodix_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		  = &gtp_pm_ops,
#endif
	},
};

/*******************************************************
* Function:
*	Driver Install function.
* Input:
*	None.
* Output:
*	Executive Outcomes. 0---succeed.
********************************************************/
static int __init goodix_ts_init(void)
{
	s32 ret;

	GTP_DEBUG_FUNC();
	pr_info("GTP driver installing....");

	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
	ret = i2c_add_driver(&goodix_ts_driver);

	return ret;
}

/*******************************************************
* Function:
*	Driver uninstall function.
* Input:
*	None.
* Output:
*	Executive Outcomes. 0---succeed.
********************************************************/
static void __exit goodix_ts_exit(void)
{
	GTP_DEBUG_FUNC();
	pr_info("GTP driver exited.");
	i2c_del_driver(&goodix_ts_driver);
}

module_init(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("GTP MMI Series Driver");
MODULE_LICENSE("GPL v2");

