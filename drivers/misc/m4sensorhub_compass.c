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
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define m4com_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

#define M4COM_DRIVER_NAME           "m4sensorhub_compass"

#define M4COM_IRQ_ENABLED_BIT       0

struct m4com_sensor_data {
	int32_t         x;
	int32_t         y;
	int32_t         z;
	int8_t          accuracy;
};

struct m4com_driver_data {
	struct platform_device      *pdev;
	struct m4sensorhub_data     *m4;
	struct mutex                mutex; /* controls driver entry points */
	struct input_dev            *indev;

	struct m4com_sensor_data    sensdat;

	int16_t         samplerate;
	uint16_t        status;
};

static void m4com_isr(enum m4sensorhub_irqs int_event, void *handle)
{
	int err = 0;
	struct m4com_driver_data *dd = handle;
	int size = 0;
	struct timespec ts;

	mutex_lock(&(dd->mutex));

	get_monotonic_boottime(&ts);

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_COMPASS_X);
	if (size < 0) {
		m4com_err("%s: Reading from invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4com_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_COMPASS_X,
		(char *)&(dd->sensdat.x));
	if (err < 0) {
		m4com_err("%s: Failed to read X data.\n", __func__);
		goto m4com_isr_fail;
	} else if (err != size) {
		m4com_err("%s: Read %d bytes instead of %d.\n",
			  __func__, err, size);
		goto m4com_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_COMPASS_Y);
	if (size < 0) {
		m4com_err("%s: Reading from invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4com_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_COMPASS_Y,
		(char *)&(dd->sensdat.y));
	if (err < 0) {
		m4com_err("%s: Failed to read Y data.\n", __func__);
		goto m4com_isr_fail;
	} else if (err != size) {
		m4com_err("%s: Read %d bytes instead of %d.\n",
			  __func__, err, size);
		goto m4com_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_COMPASS_Z);
	if (size < 0) {
		m4com_err("%s: Reading from invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4com_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_COMPASS_Z,
		(char *)&(dd->sensdat.z));
	if (err < 0) {
		m4com_err("%s: Failed to read Z data.\n", __func__);
		goto m4com_isr_fail;
	} else if (err != size) {
		m4com_err("%s: Read %d bytes instead of %d.\n",
			  __func__, err, size);
		goto m4com_isr_fail;
	}

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_COMPASS_ACCURACY);
	if (size < 0) {
		m4com_err("%s: Reading from invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4com_isr_fail;
	}

	err = m4sensorhub_reg_read(dd->m4, M4SH_REG_COMPASS_ACCURACY,
		(char *)&(dd->sensdat.accuracy));
	if (err < 0) {
		m4com_err("%s: Failed to read accuracy data.\n", __func__);
		goto m4com_isr_fail;
	} else if (err != size) {
		m4com_err("%s: Read %d bytes instead of %d.\n",
			  __func__, err, size);
		goto m4com_isr_fail;
	}

	input_event(dd->indev, EV_MSC, MSC_TIMESTAMP, ts.tv_sec);
	input_event(dd->indev, EV_MSC, MSC_TIMESTAMP, ts.tv_nsec);
	input_report_rel(dd->indev, REL_X, dd->sensdat.x);
	input_report_rel(dd->indev, REL_Y, dd->sensdat.y);
	input_report_rel(dd->indev, REL_Z, dd->sensdat.z);
	input_report_abs(dd->indev, ABS_MISC, dd->sensdat.accuracy);
	input_sync(dd->indev);

m4com_isr_fail:
	if (err < 0)
		m4com_err("%s: Failed with error code %d.\n", __func__, err);

	mutex_unlock(&(dd->mutex));

	return;
}

static int m4com_set_samplerate(struct m4com_driver_data *dd, int16_t rate)
{
	int err = 0;
	int size = 0;

	if (rate == dd->samplerate)
		goto m4com_set_samplerate_irq_check;

	size = m4sensorhub_reg_getsize(dd->m4, M4SH_REG_COMPASS_SAMPLERATE);
	if (size < 0) {
		m4com_err("%s: Writing to invalid register %d.\n",
			  __func__, size);
		err = size;
		goto m4com_set_samplerate_fail;
	}

	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_COMPASS_SAMPLERATE,
		(char *)&rate, m4sh_no_mask);
	if (err < 0) {
		m4com_err("%s: Failed to set sample rate.\n", __func__);
		goto m4com_set_samplerate_fail;
	} else if (err != size) {
		m4com_err("%s: Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
		err = -EBADE;
		goto m4com_set_samplerate_fail;
	}
	dd->samplerate = rate;

m4com_set_samplerate_irq_check:
	if (rate >= 0) {
		/* Enable the IRQ if necessary */
		if (!(dd->status & (1 << M4COM_IRQ_ENABLED_BIT))) {
			err = m4sensorhub_irq_enable(dd->m4,
				M4SH_NOWAKEIRQ_COMPASS);
			if (err < 0) {
				m4com_err("%s: Failed to enable irq.\n",
					  __func__);
				goto m4com_set_samplerate_fail;
			}
			dd->status = dd->status | (1 << M4COM_IRQ_ENABLED_BIT);
		}
	} else {
		/* Disable the IRQ if necessary */
		if (dd->status & (1 << M4COM_IRQ_ENABLED_BIT)) {
			err = m4sensorhub_irq_disable(dd->m4,
				M4SH_NOWAKEIRQ_COMPASS);
			if (err < 0) {
				m4com_err("%s: Failed to disable irq.\n",
					  __func__);
				goto m4com_set_samplerate_fail;
			}
			dd->status = dd->status & ~(1 << M4COM_IRQ_ENABLED_BIT);
		}
	}

m4com_set_samplerate_fail:
	return err;
}

static ssize_t m4com_setrate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct m4com_driver_data *dd = dev_get_drvdata(dev);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "Current rate: %hd\n", dd->samplerate);
	mutex_unlock(&(dd->mutex));
	return size;
}
static ssize_t m4com_setrate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct m4com_driver_data *dd = dev_get_drvdata(dev);
	int value = 0;

	mutex_lock(&(dd->mutex));

	err = kstrtoint(buf, 10, &value);
	if (err < 0) {
		m4com_err("%s: Failed to convert value.\n", __func__);
		goto m4com_enable_store_exit;
	}

	if ((value < -1) || (value > 32767)) {
		m4com_err("%s: Invalid sample rate requested = %d\n",
			  __func__, value);
		err = -EOVERFLOW;
		goto m4com_enable_store_exit;
	}

	err = m4com_set_samplerate(dd, value);
	if (err < 0) {
		m4com_err("%s: Failed to set sample rate.\n", __func__);
		goto m4com_enable_store_exit;
	}

m4com_enable_store_exit:
	if (err < 0) {
		m4com_err("%s: Failed with error code %d.\n", __func__, err);
		size = err;
	}
	mutex_unlock(&(dd->mutex));

	return size;
}
static DEVICE_ATTR(setrate, S_IRUSR | S_IWUSR,
		m4com_setrate_show, m4com_setrate_store);

static ssize_t m4com_sensordata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct m4com_driver_data *dd = dev_get_drvdata(dev);
	ssize_t size = 0;

	mutex_lock(&(dd->mutex));
	size = snprintf(buf, PAGE_SIZE, "%s%d\n%s%d\n%s%d\n%s%hhd\n",
		"X: ", dd->sensdat.x,
		"Y: ", dd->sensdat.y,
		"Z: ", dd->sensdat.z,
		"Accuracy: ", dd->sensdat.accuracy);
	mutex_unlock(&(dd->mutex));
	return size;
}
static DEVICE_ATTR(sensordata, S_IRUGO, m4com_sensordata_show, NULL);

static int m4com_create_sysfs(struct m4com_driver_data *dd)
{
	int err = 0;

	err = device_create_file(&(dd->pdev->dev), &dev_attr_setrate);
	if (err < 0) {
		m4com_err("%s: Failed to create setrate %s %d.\n",
			  __func__, "with error", err);
		goto m4com_create_sysfs_exit;
	}

	err = device_create_file(&(dd->pdev->dev), &dev_attr_sensordata);
	if (err < 0) {
		m4com_err("%s: Failed to create sensordata %s %d.\n",
			  __func__, "with error", err);
		goto m4com_create_sysfs_sensordata_fail;
	}

	goto m4com_create_sysfs_exit;

m4com_create_sysfs_sensordata_fail:
	device_remove_file(&(dd->pdev->dev), &dev_attr_setrate);
m4com_create_sysfs_exit:
	return err;
}

static int m4com_remove_sysfs(struct m4com_driver_data *dd)
{
	int err = 0;

	device_remove_file(&(dd->pdev->dev), &dev_attr_setrate);
	device_remove_file(&(dd->pdev->dev), &dev_attr_sensordata);

	return err;
}

static int m4com_create_m4eventdev(struct m4com_driver_data *dd)
{
	int err = 0;

	dd->indev = input_allocate_device();
	if (dd->indev == NULL) {
		m4com_err("%s: Failed to allocate input device.\n",
			  __func__);
		err = -ENODATA;
		goto m4com_create_m4eventdev_fail;
	}

	dd->indev->name = M4COM_DRIVER_NAME;
	input_set_drvdata(dd->indev, dd);
	set_bit(EV_REL, dd->indev->evbit);
	set_bit(REL_X, dd->indev->relbit);
	set_bit(REL_Y, dd->indev->relbit);
	set_bit(REL_Z, dd->indev->relbit);

	set_bit(EV_ABS, dd->indev->evbit);
	input_set_abs_params(dd->indev, ABS_MISC, 0, 3, 0, 0);
	input_set_capability(dd->indev, EV_MSC, MSC_TIMESTAMP);

	err = input_register_device(dd->indev);
	if (err < 0) {
		m4com_err("%s: Failed to register input device.\n",
			  __func__);
		input_free_device(dd->indev);
		dd->indev = NULL;
		goto m4com_create_m4eventdev_fail;
	}

m4com_create_m4eventdev_fail:
	return err;
}

static void m4com_panic_restore(struct m4sensorhub_data *m4sensorhub,
				void *data)
{
	int size, err;
	struct m4com_driver_data *dd = (struct m4com_driver_data *)data;

	if (dd == NULL) {
		m4com_err("%s: Driver data is null, unable to restore\n",
			  __func__);
		return;
	}

	mutex_lock(&(dd->mutex));

	size = m4sensorhub_reg_getsize(dd->m4,
				       M4SH_REG_COMPASS_SAMPLERATE);
	err = m4sensorhub_reg_write(dd->m4, M4SH_REG_COMPASS_SAMPLERATE,
				   (char *)&dd->samplerate, m4sh_no_mask);
	if (err < 0) {
		m4com_err("%s: Failed to set sample rate.\n", __func__);
	} else if (err != size) {
		m4com_err("%s: Wrote %d bytes instead of %d.\n",
			  __func__, err, size);
	}

	mutex_unlock(&(dd->mutex));
}

static int m4com_driver_init(struct init_calldata *p_arg)
{
	struct m4com_driver_data *dd = p_arg->p_data;
	int err = 0;

	mutex_lock(&(dd->mutex));

	err = m4com_create_m4eventdev(dd);
	if (err < 0) {
		m4com_err("%s: Failed to create M4 event device.\n", __func__);
		goto m4com_driver_init_fail;
	}

	err = m4sensorhub_irq_register(dd->m4,
		M4SH_NOWAKEIRQ_COMPASS, m4com_isr, dd, 0);
	if (err < 0) {
		m4com_err("%s: Failed to register M4 IRQ.\n", __func__);
		goto m4com_driver_init_fail;
	}

	err = m4sensorhub_panic_register(dd->m4, PANICHDL_COMPASS_RESTORE,
					 m4com_panic_restore, dd);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "Com panic callback register failed\n");

	goto m4com_driver_init_exit;

m4com_driver_init_fail:
	m4com_err("%s: Init failed with error code %d.\n", __func__, err);
m4com_driver_init_exit:
	mutex_unlock(&(dd->mutex));
	return err;
}

static int m4com_probe(struct platform_device *pdev)
{
	struct m4com_driver_data *dd = NULL;
	int err = 0;

	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (dd == NULL) {
		m4com_err("%s: Failed to allocate driver data.\n", __func__);
		err = -ENOMEM;
		goto m4com_probe_fail_nodd;
	}

	dd->pdev = pdev;
	mutex_init(&(dd->mutex));
	platform_set_drvdata(pdev, dd);
	dd->samplerate = -1; /* We always start disabled */

	dd->m4 = m4sensorhub_client_get_drvdata();
	if (dd->m4 == NULL) {
		m4com_err("%s: M4 sensor data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4com_probe_fail;
	}

	err = m4sensorhub_register_initcall(m4com_driver_init, dd);
	if (err < 0) {
		m4com_err("%s: Failed to register initcall.\n", __func__);
		goto m4com_probe_fail;
	}

	err = m4com_create_sysfs(dd);
	if (err < 0) {
		m4com_err("%s: Failed to create sysfs.\n", __func__);
		goto m4com_driver_init_sysfs_fail;
	}

	return 0;

m4com_driver_init_sysfs_fail:
	m4sensorhub_unregister_initcall(m4com_driver_init);
m4com_probe_fail:
	mutex_destroy(&(dd->mutex));
	kfree(dd);
m4com_probe_fail_nodd:
	m4com_err("%s: Probe failed with error code %d.\n", __func__, err);
	return err;
}

static int __exit m4com_remove(struct platform_device *pdev)
{
	struct m4com_driver_data *dd = platform_get_drvdata(pdev);

	mutex_lock(&(dd->mutex));
	if (dd->status & (1 << M4COM_IRQ_ENABLED_BIT)) {
		m4sensorhub_irq_disable(dd->m4, M4SH_NOWAKEIRQ_COMPASS);
		dd->status = dd->status & ~(1 << M4COM_IRQ_ENABLED_BIT);
	}
	m4sensorhub_irq_unregister(dd->m4, M4SH_NOWAKEIRQ_COMPASS);
	m4com_remove_sysfs(dd);
	m4sensorhub_unregister_initcall(m4com_driver_init);
	if (dd->indev != NULL)
		input_unregister_device(dd->indev);
	mutex_destroy(&(dd->mutex));
	kfree(dd);

	return 0;
}

static struct of_device_id m4com_match_tbl[] = {
	{ .compatible = "mot,m4compass" },
	{},
};

static struct platform_driver m4com_driver = {
	.probe		= m4com_probe,
	.remove		= __exit_p(m4com_remove),
	.shutdown	= NULL,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= M4COM_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4com_match_tbl),
	},
};

static int __init m4com_init(void)
{
	return platform_driver_register(&m4com_driver);
}

static void __exit m4com_exit(void)
{
	platform_driver_unregister(&m4com_driver);
}

module_init(m4com_init);
module_exit(m4com_exit);

MODULE_ALIAS("platform:m4com");
MODULE_DESCRIPTION("M4 Sensor Hub Compass client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
