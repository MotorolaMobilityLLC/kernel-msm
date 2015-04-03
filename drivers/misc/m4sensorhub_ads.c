/*
 *  Copyright (C) 2012 Motorola, Inc.
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
#include <linux/input.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub/m4sensorhub_registers.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/m4sensorhub/m4sensorhub_ads.h>

#define U32_MAX ((u32)(~0U))
#define m4sensorhub_ads_DRIVER_NAME	"m4sensorhub_ads"
/* this needs to be in sync with M4 code*/
#define ADS_NUM_SAMPLES 1000
#define BYTES_TO_READ (ADS_NUM_SAMPLES * sizeof(int))

struct m4sensorhub_ads_drvdata {
	struct m4sensorhub_data *m4sensorhub;
	int samplerate;
	bool enable;
	struct platform_device      *pdev;
	struct m4sensorhub_ads_data read_data;
	struct mutex				mutex;
	int* data;
	uint32_t data_seq_num;
	uint8_t	mode;
};

#define DATA_SIZE_IN_BITS  (sizeof(struct m4sensorhub_ads_data) * 8)

static const struct iio_chan_spec m4sensorhub_ads_channels[] = {
	{
		.type = IIO_ADS,
		/* Channel has a numeric index of 0 */
		.indexed = 1,
		.channel = 0,

		.scan_index = 0,
		.scan_type = { /* Description of storage in buffer */
			.sign = 'u', /* unsigned */
			.realbits = DATA_SIZE_IN_BITS,
			.storagebits = DATA_SIZE_IN_BITS,
			.shift = 0, /* zero shift */
		},
	},
};


static void m4_read_ads_data_locked(struct m4sensorhub_ads_drvdata *priv_data)
{
	int ret, i, dataremaining;
	struct iio_dev *iio_dev = platform_get_drvdata(priv_data->pdev);

	if (priv_data->data == NULL) {
		pr_err("%s:OOM\n", __func__);
		return;
	}

	dataremaining = sizeof(int)*ADS_NUM_SAMPLES;

	ret = m4sensorhub_reg_read_buffer(priv_data->m4sensorhub,
			M4SH_REG_ADSSENSOR_READBUFFEREDDATA,
			(u8*)priv_data->data, BYTES_TO_READ);

	if (ret < 0 || ret != BYTES_TO_READ) {
		pr_err("%s: unable to read data (%d)\n", __func__, ret);
		return;
	}

	for (i = 0; i < ADS_NUM_SAMPLES; i = i + 2) {
		priv_data->read_data.seq = (priv_data->data_seq_num)++;
		if (priv_data->data_seq_num == U32_MAX)
			priv_data->data_seq_num = 0;

		priv_data->read_data.ch1_data = priv_data->data[i];
		priv_data->read_data.ch2_data = priv_data->data[i+1];

		priv_data->read_data.timestamp = iio_get_time_ns();
		ret = iio_push_to_buffers(iio_dev, (unsigned char *)&(priv_data->read_data));
		if (ret < 0)
			pr_err("%s: failed to buffer sample data %d\n", __func__, priv_data->data[i]);
	}

}

static void m4sensorhub_ads_panic_restore(struct m4sensorhub_data *m4sensorhub,
                void *data)
{
	int ret;
	struct m4sensorhub_ads_drvdata *priv_data =
			(struct m4sensorhub_ads_drvdata *)data;

	mutex_lock(&(priv_data->mutex));

	if (priv_data->samplerate < 0)
		goto err;

	ret = m4sensorhub_reg_write(priv_data->m4sensorhub,
			M4SH_REG_ADSSENSOR_SAMPLERATE,
			(char *)&(priv_data->samplerate), m4sh_no_mask);

	if (ret != m4sensorhub_reg_getsize(
			priv_data->m4sensorhub,
			M4SH_REG_ADSSENSOR_SAMPLERATE)) {
		pr_err("%s:Unable to set delay\n", __func__);
		goto err;
	}

	if (priv_data->samplerate > 0)
		m4sensorhub_irq_enable(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
	else
		m4sensorhub_irq_disable(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
err:
	mutex_unlock(&(priv_data->mutex));
}

static void m4_handle_ads_irq(enum m4sensorhub_irqs int_event,
							void *data)
{
	struct iio_dev *iio_dev = (struct iio_dev *)data;
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	mutex_lock(&(priv_data->mutex));
	m4_read_ads_data_locked(priv_data);
	mutex_unlock(&(priv_data->mutex));
}

static int m4sensorhub_ads_driver_initcallback(struct init_calldata *arg)
{
	struct iio_dev *iio_dev = (struct iio_dev *)(arg->p_data);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	int ret;

	priv_data->m4sensorhub = arg->p_m4sensorhub_data;
	ret = m4sensorhub_panic_register(priv_data->m4sensorhub, PANICHDL_ADS_RESTORE,
				m4sensorhub_ads_panic_restore, priv_data);
	if (ret < 0)
		pr_err("%s: failed panic register(%d)\n", __func__, ret);

	ret = m4sensorhub_irq_register(priv_data->m4sensorhub,
			M4SH_NOWAKEIRQ_ADS, m4_handle_ads_irq,
			iio_dev, 1);

	if (ret < 0)
		pr_err("%s: failed irq register(%d)\n", __func__, ret);

	return 0;
}

/* setdelay */
static ssize_t m4sensorhub_ads_store_setdelay(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	int ret;
	int samplerate;

	ret = kstrtoint(buf, 10, &samplerate);
	if (ret < 0)
		return ret;

	if (samplerate < -1) {
		pr_err("%s: non -1 negative sample rate, rejecting\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&(priv_data->mutex));
	if (samplerate != priv_data->samplerate) {

		ret = m4sensorhub_reg_write(priv_data->m4sensorhub,
				M4SH_REG_ADSSENSOR_SAMPLERATE,
				(char *)&samplerate, m4sh_no_mask);

		if (ret != m4sensorhub_reg_getsize(
				priv_data->m4sensorhub,
				M4SH_REG_ADSSENSOR_SAMPLERATE)) {
			pr_err("%s:Unable to set delay\n", __func__);
			goto err;
		}

		priv_data->samplerate = samplerate;

		if (priv_data->samplerate > 0) {
			/* don't allocate a buffer if there's one already
			in use from a previous call to setdelay*/
			if (priv_data->data == NULL) {
				priv_data->data = kzalloc(BYTES_TO_READ, GFP_KERNEL);

				if (priv_data->data == NULL) {
					pr_err("%s:Failed to allocate memory\n", __func__);
					ret = -ENOMEM;
					goto err;
				}
			}
			m4sensorhub_irq_enable(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
		}
		else {
			kfree(priv_data->data);
			priv_data->data = NULL;
			priv_data->data_seq_num = 0;
			priv_data->mode = MODE_MAX;
			m4sensorhub_irq_disable(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
		}
	}
	ret = count;
err:
	mutex_unlock(&(priv_data->mutex));
	return ret;
}

static ssize_t m4sensorhub_ads_show_setdelay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	int count;
	mutex_lock(&(priv_data->mutex));
	count = snprintf(buf, PAGE_SIZE, "%d\n", priv_data->samplerate);
	mutex_unlock(&(priv_data->mutex));
	return count;
}

static IIO_DEVICE_ATTR(setdelay, S_IRUGO | S_IWUSR,
					m4sensorhub_ads_show_setdelay,
					m4sensorhub_ads_store_setdelay, 0);

static ssize_t m4sensorhub_ads_show_iiodata(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	return snprintf(buf, PAGE_SIZE, "ads::ch1 %d, ch2 %d\n",
					priv_data->read_data.ch1_data,
					priv_data->read_data.ch2_data);
}
static IIO_DEVICE_ATTR(iiodata, S_IRUGO | S_IWUSR,
					m4sensorhub_ads_show_iiodata,
					NULL, 0);

static ssize_t m4sensorhub_ads_store_mode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	int ret;
	int mode;

	ret = kstrtoint(buf, 10, &mode);
	if (ret < 0)
		return ret;

	if (mode < 0 || mode > MODE_MAX)
		return -EINVAL;

	mutex_lock(&(priv_data->mutex));
	ret = m4sensorhub_reg_write(priv_data->m4sensorhub,
				M4SH_REG_ADSSENSOR_MODE,
				(char *)&mode, m4sh_no_mask);

	if (ret != m4sensorhub_reg_getsize(
			priv_data->m4sensorhub,
			M4SH_REG_ADSSENSOR_MODE))
		pr_err("%s:Unable to set mode\n", __func__);
	else
		priv_data->mode = mode;

	mutex_unlock(&(priv_data->mutex));
	return count;
}

static ssize_t m4sensorhub_ads_show_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);
	int ret;

	mutex_lock(&(priv_data->mutex));

	ret = snprintf(buf, PAGE_SIZE, "mode:0x%02x\n",
					priv_data->mode);

	mutex_unlock(&(priv_data->mutex));
	return ret;
}
static IIO_DEVICE_ATTR(mode, S_IRUGO | S_IWUSR,
					m4sensorhub_ads_show_mode,
					m4sensorhub_ads_store_mode, 0);
#define M4_DEV_ATTR(name) (&iio_dev_attr_##name.dev_attr.attr)

static struct attribute *m4sensorhub_ads_attributes[] = {
	M4_DEV_ATTR(setdelay),
	M4_DEV_ATTR(iiodata),
	M4_DEV_ATTR(mode),
	NULL
};

static const struct attribute_group m4sensorhub_ads_group = {
	.attrs = m4sensorhub_ads_attributes,
};

static const struct iio_info m4sensorhub_ads_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &m4sensorhub_ads_group,
};

static int m4sensorhub_ads_setup_buffer(struct iio_dev *iio_dev)
{
	struct iio_buffer *buffer;
	int ret;
	buffer = iio_kfifo_allocate(iio_dev);
	if (buffer == NULL) {
		pr_err("%s: failed to allocate buffer\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	iio_dev->buffer = buffer;

    /* need timestamps */
	buffer->scan_timestamp = true;
	ret = iio_buffer_register(iio_dev, iio_dev->channels,
						iio_dev->num_channels);

	if (ret < 0) {
		pr_err("%s: failed to register buffer\n", __func__);
		goto err;
	}
	buffer->access->set_bytes_per_datum(buffer,
				sizeof(struct m4sensorhub_ads_data));

	ret = 0;
	return ret;
err:
	iio_kfifo_free(buffer);

	return ret;
}

static int m4sensorhub_ads_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct iio_dev *iio_dev;
	struct m4sensorhub_ads_drvdata *priv_data;

	iio_dev = iio_device_alloc(
			sizeof(struct m4sensorhub_ads_drvdata));

	if (iio_dev == NULL) {
		pr_err("%s: no mem", __func__);
		ret = -ENOMEM;
		goto err;
	}

	priv_data = iio_priv(iio_dev);
	priv_data->pdev = pdev;
	priv_data->samplerate = -1;
	priv_data->enable = false;
	priv_data->m4sensorhub = NULL;
	priv_data->data = NULL;
	priv_data->mode = MODE_MAX;
	mutex_init(&(priv_data->mutex));
	priv_data->read_data.ch1_data = -1;
	priv_data->read_data.ch2_data = -1;
	priv_data->data_seq_num = 0;

	platform_set_drvdata(pdev, iio_dev);

	iio_dev->info = &m4sensorhub_ads_iio_info;
	iio_dev->name = m4sensorhub_ads_DRIVER_NAME;
	iio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_HARDWARE;
	iio_dev->channels = m4sensorhub_ads_channels;
	iio_dev->num_channels = ARRAY_SIZE(m4sensorhub_ads_channels);

	/* Register the channel with a buffer */
	ret = m4sensorhub_ads_setup_buffer(iio_dev);
	if (ret < 0) {
		pr_err("%s: can't setup buffer", __func__);
		goto cleanup1;
	}

	ret = iio_device_register(iio_dev);
	if (ret < 0) {
		pr_err("%s: iio_register failed", __func__);
		goto cleanup2;
	}

	ret = m4sensorhub_register_initcall(
			m4sensorhub_ads_driver_initcallback,
			iio_dev);
	if (ret < 0) {
		pr_err("%s:Register init failed, ret = %d\n", __func__, ret);
		goto cleanup3;
	}

	return 0;
cleanup3:
	iio_device_unregister(iio_dev);
cleanup2:
	iio_kfifo_free(iio_dev->buffer);
	iio_buffer_unregister(iio_dev);
cleanup1:
	iio_device_free(iio_dev);
	platform_set_drvdata(pdev, NULL);
err:
	return ret;
}

static int __exit m4sensorhub_ads_remove(struct platform_device *pdev)
{
	struct iio_dev *iio_dev =
						platform_get_drvdata(pdev);
	struct m4sensorhub_ads_drvdata *priv_data = iio_priv(iio_dev);

	mutex_lock(&(priv_data->mutex));
	m4sensorhub_irq_disable(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
	kfree(priv_data->data);
	priv_data->data = NULL;
	m4sensorhub_irq_unregister(priv_data->m4sensorhub, M4SH_NOWAKEIRQ_ADS);
	m4sensorhub_unregister_initcall(
				m4sensorhub_ads_driver_initcallback);

	iio_kfifo_free(iio_dev->buffer);
	iio_buffer_unregister(iio_dev);
	iio_device_unregister(iio_dev);
	mutex_unlock(&(priv_data->mutex));
	mutex_destroy(&(priv_data->mutex));
	iio_device_free(iio_dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void m4sensorhub_ads_shutdown(struct platform_device *pdev)
{
	return;
}

#ifdef CONFIG_PM
static int m4sensorhub_ads_suspend(struct platform_device *pdev,
				pm_message_t message)
{
	return 0;
}

static int m4sensorhub_ads_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define m4sensorhub_ads_suspend NULL
#define m4sensorhub_ads_resume  NULL
#endif

static struct of_device_id m4sensorhub_ads_match_tbl[] = {
	{ .compatible = "mot,m4ads" },
	{},
};

static struct platform_driver m4sensorhub_ads_driver = {
	.probe		= m4sensorhub_ads_probe,
	.remove		= __exit_p(m4sensorhub_ads_remove),
	.shutdown	= m4sensorhub_ads_shutdown,
	.suspend	= m4sensorhub_ads_suspend,
	.resume		= m4sensorhub_ads_resume,
	.driver		= {
		.name	= m4sensorhub_ads_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4sensorhub_ads_match_tbl),
	},
};

module_platform_driver(m4sensorhub_ads_driver);

MODULE_ALIAS("platform:m4sensorhub_ads");
MODULE_DESCRIPTION("M4 Sensor Hub ads IIO driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
