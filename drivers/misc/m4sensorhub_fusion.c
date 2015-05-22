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
#include <linux/iio/m4sensorhub/m4sensorhub_fusion.h>

#define m4fus_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4FUS_IRQ_ENABLED_BIT       0

struct m4fus_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_fusion_iio_data   iiodat[M4FUS_NUM_FUSION_BUFFERS];
	int16_t         samplerate;
	uint16_t        status;
};


static void m4fus_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	int err = 0;
	struct iio_dev *iio = handle;
	struct m4fus_driver_data *dd = iio_priv(iio);
	int size = 0;

	mutex_lock(&(dd->mutex));

	/* Get Linear Accleration Data */
	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_FUSION_LINEARACCEL);
	if (size > (sizeof(int32_t) * ARRAY_SIZE(dd->iiodat[0].values))) {
		m4fus_err("%s: M4 register is too large.\n", __func__);
		err = -EOVERFLOW;
		goto m4fus_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_FUSION_LINEARACCEL,
		(char *)&(dd->iiodat[0].values[0]));
	if (err < 0) {
		m4fus_err("%s: Failed to read linear acceleration data.\n",
			__func__);
		goto m4fus_isr_fail;
	} else if (err != size) {
		m4fus_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "linear acceleration");
		err = -EBADE;
		goto m4fus_isr_fail;
	}

	dd->iiodat[0].type = FUSION_TYPE_LINEAR_ACCELERATION;
	dd->iiodat[0].timestamp = ktime_to_ns(ktime_get_boottime());

	/* Get Gravity Data */
	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_FUSION_GRAVITY);
	if (size > (sizeof(int32_t) * ARRAY_SIZE(dd->iiodat[1].values))) {
		m4fus_err("%s: M4 register is too large.\n", __func__);
		err = -EOVERFLOW;
		goto m4fus_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_FUSION_GRAVITY,
		(char *)&(dd->iiodat[1].values[0]));
	if (err < 0) {
		m4fus_err("%s: Failed to read gravity data.\n", __func__);
		goto m4fus_isr_fail;
	} else if (err != size) {
		m4fus_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "gravity");
		err = -EBADE;
		goto m4fus_isr_fail;
	}

	dd->iiodat[1].type = FUSION_TYPE_GRAVITY;
	dd->iiodat[1].timestamp = ktime_to_ns(ktime_get_boottime());

	/* Get Rotation Vector Data (includes accuracy) */
	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_FUSION_ROTATIONVECTOR);
	if (size > (sizeof(int32_t) * ARRAY_SIZE(dd->iiodat[1].values))) {
		m4fus_err("%s: M4 register is too large.\n", __func__);
		err = -EOVERFLOW;
		goto m4fus_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_FUSION_ROTATIONVECTOR,
		(char *)&(dd->iiodat[2].values[0]));
	if (err < 0) {
		m4fus_err("%s: Failed to read rotation data.\n", __func__);
		goto m4fus_isr_fail;
	} else if (err != size) {
		m4fus_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "rotation");
		err = -EBADE;
		goto m4fus_isr_fail;
	}

	dd->iiodat[2].type = FUSION_TYPE_ROTATION;
	dd->iiodat[2].timestamp = ktime_to_ns(ktime_get_boottime());

	/*
	 * The number of bytes send to the buffers is determined by this call:
	 *   iio->buffer->access->set_bytes_per_datum(iio->buffer,
	 *     sizeof(dd->iiodat));
	 * Since a static struct is used, the full array size is included,
	 *   so only one call to iio_push_to_buffers() is needed.
	 */
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat[0]));

m4fus_isr_fail:
	if (err < 0)
		m4fus_err("%s: Failed with error code %d.\n", __func__, err);

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4fus_set_samplerate(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4fus_driver_data *dd = iio_priv(iio);
	int size = 0;

	if (rate == dd->samplerate)
		goto m4fus_set_samplerate_irq_check;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_FUSION_SAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_FUSION_SAMPLERATE,
		(char *)&rate, m4sh_no_mask);
	if (err < 0) {
		m4fus_err("%s: Failed to set sample rate.\n", __func__);
		goto m4fus_set_samplerate_fail;
	} else if (err != size) {
		m4fus_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4fus_set_samplerate_fail;
	}
	dd->samplerate = rate;

m4fus_set_samplerate_irq_check:
	if (rate >= 0) {
		/* Enable the IRQ if necessary */
		if (!(dd->status & (1 << M4FUS_IRQ_ENABLED_BIT))) {
			err = m4sensorhub_irq_enable(dd->m4,
				M4SH_NOWAKEIRQ_FUSION);
			if (err < 0) {
				m4fus_err("%s: Failed to enable irq.\n",
					  __func__);
				goto m4fus_set_samplerate_fail;
			}
			dd->status = dd->status | (1 << M4FUS_IRQ_ENABLED_BIT);
		}
	} else {
		/* Disable the IRQ if necessary */
		if (dd->status & (1 << M4FUS_IRQ_ENABLED_BIT)) {
			err = m4sensorhub_irq_disable(dd->m4,
				M4SH_NOWAKEIRQ_FUSION);
			if (err < 0) {
				m4fus_err("%s: Failed to disable irq.\n",
					  __func__);
				goto m4fus_set_samplerate_fail;
			}
			dd->status = dd->status & ~(1 << M4FUS_IRQ_ENABLED_BIT);
		}
	}

m4fus_set_samplerate_fail:
	return err;
}

static ssize_t m4fus_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4fus_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4fus_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4fus_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4fus_err("%s: Failed to convert value.\n", __func__);
		goto m4fus_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4fus_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4fus_enable_store_exit;
	}

	err = m4fus_set_samplerate(iio, value);
	if (err < 0) {
		m4fus_err("%s: Failed to set sample rate.\n", __func__);
		goto m4fus_enable_store_exit;
	}

m4fus_enable_store_exit:
	if (err < 0) {
		m4fus_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4fus_setrate_show, m4fus_setrate_store, 0);

static ssize_t m4fus_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4fus_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE,
		"%s%d\n%s%d\n%s%d\n"
		"%s%d\n%s%d\n%s%d\n"
		"%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n",
		"linear_accel[x]: ", dd->iiodat[0].values[0],
		"linear_accel[y]: ", dd->iiodat[0].values[1],
		"linear_accel[z]: ", dd->iiodat[0].values[2],
		"gravity[x]: ", dd->iiodat[1].values[0],
		"gravity[y]: ", dd->iiodat[1].values[1],
		"gravity[z]: ", dd->iiodat[1].values[2],
		"rotation_vector[x]: ", dd->iiodat[2].values[0],
		"rotation_vector[y]: ", dd->iiodat[2].values[1],
		"rotation_vector[z]: ", dd->iiodat[2].values[2],
		"rotation_vector[scaler]: ", dd->iiodat[2].values[3],
		"rotation_vector[accuracy]: ", dd->iiodat[2].values[4]);
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4fus_iiodata_show, NULL, 0);

static struct attribute *m4fus_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4fus_iio_attr_group = {
	.attrs = m4fus_iio_attributes,
};

static const struct iio_info m4fus_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4fus_iio_attr_group,
};

static const struct iio_chan_spec m4fus_iio_channels[] = {
	{
		.type = IIO_FUSION,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4FUS_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4FUS_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4fus_remove_iiodev(struct iio_dev *iio)
{
	struct m4fus_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4fus_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4fus_driver_data *dd = iio_priv(iio);

	iio->name = M4FUS_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4fus_iio_info;
	iio->channels = m4fus_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4fus_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4fus_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4fus_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4fus_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4fus_err("%s: Failed to register IIO device.\n", __func__);
		goto m4fus_create_iiodev_iioreg_fail;
	}

	goto m4fus_create_iiodev_exit;

m4fus_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4fus_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4fus_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4fus_create_iiodev_exit:
	return err;
}

static void m4fus_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	int size, err;
	struct m4fus_driver_data *dd = (struct m4fus_driver_data *)data;

	if (dd == NULL) {
		m4fus_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_FUSION_SAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_FUSION_SAMPLERATE,
				   (char *)&dd->samplerate, m4sh_no_mask);
	if (err < 0) {
		m4fus_err("%s: Failed to set sample rate.\n", __func__);
	} else if (err != size) {
		m4fus_err("%s:  Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
	}

	mutex_unlock(&(dd->mutex));
}

static int m4fus_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4fus_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4fus_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4fus_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_NOWAKEIRQ_FUSION, m4fus_isr, iio, 0);
	if (err < 0) {
		m4fus_err("%s: Failed to register M4 IRQ.\n", __func__);
		goto m4fus_driver_init_fail;
	}

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_FUSION_RESTORE,
					 m4fus_panic_restore, dd);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "Fusion panic callback register failed\n");

	goto m4fus_driver_init_exit;

m4fus_driver_init_fail:
	m4fus_err("%s: Init failed with error code %d.\n", __func__, err);
m4fus_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4fus_probe(struct platform_device *pdev)
{
	struct m4fus_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4fus_driver_data));
	if (iio == NULL) {
		m4fus_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4fus_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */

	err = m4fus_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4fus_err("%s: Failed to create IIO device.\n", __func__);
		goto m4fus_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4fus_driver_init, iio);
	if (err < 0) {
		m4fus_err("%s: Failed to register initcall.\n", __func__);
		goto m4fus_probe_fail;
	}

	return 0;

m4fus_probe_fail:
	m4fus_remove_iiodev(iio); /* iio and dd are freed here */
m4fus_probe_fail_noiio:
	m4fus_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4fus_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4fus_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4fus_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4fus_remove_exit;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4FUS_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4, M4SH_NOWAKEIRQ_FUSION);
		dd->status = dd->status & ~(1 << M4FUS_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4, M4SH_NOWAKEIRQ_FUSION);
	m4sensorhub_unregister_initcall(m4fus_driver_init);
	m4fus_remove_iiodev(iio);  /* dd is freed here */

m4fus_remove_exit:
	return 0;
}

static struct of_device_id m4fusion_match_tbl[] = {
	{ .compatible = "mot,m4fusion" },
	{},
};

static struct platform_driver m4fus_driver = {
	.probe		= m4fus_probe,
	.remove		= __exit_p(m4fus_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4FUS_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4fusion_match_tbl),
	},
};

static int __init m4fus_init(void)
{
	return platform_driver_register(&m4fus_driver);
}

static void __exit m4fus_exit(void)
{
	platform_driver_unregister(&m4fus_driver);
}

module_init(m4fus_init);
module_exit(m4fus_exit);

MODULE_ALIAS("platform:m4fus");
MODULE_DESCRIPTION("M4 Sensor Hub Fusion client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
