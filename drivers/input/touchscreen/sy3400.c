/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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

/* Driver for Synaptics S3400 touchscreens that uses tdat files */
#include "sy3400.h"
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

static int sy3400_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int sy3400_remove(struct i2c_client *client);
static int sy3400_suspend(struct i2c_client *client, pm_message_t message);
static int sy3400_resume(struct i2c_client *client);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void sy3400_early_suspend(struct early_suspend *handler);
static void sy3400_late_resume(struct early_suspend *handler);
#endif
static int __devinit sy3400_init(void);
static void __devexit sy3400_exit(void);
static void sy3400_free(struct sy3400_driver_data *dd);
static void sy3400_free_ic_data(struct sy3400_driver_data *dd);
static void sy3400_set_drv_state(struct sy3400_driver_data *dd,
		enum sy3400_driver_state state);
static int sy3400_get_drv_state(struct sy3400_driver_data *dd);
static void sy3400_set_ic_state(struct sy3400_driver_data *dd,
		enum sy3400_ic_state state);
static int sy3400_get_ic_state(struct sy3400_driver_data *dd);
static int sy3400_verify_pdata(struct sy3400_driver_data *dd);
static int sy3400_request_tdat(struct sy3400_driver_data *dd);
static int sy3400_gpio_init(struct sy3400_driver_data *dd);
static int sy3400_request_irq(struct sy3400_driver_data *dd);
static int sy3400_i2c_write(struct sy3400_driver_data *dd,
		uint8_t addr, uint8_t *buf, int size);
static int sy3400_i2c_read(struct sy3400_driver_data *dd,
		uint8_t *buf, int size);
static void sy3400_tdat_callback(const struct firmware *tdat,
		void *context);
static int sy3400_validate_tdat(const struct firmware *tdat);
static int sy3400_validate_settings(uint8_t *data, uint32_t size);
static int sy3400_register_inputs(struct sy3400_driver_data *dd,
		uint8_t *rdat, int rsize);
static int sy3400_restart_ic(struct sy3400_driver_data *dd);
static int sy3400_get_pdt(struct sy3400_driver_data *dd);
static int sy3400_validate_f01(struct sy3400_driver_data *dd);
static int sy3400_validate_f34(struct sy3400_driver_data *dd);
static int sy3400_validate_f12(struct sy3400_driver_data *dd);
static int sy3400_validate_f1A(struct sy3400_driver_data *dd);
static int sy3400_get_verinfo(struct sy3400_driver_data *dd);
static int sy3400_check_bootloader(struct sy3400_driver_data *dd, bool *bl);
static bool sy3400_check_firmware(struct sy3400_driver_data *dd);
static int sy3400_check_settings(struct sy3400_driver_data *dd, bool *sett);
static int sy3400_set_ic_data(struct sy3400_driver_data *dd);
static int sy3400_create_irq_table(struct sy3400_driver_data *dd);
static irqreturn_t sy3400_isr(int irq, void *handle);
static void sy3400_active_handler(struct sy3400_driver_data *dd);
static int sy3400_process_irq(struct sy3400_driver_data *dd,
		uint8_t irq_bit);
static int sy3400_irq_handler01(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size);
static int sy3400_irq_handler12(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size);
static int sy3400_irq_handler1A(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size);
static void sy3400_report_touches(struct sy3400_driver_data *dd);
static void sy3400_release_touches(struct sy3400_driver_data *dd);
static int sy3400_resume_restart(struct sy3400_driver_data *dd);
static int sy3400_validate_firmware(uint8_t *data, uint32_t size);
static int sy3400_enable_reflashing(struct sy3400_driver_data *dd);
static int sy3400_flash_record(struct sy3400_driver_data *dd, int type);
#ifdef CONFIG_TOUCHSCREEN_DEBUG
static char *sy3400_msg2str(const uint8_t *msg, int size);
#endif
static bool sy3400_wait4irq(struct sy3400_driver_data *dd);
static int sy3400_create_sysfs_files(struct sy3400_driver_data *dd);
static void sy3400_remove_sysfs_files(struct sy3400_driver_data *dd);

static int sy3400_purge_flag(struct notifier_block *nb,
		unsigned long event, void *ignore)
{
	int value;
	struct sy3400_driver_data *dd;
	dd = container_of(nb, struct sy3400_driver_data, panel_nb);
	switch (event) {
	case MMI_PANEL_EVENT_PWR_OFF:
		value = 1; /* set purge flag */
			break;
	case MMI_PANEL_EVENT_POST_INIT:
		value = 0; /* clear purge flag */
			break;
	default: return 1; /* not interested at this time */
	}
	atomic_set(&dd->purge_flag, value);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sy3400_match_tbl[] = {
	{ .compatible = "synaptics,sy3400" },
	{ },
};
MODULE_DEVICE_TABLE(of, sy3400_match_tbl);
#endif

static const struct i2c_device_id sy3400_id[] = {
	/* This name must match the i2c_board_info name */
	{ SY3400_I2C_NAME, 0 },
	{ /* END OF LIST */ }
};

MODULE_DEVICE_TABLE(i2c, sy3400_id);

static struct i2c_driver sy3400_driver = {
	.driver = {
		.name = SY3400_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sy3400_match_tbl),
	},
	.probe = sy3400_probe,
	.remove = __devexit_p(sy3400_remove),
	.id_table = sy3400_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = sy3400_suspend,
	.resume = sy3400_resume,
#endif
};

#ifdef CONFIG_OF
static struct touch_platform_data *
sy3400_of_init(struct i2c_client *client)
{
	struct touch_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	const char *fp = NULL;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	of_property_read_string(np, "synaptics,sy3400-tdat-filename", &fp);
	of_property_read_u32(np, "always_on_capable",
			 &pdata->always_on_capable);
	of_property_read_u32(np, "synaptics,purge-enabled",
			 &pdata->purge_enabled);

	pdata->filename = (char *)fp;
	pdata->gpio_interrupt = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);
	pdata->gpio_enable = of_get_gpio(np, 2);

	return pdata;
}
#else
static inline struct touch_platform_data *
sy3400_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static void sy3400_hw_ic_reset(struct sy3400_driver_data *dd)
{
	sy3400_dbg(dd, SY3400_DBG2,
			"%s: Resetting touch IC...\n", __func__);
	gpio_set_value(dd->pdata->gpio_reset, 0);
	udelay(SY3400_IC_RESET_HOLD_TIME);
	gpio_set_value(dd->pdata->gpio_reset, 1);
	msleep(SY3400_BL_HOLDOFF_TIME);
}

static int sy3400_detect(struct sy3400_driver_data *dd)
{
	int err;
	uint16_t blk_sz, blk_ct;
	uint8_t qaddr;

	err = sy3400_get_pdt(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to find touch IC.\n", __func__);
		goto sy3400_detect_fail;
	}

	err = sy3400_validate_f34(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Function $34 failed validation.\n",
			__func__);
		goto sy3400_detect_fail;
	}

	qaddr = dd->pblk->query_base_addr;
	if (qaddr == 0) {
		printk(KERN_ERR "%s: Bad IC query address.\n", __func__);
		err = -ENODEV;
		goto sy3400_detect_fail;
	}

	sy3400_i2c_write(dd, SY3400_QUERY(qaddr, 3), NULL, 0);
	sy3400_i2c_read(dd, (uint8_t *)&blk_sz, 2);
	blk_sz = le16_to_cpu(blk_sz);

	sy3400_i2c_write(dd, SY3400_QUERY(qaddr, 5), NULL, 0);
	sy3400_i2c_read(dd, (uint8_t *)&blk_ct, 2);
	blk_ct = le16_to_cpu(blk_ct);

	pr_debug("%s: blk_sz=0x%x, blk_ct=0x%x\n", __func__, blk_sz, blk_ct);
/*
	if ((blk_sz != SY3400_BLK_SZ) ||
		((blk_ct != SY3400_BLK_CT) && (blk_ct != SY3300_BLK_CT)))
		err = -ENODEV;
*/
sy3400_detect_fail:
	return err;
}

static void sy3400_gpio_release(struct sy3400_driver_data *dd)
{
	gpio_free(dd->pdata->gpio_reset);
	gpio_free(dd->pdata->gpio_interrupt);
}

static int sy3400_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct sy3400_driver_data *dd = NULL;
	int err = 0;
	bool debugfail = false;

	printk(KERN_INFO "%s: Driver: %s, Version: %s, Date: %s\n", __func__,
		SY3400_I2C_NAME, SY3400_DRIVER_VERSION, SY3400_DRIVER_DATE);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "%s: Missing I2C adapter support.\n", __func__);
		err = -ENODEV;
		goto sy3400_probe_fail;
	}

	dd = kzalloc(sizeof(struct sy3400_driver_data), GFP_KERNEL);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Unable to create driver data.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_probe_fail;
	}

	dd->drv_stat = SY3400_DRV_INIT;
	dd->ic_stat = SY3400_IC_UNKNOWN;
	dd->status = 0x0000;
	dd->client = client;

	if (client->dev.of_node)
		dd->pdata = sy3400_of_init(client);
	else
		dd->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, dd);
	dd->in_dev = NULL;

	dd->mutex = kzalloc(sizeof(struct mutex), GFP_KERNEL);
	if (dd->mutex == NULL) {
		printk(KERN_ERR "%s: Unable to create mutex lock.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_probe_fail;
	}
	mutex_init(dd->mutex);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	dd->dbg = kzalloc(sizeof(struct sy3400_debug), GFP_KERNEL);
	if (dd->dbg == NULL) {
		printk(KERN_ERR "%s: Unable to create driver debug data.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_probe_fail;
	}
	dd->dbg->dbg_lvl = SY3400_DBG0;
#endif

	dd->tdat = kzalloc(sizeof(struct sy3400_tdat), GFP_KERNEL);
	if (dd->tdat == NULL) {
		printk(KERN_ERR "%s: Unable to create tdat binary data.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_probe_fail;
	}

	err = sy3400_verify_pdata(dd);
	if (err < 0)
		goto sy3400_probe_fail;

	err = sy3400_gpio_init(dd);
	if (err < 0)
		goto sy3400_probe_fail;

#ifdef CONFIG_HAS_EARLYSUSPEND
	dd->es.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	dd->es.suspend = sy3400_early_suspend;
	dd->es.resume = sy3400_late_resume;
	register_early_suspend(&dd->es);
#endif

	err = sy3400_request_irq(dd);
	if (err < 0)
		goto sy3400_free_gpio;

	sy3400_hw_ic_reset(dd);
	err = sy3400_detect(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: IC detection failed.\n", __func__);
		goto sy3400_free_irq;
	}

	err = sy3400_request_tdat(dd);
	if (err < 0)
		goto sy3400_free_irq;

	err = sy3400_create_sysfs_files(dd);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Probe had error %d when creating sysfs files.\n",
			__func__, err);
		debugfail = true;
	}

	if (dd->pdata->purge_enabled) {
		dd->panel_nb.notifier_call = sy3400_purge_flag;
		mmi_panel_register_notifier(&dd->panel_nb);
		pr_info("%s: Register display notifier.\n", __func__);
	}

	goto sy3400_probe_pass;

sy3400_free_irq:
	free_irq(dd->client->irq, dd);
sy3400_free_gpio:
	sy3400_gpio_release(dd);
sy3400_probe_fail:
	sy3400_free(dd);
	printk(KERN_ERR "%s: Probe failed with error code %d.\n",
		__func__, err);
	return err;

sy3400_probe_pass:
	if (debugfail) {
		printk(KERN_INFO "%s: Probe completed with errors.\n",
			__func__);
	} else {
		printk(KERN_INFO "%s: Probe successful.\n", __func__);
	}
	return 0;
}

static int sy3400_remove(struct i2c_client *client)
{
	struct sy3400_driver_data *dd = NULL;

	dd = i2c_get_clientdata(client);
	if (dd != NULL) {
		free_irq(dd->client->irq, dd);
		sy3400_remove_sysfs_files(dd);
		sy3400_gpio_release(dd);
		sy3400_free(dd);
	}

	if (dd->pdata->purge_enabled)
		mmi_panel_unregister_notifier(&dd->panel_nb);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static int sy3400_suspend(struct i2c_client *client, pm_message_t message)
{
	int err = 0;
	struct sy3400_driver_data *dd = NULL;
	int drv_state = 0;
	int ic_state = 0;
	uint8_t sleep = 0x01;

	uint8_t x_y_supp_addr = SY3400_XY_COMPR_ADDR;
	uint8_t x_y_supp[2] = {0x32, /* suppresion for x */
				0x32}; /* suppresion for y */
	uint8_t obj_rep_addr = SY3400_OBJ_REP_ADDR;
	uint8_t obj_rep_en[2] = {0x03, /* enable finger, stylus */
				0x01}; /* 1 - max number of touches */


	dd = i2c_get_clientdata(client);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Driver data is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_suspend_no_dd_fail;
	}

	mutex_lock(dd->mutex);

	if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
		disable_irq_nosync(dd->client->irq);
		dd->status = dd->status & ~(1 << SY3400_IRQ_ENABLED_FLAG);
	}

	if (dd->pdata->always_on_capable) {
		/* TODO: config for msp430 handover
		*  This config should not impact normal suspend with
		*  no handover.
		*  This should be re-worked for better implementation
		*/
		sy3400_i2c_write(dd, x_y_supp_addr, x_y_supp, 2);
		sy3400_i2c_write(dd, obj_rep_addr, obj_rep_en, 2);
	}

	sy3400_dbg(dd, SY3400_DBG3, "%s: Suspending...\n", __func__);

	drv_state = sy3400_get_drv_state(dd);
	ic_state = sy3400_get_ic_state(dd);

	switch (drv_state) {
	case SY3400_DRV_ACTIVE:
	case SY3400_DRV_IDLE:
		switch (ic_state) {
		case SY3400_IC_ACTIVE:
			sleep = sleep | dd->icdat->pwr_dat;
			/* For the sleep, the last 3 bits must be 001 */
			sleep &= ~0x7;
			sleep |= 0x01;
			sy3400_dbg(dd, SY3400_DBG3,
				"%s: Suspending touch IC...Writing %d to 0x%04x\n",
					__func__,
					sleep,
					dd->icdat->pwr_addr);
			err = sy3400_i2c_write(dd, dd->icdat->pwr_addr,
				&sleep, 1);
			if (err < 0) {
				printk(KERN_ERR
					"%s: %s %s %d.\n", __func__,
					"Failed to put touch IC to sleep",
					"with error code", err);
				goto sy3400_suspend_fail;
			} else {
				sy3400_set_ic_state(dd, SY3400_IC_SLEEP);
			}
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s suspend.\n",
				__func__, sy3400_driver_state_string[drv_state],
				sy3400_ic_state_string[ic_state]);
		}
		break;

	default:
		printk(KERN_ERR "%s: Driver state \"%s\" suspend.\n",
			__func__, sy3400_driver_state_string[drv_state]);
	}

	sy3400_dbg(dd, SY3400_DBG3, "%s: Suspend complete.\n", __func__);

sy3400_suspend_fail:
	mutex_unlock(dd->mutex);

sy3400_suspend_no_dd_fail:
	return err;
}

static int sy3400_resume(struct i2c_client *client)
{
	int err = 0;
	struct sy3400_driver_data *dd = NULL;
	int drv_state = 0;
	int ic_state = 0;
	uint8_t sleep = 0;

	uint8_t x_y_supp_addr = SY3400_XY_COMPR_ADDR;
	uint8_t x_y_supp[2] = {0x00, /* 00 - no suppresion for x */
				0x00}; /* 00 - no suppresion for y */
	uint8_t obj_rep_addr = SY3400_OBJ_REP_ADDR;
	uint8_t obj_rep_en[2] = {0x07, /* 07 - enable finger, stylus, palm */
				0x0A}; /* 10 - max number of touches */


	dd = i2c_get_clientdata(client);
	if (dd == NULL) {
		printk(KERN_ERR "%s: Driver data is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_resume_no_dd_fail;
	}

	mutex_lock(dd->mutex);

	if ((!(dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)))) {
		dd->status = dd->status | (1 << SY3400_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
	}

	if (dd->pdata->always_on_capable) {
		/* TODO: restore original config from msp430 handover
		*  This clean-up should not impact normal resume
		*  with no handover.
		*  This should be re-worked for better implementation
		*/
		sy3400_i2c_write(dd, x_y_supp_addr, x_y_supp, 2);
		sy3400_i2c_write(dd, obj_rep_addr, obj_rep_en, 2);
	}

	sy3400_dbg(dd, SY3400_DBG3, "%s: Resuming...\n", __func__);

	drv_state = sy3400_get_drv_state(dd);
	ic_state = sy3400_get_ic_state(dd);

	switch (drv_state) {
	case SY3400_DRV_ACTIVE:
	case SY3400_DRV_IDLE:
		switch (ic_state) {
		case SY3400_IC_ACTIVE:
			printk(KERN_ERR "%s: Driver %s, IC %s resume.\n",
				__func__, sy3400_driver_state_string[drv_state],
				sy3400_ic_state_string[ic_state]);
			break;
		case SY3400_IC_SLEEP:
/*
			sy3400_dbg(dd, SY3400_DBG3,
				"%s: Waking touch IC...Writing %d to 0x%04x\n",
					__func__,
					dd->icdat->pwr_dat,
					dd->icdat->pwr_addr);
			err = sy3400_i2c_write(dd, dd->icdat->pwr_addr,
				&(dd->icdat->pwr_dat), 1);
*/
			if (err < 0) {
				printk(KERN_ERR
					"%s: Failed to wake touch IC %s %d.\n",
					__func__, "with error code", err);
				err = sy3400_resume_restart(dd);
				if (err < 0) {
					printk(KERN_ERR
						"%s: %s %s %d.\n",
						__func__,
						"Failed restart after resume",
						"with error code", err);
				}
				goto sy3400_resume_fail;
			} else {
				sy3400_set_ic_state(dd, SY3400_IC_ACTIVE);
			}
			sy3400_release_touches(dd);
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s resume--%s...\n",
				__func__, sy3400_driver_state_string[drv_state],
				sy3400_ic_state_string[ic_state], "recovering");
			err = sy3400_resume_restart(dd);
			if (err < 0) {
				printk(KERN_ERR "%s: Recovery failed %s %d.\n",
					__func__, "with error code", err);
				goto sy3400_resume_fail;
			}
		}
		break;

	case SY3400_DRV_INIT:
		printk(KERN_ERR "%s: Driver state \"%s\" resume.\n",
			__func__, sy3400_driver_state_string[drv_state]);
		break;

	default:
		printk(KERN_ERR "%s: Driver %s, IC %s resume--%s...\n",
			__func__, sy3400_driver_state_string[drv_state],
			sy3400_ic_state_string[ic_state], "recovering");
		err = sy3400_resume_restart(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Recovery failed %s %d.\n",
				__func__, "with error code", err);
			goto sy3400_resume_fail;
		}
		break;
	}

	/* Perform hard reset of the IC. Seem to help on resume */
	sy3400_hw_ic_reset(dd);
	/*
	For the normal operation, the last 3 bits of power register must be 101
	*/
	sleep = dd->icdat->pwr_dat;
	sleep &= ~0x7;
	sleep |= 0x05;
	err = sy3400_i2c_write(dd, dd->icdat->pwr_addr,
			&(sleep), 1);
	sy3400_dbg(dd, SY3400_DBG3, "%s: Resume complete.\n", __func__);

sy3400_resume_fail:
	mutex_unlock(dd->mutex);

sy3400_resume_no_dd_fail:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sy3400_early_suspend(struct early_suspend *handler)
{
	int err = 0;
	struct sy3400_driver_data *dd = NULL;

	dd = container_of(handler, struct sy3400_driver_data, es);

	err = sy3400_suspend(dd->client, PMSG_SUSPEND);
	if (err < 0) {
		printk(KERN_ERR "%s: Suspend failed with error code %d",
			__func__, err);
	}

	return;
}
static void sy3400_late_resume(struct early_suspend *handler)
{
	int err = 0;
	struct sy3400_driver_data *dd = NULL;

	dd = container_of(handler, struct sy3400_driver_data, es);

	err = sy3400_resume(dd->client);
	if (err < 0) {
		printk(KERN_ERR "%s: Resume failed with error code %d.\n",
			__func__, err);
	}

	return;
}
#endif

static int __devinit sy3400_init(void)
{
	return i2c_add_driver(&sy3400_driver);
}

static void __devexit sy3400_exit(void)
{
	i2c_del_driver(&sy3400_driver);
	return;
}

module_init(sy3400_init);
module_exit(sy3400_exit);

static void sy3400_free(struct sy3400_driver_data *dd)
{
	if (dd != NULL) {
		dd->pdata = NULL;
		dd->client = NULL;
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (dd->es.suspend != NULL)
			unregister_early_suspend(&dd->es);
#endif

		if (dd->mutex != NULL) {
			kfree(dd->mutex);
			dd->mutex = NULL;
		}

		if (dd->tdat != NULL) {
			kfree(dd->tdat->data);
			dd->tdat->data = NULL;
			kfree(dd->tdat);
			dd->tdat = NULL;
		}

		if (dd->in_dev != NULL) {
			input_unregister_device(dd->in_dev);
			dd->in_dev = NULL;
		}

		sy3400_free_ic_data(dd);

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

static void sy3400_free_ic_data(struct sy3400_driver_data *dd)
{
	struct sy3400_pdt_node *cur = NULL;
	struct sy3400_pdt_node *next = NULL;

	if (dd->pblk != NULL) {
		cur = dd->pblk->pdt;
		while (cur != NULL) {
			next = cur->next;
			kfree(cur);
			cur = next;
		}
		kfree(dd->pblk->irq_table);
		dd->pblk->irq_table = NULL;
		kfree(dd->pblk);
		dd->pblk = NULL;
	}

	if (dd->icdat != NULL) {
		kfree(dd->icdat);
		dd->icdat = NULL;
	}

	return;
}

static void sy3400_set_drv_state(struct sy3400_driver_data *dd,
		enum sy3400_driver_state state)
{
	printk(KERN_INFO "%s: Driver state %s -> %s\n", __func__,
		sy3400_driver_state_string[dd->drv_stat],
		sy3400_driver_state_string[state]);
	dd->drv_stat = state;
	return;
}

static int sy3400_get_drv_state(struct sy3400_driver_data *dd)
{
	return dd->drv_stat;
}

static void sy3400_set_ic_state(struct sy3400_driver_data *dd,
		enum sy3400_ic_state state)
{
	printk(KERN_INFO "%s: IC state %s -> %s\n", __func__,
		sy3400_ic_state_string[dd->ic_stat],
		sy3400_ic_state_string[state]);
	dd->ic_stat = state;
	return;
}

static int sy3400_get_ic_state(struct sy3400_driver_data *dd)
{
	return dd->ic_stat;
}

static int sy3400_verify_pdata(struct sy3400_driver_data *dd)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3,
		"%s: Verifying platform data...\n", __func__);

	if (dd->pdata == NULL) {
		printk(KERN_ERR "%s: Platform data is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_verify_pdata_fail;
	}

	if (dd->pdata->gpio_reset == 0) {
		printk(KERN_ERR "%s: Reset GPIO is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_verify_pdata_fail;
	}

	if (dd->pdata->gpio_interrupt == 0) {
		printk(KERN_ERR "%s: Interrupt GPIO is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_verify_pdata_fail;
	}

	if (dd->pdata->filename == NULL) {
		printk(KERN_ERR "%s: Touch data filename is missing.\n",
			__func__);
		err = -ENODATA;
		goto sy3400_verify_pdata_fail;
	}

sy3400_verify_pdata_fail:
	return err;
}

static int sy3400_request_tdat(struct sy3400_driver_data *dd)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Requesting tdat data...\n", __func__);

	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG, dd->pdata->filename, &(dd->client->dev),
		GFP_KERNEL, dd, sy3400_tdat_callback);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to schedule tdat request.\n",
			__func__);
		goto sy3400_request_tdat_fail;
	}

sy3400_request_tdat_fail:
	return err;
}

static int sy3400_gpio_init(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct gpio touch_gpio[] = {
		{dd->pdata->gpio_reset, GPIOF_OUT_INIT_LOW, "touch_reset"},
		{dd->pdata->gpio_interrupt, GPIOF_IN,       "touch_irq"},
	};

	sy3400_dbg(dd, SY3400_DBG3, "%s: Requesting touch GPIOs...\n",
		__func__);

	err = gpio_request_array(touch_gpio, ARRAY_SIZE(touch_gpio));
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to request touch GPIOs.\n",
			__func__);
		goto sy3400_gpio_init_fail;
	}

	dd->client->irq = gpio_to_irq(dd->pdata->gpio_interrupt);

sy3400_gpio_init_fail:
	return err;
}

static int sy3400_request_irq(struct sy3400_driver_data *dd)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Requesting IRQ...\n", __func__);

	err = gpio_get_value(dd->pdata->gpio_interrupt);
	if (err < 0) {
		printk(KERN_ERR "%s: Cannot test IRQ line level.\n", __func__);
		goto sy3400_request_irq_fail;
	} else if (err == 0) {
		printk(KERN_ERR
			"%s: Line already active; cannot request IRQ.\n",
			__func__);
		err = -EIO;
		goto sy3400_request_irq_fail;
	}

	err = request_threaded_irq(dd->client->irq, NULL, sy3400_isr,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, SY3400_I2C_NAME, dd);
	if (err < 0) {
		printk(KERN_ERR "%s: IRQ request failed.\n", __func__);
		goto sy3400_request_irq_fail;
	}

	disable_irq_nosync(dd->client->irq);

sy3400_request_irq_fail:
	return err;
}

static int sy3400_i2c_write(struct sy3400_driver_data *dd,
		uint8_t addr, uint8_t *buf, int size)
{
	int err = 0;
	uint8_t *data_out = NULL;
	int size_out = 0;
	int i = 0;
	char *str = NULL;

	size_out = size + 1;
	data_out = kzalloc(sizeof(uint8_t) * size_out, GFP_KERNEL);
	if (data_out == NULL) {
		printk(KERN_ERR "%s: Unable to allocate write memory.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_i2c_write_exit;
	}

	data_out[0] = addr;
	if (buf != NULL && size > 0)
		memcpy(&(data_out[1]), buf, size);
	for (i = 1; i <= SY3400_I2C_ATTEMPTS; i++) {
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

		udelay(SY3400_I2C_WAIT_TIME);
	}

	if (err < 0)
		printk(KERN_ERR "%s: I2C write failed.\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if ((dd->dbg->dbg_lvl) >= SY3400_DBG2)
		str = sy3400_msg2str(data_out, size_out);
#endif
	sy3400_dbg(dd, SY3400_DBG2, "%s: %s\n", __func__, str);
	kfree(str);

sy3400_i2c_write_exit:
	kfree(data_out);

	return err;
}

static int sy3400_i2c_read(struct sy3400_driver_data *dd,
		uint8_t *buf, int size)
{
	int err = 0;
	int i = 0;
	char *str = NULL;

	for (i = 1; i <= SY3400_I2C_ATTEMPTS; i++) {
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

		udelay(SY3400_I2C_WAIT_TIME);
	}

	if (err < 0)
		printk(KERN_ERR "%s: I2C read failed.\n", __func__);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if ((dd->dbg->dbg_lvl) >= SY3400_DBG1)
		str = sy3400_msg2str(buf, size);
#endif
	sy3400_dbg(dd, SY3400_DBG1, "%s: %s\n", __func__, str);
	kfree(str);

	return err;
}

static void sy3400_tdat_callback(const struct firmware *tdat, void *context)
{
	int err = 0;
	struct sy3400_driver_data *dd = context;
	bool icfail = false;
	uint8_t cur_id = 0x00;
	uint32_t cur_size = 0;
	uint8_t *cur_data = NULL;
	size_t loc = 0;
	uint8_t sleep = 0;

	mutex_lock(dd->mutex);

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (dd->status & (1 << SY3400_WAITING_FOR_TDAT)) {
		printk(KERN_INFO "%s: Processing new tdat file...\n", __func__);
		dd->status = dd->status & ~(1 << SY3400_WAITING_FOR_TDAT);
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
		goto sy3400_tdat_callback_fail;
	}

	err = sy3400_validate_tdat(tdat);
	if (err < 0)
		goto sy3400_tdat_callback_fail;

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	if (sy3400_get_drv_state(dd) != SY3400_DRV_INIT) {
		if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			dd->status = dd->status &
				~(1 << SY3400_IRQ_ENABLED_FLAG);
		}
		sy3400_set_drv_state(dd, SY3400_DRV_INIT);
	}

	if (dd->tdat->data != NULL) {
		kfree(dd->tdat->data);
		dd->tdat->data = NULL;
		dd->tdat->size = 0;
		dd->tdat->tsett = NULL;
		dd->tdat->tsett_size = 0;
		dd->tdat->fw = NULL;
		dd->tdat->fw_size = 0;
		dd->tdat->addr[0] = 0x00;
		dd->tdat->addr[1] = 0x00;
	}
#endif

	dd->tdat->data = kzalloc(tdat->size * sizeof(uint8_t), GFP_KERNEL);
	if (dd->tdat->data == NULL) {
		printk(KERN_ERR "%s: Unable to copy tdat.\n", __func__);
		err = -ENOMEM;
		goto sy3400_tdat_callback_fail;
	}
	memcpy(dd->tdat->data, tdat->data, tdat->size);
	dd->tdat->size = tdat->size;

	sy3400_dbg(dd, SY3400_DBG3, "%s: MCV is 0x%02X.\n", __func__,
		dd->tdat->data[loc]);
	loc++;

	while (loc < dd->tdat->size) {
		cur_id = dd->tdat->data[loc];
		cur_size = (dd->tdat->data[loc+3] << 16) |
			(dd->tdat->data[loc+2] << 8) |
			dd->tdat->data[loc+1];
		cur_data = &(dd->tdat->data[loc+4]);

		switch (cur_id) {
		case 0x00:
			break;

		case 0x01:
			dd->tdat->tsett = cur_data;
			dd->tdat->tsett_size = cur_size;
			break;

		case 0x02:
			dd->tdat->fw = cur_data;
			dd->tdat->fw_size = cur_size;
			break;

		case 0x03:
			if (cur_size == 0 || cur_size % 10 != 0) {
				printk(KERN_ERR
					"%s: Abs data format is invalid.\n",
					__func__);
				err = -EINVAL;
				goto sy3400_tdat_callback_fail;
			}

			err = sy3400_register_inputs(dd, cur_data, cur_size);
			if (err < 0)
				goto sy3400_tdat_callback_fail;
			break;

		case 0x04:
			if (cur_size < 4) {
				printk(KERN_ERR
					"%s: Driver data is too small.\n",
					__func__);
				err = -EINVAL;
				goto sy3400_tdat_callback_fail;
			}
			dd->settings = (cur_data[1] << 8) | cur_data[0];
			dd->tdat->addr[0] = cur_data[2];
			dd->tdat->addr[1] = cur_data[3];
			dd->client->addr = dd->tdat->addr[0];
			break;

		default:
			printk(KERN_ERR "%s: Record %hu found but not used.\n",
				__func__, cur_id);
			break;
		}

		loc = loc + cur_size + 4;
	}

	err = sy3400_validate_settings(dd->tdat->tsett, dd->tdat->tsett_size);
	if (err < 0)
		goto sy3400_tdat_callback_fail;

	err = sy3400_validate_firmware(dd->tdat->fw, dd->tdat->fw_size);
	if (err < 0)
		goto sy3400_tdat_callback_fail;

	err = sy3400_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Restarting IC failed with error code %d.\n",
				__func__, err);
		icfail = true;
	}

	if (!icfail) {
		sy3400_set_drv_state(dd, SY3400_DRV_ACTIVE);
		dd->status = dd->status | (1 << SY3400_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
		printk(KERN_INFO "%s: Touch initialization successful.\n",
			__func__);
	} else {
		sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
		printk(KERN_INFO
			"%s: Touch initialization completed with errors.\n",
			__func__);
	}
	/*
	For the normal operation, the last 3 bits of power register must be 101
	*/
	sleep = dd->icdat->pwr_dat;
	sleep &= ~0x7;
	sleep |= 0x05;
	err = sy3400_i2c_write(dd, dd->icdat->pwr_addr,
			&(sleep), 1);

	goto sy3400_tdat_callback_exit;

sy3400_tdat_callback_fail:
	printk(KERN_ERR "%s: Touch initialization failed with error code %d.\n",
		__func__, err);

sy3400_tdat_callback_exit:
	release_firmware(tdat);
	mutex_unlock(dd->mutex);

	return;
}

static int sy3400_validate_tdat(const struct firmware *tdat)
{
	int err = 0;
	int length = 0;
	size_t loc = 0;

	if (tdat->data == NULL || tdat->size == 0) {
		printk(KERN_ERR "%s: No data found.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_tdat_fail;
	}

	if (tdat->data[loc] != 0x31) {
		printk(KERN_ERR "%s: MCV 0x%02X is not supported.\n",
			__func__, tdat->data[loc]);
		err = -EINVAL;
		goto sy3400_validate_tdat_fail;
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
			goto sy3400_validate_tdat_fail;
		}

		loc = loc + length + 4;
	}

	if (loc != (tdat->size)) {
		printk(KERN_ERR "%s: Data is misaligned.\n", __func__);
		err = -ENOEXEC;
		goto sy3400_validate_tdat_fail;
	}

sy3400_validate_tdat_fail:
	return err;
}

static int sy3400_validate_settings(uint8_t *data, uint32_t size)
{
	int err = 0;
	uint32_t iter = 0;
	uint16_t length = 0x0000;

	if (data == NULL || size == 0) {
		printk(KERN_ERR "%s: No settings data found.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_settings_fail;
	} else if (size <= 5) {
		printk(KERN_ERR "%s: Settings data is malformed.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_settings_fail;
	}

	while (iter < (size - 1)) {
		length = (data[iter+4] << 8) | data[iter+3];
		if ((iter + length + 5) > size) {
			printk(KERN_ERR
				"%s: Group record overflow on iter %u.\n",
				__func__, iter);
			err = -EOVERFLOW;
			goto sy3400_validate_settings_fail;
		}

		iter = iter + length + 5;
	}

	if (iter != size) {
		printk(KERN_ERR "%s: Group records misaligned.\n", __func__);
		err = -ENOEXEC;
		goto sy3400_validate_settings_fail;
	}

	iter = 0;
	while (iter < (size - 1)) {
		if ((data[iter+0] == 0x01) && (data[iter+1] == 0x00)) {
			if (data[iter+5] < 4) {
				printk(KERN_ERR "%s: Invalid group 1 header.\n",
					__func__);
				err = -EINVAL;
				goto sy3400_validate_settings_fail;
			} else if (data[iter+2] != 0x00) {
				printk(KERN_ERR
					"%s: %s 0x%02X is not supported.\n",
					__func__, "Group 1 tag", data[iter+2]);
				err = -EINVAL;
				goto sy3400_validate_settings_fail;
			} else if ((data[iter+5] + 1) >=
				((data[iter+4] << 8) | data[iter+3])) {
				printk(KERN_ERR "%s: Group 1 is malformed.\n",
					__func__);
				err = -EOVERFLOW;
				goto sy3400_validate_settings_fail;
			}

			break;
		} else {
			length = (data[iter+4] << 8) | data[iter+3];
			iter = iter + length + 5;
		}
	}

	if (iter == size) {
		printk(KERN_ERR "%s: Group 1 data is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_settings_fail;
	}

sy3400_validate_settings_fail:
	return err;
}

static int sy3400_register_inputs(struct sy3400_driver_data *dd,
		uint8_t *rdat, int rsize)
{
	int err = 0;
	int i = 0;
	int iter = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Registering inputs...\n", __func__);

	if (dd->rdat != NULL)
		kfree(dd->rdat);

	dd->rdat = kzalloc(sizeof(struct sy3400_report_data), GFP_KERNEL);
	if (dd->rdat == NULL) {
		printk(KERN_ERR "%s: Unable to create report data for touch.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_register_inputs_fail;
	}

	if (dd->in_dev != NULL)
		input_unregister_device(dd->in_dev);

	dd->in_dev = input_allocate_device();
	if (dd->in_dev == NULL) {
		printk(KERN_ERR "%s: Failed to allocate input device.\n",
			__func__);
		err = -ENODEV;
		goto sy3400_register_inputs_fail;
	}

	dd->in_dev->name = SY3400_I2C_NAME;
	input_set_drvdata(dd->in_dev, dd);

	set_bit(INPUT_PROP_DIRECT, dd->in_dev->propbit);

	set_bit(EV_KEY, dd->in_dev->evbit);
	input_set_capability(dd->in_dev, EV_KEY, KEY_F1);
	input_set_capability(dd->in_dev, EV_KEY, KEY_F2);
	input_set_capability(dd->in_dev, EV_KEY, KEY_F3);

	set_bit(EV_ABS, dd->in_dev->evbit);
	for (i = 0; i < rsize; i += 10) {
		if (((rdat[i+1] << 8) | rdat[i+0]) != SY3400_ABS_RESERVED) {
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
		dd->rdat->axis[i] = SY3400_ABS_RESERVED;

	input_set_events_per_packet(dd->in_dev,
		SY3400_MAX_TOUCHES * (ARRAY_SIZE(dd->rdat->axis) + 1) * 3);

	err = input_register_device(dd->in_dev);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to register input device.\n",
			__func__);
		err = -ENODEV;
		input_free_device(dd->in_dev);
		dd->in_dev = NULL;
		goto sy3400_register_inputs_fail;
	}

sy3400_register_inputs_fail:
	return err;
}

static int sy3400_restart_ic(struct sy3400_driver_data *dd)
{
	int err = 0;
	bool irq_low = false;
	bool bl_present = false;
	bool update_fw = false;
	bool update_sett = false;
	bool update_complete = false;
	int cur_drv_state = 0;

sy3400_restart_ic_start:
	sy3400_dbg(dd, SY3400_DBG3, "%s: Restarting IC...\n", __func__);

	sy3400_free_ic_data(dd);
	sy3400_release_touches(dd);
	irq_low = false;
	if (sy3400_get_ic_state(dd) != SY3400_IC_UNKNOWN)
		sy3400_set_ic_state(dd, SY3400_IC_UNKNOWN);

	if (!update_fw && !update_sett)
		sy3400_hw_ic_reset(dd);

	irq_low = sy3400_wait4irq(dd);
	if (!irq_low) {
		printk(KERN_ERR "%s: Timeout waiting for interrupt.\n",
			__func__);
		err = -ETIME;
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_get_pdt(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to find touch IC.\n", __func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_validate_f01(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Function $01 failed validation.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_validate_f34(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Function $34 failed validation.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_get_verinfo(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to get version info.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_check_bootloader(dd, &bl_present);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to verify if IC is in bootloader mode.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	if (bl_present) {
		if (update_complete) {
			printk(KERN_ERR "%s: %s.\n", __func__,
				"Touch IC in bootloader after update");
			err = -EINVAL;
			goto sy3400_restart_ic_fail;
		}

		sy3400_set_ic_state(dd, SY3400_IC_BOOTLOADER);
		printk(KERN_INFO "%s: %s: %s, %s: 0x%02X%02X%02X\n", __func__,
			"Bootloader Product ID", &(dd->pblk->prodid[0]),
			"Bootloader Build ID", dd->pblk->bldid[2],
			dd->pblk->bldid[1], dd->pblk->bldid[0]);

		cur_drv_state = sy3400_get_drv_state(dd);
		sy3400_set_drv_state(dd, SY3400_DRV_REFLASH);

		if (!update_sett) {
			err = sy3400_flash_record(dd, 2);
			if (err < 0) {
				printk(KERN_ERR "%s: %s.\n",
					"Failed to update IC firmware",
					__func__);
				sy3400_set_drv_state(dd, cur_drv_state);
				goto sy3400_restart_ic_fail;
			}
		}

		err = sy3400_flash_record(dd, 1);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to update IC settings.\n",
				__func__);
			sy3400_set_drv_state(dd, cur_drv_state);
			goto sy3400_restart_ic_fail;
		}

		sy3400_set_drv_state(dd, cur_drv_state);
		sy3400_dbg(dd, SY3400_DBG3,
			"%s: Reflash completed.  Re-starting cycle...\n",
			__func__);
		update_complete = true;
		update_fw = false;
		update_sett = false;
		goto sy3400_restart_ic_start;
	}

	sy3400_set_ic_state(dd, SY3400_IC_PRESENT);
	printk(KERN_INFO "%s: Product ID: %s, Build ID: 0x%02X%02X%02X\n",
		__func__, &(dd->pblk->prodid[0]), dd->pblk->bldid[2],
		dd->pblk->bldid[1], dd->pblk->bldid[0]);

	update_fw = sy3400_check_firmware(dd);
	if (update_fw) {
		if (update_complete) {
			printk(KERN_ERR "%s: %s.\n", __func__,
				"Firmware update required after update");
			err = -EINVAL;
			goto sy3400_restart_ic_fail;
		}

		printk(KERN_INFO
			"%s: Enabling reflashing to update firmware...\n",
			__func__);
		err = sy3400_enable_reflashing(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to update firmware.\n",
				__func__);
			goto sy3400_restart_ic_fail;
		}

		goto sy3400_restart_ic_start;
	}

	err = sy3400_check_settings(dd, &update_sett);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to verify if IC needs new settings.\n",
			__func__);
		goto sy3400_restart_ic_start;
	}

	if (update_sett) {
		if (update_complete) {
			printk(KERN_ERR "%s: %s.\n", __func__,
				"Settings update required after update");
			err = -EINVAL;
			goto sy3400_restart_ic_fail;
		}

		printk(KERN_INFO
			"%s: Enabling reflashing to update settings...\n",
			__func__);
		err = sy3400_enable_reflashing(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to update settings.\n",
				__func__);
			goto sy3400_restart_ic_fail;
		}

		goto sy3400_restart_ic_start;
	}

	err = sy3400_validate_f12(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Function $12 failed validation.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_validate_f1A(dd);
	/*
	Since the buttons are optional, we will not fail if they are not present
	*/
	if (err < 0 && err != -ENODATA) {
		printk(KERN_ERR "%s: Function $1A failed validation.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	dd->icdat = kzalloc(sizeof(struct sy3400_icdat), GFP_KERNEL);
	if (dd->icdat == NULL) {
		printk(KERN_ERR "%s: Unable to create IC data.\n", __func__);
		err = -ENOMEM;
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_set_ic_data(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to find necessary IC data.\n",
			__func__);
		goto sy3400_restart_ic_fail;
	}

	err = sy3400_create_irq_table(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to create IRQ table.\n", __func__);
		goto sy3400_restart_ic_fail;
	}

	sy3400_set_ic_state(dd, SY3400_IC_ACTIVE);

sy3400_restart_ic_fail:
	return err;
}

static int sy3400_get_pdt(struct sy3400_driver_data *dd)
{
	int err = 0;
	uint8_t buf = 0x00;
	int i = 0;
	int j = 0;
	uint8_t desc[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct sy3400_pdt_node *first = NULL;
	struct sy3400_pdt_node *cur = NULL;
	struct sy3400_pdt_node *prev = NULL;
	int current_page;

	if (dd->pblk == NULL) {
		dd->pblk = kzalloc(sizeof(struct sy3400_page_block),
					GFP_KERNEL);
		if (dd->pblk == NULL) {
			printk(KERN_ERR "%s: Unable to create page block data\n",
				__func__);
			err = -ENOMEM;
			goto sy3400_get_pdt_fail;
		}
	}

	err = sy3400_i2c_write(dd, 0xEF, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set address to PDT Top.\n",
			__func__);
		goto sy3400_get_pdt_fail;
	}

	err = sy3400_i2c_read(dd, &buf, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read PDT Top.\n",
			__func__);
		goto sy3400_get_pdt_fail;
	}

	if (buf != 0x00) {
		printk(KERN_ERR "%s: Page Select register is non-standard.\n",
			__func__);
		err = -EPROTO;
		goto sy3400_get_pdt_fail;
	}

	/*
	In order to support capacitive button sensors we have to use function
	$1A. This function currently does not reside on page 0. Also, there are
	some other functions that may be usefull (ex: $54). So, now we need to
	process page 0, 1, 2, and 3.
	*/
	for (current_page = 0; current_page < 4; current_page++) {
		sy3400_dbg(dd, SY3400_DBG3, "%s: Switching to page %d\n",
			__func__, current_page);
		buf = current_page;
		err = sy3400_i2c_write(dd, 0xFF, &buf, 1);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to switch to page %d.\n",
				__func__, current_page);
			goto sy3400_get_pdt_fail;
		}

		for (i = 0; i < 39 ; i++) {
			err = sy3400_i2c_write(dd, 0xE9 - i * 6, NULL, 0);
			if (err < 0) {
				printk(KERN_ERR
					"%s: Unable to set address for entry %d.\n",
					__func__, i);
				goto sy3400_get_pdt_fail;
			}

			err = sy3400_i2c_read(dd, &(desc[0]), 6);
			if (err < 0) {
				printk(KERN_ERR "%s: Unable to read entry %d.\n",
					__func__, i);
				goto sy3400_get_pdt_fail;
			}

			if (desc[5] == 0x00)
				break;

			sy3400_dbg(dd, SY3400_DBG3,
				"%s: Processing function 0x%x\n",
				__func__, desc[5]);

			cur = kzalloc(sizeof(struct sy3400_pdt_node),
						GFP_KERNEL);
			if (cur == NULL) {
				printk(KERN_ERR "%s: Unable to save entry $%02X.\n",
					__func__, desc[5]);
				err = -ENOMEM;
				goto sy3400_get_pdt_fail;
			}

			if (first == NULL)
				first = cur;
			if (prev != NULL)
				prev->next = cur;

			cur->page = current_page;
			for (j = 0; j <= 5; j++)
				cur->desc[j] = desc[j];

			prev = cur;
		}
		if (i >= 39) {
			printk(KERN_ERR "%s: End of PDT is missing.\n",
						__func__);
			err = -EPROTO;
			goto sy3400_get_pdt_fail;
		}
	}
	/*
	Switch back to page 0, since the rest of the driver assumes we are on
	that page
	*/
	buf = 0;
	err = sy3400_i2c_write(dd, 0xFF, &buf, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to switch to page %d.\n",
			__func__, current_page);
	}

sy3400_get_pdt_fail:
	dd->pblk->pdt = first;
	return err;
}

static int sy3400_validate_f01(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;
	uint8_t query1 = 0x00;
	uint8_t query42 = 0x00;
	uint8_t query43[2] = {0x00, 0x00};
	bool has_sensor_id = false;
	uint8_t addr = 0x00;
	bool bl = false;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x01)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $01 is missing.\n", __func__);
		err = -EPROTO;
		goto sy3400_validate_f01_fail;
	}

	if (cur->desc[4] != 0x01) {
		printk(KERN_ERR "%s: Function $01 is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f01_fail;
	}

	query = cur->desc[0];
	if ((query + 23) >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $01 query registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f01_fail;
	}

	if (cur->desc[3] + 1 >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $01 data registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f01_fail;
	}

	err = sy3400_i2c_write(dd, query+1, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query1 pointer.\n",
			__func__);
		goto sy3400_validate_f01_fail;
	}

	err = sy3400_i2c_read(dd, &query1, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query1.\n", __func__);
		goto sy3400_validate_f01_fail;
	}
	if (query1 & 0x02) {
		printk(KERN_ERR "%s: Register map is non-compliant.\n",
			__func__);
		err = -EPROTO;
		goto sy3400_validate_f01_fail;
	}
	if (query1 & 0x08)
		has_sensor_id = true;

	err = sy3400_check_bootloader(dd, &bl);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to check for bootloader mode.\n",
			__func__);
		goto sy3400_validate_f01_fail;
	} else if (bl) {
		goto sy3400_validate_f01_fail;
	}

	addr = query + 20 + 1;
	if (has_sensor_id)
		addr++;

	err = sy3400_i2c_write(dd, addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query42 pointer.\n",
			__func__);
		goto sy3400_validate_f01_fail;
	}

	err = sy3400_i2c_read(dd, &query42, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query42.\n", __func__);
		goto sy3400_validate_f01_fail;
	}

	if (!(query42 & 0x01)) {
		printk(KERN_ERR "%s: Query42 does not exist.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f01_fail;
	}

	addr = query + 20 + 2;
	if (has_sensor_id)
		addr++;

	err = sy3400_i2c_write(dd, addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query43 pointer.\n",
			__func__);
		goto sy3400_validate_f01_fail;
	}

	err = sy3400_i2c_read(dd, &(query43[0]), 2);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query43.\n", __func__);
		goto sy3400_validate_f01_fail;
	}

	if ((query43[0] & 0x0F) < 1) {
		printk(KERN_ERR "%s: Query43 is too short.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_f01_fail;
	}

	if (!(query43[1] & 0x02)) {
		printk(KERN_ERR "%s: BuildID is not available.\n",
			__func__);
		err = -ENODATA;
		goto sy3400_validate_f01_fail;
	}

sy3400_validate_f01_fail:
	return err;
}

static int sy3400_validate_f34(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;
	uint8_t query1 = 0x00;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x34)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $34 is missing.\n", __func__);
		err = -EPROTO;
		goto sy3400_validate_f34_fail;
	}

	if (cur->desc[4] != 0x21) {
		printk(KERN_ERR "%s: Function $34 is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f34_fail;
	}

	query = cur->desc[0];
	if ((query + 3) >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $34 query registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f34_fail;
	}

	if (cur->desc[2] + 3 >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $34 control registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f34_fail;
	}

	if ((cur->desc[3] + 3) >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $34 data registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f34_fail;
	}

	/* store a valid query base address for further use */
	dd->pblk->query_base_addr = (uint8_t)query;

	err = sy3400_i2c_write(dd, query+1, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query1 pointer.\n",
			__func__);
		goto sy3400_validate_f34_fail;
	}

	err = sy3400_i2c_read(dd, &query1, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query1.\n", __func__);
		goto sy3400_validate_f34_fail;
	}

	if (!(query1 & 0x04)) {
		printk(KERN_ERR "%s: ConfigID is not available (%d) .\n",
			__func__, query1);
		err = -ENODATA;
		goto sy3400_validate_f34_fail;
	}

sy3400_validate_f34_fail:
	return err;
}

static int sy3400_validate_f12(struct sy3400_driver_data *dd)
{
	int err = 0;
	int i = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;
	int data = 0;
	uint8_t size1 = 0x00;
	uint8_t packet[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t offset = 0x00;
	uint8_t pkoffset = 0x00;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x12)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $12 is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_f12_fail;
	}

	if (cur->desc[4] != 0x01) {
		printk(KERN_ERR "%s: Function $12 is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	}

	query = cur->desc[0];
	if ((query + 9) >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $12 query registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f12_fail;
	}

	data = cur->desc[3];
	if (data + 1 >= 0xE9) {
		printk(KERN_ERR
			"%s: Function $12 data registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f12_fail;
	}

	/* Temporary validation skip*/
	printk(KERN_ERR "%s: TODO: %s--%s %s.\n", __func__,
		"Validation of $12 is incomplete",
		"assuming ONLY Data1 is present (no Data0)",
		"and that Data1 is EXACTLY 8 bytes per touch");
	goto sy3400_validate_f12_fail;
	/* End of temporary validation skip */

	err = sy3400_i2c_write(dd, query, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query0 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &size1, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query0.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (size1 != 0x01) {
		printk(KERN_ERR "%s: Query0 is invalid.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	offset++;

	err = sy3400_i2c_write(dd, query+offset, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query1 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &size1, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query1.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (size1 < 0x03) {
		printk(KERN_ERR "%s: Query2 size is invalid.\n", __func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f12_fail;
	}

	offset++;

	err = sy3400_i2c_write(dd, query+offset, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query2 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &(packet[0]), 5);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query2.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (packet[0] == 0x00)
		pkoffset = 3;
	else
		pkoffset = 1;

	if (!(packet[pkoffset] & 0x80)) {
		printk(KERN_ERR "%s: Query7 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	} else if (!(packet[pkoffset+1] & 0x03)) {
		printk(KERN_ERR "%s: Missing query registers (0x%02X).\n",
			__func__, packet[pkoffset+1]);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	}

	for (i = 0; i < 4; i++) {
		if (packet[pkoffset] & (0x08 << i))
			offset++;
	}

	err = sy3400_i2c_write(dd, query+offset, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query7 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &size1, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query7.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (size1 < 0x02) {
		printk(KERN_ERR "%s: Query8 size is invalid.\n", __func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f12_fail;
	}

	offset++;

	err = sy3400_i2c_write(dd, query+offset, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query8 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &(packet[0]), 4);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query8.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (packet[0] == 0x00)
		pkoffset = 3;
	else
		pkoffset = 1;

	if (!(packet[pkoffset] & 0x01)) {
		printk(KERN_ERR "%s: Data0 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	} else if (!(packet[pkoffset] & 0x02)) {
		printk(KERN_ERR "%s: Data1 is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_f12_fail;
	}

	offset++;

	err = sy3400_i2c_write(dd, query+offset, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query9 pointer.\n",
			__func__);
		goto sy3400_validate_f12_fail;
	}

	err = sy3400_i2c_read(dd, &(packet[0]), 5);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Query9.\n", __func__);
		goto sy3400_validate_f12_fail;
	}

	if (packet[0] > 0x01) {
		printk(KERN_ERR "%s: Data0 is too large.\n", __func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f12_fail;
	} else if (packet[2] != 0x02) {
		printk(KERN_ERR "%s: Data1 size is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	}

	if ((packet[3] != 0xFF) && (packet[4] != 0x81)) {
		printk(KERN_ERR
			"%s: Data1 has invalid structure (0x%02X 0x%02X).\n",
			__func__, packet[3], packet[4]);
		err = -EINVAL;
		goto sy3400_validate_f12_fail;
	}

sy3400_validate_f12_fail:
	return err;
}

static int sy3400_validate_f1A(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	uint8_t query = 0;
	int data = 0;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x1A)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $1A is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_f1A_fail;
	}

	if (cur->desc[4] != 0x01) {
		printk(KERN_ERR "%s: Function $1A is invalid.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_f1A_fail;
	}

	/* Switch to the appropriate page */
	err = sy3400_i2c_write(dd, 0xFF, &cur->page, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to switch to page 1.\n",
			__func__);
		err = -EIO;
		goto sy3400_validate_f1A_fail;
	}

	query = cur->desc[0];
	if ((query + 9) > 0xE8) {
		printk(KERN_ERR
			"%s: Function $1A query registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f1A_fail;
	}

	data = cur->desc[3];
	if (data + 1 > 0xE8) {
		printk(KERN_ERR
			"%s: Function $1A data registers overlap PDT.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_validate_f1A_fail;
	}

	err = sy3400_i2c_write(dd, query, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to get MaxButtonCount\n",
			__func__);
		err = -EIO;
		goto sy3400_validate_f1A_fail;
	}

	err = sy3400_i2c_read(dd, &dd->maxButtonCount, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read MaxButtonCount.\n",
				__func__);
		err = -EIO;
		goto sy3400_validate_f1A_fail;
	}
	/* only least 3 bits are used for this */
	dd->maxButtonCount &= 0x07;
	/* The value is one less then actual number of supported buttons */
	dd->maxButtonCount++;

sy3400_validate_f1A_fail:
	return err;
}

static int sy3400_get_verinfo(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x01)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $01 is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_get_verinfo_fail;
	}

	query = cur->desc[0];

	err = sy3400_i2c_write(dd, query+18, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query18 pointer.\n",
			__func__);
		goto sy3400_get_verinfo_fail;
	}

	err = sy3400_i2c_read(dd, &(dd->pblk->bldid[0]), 3);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read build ID.\n", __func__);
		goto sy3400_get_verinfo_fail;
	}

	err = sy3400_i2c_write(dd, query+11, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Query11 pointer.\n",
			__func__);
		goto sy3400_get_verinfo_fail;
	}

	err = sy3400_i2c_read(dd, &(dd->pblk->prodid[0]), 10);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read product ID.\n", __func__);
		goto sy3400_get_verinfo_fail;
	}

	dd->pblk->prodid[10] = '\0';

sy3400_get_verinfo_fail:
	return err;
}

static int sy3400_check_bootloader(struct sy3400_driver_data *dd, bool *bl)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int data = 0;
	uint8_t data0 = 0x00;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x01)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $01 is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_check_bootloader_fail;
	}

	data = cur->desc[3];
	err = sy3400_i2c_write(dd, data, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Data0 pointer.\n", __func__);
		goto sy3400_check_bootloader_fail;
	}

	err = sy3400_i2c_read(dd, &data0, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Data0.\n", __func__);
		goto sy3400_check_bootloader_fail;
	}

	if (data0 & 0x40) {
		*bl = true;
		if ((data0 & 0x0E) != 0x00) {
			printk(KERN_ERR
				"%s: Touch IC is in bootloader%s0x%02X.\n",
				__func__, " with status code ", (data0 & 0x0F));
		}
	} else {
		*bl = false;
	}

sy3400_check_bootloader_fail:
	return err;
}

static bool sy3400_check_firmware(struct sy3400_driver_data *dd)
{
	bool update_fw = false;

	update_fw = !((dd->tdat->fw[1] == dd->pblk->bldid[0]) &&
		(dd->tdat->fw[2] == dd->pblk->bldid[1]) &&
		(dd->tdat->fw[3] == dd->pblk->bldid[2]));

	return update_fw;
}

static int sy3400_check_settings(struct sy3400_driver_data *dd, bool *sett)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int ctrl = 0;
	uint32_t iter = 0;
	uint16_t rec_num = 0x0000;
	uint16_t rec_length = 0x0000;
	uint8_t *grp1 = NULL;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x34)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $34 is missing.\n", __func__);
		err = -ENODATA;
		goto sy3400_check_settings_fail;
	}

	ctrl = cur->desc[2];
	err = sy3400_i2c_write(dd, ctrl, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set Ctrl0 pointer.\n", __func__);
		goto sy3400_check_settings_fail;
	}

	err = sy3400_i2c_read(dd, &(dd->pblk->cfgid[0]), 4);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read Ctrl0.\n", __func__);
		goto sy3400_check_settings_fail;
	}

	while (iter < (dd->tdat->tsett_size - 1)) {
		rec_num = (dd->tdat->tsett[iter+1] << 8) |
			dd->tdat->tsett[iter+0];
		rec_length = (dd->tdat->tsett[iter+4] << 8) |
			dd->tdat->tsett[iter+3];
		if (rec_num == 0x0001) {
			grp1 = &(dd->tdat->tsett[iter+5]);
			break;
		}
		iter = iter + rec_length + 5;
	}

	*sett = !((grp1[1] == dd->pblk->cfgid[0]) &&
		(grp1[2] == dd->pblk->cfgid[1]) &&
		(grp1[3] == dd->pblk->cfgid[2]) &&
		(grp1[4] == dd->pblk->cfgid[3]));

sy3400_check_settings_fail:
	return err;
}

static int sy3400_set_ic_data(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		switch (cur->desc[5]) {
		case 0x01:
			dd->icdat->stat_addr = cur->desc[3];
			dd->icdat->pwr_addr = cur->desc[2];

			err = sy3400_i2c_write(dd, dd->icdat->pwr_addr,
				NULL, 0);
			if (err < 0) {
				printk(KERN_ERR "%s: %s.\n",
					__func__,
					"Unable to set power pointer");
				goto sy3400_set_ic_data_fail;
			}

			err = sy3400_i2c_read(dd,
				&(dd->icdat->pwr_dat), 1);
			if (err < 0) {
				printk(KERN_ERR "%s: %s.\n",
					__func__,
					"Unable to read power data");
				goto sy3400_set_ic_data_fail;
			}

			break;

		case 0x12:
			dd->icdat->xy_addr = cur->desc[3];
			break;

		case 0x1A:
			dd->icdat->buttons_addr = cur->desc[3];
			dd->btn_curr = 0;
			dd->btn_prev = 0;
			break;
		default:
			break;
		}

		cur = cur->next;
	}

sy3400_set_ic_data_fail:
	return err;
}

static int sy3400_create_irq_table(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int i = 0;
	int irqs = 0;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if ((cur->desc[4] & 0x07) > 6) {
			printk(KERN_ERR "%s: %s $%02X.\n", __func__,
			"More than 6 bits assigned to function",
			cur->desc[5]);
			err = -EOVERFLOW;
			goto sy3400_create_irq_table_fail;
		}

		irqs += (cur->desc[4] & 0x07);
		cur = cur->next;
	}

	if (irqs > 256) {
		printk(KERN_ERR
			"%s: IC allocating more than 256 interrupt bits.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_create_irq_table_fail;
	}

	dd->pblk->irq_table = kzalloc(irqs * sizeof(uint8_t), GFP_KERNEL);
	if (dd->pblk->irq_table == NULL) {
		printk(KERN_ERR "%s: Unable to allocate IRQ table.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_create_irq_table_fail;
	}

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		for (i = 0; i < (cur->desc[4] & 0x07); i++) {
			dd->pblk->irq_table[dd->pblk->irq_size] = cur->desc[5];
			dd->pblk->irq_size++;
		}

		cur = cur->next;
	}

	if (dd->pblk->irq_size != irqs) {
		printk(KERN_ERR "%s: IRQ table misaligned.\n", __func__);
		err = -EINVAL;
		goto sy3400_create_irq_table_fail;
	}

sy3400_create_irq_table_fail:
	return err;
}

static irqreturn_t sy3400_isr(int irq, void *handle)
{
	struct sy3400_driver_data *dd = handle;
	int drv_state = 0;
	int ic_state = 0;

	mutex_lock(dd->mutex);

	drv_state = sy3400_get_drv_state(dd);
	ic_state = sy3400_get_ic_state(dd);

	sy3400_dbg(dd, SY3400_DBG3,
		"%s: IRQ Received -- Driver: %s, IC: %s\n", __func__,
		sy3400_driver_state_string[drv_state],
		sy3400_ic_state_string[ic_state]);

	switch (drv_state) {
	case SY3400_DRV_ACTIVE:
		switch (ic_state) {
		case SY3400_IC_SLEEP:
			sy3400_dbg(dd, SY3400_DBG3,
				"%s: Servicing IRQ during sleep...\n",
				__func__);
		case SY3400_IC_ACTIVE:
			sy3400_active_handler(dd);
			break;
		default:
			printk(KERN_ERR "%s: Driver %s, IC %s IRQ received.\n",
				__func__, sy3400_driver_state_string[drv_state],
				sy3400_ic_state_string[ic_state]);
			if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
				disable_irq_nosync(dd->client->irq);
				dd->status = dd->status &
					~(1 << SY3400_IRQ_ENABLED_FLAG);
				sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
			}
			break;
		}
		break;

	default:
		printk(KERN_ERR "%s: Driver state \"%s\" IRQ received.\n",
			__func__, sy3400_driver_state_string[drv_state]);
		if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			dd->status = dd->status &
				~(1 << SY3400_IRQ_ENABLED_FLAG);
			sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
		}
		break;
	}

	sy3400_dbg(dd, SY3400_DBG3, "%s: IRQ Serviced.\n", __func__);
	mutex_unlock(dd->mutex);

	return IRQ_HANDLED;
}

static void sy3400_active_handler(struct sy3400_driver_data *dd)
{
	int err = 0;
	int i = 0;
	int j = 0;
	uint8_t *irq_buf = NULL;
	int buf_size = 0;
	bool msg_fail = false;
	int last_err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Starting active handler...\n",
		__func__);

	buf_size = dd->pblk->irq_size / 8;
	if ((dd->pblk->irq_size % 8) != 0)
		buf_size++;

	irq_buf = kzalloc(sizeof(uint8_t) * buf_size, GFP_KERNEL);
	if (irq_buf == NULL) {
		printk(KERN_ERR "%s: Failed to allocate status buffer.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_active_handler_fail;
	}

	err = sy3400_i2c_write(dd, dd->icdat->stat_addr + 1, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to set status pointer.\n",
			__func__);
		goto sy3400_active_handler_fail;
	}

	err = sy3400_i2c_read(dd, irq_buf, buf_size);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to read status buffer.\n",
			__func__);
		goto sy3400_active_handler_fail;
	}

	for (i = 0; i < buf_size; i++) {
		for (j = 0; j < 7; j++) {
			if ((i * 8 + j) >= dd->pblk->irq_size)
				break;

			if (irq_buf[i] & (1 << j)) {
				sy3400_dbg(dd, SY3400_DBG3,
					"%s: Processing IRQ %d...\n",
					__func__, i * 8 + j);

				err = sy3400_process_irq(dd, i * 8 + j);
				if (err < 0) {
					printk(KERN_ERR
						"%s: %s %d failed %s %d.\n",
						__func__, "Processing IRQ",
						i * 8 + j, "with error code",
						err);
					msg_fail = true;
					last_err = err;
				}
			}
		}
	}

	if (dd->status & (1 << SY3400_RESTART_REQUIRED)) {
		printk(KERN_ERR "%s: Restarting touch IC...\n", __func__);
		dd->status = dd->status & ~(1 << SY3400_RESTART_REQUIRED);
		err = sy3400_resume_restart(dd);
		if (err < 0) {
			printk(KERN_ERR "%s: Failed to restart touch IC.\n",
				__func__);
			goto sy3400_active_handler_fail;
		} else if (msg_fail) {
			err = last_err;
			goto sy3400_active_handler_fail;
		} else {
			goto sy3400_active_handler_pass;
		}
	}

	if (dd->status & (1 << SY3400_REPORT_TOUCHES)) {
		sy3400_report_touches(dd);
		dd->status = dd->status & ~(1 << SY3400_REPORT_TOUCHES);
	}

	if (msg_fail) {
		err = last_err;
		goto sy3400_active_handler_fail;
	}

	goto sy3400_active_handler_pass;

sy3400_active_handler_fail:
	printk(KERN_ERR "%s: Touch active handler failed with error code %d.\n",
		__func__, err);

sy3400_active_handler_pass:
	kfree(irq_buf);
	return;
}

static int sy3400_process_irq(struct sy3400_driver_data *dd, uint8_t irq_bit)
{
	int err = 0;
	uint8_t *data = NULL;
	int size = 0;
	uint8_t addr = 0x00;
	struct sy3400_pdt_node *cur;
	uint8_t buf = 0x00;

	switch (dd->pblk->irq_table[irq_bit]) {
	case 0x01:
		size = 1;
		addr = dd->icdat->stat_addr;
		break;

	case 0x12:
		size = SY3400_MAX_TOUCHES * 8;
		addr = dd->icdat->xy_addr;
		break;

	case 0x1A:
		addr = dd->icdat->buttons_addr;
		size = 1;
		/* Switch to the appropriate page before reading the data */
		/* find the function in the list */
		sy3400_dbg(dd, SY3400_DBG3, "%s: Trying to handle irq $1A\n",
					 __func__);
		sy3400_dbg(dd, SY3400_DBG3, "%s: Will read from 0x%x\n",
				__func__, addr);
		cur = dd->pblk->pdt;
		while (cur != NULL) {
			if (cur->desc[5] == 0x1A)
				break;
			else
				cur = cur->next;
		}
		/* Switch to the appropriate page */
		err = sy3400_i2c_write(dd, 0xFF, &cur->page, 1);
		if (err < 0) {
			printk(KERN_ERR "%s: Unable to switch to page %d.\n",
				__func__, cur->page);
			err = -EIO;
			goto sy3400_process_irq_fail;
		}
		break;

	default:
		printk(KERN_ERR "%s: Received IRQ from function $%02X.\n",
			__func__, dd->pblk->irq_table[irq_bit]);
		goto sy3400_process_irq_fail;
		break;
	}

	data = kzalloc(sizeof(uint8_t) * size, GFP_KERNEL);
	if (data == NULL) {
		printk(KERN_ERR "%s: Failed to allocate data buffer.\n",
			__func__);
		err = -ENOMEM;
		goto sy3400_process_irq_fail;
	}

	err = sy3400_i2c_write(dd, addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to set data pointer.\n", __func__);
		goto sy3400_process_irq_fail;
	}

	err = sy3400_i2c_read(dd, data, size);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to read data buffer.\n", __func__);
		goto sy3400_process_irq_fail;
	}

	switch (dd->pblk->irq_table[irq_bit]) {
	case 0x01:
		err = sy3400_irq_handler01(dd, data, size);
		break;

	case 0x12:
		err = sy3400_irq_handler12(dd, data, size);
		break;

	case 0x1A:
		err = sy3400_irq_handler1A(dd, data, size);
		/* Switch back to page 0 */
		buf = 0x00;
		err = sy3400_i2c_write(dd, 0xFF, &buf, 1);
		if (err < 0) {
			printk(KERN_ERR "%s: Handled $1A. Unable to switch to page %d.\n",
				__func__, buf);
			err = -EIO;
			goto sy3400_process_irq_fail;
		}

		break;

	default:
		printk(KERN_ERR "%s: No handler for function $%02X.\n",
			__func__, dd->pblk->irq_table[irq_bit]);
		goto sy3400_process_irq_fail;
		break;
	}

	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to process IRQ for function $%02X.\n",
			__func__, dd->pblk->irq_table[irq_bit]);
		goto sy3400_process_irq_fail;
	}

sy3400_process_irq_fail:
	kfree(data);
	return err;
}

static int sy3400_irq_handler01(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Handling IRQ type $01...\n", __func__);

	if (size < 1) {
		printk(KERN_ERR "%s: Data size is too small.\n", __func__);
		err = -EINVAL;
		goto sy3400_irq_handler01_fail;
	}

	switch (data[0] & 0x0F) {
	case 0x00:
		printk(KERN_INFO "%s: No error.\n", __func__);
		break;

	case 0x01:
		printk(KERN_INFO "%s: Touch IC reset complete.\n", __func__);
		break;

	case 0x02:
		printk(KERN_ERR "%s: Touch IC configuration error--%s.\n",
			__func__, "check platform settings");
		break;

	case 0x03:
		printk(KERN_ERR "%s: Touch IC device failure.\n", __func__);
		dd->status = dd->status | (1 << SY3400_RESTART_REQUIRED);
		break;

	case 0x04:
		printk(KERN_ERR "%s: Configuration CRC failure.\n", __func__);
		dd->status = dd->status | (1 << SY3400_RESTART_REQUIRED);
		break;

	case 0x05:
		printk(KERN_ERR "%s: Firmware CRC failure.\n", __func__);
		dd->status = dd->status | (1 << SY3400_RESTART_REQUIRED);
		break;

	case 0x06:
		printk(KERN_ERR "%s: CRC in progress.\n", __func__);
		break;

	default:
		printk(KERN_ERR "%s: Unknown error 0x%02X received.\n",
			__func__, data[0] & 0x0F);
		break;
	}

sy3400_irq_handler01_fail:
	return err;
}

static int sy3400_irq_handler12(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size)
{
	int err = 0;
	int i = 0;
	int iter = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Handling IRQ type $12...\n", __func__);

	if (size < (SY3400_MAX_TOUCHES * 8)) {
		printk(KERN_ERR "%s: Data size is too small.\n", __func__);
		err = -EINVAL;
		goto sy3400_irq_handler12_fail;
	}

	dd->status = dd->status | (1 << SY3400_REPORT_TOUCHES);

	for (i = 0; i < SY3400_MAX_TOUCHES; i++) {
		if ((data[iter+0] == 0x00)) {
#ifdef CONFIG_TOUCHSCREEN_DEBUG
			if (dd->rdat->tchdat[i].active) {
				sy3400_dbg(dd, SY3400_DBG1,
					"%s: Touch ID %d released.\n",
					__func__, i);
			}
#endif
			dd->rdat->tchdat[i].active = false;
		} else {
			dd->rdat->tchdat[i].active = true;
		}

		dd->rdat->tchdat[i].x = data[iter+1] | (data[iter+2] << 8);
		dd->rdat->tchdat[i].y = data[iter+3] | (data[iter+4] << 8);
		dd->rdat->tchdat[i].p = data[iter+5];
		dd->rdat->tchdat[i].w = data[iter+5];
		dd->rdat->tchdat[i].id = i;
		iter = iter + 8;
	}

sy3400_irq_handler12_fail:
	return err;
}

static int sy3400_irq_handler1A(struct sy3400_driver_data *dd,
		uint8_t *data, uint8_t size)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Handling IRQ type $1A...Data: 0x%x\n",
			__func__, data[0]);

/* Buttons are controlled by bits in a single byte, so size has to be 1 */
	if (size != 1) {
		printk(KERN_ERR "%s: Data size is wrong(%d).\n",
				__func__, size);
		err = -EINVAL;
		goto sy3400_irq_handler1A_fail;
	}

	dd->status = dd->status | (1 << SY3400_REPORT_TOUCHES);

	dd->btn_curr = data[0];

sy3400_irq_handler1A_fail:
	return err;
}

static void sy3400_report_touches(struct sy3400_driver_data *dd)
{
	int i = 0;
	int j = 0;
	int rval = 0;
	int id = 0;
	int x = 0;
	int y = 0;
	int p = 0;
	int w = 0;
	bool touches_reported = false;
	uint8_t c_button;
	uint8_t p_button;

	if (atomic_read(&dd->purge_flag)) {
		sy3400_dbg(dd, SY3400_DBG3, "%s: skip reporting a touch "
			" while display is off\n", __func__);
		return;
	}

	for (i = 0; i < SY3400_MAX_TOUCHES; i++) {
		if (!(dd->rdat->tchdat[i].active))
			continue;

		touches_reported = true;

		id =  dd->rdat->tchdat[i].id;
		x = dd->rdat->tchdat[i].x;
		y = dd->rdat->tchdat[i].y;
		p = dd->rdat->tchdat[i].p;
		w = dd->rdat->tchdat[i].w;

		if (dd->settings & (1 << SY3400_SWAP_XY_FLAG)) {
			rval = x;
			x = y;
			y = rval;
		}

		if (dd->settings & (1 << SY3400_INVERT_X_FLAG))
			x = dd->rdat->axis[0] - x;

		if (dd->settings & (1 << SY3400_INVERT_Y_FLAG))
			y = dd->rdat->axis[1] - y;

		sy3400_dbg(dd, SY3400_DBG1,
			"%s: ID=%d, X=%d, Y=%d, P=%d, W=%d\n",
			__func__, id, x, y, p, w);

		for (j = 0; j < 5; j++) {
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
			if (dd->rdat->axis[j] != SY3400_ABS_RESERVED) {
				input_report_abs(dd->in_dev,
					dd->rdat->axis[j], rval);
			}
		}
		input_mt_sync(dd->in_dev);
	}

	for (i = 0; i < SY3400_MAX_BUTTONS; i++) {
		c_button = dd->btn_curr & (1<<i);
		p_button = dd->btn_prev & (1<<i);

		/* Check of status of the button has changed */
		if (c_button == p_button)
			continue;

		input_report_key(dd->in_dev, i+KEY_F1, c_button);
		touches_reported = true;
	}
	dd->btn_prev = dd->btn_curr;

	if (!touches_reported)
		input_mt_sync(dd->in_dev);

	input_sync(dd->in_dev);

	return;
}

static void sy3400_release_touches(struct sy3400_driver_data *dd)
{
	int i = 0;

	sy3400_dbg(dd, SY3400_DBG1, "%s: Releasing all touches...\n", __func__);

	for (i = 0; i < SY3400_MAX_TOUCHES; i++)
		dd->rdat->tchdat[i].active = false;

	sy3400_report_touches(dd);
	dd->status = dd->status & ~(1 << SY3400_REPORT_TOUCHES);

	return;
}

static int sy3400_resume_restart(struct sy3400_driver_data *dd)
{
	int err = 0;

	sy3400_dbg(dd, SY3400_DBG3, "%s: Resume restarting IC...\n", __func__);

	if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
		disable_irq_nosync(dd->client->irq);
		dd->status = dd->status & ~(1 << SY3400_IRQ_ENABLED_FLAG);
	}

	err = sy3400_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to restart the touch IC.\n",
			__func__);
		sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
		goto sy3400_resume_restart_fail;
	}

	if (!(dd->status & (1 << SY3400_IRQ_ENABLED_FLAG))) {
		if (sy3400_get_drv_state(dd) != SY3400_DRV_ACTIVE)
			sy3400_set_drv_state(dd, SY3400_DRV_ACTIVE);
		dd->status = dd->status | (1 << SY3400_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
	}

sy3400_resume_restart_fail:
	return err;
}

static int sy3400_validate_firmware(uint8_t *data, uint32_t size)
{
	int err = 0;

	if (data == NULL || size == 0) {
		printk(KERN_ERR "%s: No firmware data found.\n", __func__);
		err = -ENODATA;
		goto sy3400_validate_firmware_fail;
	}

	if (data[0] < 3) {
		printk(KERN_ERR "%s: Invalid firmware header.\n", __func__);
		err = -EINVAL;
		goto sy3400_validate_firmware_fail;
	} else if ((data[0] + 1) >= size) {
		printk(KERN_ERR "%s: Firmware is malformed.\n", __func__);
		err = -EOVERFLOW;
		goto sy3400_validate_firmware_fail;
	}

sy3400_validate_firmware_fail:
	return err;
}

static int sy3400_enable_reflashing(struct sy3400_driver_data *dd)
{
	int err = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;
	int data = 0;
	uint8_t key[2] = {0x00, 0x00};
	uint8_t flash_cmd = 0x0F;
	bool irq_low = false;
	uint8_t stat = 0x00;
	uint8_t irq_addr = 0x00;

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x34)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $34 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_enable_reflashing_fail;
	}

	query = cur->desc[0];
	data = cur->desc[3];

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x01)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $01 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_enable_reflashing_fail;
	}

	irq_addr = cur->desc[3] + 1;

	err = sy3400_i2c_write(dd, query, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set BL key pointer.\n",
			__func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_read(dd, &(key[0]), ARRAY_SIZE(key));
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read BL key.\n", __func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_write(dd, irq_addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set IRQ pointer.\n", __func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_read(dd, &stat, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to clear IRQ line.\n", __func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_write(dd, data+1, &(key[0]), ARRAY_SIZE(key));
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to write BL key.\n", __func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_write(dd, data+2, &flash_cmd, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to write flash command.\n",
			__func__);
		goto sy3400_enable_reflashing_fail;
	}

	irq_low = sy3400_wait4irq(dd);
	if (!irq_low) {
		printk(KERN_ERR "%s: Timeout waiting for interrupt.\n",
			__func__);
		err = -ETIME;
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_write(dd, data+3, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set flash status pointer.\n",
			__func__);
		goto sy3400_enable_reflashing_fail;
	}

	err = sy3400_i2c_read(dd, &stat, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read flash status.\n", __func__);
		goto sy3400_enable_reflashing_fail;
	}

	if ((stat & 0x3F) != 0x00) {
		printk(KERN_ERR "%s: Flash status error 0x%02X received.\n",
			__func__, stat);
		err = -EINVAL;
		goto sy3400_enable_reflashing_fail;
	}

sy3400_enable_reflashing_fail:
	return err;
}

static int sy3400_flash_record(struct sy3400_driver_data *dd, int type)
{
	int err = 0;
	int i = 0;
	struct sy3400_pdt_node *cur = NULL;
	int query = 0;
	int data = 0;
	uint8_t key[2] = {0x00, 0x00};
	uint8_t irq_addr = 0x00;
	bool irq_low = false;
	uint8_t stat = 0x00;
	uint8_t blksize[2] = {0x00, 0x00};
	uint8_t blkbuf[4] = {0x00, 0x00, 0x00, 0x00};
	int bcount = 0;
	int bsize = 0;
	uint8_t *img = NULL;
	uint32_t img_size = 0;
	uint32_t iter = 0;
	uint8_t erase_cmd = 0x00;
	uint8_t write_cmd = 0x00;
	uint8_t config_area = 0x00;
	uint8_t count_offset = 0x00;
	int write_size = 0;
	uint16_t rec_num = 0x0000;
	uint16_t rec_length = 0x0000;

	switch (type) {
	case 1:
		while (iter < (dd->tdat->tsett_size - 1)) {
			rec_num = (dd->tdat->tsett[iter+1] << 8) |
				dd->tdat->tsett[iter+0];
			rec_length = (dd->tdat->tsett[iter+4] << 8) |
				dd->tdat->tsett[iter+3];
			if (rec_num == 0x0001) {
				img = &(dd->tdat->tsett[iter+5]);
				img_size = rec_length;
				break;
			}
			iter = iter + rec_length + 5;
		}

		if (img == NULL) {
			printk(KERN_ERR
				"%s: Configuration data is missing.\n",
				__func__);
			err = -ENODATA;
			goto sy3400_flash_record_fail;
		}

		img_size = img_size - (img[0] + 1);
		img = &(img[img[0] + 1]);

		erase_cmd = 0x07;
		write_cmd = 0x06;
		config_area = 0x00;
		count_offset = 0x02;
		break;

	case 2:
		img = &(dd->tdat->fw[dd->tdat->fw[0] + 1]);
		img_size = dd->tdat->fw_size - (dd->tdat->fw[0] + 1);
		erase_cmd = 0x03;
		write_cmd = 0x02;
		config_area = 0x00;
		count_offset = 0x00;
		break;

	default:
		printk(KERN_ERR "%s: Type %d not implemented.\n",
			__func__, type);
		err = -ENOSYS;
		goto sy3400_flash_record_fail;
		break;
	}

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x34)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $34 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_flash_record_fail;
	}

	query = cur->desc[0];
	data = cur->desc[3];

	cur = dd->pblk->pdt;
	while (cur != NULL) {
		if (cur->desc[5] == 0x01)
			break;
		else
			cur = cur->next;
	}

	if (cur == NULL) {
		printk(KERN_ERR "%s: Function $01 is missing.\n", __func__);
		err = -EINVAL;
		goto sy3400_flash_record_fail;
	}

	irq_addr = cur->desc[3] + 1;

	err = sy3400_i2c_write(dd, query+2, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set block size pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &(blksize[0]), 2);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read the block size.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, query+3, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set block count pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &(blkbuf[0]), ARRAY_SIZE(blkbuf));
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read block count buffer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	bsize = blksize[0] | (blksize[1] << 8);
	bcount = blkbuf[count_offset] | (blkbuf[count_offset+1] << 8);
	if ((bsize * bcount) != img_size) {
		printk(KERN_ERR "%s: Data size does not equal IC size.\n",
			__func__);
		err = -EOVERFLOW;
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, query, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set BL key pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &(key[0]), ARRAY_SIZE(key));
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read BL key.\n", __func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, irq_addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set BL key IRQ pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &stat, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to clear BL key IRQ line.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, data+1, &(key[0]), ARRAY_SIZE(key));
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to write BL key.\n", __func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, data+2, &erase_cmd, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to send erase command.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	irq_low = sy3400_wait4irq(dd);
	if (!irq_low) {
		printk(KERN_ERR "%s: Timeout waiting erase interrupt.\n",
			__func__);
		err = -ETIME;
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, irq_addr, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set erase IRQ pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &stat, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to clear erase IRQ line.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_write(dd, data+3, NULL, 0);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set erase status pointer.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	err = sy3400_i2c_read(dd, &stat, 1);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to read erase status.\n", __func__);
		goto sy3400_flash_record_fail;
	}

	if ((stat & 0x3F) != 0x00) {
		printk(KERN_ERR "%s: Flash status error 0x%02X received %s.\n",
			__func__, stat, "from erasing");
		err = -EINVAL;
		goto sy3400_flash_record_fail;
	}

	key[0] = 0x00;
	key[1] = (config_area << 5);
	err = sy3400_i2c_write(dd, data, &(key[0]), 2);
	if (err < 0) {
		printk(KERN_ERR "%s: Unable to set starting block number.\n",
			__func__);
		goto sy3400_flash_record_fail;
	}

	iter = 0;
	for (i = 0; i < bcount; i++) {
		write_size = bsize;
		if ((iter + bsize) >= img_size) {
			write_size = img_size - iter;
			if (write_size == 0)
				break;
		}

		err = sy3400_i2c_write(dd, data+1, &(img[iter]), write_size);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to write block %d data.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		err = sy3400_i2c_write(dd, data+2, &write_cmd, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to write block %d command.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		irq_low = sy3400_wait4irq(dd);
		if (!irq_low) {
			printk(KERN_ERR
				"%s: Timeout waiting for block %d interrupt.\n",
				__func__, i);
			err = -ETIME;
			goto sy3400_flash_record_fail;
		}

		err = sy3400_i2c_write(dd, irq_addr, NULL, 0);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to set block %d IRQ pointer.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		err = sy3400_i2c_read(dd, &stat, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to read block %d IRQ status.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		err = sy3400_i2c_write(dd, data+3, NULL, 0);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to set block %d status pointer.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		err = sy3400_i2c_read(dd, &stat, 1);
		if (err < 0) {
			printk(KERN_ERR
				"%s: Unable to read block %d status.\n",
				__func__, i);
			goto sy3400_flash_record_fail;
		}

		if ((stat & 0x3F) != 0x00) {
			printk(KERN_ERR "%s: Block %d error 0x%02X received.\n",
				__func__, i, stat);
			err = -EINVAL;
			goto sy3400_flash_record_fail;
		}

		iter = iter + write_size;
	}

sy3400_flash_record_fail:
	return err;
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static char *sy3400_msg2str(const uint8_t *msg, int size)
{
	char *str = NULL, *cur;
	int i = 0;
	int err = 0;

	str = kzalloc(sizeof(char) * (size * 5) + 1, GFP_KERNEL);
	if (str == NULL) {
		printk(KERN_ERR "%s: Failed to allocate message string.\n",
			__func__);
		goto sy3400_msg2str_exit;
	}

	cur = str;
	for (i = 0; i < size; i++) {
		err = snprintf(cur, 6, "0x%02X ", msg[i]);
		if (err < 0) {
			printk(KERN_ERR "%s: Error in sprintf on pass %d",
				__func__, i);
			goto sy3400_msg2str_exit;
		}
		cur += 5;
	}


sy3400_msg2str_exit:
	return str;
}
#endif

static bool sy3400_wait4irq(struct sy3400_driver_data *dd)
{
	bool irq_low = false;
	int i = 0;

	for (i = 0; i < 5000; i++) {
		if (gpio_get_value(dd->pdata->gpio_interrupt) != 0) {
			usleep_range(1000, 1000);
		} else {
			irq_low = true;
			break;
		}
	}

	return irq_low;
}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t sy3400_debug_drv_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Debug Level: %hu\n", dd->dbg->dbg_lvl);
}
static ssize_t sy3400_debug_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto sy3400_debug_drv_debug_store_exit;
	}

	if (value > 255) {
		printk(KERN_ERR "%s: Invalid debug level %lu--setting to %u.\n",
			__func__, value, SY3400_DBG3);
		dd->dbg->dbg_lvl = SY3400_DBG3;
	} else {
		dd->dbg->dbg_lvl = value;
		printk(KERN_INFO "%s: Debug level is now %hu.\n",
			__func__, dd->dbg->dbg_lvl);
	}

	err = size;

sy3400_debug_drv_debug_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static struct device_attribute dev_attr_drv_debug = {
	.attr = {.name = "drv_debug", .mode = S_IRUSR | S_IWUSR},
	.show = sy3400_debug_drv_debug_show,
	.store = sy3400_debug_drv_debug_store,
};

static ssize_t sy3400_debug_drv_flags_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Current Driver Flags: 0x%04X\n", dd->settings);
}
static ssize_t sy3400_debug_drv_flags_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 16, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto sy3400_debug_drv_flags_store_exit;
	}

	if (value > 65535) {
		printk(KERN_ERR "%s: Invalid flag settings 0x%08lX passed.\n",
			__func__, value);
		err = -EOVERFLOW;
		goto sy3400_debug_drv_flags_store_exit;
	} else {
		dd->settings = value;
		sy3400_dbg(dd, SY3400_DBG3,
			"%s: Driver flags now set to 0x%04X.\n",
			__func__, dd->settings);
	}

	err = size;

sy3400_debug_drv_flags_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static struct device_attribute dev_attr_drv_flags = {
	.attr = {.name = "drv_flags", .mode = S_IRUSR | S_IWUSR},
	.show = sy3400_debug_drv_flags_show,
	.store = sy3400_debug_drv_flags_store,
};
#endif

static ssize_t sy3400_debug_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG))
		return sprintf(buf, "Driver interrupt is ENABLED.\n");
	else
		return sprintf(buf, "Driver interrupt is DISABLED.\n");
}
static ssize_t sy3400_debug_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);
	unsigned long value = 0;

	mutex_lock(dd->mutex);

	err = strict_strtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		goto sy3400_debug_drv_irq_store_exit;
	}

	if ((sy3400_get_drv_state(dd) != SY3400_DRV_ACTIVE) &&
		(sy3400_get_drv_state(dd) != SY3400_DRV_IDLE)) {
		printk(KERN_ERR "%s: %s %s or %s states.\n",
			__func__, "Interrupt can be changed only in",
			sy3400_driver_state_string[SY3400_DRV_ACTIVE],
			sy3400_driver_state_string[SY3400_DRV_IDLE]);
		err = -EACCES;
		goto sy3400_debug_drv_irq_store_exit;
	}

	switch (value) {
	case 0:
		if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
			disable_irq_nosync(dd->client->irq);
			sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
			dd->status =
				dd->status & ~(1 << SY3400_IRQ_ENABLED_FLAG);
		}
		break;

	case 1:
		if (!(dd->status & (1 << SY3400_IRQ_ENABLED_FLAG))) {
			dd->status =
				dd->status | (1 << SY3400_IRQ_ENABLED_FLAG);
			enable_irq(dd->client->irq);
			sy3400_set_drv_state(dd, SY3400_DRV_ACTIVE);
		}
		break;

	default:
		printk(KERN_ERR "%s: Invalid value passed.\n", __func__);
		err = -EINVAL;
		goto sy3400_debug_drv_irq_store_exit;
	}

	err = size;

sy3400_debug_drv_irq_store_exit:
	mutex_unlock(dd->mutex);

	return err;
}
static struct device_attribute dev_attr_drv_irq = {
	.attr = {.name = "drv_irq", .mode = S_IRUSR | S_IWUSR},
	.show = sy3400_debug_drv_irq_show,
	.store = sy3400_debug_drv_irq_store,
};

static ssize_t sy3400_debug_drv_stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Driver state is %s.\nIC state is %s.\n",
		sy3400_driver_state_string[sy3400_get_drv_state(dd)],
		sy3400_ic_state_string[sy3400_get_ic_state(dd)]);
}
static struct device_attribute dev_attr_drv_stat = {
	.attr = {.name = "drv_stat", .mode = S_IRUGO},
	.show = sy3400_debug_drv_stat_show,
	.store = NULL,
};

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t sy3400_debug_drv_tdat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	if (dd->status & (1 << SY3400_WAITING_FOR_TDAT))
		return sprintf(buf, "Driver is waiting for data load.\n");
	else
		return sprintf(buf, "No data loading in progress.\n");
}
static ssize_t sy3400_debug_drv_tdat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	mutex_lock(dd->mutex);

	if (dd->status & (1 << SY3400_WAITING_FOR_TDAT)) {
		printk(KERN_ERR "%s: Driver is already waiting for data.\n",
			__func__);
		err = -EALREADY;
		goto sy3400_debug_drv_tdat_store_fail;
	}

	printk(KERN_INFO "%s: Enabling firmware class loader...\n", __func__);

	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_NOHOTPLUG, "", &(dd->client->dev),
		GFP_KERNEL, dd, sy3400_tdat_callback);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Firmware request failed with error code %d.\n",
			__func__, err);
		goto sy3400_debug_drv_tdat_store_fail;
	}

	dd->status = dd->status | (1 << SY3400_WAITING_FOR_TDAT);
	err = size;

sy3400_debug_drv_tdat_store_fail:
	mutex_unlock(dd->mutex);

	return err;
}
static struct device_attribute dev_attr_drv_tdat = {
	.attr = {.name = "drv_tdat", .mode = S_IRUSR | S_IWUSR},
	.show = sy3400_debug_drv_tdat_show,
	.store = sy3400_debug_drv_tdat_store,
};
#endif

static ssize_t sy3400_debug_drv_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Driver: %s\nVersion: %s\nDate: %s\n",
		SY3400_I2C_NAME, SY3400_DRIVER_VERSION, SY3400_DRIVER_DATE);
}
static struct device_attribute dev_attr_drv_ver = {
	.attr = {.name = "drv_ver", .mode = S_IRUGO},
	.show = sy3400_debug_drv_ver_show,
	.store = NULL,
};

static ssize_t sy3400_debug_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	err = gpio_get_value(dd->pdata->gpio_interrupt);
	if (err < 0) {
		printk(KERN_ERR
			"%s: Failed to read irq level with error code %d.\n",
			__func__, err);
		err = sprintf(buf,
			"Failed to read irq level with error code %d.\n",
			err);
		goto sy3400_debug_hw_irqstat_show_exit;
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

sy3400_debug_hw_irqstat_show_exit:
	return err;
}
static struct device_attribute dev_attr_hw_irqstat = {
	.attr = {.name = "hw_irqstat", .mode = S_IRUGO},
	.show = sy3400_debug_hw_irqstat_show,
	.store = NULL,
};

#ifdef CONFIG_TOUCHSCREEN_DEBUG
static ssize_t sy3400_debug_hw_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	mutex_lock(dd->mutex);

	if (sy3400_get_drv_state(dd) == SY3400_DRV_INIT) {
		printk(KERN_ERR "%s: %s %s.\n", __func__,
			"Unable to restart IC in driver state",
			sy3400_driver_state_string[SY3400_DRV_INIT]);
		err = -EACCES;
		goto sy3400_debug_hw_reset_store_fail;
	}

	if (dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)) {
		disable_irq_nosync(dd->client->irq);
		dd->status = dd->status & ~(1 << SY3400_IRQ_ENABLED_FLAG);
	}

	err = sy3400_restart_ic(dd);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to %s with error code %d.\n",
			__func__, "re-initialize the touch IC", err);
		sy3400_set_drv_state(dd, SY3400_DRV_IDLE);
		goto sy3400_debug_hw_reset_store_fail;
	}

	if (((sy3400_get_drv_state(dd) == SY3400_DRV_ACTIVE)) &&
		(!(dd->status & (1 << SY3400_IRQ_ENABLED_FLAG)))) {
		dd->status = dd->status | (1 << SY3400_IRQ_ENABLED_FLAG);
		enable_irq(dd->client->irq);
	}

	err = size;

sy3400_debug_hw_reset_store_fail:
	mutex_unlock(dd->mutex);
	return err;
}
static struct device_attribute dev_attr_hw_reset = {
	.attr = {.name = "hw_reset", .mode = S_IWUSR},
	.show = NULL,
	.store = sy3400_debug_hw_reset_store,
};
#endif

static ssize_t sy3400_debug_hw_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	return sprintf(buf, "Touch Data File: %s\n", dd->pdata->filename);
}
static struct device_attribute dev_attr_hw_ver = {
	.attr = {.name = "hw_ver", .mode = S_IRUGO},
	.show = sy3400_debug_hw_ver_show,
	.store = NULL,
};

static ssize_t sy3400_debug_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sy3400_driver_data *dd = dev_get_drvdata(dev);

	if (dd->pblk == NULL) {
		return sprintf(buf,
			"No touch IC version information is available.\n");
	} else {
		return sprintf(buf,
			"%s%s\n%s0x%02X%02X%02X\n%s0x%02X%02X%02X%02X\n",
			"Product ID: ", &(dd->pblk->prodid[0]),
			"Build ID: ", dd->pblk->bldid[2],
			dd->pblk->bldid[1], dd->pblk->bldid[0],
			"Config ID: ", dd->pblk->cfgid[3],
			dd->pblk->cfgid[2], dd->pblk->cfgid[1],
			dd->pblk->cfgid[0]);
	}
}
static struct device_attribute dev_attr_ic_ver = {
	.attr = {.name = "ic_ver", .mode = S_IRUGO},
	.show = sy3400_debug_ic_ver_show,
	.store = NULL,
};

static int sy3400_create_sysfs_files(struct sy3400_driver_data *dd)
{
	int err = 0;
	int check = 0;

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_drv_debug);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_debug %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_flags);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_flags %s %d.\n",
			__func__, "with error", check);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_irq);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_irq %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_stat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_stat %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_drv_tdat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_ver %s %d.\n",
			__func__, "with error", check);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_drv_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create drv_ver %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_hw_irqstat);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_irqstat %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

#ifdef CONFIG_TOUCHSCREEN_DEBUG
	check = device_create_file(&(dd->client->dev), &dev_attr_hw_reset);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_reset %s %d.\n",
			__func__, "with error", check);
		err = check;
	}
#endif

	check = device_create_file(&(dd->client->dev), &dev_attr_hw_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create hw_ver %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

	check = device_create_file(&(dd->client->dev), &dev_attr_ic_ver);
	if (check < 0) {
		printk(KERN_ERR "%s: Failed to create ic_ver %s %d.\n",
			__func__, "with error", check);
		err = check;
	}

	return err;
}

static void sy3400_remove_sysfs_files(struct sy3400_driver_data *dd)
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
	device_remove_file(&(dd->client->dev), &dev_attr_ic_ver);
	return;
}
