/* Copyright (c) 2014, Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/switch.h>
#include <linux/wakelock.h>
#include <linux/irq.h>

#include <linux/motosh.h>

struct mmi_hall_data *mmi_hall_init(void)
{
	struct mmi_hall_data *mdata = kzalloc(
			sizeof(struct mmi_hall_data), GFP_KERNEL);
	if (mdata) {
		int i;
		for (i = 0; i < MMI_HALL_MAX; i++)
			BLOCKING_INIT_NOTIFIER_HEAD(&mdata->nhead[i]);
		pr_debug("%s: data structure init-ed\n", __func__);
	}
	return mdata;
}

int mmi_hall_register_notifier(struct notifier_block *nb,
		unsigned long stype, bool report)
{
	int error;
	struct mmi_hall_data *mdata = motosh_misc_data->hall_data;

	if (!mdata)
		return -ENODEV;

	mdata->enabled |= 1 << stype;
	pr_debug("%s: hall sensor %lu notifier enabled\n", __func__, stype);

	error = blocking_notifier_chain_register(&mdata->nhead[stype], nb);
	if (!error && report) {
		int state = !!(mdata->state & (1 << stype));
		/* send current folio state on register request */
		blocking_notifier_call_chain(&mdata->nhead[stype],
				stype, (void *)&state);
		pr_debug("%s: reported state %d\n", __func__, state);
	}
	return error;
}

int mmi_hall_unregister_notifier(struct notifier_block *nb,
		unsigned long stype)
{
	int error;
	struct mmi_hall_data *mdata = motosh_misc_data->hall_data;

	if (!mdata)
		return -ENODEV;

	error = blocking_notifier_chain_unregister(&mdata->nhead[stype], nb);
	pr_debug("%s: hall sensor %lu notifier unregister\n", __func__, stype);

	if (!mdata->nhead[stype].head) {
		mdata->enabled &= ~(1 << stype);
		pr_debug("%s: hall sensor %lu no clients\n", __func__, stype);
	}

	return error;
}

void mmi_hall_notify(unsigned long stype, int state)
{
	struct mmi_hall_data *mdata = motosh_misc_data->hall_data;
	int stored;

	if (!mdata) {
		pr_debug("%s: notifier not initialized yet\n", __func__);
		return;
	}

	stored = !!(mdata->state & (1 << stype));
	mdata->state &= ~(1 << stype);
	mdata->state |= state << stype;
	pr_info("%s: current state %d (0x%x)\n", __func__,
				state, mdata->state);

	if ((mdata->enabled & (1 << stype)) && stored != state) {
		blocking_notifier_call_chain(&mdata->nhead[stype],
				stype, (void *)&state);
		pr_debug("%s: notification sent\n", __func__);
	}
}
