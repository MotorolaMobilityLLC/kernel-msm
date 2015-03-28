/* add cypress new driver ttda-02.03.01.476713 */

/*
 * cyttsp4_mt_common.h
 * Cypress TrueTouch(TM) Standard Product V4 Multi-touch module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
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

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/fb.h>

#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_mt.h>
#include "cyttsp4_regs.h"

struct cyttsp4_mt_data;
struct cyttsp4_mt_function {
	int (*mt_release) (struct cyttsp4_device * ttsp);
	int (*mt_probe) (struct cyttsp4_device * ttsp,
			 struct cyttsp4_mt_data * md);
	void (*report_slot_liftoff) (struct cyttsp4_mt_data * md,
				     int max_slots);
	void (*input_sync) (struct input_dev * input);
	void (*input_report) (struct input_dev * input, int sig, int t,
			      int type);
	void (*final_sync) (struct input_dev * input, int max_slots,
			    int mt_sync_count, unsigned long *ids);
	int (*input_register_device) (struct input_dev * input, int max_slots);
};

struct cyttsp4_mt_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_mt_platform_data *pdata;
	struct cyttsp4_sysinfo *si;
	struct input_dev *input;
	struct cyttsp4_mt_function mt_function;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend es;
#endif
	struct mutex report_lock;
	bool is_suspended;
	bool input_device_registered;
	char phys[NAME_MAX];
	int num_prv_rec;	/* Number of previous touch records */
	int prv_tch_type;
#ifdef VERBOSE_DEBUG
	u8 pr_buf[CY_MAX_PRBUF_SIZE];
#endif
	struct notifier_block fb_notif;
};

extern void cyttsp4_init_function_ptrs(struct cyttsp4_mt_data *md);
extern struct cyttsp4_driver cyttsp4_mt_driver;

