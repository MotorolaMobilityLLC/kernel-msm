/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense TOF sensor
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
#include <asm/uaccess.h>
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
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
/*
 * API includes
 */
#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "vl6180x_i2c.h"
#include "stmvl6180-i2c.h"
#include "stmvl6180-cci.h"
#include "stmvl6180.h"

/* #define USE_INT */
#define IRQ_NUM	   59

/*
 * Global data
 */
stmvl6180x_dev vl6180x_dev;
static struct stmvl6180_data *gp_vl6180_data;

#ifdef CAMERA_CCI
static struct stmvl6180_module_fn_t stmvl6180_module_func_tbl = {
	.init = stmvl6180_init_cci,
	.deinit = stmvl6180_exit_cci,
	.power_up = stmvl6180_power_up_cci,
	.power_down = stmvl6180_power_down_cci,
};
#else
static struct stmvl6180_module_fn_t stmvl6180_module_func_tbl = {
	.init = stmvl6180_init_i2c,
	.deinit = stmvl6180_exit_i2c,
	.power_up = stmvl6180_power_up_i2c,
	.power_down = stmvl6180_power_down_i2c,
};
#endif

/*
 * IOCTL definitions
 */
#define VL6180_IOCTL_INIT			_IO('p', 0x01)
#define VL6180_IOCTL_XTALKCALB		_IO('p', 0x02)
#define VL6180_IOCTL_OFFCALB		_IO('p', 0x03)
#define VL6180_IOCTL_STOP			_IO('p', 0x05)
#define VL6180_IOCTL_SETXTALK		_IOW('p', 0x06, unsigned int)
#define VL6180_IOCTL_SETOFFSET		_IOW('p', 0x07, int8_t)
#define VL6180_IOCTL_GETDATAS		_IOR('p', 0x0b, VL6180x_RangeData_t)
#define VL6180_IOCTL_REGISTER		_IOWR('p', 0x0c, struct stmvl6180_register)

#define CALIBRATION_FILE 1
#ifdef CALIBRATION_FILE

#define CAL_FILE_OFFSET "/persist/data/st_offset"
#define CAL_FILE_XTALK "/persist/data/st_xtalk"
#define CAL_FILE_MODE 0640

int8_t offset_calib = 0;
int16_t xtalk_calib = 0;
#endif

static long stmvl6180_ioctl(struct file *file,
							unsigned int cmd,
							unsigned long arg);
static int stmvl6180_flush(struct file *file, fl_owner_t id);
static int stmvl6180_open(struct inode *inode, struct file *file);
static int stmvl6180_init_client(struct stmvl6180_data *data);
static int stmvl6180_start(struct stmvl6180_data *data, uint8_t scaling, init_mode_e mode);
static int stmvl6180_stop(struct stmvl6180_data *data);

#ifdef CALIBRATION_FILE
static void stmvl6180_read_calibration_file(void)
{
	struct file *f;
	char buf[8];
	mm_segment_t fs;
	int i, is_sign = 0;

	f = filp_open(CAL_FILE_OFFSET, O_RDONLY, CAL_FILE_MODE);
	if (IS_ERR_OR_NULL(f)) {
		pr_err("no offset calibration file exist! %ld\n", (long)f);
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_pos = 0;
		vfs_read(f, buf, 8, &f->f_pos);
		pr_info("offset as:%s, buf[0]:%c\n", buf, buf[0]);
		offset_calib = 0;
		for (i = 0; i < 8; i++) {
			if (i == 0 && buf[0] == '-')
				is_sign = 1;
			else if (buf[i] >= '0' && buf[i] <= '9')
				offset_calib =
				    offset_calib * 10 + (buf[i] - '0');
			else
				break;
		}
		if (is_sign == 1)
			offset_calib = -offset_calib;
		pr_info("offset_calib as %d\n", offset_calib);
		VL6180x_SetOffsetCalibrationData(vl6180x_dev, offset_calib);
		filp_close(f, NULL);
		set_fs(fs);
	}

	is_sign = 0;
	f = filp_open(CAL_FILE_XTALK, O_RDONLY, CAL_FILE_MODE);
	if (IS_ERR_OR_NULL(f)) {
		pr_info("no xtalk calibration file exist! %ld\n", (long)f);
	} else {
		fs = get_fs();
		set_fs(KERNEL_DS);
		/* init the buffer with 0 */
		for (i = 0; i < 8; i++)
			buf[i] = 0;
		f->f_pos = 0;
		vfs_read(f, buf, 8, &f->f_pos);
		pr_info("xtalk as:%s, buf[0]:%c\n", buf, buf[0]);
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
		pr_info("xtalk_calib as %d\n", xtalk_calib);
		VL6180x_SetXTalkCompensationRate(vl6180x_dev, xtalk_calib);
		filp_close(f, NULL);
		set_fs(fs);
	}

	return;
}

static void stmvl6180_write_offset_calibration_file(void)
{
	struct file *f = NULL;
	char buf[8];
	mm_segment_t fs;

	f = filp_open(CAL_FILE_OFFSET, O_CREAT | O_TRUNC | O_RDWR,
		      CAL_FILE_MODE);
	if (IS_ERR_OR_NULL(f)) {
		pr_err("fail open calibration file, %ld\n", (long)f);
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	f->f_pos = 0;
	sprintf(buf, "%d", offset_calib);
	vfs_write(f, buf, 8, &f->f_pos);
	filp_close(f, NULL);
	set_fs(fs);

	return;
}

static void stmvl6180_write_xtalk_calibration_file(void)
{
	struct file *f = NULL;
	char buf[8];
	mm_segment_t fs;

	f = filp_open(CAL_FILE_XTALK, O_CREAT | O_TRUNC | O_RDWR,
		      CAL_FILE_MODE);
	if (IS_ERR_OR_NULL(f)) {
		pr_err("fail open xtalk file, %ld\n", (long)f);
		return;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	f->f_pos = 0;
	sprintf(buf, "%d", xtalk_calib);
	vfs_write(f, buf, 8, &f->f_pos);
	filp_close(f, NULL);
	set_fs(fs);

	return;
}
#endif


static void stmvl6180_ps_read_measurement(void)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	struct timeval tv;

	do_gettimeofday(&tv);

	data->ps_data = data->rangeData.range_mm;
	input_report_abs(data->input_dev_ps, ABS_DISTANCE, (int)(data->ps_data +  5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X, tv.tv_sec);
	input_report_abs(data->input_dev_ps, ABS_HAT0Y, tv.tv_usec);
	input_report_abs(data->input_dev_ps, ABS_HAT1X, data->rangeData.range_mm);
	input_report_abs(data->input_dev_ps, ABS_HAT1Y, data->rangeData.errorStatus);
#ifdef VL6180x_HAVE_RATE_DATA
	input_report_abs(data->input_dev_ps, ABS_HAT2X, data->rangeData.signalRate_mcps);
	input_report_abs(data->input_dev_ps, ABS_HAT2Y, data->rangeData.rtnAmbRate);
	input_report_abs(data->input_dev_ps, ABS_HAT3X, data->rangeData.rtnConvTime);
#endif
#if  VL6180x_HAVE_DMAX_RANGING
	input_report_abs(data->input_dev_ps, ABS_HAT3Y, data->rangeData.DMax);
#endif
	input_sync(data->input_dev_ps);

	if (data->enableDebug)
		pr_info
		    ("range:%d, signalrate_mcps:%d, error:0x%x,rtnsgnrate:%u, rtnambrate:%u,rtnconvtime:%u\n",
			data->rangeData.range_mm,
			data->rangeData.signalRate_mcps,
			data->rangeData.errorStatus,
			data->rangeData.rtnRate,
			data->rangeData.rtnAmbRate,
			data->rangeData.rtnConvTime);
}

static void stmvl6180_cancel_handler(struct stmvl6180_data *data)
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
		vl6180_errmsg("cancel_delayed_work return FALSE\n");

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

	return;
}

static void stmvl6180_schedule_handler(struct stmvl6180_data *data)
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
static void stmvl6180_work_handler(struct work_struct *work)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	int ret = 0;
	uint8_t to_startPS = 0;
	mutex_lock(&data->work_mutex);

	if (data->enable_ps_sensor == 1) {
		uint8_t range_status=0, range_start=0;
		data->rangeData.errorStatus = 0xFF; /* to reset the data, should be set by API */
                VL6180x_RdByte(vl6180x_dev, RESULT_RANGE_STATUS, &range_status);
                VL6180x_RdByte(vl6180x_dev, SYSRANGE_START, &range_start);	
		if (data->enableDebug) {
			vl6180_errmsg("RangeStatus as 0x%x,RangeStart: 0x%x\n",range_status,range_start);
		}
		if (range_start == 0 && (range_status & 0x01) == 0x01) {
			ret = VL6180x_RangeGetMeasurementIfReady(vl6180x_dev, &(data->rangeData));
			if (!ret && data->rangeData.errorStatus !=DataNotReady ) {
				if (data->ps_is_singleshot)
					to_startPS = 1;
				if (data->rangeData.errorStatus == 17)
					data->rangeData.errorStatus = 16;
				stmvl6180_ps_read_measurement(); /* to update data */
			}
		}
		//if (!ret)
		//	stmvl6180_ps_read_measurement(); /* to update data */
		if (to_startPS)
			VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP |  MODE_SINGLESHOT);

	schedule_delayed_work(&data->dwork, msecs_to_jiffies((data->delay_ms)));	/* restart timer */

	}

	mutex_unlock(&data->work_mutex);

	return;
}

#ifdef USE_INT
static irqreturn_t stmvl6180_interrupt_handler(int vec, void *info)
{

	struct stmvl6180_data *data = gp_vl6180_data;

	if (data->irq == vec) {
		vl6180_dbgmsg("==>interrupt_handler\n");
		schedule_delayed_work(&data->dwork, 0);
	}
	return IRQ_HANDLED;
}
#endif

/*
 * SysFS support
 */
static ssize_t stmvl6180_show_enable_ps_sensor(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	return sprintf(buf, "%d\n", gp_vl6180_data->enable_ps_sensor);
}

static ssize_t stmvl6180_store_enable_ps_sensor(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	unsigned long val = simple_strtoul(buf, NULL, 10);

	vl6180_dbgmsg("enable ps senosr ( %ld)\n", val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s:store unvalid value=%ld\n", __func__, val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	if (val == 1) {
		/* turn on tof sensor */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl6180_start(data, 3, NORMAL_MODE);
		} else {
			vl6180_errmsg("Already enabled. Skip !");
		}
	} else {
		/* turn off tof sensor */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl6180_stop(data);
		}
	}
	vl6180_dbgmsg("End\n");
	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUGO | S_IRUGO,
		   stmvl6180_show_enable_ps_sensor,
		   stmvl6180_store_enable_ps_sensor);

static ssize_t stmvl6180_show_enable_debug(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%d\n", gp_vl6180_data->enableDebug);
}

/* for als integration time setup */
static ssize_t stmvl6180_store_enable_debug(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	long on = simple_strtoul(buf, NULL, 10);

	if ((on != 0) &&  (on != 1)) {
		vl6180_errmsg("set debug=%ld\n", on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

//DEVICE_ATTR(name,mode,show,store)
static DEVICE_ATTR(enable_debug, S_IWUSR | S_IRUGO,
		   stmvl6180_show_enable_debug, stmvl6180_store_enable_debug);

static ssize_t stmvl6180_show_set_delay_ms(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	return sprintf(buf, "%d\n", data->delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl6180_store_set_delay_ms(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	long delay_ms = simple_strtoul(buf, NULL, 10);
	//printk("stmvl6180_store_set_delay_ms as %ld======\n",delay_ms);
	if (delay_ms == 0) {
		pr_err("%s: set delay_ms=%ld\n", __func__, delay_ms);
		return count;
	}
	mutex_lock(&data->work_mutex);
	data->delay_ms = delay_ms;
	mutex_unlock(&data->work_mutex);
	return count;
}

/* DEVICE_ATTR(name,mode,show,store) */
static DEVICE_ATTR(set_delay_ms, S_IWUGO | S_IRUGO,
				   stmvl6180_show_set_delay_ms, stmvl6180_store_set_delay_ms);

static struct attribute *stmvl6180_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_set_delay_ms.attr,
	NULL
};

static const struct attribute_group stmvl6180_attr_group = {
	.attrs = stmvl6180_attributes,
};

/*
 * misc device file operation functions
 */
static int stmvl6180_ioctl_handler(struct file *file,
				   unsigned int cmd, unsigned long arg,
				   void __user * p)
{

	int rc = 0;
	unsigned int xtalkint = 0;
	int8_t offsetint = 0;
	struct stmvl6180_data *data = gp_vl6180_data;
	struct stmvl6180_register reg;

	if (!data)
		return -EINVAL;

	vl6180_dbgmsg("Enter enable_ps_sensor:%d\n", data->enable_ps_sensor);
	switch (cmd) {
	/* enable */
	case VL6180_IOCTL_INIT:
		vl6180_dbgmsg("VL6180_IOCTL_INIT\n");
		/* turn on tof sensor only if it's not enabled by other client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl6180_start(data, 3, NORMAL_MODE);
		} else
			rc = -EINVAL;
		break;
	/* crosstalk calibration */
	case VL6180_IOCTL_XTALKCALB:
		vl6180_dbgmsg("VL6180_IOCTL_XTALKCALB\n");
		/* turn on tof sensor only if it's not enabled by other client */
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl6180_start(data, 3, XTALKCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up Xtalk value */
	case VL6180_IOCTL_SETXTALK:
		vl6180_dbgmsg("VL6180_IOCTL_SETXTALK\n");
		if (copy_from_user(&xtalkint, (unsigned int *)p, sizeof(unsigned int))) {
			vl6180_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl6180_dbgmsg("SETXTALK as 0x%x\n", xtalkint);
#ifdef CALIBRATION_FILE
		xtalk_calib = xtalkint;
		stmvl6180_write_xtalk_calibration_file();
#endif
		VL6180x_SetXTalkCompensationRate(vl6180x_dev, xtalkint);
		break;
	/* offset calibration */
	case VL6180_IOCTL_OFFCALB:
		vl6180_dbgmsg("VL6180_IOCTL_OFFCALB\n");
		if (data->enable_ps_sensor == 0) {
			/* to start */
			stmvl6180_start(data, 3, OFFSETCALIB_MODE);
		} else
			rc = -EINVAL;
		break;
	/* set up offset value */
	case VL6180_IOCTL_SETOFFSET:
		vl6180_dbgmsg("VL6180_IOCTL_SETOFFSET\n");
		if (copy_from_user(&offsetint, (int8_t *)p, sizeof(int8_t))) {
			vl6180_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		vl6180_dbgmsg("SETOFFSET as %d\n", offsetint);
#ifdef CALIBRATION_FILE
		offset_calib = offsetint;
		stmvl6180_write_offset_calibration_file();
#endif
		VL6180x_SetOffsetCalibrationData(vl6180x_dev, offsetint);
		break;
	case VL6180_IOCTL_STOP:
		vl6180_dbgmsg("VL6180_IOCTL_STOP\n");
		/* turn off tof sensor only if it's enabled by other client */
		if (data->enable_ps_sensor == 1) {
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl6180_stop(data);
		}
		break;
	/* Get all range data */
	case VL6180_IOCTL_GETDATAS:
		vl6180_dbgmsg("VL6180_IOCTL_GETDATAS\n");
		if (copy_to_user((VL6180x_RangeData_t *)p, &(data->rangeData), sizeof(VL6180x_RangeData_t))) {
			vl6180_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	case VL6180_IOCTL_REGISTER:
		vl6180_dbgmsg("VL6180_IOCTL_REGISTER\n");
		if (copy_from_user(&reg, (struct stmvl6180_register *)p,
							sizeof(struct stmvl6180_register))) {
			vl6180_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		reg.status = 0;
		switch (reg.reg_bytes) {
		case(4):
			if (reg.is_read)
				reg.status = VL6180x_RdDWord(vl6180x_dev, (uint16_t)reg.reg_index,
											&reg.reg_data);
			else
				reg.status = VL6180x_WrDWord(vl6180x_dev, (uint16_t)reg.reg_index,
											reg.reg_data);
			break;
		case(2):
			if (reg.is_read)
				reg.status = VL6180x_RdWord(vl6180x_dev, (uint16_t)reg.reg_index,
											(uint16_t *)&reg.reg_data);
			else
				reg.status = VL6180x_WrWord(vl6180x_dev, (uint16_t)reg.reg_index,
											(uint16_t)reg.reg_data);
			break;
		case(1):
			if (reg.is_read)
				reg.status = VL6180x_RdByte(vl6180x_dev, (uint16_t)reg.reg_index,
											(uint8_t *)&reg.reg_data);
			else
				reg.status = VL6180x_WrByte(vl6180x_dev, (uint16_t)reg.reg_index,
											(uint8_t)reg.reg_data);
			break;
		default:
			reg.status = -1;
		}
		if (copy_to_user((struct stmvl6180_register *)p, &reg,
							sizeof(struct stmvl6180_register))) {
			vl6180_errmsg("%d, fail\n", __LINE__);
			return -EFAULT;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int stmvl6180_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int stmvl6180_flush(struct file *file, fl_owner_t id)
{
	struct stmvl6180_data *data = gp_vl6180_data;
	(void) file;
	(void) id;

	if (data) {
		if (data->enable_ps_sensor == 1) {
			/* turn off tof sensor if it's enabled */
			data->enable_ps_sensor = 0;
			/* to stop */
			stmvl6180_stop(data);
		}
	}
	return 0;
}

static long stmvl6180_ioctl(struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	int ret;

	mutex_lock(&gp_vl6180_data->work_mutex);
	ret = stmvl6180_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&gp_vl6180_data->work_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl6180_init_client(struct stmvl6180_data *data)
{
	uint8_t id = 0, module_major = 0, module_minor = 0;
	uint8_t model_major = 0, model_minor = 0;
	uint8_t i = 0, val;

	/* Read Model ID */
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_ID_REG, &id);
	pr_info("read MODLE_ID: 0x%x\n", id);
	if (id == 0xb4) {
		pr_info("STM VL6180 Found\n");
	} else if (id == 0) {
		pr_info("Not found STM VL6180\n");
		return -EIO;
	}
	/* Read Model Version */
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_REV_MAJOR_REG, &model_major);
	model_major &= 0x07;
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_REV_MINOR_REG, &model_minor);
	model_minor &= 0x07;
	pr_info("STM VL6180 Model Version : %d.%d\n", model_major, model_minor);

	/* Read Module Version */
	VL6180x_RdByte(vl6180x_dev, VL6180_MODULE_REV_MAJOR_REG, &module_major);
	VL6180x_RdByte(vl6180x_dev, VL6180_MODULE_REV_MINOR_REG, &module_minor);
	pr_info("STM VL6180 Module Version : %d.%d\n", module_major,
		module_minor);

	/* Read Identification */
	pr_info("STM VL6180 Serial Numbe: ");
	for (i = 0; i <= (VL6180_FIRMWARE_REVISION_ID_REG - VL6180_REVISION_ID_REG); i++) {
		VL6180x_RdByte(vl6180x_dev, (VL6180_REVISION_ID_REG + i), &val);
		pr_info("0x%x-", val);
	}
	pr_info("\n");


	/* intialization */
	if (data->reset) {
		/* no need
		vl6180_dbgmsg("WaitDeviceBoot");
		VL6180x_WaitDeviceBooted(vl6180x_dev);
		*/
		/* only called if device being reset, otherwise data being overwrite */
		vl6180_dbgmsg("Init data!");
		VL6180x_InitData(vl6180x_dev); /* only called if device being reset */
		data->reset = 0;
	}
	/* set user calibration data - need to be called after VL6180x_InitData */
#ifdef CALIBRATION_FILE
	stmvl6180_read_calibration_file();
#endif


	return 0;
}

static int stmvl6180_start(struct stmvl6180_data *data, uint8_t scaling, init_mode_e mode)
{
	int rc = 0;
	vl6180_dbgmsg("Enter\n");

	/* Power up */
	rc = data->pmodule_func_tbl->power_up(&data->client_object, &data->reset);
	if (rc) {
		vl6180_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	/* init */
	rc = stmvl6180_init_client(data);
	if (rc) {
		vl6180_errmsg("%d, error rc %d\n", __LINE__, rc);
		data->pmodule_func_tbl->power_down(&data->client_object);
		return -EINVAL;
	}
	/* prepare */
	VL6180x_Prepare(vl6180x_dev);
	VL6180x_UpscaleSetScaling(vl6180x_dev, scaling);

	/* check mode */
	if (mode != NORMAL_MODE) {
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
		/* turn off wrap around filter */
		VL6180x_FilterSetState(vl6180x_dev, 0);
#endif
		VL6180x_SetXTalkCompensationRate(vl6180x_dev, 0);
	}
	if (mode == OFFSETCALIB_MODE)
		VL6180x_SetOffsetCalibrationData(vl6180x_dev, 0);

	/* start - single shot mode */
	VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP|MODE_SINGLESHOT);
	data->ps_is_singleshot = 1;
	data->enable_ps_sensor = 1;

	/* enable work handler */
	stmvl6180_schedule_handler(data);


	return rc;
}

static int stmvl6180_stop(struct stmvl6180_data *data)
{
	int rc = 0;
	vl6180_dbgmsg("Enter\n");

	/* stop - if continuous mode */
	if (data->ps_is_singleshot == 0)
		VL6180x_RangeSetSystemMode(vl6180x_dev, MODE_START_STOP);
	/* clean interrupt */
	VL6180x_RangeClearInterrupt(vl6180x_dev);
	/* cancel work handler */
	stmvl6180_cancel_handler(data);
	/* power down */
	rc = data->pmodule_func_tbl->power_down(&data->client_object);
	if (rc) {
		vl6180_errmsg("%d, error rc %d\n", __LINE__, rc);
		return rc;
	}
	vl6180_dbgmsg("End\n");

	return rc;
}

/*
 * I2C init/probing/exit functions
 */
static const struct file_operations stmvl6180_ranging_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = stmvl6180_ioctl,
	.open = stmvl6180_open,
	.flush = stmvl6180_flush,
};

static struct miscdevice stmvl6180_ranging_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "stmvl6180_ranging",
	.fops = &stmvl6180_ranging_fops
};

static int stmvl6180_setup(struct stmvl6180_data *data)
{
	int rc = 0;
#ifdef USE_INT
	int irq = 0;
#endif

	vl6180_dbgmsg("Enter\n");

	/* init mutex */
	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);

#ifdef USE_INT
	/* init interrupt */
	gpio_request(IRQ_NUM, "vl6180_gpio_int");
	gpio_direction_input(IRQ_NUM);
	irq = gpio_to_irq(IRQ_NUM);
	if (irq < 0) {
		vl6180_errmsg("filed to map GPIO: %d to interrupt:%d\n", IRQ_NUM, irq);
	} else {
		int result;
		vl6180_dbgmsg("register_irq:%d\n", irq);
		/* IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG - poliarty:1 */
		rc = request_threaded_irq(irq, NULL, stmvl6180_interrupt_handler, IRQF_TRIGGER_RISING, "vl6180_lb_gpio_int", (void *)data);
		if (rc) {
			vl6180_errmsg("%d, Could not allocate STMVL6180_INT ! result:%d\n",  __LINE__, result);
			return rc;
		}
	}
	data->irq = irq;
	vl6180_dbgmsg("interrupt is hooked\n");
#endif

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl6180_work_handler);

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		rc = -ENOMEM;
		vl6180_errmsg("%d error:%d\n", __LINE__, rc);
#ifdef USE_INT
		free_irq(irq, data);
#endif
		return rc;
	}
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	/* range in cm*/
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);
	/* tv_sec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 0xffffffff, 0, 0);
	/* tv_usec */
	input_set_abs_params(data->input_dev_ps, ABS_HAT0Y, 0, 0xffffffff, 0, 0);
	/* range in_mm */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(data->input_dev_ps, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2X, 0, 0xffffffff, 0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(data->input_dev_ps, ABS_HAT2Y, 0, 0xffffffff, 0, 0);
	/* rtn_conv_time */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3X, 0, 0xffffffff, 0, 0);
	/* dmax */
	input_set_abs_params(data->input_dev_ps, ABS_HAT3Y, 0, 0xffffffff, 0, 0);
	data->input_dev_ps->name = "STM VL6180 proximity sensor";

	rc = input_register_device(data->input_dev_ps);
	if (rc) {
		rc = -ENOMEM;
		vl6180_errmsg("%d error:%d\n", __LINE__, rc);
#ifdef USE_INT
		free_irq(irq, data);
#endif
		input_free_device(data->input_dev_ps);
		return rc;
	}

	/* Register sysfs hooks */
	data->range_kobj = kobject_create_and_add("range", kernel_kobj);
	if (!data->range_kobj) {
		rc = -ENOMEM;
		vl6180_errmsg("%d error:%d\n", __LINE__, rc);
#ifdef USE_INT
		free_irq(irq, data);
#endif
		input_unregister_device(data->input_dev_ps);
		input_free_device(data->input_dev_ps);
		return rc;
	}
	rc = sysfs_create_group(data->range_kobj, &stmvl6180_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl6180_errmsg("%d error:%d\n", __LINE__, rc);
#ifdef USE_INT
		free_irq(irq, data);
#endif
		kobject_put(data->range_kobj);
		input_unregister_device(data->input_dev_ps);
		input_free_device(data->input_dev_ps);
		return rc;
	}

	/* to register as a misc device */
	if (misc_register(&stmvl6180_ranging_dev) != 0)
		vl6180_errmsg("Could not register misc. dev for stmvl6180 ranging\n");

	/* init default value */
	data->enable_ps_sensor = 0;
	data->reset = 1;
	data->delay_ms = 30;	/* delay time to 30ms */
	data->enableDebug = 0;
	data->client_object.power_up = 0; /* for those one-the-fly power on/off flag */

	vl6180_dbgmsg("support ver. %s enabled\n", DRIVER_VERSION);
	vl6180_dbgmsg("End");

	return rc;
}

static int __init stmvl6180_init(void)
{
	struct stmvl6180_data *vl6180_data = NULL;
	int ret = 0;

	vl6180_dbgmsg("Enter\n");

	vl6180_data = kzalloc(sizeof(struct stmvl6180_data), GFP_KERNEL);
	if (!vl6180_data) {
		vl6180_errmsg("%d failed no memory\n", __LINE__);
		return -ENOMEM;
	}
	/* assign to global variable */
	gp_vl6180_data = vl6180_data;
	/* assign function table */
	vl6180_data->pmodule_func_tbl = &stmvl6180_module_func_tbl;
	/* client specific init function */
	ret = vl6180_data->pmodule_func_tbl->init();
	if (!ret)
		ret = stmvl6180_setup(vl6180_data);
	if (ret) {
		kfree(vl6180_data);
		gp_vl6180_data = NULL;
		vl6180_errmsg("%d failed with %d\n", __LINE__, ret);
	}
	vl6180_dbgmsg("End\n");

	return ret;
}

static void __exit stmvl6180_exit(void)
{
	vl6180_dbgmsg("Enter\n");
	if (gp_vl6180_data) {
		input_unregister_device(gp_vl6180_data->input_dev_ps);
		input_free_device(gp_vl6180_data->input_dev_ps);
#ifdef USE_INT
		free_irq(data->irq, gp_vl6180_data);
#endif
		sysfs_remove_group(gp_vl6180_data->range_kobj, &stmvl6180_attr_group);
		gp_vl6180_data->pmodule_func_tbl->deinit(&gp_vl6180_data->client_object);
		kfree(gp_vl6180_data);
		gp_vl6180_data = NULL;
	}
	vl6180_dbgmsg("End\n");
}

struct stmvl6180_data *stmvl6180_getobject(void)
{
	return gp_vl6180_data;
}

MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl6180_init);
module_exit(stmvl6180_exit);
