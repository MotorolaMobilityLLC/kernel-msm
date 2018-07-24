/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include "common.h"
#include "core/config.h"
#include "core/i2c.h"
#include "core/spi.h"
#include "core/firmware.h"
#include "core/finger_report.h"
#include "core/flash.h"
#include "core/protocol.h"
#include "platform.h"
#include "core/mp_test.h"
#include "core/gesture.h"

#define DTS_INT_GPIO    "ilitek,irq-gpio"
#define DTS_RESET_GPIO  "ilitek,reset-gpio"

#define DTS_OF_NAME     "ilitek,tchip"

#define DEVICE_ID   "ILITEK_TDDI"

#define ILI_COORDS_ARR_SIZE	4


#ifdef USE_KTHREAD
static DECLARE_WAIT_QUEUE_HEAD(waiter);
#endif

/* Debug level */
uint32_t ipio_debug_level = DEBUG_NONE;
EXPORT_SYMBOL(ipio_debug_level);

struct ilitek_platform_data *ipd;

void ilitek_platform_disable_irq(void)
{
	unsigned long nIrqFlag;

	ipio_debug(DEBUG_IRQ, "IRQ = %d\n", ipd->isEnableIRQ);

	spin_lock_irqsave(&ipd->plat_spinlock, nIrqFlag);

	if (ipd->isEnableIRQ) {
		if (ipd->isr_gpio) {
			disable_irq_nosync(ipd->isr_gpio);
			ipd->isEnableIRQ = false;
			ipio_debug(DEBUG_IRQ, "Disable IRQ: %d\n",
				   ipd->isEnableIRQ);
		} else
			ipio_err("The number of gpio to irq is incorrect\n");
	} else
		ipio_debug(DEBUG_IRQ, "IRQ was already disabled\n");

	spin_unlock_irqrestore(&ipd->plat_spinlock, nIrqFlag);
}

EXPORT_SYMBOL(ilitek_platform_disable_irq);

void ilitek_platform_enable_irq(void)
{
	unsigned long nIrqFlag;

	ipio_debug(DEBUG_IRQ, "IRQ = %d\n", ipd->isEnableIRQ);

	spin_lock_irqsave(&ipd->plat_spinlock, nIrqFlag);

	if (!ipd->isEnableIRQ) {
		if (ipd->isr_gpio) {
			enable_irq(ipd->isr_gpio);
			ipd->isEnableIRQ = true;
			ipio_debug(DEBUG_IRQ, "Enable IRQ: %d\n",
				   ipd->isEnableIRQ);
		} else
			ipio_err("The number of gpio to irq is incorrect\n");
	} else
		ipio_debug(DEBUG_IRQ, "IRQ was already enabled\n");

	spin_unlock_irqrestore(&ipd->plat_spinlock, nIrqFlag);
}

EXPORT_SYMBOL(ilitek_platform_enable_irq);

void ilitek_platform_tp_hw_reset(bool isEnable)
{
	ipio_info("HW Reset: %d\n", isEnable);

	if (!gpio_is_valid(ipd->reset_gpio))
		return ;
	ilitek_platform_disable_irq();

	if (isEnable) {
		gpio_direction_output(ipd->reset_gpio, 1);
		mdelay(ipd->delay_time_high);
		gpio_set_value(ipd->reset_gpio, 0);
		mdelay(ipd->delay_time_low);
		gpio_set_value(ipd->reset_gpio, 1);
		mdelay(ipd->edge_delay);
	} else {
		gpio_set_value(ipd->reset_gpio, 0);
	}

#ifdef HOST_DOWNLOAD
	core_config_ice_mode_enable();
	core_firmware_upgrade(UPDATE_FW_PATH, true);
#endif
	mdelay(10);
	ilitek_platform_enable_irq();
}

EXPORT_SYMBOL(ilitek_platform_tp_hw_reset);

void ilitek_regulator_power_on(bool status)
{
	int res = 0;

	if (!ipd->REGULATOR_POWER_ON)
		return ;
	ipio_info("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ipd->vdd) {
			res = regulator_enable(ipd->vdd);
			if (res < 0)
				ipio_err("regulator_enable vdd fail\n");
		}
		if (ipd->vdd_i2c) {
			res = regulator_enable(ipd->vdd_i2c);
			if (res < 0)
				ipio_err("regulator_enable vdd_i2c fail\n");
		}
	} else {
		if (ipd->vdd) {
			res = regulator_disable(ipd->vdd);
			if (res < 0)
				ipio_err("regulator_enable vdd fail\n");
		}
		if (ipd->vdd_i2c) {
			res = regulator_disable(ipd->vdd_i2c);
			if (res < 0)
				ipio_err("regulator_enable vdd_i2c fail\n");
		}
	}
	core_config->icemodeenable = false;
	mdelay(5);
}

EXPORT_SYMBOL(ilitek_regulator_power_on);

#ifdef BATTERY_CHECK
static void read_power_status(uint8_t *buf)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	ssize_t byte = 0;

	old_fs = get_fs();
	set_fs(get_ds());

	f = filp_open(POWER_STATUS_PATH, O_RDONLY, 0);
	if (ERR_ALLOC_MEM(f)) {
		ipio_err("Failed to open %s\n", POWER_STATUS_PATH);
		return;
	}

	f->f_op->llseek(f, 0, SEEK_SET);
	byte = f->f_op->read(f, buf, 20, &f->f_pos);

	ipio_debug(DEBUG_BATTERY, "Read %d bytes\n", (int)byte);

	set_fs(old_fs);
	filp_close(f, NULL);
}

static void ilitek_platform_vpower_notify(struct work_struct *pWork)
{
	uint8_t charge_status[20] = { 0 };
	static int charge_mode;

	ipio_debug(DEBUG_BATTERY, "isEnableCheckPower = %d\n",
		   ipd->isEnablePollCheckPower);
	read_power_status(charge_status);
	ipio_debug(DEBUG_BATTERY, "Batter Status: %s\n", charge_status);

	if (strnstr(charge_status, "Charging", strlen(charge_status)) != NULL
	    || strnstr(charge_status, "Full", strlen(charge_status)) != NULL
	    || strnstr(charge_status, "Fully charged", strlen(charge_status)) != NULL) {
		if (charge_mode != 1) {
			ipio_debug(DEBUG_BATTERY, "Charging mode\n");
			core_config_plug_ctrl(false);
			charge_mode = 1;
		}
	} else {
		if (charge_mode != 2) {
			ipio_debug(DEBUG_BATTERY, "Not charging mode\n");
			core_config_plug_ctrl(true);
			charge_mode = 2;
		}
	}

	if (ipd->isEnablePollCheckPower)
		queue_delayed_work(ipd->check_power_status_queue,
				   &ipd->check_power_status_work,
				   ipd->work_delay);
}
#endif/*BATTERY_CHECK*/

#ifdef CONFIG_FB
static int ilitek_platform_notifier_fb(struct notifier_block *self,
				       unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;

	ipio_info("Notifier's event = %ld\n", event);

	/*
	 *  FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *  FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change
	 * occurred.
	 */
	if (evdata && evdata->data && ((event == FB_EARLY_EVENT_BLANK) || (event == FB_EVENT_BLANK))) {
	//if (evdata && evdata->data && (event == FB_EVENT_BLANK)) {
		blank = evdata->data;

		if ((*blank == FB_BLANK_POWERDOWN) && event == FB_EARLY_EVENT_BLANK) {
			ipio_info("TP Suspend\n");

			if (!core_firmware->isUpgrading)
				core_config_ic_suspend();
		} else if ((*blank == FB_BLANK_UNBLANK
			 || *blank == FB_BLANK_NORMAL) && event == FB_EVENT_BLANK){
			ipio_info("TP Resuem\n");

			if (!core_firmware->isUpgrading)
				core_config_ic_resume();
		}
	}

	return NOTIFY_OK;
}
#else /* CONFIG_HAS_EARLYSUSPEND */
static void ilitek_platform_early_suspend(struct early_suspend *h)
{
	ipio_info("TP Suspend\n");

	/* TODO: there is doing nothing if an upgrade firmware's processing. */

	core_fr_touch_release(0, 0, 0);

	input_sync(core_fr->input_device);

	core_fr->isEnableFR = false;

	core_config_ic_suspend();
}

static void ilitek_platform_late_resume(struct early_suspend *h)
{
	ipio_info("TP Resuem\n");

	core_fr->isEnableFR = true;
	core_config_ic_resume();
}

#endif

/**
 * reg_power_check - register a thread to inquery status at certain time.
 */
static int ilitek_platform_reg_power_check(void)
{
	int res = 0;

#ifdef BATTERY_CHECK
	INIT_DELAYED_WORK(&ipd->check_power_status_work,
			  ilitek_platform_vpower_notify);
	ipd->check_power_status_queue = create_workqueue("ili_power_check");
	ipd->work_delay = msecs_to_jiffies(CHECK_BATTERY_TIME);
	if (!ipd->check_power_status_queue) {
		ipio_err
		    ("Failed to create a work thread to check power status\n");
		ipd->vpower_reg_nb = false;
		res = ERROR;
	} else {
		ipio_info
		    ("Created a work thread to check power status at every %u jiffies\n", (unsigned int)ipd->work_delay);

		if (ipd->isEnablePollCheckPower) {
			queue_delayed_work(ipd->check_power_status_queue,
					   &ipd->check_power_status_work,
					   ipd->work_delay);
			ipd->vpower_reg_nb = true;
		}
	}
#endif /* BATTERY_CHECK */

	return res;
}

/**
 * Register a callback function when the event of suspend and resume occurs.
 *
 * The default used to wake up the cb function comes from notifier block
 * mechnaism. If you'd rather liek to use early suspend,
 * CONFIG_HAS_EARLYSUSPEND in kernel config must be enabled.
 */
static int ilitek_platform_reg_suspend(void)
{
	int res = 0;
	ipio_info("Register suspend/resume callback function\n");
#ifdef CONFIG_FB
	ipd->notifier_fb.notifier_call = ilitek_platform_notifier_fb;
	res = fb_register_client(&ipd->notifier_fb);
#else
	ipd->early_suspend->suspend = ilitek_platform_early_suspend;
	ipd->early_suspend->esume = ilitek_platform_late_resume;
	ipd->early_suspend->level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	res = register_early_suspend(ipd->early_suspend);
#endif /* CONFIG_FB */

	return res;
}

#ifndef USE_KTHREAD
static void ilitek_platform_work_queue(struct work_struct *work)
{
	ipio_debug(DEBUG_IRQ, "work_queue: IRQ = %d\n", ipd->isEnableIRQ);

	core_fr_handler();

	if (!ipd->isEnableIRQ)
		ilitek_platform_enable_irq();
}
#endif /* USE_KTHREAD */

static irqreturn_t ilitek_platform_irq_handler(int irq, void *dev_id)
{
	ipio_debug(DEBUG_IRQ, "IRQ = %d\n", ipd->isEnableIRQ);

	if (core_fr->actual_fw_mode == P5_0_FIRMWARE_TEST_MODE) {
		if (core_mp->busy_cdc == ISR_CHECK) {
			ipio_debug(DEBUG_IRQ, "MP INT enter irq\n");
			core_mp->mp_isr_check_busy_free = true;
			ipio_info("MP isr check busy is free ,%d\n", core_mp->mp_isr_check_busy_free);
		}
	} else {
		if (ipd->isEnableIRQ) {
			ilitek_platform_disable_irq();
	#ifdef USE_KTHREAD
			ipd->irq_trigger = true;
			ipio_debug(DEBUG_IRQ, "kthread: irq_trigger = %d\n",
				ipd->irq_trigger);
			wake_up_interruptible(&waiter);
	#else
			schedule_work(&ipd->report_work_queue);
	#endif /* USE_KTHREAD */
		}
	}
	return IRQ_HANDLED;
}

static int ilitek_platform_input_init(void)
{
	int res = 0;

	ipd->input_device = input_allocate_device();

	if (ERR_ALLOC_MEM(ipd->input_device)) {
		ipio_err("Failed to allocate touch input device\n");
		res = -ENOMEM;
		goto fail_alloc;
	}
#if (INTERFACE == I2C_INTERFACE)
	ipd->input_device->name = ipd->client->name;
	ipd->input_device->phys = "I2C";
	ipd->input_device->dev.parent = &ipd->client->dev;
	ipd->input_device->id.bustype = BUS_I2C;
#else
	ipd->input_device->name = DEVICE_ID;
	ipd->input_device->phys = "SPI";
	ipd->input_device->dev.parent = &ipd->spi->dev;
	ipd->input_device->id.bustype = BUS_SPI;
#endif

	core_fr_input_set_param(ipd->input_device);
	/* register the input device to input sub-system */
	res = input_register_device(ipd->input_device);
	if (res < 0) {
		ipio_err("Failed to register touch input device, res = %d\n",
			 res);
		goto out;
	}

	return res;

fail_alloc:
	input_free_device(core_fr->input_device);
	return res;

out:
	input_unregister_device(ipd->input_device);
	input_free_device(core_fr->input_device);
	return res;
}

#ifdef USE_KTHREAD
static int kthread_handler(void *arg)
{
	int res = 0;
	char *str = (char *)arg;

	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	if (strcmp(str, "boot_fw") == 0) {
		/* FW Upgrade event */
		core_firmware->isboot = true;

		ilitek_platform_disable_irq();

#ifdef BOOT_FW_UPGRADE
		res = core_firmware_boot_upgrade();
		if (res < 0)
			ipio_err("Failed to upgrade FW at boot stage\n");
#endif
		ilitek_platform_enable_irq();

		ilitek_platform_input_init();

		core_firmware->isboot = false;
	} else if (strcmp(str, "irq") == 0) {
		/* IRQ event */
		struct sched_param param = {.sched_priority = 4 };

		sched_setscheduler(current, SCHED_RR, &param);

		while (!kthread_should_stop() && !ipd->free_irq_thread) {
			ipio_debug(DEBUG_IRQ,
				   "kthread: before->irq_trigger = %d\n",
				   ipd->irq_trigger);
			add_wait_queue(&waiter, &wait);
			for (;;) {
				if (ipd->irq_trigger)
					break;
				wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
			}
			remove_wait_queue(&waiter, &wait);
			wait_event_interruptible(waiter, ipd->irq_trigger);
			ipd->irq_trigger = false;
			set_current_state(TASK_RUNNING);
			ipio_debug(DEBUG_IRQ,
				   "kthread: after->irq_trigger = %d\n",
				   ipd->irq_trigger);
			core_fr_handler();
			ilitek_platform_enable_irq();
		}
	} else {
		ipio_err("Unknown EVENT\n");
	}

	return res;
}
#endif

static int ilitek_platform_isr_register(void)
{
	int res = 0;

#ifdef USE_KTHREAD
	ipd->irq_thread = kthread_run(kthread_handler, "irq",
	 "ili_irq_thread");
	if (ipd->irq_thread == (struct task_struct *)ERR_PTR) {
		ipd->irq_thread = NULL;
		ipio_err("Failed to create kthread\n");
		res = -ENOMEM;
		goto out;
	}
	ipd->irq_trigger = false;
	ipd->free_irq_thread = false;
#else
	INIT_WORK(&ipd->report_work_queue, ilitek_platform_work_queue);
#endif /* USE_KTHREAD */

	ipd->isr_gpio = gpio_to_irq(ipd->int_gpio);

	ipio_info("ipd->isr_gpio = %d\n", ipd->isr_gpio);

	res = request_threaded_irq(ipd->isr_gpio,
				   NULL,
				   ilitek_platform_irq_handler,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "ilitek", NULL);

	if (res != 0) {
		ipio_err("Failed to register irq handler, irq = %d, res = %d\n", ipd->isr_gpio, res);
		goto out;
	}

	ipd->isEnableIRQ = true;

out:
	return res;
}

static int ilitek_platform_gpio(void)
{
	int res = 0;
#ifdef CONFIG_OF
#if (INTERFACE == I2C_INTERFACE)
	struct device_node *dev_node = ipd->client->dev.of_node;
#else
	struct device_node *dev_node = ipd->spi->dev.of_node;
#endif
	uint32_t flag;

	ipd->int_gpio =
	    of_get_named_gpio_flags(dev_node, DTS_INT_GPIO, 0, &flag);
	ipd->reset_gpio =
	    of_get_named_gpio_flags(dev_node, DTS_RESET_GPIO, 0, &flag);
#endif /* CONFIG_OF */

	ipio_info("GPIO INT: %d\n", ipd->int_gpio);
	ipio_info("GPIO RESET: %d\n", ipd->reset_gpio);

	if (!gpio_is_valid(ipd->int_gpio)) {
		ipio_err("Invalid INT gpio: %d\n", ipd->int_gpio);
		return -EBADR;
	}

	res = gpio_request(ipd->int_gpio, "ILITEK_TP_IRQ");
	if (res < 0) {
		ipio_err("Request IRQ GPIO failed, res = %d\n", res);
		goto out;
	}

	if (gpio_is_valid(ipd->reset_gpio)) {
		res = gpio_request(ipd->reset_gpio, "ILITEK_TP_RESET");
		if (res < 0) {
			ipio_err("Request RESET GPIO failed, res = %d\n", res);
			return 0;
		}
	}
	gpio_direction_input(ipd->int_gpio);

out:
	return res;
}

int ilitek_platform_read_tp_info(void)
{
	if (core_config_get_chip_id() < 0) {
		ipio_err("Failed to get chip id\n");
		return CHIP_ID_ERR;
	}

	if (core_config_get_protocol_ver() < 0)
		ipio_err("Failed to get protocol version\n");

	if (core_config_get_fw_ver() < 0)
		ipio_err("Failed to get firmware version\n");

	if (core_config_get_core_ver() < 0)
		ipio_err("Failed to get core version\n");

	if (core_config_get_tp_info() < 0)
		ipio_err("Failed to get TP information\n");

	if (core_config_get_key_info() < 0)
		ipio_err("Failed to get key information\n");

	return 0;

}

EXPORT_SYMBOL(ilitek_platform_read_tp_info);

/**
 * Remove Core APIs memeory being allocated.
 */
static void ilitek_platform_core_remove(void)
{
	ipio_info("Remove all core's compoenets\n");
	ilitek_proc_remove();
	ilitek_sys_remove();
	core_flash_remove();
	core_firmware_remove();
	core_fr_remove();
	core_config_remove();
	core_i2c_remove();
	core_protocol_remove();
	core_gesture_remove();
}

/**
 * The function is to initialise all necessary structurs in those core APIs,
 * they must be called before the i2c dev probes up successfully.
 */
static int ilitek_platform_core_init(void)
{
	ipio_info("Initialise core's components\n");

	if (core_config_init() < 0 || core_protocol_init() < 0 ||
	    core_firmware_init() < 0 || core_fr_init() < 0 ||
	    core_gesture_init() < 0) {
		ipio_err("Failed to initialise core components\n");
		return -EINVAL;
	}
#if (INTERFACE == I2C_INTERFACE)
	if (core_i2c_init(ipd->client) < 0)
#else
	if (core_spi_init(ipd->spi) < 0)
#endif
	{
		ipio_err("Failed to initialise interface\n");
		return -EINVAL;
	}
	return 0;
}

#if (INTERFACE == I2C_INTERFACE)
static int ilitek_platform_remove(struct i2c_client *client)
#else
static int ilitek_platform_remove(struct spi_device *spi)
#endif
{
	ipio_info("Remove platform components\n");

	if (ipd->isEnableIRQ)
		disable_irq_nosync(ipd->isr_gpio);

	if (ipd->isr_gpio != 0 && ipd->int_gpio != 0 && ipd->reset_gpio != 0) {
		free_irq(ipd->isr_gpio, (void *)ipd->i2c_id);
		gpio_free(ipd->int_gpio);
		gpio_free(ipd->reset_gpio);
	}
#ifdef CONFIG_FB
	fb_unregister_client(&ipd->notifier_fb);
#else
	unregister_early_suspend(&ipd->early_suspend);
#endif /* CONFIG_FB */

#ifdef USE_KTHREAD
	if (ipd->irq_thread != NULL) {
		ipd->irq_trigger = true;
		ipd->free_irq_thread = true;
		wake_up_interruptible(&waiter);
		kthread_stop(ipd->irq_thread);
		ipd->irq_thread = NULL;
	}
#endif /* USE_KTHREAD */

	if (ipd->input_device != NULL) {
		input_unregister_device(ipd->input_device);
//		input_free_device(ipd->input_device);
		input_free_device(NULL);
	}

	if (ipd->vpower_reg_nb) {
		cancel_delayed_work_sync(&ipd->check_power_status_work);
		destroy_workqueue(ipd->check_power_status_queue);
	}

	ilitek_platform_core_remove();

	ipio_kfree((void **)&ipd);

	return 0;
}

/*The Ili_parse_dt func would be called when driver probe*/
static int Ili_parse_dt(struct device *dev,
			struct ilitek_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;

	u32 coords[ILI_COORDS_ARR_SIZE];
	u32 temp_val;
	int coords_size;

	rc = of_property_read_string(np, "ilitek,tp_ic_type", &pdata->TP_IC_TYPE);
	if (rc && (rc != -EINVAL)) {
		ipio_err("Unable to read name\n");
		return rc;
	}

    ipio_err("pdata->TP_IC_TYPE = %s\n", pdata->TP_IC_TYPE);

	prop = of_find_property(np, "ilitek,display-coords", NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != ILI_COORDS_ARR_SIZE) {
		ipio_err("invalid %s\n", pdata->TP_IC_TYPE);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, "ilitek,display-coords", coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		ipio_err("Unable to read ilitek,display-coords\n");
		return rc;
	}

	pdata->x_min = coords[0];
	pdata->y_min = coords[1];
	pdata->x_max = coords[2];
	pdata->y_max = coords[3];

	ipio_info("x_min = %d,y_min = %d,x_max = %d,y_max = %d,coords_size = %d\n ",
		ipd->x_min, ipd->y_min, ipd->x_max, ipd->y_max, coords_size);

	pdata->MT_B_TYPE = of_property_read_bool(np,
						"ilitek,mt_b_type");
	if (pdata->MT_B_TYPE)
		ipio_info("pdata->MT_B_TYPE ture ");
	else
		ipio_info("pdata->MT_B_TYPE false ");

	pdata->REGULATOR_POWER_ON = of_property_read_bool(np,
						"ilitek,regulator_power_on");
	if (pdata->REGULATOR_POWER_ON)
		ipio_info("pdata->REGULATOR_POWER_ON ture ");
	else
		ipio_info("pdata->REGULATOR_POWER_ON false ");

	pdata->BATTERY_CHECK = of_property_read_bool(np,
						"ilitek,battery_check");
	if (pdata->BATTERY_CHECK)
		ipio_info("pdata->BATTERY_CHECK ture ");
	else
		ipio_info("pdata->BATTERY_CHECK false ");

	pdata->ENABLE_BATTERY_CHECK = of_property_read_bool(np,
						"ilitek,enable_battery_check");
	if (pdata->ENABLE_BATTERY_CHECK)
		ipio_info("pdata->ENABLE_BATTERY_CHECK ture ");
	else
		ipio_info("pdata->ENABLE_BATTERY_CHECK false ");

	if (of_property_read_bool(np, "ilitek,x_flip")) {
		pr_notice("using flipped X axis\n");
		pdata->x_flip = true;
	}

	if (of_property_read_bool(np, "ilitek,y_flip")) {
		pr_notice("using flipped Y axis\n");
		pdata->y_flip = true;
	}

	rc = of_property_read_u32(np, "ilitek,tpd_height",
							&temp_val);
	if (!rc)
		pdata->TPD_HEIGHT = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "ilitek,tpd_width",
							&temp_val);
	if (!rc)
		pdata->TPD_WIDTH = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "ilitek,max_touch_num",
							&temp_val);
	if (!rc)
		pdata->MAX_TOUCH_NUM = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "ilitek,tp_touch_ic",
							&temp_val);
	if (!rc)
		pdata->chip_id = temp_val;
	else
		return rc;

	return 0;

}


/*
 * The probe func would be called after an i2c device was detected by kernel.
 *
 * It will still return zero even if it couldn't get a touch ic info.
 * The reason for why we allow it passing the process is because
 * users/developersmight want to have access to ICE mode to upgrade a
 * firwmare forcelly.
 */
#if (INTERFACE == I2C_INTERFACE)
static int ilitek_platform_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
#else
static int ilitek_platform_probe(struct spi_device *spi)
#endif
{
	int ret;

	const char *vdd_name = "vdd";
	const char *vcc_i2c_name = "vcc_i2c";

	/* initialise the struct of touch ic memebers. */
	ipd = kzalloc(sizeof(*ipd), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ipd)) {
		ipio_err("Failed to allocate ipd memory, %ld\n", PTR_ERR(ipd));
		return -ENOMEM;
	}
#if (INTERFACE == I2C_INTERFACE)
	if (client == NULL) {
		ipio_err("i2c client is NULL\n");
		return -ENODEV;
	}

	/* Set i2c slave addr if it's not configured */
	ipio_info("I2C Slave address = 0x%x\n", client->addr);
	if (client->addr != ILI7807_SLAVE_ADDR
	    || client->addr != ILI9881_SLAVE_ADDR) {
		client->addr = ILI9881_SLAVE_ADDR;
		ipio_err
		    ("I2C Slave addr doesn't be set up, use default : 0x%x\n",
		     client->addr);
	}
	ipd->client = client;
	ipd->i2c_id = id;
#else
	if (spi == NULL) {
		ipio_err("spi device is NULL\n");
		return -ENODEV;
	}

	ipd->spi = spi;
#endif
	/* Get dtsi configured value*/
	ipd->TP_IC_TYPE = kzalloc(128, GFP_KERNEL);
	ret = Ili_parse_dt(&client->dev, ipd);
	if (ret) {
		ipio_err("DT parsing failed\n");
		return ret;
	}

	ipd->isEnableIRQ = false;
	ipd->isEnablePollCheckPower = false;
	ipd->vpower_reg_nb = false;

	ipio_info("Driver Version : %s\n", DRIVER_VERSION);
	ipio_info("Driver for Touch IC :  %x\n", ipd->chip_id);
	ipio_info("Driver interface :  %s\n",
		  (INTERFACE == I2C_INTERFACE) ? "I2C" : "SPI");

	/*
	 * Different ICs may require different delay time for the reset.
	 * They may also depend on what your platform need to.
	 */
	if (ipd->chip_id == CHIP_TYPE_ILI7807) {
		ipd->delay_time_high = 10;
		ipd->delay_time_low = 5;
		ipd->edge_delay = 200;
	} else if (ipd->chip_id == CHIP_TYPE_ILI9881) {
		ipd->delay_time_high = 10;
		ipd->delay_time_low = 5;
#if (INTERFACE == I2C_INTERFACE)
		ipd->edge_delay = 100;
#else
		ipd->edge_delay = 1;
#endif
	} else {
		ipd->delay_time_high = 10;
		ipd->delay_time_low = 10;
		ipd->edge_delay = 10;
	}

	mutex_init(&ipd->plat_mutex);
	spin_lock_init(&ipd->plat_spinlock);

	/* Init members for debug */
	mutex_init(&ipd->ilitek_debug_mutex);
	mutex_init(&ipd->ilitek_debug_read_mutex);
	init_waitqueue_head(&(ipd->inq));
	ipd->debug_data_frame = 0;
	ipd->debug_node_open = false;

	if (ipd->REGULATOR_POWER_ON) {
		ipd->vdd = regulator_get(&ipd->client->dev, vdd_name);
		if (ERR_ALLOC_MEM(ipd->vdd)) {
			ipio_err("regulator_get vdd fail\n");
			ipd->vdd = NULL;
			return -ENOMEM;
		} else {
		if (regulator_set_voltage(ipd->vdd, VDD_VOLTAGE, VDD_VOLTAGE) <
		    0)
			ipio_err("Failed to set vdd %d.\n", VDD_VOLTAGE);
			goto reg_vdd_put;
		}

		ipd->vdd_i2c = regulator_get(&ipd->client->dev, vcc_i2c_name);
		if (ERR_ALLOC_MEM(ipd->vdd_i2c)) {
			ipio_err("regulator_get vdd_i2c fail.\n");
			ipd->vdd_i2c = NULL;
			goto reg_vdd_set_vtg;
		} else {
			if (regulator_set_voltage
				(ipd->vdd_i2c, VDD_I2C_VOLTAGE, VDD_I2C_VOLTAGE) < 0)
				ipio_err("Failed to set vdd_i2c %d\n",
				VDD_I2C_VOLTAGE);
				goto reg_vcc_put;
		}
		ilitek_regulator_power_on(true);
	} /* REGULATOR_POWER_ON */

	ret  = ilitek_platform_gpio() ;
	if (ret < 0) {
		ipio_err("Failed to request gpios\n ");
		goto gpio_err;
	}

	/*
	 * If kernel failes to allocate memory to the core components, driver
	 * will be unloaded.
	 */
	ret = ilitek_platform_core_init() ;
	if (ret < 0) {
		ipio_err("Failed to allocate cores' mem\n");
		goto core_init_err;
	}
#ifdef HOST_DOWNLOAD
	core_firmware_boot_host_download();
#else
	/*ilitek_platform_tp_hw_reset(true);*/
	/* Soft reset */
	core_config_ice_mode_enable();
	core_config_set_watch_dog(false);
	mdelay(10);
	core_config_ic_reset();
#endif

	/* get our tp ic information */
	ret = ilitek_platform_read_tp_info();
	if (ret == CHIP_ID_ERR) {
		core_config->chip_pid = CHIP_ID_ERR;
		ipio_err("Failed to raed chip id %d,%d\n", ret, (-ENODEV));
		goto read_tp_info_err;
	}

	/* If it defines boot upgrade, input register will be done inside boot
	 * function.
	 */
#ifndef BOOT_FW_UPGRADE
	ret = ilitek_platform_input_init() ;
	if (ret < 0) {
		ipio_err("Failed to init input device in kernel\n");
		goto input_init_err;
	}
#endif /* BOOT_FW_UPGRADE */

	ret = ilitek_platform_isr_register() ;
	if (ret < 0) {
		ipio_err("Failed to register ISR\n");
		goto isr_register_err;
	}

	ret = ilitek_platform_reg_suspend() ;
	if (ret < 0) {
		ipio_err("Failed to register suspend/resume function\n");
		goto reg_suspend_err;
	}

	ret = ilitek_platform_reg_power_check() ;
	if (ret < 0) {
		ipio_err("Failed to register power check function\n");
		goto reg_power_check_err;
	}
	/* Create nodes for users */
	ilitek_proc_init();

	/* Create sys node */
	ret = ilitek_sys_init() ;
	if (ret < 0) {
		ipio_err("sys class files creation failed\n");
		goto sys_init_err;
	}

#ifdef BOOT_FW_UPGRADE
	ipd->update_thread =
	    kthread_run(kthread_handler, "boot_fw", "ili_fw_boot");
	if (ipd->update_thread == (struct task_struct *)ERR_PTR) {
		ipd->update_thread = NULL;
		ipio_err("Failed to create fw upgrade thread\n");
		goto sys_init_err;
	}
#endif /* BOOT_FW_UPGRADE */
	ipd->suspended = false;
	return 0;

sys_init_err:
reg_power_check_err:
reg_suspend_err:
	free_irq(ipd->isr_gpio, (void *)ipd->i2c_id);
isr_register_err:
#ifndef BOOT_FW_UPGRADE
input_init_err:
#endif
read_tp_info_err:

core_init_err:
	if (gpio_is_valid(ipd->int_gpio))
		gpio_free(ipd->int_gpio);
	if (gpio_is_valid(ipd->reset_gpio))
		gpio_free(ipd->reset_gpio);
gpio_err:

reg_vcc_put:
	if (ipd->REGULATOR_POWER_ON)
		regulator_put(ipd->vdd);
reg_vdd_set_vtg:
	if (ipd->REGULATOR_POWER_ON) {
		if (regulator_count_voltages(ipd->vdd) > 0)
			regulator_set_voltage(ipd->vdd, 0, 3300000);
	}
reg_vdd_put:
	if (ipd->REGULATOR_POWER_ON)
		regulator_put(ipd->vdd);
	return ret;
}

static const struct i2c_device_id tp_device_id[] = {
	{DEVICE_ID, 0},
	{},			/* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, tp_device_id);

/*
 * The name in the table must match the definiation
 * in a dts file.
 *
 */
static struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

#if (INTERFACE == I2C_INTERFACE)
static struct i2c_driver tp_i2c_driver = {
	.driver = {
		   .name = DEVICE_ID,
		   .owner = THIS_MODULE,
		   .of_match_table = tp_match_table,
		   },
	.probe = ilitek_platform_probe,
	.remove = ilitek_platform_remove,
	.id_table = tp_device_id,
};
#else
static struct spi_driver tp_spi_driver = {
	.driver = {
		   .name = DEVICE_ID,
		   .owner = THIS_MODULE,
		   .of_match_table = tp_match_table,
		   },
	.probe = ilitek_platform_probe,
	.remove = ilitek_platform_remove,
};
#endif

static int __init ilitek_platform_init(void)
{
	int res = 0;

	ipio_info("TP driver init\n");

#if (INTERFACE == I2C_INTERFACE)
	ipio_info("TP driver add i2c interface\n");
	res = i2c_add_driver(&tp_i2c_driver);
	ipio_info("Summer %d \n", res);
	if (res < 0) {
		ipio_err("Failed to add i2c driver\n");
		i2c_del_driver(&tp_i2c_driver);
		return -ENODEV;
	}
#else
	ipio_info("TP driver add spi interface\n");
	res = spi_register_driver(&tp_spi_driver);
	if (res < 0) {
		ipio_err("Failed to add ilitek driver\n");
		spi_unregister_driver(&tp_spi_driver);
		return -ENODEV;
	}
#endif

	ipio_info("Succeed to add ilitek driver\n");
	return res;
}

static void __exit ilitek_platform_exit(void)
{
	ipio_info("I2C driver has been removed\n");

#if (INTERFACE == I2C_INTERFACE)
	i2c_del_driver(&tp_i2c_driver);
#else
	spi_unregister_driver(&tp_spi_driver);
#endif
}

module_init(ilitek_platform_init);
module_exit(ilitek_platform_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");


