/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */

/*
 * cyttsp4_btn.c
 * Cypress TrueTouch(TM) Standard Product V4 CapSense touch reports module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
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
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_core.h>
#include "cyttsp4_regs.h"

struct cyttsp4_btn_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_btn_platform_data *pdata;
	struct cyttsp4_sysinfo *si;
	struct input_dev *input;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#endif
	struct mutex report_lock;
	bool is_suspended;
	bool input_device_registered;
	char phys[NAME_MAX];
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
};

static inline void cyttsp4_btn_key_action(struct cyttsp4_btn_data *bd,
					  int btn_no, int btn_state)
{
	struct device *dev = &bd->ttsp->dev;
	struct cyttsp4_sysinfo *si = bd->si;

	if (!si->btn[btn_no].enabled || si->btn[btn_no].state == btn_state)
		return;

	si->btn[btn_no].state = btn_state;
	input_report_key(bd->input, si->btn[btn_no].key_code, btn_state);
	input_sync(bd->input);

	tp_log_debug("%s: btn=%d key_code=%d %s\n", __func__,
		     btn_no, si->btn[btn_no].key_code,
		     btn_state == CY_BTN_PRESSED ? "PRESSED" : "RELEASED");
}

static void cyttsp4_get_btn_touches(struct cyttsp4_btn_data *bd)
{
	struct cyttsp4_sysinfo *si = bd->si;
	int num_btn_regs = si->si_ofs.num_btn_regs;
	int num_btns = si->si_ofs.num_btns;
	int cur_reg;
	int cur_reg_val;
	int cur_btn;
	int cur_btn_state;
	int i;

	for (cur_btn = 0, cur_reg = 0; cur_reg < num_btn_regs; cur_reg++) {
		cur_reg_val = si->xy_mode[si->si_ofs.rep_ofs + 2 + cur_reg];

		for (i = 0; i < CY_NUM_BTN_PER_REG && cur_btn < num_btns;
		     i++, cur_btn++) {
			/* Get current button state */
			cur_btn_state = cur_reg_val &
			    ((1 << CY_BITS_PER_BTN) - 1);
			/* Shift reg value for next iteration */
			cur_reg_val >>= CY_BITS_PER_BTN;

			cyttsp4_btn_key_action(bd, cur_btn, cur_btn_state);
		}
	}
}

static void cyttsp4_btn_lift_all(struct cyttsp4_btn_data *bd)
{
	struct cyttsp4_sysinfo *si = bd->si;
	int i;

	if (!si || si->si_ofs.num_btns == 0)
		return;

	for (i = 0; i < si->si_ofs.num_btns; i++)
		cyttsp4_btn_key_action(bd, i, CY_BTN_RELEASED);
}

#ifdef VERBOSE_DEBUG
static void cyttsp4_log_btn_data(struct cyttsp4_btn_data *bd)
{
	struct device *dev = &bd->ttsp->dev;
	struct cyttsp4_sysinfo *si = bd->si;
	int cur;
	int t;

	for (cur = 0; cur < si->si_ofs.num_btns; cur++) {
		bd->pr_buf[0] = 0;
		snprintf(bd->pr_buf, CY_MAX_PRBUF_SIZE, "btn_rec[%d]=0x", cur);
		for (t = 0; t < si->si_ofs.btn_rec_size; t++)
			snprintf(bd->pr_buf, CY_MAX_PRBUF_SIZE, "%s%02X",
				 bd->pr_buf, si->btn_rec_data
				 [(cur * si->si_ofs.btn_rec_size) + t]);

		tp_log_debug("%s: %s\n", __func__, bd->pr_buf);
	}
	return;
}
#endif

/* read xy_data for all current CapSense button touches */
static int cyttsp4_xy_worker(struct cyttsp4_btn_data *bd)
{
	struct device *dev = &bd->ttsp->dev;
	struct cyttsp4_sysinfo *si = bd->si;
	u8 rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
#ifdef VERBOSE_DEBUG
	int rc;
#endif

	/* rep_data for bad packet check */
	if (IS_BAD_PKT(rep_stat)) {
		tp_log_debug("%s: Invalid buffer detected\n", __func__);
		return 0;
	}

	/* extract button press/release touch information */
	if (si->si_ofs.num_btns > 0) {
		cyttsp4_get_btn_touches(bd);
#ifdef VERBOSE_DEBUG
		/* read button diff data */
		rc = cyttsp4_read(bd->ttsp, CY_MODE_OPERATIONAL,
				  si->si_ofs.tt_stat_ofs + 1 +
				  si->si_ofs.max_tchs * si->si_ofs.tch_rec_size,
				  si->btn_rec_data,
				  si->si_ofs.num_btns *
				  si->si_ofs.btn_rec_size);
		if (rc < 0) {
			tp_log_err("%s: read fail on button regs r=%d\n",
				   __func__, rc);
			return 0;
		}

		/* log button press/release touch information */
		cyttsp4_log_btn_data(bd);
#endif
	}

	tp_log_debug("%s: done\n", __func__);

	return 0;
}

static int cyttsp4_btn_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);
	int rc = 0;

	tp_log_debug("%s\n", __func__);

	mutex_lock(&bd->report_lock);
	if (!bd->is_suspended) {
		/* core handles handshake */
		rc = cyttsp4_xy_worker(bd);
	} else {
		tp_log_debug("%s: Ignoring report while suspended\n", __func__);
	}
	mutex_unlock(&bd->report_lock);
	if (rc < 0)
		tp_log_err("%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp4_startup_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&bd->report_lock);
	cyttsp4_btn_lift_all(bd);
	mutex_unlock(&bd->report_lock);

	return 0;
}

static int cyttsp4_btn_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp4_device *ttsp =
	    container_of(dev, struct cyttsp4_device, dev);

	tp_log_debug("%s\n", __func__);
/*change condition for waiting */
	pm_runtime_get(dev);
	tp_log_debug("%s: setup subscriptions\n", __func__);

	/* set up touch call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
				    cyttsp4_btn_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
				    cyttsp4_startup_attention, 0);

	return 0;
}

static void cyttsp4_btn_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp4_device *ttsp =
	    container_of(dev, struct cyttsp4_device, dev);

	tp_log_debug("%s\n", __func__);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
				      cyttsp4_btn_attention,
				      CY_MODE_OPERATIONAL);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_startup_attention, 0);

	pm_runtime_put(dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp4_btn_early_suspend(struct early_suspend *h)
{
	struct cyttsp4_btn_data *bd =
	    container_of(h, struct cyttsp4_btn_data, es);
	struct device *dev = &bd->ttsp->dev;

	tp_log_debug("%s\n", __func__);

#ifndef CONFIG_PM_RUNTIME
	mutex_lock(&bd->report_lock);
	bd->is_suspended = true;
	cyttsp4_btn_lift_all(bd);
	mutex_unlock(&bd->report_lock);
#endif

	pm_runtime_put(dev);
}

static void cyttsp4_btn_late_resume(struct early_suspend *h)
{
	struct cyttsp4_btn_data *bd =
	    container_of(h, struct cyttsp4_btn_data, es);
	struct device *dev = &bd->ttsp->dev;

	tp_log_debug("%s\n", __func__);

#ifndef CONFIG_PM_RUNTIME
	mutex_lock(&bd->report_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->report_lock);
#endif

	pm_runtime_get(dev);
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int cyttsp4_btn_suspend(struct device *dev)
{
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&bd->report_lock);
	bd->is_suspended = true;
	cyttsp4_btn_lift_all(bd);
	mutex_unlock(&bd->report_lock);

	return 0;
}

static int cyttsp4_btn_resume(struct device *dev)
{
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	mutex_lock(&bd->report_lock);
	bd->is_suspended = false;
	mutex_unlock(&bd->report_lock);

	return 0;
}
#endif

static const struct dev_pm_ops cyttsp4_btn_pm_ops = {
	SET_RUNTIME_PM_OPS(cyttsp4_btn_suspend, cyttsp4_btn_resume, NULL)
};

static int cyttsp4_setup_input_device(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);
	int i;
	int rc;

	tp_log_debug("%s: Initialize event signals\n", __func__);
	__set_bit(EV_KEY, bd->input->evbit);
	for (i = 0; i < bd->si->si_ofs.num_btns; i++)
		__set_bit(bd->si->btn[i].key_code, bd->input->keybit);

	rc = input_register_device(bd->input);
	if (rc < 0)
		tp_log_err("%s: Error, failed register input device r=%d\n",
			   __func__, rc);
	else
		bd->input_device_registered = true;

	return rc;
}

static int cyttsp4_setup_input_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);
	int rc;

	tp_log_debug("%s\n", __func__);

	bd->si = cyttsp4_request_sysinfo(ttsp);
	if (!bd->si)
		return -1;

	rc = cyttsp4_setup_input_device(ttsp);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_setup_input_attention, 0);

	return rc;
}

static int cyttsp4_btn_probe(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_btn_data *bd;
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	tp_log_info("%s\n", __func__);
	tp_log_debug("%s: debug on\n", __func__);

/*Increase the judgment conditions*/
	if (pdata == NULL) {
		tp_log_err("%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	bd = kzalloc(sizeof(*bd), GFP_KERNEL);
	if (bd == NULL) {
		tp_log_err("%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	mutex_init(&bd->report_lock);
	bd->ttsp = ttsp;
	bd->pdata = pdata;
	dev_set_drvdata(dev, bd);
	/* Create the input device and register it. */
	tp_log_debug("%s: Create the input device and register it\n", __func__);
	bd->input = input_allocate_device();
	if (bd->input == NULL) {
		tp_log_err("%s: Error, failed to allocate input device\n",
			   __func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	bd->input->name = ttsp->name;
	scnprintf(bd->phys, sizeof(bd->phys) - 1, "%s", dev_name(dev));
	bd->input->phys = bd->phys;
	bd->input->dev.parent = &bd->ttsp->dev;
	bd->input->open = cyttsp4_btn_open;
	bd->input->close = cyttsp4_btn_close;
	input_set_drvdata(bd->input, bd);

	pm_runtime_enable(dev);

	/* get sysinfo */
	bd->si = cyttsp4_request_sysinfo(ttsp);
	if (bd->si) {
		rc = cyttsp4_setup_input_device(ttsp);
		if (rc)
			goto error_init_input;
	} else {
		tp_log_err("%s: Fail get sysinfo pointer from core \n",
			   __func__);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
					    cyttsp4_setup_input_attention, 0);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	bd->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	bd->es.suspend = cyttsp4_btn_early_suspend;
	bd->es.resume = cyttsp4_btn_late_resume;
	register_early_suspend(&bd->es);
#endif

	tp_log_info("%s: ok\n", __func__);
	return 0;

error_init_input:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	input_free_device(bd->input);
error_alloc_failed:
	dev_set_drvdata(dev, NULL);
	kfree(bd);
error_alloc_data_failed:
error_no_pdata:
	tp_log_err("%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_btn_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_btn_data *bd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*
	 * This check is to prevent pm_runtime usage_count drop below zero
	 * because of removing the module while in suspended state
	 */
	if (bd->is_suspended)
		pm_runtime_get_noresume(dev);

	unregister_early_suspend(&bd->es);
#endif

	if (bd->input_device_registered) {
		input_unregister_device(bd->input);
	} else {
		input_free_device(bd->input);
		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
					      cyttsp4_setup_input_attention, 0);
	}

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	dev_set_drvdata(dev, NULL);
	kfree(bd);
	return 0;
}

static struct cyttsp4_driver cyttsp4_btn_driver = {
	.probe = cyttsp4_btn_probe,
	.remove = cyttsp4_btn_release,
	.driver = {
		   .name = CYTTSP4_BTN_NAME,
		   .bus = &cyttsp4_bus_type,
		   .owner = THIS_MODULE,
		   .pm = &cyttsp4_btn_pm_ops,
		   },
};

static int __init cyttsp4_btn_init(void)
{
	int rc = 0;
	rc = cyttsp4_register_driver(&cyttsp4_btn_driver);
	tp_log_info("%s: Cypress TTSP MT v4 CapSense BTN (Built %s), rc=%d\n",
		    __func__, CY_DRIVER_DATE, rc);
	return rc;
}

module_init(cyttsp4_btn_init);

static void __exit cyttsp4_btn_exit(void)
{
	cyttsp4_unregister_driver(&cyttsp4_btn_driver);
	tp_log_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_btn_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TTSP 2D multi-touch CapSense BTN driver");
MODULE_AUTHOR("Cypress Semiconductor");

