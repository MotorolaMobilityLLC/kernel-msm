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
#ifndef _PSB_HOTPLUG_H_
#define _PSB_HOTPLUG_H_
/**
 * required includes
 *
 */
#include "psb_umevents.h"
/**
 * hotplug specific defines
 *
 */
#define DRM_HOTPLUG_RING_DEPTH 256
#define DRM_HOTPLUG_RING_DEPTH_MAX (DRM_HOTPLUG_RING_DEPTH-1)
#define DRM_HOTPLUG_READY_TO_READ 1
#define DRM_HOTPLUG_READ_COMPLETE 2
/**
 * hotplug workqueue data struct.
 */
struct hotplug_disp_workqueue_data {
	struct work_struct work;
	const char *dev_name;
	int dev_name_write;
	int dev_name_read;
	int dev_name_write_wrap;
	int dev_name_read_write_wrap_ack;
	char dev_name_arry[DRM_HOTPLUG_RING_DEPTH][24];
	int dev_name_arry_rw_status[DRM_HOTPLUG_RING_DEPTH];
	struct umevent_list *hotplug_dev_list;
};
/**
 * hotplug state structure
 *
 */
struct hotplug_state {
	struct workqueue_struct *hotplug_wq;
	struct hotplug_disp_workqueue_data hotplug_remove_wq_data;
	struct hotplug_disp_workqueue_data hotplug_create_wq_data;
	struct hotplug_disp_workqueue_data hotplug_change_wq_data;
	struct umevent_list *list;
};
/**
 * main interface function prototytpes for hotplug support.
 *
 */
struct hotplug_state *psb_hotplug_init(struct kobject *parent_kobj);
extern int psb_hotplug_notify_change_um(const char *name,
					struct hotplug_state *state);
extern int psb_hotplug_create_and_notify_um(const char *name,
					    struct hotplug_state *state);
extern int psb_hotplug_remove_and_notify_um(const char *name,
					    struct hotplug_state *state);
extern struct umevent_list *psb_hotplug_device_pool_create_and_init(struct
								    kobject
								    *parent_kobj, struct
								    hotplug_state
								    *state);
extern void psb_hotplug_device_pool_destroy(struct hotplug_state *state);
/**
 * to go back and forth between work strauct and workqueue data
 *
 */
#define to_hotplug_disp_workqueue_data(x) \
	container_of(x, struct hotplug_disp_workqueue_data, work)

/**
 * function prototypes for workqueue implementation
 *
 */
extern void psb_hotplug_dev_create_wq(struct work_struct *work);
extern void psb_hotplug_dev_remove_wq(struct work_struct *work);
extern void psb_hotplug_dev_change_wq(struct work_struct *work);
#endif
