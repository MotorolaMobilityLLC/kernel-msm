/*
 *  Copyright (C) 2014 Motorola, Inc.
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
#include <linux/m4sensorhub_batch.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/m4sensorhub/m4sensorhub_gyro.h>
#include <linux/delay.h>

#define m4gyr_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4GYR_IRQ_ENABLED_BIT       0

struct m4gyr_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_gyro_iio_data   iiodat;
	int32_t         samplerate;
	int32_t         maxlatency;
	uint8_t         enable;
	uint16_t        status;

	uint8_t         dbg_addr;
};

static void m4gyr_data_callback(void *event_data, void *priv_data,
	int64_t monobase, int num_events)
{
	struct iio_dev *iio = (struct iio_dev *)priv_data;
	struct m4gyr_driver_data *dd = iio_priv(iio);
	struct m4sensorhub_batch_sample *fifo_data =
		(struct m4sensorhub_batch_sample *)event_data;
	int i;
	int64_t cur_ts;

	mutex_lock(&(dd->mutex));

	if (!dd->enable)
		goto m4gyr_data_callback_exit;

	/* Need to set the correct timebase first */
	cur_ts = monobase;
	for (i = num_events - 1; i >= 0; i--) {
		if (fifo_data[i].sensor_id != M4SH_TYPE_GYRO)
			continue;

		cur_ts -= (fifo_data[i].ts_delta * 1000000);
	}
	/* cur_ts should be before the timestamp of the first event */

	for (i = 0; i < num_events; i++) {
		if (fifo_data[i].sensor_id != M4SH_TYPE_GYRO)
			continue;

		if (fifo_data[i].cmd_id == M4SH_BATCH_CMD_FLUSH) {
			dd->iiodat.type = GYRO_TYPE_EVENT_FLUSH;
		} else {
			dd->iiodat.type = GYRO_TYPE_EVENT_DATA;
			dd->iiodat.event_data.x = fifo_data[i].sensor_data[0];
			dd->iiodat.event_data.y = fifo_data[i].sensor_data[1];
			dd->iiodat.event_data.z = fifo_data[i].sensor_data[2];
		}

		cur_ts += (fifo_data[i].ts_delta * 1000000);
		dd->iiodat.timestamp = cur_ts;
		iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));
	}
	/* cur_ts should be the monobase value again */

	if (cur_ts != monobase)
		m4gyr_err("%s: %s (monobase=%lld, cur_ts=%lld)\n", __func__,
			"Timebase alignment error", monobase, cur_ts);

m4gyr_data_callback_exit:
	mutex_unlock(&(dd->mutex));
	return;
}

static int m4gyr_set_samplerate_locked(struct iio_dev *iio, int32_t rate)
{
	int err = 0;
	int size = 0;
	struct m4gyr_driver_data *dd = iio_priv(iio);

	if (rate == dd->samplerate)
		goto m4gyr_set_samplerate_fail;

	size = m4sensorhub_reg_getsize(dd->m4,
		M4SH_REG_GYRO_SAMPLERATE);
	if (size < 0) {
		m4gyr_err("%s: Writing to invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4gyr_set_samplerate_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_GYRO_SAMPLERATE,
		(char *)&rate, m4sh_no_mask);
	if (err < 0) {
		m4gyr_err("%s: Failed to set sample rate.\n", __func__);
		goto m4gyr_set_samplerate_fail;
	} else if (err != size) {
		m4gyr_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4gyr_set_samplerate_fail;
	}
	dd->samplerate = rate;

m4gyr_set_samplerate_fail:
	return err;
}

static int m4gyr_set_maxlatency(struct iio_dev *iio, uint16_t maxlatency)
{
	int err = 0;
	int size = 0;
	struct m4gyr_driver_data *dd = iio_priv(iio);

	if (maxlatency == dd->maxlatency)
		goto m4gyr_set_maxlatency_fail;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_GYRO_REPORTLATENCY);
	if (size < 0) {
		m4gyr_err("%s: Writing to invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4gyr_set_maxlatency_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_GYRO_REPORTLATENCY,
		(char *)&maxlatency, m4sh_no_mask);
	if (err < 0) {
		m4gyr_err("%s: Failed to set maxlatency.\n", __func__);
		goto m4gyr_set_maxlatency_fail;
	} else if (err != size) {
		m4gyr_err("%s: Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4gyr_set_maxlatency_fail;
	}
	dd->maxlatency = maxlatency;

m4gyr_set_maxlatency_fail:
	return err;
}

static int m4gyr_set_flush(struct iio_dev *iio, uint8_t value)
{
	int err = 0;
	int size = 0;
	struct m4gyr_driver_data *dd = iio_priv(iio);

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_GYRO_FLUSH);
	if (size < 0) {
		m4gyr_err("%s: Writing to invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4gyr_set_flush_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_GYRO_FLUSH,
		(char *)&value, m4sh_no_mask);
	if (err < 0) {
		m4gyr_err("%s: Failed to set flush.\n", __func__);
		goto m4gyr_set_flush_fail;
	} else if (err != size) {
		m4gyr_err("%s: Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4gyr_set_flush_fail;
	}

m4gyr_set_flush_fail:
	return err;
}

static ssize_t m4gyr_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4gyr_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4gyr_err("%s: Failed to convert value.\n", __func__);
		goto m4gyr_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4gyr_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EOVERFLOW;
		goto m4gyr_enable_store_exit;
	}

	err = m4gyr_set_samplerate_locked(iio, value);
	if (err < 0) {
		m4gyr_err("%s: Failed to set sample rate.\n", __func__);
		goto m4gyr_enable_store_exit;
	}

m4gyr_enable_store_exit:
	if (err < 0) {
		m4gyr_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4gyr_setrate_show, m4gyr_setrate_store, 0);

static ssize_t m4gyr_maxlatency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t size = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current max_latency: %hu\n",
		dd->maxlatency);
	mutex_unlock(&(dd->mutex));
	return size;
}

static ssize_t m4gyr_maxlatency_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);
	uint32_t value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtouint(buf, 10, &value);
	if (err < 0) {
		m4gyr_err("%s: Failed to convert value.\n", __func__);
		goto m4gyr_maxlatency_store_exit;
	}

	if (value > 65535) {
		m4gyr_err("%s: Invalid maxlatency requested = %d\n",
			  __func__, value);
		err = -EOVERFLOW;
		goto m4gyr_maxlatency_store_exit;
	}

	err = m4gyr_set_maxlatency(iio, (uint16_t)value);
	if (err < 0) {
		m4gyr_err("%s: Failed to set maxlatency.\n", __func__);
		goto m4gyr_maxlatency_store_exit;
	}

m4gyr_maxlatency_store_exit:
	if (err < 0) {
		m4gyr_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}
	mutex_unlock(&(dd->mutex));

	return size;
}

static IIO_DEVICE_ATTR(maxlatency, S_IRUSR | S_IWUSR,
		m4gyr_maxlatency_show, m4gyr_maxlatency_store, 0);

static ssize_t m4gyr_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);
	uint8_t value = 1;

	mutex_lock(&(dd->mutex));

	err = m4gyr_set_flush(iio, (uint8_t)value);
	if (err < 0) {
		m4gyr_err("%s: Failed to set flush.\n", __func__);
		goto m4gyr_flush_store_exit;
	}

m4gyr_flush_store_exit:
	if (err < 0) {
		m4gyr_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}
	mutex_unlock(&(dd->mutex));

	return size;
}

static IIO_DEVICE_ATTR(flush, S_IRUSR | S_IWUSR,
		NULL, m4gyr_flush_store, 0);

static ssize_t m4gyr_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	if (dd->enable == 1) {
		if (dd->iiodat.type == GYRO_TYPE_EVENT_DATA) {
			size = snprintf(buf, PAGE_SIZE, "%s%d\n%s%d\n%s%d\n",
				"X: ", dd->iiodat.event_data.x,
				"Y: ", dd->iiodat.event_data.y,
				"Z: ", dd->iiodat.event_data.z);
		} else {
			size = snprintf(buf, PAGE_SIZE, "%s\n", "Flush event");
		}
	} else {
		size = snprintf(buf, PAGE_SIZE, "%s\n", "Gyro is disabled");
	}
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4gyr_iiodata_show, NULL, 0);

static struct attribute *m4gyr_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_maxlatency.dev_attr.attr,
	&iio_dev_attr_flush.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4gyr_iio_attr_group = {
	.attrs = m4gyr_iio_attributes,
};

static const struct iio_info m4gyr_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4gyr_iio_attr_group,
};

static const struct iio_chan_spec m4gyr_iio_channels[] = {
	{
		.type = IIO_GYRO,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4GYR_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4GYR_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4gyr_remove_iiodev(struct iio_dev *iio)
{
	struct m4gyr_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4gyr_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4gyr_driver_data *dd = iio_priv(iio);

	iio->name = M4GYR_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4gyr_iio_info;
	iio->channels = m4gyr_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4gyr_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4gyr_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4gyr_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4gyr_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4gyr_err("%s: Failed to register IIO device.\n", __func__);
		goto m4gyr_create_iiodev_iioreg_fail;
	}

	goto m4gyr_create_iiodev_exit;

m4gyr_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4gyr_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4gyr_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4gyr_create_iiodev_exit:
	return err;
}

static void m4gyr_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	int size, err;
	struct m4gyr_driver_data *dd = (struct m4gyr_driver_data *)data;

	if (dd == NULL) {
		m4gyr_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4,
				      M4SH_REG_GYRO_SAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_GYRO_SAMPLERATE,
				   (char *)&dd->samplerate, m4sh_no_mask);
	if (err < 0) {
		m4gyr_err("%s: Failed to set sample rate.\n", __func__);
	} else if (err != size) {
		m4gyr_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
	}

	size = m4sensorhub_reg_getsize(dd->m4,
				      M4SH_REG_GYRO_REPORTLATENCY);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_GYRO_REPORTLATENCY,
				   (char *)&dd->maxlatency, m4sh_no_mask);
	if (err < 0) {
		m4gyr_err("%s: Failed to set maxlatency.\n", __func__);
	} else if (err != size) {
		m4gyr_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
	}
	mutex_unlock(&(dd->mutex));
}

static int m4gyr_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4gyr_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	err = m4sensorhub_batch_register_data_callback(
		M4SH_BATCH_SENSOR_TYPE_GYRO, iio, m4gyr_data_callback);
	if (err < 0) {
		m4gyr_err("%s: Failed to register callback for batch err:%d\n",
			  __func__, err);
	}

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_GYRO_RESTORE,
					m4gyr_panic_restore, dd);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "Gyro panic callback register failed\n");

	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4gyr_probe(struct platform_device *pdev)
{
	struct m4gyr_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4gyr_driver_data));
	if (iio == NULL) {
		m4gyr_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4gyr_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */
	dd->enable = 1; /* TODO: This needs to change to 0 */
	dd->maxlatency = 0;

	dd->m4 = m4sensorhub_client_get_drvdata();
	if (dd->m4 == NULL) {
		m4gyr_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4gyr_probe_fail_noiio;
	}

	err = m4gyr_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4gyr_err("%s: Failed to create IIO device.\n", __func__);
		goto m4gyr_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4gyr_driver_init, iio);
	if (err < 0) {
		m4gyr_err("%s: Failed to register initcall.\n", __func__);
		goto m4gyr_probe_fail;
	}

	return 0;
m4gyr_probe_fail:
	m4gyr_remove_iiodev(iio); /* iio and dd are freed here */
m4gyr_probe_fail_noiio:
	m4gyr_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4gyr_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4gyr_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4gyr_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4gyr_remove_exit;

	mutex_lock(&(dd->mutex));
	m4sensorhub_unregister_initcall(m4gyr_driver_init);
	m4sensorhub_batch_unregister_data_callback(M4SH_BATCH_SENSOR_TYPE_GYRO,
		m4gyr_data_callback);
	m4gyr_remove_iiodev(iio);  /* dd is freed here */

m4gyr_remove_exit:
	return 0;
}

static struct of_device_id m4gyr_match_tbl[] = {
	{ .compatible = "mot,m4gyro" },
	{},
};

static struct platform_driver m4gyr_driver = {
	.probe		= m4gyr_probe,
	.remove		= __exit_p(m4gyr_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4GYR_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4gyr_match_tbl),
	},
};

static int __init m4gyr_init(void)
{
	return platform_driver_register(&m4gyr_driver);
}

static void __exit m4gyr_exit(void)
{
	platform_driver_unregister(&m4gyr_driver);
}

module_init(m4gyr_init);
module_exit(m4gyr_exit);

MODULE_ALIAS("platform:m4gyr");
MODULE_DESCRIPTION("M4 Sensor Hub Gyro client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
