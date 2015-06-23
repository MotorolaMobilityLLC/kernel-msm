/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense Time-of-Flight sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division.
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
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "stmvl6180.h"
stmvl6180x_dev vl6180x_dev;
//#define USE_INT
#define IRQ_NUM        59
#define VL6180_I2C_ADDRESS  (0x52>>1)

/*
 * Global data
 *
******************************** IOCTL definitions
*/
#define VL6180_IOCTL_INIT       _IO('p', 0x01)
#define VL6180_IOCTL_XTALKCALB      _IO('p', 0x02)
#define VL6180_IOCTL_OFFCALB        _IO('p', 0x03)
#define VL6180_IOCTL_STOP       _IO('p', 0x05)
#define VL6180_IOCTL_SETXTALK       _IOW('p', 0x06, unsigned int)
#define VL6180_IOCTL_SETOFFSET      _IOW('p', 0x07, int8_t)
#define VL6180_IOCTL_GETDATA        _IOR('p', 0x0a, unsigned long)
#define VL6180_IOCTL_GETDATAS       _IOR('p', 0x0b, VL6180x_RangeData_t)
#define VL6180_IOCTL_READCALB       _IO('p', 0x0c)
struct mutex vl6180_mutex;
static unsigned int cs_gpio_num;

#define CALIBRATION_FILE 1
#ifdef CALIBRATION_FILE

#define CAL_FILE_OFFSET "/persist/data/st_offset"
#define CAL_FILE_XTALK "/persist/data/st_xtalk"
#define CAL_FILE_MODE 0640

int8_t offset_calib = 0;
int16_t xtalk_calib = 0;
#endif
extern void i2c_setclient(struct i2c_client *client);
struct i2c_client *i2c_getclient(void);
static int stmvl6180_set_enable(struct i2c_client *client, unsigned int enable)
{

	return 0;

}

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
		VL6180x_SetUserOffsetCalibration(vl6180x_dev, offset_calib);
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
		VL6180x_SetUserXTalkCompensationRate(vl6180x_dev, xtalk_calib);
		filp_close(f, NULL);
		set_fs(fs);
	}

	return;
}

static void stmvl6180_write_offset_calibration_file(int8_t data)
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
	sprintf(buf, "%d", data);
	vfs_write(f, buf, 8, &f->f_pos);
	VL6180x_SetUserOffsetCalibration(vl6180x_dev, offset_calib);
	filp_close(f, NULL);
	set_fs(fs);

	return;
}

static void stmvl6180_write_xtalk_calibration_file(int data)
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
	sprintf(buf, "%d", data);
	vfs_write(f, buf, 8, &f->f_pos);
	VL6180x_SetUserXTalkCompensationRate(vl6180x_dev, xtalk_calib);
	filp_close(f, NULL);
	set_fs(fs);

	return;
}

#endif
static void stmvl6180_ps_read_measurement(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	VL6180x_RangeGetMeasurement(vl6180x_dev, &(data->rangeData));

	data->ps_data = data->rangeData.range_mm;

	input_report_abs(data->input_dev_ps, ABS_DISTANCE,
			 (int)(data->ps_data + 5) / 10);
	input_report_abs(data->input_dev_ps, ABS_HAT0X,
			 data->rangeData.range_mm);
	input_report_abs(data->input_dev_ps, ABS_X,
			 data->rangeData.signalRate_mcps);
	input_sync(data->input_dev_ps);
	if (data->enableDebug)
		pr_info
		    ("range:%d, signalrate_mcps:%d, error:0x%x,rtnsgnrate:%u, rtnambrate:%u,rtnconvtime:%u\n",
		     data->rangeData.range_mm, data->rangeData.signalRate_mcps,
		     data->rangeData.errorStatus, data->rangeData.rtnRate,
		     data->rangeData.rtnAmbRate, data->rangeData.rtnConvTime);
}

/* interrupt work handler */
static void stmvl6180_work_handler(struct work_struct *work)
{
	struct stmvl6180_data *data =
	    container_of(work, struct stmvl6180_data, dwork.work);
	struct i2c_client *client = data->client;
	uint8_t gpio_status = 0;
	uint8_t to_startPS = 0;

	mutex_lock(&data->work_mutex);
	VL6180x_RangeGetInterruptStatus(vl6180x_dev, &gpio_status);
	if (gpio_status == RES_INT_STAT_GPIO_NEW_SAMPLE_READY) {
		if (data->enable_ps_sensor) {
			stmvl6180_ps_read_measurement(client);
			if (data->ps_is_singleshot)
				to_startPS = 1;
		}
		VL6180x_RangeClearInterrupt(vl6180x_dev);

	}

	if (to_startPS) {
		VL6180x_RangeSetSystemMode(vl6180x_dev,
					   MODE_START_STOP | MODE_SINGLESHOT);
	}
	schedule_delayed_work(&data->dwork, msecs_to_jiffies((INT_POLLING_DELAY)));	/* restart timer */
	mutex_unlock(&data->work_mutex);
	return;
}

#ifdef USE_INT
static irqreturn_t stmvl6180_interrupt_handler(int vec, void *info)
{
	struct i2c_client *client = (struct i2c_client *)info;
	struct stmvl6180_data *data = i2c_get_clientdata(client);

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
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t stmvl6180_store_enable_ps_sensor(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned long flags;

	vl6180_dbgmsg("enable ps senosr ( %ld),addr:0x%x\n", val, client->addr);

	if ((val != 0) && (val != 1)) {
		pr_err("%s:store unvalid value=%ld\n", __func__, val);
		return count;
	}
	mutex_lock(&data->work_mutex);
	if (val == 1) {
		/* turn on p sensor */
		if (data->enable_ps_sensor == 0) {
			stmvl6180_set_enable(client, 0);	/* Power Off */
			/* re-init */
			VL6180x_Prepare(vl6180x_dev);
			VL6180x_UpscaleSetScaling(vl6180x_dev, 3);

			VL6180x_RangeConfigInterrupt(vl6180x_dev,
						     CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);

			/* start */
			VL6180x_RangeSetSystemMode(vl6180x_dev,
						   MODE_START_STOP |
						   MODE_SINGLESHOT);
			data->ps_is_singleshot = 1;
			data->enable_ps_sensor = 1;

			/* we need this polling timer routine for house keeping */
			spin_lock_irqsave(&data->update_lock.wait_lock, flags);
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			cancel_delayed_work(&data->dwork);
			schedule_delayed_work(&data->dwork,
					      msecs_to_jiffies
					      (INT_POLLING_DELAY));
			spin_unlock_irqrestore(&data->update_lock.wait_lock,
					       flags);

			stmvl6180_set_enable(client, 1);	/* Power On */
		}
	} else {
		/* turn off p sensor */
		data->enable_ps_sensor = 0;
		if (data->ps_is_singleshot == 0)
			VL6180x_RangeSetSystemMode(vl6180x_dev,
						   MODE_START_STOP);
		VL6180x_RangeClearInterrupt(vl6180x_dev);

		stmvl6180_set_enable(client, 0);

		spin_lock_irqsave(&data->update_lock.wait_lock, flags);
		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		cancel_delayed_work(&data->dwork);
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

	}

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
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->enableDebug);
}

/* for als integration time setup */
static ssize_t stmvl6180_store_enable_debug(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	long on = simple_strtol(buf, NULL, 10);
	if ((on != 0) && (on != 1)) {
		pr_err("%s: set debug=%ld\n", __func__, on);
		return count;
	}
	data->enableDebug = on;

	return count;
}

//DEVICE_ATTR(name,mode,show,store)
static DEVICE_ATTR(enable_debug, S_IWUSR | S_IRUGO,
		   stmvl6180_show_enable_debug, stmvl6180_store_enable_debug);

static struct attribute *stmvl6180_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_debug.attr,
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
	unsigned long flags;
	unsigned long distance = 0;
	struct i2c_client *client;
	switch (cmd) {
	case VL6180_IOCTL_INIT:	/* init. */
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			/* turn on p sensor only if it's not enabled by other client */
			if (data->enable_ps_sensor == 0) {
				pr_info
				    ("ioclt INIT to enable PS sensor=====\n");
				stmvl6180_set_enable(client, 0);	/* Power Off */
				/* re-init */
				gpio_set_value(cs_gpio_num, 1);
				VL6180x_Prepare(vl6180x_dev);
				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
				VL6180x_FilterSetState(vl6180x_dev, 1);	/* turn on wrap around filter */
#endif
				VL6180x_RangeConfigInterrupt(vl6180x_dev,
							     CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);

				VL6180x_RangeSetSystemMode(vl6180x_dev,
							   MODE_START_STOP |
							   MODE_SINGLESHOT);
				data->ps_is_singleshot = 1;
				data->enable_ps_sensor = 1;

				/* we need this polling timer routine for house keeping */
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->dwork);
				schedule_delayed_work(&data->dwork,
						      msecs_to_jiffies
						      (INT_POLLING_DELAY));
				spin_unlock_irqrestore(&data->update_lock.
						       wait_lock, flags);

				stmvl6180_set_enable(client, 1);	/* Power On */
			}
		}
		break;
	case VL6180_IOCTL_XTALKCALB:	/* crosstalk calibration */
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			/* turn on p sensor only if it's not enabled by other client */
			if (data->enable_ps_sensor == 0) {
				pr_info
				    ("ioclt XTALKCALB to enable PS sensor for crosstalk calibration=====\n");
				stmvl6180_set_enable(client, 0);	/* Power Off */
				//re-init
				VL6180x_Prepare(vl6180x_dev);
				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
				VL6180x_FilterSetState(vl6180x_dev, 1);	/* turn off wrap around filter */
#endif

				VL6180x_RangeConfigInterrupt
				    (vl6180x_dev,
				     CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				VL6180x_WrWord(vl6180x_dev,
					       SYSRANGE_CROSSTALK_COMPENSATION_RATE,
					       0);

				/* start */
				VL6180x_RangeSetSystemMode(vl6180x_dev,
							   MODE_START_STOP
							   | MODE_SINGLESHOT);
				data->ps_is_singleshot = 1;
				data->enable_ps_sensor = 1;

				/* we need this polling timer routine for house keeping */
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->dwork);
				schedule_delayed_work(&data->dwork,
						      msecs_to_jiffies
						      (INT_POLLING_DELAY));
				spin_unlock_irqrestore
				    (&data->update_lock.wait_lock, flags);

				stmvl6180_set_enable(client, 1);	/* Power On */
			}

		}
		break;
	case VL6180_IOCTL_SETXTALK:
		client = i2c_getclient();
		if (client) {
			int xtalkint = 0;
			if (copy_from_user(&xtalkint, (int *)p, sizeof(int))) {
				rc = -EFAULT;
			}
			pr_info("ioctl SETXTALK as 0x%x\n", xtalkint);
#ifdef CALIBRATION_FILE
			xtalk_calib = xtalkint;
			stmvl6180_write_xtalk_calibration_file(xtalkint);
#endif
			VL6180x_SetXTalkCompensationRate(vl6180x_dev, xtalkint);

		}
		break;
	case VL6180_IOCTL_OFFCALB:	/* offset calibration */
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			/* turn on p sensor only if it's not enabled by other client */
			if (data->enable_ps_sensor == 0) {
				pr_info
				    ("ioclt OFFCALB to enable PS sensor for offset calibration=====\n");
				stmvl6180_set_enable(client, 0);	/* Power Off */
				//re-init
				VL6180x_Prepare(vl6180x_dev);

				VL6180x_UpscaleSetScaling(vl6180x_dev, 3);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
				VL6180x_FilterSetState(vl6180x_dev, 0);	/* turn off wrap around filter */
#endif

				VL6180x_RangeConfigInterrupt
				    (vl6180x_dev,
				     CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				VL6180x_WrWord(vl6180x_dev,
					       SYSRANGE_PART_TO_PART_RANGE_OFFSET,
					       0);
				VL6180x_WrWord(vl6180x_dev,
					       SYSRANGE_CROSSTALK_COMPENSATION_RATE,
					       0);

				/* start */
				VL6180x_RangeSetSystemMode(vl6180x_dev,
							   MODE_START_STOP
							   | MODE_SINGLESHOT);
				data->ps_is_singleshot = 1;
				data->enable_ps_sensor = 1;

				/* we need this polling timer routine for house keeping */
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->dwork);
				schedule_delayed_work(&data->dwork,
						      msecs_to_jiffies
						      (INT_POLLING_DELAY));
				spin_unlock_irqrestore
				    (&data->update_lock.wait_lock, flags);

				stmvl6180_set_enable(client, 1);	/* Power On */
			}
		}
		break;
	case VL6180_IOCTL_SETOFFSET:
		client = i2c_getclient();
		if (client) {
			int8_t offsetint = 0;
			if (copy_from_user
			    (&offsetint, (int8_t *) p, sizeof(int8_t))) {
				rc = -EFAULT;
			}
			pr_info("ioctl SETOFFSET as %d\n", offsetint);
#ifdef CALIBRATION_FILE
			offset_calib = offsetint;
			stmvl6180_write_offset_calibration_file(offsetint);
#endif
			VL6180x_SetOffset(vl6180x_dev, offsetint);
		}
		break;
	case VL6180_IOCTL_STOP:
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			/* turn off p sensor only if it's enabled by other client */
			if (data->enable_ps_sensor == 1) {
				/* turn off p sensor */
				data->enable_ps_sensor = 0;
				if (data->ps_is_singleshot == 0)
					VL6180x_RangeSetSystemMode
					    (vl6180x_dev, MODE_START_STOP);
				VL6180x_RangeClearInterrupt(vl6180x_dev);
				stmvl6180_set_enable(client, 0);
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->dwork);
				spin_unlock_irqrestore
				    (&data->update_lock.wait_lock, flags);
			}
		}
		break;
	case VL6180_IOCTL_GETDATA:	/* Get proximity value only */
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			distance = data->rangeData.FilteredData.range_mm;
		}
		pr_debug("vl6180_getDistance return %ld\n", distance);
		return put_user(distance, (unsigned long *)p);
		break;
	case VL6180_IOCTL_GETDATAS:	/* Get all range data */
		client = i2c_getclient();
		if (client) {
			struct stmvl6180_data *data =
			    i2c_get_clientdata(client);
			pr_debug("IOCTL_GETDATAS, range_mm:%d===\n",
				 data->rangeData.range_mm);
			if (copy_to_user
			    ((VL6180x_RangeData_t *) p, &(data->rangeData),
			     sizeof(VL6180x_RangeData_t))) {
				rc = -EFAULT;
			}
		} else
			return -EFAULT;

		break;
	case VL6180_IOCTL_READCALB:	/* Read calibration data */
#ifdef CALIBRATION_FILE
		stmvl6180_read_calibration_file();
#endif
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

static int stmvl6180_flush(struct file *file1, fl_owner_t id)
{
	unsigned long flags;
	struct i2c_client *client;
	client = i2c_getclient();

	if (client) {
		struct stmvl6180_data *data = i2c_get_clientdata(client);
		if (data->enable_ps_sensor == 1) {
			/* turn off p sensor if it's enabled */
			data->enable_ps_sensor = 0;
			VL6180x_RangeClearInterrupt(vl6180x_dev);

			stmvl6180_set_enable(client, 0);

			gpio_set_value(cs_gpio_num, 0);

			spin_lock_irqsave(&data->update_lock.wait_lock, flags);
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			cancel_delayed_work(&data->dwork);
			spin_unlock_irqrestore(&data->update_lock.wait_lock,
					       flags);
		}
	}
	return 0;
}

static long stmvl6180_ioctl(struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	int ret;
	mutex_lock(&vl6180_mutex);
	ret = stmvl6180_ioctl_handler(file, cmd, arg, (void __user *)arg);
	mutex_unlock(&vl6180_mutex);

	return ret;
}

/*
 * Initialization function
 */
static int stmvl6180_init_client(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);
	uint8_t id = 0, module_major = 0, module_minor = 0;
	uint8_t model_major = 0, model_minor = 0;
	uint8_t i = 0, val;

	/* Read Model ID */
	VL6180x_RdByte(vl6180x_dev, VL6180_MODEL_ID_REG, &id);
	pr_info("read MODLE_ID: 0x%x, i2cAddr:0x%x\n", id, client->addr);
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
	for (i = 0;
	     i <=
	     (VL6180_FIRMWARE_REVISION_ID_REG - VL6180_REVISION_ID_REG); i++) {
		VL6180x_RdByte(vl6180x_dev, (VL6180_REVISION_ID_REG + i), &val);
		pr_info("0x%x-", val);
	}
	pr_info("\n");

	data->ps_data = 0;
	data->enableDebug = 0;
#ifdef CALIBRATION_FILE
	//stmvl6180_read_calibration_file();
#endif

	/* VL6180 Initialization */
	VL6180x_WaitDeviceBooted(vl6180x_dev);
	VL6180x_InitData(vl6180x_dev);
	//VL6180x_FilterSetState(vl6180x_dev, 1); /* activate wrap around filter */
	//VL6180x_DisableGPIOxOut(vl6180x_dev, 1); /* diable gpio 1 output, not needed when polling */

	return 0;
}

/*
 * I2C init/probing/exit functions
 */
struct regulator *vdd_stmvl6180;
struct regulator *vcc_stmvl6180;

static struct i2c_driver stmvl6180_driver;
static char const *power_pin_vdd;
static char const *power_pin_vcc;

static int stmvl6180_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct stmvl6180_data *data;
	int err = 0;
#ifdef USE_INT
	int irq = 0;
#endif
	pr_info("stmvl6180_probe==========\n");

	err = of_property_read_string(dev->of_node, "st,vdd", &power_pin_vdd);
	if (err) {
		pr_err("%s: OF error rc=%d at line %d for st,vdd\n",
		       __func__, err, __LINE__);
		goto exit;
	}

	err = of_property_read_string(dev->of_node, "st,vcc", &power_pin_vcc);
	if (err) {
		pr_err("%s: OF error rc=%d at line %d for st,vdd\n",
		       __func__, err, __LINE__);
		goto exit;
	}
	cs_gpio_num = of_get_named_gpio(dev->of_node, "st,cs_gpio", 0);
	if (!gpio_is_valid(cs_gpio_num)) {
		pr_err("%s: OF error rc=%d at line %d for cy,cs_gpio\n",
		       __func__, err, __LINE__);
		goto exit;
	}

	/* VDD power on */
	vdd_stmvl6180 = regulator_get(dev, power_pin_vdd);

	if (IS_ERR(vdd_stmvl6180)) {
		pr_err("%s: failed to get st vdd\n", __func__);
		goto exit;
	}

	err = regulator_set_voltage(vdd_stmvl6180, 2850000, 2850000);
	if (err < 0) {
		pr_err("%s: failed to set st vdd\n", __func__);
		goto exit_vdd_regulator_put;
	}

	err = regulator_enable(vdd_stmvl6180);
	if (err < 0) {
		pr_err("%s: failed to enable st vdd\n", __func__);
		goto exit_vdd_regulator_put;
	}

	/* VCC power on */
	vcc_stmvl6180 = regulator_get(dev, power_pin_vcc);

	if (IS_ERR(vcc_stmvl6180)) {
		pr_err("%s: failed to get st vcc\n", __func__);
		goto exit_vdd_regulator_put;
	}

	err = regulator_set_voltage(vcc_stmvl6180, 0, 0);
	if (err < 0) {
		pr_err("%s: failed to set st vcc\n", __func__);
		goto exit_vcc_regulator_put;
	}

	err = regulator_enable(vcc_stmvl6180);
	if (err < 0) {
		pr_err("%s: failed to enable st vcc\n", __func__);
		goto exit_vcc_regulator_put;
	}

	err = gpio_request(cs_gpio_num, "tmvl6180");
	if (err < 0) {
		pr_err("%s: failed to get cs gpio\n", __func__);
		goto exit_vcc_regulator_put;
	}

	err = gpio_direction_output(cs_gpio_num, 1);
	if (err < 0) {
		pr_err("%s: failed to get cs gpio\n", __func__);
		goto exit_vcc_regulator_put;
	}
	gpio_set_value(cs_gpio_num, 1);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit_vcc_regulator_put;
	}

	data = kzalloc(sizeof(struct stmvl6180_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit_vcc_regulator_put;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */

	pr_info("enable = %x\n", data->enable);

	mutex_init(&data->update_lock);
	mutex_init(&data->work_mutex);
	mutex_init(&vl6180_mutex);

	/* setup platform i2c client */
	i2c_setclient(client);
	//vl6180x_dev.I2cAddress = client->addr;

/* interrupt set up */
#ifdef USE_INT
	gpio_request(IRQ_NUM, "vl6180_gpio_int");
	gpio_direction_input(IRQ_NUM);
	irq = gpio_to_irq(IRQ_NUM);
	if (irq < 0) {
		pr_err("filed to map GPIO :%d to interrupt:%d\n", IRQ_NUM, irq);
	} else {
		int result;
		vl6180_dbgmsg("register_irq:%d\n", irq);
		if ((result = request_threaded_irq(irq, NULL, stmvl6180_interrupt_handler, IRQF_TRIGGER_RISING,	//IRQF_TRIGGER_FALLING- poliarity:0 IRQF_TRIGGER_RISNG - poliarty:1
						   "vl6180_lb_gpio_int",
						   (void *)client))) {
			pr_err
			    ("%s Could not allocate STMVL6180_INT ! result:%d\n",
			     __func__, result);
			goto exit_free_irq;
		}
	}
	//disable_irq(irq);
	data->irq = irq;
	vl6180_dbgmsg("%s interrupt is hooked\n", __func__);
#endif

	INIT_DELAYED_WORK(&data->dwork, stmvl6180_work_handler);

	/* Initialize the STM VL6180 chip */
	err = stmvl6180_init_client(client);
	if (err)
		goto exit_kfree;

	/* Register to Input Device */
	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		pr_err("%s Failed to allocate input device ps\n", __func__);
		goto exit_free_dev_ps;
	}

	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 76, 0, 0);	//range in cm
	input_set_abs_params(data->input_dev_ps, ABS_HAT0X, 0, 765, 0, 0);	//range in_mm
	input_set_abs_params(data->input_dev_ps, ABS_X, 0, 65535, 0, 0);	//rtnRate

	data->input_dev_ps->name = "STM VL6180 proximity sensor";

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		pr_err("%sUnable to register input device ps: %s\n",
		       __func__, data->input_dev_ps->name);
		goto exit_unregister_dev_ps;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &stmvl6180_attr_group);
	if (err) {
		pr_err("%sUnable to create sysfs group\n", __func__);
		goto exit_unregister_dev_ps;
	}

	pr_err("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);

	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
#ifdef USE_INT
exit_free_irq:
	free_irq(irq, client);
#endif
exit_kfree:
	kfree(data);
exit_vcc_regulator_put:
	regulator_put(vcc_stmvl6180);
exit_vdd_regulator_put:
	regulator_put(vdd_stmvl6180);
exit:
	return err;
}

static int stmvl6180_remove(struct i2c_client *client)
{
	struct stmvl6180_data *data = i2c_get_clientdata(client);

	//input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);

	//input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);

#ifdef  USE_INT
	free_irq(data->irq, client);
#endif

	sysfs_remove_group(&client->dev.kobj, &stmvl6180_attr_group);

	/* Power down the device */
	stmvl6180_set_enable(client, 0);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int stmvl6180_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return stmvl6180_set_enable(client, 0);
}

static int stmvl6180_resume(struct i2c_client *client)
{
	return stmvl6180_set_enable(client, 0);
}

#else

#define stmvl6180_suspend   NULL
#define stmvl6180_resume    NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id stmvl6180_id[] = {
	{STMVL6180_DRV_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, stmvl6180_id);

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

static const struct of_device_id stmvl6180_of_id_table[] = {
	{.compatible = "st,stmvl6180"},
	{},
};

static struct i2c_driver stmvl6180_driver = {
	.driver = {
		   .name = STMVL6180_DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = stmvl6180_of_id_table,
		   },
	.suspend = stmvl6180_suspend,
	.resume = stmvl6180_resume,
	.probe = stmvl6180_probe,
	.remove = stmvl6180_remove,
	.id_table = stmvl6180_id,

};

static int __init stmvl6180_init(void)
{
	int ret = 0;

	pr_info("stmvl6180_init===\n");
	/* to register as a misc device */
	if (misc_register(&stmvl6180_ranging_dev) != 0)
		pr_info(KERN_INFO
			"Could not register misc. dev for stmvl6180 ranging\n");

	ret = i2c_add_driver(&stmvl6180_driver);

	return ret;
}

static void __exit stmvl6180_exit(void)
{
	pr_info("stmvl6180_exit===\n");
	i2c_del_driver(&stmvl6180_driver);
}

MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl6180_init);
module_exit(stmvl6180_exit);
