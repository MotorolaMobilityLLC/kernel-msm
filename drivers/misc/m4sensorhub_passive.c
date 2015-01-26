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
#include <linux/iio/m4sensorhub/m4sensorhub_passive.h>

#define m4pas_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4PAS_IRQ_ENABLED_BIT       0

struct m4pas_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_passive_iio_data   iiodat[M4PAS_NUM_PASSIVE_BUFFERS];
	int16_t         samplerate;
	int16_t         latest_samplerate;
	uint16_t        status;
};


static void m4pas_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	int err = 0;
	struct iio_dev *iio = handle;
	struct m4pas_driver_data *dd = iio_priv(iio);
	int size = 0;
	uint32_t passive_timestamp[M4PAS_NUM_PASSIVE_BUFFERS];
	uint16_t steps[M4PAS_NUM_PASSIVE_BUFFERS];
	uint16_t calories[M4PAS_NUM_PASSIVE_BUFFERS];
	uint16_t heartrate[M4PAS_NUM_PASSIVE_BUFFERS];
	uint8_t hrconfidence[M4PAS_NUM_PASSIVE_BUFFERS];
	uint8_t healthy_minutes[M4PAS_NUM_PASSIVE_BUFFERS];
	int i = 0;

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_TIMESTAMP);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_TIMESTAMP,
		(char *)&(passive_timestamp));
	if (err < 0) {
		m4pas_err("%s: Failed to read passive_timestamp data.\n",
			  __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "passive_timestamp");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_STEPS);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_STEPS,
		(char *)&(steps));
	if (err < 0) {
		m4pas_err("%s: Failed to read steps data.\n", __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "steps");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_CALORIES);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_CALORIES,
		(char *)&(calories));
	if (err < 0) {
		m4pas_err("%s: Failed to read calories data.\n", __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "calories");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_HEARTRATE);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_HEARTRATE,
		(char *)&(heartrate));
	if (err < 0) {
		m4pas_err("%s: Failed to read heartrate data.\n",
			  __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "heartrate");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_HRCONFIDENCE);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_HRCONFIDENCE,
		(char *)&(hrconfidence));
	if (err < 0) {
		m4pas_err("%s: Failed to read hrconfidence data.\n",
			  __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "hrconfidence");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_PASSIVE_HEALTHYMINUTES);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_PASSIVE_HEALTHYMINUTES,
		(char *)&(healthy_minutes));
	if (err < 0) {
		m4pas_err("%s: Failed to read healthy_minutes data.\n",
			  __func__);
		goto m4pas_isr_fail;
	} else if (err != size) {
		m4pas_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "healthy_minutes");
		err = -EBADE;
		goto m4pas_isr_fail;
	}

	for (i = 0; i < M4PAS_NUM_PASSIVE_BUFFERS; i++) {
		dd->iiodat[i].passive_timestamp = passive_timestamp[i];
		dd->iiodat[i].steps = steps[i];
		dd->iiodat[i].calories = calories[i];
		dd->iiodat[i].heartrate = heartrate[i];
		dd->iiodat[i].hrconfidence = hrconfidence[i];
		dd->iiodat[i].healthy_minutes = healthy_minutes[i];
		dd->iiodat[i].timestamp = iio_get_time_ns();
	}

	/*
	 * For some reason, IIO knows we are sending an array,
	 * so all M4PAS_NUM_PASSIVE_BUFFERS indicies will be sent
	 * in this one call (it does not need to go in the for-loop).
	 */
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat[0]));

m4pas_isr_fail:
	if (err < 0)
		m4pas_err("%s: Failed with error code %d.\n", __func__, err);

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4pas_set_samplerate(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4pas_driver_data *dd = iio_priv(iio);

	/*
	 * Currently, there is no concept of setting a sample rate for this
	 * sensor, so this function only enables/disables interrupt reporting.
	 */
	dd->latest_samplerate = rate;
	if (dd->samplerate == rate)
		goto m4pas_set_samplerate_fail;

	if (rate >= 0) {
		/* Enable passive mode feature */
		err = m4sensorhub_reg_write_1byte(dd->m4,
						  M4SH_REG_PASSIVE_ENABLE,
						  0x01, 0xFF);
		if (err != 1) {
			m4pas_err("%s: Failed to enable with %d.\n",
				  __func__, err);
			goto m4pas_set_samplerate_fail;
		}
		/* Enable the IRQ if necessary */
		if (!(dd->status & (1 << M4PAS_IRQ_ENABLED_BIT))) {
			err = m4sensorhub_irq_enable(dd->m4,
				M4SH_IRQ_PASSIVE_BUFFER_FULL);
			if (err < 0) {
				m4pas_err("%s: Failed to enable irq.\n",
					  __func__);
				goto m4pas_set_samplerate_fail;
			}
			dd->status = dd->status | (1 << M4PAS_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	} else {
		/* Disable passive mode feature */
		err = m4sensorhub_reg_write_1byte(dd->m4,
						  M4SH_REG_PASSIVE_ENABLE,
						  0x0, 0xFF);
		if (err != 1) {
			m4pas_err("%s: Failed to disable with %d.\n",
				  __func__, err);
			goto m4pas_set_samplerate_fail;
		}
		/* Disable the IRQ if necessary */
		if (dd->status & (1 << M4PAS_IRQ_ENABLED_BIT)) {
			err = m4sensorhub_irq_disable(dd->m4,
				M4SH_IRQ_PASSIVE_BUFFER_FULL);
			if (err < 0) {
				m4pas_err("%s: Failed to disable irq.\n",
					  __func__);
				goto m4pas_set_samplerate_fail;
			}
			dd->status = dd->status & ~(1 << M4PAS_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	}

m4pas_set_samplerate_fail:
	return err;
}

static ssize_t m4pas_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4pas_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4pas_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4pas_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4pas_err("%s: Failed to convert value.\n", __func__);
		goto m4pas_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4pas_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4pas_enable_store_exit;
	}

	err = m4pas_set_samplerate(iio, value);
	if (err < 0) {
		m4pas_err("%s: Failed to set sample rate.\n", __func__);
		goto m4pas_enable_store_exit;
	}

m4pas_enable_store_exit:
	if (err < 0) {
		m4pas_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4pas_setrate_show, m4pas_setrate_store, 0);

static ssize_t m4pas_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4pas_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;
	int i = 0;

	mutex_lock(&(dd->mutex));
	buf[0] = '\0';  /* Start with NULL terminator for concatenation */;
	for (i = 0; i < M4PAS_NUM_PASSIVE_BUFFERS; i++) {
		size = snprintf(buf, PAGE_SIZE,
			"%s%s%d\n%s%u\n%s%hu\n%s%hu\n%s%hu\n%s%hhu\n%s%hhu\n",
			buf, "Buffer ", i,
			"passive_timestamp: ", dd->iiodat[i].passive_timestamp,
			"steps: ", dd->iiodat[i].steps,
			"calories: ", dd->iiodat[i].calories,
			"heartrate: ", dd->iiodat[i].heartrate,
			"hrconfidence: ", dd->iiodat[i].hrconfidence,
			"healthy_minutes: ", dd->iiodat[i].healthy_minutes);
	}
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4pas_iiodata_show, NULL, 0);

static struct attribute *m4pas_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4pas_iio_attr_group = {
	.attrs = m4pas_iio_attributes,
};

static const struct iio_info m4pas_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4pas_iio_attr_group,
};

static const struct iio_chan_spec m4pas_iio_channels[] = {
	{
		.type = IIO_PASSIVE,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4PAS_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4PAS_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4pas_remove_iiodev(struct iio_dev *iio)
{
	struct m4pas_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4pas_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4pas_driver_data *dd = iio_priv(iio);

	iio->name = M4PAS_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4pas_iio_info;
	iio->channels = m4pas_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4pas_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4pas_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4pas_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4pas_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4pas_err("%s: Failed to register IIO device.\n", __func__);
		goto m4pas_create_iiodev_iioreg_fail;
	}

	goto m4pas_create_iiodev_exit;

m4pas_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4pas_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4pas_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4pas_create_iiodev_exit:
	return err;
}

static void m4pas_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	int err;
	char en;
	struct m4pas_driver_data *dd = (struct m4pas_driver_data *)data;

	if (dd == NULL) {
		m4pas_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	en = (dd->samplerate >= 0) ? 1 : 0;
	err = m4sensorhub_reg_write_1byte(dd->m4, M4SH_REG_PASSIVE_ENABLE,
					  en, 0xFF);
	if (err != 1)
		m4pas_err("%s: Failed to enable with %d.\n",
			  __func__, err);

	mutex_unlock(&(dd->mutex));
}

static int m4pas_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4pas_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4pas_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4pas_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_IRQ_PASSIVE_BUFFER_FULL, m4pas_isr, iio, 1);
	if (err < 0) {
		m4pas_err("%s: Failed to register M4 IRQ.\n", __func__);
		goto m4pas_driver_init_fail;
	}

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_PASSIVE_RESTORE,
					 m4pas_panic_restore, dd);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "Passive panic callback register failed\n");
	goto m4pas_driver_init_exit;

m4pas_driver_init_fail:
	m4pas_err("%s: Init failed with error code %d.\n", __func__, err);
m4pas_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4pas_probe(struct platform_device *pdev)
{
	struct m4pas_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4pas_driver_data));
	if (iio == NULL) {
		m4pas_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4pas_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */
	dd->latest_samplerate = dd->samplerate;

	err = m4pas_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4pas_err("%s: Failed to create IIO device.\n", __func__);
		goto m4pas_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4pas_driver_init, iio);
	if (err < 0) {
		m4pas_err("%s: Failed to register initcall.\n", __func__);
		goto m4pas_probe_fail;
	}

	return 0;

m4pas_probe_fail:
	m4pas_remove_iiodev(iio); /* iio and dd are freed here */
m4pas_probe_fail_noiio:
	m4pas_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4pas_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4pas_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4pas_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4pas_remove_exit;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4PAS_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4,
					M4SH_IRQ_PASSIVE_BUFFER_FULL);
		dd->status = dd->status & ~(1 << M4PAS_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4,
				   M4SH_IRQ_PASSIVE_BUFFER_FULL);
	m4sensorhub_unregister_initcall(m4pas_driver_init);
	m4pas_remove_iiodev(iio);  /* dd is freed here */

m4pas_remove_exit:
	return 0;
}

static int m4pas_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4pas_driver_data *dd = iio_priv(iio);
	mutex_lock(&(dd->mutex));
	if (m4pas_set_samplerate(iio, dd->latest_samplerate) < 0)
		m4pas_err("%s: setrate retry failed\n", __func__);
	mutex_unlock(&(dd->mutex));
	return 0;
}

static struct of_device_id m4passive_match_tbl[] = {
	{ .compatible = "mot,m4passive" },
	{},
};

static struct platform_driver m4pas_driver = {
	.probe		= m4pas_probe,
	.remove		= __exit_p(m4pas_remove),
	.shutdown	= NULL,
	.suspend	= m4pas_suspend,
	.resume		= NULL,
	.driver		= {
		.name	= M4PAS_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4passive_match_tbl),
	},
};

static int __init m4pas_init(void)
{
	return platform_driver_register(&m4pas_driver);
}

static void __exit m4pas_exit(void)
{
	platform_driver_unregister(&m4pas_driver);
}

module_init(m4pas_init);
module_exit(m4pas_exit);

MODULE_ALIAS("platform:m4pas");
MODULE_DESCRIPTION("M4 Sensor Hub Passive client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
