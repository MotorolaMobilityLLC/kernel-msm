/*
 *  Copyright (C) 2012-2015 Motorola, Inc.
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
#include <linux/iio/m4sensorhub/m4sensorhub_significant_motion.h>

#define m4sgm_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4SGM_IRQ_ENABLED_BIT       0

struct m4sgm_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_significant_motion_iio_data   iiodat;
	int16_t         samplerate;
	uint32_t        motion_count;
	uint16_t        status;
};

static void m4sgm_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	struct iio_dev *iio = handle;
	struct m4sgm_driver_data *dd = iio_priv(iio);

	mutex_lock(&(dd->mutex));

	dd->iiodat.motion_detected = 1;
	dd->iiodat.timestamp = ktime_to_ns(ktime_get_boottime());
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));
	dd->motion_count++;

	pr_info("%s: Significant motion detected: count=%u.\n",
		__func__, dd->motion_count);

	/* This is a one-shot sensor */
	dd->samplerate = -1;

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4sgm_set_samplerate(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4sgm_driver_data *dd = iio_priv(iio);
	int size = 0;
	uint8_t enable;

	/*
	 * This is a one-shot sensor, so only the feature enable
	 * is actually set/cleared here.
	 */
	if (rate == dd->samplerate)
		goto m4sgm_set_samplerate_fail;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_METS_SIGMO_ENABLE);
	if (size < 0) {
		m4sgm_err("%s: Writing to invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4sgm_set_samplerate_fail;
	}

	if (rate >= 0)
		enable = 0x01;
	else
		enable = 0x00;

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_METS_SIGMO_ENABLE,
		(char *)&enable, m4sh_no_mask);
	if (err < 0) {
		m4sgm_err("%s: Failed to set sample rate.\n", __func__);
		goto m4sgm_set_samplerate_fail;
	} else if (err != size) {
		m4sgm_err("%s: Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4sgm_set_samplerate_fail;
	}
	dd->samplerate = rate;

m4sgm_set_samplerate_fail:
	return err;
}

static ssize_t m4sgm_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4sgm_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4sgm_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4sgm_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4sgm_err("%s: Failed to convert value.\n", __func__);
		goto m4sgm_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4sgm_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4sgm_enable_store_exit;
	}

	err = m4sgm_set_samplerate(iio, value);
	if (err < 0) {
		m4sgm_err("%s: Failed to set sample rate.\n", __func__);
		goto m4sgm_enable_store_exit;
	}

m4sgm_enable_store_exit:
	if (err < 0) {
		m4sgm_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4sgm_setrate_show, m4sgm_setrate_store, 0);

static ssize_t m4sgm_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4sgm_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE,
		"%s%hhu\n%s%u\n",
		"motion_detected: ", dd->iiodat.motion_detected,
		"motion_count: ", dd->motion_count);
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4sgm_iiodata_show, NULL, 0);

static struct attribute *m4sgm_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4sgm_iio_attr_group = {
	.attrs = m4sgm_iio_attributes,
};

static const struct iio_info m4sgm_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4sgm_iio_attr_group,
};

static const struct iio_chan_spec m4sgm_iio_channels[] = {
	{
		.type = IIO_GESTURE,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4SGM_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4SGM_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4sgm_remove_iiodev(struct iio_dev *iio)
{
	struct m4sgm_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4sgm_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4sgm_driver_data *dd = iio_priv(iio);

	iio->name = M4SGM_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4sgm_iio_info;
	iio->channels = m4sgm_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4sgm_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4sgm_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4sgm_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4sgm_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4sgm_err("%s: Failed to register IIO device.\n", __func__);
		goto m4sgm_create_iiodev_iioreg_fail;
	}

	goto m4sgm_create_iiodev_exit;

m4sgm_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4sgm_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4sgm_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4sgm_create_iiodev_exit:
	return err;
}

static int m4sgm_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4sgm_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4sgm_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4sgm_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_WAKEIRQ_SIGNIFICANT_MOTION, m4sgm_isr, iio, 1);
	if (err < 0) {
		m4sgm_err("%s: Failed to register M4 IRQ.\n", __func__);
		goto m4sgm_driver_init_fail;
	}

	/* Since this is a one-shot sensor, enable once for interrupts */
	err = m4sensorhub_irq_enable(dd->m4, M4SH_WAKEIRQ_SIGNIFICANT_MOTION);
	if (err < 0) {
		m4sgm_err("%s: Failed to enable irq.\n", __func__);
		goto m4sgm_driver_init_fail;
	}
	dd->status = dd->status | (1 << M4SGM_IRQ_ENABLED_BIT);

	goto m4sgm_driver_init_exit;

m4sgm_driver_init_fail:
	m4sgm_err("%s: Init failed with error code %d.\n", __func__, err);
m4sgm_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4sgm_probe(struct platform_device *pdev)
{
	struct m4sgm_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4sgm_driver_data));
	if (iio == NULL) {
		m4sgm_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4sgm_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */

	err = m4sgm_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4sgm_err("%s: Failed to create IIO device.\n", __func__);
		goto m4sgm_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4sgm_driver_init, iio);
	if (err < 0) {
		m4sgm_err("%s: Failed to register initcall.\n", __func__);
		goto m4sgm_probe_fail;
	}

	return 0;

m4sgm_probe_fail:
	m4sgm_remove_iiodev(iio); /* iio and dd are freed here */
m4sgm_probe_fail_noiio:
	m4sgm_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4sgm_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4sgm_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4sgm_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4sgm_remove_exit;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4SGM_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4,
					M4SH_WAKEIRQ_SIGNIFICANT_MOTION);
		dd->status = dd->status & ~(1 << M4SGM_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4,
				   M4SH_WAKEIRQ_SIGNIFICANT_MOTION);
	m4sensorhub_unregister_initcall(m4sgm_driver_init);
	m4sgm_remove_iiodev(iio);  /* dd is freed here */

m4sgm_remove_exit:
	return 0;
}

static struct of_device_id m4gesture_match_tbl[] = {
	{ .compatible = "mot,m4significantmotion" },
	{},
};

static struct platform_driver m4sgm_driver = {
	.probe		= m4sgm_probe,
	.remove		= __exit_p(m4sgm_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4SGM_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4gesture_match_tbl),
	},
};

static int __init m4sgm_init(void)
{
	return platform_driver_register(&m4sgm_driver);
}

static void __exit m4sgm_exit(void)
{
	platform_driver_unregister(&m4sgm_driver);
}

module_init(m4sgm_init);
module_exit(m4sgm_exit);

MODULE_ALIAS("platform:m4ges");
MODULE_DESCRIPTION("M4 Sensor Hub Significant Motion client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
