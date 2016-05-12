/*
 * Copyright (c) 2016, Sharp. All rights reserved.
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

#include "shtps_fts.h"

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(shtps_ctrl_lock);
static DEFINE_MUTEX(shtps_loader_lock);
static DEFINE_MUTEX(shtps_proc_lock);

/* -----------------------------------------------------------------------------------
 */
struct shtps_fts*	gShtps_fts = NULL;

/* -----------------------------------------------------------------------------------
 */
typedef int (shtps_state_func)(struct shtps_fts *ts, int param);
struct shtps_state_func {
	shtps_state_func	*enter;
	shtps_state_func	*start;
	shtps_state_func	*stop;
	shtps_state_func	*sleep;
	shtps_state_func	*wakeup;
	shtps_state_func	*start_ldr;
	shtps_state_func	*interrupt;
};

/* -----------------------------------------------------------------------------------
 */
static int shtpsif_init(void);
static void shtpsif_exit(void);

static void shtps_irq_proc(struct shtps_fts *ts, u8 dummy);
static void shtps_setsleep_proc(struct shtps_fts *ts, u8 sleep);
static void shtps_async_open_proc(struct shtps_fts *ts, u8 dummy);
static void shtps_async_close_proc(struct shtps_fts *ts, u8 dummy);
static void shtps_async_enable_proc(struct shtps_fts *ts, u8 dummy);
static void shtps_suspend_i2c_wake_lock(struct shtps_fts *ts, u8 lock);
static void shtps_exec_suspend_pending_proc(struct shtps_fts *ts);
static void shtps_deter_suspend_i2c_pending_proc_work_function(struct work_struct *work);

static irqreturn_t shtps_irq_handler(int irq, void *dev_id);
static irqreturn_t shtps_irq(int irq, void *dev_id);

static int shtps_init_param(struct shtps_fts *ts);
static void shtps_standby_param(struct shtps_fts *ts);
static void shtps_set_touch_info(struct shtps_fts *ts, u8 *buf, struct shtps_touch_info *info);
static void shtps_calc_notify(struct shtps_fts *ts, u8 *buf, struct shtps_touch_info *info, u8 *event);

static int shtps_irq_resuest(struct shtps_fts *ts);

static int state_change(struct shtps_fts *ts, int state);
static int shtps_statef_nop(struct shtps_fts *ts, int param);
static int shtps_statef_cmn_stop(struct shtps_fts *ts, int param);
static int shtps_statef_idle_start(struct shtps_fts *ts, int param);
static int shtps_statef_idle_wakeup(struct shtps_fts *ts, int param);
static int shtps_statef_idle_start_ldr(struct shtps_fts *ts, int param);
static int shtps_statef_idle_int(struct shtps_fts *ts, int param);
static int shtps_statef_active_enter(struct shtps_fts *ts, int param);
static int shtps_statef_active_stop(struct shtps_fts *ts, int param);
static int shtps_statef_active_sleep(struct shtps_fts *ts, int param);
static int shtps_statef_active_start_ldr(struct shtps_fts *ts, int param);
static int shtps_statef_active_int(struct shtps_fts *ts, int param);
static int shtps_statef_loader_enter(struct shtps_fts *ts, int param);
static int shtps_statef_loader_start(struct shtps_fts *ts, int param);
static int shtps_statef_loader_stop(struct shtps_fts *ts, int param);
static int shtps_statef_loader_int(struct shtps_fts *ts, int param);
static int shtps_statef_sleep_enter(struct shtps_fts *ts, int param);
static int shtps_statef_sleep_wakeup(struct shtps_fts *ts, int param);
static int shtps_statef_sleep_start_ldr(struct shtps_fts *ts, int param);
static int shtps_statef_sleep_int(struct shtps_fts *ts, int param);
static int shtps_fts_open(struct input_dev *dev);
static void shtps_fts_close(struct input_dev *dev);
static int shtps_init_internal_variables(struct shtps_fts *ts);
static void shtps_deinit_internal_variables(struct shtps_fts *ts);
static int shtps_init_inputdev(struct shtps_fts *ts, struct device *ctrl_dev_p, char *modalias);
static void shtps_deinit_inputdev(struct shtps_fts *ts);

/* -----------------------------------------------------------------------------------
 */
const static struct shtps_state_func state_idle = {
    .enter          = shtps_statef_nop,
    .start          = shtps_statef_idle_start,
    .stop           = shtps_statef_nop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_idle_wakeup,
    .start_ldr      = shtps_statef_idle_start_ldr,
    .interrupt      = shtps_statef_idle_int,
};

const static struct shtps_state_func state_active = {
    .enter          = shtps_statef_active_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_active_stop,
    .sleep          = shtps_statef_active_sleep,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_active_start_ldr,
    .interrupt      = shtps_statef_active_int,
};

const static struct shtps_state_func state_loader = {
    .enter          = shtps_statef_loader_enter,
    .start          = shtps_statef_loader_start,
    .stop           = shtps_statef_loader_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_nop,
    .interrupt      = shtps_statef_loader_int,
};

const static struct shtps_state_func state_sleep = {
    .enter          = shtps_statef_sleep_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_cmn_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_sleep_wakeup,
    .start_ldr      = shtps_statef_sleep_start_ldr,
    .interrupt      = shtps_statef_sleep_int,
};

const static struct shtps_state_func *state_func_tbl[] = {
	&state_idle,
	&state_active,
	&state_loader,
	&state_sleep,
};

typedef void (*shtps_deter_suspend_i2c_pending_func_t)(struct shtps_fts*, u8);
static const shtps_deter_suspend_i2c_pending_func_t SHTPS_SUSPEND_PENDING_FUNC_TBL[] = {
	shtps_irq_proc,							/**< SHTPS_DETER_SUSPEND_I2C_PROC_IRQ */
	shtps_setsleep_proc,					/**< SHTPS_DETER_SUSPEND_I2C_PROC_SETSLEEP */
	shtps_async_open_proc,					/**< SHTPS_DETER_SUSPEND_I2C_PROC_OPEN */
	shtps_async_close_proc,					/**< SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE */
	shtps_async_enable_proc,				/**< SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE */
};

/* -----------------------------------------------------------------------------------
 */
int shtps_device_setup(int irq, int rst)
{
	int rc = 0;

	rc = gpio_request(rst, "shtps_rst");
	if (rc) {
		return rc;
	}

	return 0;
}

void shtps_device_teardown(int irq, int rst)
{
	gpio_free(rst);
}

void shtps_device_reset(int rst)
{
	gpio_set_value(rst, 0);
	mb();
	mdelay(SHTPS_HWRESET_TIME_MS);

	gpio_set_value(rst, 1);
	mb();
	mdelay(SHTPS_HWRESET_AFTER_TIME_MS);
}

void shtps_device_poweroff_reset(int rst)
{
	gpio_set_value(rst, 0);
	mb();
	mdelay(SHTPS_HWRESET_TIME_MS);
}

/* -----------------------------------------------------------------------------------
 */
void shtps_mutex_lock_ctrl(void)
{
	mutex_lock(&shtps_ctrl_lock);
}

void shtps_mutex_unlock_ctrl(void)
{
	mutex_unlock(&shtps_ctrl_lock);
}

void shtps_mutex_lock_loader(void)
{
	mutex_lock(&shtps_loader_lock);
}

void shtps_mutex_unlock_loader(void)
{
	mutex_unlock(&shtps_loader_lock);
}

void shtps_mutex_lock_proc(void)
{
	mutex_lock(&shtps_proc_lock);
}

void shtps_mutex_unlock_proc(void)
{
	mutex_unlock(&shtps_proc_lock);
}

/* -----------------------------------------------------------------------------------
 */
void shtps_system_set_sleep(struct shtps_fts *ts)
{
	shtps_device_poweroff_reset(ts->rst_pin);
}

void shtps_system_set_wakeup(struct shtps_fts *ts)
{
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_irq_proc(struct shtps_fts *ts, u8 dummy)
{
	shtps_wake_lock_idle(ts);

	request_event(ts, SHTPS_EVENT_INTERRUPT, 1);

	shtps_wake_unlock_idle(ts);
}

static void shtps_setsleep_proc(struct shtps_fts *ts, u8 sleep)
{
	if(sleep){
		shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVEMT_LCD_OFF);
	}else{
		shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVEMT_LCD_ON);
	}
}

static void shtps_async_open_proc(struct shtps_fts *ts, u8 dummy)
{
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVEMT_OPEN);
}

static void shtps_async_close_proc(struct shtps_fts *ts, u8 dummy)
{
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVEMT_CLOSE);
}

static void shtps_async_enable_proc(struct shtps_fts *ts, u8 dummy)
{
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVEMT_ENABLE);
}

static void shtps_suspend_i2c_wake_lock(struct shtps_fts *ts, u8 lock)
{
	if(lock){
		if(ts->deter_suspend_i2c.wake_lock_state == 0){
			ts->deter_suspend_i2c.wake_lock_state = 1;
			wake_lock(&ts->deter_suspend_i2c.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, SHTPS_QOS_LATENCY_DEF_VALUE);
		}
	}else{
		if(ts->deter_suspend_i2c.wake_lock_state == 1){
			ts->deter_suspend_i2c.wake_lock_state = 0;
			wake_unlock(&ts->deter_suspend_i2c.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
		}
	}
}

int shtps_check_suspend_state(struct shtps_fts *ts, int proc, u8 param)
{
	int ret = 0;

	shtps_mutex_lock_proc();
	if(ts->deter_suspend_i2c.suspend){
		shtps_suspend_i2c_wake_lock(ts, 1);
		ts->deter_suspend_i2c.pending_info[proc].pending= 1;
		ts->deter_suspend_i2c.pending_info[proc].param  = param;
		ret = 1;
	}else{
		ts->deter_suspend_i2c.pending_info[proc].pending= 0;
		ret = 0;
	}

	if(proc == SHTPS_DETER_SUSPEND_I2C_PROC_OPEN ||
		proc == SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE)
	{
		if(ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE].pending){
			ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE].pending = 0;
		}
	}else if(proc == SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE){
		if(ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_OPEN].pending){
			ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_OPEN].pending  = 0;
		}
		if(ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE].pending){
			ts->deter_suspend_i2c.pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE].pending= 0;
		}
	}
	shtps_mutex_unlock_proc();
	return ret;
}

static void shtps_exec_suspend_pending_proc(struct shtps_fts *ts)
{
	cancel_work_sync(&ts->deter_suspend_i2c.pending_proc_work);

	schedule_work(&ts->deter_suspend_i2c.pending_proc_work);
}

static void shtps_deter_suspend_i2c_pending_proc_work_function(struct work_struct *work)
{
	struct shtps_deter_suspend_i2c *dss = container_of(work, struct shtps_deter_suspend_i2c, pending_proc_work);
	struct shtps_fts *ts = container_of(dss, struct shtps_fts, deter_suspend_i2c);
	int i;

	shtps_mutex_lock_proc();

	for(i = 0;i < SHTPS_DETER_SUSPEND_I2C_PROC_NUM;i++){
		if(ts->deter_suspend_i2c.pending_info[i].pending){
			SHTPS_SUSPEND_PENDING_FUNC_TBL[i](ts, ts->deter_suspend_i2c.pending_info[i].param);
			ts->deter_suspend_i2c.pending_info[i].pending = 0;
		}
	}
	shtps_suspend_i2c_wake_lock(ts, 0);

	shtps_mutex_unlock_proc();
}

void shtps_set_suspend_state(struct shtps_fts *ts)
{
	shtps_mutex_lock_proc();
	ts->deter_suspend_i2c.suspend = 1;
	shtps_mutex_unlock_proc();
}

void shtps_clr_suspend_state(struct shtps_fts *ts)
{
	int i;
	int hold_process = 0;

	shtps_mutex_lock_proc();
	ts->deter_suspend_i2c.suspend = 0;
	for(i = 0;i < SHTPS_DETER_SUSPEND_I2C_PROC_NUM;i++){
		if(ts->deter_suspend_i2c.pending_info[i].pending){
			hold_process = 1;
			break;
		}
	}

	if(hold_process){
		shtps_exec_suspend_pending_proc(ts);
	}else{
		shtps_suspend_i2c_wake_lock(ts, 0);
	}
	shtps_mutex_unlock_proc();

	shtps_mutex_lock_ctrl();
	if(ts->deter_suspend_i2c.suspend_irq_detect != 0){
		if(ts->deter_suspend_i2c.suspend_irq_state == SHTPS_IRQ_STATE_ENABLE){
			shtps_irq_enable(ts);
		}

		ts->deter_suspend_i2c.suspend_irq_detect = 0;
	}
	shtps_mutex_unlock_ctrl();
}

/* -----------------------------------------------------------------------------------
 */
static irqreturn_t shtps_irq_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t shtps_irq(int irq, void *dev_id)
{
	struct shtps_fts	*ts = dev_id;

	if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_I2C_PROC_IRQ, 0) == 0){
		shtps_irq_proc(ts, 0);
	}else{
		shtps_mutex_lock_ctrl();
		ts->deter_suspend_i2c.suspend_irq_state = ts->irq_mgr.state;
		ts->deter_suspend_i2c.suspend_irq_wake_state = ts->irq_mgr.wake;
		ts->deter_suspend_i2c.suspend_irq_detect = 1;
		shtps_irq_disable(ts);
		shtps_mutex_unlock_ctrl();
	}

	return IRQ_HANDLED;
}

/* -----------------------------------------------------------------------------------
 */
int shtps_start(struct shtps_fts *ts)
{
	return request_event(ts, SHTPS_EVENT_START, 0);
}

void shtps_shutdown(struct shtps_fts *ts)
{
	request_event(ts, SHTPS_EVENT_STOP, 0);
}

/* -----------------------------------------------------------------------------------
 */
void shtps_reset(struct shtps_fts *ts)
{
	shtps_irq_disable(ts);
	shtps_device_reset(ts->rst_pin);
}

void shtps_sleep(struct shtps_fts *ts, int on)
{
	if(on){
		shtps_fwctl_set_sleepmode_on(ts);
	}else{
		shtps_fwctl_set_sleepmode_off(ts);
	}
}

u16 shtps_fwver(struct shtps_fts *ts)
{
	int rc;
	u16 ver = 0;
	u8 retry = 3;

	do{
		rc = shtps_fwctl_get_fwver(ts, &ver);
		if(rc == 0)	break;
	}while(retry-- > 0);

	return ver;
}

u16 shtps_fwver_builtin(struct shtps_fts *ts)
{
	u16 ver = 0;

	ver = SHTPS_FWVER_NEWER;

	return ver;
}

int shtps_fwsize_builtin(struct shtps_fts *ts)
{
	int size = 0;

	size = SHTPS_FWSIZE_NEWER;

	return size;
}

unsigned char* shtps_fwdata_builtin(struct shtps_fts *ts)
{
	unsigned char *data = NULL;

	data = (unsigned char *)tps_fw_data;

	return data;
}

static int shtps_init_param(struct shtps_fts *ts)
{
	int rc;

	rc = shtps_fwctl_initparam(ts);
	SHTPS_ERR_CHECK(rc, err_exit);

	return 0;

err_exit:
	return -1;
}

static void shtps_standby_param(struct shtps_fts *ts)
{
	shtps_sleep(ts, 1);
}

int shtps_get_fingermax(struct shtps_fts *ts)
{
	return shtps_fwctl_get_fingermax(ts);
}

/* -----------------------------------------------------------------------------------
 */
int shtps_get_diff(unsigned short pos1, unsigned short pos2)
{
	int diff;
	diff = pos1 - pos2;
	return (diff >= 0)? diff : -diff;
}

int shtps_get_fingerwidth(struct shtps_fts *ts, int num, struct shtps_touch_info *info)
{
	int w = (info->fingers[num].wx >= info->fingers[num].wy)? info->fingers[num].wx : info->fingers[num].wy;

	return (w < 1)? 1 : w;
}

void shtps_set_eventtype(u8 *event, u8 type)
{
	*event = type;
}

static void shtps_set_touch_info(struct shtps_fts *ts, u8 *buf, struct shtps_touch_info *info)
{
	u8* fingerInfo;
	int FingerNum = 0;
	int fingerMax = shtps_get_fingermax(ts);
	int point_num = shtps_fwctl_get_num_of_touch_fingers(ts, buf);
	int i;
	int pointid;

	memset(&ts->fw_report_info, 0, sizeof(ts->fw_report_info));
	memset(info, 0, sizeof(struct shtps_touch_info));

	for(i = 0; i < point_num; i++){
		fingerInfo = shtps_fwctl_get_finger_info_buf(ts, i, fingerMax, buf);
		pointid = shtps_fwctl_get_finger_pointid(ts, fingerInfo);
		if (pointid >= fingerMax){
			break;
		}

		ts->fw_report_info.fingers[pointid].state	= shtps_fwctl_get_finger_state(ts, i, fingerMax, buf);
		ts->fw_report_info.fingers[pointid].x		= shtps_fwctl_get_finger_pos_x(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].y		= shtps_fwctl_get_finger_pos_y(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].wx		= shtps_fwctl_get_finger_wx(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].wy		= shtps_fwctl_get_finger_wy(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].z		= shtps_fwctl_get_finger_z(ts, fingerInfo);

		info->fingers[pointid].state	= ts->fw_report_info.fingers[pointid].state;
		info->fingers[pointid].x		= ts->fw_report_info.fingers[pointid].x;
		info->fingers[pointid].y		= ts->fw_report_info.fingers[pointid].y;
		info->fingers[pointid].wx		= ts->fw_report_info.fingers[pointid].wx;
		info->fingers[pointid].wy		= ts->fw_report_info.fingers[pointid].wy;
		info->fingers[pointid].z		= ts->fw_report_info.fingers[pointid].z;
	}

	for(i = 0; i < fingerMax; i++){
		if(ts->fw_report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			FingerNum++;
		}
		else if(ts->fw_report_info_store.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
		}
	}

	ts->fw_report_info.finger_num = FingerNum;
}


static void shtps_calc_notify(struct shtps_fts *ts, u8 *buf, struct shtps_touch_info *info, u8 *event)
{
	int i;
	int fingerMax = shtps_get_fingermax(ts);
	int numOfFingers = 0;
	int diff_x;
	int diff_y;

	shtps_set_eventtype(event, 0xff);
	shtps_set_touch_info(ts, buf, info);

	for (i = 0; i < fingerMax; i++) {
		if(info->fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			numOfFingers++;
		}
	}
	info->finger_num = numOfFingers;

	/* set event type */
	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			if(ts->report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
				diff_x = shtps_get_diff(info->fingers[i].x, ts->report_info.fingers[i].x);
				diff_y = shtps_get_diff(info->fingers[i].y, ts->report_info.fingers[i].y);

				if((diff_x > 0) || (diff_y > 0)){
					shtps_set_eventtype(event, SHTPS_EVENT_DRAG);
				}
			}
		}

		if(info->fingers[i].state != ts->report_info.fingers[i].state){
			shtps_set_eventtype(event, SHTPS_EVENT_MTDU);
		}
	}

	if(numOfFingers > 0){
		if(ts->touch_state.numOfFingers == 0){
			shtps_set_eventtype(event, SHTPS_EVENT_TD);
		}
	}else{
		if(ts->touch_state.numOfFingers != 0){
			shtps_set_eventtype(event, SHTPS_EVENT_TU);
		}
	}
}


void shtps_report_touch_on(struct shtps_fts *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	int lcd_x;
	int lcd_y;

	lcd_x = x;
	lcd_y = y;
	if(lcd_x >= CONFIG_SHTPS_LCD_SIZE_X){
		lcd_x = CONFIG_SHTPS_LCD_SIZE_X - 1;
	}
	if(lcd_y >= CONFIG_SHTPS_LCD_SIZE_Y){
		lcd_y = CONFIG_SHTPS_LCD_SIZE_Y - 1;
	}

	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input, ABS_MT_POSITION_X,  lcd_x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y,  lcd_y);
	input_report_abs(ts->input, ABS_MT_PRESSURE,    z);
}

void shtps_report_touch_off(struct shtps_fts *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
}

void shtps_event_report(struct shtps_fts *ts, struct shtps_touch_info *info, u8 event)
{
	int	i;
	int fingerMax = shtps_get_fingermax(ts);

	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state == SHTPS_TOUCH_STATE_FINGER){
			shtps_report_touch_on(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  shtps_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);

		}else if(ts->report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			shtps_report_touch_off(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  shtps_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);
		}
	}
	input_sync(ts->input);

	ts->touch_state.numOfFingers = info->finger_num;

	memcpy(&ts->report_info, info, sizeof(ts->report_info));
}

void shtps_event_force_touchup(struct shtps_fts *ts)
{
	int	i;
	int isEvent = 0;
	int fingerMax = shtps_get_fingermax(ts);

	for(i = 0;i < fingerMax;i++){
		if(ts->report_info.fingers[i].state != 0x00){
			isEvent = 1;
			shtps_report_touch_off(ts, i,
								  ts->report_info.fingers[i].x,
								  ts->report_info.fingers[i].y,
								  0,
								  0,
								  0,
								  0);
		}
	}
	if(isEvent){
		input_sync(ts->input);
		ts->touch_state.numOfFingers = 0;
		memset(&ts->report_info, 0, sizeof(ts->report_info));
	}
}

/* -----------------------------------------------------------------------------------
 */
void shtps_irq_disable(struct shtps_fts *ts)
{
	if(ts->irq_mgr.state != SHTPS_IRQ_STATE_DISABLE){
		disable_irq_nosync(ts->irq_mgr.irq);
		ts->irq_mgr.state = SHTPS_IRQ_STATE_DISABLE;
	}

	if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_DISABLE){
		disable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = SHTPS_IRQ_WAKE_DISABLE;
	}
}

void shtps_irq_enable(struct shtps_fts *ts)
{
	if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_ENABLE){
		enable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = SHTPS_IRQ_WAKE_ENABLE;
	}

	if(ts->irq_mgr.state != SHTPS_IRQ_STATE_ENABLE){
		enable_irq(ts->irq_mgr.irq);
		ts->irq_mgr.state = SHTPS_IRQ_STATE_ENABLE;
	}
}

static int shtps_irq_resuest(struct shtps_fts *ts)
{
	int rc;

	rc = request_threaded_irq(ts->irq_mgr.irq,
						  shtps_irq_handler,
						  shtps_irq,
						  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						  SH_TOUCH_DEVNAME,
						  ts);

	if(rc){
		return -1;
	}

	ts->irq_mgr.state = SHTPS_IRQ_STATE_ENABLE;
	ts->irq_mgr.wake  = SHTPS_IRQ_WAKE_DISABLE;
	shtps_irq_disable(ts);
	return 0;
}

void shtps_read_touchevent(struct shtps_fts *ts, int state)
{
	u8 buf[3 + (10 * 6)];
	u8 event;
	u8 irq_sts, ext_sts;
	u8 *pFingerInfoBuf;
	struct shtps_touch_info info;

	memset(buf, 0, sizeof(buf));
	memset(&info, 0, sizeof(info));
	shtps_fwctl_get_fingerinfo(ts, buf, 0, &irq_sts, &ext_sts, &pFingerInfoBuf);

	switch(state){
	case SHTPS_STATE_SLEEP:
	case SHTPS_STATE_IDLE:
		break;

	case SHTPS_STATE_ACTIVE:
	default:
		shtps_calc_notify(ts, pFingerInfoBuf, &info, &event);

		if(event != 0xff){
			shtps_event_report(ts, &info, event);
		}

		memcpy(&ts->fw_report_info_store, &ts->fw_report_info, sizeof(ts->fw_report_info));

		break;
	}
}

/* -----------------------------------------------------------------------------------
 */
int shtps_enter_bootloader(struct shtps_fts *ts)
{
	shtps_mutex_lock_loader();

	if(request_event(ts, SHTPS_EVENT_STARTLOADER, 0) != 0){
		goto err_exit;
	}
	shtps_fwctl_set_dev_state(ts, SHTPS_DEV_STATE_LOADER);

	shtps_mutex_unlock_loader();
	return 0;

err_exit:
	shtps_mutex_unlock_loader();
	return -1;
}

static int shtps_exit_bootloader(struct shtps_fts *ts)
{
	request_event(ts, SHTPS_EVENT_START, 0);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
int request_event(struct shtps_fts *ts, int event, int param)
{
	int ret;

	shtps_mutex_lock_ctrl();

	switch(event){
	case SHTPS_EVENT_START:
		ret = state_func_tbl[ts->state_mgr.state]->start(ts, param);
		break;
	case SHTPS_EVENT_STOP:
		ret = state_func_tbl[ts->state_mgr.state]->stop(ts, param);
		break;
	case SHTPS_EVENT_SLEEP:
		ret = state_func_tbl[ts->state_mgr.state]->sleep(ts, param);
		break;
	case SHTPS_EVENT_WAKEUP:
		ret = state_func_tbl[ts->state_mgr.state]->wakeup(ts, param);
		break;
	case SHTPS_EVENT_STARTLOADER:
		ret = state_func_tbl[ts->state_mgr.state]->start_ldr(ts, param);
		break;
	case SHTPS_EVENT_INTERRUPT:
		ret = state_func_tbl[ts->state_mgr.state]->interrupt(ts, param);
		break;
	default:
		ret = -1;
		break;
	}

	shtps_mutex_unlock_ctrl();

	return ret;
}

static int state_change(struct shtps_fts *ts, int state)
{
	int ret = 0;
	int old_state = ts->state_mgr.state;

	if(ts->state_mgr.state != state){
		ts->state_mgr.state = state;
		ret = state_func_tbl[ts->state_mgr.state]->enter(ts, old_state);
	}
	return ret;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_nop(struct shtps_fts *ts, int param)
{
	return 0;
}

static int shtps_statef_cmn_stop(struct shtps_fts *ts, int param)
{
	shtps_standby_param(ts);
	shtps_irq_disable(ts);
	shtps_system_set_sleep(ts);
	state_change(ts, SHTPS_STATE_IDLE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_idle_start(struct shtps_fts *ts, int param)
{
	u8 buf;

	shtps_system_set_wakeup(ts);
	shtps_reset(ts);

	shtps_fwctl_get_device_status(ts, &buf);
	if(buf == SHTPS_REGVAL_STATUS_CRC_ERROR){
		shtps_reset(ts);
	}

	state_change(ts, SHTPS_STATE_ACTIVE);

	return 0;
}

static int shtps_statef_idle_start_ldr(struct shtps_fts *ts, int param)
{
	shtps_system_set_wakeup(ts);
	shtps_reset(ts);

	state_change(ts, SHTPS_STATE_BOOTLOADER);

	return 0;
}

static int shtps_statef_idle_wakeup(struct shtps_fts *ts, int param)
{
	shtps_statef_idle_start(ts, param);
	return 0;
}

static int shtps_statef_idle_int(struct shtps_fts *ts, int param)
{
	shtps_read_touchevent(ts, SHTPS_STATE_IDLE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_active_enter(struct shtps_fts *ts, int param)
{
	if((param == SHTPS_STATE_IDLE) || (param == SHTPS_STATE_SLEEP) || (param == SHTPS_STATE_BOOTLOADER)){
		shtps_init_param(ts);
		shtps_irq_enable(ts);
	}

	return 0;
}

static int shtps_statef_active_stop(struct shtps_fts *ts, int param)
{
	shtps_irq_disable(ts);

	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_active_sleep(struct shtps_fts *ts, int param)
{
	state_change(ts, SHTPS_STATE_SLEEP);
	return 0;
}

static int shtps_statef_active_start_ldr(struct shtps_fts *ts, int param)
{
	state_change(ts, SHTPS_STATE_BOOTLOADER);
	return 0;
}

static int shtps_statef_active_int(struct shtps_fts *ts, int param)
{
	shtps_read_touchevent(ts, SHTPS_STATE_ACTIVE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_loader_enter(struct shtps_fts *ts, int param)
{
	shtps_wake_lock_for_fwupdate(ts);

	shtps_irq_disable(ts);

	return 0;
}

static int shtps_statef_loader_start(struct shtps_fts *ts, int param)
{
	shtps_wake_unlock_for_fwupdate(ts);

	state_change(ts, SHTPS_STATE_ACTIVE);

	return 0;
}

static int shtps_statef_loader_stop(struct shtps_fts *ts, int param)
{
	shtps_irq_disable(ts);

	shtps_wake_unlock_for_fwupdate(ts);

	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_loader_int(struct shtps_fts *ts, int param)
{
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_sleep_enter(struct shtps_fts *ts, int param)
{
	shtps_event_force_touchup(ts);

	shtps_irq_disable(ts);

	shtps_statef_cmn_stop(ts, 0);

	return 0;
}

static int shtps_statef_sleep_wakeup(struct shtps_fts *ts, int param)
{
	shtps_irq_enable(ts);

	shtps_system_set_wakeup(ts);

	shtps_sleep(ts, 0);

	shtps_reset(ts);

	state_change(ts, SHTPS_STATE_ACTIVE);
	return 0;
}

static int shtps_statef_sleep_start_ldr(struct shtps_fts *ts, int param)
{
	state_change(ts, SHTPS_STATE_BOOTLOADER);
	return 0;
}

static int shtps_statef_sleep_int(struct shtps_fts *ts, int param)
{
	shtps_read_touchevent(ts, SHTPS_STATE_SLEEP);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_fts_open(struct input_dev *dev)
{
	struct shtps_fts *ts = (struct shtps_fts*)input_get_drvdata(dev);

	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVEMT_OPEN);

	return 0;
}

static void shtps_fts_close(struct input_dev *dev)
{
	struct shtps_fts *ts = (struct shtps_fts*)input_get_drvdata(dev);

	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVEMT_CLOSE);
}

static int shtps_init_internal_variables(struct shtps_fts *ts)
{
	int result = 0;

	result = shtps_func_async_init(ts);
	if(result < 0){
		goto fail_init;
	}

	ts->state_mgr.state = SHTPS_STATE_IDLE;

	memset(&ts->fw_report_info, 0, sizeof(ts->fw_report_info));
	memset(&ts->fw_report_info_store, 0, sizeof(ts->fw_report_info_store));
	memset(&ts->report_info, 0, sizeof(ts->report_info));
	memset(&ts->touch_state, 0, sizeof(ts->touch_state));

	shtps_cpu_idle_sleep_wake_lock_init(ts);
	shtps_fwupdate_wake_lock_init(ts);

	shtps_fwctl_set_dev_state(ts, SHTPS_DEV_STATE_SLEEP);

    ts->deter_suspend_i2c.wake_lock_state = 0;
	memset(&ts->deter_suspend_i2c, 0, sizeof(ts->deter_suspend_i2c));
    wake_lock_init(&ts->deter_suspend_i2c.wake_lock, WAKE_LOCK_SUSPEND, "shtps_resume_wake_lock");
	ts->deter_suspend_i2c.pm_qos_lock_idle.type = PM_QOS_REQ_AFFINE_CORES;
	ts->deter_suspend_i2c.pm_qos_lock_idle.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	INIT_WORK(&ts->deter_suspend_i2c.pending_proc_work, shtps_deter_suspend_i2c_pending_proc_work_function);

	/* -------------------------------------------------------------------------- */
	return 0;

fail_init:
	shtps_func_async_deinit(ts);

	return result;
}


static void shtps_deinit_internal_variables(struct shtps_fts *ts)
{
	if(ts){
		shtps_func_async_deinit(ts);
		shtps_cpu_idle_sleep_wake_lock_deinit(ts);
		shtps_fwupdate_wake_lock_deinit(ts);
	}
}

static int shtps_init_inputdev(struct shtps_fts *ts, struct device *ctrl_dev_p, char *modalias)
{
	ts->input = input_allocate_device();
	if (!ts->input){
		return -ENOMEM;
	}

	ts->input->name 		= modalias;
	ts->input->phys			= ts->phys;
	ts->input->id.vendor	= 0x0001;
	ts->input->id.product	= 0x0002;
	ts->input->id.version	= 0x0100;
	ts->input->dev.parent	= ctrl_dev_p;
	ts->input->open			= shtps_fts_open;
	ts->input->close		= shtps_fts_close;

	/** set properties */
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input->propbit);

	input_set_drvdata(ts->input, ts);
	input_mt_init_slots(ts->input, SHTPS_FINGER_MAX, 0);

	if(ts->input->mt == NULL){
		input_free_device(ts->input);
		return -ENOMEM;
	}

	/** set parameters */
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, SHTPS_VAL_FINGER_WIDTH_MAXSIZE, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_X,  0, CONFIG_SHTPS_LCD_SIZE_X - 1, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y,  0, CONFIG_SHTPS_LCD_SIZE_Y - 1, 0, 0);

	input_set_abs_params(ts->input, ABS_MT_PRESSURE,    0, 255, 0, 0);

	/** register input device */
	if(input_register_device(ts->input) != 0){
		input_free_device(ts->input);
		return -EFAULT;
	}

	return 0;
}

static void shtps_deinit_inputdev(struct shtps_fts *ts)
{
	if(ts && ts->input){
		if(ts->input->mt){
			input_mt_destroy_slots(ts->input);
		}
		input_free_device(ts->input);
	}
}

int shtps_fts_core_probe(
						struct device *ctrl_dev_p,
						struct shtps_ctrl_functbl *func_p,
						void *ctrl_p,
						char *modalias,
						int gpio_irq)
{
	int result = 0;
	struct shtps_fts *ts;
	#ifdef CONFIG_OF
		; // nothing
	#else
		struct shtps_platform_data *pdata = ctrl_dev_p->platform_data;
	#endif /* CONFIG_OF */

	shtpsif_init();

	shtps_mutex_lock_ctrl();

	ts = kzalloc(sizeof(struct shtps_fts), GFP_KERNEL);
	if(!ts){
		result = -ENOMEM;
		shtps_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	result = shtps_fwctl_init(ts, ctrl_p, func_p);
	if(result){
		result = -EFAULT;
		shtps_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	if(shtps_init_internal_variables(ts)){
		shtps_mutex_unlock_ctrl();
		goto fail_init_internal_variables;
	}

	/** set device info */
	dev_set_drvdata(ctrl_dev_p, ts);
	gShtps_fts = ts;


	#ifdef CONFIG_OF
		ts->irq_mgr.irq	= irq_of_parse_and_map(ctrl_dev_p->of_node, 0);
		ts->rst_pin		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_fts,rst_pin", 0);
	#else
		ts->rst_pin		= pdata->gpio_rst;
		ts->irq_mgr.irq	= gpio_irq;
	#endif /* CONFIG_OF */

    if(!gpio_is_valid(ts->rst_pin)){
		result = -EFAULT;
		shtps_mutex_unlock_ctrl();
		goto fail_get_dtsinfo;
	}

	snprintf(ts->phys, sizeof(ts->phys), "%s", dev_name(ctrl_dev_p));

	/** setup device */
	#ifdef CONFIG_OF
		result = shtps_device_setup(ts->irq_mgr.irq, ts->rst_pin);
		if(result){
			shtps_mutex_unlock_ctrl();
			goto fail_device_setup;
		}
	#else
		if (pdata && pdata->setup) {
			result = pdata->setup(ctrl_dev_p);
			if (result){
				shtps_mutex_unlock_ctrl();
				goto fail_alloc_mem;
			}
		}
	#endif /* CONFIG_OF */

	if(shtps_irq_resuest(ts)){
		result = -EFAULT;
		shtps_mutex_unlock_ctrl();
		goto fail_irq_request;
	}

	shtps_mutex_unlock_ctrl();


	/** init device info */
	result = shtps_init_inputdev(ts, ctrl_dev_p, modalias);
	if(result != 0){
		goto fail_init_inputdev;
	}

	return 0;

fail_init_inputdev:
fail_irq_request:
fail_get_dtsinfo:
	shtps_deinit_internal_variables(ts);

fail_init_internal_variables:
fail_device_setup:
	kfree(ts);

fail_alloc_mem:
	shtpsif_exit();
	return result;
}

int shtps_fts_core_remove(struct shtps_fts *ts, struct device *ctrl_dev_p)
{
	#ifndef CONFIG_OF
		struct shtps_platform_data *pdata = (struct shtps_platform_data *)&ctrl_dev_p->platform_data;
	#endif /* !CONFIG_OF */

	gShtps_fts = NULL;

	if(ts){
		free_irq(ts->irq_mgr.irq, ts);

		#ifdef CONFIG_OF
			shtps_device_teardown(ts->irq_mgr.irq, ts->rst_pin);
		#else
			if (pdata && pdata->teardown){
				pdata->teardown(&ctrl_dev_p);
			}
		#endif /* CONFIG_OF */

		shtps_deinit_internal_variables(ts);

		shtps_deinit_inputdev(ts);

		shtps_fwctl_deinit(ts);

		kfree(ts);
	}

	shtpsif_exit();

	return 0;
}

int shtps_fts_core_suspend(struct shtps_fts *ts, pm_message_t mesg)
{
	shtps_set_suspend_state(ts);

	return 0;
}

int shtps_fts_core_resume(struct shtps_fts *ts)
{
	shtps_clr_suspend_state(ts);

	return 0;
}


/* -----------------------------------------------------------------------------------
 */
int shtps_fw_update(struct shtps_fts *ts, const unsigned char *fw_data, int fw_size)
{
	int ret_update;
	u8 buf;

	if(fw_data == NULL || fw_size <= 0){
		return -1;
	}

	if(shtps_enter_bootloader(ts) != 0){
		return -1;
	}

	ret_update = shtps_fwctl_loader_write_pram(ts, (u8*)tps_fw_pram, sizeof(tps_fw_pram));
	if(ret_update == 0){
		ret_update = shtps_fwctl_loader_upgrade(ts, (u8*)fw_data, fw_size);
		if(ret_update == 0){
			shtps_fwctl_get_device_status(ts, &buf);
			if(buf == SHTPS_REGVAL_STATUS_CRC_ERROR){
				ret_update = -1;
			}
		}
	}

	if(shtps_exit_bootloader(ts) != 0){
		return -1;
	}

	return ret_update;
}

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(shtps_cpu_idle_sleep_ctrl_lock);
static DEFINE_MUTEX(shtps_cpu_sleep_ctrl_for_fwupdate_lock);

/* -----------------------------------------------------------------------------------
 */
static void shtps_mutex_lock_cpu_idle_sleep_ctrl(void);
static void shtps_mutex_unlock_cpu_idle_sleep_ctrl(void);
static void shtps_wake_lock_idle_l(struct shtps_fts *ts);
static void shtps_wake_unlock_idle_l(struct shtps_fts *ts);

static void shtps_mutex_lock_cpu_sleep_ctrl(void);
static void shtps_mutex_unlock_cpu_sleep_ctrl(void);

static void shtps_func_open(struct shtps_fts *ts);
static void shtps_func_close(struct shtps_fts *ts);
static int shtps_func_enable(struct shtps_fts *ts);
static void shtps_func_disable(struct shtps_fts *ts);

static void shtps_func_request_async_complete(void *arg_p);
static void shtps_func_request_sync_complete(void *arg_p);
static void shtps_func_workq( struct work_struct *work_p );

/* -------------------------------------------------------------------------- */
struct shtps_req_msg {	//**********	SHTPS_ASYNC_OPEN_ENABLE
	struct list_head queue;
	void	(*complete)(void *context);
	void	*context;
	int		status;
	int		event;
	void	*param_p;
};

/* -----------------------------------------------------------------------------------
 */
static void shtps_mutex_lock_cpu_idle_sleep_ctrl(void)
{
	mutex_lock(&shtps_cpu_idle_sleep_ctrl_lock);
}

static void shtps_mutex_unlock_cpu_idle_sleep_ctrl(void)
{
	mutex_unlock(&shtps_cpu_idle_sleep_ctrl_lock);
}

static void shtps_wake_lock_idle_l(struct shtps_fts *ts)
{
	shtps_mutex_lock_cpu_idle_sleep_ctrl();
	if(ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state == 0){
		pm_qos_update_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, SHTPS_QOS_LATENCY_DEF_VALUE);
		ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state = 1;
	}
	shtps_mutex_unlock_cpu_idle_sleep_ctrl();
}

static void shtps_wake_unlock_idle_l(struct shtps_fts *ts)
{
	shtps_mutex_lock_cpu_idle_sleep_ctrl();
	if(ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state != 0){
		pm_qos_update_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, PM_QOS_DEFAULT_VALUE);
		ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state = 0;
	}
	shtps_mutex_unlock_cpu_idle_sleep_ctrl();
}

void shtps_wake_lock_idle(struct shtps_fts *ts)
{
	if(SHTPS_STATE_ACTIVE == ts->state_mgr.state){
		shtps_wake_lock_idle_l(ts);
	}
}

void shtps_wake_unlock_idle(struct shtps_fts *ts)
{
	shtps_wake_unlock_idle_l(ts);
}

void shtps_cpu_idle_sleep_wake_lock_init( struct shtps_fts *ts )
{
	ts->cpu_idle_sleep_ctrl_p = kzalloc(sizeof(struct shtps_cpu_idle_sleep_ctrl_info), GFP_KERNEL);
	if(ts->cpu_idle_sleep_ctrl_p == NULL){
		return;
	}
	ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency.type = PM_QOS_REQ_AFFINE_CORES;
	ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

void shtps_cpu_idle_sleep_wake_lock_deinit( struct shtps_fts *ts )
{
	pm_qos_remove_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency);

	if(ts->cpu_idle_sleep_ctrl_p)	kfree(ts->cpu_idle_sleep_ctrl_p);
	ts->cpu_idle_sleep_ctrl_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_mutex_lock_cpu_sleep_ctrl(void)
{
	mutex_lock(&shtps_cpu_sleep_ctrl_for_fwupdate_lock);
}

static void shtps_mutex_unlock_cpu_sleep_ctrl(void)
{
	mutex_unlock(&shtps_cpu_sleep_ctrl_for_fwupdate_lock);
}

void shtps_wake_lock_for_fwupdate(struct shtps_fts *ts)
{
	shtps_mutex_lock_cpu_sleep_ctrl();
	if(ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state == 0){
		wake_lock(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);
		pm_qos_update_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, SHTPS_QOS_LATENCY_DEF_VALUE);
		ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state = 1;
	}
	shtps_mutex_unlock_cpu_sleep_ctrl();
}

void shtps_wake_unlock_for_fwupdate(struct shtps_fts *ts)
{
	shtps_mutex_lock_cpu_sleep_ctrl();
	if(ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state != 0){
		wake_unlock(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);
		pm_qos_update_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, PM_QOS_DEFAULT_VALUE);
		ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state = 0;
	}
	shtps_mutex_unlock_cpu_sleep_ctrl();
}

void shtps_fwupdate_wake_lock_init( struct shtps_fts *ts )
{
	ts->cpu_sleep_ctrl_fwupdate_p = kzalloc(sizeof(struct shtps_cpu_sleep_ctrl_fwupdate_info), GFP_KERNEL);
	if(ts->cpu_sleep_ctrl_fwupdate_p == NULL){
		return;
	}

	wake_lock_init(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate, WAKE_LOCK_SUSPEND, "shtps_wake_lock_for_fwupdate");
	ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate.type = PM_QOS_REQ_AFFINE_CORES;
	ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

void shtps_fwupdate_wake_lock_deinit( struct shtps_fts *ts )
{
	pm_qos_remove_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate);
	wake_lock_destroy(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);

	if(ts->cpu_sleep_ctrl_fwupdate_p)	kfree(ts->cpu_sleep_ctrl_fwupdate_p);
	ts->cpu_sleep_ctrl_fwupdate_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_func_lcd_on(struct shtps_fts *ts)
{
	request_event(ts, SHTPS_EVENT_WAKEUP, 0);
}

static void shtps_func_lcd_off(struct shtps_fts *ts)
{
	request_event(ts, SHTPS_EVENT_SLEEP, 0);
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_func_boot_fw_update(struct shtps_fts *ts)
{
	u8 buf;
	const unsigned char* fw_data = NULL;
	int ver;
	u8 update = 0;

	shtps_start(ts);

	shtps_fwctl_get_device_status(ts, &buf);

	ver = shtps_fwver(ts);
	if(ver != shtps_fwver_builtin(ts)){
		update = 1;
	}

	if(buf == SHTPS_REGVAL_STATUS_CRC_ERROR){
		update = 1;
	}

	if(update != 0){
		fw_data = shtps_fwdata_builtin(ts);
		if(fw_data){
			int ret;
			int retry = 5;
			do{
				ret = shtps_fw_update(ts, fw_data, shtps_fwsize_builtin(ts));
			}while(ret != 0 && (retry-- > 0));
		}
	}

	if(ts->is_lcd_on == 0){
		shtps_shutdown(ts);
	}
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_func_open(struct shtps_fts *ts)
{
}

static void shtps_func_close(struct shtps_fts *ts)
{
	shtps_shutdown(ts);
}

static int shtps_func_enable(struct shtps_fts *ts)
{
	if(shtps_start(ts) != 0){
		return -EFAULT;
	}

	return 0;
}

static void shtps_func_wakelock_init(struct shtps_fts *ts)
{
	wake_lock_init(&ts->work_wake_lock, WAKE_LOCK_SUSPEND, "shtps_work_wake_lock");
	ts->work_pm_qos_lock_idle.type = PM_QOS_REQ_AFFINE_CORES;
	ts->work_pm_qos_lock_idle.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->work_pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

static void shtps_func_wakelock_deinit(struct shtps_fts *ts)
{
	wake_lock_destroy(&ts->work_wake_lock);
	pm_qos_remove_request(&ts->work_pm_qos_lock_idle);
}

static void shtps_func_wakelock(struct shtps_fts *ts, int on)
{
	if(on){
		wake_lock(&ts->work_wake_lock);
		pm_qos_update_request(&ts->work_pm_qos_lock_idle, SHTPS_QOS_LATENCY_DEF_VALUE);
	}else{
		wake_unlock(&ts->work_wake_lock);
		pm_qos_update_request(&ts->work_pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
	}
}

static void shtps_func_disable(struct shtps_fts *ts)
{
	shtps_shutdown(ts);
}


static void shtps_func_request_async_complete(void *arg_p)
{
	kfree( arg_p );
}

void shtps_func_request_async( struct shtps_fts *ts, int event)
{
	struct shtps_req_msg		*msg_p;
	unsigned long	flags;

	msg_p = (struct shtps_req_msg *)kzalloc( sizeof( struct shtps_req_msg ), GFP_KERNEL );
	if ( msg_p == NULL ){
		return;
	}

	msg_p->complete = shtps_func_request_async_complete;
	msg_p->event = event;
	msg_p->context = msg_p;
	msg_p->status = -1;

	spin_lock_irqsave( &(ts->queue_lock), flags);
	list_add_tail( &(msg_p->queue), &(ts->queue) );
	spin_unlock_irqrestore( &(ts->queue_lock), flags);
	queue_work(ts->workqueue_p, &(ts->work_data) );
}

static void shtps_func_request_sync_complete(void *arg_p)
{
	complete( arg_p );
}

int shtps_func_request_sync( struct shtps_fts *ts, int event)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct shtps_req_msg msg;
	unsigned long	flags;

	msg.complete = shtps_func_request_sync_complete;
	msg.event = event;
	msg.context = &done;
	msg.status = -1;

	spin_lock_irqsave( &(ts->queue_lock), flags);
	list_add_tail( &(msg.queue), &(ts->queue) );
	spin_unlock_irqrestore( &(ts->queue_lock), flags);
	queue_work(ts->workqueue_p, &(ts->work_data) );

	wait_for_completion(&done);

	return msg.status;
}

static void shtps_overlap_event_check(struct shtps_fts *ts)
{
	unsigned long flags;
	struct list_head *list_p;
	int overlap_req_cnt_lcd = -1;

	spin_lock_irqsave( &(ts->queue_lock), flags );
	list_for_each(list_p, &(ts->queue)){
		ts->cur_msg_p = list_entry(list_p, struct shtps_req_msg, queue);
		spin_unlock_irqrestore( &(ts->queue_lock), flags );

		if( (ts->cur_msg_p->event == SHTPS_FUNC_REQ_EVEMT_LCD_ON) ||
			(ts->cur_msg_p->event == SHTPS_FUNC_REQ_EVEMT_LCD_OFF) )
		{
			overlap_req_cnt_lcd++;
		}

		spin_lock_irqsave( &(ts->queue_lock), flags );
	}
	spin_unlock_irqrestore( &(ts->queue_lock), flags );

	if(overlap_req_cnt_lcd > 0){
		spin_lock_irqsave( &(ts->queue_lock), flags );
		list_for_each(list_p, &(ts->queue)){
			ts->cur_msg_p = list_entry(list_p, struct shtps_req_msg, queue);
			spin_unlock_irqrestore( &(ts->queue_lock), flags );

			if(ts->cur_msg_p != NULL){
				if( (ts->cur_msg_p->event == SHTPS_FUNC_REQ_EVEMT_LCD_ON) ||
					(ts->cur_msg_p->event == SHTPS_FUNC_REQ_EVEMT_LCD_OFF) )
				{
					if(overlap_req_cnt_lcd > 0){
						spin_lock_irqsave( &(ts->queue_lock), flags );
						list_p = list_p->prev;
						list_del_init( &(ts->cur_msg_p->queue) );
						spin_unlock_irqrestore( &(ts->queue_lock), flags );
						ts->cur_msg_p->status = 0;
						if( ts->cur_msg_p->complete ){
							ts->cur_msg_p->complete( ts->cur_msg_p->context );
						}
						overlap_req_cnt_lcd--;
					}
				}
			}

			spin_lock_irqsave( &(ts->queue_lock), flags );
		}

		spin_unlock_irqrestore( &(ts->queue_lock), flags );
	}
}

static void shtps_func_workq( struct work_struct *work_p )
{
	struct shtps_fts	*ts;
	unsigned long			flags;

	ts = container_of(work_p, struct shtps_fts, work_data);

	spin_lock_irqsave( &(ts->queue_lock), flags );
	while( list_empty( &(ts->queue) ) == 0 ){
		ts->cur_msg_p = list_entry( ts->queue.next, struct shtps_req_msg, queue);
		list_del_init( &(ts->cur_msg_p->queue) );
		spin_unlock_irqrestore( &(ts->queue_lock), flags );

		shtps_func_wakelock(ts, 1);

		switch(ts->cur_msg_p->event){
			case SHTPS_FUNC_REQ_EVEMT_OPEN:
				if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_I2C_PROC_OPEN, 0) == 0){
					shtps_func_open(ts);
				}
				ts->cur_msg_p->status = 0;
				break;

			case SHTPS_FUNC_REQ_EVEMT_CLOSE:
				if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE, 0) == 0){
					shtps_func_close(ts);
				}
				ts->cur_msg_p->status = 0;
				break;

			case SHTPS_FUNC_REQ_EVEMT_ENABLE:
				if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE, 0) == 0){
					ts->cur_msg_p->status = shtps_func_enable(ts);
				}else{
					ts->cur_msg_p->status = 0;
				}
				break;

			case SHTPS_FUNC_REQ_EVEMT_DISABLE:
				shtps_func_disable(ts);
				ts->cur_msg_p->status = 0;
				break;

			case SHTPS_FUNC_REQ_EVEMT_LCD_ON:
				shtps_func_lcd_on(ts);
				ts->cur_msg_p->status = 0;
				break;

			case SHTPS_FUNC_REQ_EVEMT_LCD_OFF:
				shtps_func_lcd_off(ts);
				ts->cur_msg_p->status = 0;
				break;

			case SHTPS_FUNC_REQ_BOOT_FW_UPDATE:
				shtps_func_boot_fw_update(ts);
				ts->cur_msg_p->status = 0;
				break;

			default:
				ts->cur_msg_p->status = -1;
				break;
		}

		if( ts->cur_msg_p->complete ){
			ts->cur_msg_p->complete( ts->cur_msg_p->context );
		}

		shtps_overlap_event_check(ts);

		shtps_func_wakelock(ts, 0);

		spin_lock_irqsave( &(ts->queue_lock), flags );
	}
	spin_unlock_irqrestore( &(ts->queue_lock), flags );
}

int shtps_func_async_init( struct shtps_fts *ts)
{
	ts->workqueue_p = alloc_workqueue("TPS_WORK", WQ_UNBOUND, 1);
	if(ts->workqueue_p == NULL){
		return -ENOMEM;
	}
	INIT_WORK( &(ts->work_data), shtps_func_workq );
	INIT_LIST_HEAD( &(ts->queue) );
	spin_lock_init( &(ts->queue_lock) );
	shtps_func_wakelock_init(ts);

	return 0;
}

void shtps_func_async_deinit( struct shtps_fts *ts)
{
	shtps_func_wakelock_deinit(ts);
	if(ts->workqueue_p){
		destroy_workqueue(ts->workqueue_p);
	}
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_ic_init(struct shtps_fts *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_Upgrade_ReadPram(struct shtps_fwctl_info *fc_p, unsigned int Addr, unsigned char * pData, unsigned short Datalen)
{
	int ret=-1;
	unsigned char pDataSend[16];

	{
		pDataSend[0] = 0x85;
		pDataSend[1] = 0x00;
		pDataSend[2] = Addr>>8;
		pDataSend[3] = Addr;
		M_DIRECT_WRITE_FUNC(fc_p, pDataSend, 4);

		ret =M_DIRECT_READ_FUNC(fc_p, NULL, 0, pData, Datalen);
		if (ret < 0)
		{
			return ret;
		}
	}

	return 0;
}
int shtps_fwctl_loader_write_pram(struct shtps_fts *ts_p, u8 *pbt_buf, u32 dw_lenth)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp,nowAddress=0,StartFlashAddr=0,FlashAddr=0;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 *pCheckBuffer = NULL;
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret,ReCode=-1;

	if(dw_lenth > 0x10000 || dw_lenth ==0)
	{
		return -EIO;
	}
	pCheckBuffer=(u8 *)kzalloc(dw_lenth+1, GFP_KERNEL);
	for (i = 0; i < FTS_UPGRADE_LOOP; i++)
	{
		/********* Step 1:Reset  CTPM *****/
		M_WRITE_FUNC(fc_p, 0xfc, FTS_UPGRADE_AA);
		msleep(FTS_RST_DELAY_AA_MS);
		M_WRITE_FUNC(fc_p, 0xfc, FTS_UPGRADE_55);
		msleep(200);
		/********* Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 1);
		if(i_ret < 0)
		{
			continue;
		}

		/********* Step 3:check READ-ID ***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
			0x00;
		reg_val[0] = reg_val[1] = 0x00;

		M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == 0x86 && reg_val[1] == 0x06) ||
		    (reg_val[0] == 0x86 && reg_val[1] == 0x07))
		{
			msleep(50);
			break;
		}
		else
		{
			continue;
		}
	}

	if (i >= FTS_UPGRADE_LOOP ) {
		i_ret = -EIO;
		goto ERROR;
	}

	/********* Step 4:write firmware(FW) to ctpm flash *********/
	bt_ecc = 0;
	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xae;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++)
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++)
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, packet_buf, FTS_PACKET_LENGTH + 6);
		if(i_ret < 0)
		{
			goto ERROR;
		}
		nowAddress=nowAddress+FTS_PACKET_LENGTH;
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++)
		{
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, packet_buf, temp + 6);
		if(i_ret < 0)
		{
			goto ERROR;
		}
		nowAddress=nowAddress+temp;
	}

	/********* Step 5: read out checksum ***********************/
	/* send the opration head */
	msleep(100);
	//-----------------------------------------------------------------------------------------------------
	if(nowAddress == dw_lenth)
	{

		FlashAddr=0;
		while(1)
		{
			StartFlashAddr = FlashAddr;
			if(FlashAddr == dw_lenth)
			{
				break;
			}
			else if(FlashAddr+FTS_PACKET_LENGTH > dw_lenth)
			{
				if(shtps_Upgrade_ReadPram(fc_p,StartFlashAddr, pCheckBuffer+FlashAddr, dw_lenth-FlashAddr))
				{
					i_ret = -EIO;
					goto ERROR;
				}
				ReCode = ERROR_CODE_OK;
				FlashAddr = dw_lenth;
			}
			else
			{
				if(shtps_Upgrade_ReadPram(fc_p,StartFlashAddr, pCheckBuffer+FlashAddr, FTS_PACKET_LENGTH))
				{
					i_ret = -EIO;
					goto ERROR;
				}
				FlashAddr += FTS_PACKET_LENGTH;
				ReCode = ERROR_CODE_OK;
			}

			if(ReCode != ERROR_CODE_OK){
				i_ret = -EIO;
				goto ERROR;
			}

		}

		if(FlashAddr == dw_lenth)
		{
			for(i=0; i<dw_lenth; i++)
			{
				if(pCheckBuffer[i] != pbt_buf[i])
				{
					i_ret = -EIO;
					goto ERROR;
				}
			}
		}

	}

	/********* Step 6: start app ***********************/
	auc_i2c_write_buf[0] = 0x08;
	M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 1);
	msleep(20);

	if(pCheckBuffer){
		kfree(pCheckBuffer);
	}

	return 0;

ERROR:
	if(pCheckBuffer){
		kfree(pCheckBuffer);
	}
	return i_ret;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_loader_upgrade(struct shtps_fts *ts_p, u8 *pbt_buf, u32 dw_lenth)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	u8 reg_val[4] = {0};
	u8 reg_val_id[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 lenght;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	int i_ret;
	unsigned char cmd[20];
	unsigned char Checksum = 0;
	u32 uCheckStart = 0x1000;
	u32 uCheckOff = 0x20;
	u32 uStartAddr = 0x00;

	auc_i2c_write_buf[0] = 0x05;
	reg_val_id[0] = 0x00;

	i_ret = M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val_id, 1);
	if(dw_lenth == 0)
	{
		return -EIO;
	}

	if(0x81 == (int)reg_val_id[0])
	{
		if(dw_lenth > 1024*60)
		{
			return -EIO;
		}
	}
	else if(0x80 == (int)reg_val_id[0])
	{
		if(dw_lenth > 1024*64)
		{
			return -EIO;
		}
	}

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/********* Step 2:Enter upgrade mode *****/
		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 2);
		if(i_ret < 0)
		{
			continue;
		}

		/********* Step 3:check READ-ID ***********************/
		msleep(1);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		reg_val[0] = reg_val[1] = 0x00;

		M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 4, reg_val, 2);

		if ((reg_val[0] == 0x86 && reg_val[1] == 0xA6) ||
		    (reg_val[0] == 0x86 && reg_val[1] == 0xA7)) {
			break;
		}
		else {
			continue;
		}
	}

	if (i >= FTS_UPGRADE_LOOP )
		return -EIO;

	/* Step 4:erase app and panel paramenter area */
	{
		cmd[0] = 0x05;
		cmd[1] = reg_val_id[0];//0x80;
		cmd[2] = 0x00;
		M_DIRECT_WRITE_FUNC(fc_p, cmd, 3);
	}

	// Set pramboot download mode
	{
		cmd[0] = 0x09;
		cmd[1] = 0x0B;
		M_DIRECT_WRITE_FUNC(fc_p, cmd, 2);
	}

	for(i=0; i<dw_lenth ; i++)
	{
		Checksum ^= pbt_buf[i];
	}
	msleep(50);

	// erase app area
	auc_i2c_write_buf[0] = 0x61;
	M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 1);
	msleep(1350);

	for(i = 0;i < 15;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 2);

		if(0xF0==reg_val[0] && 0xAA==reg_val[1])
		{
			break;
		}
		msleep(50);
	}

	/********* Step 5:write firmware(FW) to ctpm flash *********/
	bt_ecc = 0;

	temp = 0;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;

	for (j = 0; j < packet_number; j++) {
		temp = uCheckStart+j * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr=temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		uCheckOff = uStartAddr/lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++)
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, packet_buf, FTS_PACKET_LENGTH + 6);
		if(i_ret < 0)
		{
			return -EIO;
		}

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff +uCheckStart) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
//			msleep(1);
			mdelay(1);

		}

		temp = (((reg_val[0]) << 8) | reg_val[1]);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) {
		temp = uCheckStart+packet_number * FTS_PACKET_LENGTH;
		packet_buf[1] = (u8) (temp >> 16);
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		uStartAddr=temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		uCheckOff=uStartAddr/temp;

		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, packet_buf, temp + 6);
		if(i_ret < 0)
		{
			return -EIO;
		}

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 2);

			if ((uCheckOff + uCheckStart) == (((reg_val[0]) << 8) | reg_val[1]))
			{
				break;
			}
//			msleep(1);
			mdelay(1);

		}

		temp = (((reg_val[0]) << 8) | reg_val[1]);
	}

	msleep(50);

	/********* Step 6: read out checksum ***********************/
	/*send the opration head */
	auc_i2c_write_buf[0] = 0x64;
	M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 1);
	msleep(300);

	temp = uCheckStart+0;

	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);

	if (dw_lenth > FTS_LEN_FLASH_ECC_MAX)
	{
		temp = FTS_LEN_FLASH_ECC_MAX;
	}
	else
	{
		temp = dw_lenth;
	}
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 6);
	msleep(dw_lenth/256);

	for(i = 0;i < 100;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = reg_val[1] = 0x00;
		M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 2);

		if (0xF0==reg_val[0] && 0x55==reg_val[1])
		{
			break;
		}
		msleep(1);

	}
	//----------------------------------------------------------------------
	if (dw_lenth > FTS_LEN_FLASH_ECC_MAX)
	{
		temp = FTS_LEN_FLASH_ECC_MAX;
		auc_i2c_write_buf[0] = 0x65;
		auc_i2c_write_buf[1] = (u8)(temp >> 16);
		auc_i2c_write_buf[2] = (u8)(temp >> 8);
		auc_i2c_write_buf[3] = (u8)(temp);
		temp = dw_lenth-FTS_LEN_FLASH_ECC_MAX;
		auc_i2c_write_buf[4] = (u8)(temp >> 8);
		auc_i2c_write_buf[5] = (u8)(temp);
		i_ret = M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 6);

		msleep(dw_lenth/256);

		for(i = 0;i < 100;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 2);

			if (0xF0==reg_val[0] && 0x55==reg_val[1])
			{
				break;
			}
			msleep(1);

		}
	}
	auc_i2c_write_buf[0] = 0x66;
	M_DIRECT_READ_FUNC(fc_p, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc)
	{
		return -EIO;
	}
	/********* Step 7: reset the new FW ***********************/
	auc_i2c_write_buf[0] = 0x07;
	M_DIRECT_WRITE_FUNC(fc_p, auc_i2c_write_buf, 1);
//	msleep(200);
	msleep(300);

	return 0;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_device_status(struct shtps_fts *ts_p, u8 *status_p)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	int rc=0;
	u8 buf;

	rc = M_READ_FUNC(fc_p, FTS_REG_STATUS, &buf, 1);
	if(rc == 0){
		*status_p = buf;
	}
	else{
		*status_p = 0;
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_set_active(struct shtps_fts *ts_p)
{
	shtps_fwctl_set_dev_state(ts_p, SHTPS_DEV_STATE_ACTIVE);
	return 0;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_set_sleepmode_on(struct shtps_fts *ts_p)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	M_WRITE_FUNC(fc_p, FTS_REG_PMODE, FTS_PMODE_HIBERNATE);
	shtps_fwctl_set_dev_state(ts_p, SHTPS_DEV_STATE_SLEEP);
	return 0;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_set_sleepmode_off(struct shtps_fts *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_fingermax(struct shtps_fts *ts_p)
{
	return SHTPS_FINGER_MAX;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_fingerinfo(struct shtps_fts *ts_p, u8 *buf_p, int read_cnt, u8 *irqsts_p, u8 *extsts_p, u8 **finger_pp)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	buf_p[0] = 0x00;
	buf_p[1] = 0x00;
	M_READ_PACKET_FUNC(fc_p, 0x02, &buf_p[2], 1);
	if(buf_p[2] < FTS_MAX_POINTS){
		M_READ_PACKET_FUNC(fc_p, 0x03, &buf_p[3], buf_p[2]*FTS_ONE_TCH_LEN);
	}
	else{
		buf_p[2] = 0x00;
	}
	*finger_pp = &buf_p[0];
	return 0;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_num_of_touch_fingers(struct shtps_fts *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_POINT_NUM];
}

/* -------------------------------------------------------------------------- */
u8* shtps_fwctl_get_finger_info_buf(struct shtps_fts *ts_p, int fingerid, int fingerMax, u8 *buf_p)
{
	return &buf_p[FTS_TCH_LEN(fingerid)];
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_state(struct shtps_fts *ts_p, int fingerid, int fingerMax, u8 *buf_p)
{
	u8 event = buf_p[FTS_TOUCH_EVENT_POS] >> 6;
	if(event == FTS_TOUCH_DOWN || event == FTS_TOUCH_CONTACT){
		return SHTPS_TOUCH_STATE_FINGER;
	}
	return SHTPS_TOUCH_STATE_NO_TOUCH;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_pointid(struct shtps_fts *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_ID_POS] >> 4;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_pos_x(struct shtps_fts *ts_p, u8 *buf_p)
{
	return (s16) (buf_p[FTS_TOUCH_X_H_POS] & 0x0F) << 8
	     | (s16) buf_p[FTS_TOUCH_X_L_POS];
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_pos_y(struct shtps_fts *ts_p, u8 *buf_p)
{
	return (s16) (buf_p[FTS_TOUCH_Y_H_POS] & 0x0F) << 8
	     | (s16) buf_p[FTS_TOUCH_Y_L_POS];
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_wx(struct shtps_fts *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_AREA] >> 4;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_wy(struct shtps_fts *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_AREA] >> 4;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_finger_z(struct shtps_fts *ts_p, u8 *buf_p)
{
	u8 weight = buf_p[FTS_TOUCH_WEIGHT];
	if(weight == 0){
		return FTS_PRESS;
	}
	return weight;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_fwver(struct shtps_fts *ts_p, u16 *ver_p)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	int rc=0;
	u8 buf[2] = { 0x00, 0x00 };

	*ver_p = 0;

	rc = M_READ_FUNC(fc_p, FTS_REG_FW_VER, &buf[0], 1);
	if(rc == 0){
		rc = M_READ_FUNC(fc_p, FTS_REG_MODEL_VER, &buf[1], 1);
		if(rc == 0){
			*ver_p = (buf[1] << 8) + buf[0];
		}
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_initparam(struct shtps_fts *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
void shtps_fwctl_set_dev_state(struct shtps_fts *ts_p, u8 state)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	fc_p->dev_state = state;
}

/* -------------------------------------------------------------------------- */
u8 shtps_fwctl_get_dev_state(struct shtps_fts *ts_p)
{
	struct shtps_fwctl_info *fc_p = ts_p->fwctl_p;
	return fc_p->dev_state;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_maxXPosition(struct shtps_fts *ts_p)
{
	return CONFIG_SHTPS_LCD_SIZE_X;
}

/* -------------------------------------------------------------------------- */
int shtps_fwctl_get_maxYPosition(struct shtps_fts *ts_p)
{
	return CONFIG_SHTPS_LCD_SIZE_Y;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
int shtps_fwctl_init(struct shtps_fts *ts_p, void *tps_ctrl_p, struct shtps_ctrl_functbl *func_p)
{
	ts_p->fwctl_p = kzalloc(sizeof(struct shtps_fwctl_info), GFP_KERNEL);
	if(ts_p->fwctl_p == NULL){
		return -ENOMEM;
	}
	ts_p->fwctl_p->devctrl_func_p = func_p;	/* func.table i2c */
	ts_p->fwctl_p->tps_ctrl_p = tps_ctrl_p;	/* struct device */

	shtps_fwctl_ic_init(ts_p);

	return 0;
}

/* -------------------------------------------------------------------------- */
void shtps_fwctl_deinit(struct shtps_fts *ts_p)
{
	if(ts_p->fwctl_p)	kfree(ts_p->fwctl_p);
	ts_p->fwctl_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
dev_t 					shtpsif_devid;
struct class*			shtpsif_class = NULL;
struct device*			shtpsif_device = NULL;
struct cdev 			shtpsif_cdev;

/* -----------------------------------------------------------------------------------
 */
static int shtps_copy_to_user(u8 *krnl, unsigned long arg, int size, int isCompat)
{
#ifdef CONFIG_COMPAT
	if(isCompat == 1){
		if(copy_to_user(compat_ptr(arg), krnl, size)){
			return -EFAULT;
		}
		return 0;
	}
	else
#endif /* CONFIG_COMPAT */
	{
		if(copy_to_user((u8*)arg, krnl, size)){
			return -EFAULT;
		}
		return 0;
	}
}

static int shtps_fw_upgrade_copy_from_user(struct shtps_ioctl_param *krnl, unsigned long arg, int isCompat)
{
#ifdef CONFIG_COMPAT
	if(isCompat == 1)
	{
		int err = 0;
		int size = 0;
		struct shtps_compat_ioctl_param *usr =
			(struct shtps_compat_ioctl_param __user *) arg;

		if(!usr){
			return -EINVAL;
		}

		err |= get_user(krnl->size, &usr->size);
		if(!err){
			size = krnl->size;
			if((size <= 0) || (!usr->data)){
				krnl->data = NULL;
				krnl->size = 0;
			}else{
				krnl->data = (u8*)kmalloc(size, GFP_KERNEL);
				if(!krnl->data){
					return -ENOMEM;
				}

				if(copy_from_user(krnl->data, compat_ptr(usr->data), size)){
					kfree(krnl->data);
					return -EFAULT;
				}
			}
		}

		return err;
	}
	else
#endif /* CONFIG_COMPAT */
	{
		int err = 0;
		int size = 0;
		struct shtps_ioctl_param *usr =
			(struct shtps_ioctl_param __user *) arg;

		if(!usr){
			return -EINVAL;
		}

		err |= get_user(krnl->size, &usr->size);
		if(!err){
			size = krnl->size;
			if((size <= 0) || (!usr->data)){
				krnl->data = NULL;
				krnl->size = 0;
			}else{
				krnl->data = (u8*)kmalloc(size, GFP_KERNEL);
				if(!krnl->data){
					return -ENOMEM;
				}

				if(copy_from_user(krnl->data, usr->data, size)){
					kfree(krnl->data);
					return -EFAULT;
				}
			}
		}

		return err;
	}
}

static void shtps_fw_upgrade_release(struct shtps_ioctl_param *krnl)
{
	if(krnl->data){
		kfree(krnl->data);
	}
}

static int shtps_ioctl_getver(struct shtps_fts *ts, unsigned long arg, int isCompat)
{
	u8  tps_stop_request = 0;
	u16 ver;

	if(0 == arg){
		return -EINVAL;
	}

	if(ts->state_mgr.state == SHTPS_STATE_IDLE){
		tps_stop_request = 1;
	}

	if(0 != shtps_start(ts)){
		return -EFAULT;
	}

	shtps_mutex_lock_ctrl();

	ver = shtps_fwver(ts);

	shtps_mutex_unlock_ctrl();

	if(tps_stop_request){
		shtps_shutdown(ts);
	}

	if(shtps_copy_to_user((u8*)&ver, arg, sizeof(ver), isCompat)){
		return -EFAULT;
	}

	return 0;
}

static int shtps_ioctl_getver_builtin(struct shtps_fts *ts, unsigned long arg, int isCompat)
{
	u16 ver = shtps_fwver_builtin(ts);

	if(shtps_fwdata_builtin(ts) == NULL){
		return -EFAULT;
	}
	if(0 == arg){
		return -EINVAL;
	}

	if(shtps_copy_to_user((u8*)&ver, arg, sizeof(ver), isCompat)){
		return -EFAULT;
	}
	return 0;
}

static int shtps_ioctl_fw_upgrade(struct shtps_fts *ts, unsigned long arg, int isCompat)
{
	int rc;
	struct shtps_ioctl_param param;
	const unsigned char* fw_data = NULL;

	if(0 == arg || 0 != shtps_fw_upgrade_copy_from_user(&param, arg, isCompat)){
		return -EINVAL;
	}

	if(param.size == 0){
		fw_data = shtps_fwdata_builtin(ts);
		if(fw_data){
			rc = shtps_fw_update(ts, (u8*)fw_data, shtps_fwsize_builtin(ts));
		}
		else{
			rc = -1;
		}
	}
	else{
		rc = shtps_fw_update(ts, param.data, param.size);
	}

	if(ts->is_lcd_on == 0){
		shtps_shutdown(ts);
	}

	shtps_fw_upgrade_release(&param);
	if(rc){
		return -EFAULT;
	}

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtpsif_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int shtpsif_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long shtpsif_ioctl_cmd(struct shtps_fts *ts, unsigned int cmd, unsigned long arg, int isCompat)
{
	int	rc = 0;

	switch(cmd){
	case TPSDEV_GET_FW_VERSION:			rc = shtps_ioctl_getver(ts, arg, isCompat);		break;
	case TPSDEV_GET_FW_VERSION_BUILTIN:	rc = shtps_ioctl_getver_builtin(ts, arg, isCompat);	break;
	case TPSDEV_FW_UPGRADE:				rc = shtps_ioctl_fw_upgrade(ts, arg, isCompat);	break;

	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

static long shtpsif_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	struct shtps_fts *ts = gShtps_fts;
	int isCompat = 0;

	if(ts == NULL){
		return -EFAULT;
	}

	rc = shtpsif_ioctl_cmd(ts, cmd, arg, isCompat);

	return rc;
}

#ifdef CONFIG_COMPAT
static long shtpsif_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	struct shtps_fts *ts = gShtps_fts;
	int isCompat = 1;

	if(ts == NULL){
		return -EFAULT;
	}

	switch(cmd){
	case COMPAT_TPSDEV_FW_UPGRADE:				cmd = TPSDEV_FW_UPGRADE;			break;
	}

	rc = shtpsif_ioctl_cmd(ts, cmd, arg, isCompat);

	return rc;
}
#endif /* CONFIG_COMPAT */

static const struct file_operations shtpsif_fileops = {
	.owner   = THIS_MODULE,
	.open    = shtpsif_open,
	.release = shtpsif_release,
	.unlocked_ioctl   = shtpsif_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =	shtpsif_compat_ioctl,
#endif /* CONFIG_COMPAT */
};

static int shtpsif_init(void)
{
	int rc;

	rc = alloc_chrdev_region(&shtpsif_devid, 0, 1, SH_TOUCH_IF_DEVNAME);
	if(rc < 0){
		return rc;
	}

	shtpsif_class = class_create(THIS_MODULE, SH_TOUCH_IF_DEVNAME);
	if (IS_ERR(shtpsif_class)) {
		rc = PTR_ERR(shtpsif_class);
		goto error_vid_class_create;
	}

	shtpsif_device = device_create(shtpsif_class, NULL,
								shtpsif_devid, &shtpsif_cdev,
								SH_TOUCH_IF_DEVNAME);
	if (IS_ERR(shtpsif_device)) {
		rc = PTR_ERR(shtpsif_device);
		goto error_vid_class_device_create;
	}

	cdev_init(&shtpsif_cdev, &shtpsif_fileops);
	shtpsif_cdev.owner = THIS_MODULE;
	rc = cdev_add(&shtpsif_cdev, shtpsif_devid, 1);
	if(rc < 0){
		goto err_via_cdev_add;
	}

	return 0;

err_via_cdev_add:
	cdev_del(&shtpsif_cdev);
error_vid_class_device_create:
	class_destroy(shtpsif_class);
	shtpsif_class = NULL;
error_vid_class_create:
	unregister_chrdev_region(shtpsif_devid, 1);

	return rc;
}

static void shtpsif_exit(void)
{
	if(shtpsif_class != NULL){
		cdev_del(&shtpsif_cdev);
		class_destroy(shtpsif_class);
		shtpsif_class = NULL;
		unregister_chrdev_region(shtpsif_devid, 1);
	}
}

/* -------------------------------------------------------------------------- */
static DEFINE_MUTEX(i2c_rw_access);

static int shtps_i2c_write_main(void *client, u16 addr, u8 *writebuf, u32 size);
static int shtps_i2c_write(void *client, u16 addr, u8 data);
static int shtps_i2c_write_packet(void *client, u16 addr, u8 *data, u32 size);

static int shtps_i2c_read(void *client, u16 addr, u8 *buf, u32 size);
static int shtps_i2c_read_packet (void *client, u16 addr, u8 *buf,  u32 size);

/* -----------------------------------------------------------------------------------
 */
static int shtps_i2c_transfer(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret;

	mutex_lock(&i2c_rw_access);

	if(writelen > 0){
		if(readlen > 0){
			struct i2c_msg msgs[] = {
				{
					.addr = client->addr,
					.flags = 0,
					.len = writelen,
					.buf = writebuf,
				},
				{
					.addr = client->addr,
					.flags = I2C_M_RD,
					.len = readlen,
					.buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
		}
		else{
			struct i2c_msg msgs[] = {
				{
					.addr = client->addr,
					.flags = 0,
					.len = writelen,
					.buf = writebuf,
				},
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
		}
	}
	else{
		struct i2c_msg msgs[] = {
			{
				.addr = client->addr,
				.flags = I2C_M_RD,
				.len = readlen,
				.buf = readbuf,
			},
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
	}

	mutex_unlock(&i2c_rw_access);

	return ret;
}
/* -----------------------------------------------------------------------------------
 */
static int shtps_i2c_write_main(void *client, u16 addr, u8 *writebuf, u32 size)
{
	int ret = 0;
	u8 *buf;
	u8 buf_local[1 + SHTPS_I2C_BLOCKWRITE_BUFSIZE];
	int allocSize = 0;

	if(size > SHTPS_I2C_BLOCKWRITE_BUFSIZE){
		allocSize = sizeof(u8) * (1 + size);
		buf = (u8 *)kzalloc(allocSize, GFP_KERNEL);
		if(buf == NULL){
			return -ENOMEM;
		}
	}
	else{
		buf = buf_local;
	}

	buf[0] = addr;
	memcpy(&buf[1], writebuf, size);

	ret = shtps_i2c_transfer((struct i2c_client *)client, buf, (1 + size), NULL, 0);
	if(ret >= 0){
		ret = 0;
	}

	if(allocSize > 0){
		kfree(buf);
	}
	return ret;
}

static int shtps_i2c_write(void *client, u16 addr, u8 data)
{
	return shtps_i2c_write_packet(client, addr, &data, 1);
}

static int shtps_i2c_write_packet(void *client, u16 addr, u8 *data, u32 size)
{
	int status = 0;
	int retry = SHTPS_I2C_RETRY_COUNT;

	do{
		status = shtps_i2c_write_main(client,
			addr,
			data,
			size);
		//
		if(status){
			struct timespec tu;
			tu.tv_sec = (time_t)0;
			tu.tv_nsec = SHTPS_I2C_RETRY_WAIT * 1000000;
			hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
		}
	}while(status != 0 && retry-- > 0);

	return status;
}

static int shtps_i2c_read_main(void *client, u16 addr, u8 *readbuf, u32 size)
{
	int ret;

	if(addr != 0x7FFF){
		u8 writebuf[1];

		writebuf[0] = addr;

		ret = shtps_i2c_transfer((struct i2c_client *)client, writebuf, 1, readbuf, size);
		if(ret >= 0){
			ret = 0;
		}
	}
	else{
		ret = shtps_i2c_transfer((struct i2c_client *)client, NULL, 0, readbuf, size);
		if(ret >= 0){
			ret = 0;
		}
	}

	return ret;
}

static int shtps_i2c_read(void *client, u16 addr, u8 *buf, u32 size)
{
	return shtps_i2c_read_packet(client, addr, buf, size);
}

static int shtps_i2c_read_packet(void *client, u16 addr, u8 *buf, u32 size)
{
	int status = 0;
	int i;
	u32 s;
	int retry = SHTPS_I2C_RETRY_COUNT;

	do{
		s = size;
		for(i = 0; i < size; i += SHTPS_I2C_BLOCKREAD_BUFSIZE){
			status = shtps_i2c_read_main(client,
				addr,
				buf + i,
				(s > SHTPS_I2C_BLOCKREAD_BUFSIZE) ? (SHTPS_I2C_BLOCKREAD_BUFSIZE) : (s));
			//
			if(status){
				struct timespec tu;
				tu.tv_sec = (time_t)0;
				tu.tv_nsec = SHTPS_I2C_RETRY_WAIT * 1000000;
				hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
				break;
			}
			s -= SHTPS_I2C_BLOCKREAD_BUFSIZE;
			addr = 0x7FFF;	/* specifying no address after first read */
		}
	}while(status != 0 && retry-- > 0);

	return status;
}

static int shtps_i2c_direct_write(void *client, u8 *writebuf, u32 writelen)
{
	int ret = 0;

	if (writelen > 0) {
		ret = shtps_i2c_transfer((struct i2c_client *)client, writebuf, writelen, NULL, 0);
	}
	return ret;
}

static int shtps_i2c_direct_read(void *client, u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
{
	int ret;

	if (writelen > 0) {
		ret = shtps_i2c_transfer((struct i2c_client *)client, writebuf, writelen, readbuf, readlen);
	} else {
		ret = shtps_i2c_transfer((struct i2c_client *)client, NULL, 0, readbuf, readlen);
	}

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static const struct shtps_ctrl_functbl TpsCtrl_I2cFuncTbl = {
	.write_f          = shtps_i2c_write,
	.read_f           = shtps_i2c_read,
	.packet_write_f   = shtps_i2c_write_packet,
	.packet_read_f    = shtps_i2c_read_packet,
	.direct_write_f   = shtps_i2c_direct_write,
	.direct_read_f    = shtps_i2c_direct_read,
};

/* -----------------------------------------------------------------------------------
*/
static int shtps_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	extern	int shtps_fts_core_probe( struct device *ctrl_dev_p,
									struct shtps_ctrl_functbl *func_p,
									void *ctrl_p,
									char *modalias,
									int gpio_irq);
	int result;

	/* ---------------------------------------------------------------- */
	result = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (result == 0) {
		goto fail_err;
	}

	/* ---------------------------------------------------------------- */
	result = shtps_fts_core_probe(
							&client->dev,
							(struct shtps_ctrl_functbl*)&TpsCtrl_I2cFuncTbl,
							(void *)client,
							client->name,
							client->irq );
	if(result){
		goto fail_err;
	}

	/* ---------------------------------------------------------------- */

	return 0;
	/* ---------------------------------------------------------------- */


fail_err:
	return result;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_i2c_remove(struct i2c_client *client)
{
	extern int shtps_fts_core_remove(struct shtps_fts *, struct device *);
	struct shtps_fts *ts_p = i2c_get_clientdata(client);
	int ret = shtps_fts_core_remove(ts_p, &client->dev);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	extern int shtps_fts_core_suspend(struct shtps_fts *, pm_message_t);
	struct shtps_fts *ts_p = i2c_get_clientdata(client);
	int ret = shtps_fts_core_suspend(ts_p, mesg);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_i2c_resume(struct i2c_client *client)
{
	extern int shtps_fts_core_resume(struct shtps_fts *);
	struct shtps_fts *ts_p = i2c_get_clientdata(client);
	int ret = shtps_fts_core_resume(ts_p);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static const struct dev_pm_ops shtps_fts_ts_pm_ops = {
};
static const struct i2c_device_id shtps_fts_ts_id[] = {
	{"fts_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, shtps_fts_ts_id);

#ifdef CONFIG_OF // Open firmware must be defined for dts useage
static struct of_device_id shtps_fts_table[] = {
	{ . compatible = "sharp,shtps_fts" ,}, // Compatible node must match dts
	{ },
};
#endif

static struct i2c_driver shtps_i2c_driver = {
	.probe		= shtps_i2c_probe,
	.remove		= shtps_i2c_remove,
	.suspend	= shtps_i2c_suspend,
	.resume		= shtps_i2c_resume,
	.driver		= {
		#ifdef CONFIG_OF // Open firmware must be defined for dts useage
			.of_match_table = shtps_fts_table,
		#endif
		.name = "shtps_fts",	// SH_TOUCH_DEVNAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &shtps_fts_ts_pm_ops,
#endif
	},
	.id_table = shtps_fts_ts_id,
};

/* -----------------------------------------------------------------------------------
*/
static int __init shtps_i2c_init(void)
{
	return i2c_add_driver(&shtps_i2c_driver);
}
module_init(shtps_i2c_init);

/* -----------------------------------------------------------------------------------
*/
static void __exit shtps_i2c_exit(void)
{
	i2c_del_driver(&shtps_i2c_driver);
}
module_exit(shtps_i2c_exit);

/* -----------------------------------------------------------------------------------
*/
void msm_tps_setsleep(int on)
{
	struct shtps_fts *ts = gShtps_fts;

	if(ts){
		if(on == 1){
			ts->is_lcd_on = 0;
		}
		else{
			ts->is_lcd_on = 1;
		}

		if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_I2C_PROC_SETSLEEP, (u8)on) == 0){
			shtps_setsleep_proc(ts, (u8)on);
		}
	}
}
EXPORT_SYMBOL(msm_tps_setsleep);


/* -----------------------------------------------------------------------------------
*/
MODULE_DESCRIPTION("SHARP TOUCHPANEL DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
