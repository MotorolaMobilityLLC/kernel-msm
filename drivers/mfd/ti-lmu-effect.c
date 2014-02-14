/*
 * TI LMU Effect Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * LMU effect driver is used for setting any light effects.
 * Each device has specific time value and register map.
 * Light effect can be controlled with consistent APIs.
 *
 * Examples:
 *   Backlight ramp time control - LM3532, LM3631, LM3633 and LM3697
 *   LED pattern display - LM3633
 *
 * Flow:
 *   1) LMU backlight and LED drivers request to the light effect driver
 *      by using ti_lmu_effect_request().
 *   2) Call ti_lmu_effect_set_* APIs in each callback function.
 */

#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-effect.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define LMU_EFFECT_MAX_TIME_PERIOD		9700

struct ti_lmu_effect_req {
	struct list_head list;
	const char *name;
	ti_lmu_effect_cb_t *cbfunc;
	int req_id;
	void *data;
};

struct ti_lmu_effect {
	struct list_head list;
	struct ti_lmu *lmu;

	const char *name;
	u8 addr;
	u8 mask;
	u8 shift;
	unsigned int val;
};

static DEFINE_MUTEX(lmu_effect_list_mutex);
static LIST_HEAD(lmu_effect_list);
static LIST_HEAD(lmu_effect_pending_list);

static const int lm3532_ramp_table[] = { 0, 1, 2, 4, 8, 16, 32, 65 };
static const int lm3631_ramp_table[] = {
	   0,   1,   2,    5,   10,   20,   50,  100,
	 250, 500, 750, 1000, 1500, 2000, 3000, 4000,
};
static const int lm3633_ramp_table[] = {
	   2, 250, 500, 1000, 2000, 4000, 8000, 16000,
};

static u8 ti_lmu_effect_get_ramp_index(struct ti_lmu_effect *lmu_effect,
				       int msec)
{
	const int *table = NULL;
	int size = 0;
	int index = 0;
	int i;

	switch (lmu_effect->lmu->id) {
	case LM3532:
		table = lm3532_ramp_table;
		size = ARRAY_SIZE(lm3532_ramp_table);
		break;
	case LM3631:
		table = lm3631_ramp_table;
		size = ARRAY_SIZE(lm3631_ramp_table);
		break;
	case LM3633:
	case LM3697:	/* LM3697 has same ramp table as LM3633 */
		table = lm3633_ramp_table;
		size = ARRAY_SIZE(lm3633_ramp_table);
		break;
	default:
		break;
	}

	if (msec <= table[0]) {
		index = 0;
		goto out;
	}

	if (msec >= table[size-1]) {
		index = size - 1;
		goto out;
	}

	/* Find appropriate register index from the table */
	for (i = 1; i < size; i++) {
		if (msec >= table[i-1] && msec < table[i]) {
			index = i - 1;
			goto out;
		}
	}

	return 0;
out:
	return index;
}

static u8 ti_lmu_effect_get_time_index(struct ti_lmu_effect *lmu_effect,
				       int msec)
{
	u8 idx, offset;

	/*
	 * Find appropriate register index around input time value
	 *
	 *      0 <= time <= 1000 : 16ms step
	 *   1000 <  time <= 9700 : 131ms step, base index is 61
	 */

	msec = min_t(int, msec, LMU_EFFECT_MAX_TIME_PERIOD);

	if (msec >= 0 && msec <= 1000) {
		idx = msec / 16;
		if (idx > 1)
			idx--;
		offset = 0;
	} else {
		idx = (msec - 1000) / 131;
		offset = 61;
	}

	return idx + offset;
}

static struct ti_lmu_effect *ti_lmu_effect_lookup(const char *name)
{
	struct ti_lmu_effect *lmu_effect;

	list_for_each_entry(lmu_effect, &lmu_effect_list, list)
		if (!strcmp(lmu_effect->name, name))
			return lmu_effect;

	return NULL;
}

int ti_lmu_effect_request(const char *name, ti_lmu_effect_cb_t cbfunc,
			  int req_id, void *data)
{
	struct ti_lmu_effect *lmu_effect;
	struct ti_lmu_effect_req *lmu_effect_req;

	if (!cbfunc || !name)
		return -EINVAL;

	/* If requested effect is in the list, handle it immediately */
	lmu_effect = ti_lmu_effect_lookup(name);
	if (lmu_effect) {
		cbfunc(lmu_effect, req_id, data);
		goto out;
	}

	/*
	 * If requested effect is not in the list yet,
	 * add it into pending request list to handle it later.
	 */

	lmu_effect_req = kzalloc(sizeof(*lmu_effect_req), GFP_KERNEL);
	if (!lmu_effect_req)
		return -ENOMEM;

	lmu_effect_req->name = name;
	lmu_effect_req->cbfunc = cbfunc;
	lmu_effect_req->req_id = req_id;
	lmu_effect_req->data = data;

	mutex_lock(&lmu_effect_list_mutex);
	list_add(&lmu_effect_req->list, &lmu_effect_pending_list);
	mutex_unlock(&lmu_effect_list_mutex);

out:
	return 0;
}
EXPORT_SYMBOL_GPL(ti_lmu_effect_request);

int ti_lmu_effect_set_ramp(struct ti_lmu_effect *lmu_effect, int msec)
{
	u8 val = ti_lmu_effect_get_ramp_index(lmu_effect, msec);

	return ti_lmu_update_bits(lmu_effect->lmu, lmu_effect->addr,
				  lmu_effect->mask, val << lmu_effect->shift);
}
EXPORT_SYMBOL_GPL(ti_lmu_effect_set_ramp);

int ti_lmu_effect_set_time(struct ti_lmu_effect *lmu_effect, int msec,
			   u8 reg_offset)
{
	u8 val = ti_lmu_effect_get_time_index(lmu_effect, msec);

	return ti_lmu_update_bits(lmu_effect->lmu,
				  lmu_effect->addr + reg_offset,
				  lmu_effect->mask, val << lmu_effect->shift);
}
EXPORT_SYMBOL_GPL(ti_lmu_effect_set_time);

int ti_lmu_effect_set_level(struct ti_lmu_effect *lmu_effect, u8 val,
			    u8 reg_offset)
{
	return ti_lmu_update_bits(lmu_effect->lmu,
				  lmu_effect->addr + reg_offset,
				  lmu_effect->mask, val << lmu_effect->shift);
}
EXPORT_SYMBOL_GPL(ti_lmu_effect_set_level);

static void ti_lmu_effect_handle_pending_list(struct ti_lmu_effect *lmu_effect)
{
	struct ti_lmu_effect_req *req, *n;

	/*
	 * If an effect driver is requested before, then handle it.
	 * Then, requested list and memory should be released.
	 */

	mutex_lock(&lmu_effect_list_mutex);

	list_for_each_entry_safe(req, n, &lmu_effect_pending_list, list) {
		if (!strcmp(req->name, lmu_effect->name)) {
			req->cbfunc(lmu_effect, req->req_id, req->data);
			list_del(&req->list);
			kfree(req);
		}
	}

	mutex_unlock(&lmu_effect_list_mutex);
}

static int ti_lmu_effect_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct ti_lmu_effect *lmu_effect;
	struct ti_lmu_effect *each;
	struct resource *r;
	int num_res = pdev->num_resources;
	int i;

	if (num_res <= 0) {
		dev_err(&pdev->dev, "Invalid numbers of resources: %d\n",
			num_res);
		return -EINVAL;
	}

	lmu_effect = devm_kzalloc(&pdev->dev, sizeof(*lmu_effect) * num_res,
				  GFP_KERNEL);
	if (!lmu_effect)
		return -ENOMEM;

	for (i = 0; i < num_res; i++) {
		r = platform_get_resource(pdev, IORESOURCE_REG, i);
		if (!r)
			continue;

		each = lmu_effect + i;

		each->name   = r->name;
		each->lmu    = lmu;
		each->addr   = LMU_EFFECT_GET_ADDR(r->start);
		each->mask   = LMU_EFFECT_GET_MASK(r->start);
		each->shift  = LMU_EFFECT_GET_SHIFT(r->start);

		mutex_lock(&lmu_effect_list_mutex);
		list_add(&each->list, &lmu_effect_list);
		mutex_unlock(&lmu_effect_list_mutex);

		/*
		 * If an effect driver is requested when the driver is not
		 * ready, the effect driver is inserted into the pending list.
		 * Then, pending requested driver is handled here.
		 */

		ti_lmu_effect_handle_pending_list(each);
	}

	platform_set_drvdata(pdev, lmu_effect);

	return 0;
}

static int ti_lmu_effect_remove(struct platform_device *pdev)
{
	struct ti_lmu_effect *lmu_effect, *n;
	struct ti_lmu_effect_req *req, *m;

	mutex_lock(&lmu_effect_list_mutex);

	list_for_each_entry_safe(lmu_effect, n, &lmu_effect_list, list)
		list_del(&lmu_effect->list);

	/* Remove pending list forcedly */
	list_for_each_entry_safe(req, m, &lmu_effect_pending_list, list) {
		list_del(&req->list);
		kfree(req);
	}

	mutex_unlock(&lmu_effect_list_mutex);

	return 0;
}

static struct platform_driver ti_lmu_effect_driver = {
	.probe = ti_lmu_effect_probe,
	.remove = ti_lmu_effect_remove,
	.driver = {
		.name = "lmu-effect",
		.owner = THIS_MODULE,
	},
};
module_platform_driver(ti_lmu_effect_driver);

MODULE_DESCRIPTION("TI LMU Effect Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lmu-effect");
