/*
 * siw_touch_mon_main.c - SiW touch monitor
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/input.h>

#include "../siw_touch.h"
#include "../siw_touch_bus.h"

#include "siw_touch_mon.h"


#define SIW_MON_VERSION		"v1.00"


DEFINE_MUTEX(__siw_mon_lock);

static LIST_HEAD(__siw_mon_term_list);

static struct siw_mon_term __siw_mon_term = { 0, };

struct siw_mon_buf_cache_operations {
	int (*create)(void);
	void (*destroy)(void);
	void *(*get)(unsigned char *src, int len);
	void (*put)(void *buf);
};

#if defined(__SIW_MON_USE_BUF_SLAB)

#define SIW_MON_CACHE_TAG	"cache: "

#define mon_pr_cache_info(fmt, args...)	\
		mon_pr_info(SIW_MON_CACHE_TAG fmt, ##args)

#define mon_pr_cache_err(fmt, args...)	\
		mon_pr_err(SIW_MON_CACHE_TAG fmt, ##args)

#define SIW_MON_CREATE_TAG	"create: "

#define mon_pr_create_info(fmt, args...)	\
		mon_pr_cache_info(SIW_MON_CREATE_TAG fmt, ##args)

#define mon_pr_create_err(fmt, args...)	\
		mon_pr_cache_err(SIW_MON_CREATE_TAG fmt, ##args)

#define SIW_MON_ZALLOC_TAG	"zalloc: "

#define mon_pr_zalloc_info(fmt, args...)	\
		mon_pr_cache_info(SIW_MON_ZALLOC_TAG fmt, ##args)

#define mon_pr_zalloc_err(fmt, args...)	\
		mon_pr_cache_err(SIW_MON_ZALLOC_TAG fmt, ##args)

struct siw_mon_buf_cache_ctrl {
	struct list_head list;
	struct kmem_cache *cache_buf;
	struct kmem_cache *cache_unit;
	atomic_t init_done;
};

struct siw_mon_buf_cache_unit {
	struct list_head link;
	unsigned char *buf;
};

static struct siw_mon_buf_cache_ctrl __siw_mon_buf_cache_ctrl = {
	.list = { NULL, },
	.cache_buf = NULL,
	.cache_unit = NULL,
	.init_done = { .counter = 0, },
};

static void __siw_mon_buf_ctor(void *mem)
{
	memset(mem, 0, siw_mon_prt_max());
}

static void __siw_mon_unit_ctor(void *mem)
{
	memset(mem, 0, sizeof(struct siw_mon_buf_cache_unit));
}

static int __siw_mon_create_buf_cache(void)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;
	struct kmem_cache *cache;
	int ret = 0;

	INIT_LIST_HEAD(&ctrl->list);

	cache = kmem_cache_create("siwmon_cache_buf",
					siw_mon_prt_max(), sizeof(u32), 0,
					__siw_mon_buf_ctor);
	if (cache == NULL) {
		mon_pr_create_err("failed to create memory cache for buf\n");
		ret = -ENOMEM;
		goto out;
	}
	ctrl->cache_buf = cache;

	cache = kmem_cache_create("siwmon_cache_unit",
				sizeof(struct siw_mon_buf_cache_unit), 0, 0,
				__siw_mon_unit_ctor);
	if (cache == NULL) {
		mon_pr_create_err("failed to create memory cache for unit\n");
		ret = -ENOMEM;
		goto out_ctrl;
	}
	ctrl->cache_unit = cache;

	atomic_set(&ctrl->init_done, 1);

	return 0;

out_ctrl:
	kmem_cache_destroy(ctrl->cache_buf);
	ctrl->cache_buf = NULL;

out:
	return ret;
}

static void __siw_mon_del_unit_cache(struct siw_mon_buf_cache_unit *unit)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;

	kmem_cache_free(ctrl->cache_buf, unit->buf);
	kmem_cache_free(ctrl->cache_unit, unit);
}

static void __siw_mon_del_buf_list(void *buf)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;
	struct siw_mon_buf_cache_unit *unit;
	struct list_head *p;

	while (!list_empty(&ctrl->list)) {
		p = ctrl->list.next;
		unit = list_entry(p, struct siw_mon_buf_cache_unit, link);
		if (buf == NULL) {
			/* Clear all */
			list_del(p);
			__siw_mon_del_unit_cache(unit);
		} else {
			if (unit->buf == buf) {
				list_del(p);
				__siw_mon_del_unit_cache(unit);
				return;
			}
		}
	}
}

static void __siw_mon_destroy_buf_cache(void)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;

	if (!atomic_read(&ctrl->init_done))
		return;

	atomic_set(&ctrl->init_done, 0);

	/* Clear all */
	__siw_mon_del_buf_list(NULL);

	kmem_cache_destroy(ctrl->cache_unit);
	kmem_cache_destroy(ctrl->cache_buf);

	ctrl->cache_buf = NULL;
	ctrl->cache_unit = NULL;

	INIT_LIST_HEAD(&ctrl->list);
}

static void *__siw_mon_zalloc_buf_cache(void)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;
	struct siw_mon_buf_cache_unit *unit;
	void *buf = NULL;

	if (!atomic_read(&ctrl->init_done)) {
		mon_pr_zalloc_err("not initialized\n");
		goto out;
	}

	unit = kmem_cache_zalloc(ctrl->cache_unit, GFP_ATOMIC);
	if (unit == NULL) {
		mon_pr_zalloc_err("failed to get memory from cache_unit\n");
		goto out;
	}

	buf = kmem_cache_zalloc(ctrl->cache_buf, GFP_ATOMIC);
	if (buf == NULL) {
		mon_pr_zalloc_err("failed to get memory from cache_buf\n");
		kmem_cache_free(ctrl->cache_unit, unit);
		goto out;
	}

	unit->buf = buf;
	list_add_tail(&unit->link, &ctrl->list);

out:
	return buf;
}

void *__siw_mon_get_buf_cache(unsigned char *src, int len)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;
	unsigned char *buf;

	if (!atomic_read(&ctrl->init_done))
		return src;

	buf = __siw_mon_zalloc_buf_cache();
	if (buf && src)
		memcpy(buf, src, len);

	return buf;
}

void __siw_mon_put_buf_cache(void *buf)
{
	struct siw_mon_buf_cache_ctrl *ctrl = &__siw_mon_buf_cache_ctrl;

	if (!atomic_read(&ctrl->init_done))
		return;

	if (!buf)
		return;

	/* Clear target buf : buf should be non-zero */
	__siw_mon_del_buf_list(buf);
}

static const struct siw_mon_buf_cache_operations __siw_mon_buf_cache_ops = {
	.create		= __siw_mon_create_buf_cache,
	.destroy	= __siw_mon_destroy_buf_cache,
	.get		= __siw_mon_get_buf_cache,
	.put		= __siw_mon_put_buf_cache,
};
#else	/* __SIW_MON_USE_BUF_SLAB */
static const struct siw_mon_buf_cache_operations __siw_mon_buf_cache_ops = {
	NULL,
};
#endif	/* __SIW_MON_USE_BUF_SLAB */

static int __used siw_mon_create_buf_cache(void)
{
	return (__siw_mon_buf_cache_ops.create) ?
			(__siw_mon_buf_cache_ops.create)() : 0;
}

static void __used siw_mon_destroy_buf_cache(void)
{
	return (__siw_mon_buf_cache_ops.destroy) ?
			(__siw_mon_buf_cache_ops.destroy)() : 0;
}

void __used *siw_mon_get_buf_cache(unsigned char *src, int len)
{
	return (__siw_mon_buf_cache_ops.get) ?
			(__siw_mon_buf_cache_ops.get)(src, len) : src;
}

void __used siw_mon_put_buf_cache(void *buf)
{
	if (__siw_mon_buf_cache_ops.put)
		(__siw_mon_buf_cache_ops.put)(buf);
}

static void siw_mon_stop(struct siw_mon_term *mterm)
{
	struct list_head *p;

	if (mterm == &__siw_mon_term) {
		list_for_each(p, &__siw_mon_term_list) {
			mterm = list_entry(p, struct siw_mon_term, term_link);
			if (!mterm->nreaders)
				mterm->monitored = 0;
		}
	} else {
		if (!__siw_mon_term.nreaders) {
			__siw_mon_term.monitored = 0;
			mb();
		}
	}
}

static void siw_mon_drop(struct kref *r)
{
	struct siw_mon_term *mterm = container_of(r, struct siw_mon_term, ref);

	mon_pr_dbg("siw mon term[0x%02X, %s] released\n",
				mterm->type, mterm->name);

	if (mterm == &__siw_mon_term)
		return;

	kfree(mterm);
}

/*
 * Tear usb_bus and mon_bus apart.
 */
static void siw_mon_dissolve(struct siw_mon_term *mterm)
{

	if (mterm->monitored) {
		mterm->monitored = 0;
		mb();
	}
	/* We want synchronize_irq() here, but that needs an argument. */
}


/*
 * Link a reader into the bus.
 *
 * This must be called with mon_lock taken because of mbus->ref.
 */
void siw_mon_reader_add(struct siw_mon_term *mterm,
					struct siw_mon_reader *r)
{
	unsigned long flags;
	struct list_head *p;

	spin_lock_irqsave(&mterm->lock, flags);
	if (mterm->nreaders == 0) {
		if (mterm == &__siw_mon_term) {
			list_for_each(p, &__siw_mon_term_list) {
				struct siw_mon_term *mt;

				mt = list_entry(p, struct siw_mon_term,
					term_link);
				mt->monitored = 1;
			}
		} else {
			mterm->monitored = 1;
		}
	}
	mterm->nreaders++;
	list_add_tail(&r->r_link, &mterm->r_list);
	spin_unlock_irqrestore(&mterm->lock, flags);

	kref_get(&mterm->ref);
}

/*
 * Unlink reader from the bus.
 *
 * This is called with mon_lock taken, so we can decrement mbus->ref.
 */
void siw_mon_reader_del(struct siw_mon_term *mterm,
					struct siw_mon_reader *r)
{
	unsigned long flags;

	spin_lock_irqsave(&mterm->lock, flags);
	list_del(&r->r_link);
	--mterm->nreaders;
	if (mterm->nreaders == 0)
		siw_mon_stop(mterm);
	spin_unlock_irqrestore(&mterm->lock, flags);

	kref_put(&mterm->ref, siw_mon_drop);
}


static void siw_mon_do_data_submit(struct siw_mon_term *mterm,
				struct siw_mon_data *data)
{
	unsigned long flags;
	struct list_head *pos;
	struct siw_mon_reader *r;

	if (mterm == NULL) {
		mon_pr_warn("NULL mterm\n");
		return;
	}
	if (mterm->type == SIW_MON_NONE) {
		mon_pr_warn("NULL type\n");
		return;
	}
	if ((mterm->type > SIW_MON_OPS) && (mterm->type != SIW_MON_ALL)) {
		mon_pr_warn("Invalid type, %d\n", mterm->type);
		return;
	}
	if (data == NULL) {
		mon_pr_warn("NULL data\n");
		return;
	}

	spin_lock_irqsave(&mterm->lock, flags);

	if (mterm != &__siw_mon_term) {
		if (!mterm->monitored)
			goto out;
	}

	mterm->cnt_events += data->inc_cnt;
	data->cnt_events = mterm->cnt_events;
	list_for_each(pos, &mterm->r_list) {
		r = list_entry(pos, struct siw_mon_reader, r_link);
		r->submit(r->r_data, data, &flags);
	}

out:
	spin_unlock_irqrestore(&mterm->lock, flags);
}

static void siw_mon_data_submit(struct siw_mon_data *data, int inc_cnt)
{
	struct list_head *pos;
	struct siw_mon_term *mterm = NULL;

	data->inc_cnt = inc_cnt;

	list_for_each(pos, &__siw_mon_term_list) {
		mterm = list_entry(pos, struct siw_mon_term, term_link);
		if (mterm->type == data->type) {
			siw_mon_do_data_submit(mterm, data);
			break;
		}
	}
	siw_mon_do_data_submit(&__siw_mon_term, data);
}

static void siw_mon_submit_bus_slice(struct siw_mon_data *smdata, void *data)
{
	struct siw_mon_data_bus *bus = &smdata->d.bus;
	struct touch_bus_msg *msg = data;
	unsigned char *buf_org = NULL;
	int tx_size, rx_size;
	int tx_len, rx_len, cur;
	int bus_cnt, tx_cnt, rx_cnt;
	int buf_max, prt_max;
	int bus_priv = bus->priv & MASK_BIT16;
	int i, inc_cnt = 1;

	/* delay for xfer case */
	if (bus_priv)
		usleep_range(1000, 1500);

	buf_max = siw_mon_buf_max();
	prt_max = siw_mon_prt_max();

	/* 1. backup original data size */
	tx_size = msg->tx_size;
	rx_size = msg->rx_size;

	/* 2. total limit : up to buffer limit */
	tx_len = (buf_max) ? min(tx_size, buf_max) : tx_size;
	rx_len = (buf_max) ? min(rx_size, buf_max) : rx_size;

	/* 3. line count : 1 line has 16 hex numbers */
	tx_cnt = (tx_len + prt_max - 1)/prt_max;
	rx_cnt = (rx_len + prt_max - 1)/prt_max;
	bus_cnt = tx_cnt + rx_cnt;

	/* Tx part */
	bus->rx_buf = NULL;
	bus->rx_size = 0;
	buf_org = msg->tx_buf;
	for (i = 0; i < tx_cnt; i++) {
		cur = min(tx_len, prt_max);

		bus->tx_buf = buf_org;
		bus->tx_size = siw_mon_set_buf_size(cur, tx_size);
		tx_len -= cur;
		buf_org += cur;

		bus->priv = siw_mon_set_priv_sub(bus->priv, i, bus_cnt);

		siw_mon_data_submit(smdata, inc_cnt);
		inc_cnt = 0;
	}

	/* Rx part */
	bus->tx_buf = NULL;
	bus->tx_size = 0;
	buf_org = msg->rx_buf;
	for (i = 0; i < rx_cnt; i++) {
		cur = min(rx_len, prt_max);

		bus->rx_buf = buf_org;
		bus->rx_size = siw_mon_set_buf_size(cur, rx_size);
		rx_len -= cur;
		buf_org += cur;

		bus->priv = siw_mon_set_priv_sub(bus->priv, i+tx_cnt, bus_cnt);

		siw_mon_data_submit(smdata, inc_cnt);
		inc_cnt = 0;
	}
}

static void
siw_mon_submit_bus(struct device *dev, char *dir, void *data, int ret)
{
	struct touch_bus_msg *msg = data;
	struct siw_mon_data smdata = {
		.type	= SIW_MON_BUS,
		.dev	= dev,
		.ret	= ret,
		.d	= {
			.bus = {
				.tx_buf		= msg->tx_buf,
				.tx_size	= msg->tx_size,
				.rx_buf		= msg->rx_buf,
				.rx_size	= msg->rx_size,
				.tx_dma		= msg->tx_dma,
				.rx_dma		= msg->rx_dma,
				.priv		= msg->priv,
			},
		},
	};
	struct siw_mon_data_bus *bus = &smdata.d.bus;

	snprintf(bus->dir, BUS_NAME_SZ, "%s", dir);

	siw_mon_submit_bus_slice(&smdata, data);
}

static void siw_mon_submit_evt(struct device *dev, char *type, int type_v,
				char *code, int code_v, int value, int ret)
{
	struct siw_mon_data smdata = {
		.type	= SIW_MON_EVT,
		.dev	= dev,
		.ret	= ret,
		.d	= {
			.evt = {
				.type_v	= type_v,
				.code_v	= code_v,
				.value	= value,
			},
		},
	};
	struct siw_mon_data_evt *evt = &smdata.d.evt;

	snprintf(evt->type, EVT_NAME_SZ, "%s", type);
	snprintf(evt->code, EVT_NAME_SZ, "%s", code);

	siw_mon_data_submit(&smdata, 1);
}

static int siw_mon_submit_ops_sock(struct siw_mon_data *smdata, int ret)
{
	struct siw_mon_data_ops *ops = &smdata->d.ops;
	unsigned char *buf = NULL;
	unsigned char *buf_org = NULL;
	int buf_max, prt_max;
	int buf_len = 0;
	int idx;
	int tot, cur;
	int ops_cnt, i;
	int inc_cnt = 1;

	idx = siw_mon_txt_sock_str_cmp(ops->ops);
	if (idx < 0)
		return idx;

	if (ret <= 0) {
		switch (idx) {
		case SIW_MON_SOCK_R:
			memset(ops->data, 0, sizeof(ops->data));
			break;
		case SIW_MON_SOCK_S:
			break;
		};
	}

	buf_max = siw_mon_buf_max();
	prt_max = siw_mon_prt_max();

	buf_org = (unsigned char *)ops->data[0];
	buf_len = ops->data[1];
	buf_len = (ret >= 0) ? min(buf_len, ret) : buf_len;

	if (!buf_org ||
		(!prt_max || (buf_len <= prt_max))) {
		if (buf_org && buf_len)
			buf = buf_org;

		ops->data[0] = (buf_len) ? ((size_t)buf) : 0;
		ops->data[1] = buf_len;
		ops->priv = 0;

		siw_mon_data_submit(smdata, inc_cnt);
		return 0;
	}

	tot = buf_len;
	buf_len = (buf_max) ? min(buf_len, buf_max) : buf_len;
	ops_cnt = (buf_len + prt_max - 1)/prt_max;

	for (i = 0; i < ops_cnt; i++) {
		cur = min(buf_len, prt_max);

		buf = buf_org;

		ops->data[0] = (cur) ? ((size_t)buf) : 0;
		ops->data[1] = siw_mon_set_buf_size(cur, tot);
		buf_len -= cur;
		buf_org += cur;

		ops->priv = siw_mon_set_priv_sub(ops->priv, i, ops_cnt);

		siw_mon_data_submit(smdata, inc_cnt);
		inc_cnt = 0;
	}

	return 0;
}

static void siw_mon_submit_ops(struct device *dev,
					char *ops_str, void *data,
					int size, int ret)
{
	struct siw_mon_data smdata = {
		.type	= SIW_MON_OPS,
		.dev	= dev,
		.ret	= ret,
		.d	= {
			.ops = {
				.priv = 0,
			},
		},
	};
	struct siw_mon_data_ops *ops = &smdata.d.ops;
	int len = (data != NULL) ? min(OPS_DATA_SZ, size) : 0;

	snprintf(ops->ops, OPS_NAME_SZ, "%s", ops_str);
	if (len && data)
		memcpy(ops->data, data, sizeof(ops->data[0]) * len);
	else
		memset(ops->data, 0, sizeof(ops->data));

	ops->len = len;

	if (!siw_mon_submit_ops_sock(&smdata, ret))
		return;

	siw_mon_data_submit(&smdata, 1);
}

static void __siw_mon_init_term(struct siw_mon_term *mterm,
				int type, char *name)
{
	mterm->type = type;
	snprintf(mterm->name, sizeof(mterm->name), "%s", name);
	mterm->prt_inited = siw_mon_prt_add(mterm, name);

	kref_init(&mterm->ref);
	spin_lock_init(&mterm->lock);
	INIT_LIST_HEAD(&mterm->r_list);

	mon_pr_dbg("siw mon term[0x%02X, %s] created\n",
				type, name);
}

static void __siw_mon_exit_term(struct siw_mon_term *mterm)
{
	if (mterm->prt_inited)
		siw_mon_prt_del(mterm);

	if (mterm->nreaders) {
		mon_pr_err("Outstanding opens (%d), leaking...\n",
			mterm->nreaders);
		atomic_set(&mterm->ref.refcount, 2);	/* Force leak */
	}

	siw_mon_dissolve(mterm);
	kref_put(&mterm->ref, siw_mon_drop);
}

static int siw_mon_subterm_add(int type, char *name)
{
	struct siw_mon_term *mterm = NULL;

	mterm = kzalloc(sizeof(struct siw_mon_term), GFP_KERNEL);
	if (mterm == NULL)
		return -ENOMEM;

	__siw_mon_init_term(mterm, type, name);

	mutex_lock(&__siw_mon_lock);
	list_add_tail(&mterm->term_link, &__siw_mon_term_list);
	mutex_unlock(&__siw_mon_lock);

	return 0;
}

static int siw_mon_subterm_init(void)
{
	(void)siw_mon_subterm_add(SIW_MON_BUS, SIW_MON_BUS_NAME);

	(void)siw_mon_subterm_add(SIW_MON_EVT, SIW_MON_EVT_NAME);

	(void)siw_mon_subterm_add(SIW_MON_OPS, SIW_MON_OPS_NAME);

	return 0;
}

static void siw_mon_subterm_exit(void)
{
	struct siw_mon_term *mterm = NULL;
	struct list_head *p;

	mutex_lock(&__siw_mon_lock);

	while (!list_empty(&__siw_mon_term_list)) {
		p = __siw_mon_term_list.next;
		mterm = list_entry(p, struct siw_mon_term, term_link);
		list_del(p);

		__siw_mon_exit_term(mterm);
	}

	mutex_unlock(&__siw_mon_lock);
}

static void siw_mon_mainterm_init(void)
{
	__siw_mon_init_term(&__siw_mon_term, SIW_MON_ALL, SIW_MON_ALL_NAME);
}

static void siw_mon_mainterm_exit(void)
{
	__siw_mon_exit_term(&__siw_mon_term);
	memset(&__siw_mon_term, 0, sizeof(__siw_mon_term));
}

static struct siw_mon_operations siw_mon_0 = {
	.submit_bus = siw_mon_submit_bus,
	.submit_evt = siw_mon_submit_evt,
	.submit_ops = siw_mon_submit_ops,
};

static int __init siw_mon_init(void)
{
	int ret = 0;

	ret = siw_mon_create_buf_cache();
	if (ret < 0)
		goto out;

	ret = siw_mon_prt_init();
	if (ret)
		goto out_prt;

	siw_mon_mainterm_init();

	ret = siw_mon_subterm_init();
	if (ret)
		goto out_subterm;

	if (siw_mon_register(&siw_mon_0) != 0) {
		mon_pr_err("unable to register with the core\n");
		ret = -ENODEV;
		goto out_reg;
	}

	mon_pr_info("siw mon created - %s\n", SIW_MON_VERSION);

	return 0;

out_reg:
	siw_mon_subterm_exit();

out_subterm:
	siw_mon_mainterm_exit();
	siw_mon_prt_exit();

out_prt:
	siw_mon_destroy_buf_cache();

out:

	return ret;
}

static void __exit siw_mon_exit(void)
{
	siw_mon_deregister();

	siw_mon_subterm_exit();

	siw_mon_mainterm_exit();

	siw_mon_prt_exit();

	siw_mon_destroy_buf_cache();

	mon_pr_info("siw mon removed - %s\n", SIW_MON_VERSION);
}

module_init(siw_mon_init);
module_exit(siw_mon_exit);

MODULE_AUTHOR("kimhh@siliconworks.co.kr");
MODULE_DESCRIPTION("SiW Touch Monitor Driver");
MODULE_VERSION(SIW_MON_VERSION);
MODULE_LICENSE("GPL");



