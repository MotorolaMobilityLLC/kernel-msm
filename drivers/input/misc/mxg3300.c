/*
 *  MXG3300 Magnetometer device driver
 *
 *  Copyright (C) 2015 joan.na <joan.na@magnachip.com>
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
 *  along with this program
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/sensors.h>
#include <linux/mxg3300.h>

#define REGULATOR_CONTROL_ENABLE (0)
#define CHECK_IC_NUM_ENABLE (0) /* No need for MXG3300. */

#define MXG_DELAY_MAX (200) /* ms */
#define MXG_DELAY_MIN (10) /* ms */

#define MXG_DELAY_FOR_READY (10) /* ms */
#define MXG_DELAY_FOR_MODE_TRANSITS (1000) /* us */

#define MXG_DATA_MIN (-32768)
#define MXG_DATA_MAX (32767)

#define MXG_VDD_MIN 2500000 /* uV */
#define MXG_VDD_MAX 3300000 /* uV */
#define MXG_VIO_MIN 1750000 /* uV */
#define MXG_VIO_MAX 1950000 /* uV */

#define MXG_COEF_X_MIN -40
#define MXG_COEF_X_MAX 40
#define MXG_COEF_Y_MIN -40
#define MXG_COEF_Y_MAX 40
#define MXG_COEF_Z_MIN -640
#define MXG_COEF_Z_MAX -160

#define MXG_MEASUREMENT_DATA_SIZE 9
#define MXG_MEASUREMENT_ST1_POS 0
#define MXG_MEASUREMENT_ST2_POS 8
#define MXG_MEASUREMENT_RAW_POS 1

#define DATA_READY_CHECK(x) (x & 0x1)
#define DATA_OVERFLOW_CHECK(x) (x & 0x1)
#define RAW_TO_GAUSS(x, y, z) (((x*y/128)+(x))*z)

#if CHECK_IC_NUM_ENABLE
static u8 mxg_write_buf[10] = {0x13, 0x20, 0x01, 0xc0, 0x0b, 0x08, 0x00, 0x00, 0x08, 0x01};
static u8 mxg_address_buf[10] = {0x90, 0x91, 0x98, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97};
#endif

static struct sensors_classdev mxg_sensors_cdev = {
	.name = "mxg3300_compass",
	.vendor = "MagnaChip Corporation",
	.version = 1,
	.handle = SENSORS_MAGNETIC_FIELD_HANDLE,
	.type = SENSOR_TYPE_MAGNETIC_FIELD,
	.max_range = "4914",
	.resolution = "0.6",
	.sensor_power = "0.35",
	.min_delay = 10000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 20,/* 20ms==50Hz */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct mxg_sensor_data *mxdata;

static int mxg_i2c_write_byte(struct device *dev, u8 reg, u8 val);
static int mxg_i2c_read_block(struct device *dev, u8 reg_addr, u8 *data, int len);
static int mxg_do_self_test(struct mxg_sensor_data *mx_data);

static int mxg_i2c_write_byte(struct device *dev, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(to_i2c_client(dev), reg, val);
	if (ret < 0) {
		dev_err(dev, "write(byte) to device fails status %x\n", ret);
		return ret;
	}
	return 0;
}

static int mxg_i2c_read_block(struct device *dev, u8 reg_addr, u8 *data, int len)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(to_i2c_client(dev), reg_addr, len, data);
	if (ret < 0) {
		dev_err(dev, "read(block) to device fails status %x\n", ret);
		goto i2c_read_block_error;
	}

i2c_read_block_error:
	return ret < 0 ? ret : 0;
}

static inline uint8_t mxg_select_frequency(int64_t delay_ns)
{
	if (delay_ns >= 100000000LL)
		return MXG_MODE_CONT_10HZ;
	else if (delay_ns >= 50000000LL)
		return MXG_MODE_CONT_20HZ;
	else if (delay_ns >= 20000000LL)
		return MXG_MODE_CONT_50HZ;
	else if (delay_ns >= 10000000LL)
		return MXG_MODE_CONT_100HZ;
	else if (delay_ns >= 0LL)
		return MXG_MODE_CONT_200HZ;
	else
		return MXG_MODE_POWER_DOWN;
}

static int mxg_operation_mode(struct device *dev, u8 mode)
{
	int ret;

	ret = mxg_i2c_write_byte(dev, MXG_REG_OPMODE, mode);
	if (ret < 0) {
		dev_err(dev, "change operation mode failed.\n");
	} else {
		if (mode == MXG_MODE_POWER_DOWN)
			udelay(MXG_DELAY_FOR_MODE_TRANSITS);
	}
	return ret;
}

#if REGULATOR_CONTROL_ENABLE
static int mxg_power_control(struct mxg_sensor_data *mx_data, bool on)
{
	int ret = 0;

	if (!on && mx_data->power_on) {
#ifdef MXG_USE_VDD
		ret = regulator_disable(mx_data->vdd_ana);
		if (ret) {
			dev_err(mx_data->dev, "regulator vdd_ana disable failed.\n");
			return ret;
		}
#endif

		ret = regulator_disable(mx_data->vdd_io);
		if (ret) {
			dev_err(mx_data->dev, "regulator vdd_io disable failed.\n");
#ifdef MXG_USE_VDD
			if (!(regulator_is_enabled(mx_data->vdd_ana) > 0))
				ret = regulator_enable(mx_data->vdd_ana);
#endif
			return ret;
		}
		mx_data->power_on = false;
		return ret;
	} else if (on && !mx_data->power_on) {
#ifdef MXG_USE_VDD
		ret = regulator_enable(mx_data->vdd_ana);
		if (ret) {
			dev_err(mx_data->dev, "regulator vdd_ana enable failed.\n");
			return ret;
		}
#endif

		ret = regulator_enable(mx_data->vdd_io);
		if (ret) {
			dev_err(mx_data->dev, "regulator vdd_io enable failed.\n");
#ifdef MXG_USE_VDD
			if (regulator_is_enabled(mx_data->vdd_ana) > 0)
				ret = regulator_disable(mx_data->vdd_ana);
#endif
			return ret;
		}
		mx_data->power_on = true;

		/* wait for power up time */
		msleep(80);
		return ret;
	} else {
		dev_warn(mx_data->dev, "power = %d. on = %d\n", on, mx_data->power_on);
		return ret;
	}

	return 0;
}

static int mxg_power_register(struct mxg_sensor_data *mx_data)
{
	int ret;
#ifdef MXG_USE_VDD
	mx_data->vdd_ana = regulator_get(mx_data->dev, "mx,vdd_ana");
	if (IS_ERR(mx_data->vdd_ana)) {
		dev_err(mx_data->dev, "regulator get failed vdd_ana-supply.\n");
		return PTR_ERR(mx_data->vdd_ana);
	}

	if (regulator_count_voltages(mx_data->vdd_ana) > 0) {
		ret = regulator_set_voltage(mx_data->vdd_ana, MXG_VDD_MIN, MXG_VDD_MAX);
		if (ret) {
			dev_err(mx_data->dev, "regulator set voltage failed vdd.\n");
			goto err_reg_vdd_put;
		}
	}
#endif

	mx_data->vdd_io = regulator_get(mx_data->dev, "mx,vdd_io");
	if (IS_ERR(mx_data->vdd_io)) {
		dev_err(mx_data->dev, "regulator get failed vdd_io-supply.\n");
		goto err_reg_vdd_set_voltage;
	}

	if (regulator_count_voltages(mx_data->vdd_io) > 0) {
		ret = regulator_set_voltage(mx_data->vdd_io, MXG_VIO_MIN, MXG_VIO_MAX);
		if (ret) {
			dev_err(mx_data->dev, "regulator set voltage failed vio.\n");
			goto err_reg_vio_put;
		}
	}

	return 0;
err_reg_vio_put:
	regulator_put(mx_data->vdd_io);
err_reg_vdd_set_voltage:
#ifdef MXG_USE_VDD
	if (regulator_count_voltages(mx_data->vdd_ana) > 0)
		ret = regulator_set_voltage(mx_data->vdd_ana, 0, MXG_VDD_MAX);
	err_reg_vdd_put:
	regulator_put(mx_data->vdd_ana);
#endif
	return ret;
}

static int mxg_power_unregister(struct mxg_sensor_data *mx_data)
{
	int ret = 0;
	if (regulator_count_voltages(mx_data->vdd_io) > 0)
		ret = regulator_set_voltage(mx_data->vdd_io, 0, MXG_VIO_MAX);

	regulator_put(mx_data->vdd_io);
	return 0;
}
#else
static int mxg_power_register(struct mxg_sensor_data *mx_data) {return 0; }
static int mxg_power_unregister(struct mxg_sensor_data *mx_data) {return 0; }
static int mxg_power_control(struct mxg_sensor_data *mx_data, bool on) {return 0; }
#endif

static void mxg_self_test_work_func(struct work_struct *work)
{
	int i, ret;
	u8 *st1, *data;
	u8 buf[MXG_MEASUREMENT_DATA_SIZE] = {0,};
	struct mxg_sensor_data *mx_data = container_of(work,
			struct mxg_sensor_data,
			selftest_work.work);

	st1 = &buf[MXG_MEASUREMENT_ST1_POS];
	data = &buf[MXG_MEASUREMENT_RAW_POS];

	ret = mxg_i2c_read_block(mx_data->dev,
			MXG_REG_ST1,
			buf,
			MXG_MEASUREMENT_DATA_SIZE);
	if (ret)
		goto drdy_work_func_end;

	if (DATA_READY_CHECK(*st1)) {
		for (i = 0; i < 3; i++) {
			mx_data->self[i] = (short)((data[(i*2)+1])|(data[(i*2)] << 8));
			/* dev_info(mx_data->dev, "self-test [%d] : %d\n", i, mx_data->self[i]); */
		}
	} else {
		dev_err(mx_data->dev, "Drdy Data is not read yet.\n");
		goto drdy_work_func_end;
	}

	atomic_set(&mx_data->selftest_done, 1);

drdy_work_func_end:
	wake_up(&mx_data->selftest_wq);
}

static int mxg_sensor_enable_disable(struct mxg_sensor_data *mx_data, int en)
{
	int ret, msec;
	int64_t delay_ns;

	if ((en != 0) && (en != 1)) {
		dev_err(mx_data->dev, "invalid value.\n");
		return -EINVAL;
	}

	atomic_set(&mx_data->en, en);

	if (en) {
		if (!mx_data->power_on) {
			ret = mxg_power_control(mx_data, true);
			if (ret) {
				dev_err(mx_data->dev, "failed to power up mxg sensor.\n");
				return ret;
			}
		}
		mutex_lock(&mx_data->lock);
		msec = mx_data->odr;
		delay_ns = msec * 1000000;
		mutex_unlock(&mx_data->lock);
		if (mx_data->use_hrtimer) {
			hrtimer_cancel(&mx_data->poll_timer);
			cancel_work_sync(&mx_data->dwork.work);
			hrtimer_start(&mx_data->poll_timer,
				ns_to_ktime(delay_ns),
				HRTIMER_MODE_REL);
		} else {
			cancel_delayed_work_sync(&mx_data->dwork);
			queue_delayed_work(mx_data->work_queue, &mx_data->dwork,
				(unsigned long)nsecs_to_jiffies64(delay_ns));
		}
	} else {
		if (mx_data->use_hrtimer) {
			hrtimer_cancel(&mx_data->poll_timer);
			cancel_work_sync(&mx_data->dwork.work);
		} else {
			cancel_delayed_work_sync(&mx_data->dwork);
		}

		ret = mxg_operation_mode(mx_data->dev, MXG_MODE_POWER_DOWN);
		if (ret) {
			dev_err(mx_data->dev, "failed on mxg_operation_mode.\n");
			return ret;
		}

		if (mx_data->power_on) {
			ret = mxg_power_control(mx_data, false);
			if (ret) {
				dev_err(mx_data->dev, "failed to power down mxg sensor.\n");
				return ret;
			}
		}
	}

	return 0;
}

static int mxg_sensor_delay(struct mxg_sensor_data *mx_data, unsigned int msec)
{
	int ret = 0;
	int64_t delay_ns;
	uint8_t mode;

	mutex_lock(&mx_data->lock);

	if (msec < MXG_DELAY_MIN)
		msec = MXG_DELAY_MIN;
	if (msec > MXG_DELAY_MAX)
		msec = MXG_DELAY_MAX;
	mx_data->odr = msec;

	ret = mxg_operation_mode(mx_data->dev, MXG_MODE_POWER_DOWN);
	if (ret) {
		dev_err(mx_data->dev, "failed to set power down mode.\n");
		mutex_unlock(&mx_data->lock);
		return ret;
	}

	delay_ns = msec * 1000000;
	mode = mxg_select_frequency(delay_ns);
	ret = mxg_operation_mode(mx_data->dev, mode);
	if (ret) {
		dev_err(mx_data->dev, "failed to set continuous mode(0x%02x).\n", mode);
		mutex_unlock(&mx_data->lock);
		return ret;
	}

	mutex_unlock(&mx_data->lock);

	return ret;
}

static int mxg_sensors_cdev_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct mxg_sensor_data *mx_data = container_of(sensors_cdev,
			struct mxg_sensor_data, sensor_cdev);

	return mxg_sensor_enable_disable(mx_data, enable);
}

static int mxg_sensors_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_ms)
{
	int ret;
	struct mxg_sensor_data *mx_data = container_of(sensors_cdev,
			struct mxg_sensor_data, sensor_cdev);

	ret = mxg_sensor_delay(mx_data, delay_ms);

	return 0;
}

static int mxg_self_test(struct sensors_classdev *sensors_cdev)
{
    int ret = 0;

	if (mxdata)
	    ret = mxg_do_self_test(mxdata);
    return ret;
}

static int mxg_sensors_flush_set(struct sensors_classdev *sensors_cdev)
{
	struct mxg_sensor_data *mx_data = container_of(sensors_cdev,
			struct mxg_sensor_data, sensor_cdev);

	input_event(mx_data->input_dev, EV_SYN, SYN_CONFIG, mx_data->flush_count++);
	input_sync(mx_data->input_dev);

	dev_dbg(mx_data->dev, "%s: end \n", __func__);

	return 0;
}

static int mxg_read_mag_data(struct mxg_sensor_data *mx_data)
{
	int i, ret;
	int axis, sign;
	short raw_temp[3] = {0,};
	u8 *st1, *st2, *data;
	uint8_t mode;
	u8 buf[MXG_MEASUREMENT_DATA_SIZE] = {0,};

	st1 = &buf[MXG_MEASUREMENT_ST1_POS];
	st2 = &buf[MXG_MEASUREMENT_ST2_POS];
	data = &buf[MXG_MEASUREMENT_RAW_POS];

	ret = mxg_i2c_read_block(mx_data->dev,
		MXG_REG_ST1,
		buf,
		MXG_MEASUREMENT_DATA_SIZE);
	if (ret)
		return ret;

	if (DATA_READY_CHECK(*st1)) {
		for (i = 0; i < 3; i++) {
			axis = mx_data->plat_data->position_array[i];
			sign = ((mx_data->plat_data->position_array[i+3] > 0) ? -1:1);
			raw_temp[i] = (short)((data[(i*2)+1])|(data[(i*2)] << 8));
			mx_data->raw_data[axis] =
				(int)RAW_TO_GAUSS(raw_temp[i], mx_data->mcoef[i], sign);
		}
	} else
		dev_dbg(mx_data->dev, "data is not read yet.\n");

	if (DATA_OVERFLOW_CHECK(*st2)) {
		dev_warn(mx_data->dev, "data is overflow.\n");
		mxg_operation_mode(mx_data->dev, MXG_SOFT_RESET);
		mode = mxg_select_frequency(mx_data->odr * 1000000);
		mxg_operation_mode(mx_data->dev, mode);
		return -EIO;
	}

	dev_dbg(mx_data->dev, "mag data[0] : %d mag data[1] : %d mag data[2] : %d",
	mx_data->raw_data[0], mx_data->raw_data[1], mx_data->raw_data[2]);

	return ret;
}

static int mx_report_data(void)
{
	int ret;

	ret = mxg_read_mag_data(mxdata);
	if (ret) {
		dev_err(mxdata->dev, "Get data failed.\n");
		return -EIO;
	}

	get_monotonic_boottime(&mxdata->ts);
	input_report_abs(mxdata->input_dev, EVENT_RAW_X, mxdata->raw_data[0]);
	input_report_abs(mxdata->input_dev, EVENT_RAW_Y, mxdata->raw_data[1]);
	input_report_abs(mxdata->input_dev, EVENT_RAW_Z, mxdata->raw_data[2]);
	input_event(mxdata->input_dev, EV_REL, SYN_TIME_SEC, mxdata->ts.tv_sec);
	input_event(mxdata->input_dev, EV_REL, SYN_TIME_NSEC, mxdata->ts.tv_nsec);

	/* avoid repeat by input subsystem framework */
	if ((mxdata->raw_data[0] == mxdata->lastData[0]) && (mxdata->raw_data[1] == mxdata->lastData[1]) &&
		(mxdata->raw_data[2] == mxdata->lastData[2])) {
		input_report_abs(mxdata->input_dev, ABS_MISC, mxdata->rep_cnt++);
	}

	mxdata->lastData[0] = mxdata->raw_data[0];
	mxdata->lastData[1] = mxdata->raw_data[1];
	mxdata->lastData[2] = mxdata->raw_data[2];

	input_sync(mxdata->input_dev);

	return 0;
}

static void mx_dev_poll(struct work_struct *work)
{
	int ret, msec;
	int64_t delay_ns;

	if (mxdata == NULL) {
		return;
	}

	ret = mx_report_data();
	if (ret < 0)
		dev_err(mxdata->dev, "Failed to report data\n");

	if (!mxdata->use_hrtimer) {
		msec = mxdata->odr;
		delay_ns = msec * 1000000;

		queue_delayed_work(mxdata->work_queue, &mxdata->dwork,
			(unsigned long)nsecs_to_jiffies64(delay_ns));
	}
}

static enum hrtimer_restart mx_timer_func(struct hrtimer *timer)
{
	if (mxdata != NULL) {
		queue_work(mxdata->work_queue, &mxdata->dwork.work);
		hrtimer_forward_now(&mxdata->poll_timer,
			ns_to_ktime(mxdata->odr * 1000000));/* odr:ms, odr*1000000-->ns */
	}
	return HRTIMER_RESTART;
}
static int mxg_sensors_cdev_class_init(struct mxg_sensor_data *mx_data)
{
	int ret;

	mx_data->sensor_cdev = mxg_sensors_cdev;
	mx_data->sensor_cdev.sensors_enable = mxg_sensors_cdev_enable;
	mx_data->sensor_cdev.sensors_poll_delay = mxg_sensors_cdev_poll_delay;
	mx_data->sensor_cdev.sensors_self_test = mxg_self_test;
	mx_data->sensor_cdev.sensors_flush = mxg_sensors_flush_set;

	ret = sensors_classdev_register(mx_data->dev, &mx_data->sensor_cdev);
	if (ret) {
		dev_err(mx_data->dev, "failed to create class device file. (%d)\n", ret);
	}

	return ret;
}

static ssize_t mxg_sensor_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	return snprintf(buf, 128, "%d\n", atomic_read(&mx_data->en));
}

static ssize_t mxg_sensor_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned int en = 0;
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &en);
	if (ret)
		return -EINVAL;

	ret = mxg_sensor_enable_disable(mx_data, en);
	if (ret)
		return ret;

	return ret ? -EBUSY : size;
}

static ssize_t mxg_sensor_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int odr;
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	mutex_lock(&mx_data->lock);
	odr = mx_data->odr;
	mutex_unlock(&mx_data->lock);

	return snprintf(buf, 128, "%d\n", odr);
}

static ssize_t mxg_sensor_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned int msec;
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &msec);
	if (ret)
		return -EINVAL;

	mxg_sensor_delay(mx_data, msec);

	return ret ? EBUSY : size;
}


static ssize_t mxg_sensor_mcoef_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	return snprintf(buf, 128, "%u %u %u\n", mx_data->mcoef[0], mx_data->mcoef[1], mx_data->mcoef[2]);
}


static int mxg_do_self_test(struct mxg_sensor_data *mx_data)
{
	int ret, i, result = 0;
	uint8_t mode;
	short magn_self[3];

	if (atomic_read(&mx_data->en)) {
		if (mx_data->use_hrtimer) {
			hrtimer_cancel(&mx_data->poll_timer);
			cancel_work_sync(&mx_data->dwork.work);
		} else {
			cancel_delayed_work_sync(&mx_data->dwork);
		}
		udelay(MXG_DELAY_FOR_READY);
	}

	ret = mxg_operation_mode(mx_data->dev, MXG_MODE_POWER_DOWN);
	if (ret) {
		result = 1;
		return ret;
	}

	ret = mxg_operation_mode(mx_data->dev, MXG_MODE_SELF_TEST);
	if (ret) {
		result = 2;
		return ret;
	}

	mdelay(MXG_DELAY_FOR_READY);
	schedule_delayed_work(&mx_data->selftest_work, msecs_to_jiffies(100));

	ret = wait_event_interruptible_timeout(mx_data->selftest_wq,
			atomic_read(&mx_data->selftest_done),
			msecs_to_jiffies(10000));
	if (ret < 0) {
		dev_info(mx_data->dev, "Wait drdy event failed. (%d)\n", ret);
		result = 3;
		goto notify_result;
	} else if (!atomic_read(&mx_data->selftest_done)) {
		dev_info(mx_data->dev, "Drdy is not working...\n");
		result = 4;
		goto notify_result;
	}

	for (i = 0; i < 3; i++) {
		magn_self[i] = mx_data->self[i];
		dev_info(mx_data->dev, "self-test data [%d] : %d\n", i, magn_self[i]);
	}

	if (((magn_self[0] > MXG_COEF_X_MAX) || (magn_self[0] < MXG_COEF_X_MIN)) ||
			((magn_self[1] > MXG_COEF_Y_MAX) || (magn_self[1] < MXG_COEF_Y_MIN)) ||
			((magn_self[2] > MXG_COEF_Z_MAX) || (magn_self[2] < MXG_COEF_Z_MIN))) {
		dev_info(mx_data->dev, "self-test data out of range\n");
		result = 5;
		goto notify_result;
	}

notify_result:
	atomic_set(&mx_data->selftest_done, 0);
	if (atomic_read(&mx_data->en)) {
		mode = mxg_select_frequency(mx_data->odr * 1000000);
		mxg_operation_mode(mx_data->dev, MXG_MODE_POWER_DOWN);
		mxg_operation_mode(mx_data->dev, mode);
		if ((mx_data->use_hrtimer)) {
			hrtimer_start(&mx_data->poll_timer,
				ns_to_ktime(mx_data->odr * 1000000),
				HRTIMER_MODE_REL);
		} else {
			queue_delayed_work(mx_data->work_queue, &mx_data->dwork,
				(unsigned long)nsecs_to_jiffies64(mx_data->odr * 1000000));
		}
	}

    return result;
}

static ssize_t mxg_sensor_self_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct mxg_sensor_data *mxdata = dev_get_drvdata(dev);

	ret = mxg_do_self_test(mxdata);

	return snprintf(buf, PAGE_SIZE, "%s(%d):%d,%d,%d\n",
		ret?"failed":"success", ret, mxdata->self[0],
		mxdata->self[1], mxdata->self[2]);
}

static int mxg_file_open(struct inode *inode, struct file *filp)
{
	int ret;

	filp->private_data = mxdata;

	ret = nonseekable_open(inode, filp);
	if (ret) {
		dev_err(mxdata->dev, "nonseekable open failed. (%d)\n", ret);
		return ret;
	}
	return 0;
}

static int mxg_file_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long mxg_file_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	unsigned int value = 0;
	void __user *argp = (void __user *)arg;
	struct mxg_sensor_data *mx_data = filp->private_data;

	if ((argp == NULL) ||  (_IOC_TYPE(cmd) != 'M')) {
		dev_err(mx_data->dev, "invalid ioctl argument.");
		return -EINVAL;
	}

	switch (cmd) {
	case MXG_IOCTL_SET_ENABLE:
	case MXG_IOCTL_SET_DELAY:
		if (copy_from_user(&value, argp, sizeof(value))) {
			dev_err(mx_data->dev, "copy_from_user failed.");
			return -EFAULT;
		}
		break;
	case MXG_IOCTL_GET_ENABLE:
		value = (unsigned int) atomic_read(&mx_data->en);
		if (copy_to_user(argp, &value, sizeof(value))) {
			dev_err(mx_data->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case MXG_IOCTL_GET_DELAY:
		if (copy_to_user(argp, &mx_data->odr, sizeof(mx_data->odr))) {
			dev_err(mx_data->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case MXG_IOCTL_GET_INFO:
		if (copy_to_user(argp, &mx_data->info, sizeof(mx_data->info))) {
			dev_err(mx_data->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case MXG_IOCTL_GET_MCOEF:
		if (copy_to_user(argp, &mx_data->mcoef, sizeof(mx_data->mcoef))) {
			dev_err(mx_data->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	switch (cmd) {
	case MXG_IOCTL_SET_ENABLE:
		dev_err(mx_data->dev, "MXG_IOCTL_SET_ENABLE command.");
		ret = mxg_sensor_enable_disable(mx_data, (int)value);
		if (ret)
			return ret;
		break;
	case MXG_IOCTL_SET_DELAY:
		dev_err(mx_data->dev, "MXG_IOCTL_SET_DELAY command.");
		mxg_sensor_delay(mx_data, value);
		break;
	case MXG_IOCTL_GET_ENABLE:
		dev_err(mx_data->dev, "MXG_IOCTL_GET_ENABLE command.");
		break;
	case MXG_IOCTL_GET_DELAY:
		dev_err(mx_data->dev, "MXG_IOCTL_GET_DELAY command.");
		break;
	case MXG_IOCTL_GET_INFO:
		dev_err(mx_data->dev, "MXG_IOCTL_GET_INFO command.");
		break;
	case MXG_IOCTL_GET_MCOEF:
		dev_err(mx_data->dev, "MXG_IOCTL_GET_MCOEF command.");
		break;
	default:
		break;
	}

	return 0;
}


static int mxg_load_fuse_rom(struct mxg_sensor_data *sensor)
{
	int ret, i;

#if CHECK_IC_NUM_ENABLE
	ret = mxg_i2c_read_block(sensor->dev, 0x0, sensor->info, 2);
	if (ret) {
		dev_err(sensor->dev, "check ic number. (%d)\n", ret);
		return ret;
	}
	if ((sensor->info[0] == 0x48)) {
		dev_info(sensor->dev, "e-fuse write start.\n");
		for (i = 0; i < 9; i++)
			mxg_i2c_write_byte(sensor->dev, mxg_address_buf[i], mxg_write_buf[i]);
		msleep(3);
		mxg_i2c_write_byte(sensor->dev, mxg_address_buf[9], mxg_write_buf[9]);
		dev_info(sensor->dev, "e-fuse write success.\n");
	}
#endif

	ret = mxg_operation_mode(sensor->dev, MXG_MODE_FUSE_ACCESS);
	if (ret)
		return ret;

	ret = mxg_i2c_read_block(sensor->dev, MXG_REG_DID, sensor->info, 2);
	if (ret) {
		dev_err(sensor->dev, "failed to read device information. (%d)\n", ret);
		return ret;
	}

	if ((sensor->info[0] != MXG_CMP1_ID)) {
		dev_err(sensor->dev, "current id(0x%02X)(0x%02X) is not for %s.",
				sensor->info[0], sensor->info[1], MXG_DEV_NAME);
		return -ENXIO;
	} else {
		dev_err(sensor->dev, "current id(0x%02X)(0x%02X) is: %s.",
				sensor->info[0], sensor->info[1], MXG_DEV_NAME);
	}

	ret = mxg_i2c_read_block(sensor->dev, MXG_REG_XASA, sensor->mcoef, 3);
	if (ret) {
		dev_err(sensor->dev, "failed to read sensitivity. (%d)\n", ret);
		return ret;
	}

	ret = mxg_i2c_write_byte(sensor->dev, MXG_REG_CHOP, 0x95);
	if (ret)
		return ret;

	ret = mxg_operation_mode(sensor->dev, MXG_MODE_POWER_DOWN);
	if (ret)
		return ret;

	/* Set Choping num */

	for (i = 0; i < 3; i++) {
		dev_info(sensor->dev, "magmetic coef[%d] : %d\n", i, sensor->mcoef[i]);
	}
	for (i = 0; i < 2; i++) {
		dev_info(sensor->dev, "information[%d] : 0x%02X\n", i, sensor->info[i]);
	}

	return 0;
}


static void mxg_init_device(struct mxg_sensor_data *sensor)
{
	/* device instance specific init */
	mutex_init(&sensor->lock);

	sensor->odr = 20; /* James: 20ms=50Hz. */
	sensor->mode = MXG_MODE_POWER_DOWN;

	atomic_set(&sensor->selftest_done, 0);
	atomic_set(&sensor->en, 0);

	init_waitqueue_head(&sensor->selftest_wq);

	INIT_DELAYED_WORK(&sensor->selftest_work, mxg_self_test_work_func);

	/* Global Static Variable Setting */
	mxdata = sensor;
}

static int mxg_hw_init(struct mxg_sensor_data *sensor)
{
	int ret;

	ret = mxg_operation_mode(sensor->dev, MXG_MODE_POWER_DOWN);
	if (ret)
		return ret;

	/* loading e-fuse rom data */
	ret = mxg_load_fuse_rom(sensor);

	return ret;
}

static int mxg_input_dev_init(struct mxg_sensor_data *sensor)
{
	int ret;

	sensor->input_dev = input_allocate_device();
	if (!sensor->input_dev)
		return -ENOMEM;

	input_set_capability(sensor->input_dev, EV_ABS, ABS_MISC);
/*
	input_set_capability(sensor->input_dev, EV_SYN, SYN_TIME_NSEC);
	input_set_capability(sensor->input_dev, EV_SYN, SYN_TIME_SEC);
*/
	set_bit(EV_SYN, sensor->input_dev->evbit);

	input_set_abs_params(sensor->input_dev, ABS_X, MXG_DATA_MIN, MXG_DATA_MAX, 0, 0);
	input_set_abs_params(sensor->input_dev, ABS_Y, MXG_DATA_MIN, MXG_DATA_MAX, 0, 0);
	input_set_abs_params(sensor->input_dev, ABS_Z, MXG_DATA_MIN, MXG_DATA_MAX, 0, 0);

	input_set_capability(sensor->input_dev, EV_REL, SYN_TIME_SEC);
	input_set_capability(sensor->input_dev, EV_REL, SYN_TIME_NSEC);

	sensor->input_dev->name = MXG_INPUT_DEV_NAME;
	sensor->input_dev->id.bustype = BUS_I2C;
	sensor->input_dev->dev.parent = &sensor->client->dev;

	input_set_drvdata(sensor->input_dev, sensor);

	ret = input_register_device(sensor->input_dev);
	if (ret) {
		input_free_device(sensor->input_dev);
		dev_err(sensor->dev, "failed to register input device. (%d)\n", ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_OF
static int mxg_parse_dt(struct device *dev, struct mxg_platform_data *pdata)
{
	int ret;

	ret = of_property_read_u32_array(dev->of_node, "mx,position_array",
			pdata->position_array, 6);
	if (ret) {
		dev_err(dev, "read failed position_array.\n");
		return -EINVAL;
	}

	dev_info(dev, "position_array : [ %d %d %d %d %d %d ]\n",
			pdata->position_array[0], pdata->position_array[1],
			pdata->position_array[2], pdata->position_array[3],
			pdata->position_array[4], pdata->position_array[5]);

	/* reserved: if need, you could using "mx,use-hrtimer" */
	pdata->use_hrtimer = of_property_read_bool(dev->of_node, "mx,use-hrtimer");

	return 0;
}
#else
static int mxg_parse_dt(struct device *dev, struct mxg_platform_data *pdata)
{
	return -EINVAL;
}
#endif

static const struct file_operations mxg_file_fops = {
	.owner = THIS_MODULE,
	.open = mxg_file_open,
	.release = mxg_file_release,
	.unlocked_ioctl = mxg_file_ioctl,
};

static struct miscdevice mxg_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MXG_MISC_DEV_NAME,
	.fops = &mxg_file_fops,
};

static DEVICE_ATTR(enable, S_IWUSR | S_IWGRP | S_IRUGO,
		mxg_sensor_enable_show, mxg_sensor_enable_store);
static DEVICE_ATTR(delay, S_IWUSR | S_IWGRP | S_IRUGO,
		mxg_sensor_delay_show, mxg_sensor_delay_store);
static DEVICE_ATTR(mcoef, S_IRUGO, mxg_sensor_mcoef_show, NULL);
static DEVICE_ATTR(self, S_IRUGO, mxg_sensor_self_show, NULL);

static struct attribute *mxg_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_mcoef.attr,
	&dev_attr_self.attr,
	NULL
};

static struct attribute_group mxg_attribute_group = {
	.attrs = mxg_attributes
};

int mxg_i2c_drv_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct mxg_sensor_data *mx_data = NULL;

	dev_err(&client->dev, "mxg3300: probe at 0x%02x\n", client->addr);
	client->addr = client->addr<<1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c funcationality check error\n");
		return -ENODEV;
	}

	mx_data = devm_kzalloc(&client->dev, sizeof(struct mxg_sensor_data),
				GFP_KERNEL);
	if (!mx_data) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	mx_data->client = client;
	mx_data->dev = &client->dev;

	i2c_set_clientdata(client, mx_data);

	if (client->dev.of_node) {
		mx_data->plat_data = devm_kzalloc(&client->dev,
				sizeof(struct mxg_platform_data), GFP_KERNEL);
		if (!mx_data->plat_data) {
			dev_err(&client->dev, "failed to allcated platform "
				"data\n");
			ret = -ENOMEM;
			goto err_free_devmem;
		}
		/* Set up from Device Tree */
		ret = mxg_parse_dt(&client->dev, mx_data->plat_data);
		if (ret) {
			dev_err(&client->dev, "failed to parse device tree. "
				"(%d)\n", ret);
			ret = -EINVAL;
			goto err_free_devmem;
		}
	} else {
		mx_data->plat_data = client->dev.platform_data;
	}

	if (!mx_data->plat_data) {
		dev_err(&client->dev, "failed to initialize device platform "
			"data.\n");
		ret = -EINVAL;
		goto err_free_devmem;
	}

	mx_data->use_hrtimer = mx_data->plat_data->use_hrtimer;
	/* Initialize MX sensor data */
	mxg_init_device(mx_data);

	/* Sensor Power Register */
	ret = mxg_power_register(mx_data);
	if (ret) {
		dev_err(&client->dev, "failed to power register. (%d)\n", ret);
		goto err_free_devmem;
	}

	/* Sensor Power ON */
	ret = mxg_power_control(mx_data, true);
	if (ret) {
		dev_err(&client->dev, "failed to power control. (%d)\n", ret);
		goto err_power_unregister;
	}

	/* Haredware initialize */
	ret = mxg_hw_init(mx_data);
	if (ret < 0)
		goto err_power_control;

	/*  Initialize Input device */
	ret = mxg_input_dev_init(mx_data);
	if (ret < 0)
		goto err_free_devmem;

	ret = sysfs_create_group(&mx_data->input_dev->dev.kobj,
				&mxg_attribute_group);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs attribute. "
			"(%d)\n", ret);
		goto err_unregister_input;
	}

	/* Initialize misc device */
	ret = misc_register(&mxg_misc_dev);
	if (ret < 0) {
		dev_err(&client->dev, "misc device register failed. (%d)\n",
			ret);
		goto err_remove_sysfs_group;
	}

	if (mx_data->use_hrtimer) {
		dev_info(&client->dev, "mxg3300 probe -- use_hrtimer.\n");
		hrtimer_init(&mx_data->poll_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
		mx_data->poll_timer.function = mx_timer_func;
		mx_data->work_queue = alloc_workqueue("mx_poll_work",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
		INIT_WORK(&mx_data->dwork.work, mx_dev_poll);
	} else {
		mx_data->work_queue = alloc_workqueue("mx_poll_work", 0, 0);
		INIT_DELAYED_WORK(&mx_data->dwork, mx_dev_poll);
	}

	/* Initialize Sensor class device */
	ret = mxg_sensors_cdev_class_init(mx_data);
	if (ret < 0)
		goto err_unregister_misc;

	dev_info(&client->dev, "mxg3300 probe device success.\n");

	/* Enter the power down state */
	mxg_power_control(mx_data, false);

	return 0;

err_unregister_misc:
	misc_deregister(&mxg_misc_dev);
err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxg_attribute_group);
err_unregister_input:
	input_unregister_device(mx_data->input_dev);
err_power_control:
	mxg_power_control(mx_data, false);
err_power_unregister:
	mxg_power_unregister(mx_data);
err_free_devmem:
	devm_kfree(&client->dev, mx_data);
	dev_err(&client->dev, "probe device return error (%d)\n", ret);
	return ret;
}

static int mxg_i2c_drv_remove(struct i2c_client *client)
{
	struct mxg_sensor_data *mx_data = i2c_get_clientdata(client);

	if (mx_data->use_hrtimer) {
		hrtimer_cancel(&mx_data->poll_timer);
		cancel_work_sync(&mx_data->dwork.work);
	} else {
		cancel_delayed_work_sync(&mx_data->dwork);
	}
	destroy_workqueue(mx_data->work_queue);

	sensors_classdev_unregister(&mx_data->sensor_cdev);
	misc_deregister(&mxg_misc_dev);
	sysfs_remove_group(&client->dev.kobj, &mxg_attribute_group);
	input_unregister_device(mx_data->input_dev);

	mxg_power_control(mx_data, false);
	mxg_power_unregister(mx_data);

	devm_kfree(&client->dev, mx_data);
	mxdata = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int mxg_i2c_drv_suspend(struct device *dev)
{
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);

	if (atomic_read(&mx_data->en)) {
		if (mx_data->use_hrtimer) {
			hrtimer_cancel(&mx_data->poll_timer);
			cancel_work_sync(&mx_data->dwork.work);
		} else {
			cancel_delayed_work_sync(&mx_data->dwork);
		}
		mxg_operation_mode(mx_data->dev, MXG_MODE_POWER_DOWN);
	}

	if (mx_data->power_on)
		mxg_power_control(mx_data, false);

	dev_info(mx_data->dev, "mxg_i2c_drv_suspend\n");
	return 0;
}

static int mxg_i2c_drv_resume(struct device *dev)
{
	struct mxg_sensor_data *mx_data = dev_get_drvdata(dev);
	int ret, msec;
	uint8_t mode;
	int64_t delay_ns;

	if (mx_data->power_on) {
		mxg_power_control(mx_data, true);
	}
	if (atomic_read(&mx_data->en)) {
		mode = mxg_select_frequency(mx_data->odr * 1000000);
		ret = mxg_operation_mode(mx_data->dev, mode);
		if (ret) {
			dev_err(mx_data->dev, "failed on mxg_operation_mode.\n");
			return ret;
		}

		msec = mx_data->odr;
		delay_ns = msec * 1000000;

		if ((mx_data->use_hrtimer)) {
			hrtimer_start(&mx_data->poll_timer, ns_to_ktime(delay_ns), HRTIMER_MODE_REL);
		} else {
			queue_delayed_work(mx_data->work_queue, &mx_data->dwork,
				(unsigned long)nsecs_to_jiffies64(delay_ns));
		}
	}

	dev_info(mx_data->dev, "mxg_i2c_drv_resume\n");
	return 0;
}
#else
#define mxg_i2c_drv_suspend NULL
#define mxg_i2c_drv_resume NULL
#endif

static UNIVERSAL_DEV_PM_OPS(mxg_pm_ops, mxg_i2c_drv_suspend, mxg_i2c_drv_resume, NULL);
#define MXG_PM_OPS (&mxg_pm_ops)

static const struct i2c_device_id mxg_i2c_drv_id_table[] = {
	{MXG_DEV_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, mxg_i2c_drv_id_table);

static struct of_device_id mxg_match_table[] = {
	{ .compatible = "mx,g3300", },
	{ },
};

static const unsigned short detect_i2c[] = { I2C_CLIENT_END };

static struct i2c_driver mxg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MXG_DEV_NAME,
		.of_match_table = mxg_match_table,
		.pm = MXG_PM_OPS,
	},
	.probe          = mxg_i2c_drv_probe,
	.remove         = mxg_i2c_drv_remove,
	.id_table       = mxg_i2c_drv_id_table,
	.address_list   = detect_i2c,
};

module_i2c_driver(mxg_driver);

MODULE_DESCRIPTION("MXG3300 e-compass driver");
MODULE_LICENSE("GPL");
