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
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/m4sensorhub/m4sensorhub_heartrate.h>
#include <linux/delay.h>

#define m4hrt_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4HRT_IRQ_ENABLED_BIT       0

struct m4hrt_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_heartrate_iio_data   iiodat;
	struct delayed_work    m4hrt_work;
	int32_t         samplerate;
	int32_t         latest_samplerate;
	int32_t         fastest_rate;
	uint16_t        status;

	uint8_t         dbg_addr;
};

static void m4hrt_work_func(struct work_struct *work)
{
	int err = 0;
	struct m4hrt_driver_data *dd = container_of(work,
						    struct m4hrt_driver_data,
						    m4hrt_work.work);
	struct iio_dev *iio = platform_get_drvdata(dd->pdev);
	int size = 0;

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_HEARTRATE_HEARTRATE);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_HEARTRATE_HEARTRATE,
		(char *)&(dd->iiodat.heartrate));
	if (err < 0) {
		m4hrt_err("%s: Failed to read heartrate data.\n", __func__);
		goto m4hrt_isr_fail;
	} else if (err != size) {
		m4hrt_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "heartrate");
		err = -EBADE;
		goto m4hrt_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_HEARTRATE_CONFIDENCE);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_HEARTRATE_CONFIDENCE,
		(char *)&(dd->iiodat.confidence));
	if (err < 0) {
		m4hrt_err("%s: Failed to read confidence data.\n", __func__);
		goto m4hrt_isr_fail;
	} else if (err != size) {
		m4hrt_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "confidence");
		err = -EBADE;
		goto m4hrt_isr_fail;
	}

	dd->iiodat.timestamp = iio_get_time_ns();
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));
	if (dd->samplerate > 0)
		queue_delayed_work(system_freezable_wq, &(dd->m4hrt_work),
				      msecs_to_jiffies(dd->samplerate));

m4hrt_isr_fail:
	if (err < 0)
		m4hrt_err("%s: Failed with error code %d.\n", __func__, err);

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4hrt_set_samplerate(struct iio_dev *iio, int32_t rate)
{
	int err = 0;
	struct m4hrt_driver_data *dd = iio_priv(iio);
	int size = 0;

	if ((rate >= 0) && (rate <= dd->fastest_rate))
		rate = dd->fastest_rate;

	dd->latest_samplerate = rate;

	if (rate == dd->samplerate)
		goto m4hrt_set_samplerate_fail;

	size = m4sensorhub_reg_getsize(dd->m4,
		M4SH_REG_HEARTRATE_APSAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HEARTRATE_APSAMPLERATE,
		(char *)&rate, m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to set sample rate.\n", __func__);
		goto m4hrt_set_samplerate_fail;
	} else if (err != size) {
		m4hrt_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4hrt_set_samplerate_fail;
	}
	cancel_delayed_work(&(dd->m4hrt_work));
	dd->samplerate = rate;
	if (dd->samplerate > 0)
		queue_delayed_work(system_freezable_wq, &(dd->m4hrt_work),
				      msecs_to_jiffies(dd->samplerate));
m4hrt_set_samplerate_fail:
	return err;
}

static ssize_t m4hrt_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %d\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4hrt_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4hrt_err("%s: Failed to convert value.\n", __func__);
		goto m4hrt_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4hrt_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4hrt_enable_store_exit;
	}

	err = m4hrt_set_samplerate(iio, value);
	if (err < 0) {
		m4hrt_err("%s: Failed to set sample rate.\n", __func__);
		goto m4hrt_enable_store_exit;
	}

m4hrt_enable_store_exit:
	if (err < 0) {
		m4hrt_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4hrt_setrate_show, m4hrt_setrate_store, 0);

static ssize_t m4hrt_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "%s%hu\n%s%hhu\n",
		"heartrate: ", dd->iiodat.heartrate,
		"confidence: ", dd->iiodat.confidence);
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4hrt_iiodata_show, NULL, 0);

/* To set the desired address, "echo 0xHH > regaddr" (sets variable only). */
static ssize_t m4hrt_regaddr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "%s0x%02X\n",
		"addr: ", dd->dbg_addr);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4hrt_regaddr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	unsigned int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtouint(buf, 16, &value);
	if (err < 0) {
		m4hrt_err("%s: Failed to convert value.\n", __func__);
		goto m4hrt_regaddr_store_exit;
	}

	if (value > 255) {
		m4hrt_err("%s: Invalid register address %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4hrt_regaddr_store_exit;
	}

	dd->dbg_addr = value;

m4hrt_regaddr_store_exit:
	if (err < 0) {
		m4hrt_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(regaddr, S_IRUSR | S_IWUSR,
		m4hrt_regaddr_show, m4hrt_regaddr_store, 0);

/* To read data, "cat regdata" after setting regaddr above. */
static ssize_t m4hrt_regdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;
	uint8_t data[3] = {0x00, 0x00, 0x00};
	uint8_t cmd = 0x01; /* Read data */

	mutex_lock(&(dd->mutex));

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HRSENSOR_REGADDR,
		(char *)&(dd->dbg_addr), m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to write register address.\n", __func__);
		goto m4hrt_regdata_show_exit;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HRSENSOR_REGRWCMD,
		(char *)&cmd, m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to write register command.\n", __func__);
		goto m4hrt_regdata_show_exit;
	}

	msleep(1); /* Give M4 time to read the data */

	err = m4sensorhub_reg_read_n(dd->m4, M4SH_REG_HRSENSOR_REGVALUE,
		(char *)&(data[0]), ARRAY_SIZE(data));
	if (err < 0) {
		m4hrt_err("%s: Failed to read register data.\n", __func__);
		goto m4hrt_regdata_show_exit;
	}

	size = snprintf(buf, PAGE_SIZE, "%s\n0x%02X\n0x%02X\n0x%02X\n",
		"data (3 bytes): ", data[0], data[1], data[2]);

m4hrt_regdata_show_exit:
	if (err < 0) {
		m4hrt_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
/* To write data, "echo 0xHHHHHH > regdata" after setting regaddr above. */
static ssize_t m4hrt_regdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	unsigned int value = 0;
	uint8_t cmd = 0x00; /* Write data */

	mutex_lock(&(dd->mutex));

	err = kstrtouint(buf, 16, &value);
	if (err < 0) {
		m4hrt_err("%s: Failed to convert value.\n", __func__);
		goto m4hrt_regdata_store_exit;
	} else if (value > 0x00FFFFFF) { /* Keep only three bytes */
		value = value & 0x00FFFFFF;
		m4hrt_err("%s: Clipping data to write only 0x%06X...\n",
			  __func__, value);
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HRSENSOR_REGADDR,
		(char *)&(dd->dbg_addr), m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to write register address.\n", __func__);
		goto m4hrt_regdata_store_exit;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HRSENSOR_REGVALUE,
		(char *)&value, m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to write register data.\n", __func__);
		goto m4hrt_regdata_store_exit;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HRSENSOR_REGRWCMD,
		(char *)&cmd, m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to write register command.\n", __func__);
		goto m4hrt_regdata_store_exit;
	}

m4hrt_regdata_store_exit:
	if (err < 0) {
		m4hrt_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(regdata, S_IRUSR | S_IWUSR,
		m4hrt_regdata_show, m4hrt_regdata_store, 0);

static struct attribute *m4hrt_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	&iio_dev_attr_regaddr.dev_attr.attr,
	&iio_dev_attr_regdata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4hrt_iio_attr_group = {
	.attrs = m4hrt_iio_attributes,
};

static const struct iio_info m4hrt_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4hrt_iio_attr_group,
};

static const struct iio_chan_spec m4hrt_iio_channels[] = {
	{
		.type = IIO_HEARTRATE,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4HRT_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4HRT_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4hrt_remove_iiodev(struct iio_dev *iio)
{
	struct m4hrt_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4hrt_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4hrt_driver_data *dd = iio_priv(iio);

	iio->name = M4HRT_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4hrt_iio_info;
	iio->channels = m4hrt_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4hrt_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4hrt_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4hrt_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4hrt_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4hrt_err("%s: Failed to register IIO device.\n", __func__);
		goto m4hrt_create_iiodev_iioreg_fail;
	}

	goto m4hrt_create_iiodev_exit;

m4hrt_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4hrt_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4hrt_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4hrt_create_iiodev_exit:
	return err;
}

static void m4hrt_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	int size, err;
	struct m4hrt_driver_data *dd = (struct m4hrt_driver_data *)data;

	if (dd == NULL) {
		m4hrt_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4,
				      M4SH_REG_HEARTRATE_APSAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_HEARTRATE_APSAMPLERATE,
				   (char *)&dd->samplerate, m4sh_no_mask);
	if (err < 0) {
		m4hrt_err("%s: Failed to set sample rate.\n", __func__);
	} else if (err != size) {
		m4hrt_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
	}
	cancel_delayed_work(&(dd->m4hrt_work));
	if (dd->samplerate > 0)
		queue_delayed_work(system_freezable_wq, &(dd->m4hrt_work),
				      msecs_to_jiffies(dd->samplerate));
	mutex_unlock(&(dd->mutex));
}

static int m4hrt_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4hrt_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4hrt_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4hrt_driver_init_fail;
	}

	INIT_DELAYED_WORK(&(dd->m4hrt_work), m4hrt_work_func);

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_HEARTRATE_RESTORE,
					m4hrt_panic_restore, dd);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "HR panic callback register failed\n");

	goto m4hrt_driver_init_exit;

m4hrt_driver_init_fail:
	m4hrt_err("%s: Init failed with error code %d.\n", __func__, err);
m4hrt_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4hrt_probe(struct platform_device *pdev)
{
	struct m4hrt_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4hrt_driver_data));
	if (iio == NULL) {
		m4hrt_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4hrt_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */
	dd->latest_samplerate = dd->samplerate;
	dd->fastest_rate = 80; /* in milli secs */

	err = m4hrt_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4hrt_err("%s: Failed to create IIO device.\n", __func__);
		goto m4hrt_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4hrt_driver_init, iio);
	if (err < 0) {
		m4hrt_err("%s: Failed to register initcall.\n", __func__);
		goto m4hrt_probe_fail;
	}

	return 0;

m4hrt_probe_fail:
	m4hrt_remove_iiodev(iio); /* iio and dd are freed here */
m4hrt_probe_fail_noiio:
	m4hrt_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4hrt_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4hrt_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4hrt_remove_exit;

	mutex_lock(&(dd->mutex));
	cancel_delayed_work(&(dd->m4hrt_work));
	m4sensorhub_unregister_initcall(m4hrt_driver_init);
	m4hrt_remove_iiodev(iio);  /* dd is freed here */

m4hrt_remove_exit:
	return 0;
}

static int m4hrt_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4hrt_driver_data *dd = iio_priv(iio);
	if (m4hrt_set_samplerate(iio, dd->latest_samplerate) < 0)
		m4hrt_err("%s: setrate retry failed\n", __func__);
	return 0;
}

static struct of_device_id m4heartrate_match_tbl[] = {
	{ .compatible = "mot,m4heartrate" },
	{},
};

static struct platform_driver m4hrt_driver = {
	.probe		= m4hrt_probe,
	.remove		= __exit_p(m4hrt_remove),
	.shutdown	= NULL,
	.suspend	= m4hrt_suspend,
	.resume		= NULL,
	.driver		= {
		.name	= M4HRT_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4heartrate_match_tbl),
	},
};

static int __init m4hrt_init(void)
{
	return platform_driver_register(&m4hrt_driver);
}

static void __exit m4hrt_exit(void)
{
	platform_driver_unregister(&m4hrt_driver);
}

module_init(m4hrt_init);
module_exit(m4hrt_exit);

MODULE_ALIAS("platform:m4hrt");
MODULE_DESCRIPTION("M4 Sensor Hub Heartrate client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
