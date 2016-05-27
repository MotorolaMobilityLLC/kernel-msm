/*
 * FocalTech ft8607 TouchScreen driver.
 *
 * Copyright (c) 2016  Focal tech Ltd.
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

#include "ft8607_ts.h"

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(fts_ctrl_lock);
static DEFINE_MUTEX(fts_loader_lock);
static DEFINE_MUTEX(fts_proc_lock);

/* -----------------------------------------------------------------------------------
 */
struct fts_data*	gFts_data = NULL;

/* -----------------------------------------------------------------------------------
 */
typedef int (fts_state_func)(struct fts_data *ts, int param);
struct fts_state_func {
	fts_state_func	*enter;
	fts_state_func	*start;
	fts_state_func	*stop;
	fts_state_func	*sleep;
	fts_state_func	*wakeup;
	fts_state_func	*start_ldr;
	fts_state_func	*interrupt;
};

/* -----------------------------------------------------------------------------------
 */
static int ftsif_init(void);
static void ftsif_exit(void);

static void fts_irq_proc(struct fts_data *ts, u8 dummy);
static void fts_setsleep_proc(struct fts_data *ts, u8 sleep);
static void fts_async_open_proc(struct fts_data *ts, u8 dummy);
static void fts_async_close_proc(struct fts_data *ts, u8 dummy);
static void fts_async_enable_proc(struct fts_data *ts, u8 dummy);
static void fts_suspend_i2c_wake_lock(struct fts_data *ts, u8 lock);
static void fts_exec_suspend_pending_proc(struct fts_data *ts);
static void fts_deter_suspend_i2c_pending_proc_work_function(struct work_struct *work);

static irqreturn_t fts_irq_handler(int irq, void *dev_id);
static irqreturn_t fts_irq(int irq, void *dev_id);

static int fts_init_param(struct fts_data *ts);
static void fts_standby_param(struct fts_data *ts);
static void fts_set_touch_info(struct fts_data *ts, u8 *buf, struct fts_touch_info *info);
static void fts_calc_notify(struct fts_data *ts, u8 *buf, struct fts_touch_info *info, u8 *event);

static int fts_irq_resuest(struct fts_data *ts);

static int state_change(struct fts_data *ts, int state);
static int fts_statef_nop(struct fts_data *ts, int param);
static int fts_statef_cmn_stop(struct fts_data *ts, int param);
static int fts_statef_idle_start(struct fts_data *ts, int param);
static int fts_statef_idle_wakeup(struct fts_data *ts, int param);
static int fts_statef_idle_start_ldr(struct fts_data *ts, int param);
static int fts_statef_idle_int(struct fts_data *ts, int param);
static int fts_statef_active_enter(struct fts_data *ts, int param);
static int fts_statef_active_stop(struct fts_data *ts, int param);
static int fts_statef_active_sleep(struct fts_data *ts, int param);
static int fts_statef_active_start_ldr(struct fts_data *ts, int param);
static int fts_statef_active_int(struct fts_data *ts, int param);
static int fts_statef_loader_enter(struct fts_data *ts, int param);
static int fts_statef_loader_start(struct fts_data *ts, int param);
static int fts_statef_loader_stop(struct fts_data *ts, int param);
static int fts_statef_loader_int(struct fts_data *ts, int param);
static int fts_statef_sleep_enter(struct fts_data *ts, int param);
static int fts_statef_sleep_wakeup(struct fts_data *ts, int param);
static int fts_statef_sleep_start_ldr(struct fts_data *ts, int param);
static int fts_statef_sleep_int(struct fts_data *ts, int param);
static int fts_open(struct input_dev *dev);
static void fts_close(struct input_dev *dev);
static int fts_init_internal_variables(struct fts_data *ts);
static void fts_deinit_internal_variables(struct fts_data *ts);
static int fts_init_inputdev(struct fts_data *ts, struct device *ctrl_dev_p, char *modalias);
static void fts_deinit_inputdev(struct fts_data *ts);

/* -----------------------------------------------------------------------------------
 */
const static struct fts_state_func state_idle = {
    .enter          = fts_statef_nop,
    .start          = fts_statef_idle_start,
    .stop           = fts_statef_nop,
    .sleep          = fts_statef_nop,
    .wakeup         = fts_statef_idle_wakeup,
    .start_ldr      = fts_statef_idle_start_ldr,
    .interrupt      = fts_statef_idle_int,
};

const static struct fts_state_func state_active = {
    .enter          = fts_statef_active_enter,
    .start          = fts_statef_nop,
    .stop           = fts_statef_active_stop,
    .sleep          = fts_statef_active_sleep,
    .wakeup         = fts_statef_nop,
    .start_ldr      = fts_statef_active_start_ldr,
    .interrupt      = fts_statef_active_int,
};

const static struct fts_state_func state_loader = {
    .enter          = fts_statef_loader_enter,
    .start          = fts_statef_loader_start,
    .stop           = fts_statef_loader_stop,
    .sleep          = fts_statef_nop,
    .wakeup         = fts_statef_nop,
    .start_ldr      = fts_statef_nop,
    .interrupt      = fts_statef_loader_int,
};

const static struct fts_state_func state_sleep = {
    .enter          = fts_statef_sleep_enter,
    .start          = fts_statef_nop,
    .stop           = fts_statef_cmn_stop,
    .sleep          = fts_statef_nop,
    .wakeup         = fts_statef_sleep_wakeup,
    .start_ldr      = fts_statef_sleep_start_ldr,
    .interrupt      = fts_statef_sleep_int,
};

const static struct fts_state_func *state_func_tbl[] = {
	&state_idle,
	&state_active,
	&state_loader,
	&state_sleep,
};

typedef void (*fts_deter_suspend_i2c_pending_func_t)(struct fts_data*, u8);
static const fts_deter_suspend_i2c_pending_func_t FTS_SUSPEND_PENDING_FUNC_TBL[] = {
	fts_irq_proc,						/**< FTS_DETER_SUSPEND_I2C_PROC_IRQ */
	fts_setsleep_proc,					/**< FTS_DETER_SUSPEND_I2C_PROC_SETSLEEP */
	fts_async_open_proc,				/**< FTS_DETER_SUSPEND_I2C_PROC_OPEN */
	fts_async_close_proc,				/**< FTS_DETER_SUSPEND_I2C_PROC_CLOSE */
	fts_async_enable_proc,				/**< FTS_DETER_SUSPEND_I2C_PROC_ENABLE */
};

/* -----------------------------------------------------------------------------------
 */
int fts_device_setup(int irq, int rst)
{
	int rc = 0;

	rc = gpio_request(rst, "fts_rst");
	if (rc) {
		return rc;
	}

	return 0;
}

void fts_device_teardown(int irq, int rst)
{
	gpio_free(rst);
}

void fts_device_reset(int rst)
{
	gpio_set_value(rst, 0);
	mb();
	mdelay(FTS_HWRESET_TIME_MS);

	gpio_set_value(rst, 1);
	mb();
	mdelay(FTS_HWRESET_AFTER_TIME_MS);
}

void fts_device_poweroff_reset(int rst)
{
	gpio_set_value(rst, 0);
	mb();
	mdelay(FTS_HWRESET_TIME_MS);
}

/* -----------------------------------------------------------------------------------
 */
void fts_mutex_lock_ctrl(void)
{
	mutex_lock(&fts_ctrl_lock);
}

void fts_mutex_unlock_ctrl(void)
{
	mutex_unlock(&fts_ctrl_lock);
}

void fts_mutex_lock_loader(void)
{
	mutex_lock(&fts_loader_lock);
}

void fts_mutex_unlock_loader(void)
{
	mutex_unlock(&fts_loader_lock);
}

void fts_mutex_lock_proc(void)
{
	mutex_lock(&fts_proc_lock);
}

void fts_mutex_unlock_proc(void)
{
	mutex_unlock(&fts_proc_lock);
}

/* -----------------------------------------------------------------------------------
 */
void fts_system_set_sleep(struct fts_data *ts)
{
	fts_device_poweroff_reset(ts->rst_pin);
}

void fts_system_set_wakeup(struct fts_data *ts)
{
}

/* -----------------------------------------------------------------------------------
 */
static void fts_irq_proc(struct fts_data *ts, u8 dummy)
{
	fts_wake_lock_idle(ts);

	request_event(ts, FTS_EVENT_INTERRUPT, 1);

	fts_wake_unlock_idle(ts);
}

static void fts_setsleep_proc(struct fts_data *ts, u8 sleep)
{
	if(sleep){
		fts_func_request_sync(ts, FTS_FUNC_REQ_EVEMT_LCD_OFF);
	}else{
		fts_func_request_sync(ts, FTS_FUNC_REQ_EVEMT_LCD_ON);

		if(ts->boot_fw_update_checked == 0){
			ts->boot_fw_update_checked = 1;
			fts_func_request_async(ts, FTS_FUNC_REQ_BOOT_FW_UPDATE);
		}
	}
}

static void fts_async_open_proc(struct fts_data *ts, u8 dummy)
{
	fts_func_request_async(ts, FTS_FUNC_REQ_EVEMT_OPEN);
}

static void fts_async_close_proc(struct fts_data *ts, u8 dummy)
{
	fts_func_request_async(ts, FTS_FUNC_REQ_EVEMT_CLOSE);
}

static void fts_async_enable_proc(struct fts_data *ts, u8 dummy)
{
	fts_func_request_async(ts, FTS_FUNC_REQ_EVEMT_ENABLE);
}

static void fts_suspend_i2c_wake_lock(struct fts_data *ts, u8 lock)
{
	if(lock){
		if(ts->deter_suspend_i2c.wake_lock_state == 0){
			ts->deter_suspend_i2c.wake_lock_state = 1;
			wake_lock(&ts->deter_suspend_i2c.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, FTS_QOS_LATENCY_DEF_VALUE);
		}
	}else{
		if(ts->deter_suspend_i2c.wake_lock_state == 1){
			ts->deter_suspend_i2c.wake_lock_state = 0;
			wake_unlock(&ts->deter_suspend_i2c.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
		}
	}
}

int fts_check_suspend_state(struct fts_data *ts, int proc, u8 param)
{
	int ret = 0;

	fts_mutex_lock_proc();
	if(ts->deter_suspend_i2c.suspend){
		fts_suspend_i2c_wake_lock(ts, 1);
		ts->deter_suspend_i2c.pending_info[proc].pending= 1;
		ts->deter_suspend_i2c.pending_info[proc].param  = param;
		ret = 1;
	}else{
		ts->deter_suspend_i2c.pending_info[proc].pending= 0;
		ret = 0;
	}

	if(proc == FTS_DETER_SUSPEND_I2C_PROC_OPEN ||
		proc == FTS_DETER_SUSPEND_I2C_PROC_ENABLE)
	{
		if(ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_CLOSE].pending){
			ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_CLOSE].pending = 0;
		}
	}else if(proc == FTS_DETER_SUSPEND_I2C_PROC_CLOSE){
		if(ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_OPEN].pending){
			ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_OPEN].pending  = 0;
		}
		if(ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_ENABLE].pending){
			ts->deter_suspend_i2c.pending_info[FTS_DETER_SUSPEND_I2C_PROC_ENABLE].pending= 0;
		}
	}
	fts_mutex_unlock_proc();
	return ret;
}

static void fts_exec_suspend_pending_proc(struct fts_data *ts)
{
	cancel_work_sync(&ts->deter_suspend_i2c.pending_proc_work);

	schedule_work(&ts->deter_suspend_i2c.pending_proc_work);
}

static void fts_deter_suspend_i2c_pending_proc_work_function(struct work_struct *work)
{
	struct fts_deter_suspend_i2c *dss = container_of(work, struct fts_deter_suspend_i2c, pending_proc_work);
	struct fts_data *ts = container_of(dss, struct fts_data, deter_suspend_i2c);
	int i;

	fts_mutex_lock_proc();

	for(i = 0;i < FTS_DETER_SUSPEND_I2C_PROC_NUM;i++){
		if(ts->deter_suspend_i2c.pending_info[i].pending){
			FTS_SUSPEND_PENDING_FUNC_TBL[i](ts, ts->deter_suspend_i2c.pending_info[i].param);
			ts->deter_suspend_i2c.pending_info[i].pending = 0;
		}
	}
	fts_suspend_i2c_wake_lock(ts, 0);

	fts_mutex_unlock_proc();
}

void fts_set_suspend_state(struct fts_data *ts)
{
	fts_mutex_lock_proc();
	ts->deter_suspend_i2c.suspend = 1;
	fts_mutex_unlock_proc();
}

void fts_clr_suspend_state(struct fts_data *ts)
{
	int i;
	int hold_process = 0;

	fts_mutex_lock_proc();
	ts->deter_suspend_i2c.suspend = 0;
	for(i = 0;i < FTS_DETER_SUSPEND_I2C_PROC_NUM;i++){
		if(ts->deter_suspend_i2c.pending_info[i].pending){
			hold_process = 1;
			break;
		}
	}

	if(hold_process){
		fts_exec_suspend_pending_proc(ts);
	}else{
		fts_suspend_i2c_wake_lock(ts, 0);
	}
	fts_mutex_unlock_proc();

	fts_mutex_lock_ctrl();
	if(ts->deter_suspend_i2c.suspend_irq_detect != 0){
		if(ts->deter_suspend_i2c.suspend_irq_state == FTS_IRQ_STATE_ENABLE){
			fts_irq_enable(ts);
		}

		ts->deter_suspend_i2c.suspend_irq_detect = 0;
	}
	fts_mutex_unlock_ctrl();
}

/* -----------------------------------------------------------------------------------
 */
static irqreturn_t fts_irq_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t fts_irq(int irq, void *dev_id)
{
	struct fts_data	*ts = dev_id;

	if(fts_check_suspend_state(ts, FTS_DETER_SUSPEND_I2C_PROC_IRQ, 0) == 0){
		fts_irq_proc(ts, 0);
	}else{
		fts_mutex_lock_ctrl();
		ts->deter_suspend_i2c.suspend_irq_state = ts->irq_mgr.state;
		ts->deter_suspend_i2c.suspend_irq_wake_state = ts->irq_mgr.wake;
		ts->deter_suspend_i2c.suspend_irq_detect = 1;
		fts_irq_disable(ts);
		fts_mutex_unlock_ctrl();
	}

	return IRQ_HANDLED;
}

/* -----------------------------------------------------------------------------------
 */
int fts_start(struct fts_data *ts)
{
	return request_event(ts, FTS_EVENT_START, 0);
}

void fts_shutdown(struct fts_data *ts)
{
	request_event(ts, FTS_EVENT_STOP, 0);
}

/* -----------------------------------------------------------------------------------
 */
void fts_reset(struct fts_data *ts)
{
	fts_irq_disable(ts);
	fts_device_reset(ts->rst_pin);
}

void fts_sleep(struct fts_data *ts, int on)
{
	if(on){
		fts_fwctl_set_sleepmode_on(ts);
	}else{
		fts_fwctl_set_sleepmode_off(ts);
	}
}

u16 fts_fwver(struct fts_data *ts)
{
	int rc;
	u16 ver = 0;
	u8 retry = 3;

	do{
		rc = fts_fwctl_get_fwver(ts, &ver);
		if(rc == 0)	break;
	}while(retry-- > 0);

	return ver;
}

u16 fts_fwver_builtin(struct fts_data *ts)
{
	u16 ver = 0;

	ver = FTS_FWVER_NEWER;

	return ver;
}

int fts_fwsize_builtin(struct fts_data *ts)
{
	int size = 0;

	size = FTS_FWSIZE_NEWER;

	return size;
}

unsigned char* fts_fwdata_builtin(struct fts_data *ts)
{
	unsigned char *data = NULL;

	data = (unsigned char *)fts_fw_data;

	return data;
}

static int fts_init_param(struct fts_data *ts)
{
	int rc;

	rc = fts_fwctl_initparam(ts);
	FTS_ERR_CHECK(rc, err_exit);

	return 0;

err_exit:
	return -1;
}

static void fts_standby_param(struct fts_data *ts)
{
	fts_sleep(ts, 1);
}

int fts_get_fingermax(struct fts_data *ts)
{
	return fts_fwctl_get_fingermax(ts);
}

/* -----------------------------------------------------------------------------------
 */
int fts_get_diff(unsigned short pos1, unsigned short pos2)
{
	int diff;
	diff = pos1 - pos2;
	return (diff >= 0)? diff : -diff;
}

int fts_get_fingerwidth(struct fts_data *ts, int num, struct fts_touch_info *info)
{
	int w = (info->fingers[num].wx >= info->fingers[num].wy)? info->fingers[num].wx : info->fingers[num].wy;

	return (w < 1)? 1 : w;
}

void fts_set_eventtype(u8 *event, u8 type)
{
	*event = type;
}

static void fts_set_touch_info(struct fts_data *ts, u8 *buf, struct fts_touch_info *info)
{
	u8* fingerInfo;
	int FingerNum = 0;
	int fingerMax = fts_get_fingermax(ts);
	int point_num = fts_fwctl_get_num_of_touch_fingers(ts, buf);
	int i;
	int pointid;

	memset(&ts->fw_report_info, 0, sizeof(ts->fw_report_info));
	memset(info, 0, sizeof(struct fts_touch_info));

	for(i = 0; i < point_num; i++){
		fingerInfo = fts_fwctl_get_finger_info_buf(ts, i, fingerMax, buf);
		pointid = fts_fwctl_get_finger_pointid(ts, fingerInfo);
		if (pointid >= fingerMax){
			break;
		}

		ts->fw_report_info.fingers[pointid].state	= fts_fwctl_get_finger_state(ts, i, fingerMax, buf);
		ts->fw_report_info.fingers[pointid].x		= fts_fwctl_get_finger_pos_x(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].y		= fts_fwctl_get_finger_pos_y(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].wx		= fts_fwctl_get_finger_wx(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].wy		= fts_fwctl_get_finger_wy(ts, fingerInfo);
		ts->fw_report_info.fingers[pointid].z		= fts_fwctl_get_finger_z(ts, fingerInfo);

		info->fingers[pointid].state	= ts->fw_report_info.fingers[pointid].state;
		info->fingers[pointid].x		= ts->fw_report_info.fingers[pointid].x;
		info->fingers[pointid].y		= ts->fw_report_info.fingers[pointid].y;
		info->fingers[pointid].wx		= ts->fw_report_info.fingers[pointid].wx;
		info->fingers[pointid].wy		= ts->fw_report_info.fingers[pointid].wy;
		info->fingers[pointid].z		= ts->fw_report_info.fingers[pointid].z;
	}

	for(i = 0; i < fingerMax; i++){
		if(ts->fw_report_info.fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
			FingerNum++;
		}
		else if(ts->fw_report_info_store.fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
		}
	}

	ts->fw_report_info.finger_num = FingerNum;
}


static void fts_calc_notify(struct fts_data *ts, u8 *buf, struct fts_touch_info *info, u8 *event)
{
	int i;
	int fingerMax = fts_get_fingermax(ts);
	int numOfFingers = 0;
	int diff_x;
	int diff_y;

	fts_set_eventtype(event, 0xff);
	fts_set_touch_info(ts, buf, info);

	for (i = 0; i < fingerMax; i++) {
		if(info->fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
			numOfFingers++;
		}
	}
	info->finger_num = numOfFingers;

	/* set event type */
	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
			if(ts->report_info.fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
				diff_x = fts_get_diff(info->fingers[i].x, ts->report_info.fingers[i].x);
				diff_y = fts_get_diff(info->fingers[i].y, ts->report_info.fingers[i].y);

				if((diff_x > 0) || (diff_y > 0)){
					fts_set_eventtype(event, FTS_EVENT_DRAG);
				}
			}
		}

		if(info->fingers[i].state != ts->report_info.fingers[i].state){
			fts_set_eventtype(event, FTS_EVENT_MTDU);
		}
	}

	if(numOfFingers > 0){
		if(ts->touch_state.numOfFingers == 0){
			fts_set_eventtype(event, FTS_EVENT_TD);
		}
	}else{
		if(ts->touch_state.numOfFingers != 0){
			fts_set_eventtype(event, FTS_EVENT_TU);
		}
	}
}


void fts_report_touch_on(struct fts_data *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	int lcd_x;
	int lcd_y;

	lcd_x = x;
	lcd_y = y;
	if(lcd_x >= CONFIG_FTS_LCD_SIZE_X){
		lcd_x = CONFIG_FTS_LCD_SIZE_X - 1;
	}
	if(lcd_y >= CONFIG_FTS_LCD_SIZE_Y){
		lcd_y = CONFIG_FTS_LCD_SIZE_Y - 1;
	}

	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
	input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input, ABS_MT_POSITION_X,  lcd_x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y,  lcd_y);
	input_report_abs(ts->input, ABS_MT_PRESSURE,    z);
}

void fts_report_touch_off(struct fts_data *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
}

void fts_event_report(struct fts_data *ts, struct fts_touch_info *info, u8 event)
{
	int	i;
	int fingerMax = fts_get_fingermax(ts);

	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state == FTS_TOUCH_STATE_FINGER){
			fts_report_touch_on(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  fts_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);

		}else if(ts->report_info.fingers[i].state != FTS_TOUCH_STATE_NO_TOUCH){
			fts_report_touch_off(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  fts_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);
		}
	}
	input_sync(ts->input);

	ts->touch_state.numOfFingers = info->finger_num;

	memcpy(&ts->report_info, info, sizeof(ts->report_info));
}

void fts_event_force_touchup(struct fts_data *ts)
{
	int	i;
	int isEvent = 0;
	int fingerMax = fts_get_fingermax(ts);

	for(i = 0;i < fingerMax;i++){
		if(ts->report_info.fingers[i].state != 0x00){
			isEvent = 1;
			fts_report_touch_off(ts, i,
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
void fts_irq_disable(struct fts_data *ts)
{
	if(ts->irq_mgr.state != FTS_IRQ_STATE_DISABLE){
		disable_irq_nosync(ts->irq_mgr.irq);
		ts->irq_mgr.state = FTS_IRQ_STATE_DISABLE;
	}

	if(ts->irq_mgr.wake != FTS_IRQ_WAKE_DISABLE){
		disable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = FTS_IRQ_WAKE_DISABLE;
	}
}

void fts_irq_enable(struct fts_data *ts)
{
	if(ts->irq_mgr.wake != FTS_IRQ_WAKE_ENABLE){
		enable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = FTS_IRQ_WAKE_ENABLE;
	}

	if(ts->irq_mgr.state != FTS_IRQ_STATE_ENABLE){
		enable_irq(ts->irq_mgr.irq);
		ts->irq_mgr.state = FTS_IRQ_STATE_ENABLE;
	}
}

static int fts_irq_resuest(struct fts_data *ts)
{
	int rc;

	rc = request_threaded_irq(ts->irq_mgr.irq,
						  fts_irq_handler,
						  fts_irq,
						  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						  FTS_TOUCH_DEVNAME,
						  ts);

	if(rc){
		return -1;
	}

	ts->irq_mgr.state = FTS_IRQ_STATE_ENABLE;
	ts->irq_mgr.wake  = FTS_IRQ_WAKE_DISABLE;
	fts_irq_disable(ts);
	return 0;
}

void fts_read_touchevent(struct fts_data *ts, int state)
{
	u8 buf[3 + (10 * 6)];
	u8 event;
	u8 irq_sts, ext_sts;
	u8 *pFingerInfoBuf;
	struct fts_touch_info info;

	memset(buf, 0, sizeof(buf));
	memset(&info, 0, sizeof(info));
	fts_fwctl_get_fingerinfo(ts, buf, 0, &irq_sts, &ext_sts, &pFingerInfoBuf);

	switch(state){
	case FTS_STATE_SLEEP:
	case FTS_STATE_IDLE:
		break;

	case FTS_STATE_ACTIVE:
	default:
		fts_calc_notify(ts, pFingerInfoBuf, &info, &event);

		if(event != 0xff){
			fts_event_report(ts, &info, event);
		}

		memcpy(&ts->fw_report_info_store, &ts->fw_report_info, sizeof(ts->fw_report_info));

		break;
	}
}

/* -----------------------------------------------------------------------------------
 */
int fts_enter_bootloader(struct fts_data *ts)
{
	fts_mutex_lock_loader();

	if(request_event(ts, FTS_EVENT_STARTLOADER, 0) != 0){
		goto err_exit;
	}
	fts_fwctl_set_dev_state(ts, FTS_DEV_STATE_LOADER);

	fts_mutex_unlock_loader();
	return 0;

err_exit:
	fts_mutex_unlock_loader();
	return -1;
}

static int fts_exit_bootloader(struct fts_data *ts)
{
	request_event(ts, FTS_EVENT_START, 0);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
int request_event(struct fts_data *ts, int event, int param)
{
	int ret;

	fts_mutex_lock_ctrl();

	switch(event){
	case FTS_EVENT_START:
		ret = state_func_tbl[ts->state_mgr.state]->start(ts, param);
		break;
	case FTS_EVENT_STOP:
		ret = state_func_tbl[ts->state_mgr.state]->stop(ts, param);
		break;
	case FTS_EVENT_SLEEP:
		ret = state_func_tbl[ts->state_mgr.state]->sleep(ts, param);
		break;
	case FTS_EVENT_WAKEUP:
		ret = state_func_tbl[ts->state_mgr.state]->wakeup(ts, param);
		break;
	case FTS_EVENT_STARTLOADER:
		ret = state_func_tbl[ts->state_mgr.state]->start_ldr(ts, param);
		break;
	case FTS_EVENT_INTERRUPT:
		ret = state_func_tbl[ts->state_mgr.state]->interrupt(ts, param);
		break;
	default:
		ret = -1;
		break;
	}

	fts_mutex_unlock_ctrl();

	return ret;
}

static int state_change(struct fts_data *ts, int state)
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
static int fts_statef_nop(struct fts_data *ts, int param)
{
	return 0;
}

static int fts_statef_cmn_stop(struct fts_data *ts, int param)
{
	fts_standby_param(ts);
	fts_irq_disable(ts);
	fts_system_set_sleep(ts);
	state_change(ts, FTS_STATE_IDLE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int fts_statef_idle_start(struct fts_data *ts, int param)
{
	u8 buf;

	fts_system_set_wakeup(ts);
	fts_reset(ts);

	fts_fwctl_get_device_status(ts, &buf);
	if(buf == FTS_REGVAL_STATUS_CRC_ERROR){
		fts_reset(ts);
	}

	state_change(ts, FTS_STATE_ACTIVE);

	return 0;
}

static int fts_statef_idle_start_ldr(struct fts_data *ts, int param)
{
	fts_system_set_wakeup(ts);
	fts_reset(ts);

	state_change(ts, FTS_STATE_BOOTLOADER);

	return 0;
}

static int fts_statef_idle_wakeup(struct fts_data *ts, int param)
{
	fts_statef_idle_start(ts, param);
	return 0;
}

static int fts_statef_idle_int(struct fts_data *ts, int param)
{
	fts_read_touchevent(ts, FTS_STATE_IDLE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int fts_statef_active_enter(struct fts_data *ts, int param)
{
	if((param == FTS_STATE_IDLE) || (param == FTS_STATE_SLEEP) || (param == FTS_STATE_BOOTLOADER)){
		fts_init_param(ts);
		fts_irq_enable(ts);
	}

	return 0;
}

static int fts_statef_active_stop(struct fts_data *ts, int param)
{
	fts_irq_disable(ts);

	return fts_statef_cmn_stop(ts, param);
}

static int fts_statef_active_sleep(struct fts_data *ts, int param)
{
	state_change(ts, FTS_STATE_SLEEP);
	return 0;
}

static int fts_statef_active_start_ldr(struct fts_data *ts, int param)
{
	state_change(ts, FTS_STATE_BOOTLOADER);
	return 0;
}

static int fts_statef_active_int(struct fts_data *ts, int param)
{
	fts_read_touchevent(ts, FTS_STATE_ACTIVE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int fts_statef_loader_enter(struct fts_data *ts, int param)
{
	fts_wake_lock_for_fwupdate(ts);

	fts_irq_disable(ts);

	return 0;
}

static int fts_statef_loader_start(struct fts_data *ts, int param)
{
	fts_wake_unlock_for_fwupdate(ts);

	state_change(ts, FTS_STATE_ACTIVE);

	return 0;
}

static int fts_statef_loader_stop(struct fts_data *ts, int param)
{
	fts_irq_disable(ts);

	fts_wake_unlock_for_fwupdate(ts);

	return fts_statef_cmn_stop(ts, param);
}

static int fts_statef_loader_int(struct fts_data *ts, int param)
{
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int fts_statef_sleep_enter(struct fts_data *ts, int param)
{
	fts_event_force_touchup(ts);

	fts_irq_disable(ts);

	fts_statef_cmn_stop(ts, 0);

	return 0;
}

static int fts_statef_sleep_wakeup(struct fts_data *ts, int param)
{
	fts_irq_enable(ts);

	fts_system_set_wakeup(ts);

	fts_sleep(ts, 0);

	fts_reset(ts);

	state_change(ts, FTS_STATE_ACTIVE);
	return 0;
}

static int fts_statef_sleep_start_ldr(struct fts_data *ts, int param)
{
	state_change(ts, FTS_STATE_BOOTLOADER);
	return 0;
}

static int fts_statef_sleep_int(struct fts_data *ts, int param)
{
	fts_read_touchevent(ts, FTS_STATE_SLEEP);

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int fts_open(struct input_dev *dev)
{
	struct fts_data *ts = (struct fts_data*)input_get_drvdata(dev);

	fts_func_request_async(ts, FTS_FUNC_REQ_EVEMT_OPEN);

	return 0;
}

static void fts_close(struct input_dev *dev)
{
	struct fts_data *ts = (struct fts_data*)input_get_drvdata(dev);

	fts_func_request_async(ts, FTS_FUNC_REQ_EVEMT_CLOSE);
}

static int fts_init_internal_variables(struct fts_data *ts)
{
	int result = 0;

	result = fts_func_async_init(ts);
	if(result < 0){
		goto fail_init;
	}

	ts->state_mgr.state = FTS_STATE_IDLE;

	memset(&ts->fw_report_info, 0, sizeof(ts->fw_report_info));
	memset(&ts->fw_report_info_store, 0, sizeof(ts->fw_report_info_store));
	memset(&ts->report_info, 0, sizeof(ts->report_info));
	memset(&ts->touch_state, 0, sizeof(ts->touch_state));

	fts_cpu_idle_sleep_wake_lock_init(ts);
	fts_fwupdate_wake_lock_init(ts);

	fts_fwctl_set_dev_state(ts, FTS_DEV_STATE_SLEEP);

    ts->deter_suspend_i2c.wake_lock_state = 0;
	memset(&ts->deter_suspend_i2c, 0, sizeof(ts->deter_suspend_i2c));
    wake_lock_init(&ts->deter_suspend_i2c.wake_lock, WAKE_LOCK_SUSPEND, "fts_resume_wake_lock");
	ts->deter_suspend_i2c.pm_qos_lock_idle.type = PM_QOS_REQ_AFFINE_CORES;
	ts->deter_suspend_i2c.pm_qos_lock_idle.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->deter_suspend_i2c.pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	INIT_WORK(&ts->deter_suspend_i2c.pending_proc_work, fts_deter_suspend_i2c_pending_proc_work_function);

	/* -------------------------------------------------------------------------- */
	return 0;

fail_init:
	fts_func_async_deinit(ts);

	return result;
}


static void fts_deinit_internal_variables(struct fts_data *ts)
{
	if(ts){
		fts_func_async_deinit(ts);
		fts_cpu_idle_sleep_wake_lock_deinit(ts);
		fts_fwupdate_wake_lock_deinit(ts);
	}
}

static int fts_init_inputdev(struct fts_data *ts, struct device *ctrl_dev_p, char *modalias)
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
	ts->input->open			= fts_open;
	ts->input->close		= fts_close;

	/** set properties */
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input->propbit);

	input_set_drvdata(ts->input, ts);
	input_mt_init_slots(ts->input, FTS_FINGER_MAX, 0);

	if(ts->input->mt == NULL){
		input_free_device(ts->input);
		return -ENOMEM;
	}

	/** set parameters */
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, FTS_VAL_FINGER_WIDTH_MAXSIZE, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_X,  0, CONFIG_FTS_LCD_SIZE_X - 1, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y,  0, CONFIG_FTS_LCD_SIZE_Y - 1, 0, 0);

	input_set_abs_params(ts->input, ABS_MT_PRESSURE,    0, 255, 0, 0);

	/** register input device */
	if(input_register_device(ts->input) != 0){
		input_free_device(ts->input);
		return -EFAULT;
	}

	return 0;
}

static void fts_deinit_inputdev(struct fts_data *ts)
{
	if(ts && ts->input){
		if(ts->input->mt){
			input_mt_destroy_slots(ts->input);
		}
		input_free_device(ts->input);
	}
}

int fts_core_probe(
						struct device *ctrl_dev_p,
						struct fts_ctrl_functbl *func_p,
						void *ctrl_p,
						char *modalias,
						int gpio_irq)
{
	int result = 0;
	struct fts_data *ts;
	#ifdef CONFIG_OF
		; // nothing
	#else
		struct fts_platform_data *pdata = ctrl_dev_p->platform_data;
	#endif /* CONFIG_OF */

	ftsif_init();

	fts_mutex_lock_ctrl();

	ts = kzalloc(sizeof(struct fts_data), GFP_KERNEL);
	if(!ts){
		result = -ENOMEM;
		fts_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	result = fts_fwctl_init(ts, ctrl_p, func_p);
	if(result){
		result = -EFAULT;
		fts_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	if(fts_init_internal_variables(ts)){
		fts_mutex_unlock_ctrl();
		goto fail_init_internal_variables;
	}

	/** set device info */
	dev_set_drvdata(ctrl_dev_p, ts);
	gFts_data = ts;


	#ifdef CONFIG_OF
		ts->irq_mgr.irq	= irq_of_parse_and_map(ctrl_dev_p->of_node, 0);
		ts->rst_pin		= of_get_named_gpio(ctrl_dev_p->of_node, "fts_ts,rst_pin", 0);
	#else
		ts->rst_pin		= pdata->gpio_rst;
		ts->irq_mgr.irq	= gpio_irq;
	#endif /* CONFIG_OF */

    if(!gpio_is_valid(ts->rst_pin)){
		result = -EFAULT;
		fts_mutex_unlock_ctrl();
		goto fail_get_dtsinfo;
	}

	snprintf(ts->phys, sizeof(ts->phys), "%s", dev_name(ctrl_dev_p));

	/** setup device */
	#ifdef CONFIG_OF
		result = fts_device_setup(ts->irq_mgr.irq, ts->rst_pin);
		if(result){
			fts_mutex_unlock_ctrl();
			goto fail_device_setup;
		}
	#else
		if (pdata && pdata->setup) {
			result = pdata->setup(ctrl_dev_p);
			if (result){
				fts_mutex_unlock_ctrl();
				goto fail_alloc_mem;
			}
		}
	#endif /* CONFIG_OF */

	if(fts_irq_resuest(ts)){
		result = -EFAULT;
		fts_mutex_unlock_ctrl();
		goto fail_irq_request;
	}

	fts_mutex_unlock_ctrl();


	/** init device info */
	result = fts_init_inputdev(ts, ctrl_dev_p, modalias);
	if(result != 0){
		goto fail_init_inputdev;
	}

	return 0;

fail_init_inputdev:
fail_irq_request:
fail_get_dtsinfo:
	fts_deinit_internal_variables(ts);

fail_init_internal_variables:
fail_device_setup:
	kfree(ts);

fail_alloc_mem:
	ftsif_exit();
	return result;
}

int fts_core_remove(struct fts_data *ts, struct device *ctrl_dev_p)
{
	#ifndef CONFIG_OF
		struct fts_platform_data *pdata = (struct fts_platform_data *)&ctrl_dev_p->platform_data;
	#endif /* !CONFIG_OF */

	gFts_data = NULL;

	if(ts){
		free_irq(ts->irq_mgr.irq, ts);

		#ifdef CONFIG_OF
			fts_device_teardown(ts->irq_mgr.irq, ts->rst_pin);
		#else
			if (pdata && pdata->teardown){
				pdata->teardown(&ctrl_dev_p);
			}
		#endif /* CONFIG_OF */

		fts_deinit_internal_variables(ts);

		fts_deinit_inputdev(ts);

		fts_fwctl_deinit(ts);

		kfree(ts);
	}

	ftsif_exit();

	return 0;
}

int fts_core_suspend(struct fts_data *ts, pm_message_t mesg)
{
	fts_set_suspend_state(ts);

	return 0;
}

int fts_core_resume(struct fts_data *ts)
{
	fts_clr_suspend_state(ts);

	return 0;
}


/* -----------------------------------------------------------------------------------
 */
int fts_fw_update(struct fts_data *ts, const unsigned char *fw_data, int fw_size)
{
	int ret_update;
	u8 buf;

	if(fw_data == NULL || fw_size <= 0){
		return -1;
	}

	if(fts_enter_bootloader(ts) != 0){
		return -1;
	}

	ret_update = fts_fwctl_loader_write_pram(ts, (u8*)fts_fw_pram, sizeof(fts_fw_pram));
	if(ret_update == 0){
		ret_update = fts_fwctl_loader_upgrade(ts, (u8*)fw_data, fw_size);
		if(ret_update == 0){
			fts_fwctl_get_device_status(ts, &buf);
			if(buf == FTS_REGVAL_STATUS_CRC_ERROR){
				ret_update = -1;
			}
		}
	}

	if(fts_exit_bootloader(ts) != 0){
		return -1;
	}

	return ret_update;
}

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(fts_cpu_idle_sleep_ctrl_lock);
static DEFINE_MUTEX(fts_cpu_sleep_ctrl_for_fwupdate_lock);

/* -----------------------------------------------------------------------------------
 */
static void fts_mutex_lock_cpu_idle_sleep_ctrl(void);
static void fts_mutex_unlock_cpu_idle_sleep_ctrl(void);
static void fts_wake_lock_idle_l(struct fts_data *ts);
static void fts_wake_unlock_idle_l(struct fts_data *ts);

static void fts_mutex_lock_cpu_sleep_ctrl(void);
static void fts_mutex_unlock_cpu_sleep_ctrl(void);

static void fts_func_open(struct fts_data *ts);
static void fts_func_close(struct fts_data *ts);
static int fts_func_enable(struct fts_data *ts);
static void fts_func_disable(struct fts_data *ts);

static void fts_func_request_async_complete(void *arg_p);
static void fts_func_request_sync_complete(void *arg_p);
static void fts_func_workq( struct work_struct *work_p );

/* -------------------------------------------------------------------------- */
struct fts_req_msg {	//**********	FTS_ASYNC_OPEN_ENABLE
	struct list_head queue;
	void	(*complete)(void *context);
	void	*context;
	int		status;
	int		event;
	void	*param_p;
};

/* -----------------------------------------------------------------------------------
 */
static void fts_mutex_lock_cpu_idle_sleep_ctrl(void)
{
	mutex_lock(&fts_cpu_idle_sleep_ctrl_lock);
}

static void fts_mutex_unlock_cpu_idle_sleep_ctrl(void)
{
	mutex_unlock(&fts_cpu_idle_sleep_ctrl_lock);
}

static void fts_wake_lock_idle_l(struct fts_data *ts)
{
	fts_mutex_lock_cpu_idle_sleep_ctrl();
	if(ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state == 0){
		pm_qos_update_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, FTS_QOS_LATENCY_DEF_VALUE);
		ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state = 1;
	}
	fts_mutex_unlock_cpu_idle_sleep_ctrl();
}

static void fts_wake_unlock_idle_l(struct fts_data *ts)
{
	fts_mutex_lock_cpu_idle_sleep_ctrl();
	if(ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state != 0){
		pm_qos_update_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, PM_QOS_DEFAULT_VALUE);
		ts->cpu_idle_sleep_ctrl_p->wake_lock_idle_state = 0;
	}
	fts_mutex_unlock_cpu_idle_sleep_ctrl();
}

void fts_wake_lock_idle(struct fts_data *ts)
{
	if(FTS_STATE_ACTIVE == ts->state_mgr.state){
		fts_wake_lock_idle_l(ts);
	}
}

void fts_wake_unlock_idle(struct fts_data *ts)
{
	fts_wake_unlock_idle_l(ts);
}

void fts_cpu_idle_sleep_wake_lock_init( struct fts_data *ts )
{
	ts->cpu_idle_sleep_ctrl_p = kzalloc(sizeof(struct fts_cpu_idle_sleep_ctrl_info), GFP_KERNEL);
	if(ts->cpu_idle_sleep_ctrl_p == NULL){
		return;
	}
	ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency.type = PM_QOS_REQ_AFFINE_CORES;
	ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

void fts_cpu_idle_sleep_wake_lock_deinit( struct fts_data *ts )
{
	pm_qos_remove_request(&ts->cpu_idle_sleep_ctrl_p->qos_cpu_latency);

	if(ts->cpu_idle_sleep_ctrl_p)	kfree(ts->cpu_idle_sleep_ctrl_p);
	ts->cpu_idle_sleep_ctrl_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
static void fts_mutex_lock_cpu_sleep_ctrl(void)
{
	mutex_lock(&fts_cpu_sleep_ctrl_for_fwupdate_lock);
}

static void fts_mutex_unlock_cpu_sleep_ctrl(void)
{
	mutex_unlock(&fts_cpu_sleep_ctrl_for_fwupdate_lock);
}

void fts_wake_lock_for_fwupdate(struct fts_data *ts)
{
	fts_mutex_lock_cpu_sleep_ctrl();
	if(ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state == 0){
		wake_lock(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);
		pm_qos_update_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, FTS_QOS_LATENCY_DEF_VALUE);
		ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state = 1;
	}
	fts_mutex_unlock_cpu_sleep_ctrl();
}

void fts_wake_unlock_for_fwupdate(struct fts_data *ts)
{
	fts_mutex_lock_cpu_sleep_ctrl();
	if(ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state != 0){
		wake_unlock(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);
		pm_qos_update_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, PM_QOS_DEFAULT_VALUE);
		ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate_state = 0;
	}
	fts_mutex_unlock_cpu_sleep_ctrl();
}

void fts_fwupdate_wake_lock_init( struct fts_data *ts )
{
	ts->cpu_sleep_ctrl_fwupdate_p = kzalloc(sizeof(struct fts_cpu_sleep_ctrl_fwupdate_info), GFP_KERNEL);
	if(ts->cpu_sleep_ctrl_fwupdate_p == NULL){
		return;
	}

	wake_lock_init(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate, WAKE_LOCK_SUSPEND, "fts_wake_lock_for_fwupdate");
	ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate.type = PM_QOS_REQ_AFFINE_CORES;
	ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

void fts_fwupdate_wake_lock_deinit( struct fts_data *ts )
{
	pm_qos_remove_request(&ts->cpu_sleep_ctrl_fwupdate_p->qos_cpu_latency_for_fwupdate);
	wake_lock_destroy(&ts->cpu_sleep_ctrl_fwupdate_p->wake_lock_for_fwupdate);

	if(ts->cpu_sleep_ctrl_fwupdate_p)	kfree(ts->cpu_sleep_ctrl_fwupdate_p);
	ts->cpu_sleep_ctrl_fwupdate_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
static void fts_func_lcd_on(struct fts_data *ts)
{
	request_event(ts, FTS_EVENT_WAKEUP, 0);
}

static void fts_func_lcd_off(struct fts_data *ts)
{
	request_event(ts, FTS_EVENT_SLEEP, 0);
}

/* -----------------------------------------------------------------------------------
 */
static void fts_func_boot_fw_update(struct fts_data *ts)
{
	u8 buf;
	const unsigned char* fw_data = NULL;
	int ver;
	u8 update = 0;

	fts_start(ts);

	fts_fwctl_get_device_status(ts, &buf);

	ver = fts_fwver(ts);
	if(ver != fts_fwver_builtin(ts)){
		update = 1;
	}

	if(buf == FTS_REGVAL_STATUS_CRC_ERROR){
		update = 1;
	}

	if(update != 0){
		fw_data = fts_fwdata_builtin(ts);
		if(fw_data){
			int ret;
			int retry = 5;
			do{
				ret = fts_fw_update(ts, fw_data, fts_fwsize_builtin(ts));
			}while(ret != 0 && (retry-- > 0));
		}
	}

	if(ts->is_lcd_on == 0){
		fts_shutdown(ts);
	}
}

/* -----------------------------------------------------------------------------------
 */
static void fts_func_open(struct fts_data *ts)
{
}

static void fts_func_close(struct fts_data *ts)
{
	fts_shutdown(ts);
}

static int fts_func_enable(struct fts_data *ts)
{
	if(fts_start(ts) != 0){
		return -EFAULT;
	}

	return 0;
}

static void fts_func_wakelock_init(struct fts_data *ts)
{
	wake_lock_init(&ts->work_wake_lock, WAKE_LOCK_SUSPEND, "fts_work_wake_lock");
	ts->work_pm_qos_lock_idle.type = PM_QOS_REQ_AFFINE_CORES;
	ts->work_pm_qos_lock_idle.cpus_affine.bits[0] = 0x0f;
	pm_qos_add_request(&ts->work_pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

static void fts_func_wakelock_deinit(struct fts_data *ts)
{
	wake_lock_destroy(&ts->work_wake_lock);
	pm_qos_remove_request(&ts->work_pm_qos_lock_idle);
}

static void fts_func_wakelock(struct fts_data *ts, int on)
{
	if(on){
		wake_lock(&ts->work_wake_lock);
		pm_qos_update_request(&ts->work_pm_qos_lock_idle, FTS_QOS_LATENCY_DEF_VALUE);
	}else{
		wake_unlock(&ts->work_wake_lock);
		pm_qos_update_request(&ts->work_pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
	}
}

static void fts_func_disable(struct fts_data *ts)
{
	fts_shutdown(ts);
}


static void fts_func_request_async_complete(void *arg_p)
{
	kfree( arg_p );
}

void fts_func_request_async( struct fts_data *ts, int event)
{
	struct fts_req_msg		*msg_p;
	unsigned long	flags;

	msg_p = (struct fts_req_msg *)kzalloc( sizeof( struct fts_req_msg ), GFP_KERNEL );
	if ( msg_p == NULL ){
		return;
	}

	msg_p->complete = fts_func_request_async_complete;
	msg_p->event = event;
	msg_p->context = msg_p;
	msg_p->status = -1;

	spin_lock_irqsave( &(ts->queue_lock), flags);
	list_add_tail( &(msg_p->queue), &(ts->queue) );
	spin_unlock_irqrestore( &(ts->queue_lock), flags);
	queue_work(ts->workqueue_p, &(ts->work_data) );
}

static void fts_func_request_sync_complete(void *arg_p)
{
	complete( arg_p );
}

int fts_func_request_sync( struct fts_data *ts, int event)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct fts_req_msg msg;
	unsigned long	flags;

	msg.complete = fts_func_request_sync_complete;
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

static void fts_overlap_event_check(struct fts_data *ts)
{
	unsigned long flags;
	struct list_head *list_p;
	int overlap_req_cnt_lcd = -1;

	spin_lock_irqsave( &(ts->queue_lock), flags );
	list_for_each(list_p, &(ts->queue)){
		ts->cur_msg_p = list_entry(list_p, struct fts_req_msg, queue);
		spin_unlock_irqrestore( &(ts->queue_lock), flags );

		if( (ts->cur_msg_p->event == FTS_FUNC_REQ_EVEMT_LCD_ON) ||
			(ts->cur_msg_p->event == FTS_FUNC_REQ_EVEMT_LCD_OFF) )
		{
			overlap_req_cnt_lcd++;
		}

		spin_lock_irqsave( &(ts->queue_lock), flags );
	}
	spin_unlock_irqrestore( &(ts->queue_lock), flags );

	if(overlap_req_cnt_lcd > 0){
		spin_lock_irqsave( &(ts->queue_lock), flags );
		list_for_each(list_p, &(ts->queue)){
			ts->cur_msg_p = list_entry(list_p, struct fts_req_msg, queue);
			spin_unlock_irqrestore( &(ts->queue_lock), flags );

			if(ts->cur_msg_p != NULL){
				if( (ts->cur_msg_p->event == FTS_FUNC_REQ_EVEMT_LCD_ON) ||
					(ts->cur_msg_p->event == FTS_FUNC_REQ_EVEMT_LCD_OFF) )
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

static void fts_func_workq( struct work_struct *work_p )
{
	struct fts_data	*ts;
	unsigned long			flags;

	ts = container_of(work_p, struct fts_data, work_data);

	spin_lock_irqsave( &(ts->queue_lock), flags );
	while( list_empty( &(ts->queue) ) == 0 ){
		ts->cur_msg_p = list_entry( ts->queue.next, struct fts_req_msg, queue);
		list_del_init( &(ts->cur_msg_p->queue) );
		spin_unlock_irqrestore( &(ts->queue_lock), flags );

		fts_func_wakelock(ts, 1);

		switch(ts->cur_msg_p->event){
			case FTS_FUNC_REQ_EVEMT_OPEN:
				if(fts_check_suspend_state(ts, FTS_DETER_SUSPEND_I2C_PROC_OPEN, 0) == 0){
					fts_func_open(ts);
				}
				ts->cur_msg_p->status = 0;
				break;

			case FTS_FUNC_REQ_EVEMT_CLOSE:
				if(fts_check_suspend_state(ts, FTS_DETER_SUSPEND_I2C_PROC_CLOSE, 0) == 0){
					fts_func_close(ts);
				}
				ts->cur_msg_p->status = 0;
				break;

			case FTS_FUNC_REQ_EVEMT_ENABLE:
				if(fts_check_suspend_state(ts, FTS_DETER_SUSPEND_I2C_PROC_ENABLE, 0) == 0){
					ts->cur_msg_p->status = fts_func_enable(ts);
				}else{
					ts->cur_msg_p->status = 0;
				}
				break;

			case FTS_FUNC_REQ_EVEMT_DISABLE:
				fts_func_disable(ts);
				ts->cur_msg_p->status = 0;
				break;

			case FTS_FUNC_REQ_EVEMT_LCD_ON:
				fts_func_lcd_on(ts);
				ts->cur_msg_p->status = 0;
				break;

			case FTS_FUNC_REQ_EVEMT_LCD_OFF:
				fts_func_lcd_off(ts);
				ts->cur_msg_p->status = 0;
				break;

			case FTS_FUNC_REQ_BOOT_FW_UPDATE:
				fts_func_boot_fw_update(ts);
				ts->cur_msg_p->status = 0;
				break;

			default:
				ts->cur_msg_p->status = -1;
				break;
		}

		if( ts->cur_msg_p->complete ){
			ts->cur_msg_p->complete( ts->cur_msg_p->context );
		}

		fts_overlap_event_check(ts);

		fts_func_wakelock(ts, 0);

		spin_lock_irqsave( &(ts->queue_lock), flags );
	}
	spin_unlock_irqrestore( &(ts->queue_lock), flags );
}

int fts_func_async_init( struct fts_data *ts)
{
	ts->workqueue_p = alloc_workqueue("FTS_WORK", WQ_UNBOUND, 1);
	if(ts->workqueue_p == NULL){
		return -ENOMEM;
	}
	INIT_WORK( &(ts->work_data), fts_func_workq );
	INIT_LIST_HEAD( &(ts->queue) );
	spin_lock_init( &(ts->queue_lock) );
	fts_func_wakelock_init(ts);

	return 0;
}

void fts_func_async_deinit( struct fts_data *ts)
{
	fts_func_wakelock_deinit(ts);
	if(ts->workqueue_p){
		destroy_workqueue(ts->workqueue_p);
	}
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_ic_init(struct fts_data *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
static int fts_Upgrade_ReadPram(struct fts_fwctl_info *fc_p, unsigned int Addr, unsigned char * pData, unsigned short Datalen)
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
int fts_fwctl_loader_write_pram(struct fts_data *ts_p, u8 *pbt_buf, u32 dw_lenth)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
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
				if(fts_Upgrade_ReadPram(fc_p,StartFlashAddr, pCheckBuffer+FlashAddr, dw_lenth-FlashAddr))
				{
					i_ret = -EIO;
					goto ERROR;
				}
				ReCode = ERROR_CODE_OK;
				FlashAddr = dw_lenth;
			}
			else
			{
				if(fts_Upgrade_ReadPram(fc_p,StartFlashAddr, pCheckBuffer+FlashAddr, FTS_PACKET_LENGTH))
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
int fts_fwctl_loader_upgrade(struct fts_data *ts_p, u8 *pbt_buf, u32 dw_lenth)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
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
int fts_fwctl_get_device_status(struct fts_data *ts_p, u8 *status_p)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
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
int fts_fwctl_set_active(struct fts_data *ts_p)
{
	fts_fwctl_set_dev_state(ts_p, FTS_DEV_STATE_ACTIVE);
	return 0;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_set_sleepmode_on(struct fts_data *ts_p)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
	M_WRITE_FUNC(fc_p, FTS_REG_PMODE, FTS_PMODE_HIBERNATE);
	fts_fwctl_set_dev_state(ts_p, FTS_DEV_STATE_SLEEP);
	return 0;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_set_sleepmode_off(struct fts_data *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_fingermax(struct fts_data *ts_p)
{
	return FTS_FINGER_MAX;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_fingerinfo(struct fts_data *ts_p, u8 *buf_p, int read_cnt, u8 *irqsts_p, u8 *extsts_p, u8 **finger_pp)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
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
int fts_fwctl_get_num_of_touch_fingers(struct fts_data *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_POINT_NUM];
}

/* -------------------------------------------------------------------------- */
u8* fts_fwctl_get_finger_info_buf(struct fts_data *ts_p, int fingerid, int fingerMax, u8 *buf_p)
{
	return &buf_p[FTS_TCH_LEN(fingerid)];
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_state(struct fts_data *ts_p, int fingerid, int fingerMax, u8 *buf_p)
{
	u8 event = buf_p[FTS_TOUCH_EVENT_POS] >> 6;
	if(event == FTS_TOUCH_DOWN || event == FTS_TOUCH_CONTACT){
		return FTS_TOUCH_STATE_FINGER;
	}
	return FTS_TOUCH_STATE_NO_TOUCH;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_pointid(struct fts_data *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_ID_POS] >> 4;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_pos_x(struct fts_data *ts_p, u8 *buf_p)
{
	return (s16) (buf_p[FTS_TOUCH_X_H_POS] & 0x0F) << 8
	     | (s16) buf_p[FTS_TOUCH_X_L_POS];
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_pos_y(struct fts_data *ts_p, u8 *buf_p)
{
	return (s16) (buf_p[FTS_TOUCH_Y_H_POS] & 0x0F) << 8
	     | (s16) buf_p[FTS_TOUCH_Y_L_POS];
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_wx(struct fts_data *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_AREA] >> 4;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_wy(struct fts_data *ts_p, u8 *buf_p)
{
	return buf_p[FTS_TOUCH_AREA] >> 4;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_finger_z(struct fts_data *ts_p, u8 *buf_p)
{
	u8 weight = buf_p[FTS_TOUCH_WEIGHT];
	if(weight == 0){
		return FTS_PRESS;
	}
	return weight;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_fwver(struct fts_data *ts_p, u16 *ver_p)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
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
int fts_fwctl_initparam(struct fts_data *ts_p)
{
	return 0;
}

/* -------------------------------------------------------------------------- */
void fts_fwctl_set_dev_state(struct fts_data *ts_p, u8 state)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
	fc_p->dev_state = state;
}

/* -------------------------------------------------------------------------- */
u8 fts_fwctl_get_dev_state(struct fts_data *ts_p)
{
	struct fts_fwctl_info *fc_p = ts_p->fwctl_p;
	return fc_p->dev_state;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_maxXPosition(struct fts_data *ts_p)
{
	return CONFIG_FTS_LCD_SIZE_X;
}

/* -------------------------------------------------------------------------- */
int fts_fwctl_get_maxYPosition(struct fts_data *ts_p)
{
	return CONFIG_FTS_LCD_SIZE_Y;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
int fts_fwctl_init(struct fts_data *ts_p, void *fts_ctrl_p, struct fts_ctrl_functbl *func_p)
{
	ts_p->fwctl_p = kzalloc(sizeof(struct fts_fwctl_info), GFP_KERNEL);
	if(ts_p->fwctl_p == NULL){
		return -ENOMEM;
	}
	ts_p->fwctl_p->devctrl_func_p = func_p;	/* func.table i2c */
	ts_p->fwctl_p->fts_ctrl_p = fts_ctrl_p;	/* struct device */

	fts_fwctl_ic_init(ts_p);

	return 0;
}

/* -------------------------------------------------------------------------- */
void fts_fwctl_deinit(struct fts_data *ts_p)
{
	if(ts_p->fwctl_p)	kfree(ts_p->fwctl_p);
	ts_p->fwctl_p = NULL;
}

/* -----------------------------------------------------------------------------------
 */
dev_t 					ftsif_devid;
struct class*			ftsif_class = NULL;
struct device*			ftsif_device = NULL;
struct cdev 			ftsif_cdev;

/* -----------------------------------------------------------------------------------
 */
static int fts_copy_to_user(u8 *krnl, unsigned long arg, int size, int isCompat)
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

static int fts_fw_upgrade_copy_from_user(struct fts_ioctl_param *krnl, unsigned long arg, int isCompat)
{
#ifdef CONFIG_COMPAT
	if(isCompat == 1)
	{
		int err = 0;
		int size = 0;
		struct fts_compat_ioctl_param *usr =
			(struct fts_compat_ioctl_param __user *) arg;

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
		struct fts_ioctl_param *usr =
			(struct fts_ioctl_param __user *) arg;

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

static void fts_fw_upgrade_release(struct fts_ioctl_param *krnl)
{
	if(krnl->data){
		kfree(krnl->data);
	}
}

static int fts_ioctl_getver(struct fts_data *ts, unsigned long arg, int isCompat)
{
	u8  fts_stop_request = 0;
	u16 ver;

	if(0 == arg){
		return -EINVAL;
	}

	if(ts->state_mgr.state == FTS_STATE_IDLE){
		fts_stop_request = 1;
	}

	if(0 != fts_start(ts)){
		return -EFAULT;
	}

	fts_mutex_lock_ctrl();

	ver = fts_fwver(ts);

	fts_mutex_unlock_ctrl();

	if(fts_stop_request){
		fts_shutdown(ts);
	}

	if(fts_copy_to_user((u8*)&ver, arg, sizeof(ver), isCompat)){
		return -EFAULT;
	}

	return 0;
}

static int fts_ioctl_getver_builtin(struct fts_data *ts, unsigned long arg, int isCompat)
{
	u16 ver = fts_fwver_builtin(ts);

	if(fts_fwdata_builtin(ts) == NULL){
		return -EFAULT;
	}
	if(0 == arg){
		return -EINVAL;
	}

	if(fts_copy_to_user((u8*)&ver, arg, sizeof(ver), isCompat)){
		return -EFAULT;
	}
	return 0;
}

static int fts_ioctl_fw_upgrade(struct fts_data *ts, unsigned long arg, int isCompat)
{
	int rc;
	struct fts_ioctl_param param;
	const unsigned char* fw_data = NULL;

	if(0 == arg || 0 != fts_fw_upgrade_copy_from_user(&param, arg, isCompat)){
		return -EINVAL;
	}

	if(param.size == 0){
		fw_data = fts_fwdata_builtin(ts);
		if(fw_data){
			rc = fts_fw_update(ts, (u8*)fw_data, fts_fwsize_builtin(ts));
		}
		else{
			rc = -1;
		}
	}
	else{
		rc = fts_fw_update(ts, param.data, param.size);
	}

	if(ts->is_lcd_on == 0){
		fts_shutdown(ts);
	}

	fts_fw_upgrade_release(&param);
	if(rc){
		return -EFAULT;
	}

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int ftsif_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int ftsif_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long ftsif_ioctl_cmd(struct fts_data *ts, unsigned int cmd, unsigned long arg, int isCompat)
{
	int	rc = 0;

	switch(cmd){
	case FTSDEV_GET_FW_VERSION:			rc = fts_ioctl_getver(ts, arg, isCompat);		break;
	case FTSDEV_GET_FW_VERSION_BUILTIN:	rc = fts_ioctl_getver_builtin(ts, arg, isCompat);	break;
	case FTSDEV_FW_UPGRADE:				rc = fts_ioctl_fw_upgrade(ts, arg, isCompat);	break;

	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}

static long ftsif_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	struct fts_data *ts = gFts_data;
	int isCompat = 0;

	if(ts == NULL){
		return -EFAULT;
	}

	rc = ftsif_ioctl_cmd(ts, cmd, arg, isCompat);

	return rc;
}

#ifdef CONFIG_COMPAT
static long ftsif_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int	rc = 0;
	struct fts_data *ts = gFts_data;
	int isCompat = 1;

	if(ts == NULL){
		return -EFAULT;
	}

	switch(cmd){
	case COMPAT_FTSDEV_FW_UPGRADE:				cmd = FTSDEV_FW_UPGRADE;			break;
	}

	rc = ftsif_ioctl_cmd(ts, cmd, arg, isCompat);

	return rc;
}
#endif /* CONFIG_COMPAT */

static const struct file_operations ftsif_fileops = {
	.owner   = THIS_MODULE,
	.open    = ftsif_open,
	.release = ftsif_release,
	.unlocked_ioctl   = ftsif_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =	ftsif_compat_ioctl,
#endif /* CONFIG_COMPAT */
};

static int ftsif_init(void)
{
	int rc;

	rc = alloc_chrdev_region(&ftsif_devid, 0, 1, FTS_TOUCH_IF_DEVNAME);
	if(rc < 0){
		return rc;
	}

	ftsif_class = class_create(THIS_MODULE, FTS_TOUCH_IF_DEVNAME);
	if (IS_ERR(ftsif_class)) {
		rc = PTR_ERR(ftsif_class);
		goto error_vid_class_create;
	}

	ftsif_device = device_create(ftsif_class, NULL,
								ftsif_devid, &ftsif_cdev,
								FTS_TOUCH_IF_DEVNAME);
	if (IS_ERR(ftsif_device)) {
		rc = PTR_ERR(ftsif_device);
		goto error_vid_class_device_create;
	}

	cdev_init(&ftsif_cdev, &ftsif_fileops);
	ftsif_cdev.owner = THIS_MODULE;
	rc = cdev_add(&ftsif_cdev, ftsif_devid, 1);
	if(rc < 0){
		goto err_via_cdev_add;
	}

	return 0;

err_via_cdev_add:
	cdev_del(&ftsif_cdev);
error_vid_class_device_create:
	class_destroy(ftsif_class);
	ftsif_class = NULL;
error_vid_class_create:
	unregister_chrdev_region(ftsif_devid, 1);

	return rc;
}

static void ftsif_exit(void)
{
	if(ftsif_class != NULL){
		cdev_del(&ftsif_cdev);
		class_destroy(ftsif_class);
		ftsif_class = NULL;
		unregister_chrdev_region(ftsif_devid, 1);
	}
}

/* -------------------------------------------------------------------------- */
static DEFINE_MUTEX(i2c_rw_access);

static int fts_i2c_write_main(void *client, u16 addr, u8 *writebuf, u32 size);
static int fts_i2c_write(void *client, u16 addr, u8 data);
static int fts_i2c_write_packet(void *client, u16 addr, u8 *data, u32 size);

static int fts_i2c_read(void *client, u16 addr, u8 *buf, u32 size);
static int fts_i2c_read_packet (void *client, u16 addr, u8 *buf,  u32 size);

/* -----------------------------------------------------------------------------------
 */
static int fts_i2c_transfer(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
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
static int fts_i2c_write_main(void *client, u16 addr, u8 *writebuf, u32 size)
{
	int ret = 0;
	u8 *buf;
	u8 buf_local[1 + FTS_I2C_BLOCKWRITE_BUFSIZE];
	int allocSize = 0;

	if(size > FTS_I2C_BLOCKWRITE_BUFSIZE){
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

	ret = fts_i2c_transfer((struct i2c_client *)client, buf, (1 + size), NULL, 0);
	if(ret >= 0){
		ret = 0;
	}

	if(allocSize > 0){
		kfree(buf);
	}
	return ret;
}

static int fts_i2c_write(void *client, u16 addr, u8 data)
{
	return fts_i2c_write_packet(client, addr, &data, 1);
}

static int fts_i2c_write_packet(void *client, u16 addr, u8 *data, u32 size)
{
	int status = 0;
	int retry = FTS_I2C_RETRY_COUNT;

	do{
		status = fts_i2c_write_main(client,
			addr,
			data,
			size);
		//
		if(status){
			struct timespec tu;
			tu.tv_sec = (time_t)0;
			tu.tv_nsec = FTS_I2C_RETRY_WAIT * 1000000;
			hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
		}
	}while(status != 0 && retry-- > 0);

	return status;
}

static int fts_i2c_read_main(void *client, u16 addr, u8 *readbuf, u32 size)
{
	int ret;

	if(addr != 0x7FFF){
		u8 writebuf[1];

		writebuf[0] = addr;

		ret = fts_i2c_transfer((struct i2c_client *)client, writebuf, 1, readbuf, size);
		if(ret >= 0){
			ret = 0;
		}
	}
	else{
		ret = fts_i2c_transfer((struct i2c_client *)client, NULL, 0, readbuf, size);
		if(ret >= 0){
			ret = 0;
		}
	}

	return ret;
}

static int fts_i2c_read(void *client, u16 addr, u8 *buf, u32 size)
{
	return fts_i2c_read_packet(client, addr, buf, size);
}

static int fts_i2c_read_packet(void *client, u16 addr, u8 *buf, u32 size)
{
	int status = 0;
	int i;
	u32 s;
	int retry = FTS_I2C_RETRY_COUNT;

	do{
		s = size;
		for(i = 0; i < size; i += FTS_I2C_BLOCKREAD_BUFSIZE){
			status = fts_i2c_read_main(client,
				addr,
				buf + i,
				(s > FTS_I2C_BLOCKREAD_BUFSIZE) ? (FTS_I2C_BLOCKREAD_BUFSIZE) : (s));
			//
			if(status){
				struct timespec tu;
				tu.tv_sec = (time_t)0;
				tu.tv_nsec = FTS_I2C_RETRY_WAIT * 1000000;
				hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);
				break;
			}
			s -= FTS_I2C_BLOCKREAD_BUFSIZE;
			addr = 0x7FFF;	/* specifying no address after first read */
		}
	}while(status != 0 && retry-- > 0);

	return status;
}

static int fts_i2c_direct_write(void *client, u8 *writebuf, u32 writelen)
{
	int ret = 0;

	if (writelen > 0) {
		ret = fts_i2c_transfer((struct i2c_client *)client, writebuf, writelen, NULL, 0);
	}
	return ret;
}

static int fts_i2c_direct_read(void *client, u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
{
	int ret;

	if (writelen > 0) {
		ret = fts_i2c_transfer((struct i2c_client *)client, writebuf, writelen, readbuf, readlen);
	} else {
		ret = fts_i2c_transfer((struct i2c_client *)client, NULL, 0, readbuf, readlen);
	}

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static const struct fts_ctrl_functbl FtsCtrl_I2cFuncTbl = {
	.write_f          = fts_i2c_write,
	.read_f           = fts_i2c_read,
	.packet_write_f   = fts_i2c_write_packet,
	.packet_read_f    = fts_i2c_read_packet,
	.direct_write_f   = fts_i2c_direct_write,
	.direct_read_f    = fts_i2c_direct_read,
};

/* -----------------------------------------------------------------------------------
*/
static int fts_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	extern	int fts_core_probe( struct device *ctrl_dev_p,
									struct fts_ctrl_functbl *func_p,
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
	result = fts_core_probe(
							&client->dev,
							(struct fts_ctrl_functbl*)&FtsCtrl_I2cFuncTbl,
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
static int fts_i2c_remove(struct i2c_client *client)
{
	extern int fts_core_remove(struct fts_data *, struct device *);
	struct fts_data *ts_p = i2c_get_clientdata(client);
	int ret = fts_core_remove(ts_p, &client->dev);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static int fts_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	extern int fts_core_suspend(struct fts_data *, pm_message_t);
	struct fts_data *ts_p = i2c_get_clientdata(client);
	int ret = fts_core_suspend(ts_p, mesg);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static int fts_i2c_resume(struct i2c_client *client)
{
	extern int fts_core_resume(struct fts_data *);
	struct fts_data *ts_p = i2c_get_clientdata(client);
	int ret = fts_core_resume(ts_p);

	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static const struct dev_pm_ops fts_ts_pm_ops = {
};
static const struct i2c_device_id fts_ts_id[] = {
	{"fts_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, fts_ts_id);

#ifdef CONFIG_OF // Open firmware must be defined for dts useage
static struct of_device_id fts_table[] = {
	{ . compatible = "sharp,fts_ts" ,}, // Compatible node must match dts
	{ },
};
#endif

static struct i2c_driver fts_i2c_driver = {
	.probe		= fts_i2c_probe,
	.remove		= fts_i2c_remove,
	.suspend	= fts_i2c_suspend,
	.resume		= fts_i2c_resume,
	.driver		= {
		#ifdef CONFIG_OF // Open firmware must be defined for dts useage
			.of_match_table = fts_table,
		#endif
		.name = FTS_TOUCH_IF_DEVNAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &fts_ts_pm_ops,
#endif
	},
	.id_table = fts_ts_id,
};

/* -----------------------------------------------------------------------------------
*/
static int __init fts_i2c_init(void)
{
	return i2c_add_driver(&fts_i2c_driver);
}
module_init(fts_i2c_init);

/* -----------------------------------------------------------------------------------
*/
static void __exit fts_i2c_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}
module_exit(fts_i2c_exit);

/* -----------------------------------------------------------------------------------
*/
void fts_setsleep(int on)
{
	struct fts_data *ts = gFts_data;

	if(ts){
		if(on == 1){
			ts->is_lcd_on = 0;
		}
		else{
			ts->is_lcd_on = 1;
		}

		if(fts_check_suspend_state(ts, FTS_DETER_SUSPEND_I2C_PROC_SETSLEEP, (u8)on) == 0){
			fts_setsleep_proc(ts, (u8)on);
		}
	}
}
EXPORT_SYMBOL(fts_setsleep);


/* -----------------------------------------------------------------------------------
*/
MODULE_DESCRIPTION("FocalTech fts TouchScreen driver"); 
MODULE_LICENSE("GPLv2");
