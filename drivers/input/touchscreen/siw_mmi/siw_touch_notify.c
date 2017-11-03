/*
 * siw_touch_notify.c - SiW touch notify driver
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
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include <linux/notifier.h>
#include <linux/export.h>
#include <linux/input/siw_touch_notify.h>

#include "siw_touch.h"
#include "siw_touch_bus.h"
#include "siw_touch_irq.h"
#include "siw_touch_sys.h"


static BLOCKING_NOTIFIER_HEAD(siw_touch_blocking_notifier);
static ATOMIC_NOTIFIER_HEAD(siw_touch_atomic_notifier);

int siw_touch_blocking_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&siw_touch_blocking_notifier,
		nb);
}
EXPORT_SYMBOL(siw_touch_blocking_notifier_register);

int siw_touch_blocking_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&siw_touch_blocking_notifier,
		nb);
}
EXPORT_SYMBOL(siw_touch_blocking_notifier_unregister);

int siw_touch_blocking_notifier_call(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&siw_touch_blocking_notifier, val,
		v);
}
EXPORT_SYMBOL_GPL(siw_touch_blocking_notifier_call);

int siw_touch_atomic_notifier_register(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&siw_touch_atomic_notifier, nb);
}
EXPORT_SYMBOL(siw_touch_atomic_notifier_register);

int siw_touch_atomic_notifier_unregister(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&siw_touch_atomic_notifier, nb);
}
EXPORT_SYMBOL(siw_touch_atomic_notifier_unregister);

int siw_touch_atomic_notifier_call(unsigned long val, void *v)
{
	return atomic_notifier_call_chain(&siw_touch_atomic_notifier, val, v);
}
EXPORT_SYMBOL_GPL(siw_touch_atomic_notifier_call);

int siw_touch_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&siw_touch_blocking_notifier,
		nb);
}
EXPORT_SYMBOL(siw_touch_register_client);

int siw_touch_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&siw_touch_blocking_notifier,
		nb);
}
EXPORT_SYMBOL(siw_touch_unregister_client);

int siw_touch_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&siw_touch_blocking_notifier, val,
		v);
}
EXPORT_SYMBOL_GPL(siw_touch_notifier_call_chain);

void siw_touch_notify_connect(u32 type)
{
	siw_touch_atomic_notifier_call(NOTIFY_CONNECTION, &type);
}
EXPORT_SYMBOL(siw_touch_notify_connect);

void siw_touch_notify_wireless(u32 type)
{
	siw_touch_atomic_notifier_call(NOTIFY_WIRELEES, &type);
}
EXPORT_SYMBOL(siw_touch_notify_wireless);

void siw_touch_notify_earjack(u32 type)
{
	siw_touch_atomic_notifier_call(NOTIFY_EARJACK, &type);
}
EXPORT_SYMBOL(siw_touch_notify_earjack);

static int _siw_touch_do_notify(struct siw_ts *ts,
				unsigned long event, void *data)
{
	struct device *dev = ts->dev;
	char *noti_str = "(unknown)";
	int call_hal_notify = 1;
	u32 value = 0;
	int ret = 0;

	if (data)
		value = *((int *)data);

	switch (event) {
	case LCD_EVENT_TOUCH_INIT_LATE:
		ret = siw_touch_init_late(ts, value);

		call_hal_notify = 0;
		noti_str = "INIT_LATE";
		goto out;
	}

	if (touch_get_dev_data(ts) == NULL)
		return 0;

	if (siw_ops_is_null(ts, notify))
		return 0;

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		ret = siw_ops_notify(ts, event, data);
		if (ret != NOTIFY_STOP)
			siw_touch_irq_control(dev, INTERRUPT_DISABLE);
		break;

	case NOTIFY_CONNECTION:
		if (atomic_read(&ts->state.connect) != value) {
			atomic_set(&ts->state.connect, value);
			ret = siw_ops_notify(ts, event, data);

#if defined(__SIW_SUPPORT_ASC)
			if (ts->asc.use_asc == ASC_ON)
				siw_touch_qd_toggle_delta_work_jiffies(ts, 0);
#endif	/* __SIW_SUPPORT_ASC */
		}
		break;

	case NOTIFY_WIRELEES:
		atomic_set(&ts->state.wireless, value);
		ret = siw_ops_notify(ts, event, data);

#if defined(__SIW_SUPPORT_ASC)
		if (ts->asc.use_asc == ASC_ON)
			siw_touch_qd_toggle_delta_work_jiffies(ts, 0);
#endif	/* __SIW_SUPPORT_ASC */

		break;

	case NOTIFY_FB:
		atomic_set(&ts->state.fb, value);
		siw_touch_qd_fb_work_now(ts);

		call_hal_notify = 0;
		noti_str = "FB";
		break;

	case NOTIFY_EARJACK:
		atomic_set(&ts->state.earjack, value);
		ret = siw_ops_notify(ts, event, data);
		break;

	default:
		ret = siw_ops_notify(ts, event, data);
		break;
	}

out:
	if (!call_hal_notify) {
		t_dev_info(dev, "notify: event %s(%Xh), value %Xh\n",
			   noti_str, (int)event, value);
		siwmon_submit_evt(dev, "NOTIFY", 0, noti_str, event, value,
			ret);
	}

	return ret;
}

int siw_touch_notify(struct siw_ts *ts, unsigned long event, void *data)
{
	int noti_allowed = 0;
	int core_state = atomic_read(&ts->state.core);
	int ret = 0;

	switch (event) {
	case LCD_EVENT_TOUCH_INIT_LATE:
		noti_allowed = !!(core_state == CORE_PROBE);
		break;
	default:
		if (ts->is_charger)
			return 0;

		noti_allowed = !!(core_state == CORE_NORMAL);
		break;
	}

	if (!noti_allowed) {
		t_dev_dbg_base(ts->dev,
			       "notify: event(%Xh) is skipped: core %d\n",
			       (int)event, core_state);
		return 0;
	}

	mutex_lock(&ts->lock);
	ret = _siw_touch_do_notify(ts, event, data);
	mutex_unlock(&ts->lock);

	return ret;
}

static int siw_touch_blocking_notifier_callback
	(struct notifier_block *this, unsigned long event, void *data)
{
	struct siw_ts *ts =
		container_of(this, struct siw_ts, blocking_notif);
	return siw_touch_notify(ts, event, data);
}

static int siw_touch_atomic_notifier_callback
	(struct notifier_block *this, unsigned long event, void *data)
{
	struct siw_ts *ts =
		container_of(this, struct siw_ts, atomic_notif);

	ts->notify_event = event;
	ts->notify_data = (data == NULL) ? 0 : *(int *)data;

	siw_touch_qd_notify_work_now(ts);

	return 0;
}

void siw_touch_atomic_notifer_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
		container_of(to_delayed_work(work),
			     struct siw_ts, notify_work);

	siw_touch_notify(ts, ts->notify_event, &ts->notify_data);
}

int siw_touch_init_notify(struct siw_ts *ts)
{
	struct device *dev = ts->dev;
	int ret = 0;

	ts->blocking_notif.notifier_call = siw_touch_blocking_notifier_callback;
	ret = siw_touch_blocking_notifier_register(&ts->blocking_notif);
	if (ret < 0) {
		t_dev_err(dev,
			"failed to regiseter touch blocking_notify callback\n");
		goto out;
	}

	ts->atomic_notif.notifier_call = siw_touch_atomic_notifier_callback;
	ret = siw_touch_atomic_notifier_register(&ts->atomic_notif);
	if (ret < 0) {
		t_dev_err(dev,
			"failed to regiseter touch atomic_notify callback\n");
		goto out;
	}

out:
	return ret;
}

void siw_touch_free_notify(struct siw_ts *ts)
{
	(void)siw_touch_atomic_notifier_unregister(&ts->atomic_notif);
	(void)siw_touch_blocking_notifier_unregister(&ts->blocking_notif);
}

