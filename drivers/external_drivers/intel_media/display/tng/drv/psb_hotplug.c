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

#include "psb_umevents.h"
#include "psb_hotplug.h"
/**
 * inform the kernel of the work to be performed and related function.
 *
 */
DECLARE_WORK(hotplug_dev_create_work, &psb_hotplug_dev_create_wq);
DECLARE_WORK(hotplug_dev_remove_work, &psb_hotplug_dev_remove_wq);
DECLARE_WORK(hotplug_dev_change_work, &psb_hotplug_dev_change_wq);
/**
 * psb_hotplug_notify_change_um - notify user mode of hotplug changes
 *
 * @name: name of event to notify user mode of change to
 * @state: hotplug state to search for event object in
 *
 */
int psb_hotplug_notify_change_um(const char *name, struct hotplug_state *state)
{
	strcpy(&(state->hotplug_change_wq_data.dev_name_arry
		 [state->hotplug_change_wq_data.dev_name_write][0]), name);
	state->hotplug_change_wq_data.dev_name_arry_rw_status
	    [state->hotplug_change_wq_data.dev_name_write] =
	    DRM_HOTPLUG_READY_TO_READ;
	if (state->hotplug_change_wq_data.dev_name_read_write_wrap_ack == 1)
		state->hotplug_change_wq_data.dev_name_read_write_wrap_ack = 0;
	state->hotplug_change_wq_data.dev_name_write++;
	if (state->hotplug_change_wq_data.dev_name_write ==
	    state->hotplug_change_wq_data.dev_name_read) {
		state->hotplug_change_wq_data.dev_name_write--;
		return IRQ_NONE;
	}
	if (state->hotplug_change_wq_data.dev_name_write >
	    DRM_HOTPLUG_RING_DEPTH_MAX) {
		state->hotplug_change_wq_data.dev_name_write = 0;
		state->hotplug_change_wq_data.dev_name_write_wrap = 1;
	}
	state->hotplug_change_wq_data.hotplug_dev_list = state->list;
	queue_work(state->hotplug_wq, &(state->hotplug_change_wq_data.work));
	return IRQ_HANDLED;
}

/**
 *
 * psb_hotplug_create_and_notify_um - create and notify user mode of new dev
 *
 * @name: name to give for new event / device
 * @state: hotplug state to track new event /device in
 *
 */
int psb_hotplug_create_and_notify_um(const char *name,
				     struct hotplug_state *state)
{
	strcpy(&(state->hotplug_create_wq_data.dev_name_arry
		 [state->hotplug_create_wq_data.dev_name_write][0]), name);
	state->hotplug_create_wq_data.dev_name_arry_rw_status
	    [state->hotplug_create_wq_data.dev_name_write] =
	    DRM_HOTPLUG_READY_TO_READ;
	if (state->hotplug_create_wq_data.dev_name_read_write_wrap_ack == 1)
		state->hotplug_create_wq_data.dev_name_read_write_wrap_ack = 0;
	state->hotplug_create_wq_data.dev_name_write++;
	if (state->hotplug_create_wq_data.dev_name_write ==
	    state->hotplug_create_wq_data.dev_name_read) {
		state->hotplug_create_wq_data.dev_name_write--;
		return IRQ_NONE;
	}
	if (state->hotplug_create_wq_data.dev_name_write >
	    DRM_HOTPLUG_RING_DEPTH_MAX) {
		state->hotplug_create_wq_data.dev_name_write = 0;
		state->hotplug_create_wq_data.dev_name_write_wrap = 1;
	}
	state->hotplug_create_wq_data.hotplug_dev_list = state->list;
	queue_work(state->hotplug_wq, &(state->hotplug_create_wq_data.work));
	return IRQ_HANDLED;
}

/*EXPORT_SYMBOL(psb_hotplug_create_and_notify_um); */
/**
 * psb_hotplug_remove_and_notify_um - remove device and notify user mode
 *
 * @name: name of event / device to remove
 * @state: hotplug state to remove event / device from
 *
 */
int psb_hotplug_remove_and_notify_um(const char *name,
				     struct hotplug_state *state)
{
	strcpy(&(state->hotplug_remove_wq_data.dev_name_arry
		 [state->hotplug_remove_wq_data.dev_name_write][0]), name);
	state->hotplug_remove_wq_data.dev_name_arry_rw_status
	    [state->hotplug_remove_wq_data.dev_name_write] =
	    DRM_HOTPLUG_READY_TO_READ;
	if (state->hotplug_remove_wq_data.dev_name_read_write_wrap_ack == 1)
		state->hotplug_remove_wq_data.dev_name_read_write_wrap_ack = 0;
	state->hotplug_remove_wq_data.dev_name_write++;
	if (state->hotplug_remove_wq_data.dev_name_write ==
	    state->hotplug_remove_wq_data.dev_name_read) {
		state->hotplug_remove_wq_data.dev_name_write--;
		return IRQ_NONE;
	}
	if (state->hotplug_remove_wq_data.dev_name_write >
	    DRM_HOTPLUG_RING_DEPTH_MAX) {
		state->hotplug_remove_wq_data.dev_name_write = 0;
		state->hotplug_remove_wq_data.dev_name_write_wrap = 1;
	}
	state->hotplug_remove_wq_data.hotplug_dev_list = state->list;
	queue_work(state->hotplug_wq, &(state->hotplug_remove_wq_data.work));
	return IRQ_HANDLED;
}

/*EXPORT_SYMBOL(psb_hotplug_remove_and_notify_um); */
/**
 * psb_hotplug_device_pool_create_and_init - make new hotplug device pool
 *
 * @parent_kobj: parent kobject to associate hotplug kset with
 * @state: hotplug state to assocaite workqueues with
 *
 */
struct umevent_list *psb_hotplug_device_pool_create_and_init(struct kobject
							     *parent_kobj, struct
							     hotplug_state
							     *state)
{
	struct umevent_list *new_hotplug_dev_list = NULL;

	new_hotplug_dev_list = psb_umevent_create_list();
	if (new_hotplug_dev_list)
		psb_umevent_init(parent_kobj, new_hotplug_dev_list,
				 "psb_hotplug");

	state->hotplug_wq = create_singlethread_workqueue("hotplug-wq");
	if (!state->hotplug_wq)
		return NULL;

	INIT_WORK(&state->hotplug_create_wq_data.work,
		  psb_hotplug_dev_create_wq);
	INIT_WORK(&state->hotplug_remove_wq_data.work,
		  psb_hotplug_dev_remove_wq);
	INIT_WORK(&state->hotplug_change_wq_data.work,
		  psb_hotplug_dev_change_wq);

	state->hotplug_create_wq_data.dev_name_read = 0;
	state->hotplug_create_wq_data.dev_name_write = 0;
	state->hotplug_create_wq_data.dev_name_write_wrap = 0;
	state->hotplug_create_wq_data.dev_name_read_write_wrap_ack = 0;
	memset(&(state->hotplug_create_wq_data.dev_name_arry_rw_status[0]),
	       0, sizeof(int) * DRM_HOTPLUG_RING_DEPTH);

	state->hotplug_remove_wq_data.dev_name_read = 0;
	state->hotplug_remove_wq_data.dev_name_write = 0;
	state->hotplug_remove_wq_data.dev_name_write_wrap = 0;
	state->hotplug_remove_wq_data.dev_name_read_write_wrap_ack = 0;
	memset(&(state->hotplug_remove_wq_data.dev_name_arry_rw_status[0]),
	       0, sizeof(int) * DRM_HOTPLUG_RING_DEPTH);

	state->hotplug_change_wq_data.dev_name_read = 0;
	state->hotplug_change_wq_data.dev_name_write = 0;
	state->hotplug_change_wq_data.dev_name_write_wrap = 0;
	state->hotplug_change_wq_data.dev_name_read_write_wrap_ack = 0;
	memset(&(state->hotplug_change_wq_data.dev_name_arry_rw_status[0]),
	       0, sizeof(int) * DRM_HOTPLUG_RING_DEPTH);

	return new_hotplug_dev_list;
}

/*EXPORT_SYMBOL(psb_hotplug_device_pool_create_and_init); */
/**
 *
 * psb_hotplug_init - init hotplug subsystem
 *
 * @parent_kobj: parent kobject to associate hotplug state with
 *
 */
struct hotplug_state *psb_hotplug_init(struct kobject *parent_kobj)
{
	struct hotplug_state *state;
	state = kzalloc(sizeof(struct hotplug_state), GFP_KERNEL);

	if (!state)
		return state;

	state->list = NULL;
	state->list = psb_hotplug_device_pool_create_and_init(parent_kobj,
							      state);

	psb_hotplug_create_and_notify_um("hpd_hdmi", state);

	return state;
}

/**
 * psb_hotplug_device_pool_destroy - destroy all hotplug related resources
 *
 * @state: hotplug state to destroy
 *
 */
void psb_hotplug_device_pool_destroy(struct hotplug_state *state)
{
	flush_workqueue(state->hotplug_wq);
	destroy_workqueue(state->hotplug_wq);
	psb_umevent_cleanup(state->list);
	kfree(state);
}

/*EXPORT_SYMBOL(psb_hotplug_device_pool_destroy); */
/**
 * psb_hotplug_dev_create_wq - create workqueue implementation
 *
 * @work: work struct to use for kernel scheduling
 *
 */
void psb_hotplug_dev_create_wq(struct work_struct *work)
{
	struct hotplug_disp_workqueue_data *wq_data;
	struct umevent_obj *wq_working_hotplug_disp_obj;
	wq_data = to_hotplug_disp_workqueue_data(work);
	if (wq_data->dev_name_write_wrap == 1) {
		wq_data->dev_name_read_write_wrap_ack = 1;
		wq_data->dev_name_write_wrap = 0;
		while (wq_data->dev_name_read != DRM_HOTPLUG_RING_DEPTH_MAX) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_working_hotplug_disp_obj =
				    psb_create_umevent_obj
				    (&wq_data->dev_name_arry
				     [wq_data->dev_name_read][0],
				     wq_data->hotplug_dev_list);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
				/* psb_umevent_notify
				   (wq_working_hotplug_disp_obj); */
			}
			wq_data->dev_name_read++;
		}
		wq_data->dev_name_read = 0;
		while (wq_data->dev_name_read < wq_data->dev_name_write - 1) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_working_hotplug_disp_obj =
				    psb_create_umevent_obj
				    (&wq_data->dev_name_arry
				     [wq_data->dev_name_read][0],
				     wq_data->hotplug_dev_list);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
				/*psb_umevent_notify
				   (wq_working_hotplug_disp_obj); */
			}
			wq_data->dev_name_read++;
		}
	} else {
		while (wq_data->dev_name_read < wq_data->dev_name_write) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_working_hotplug_disp_obj =
				    psb_create_umevent_obj
				    (&wq_data->dev_name_arry
				     [wq_data->dev_name_read][0],
				     wq_data->hotplug_dev_list);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
				/*psb_umevent_notify
				   (wq_working_hotplug_disp_obj); */
			}
			wq_data->dev_name_read++;
		}
	}
	if (wq_data->dev_name_read > DRM_HOTPLUG_RING_DEPTH_MAX)
		wq_data->dev_name_read = 0;
}

/*EXPORT_SYMBOL(psb_hotplug_dev_create_wq); */
/**
 * psb_hotplug_dev_remove_wq - remove workqueue implementation
 *
 * @work: work struct to use for kernel scheduling
 *
 */
void psb_hotplug_dev_remove_wq(struct work_struct *work)
{
	struct hotplug_disp_workqueue_data *wq_data;
	wq_data = to_hotplug_disp_workqueue_data(work);
	if (wq_data->dev_name_write_wrap == 1) {
		wq_data->dev_name_read_write_wrap_ack = 1;
		wq_data->dev_name_write_wrap = 0;
		while (wq_data->dev_name_read != DRM_HOTPLUG_RING_DEPTH_MAX) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				psb_umevent_remove_from_list
				    (wq_data->hotplug_dev_list,
				     &wq_data->dev_name_arry
				     [wq_data->dev_name_read]
				     [0]);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
			}
			wq_data->dev_name_read++;
		}
		wq_data->dev_name_read = 0;
		while (wq_data->dev_name_read < wq_data->dev_name_write - 1) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				psb_umevent_remove_from_list
				    (wq_data->hotplug_dev_list,
				     &wq_data->dev_name_arry
				     [wq_data->dev_name_read]
				     [0]);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
			}
			wq_data->dev_name_read++;
		}
	} else {
		while (wq_data->dev_name_read < wq_data->dev_name_write) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				psb_umevent_remove_from_list
				    (wq_data->hotplug_dev_list,
				     &wq_data->dev_name_arry
				     [wq_data->dev_name_read]
				     [0]);
				wq_data->
				    dev_name_arry_rw_status
				    [wq_data->dev_name_read]
				    = DRM_HOTPLUG_READ_COMPLETE;
			}
			wq_data->dev_name_read++;
		}
	}
	if (wq_data->dev_name_read > DRM_HOTPLUG_RING_DEPTH_MAX)
		wq_data->dev_name_read = 0;
}

/*EXPORT_SYMBOL(psb_hotplug_dev_remove_wq); */
/**
 * psb_hotplug_dev_change_wq - change workqueue implementation
 *
 * @work: work struct to use for kernel scheduling
 *
 */
void psb_hotplug_dev_change_wq(struct work_struct *work)
{
	struct hotplug_disp_workqueue_data *wq_data;
	struct umevent_obj *wq_working_hotplug_disp_obj;
	wq_data = to_hotplug_disp_workqueue_data(work);
	if (wq_data->dev_name_write_wrap == 1) {
		wq_data->dev_name_read_write_wrap_ack = 1;
		wq_data->dev_name_write_wrap = 0;
		while (wq_data->dev_name_read != DRM_HOTPLUG_RING_DEPTH_MAX) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_HOTPLUG_READ_COMPLETE;

				wq_working_hotplug_disp_obj =
				    psb_umevent_find_obj(&wq_data->dev_name_arry
							 [wq_data->dev_name_read]
							 [0],
							 wq_data->hotplug_dev_list);
				psb_umevent_notify_change_gfxsock
				    (wq_working_hotplug_disp_obj,
				     DRM_HOTPLUG_SOCKET_GROUP_ID);
			}
			wq_data->dev_name_read++;
		}
		wq_data->dev_name_read = 0;
		while (wq_data->dev_name_read < wq_data->dev_name_write - 1) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_HOTPLUG_READ_COMPLETE;

				wq_working_hotplug_disp_obj =
				    psb_umevent_find_obj(&wq_data->dev_name_arry
							 [wq_data->dev_name_read]
							 [0],
							 wq_data->hotplug_dev_list);
				psb_umevent_notify_change_gfxsock
				    (wq_working_hotplug_disp_obj,
				     DRM_HOTPLUG_SOCKET_GROUP_ID);
			}
			wq_data->dev_name_read++;
		}
	} else {
		while (wq_data->dev_name_read < wq_data->dev_name_write) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_HOTPLUG_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_HOTPLUG_READ_COMPLETE;

				wq_working_hotplug_disp_obj =
				    psb_umevent_find_obj(&wq_data->dev_name_arry
							 [wq_data->dev_name_read]
							 [0],
							 wq_data->hotplug_dev_list);
				psb_umevent_notify_change_gfxsock
				    (wq_working_hotplug_disp_obj,
				     DRM_HOTPLUG_SOCKET_GROUP_ID);
			}
			wq_data->dev_name_read++;
		}
	}
	if (wq_data->dev_name_read > DRM_HOTPLUG_RING_DEPTH_MAX)
		wq_data->dev_name_read = 0;
}

/*EXPORT_SYMBOL(psb_hotplug_dev_change_wq); */
