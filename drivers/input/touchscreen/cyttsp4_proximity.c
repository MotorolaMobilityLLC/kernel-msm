/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */

/*
 * cyttsp4_proximity.c
 * Cypress TrueTouch(TM) Standard Product V4 Proximity touch reports module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/cyttsp4_bus.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_mt.h>
#include <linux/cyttsp4_proximity.h>
#include "cyttsp4_regs.h"

/* Timeout value in ms. */
#define CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT		1000

#define CY_PROXIMITY_ON 0
#define CY_PROXIMITY_OFF 1

static int cyttsp4_proximity_attention(struct cyttsp4_device *ttsp);
static int cyttsp4_startup_attention(struct cyttsp4_device *ttsp);

struct cyttsp4_proximity_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_proximity_platform_data *pdata;
	struct cyttsp4_sysinfo *si;
	struct input_dev *input;
	struct mutex report_lock;
	struct mutex sysfs_lock;
	int enable_count;
	bool is_suspended;
	bool input_device_registered;
	char phys[NAME_MAX];
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
};

static void cyttsp4_report_proximity(struct cyttsp4_proximity_data *pd, bool on)
{
	int val = on ? CY_PROXIMITY_ON : CY_PROXIMITY_OFF;

	input_report_abs(pd->input, ABS_DISTANCE, val);
	input_sync(pd->input);
}

static void cyttsp4_get_proximity_touch(struct cyttsp4_proximity_data *pd,
					int num_cur_rec)
{
	struct cyttsp4_touch tch;
	int i;

	for (i = 0; i < num_cur_rec; i++) {
		cyttsp4_get_touch_record(pd->ttsp, i, tch.abs);

		/* Check for proximity event */
		if (tch.abs[CY_TCH_O] == CY_OBJ_PROXIMITY) {
			if (tch.abs[CY_TCH_E] == CY_EV_TOUCHDOWN)
				cyttsp4_report_proximity(pd, true);
			else if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF)
				cyttsp4_report_proximity(pd, false);
			break;
		}
	}
}

/* read xy_data for all current touches */
static int cyttsp4_xy_worker(struct cyttsp4_proximity_data *pd)
{
	struct device *dev = &pd->ttsp->dev;
	struct cyttsp4_sysinfo *si = pd->si;
	u8 num_cur_rec;
	u8 rep_len;
	u8 rep_stat;
	u8 tt_stat;
	int rc = 0;

	/*
	 * Get event data from cyttsp4 device.
	 * The event data includes all data
	 * for all active touches.
	 * Event data also includes button data
	 */
	rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
	tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];

	num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);

	if (rep_len == 0 && num_cur_rec > 0) {
		tp_log_err("%s: report length error rep_len=%d num_rec=%d\n",
			   __func__, rep_len, num_cur_rec);
		goto cyttsp4_xy_worker_exit;
	}

	/* check any error conditions */
	if (IS_BAD_PKT(rep_stat)) {
		tp_log_debug("%s: Invalid buffer detected\n", __func__);
		rc = 0;
		goto cyttsp4_xy_worker_exit;
	}

	if (IS_LARGE_AREA(tt_stat))
		tp_log_debug("%s: Large area detected\n", __func__);

	if (num_cur_rec > si->si_ofs.max_tchs) {
		tp_log_err("%s: %s (n=%d c=%d)\n", __func__,
			   "too many tch; set to max tch",
			   num_cur_rec, si->si_ofs.max_tchs);
		num_cur_rec = si->si_ofs.max_tchs;
	}

	/* extract xy_data for all currently reported touches */
	tp_log_debug("%s: extract data num_cur_rec=%d\n", __func__,
		     num_cur_rec);
	if (num_cur_rec)
		cyttsp4_get_proximity_touch(pd, num_cur_rec);
	else
		cyttsp4_report_proximity(pd, false);

	tp_log_debug("%s: done\n", __func__);
	rc = 0;

cyttsp4_xy_worker_exit:
	return rc;
}

static int _cyttsp4_proximity_enable(struct cyttsp4_proximity_data *pd)
{
	struct cyttsp4_device *ttsp = pd->ttsp;
	struct device *dev = &ttsp->dev;
	int rc = 0;

	tp_log_debug("%s\n", __func__);

	/* We use pm_runtime_get_sync to activate
	 * the core device until it is disabled back
	 */
	pm_runtime_get_sync(dev);

	rc = cyttsp4_request_exclusive(ttsp,
				       CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Error on request exclusive r=%d\n",
			   __func__, rc);
		goto exit;
	}

	rc = cyttsp4_request_enable_scan_type(ttsp, CY_ST_PROXIMITY);
	if (rc < 0) {
		tp_log_err
		    ("%s: Error on request enable proximity scantype r=%d\n",
		     __func__, rc);
		goto exit_release;
	}

	tp_log_debug("%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
				    cyttsp4_proximity_attention,
				    CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
				    cyttsp4_startup_attention, 0);

exit_release:
	cyttsp4_release_exclusive(ttsp);
exit:
	return rc;
}

static int _cyttsp4_proximity_disable(struct cyttsp4_proximity_data *pd,
				      bool force)
{
	struct cyttsp4_device *ttsp = pd->ttsp;
	struct device *dev = &ttsp->dev;
	int rc = 0;

	tp_log_debug("%s\n", __func__);

	rc = cyttsp4_request_exclusive(ttsp,
				       CY_PROXIMITY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Error on request exclusive r=%d\n",
			   __func__, rc);
		goto exit;
	}

	rc = cyttsp4_request_disable_scan_type(ttsp, CY_ST_PROXIMITY);
	if (rc < 0) {
		tp_log_err("%s: Error on request disable proximity scan r=%d\n",
			   __func__, rc);
		goto exit_release;
	}

exit_release:
	cyttsp4_release_exclusive(ttsp);

exit:
	if (!rc || force) {
		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
					      cyttsp4_proximity_attention,
					      CY_MODE_OPERATIONAL);

		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
					      cyttsp4_startup_attention, 0);

		pm_runtime_put(dev);
	}

	return rc;
}

static ssize_t cyttsp4_proximity_enable_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);
	int val = 0;

	mutex_lock(&pd->sysfs_lock);
	val = pd->enable_count;
	mutex_unlock(&pd->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "%d\n", val);
}

static ssize_t cyttsp4_proximity_enable_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, u32 size)
{
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0 || (value != 0 && value != 1)) {
		tp_log_err("%s: Invalid value\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pd->sysfs_lock);
	if (value) {
		if (pd->enable_count++) {
			tp_log_debug("%s: '%s' already enabled\n", __func__,
				     pd->ttsp->name);
		} else {
			rc = _cyttsp4_proximity_enable(pd);
			if (rc)
				pd->enable_count--;
		}
	} else {
		if (--pd->enable_count) {
			if (pd->enable_count < 0) {
				tp_log_err("%s: '%s' unbalanced disable\n",
					   __func__, pd->ttsp->name);
				pd->enable_count = 0;
			}
		} else {
			rc = _cyttsp4_proximity_disable(pd, false);
			if (rc)
				pd->enable_count++;
		}
	}
	mutex_unlock(&pd->sysfs_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(enable, S_IRUSR | S_IWUSR,
		   cyttsp4_proximity_enable_show,
		   cyttsp4_proximity_enable_store);

static int cyttsp4_proximity_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);
	int rc = 0;

	tp_log_debug("%s\n", __func__);

	mutex_lock(&pd->report_lock);
	if (!pd->is_suspended) {
		/* core handles handshake */
		rc = cyttsp4_xy_worker(pd);
	} else {
		tp_log_debug("%s: Ignoring report while suspended\n", __func__);
	}
	mutex_unlock(&pd->report_lock);
	if (rc < 0)
		tp_log_err("%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp4_startup_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&pd->report_lock);
	cyttsp4_report_proximity(pd, false);
	mutex_unlock(&pd->report_lock);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int cyttsp4_proximity_suspend(struct device *dev)
{
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&pd->report_lock);
	pd->is_suspended = true;
	cyttsp4_report_proximity(pd, false);
	mutex_unlock(&pd->report_lock);

	return 0;
}

static int cyttsp4_proximity_resume(struct device *dev)
{
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&pd->report_lock);
	pd->is_suspended = false;
	mutex_unlock(&pd->report_lock);

	return 0;
}
#endif

static const struct dev_pm_ops cyttsp4_proximity_pm_ops = {
	SET_RUNTIME_PM_OPS(cyttsp4_proximity_suspend,
			   cyttsp4_proximity_resume, NULL)
};

static int cyttsp4_setup_input_device_and_sysfs(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);
	int signal = CY_IGNORE_VALUE;
	int min, max;
	int i;
	int rc;

	rc = device_create_file(dev, &dev_attr_enable);
	if (rc) {
		tp_log_err("%s: Error, could not create enable\n", __func__);
		goto exit;
	}

	tp_log_debug("%s: Initialize event signals\n", __func__);

	__set_bit(EV_ABS, pd->input->evbit);

	for (i = 0; i < (pd->pdata->frmwrk->size / CY_NUM_ABS_SET); i++) {
		signal = pd->pdata->frmwrk->abs
		    [(i * CY_NUM_ABS_SET) + CY_SIGNAL_OST];
		if (signal != CY_IGNORE_VALUE) {
			min = pd->pdata->frmwrk->abs
			    [(i * CY_NUM_ABS_SET) + CY_MIN_OST];
			max = pd->pdata->frmwrk->abs
			    [(i * CY_NUM_ABS_SET) + CY_MAX_OST];
			input_set_abs_params(pd->input, signal, min, max,
					     pd->pdata->frmwrk->abs
					     [(i * CY_NUM_ABS_SET) +
					      CY_FUZZ_OST],
					     pd->pdata->frmwrk->
					     abs[(i * CY_NUM_ABS_SET) +
						 CY_FLAT_OST]);
		}
	}

	rc = input_register_device(pd->input);
	if (rc) {
		tp_log_err("%s: Error, failed register input device r=%d\n",
			   __func__, rc);
		goto unregister_enable;
	}

	pd->input_device_registered = true;
	return rc;

unregister_enable:
	device_remove_file(dev, &dev_attr_enable);
exit:
	return rc;
}

static int cyttsp4_setup_input_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);
	int rc;

	tp_log_debug("%s\n", __func__);

	pd->si = cyttsp4_request_sysinfo(ttsp);
	if (!pd->si)
		return -EINVAL;

	rc = cyttsp4_setup_input_device_and_sysfs(ttsp);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_setup_input_attention, 0);

	return rc;
}

static int cyttsp4_proximity_probe(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_proximity_data *pd;
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	tp_log_info("%s\n", __func__);
	tp_log_debug("%s: debug on\n", __func__);
	tp_log_debug("%s: verbose debug on\n", __func__);

	if (pdata == NULL) {
		tp_log_err("%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	pd = kzalloc(sizeof(*pd), GFP_KERNEL);
	if (pd == NULL) {
		tp_log_err("%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	mutex_init(&pd->report_lock);
	mutex_init(&pd->sysfs_lock);
	pd->ttsp = ttsp;
	pd->pdata = pdata;
	dev_set_drvdata(dev, pd);
	/* Create the input device and register it. */
	tp_log_debug("%s: Create the input device and register it\n", __func__);
	pd->input = input_allocate_device();
	if (pd->input == NULL) {
		tp_log_err("%s: Error, failed to allocate input device\n",
			   __func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	pd->input->name = ttsp->name;
	scnprintf(pd->phys, sizeof(pd->phys) - 1, "%s", dev_name(dev));
	pd->input->phys = pd->phys;
	pd->input->dev.parent = &pd->ttsp->dev;
	input_set_drvdata(pd->input, pd);

	pm_runtime_enable(dev);

	/* get sysinfo */
	pd->si = cyttsp4_request_sysinfo(ttsp);
	if (pd->si) {
		rc = cyttsp4_setup_input_device_and_sysfs(ttsp);
		if (rc)
			goto error_init_input;
	} else {
		tp_log_err("%s: Fail get sysinfo pointer from core \n",
			   __func__);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
					    cyttsp4_setup_input_attention, 0);
	}

	tp_log_debug("%s: ok\n", __func__);
	return 0;

error_init_input:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	input_free_device(pd->input);
error_alloc_failed:
	dev_set_drvdata(dev, NULL);
	kfree(pd);
error_alloc_data_failed:
error_no_pdata:
	tp_log_err("%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_proximity_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_proximity_data *pd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	if (pd->input_device_registered) {
		/* Disable proximity sensing */
		mutex_lock(&pd->sysfs_lock);
		if (pd->enable_count)
			_cyttsp4_proximity_disable(pd, true);
		mutex_unlock(&pd->sysfs_lock);
		device_remove_file(dev, &dev_attr_enable);
		input_unregister_device(pd->input);
	} else {
		input_free_device(pd->input);
		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
					      cyttsp4_setup_input_attention, 0);
	}

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	dev_set_drvdata(dev, NULL);
	kfree(pd);
	return 0;
}

static struct cyttsp4_driver cyttsp4_proximity_driver = {
	.probe = cyttsp4_proximity_probe,
	.remove = cyttsp4_proximity_release,
	.driver = {
		   .name = CYTTSP4_PROXIMITY_NAME,
		   .bus = &cyttsp4_bus_type,
		   .owner = THIS_MODULE,
		   .pm = &cyttsp4_proximity_pm_ops,
		   },
};

static int __init cyttsp4_proximity_init(void)
{
	int rc = 0;
	rc = cyttsp4_register_driver(&cyttsp4_proximity_driver);
	tp_log_info("%s: Cypress TTSP MT v4 Proximity (Built %s), rc=%d\n",
		    __func__, CY_DRIVER_DATE, rc);
	return rc;
}

module_init(cyttsp4_proximity_init);

static void __exit cyttsp4_proximity_exit(void)
{
	cyttsp4_unregister_driver(&cyttsp4_proximity_driver);
	tp_log_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_proximity_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TTSP Proximity driver");
MODULE_AUTHOR("Cypress Semiconductor");

