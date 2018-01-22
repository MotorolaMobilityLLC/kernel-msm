/*
 * siw_touch.c - SiW touch core driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include <linux/input/siw_touch_notify.h>

#include "siw_touch.h"
#include "siw_touch_hal.h"
#include "siw_touch_bus.h"
#include "siw_touch_event.h"
#include "siw_touch_gpio.h"
#include "siw_touch_irq.h"
#include "siw_touch_sys.h"

#if !defined(__SIW_CONFIG_OF)
	#pragma message("[SiW - Warning] No COFIG_OF")
#endif

u32 t_pr_dbg_mask = DBG_NONE;
u32 t_dev_dbg_mask = DBG_NONE;
/*
 * DBG_NONE | DBG_BASE | DBG_IRQ | DBG_NOTI | DBG_EVENT = 201326721(0xC000081)
 */

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/pr_dbg_mask
 * (2) insmod {Siw Touch Module Name}.ko pr_dbg_mask=<value>
 */
module_param_named(pr_dbg_mask, t_pr_dbg_mask, uint, S_IRUGO|S_IWUSR|S_IWGRP);

/* usage
 * (1) echo <value> > /sys/module/{Module Name}/parameters/dev_dbg_mask
 * (2) insmod {Siw Touch Module Name}.ko dev_dbg_mask=<value>
 */
module_param_named(dev_dbg_mask, t_dev_dbg_mask, uint, S_IRUGO|S_IWUSR|S_IWGRP);


u32 t_dbg_flag;
/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/dbg_flag
 * (2) insmod {Siw Touch Module Name}.ko dbg_flag=<value>
 */
module_param_named(dbg_flag, t_dbg_flag, uint, S_IRUGO|S_IWUSR|S_IWGRP);


u32 t_mfts_lpwg;
u32 t_lpwg_mode = LPWG_NONE;
u32 t_lpwg_screen = 1;
u32 t_lpwg_sensor = PROX_FAR;
u32 t_lpwg_qcover;

/* usage
 * (1) echo <value> > /sys/module/{Module Name}/parameters/mfts_lpwg_mode
 * (2) insmod {Siw Touch Module Name}.ko mfts_lpwg_mode=<value>
 */
module_param_named(mfts_lpwg_mode, t_mfts_lpwg, uint, S_IRUGO|S_IWUSR|S_IWGRP);

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/lpwg_mode
 * (2) insmod {Siw Touch Module Name}.ko lpwg_mode=<value>
 */
module_param_named(lpwg_mode, t_lpwg_mode, uint, S_IRUGO|S_IWUSR|S_IWGRP);

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/lpwg_screen
 * (2) insmod {Siw Touch Module Name}.ko lpwg_screen=<value>
 */
module_param_named(lpwg_screen, t_lpwg_screen, uint, S_IRUGO|S_IWUSR|S_IWGRP);

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/lpwg_sensor
 * (2) insmod {Siw Touch Module Name}.ko lpwg_sensor=<value>
 */
module_param_named(lpwg_sensor, t_lpwg_sensor, uint, S_IRUGO|S_IWUSR|S_IWGRP);

/* usage
 * (1) echo <value> > /sys/module/{Siw Touch Module Name}/parameters/lpwg_qcover
 * (2) insmod {Siw Touch Module Name}.ko lpwg_qcover=<value>
 */
module_param_named(lpwg_qcover, t_lpwg_qcover, uint, S_IRUGO|S_IWUSR|S_IWGRP);


static void siw_config_status(struct device *dev)
{
#if defined(__SIW_CONFIG_FB)
	t_dev_info(dev, "cfg status : __SIW_CONFIG_FB\n");
#endif

#if defined(__SIW_CONFIG_SYSTEM_PM)
	t_dev_info(dev, "cfg status : __SIW_CONFIG_SYSTEM_PM\n");
#endif

#if defined(__SIW_CONFIG_FASTBOOT)
	t_dev_info(dev, "cfg status : __SIW_CONFIG_FASTBOOT\n");
#endif
}

static int siw_setup_names(struct siw_ts *ts, struct siw_touch_pdata *pdata)
{
	struct device *dev = ts->dev;
	char *name;

	/*
	 * Mandatory
	 */
	name = pdata_chip_id(pdata);
	if (name == NULL)
		return -EFAULT;

	ts->chip_id = name;
	t_dev_info(dev, "chip id    : %s\n", name);

	name = pdata_chip_name(pdata);
	if (name == NULL)
		return -EFAULT;

	ts->chip_name = name;
	t_dev_info(dev, "chip name  : %s\n", name);

	/*
	 * Optional
	 */
	name = pdata_drv_name(pdata);
	if (name == NULL)
		name = SIW_TOUCH_NAME;

	ts->drv_name = name;
	t_dev_info(dev, "drv name   : %s\n", name);

	name = pdata_idrv_name(pdata);
	if (name == NULL)
		name = SIW_TOUCH_INPUT;

	ts->idrv_name = name;
	t_dev_info(dev, "idrv name  : %s\n", name);

	if (!pdata_test_quirks(pdata, CHIP_QUIRK_NOT_SUPPORT_WATCH)) {
		name = pdata_ext_watch_name(pdata);
		if (name == NULL)
			name = SIW_TOUCH_EXT_WATCH;

		ts->ext_watch_name = name;
		t_dev_info(dev, "watch name : %s\n", name);
	}

	return 0;
}

int siw_setup_params(struct siw_ts *ts, struct siw_touch_pdata *pdata)
{
	struct device *dev = ts->dev;
	int max_finger = 0;
	int type = 0;
	u32 mode_allowed = 0;
	int ret = 0;

	siw_config_status(dev);

	max_finger = pdata_max_finger(pdata);
	if ((max_finger < 0) || (max_finger > MAX_FINGER)) {
		t_dev_err(dev, "invalid max finger, %d\n", max_finger);
		return -EFAULT;
	}
	ts->max_finger = max_finger;
	t_dev_info(dev, "max finger : %d\n", max_finger);

	type = pdata_chip_type(pdata);
	if (!type)
		return -EFAULT;

	ts->chip_type = type;
	t_dev_info(dev, "chip type  : 0x%04X\n", type);

	mode_allowed = pdata->mode_allowed;
	if (!mode_allowed)
		return -EFAULT;

#if defined(__SIW_CONFIG_SYSTEM_PM)
	mode_allowed &= ~(LCD_MODE_BIT_U0|LCD_MODE_BIT_U3_PARTIAL);
	mode_allowed &= ~(LCD_MODE_BIT_U2|LCD_MODE_BIT_U2_UNBLANK);
#endif	/* __SIW_CONFIG_SYSTEM_PM */
	ts->mode_allowed = mode_allowed;
	t_dev_info(dev, "mode bit   : 0x%08X\n", mode_allowed);

	ret = siw_setup_names(ts, pdata);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * SiW Operations
 */
static void siw_setup_reg_quirks(struct siw_ts *ts)
{
	if (ts->pdata->reg_quirks) {
		struct device *dev = ts->dev;
		u32 *curr_reg;
		u32 *copy_reg;
		struct siw_hal_reg *reg_org;
		struct siw_hal_reg_quirk *reg_quirks = ts->pdata->reg_quirks;
		int cnt = sizeof(struct siw_hal_reg)>>2;
		char *name = touch_chip_name(ts);
		u32 new_addr, old_addr;
		int total = 0;
		int missing = 0;
		int show_log = 1;
		int found = 0;
		int i;

		reg_org = ts->ops_ext->reg;

		while (1) {
			old_addr = reg_quirks->old_addr;
			new_addr = reg_quirks->new_addr;

			if (old_addr == (1<<31)) {
				t_dev_info(dev, "%s reg quirks: ...\n",
					name);
				show_log = 0;
				reg_quirks++;
				continue;
			}

			if ((old_addr == ~0) || (new_addr == ~0))
				break;

			found = 0;
			copy_reg = (u32 *)reg_org;
			curr_reg = (u32 *)siw_ops_reg(ts);
			for (i = 0; i < cnt; i++) {
				if ((*copy_reg) == old_addr) {
					(*curr_reg) = new_addr;
					found = 1;

					break;
				}
				copy_reg++;
				curr_reg++;
			}
			if (found) {
				if (show_log) {
					t_dev_info(dev, "%s reg quirks: [%d] %04Xh -> %04Xh\n",
						name, total,
						old_addr, new_addr);
				}
			} else {
				t_dev_warn(dev, "%s reg quirks: [%d] %04Xh not found\n",
					name, total,
					old_addr);
				missing++;
			}
			total++;

			reg_quirks++;
		}
		t_dev_info(dev, "%s reg quirks: t %d, m %d\n",
			name, total, missing);
	}
}

void *siw_setup_operations(struct siw_ts *ts,
	struct siw_touch_operations *ops_ext)
{
	if (!ops_ext)
		return NULL;

	ts->ops_ext = ops_ext;
	memcpy(&ts->ops_in, ops_ext, sizeof(struct siw_touch_operations));

	ts->ops_in.reg = &ts->__reg;
	memcpy(ts->ops_in.reg, ops_ext->reg, sizeof(struct siw_hal_reg));

	ts->ops = &ts->ops_in;

	siw_setup_reg_quirks(ts);

	return ts->ops;
}

/**
 * siw_touch_set() - set touch data
 * @dev: device to use
 * @cmd: set command
 * @buf: data to store
 *
 * Return:
 * On success, the total number of bytes of data stored to device.
 * Otherwise, it returns zero or minus value
 */
int siw_touch_set(struct device *dev, u32 cmd, void *buf)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	if (!buf)
		t_dev_err(dev, "NULL buf\n");

	mutex_lock(&ts->lock);
	ret = siw_ops_set(ts, cmd, buf);
	mutex_unlock(&ts->lock);

	return ret;
}

/**
 * siw_touch_get() - get touch data
 * @dev: device to use
 * @cmd: set command
 * @buf: data to store
 *
 * Return:
 * On success, the total number of bytes of data loaded from device.
 * Otherwise, it returns zero or minus value
 */
int siw_touch_get(struct device *dev, u32 cmd, void *buf)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	if (!buf)
		t_dev_err(dev, "NULL buf\n");

	mutex_lock(&ts->lock);
	ret = siw_ops_get(ts, cmd, buf);
	mutex_unlock(&ts->lock);

	return ret;
}

/**
 * siw_touch_suspend() - touch suspend
 * @dev: device to use
 *
 */
static void siw_touch_suspend(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	t_dev_info(dev, "touch core pm suspend start\n");

	cancel_delayed_work_sync(&ts->init_work);
	cancel_delayed_work_sync(&ts->upgrade_work);
	atomic_set(&ts->state.uevent, UEVENT_IDLE);

	mutex_lock(&ts->lock);
	siw_touch_report_all_event(ts);
	atomic_set(&ts->state.fb, FB_SUSPEND);
	/* if need skip, return value is not 0 in pre_suspend */
	ret = siw_ops_suspend(ts);
	mutex_unlock(&ts->lock);

	t_dev_info(dev, "touch core pm suspend end(%d)\n", ret);

	if (ret == 1)
		mod_delayed_work(ts->wq, &ts->init_work, 0);
}

/**
 * siw_touch_resume() - touch resume
 * @dev: device to use
 *
 */
static void siw_touch_resume(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int ret = 0;

	t_dev_info(dev, "touch core pm resume start\n");

	mutex_lock(&ts->lock);
	atomic_set(&ts->state.fb, FB_RESUME);
	/* if need skip, return value is not 0 in pre_resume */
	ret = siw_ops_resume(ts);
	mutex_unlock(&ts->lock);

	t_dev_info(dev, "touch core pm resume end(%d)\n", ret);

	if (ret == 0)
		mod_delayed_work(ts->wq, &ts->init_work, 0);
}

void siw_touch_suspend_call(struct device *dev)
{
	siw_touch_suspend(dev);
}

void siw_touch_resume_call(struct device *dev)
{
	siw_touch_resume(dev);
}

#if !defined(__SIW_CONFIG_EARLYSUSPEND) &&	\
	!defined(__SIW_CONFIG_FB)
#define __SIW_CONFIG_PM_BUS
#endif

void siw_touch_suspend_bus(struct device *dev)
{
#if defined(__SIW_CONFIG_PM_BUS)
	siw_touch_suspend(dev);
#else
	t_dev_info(dev, "touch_suspend_bus(noop)\n");
#endif
}


void siw_touch_resume_bus(struct device *dev)
{
#if defined(__SIW_CONFIG_PM_BUS)
	siw_touch_resume(dev);
#else
	t_dev_info(dev, "touch_resume_bus(noop)\n");
#endif
}

#if defined(__SIW_CONFIG_EARLYSUSPEND)
/**
 * touch pm control using early pm
 *
 */
static void siw_touch_early_suspend(struct early_suspend *h)
{
	struct siw_ts *ts =
		container_of(h, struct siw_ts, early_suspend);
	struct device *dev = ts->dev;

	t_dev_info(ts->dev, "early suspend\n");

	siw_touch_suspend(dev);
}

static void siw_touch_early_resume(struct early_suspend *h)
{
	struct siw_ts *ts =
		container_of(h, struct siw_ts, early_suspend);
	struct device *dev = ts->dev;

	t_dev_info(ts->dev, "early resume\n");

	siw_touch_resume(dev);
}

static int __used siw_touch_init_pm(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	t_dev_dbg_pm(dev, "init pm\n");

	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = siw_touch_early_suspend;
	ts->early_suspend.resume = siw_touch_early_resume;
	register_early_suspend(&ts->early_suspend);
	return 0;
}

static int __used siw_touch_free_pm(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	t_dev_dbg_pm(dev, "free pm\n");

	unregister_early_suspend(&ts->early_suspend);

	return 0;
}
#elif defined(__SIW_CONFIG_FB)
/**
 * touch pm control using FB notifier
 *
 */
static int siw_touch_fb_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
	struct siw_ts *ts =
		container_of(self, struct siw_ts, fb_notif);
	struct fb_event *ev = (struct fb_event *)data;
	int *blank;

	if (!ev || !ev->data)
		return 0;

	blank = (int *)ev->data;

#if defined(__SIW_CONFIG_SYSTEM_PM)
	if (event == FB_EARLY_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK) {
			t_dev_info(ts->dev, "fb_unblank(early)\n");
		} else if (*blank == FB_BLANK_POWERDOWN) {
			t_dev_info(ts->dev, "fb_blank(early)\n");
			siw_touch_suspend(ts->dev);
		}
	}

	if (event == FB_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK) {
			t_dev_info(ts->dev, "fb_unblank\n");
			siw_touch_resume(ts->dev);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			t_dev_info(ts->dev, "fb_blank\n");
		}
	}
#else	/* __SIW_CONFIG_SYSTEM_PM */
	if (event == FB_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK) {
			t_dev_info(ts->dev, "fb_unblank\n");
			siw_touch_resume(ts->dev);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			t_dev_info(ts->dev, "fb_blank\n");
			siw_touch_suspend(ts->dev);
		}
	}
#endif	/* __SIW_CONFIG_SYSTEM_PM */

	return 0;
}

static int __used siw_touch_init_pm(struct siw_ts *ts)
{
	t_dev_dbg_pm(ts->dev, "fb_register_client - fb_notif\n");

	ts->fb_notif.notifier_call = siw_touch_fb_notifier_callback;
	return fb_register_client(&ts->fb_notif);
}

static int __used siw_touch_free_pm(struct siw_ts *ts)
{
	t_dev_dbg_pm(ts->dev, "fb_unregister_client - fb_notif\n");

	fb_unregister_client(&ts->fb_notif);

	return 0;
}
#else
	#pragma message("[SiW - Warning] No core pm operation")
static int __used __siw_touch_init_pm_none(struct siw_ts *ts, int init)
{
	t_dev_dbg_pm(ts->dev, "pm %s none\n", (init) ? "free" : "init");
	return 0;
}
#define siw_touch_init_pm(_ts)		__siw_touch_init_pm_none(_ts, 0)
#define siw_touch_free_pm(_ts)		__siw_touch_init_pm_none(_ts, 1)
#endif

#if defined(__SIW_SUPPORT_ASC)
/**
 * siw_touch_get_max_delta -
 * @ts : touch core info
 *
 */
static void siw_touch_get_max_delta(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	struct asc_info *asc = &(ts->asc);
	int ret;

	if (asc->use_delta_chk == DELTA_CHK_OFF) {
		t_dev_warn(dev, "DELTA_CHK is OFf\n");
		return;
	}

	if (atomic_read(&ts->state.fb) != FB_RESUME) {
		t_dev_warn(dev, "fb state is not FB_RESUME\n");
		return;
	}

	if (atomic_read(&ts->state.core) != CORE_NORMAL) {
		t_dev_warn(dev, "core state is not CORE_NORMAL\n");
		return;
	}

	mutex_lock(&ts->lock);
	ret = siw_ops_asc(ts, ASC_READ_MAX_DELTA, 0);
	mutex_unlock(&ts->lock);
	if (ret < 0) {
		t_dev_err(dev, "delta change failed, %d\n", ret);
		return;
	}

	asc->delta = ret;
	asc->delta_updated = true;

	t_dev_info(dev, "delta = %d\n", asc->delta);
}

static const char * const asc_str[] = {
	"NORMAL",
	"ACUTE",
	"OBTUSE",
};

/**
 * siw_touch_change_sensitivity - change touch sensitivity
 * @ts : touch core info
 * @target : target sensitivity
 *
 */
void siw_touch_change_sensitivity(struct siw_ts *ts,
						int target)
{
	struct device *dev = ts->dev;
	struct asc_info *asc = &(ts->asc);
	int ret;

	if (atomic_read(&ts->state.fb) != FB_RESUME) {
		t_dev_warn(dev, "fb state is not FB_RESUME\n");
		return;
	}

	if (atomic_read(&ts->state.core) != CORE_NORMAL) {
		t_dev_warn(dev, "core state is not CORE_NORMAL\n");
		return;
	}

	if (asc->curr_sensitivity == target)
		return;

	t_dev_info(dev, "sensitivity(curr->next) = (%s -> %s)\n",
				asc_str[asc->curr_sensitivity],
				asc_str[target]);

	mutex_lock(&ts->lock);
	ret = siw_ops_asc(ts, ASC_WRITE_SENSITIVITY, target);
	mutex_unlock(&ts->lock);
	if (ret < 0) {
		t_dev_err(dev, "sensitivity change failed, %d\n", ret);
		return;
	}

	asc->curr_sensitivity = target;
}

static void siw_touch_update_sensitivity(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	struct asc_info *asc = &(ts->asc);
	int target = NORMAL_SENSITIVITY;

	if (asc->use_delta_chk == DELTA_CHK_OFF) {
		t_dev_warn(dev, "DELTA_CHK is OFf\n");
		return;
	}

	if (asc->delta_updated == false) {
		t_dev_warn(dev, "delta is not updated.\n");
		return;
	}

	if (asc->delta < asc->low_delta_thres)
		target = ACUTE_SENSITIVITY;
	else if (asc->delta > asc->high_delta_thres)
		target = OBTUSE_SENSITIVITY;
	else
		target = NORMAL_SENSITIVITY;

	asc->delta_updated = false;
	siw_touch_change_sensitivity(ts, target);
}
#endif	/* __SIW_SUPPORT_ASC */

#define SIW_TOUCH_LPWG_LOCK_NAME		"touch_lpwg"

static void __used siw_touch_init_locks(struct siw_ts *ts)
{
	t_dev_dbg_base(ts->dev, "touch init locks\n");

	mutex_init(&ts->lock);
	mutex_init(&ts->reset_lock);
	mutex_init(&ts->probe_lock);
#if defined(__SIW_SUPPORT_WAKE_LOCK)
	wake_lock_init(&ts->lpwg_wake_lock,
		WAKE_LOCK_SUSPEND, SIW_TOUCH_LPWG_LOCK_NAME);
#endif
}

static void __used siw_touch_free_locks(struct siw_ts *ts)
{
	t_dev_dbg_base(ts->dev, "free locks\n");

	mutex_destroy(&ts->lock);
	mutex_destroy(&ts->reset_lock);
	mutex_destroy(&ts->probe_lock);
#if defined(__SIW_SUPPORT_WAKE_LOCK)
	wake_lock_destroy(&ts->lpwg_wake_lock);
#endif
}

static void siw_touch_initialize(struct siw_ts *ts)
{
	/* lockscreen */
	siw_touch_report_all_event(ts);
}

static void siw_touch_init_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
						struct siw_ts, init_work);
	struct device *dev = ts->dev;
	int do_fw_upgarde = 0;
	int ret = 0;

	t_dev_dbg_base(dev, "init work start\n");

	if (atomic_read(&ts->state.core) == CORE_PROBE)
		do_fw_upgarde |= !!(ts->role.use_fw_upgrade);

	mutex_lock(&ts->lock);
	siw_touch_initialize(ts);
	ret = siw_ops_init(ts);
	if (!ret)
		siw_touch_irq_control(dev, INTERRUPT_ENABLE);

	mutex_unlock(&ts->lock);
	if (ret == -ETDSENTESD)
		/* boot fail detected, but skip(postpone) fw_upgrade */
		return;
	else if (ret == -ETDBOOTFAIL)
		/* boot fail detected, do fw_upgrade */
		do_fw_upgarde |= 0x2;
	else if (ret < 0) {
		if (atomic_read(&ts->state.core) == CORE_PROBE) {
			t_dev_err(dev, "%s init work failed(%d), try again\n",
				touch_chip_name(ts), ret);
			atomic_set(&ts->state.core, CORE_NORMAL);
			siw_touch_qd_init_work_now(ts);
			return;
		}

		t_dev_err(dev, "%s init work failed, %d\n",
				touch_chip_name(ts), ret);
		return;
	}

	if (do_fw_upgarde) {
		t_dev_info(dev, "Touch F/W upgrade triggered(%Xh)\n",
			do_fw_upgarde);
		siw_touch_qd_upgrade_work_now(ts);
		return;
	}

#if defined(__SIW_SUPPORT_ASC)
	if (ts->asc.use_asc == ASC_ON) {
		if (atomic_read(&ts->state.core) == CORE_UPGRADE) {
			mutex_lock(&ts->lock);
			ret = siw_ops_asc(ts, ASC_GET_FW_SENSITIVITY, 0);
			mutex_unlock(&ts->lock);
			if (ret < 0) {
				t_dev_warn(dev,
					"sensitivity change failed, %d\n", ret);
			}
		}

		siw_touch_qd_toggle_delta_work_jiffies(ts, 0);
	}
#endif	/* __SIW_SUPPORT_ASC */

	atomic_set(&ts->state.core, CORE_NORMAL);

	t_dev_dbg_base(dev, "init work done\n");
}

static int siw_touch_upgrade_work(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	int core_state = atomic_read(&ts->state.core);
	int irq_state = atomic_read(&ts->state.irq_enable);
	int ret = 0;

	t_dev_info(dev, "FW upgrade work func\n");

	atomic_set(&ts->state.core, CORE_UPGRADE);
	ts->role.use_fw_upgrade = 0;

	mutex_lock(&ts->lock);
	siw_touch_irq_control(dev, INTERRUPT_DISABLE);

	ret = siw_ops_upgrade(ts);
	mutex_unlock(&ts->lock);

	/* init force_upgrade */
	ts->force_fwup = FORCE_FWUP_CLEAR;
	ts->test_fwpath[0] = '\0';

	/* upgrade not granted */
	if (ret == EACCES) {
		atomic_set(&ts->state.core, core_state);
		if (irq_state)
			siw_touch_irq_control(dev, INTERRUPT_ENABLE);

		return 0;	/* skip reset */
	}

	if (ret < 0) {
		if (ret == -EPERM)
			t_dev_err(dev, "FW upgrade skipped\n");
		else
			t_dev_err(dev, "FW upgrade halted, %d\n", ret);
	}

	return 1;		/* do reset */
}

static void siw_touch_upgrade_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
						struct siw_ts, upgrade_work);

	if (siw_touch_upgrade_work(ts))
		siw_ops_reset(ts, HW_RESET_ASYNC);
}

static void siw_touch_fb_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
				struct siw_ts, fb_work);

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		siw_touch_suspend(ts->dev);
		siwmon_submit_ops_step_core(ts->dev, "FB suspend", 0);
	} else if (atomic_read(&ts->state.fb) == FB_RESUME) {
		siw_touch_resume(ts->dev);
		siwmon_submit_ops_step_core(ts->dev, "FB Resume", 0);
	}
}

#if defined(__SIW_CONFIG_USE_SYS_PANEL_RESET)
static void siw_touch_sys_reset_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
				struct siw_ts, sys_reset_work);
	struct device *dev = ts->dev;
	struct siw_touch_chip *chip = to_touch_chip(dev);
	u32 lcd_mode = (u32)chip->lcd_mode;
	int ret = 0;

	t_dev_info(dev, "sys reset start\n");

	siw_touch_notify(ts, NOTIFY_TOUCH_RESET, NULL);

	/*
	 * direct call for LCD reset
	 */
	ret = siw_touch_sys_panel_reset(dev);
	if (ret < 0)
		t_dev_err(dev, "failed to reset panel\n");

	siw_touch_notify(ts, LCD_EVENT_LCD_MODE, (void *)&lcd_mode);
}

static int siw_touch_reset(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	siw_touch_irq_control(dev, INTERRUPT_DISABLE);

	siw_touch_qd_sys_reset_work_now(ts);

	return 0;
}
#else	/* __SIW_CONFIG_USE_SYS_PANEL_RESET */
static void siw_touch_sys_reset_work_func(struct work_struct *work)
{

}

static int siw_touch_reset(struct siw_ts *ts)
{
	return siw_ops_reset(ts, HW_RESET_ASYNC);
}
#endif	/* __SIW_CONFIG_USE_SYS_PANEL_RESET */


#if defined(__SIW_SUPPORT_ASC)
static void siw_touch_toggle_delta_check_work_func(
			struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
				struct siw_ts, toggle_delta_work);
	struct device *dev = ts->dev;
	struct asc_info *asc = &(ts->asc);
	int connect_status = atomic_read(&ts->state.connect);
	int wireless_status = atomic_read(&ts->state.wireless);
	int call_status = atomic_read(&ts->state.incoming_call);
	int onhand_status = atomic_read(&ts->state.onhand);
	int delta = DELTA_CHK_OFF;
	int target = NORMAL_SENSITIVITY;

	if (asc->use_asc == ASC_OFF) {
		t_dev_info(dev, "ASC is off\n");
		return;
	}

	t_dev_info(dev, "connect = %d, wireless = %d, call = %d, onhand = %d\n",
			connect_status, wireless_status,
			call_status, onhand_status);

	if ((connect_status != CONNECT_INVALID) ||
		(wireless_status != 0) ||
		(call_status != INCOMING_CALL_IDLE)) {
		/* */
	} else {
		delta = DELTA_CHK_ON;

		switch (onhand_status) {
		case IN_HAND_ATTN:
		case IN_HAND_NO_ATTN:
			break;
		case NOT_IN_HAND:
			target = ACUTE_SENSITIVITY;
			break;
		default:
			delta = -1;
			break;
		}
	}

	if (delta < 0) {
		t_dev_info(dev, "Unknown onhand_status\n");
		return;
	}

	asc->use_delta_chk = delta;
	siw_touch_change_sensitivity(ts, target);

	t_dev_info(dev, "curr_sensitivity = %s, use_delta_chk = %d\n",
			asc_str[asc->curr_sensitivity], asc->use_delta_chk);

	siwmon_submit_ops_step_core(dev, "Delta work done", 0);
}

static void siw_touch_finger_input_check_work_func(
		struct work_struct *work)
{
	struct siw_ts *ts =
		container_of(to_delayed_work(work),
				struct siw_ts, finger_input_work);

	if ((ts->tcount == 1) && (!ts->asc.delta_updated))
		siw_touch_get_max_delta(ts);

	if ((ts->tcount == 0) && (ts->asc.delta_updated))
		siw_touch_update_sensitivity(ts);
}
#endif	/* __SIW_SUPPORT_ASC */

static int siw_touch_mon_chk_pause(struct siw_ts *ts)
{
	struct siw_ts_thread *ts_thread = &ts->mon_thread;
	int curr_state;

	mutex_lock(&ts_thread->lock);
	curr_state = (atomic_read(&ts->state.mon_ignore)) ?
				TS_THREAD_PAUSE : TS_THREAD_ON;
	atomic_set(&ts_thread->state, curr_state);
	mutex_unlock(&ts_thread->lock);

	return (curr_state == TS_THREAD_PAUSE);
}

static int siw_touch_mon_can_handler(struct siw_ts *ts)
{
	if (touch_get_dev_data(ts) == NULL)
		return 0;

	if (atomic_read(&ts->state.core) != CORE_NORMAL)
		return 0;

	return 1;
}

static int siw_touch_mon_thread(void *d)
{
	struct siw_ts *ts = d;
	struct siw_ts_thread *ts_thread = &ts->mon_thread;
	struct device *dev = ts->dev;
	siw_mon_handler_t handler;
	unsigned long timeout;
	int ret = 0;

	handler = ts_thread->handler;
	timeout = ts_thread->interval * HZ;

	atomic_set(&ts_thread->state, TS_THREAD_ON);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout_interruptible(timeout);

		if (kthread_should_stop()) {
			t_dev_info(dev, "stopping mon thread[%s]\n",
					ts_thread->thread->comm);
			set_current_state(TASK_RUNNING);
			break;
		}

		set_current_state(TASK_RUNNING);

		if (siw_touch_mon_chk_pause(ts))
			continue;

		if (siw_touch_mon_can_handler(ts))
			ret = handler(dev, 0);

		siw_touch_mon_chk_pause(ts);
	}

	atomic_set(&ts_thread->state, TS_THREAD_OFF);

	return 0;
}

static int siw_touch_mon_hold_set(struct device *dev, int pause)
{
	struct siw_ts *ts = to_touch_core(dev);
	struct siw_ts_thread *ts_thread = &ts->mon_thread;
	char *name = (pause) ? "pause" : "resume";
	int prev_state = (pause) ? TS_THREAD_ON : TS_THREAD_PAUSE;

	if (ts_thread->thread == NULL)
		return -EINVAL;

	if (atomic_read(&ts_thread->state) != prev_state)
		return -EINVAL;

	t_dev_info(dev, "mon thread %s\n", name);

	atomic_set(&ts->state.mon_ignore, !!pause);

	return 0;
}

static void siw_touch_mon_hold(struct device *dev, int pause)
{
	struct siw_ts *ts = to_touch_core(dev);
	struct siw_ts_thread *ts_thread = &ts->mon_thread;
	char *name = (pause) ? "pause" : "resume";
	int new_state = (pause) ? TS_THREAD_PAUSE : TS_THREAD_ON;
	int ret = 0;

	if (!(touch_flags(ts) & TOUCH_USE_MON_THREAD))
		return;

	mutex_lock(&ts_thread->lock);
	ret = siw_touch_mon_hold_set(dev, pause);
	mutex_unlock(&ts_thread->lock);
	if (ret < 0)
		return;

	while (1) {
		wake_up_process(ts_thread->thread);

		touch_msleep(1);

		if (atomic_read(&ts_thread->state) == new_state)
			break;

		t_dev_info(dev,
			"waiting for mon thread %s\n", name);
	}
}

void siw_touch_mon_pause(struct device *dev)
{
	siw_touch_mon_hold(dev, 1);
}

void siw_touch_mon_resume(struct device *dev)
{
	siw_touch_mon_hold(dev, 0);
}

#define siw_touch_kthread_run(__dev, __threadfn, __data, __name) \
({	\
	struct task_struct *__k;	\
	__k = kthread_run(__threadfn, __data, "%s", __name);	\
	if (IS_ERR(__k))	\
		t_dev_err(__dev, "kthread_run failed : %s\n", __name);	\
	else	\
		t_dev_dbg_base(__dev, "kthread[%s] is running\n", __k->comm);\
	__k;	\
})

static int __used siw_touch_init_thread(struct siw_ts *ts)
{
	struct siw_touch_fquirks *fquirks = touch_fquirks(ts);
	struct device *dev = ts->dev;
	struct siw_ts_thread *ts_thread = NULL;
	struct task_struct *thread;
	siw_mon_handler_t handler;
	char *thread_name;
	int interval;
	int ret = 0;

	if (t_dbg_flag & DBG_FLAG_SKIP_MON_THREAD)
		ts->flags &= ~TOUCH_USE_MON_THREAD;

	if (t_dbg_flag & DBG_FLAG_TEST_MON_THREAD)
		ts->flags |= TOUCH_USE_MON_THREAD;

	if (!(touch_flags(ts) & TOUCH_USE_MON_THREAD))
		goto out;

	ts_thread = &ts->mon_thread;

	if (ts_thread->thread != NULL)
		goto out;

	mutex_init(&ts_thread->lock);

	if (fquirks->mon_handler) {
		handler = fquirks->mon_handler;
		interval = fquirks->mon_interval;
	} else {
		handler = ts->ops->mon_handler;
		interval = ts->ops->mon_interval;
	}

	if (!handler) {
		t_dev_warn(dev, "No mon_handler defined!\n");
		goto out;
	}

	if (!interval) {
		t_dev_warn(dev, "mon_interval is zero!\n");
		goto out;
	}

	atomic_set(&ts_thread->state, TS_THREAD_OFF);
	ts_thread->interval = interval;
	ts_thread->handler = handler;

	thread_name = touch_drv_name(ts);
	thread_name = (thread_name) ? thread_name : SIW_TOUCH_NAME;
	thread = siw_touch_kthread_run(dev,
					siw_touch_mon_thread, ts,
					thread_name);
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		goto out;
	}
	ts_thread->thread = thread;
	t_dev_info(dev, "mon thread[%s, %d] begins\n",
			thread->comm, ts->ops->mon_interval);

out:
	return ret;
}

static void __used siw_touch_free_thread(struct siw_ts *ts)
{
	struct siw_ts_thread *ts_thread = NULL;

	if (!(touch_flags(ts) & TOUCH_USE_MON_THREAD))
		return;

	ts_thread = &ts->mon_thread;

	if (ts_thread->thread == NULL)
		return;

	kthread_stop(ts_thread->thread);

	ts_thread->thread = NULL;

	mutex_destroy(&ts_thread->lock);
}

static int __used siw_touch_init_works(struct siw_ts *ts)
{
	ts->wq = create_singlethread_workqueue("touch_wq");
	if (!ts->wq) {
		t_dev_err(ts->dev, "failed to create workqueue\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&ts->init_work, siw_touch_init_work_func);
	INIT_DELAYED_WORK(&ts->upgrade_work, siw_touch_upgrade_work_func);
	INIT_DELAYED_WORK(&ts->fb_work, siw_touch_fb_work_func);
#if defined(__SIW_SUPPORT_ASC)
	INIT_DELAYED_WORK(&ts->toggle_delta_work,
					siw_touch_toggle_delta_check_work_func);
	INIT_DELAYED_WORK(&ts->finger_input_work,
					siw_touch_finger_input_check_work_func);
#endif	/* __SIW_SUPPORT_ASC */
	INIT_DELAYED_WORK(&ts->notify_work, siw_touch_atomic_notifer_work_func);
	INIT_DELAYED_WORK(&ts->sys_reset_work, siw_touch_sys_reset_work_func);

	return 0;
}

static void __used siw_touch_free_works(struct siw_ts *ts)
{
	if (ts->wq) {
		cancel_delayed_work(&ts->sys_reset_work);
		cancel_delayed_work(&ts->notify_work);
	#if defined(__SIW_SUPPORT_ASC)
		cancel_delayed_work(&ts->finger_input_work);
		cancel_delayed_work(&ts->toggle_delta_work);
	#endif	/* __SIW_SUPPORT_ASC */
		cancel_delayed_work(&ts->fb_work);
		cancel_delayed_work(&ts->upgrade_work);
		cancel_delayed_work(&ts->init_work);

		destroy_workqueue(ts->wq);
		ts->wq = NULL;
	}
}

static irqreturn_t __used siw_touch_irq_handler(int irq, void *dev_id)
{
	struct siw_ts *ts = (struct siw_ts *)dev_id;
	struct device *dev = ts->dev;

	t_dev_dbg_irq(dev, "irq_handler\n");

	if (atomic_read(&ts->state.pm) >= DEV_PM_SUSPEND) {
		t_dev_info(dev, "interrupt in suspend[%d]\n",
				atomic_read(&ts->state.pm));
		atomic_set(&ts->state.pm, DEV_PM_SUSPEND_IRQ);
	#if defined(__SIW_SUPPORT_WAKE_LOCK)
		wake_lock_timeout(&ts->lpwg_wake_lock, msecs_to_jiffies(1000));
	#endif
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static int _siw_touch_do_irq_thread(struct siw_ts *ts)
{
	int ret = 0;

	ts->intr_status = 0;

	if (atomic_read(&ts->state.core) != CORE_NORMAL)
		goto out;

	ret = siw_ops_irq_handler(ts);
	if (ret < 0) {
		t_dev_dbg_irq(ts->dev, "Err in irq_handler of %s, %d",
				touch_chip_name(ts), ret);
		if (ret == -ERESTART) {
			if (t_dbg_flag & DBG_FLAG_SKIP_IRQ_RESET)
				return ret;

			if (atomic_read(&ts->state.pm) == DEV_PM_RESUME) {
			#if defined(__SIW_SUPPORT_WAKE_LOCK)
				wake_lock_timeout(&ts->lpwg_wake_lock,
					msecs_to_jiffies(1000));
			#endif
			}

			siw_touch_reset(ts);
		}
		return ret;
	}

	if (ts->intr_status & TOUCH_IRQ_FINGER) {
		siw_touch_report_event(ts);

	#if defined(__SIW_SUPPORT_ASC)
		if (ts->asc.use_delta_chk == DELTA_CHK_ON)
			siw_touch_qd_finger_input_work_jiffies(ts, 0);
	#endif
	}

	if (ts->intr_status & TOUCH_IRQ_KNOCK)
		siw_touch_send_uevent(ts, TOUCH_UEVENT_KNOCK);

	if (ts->intr_status & TOUCH_IRQ_PASSWD)
		siw_touch_send_uevent(ts, TOUCH_UEVENT_PASSWD);

	if (ts->intr_status & TOUCH_IRQ_SWIPE_RIGHT)
		siw_touch_send_uevent(ts, TOUCH_UEVENT_SWIPE_RIGHT);

	if (ts->intr_status & TOUCH_IRQ_SWIPE_LEFT)
		siw_touch_send_uevent(ts, TOUCH_UEVENT_SWIPE_LEFT);

	if (ts->intr_status & TOUCH_IRQ_GESTURE)
		siw_touch_send_uevent(ts, ts->intr_gesture);

out:
	return ret;
}

static irqreturn_t __used siw_touch_irq_thread(int irq, void *dev_id)
{
	struct siw_ts *ts = (struct siw_ts *)dev_id;
	struct device *dev = ts->dev;
	int ret = 0;

	t_dev_dbg_irq(dev, "irq_thread\n");

	if (t_dbg_flag & DBG_FLAG_SKIP_IRQ)
		goto out;

	if (ts->ops->irq_dbg_handler) {
		ret = ts->ops->irq_dbg_handler(dev);
		goto out;
	}

	mutex_lock(&ts->lock);
	ret = _siw_touch_do_irq_thread(ts);
	mutex_unlock(&ts->lock);

out:
	return IRQ_HANDLED;
}

static int __used siw_touch_verify_pdata(struct siw_ts *ts)
{
	struct siw_touch_operations *ops = ts->ops;

	if (0 ||
		!ops->probe ||
		!ops->remove ||
		!ops->suspend ||
		!ops->resume ||
		!ops->init ||
		!ops->reset ||
		!ops->ic_info ||
		!ops->tc_driving ||
		!ops->chk_status ||
		!ops->irq_handler ||
		!ops->irq_abs ||
		!ops->irq_lpwg ||
		!ops->power ||
		!ops->upgrade ||
		!ops->asc ||
		0)
		return -EPERM;

	return 0;
}

static struct siw_touch_pdata *siw_touch_probe_common(struct siw_ts *ts)
{
	struct siw_touch_pdata *pdata = NULL;
	struct device *dev = ts->dev;
	int ret = 0;

	pdata = (struct siw_touch_pdata *)ts->pdata;
	if (!pdata) {
		t_dev_err(dev, "NULL core pdata\n");
		goto out;
	}

	t_dev_info(dev, "%s quirks = 0x%08X\n",
			touch_chip_name(ts), (u32)touch_get_quirks(ts));

	atomic_set(&ts->state.core, CORE_EARLY_PROBE);

	if (!ts->ops) {
		t_dev_warn(dev, "%s ops is NULL : default ops selected\n",
				touch_chip_name(ts));
		siw_setup_operations(ts, siw_hal_get_default_ops(0));
	}

	ret = siw_ops_early_probe(ts);
	if (ret) {
		t_dev_err(dev, "failed to early_probe, %d\n", ret);
		goto out;
	}

	atomic_set(&ts->state.core, CORE_PROBE);

	ret = siw_touch_verify_pdata(ts);
	if (ret) {
		t_dev_err(dev, "failed to check functions, %d\n", ret);
		goto out;
	}

	ret = siw_touch_parse_data(ts);
	if (ret < 0) {
		t_dev_err(dev, "failed to parse touch data, %d\n", ret);
		goto out;
	}

	ret = siw_touch_bus_alloc_buffer(ts);
	if (ret < 0) {
		t_dev_err(dev, "failed to alloc bus buffer, %d\n", ret);
		goto out;
	}

	ret = siw_touch_bus_pin_get(ts);
	if (ret < 0) {
		t_dev_err(dev, "failed to setup bus pin, %d\n", ret);
		goto out_bus_pin;
	}

	ret = siw_touch_bus_init(ts->dev);
	if (ret < 0) {
		t_dev_err(dev, "failed to setup bus, %d\n", ret);
		goto out_bus_init;
	}

	siw_touch_init_locks(ts);

	ret = siw_touch_init_works(ts);
	if (ret) {
		t_dev_err(dev, "failed to initialize works, %d\n", ret);
		goto out_init_works;
	}

	ret = siw_touch_init_input(ts);
	if (ret) {
		t_dev_err(dev, "failed to register input device, %d\n", ret);
		goto out_init_input;
	}

	ret = siw_touch_init_uevent(ts);
	if (ret) {
		t_dev_err(dev, "failed to initialize uevent, %d\n", ret);
		goto out_init_uevent;
	}

	ret = siw_touch_init_sysfs(ts);
	if (ret) {
		t_dev_err(dev, "failed to initialize sysfs, %d\n", ret);
		goto out_init_sysfs;
	}

	ret = siw_touch_init_notify(ts);
	if (ret) {
		t_dev_err(dev, "failed to initialize notifier, %d\n", ret);
		goto out_init_notify;
	}

	return pdata;

out_init_notify:
	siw_touch_free_sysfs(ts);

out_init_sysfs:
	siw_touch_free_uevent(ts);

out_init_uevent:
	siw_touch_free_input(ts);

out_init_input:
	siw_touch_free_works(ts);

out_init_works:
	siw_touch_free_locks(ts);

out_bus_init:
	siw_touch_bus_pin_put(ts);

out_bus_pin:
	siw_touch_bus_free_buffer(ts);

out:
	return NULL;
}

static void siw_touch_remove_common(struct siw_ts *ts)
{
	siw_touch_free_notify(ts);

	siw_touch_free_sysfs(ts);

	siw_touch_free_uevent(ts);

	siw_touch_free_input(ts);

	siw_touch_free_works(ts);

	siw_touch_free_locks(ts);

	siw_touch_bus_pin_put(ts);

	siw_touch_bus_free_buffer(ts);
}

static int __siw_touch_probe_init(void *data)
{
	struct siw_ts *ts = data;
	struct device *dev = ts->dev;
	const char *irq_name = NULL;
	int ret = 0;

	ret = siw_ops_probe(ts);
	if (ret) {
		t_dev_err(dev, "failed to probe, %d\n", ret);
		goto out;
	}

	ret = siw_touch_add_sysfs(ts);
	if (ret) {
		t_dev_err(dev, "failed to add sysfs, %d\n", ret);
		goto out_add_sysfs;
	}

	ret = siw_touch_init_pm(ts);
	if (ret) {
		t_dev_err(dev, "failed to init pm\n");
		goto out_init_pm;
	}

	if (ts->is_charger)
		goto skip_charger;

#if defined(__SIW_TEST_IRQ_OFF)
	ts->irq = 0;
#else	/* __SIW_TEST_IRQ_OFF */
	irq_name = (const char *)touch_drv_name(ts);
	irq_name = (irq_name) ? irq_name : SIW_TOUCH_NAME;
	ret = siw_touch_request_irq(ts,
						siw_touch_irq_handler,
						siw_touch_irq_thread,
						touch_irqflags(ts),
						irq_name);
	if (ret) {
		t_dev_err(dev, "failed to request thread irq(%d), %d\n",
					ts->irq, ret);
		goto out_request_irq;
	}
#endif	/* __SIW_TEST_IRQ_OFF */

	siw_touch_disable_irq(dev, ts->irq);

	ret = siw_touch_init_thread(ts);
	if (ret) {
		t_dev_err(dev, "failed to create thread\n");
		goto out_init_thread;
	}

	siw_touch_qd_init_work_now(ts);

skip_charger:
	ts->init_late_done = 1;

	return 0;

out_init_thread:
	siw_touch_free_irq(ts);

out_request_irq:
	siw_touch_free_pm(ts);

out_init_pm:
	siw_touch_del_sysfs(ts);

out_add_sysfs:
	siw_ops_remove(ts);

out:
	return ret;
}

static void __siw_touch_probe_free(void *data)
{
	struct siw_ts *ts = data;

	if (!ts->init_late_done)
		return;

	if (ts->is_charger)
		goto skip_charger;

	cancel_delayed_work_sync(&ts->upgrade_work);
	cancel_delayed_work_sync(&ts->init_work);

	siw_touch_free_thread(ts);

	siw_touch_free_irq(ts);

skip_charger:
	siw_touch_free_pm(ts);

	siw_touch_del_sysfs(ts);

	siw_ops_remove(ts);
}

static int siw_touch_probe_init(void *data)
{
	struct siw_ts *ts = data;
	int ret = 0;

	mutex_lock(&ts->probe_lock);
	ret = __siw_touch_probe_init(data);
	mutex_unlock(&ts->probe_lock);

	return ret;
}

static void siw_touch_probe_free(void *data)
{
	struct siw_ts *ts = data;

	mutex_lock(&ts->probe_lock);
	__siw_touch_probe_free(ts);
	mutex_unlock(&ts->probe_lock);
}

enum {
	INIT_LATE_SIG_WQ	= 0x5A5A,
	INIT_LATE_SIG_DONE	= 0xAA55,
};

static void siw_touch_init_late_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
			container_of(to_delayed_work(work),
						struct siw_ts, init_late_work);
	int ret = 0;

	mutex_lock(&ts->lock);

	if (ts->init_late == NULL)
		goto out;

	ret = siw_touch_init_late(ts, INIT_LATE_SIG_WQ);
	t_dev_info(ts->dev, "init_late_work done, %d\n", ret);

out:
	mutex_unlock(&ts->lock);
}

static int siw_touch_init_late_work_trigger(struct siw_ts *ts)
{
	int init_late_time = 0;

	if (t_dbg_flag & DBG_FLAG_SKIP_INIT_LATE_WORK)
		ts->init_late_time = 0;

	if (t_dbg_flag & DBG_FLAG_TEST_INIT_LATE_WORK)
		ts->init_late_time = 5000;

	init_late_time = ts->init_late_time;

	if (!init_late_time)
		return 0;

	INIT_DELAYED_WORK(&ts->init_late_work, siw_touch_init_late_work_func);

	queue_delayed_work(ts->wq,
		&ts->init_late_work,
		msecs_to_jiffies(init_late_time));

	t_dev_info(ts->dev,
		"init_late_work triggered, %d msec\n",
		init_late_time);

	return 0;
}

static int siw_touch_probe_post(struct siw_ts *ts)
{
	ts->init_late_time = ts->pdata->init_late_time;

	if (t_dbg_flag & DBG_FLAG_SKIP_INIT_LATE)
		ts->flags &= ~TOUCH_USE_PROBE_INIT_LATE;
	if (t_dbg_flag & DBG_FLAG_TEST_INIT_LATE)
		ts->flags |= TOUCH_USE_PROBE_INIT_LATE;

	if (touch_flags(ts) & TOUCH_USE_PROBE_INIT_LATE) {
		/*
		 * Postpone actual init control
		 * This is related to LCD_EVENT_TOUCH_INIT_LATE and
		 * shall be controlled by MIPI via notifier
		 */
		ts->init_late = siw_touch_probe_init;

		siw_touch_init_late_work_trigger(ts);
		return 0;
	}

	ts->init_late = NULL;

	touch_msleep(200);

	return siw_touch_probe_init(ts);
}

static void siw_touch_remove_post(struct siw_ts *ts)
{
	if (ts->init_late) {
		if (ts->init_late_time) {
			t_dev_info(ts->dev, "init_late_work canceled\n");
			cancel_delayed_work_sync(&ts->init_late_work);
		}
	}

	siw_touch_probe_free(ts);
}

int siw_touch_probe(struct siw_ts *ts)
{
	struct siw_touch_pdata *pdata = NULL;
	struct device *dev = ts->dev;
	int ret = 0;

	t_dev_info(dev, "SiW Touch Probe\n");

	ts->is_charger = (siw_touch_get_boot_mode() == SIW_TOUCH_CHARGER_MODE);

	if (ts->is_charger)
		t_dev_info(dev, "Probe - Charger mode\n");

	/* set defalut lpwg value because of AAT */
	ts->role.mfts_lpwg = t_mfts_lpwg;

	ts->lpwg.mode = t_lpwg_mode;
	ts->lpwg.screen = t_lpwg_screen;
	ts->lpwg.sensor = t_lpwg_sensor;
	ts->lpwg.qcover = t_lpwg_qcover;

	pdata = siw_touch_probe_common(ts);
	if (!pdata)
		return -EINVAL;

	ret = siw_touch_probe_post(ts);
	if (ret)
		goto out_probe_post;

	siw_touch_blocking_notifier_call(
		LCD_EVENT_TOUCH_DRIVER_REGISTERED,
		NULL);

	t_dev_info(dev, "probe(%s) done\n",
		(ts->is_charger) ? "charger" : "normal");

	return 0;

out_probe_post:
	siw_touch_remove_common(ts);

	return ret;
}

int siw_touch_remove(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	siw_touch_blocking_notifier_call(
		LCD_EVENT_TOUCH_DRIVER_UNREGISTERED,
		NULL);

	if (ts->is_charger)
		t_dev_info(dev, "Remove - Charger mode\n");

	siw_touch_remove_post(ts);

	siw_touch_remove_common(ts);

	t_dev_info(dev, "SiW Touch Removed\n");

	return 0;
}

int siw_touch_init_late(struct siw_ts *ts, int value)
{
	struct device *dev = ts->dev;
	int ret = 0;

	if (!value)
		goto out;

	if (!(touch_flags(ts) & TOUCH_USE_PROBE_INIT_LATE)) {
		t_dev_info(dev, "init_late not enabled\n");
		goto out;
	}

	if (ts->init_late == NULL)
		goto out;

	t_dev_info(dev, "trigger init_late(%Xh)\n", value);

	ret = ts->init_late(ts);
	if (ret < 0) {
		atomic_set(&ts->state.core, CORE_PROBE);
		t_dev_err(dev, "init_late failed, %d\n", ret);
		goto out;
	}

	t_dev_info(dev, "init_late done(%Xh)\n", value);

	ts->init_late = NULL;

	value = INIT_LATE_SIG_DONE;
	siw_touch_atomic_notifier_call(
		LCD_EVENT_TOUCH_INIT_LATE,
		&value);

out:
	return ret;
}


#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_MON_MODULE)

struct siw_mon_operations *siw_mon_ops;
EXPORT_SYMBOL(siw_mon_ops);

int siw_mon_register(struct siw_mon_operations *ops)
{
	if (siw_mon_ops)
		return -EBUSY;

	siw_mon_ops = ops;
	t_pr_dbg(DBG_BASE, "siw mon ops assigned\n");
	mb();
	return 0;
}
EXPORT_SYMBOL_GPL(siw_mon_register);

void siw_mon_deregister(void)
{
	if (siw_mon_ops == NULL) {
		t_pr_err("monitor was not registered\n");
		return;
	}
	siw_mon_ops = NULL;
	t_pr_dbg(DBG_BASE, "siw mon ops released\n");
	mb();
}
EXPORT_SYMBOL_GPL(siw_mon_deregister);

#endif	/* CONFIG_TOUCHSCREEN_SIW_MMI_MON */

__siw_setup_u32("siw_pr_dbg_mask=", siw_setup_pr_dbg_mask, t_pr_dbg_mask);
__siw_setup_u32("siw_dev_dbg_mask=", siw_setup_dev_dbg_mask, t_dev_dbg_mask);

__siw_setup_u32("siw_mfts_lpwg=", siw_setup_mfts_lpwg, t_mfts_lpwg);
__siw_setup_u32("siw_lpwg_mode=", siw_setup_lpwg_mode, t_lpwg_mode);
__siw_setup_u32("siw_lpwg_screen=", siw_setup_lpwg_screen, t_lpwg_screen);
__siw_setup_u32("siw_lpwg_sensor=", siw_setup_lpwg_sensor, t_lpwg_sensor);


