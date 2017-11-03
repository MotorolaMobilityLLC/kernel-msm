/*
 * siw_touch_irq.c - SiW touch interrupt driver
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
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/irq.h>
#include <asm/memory.h>

#include "siw_touch.h"
#include "siw_touch_irq.h"


static void siw_touch_irq_pending_onoff(struct device *dev,
					unsigned int irq, int onoff)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc)
		return;

	raw_spin_lock_irqsave(&desc->lock, flags);
	if (onoff) {
		desc->istate |= IRQS_PENDING;
		t_dev_info(dev, "set IRQS_PENDING(%d)\n", irq);
	} else {
		if (desc->istate & IRQS_PENDING)
			t_dev_info(dev, "clr IRQS_PENDING(%d)\n", irq);
		desc->istate &= ~IRQS_PENDING;
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}

static void siw_touch_set_irq_pending(struct device *dev, unsigned int irq)
{
	siw_touch_irq_pending_onoff(dev, irq, 1);
}

static void siw_touch_clr_irq_pending(struct device *dev, unsigned int irq)
{
	siw_touch_irq_pending_onoff(dev, irq, 0);
}


void siw_touch_enable_irq_wake(struct device *dev,
			       unsigned int irq)
{
	struct siw_ts *ts = to_touch_core(dev);

	if (!ts->role.use_lpwg)
		return;

	enable_irq_wake(irq);
	t_dev_info(dev, "irq(%d) wake enabled\n", irq);
}

void siw_touch_disable_irq_wake(struct device *dev,
				unsigned int irq)
{
	struct siw_ts *ts = to_touch_core(dev);

	if (!ts->role.use_lpwg)
		return;

	disable_irq_wake(irq);
	t_dev_info(dev, "irq(%d) wake disabled\n", irq);
}

void siw_touch_enable_irq(struct device *dev, unsigned int irq)
{
	siw_touch_clr_irq_pending(dev, irq);
	enable_irq(irq);
	t_dev_info(dev, "irq(%d) enabled\n", irq);
}

void siw_touch_disable_irq(struct device *dev, unsigned int irq)
{
	disable_irq_nosync(irq);	/* No waiting */
	t_dev_info(dev, "irq(%d) disabled\n", irq);
}

void siw_touch_resume_irq(struct device *dev)
{
	struct siw_ts *ts = to_touch_core(dev);
	int irq = ts->irq;

	t_dev_info(dev, "resume irq(%d)\n", irq);

	disable_irq(irq);
	siw_touch_set_irq_pending(dev, irq);
	enable_irq(irq);
}


void siw_touch_irq_control(struct device *dev, int on_off)
{
	struct siw_ts *ts = to_touch_core(dev);

	t_dev_dbg_irq(dev, "touch_irq_control(%d)\n", on_off);

	if (on_off == INTERRUPT_ENABLE) {
		if (atomic_cmpxchg(&ts->state.irq_enable, 0, 1)) {
			t_dev_warn(dev, "(warn) already irq enabled\n");
			return;
		}

		siw_touch_enable_irq(dev, ts->irq);

		siw_touch_enable_irq_wake(dev, ts->irq);

		return;
	}

	if (!atomic_cmpxchg(&ts->state.irq_enable, 1, 0)) {
		t_dev_warn(dev, "(warn) already irq disabled\n");
		return;
	}

	siw_touch_disable_irq_wake(dev, ts->irq);

	siw_touch_disable_irq(dev, ts->irq);
}

#if defined(__SIW_CONFIG_OF)
static void siw_touch_irq_work_func(struct work_struct *work)
{
	struct siw_ts *ts =
		container_of(work, struct siw_ts, work_irq.work);
	struct device *dev = ts->dev;

	t_dev_dbg_irq(dev, "irq_work_func\n");

	if (ts->thread_fn)
		ts->thread_fn(ts->irq, ts);
}

static irqreturn_t siw_touch_work_irq_handler(int irq, void *dev_id)
{
	struct siw_ts *ts = (struct siw_ts *)dev_id;
	struct device *dev = ts->dev;

	t_dev_dbg_irq(dev, "work_irq_handler\n");

	if (ts->handler_fn(ts->irq, ts) == IRQ_WAKE_THREAD)
		queue_delayed_work(ts->wq, &ts->work_irq, 0);

	return IRQ_HANDLED;
}
#endif

static int siw_touch_request_irq_queue_work(struct siw_ts *ts,
		irq_handler_t handler_fn,
		irq_handler_t thread_fn,
		unsigned long flags,
		const char *name)
{
#if defined(__SIW_CONFIG_OF)
	struct device *dev = ts->dev;
	struct device_node *node = NULL;
	u32 ints[2] = { 0, 0 };
	int ret = 0;

	ts->handler_fn = handler_fn;
	ts->thread_fn = thread_fn;
	INIT_DELAYED_WORK(&ts->work_irq, siw_touch_irq_work_func);

	node = of_find_compatible_node(NULL, NULL, "vendor,vendor-touch");
	if (!node) {
		t_dev_err(dev,
			"request_irq can not find touch eint device node!.\n");
		ret = -ENXIO;
		goto out;
	}

	ret = of_property_read_u32_array(node, "debounce",
		ints, ARRAY_SIZE(ints));
	if (ret < 0) {
		t_dev_err(dev, "failed to parse GPIO defaults: %d\n", ret);
		goto out;
	}
	gpio_set_debounce(ints[0], ints[1]);

	/*
	 * Default flags = 0x2002 : IRQF_ONESHOT | IRQF_TRIGGER_FALLING
	*/
	ts->irq = irq_of_parse_and_map(node, 0);
	ret = request_irq(ts->irq, siw_touch_work_irq_handler,
			  touch_irqflags(ts), name, (void *)ts);
	if (ret) {
		t_dev_err(dev,
			"request_irq IRQ LINE NOT AVAILABLE!, %d\n", ret);
		goto out;
	}

	t_dev_info(dev, "irq:%d, debounce:%d-%d:\n", ts->irq, ints[0], ints[1]);

out:
	return ret;
#else
	return -EFAULT;
#endif
}

int siw_touch_request_irq(struct siw_ts *ts,
			  irq_handler_t handler,
			  irq_handler_t thread_fn,
			  unsigned long flags,
			  const char *name)
{
	struct device *dev = ts->dev;
	u32 irq_use_scheule_work = touch_flags(ts) & IRQ_USE_SCHEDULE_WORK;
	int ret = 0;

	if (!ts->irq) {
		t_dev_err(dev, "failed to request irq : zero irq\n");
		ret = -EFAULT;
		goto out;
	}

	ts->irqflags_curr = flags;

	if (irq_use_scheule_work) {
		ret = siw_touch_request_irq_queue_work(ts,
						       handler, thread_fn,
						       flags, name);
	} else
		ret = request_threaded_irq(ts->irq, handler, thread_fn,
			flags, name, (void *)ts);
	if (ret) {
		t_dev_err(dev, "failed to request irq(%d, %s, 0x%X), %d\n",
			  ts->irq, name, (u32)flags, ret);
		goto out;
	}

	t_dev_info(dev, "%s irq request done(%d, %s, 0x%X)\n",
		   (irq_use_scheule_work) ?
		   "queue work" : "threaded",
		   ts->irq, name, (u32)flags);

out:
	return ret;
}

void siw_touch_free_irq(struct siw_ts *ts)
{
	struct device *dev = ts->dev;

	if (ts->irq) {
		free_irq(ts->irq, (void *)ts);
		t_dev_info(dev, "irq(%d) release done\n", ts->irq);
		ts->irq = 0;
		return;
	}
}


