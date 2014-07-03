/*
 * Copyright © 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *    James C. Gualario <james.c.gualario@intel.com>
 *
 */

#ifndef _PSB_DPST_H_
#define _PSB_DPST_H_
/**
 * required includes
 *
 */
#include "psb_umevents.h"
/**
 * dpst event enumeration
 *
 */
enum dpst_event_enum {
	DPST_EVENT_INIT_COMPLETE,
	DPST_EVENT_HIST_INTERRUPT,
	DPST_EVENT_TERMINATE,
	DPST_EVENT_PHASE_COMPLETE,
	DPST_MAX_EVENT
};
/**
 * dpst specific defines
 *
 */
#define DRM_DPST_RING_DEPTH 256
#define DRM_DPST_RING_DEPTH_MAX (DRM_DPST_RING_DEPTH-1)
#define DRM_DPST_READY_TO_READ 1
#define DRM_DPST_READ_COMPLETE 2
#define DRM_DPST_MAX_NUM_EVENTS (DPST_MAX_EVENT)
/**
 * dpst workqueue data struct.
 */
struct dpst_disp_workqueue_data {
	struct work_struct work;
	const char *dev_name;
	int dev_name_write;
	int dev_name_read;
	int dev_name_write_wrap;
	int dev_name_read_write_wrap_ack;
	enum dpst_event_enum dpst_events[DRM_DPST_RING_DEPTH];
	int dev_name_arry_rw_status[DRM_DPST_RING_DEPTH];
	struct umevent_list *hotplug_dev_list;
	struct list_head *dev_umevent_arry[DRM_DPST_MAX_NUM_EVENTS];
};
/**
 * dpst state structure
 *
 */
struct dpst_state {
	struct workqueue_struct *dpst_wq;
	struct dpst_disp_workqueue_data dpst_change_wq_data;
	struct umevent_list *list;
};
/**
 * main interface function prototytpes for dpst support.
 *
 */
extern struct dpst_state *psb_dpst_init(struct kobject *parent_kobj);
extern int psb_dpst_notify_change_um(enum dpst_event_enum event,
				     struct dpst_state *state);
extern struct umevent_obj *psb_dpst_create_and_notify_um(const char *name, struct dpst_state
							 *state);
extern struct umevent_list *psb_dpst_device_pool_create_and_init(struct kobject
								 *parent_kobj, struct
								 dpst_state
								 *state);
extern void psb_dpst_device_pool_destroy(struct dpst_state *state);
/**
 * to go back and forth between work struct and workqueue data
 *
 */
#define to_dpst_disp_workqueue_data(x) \
	container_of(x, struct dpst_disp_workqueue_data, work)

/**
 * function prototypes for workqueue implementation
 *
 */
extern void psb_dpst_dev_change_wq(struct work_struct *work);
#endif
