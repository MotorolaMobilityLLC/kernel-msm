/*
 *  Copyright (C) 2015 Motorola, Inc.
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
#include <linux/m4sensorhub_batch.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define U32_MAX ((u32)(~0U))
#define m4sensorhub_batch_DRIVER_NAME	"m4sensorhub_batch"

#define DATA_SIZE_IN_BITS  (sizeof(struct m4sensorhub_batch_data) * 8)

#define MAX_NUM_BATCH_SAMPLES 300

struct m4sensorhub_batch_drvdata {
	struct m4sensorhub_data *m4sensorhub;
	struct platform_device  *pdev;
	struct mutex		mutex; /* controls driver entry points */
	struct m4sensorhub_batch_sample *data;
	uint32_t data_seq_num;
	int num_buffered_samples;
	uint8_t init_complete;
};

struct m4sensorhub_batch_data_callback {
	void *priv_data;
	void (*data_callback)(void *batch_event_data, void *priv_data,
		int64_t monobase, int num_events);
};

/* --------------- Local Declarations -------------- */
struct m4sensorhub_batch_drvdata m4sensorhub_drvdata;

struct m4sensorhub_batch_data_callback
	batch_data_callback_list[M4SH_BATCH_SENSOR_TYPE_MAX];

int m4sensorhub_batch_register_data_callback(u8 sensor_type, void *priv_data,
	void (*data_callback)(void *batch_event_data, void *priv_data,
		int64_t monobase, int num_events))
{
	int ret = 0;
	struct m4sensorhub_batch_drvdata *dd = &m4sensorhub_drvdata;

	if (dd->init_complete != 1) {
		pr_err("%s: Init not complete, try again. sensor_type: %d\n",
		       __func__, sensor_type);
		ret = -EAGAIN;
		goto err;
	}
	if (sensor_type < 0 || sensor_type > M4SH_BATCH_SENSOR_TYPE_MAX) {
		pr_err("%s: Invalid sensor_type: %d\n", __func__, sensor_type);
		ret = -EINVAL;
		goto err;
	}

	if (data_callback == NULL) {
		pr_err("%s: data_callback is null\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	mutex_lock(&(dd->mutex));
	if (batch_data_callback_list[sensor_type].priv_data == NULL &&
	    batch_data_callback_list[sensor_type].data_callback == NULL) {
		batch_data_callback_list[sensor_type].priv_data = priv_data;
		batch_data_callback_list[sensor_type].data_callback =
			data_callback;
		pr_debug("%s: callback_set for sensor: %d\n", __func__,
			 sensor_type);
	} else {
		pr_err("%s: priv_data or callback already set sensor: %d\n",
		       __func__, sensor_type);
		ret = -EEXIST;
	}
	mutex_unlock(&(dd->mutex));
err:
	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_batch_register_data_callback);

int m4sensorhub_batch_unregister_data_callback(u8 sensor_type,
	void (*data_callback)(void *batch_event_data, void *priv_data,
	int64_t monobase, int num_events))
{
	int ret = 0;
	struct m4sensorhub_batch_drvdata *dd = &m4sensorhub_drvdata;
	if (sensor_type < 0 || sensor_type > M4SH_BATCH_SENSOR_TYPE_MAX) {
		pr_err("%s: Invalid sensor_type: %d\n", __func__, sensor_type);
		ret = -EINVAL;
		goto err;
	}

	mutex_lock(&(dd->mutex));
	batch_data_callback_list[sensor_type].priv_data = NULL;
	batch_data_callback_list[sensor_type].data_callback = NULL;
	mutex_unlock(&(dd->mutex));
err:
	return ret;

}
EXPORT_SYMBOL_GPL(m4sensorhub_batch_unregister_data_callback);

static void m4_read_batch_data_locked(struct m4sensorhub_batch_drvdata *dd)
{
	int ret, i;
	int bytes_to_read;
	int64_t monobase;

	/* read the amount of data M4 expects to push.
	 * This keeps changing at runtime */
	ret = m4sensorhub_reg_read(dd->m4sensorhub,
				M4SH_REG_GENERAL_NUMBATCHSAMPLES,
				(char *)&(dd->num_buffered_samples));
	if (ret < 0) {
		pr_err("%s: err:buffered data length(%d)\n", __func__, ret);
		dd->num_buffered_samples = -1;
		return;
	}

	if (dd->num_buffered_samples > MAX_NUM_BATCH_SAMPLES) {
		pr_err("%s: err: OOM\n", __func__);
		return;
	}
	bytes_to_read = dd->num_buffered_samples *
		sizeof(struct m4sensorhub_batch_sample);

	monobase = ktime_to_ns(ktime_get_boottime());

	ret = m4sensorhub_reg_read_buffer(dd->m4sensorhub,
			M4SH_REG_GENERAL_READBATCHSAMPLES,
			(u8 *)dd->data, bytes_to_read);

	if (ret < 0 || ret != bytes_to_read) {
		pr_err("%s: unable to read data (%d)\n", __func__, ret);
		return;
	}

	for (i = 0; i < M4SH_BATCH_SENSOR_TYPE_MAX; i++) {
		if (batch_data_callback_list[i].data_callback != NULL) {
			batch_data_callback_list[i].data_callback(
				dd->data, batch_data_callback_list[i].priv_data,
				monobase, dd->num_buffered_samples);
		}
	}

	return;
}

static void m4sensorhub_batch_panic_restore(
	struct m4sensorhub_data *m4sensorhub, void *data)
{
	struct m4sensorhub_batch_drvdata *dd =
			(struct m4sensorhub_batch_drvdata *)data;

	mutex_lock(&(dd->mutex));
	m4sensorhub_irq_enable(dd->m4sensorhub, M4SH_NOWAKEIRQ_BATCH);
	mutex_unlock(&(dd->mutex));
}

static void m4_handle_batch_irq(enum m4sensorhub_irqs int_event,
							void *data)
{
	struct m4sensorhub_batch_drvdata *dd = data;
	mutex_lock(&(dd->mutex));
	m4_read_batch_data_locked(dd);
	mutex_unlock(&(dd->mutex));
}

static int m4sensorhub_batch_driver_initcallback(struct init_calldata *arg)
{
	struct m4sensorhub_batch_drvdata *dd = arg->p_data;
	int ret;

	ret = m4sensorhub_panic_register(dd->m4sensorhub,
		PANICHDL_BATCH_RESTORE, m4sensorhub_batch_panic_restore, dd);
	if (ret < 0)
		pr_err("%s: failed panic register(%d)\n", __func__, ret);

	ret = m4sensorhub_irq_register(dd->m4sensorhub,
			M4SH_NOWAKEIRQ_BATCH, m4_handle_batch_irq,
			dd, 0);
	if (ret < 0)
		pr_err("%s: failed irq register(%d)\n", __func__, ret);

	m4sensorhub_irq_enable(dd->m4sensorhub, M4SH_NOWAKEIRQ_BATCH);

	return 0;
}

static int m4sensorhub_batch_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct m4sensorhub_batch_drvdata *dd = &m4sensorhub_drvdata;

	dd->pdev = pdev;
	dd->m4sensorhub = NULL;
	dd->data = NULL;
	mutex_init(&(dd->mutex));

	mutex_lock(&(dd->mutex));
	dd->num_buffered_samples = -1;

	platform_set_drvdata(pdev, dd);

	dd->data = kzalloc(MAX_NUM_BATCH_SAMPLES *
			   sizeof(struct m4sensorhub_batch_sample),
			   GFP_KERNEL);
	if (dd->data == NULL) {
		pr_err("%s: err: OOM\n", __func__);
		ret = -ENOMEM;
		goto cleanup;
	}

	dd->m4sensorhub = m4sensorhub_client_get_drvdata();
	if (dd->m4sensorhub == NULL) {
		pr_err("%s: M4 sensor data is NULL.\n", __func__);
		ret = -ENODATA;
		goto cleanup_mem;
	}

	ret = m4sensorhub_register_initcall(
			m4sensorhub_batch_driver_initcallback,
			dd);
	if (ret < 0) {
		pr_err("%s:Register init failed, ret = %d\n", __func__, ret);
		goto cleanup_mem;
	}

	dd->init_complete = 1;
	mutex_unlock(&(dd->mutex));
	return 0;
cleanup_mem:
	kfree(dd->data);
	dd->data = NULL;
cleanup:
	platform_set_drvdata(pdev, NULL);
	mutex_unlock(&(dd->mutex));
	mutex_destroy(&(dd->mutex));
	return ret;
}

static int __exit m4sensorhub_batch_remove(struct platform_device *pdev)
{
	struct m4sensorhub_batch_drvdata *dd = platform_get_drvdata(pdev);

	mutex_lock(&(dd->mutex));
	m4sensorhub_irq_disable(dd->m4sensorhub, M4SH_NOWAKEIRQ_BATCH);
	m4sensorhub_irq_unregister(dd->m4sensorhub, M4SH_NOWAKEIRQ_BATCH);
	m4sensorhub_unregister_initcall(
				m4sensorhub_batch_driver_initcallback);

	kfree(dd->data);
	dd->data = NULL;
	mutex_unlock(&(dd->mutex));
	mutex_destroy(&(dd->mutex));
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void m4sensorhub_batch_shutdown(struct platform_device *pdev)
{
	return;
}

#ifdef CONFIG_PM
static int m4sensorhub_batch_suspend(struct platform_device *pdev,
				pm_message_t message)
{
	return 0;
}

static int m4sensorhub_batch_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define m4sensorhub_batch_suspend NULL
#define m4sensorhub_batch_resume  NULL
#endif

static struct of_device_id m4sensorhub_batch_match_tbl[] = {
	{ .compatible = "mot,m4batch" },
	{},
};

static struct platform_driver m4sensorhub_batch_driver = {
	.probe		= m4sensorhub_batch_probe,
	.remove		= __exit_p(m4sensorhub_batch_remove),
	.shutdown	= m4sensorhub_batch_shutdown,
	.suspend	= m4sensorhub_batch_suspend,
	.resume		= m4sensorhub_batch_resume,
	.driver		= {
		.name	= m4sensorhub_batch_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4sensorhub_batch_match_tbl),
	},
};

module_platform_driver(m4sensorhub_batch_driver);

MODULE_ALIAS("platform:m4sensorhub_batch");
MODULE_DESCRIPTION("M4 Sensor Hub batch driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
