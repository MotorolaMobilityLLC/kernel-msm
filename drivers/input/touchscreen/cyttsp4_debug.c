/* add cypress new driver ttda-02.03.01.476713 */
/* add the log dynamic control */

/*
 * cyttsp4_debug.c
 * Cypress TrueTouch(TM) Standard Product V4 Core driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modified by: Cypress Semiconductor to add device functions
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
#include <linux/cyttsp4_core.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "cyttsp4_regs.h"

#define CYTTSP4_DEBUG_NAME "cyttsp4_debug"

enum cyttsp4_monitor_status {
	CY_MNTR_DISABLED,
	CY_MNTR_ENABLED,
};

struct cyttsp4_sensor_monitor {
	enum cyttsp4_monitor_status mntr_status;
	u8 sensor_data[150];	/* operational sensor data */
};

struct cyttsp4_debug_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_debug_platform_data *pdata;
	struct cyttsp4_sysinfo *si;
	uint32_t interrupt_count;
	uint32_t formated_output;
	struct mutex sysfs_lock;
	struct cyttsp4_sensor_monitor monitor;
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
};

struct cyttsp4_debug_platform_data {
	char const *debug_dev_name;
};

/*
 * This function provide output of combined xy_mode and xy_data.
 * Required by TTHE.
 */
static void cyttsp4_pr_buf_op_mode(struct device *dev, u8 * pr_buf,
				   struct cyttsp4_sysinfo *si, u8 cur_touch)
{
	int i, k;
	const char fmt[] = "%02X ";
	int max = (CY_MAX_PRBUF_SIZE - 1) - sizeof(CY_PR_TRUNCATED);
	int total_size = si->si_ofs.mode_size
	    + (cur_touch * si->si_ofs.tch_rec_size);
	u8 num_btns = si->si_ofs.num_btns;

	pr_buf[0] = 0;
	for (i = k = 0; i < si->si_ofs.mode_size && i < max; i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, si->xy_mode[i]);

	for (i = 0; i < (cur_touch * si->si_ofs.tch_rec_size) && i < max;
	     i++, k += 3)
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt, si->xy_data[i]);

	if (num_btns) {
		/* print btn diff data for TTHE */
		scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, "%s", "=");
		k++;
		for (i = 0; i < (num_btns * si->si_ofs.btn_rec_size) && i < max;
		     i++, k += 3)
			scnprintf(pr_buf + k, CY_MAX_PRBUF_SIZE, fmt,
				  si->btn_rec_data[i]);
		total_size += num_btns * si->si_ofs.btn_rec_size + 1;
	}
#if 0				//hauwei 0701
	tp_log_info("%s=%s%s\n", "cyttsp4_OpModeData", pr_buf,
		    total_size <= max ? "" : CY_PR_TRUNCATED);
#endif //huawei 0701
}

static void cyttsp4_debug_print(struct device *dev, u8 * pr_buf, u8 * sptr,
				int size, const char *data_name)
{
	int i, j;
	int elem_size = sizeof("XX ") - 1;
	int max = (CY_MAX_PRBUF_SIZE - 1) / elem_size;
	int limit = size < max ? size : max;

	if (limit < 0)
		limit = 0;

	pr_buf[0] = 0;
	for (i = j = 0; i < limit; i++, j += elem_size)
		scnprintf(pr_buf + j, CY_MAX_PRBUF_SIZE - j, "%02X ", sptr[i]);

	tp_log_info("%s[0..%d]=%s%s\n", data_name, size ? size - 1 : 0, pr_buf,
		    size <= max ? "" : CY_PR_TRUNCATED);
}

static void cyttsp4_debug_formated(struct device *dev, u8 * pr_buf,
				   struct cyttsp4_sysinfo *si, u8 num_cur_rec)
{
	u8 mode_size = si->si_ofs.mode_size;
	u8 rep_len = si->xy_mode[si->si_ofs.rep_ofs];
	u8 tch_rec_size = si->si_ofs.tch_rec_size;
	u8 num_btns = si->si_ofs.num_btns;
	u8 num_btn_regs = (num_btns + CY_NUM_BTN_PER_REG - 1)
	    / CY_NUM_BTN_PER_REG;
	u8 num_btn_tch;
	u8 data_name[] = "touch[99]";
	int max_print_length = 18;
	int i;

	/* xy_mode */
	cyttsp4_debug_print(dev, pr_buf, si->xy_mode, mode_size, "xy_mode");

	/* xy_data */
	if (rep_len > max_print_length) {
		tp_log_info("xy_data[0..%d]:\n", rep_len);
		for (i = 0; i < rep_len - max_print_length;
		     i += max_print_length) {
			cyttsp4_debug_print(dev, pr_buf, si->xy_data + i,
					    max_print_length, " ");
		}
		if (rep_len - i)
			cyttsp4_debug_print(dev, pr_buf, si->xy_data + i,
					    rep_len - i, " ");
	} else {
		cyttsp4_debug_print(dev, pr_buf, si->xy_data,
				    rep_len - si->si_ofs.rep_hdr_size,
				    "xy_data");
	}

	/* touches */
	for (i = 0; i < num_cur_rec; i++) {
		scnprintf(data_name, sizeof(data_name) - 1, "touch[%u]", i);
		cyttsp4_debug_print(dev, pr_buf,
				    si->xy_data + (i * tch_rec_size),
				    tch_rec_size, data_name);
	}

	/* buttons */
	if (num_btns) {
		num_btn_tch = 0;
		for (i = 0; i < num_btn_regs; i++) {
			if (si->xy_mode[si->si_ofs.rep_ofs + 2 + i]) {
				num_btn_tch++;
				break;
			}
		}
		if (num_btn_tch)
			cyttsp4_debug_print(dev, pr_buf,
					    &si->xy_mode[si->si_ofs.rep_ofs +
							 2], num_btn_regs,
					    "button");
	}
}

/* read xy_data for all touches for debug */
static int cyttsp4_xy_worker(struct cyttsp4_debug_data *dd)
{
	struct device *dev = &dd->ttsp->dev;
	struct cyttsp4_sysinfo *si = dd->si;
	u8 tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];
	u8 num_cur_rec = GET_NUM_TOUCH_RECORDS(tt_stat);
	uint32_t formated_output;
	int rc;

	mutex_lock(&dd->sysfs_lock);
	dd->interrupt_count++;
	formated_output = dd->formated_output;
	mutex_unlock(&dd->sysfs_lock);

	/* Read command parameters */
	rc = cyttsp4_read(dd->ttsp, CY_MODE_OPERATIONAL,
			  si->si_ofs.cmd_ofs + 1,
			  &si->xy_mode[si->si_ofs.cmd_ofs + 1],
			  si->si_ofs.rep_ofs - si->si_ofs.cmd_ofs - 1);
	if (rc < 0) {
		tp_log_err("%s: read fail on command parameter regs r=%d\n",
			   __func__, rc);
	}

	if (si->si_ofs.num_btns > 0) {
		/* read button diff data */
		rc = cyttsp4_read(dd->ttsp, CY_MODE_OPERATIONAL,
				  /*  replace with btn_diff_ofs when that field
				   *  becomes supported in the firmware */
				  si->si_ofs.tt_stat_ofs + 1 +
				  si->si_ofs.max_tchs * si->si_ofs.tch_rec_size,
				  si->btn_rec_data,
				  si->si_ofs.num_btns *
				  si->si_ofs.btn_rec_size);
		if (rc < 0) {
			tp_log_err("%s: read fail on button regs r=%d\n",
				   __func__, rc);
		}
	}

	/* Interrupt */
#if 0				//huawei 0701
	tp_log_info("Interrupt(%u)\n", dd->interrupt_count);
#endif //huawei 0701
	if (formated_output)
		cyttsp4_debug_formated(dev, dd->pr_buf, si, num_cur_rec);
	else
		/* print data for TTHE */
		cyttsp4_pr_buf_op_mode(dev, dd->pr_buf, si, num_cur_rec);

	if (dd->monitor.mntr_status == CY_MNTR_ENABLED) {
		int offset = (si->si_ofs.max_tchs * si->si_ofs.tch_rec_size)
		    + (si->si_ofs.num_btns * si->si_ofs.btn_rec_size)
		    + (si->si_ofs.tt_stat_ofs + 1);
		rc = cyttsp4_read(dd->ttsp, CY_MODE_OPERATIONAL,
				  offset, &(dd->monitor.sensor_data[0]), 150);
		if (rc < 0)
			tp_log_err
			    ("%s: read fail on sensor monitor regs r=%d\n",
			     __func__, rc);
		/* print data for the sensor monitor */
		cyttsp4_debug_print(dev, dd->pr_buf, dd->monitor.sensor_data,
				    150, "cyttsp4_sensor_monitor");
	}
#if 0				//huawei 0701
	tp_log_info("\n");
#endif //huawei 0701

	tp_log_debug("%s: done\n", __func__);

	return 0;
}

static int cyttsp4_debug_op_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	int rc = 0;

	tp_log_debug("%s\n", __func__);

	/* core handles handshake */
	rc = cyttsp4_xy_worker(dd);
	if (rc < 0)
		tp_log_err("%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

static int cyttsp4_debug_cat_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	struct cyttsp4_sysinfo *si = dd->si;
	u8 cat_masked_cmd;

	tp_log_debug("%s\n", __func__);

	/* Check for CaT command executed */
	cat_masked_cmd = si->xy_mode[CY_REG_CAT_CMD] & CY_CMD_MASK;
	if (cat_masked_cmd == CY_CMD_CAT_START_SENSOR_DATA_MODE) {
		tp_log_debug("%s: Sensor data mode enabled\n", __func__);
		dd->monitor.mntr_status = CY_MNTR_ENABLED;
	} else if (cat_masked_cmd == CY_CMD_CAT_STOP_SENSOR_DATA_MODE) {
		tp_log_debug("%s: Sensor data mode disabled\n", __func__);
		dd->monitor.mntr_status = CY_MNTR_DISABLED;
	}

	return 0;
}

static int cyttsp4_debug_startup_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	dd->monitor.mntr_status = CY_MNTR_DISABLED;

	return 0;
}

static ssize_t cyttsp4_interrupt_count_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	int val;

	mutex_lock(&dd->sysfs_lock);
	val = dd->interrupt_count;
	mutex_unlock(&dd->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE, "Interrupt Count: %d\n", val);
}

static ssize_t cyttsp4_interrupt_count_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	mutex_lock(&dd->sysfs_lock);
	dd->interrupt_count = 0;
	mutex_unlock(&dd->sysfs_lock);
	return size;
}

static DEVICE_ATTR(int_count, S_IRUSR | S_IWUSR,
		   cyttsp4_interrupt_count_show, cyttsp4_interrupt_count_store);

static ssize_t cyttsp4_formated_output_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	int val;

	mutex_lock(&dd->sysfs_lock);
	val = dd->formated_output;
	mutex_unlock(&dd->sysfs_lock);

	return scnprintf(buf, CY_MAX_PRBUF_SIZE,
			 "Formated debug output: %x\n", val);
}

static ssize_t cyttsp4_formated_output_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, 10, &value);
	if (rc < 0) {
		tp_log_err("%s: Invalid value\n", __func__);
		return size;
	}

	/* Expecting only 0 or 1 */
	if (value != 0 && value != 1) {
		tp_log_err("%s: Invalid value %lu\n", __func__, value);
		return size;
	}

	mutex_lock(&dd->sysfs_lock);
	dd->formated_output = value;
	mutex_unlock(&dd->sysfs_lock);
	return size;
}

static DEVICE_ATTR(formated_output, S_IRUSR | S_IWUSR,
		   cyttsp4_formated_output_show, cyttsp4_formated_output_store);

static int cyttsp4_debug_probe(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_debug_data *dd;
	struct cyttsp4_debug_platform_data *pdata = dev_get_platdata(dev);
	int rc;

	tp_log_info("%s: startup\n", __func__);
	tp_log_debug("%s: debug on\n", __func__);
	tp_log_debug("%s: verbose debug on\n", __func__);

	/* get context and debug print buffers */
	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (dd == NULL) {
		tp_log_err("%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto cyttsp4_debug_probe_alloc_failed;
	}

	rc = device_create_file(dev, &dev_attr_int_count);
	if (rc) {
		tp_log_err("%s: Error, could not create int_count\n", __func__);
		goto cyttsp4_debug_probe_create_int_count_failed;
	}

	rc = device_create_file(dev, &dev_attr_formated_output);
	if (rc) {
		tp_log_err("%s: Error, could not create formated_output\n",
			   __func__);
		goto cyttsp4_debug_probe_create_formated_failed;
	}

	mutex_init(&dd->sysfs_lock);
	dd->ttsp = ttsp;
	dd->pdata = pdata;
	dev_set_drvdata(dev, dd);

	pm_runtime_enable(dev);

	dd->si = cyttsp4_request_sysinfo(ttsp);
	if (dd->si == NULL) {
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);
		rc = -ENODEV;
		goto cyttsp4_debug_probe_sysinfo_failed;
	}

	rc = cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
					 cyttsp4_debug_op_attention,
					 CY_MODE_OPERATIONAL);
	if (rc < 0) {
		tp_log_err
		    ("%s: Error, could not subscribe Operating mode attention cb\n",
		     __func__);
		goto cyttsp4_debug_probe_subscribe_op_failed;
	}

	rc = cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
					 cyttsp4_debug_cat_attention,
					 CY_MODE_CAT);
	if (rc < 0) {
		tp_log_err
		    ("%s: Error, could not subscribe CaT mode attention cb\n",
		     __func__);
		goto cyttsp4_debug_probe_subscribe_cat_failed;
	}

	rc = cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
					 cyttsp4_debug_startup_attention, 0);
	if (rc < 0) {
		tp_log_err
		    ("%s: Error, could not subscribe startup attention cb\n",
		     __func__);
		goto cyttsp4_debug_probe_subscribe_startup_failed;
	}
	return 0;

cyttsp4_debug_probe_subscribe_startup_failed:
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
				      cyttsp4_debug_cat_attention, CY_MODE_CAT);
cyttsp4_debug_probe_subscribe_cat_failed:
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
				      cyttsp4_debug_op_attention,
				      CY_MODE_OPERATIONAL);
cyttsp4_debug_probe_subscribe_op_failed:
cyttsp4_debug_probe_sysinfo_failed:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	dev_set_drvdata(dev, NULL);
	device_remove_file(dev, &dev_attr_formated_output);
cyttsp4_debug_probe_create_formated_failed:
	device_remove_file(dev, &dev_attr_int_count);
cyttsp4_debug_probe_create_int_count_failed:
	kfree(dd);
cyttsp4_debug_probe_alloc_failed:
	tp_log_err("%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_debug_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_debug_data *dd = dev_get_drvdata(dev);
	int rc = 0;
	tp_log_debug("%s\n", __func__);

	if (dev_get_drvdata(&ttsp->core->dev) == NULL) {
		tp_log_err("%s: Unable to un-subscribe attention\n", __func__);
		goto cyttsp4_debug_release_exit;
	}

	/* Unsubscribe from attentions */
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
				      cyttsp4_debug_op_attention,
				      CY_MODE_OPERATIONAL);
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
				      cyttsp4_debug_cat_attention, CY_MODE_CAT);
	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_debug_startup_attention, 0);

cyttsp4_debug_release_exit:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	device_remove_file(dev, &dev_attr_int_count);
	dev_set_drvdata(dev, NULL);
	kfree(dd);

	return rc;
}

static struct cyttsp4_driver cyttsp4_debug_driver = {
	.probe = cyttsp4_debug_probe,
	.remove = cyttsp4_debug_release,
	.driver = {
		   .name = CYTTSP4_DEBUG_NAME,
		   .bus = &cyttsp4_bus_type,
		   .owner = THIS_MODULE,
		   },
};

static struct cyttsp4_debug_platform_data _cyttsp4_debug_platform_data = {
	.debug_dev_name = CYTTSP4_DEBUG_NAME,
};

static const char cyttsp4_debug_name[] = CYTTSP4_DEBUG_NAME;
static struct cyttsp4_device_info cyttsp4_debug_infos[CY_MAX_NUM_CORE_DEVS];

static char *core_ids[CY_MAX_NUM_CORE_DEVS] = {
	CY_DEFAULT_CORE_ID,
	NULL,
	NULL,
	NULL,
	NULL
};

static int num_core_ids = 1;

module_param_array(core_ids, charp, &num_core_ids, 0);
MODULE_PARM_DESC(core_ids,
		 "Core id list of cyttsp4 core devices for debug module");

static int __init cyttsp4_debug_init(void)
{
	int rc = 0;
	int i, j;

	/* Check for invalid or duplicate core_ids */
	for (i = 0; i < num_core_ids; i++) {
		if (!strlen(core_ids[i])) {
			tp_log_err("%s: core_id %d is empty\n",
				   __func__, i + 1);
			return -EINVAL;
		}
		for (j = i + 1; j < num_core_ids; j++)
			if (!strcmp(core_ids[i], core_ids[j])) {
				tp_log_err("%s: core_ids %d and %d are same\n",
					   __func__, i + 1, j + 1);
				return -EINVAL;
			}
	}

	for (i = 0; i < num_core_ids; i++) {
		cyttsp4_debug_infos[i].name = cyttsp4_debug_name;
		cyttsp4_debug_infos[i].core_id = core_ids[i];
		cyttsp4_debug_infos[i].platform_data =
		    &_cyttsp4_debug_platform_data;
		tp_log_debug("%s: Registering debug device for core_id: %s\n",
			     __func__, cyttsp4_debug_infos[i].core_id);
		rc = cyttsp4_register_device(&cyttsp4_debug_infos[i]);
		if (rc < 0) {
			tp_log_err("%s: Error, failed registering device\n",
				   __func__);
			goto fail_unregister_devices;
		}
	}
	rc = cyttsp4_register_driver(&cyttsp4_debug_driver);
	if (rc) {
		tp_log_err("%s: Error, failed registering driver\n", __func__);
		goto fail_unregister_devices;
	}

	tp_log_info("%s: Cypress TTSP Debug (Built %s) rc=%d\n",
		    __func__, CY_DRIVER_DATE, rc);
	return 0;

fail_unregister_devices:
	for (i--; i >= 0; i--) {
		cyttsp4_unregister_device(cyttsp4_debug_infos[i].name,
					  cyttsp4_debug_infos[i].core_id);
		tp_log_info
		    ("%s: Unregistering device access device for core_id: %s\n",
		     __func__, cyttsp4_debug_infos[i].core_id);
	}
	return rc;
}

module_init(cyttsp4_debug_init);

static void __exit cyttsp4_debug_exit(void)
{
	int i;

	cyttsp4_unregister_driver(&cyttsp4_debug_driver);
	for (i = 0; i < num_core_ids; i++) {
		cyttsp4_unregister_device(cyttsp4_debug_infos[i].name,
					  cyttsp4_debug_infos[i].core_id);
		tp_log_info("%s: Unregistering debug device for core_id: %s\n",
			    __func__, cyttsp4_debug_infos[i].core_id);
	}
	tp_log_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_debug_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen debug driver");
MODULE_AUTHOR("Cypress Semiconductor");
