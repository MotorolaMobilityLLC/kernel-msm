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
#include <linux/iio/m4sensorhub/m4sensorhub_gesture.h>
#include <linux/m4sensorhub/MemMapGesture.h>
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
#include <linux/wakeup_source_notify.h>
#endif
#ifdef CONFIG_DISPLAY_STATE_NOTIFY
#include <linux/display_state_notify.h>
#endif

#define m4ges_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

/* This bit is used to track status if app has enabled the sensor*/
#define M4GES_IRQ_ENABLED_BIT       0

struct m4ges_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */

	struct m4sensorhub_gesture_iio_data   iiodat;
	int16_t         samplerate;
	uint32_t        gesture_count;
	uint16_t        status;
#ifdef CONFIG_DISPLAY_STATE_NOTIFY
	struct          notifier_block display_nb;
#endif
};


static void m4ges_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	int err = 0;
	struct iio_dev *iio = handle;
	struct m4ges_driver_data *dd = iio_priv(iio);
	int size = 0;

	mutex_lock(&(dd->mutex));

	dd->iiodat.type = GESTURE_TYPE_EVENT_DATA;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_GESTURE_GESTURE1);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_GESTURE_GESTURE1,
		(char *)&(dd->iiodat.event_data.gesture_type));
	if (err < 0) {
		m4ges_err("%s: Failed to read gesture_type data.\n", __func__);
		goto m4ges_isr_fail;
	} else if (err != size) {
		m4ges_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "gesture_type");
		err = -EBADE;
		goto m4ges_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_GESTURE_CONFIDENCE1);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_GESTURE_CONFIDENCE1,
		(char *)&(dd->iiodat.event_data.gesture_confidence));
	if (err < 0) {
		m4ges_err("%s: Failed to read gesture_confidence data.\n",
			  __func__);
		goto m4ges_isr_fail;
	} else if (err != size) {
		m4ges_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "gesture_confidence");
		err = -EBADE;
		goto m4ges_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_GESTURE_VALUE1);
	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_GESTURE_VALUE1,
		(char *)&(dd->iiodat.event_data.gesture_value));
	if (err < 0) {
		m4ges_err("%s: Failed to read gesture_value data.\n", __func__);
		goto m4ges_isr_fail;
	} else if (err != size) {
		m4ges_err("%s: Read %d bytes instead of %d for %s.\n",
			  __func__, err, size, "gesture_value");
		err = -EBADE;
		goto m4ges_isr_fail;
	}

#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	if (dd->iiodat.event_data.gesture_type == GESTURE_WRIST_ROTATE) {
		notify_display_wakeup(GESTURE);
	} else if (dd->iiodat.event_data.gesture_type == GESTURE_VIEW) {
		if (dd->iiodat.event_data.gesture_value == GESTURE_VIEW_ON)
			notify_display_wakeup(GESTURE_VIEWON);
		else
			notify_display_wakeup(GESTURE_VIEWOFF);
		/* the GESTURE_VIEW is only effect for kernel now
		 * do not send gesture to android
		 */
		goto m4ges_isr_fail;
	}
#endif /* CONFIG_WAKEUP_SOURCE_NOTIFY */

	dd->iiodat.timestamp = ktime_to_ns(ktime_get_boottime());
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));
	dd->gesture_count++;

m4ges_isr_fail:
	if (err < 0)
		m4ges_err("%s: Failed with error code %d.\n", __func__, err);

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4ges_set_samplerate_locked(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4ges_driver_data *dd = iio_priv(iio);

	/*
	 * Currently, there is no concept of setting a sample rate for this
	 * sensor, so this function only enables/disables interrupt reporting.
	 */
	if (rate == dd->samplerate)
		goto m4ges_set_samplerate_fail;

	if (rate >= 0) {
		/* Enable the IRQ if necessary */
		if (!(dd->status & (1 << M4GES_IRQ_ENABLED_BIT))) {
			err = m4sensorhub_irq_enable(dd->m4,
				M4SH_WAKEIRQ_GESTURE);
			if (err < 0) {
				m4ges_err("%s: Failed to enable irq.\n",
					  __func__);
				goto m4ges_set_samplerate_fail;
			}
			dd->status = dd->status | (1 << M4GES_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	} else {
		/* Disable the IRQ if necessary */
		if (dd->status & (1 << M4GES_IRQ_ENABLED_BIT)) {
			err = m4sensorhub_irq_disable(dd->m4,
				M4SH_WAKEIRQ_GESTURE);
			if (err < 0) {
				m4ges_err("%s: Failed to disable irq.\n",
					  __func__);
				goto m4ges_set_samplerate_fail;
			}
			dd->status = dd->status & ~(1 << M4GES_IRQ_ENABLED_BIT);
			dd->samplerate = rate;
		}
	}

m4ges_set_samplerate_fail:
	return err;
}

static int m4ges_send_flush_locked(struct iio_dev *iio, int16_t rate)
{
	int err = 0;
	struct m4ges_driver_data *dd = iio_priv(iio);

	dd->iiodat.type = GESTURE_TYPE_EVENT_FLUSH;
	dd->iiodat.timestamp = ktime_to_ns(ktime_get_boottime());
	iio_push_to_buffers(iio, (unsigned char *)&(dd->iiodat));

	return err;
}

static ssize_t m4ges_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ges_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4ges_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ges_driver_data *dd = iio_priv(iio);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4ges_err("%s: Failed to convert value.\n", __func__);
		goto m4ges_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4ges_err("%s: Invalid samplerate %d passed.\n",
			  __func__, value);
		err = -EINVAL;
		goto m4ges_enable_store_exit;
	}

	err = m4ges_set_samplerate_locked(iio, value);
	if (err < 0) {
		m4ges_err("%s: Failed to set sample rate.\n", __func__);
		goto m4ges_enable_store_exit;
	}

m4ges_enable_store_exit:
	if (err < 0) {
		m4ges_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4ges_setrate_show, m4ges_setrate_store, 0);

static ssize_t m4ges_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ges_driver_data *dd = iio_priv(iio);
	int value = 1;

	mutex_lock(&(dd->mutex));

	err = m4ges_send_flush_locked(iio, value);
	if (err < 0) {
		m4ges_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}

	mutex_unlock(&(dd->mutex));

	return size;
}
static IIO_DEVICE_ATTR(flush, S_IRUSR | S_IWUSR,
		NULL, m4ges_flush_store, 0);

static ssize_t m4ges_iiodata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ges_driver_data *dd = iio_priv(iio);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	if (dd->iiodat.type == GESTURE_TYPE_EVENT_DATA) {
		size = snprintf(buf, PAGE_SIZE,
			"%s%hhu\n%s%hhu\n%s%hhd\n%s%u\n",
			"gesture_type: ",
			dd->iiodat.event_data.gesture_type,
			"gesture_confidence: ",
			dd->iiodat.event_data.gesture_confidence,
			"gesture_value: ",
			dd->iiodat.event_data.gesture_value,
			"gesture_count: ",
			dd->gesture_count);
	} else if (dd->iiodat.type == GESTURE_TYPE_EVENT_FLUSH) {
		size = snprintf(buf, PAGE_SIZE,
			"Flush event\n");
	}
	mutex_unlock(&(dd->mutex));
	return size;
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO, m4ges_iiodata_show, NULL, 0);

static struct attribute *m4ges_iio_attributes[] = {
	&iio_dev_attr_setrate.dev_attr.attr,
	&iio_dev_attr_flush.dev_attr.attr,
	&iio_dev_attr_iiodata.dev_attr.attr,
	NULL,
};

static const struct attribute_group m4ges_iio_attr_group = {
	.attrs = m4ges_iio_attributes,
};

static const struct iio_info m4ges_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4ges_iio_attr_group,
};

static const struct iio_chan_spec m4ges_iio_channels[] = {
	{
		.type = IIO_GESTURE,
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = M4GES_DATA_STRUCT_SIZE_BITS,
			.storagebits = M4GES_DATA_STRUCT_SIZE_BITS,
			.shift = 0,
		},
	},
};

static void m4ges_remove_iiodev(struct iio_dev *iio)
{
	struct m4ges_driver_data *dd = iio_priv(iio);

	/* Remember, only call when dd->mutex is locked */
	iio_kfifo_free(iio->buffer);
	iio_buffer_unregister(iio);
	iio_device_unregister(iio);
	mutex_destroy(&(dd->mutex));
	iio_device_free(iio); /* dd is freed here */
	return;
}

static int m4ges_create_iiodev(struct iio_dev *iio)
{
	int err = 0;
	struct m4ges_driver_data *dd = iio_priv(iio);

	iio->name = M4GES_DRIVER_NAME;
	iio->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio->num_channels = 1;
	iio->info = &m4ges_iio_info;
	iio->channels = m4ges_iio_channels;

	iio->buffer = iio_kfifo_allocate(iio);
	if (iio->buffer == NULL) {
		m4ges_err("%s: Failed to allocate IIO buffer.\n", __func__);
		err = -ENOMEM;
		goto m4ges_create_iiodev_kfifo_fail;
	}

	iio->buffer->scan_timestamp = true;
	iio->buffer->access->set_bytes_per_datum(iio->buffer,
		sizeof(dd->iiodat));
	err = iio_buffer_register(iio, iio->channels, iio->num_channels);
	if (err < 0) {
		m4ges_err("%s: Failed to register IIO buffer.\n", __func__);
		goto m4ges_create_iiodev_buffer_fail;
	}

	err = iio_device_register(iio);
	if (err < 0) {
		m4ges_err("%s: Failed to register IIO device.\n", __func__);
		goto m4ges_create_iiodev_iioreg_fail;
	}

	goto m4ges_create_iiodev_exit;

m4ges_create_iiodev_iioreg_fail:
	iio_buffer_unregister(iio);
m4ges_create_iiodev_buffer_fail:
	iio_kfifo_free(iio->buffer);
m4ges_create_iiodev_kfifo_fail:
	iio_device_free(iio); /* dd is freed here */
m4ges_create_iiodev_exit:
	return err;
}

#ifdef CONFIG_DISPLAY_STATE_NOTIFY
static int display_notify(struct notifier_block *self,
			unsigned long action, void *dev)
{
	struct m4ges_driver_data *dd =
		container_of(self, struct m4ges_driver_data, display_nb);

	mutex_lock(&(dd->mutex));
	/* check if gesture is enabled by app */
	if ((dd->status & (1 << M4GES_IRQ_ENABLED_BIT))) {
		/* enable interrupt only when display is not fully on */
		int state = DISP_EVENT_STATE(action);
		int enable = (state != DISPLAY_STATE_ON);
		if (enable)
			m4sensorhub_irq_enable(dd->m4, M4SH_WAKEIRQ_GESTURE);
		else
			m4sensorhub_irq_disable(dd->m4, M4SH_WAKEIRQ_GESTURE);
		pr_info("%s(%d): gesture irq is %s\n", __func__, state,
			enable ? "enabled" : "disabled");
	}
	mutex_unlock(&(dd->mutex));

	return NOTIFY_OK;
}
#endif

static int m4ges_driver_init(struct init_calldata *p_arg)
{
	struct iio_dev *iio = p_arg->p_data;
	struct m4ges_driver_data *dd = iio_priv(iio);
	int err = 0;

	mutex_lock(&(dd->mutex));

	dd->m4 = p_arg->p_m4sensorhub_data;
	if (dd->m4 == NULL) {
		m4ges_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4ges_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_WAKEIRQ_GESTURE, m4ges_isr, iio, 1);
	if (err < 0) {
		m4ges_err("%s: Failed to register M4 IRQ.\n", __func__);
		goto m4ges_driver_init_fail;
	}

#ifdef CONFIG_DISPLAY_STATE_NOTIFY
	dd->display_nb.notifier_call = display_notify;
	display_state_register_notify(&dd->display_nb);
#endif

	goto m4ges_driver_init_exit;

m4ges_driver_init_fail:
	m4ges_err("%s: Init failed with error code %d.\n", __func__, err);
m4ges_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4ges_probe(struct platform_device *pdev)
{
	struct m4ges_driver_data *dd = NULL;
	struct iio_dev *iio = NULL;
	int err = 0;

	iio = iio_device_alloc(sizeof(struct m4ges_driver_data));
	if (iio == NULL) {
		m4ges_err("%s: Failed to allocate IIO data.\n", __func__);
		err = -ENOMEM;
		goto m4ges_probe_fail_noiio;
	}

	dd = iio_priv(iio);
	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, iio);
	dd->samplerate = -1; /* We always start disabled */

	err = m4ges_create_iiodev(iio); /* iio and dd are freed on fail */
	if (err < 0) {
		m4ges_err("%s: Failed to create IIO device.\n", __func__);
		goto m4ges_probe_fail_noiio;
	}

	err = m4sensorhub_register_initcall(m4ges_driver_init, iio);
	if (err < 0) {
		m4ges_err("%s: Failed to register initcall.\n", __func__);
		goto m4ges_probe_fail;
	}

	return 0;

m4ges_probe_fail:
	m4ges_remove_iiodev(iio); /* iio and dd are freed here */
m4ges_probe_fail_noiio:
	m4ges_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4ges_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct m4ges_driver_data *dd = NULL;

	if (iio == NULL)
		goto m4ges_remove_exit;

	dd = iio_priv(iio);
	if (dd == NULL)
		goto m4ges_remove_exit;

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4GES_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4, M4SH_WAKEIRQ_GESTURE);
		dd->status = dd->status & ~(1 << M4GES_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4, M4SH_WAKEIRQ_GESTURE);
	m4sensorhub_unregister_initcall(m4ges_driver_init);
#ifdef CONFIG_DISPLAY_STATE_NOTIFY
	display_state_unregister_notify(&dd->display_nb);
#endif
	m4ges_remove_iiodev(iio);  /* dd is freed here */

m4ges_remove_exit:
	return 0;
}

static struct of_device_id m4gesture_match_tbl[] = {
	{ .compatible = "mot,m4gesture" },
	{},
};

static struct platform_driver m4ges_driver = {
	.probe		= m4ges_probe,
	.remove		= __exit_p(m4ges_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4GES_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4gesture_match_tbl),
	},
};

static int __init m4ges_init(void)
{
	return platform_driver_register(&m4ges_driver);
}

static void __exit m4ges_exit(void)
{
	platform_driver_unregister(&m4ges_driver);
}

module_init(m4ges_init);
module_exit(m4ges_exit);

MODULE_ALIAS("platform:m4ges");
MODULE_DESCRIPTION("M4 Sensor Hub Gesture client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
