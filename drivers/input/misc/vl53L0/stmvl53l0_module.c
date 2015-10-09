/*
 *  stmvl53l0_module.c - Linux kernel modules for STM VL53L0 FlightSense TOF
 *						 sensor
 *
 *  Copyright (C) 2015 STMicroelectronics Imaging Division.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
/*
 * API includes
 */
#include "vl53l0_api.h"
/*
#include "vl53l0_def.h"
#include "vl53l0_platform.h"
#include "stmvl53l0-i2c.h"
#include "stmvl53l0-cci.h"
#include "stmvl53l0.h"
*/

#define USE_INT
#define IRQ_NUM	   59
/*#define DEBUG_TIME_LOG*/
#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

/*
 * Global data
 */

#ifdef CAMERA_CCI
static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl = {
	.init = stmvl53l0_init_cci,
	.deinit = stmvl53l0_exit_cci,
	.power_up = stmvl53l0_power_up_cci,
	.power_down = stmvl53l0_power_down_cci,
};
#else
static struct stmvl53l0_module_fn_t stmvl53l0_module_func_tbl = {
	.init = stmvl53l0_init_i2c,
	.deinit = stmvl53l0_exit_i2c,
	.power_up = stmvl53l0_power_up_i2c,
	.power_down = stmvl53l0_power_down_i2c,
};
#endif
struct stmvl53l0_module_fn_t *pmodule_func_tbl;

/*
 * IOCTL definitions
 */
#define VL53L0_IOCTL_INIT			_IO('p', 0x01)
#define VL53L0_IOCTL_XTALKCALB		_IO('p', 0x02)
#define VL53L0_IOCTL_OFFCALB		_IO('p', 0x03)
#define VL53L0_IOCTL_STOP			_IO('p', 0x05)
#define VL53L0_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL53L0_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL53L0_IOCTL_GETDATAS \
			_IOR('p', 0x0b, VL53L0_RangingMeasurementData_t)
#define VL53L0_IOCTL_REGISTER \
			_IOWR('p', 0x0c, struct stmvl53l0_register)
#define VL53L0_IOCTL_PARAMETER \
			_IOWR('p', 0x0d, struct stmvl53l0_parameter)

#define CALIBRATION_FILE 1
#ifdef CALIBRATION_FILE
int8_t offset_calib;
int16_t xtalk_calib;
#endif

static long stmvl53l0_ioctl(struct file *file,
							unsigned int cmd,
							unsigned long arg);
/*static int stmvl53l0_flush(struct file *file, fl_owner_t id);*/
static int stmvl53l0_open(struct inode *inode, struct file *file);
static int stmvl53l0_init_client(struct stmvl53l0_data *data);
static int stmvl53l0_start(struct stmvl53l0_data *data, uint8_t scaling,
			init_mode_e mode);
static int stmvl53l0_stop(struct stmvl53l0_data *data);

#ifdef CALIBRATION_FILE
static void stmvl53l0_read_calibration_file(struct stmvl53l0_data *data)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;
	int i, is_sign = 0;

	f = filp_open("/data/calibration/offset", O_RDONLY, 0);
	if (f != NULL && !IS_ERR(f) && f->f_dentry != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		vl53l0_dbgmsg("offset as:%s, buf[0]:%c\n", buf, buf[0]);
		offset_calib = 0;
		for (i = 0; i < 8; i++) {
			if (i == 0 && buf[0] == '-')
				is_sign = 1;
			else if (buf[i] >= '0' && buf[i] <= '9')
				offset_calib = offset_calib * 10 +
					(buf[i] - '0');
			else
				break;
		}
		if (is_sign == 1)
			offset_calib = -offset_calib;
		vl53l0_dbgmsg("offset_calib as %d\n", offset_calib);
/*later
		VL6180x_SetOffsetCalibrationData(vl53l0_dev, offset_calib);
*/
		filp_close(f, NULL);
	} else {
		vl53l0_errmsg("no offset calibration file exist!\n");
	}

	is_sign = 0;
	f = filp_open("/data/calibration/xtalk", O_RDONLY, 0);
	if (f != NULL && !IS_ERR(f) && f->f_dentry != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_op->read(f, buf, 8, &f->f_pos);
		set_fs(fs);
		vl53l0_dbgmsg("xtalk as:%s, buf[0]:%c\n", buf, buf[0]);
		xtalk_calib = 0;
		for (i = 0; i < 8; i++) {
			if (i == 0 && buf[0] == '-')
				is_sign = 1;
			else if (buf[i] >= '0' && buf[i] <= '9')
				xtalk_calib = xtalk_calib * 10 + (buf[i] - '0');
			else
				break;
		}
		if (is_sign == 1)
			xtalk_calib = -xtalk_calib;
		vl53l0_dbgmsg("xtalk_calib as %d\n", xtalk_calib);
/* later
		VL6180x_SetXTalkCompensationRate(vl53l0_dev, xtalk_calib);
*/
		filp_close(f, NULL);
	} else {
		vl53l0_errmsg("no xtalk calibration file exist!\n");
	}
	return;
}

static void stmvl53l0_write_offset_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/offset", O_WRONLY|O_CREAT, 0644);
	if (f != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		sprintf(buf, "%d", offset_calib);
		vl53l0_dbgmsg("write offset as:%s, buf[0]:%c\n", buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
	}
	filp_close(f, NULL);

	return;
}

static void stmvl53l0_write_xtalk_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;

	f = filp_open("/data/calibration/xtalk", O_WRONLY|O_CREAT, 0644);
	if (f != NULL) {
		fs = get_fs();
		set_fs(get_ds());
		sprintf(buf, "%d", xtalk_calib);
		vl53l0_dbgmsg("write xtalk as:%s, buf[0]:%c\n", buf, buf[0]);
		f->f_op->write(f, buf, 8, &f->f_pos);
		set_fs(fs);
	}
	filp_close(f, NULL);

	return;
}
#endif


static void stmvl53l0_ps_read_measurement(struct stmvl53l0_data *data)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	data->ps_data = data->rangeData.RangeMilliMeter;
	input_report_abs(data->input_dev_ps, ABS_DISTANCE,
		(int)(data->ps_data + 5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X, tv.tv_sec);
	input_report_abs(data->input_dev_ps, ABS_HAT0Y, tv.tv_usec);
	input_report_abs(data->input_dev_ps, ABS_HAT1X,
		data->rangeData.RangeMilliMeter);
	input_report_abs(data->input_dev_ps, ABS_HAT1Y,
		data->rangeData.RangeStatus);
	input_report_abs(data->input_dev_ps, ABS_HAT2X,
		data->rangeData.SignalRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT2Y,
		data->rangeData.AmbientRateRtnMegaCps);
	input_report_abs(data->input_dev_ps, ABS_HAT3X,
		data->rangeData.MeasurementTimeUsec);
	input_report_abs(data->input_dev_ps, ABS_HAT3Y,
		data->rangeData.RangeDMaxMilliMeter);
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		vl53l0_errmsg("range:%d, signalRateRtnMegaCps:%d, \
				error:0x%x,rtnambrate:%u,measuretime:%u\n",
			data->rangeData.RangeMilliMeter,
			data->rangeData.SignalRateRtnMegaCps,
			data->rangeData.RangeStatus,
			data->rangeData.AmbientRateRtnMegaCps,
			data->rangeData.MeasurementTimeUsec);


}

static void stmvl53l0_cancel_handler(struct stmvl53l0_data *data)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	ret = cancel_delayed_work(&data->dwork);
	if (ret == 0)
		vl53l0_errmsg("cancel_delayed_work return FALSE\n");

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

	return;
}

static void stmvl53l0_schedule_handler(struct stmvl53l0_data *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	schedule_delayed_work(&data->dwork, msecs_to_jiffies(data->delay_ms));
	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

	return;
}

/* interrupt work handler */
static void stmvl53l0_work_handler(struct work_struct *work)
{
	struct stmvl53l0_data *data = container_of(work, struct stmvl53l0_data,
				dwork.work);
	VL53L0_DEV vl53l0_dev = data;
	uint8_t reg_val;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
#ifdef DEBUG_TIME_LOG
	long total_sec, total_msec;
#endif

	mutex_lock(&data->work_mutex);

	if (data->enable_ps_sensor == 1) {
#ifdef USE_INT
		VL53L0_RdByte(vl53l0_dev, VL53L0_REG_RESULT_INTERRUPT_STATUS,
			&reg_val);
		if (data->enableDebug)
			pr_err("interrupt status reg as:0x%x, \
				interrupt_received:%d\n",
				reg_val, data->interrupt_received);

		if (data->interrupt_received == 1) {

			data->interrupt_received = 0;
			if ((reg_val & 0x07) > 0) {
				Status = VL53L0_ClearInterruptMask(vl53l0_dev,
						0);
				Status = VL53L0_GetRangingMeasurementData(
						vl53l0_dev, &(data->rangeData));
				if (data->enableDebug)
					pr_err("Measured range:%d\n",
					data->rangeData.RangeMilliMeter);
			}
		}


		if (data->ps_is_singleshot == 1) {
			VL53L0_SetDeviceMode(vl53l0_dev,
				VL53L0_DEVICEMODE_SINGLE_RANGING);
		    Status = VL53L0_StartMeasurement(vl53l0_dev);
		} else if (data->ps_is_started == 0) { /*this is for continuous
			mode  */
			/*VL53L0_SetDeviceMode(vl53l0_dev,
				VL53L0_DEVICEMODE_CONTINUOUS_RANGING); */
			VL53L0_SetDeviceMode(vl53l0_dev,
				VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING);

			VL53L0_SetInterMeasurementPeriodMilliSeconds(vl53l0_dev,
				20);
		    Status = VL53L0_StartMeasurement(vl53l0_dev);
			data->ps_is_started = 1;
		}

#else
		/* vl53l0_dbgmsg("Enter\n"); */
#ifdef DEBUG_TIME_LOG
		do_gettimeofday(&start_tv);
#endif
		Status = VL53L0_PerformSingleRangingMeasurement(vl53l0_dev,
			&(data->rangeData));
#ifdef DEBUG_TIME_LOG
		do_gettimeofday(&stop_tv);
		total_sec = stop_tv.tv_sec - start_tv.tv_sec;
		total_msec = (stop_tv.tv_usec - start_tv.tv_usec)/1000;
		total_msec += total_sec * 1000;
		pr_err("VL6180x_RangeGetMeasurement,\
			elapsedTime:%ld\n", total_msec);
#endif
		/* to push the measurement *
		if (RangingMeasurementData.status not equal NOT VALID)
			stmvl53l0_ps_read_measurement(data);
		*/
		pr_err("Measured range:%d\n",\
			data->rangeData.RangeMilliMeter);

		/* restart timer */
		schedule_delayed_work(&data->dwork,
			msecs_to_jiffies((data->delay_ms)));
#endif
	    /* vl53l0_dbgmsg("End\n"); */
	}

	mutex_unlock(&data->work_mutex);

	return;
}

#ifdef USE_INT
static irqreturn_t stmvl53l0_interrupt_handler(int vec, void *info)
{

	struct stmvl53l0_data *data = (struct stmvl53l0_data *)info;

	if (data->irq == vec) {
		data->interrupt_received = 1;
		schedule_delayed_work(&data->dwork, 0);
	}
	return IRQ_HANDLED;
}
#endif

/*
 * SysFS support
 */
static ssize_t stmvl53l0_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl53l0_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	unsigned long val = simple_strtoul(buf, NULL, 10);

	if ((val != 0) && (val != 1)) {
		vl53l0_errmsg("store unvalid value=%ld\n", val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	vl53l0_dbgmsg("Enter, enable_ps_sensor flag:%d\n",
		data->enable_ps_sensor);
	vl53l0_dbgmsg("enable ps senosr ( %ld)\n", val);

	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, NORMAL_MODE);
		} else {
			vl53l0_errmsg("Already enabled. Skip !");
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
	}
	vl53l0_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUGO | S_IRUGO,
				   stmvl53l0_show_enable_ps_sensor,
					stmvl53l0_store_enable_ps_sensor);

static ssize_t stmvl53l0_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->enableDebug);
}

/* for debug */
static ssize_t stmvl53l0_store_enable_debug(struct device *dev,
					struct device_attribute *attr, const
					char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	long on = simple_strtoul(buf, NULL, 10);

	if ((on != 0) &&  (on != 1)) {
		vl53l0_errmsg("set debug=%ld\n", on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(enable_debug, S_IWUSR | S_IRUGO,
				   stmvl53l0_show_enable_debug,
					stmvl53l0_store_enable_debug);

static ssize_t stmvl53l0_show_set_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l0_store_set_delay_ms(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l0_data *data = dev_get_drvdata(dev);
	long delay_ms = simple_strtoul(buf, NULL, 10);

	if (delay_ms == 0) {
		vl53l0_errmsg("set delay_ms=%ld\n", delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);

	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, S_IWUGO | S_IRUGO,
				   stmvl53l0_show_set_delay_ms,
					stmvl53l0_store_set_delay_ms);

static struct attribute *stmvl53l0_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr ,
	NULL
};


static const struct attribute_group stmvl53l0_attr_group = {
	.attrs = stmvl53l0_attributes,
};

/*
 * misc device file operation functions
 */
static int stmvl53l0_ioctl_handler(struct file *file,
			unsigned int cmd, unsigned long arg,
			void __user *p)
{
	int rc = 0;
	unsigned int xtalkint = 0;
	int8_t offsetint = 0;
	struct stmvl53l0_data *data =
			container_of(file->private_data,
				struct stmvl53l0_data, miscdev);
	struct stmvl53l0_register reg;
	struct stmvl53l0_parameter parameter;
	VL53L0_DEV vl53l0_dev = data;
	uint8_t page_num = 0;
	if (!data)
		return -EINVAL;

	vl53l0_dbgmsg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
	/* enable */
	case VL53L0_IOCTL_INIT:
		vl53l0_dbgmsg("VL53L0_IOCTL_INIT\n");
		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, NORMAL_MODE);
		} else
			rc = -EINVAL;
		break;
	/* crosstalk calibration */
	case VL53L0_IOCTL_XTALKCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_XTALKCALB\n");
		/* turn on tof sensor only if it's not enabled by other
		client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, XTALKCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up Xtalk value */
	case VL53L0_IOCTL_SETXTALK:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p,
			sizeof(unsigned int))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETXTALK as 0x%x\n", xtalkint);
#ifdef CALIBRATION_FILE
		xtalk_calib = xtalkint;
		stmvl53l0_write_xtalk_calibration_file();
#endif
/* later
		VL6180x_SetXTalkCompensationRate(vl53l0_dev, xtalkint);
*/
		break;
	/* offset calibration */
	case VL53L0_IOCTL_OFFCALB:
		vl53l0_dbgmsg("VL53L0_IOCTL_OFFCALB\n");
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl53l0_start(data, 3, OFFSETCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up offset value */
	case VL53L0_IOCTL_SETOFFSET:
		vl53l0_dbgmsg("VL53L0_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl53l0_dbgmsg("SETOFFSET as %d\n", offsetint);
#ifdef CALIBRATION_FILE
		offset_calib = offsetint;
		stmvl53l0_write_offset_calibration_file();
#endif
/* later
		VL6180x_SetOffsetCalibrationData(vl53l0_dev, offsetint);
*/
		break;
	/* disable */
	case VL53L0_IOCTL_STOP:
		vl53l0_dbgmsg("VL53L0_IOCTL_STOP\n");
		/* turn off tof sensor only if it's enabled by other client */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
		break;
	/* Get all range data */
	case VL53L0_IOCTL_GETDATAS:
		vl53l0_dbgmsg("VL53L0_IOCTL_GETDATAS\n");
		if (copy_to_user((VL53L0_RangingMeasurementData_t *)p,
			&(data->rangeData),
			sizeof(VL53L0_RangingMeasurementData_t))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* Register tool */
	case VL53L0_IOCTL_REGISTER:
		vl53l0_dbgmsg("VL53L0_IOCTL_REGISTER\n");
		if (copy_from_user(&reg, (struct stmvl53l0_register *)p,
			sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		page_num = (uint8_t)((reg.reg_index & 0x0000ff00) >> 8);
		vl53l0_dbgmsg("VL53L0_IOCTL_REGISTER,\
			page number:%d\n", page_num);
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, page_num);

		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL53L0_RdDWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					&reg.reg_data);
			else
				reg.status = VL53L0_WrDWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL53L0_RdWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint16_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrWord(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL53L0_RdByte(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint8_t *)&reg.reg_data);
			else
				reg.status = VL53L0_WrByte(vl53l0_dev,
					(uint8_t)reg.reg_index,
					(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;

		}
		if (page_num != 0)
			reg.status = VL53L0_WrByte(vl53l0_dev, 0xFF, 0);


		if (copy_to_user((struct stmvl53l0_register *)p, &reg,
				sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	/* parameter access */
	case VL53L0_IOCTL_PARAMETER:
		vl53l0_dbgmsg("VL53L0_IOCTL_PARAMETER\n");
		if (copy_from_user(&parameter, (struct stmvl53l0_parameter *)p,
				sizeof(struct stmvl53l0_parameter))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		parameter.status = 0;
		switch (parameter.name) {
		case (OFFSET_PAR):
			if (parameter.is_read)
				parameter.status =
				VL53L0_GetOffsetCalibrationDataMicroMeter(
						vl53l0_dev, &parameter.value);
			else
				parameter.status =
				VL53L0_SetOffsetCalibrationDataMicroMeter(
						vl53l0_dev, parameter.value);
			break;
		case (XTALKRATE_PAR):
			if (parameter.is_read)
				parameter.status =
					VL53L0_GetXTalkCompensationRateMegaCps(
						vl53l0_dev, (FixPoint1616_t *)
						&parameter.value);
			else
				parameter.status =
					VL53L0_SetXTalkCompensationRateMegaCps(
						vl53l0_dev,
						(FixPoint1616_t)
							parameter.value);

			break;
		case (XTALKENABLE_PAR):
			if (parameter.is_read)
				parameter.status =
					VL53L0_GetXTalkCompensationEnable(
						vl53l0_dev,
						(uint8_t *) &parameter.value);
			else
				parameter.status =
					VL53L0_SetXTalkCompensationEnable(
						vl53l0_dev,
						(uint8_t) parameter.value);
			break;

		}
		if (copy_to_user((struct stmvl53l0_parameter *)p, &reg,
				sizeof(struct stmvl53l0_register))) {
			vl53l0_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int stmvl53l0_open(struct inode *inode, struct file *file)
{
	return 0;
}
#if 0
static int stmvl53l0_flush(struct file *file, fl_owner_t id)
{
	struct stmvl53l0_data *data = container_of(file->private_data,
						struct stmvl53l0_data, miscdev);
	(void) file;
	(void) id;

	if (data) {
		if (data->enable_ps_sensor == 1) {
			/* turn off tof sensor if it's enabled */
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl53l0_stop(data);
		}
	}
	return 0;
}
#endif
static long stmvl53l0_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret;
	struct stmvl53l0_data *data =
			container_of(file->private_data,
					struct stmvl53l0_data, miscdev);
	mutex_lock(&data->work_mutex);
	ret = stmvl53l0_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&data->work_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl53l0_init_client(struct stmvl53l0_data *data)
{
	uint8_t id = 0, type = 0;
	uint8_t revision = 0, module_id = 0;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceInfo_t DeviceInfo;
	VL53L0_DEV vl53l0_dev = data;

	vl53l0_dbgmsg("Enter\n");


	/* Read Model ID */
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_MODEL_ID, &id);
	vl53l0_errmsg("read MODLE_ID: 0x%x\n", id);
	if (id == 0xee) {
		vl53l0_errmsg("STM VL53L0 Found\n");
	} else if (id == 0) {
		vl53l0_errmsg("Not found STM VL53L0\n");
		return -EIO;
	}
	VL53L0_RdByte(vl53l0_dev, 0xc1, &id);
	vl53l0_errmsg("read 0xc1: 0x%x\n", id);
	VL53L0_RdByte(vl53l0_dev, 0xc2, &id);
	vl53l0_errmsg("read 0xc2: 0x%x\n", id);
	VL53L0_RdByte(vl53l0_dev, 0xc3, &id);
	vl53l0_errmsg("read 0xc3: 0x%x\n", id);

	/* Read Model Version */
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_MODEL_TYPE, &type);
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_REVISION_ID,
		&revision);
	VL53L0_RdByte(vl53l0_dev, VL53L0_REG_IDENTIFICATION_MODULE_ID,
		&module_id);
	vl53l0_errmsg("STM VL53L0 Model type : %d. rev:%d. module:%d\n", type,
		revision, module_id);

	vl53l0_dev->I2cDevAddr      = 0x52;
	vl53l0_dev->comms_type      =  1;
	vl53l0_dev->comms_speed_khz =  400;

	if (Status == VL53L0_ERROR_NONE && data->reset) {
		pr_err("Call of VL53L0_DataInit\n");
		Status = VL53L0_DataInit(vl53l0_dev); /* Data initialization */
		data->reset = 0;
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("VL53L0_GetDeviceInfo:\n");
		Status = VL53L0_GetDeviceInfo(vl53l0_dev, &DeviceInfo);
		if (Status == VL53L0_ERROR_NONE) {
			pr_err("Device Name : %s\n", DeviceInfo.Name);
			pr_err("Device Type : %s\n", DeviceInfo.Type);
			pr_err("ProductType : %d\n", DeviceInfo.ProductType);
			pr_err("ProductRevisionMajor : %d\n",
				DeviceInfo.ProductRevisionMajor);
			pr_err("ProductRevisionMinor : %d\n",
				DeviceInfo.ProductRevisionMinor);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		pr_err("Call of VL53L0_StaticInit\n");
		Status = VL53L0_StaticInit(vl53l0_dev);
		/* Device Initialization */
	}

	if (Status == VL53L0_ERROR_NONE) {

		pr_err("Call of VL53L0_SetDeviceMode\n");
		Status = VL53L0_SetDeviceMode(vl53l0_dev,
					VL53L0_DEVICEMODE_SINGLE_RANGING);
		/* Setup in	single ranging mode */
	}
	/*  Enable/Disable Sigma and Signal check */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_SetSigmaLimitCheckEnable(vl53l0_dev, 0, 1);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_SetSignalLimitCheckEnable(vl53l0_dev, 0, 1);

	/* set user calibration data - need to be called after
	VL6180x_InitData
	*/
#ifdef CALIBRATION_FILE
	/*stmvl53l0_read_calibration_file(data);*/
#endif

	vl53l0_dbgmsg("End\n");

	return 0;
}

static int stmvl53l0_start(struct stmvl53l0_data *data, uint8_t scaling,
	init_mode_e mode)
{
	int rc = 0;
	VL53L0_DEV vl53l0_dev = data;

	vl53l0_dbgmsg("Enter\n");

	/* Power up */
	rc = pmodule_func_tbl->power_up(&data->client_object, &data->reset);
	if (rc) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	/* init */
	rc = stmvl53l0_init_client(data);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		pmodule_func_tbl->power_down(&data->client_object);
		return -EINVAL;
	}

	/* check mode */
	if (mode != NORMAL_MODE)
		VL53L0_SetXTalkCompensationEnable(vl53l0_dev, 0);

	if (mode == OFFSETCALIB_MODE)
		VL53L0_SetOffsetCalibrationDataMicroMeter(vl53l0_dev, 0);
	else if (mode == XTALKCALIB_MODE) {
		FixPoint1616_t XTalkCompensationRateMegaCps;
		/*caltarget distance : 100mm and convert to
		* fixed point 16 16 format
		*/
		VL53L0_PerformXTalkCalibration(vl53l0_dev, (100<<16),
			&XTalkCompensationRateMegaCps);
		pr_err("Xtalk comensation:%u\n", XTalkCompensationRateMegaCps);
	}

	VL53L0_SetGpioConfig(vl53l0_dev, 0, 0,
		VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW,
		VL53L0_INTERRUPTPOLARITY_LOW);
	VL53L0_SetInterruptThresholds(vl53l0_dev, 0, (100<<16), 0);

	/* ps_is_singleshot:	1-> for single shot mode
	*						0-> for continus mode
	*/
	data->ps_is_singleshot = 0;
	data->ps_is_started = 0; /* mainly for continuous mode*/
	data->enable_ps_sensor = 1;

	/* enable work handler */
	stmvl53l0_schedule_handler(data);

	vl53l0_dbgmsg("End\n");

	return rc;
}

static int stmvl53l0_stop(struct stmvl53l0_data *data)
{
	int rc = 0;
	VL53L0_DEV vl53l0_dev = data;

	vl53l0_dbgmsg("Enter\n");

	/* stop - if continuous mode */
	if (data->ps_is_singleshot == 0)
		VL53L0_StopMeasurement(vl53l0_dev);

	/* clean interrupt */
	VL53L0_ClearInterruptMask(vl53l0_dev, 0);

	/* cancel work handler */
	stmvl53l0_cancel_handler(data);
	/* power down */
	rc = pmodule_func_tbl->power_down(&data->client_object);
	if (rc) {
		vl53l0_errmsg("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	vl53l0_dbgmsg("End\n");

	return rc;
}

/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl53l0_ranging_fops = {
	.owner =			THIS_MODULE,
	.unlocked_ioctl =	stmvl53l0_ioctl,
	.open =				stmvl53l0_open,
	/*.flush =			stmvl53l0_flush,*/
};

/*
static struct miscdevice stmvl53l0_ranging_dev = {
	.minor =	MISC_DYNAMIC_MINOR,
	.name =		"stmvl53l0_ranging",
	.fops =		&stmvl53l0_ranging_fops
};
*/

int stmvl53l0_setup(struct stmvl53l0_data *data)
{
	int rc = 0;
#ifdef USE_INT
	int irq = 0;
#endif

	vl53l0_dbgmsg("Enter\n");

	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);

#ifdef USE_INT
	/* init interrupt */
	gpio_request(IRQ_NUM, "vl53l0_gpio_int");
	gpio_direction_input(IRQ_NUM);
	irq = gpio_to_irq(IRQ_NUM);
	if (irq < 0) {
		vl53l0_errmsg("filed to map GPIO: %d to interrupt:%d\n",
			IRQ_NUM, irq);
	} else {
		vl53l0_dbgmsg("register_irq:%d\n", irq);
		/* IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG -
		poliarty:1 */
		rc = request_threaded_irq(irq, NULL,
				stmvl53l0_interrupt_handler,
				IRQF_TRIGGER_FALLING,
				"vl53l0_interrupt",
				(void *)data);
		if (rc) {
			vl53l0_errmsg("%d, Could not allocate STMVL53L0_INT !\
				result:%d\n",  __LINE__, rc);
#ifdef USE_INT
			free_irq(irq, data);
#endif
			goto exit_free_irq;
		}
	}
	data->irq = irq;
	vl53l0_errmsg("interrupt is hooked\n");
#endif

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l0_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
/*
#ifdef USE_INT
		free_irq(irq, data);
#endif
*/
		goto exit_free_irq;
	}
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	/* range in cm*/
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);
	/* tv_sec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 0xffffffff,
		0, 0);
	/* tv_usec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0Y, 0, 0xffffffff,
		0, 0);
	/* range in_mm */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2X, 0, 0xffffffff,
		0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2Y, 0, 0xffffffff,
		0, 0);
	/* rtn_conv_time */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3X, 0, 0xffffffff,
		0, 0);
	/* dmax */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3Y, 0, 0xffffffff,
		0, 0);
	data->input_dev_ps->name = "STM VL53L0 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
/*
#ifdef USE_INT
		free_irq(irq, data);
#endif
		input_free_device(data->input_dev_ps);
		return rc;
*/
		goto exit_free_dev_ps;
	}
	/* setup drv data */
	input_set_drvdata(data->input_dev_ps, data);

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
/*
#ifdef USE_INT
		free_irq(irq, data);
#endif

		input_unregister_device(data->input_dev_ps);
		input_free_device(data->input_dev_ps);
		return rc;
*/
		goto exit_unregister_dev_ps;
	}
	/*rc = sysfs_create_group(data->range_kobj, &stmvl53l0_attr_group);
	 rc = sysfs_create_group(&data->client_object.client->dev.kobj,
			&stmvl53l0_attr_group);
	*/
	rc = sysfs_create_group(&data->input_dev_ps->dev.kobj,
			&stmvl53l0_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl53l0_errmsg("%d error:%d\n", __LINE__, rc);
/*
#ifdef USE_INT
		free_irq(irq, data);
#endif
		kobject_put(data->range_kobj);
		input_unregister_device(data->input_dev_ps);
		input_free_device(data->input_dev_ps);
		return rc;
*/
		goto exit_unregister_dev_ps_1;
	}

	/* to register as a misc device */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = "stmvl53l0_ranging";
	data->miscdev.fops = &stmvl53l0_ranging_fops;
	vl53l0_errmsg("Misc device registration name:%s\n", data->dev_name);
	if (misc_register(&data->miscdev) != 0)
		vl53l0_errmsg("Could not register misc. dev for stmvl53l0\
				ranging\n");

	/* init default value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 30;	/* delay time to 30ms */
	data->enableDebug = 0;
	data->client_object.power_up = 0;
	/* for those one-the-fly power on/off flag */

	vl53l0_dbgmsg("support ver. %s enabled\n", DRIVER_VERSION);
	vl53l0_dbgmsg("End");

	return 0;
exit_unregister_dev_ps_1:
	kobject_put(data->range_kobj);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_irq:
#ifdef USE_INT
	free_irq(irq, data);
#endif
	kfree(data);
	return rc;
}

static int __init stmvl53l0_init(void)
{
	int ret = -1;

	vl53l0_dbgmsg("Enter\n");

	/* assign function table */
	pmodule_func_tbl = &stmvl53l0_module_func_tbl;
#if 0
	vl53l0_data = kzalloc(sizeof(struct stmvl53l0_data), GFP_KERNEL);
	if (!vl53l0_data) {
		vl53l0_errmsg("%d failed no memory\n", __LINE__);
		return -ENOMEM;
	}
	/* assign to global variable */
	gp_vl53l0_data = vl53l0_data;
	/* assign function table */
	vl53l0_data->pmodule_func_tbl = &stmvl53l0_module_func_tbl;
#endif
	/* client specific init function */
	ret = pmodule_func_tbl->init();
#if 0
	if (!ret)
		ret = stmvl53l0_setup(vl53l0_data);
#endif
	if (ret)
		vl53l0_errmsg("%d failed with %d\n", __LINE__, ret);

	vl53l0_dbgmsg("End\n");

	return ret;
}

static void __exit stmvl53l0_exit(void)
{
	vl53l0_dbgmsg("Enter\n");
#if 0
	if (gp_vl53l0_data) {
		input_unregister_device(gp_vl53l0_data->input_dev_ps);
		input_free_device(gp_vl53l0_data->input_dev_ps);
#ifdef USE_INT
		free_irq(data->irq, gp_vl53l0_data);
#endif
		sysfs_remove_group(gp_vl53l0_data->range_kobj,
&stmvl53l0_attr_group);

gp_vl53l0_data->pmodule_func_tbl->deinit(&gp_vl53l0_data->client_object);
		kfree(gp_vl53l0_data);
		gp_vl53l0_data = NULL;
	}
#endif
	vl53l0_dbgmsg("End\n");
}


MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl53l0_init);
module_exit(stmvl53l0_exit);

