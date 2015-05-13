/*
 *  Copyright (C) 2012-2014 Motorola, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/m4sensorhub/m4sensorhub_pedometer.h>

#define m4ped_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4PED_IRQ_ENABLED_BIT       0
#define M4PED_FEATURE_ENABLED_BIT   1

struct m4ped_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_pedometer_iio_data
		iiodat[M4FUS_NUM_PEDOMETER_BUFFERS];
	struct m4sensorhub_pedometer_iio_data   base_dat;
	
	int16_t         samplerate;
	int16_t         latest_samplerate;

	uint16_t        status;
};

static int m4ped_read_report_data(struct iio_dev *iio,
				  struct m4ped_driver_data *dd)
{
	int err = 0, size = 0;
	long long timestamp = 0L;

	/*input validations */
	if ((iio == NULL) || (dd == NULL)) {
		m4ped_err("%s: invalid inputs passed in\n", __func__);
		return -EINVAL;
	}

	dd->iiodat[0].type = PEDOMETER_TYPE_EVENT_1;
	dd->iiodat[1].type = PEDOMETER_TYPE_EVENT_2;

	timestamp = ktime_to_ns(ktime_get_boottime());
	dd->iiodat[0].timestamp = timestamp;
	dd->iiodat[1].timestamp = timestamp;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PEDOMETER_ACTIVITY);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_ACTIVITY,
		(char *)&(dd->iiodat[0].event1.ped_activity));
	if (err < 0) {
		m4ped_err("%s: Failed to read ped_activity data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "ped_activity");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4,
		M4SH_REG_PEDOMETER_TOTALDISTANCE);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_TOTALDISTANCE,
		(char *)&(dd->iiodat[0].event1.total_distance));
	if (err < 0) {
		m4ped_err("%s: Failed to read total_distance data.\n",
			  __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "total_distance");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PEDOMETER_TOTALSTEPS);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_TOTALSTEPS,
		(char *)&(dd->iiodat[0].event1.total_steps));
	if (err < 0) {
		m4ped_err("%s: Failed to read total_steps data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "total_steps");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4,
		M4SH_REG_PEDOMETER_CURRENTSPEED);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_CURRENTSPEED,
		(char *)&(dd->iiodat[0].event1.current_speed));
	if (err < 0) {
		m4ped_err("%s: Failed to read current_speed data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "current_speed");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_METS_HEALTHYMINUTES);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_METS_HEALTHYMINUTES,
		(char *)&(dd->iiodat[0].event1.healthy_minutes));
	if (err < 0) {
		m4ped_err("%s: Failed to read healthy_minutes data.\n",
			  __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "healthy_minutes");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_METS_CALORIES);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_METS_CALORIES,
		(char *)&(dd->iiodat[0].event1.calories));
	if (err < 0) {
		m4ped_err("%s: Failed to read calories data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "calories");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_METS_CALORIES_NO_RMR);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_METS_CALORIES_NO_RMR,
		(char *)&(dd->iiodat[1].event2.calories_normr));
	if (err < 0) {
		m4ped_err("%s: Failed to read calories normr data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "calories_normr");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4,
		M4SH_REG_PEDOMETER_STEPFREQUENCY);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_STEPFREQUENCY,
		(char *)&(dd->iiodat[1].event2.step_frequency));
	if (err < 0) {
		m4ped_err("%s: Failed to read step frequency data.\n",
			  __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "step_frequency");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PEDOMETER_STEPLENGTH);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PEDOMETER_STEPLENGTH,
		(char *)&(dd->iiodat[1].event2.step_length));
	if (err < 0) {
		m4ped_err("%s: Failed to read step length data.\n", __func__);
		goto m4ped_read_fail;
	} else if (err != size) {
		m4ped_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "step_length");
		err = -EBADE;
		goto m4ped_read_fail;
	}

	dd->iiodat[0].event1.total_distance +=
		dd->base_dat.event1.total_distance;
	dd->iiodat[0].event1.total_steps +=
		dd->base_dat.event1.total_steps;
	dd->iiodat[0].event1.healthy_minutes +=
		dd->base_dat.event1.healthy_minutes;
	dd->iiodat[0].event1.calories +=
		dd->base_dat.event1.calories;

	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));

m4ped_read_fail:
	if (err < 0)
		m4ped_err("%s: Failed with error code %d.\n", __func__, err);

	return err;
}

static void m4ped_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	int err = 0;
	struct iio_dev *iio = handle;
	struct m4ped_driver_data *dd = iio_priv(iio);
	mutex_lock(&(dd->mutex));
	err = m4ped_read_report_data(iio, dd);
	if (err < 0)
		m4ped_err("%s: Failed with error code %d.\n", __func__, err);
	mutex_unlock(&(dd->mutex));
	return;
}

static int m4ped_set_samplerate(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4ped_driver_data *dd = iio_priv(iio);

	/*
	 * Currently, there is no concept of setting a sample rate for this
	 * sensor, so this function only enables/disables interrupt reporting.
	 */
	dd->latest_samplerate = rate;
	if (rate == dd->samplerate)
		goto m4ped_set_samplerate_fail;

	if (rate >= 0) {
		/* Enable the IRQ if necessary */
		if (!(dd->status & (1 << M4PED_IRQ_ENABLED_BIT))) {
			err = m4sensorhub_irq_enable(dd->m4,
				M4SH_NOWAKEIRQ_PEDOMETER);
			if (err < 0) {
				m4ped_err("%s: Failed to enable ped irq.\n",
					  __func__);
				goto m4ped_set_samplerate_fail;
			}

			dd->status = dd->status | (1 << M4PED_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	} else {
		/* Disable the IRQ if necessary */
		if (dd->status & (1 << M4PED_IRQ_ENABLED_BIT)) {
			err = m4sensorhub_irq_disable(dd->m4,
				M4SH_NOWAKEIRQ_PEDOMETER);
			if (err < 0) {
				m4ped_err("%s: Failed to disable ped irq.\n",
					  __func__);
				goto m4ped_set_samplerate_fail;
			}

			dd->status = dd->status & ~(1 << M4PED_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	}

m4ped_set_samplerate_fail:
	return err;
}

static ssize_t m4ped_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4ped_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4ped_err("%s: Failed to convert value.\n", __func__);
		goto m4ped_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4ped_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4ped_enable_store_exit;
	}

	err = m4ped_set_samplerate(iio, value);
	if (err < 0) {
		m4ped_err("%s: Failed to set sample rate.\n", __func__);
		goto m4ped_enable_store_exit;
	}
	if (value >= 0) {
		/* When an app registers, there is no data reported
		unless the user starts walking. But the application
		would like to have atleast one set of data sent
		immediately following the register */
		err = m4ped_read_report_data(iio, dd);
		if (err < 0) {
			m4ped_err("%s:Failed to report pedo data\n", __func__);
			goto m4ped_enable_store_exit;
		}
	}

m4ped_enable_store_exit:
	if (err < 0) {
		m4ped_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4ped_setrate_show, m4ped_setrate_store, 0);

static ssize_t m4ped_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE,
		"%s%hhu\n%s%u\n%s%u\n%s%hu\n%s%u\n%s%u\n%s%u\n%s%hu\n%s%hu\n",
		"ped_activity: ", dd->iiodat[0].event1.ped_activity,
		"total_distance: ", dd->iiodat[0].event1.total_distance,
		"total_steps: ", dd->iiodat[0].event1.total_steps,
		"current_speed: ", dd->iiodat[0].event1.current_speed,
		"healthy_minutes: ", dd->iiodat[0].event1.healthy_minutes,
		"calories: ", dd->iiodat[0].event1.calories,
		"calories_normr: ", dd->iiodat[1].event2.calories_normr,
		"step_frequency: ", dd->iiodat[1].event2.step_frequency,
		"step_length: ", dd->iiodat[1].event2.step_length);
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4ped_iiodata_show, NULL, 0);

static ssize_t m4ped_userdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;
	uint8_t data[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

	mutex_lock(&(dd->mutex));

	err = m4sensorhub_reg_read_n(dd->m4, M4SH_REG_USERSETTINGS_SCREENSTATUS,
		(char *)&(data[0]), ARRAY_SIZE(data));
	if (err < 0) {
		m4ped_err("%s: Failed to read user data.\n", __func__);
		goto m4ped_userdata_show_fail;
	} else if (err < ARRAY_SIZE(data)) {
		m4ped_err("%s: Read %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4ped_userdata_show_fail;
	}

	if (data[0] > 0) {
		m4ped_err("%s: User settings version is too new (0x%02X)\n",
			  __func__, data[0]);
		err = -EINVAL;
		goto m4ped_userdata_show_fail;
	}

	size = snprintf(buf, PAGE_SIZE,
		"%s%s\n%s%hhu\n%s%hhu\n%s%hhu\n",
		"Gender (M/F): ", data[2] ? "M" : "F",
		"Age    (yrs): ", data[1],
		"Height  (cm): ", data[3],
		"Weight  (kg): ", data[4]);

m4ped_userdata_show_fail:
	mutex_unlock(&(dd->mutex));

	if (err < 0)
		size = snprintf(buf, PAGE_SIZE, "Read failed (check dmesg)\n");

	return size;
}
/*
 * Expected input is Gender, Age, Height, Weight
 * Example:
 *    Female, 22, 168cm, 49kg
 *    0x00,0x16,0xA7,0x31\n
 */
static ssize_t m4ped_userdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	unsigned int value = 0;
	unsigned char convbuf[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char outbuf[4] = {0x00, 0x00, 0x00, 0x00};
	int i = 0;

	mutex_lock(&(dd->mutex));

	/* Size includes a terminating '\n' but not the terminating null */
	if (size != 20) {
		m4ped_err("%s: Invalid data format.  %s\n",
			  __func__, "Use \"0xHH,0xHH,0xHH,0xHH\\n\" instead.");
		err = -EINVAL;
		goto m4ped_userdata_store_fail;
	}

	for (i = 0; i < 4; i++) {
		memcpy(convbuf, &(buf[i*5]), 4);
		err = kstrtouint(convbuf, 16, &value);
		if (err < 0) {
			m4ped_err("%s: Argument %d conversion failed.\n",
				  __func__, i);
			goto m4ped_userdata_store_fail;
		} else if (value > 255) {
			m4ped_err("%s: Value 0x%08X is too large.\n",
				  __func__, value);
			err = -EOVERFLOW;
			goto m4ped_userdata_store_fail;
		}
		outbuf[i] = (unsigned char) value;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_USERSETTINGS_USERAGE,
		&(outbuf[1]), m4sh_no_mask);
	if (err < 0) {
		m4ped_err("%s: Failed to write user data.\n", __func__);
		goto m4ped_userdata_store_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_USERSETTINGS_USERGENDER,
		&(outbuf[0]), m4sh_no_mask);
	if (err < 0) {
		m4ped_err("%s: Failed to write user data.\n", __func__);
		goto m4ped_userdata_store_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_USERSETTINGS_USERHEIGHT,
		&(outbuf[2]), m4sh_no_mask);
	if (err < 0) {
		m4ped_err("%s: Failed to write user data.\n", __func__);
		goto m4ped_userdata_store_fail;
	}

	err = m4sensorhub_reg_write_n(dd->m4, M4SH_REG_USERSETTINGS_USERWEIGHT,
		&(outbuf[3]), m4sh_no_mask, 1);
	if (err < 0) {
		m4ped_err("%s: Failed to write user data.\n", __func__);
		goto m4ped_userdata_store_fail;
	}

	err = size;

m4ped_userdata_store_fail:
	mutex_unlock(&(dd->mutex));

	return (ssize_t) err;
}
static IIO_DEVICE_ATTR(userdata, S_IRUSR | S_IWUSR,
		m4ped_userdata_show, m4ped_userdata_store, 0);

static ssize_t m4ped_feature_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4PED_FEATURE_ENABLED_BIT))
		size = snprintf(buf, PAGE_SIZE, "Enabled\n");
	else
		size = snprintf(buf, PAGE_SIZE, "Disabled\n");
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4ped_feature_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = iio_priv(iio);
	int value = 0;
	uint8_t enable = 0x00;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4ped_err("%s: Failed to convert value.\n", __func__);
		goto m4ped_feature_enable_store_fail;
	}

	if (value == 0) {
		if (dd->status & (1 << M4PED_FEATURE_ENABLED_BIT)) {
			enable = 0x00;
			err = m4sensorhub_reg_write(dd->m4,
				M4SH_REG_PEDOMETER_ENABLE, &enable,
				m4sh_no_mask);
			if (err < 0) {
				m4ped_err("%s: Failed to write ped disable.\n",
					__func__);
				goto m4ped_feature_enable_store_fail;
			}

			err = m4sensorhub_reg_write(dd->m4,
				M4SH_REG_METS_ENABLE, &enable,
				m4sh_no_mask);
			if (err < 0) {
				m4ped_err("%s: Failed to write mets disable.\n",
					__func__);
				goto m4ped_feature_enable_store_fail;
			}

			dd->status = dd->status &
				~(1 << M4PED_FEATURE_ENABLED_BIT);
		}
	} else {
		if (!(dd->status & (1 << M4PED_FEATURE_ENABLED_BIT))) {
			enable = 0x01;
			err = m4sensorhub_reg_write(dd->m4,
				M4SH_REG_PEDOMETER_ENABLE, &enable,
				m4sh_no_mask);
			if (err < 0) {
				m4ped_err("%s: Failed to write ped enable.\n",
					__func__);
				goto m4ped_feature_enable_store_fail;
			}

			err = m4sensorhub_reg_write(dd->m4,
				M4SH_REG_METS_ENABLE, &enable,
				m4sh_no_mask);
			if (err < 0) {
				m4ped_err("%s: Failed to write mets enable.\n",
					__func__);
				goto m4ped_feature_enable_store_fail;
			}

			dd->status = dd->status |
				1 << M4PED_FEATURE_ENABLED_BIT;
		}
	}

m4ped_feature_enable_store_fail:
	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(feature_enable, S_IRUSR | S_IWUSR,
		m4ped_feature_enable_show, m4ped_feature_enable_store, 0);

static struct attribute *m4ped_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	&iio_dev_attr_userdata.dev_attr.attr,
	&iio_dev_attr_feature_enable.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4ped_iio_attr_group = {
	.attrs = m4ped_iio_attributes,
};

static const struct iio_info m4ped_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4ped_iio_attr_group,
};

static const struct iio_chan_spec m4ped_iio_channels[] = {
	{
		.type = IIO_PEDOMETER,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4PED_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4PED_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4ped_remove_iiodev(struct iio_dev *iio)
{
	struct m4ped_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4ped_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4ped_driver_data *dd = iio_priv(iio);

	iio->name = M4PED_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4ped_iio_info;
	iio->channels = m4ped_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4ped_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4ped_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4ped_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4ped_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4ped_err("%s: Failed to register IIO device.\n", __func__);
		goto m4ped_create_iiodev_iioreg_fail;
	}

	goto m4ped_create_iiodev_exit;

m4ped_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4ped_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4ped_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4ped_create_iiodev_exit:
	return err;
}

static void m4ped_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	struct m4ped_driver_data *dd = (struct m4ped_driver_data *)data;
	int err = 0;
	uint8_t disable = 0x00;

	if (dd == NULL) {
		m4ped_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	dd->base_dat.event1.total_distance =
		dd->iiodat[0].event1.total_distance;
	dd->base_dat.event1.total_steps =
		dd->iiodat[0].event1.total_steps;
	dd->base_dat.event1.healthy_minutes =
		dd->iiodat[0].event1.healthy_minutes;
	dd->base_dat.event1.calories =
		dd->iiodat[0].event1.calories;
	m4ped_err("%s: Pedometer bases after panic = %d %d %d %d", __func__,
		  dd->base_dat.event1.total_distance,
		  dd->base_dat.event1.total_steps,
		  dd->base_dat.event1.healthy_minutes,
		  dd->base_dat.event1.calories);

	if (!(dd->status & (1 << M4PED_FEATURE_ENABLED_BIT))) {
		err = m4sensorhub_reg_write(dd->m4, M4SH_REG_PEDOMETER_ENABLE,
			&disable, m4sh_no_mask);
		if (err < 0) {
			m4ped_err("%s: Failed to write ped disable (%d).\n",
				__func__, err);
			goto m4ped_panic_restore_fail;
		}

		err = m4sensorhub_reg_write(dd->m4, M4SH_REG_METS_ENABLE,
			&disable, m4sh_no_mask);
		if (err < 0) {
			m4ped_err("%s: Failed to write mets disable (%d).\n",
				__func__, err);
			goto m4ped_panic_restore_fail;
		}
	}
m4ped_panic_restore_fail:
	mutex_unlock(&(dd->mutex));
}

static int m4ped_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4ped_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4ped_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4ped_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_NOWAKEIRQ_PEDOMETER, m4ped_isr, iio, 0);
	if (err < 0) {
		m4ped_err("%s: Failed to register M4 PED IRQ.\n", __func__);
		goto m4ped_driver_init_fail;
	}

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_PEDOMETER_RESTORE,
					 m4ped_panic_restore, dd);
	if (err < 0)
		m4ped_err("Pedometer panic callbk register failed\n");

	goto m4ped_driver_init_exit;

m4ped_driver_init_fail:
	m4ped_err("%s: Init failed with error code %d.\n", __func__, err);
m4ped_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4ped_probe(struct platform_device *pdev)
{
	struct m4ped_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4ped_driver_data));
	if (iio == NULL) {
		m4ped_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4ped_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */
	dd->latest_samplerate = dd->samplerate;
    dd->status = dd->status | (1 << M4PED_FEATURE_ENABLED_BIT);

	err = m4ped_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4ped_err("%s: Failed to create IIO device.\n", __func__);
		goto m4ped_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4ped_driver_init, iio);
	if (err < 0) {
		m4ped_err("%s: Failed to register initcall.\n", __func__);
		goto m4ped_probe_fail;
	}

	return 0;

m4ped_probe_fail:
	m4ped_remove_iiodev(iio); /* iio and dd are freed here */
m4ped_probe_fail_noiio:
	m4ped_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4ped_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ped_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4ped_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4ped_remove_exit;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4PED_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4,
					M4SH_NOWAKEIRQ_PEDOMETER);
		dd->status = dd->status & ~(1 << M4PED_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4,
				   M4SH_NOWAKEIRQ_PEDOMETER);
	m4sensorhub_unregister_initcall(m4ped_driver_init);
	m4ped_remove_iiodev(iio);  /* dd is freed here */

m4ped_remove_exit:
	return 0;
}

static struct of_device_id m4pedometer_match_tbl[] = {
	{ .compatible = "mot,m4pedometer" },
	{},
};

static struct platform_driver m4ped_driver = {
	.probe		= m4ped_probe,
	.remove		= __exit_p(m4ped_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4PED_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4pedometer_match_tbl),
	},
};

static int __init m4ped_init(void)
{
	return platform_driver_register(&m4ped_driver);
}

static void __exit m4ped_exit(void)
{
	platform_driver_unregister(&m4ped_driver);
}

module_init(m4ped_init);
module_exit(m4ped_exit);

MODULE_ALIAS("platform:m4ped");
MODULE_DESCRIPTION("M4 Sensor Hub Pedometer client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
