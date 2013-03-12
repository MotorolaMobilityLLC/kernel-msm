/*
 * Copyright (C) 2010-2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/* Driver for Atmel maXTouch touchscreens that uses tdat files */
#include "atmxt.h"
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/firmware.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define FAMILY_ID 0x82

static int atmxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int atmxt_remove(struct i2c_client *client);
static int atmxt_suspend(struct i2c_client *client, pm_message_t message);
static int atmxt_resume(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void atmxt_early_suspend(struct early_suspend *handler);
static void atmxt_late_resume(struct early_suspend *handler);
#endif
static int __devinit atmxt_init(void);
static void __devexit atmxt_exit(void);
static void atmxt_free(struct atmxt_driver_data *dd);
static void atmxt_free_ic_data(struct atmxt_driver_data *dd);
static void atmxt_set_drv_state(struct atmxt_driver_data *dd,
		enum atmxt_driver_state state);
static int atmxt_get_drv_state(struct atmxt_driver_data *dd);
static void atmxt_set_ic_state(struct atmxt_driver_data *dd,
		enum atmxt_ic_state state);
static int atmxt_get_ic_state(struct atmxt_driver_data *dd);
static int atmxt_verify_pdata(struct atmxt_driver_data *dd);
static int atmxt_request_tdat(struct atmxt_driver_data *dd);
static void atmxt_tdat_callback(const struct firmware *tdat, void *context);
static int atmxt_validate_tdat(const struct firmware *tdat);
static int atmxt_validate_settings(uint8_t *data, uint32_t size);
static int atmxt_gpio_init(struct atmxt_driver_data *dd);
static int atmxt_register_inputs(struct atmxt_driver_data *dd,
		uint8_t *rdat, int rsize);
static int atmxt_request_irq(struct atmxt_driver_data *dd);
static int atmxt_restart_ic(struct atmxt_driver_data *dd);
static irqreturn_t atmxt_isr(int irq, void *handle);
static int atmxt_get_info_header(struct atmxt_driver_data *dd);
static int atmxt_get_object_table(struct atmxt_driver_data *dd);
static int atmxt_process_object_table(struct atmxt_driver_data *dd);
static uint8_t *atmxt_get_settings_entry(struct atmxt_driver_data *dd,
		uint16_t num);
static int atmxt_copy_platform_data(uint8_t *reg, uint8_t *entry,
		uint8_t *tsett);
static int atmxt_check_settings(struct atmxt_driver_data *dd, bool *reset);
static int atmxt_send_settings(struct atmxt_driver_data *dd, bool save_nvm);
static int atmxt_recalibrate_ic(struct atmxt_driver_data *dd);
static int atmxt_start_ic_calibration_fix(struct atmxt_driver_data *dd);
static int atmxt_verify_ic_calibration_fix(struct atmxt_driver_data *dd);
static int atmxt_stop_ic_calibration_fix(struct atmxt_driver_data *dd);
static int atmxt_i2c_write(struct atmxt_driver_data *dd,
		uint8_t addr_lo, uint8_t addr_hi, uint8_t *buf, int size);
static int atmxt_i2c_read(struct atmxt_driver_data *dd, uint8_t *buf, int size);
static int atmxt_save_internal_data(struct atmxt_driver_data *dd);
static int atmxt_save_data5(struct atmxt_driver_data *dd, uint8_t *entry);
static int atmxt_save_data6(struct atmxt_driver_data *dd, uint8_t *entry);
static int atmxt_save_data7(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg);
static int atmxt_save_data8(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg);
static int atmxt_save_data9(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg);
static void atmxt_compute_checksum(struct atmxt_driver_data *dd);
static void atmxt_compute_partial_checksum(uint8_t *byte1, uint8_t *byte2,
		uint8_t *low, uint8_t *mid, uint8_t *high);
static void atmxt_active_handler(struct atmxt_driver_data *dd);
static int atmxt_process_message(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size);
static void atmxt_report_touches(struct atmxt_driver_data *dd);
static void atmxt_release_touches(struct atmxt_driver_data *dd);
static int atmxt_message_handler6(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size);
static int atmxt_message_handler9(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size);
static int atmxt_message_handler42(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size);
static int atmxt_resume_restart(struct atmxt_driver_data *dd);
static int atmxt_force_bootloader(struct atmxt_driver_data *dd);
static bool atmxt_check_firmware_update(struct atmxt_driver_data *dd);
static int atmxt_validate_firmware(uint8_t *data, uint32_t size);
static int atmxt_flash_firmware(struct atmxt_driver_data *dd);
static char *atmxt_msg2str(const uint8_t *msg, uint8_t size);
static bool atmxt_wait4irq(struct atmxt_driver_data *dd);
static int atmxt_create_sysfs_files(struct atmxt_driver_data *dd);
static void atmxt_remove_sysfs_files(struct atmxt_driver_data *dd);

static const struct i2c_device_id atmxt_id[] = {
	/* This name must match the i2c_board_info name */
	{ ATMXT_I2C_NAME, 0 }, { }
};

MODULE_DEVICE_TABLE(i2c, atmxt_id);

#ifdef CONFIG_OF
static struct of_device_id atmxt_match_tbl[] = {
	{ .compatible = "atmel,atmxt-ts" },
	{ },
};
MODULE_DEVICE_TABLE(of, atmxt_match_tbl);
#endif

static struct i2c_driver atmxt_driver = {
	.driver = {
		.name = ATMXT_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(atmxt_match_tbl),
	},
	.probe = atmxt_probe,
	.remove = __devexit_p(atmxt_remove),
	.id_table = atmxt_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = atmxt_suspend,
	.resume = atmxt_resume,
#endif
};

#ifdef CONFIG_OF
static struct touch_platform_data *
atmxt_of_init(struct i2c_client *client)
{
	struct touch_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	const char *fp = NULL;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	of_property_read_string(np, "atmel,atmxt-tdat-filename", &fp);

	pdata->filename = (char *)fp;
	pdata->gpio_interrupt = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);
	pdata->gpio_enable = of_get_gpio(np, 2);

	return pdata;
}
#else
static inline struct touch_platform_data *
atmxt_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int atmxt_probe_ic(struct atmxt_driver_data *dd)
{
	uint8_t infoblk[7];
	int err = 0;
	bool irq_low = false;

	atmxt_dbg(dd, ATMXT_DBG2, "%s: Probing touch IC...\n", __func__);
	gpio_set_value(dd->pdata->gpio_reset, 0);
	udelay(ATMXT_IC_RESET_HOLD_TIME);
	gpio_set_value(dd->pdata->gpio_reset, 1);
	irq_low = atmxt_wait4irq(dd);
	if (!irq_low) {
		printk(KERN_ERR "%s: Timeout waiting for interrupt.\n",
			__func__);
		return -ETIME;
	}

	err = atmxt_i2c_write(dd, 0x00, 0x00, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: failed to set addr\n", __func__);
		return err;
	}

	err = atmxt_i2c_read(dd, infoblk, sizeof(infoblk));
	if (err < 0) {
		printk(KERN_ERR "%s: failed to read\n",	__func__);
		return err;
	}

	if (infoblk[0] != FAMILY_ID) {
		printk(KERN_ERR "%s: Family ID mismatch:"
			" expected 0x%02x actual is 0x%02x\n",
			__func__, FAMILY_ID, infoblk[0]);
		return -EIO;
	}

	return 0;
}

static int atmxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct atmxt_driver_data *dd = NULL;
	int err = 0;
	bool debugfail = false;

	printk(KERN_INFO "%s: Driver: %s, Version: %s, Date: %s\n", __func__,
		ATMXT_I2C_NAME, ATMXT_DRIVER_VERSION, ATMXT_DRIVER_DATE);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: Missing I2C adapter support.\n", __func__);
		err = -ENODEV;
		goto atmxt_probe_fail;
	}

	dd = kzalloc(sizeof(struct atmxt_driver_data), GFP_KERNEL);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Unable to create driver data.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_probe_fail;
	}

	dd->drv_stat = ATMXT_DRV_INIT;
	dd->ic_stat = ATMXT_IC_UNKNOWN;
	dd->status = 0x0000;
	dd->client = client;

	if (client->dev.of_node)
		dd->pdata = atmxt_of_init(client);
	else
		dd->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, dd);
	dd->in_dev = NULL;

	dd->mutex = kzalloc(sizeof(struct mutex), GFP_KERNEL);
	if (dd->mutex == NULL) {
		printk(KERN_ERR "%s: Unable to create mutex lock.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_probe_fail;
	}
	mutex_init(dd->mutex);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	dd->dbg = kzalloc(sizeof(struct atmxt_debug), GFP_KERNEL);
	if (dd->dbg == NULL) {
		printk(KERN_ERR "%s: Unable to create driver debug data.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_probe_fail;
	}
	dd->dbg->dbg_lvl = ATMXT_DBG0;
#endif

	dd->util = kzalloc(sizeof(struct atmxt_util_data), GFP_KERNEL);
	if (dd->util == NULL) {
		printk(KERN_ERR "%s: Unable to create touch utility data.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_probe_fail;
	}

	err = atmxt_verify_pdata(dd);
	if (err < 0)
		goto atmxt_probe_fail;

	err = atmxt_gpio_init(dd);
	if (err < 0)
		goto atmxt_probe_fail;

#ifdef CONFIG_HAS_EARLYSUSPEND
	dd->es.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	dd->es.suspend = atmxt_early_suspend;
	dd->es.resume = atmxt_late_resume;
	register_early_suspend(&dd->es);
#endif

	err = atmxt_request_irq(dd);
	if (err < 0)
		goto atmxt_unreg_suspend;

	err = atmxt_probe_ic(dd);
	if (err < 0)
		goto atmxt_free_irq;

	err = atmxt_request_tdat(dd);
	if (err < 0)
		goto atmxt_free_irq;

	err = atmxt_create_sysfs_files(dd);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Probe had error %d when creating sysfs files.\n",
			__func__, err);
		debugfail = true;
	}

	goto atmxt_probe_pass;

atmxt_free_irq:
	free_irq(dd->client->irq, dd);
atmxt_unreg_suspend:
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&dd->es);
#endif
	gpio_free(dd->pdata->gpio_reset);
	gpio_free(dd->pdata->gpio_interrupt);
atmxt_probe_fail:
	atmxt_free(dd);
	printk(KERN_ERR "%s: Probe failed with error code %d.\n",
		__func__, err);
	return err;

atmxt_probe_pass:
	if (debugfail) {
		printk(KERN_INFO "%s: Probe completed with errors.\n",
			__func__);
	} else {
		printk(KERN_INFO "%s: Probe successful.\n", __func__);
	}
	return 0;
}

static int atmxt_remove(struct i2c_client *client)
{
	struct atmxt_driver_data *dd = NULL;

	dd = i2c_get_clientdata(client);
	if (dd != NULL) {
		free_irq(dd->client->irq, dd);
		atmxt_remove_sysfs_files(dd);
		gpio_free(dd->pdata->gpio_reset);
		gpio_free(dd->pdata->gpio_interrupt);
		unregister_early_suspend(&dd->es);
		atmxt_free(dd);
	}

	i2c_set_clientdata(client, NULL);

	return 0;
}

static int atmxt_suspend(struct i2c_client *client, pm_message_t message)
{
	int err = 0;
	struct atmxt_driver_data *dd;
	int drv_state;
	int ic_state;
	uint8_t sleep_cmd[2] = {0x00, 0x00};

	dd = i2c_get_clientdata(client);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Driver data is missing.\n", __func__);
		err = -ENODATA;
		goto atmxt_suspend_no_dd_fail;
	}

	mutex_lock(dd->mutex);

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Suspending...\n", __func__);

	drv_state = atmxt_get_drv_state(dd);
	ic_state = atmxt_get_ic_state(dd);

	switch (drv_state) {
	case ATMXT_DRV_ACTIVE:
	case ATMXT_DRV_IDLE:
		switch (ic_state) {
		case ATMXT_IC_ACTIVE:
			atmxt_dbg(dd, ATMXT_DBG3,
				"%s: Putting touch IC to sleep...\n", __func__);
			dd->status = dd->status &
				~(1 << ATMXT_FIXING_CALIBRATION);
			err = atmxt_i2c_write(dd,
				dd->addr->pwr[0], dd->addr->pwr[1],
				&(sleep_cmd[0]), 2);
			if (err < 0) {
				printk(KERN_ERR
					"%s: %s %s %d.\n", __func__,
					"Failed to put touch IC to sleep",
					"with error code", err);
				goto atmxt_suspend_fail;
			} else {
				atmxt_set_ic_state(dd, ATMXT_IC_SLEEP);
			}
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s suspend.\n",
				__func__, atmxt_driver_state_string[drv_state],
				atmxt_ic_state_string[ic_state]);
		}
		break;

	default:
		printk(KERN_ERR "%s: Driver state \"%s\" suspend.\n",
			__func__, atmxt_driver_state_string[drv_state]);
	}

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Suspend complete.\n", __func__);

atmxt_suspend_fail:
	mutex_unlock(dd->mutex);

atmxt_suspend_no_dd_fail:
	return err;
}

static int atmxt_resume(struct i2c_client *client)
{
	int err = 0;
	struct atmxt_driver_data *dd;
	int drv_state;
	int ic_state;

	dd = i2c_get_clientdata(client);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Driver data is missing.\n", __func__);
		err = -ENODATA;
		goto atmxt_resume_no_dd_fail;
	}

	mutex_lock(dd->mutex);

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Resuming...\n", __func__);

	drv_state = atmxt_get_drv_state(dd);
	ic_state = atmxt_get_ic_state(dd);

	switch (drv_state) {
	case ATMXT_DRV_ACTIVE:
	case ATMXT_DRV_IDLE:
		switch (ic_state) {
		case ATMXT_IC_ACTIVE:
			printk(KERN_ERR "%s: Driver %s, IC %s resume.\n",
				__func__, atmxt_driver_state_string[drv_state],
				atmxt_ic_state_string[ic_state]);
			break;
		case ATMXT_IC_SLEEP:
			atmxt_dbg(dd, ATMXT_DBG3,
				"%s: Waking touch IC...\n", __func__);
			err = atmxt_i2c_write(dd,
				dd->addr->pwr[0], dd->addr->pwr[1],
				&(dd->data->pwr[0]), 2);
			if (err < 0) {
				printk(KERN_ERR
					"%s: Failed to wake touch IC %s %d.\n",
					__func__, "with error code", err);
				err = atmxt_resume_restart(dd);
				if (err < 0) {
					printk(KERN_ERR
						"%s: %s %s %d.\n",
						__func__,
						"Failed restart after resume",
						"with error code", err);
				}
				goto atmxt_resume_fail;
			} else {
				atmxt_set_ic_state(dd, ATMXT_IC_ACTIVE);
			}
			err = atmxt_start_ic_calibration_fix(dd);
			if (err < 0) {
				printk(KERN_ERR "%s: %s %s %d.\n", __func__,
					"Failed to start calibration fix",
					"with error code", err);
				goto atmxt_resume_fail;
			}
			err = atmxt_recalibrate_ic(dd);
			if (err < 0) {
				printk(KERN_ERR
					"%s: Recalibration failed %s %d.\n",
					__func__, "with error code", err);
				goto atmxt_resume_fail;
			}
			atmxt_release_touches(dd);
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s resume--%s...\n",
				__func__, atmxt_driver_state_string[drv_state],
				atmxt_ic_state_string[ic_state], "recovering");
			err = atmxt_resume_restart(dd);
			if (err < 0) {
				printk(KERN_ERR "%s: Recovery failed %s %d.\n",
					__func__, "with error code", err);
				goto atmxt_resume_fail;
			}
		}
		break;

	case ATMXT_DRV_INIT:
		printk(KERN_ERR "%s: Driver state \"%s\" resume.\n",
			__func__, atmxt_driver_state_string[drv_state]);
		break;

	default:
		printk(KERN_ERR "%s: Driver %s, IC %s resume--%s...\n",
			__func__, atmxt_driver_state_string[drv_state],
			atmxt_ic_state_string[ic_state], "recovering");
		err = atmxt_resume_restart(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Recovery failed %s %d.\n",
				__func__, "with error code", err);
			goto atmxt_resume_fail;
		}
		break;
	}

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Resume complete.\n", __func__);

atmxt_resume_fail:
	mutex_unlock(dd->mutex);

atmxt_resume_no_dd_fail:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void atmxt_early_suspend(struct early_suspend *handler)
{
	int err = 0;
	struct atmxt_driver_data *dd;

	dd = container_of(handler, struct atmxt_driver_data, es);

	err = atmxt_suspend(dd->client, PMSG_SUSPEND);
	if (err < 0) {
		printk(KERN_ERR "%s: Suspend failed with error code %d",
			__func__, err);
	}

	return;
}
static void atmxt_late_resume(struct early_suspend *handler)
{
	int err = 0;
	struct atmxt_driver_data *dd;

	dd = container_of(handler, struct atmxt_driver_data, es);

	err = atmxt_resume(dd->client);
	if (err < 0) {
		printk(KERN_ERR "%s: Resume failed with error code %d",
			__func__, err);
	}

	return;
}
#endif

static int __devinit atmxt_init(void)
{
	return i2c_add_driver(&atmxt_driver);
}

static void __devexit atmxt_exit(void)
{
	i2c_del_driver(&atmxt_driver);
	return;
}

module_init(atmxt_init);
module_exit(atmxt_exit);

static void atmxt_free(struct atmxt_driver_data *dd)
{
	if (dd != NULL) {
		dd->pdata = NULL;
		dd->client = NULL;

		if (dd->mutex != NULL) {
			kfree(dd->mutex);
			dd->mutex = NULL;
		}

		if (dd->util != NULL) {
			kfree(dd->util->data);
			dd->util->data = NULL;
			kfree(dd->util);
			dd->util = NULL;
		}

		if (dd->in_dev != NULL) {
			input_unregister_device(dd->in_dev);
			dd->in_dev = NULL;
		}

		atmxt_free_ic_data(dd);

		if (dd->rdat != NULL) {
			kfree(dd->rdat);
			dd->rdat = NULL;
		}

		if (dd->dbg != NULL) {
			kfree(dd->dbg);
			dd->dbg = NULL;
		}

		kfree(dd);
		dd = NULL;
	}

	return;
}

static void atmxt_free_ic_data(struct atmxt_driver_data *dd)
{
	if (dd->info_blk != NULL) {
		kfree(dd->info_blk->data);
		dd->info_blk->data = NULL;
		kfree(dd->info_blk->msg_id);
		dd->info_blk->msg_id = NULL;
		kfree(dd->info_blk);
		dd->info_blk = NULL;
	}

	if (dd->nvm != NULL) {
		kfree(dd->nvm->data);
		dd->nvm->data = NULL;
		kfree(dd->nvm);
		dd->nvm = NULL;
	}

	if (dd->addr != NULL) {
		kfree(dd->addr);
		dd->addr = NULL;
	}

	if (dd->data != NULL) {
		kfree(dd->data);
		dd->data = NULL;
	}

	return;
}

static void atmxt_set_drv_state(struct atmxt_driver_data *dd,
		enum atmxt_driver_state state)
{
	printk(KERN_INFO "%s: Driver state %s -> %s\n", __func__,
		atmxt_driver_state_string[dd->drv_stat],
		atmxt_driver_state_string[state]);
	dd->drv_stat = state;
	return;
}

static int atmxt_get_drv_state(struct atmxt_driver_data *dd)
{
	return dd->drv_stat;
}

static void atmxt_set_ic_state(struct atmxt_driver_data *dd,
		enum atmxt_ic_state state)
{
	printk(KERN_INFO "%s: IC state %s -> %s\n", __func__,
		atmxt_ic_state_string[dd->ic_stat],
		atmxt_ic_state_string[state]);
	dd->ic_stat = state;
	return;
}

static int atmxt_get_ic_state(struct atmxt_driver_data *dd)
{
	return dd->ic_stat;
}

static int atmxt_verify_pdata(struct atmxt_driver_data *dd)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Verifying platform data...\n", __func__);

	if (dd->pdata == NULL) {
		printk(KERN_ERR "%s: Platform data is missing.\n", __func__);
		err = -ENODATA;
		goto atmxt_verify_pdata_fail;
	}

	if (dd->pdata->gpio_reset == 0) {
		printk(KERN_ERR "%s: Reset GPIO is invalid.\n", __func__);
		err = -EINVAL;
		goto atmxt_verify_pdata_fail;
	}

	if (dd->pdata->gpio_interrupt == 0) {
		printk(KERN_ERR "%s: Interrupt GPIO is invalid.\n", __func__);
		err = -EINVAL;
		goto atmxt_verify_pdata_fail;
	}

	if (dd->pdata->filename == NULL) {
		printk(KERN_ERR "%s: Touch data filename is missing.\n",
			__func__);
		err = -ENODATA;
		goto atmxt_verify_pdata_fail;
	}

atmxt_verify_pdata_fail:
	return err;
}

static int atmxt_request_tdat(struct atmxt_driver_data *dd)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Requesting tdat data...\n", __func__);

	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG, dd->pdata->filename, &(dd->client->dev),
		GFP_KERNEL, dd, atmxt_tdat_callback);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to schedule tdat request.\n",
			__func__);
		goto atmxt_request_tdat_fail;
	}

atmxt_request_tdat_fail:
	return err;
}

static void atmxt_tdat_callback(const struct firmware *tdat, void *context)
{
	int err = 0;
	struct atmxt_driver_data *dd = context;
	bool icfail = false;
	uint8_t cur_id = 0x00;
	uint32_t cur_size = 0;
	uint8_t *cur_data = NULL;
	size_t loc = 0;

	mutex_lock(dd->mutex);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (dd->status & (1 << ATMXT_WAITING_FOR_TDAT)) {
		printk(KERN_INFO "%s: Processing new tdat file...\n", __func__);
		dd->status = dd->status & ~(1 << ATMXT_WAITING_FOR_TDAT);
	} else {
		printk(KERN_INFO "%s: Processing %s...\n", __func__,
			dd->pdata->filename);
	}
#else
	printk(KERN_INFO "%s: Processing %s...\n", __func__,
		dd->pdata->filename);
#endif

	if (tdat == NULL) {
		printk(KERN_ERR "%s: No data received.\n", __func__);
		err = -ENODATA;
		goto atmxt_tdat_callback_fail;
	}

	err = atmxt_validate_tdat(tdat);
	if (err < 0)
		goto atmxt_tdat_callback_fail;

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (atmxt_get_drv_state(dd) != ATMXT_DRV_INIT) {
		if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			dd->status = dd->status &
				~(1 << ATMXT_IRQ_ENABLED_FLAG);
		}
		atmxt_set_drv_state(dd, ATMXT_DRV_INIT);
	}

	if (dd->util->data != NULL) {
		kfree(dd->util->data);
		dd->util->data = NULL;
		dd->util->size = 0;
		dd->util->tsett = NULL;
		dd->util->tsett_size = 0;
		dd->util->fw = NULL;
		dd->util->fw_size = 0;
		dd->util->addr[0] = 0x00;
		dd->util->addr[1] = 0x00;
	}
#endif

	dd->util->data = kzalloc(tdat->size * sizeof(uint8_t), GFP_KERNEL);
	if (dd->util->data == NULL) {
		printk(KERN_ERR "%s: Unable to copy tdat.\n", __func__);
		err = -ENOMEM;
		goto atmxt_tdat_callback_fail;
	}
	memcpy(dd->util->data, tdat->data, tdat->size);
	dd->util->size = tdat->size;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: MCV is 0x%02X.\n", __func__,
		dd->util->data[loc]);
	loc++;

	while (loc < dd->util->size) {
		cur_id = dd->util->data[loc];
		cur_size = (dd->util->data[loc+3] << 16) |
			(dd->util->data[loc+2] << 8) |
			dd->util->data[loc+1];
		cur_data = &(dd->util->data[loc+4]);

		switch (cur_id) {
		case 0x00:
			break;

		case 0x01:
			dd->util->tsett = cur_data;
			dd->util->tsett_size = cur_size;
			break;

		case 0x02:
			dd->util->fw = cur_data;
			dd->util->fw_size = cur_size;
			break;

		case 0x03:
			if (cur_size == 0 || cur_size % 10 != 0) {
				printk(KERN_ERR
					"%s: Abs data format is invalid.\n",
					__func__);
				err = -EINVAL;
				goto atmxt_tdat_callback_fail;
			}

			err = atmxt_register_inputs(dd, cur_data, cur_size);
			if (err < 0)
				goto atmxt_tdat_callback_fail;
			break;

		case 0x04:
			if (cur_size < 4) {
				printk(KERN_ERR
					"%s: Driver data is too small.\n",
					__func__);
				err = -EINVAL;
				goto atmxt_tdat_callback_fail;
			}
			dd->settings = (cur_data[1] << 8) | cur_data[0];
			dd->util->addr[0] = cur_data[2];
			dd->util->addr[1] = cur_data[3];
			dd->client->addr = dd->util->addr[0];
			break;

		default:
			printk(KERN_ERR "%s: Record %hu found but not used.\n",
				__func__, cur_id);
			break;
		}

		loc = loc + cur_size + 4;
	}

	err = atmxt_validate_settings(dd->util->tsett, dd->util->tsett_size);
	if (err < 0)
		goto atmxt_tdat_callback_fail;

	err = atmxt_validate_firmware(dd->util->fw, dd->util->fw_size);
	if (err < 0)
		goto atmxt_tdat_callback_fail;

	err = atmxt_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Restarting IC failed with error code %d.\n",
				__func__, err);
		icfail = true;
	}

	if (!icfail) {
		atmxt_set_drv_state(dd, ATMXT_DRV_ACTIVE);
		dd->status = dd->status | (1 << ATMXT_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
		printk(KERN_INFO "%s: Touch initialization successful.\n",
			__func__);
	} else {
		atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
		printk(KERN_INFO
			"%s: Touch initialization completed with errors.\n",
			__func__);
	}

	goto atmxt_tdat_callback_exit;

atmxt_tdat_callback_fail:
	printk(KERN_ERR "%s: Touch initialization failed with error code %d.\n",
		__func__, err);

atmxt_tdat_callback_exit:
	release_firmware(tdat);
	mutex_unlock(dd->mutex);

	return;
}

static int atmxt_validate_tdat(const struct firmware *tdat)
{
	int err = 0;
	int length = 0;
	size_t loc = 0;

	if (tdat->data == NULL || tdat->size == 0) {
		printk(KERN_ERR "%s: No data found.\n", __func__);
		err = -ENODATA;
		goto atmxt_validate_tdat_fail;
	}

	if (tdat->data[loc] != 0x31) {
		printk(KERN_ERR "%s: MCV 0x%02X is not supported.\n",
			__func__, tdat->data[loc]);
		err = -EINVAL;
		goto atmxt_validate_tdat_fail;
	}
	loc++;

	while (loc < (tdat->size - 3)) {
		length = (tdat->data[loc+3] << 16) |
			(tdat->data[loc+2] << 8) |
			tdat->data[loc+1];
		if ((loc + length + 4) > tdat->size) {
			printk(KERN_ERR
				"%s: Overflow in data at byte %u %s %hu.\n",
				__func__, loc, "and record", tdat->data[loc]);
			err = -EOVERFLOW;
			goto atmxt_validate_tdat_fail;
		}

		loc = loc + length + 4;
	}

	if (loc != (tdat->size)) {
		printk(KERN_ERR "%s: Data is misaligned.\n", __func__);
		err = -ENOEXEC;
		goto atmxt_validate_tdat_fail;
	}

atmxt_validate_tdat_fail:
	return err;
}

static int atmxt_validate_settings(uint8_t *data, uint32_t size)
{
	int err = 0;
	uint32_t iter = 0;
	uint16_t length = 0x0000;

	if (data == NULL || size == 0) {
		printk(KERN_ERR "%s: No settings data found.\n", __func__);
		err = -ENODATA;
		goto atmxt_validate_settings_fail;
	} else if (size <= 5) {
		printk(KERN_ERR "%s: Settings data is malformed.\n", __func__);
		err = -EINVAL;
		goto atmxt_validate_settings_fail;
	}

	while (iter < (size - 1)) {
		length = (data[iter+4] << 8) | data[iter+3];
		if ((iter + length + 5) > size) {
			printk(KERN_ERR
				"%s: Group record overflow on iter %u.\n",
				__func__, iter);
			err = -EOVERFLOW;
			goto atmxt_validate_settings_fail;
		}

		iter = iter + length + 5;
	}

	if (iter != size) {
		printk(KERN_ERR "%s: Group records misaligned.\n", __func__);
		err = -ENOEXEC;
		goto atmxt_validate_settings_fail;
	}

atmxt_validate_settings_fail:
	return err;
}

static int atmxt_gpio_init(struct atmxt_driver_data *dd)
{
	int err = 0;
	struct gpio touch_gpio[] = {
		{dd->pdata->gpio_reset, GPIOF_OUT_INIT_LOW, "touch_reset"},
		{dd->pdata->gpio_interrupt,   GPIOF_IN,           "touch_irq"},
	};

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Requesting touch GPIOs...\n", __func__);

	err = gpio_request_array(touch_gpio, ARRAY_SIZE(touch_gpio));
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to request touch GPIOs.\n",
			__func__);
		goto atmxt_gpio_init_fail;
	}
	udelay(ATMXT_IC_RESET_HOLD_TIME);

	dd->client->irq = gpio_to_irq(dd->pdata->gpio_interrupt);

atmxt_gpio_init_fail:
	return err;
}

static int atmxt_register_inputs(struct atmxt_driver_data *dd,
		uint8_t *rdat, int rsize)
{
	int err = 0;
	int i = 0;
	int iter = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Registering inputs...\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (dd->rdat != NULL)
		kfree(dd->rdat);
#endif

	dd->rdat = kzalloc(sizeof(struct atmxt_report_data), GFP_KERNEL);
	if (dd->rdat == NULL) {
		printk(KERN_ERR "%s: Unable to create report data.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_register_inputs_fail;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (dd->in_dev != NULL)
		input_unregister_device(dd->in_dev);
#endif

	dd->in_dev = input_allocate_device();
	if (dd->in_dev == NULL) {
		printk(KERN_ERR "%s: Failed to allocate input device.\n",
			__func__);
		err = -ENODEV;
		goto atmxt_register_inputs_fail;
	}

	dd->in_dev->name = ATMXT_I2C_NAME;
	input_set_drvdata(dd->in_dev, dd);
	set_bit(INPUT_PROP_DIRECT, dd->in_dev->propbit);

	set_bit(EV_ABS, dd->in_dev->evbit);
	for (i = 0; i < rsize; i += 10) {
		if (((rdat[i+1] << 8) | rdat[i+0]) != ATMXT_ABS_RESERVED) {
			input_set_abs_params(dd->in_dev,
				(rdat[i+1] << 8) | rdat[i+0],
				(rdat[i+3] << 8) | rdat[i+2],
				(rdat[i+5] << 8) | rdat[i+4],
				(rdat[i+7] << 8) | rdat[i+6],
				(rdat[i+9] << 8) | rdat[i+8]);
		}

		if (iter < ARRAY_SIZE(dd->rdat->axis)) {
			dd->rdat->axis[iter] = (rdat[i+1] << 8) | rdat[i+0];
			iter++;
		}
	}

	for (i = iter; i < ARRAY_SIZE(dd->rdat->axis); i++)
		dd->rdat->axis[i] = ATMXT_ABS_RESERVED;

	input_set_events_per_packet(dd->in_dev,
		ATMXT_MAX_TOUCHES * (ARRAY_SIZE(dd->rdat->axis) + 1));

	err = input_register_device(dd->in_dev);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to register input device.\n",
			__func__);
		err = -ENODEV;
		input_free_device(dd->in_dev);
		dd->in_dev = NULL;
		goto atmxt_register_inputs_fail;
	}

atmxt_register_inputs_fail:
	return err;
}

static int atmxt_request_irq(struct atmxt_driver_data *dd)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Requesting IRQ...\n", __func__);

	err = gpio_get_value(dd->pdata->gpio_interrupt);
	if (err < 0) {
		printk(KERN_ERR "%s: Cannot test IRQ line level.\n", __func__);
		goto atmxt_request_irq_fail;
	} else if (err == 0) {
		printk(KERN_ERR
			"%s: Line already active; cannot request IRQ.\n",
			__func__);
		err = -EIO;
		goto atmxt_request_irq_fail;
	}

	err = request_threaded_irq(dd->client->irq, NULL, atmxt_isr,
			IRQF_TRIGGER_FALLING, ATMXT_I2C_NAME, dd);
	if (err < 0) {
		printk(KERN_ERR "%s: IRQ request failed.\n", __func__);
		goto atmxt_request_irq_fail;
	}

	disable_irq_nosync(dd->client->irq);

atmxt_request_irq_fail:
	return err;
}

static int atmxt_restart_ic(struct atmxt_driver_data *dd)
{
	int err = 0;
	bool irq_low = false;
	uint32_t size = 0;
	bool update_fw = false;
	bool need_reset = false;
	int cur_drv_state = 0;
	bool update_complete = false;

atmxt_restart_ic_start:
	atmxt_dbg(dd, ATMXT_DBG3, "%s: Restarting IC...\n", __func__);

	atmxt_free_ic_data(dd);
	atmxt_release_touches(dd);
	irq_low = false;
	if (atmxt_get_ic_state(dd) != ATMXT_IC_UNKNOWN)
		atmxt_set_ic_state(dd, ATMXT_IC_UNKNOWN);

	if (!update_fw && !update_complete) {
		atmxt_dbg(dd, ATMXT_DBG2,
			"%s: Resetting touch IC...\n", __func__);
		gpio_set_value(dd->pdata->gpio_reset, 0);
		udelay(ATMXT_IC_RESET_HOLD_TIME);
		gpio_set_value(dd->pdata->gpio_reset, 1);
	}

	irq_low = atmxt_wait4irq(dd);
	if (!irq_low && !update_fw) {
		printk(KERN_ERR "%s: Timeout waiting for interrupt.\n",
			__func__);
		err = -ETIME;
		goto atmxt_restart_ic_fail;
	}

	dd->info_blk = kzalloc(sizeof(struct atmxt_info_block), GFP_KERNEL);
	if (dd->info_blk == NULL) {
		printk(KERN_ERR "%s: Unable to create info block data.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_restart_ic_fail;
	}

	if (update_fw) {
		if (!irq_low) {
			atmxt_dbg(dd, ATMXT_DBG3,
				"%s: Ignored interrupt timeout.\n", __func__);
		}

		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Trying bootloader...\n", __func__);
		update_fw = false;
		goto atmxt_restart_ic_updatefw_start;
	}

	err = atmxt_get_info_header(dd);
	if (err < 0) {
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Error reaching IC in normal mode.  %s\n",
			__func__, "Trying bootloader...");
atmxt_restart_ic_updatefw_start:
		dd->client->addr = dd->util->addr[1];
		err = atmxt_i2c_read(dd, &(dd->info_blk->header[0]), 3);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to find touch IC.\n",
				__func__);
			dd->client->addr = dd->util->addr[0];
			goto atmxt_restart_ic_fail;
		}

		atmxt_set_ic_state(dd, ATMXT_IC_BOOTLOADER);
		if (dd->info_blk->header[0] & 0x20) {
			printk(KERN_INFO "%s: %s: 0x%02X, %s: 0x%02X\n",
				__func__,
				"Bootloader ID", dd->info_blk->header[1],
				"Bootloader Version", dd->info_blk->header[2]);
		} else {
			printk(KERN_INFO "%s: Bootloader ID: 0x%02X\n",
				__func__, dd->info_blk->header[0] & 0x1F);
		}

		if ((dd->info_blk->header[0] & 0xC0) == 0x40) {
			if (update_complete) {
				printk(KERN_ERR "%s: Firmware CRC failure.\n",
					__func__);
			} else {
				printk(KERN_INFO
					"%s: Firmware CRC failure; %s.\n",
					__func__, "going to try reflashing IC");
			}
		}

		if (update_complete) {
			printk(KERN_ERR "%s: %s--%s.\n", __func__,
				"Still in bootloader mode after reflash",
				"check firmware image");
			dd->client->addr = dd->util->addr[0];
			err = -EINVAL;
			goto atmxt_restart_ic_fail;
		}

		cur_drv_state = atmxt_get_drv_state(dd);
		atmxt_set_drv_state(dd, ATMXT_DRV_REFLASH);
		err = atmxt_flash_firmware(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to update IC firmware.\n",
				__func__);
			dd->client->addr = dd->util->addr[0];
			atmxt_set_drv_state(dd, cur_drv_state);
			goto atmxt_restart_ic_fail;
		}

		atmxt_set_drv_state(dd, cur_drv_state);
		dd->client->addr = dd->util->addr[0];
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Reflash completed.  Re-starting cycle...\n",
			__func__);
		update_complete = true;
		goto atmxt_restart_ic_start;
	}

	atmxt_set_ic_state(dd, ATMXT_IC_PRESENT);
	printk(KERN_INFO "%s: Family ID: 0x%02X, Variant ID: 0x%02X, " \
		"Version: 0x%02X, Build: 0x%02X, Matrix: %ux%u, Objects: %u\n",
		__func__, dd->info_blk->header[0], dd->info_blk->header[1],
		dd->info_blk->header[2], dd->info_blk->header[3],
		dd->info_blk->header[4], dd->info_blk->header[5],
		dd->info_blk->header[6]);

	if (atmxt_get_drv_state(dd) == ATMXT_DRV_INIT) {
		update_fw = atmxt_check_firmware_update(dd);
		if (update_fw & update_complete) {
			printk(KERN_ERR "%s: %s %s %s.\n", __func__,
					"Platform firmware version",
					"does not match platform firmware",
					"after update");
				update_fw = false;
		}
	}

	size = dd->info_blk->header[6] * 6;
	if (size > 255) {
		printk(KERN_ERR "%s: Too many objects present.\n", __func__);
		err = -EOVERFLOW;
		goto atmxt_restart_ic_fail;
	}
	dd->info_blk->size = size;

	dd->info_blk->data = kzalloc(sizeof(uint8_t) * size, GFP_KERNEL);
	if (dd->info_blk->data == NULL) {
		printk(KERN_ERR "%s: Unable to create table data.\n", __func__);
		err = -ENOMEM;
		goto atmxt_restart_ic_fail;
	}

	err = atmxt_get_object_table(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Error getting object table.\n", __func__);
		if (update_fw)
			goto atmxt_restart_updatefw_check;
		else
			goto atmxt_restart_ic_fail;
	}

atmxt_restart_updatefw_check:
	if (update_fw) {
		printk(KERN_INFO "%s: Resetting IC to update firmware...\n",
			__func__);
		err = atmxt_force_bootloader(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to force flash mode.\n",
				__func__);
			goto atmxt_restart_ic_fail;
		}
		goto atmxt_restart_ic_start;
	}

	dd->nvm = kzalloc(sizeof(struct atmxt_nvm), GFP_KERNEL);
	if (dd->nvm == NULL) {
		printk(KERN_ERR "%s: Unable to create NVM struct.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_restart_ic_fail;
	}

	dd->addr = kzalloc(sizeof(struct atmxt_addr), GFP_KERNEL);
	if (dd->addr == NULL) {
		printk(KERN_ERR "%s: Unable to create address book.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_restart_ic_fail;
	}

	dd->data = kzalloc(sizeof(struct atmxt_data), GFP_KERNEL);
	if (dd->data == NULL) {
		printk(KERN_ERR "%s: Unable to create data book.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_restart_ic_fail;
	}

	err = atmxt_process_object_table(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Processing info block failed.\n",
			__func__);
		goto atmxt_restart_ic_fail;
	}

	err = atmxt_save_internal_data(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to save internal data.\n",
			__func__);
		goto atmxt_restart_ic_fail;
	}

	atmxt_compute_checksum(dd);

	err = atmxt_check_settings(dd, &need_reset);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to check/update IC %s.\n",
			__func__, "with platform settings");
		goto atmxt_restart_ic_fail;
	} else if (need_reset) {
		update_fw = false;
		update_complete = false;
		goto atmxt_restart_ic_start;
	}

	atmxt_set_ic_state(dd, ATMXT_IC_ACTIVE);

	err = atmxt_start_ic_calibration_fix(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to start IC calibration fix.\n",
			__func__);
		goto atmxt_restart_ic_fail;
	}

atmxt_restart_ic_fail:
	return err;
}

static irqreturn_t atmxt_isr(int irq, void *handle)
{
	struct atmxt_driver_data *dd = handle;
	int drv_state;
	int ic_state;

	mutex_lock(dd->mutex);

	drv_state = atmxt_get_drv_state(dd);
	ic_state = atmxt_get_ic_state(dd);

	atmxt_dbg(dd, ATMXT_DBG3,
		"%s: IRQ Received -- Driver: %s, IC: %s\n", __func__,
		atmxt_driver_state_string[drv_state],
		atmxt_ic_state_string[ic_state]);

	switch (drv_state) {
	case ATMXT_DRV_ACTIVE:
		switch (ic_state) {
		case ATMXT_IC_SLEEP:
			atmxt_dbg(dd, ATMXT_DBG3,
				"%s: Servicing IRQ during sleep...\n",
				__func__);
		case ATMXT_IC_ACTIVE:
			atmxt_active_handler(dd);
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s IRQ received.\n",
				__func__, atmxt_driver_state_string[drv_state],
				atmxt_ic_state_string[ic_state]);
			if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
				disable_irq_nosync(dd->client->irq);
				dd->status = dd->status &
					~(1 << ATMXT_IRQ_ENABLED_FLAG);
				atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
			}
			break;
		}
		break;

	default:
		printk(KERN_ERR "%s: Driver state \"%s\" IRQ received.\n",
			__func__, atmxt_driver_state_string[drv_state]);
		if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			dd->status = dd->status &
				~(1 << ATMXT_IRQ_ENABLED_FLAG);
			atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
		}
		break;
	}

	atmxt_dbg(dd, ATMXT_DBG3, "%s: IRQ Serviced.\n", __func__);
	mutex_unlock(dd->mutex);

	return IRQ_HANDLED;
}

static int atmxt_get_info_header(struct atmxt_driver_data *dd)
{
	int err = 0;

	err = atmxt_i2c_write(dd, 0x00, 0x00, NULL, 0);
	if (err < 0)
		goto atmxt_get_info_header_fail;

	err = atmxt_i2c_read(dd, &(dd->info_blk->header[0]), 7);
	if (err < 0)
		goto atmxt_get_info_header_fail;

atmxt_get_info_header_fail:
	return err;
}

static int atmxt_get_object_table(struct atmxt_driver_data *dd)
{
	int err = 0;
	int i = 0;                      /* Fix Atmel's data order */
	int top = 0;                    /* Fix Atmel's data order */
	int cur = 0;                    /* Fix Atmel's data order */
	uint8_t lo_addr = 255;          /* Fix Atmel's data order */
	uint8_t hi_addr = 255;          /* Fix Atmel's data order */
	uint8_t temp[6];                /* Fix Atmel's data order */

	err = atmxt_i2c_write(dd, 0x07, 0x00, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Unable to set address pointer to object table.\n",
			__func__);
		goto atmxt_get_object_table_fail;
	}

	err = atmxt_i2c_read(dd, dd->info_blk->data, dd->info_blk->size);
	if (err < 0) {
		printk(KERN_ERR "%s: Object table read failed.\n", __func__);
		goto atmxt_get_object_table_fail;
	}

	/* Fix Atmel's data order */
	while (top < dd->info_blk->size) {
		for (i = top; i < dd->info_blk->size; i += 6) {
			if (dd->info_blk->data[i+2] < hi_addr) {
				lo_addr = dd->info_blk->data[i+1];
				hi_addr = dd->info_blk->data[i+2];
				cur = i;
			} else if ((dd->info_blk->data[i+2] == hi_addr) &&
				(dd->info_blk->data[i+1] < lo_addr)) {
					lo_addr = dd->info_blk->data[i+1];
					hi_addr = dd->info_blk->data[i+2];
					cur = i;
			}
		}

		memcpy(&(temp[0]), &(dd->info_blk->data[top]), 6);
		memmove(&(dd->info_blk->data[top]),
			&(dd->info_blk->data[cur]), 6);
		memcpy(&(dd->info_blk->data[cur]), &(temp[0]), 6);

		lo_addr = 255;
		hi_addr = 255;
		top = top + 6;
		cur = top;
	}

atmxt_get_object_table_fail:
	return err;
}

static int atmxt_process_object_table(struct atmxt_driver_data *dd)
{
	int err = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	uint32_t ids = 0;
	uint32_t nvm_size = 0;
	int usr_start = 0;
	bool usr_start_seen = false;
	uint8_t *tsett = NULL;
	int id_iter = 0;
	int nvm_iter = 0;

	ids++;
	for (i = 0; i < dd->info_blk->size; i += 6) {
		ids += dd->info_blk->data[i+5] * (dd->info_blk->data[i+4] + 1);
		if (usr_start_seen) {
			nvm_size += (dd->info_blk->data[i+3] + 1)
				* (dd->info_blk->data[i+4] + 1);
		} else if (dd->info_blk->data[i+0] == 38) {
			usr_start = i;
			usr_start_seen = true;
		}
	}

	if (ids > 255) {
		printk(KERN_ERR "%s: Too many report IDs used.\n", __func__);
		err = -EOVERFLOW;
		goto atmxt_process_object_table_fail;
	}

	dd->info_blk->msg_id = kzalloc(sizeof(uint8_t) * ids, GFP_KERNEL);
	if (dd->info_blk->msg_id == NULL) {
		printk(KERN_ERR "%s: Unable to create ID table.\n", __func__);
		err = -ENOMEM;
		goto atmxt_process_object_table_fail;
	}
	dd->info_blk->id_size = ids;

	dd->nvm->data = kzalloc(sizeof(uint8_t) * nvm_size, GFP_KERNEL);
	if (dd->nvm->data == NULL) {
		printk(KERN_ERR "%s: Unable to create NVM block.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_process_object_table_fail;
	}
	dd->nvm->size = nvm_size;
	dd->nvm->addr[0] = dd->info_blk->data[usr_start+6+1];
	dd->nvm->addr[1] = dd->info_blk->data[usr_start+6+2];

	dd->info_blk->msg_id[id_iter] = 0;
	id_iter++;
	for (i = 0; i < dd->info_blk->size; i += 6) {
		for (j = 0; j <= dd->info_blk->data[i+4]; j++) {
			for (k = 0; k < dd->info_blk->data[i+5]; k++) {
				dd->info_blk->msg_id[id_iter] =
					dd->info_blk->data[i+0];
				id_iter++;
			}
		}

		if (i <= usr_start)
			continue;

		tsett = atmxt_get_settings_entry(dd, dd->info_blk->data[i+0]);
		err = atmxt_copy_platform_data(&(dd->nvm->data[nvm_iter]),
			&(dd->info_blk->data[i+0]), tsett);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to copy platform data.\n",
				__func__);
			goto atmxt_process_object_table_fail;
		}

		nvm_iter += (dd->info_blk->data[i+3] + 1) *
			(dd->info_blk->data[i+4] + 1);
	}

atmxt_process_object_table_fail:
	return err;
}

static uint8_t *atmxt_get_settings_entry(struct atmxt_driver_data *dd,
		uint16_t num)
{
	uint8_t *entry = NULL;
	uint32_t iter = 0;

	while (iter < dd->util->tsett_size) {
		if (num == ((dd->util->tsett[iter+1] << 8) |
			dd->util->tsett[iter])) {
			entry = &(dd->util->tsett[iter]);
			break;
		} else {
			iter += 5 + ((dd->util->tsett[iter+4] << 8) |
				dd->util->tsett[iter+3]);
		}
	}

	return entry;
}

static int atmxt_copy_platform_data(uint8_t *reg, uint8_t *entry,
		uint8_t *tsett)
{
	int err = 0;
	int i = 0;
	int iter = 0;
	int size = 0;
	uint8_t *data = NULL;
	int data_size = 0;
	int obj_size = 0;
	int obj_inst = 0;
	uint8_t inst_count = 0x00;
	uint16_t tsett_size = 0x0000;

	if (tsett == NULL)
		goto atmxt_copy_platform_data_fail;

	tsett_size = (tsett[4] << 8) | tsett[3];
	if (tsett[2] == 0) {
		inst_count = 1;
		data_size = tsett_size;
		data = &(tsett[5]);
	} else {
		inst_count = tsett[2];

		if ((tsett_size % inst_count) != 0) {
			printk(KERN_ERR "%s: Settings data unevenly packed.\n",
				__func__);
			err = -EINVAL;
			goto atmxt_copy_platform_data_fail;
		}

		data_size = tsett_size / inst_count;
		data = &(tsett[5]);
	}

	obj_size = entry[3] + 1;
	obj_inst = entry[4] + 1;

	if (data_size > obj_size)
		size = obj_size;
	else
		size = data_size;

	while ((i < inst_count) && (i < obj_inst)) {
		memcpy(&(reg[i*obj_size]), &(data[iter]), size);
		iter += data_size;
		i++;
	}

atmxt_copy_platform_data_fail:
	return err;
}

static int atmxt_check_settings(struct atmxt_driver_data *dd, bool *reset)
{
	int err = 0;
	uint8_t *msg_buf = NULL;
	char *contents = NULL;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Checking IC settings...\n", __func__);

	if (dd->data->max_msg_size < 5) {
		printk(KERN_ERR "%s: Message size is too small.\n", __func__);
		err = -EINVAL;
		goto atmxt_check_settings_fail;
	}

	msg_buf = kzalloc(sizeof(uint8_t) * dd->data->max_msg_size, GFP_KERNEL);
	if (msg_buf == NULL) {
		printk(KERN_ERR
			"%s: Unable to allocate memory for message buffer.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_check_settings_fail;
	}

	err = atmxt_i2c_write(dd, dd->addr->msg[0], dd->addr->msg[1], NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to set message buffer pointer.\n",
			__func__);
		goto atmxt_check_settings_fail;
	}

	dd->status = dd->status | (1 << ATMXT_SET_MESSAGE_POINTER);

	err = atmxt_i2c_read(dd, msg_buf, dd->data->max_msg_size);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to read message.\n", __func__);
		goto atmxt_check_settings_fail;
	}

	if (msg_buf[0] <= (dd->info_blk->id_size-1)) {
		if (dd->info_blk->msg_id[msg_buf[0]] == 6) {
			if (!(msg_buf[1] & 0x80)) {
				printk(KERN_ERR "%s: %s:  0x%02X\n", __func__,
					"Received checksum without reset",
					msg_buf[1]);
			}
		} else {
			contents = atmxt_msg2str(msg_buf,
				dd->data->max_msg_size);
			printk(KERN_ERR "%s: %s--%s %u instead:  %s.\n",
				__func__, "Failed to receive reset message",
				"received this from Object",
				dd->info_blk->msg_id[msg_buf[0]], contents);
			err = -EIO;
			goto atmxt_check_settings_fail;
		}
	} else {
		contents = atmxt_msg2str(msg_buf, dd->data->max_msg_size);
		printk(KERN_ERR "%s: %s--%s:  %s.\n",
			__func__, "Failed to receive reset message",
			"received unknown message instead", contents);
		err = -EIO;
		goto atmxt_check_settings_fail;
	}

	atmxt_dbg(dd, ATMXT_DBG3,
		"%s: %s 0x%02X%02X%02X, %s 0x%02X%02X%02X.\n", __func__,
		"Driver checksum is", dd->nvm->chksum[0],
		dd->nvm->chksum[1], dd->nvm->chksum[2],
		"IC checksum is", msg_buf[2], msg_buf[3], msg_buf[4]);

	if ((msg_buf[2] == dd->nvm->chksum[0]) &&
		(msg_buf[3] == dd->nvm->chksum[1]) &&
		(msg_buf[4] == dd->nvm->chksum[2])) {
		err = atmxt_process_message(dd,
			msg_buf, dd->data->max_msg_size);
		if (err < 0) {
			printk(KERN_ERR "%s: Error processing first message.\n",
				__func__);
			goto atmxt_check_settings_fail;
		}
		*reset = false;
	} else if (*reset) {
		printk(KERN_ERR "%s: %s.\n", __func__,
			"Previous attempt to write platform settings failed");
		err = -EINVAL;
		goto atmxt_check_settings_fail;
	} else {
		printk(KERN_INFO "%s: Updating IC settings...\n", __func__);
		err = atmxt_send_settings(dd, true);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to update IC settings.\n",
				__func__);
			goto atmxt_check_settings_fail;
		}

		msleep(500);
		*reset = true;
	}

atmxt_check_settings_fail:
	kfree(msg_buf);
	kfree(contents);
	return err;
}

static int atmxt_send_settings(struct atmxt_driver_data *dd, bool save_nvm)
{
	int err = 0;
	uint8_t nvm_cmd = 0x55;

	err = atmxt_i2c_write(dd, dd->nvm->addr[0], dd->nvm->addr[1],
		dd->nvm->data, dd->nvm->size);
	if (err < 0) {
		printk(KERN_ERR "%s: Error writing settings to IC.\n",
			__func__);
		goto atmxt_send_settings_fail;
	}

	if (save_nvm) {
		err = atmxt_i2c_write(dd, dd->addr->nvm[0], dd->addr->nvm[1],
			&nvm_cmd, sizeof(uint8_t));
		if (err < 0) {
			printk(KERN_ERR "%s: Error backing up to NVM.\n",
				__func__);
			goto atmxt_send_settings_fail;
		}
	}

atmxt_send_settings_fail:
	return err;
}

static int atmxt_recalibrate_ic(struct atmxt_driver_data *dd)
{
	int err = 0;
	uint8_t cmd = 0x01;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Asking touch IC to recalibrate...\n",
		__func__);

	err = atmxt_i2c_write(dd, dd->addr->cal[0], dd->addr->cal[1], &cmd, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to send calibrate to touch IC.\n",
			__func__);
		goto atmxt_recalibrate_ic_fail;
	}

atmxt_recalibrate_ic_fail:
	return err;
}

static int atmxt_start_ic_calibration_fix(struct atmxt_driver_data *dd)
{
	int err = 0;
	uint8_t sett[6] = {0x05, 0x00, 0x00, 0x00, 0x01, 0x80};

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Starting IC calibration fix...\n",
		__func__);

	sett[1] = dd->data->acq[1];
	err = atmxt_i2c_write(dd, dd->addr->acq[0], dd->addr->acq[1],
		&(sett[0]), 6);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to update acquisition settings.\n",
			__func__);
		goto atmxt_start_ic_calibration_fix_fail;
	}

	dd->data->timer = 0;
	dd->status = dd->status & ~(1 << ATMXT_RECEIVED_CALIBRATION);
	dd->status = dd->status | (1 << ATMXT_FIXING_CALIBRATION);

atmxt_start_ic_calibration_fix_fail:
	return err;
}

static int atmxt_verify_ic_calibration_fix(struct atmxt_driver_data *dd)
{
	int err = 0;
	unsigned long toc = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Verifying IC calibration fix...\n",
		__func__);

	toc = jiffies;
	if (dd->status & (1 << ATMXT_RECEIVED_CALIBRATION)) {
		dd->data->timer = 0;
		dd->status = dd->status & ~(1 << ATMXT_RECEIVED_CALIBRATION);
	} else if (dd->status & (1 << ATMXT_REPORT_TOUCHES)) {
		if (dd->data->timer == 0)
			dd->data->timer = toc;

		if (((toc - dd->data->timer) * 1000 / HZ) >= 2500) {
			err = atmxt_stop_ic_calibration_fix(dd);
			if (err < 0) {
				printk(KERN_ERR "%s: %s %s.\n", __func__,
					"Failed to stop",
					"fixing IC calibration");
				goto atmxt_verify_ic_calibration_fix_fail;
			}
		}
	}

atmxt_verify_ic_calibration_fix_fail:
	return err;
}

static int atmxt_stop_ic_calibration_fix(struct atmxt_driver_data *dd)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Stopping IC calibration fix...\n",
		__func__);

	err = atmxt_i2c_write(dd, dd->addr->acq[0], dd->addr->acq[1],
		&(dd->data->acq[0]), 6);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to update acquisition settings.\n",
			__func__);
		goto atmxt_stop_ic_calibration_fix_fail;
	}

	dd->status = dd->status & ~(1 << ATMXT_FIXING_CALIBRATION);

atmxt_stop_ic_calibration_fix_fail:
	return err;
}

static int atmxt_i2c_write(struct atmxt_driver_data *dd,
		uint8_t addr_lo, uint8_t addr_hi, uint8_t *buf, int size)
{
	int err = 0;
	uint8_t *data_out = NULL;
	int size_out = 0;
	int i = 0;
	char *str = NULL;

	dd->status = dd->status & ~(1 << ATMXT_SET_MESSAGE_POINTER);

	size_out = size + 2;
	data_out = kzalloc(sizeof(uint8_t) * size_out, GFP_KERNEL);
	if (data_out == NULL) {
		printk(KERN_ERR "%s: Unable to allocate write memory.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_i2c_write_exit;
	}

	data_out[0] = addr_lo;
	data_out[1] = addr_hi;
	if (buf != NULL && size > 0)
		memcpy(&(data_out[2]), buf, size);

	for (i = 1; i <= ATMXT_I2C_ATTEMPTS; i++) {
		err = i2c_master_send(dd->client, data_out, size_out);
		if (err < 0) {
			printk(KERN_ERR
				"%s: %s %d, failed with error code %d.\n",
				__func__, "On I2C write attempt", i, err);
		} else if (err < size_out) {
			printk(KERN_ERR
				"%s: %s %d, wrote %d bytes instead of %d.\n",
				__func__, "On I2C write attempt", i, err,
				size_out);
			err = -EBADE;
		} else {
			break;
		}

		udelay(ATMXT_I2C_WAIT_TIME);
	}

	if (err < 0)
		printk(KERN_ERR "%s: I2C write failed.\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if ((dd->dbg->dbg_lvl) >= ATMXT_DBG2)
		str = atmxt_msg2str(data_out, size_out);
#endif
	atmxt_dbg(dd, ATMXT_DBG2, "%s: %s\n", __func__, str);
	kfree(str);

atmxt_i2c_write_exit:
	kfree(data_out);

	return err;
}

static int atmxt_i2c_read(struct atmxt_driver_data *dd, uint8_t *buf, int size)
{
	int err = 0;
	int i = 0;
	char *str = NULL;

	for (i = 1; i <= ATMXT_I2C_ATTEMPTS; i++) {
		err = i2c_master_recv(dd->client, buf, size);
		if (err < 0) {
			printk(KERN_ERR
				"%s: %s %d, failed with error code %d.\n",
				__func__, "On I2C read attempt", i, err);
		} else if (err < size) {
			printk(KERN_ERR
				"%s: %s %d, received %d bytes instead of %d.\n",
				__func__, "On I2C read attempt", i, err, size);
			err = -EBADE;
		} else {
			break;
		}

		udelay(ATMXT_I2C_WAIT_TIME);
	}

	if (err < 0)
		printk(KERN_ERR "%s: I2C read failed.\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if ((dd->dbg->dbg_lvl) >= ATMXT_DBG1)
		str = atmxt_msg2str(buf, size);
#endif
	atmxt_dbg(dd, ATMXT_DBG1, "%s: %s\n", __func__, str);
	kfree(str);

	return err;
}

static int atmxt_save_internal_data(struct atmxt_driver_data *dd)
{
	int err = 0;
	int i = 0;
	bool usr_start_seen = false;
	int nvm_iter = 0;
	bool chk_5 = false;
	bool chk_6 = false;
	bool chk_7 = false;
	bool chk_8 = false;
	bool chk_9 = false;

	for (i = 0; i < dd->info_blk->size; i += 6) {
		switch (dd->info_blk->data[i+0]) {
		case 5:
			chk_5 = true;
			err = atmxt_save_data5(dd, &(dd->info_blk->data[i+0]));
			if (err < 0)
				goto atmxt_save_internal_data_fail;
			break;

		case 6:
			chk_6 = true;
			err = atmxt_save_data6(dd, &(dd->info_blk->data[i+0]));
			if (err < 0)
				goto atmxt_save_internal_data_fail;
			break;

		case 7:
			chk_7 = true;
			err = atmxt_save_data7(dd, &(dd->info_blk->data[i+0]),
				&(dd->nvm->data[nvm_iter]));
			if (err < 0)
				goto atmxt_save_internal_data_fail;
			break;

		case 8:
			chk_8 = true;
			err = atmxt_save_data8(dd, &(dd->info_blk->data[i+0]),
				&(dd->nvm->data[nvm_iter]));
			if (err < 0)
				goto atmxt_save_internal_data_fail;
			break;

		case 9:
			chk_9 = true;
			err = atmxt_save_data9(dd, &(dd->info_blk->data[i+0]),
				&(dd->nvm->data[nvm_iter]));
			if (err < 0)
				goto atmxt_save_internal_data_fail;
			break;

		case 38:
			usr_start_seen = true;
			break;

		default:
			break;
		}

		if (usr_start_seen && (dd->info_blk->data[i+0] != 38)) {
			nvm_iter += (dd->info_blk->data[i+3] + 1) *
				(dd->info_blk->data[i+4] + 1);
		}
	}

	if (!chk_5) {
		printk(KERN_ERR "%s: Object 5 is missing.\n", __func__);
		err = -ENODATA;
	}

	if (!chk_6) {
		printk(KERN_ERR "%s: Object 6 is missing.\n", __func__);
		err = -ENODATA;
	}

	if (!chk_7) {
		printk(KERN_ERR "%s: Object 7 is missing.\n", __func__);
		err = -ENODATA;
	}

	if (!chk_8) {
		printk(KERN_ERR "%s: Object 8 is missing.\n", __func__);
		err = -ENODATA;
	}

	if (!chk_9) {
		printk(KERN_ERR "%s: Object 9 is missing.\n", __func__);
		err = -ENODATA;
	}

atmxt_save_internal_data_fail:
	return err;
}

static int atmxt_save_data5(struct atmxt_driver_data *dd, uint8_t *entry)
{
	int err = 0;

	dd->addr->msg[0] = entry[1];
	dd->addr->msg[1] = entry[2];
	dd->data->max_msg_size = entry[3];

	return err;
}

static int atmxt_save_data6(struct atmxt_driver_data *dd, uint8_t *entry)
{
	int err = 0;

	if (entry[3] < 2) {
		printk(KERN_ERR "%s: Command object is too small.\n", __func__);
		err = -ENODATA;
		goto atmxt_save_data6_fail;
	}

	dd->addr->rst[0] = entry[1];
	dd->addr->rst[1] = entry[2];

	dd->addr->nvm[0] = entry[1] + 1;
	dd->addr->nvm[1] = entry[2];
	if (dd->addr->nvm[0] < entry[1])
		dd->addr->nvm[1]++;

	dd->addr->cal[0] = entry[1] + 2;
	dd->addr->cal[1] = entry[2];
	if (dd->addr->cal[0] < entry[1])
		dd->addr->cal[1]++;

atmxt_save_data6_fail:
	return err;
}

static int atmxt_save_data7(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg)
{
	int err = 0;

	if (entry[3] < 1) {
		printk(KERN_ERR "%s: Power object is too small.\n", __func__);
		err = -ENODATA;
		goto atmxt_save_data7_fail;
	}

	dd->addr->pwr[0] = entry[1];
	dd->addr->pwr[1] = entry[2];
	dd->data->pwr[0] = reg[0];
	dd->data->pwr[1] = reg[1];

atmxt_save_data7_fail:
	return err;
}

static int atmxt_save_data8(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg)
{
	int err = 0;

	if (entry[3] < 9) {
		printk(KERN_ERR "%s: Acquisition object is too small.\n",
			__func__);
		err = -ENODATA;
		goto atmxt_save_data8_fail;
	}

	dd->addr->acq[0] = entry[1] + 4;
	dd->addr->acq[1] = entry[2];
	if (dd->addr->acq[0] < entry[1])
		dd->addr->acq[1]++;

	dd->data->acq[0] = reg[4];
	dd->data->acq[1] = reg[5];
	dd->data->acq[2] = reg[6];
	dd->data->acq[3] = reg[7];
	dd->data->acq[4] = reg[8];
	dd->data->acq[5] = reg[9];

atmxt_save_data8_fail:
	return err;
}

static int atmxt_save_data9(struct atmxt_driver_data *dd,
		uint8_t *entry, uint8_t *reg)
{
	int err = 0;
	int i = 0;

	for (i = 1; i < dd->info_blk->id_size; i++) {
		if (dd->info_blk->msg_id[i] == 9) {
			dd->data->touch_id_offset = i;
			break;
		}
	}

	if (dd->data->touch_id_offset == 0) {
		printk(KERN_ERR
			"%s: Touch object has reporting error.\n",
			__func__);
		err = -ENODATA;
		goto atmxt_save_data9_fail;
	}

	dd->data->res[0] = false;
	dd->data->res[1] = false;

	if (entry[3] < 21) {
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Only 10-bit resolution is available.\n", __func__);
	} else {
		if (reg[19] >= 0x04)
			dd->data->res[0] = true;

		if (reg[21] >= 0x04)
			dd->data->res[1] = true;
	}

atmxt_save_data9_fail:
	return err;
}

static void atmxt_compute_checksum(struct atmxt_driver_data *dd)
{
	uint8_t low = 0x00;
	uint8_t mid = 0x00;
	uint8_t high = 0x00;
	uint8_t byte1 = 0x00;
	uint8_t byte2 = 0x00;
	uint32_t iter = 0;
	uint32_t range = 0;

	range = dd->nvm->size - (dd->nvm->size % 2);

	while (iter < range) {
		byte1 = dd->nvm->data[iter];
		iter++;
		byte2 = dd->nvm->data[iter];
		iter++;
		atmxt_compute_partial_checksum(&byte1, &byte2,
			&low, &mid, &high);
	}

	if ((dd->nvm->size % 2) != 0) {
		byte1 = dd->nvm->data[iter];
		byte2 = 0x00;
		atmxt_compute_partial_checksum(&byte1, &byte2,
			&low, &mid, &high);
	}

	dd->nvm->chksum[0] = low;
	dd->nvm->chksum[1] = mid;
	dd->nvm->chksum[2] = high;

	return;
}

static void atmxt_compute_partial_checksum(uint8_t *byte1, uint8_t *byte2,
		uint8_t *low, uint8_t *mid, uint8_t *high)
{
	bool xor_result = false;

	if (*high & 0x80)
		xor_result = true;

	*high = *high << 1;
	if (*mid & 0x80)
		(*high)++;

	*mid = *mid << 1;
	if (*low & 0x80)
		(*mid)++;

	*low = *low << 1;

	*low = *low ^ *byte1;
	*mid = *mid ^ *byte2;

	if (xor_result) {
		*low = *low ^ 0x1B;
		*high = *high ^ 0x80;
	}

	return;
}

static void atmxt_active_handler(struct atmxt_driver_data *dd)
{
	int err = 0;
	int i = 0;
	uint8_t *msg_buf = NULL;
	int size = 0;
	char *contents = NULL;
	bool msg_fail = false;
	int last_err = 0;
	int msg_size = 0;
	bool inv_msg_seen = false;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Starting active handler...\n", __func__);

	msg_size = dd->data->max_msg_size;
	size = (dd->rdat->active_touches + 1) * msg_size;
	if (size == msg_size)
		size = msg_size * 2;

	msg_buf = kzalloc(sizeof(uint8_t) * size, GFP_KERNEL);
	if (msg_buf == NULL) {
		printk(KERN_ERR
			"%s: Unable to allocate memory for message buffer.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_active_handler_fail;
	}

	if (!(dd->status & (1 << ATMXT_SET_MESSAGE_POINTER))) {
		err = atmxt_i2c_write(dd, dd->addr->msg[0], dd->addr->msg[1],
			NULL, 0);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Failed to set message buffer pointer.\n",
				__func__);
			goto atmxt_active_handler_fail;
		}

		dd->status = dd->status | (1 << ATMXT_SET_MESSAGE_POINTER);
	}

	err = atmxt_i2c_read(dd, msg_buf, size);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to read messages.\n", __func__);
		goto atmxt_active_handler_fail;
	}

	if (msg_buf[0] == 0xFF) {
		contents = atmxt_msg2str(msg_buf, size);
		printk(KERN_ERR "%s: Received invalid data:  %s.\n",
			__func__, contents);
		err = -EINVAL;
		goto atmxt_active_handler_fail;
	}

	for (i = 0; i < size; i += msg_size) {
		if (msg_buf[i] == 0xFF) {
			atmxt_dbg(dd, ATMXT_DBG3, "%s: Reached 0xFF message.\n",
				__func__);
			inv_msg_seen = true;
			continue;
		}

		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Processing message %d...\n", __func__,
			(i + 1) / msg_size);

		if (inv_msg_seen) {
			printk(KERN_INFO "%s: %s %s (%d).\n", __func__,
				"System response time lagging",
				"IC report rate",
				(i / msg_size) + 1);
		}

		err = atmxt_process_message(dd, &(msg_buf[i]), msg_size);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Processing message %d failed %s %d.\n",
				__func__, (i / msg_size) + 1,
				"with error code", err);
			msg_fail = true;
			last_err = err;
		}
	}

	if (dd->status & (1 << ATMXT_RESTART_REQUIRED)) {
		printk(KERN_ERR "%s: Restarting touch IC...\n", __func__);
		dd->status = dd->status & ~(1 << ATMXT_RESTART_REQUIRED);
		err = atmxt_resume_restart(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to restart touch IC.\n",
				__func__);
			goto atmxt_active_handler_fail;
		} else if (msg_fail) {
			err = last_err;
			goto atmxt_active_handler_fail;
		} else {
			goto atmxt_active_handler_pass;
		}
	}

	if (dd->status & (1 << ATMXT_FIXING_CALIBRATION)) {
		err = atmxt_verify_ic_calibration_fix(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to verify IC calibration.\n",
				__func__);
			goto atmxt_active_handler_fail;
		}
	}

	if (dd->status & (1 << ATMXT_REPORT_TOUCHES)) {
		atmxt_report_touches(dd);
		dd->status = dd->status & ~(1 << ATMXT_REPORT_TOUCHES);
	}

	if (msg_fail) {
		err = last_err;
		goto atmxt_active_handler_fail;
	}

	goto atmxt_active_handler_pass;

atmxt_active_handler_fail:
	printk(KERN_ERR "%s: Touch active handler failed with error code %d.\n",
		__func__, err);

atmxt_active_handler_pass:
	kfree(msg_buf);
	kfree(contents);
	return;
}

static int atmxt_process_message(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size)
{
	int err = 0;
	char *contents = NULL;

	if (msg[0] <= (dd->info_blk->id_size-1)) {
		switch (dd->info_blk->msg_id[msg[0]]) {
		case 6:
			err = atmxt_message_handler6(dd, msg, size);
			break;
		case 9:
			err = atmxt_message_handler9(dd, msg, size);
			break;
		case 42:
			err = atmxt_message_handler42(dd, msg, size);
			break;
		default:
			contents = atmxt_msg2str(msg, size);
			printk(KERN_ERR "%s: Object %u sent this:  %s.\n",
				__func__, dd->info_blk->msg_id[msg[0]],
				contents);
			break;
		}
	} else {
		contents = atmxt_msg2str(msg, size);
		printk(KERN_ERR "%s: Received unknown message:  %s.\n",
			__func__, contents);
	}

	if (err < 0)
		printk(KERN_ERR "%s: Message processing failed.\n", __func__);

	kfree(contents);
	return err;
}

static void atmxt_report_touches(struct atmxt_driver_data *dd)
{
	int i = 0;
	int j = 0;
	int rval = 0;
	int id = 0;
	int x = 0;
	int y = 0;
	int p = 0;
	int w = 0;

	dd->rdat->active_touches = 0;

	for (i = 0; i < ATMXT_MAX_TOUCHES; i++) {
		if (!(dd->rdat->tchdat[i].active))
			continue;

		id =  dd->rdat->tchdat[i].id;
		x = dd->rdat->tchdat[i].x;
		y = dd->rdat->tchdat[i].y;
		p = dd->rdat->tchdat[i].p;
		w = dd->rdat->tchdat[i].w;

		dd->rdat->active_touches++;

		atmxt_dbg(dd, ATMXT_DBG1, "%s: ID=%d, X=%d, Y=%d, P=%d, W=%d\n",
			__func__, id, x, y, p, w);

		for (j = 0; j < ARRAY_SIZE(dd->rdat->axis); j++) {
			switch (j) {
			case 0:
				rval = x;
				break;
			case 1:
				rval = y;
				break;
			case 2:
				rval = p;
				break;
			case 3:
				rval = w;
				break;
			case 4:
				rval = id;
				break;
			}
			if (dd->rdat->axis[j] != ATMXT_ABS_RESERVED) {
				input_report_abs(dd->in_dev,
					dd->rdat->axis[j], rval);
			}
		}
		input_mt_sync(dd->in_dev);
	}

	if (dd->rdat->active_touches == 0)
		input_mt_sync(dd->in_dev);

	input_sync(dd->in_dev);

	return;
}

static void atmxt_release_touches(struct atmxt_driver_data *dd)
{
	int i = 0;

	atmxt_dbg(dd, ATMXT_DBG1, "%s: Releasing all touches...\n", __func__);

	for (i = 0; i < ATMXT_MAX_TOUCHES; i++)
		dd->rdat->tchdat[i].active = false;

	atmxt_report_touches(dd);
	dd->status = dd->status & ~(1 << ATMXT_REPORT_TOUCHES);

	return;
}

static int atmxt_message_handler6(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Handling message type 6...\n", __func__);

	if (size < 5) {
		printk(KERN_ERR "%s: Message size is too small.\n", __func__);
		err = -EINVAL;
		goto atmxt_message_handler6_fail;
	}

	if (msg[1] & 0x80) {
		printk(KERN_INFO "%s: Touch IC reset complete.\n", __func__);
		dd->data->last_stat = 0x00;
	}

	if ((msg[1] & 0x40) && !(dd->data->last_stat & 0x40)) {
		printk(KERN_ERR "%s: Acquisition cycle overflow.\n", __func__);
	} else if (!(msg[1] & 0x40) && (dd->data->last_stat & 0x40)) {
		printk(KERN_INFO "%s: Acquisition cycle now normal.\n",
			__func__);
	}

	if ((msg[1] & 0x20) && !(dd->data->last_stat & 0x20)) {
		printk(KERN_ERR "%s: Signal error in IC acquisition.\n",
			__func__);
	} else if (!(msg[1] & 0x20) && (dd->data->last_stat & 0x20)) {
		printk(KERN_INFO "%s: IC acquisition signal now in range.\n",
			__func__);
	}

	if ((msg[1] & 0x10) && !(dd->data->last_stat & 0x10)) {
		printk(KERN_INFO "%s: Touch IC is calibrating.\n", __func__);
		dd->status = dd->status | (1 << ATMXT_RECEIVED_CALIBRATION);
	} else if (!(msg[1] & 0x10) && (dd->data->last_stat & 0x10)) {
		printk(KERN_INFO "%s: Touch IC calibration complete.\n",
			__func__);
		dd->status = dd->status | (1 << ATMXT_RECEIVED_CALIBRATION);
	}

	if (msg[1] & 0x08) {
		printk(KERN_ERR "%s: Hardware configuration error--%s.\n",
			__func__, "check platform settings");
		dd->data->last_stat = dd->data->last_stat & 0xF7;
	} else if (!(msg[1] & 0x08) && (dd->data->last_stat & 0x08)) {
		printk(KERN_INFO
			"%s: Hardware configuration error corrected.\n",
			__func__);
	}

	if (msg[1] & 0x04) {
		printk(KERN_ERR "%s: IC reports I2C communication error.\n",
			__func__);
	}

	if (msg[1] == dd->data->last_stat) {
		printk(KERN_INFO "%s: Received checksum 0x%02X%02X%02X.\n",
			__func__, msg[2], msg[3], msg[4]);
	}

	dd->data->last_stat = msg[1];

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (dd->status & (1 << ATMXT_IGNORE_CHECKSUM))
		goto atmxt_message_handler6_fail;
#endif

	if ((msg[2] != dd->nvm->chksum[0]) ||
		(msg[3] != dd->nvm->chksum[1]) ||
		(msg[4] != dd->nvm->chksum[2])) {
		if (!(dd->status & (1 << ATMXT_CHECKSUM_FAILED))) {
			printk(KERN_ERR "%s: IC settings checksum fail.\n",
				__func__);
			dd->status = dd->status | (1 << ATMXT_RESTART_REQUIRED);
			dd->status = dd->status | (1 << ATMXT_CHECKSUM_FAILED);
		} else {
			printk(KERN_ERR "%s: IC settings checksum fail.  %s\n",
				__func__, "Sending settings (no backup)...");
			err = atmxt_send_settings(dd, false);
			if (err < 0) {
				printk(KERN_ERR
					"%s: Failed to update IC settings.\n",
					__func__);
				goto atmxt_message_handler6_fail;
			}
		}
	}

atmxt_message_handler6_fail:
	return err;
}

static int atmxt_message_handler9(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size)
{
	int err = 0;
	uint8_t tchidx = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Handling message type 9...\n", __func__);

	if (size < 7) {
		printk(KERN_ERR "%s: Message size is too small.\n", __func__);
		err = -EINVAL;
		goto atmxt_message_handler9_fail;
	}

	tchidx = msg[0] - dd->data->touch_id_offset;
	if (tchidx >= ARRAY_SIZE(dd->rdat->tchdat)) {
		printk(KERN_ERR "%s: Touch %hu is unsupported.\n",
			__func__, tchidx);
		err = -EOVERFLOW;
		goto atmxt_message_handler9_fail;
	}

	dd->status = dd->status | (1 << ATMXT_REPORT_TOUCHES);

	dd->rdat->tchdat[tchidx].id = tchidx;

	dd->rdat->tchdat[tchidx].x = (msg[2] << 4) | ((msg[4] & 0xF0) >> 4);
	if (!(dd->data->res[0]))
		dd->rdat->tchdat[tchidx].x = dd->rdat->tchdat[tchidx].x >> 2;

	dd->rdat->tchdat[tchidx].y = (msg[3] << 4) | (msg[4] & 0x0F);
	if (!(dd->data->res[1]))
		dd->rdat->tchdat[tchidx].y = dd->rdat->tchdat[tchidx].y >> 2;

	dd->rdat->tchdat[tchidx].p = msg[6];
	dd->rdat->tchdat[tchidx].w = msg[5];

	if (((msg[1] & 0x40) && (msg[1] & 0x20)) ||
		((msg[1] & 0x40) && (msg[1] & 0x02)) ||
		((msg[1] & 0x20) && (msg[1] & 0x02))) {
		printk(KERN_ERR "%s: System too slow %s 0x%02X.\n",
			__func__, "to see all touch events for report id",
			msg[0]);
	}

	if (msg[1] & 0x22) {
		atmxt_dbg(dd, ATMXT_DBG1, "%s: Touch ID %hu released.\n",
			__func__, tchidx);
		dd->rdat->tchdat[tchidx].active = false;
	} else {
		dd->rdat->tchdat[tchidx].active = true;
	}

	if (msg[1] & 0x02) {
		printk(KERN_INFO "%s: Touch ID %hu suppressed.\n",
			__func__, tchidx);
	}

atmxt_message_handler9_fail:
	return err;
}

static int atmxt_message_handler42(struct atmxt_driver_data *dd,
		uint8_t *msg, uint8_t size)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3,
		"%s: Handling message type 42...\n", __func__);

	if (size < 2) {
		printk(KERN_ERR "%s: Message size is too small.\n", __func__);
		err = -EINVAL;
		goto atmxt_message_handler42_fail;
	}

	if (msg[1] & 0x01) {
		printk(KERN_ERR "%s: Touch suppression is active.\n",
			__func__);
	} else {
		printk(KERN_INFO "%s: Touch suppression is disabled.\n",
			__func__);
	}

atmxt_message_handler42_fail:
	return err;
}

static int atmxt_resume_restart(struct atmxt_driver_data *dd)
{
	int err = 0;

	atmxt_dbg(dd, ATMXT_DBG3, "%s: Resume restarting IC...\n", __func__);

	if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
		disable_irq_nosync(dd->client->irq);
		dd->status = dd->status & ~(1 << ATMXT_IRQ_ENABLED_FLAG);
	}

	err = atmxt_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to restart the touch IC.\n",
			__func__);
		atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
		goto atmxt_resume_restart_fail;
	}

	if (!(dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG))) {
		if (atmxt_get_drv_state(dd) != ATMXT_DRV_ACTIVE)
			atmxt_set_drv_state(dd, ATMXT_DRV_ACTIVE);
		dd->status = dd->status | (1 << ATMXT_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
	}

atmxt_resume_restart_fail:
	return err;
}

static int atmxt_force_bootloader(struct atmxt_driver_data *dd)
{
	int err = 0;
	int i = 0;
	uint8_t cmd = 0x00;
	bool chg_used = false;
	uint8_t chg_cmd[2] = {0x00, 0x00};
	uint8_t rst[2] = {0x00, 0x00};

	for (i = 0; i < dd->info_blk->size; i += 6) {
		if (dd->info_blk->data[i+0] == 18) {
			if (dd->info_blk->data[i+3] < 1) {
				atmxt_dbg(dd, ATMXT_DBG3,
					"%s: Comm is too small--%s.\n",
					__func__, "will pool instead");
				goto atmxt_force_bootloader_check_reset;
			}
			chg_cmd[0] = dd->info_blk->data[i+1] + 1;
			chg_cmd[1] = dd->info_blk->data[i+2];
			if (chg_cmd[0] < dd->info_blk->data[i+1])
				chg_cmd[1]++;
			break;
		}
	}

	if ((chg_cmd[0] == 0x00) && (chg_cmd[1] == 0x00)) {
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: No interrupt force available--%s.\n",
			__func__, "will pool instead");
		goto atmxt_force_bootloader_check_reset;
	}

	cmd = 0x02;
	err = atmxt_i2c_write(dd, chg_cmd[0], chg_cmd[1], &cmd, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to force interrupt low--%s.\n",
			__func__, "will poll instead");
	} else {
		chg_used = true;
	}

atmxt_force_bootloader_check_reset:
	for (i = 0; i < dd->info_blk->size; i += 6) {
		if (dd->info_blk->data[i+0] == 6) {
			rst[0] = dd->info_blk->data[i+1];
			rst[1] = dd->info_blk->data[i+2];
			break;
		}
	}

	if ((rst[0] == 0x00) && (rst[1] == 0x00)) {
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: No soft reset available--%s.\n", __func__,
			"will try hardware recovery instead");
		goto atmxt_force_bootloader_use_recov;
	}

	cmd = 0xA5;
	err = atmxt_i2c_write(dd, rst[0], rst[1], &cmd, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to send flash reset command.\n",
			__func__);
		goto atmxt_force_bootloader_use_recov;
	}

	if (!chg_used) {
		for (i = 0; i < 128; i++) {
			if (gpio_get_value(dd->pdata->gpio_interrupt) == 1)
				break;
			else
				udelay(2000);
		}

		if (i == 128) {
			printk(KERN_ERR "%s: %s.\n", __func__,
				"Waiting for flash reset timed out");
			err = -ETIME;
			goto atmxt_force_bootloader_exit;
		}
	}

	goto atmxt_force_bootloader_exit;

atmxt_force_bootloader_use_recov:
	atmxt_dbg(dd, ATMXT_DBG2, "%s: Using hardware recovery...\n", __func__);
	printk(KERN_ERR "%s: Forced hardware recovery failed--%s.\n",
		__func__, "unable to reflash IC");
	err = -ENOSYS;

atmxt_force_bootloader_exit:
	return err;
}

static bool atmxt_check_firmware_update(struct atmxt_driver_data *dd)
{
	bool update_fw = false;

	if ((dd->util->fw[1] != dd->info_blk->header[0]) ||
		(dd->util->fw[2] != dd->info_blk->header[1])) {
		printk(KERN_ERR
			"%s: Platform firmware does not match touch IC.  %s\n",
			__func__, "Unable to check for firmware update.");
		goto atmxt_check_firmware_update_exit;
	}

	update_fw = !((dd->util->fw[3] == dd->info_blk->header[2]) &&
		(dd->util->fw[4] == dd->info_blk->header[3]));

atmxt_check_firmware_update_exit:
	return update_fw;
}

static int atmxt_validate_firmware(uint8_t *data, uint32_t size)
{
	int err = 0;
	uint32_t iter = 0;
	int length = 0;

	if (data == NULL || size == 0) {
		printk(KERN_ERR "%s: No firmware data found.\n", __func__);
		err = -ENODATA;
		goto atmxt_validate_firmware_fail;
	}

	if (data[0] < 4) {
		printk(KERN_ERR "%s: Invalid firmware header.\n", __func__);
		err = -EINVAL;
		goto atmxt_validate_firmware_fail;
	} else if ((data[0] + 1) >= size) {
		printk(KERN_ERR "%s: Firmware is malformed.\n", __func__);
		err = -EOVERFLOW;
		goto atmxt_validate_firmware_fail;
	}

	iter = iter + data[0] + 1;

	while (iter < (size - 1)) {
		length = (data[iter+0] << 8) | data[iter+1];
		if ((iter + length + 2) > size) {
			printk(KERN_ERR
				"%s: Overflow in firmware image %s %u.\n",
				__func__, "on iter", iter);
			err = -EOVERFLOW;
			goto atmxt_validate_firmware_fail;
		}

		iter = iter + length + 2;
	}

	if (iter != size) {
		printk(KERN_ERR "%s: Firmware image misaligned.\n", __func__);
		err = -ENOEXEC;
		goto atmxt_validate_firmware_fail;
	}

atmxt_validate_firmware_fail:
	return err;
}

static int atmxt_flash_firmware(struct atmxt_driver_data *dd)
{
	int err = 0;
	uint8_t *img = dd->util->fw;
	uint32_t size = dd->util->fw_size;
	uint32_t iter = 0;
	uint8_t status = 0x00;
	bool irq_low = false;
	bool frame_crc_failed = false;

	printk(KERN_INFO "%s: Reflashing touch IC...\n", __func__);

	err = atmxt_i2c_write(dd, 0xDC, 0xAA, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to send unlock command.\n",
			__func__);
		goto atmxt_flash_firmware_fail;
	}

	iter = img[0] + 1;
	while (iter < (size - 1)) {
		irq_low = atmxt_wait4irq(dd);
		if (!irq_low) {
			printk(KERN_ERR
				"%s: Timeout waiting %s for iter %u.\n",
				__func__, "for frame interrupt", iter);
			err = -ETIME;
			goto atmxt_flash_firmware_fail;
		}

		err = atmxt_i2c_read(dd, &status, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Error reading frame byte for iter %u.\n",
				__func__, iter);
			goto atmxt_flash_firmware_fail;
		}

		if ((status & 0xC0) != 0x80) {
			printk(KERN_ERR "%s: %s 0x%02X %s %u.\n", __func__,
				"Unexpected wait status", status,
				"received for iter", iter);
			err = -EPROTO;
			goto atmxt_flash_firmware_fail;
		}

		err = atmxt_i2c_write(dd, img[iter+0], img[iter+1],
			&(img[iter+2]), (img[iter+0] << 8) | img[iter+1]);
		if (err < 0) {
			printk(KERN_ERR "%s: Error sending frame iter %u.\n",
				__func__, iter);
			goto atmxt_flash_firmware_fail;
		}

		irq_low = atmxt_wait4irq(dd);
		if (!irq_low) {
			printk(KERN_ERR
				"%s: Timeout waiting %s for iter %u.\n",
				__func__, "for check interrupt", iter);
			err = -ETIME;
			goto atmxt_flash_firmware_fail;
		}

		err = atmxt_i2c_read(dd, &status, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Error reading check byte for iter %u.\n",
				__func__, iter);
			goto atmxt_flash_firmware_fail;
		}

		if (status != 0x02) {
			printk(KERN_ERR "%s: %s 0x%02X %s %u.\n", __func__,
				"Unexpected frame status", status,
				"received for iter", iter);
			err = -EPROTO;
			goto atmxt_flash_firmware_fail;
		}

		irq_low = atmxt_wait4irq(dd);
		if (!irq_low) {
			printk(KERN_ERR
				"%s: Timeout waiting %s for iter %u.\n",
				__func__, "for result interrupt", iter);
			err = -ETIME;
			goto atmxt_flash_firmware_fail;
		}

		err = atmxt_i2c_read(dd, &status, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Error reading result byte for iter %u.\n",
				__func__, iter);
			goto atmxt_flash_firmware_fail;
		}

		if (status == 0x04) {
			iter = iter + ((img[iter+0] << 8) | img[iter+1]) + 2;
			frame_crc_failed = false;
		} else if (!frame_crc_failed) {
			printk(KERN_ERR "%s: %s %u--%s.\n",
				__func__, "Frame CRC failed for iter",
				iter, "will try to re-send");
			frame_crc_failed = true;
		} else {
			printk(KERN_ERR "%s: %s %u--%s.\n",
				__func__, "Frame CRC failed for iter",
				iter, "check firmware image");
			err = -ECOMM;
			goto atmxt_flash_firmware_fail;
		}
	}

atmxt_flash_firmware_fail:
	return err;
}

static char *atmxt_msg2str(const uint8_t *msg, uint8_t size)
{
	char *str = NULL;
	int i = 0;
	int err = 0;

	str = kzalloc(sizeof(char) * (size * 5), GFP_KERNEL);
	if (str == NULL) {
		printk(KERN_ERR "%s: Failed to allocate message string.\n",
			__func__);
		goto atmxt_msg2str_exit;
	}

	for (i = 0; i < size; i++) {
		err = sprintf(str, "%s0x%02X ", str, msg[i]);
		if (err < 0) {
			printk(KERN_ERR "%s: Error in sprintf on pass %d",
				__func__, i);
			goto atmxt_msg2str_exit;
		}
	}

	str[err-1] = '\0';

atmxt_msg2str_exit:
	return str;
}

static bool atmxt_wait4irq(struct atmxt_driver_data *dd)
{
	bool irq_low = false;
	int i = 0;

	for (i = 0; i < 500; i++) {
		if (gpio_get_value(dd->pdata->gpio_interrupt) != 0) {
			msleep(20);
		} else {
			irq_low = true;
			break;
		}
	}

	return irq_low;
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t atmxt_debug_drv_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Debug Level: %hu\n", dd->dbg->dbg_lvl);
}
static ssize_t atmxt_debug_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto atmxt_debug_drv_debug_store_exit;
	}

	if (value > 255) {
		printk(KERN_ERR "%s: Invalid debug level %lu--setting to %u.\n",
			__func__, value, ATMXT_DBG3);
		dd->dbg->dbg_lvl = ATMXT_DBG3;
	} else {
		dd->dbg->dbg_lvl = value;
		printk(KERN_INFO "%s: Debug level is now %hu.\n",
			__func__, dd->dbg->dbg_lvl);
	}

	err = size;

atmxt_debug_drv_debug_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(drv_debug, S_IRUSR | S_IWUSR,
	atmxt_debug_drv_debug_show, atmxt_debug_drv_debug_store);

static ssize_t atmxt_debug_drv_flags_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Driver Flags: 0x%04X\n", dd->settings);
}
static ssize_t atmxt_debug_drv_flags_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 16, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto atmxt_debug_drv_flags_store_exit;
	}

	if (value > 65535) {
		printk(KERN_ERR "%s: Invalid flag settings 0x%08lX passed.\n",
			__func__, value);
		err = -EOVERFLOW;
		goto atmxt_debug_drv_flags_store_exit;
	} else {
		dd->settings = value;
		atmxt_dbg(dd, ATMXT_DBG3,
			"%s: Driver flags now set to 0x%04X.\n",
			__func__, dd->settings);
	}

	err = size;

atmxt_debug_drv_flags_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(drv_flags, S_IRUSR | S_IWUSR,
	atmxt_debug_drv_flags_show, atmxt_debug_drv_flags_store);
#endif

static ssize_t atmxt_debug_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG))
		return sprintf(buf, "Driver interrupt is ENABLED.\n");
	else
		return sprintf(buf, "Driver interrupt is DISABLED.\n");
}
static ssize_t atmxt_debug_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto atmxt_debug_drv_irq_store_exit;
	}

	if ((atmxt_get_drv_state(dd) != ATMXT_DRV_ACTIVE) &&
		(atmxt_get_drv_state(dd) != ATMXT_DRV_IDLE)) {
		printk(KERN_ERR "%s: %s %s or %s states.\n",
			__func__, "Interrupt can be changed only in",
			atmxt_driver_state_string[ATMXT_DRV_ACTIVE],
			atmxt_driver_state_string[ATMXT_DRV_IDLE]);
		err = -EACCES;
		goto atmxt_debug_drv_irq_store_exit;
	}

	switch (value) {
	case 0:
		if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
			dd->status =
				dd->status & ~(1 << ATMXT_IRQ_ENABLED_FLAG);
		}
		break;

	case 1:
		if (!(dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG))) {
			dd->status =
				dd->status | (1 << ATMXT_IRQ_ENABLED_FLAG);
			enable_irq(dd->client->irq);
			atmxt_set_drv_state(dd, ATMXT_DRV_ACTIVE);
		}
		break;

	default:
		printk(KERN_ERR "%s: Invalid value passed.\n", __func__);
		err = -EINVAL;
		goto atmxt_debug_drv_irq_store_exit;
	}

	err = size;

atmxt_debug_drv_irq_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(drv_irq, S_IRUSR | S_IWUSR,
	atmxt_debug_drv_irq_show, atmxt_debug_drv_irq_store);

static ssize_t atmxt_debug_drv_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Driver state is %s.\nIC state is %s.\n",
		atmxt_driver_state_string[atmxt_get_drv_state(dd)],
		atmxt_ic_state_string[atmxt_get_ic_state(dd)]);
}
static DEVICE_ATTR(drv_stat, S_IRUGO, atmxt_debug_drv_stat_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t atmxt_debug_drv_tdat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	if (dd->status & (1 << ATMXT_WAITING_FOR_TDAT))
		return sprintf(buf, "Driver is waiting for data load.\n");
	else
		return sprintf(buf, "No data loading in progress.\n");
}
static ssize_t atmxt_debug_drv_tdat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	mutex_lock(dd->mutex);

	if (dd->status & (1 << ATMXT_WAITING_FOR_TDAT)) {
		printk(KERN_ERR "%s: Driver is already waiting for data.\n",
			__func__);
		err = -EALREADY;
		goto atmxt_debug_drv_tdat_store_fail;
	}

	printk(KERN_INFO "%s: Enabling firmware class loader...\n", __func__);

	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_NOHOTPLUG, "", &(dd->client->dev),
		GFP_KERNEL, dd, atmxt_tdat_callback);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Firmware request failed with error code %d.\n",
			__func__, err);
		goto atmxt_debug_drv_tdat_store_fail;
	}

	dd->status = dd->status | (1 << ATMXT_WAITING_FOR_TDAT);
	err = size;

atmxt_debug_drv_tdat_store_fail:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(drv_tdat, S_IRUSR | S_IWUSR,
	atmxt_debug_drv_tdat_show, atmxt_debug_drv_tdat_store);
#endif

static ssize_t atmxt_debug_drv_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Driver: %s\nVersion: %s\nDate: %s\n",
		ATMXT_I2C_NAME, ATMXT_DRIVER_VERSION, ATMXT_DRIVER_DATE);
}
static DEVICE_ATTR(drv_ver, S_IRUGO, atmxt_debug_drv_ver_show, NULL);

static ssize_t atmxt_debug_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	err = gpio_get_value(dd->pdata->gpio_interrupt);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to read irq level with error code %d.\n",
			__func__, err);
		err = sprintf(buf,
			"Failed to read irq level with error code %d.\n",
			err);
		goto atmxt_debug_hw_irqstat_show_exit;
	}

	switch (err) {
	case 0:
		err = sprintf(buf, "Interrupt line is LOW.\n");
		break;
	case 1:
		err = sprintf(buf, "Interrupt line is HIGH.\n");
		break;
	default:
		err = sprintf(buf, "Read irq level of %d.\n", err);
		break;
	}

atmxt_debug_hw_irqstat_show_exit:
	return err;
}
static DEVICE_ATTR(hw_irqstat, S_IRUGO, atmxt_debug_hw_irqstat_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t atmxt_debug_hw_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	mutex_lock(dd->mutex);

	if (atmxt_get_drv_state(dd) == ATMXT_DRV_INIT) {
		printk(KERN_ERR "%s: %s %s.\n", __func__,
			"Unable to restart IC in driver state",
			atmxt_driver_state_string[ATMXT_DRV_INIT]);
		err = -EACCES;
		goto atmxt_debug_hw_reset_store_fail;
	}

	if (dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)) {
		disable_irq_nosync(dd->client->irq);
		dd->status = dd->status & ~(1 << ATMXT_IRQ_ENABLED_FLAG);
	}

	err = atmxt_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to %s with error code %d.\n",
			__func__, "re-initialize the touch IC", err);
		atmxt_set_drv_state(dd, ATMXT_DRV_IDLE);
		goto atmxt_debug_hw_reset_store_fail;
	}

	if (((atmxt_get_drv_state(dd) == ATMXT_DRV_ACTIVE)) &&
		(!(dd->status & (1 << ATMXT_IRQ_ENABLED_FLAG)))) {
		dd->status = dd->status | (1 << ATMXT_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
	}

	err = size;

atmxt_debug_hw_reset_store_fail:
	mutex_unlock(dd->mutex);
	return err;
}
static DEVICE_ATTR(hw_reset, S_IWUSR, NULL, atmxt_debug_hw_reset_store);
#endif

static ssize_t atmxt_debug_hw_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Touch Data File: %s\n", dd->pdata->filename);
}
static DEVICE_ATTR(hw_ver, S_IRUGO, atmxt_debug_hw_ver_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t atmxt_debug_ic_grpdata_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	int i = 0;
	uint8_t addr_lo = 0x00;
	uint8_t addr_hi = 0x00;
	uint8_t *entry = NULL;
	int size = 0;
	uint8_t *data_in = NULL;

	mutex_lock(dd->mutex);

	if ((atmxt_get_ic_state(dd) != ATMXT_IC_ACTIVE) &&
		(atmxt_get_ic_state(dd) != ATMXT_IC_SLEEP)) {
		printk(KERN_ERR "%s: %s %s or %s states.\n", __func__,
			"Group data can be read only in IC",
			atmxt_ic_state_string[ATMXT_IC_ACTIVE],
			atmxt_ic_state_string[ATMXT_IC_SLEEP]);
		err = sprintf(buf, "%s %s or %s states.\n",
			"Group data can be read only in IC",
			atmxt_ic_state_string[ATMXT_IC_ACTIVE],
			atmxt_ic_state_string[ATMXT_IC_SLEEP]);
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	for (i = 0; i < dd->info_blk->size; i += 6) {
		if (dd->info_blk->data[i+0] == dd->dbg->grp_num) {
			entry = &(dd->info_blk->data[i]);
			break;
		}
	}

	if (entry == NULL) {
		printk(KERN_ERR "%s: Group %hu does not exist.\n",
			__func__, dd->dbg->grp_num);
		err = sprintf(buf, "Group %hu does not exist.\n",
			dd->dbg->grp_num);
			goto atmxt_debug_ic_grpdata_show_exit;
	}

	if (dd->dbg->grp_off > entry[3]) {
		printk(KERN_ERR "%s: Offset %hu exceeds group size of %u.\n",
			__func__, dd->dbg->grp_off, entry[3]+1);
		err = sprintf(buf, "Offset %hu exceeds group size of %u.\n",
			dd->dbg->grp_off, entry[3]+1);
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	addr_lo = entry[1] + dd->dbg->grp_off;
	addr_hi = entry[2];
	if (addr_lo < entry[1])
		addr_hi++;

	err = atmxt_i2c_write(dd, addr_lo, addr_hi, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to set group pointer with error code %d.\n",
			__func__, err);
		err = sprintf(buf,
			"Failed to set group pointer with error code %d.\n",
			err);
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	size = entry[3] - dd->dbg->grp_off + 1;
	data_in = kzalloc(sizeof(uint8_t) * size, GFP_KERNEL);
	if (data_in == NULL) {
		printk(KERN_ERR "%s: Unable to allocate memory buffer.\n",
			__func__);
		err = sprintf(buf, "Unable to allocate memory buffer.\n");
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	err = atmxt_i2c_read(dd, data_in, size);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to read group data.\n", __func__);
		err = sprintf(buf, "Failed to read group data.\n");
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	err = sprintf(buf, "Group %hu, Offset %hu:\n",
		dd->dbg->grp_num, dd->dbg->grp_off);
	if (err < 0) {
		printk(KERN_ERR "%s: Error in header sprintf.\n", __func__);
		goto atmxt_debug_ic_grpdata_show_exit;
	}

	for (i = 0; i < size; i++) {
		err = sprintf(buf, "%s0x%02hX\n", buf, data_in[i]);
		if (err < 0) {
			printk(KERN_ERR "%s: Error in sprintf loop %d.\n",
				__func__, i);
			goto atmxt_debug_ic_grpdata_show_exit;
		}
	}

	err = sprintf(buf, "%s(%u bytes)\n", buf, size);
	if (err < 0) {
		printk(KERN_ERR "%s: Error in byte count sprintf.\n", __func__);
		goto atmxt_debug_ic_grpdata_show_exit;
	}

atmxt_debug_ic_grpdata_show_exit:
	kfree(data_in);
	mutex_unlock(dd->mutex);

	return (ssize_t) err;
}
static ssize_t atmxt_debug_ic_grpdata_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	int i = 0;
	uint8_t addr_lo = 0x00;
	uint8_t addr_hi = 0x00;
	uint8_t *entry = NULL;
	int data_size = 0;
	uint8_t *data_out = NULL;
	unsigned long value = 0;
	uint8_t *conv_buf = NULL;

	mutex_lock(dd->mutex);

	if ((atmxt_get_ic_state(dd) != ATMXT_IC_ACTIVE) &&
		(atmxt_get_ic_state(dd) != ATMXT_IC_SLEEP)) {
		printk(KERN_ERR "%s: %s %s or %s states.\n", __func__,
			"Group data can be written only in IC",
			atmxt_ic_state_string[ATMXT_IC_ACTIVE],
			atmxt_ic_state_string[ATMXT_IC_SLEEP]);
		err = -EACCES;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	for (i = 0; i < dd->info_blk->size; i += 6) {
		if (dd->info_blk->data[i+0] == dd->dbg->grp_num) {
			entry = &(dd->info_blk->data[i]);
			break;
		}
	}

	if (entry == NULL) {
		printk(KERN_ERR "%s: Group %hu does not exist.\n",
			__func__, dd->dbg->grp_num);
		err = -ENOENT;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	if ((dd->dbg->grp_off) > entry[3]) {
		printk(KERN_ERR "%s: Offset %hu exceeds data size.\n",
			__func__, dd->dbg->grp_off);
		err = -EOVERFLOW;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	if ((size % 5 != 0) || (size == 0)) {
		printk(KERN_ERR "%s: Invalid data format.  %s\n",
			 __func__, "Use \"0xHH,0xHH,...,0xHH\" instead.");
		err = -EINVAL;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	data_out = kzalloc(sizeof(uint8_t) * (size / 5), GFP_KERNEL);
	if (data_out == NULL) {
		printk(KERN_ERR "%s: Unable to allocate output buffer.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	conv_buf = kzalloc(sizeof(uint8_t) * 5, GFP_KERNEL);
	if (conv_buf == NULL) {
		printk(KERN_ERR "%s: Unable to allocate conversion buffer.\n",
			__func__);
		err = -ENOMEM;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	for (i = 0; i < size; i += 5) {
		memcpy(conv_buf, &(buf[i]), 4);
		err = strict_strtoul(conv_buf, 16, &value);
		if (err < 0) {
			printk(KERN_ERR "%s: Argument conversion failed.\n",
				__func__);
			goto atmxt_debug_ic_grpdata_store_exit;
		} else if (value > 255) {
			printk(KERN_ERR "%s: Value 0x%lX is too large.\n",
				__func__, value);
			err = -EOVERFLOW;
			goto atmxt_debug_ic_grpdata_store_exit;
		}

		data_out[data_size] = value;
		data_size++;
	}

	if ((dd->dbg->grp_off + data_size) > (entry[3] + 1)) {
		printk(KERN_ERR "%s: Trying to write %d bytes at offset %hu, "
			"which exceeds group size of %hu.\n", __func__,
			 data_size, dd->dbg->grp_off, entry[3]+1);
		err = -EOVERFLOW;
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	addr_lo = entry[1] + dd->dbg->grp_off;
	addr_hi = entry[2];
	if (addr_lo < entry[1])
		addr_hi++;

	err = atmxt_i2c_write(dd, addr_lo, addr_hi, data_out, data_size);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to write data with error code %d.\n",
			__func__, err);
		goto atmxt_debug_ic_grpdata_store_exit;
	}

	if (!(dd->status & (1 << ATMXT_IGNORE_CHECKSUM))) {
		printk(KERN_INFO
			"%s: Disabled settings checksum verification %s.\n",
			__func__, "until next boot");
	}
	dd->status = dd->status | (1 << ATMXT_IGNORE_CHECKSUM);

	err = size;

atmxt_debug_ic_grpdata_store_exit:
	kfree(data_out);
	kfree(conv_buf);
	mutex_unlock(dd->mutex);

	return (ssize_t) err;
}
static DEVICE_ATTR(ic_grpdata, S_IRUSR | S_IWUSR,
	atmxt_debug_ic_grpdata_show, atmxt_debug_ic_grpdata_store);

static ssize_t atmxt_debug_ic_grpnum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Group: %hu\n", dd->dbg->grp_num);
}
static ssize_t atmxt_debug_ic_grpnum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto atmxt_debug_ic_grpnum_store_exit;
	}

	if (value > 255) {
		printk(KERN_ERR "%s: Invalid group number %lu--%s.\n",
			__func__, value, "setting to 255");
		dd->dbg->grp_num = 255;
	} else {
		dd->dbg->grp_num = value;
	}

	err = size;

atmxt_debug_ic_grpnum_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(ic_grpnum, S_IRUSR | S_IWUSR,
	atmxt_debug_ic_grpnum_show, atmxt_debug_ic_grpnum_store);

static ssize_t atmxt_debug_ic_grpoffset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Offset: %hu\n", dd->dbg->grp_off);
}
static ssize_t atmxt_debug_ic_grpoffset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto atmxt_debug_ic_grpoffset_store_exit;
	}

	if (value > 255) {
		printk(KERN_ERR "%s: Invalid offset %lu--setting to 255.\n",
			__func__, value);
		dd->dbg->grp_off = 255;
	} else {
		dd->dbg->grp_off = value;
	}

	err = size;

atmxt_debug_ic_grpoffset_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static DEVICE_ATTR(ic_grpoffset, S_IRUSR | S_IWUSR,
	atmxt_debug_ic_grpoffset_show, atmxt_debug_ic_grpoffset_store);
#endif

static ssize_t atmxt_debug_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct atmxt_driver_data *dd = dev_get_drvdata(dev);

	if (dd->info_blk == NULL) {
		return sprintf(buf,
			"No touch IC version information is available.\n");
	} else {
		return sprintf(buf, "%s0x%02X\n%s0x%02X\n%s0x%02X\n%s0x%02X\n",
			"Family ID: ", dd->info_blk->header[0],
			"Variant ID: ", dd->info_blk->header[1],
			"Version: ", dd->info_blk->header[2],
			"Build: ", dd->info_blk->header[3]);
	}
}
static DEVICE_ATTR(ic_ver, S_IRUGO, atmxt_debug_ic_ver_show, NULL);

static int atmxt_create_sysfs_files(struct atmxt_driver_data *dd)
{
	int err = 0;
	int check = 0;

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_drv_debug);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_debug.\n", __func__);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_flags);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_flags.\n", __func__);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_irq);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_irq.\n", __func__);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_stat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_stat.\n", __func__);
		err = check;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_drv_tdat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_tdat.\n", __func__);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_ver.\n", __func__);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_hw_irqstat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_irqstat.\n", __func__);
		err = check;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_hw_reset);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_reset.\n", __func__);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_hw_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_ver.\n", __func__);
		err = check;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_ic_grpdata);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create ic_grpdata.\n", __func__);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_ic_grpnum);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create ic_grpnum.\n", __func__);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_ic_grpoffset);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create ic_grpoffset.\n",
			__func__);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_ic_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create ic_ver.\n", __func__);
		err = check;
	}

	return err;
}

static void atmxt_remove_sysfs_files(struct atmxt_driver_data *dd)
{
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(&(dd->client->dev), &dev_attr_drv_debug);
	device_remove_file(&(dd->client->dev), &dev_attr_drv_flags);
#endif
	device_remove_file(&(dd->client->dev), &dev_attr_drv_irq);
	device_remove_file(&(dd->client->dev), &dev_attr_drv_stat);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(&(dd->client->dev), &dev_attr_drv_tdat);
#endif
	device_remove_file(&(dd->client->dev), &dev_attr_drv_ver);
	device_remove_file(&(dd->client->dev), &dev_attr_hw_irqstat);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(&(dd->client->dev), &dev_attr_hw_reset);
#endif
	device_remove_file(&(dd->client->dev), &dev_attr_hw_ver);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	device_remove_file(&(dd->client->dev), &dev_attr_ic_grpdata);
	device_remove_file(&(dd->client->dev), &dev_attr_ic_grpnum);
	device_remove_file(&(dd->client->dev), &dev_attr_ic_grpoffset);
#endif
	device_remove_file(&(dd->client->dev), &dev_attr_ic_ver);
	return;
}
