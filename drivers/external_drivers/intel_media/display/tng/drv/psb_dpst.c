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
#include "psb_dpst.h"
#include "psb_dpst_func.h"

/**
 * inform the kernel of the work to be performed and related function.
 *
 */
DECLARE_WORK(dpst_dev_change_work, &psb_dpst_dev_change_wq);
/**
 * psb_dpst_notify_change_um - notify user mode of hotplug changes
 *
 * @name: name of event to notify user mode of change to
 * @state: dpst state struct to get workqueue from
 *
 */
int psb_dpst_notify_change_um(enum dpst_event_enum event,
			      struct dpst_state *state)
{
	if (state == NULL)
		return IRQ_HANDLED;

	state->dpst_change_wq_data.dev_name_arry_rw_status
	    [state->dpst_change_wq_data.dev_name_write] =
	    DRM_DPST_READY_TO_READ;
	state->dpst_change_wq_data.dpst_events
	    [state->dpst_change_wq_data.dev_name_write] = event;
	if (state->dpst_change_wq_data.dev_name_read_write_wrap_ack == 1)
		state->dpst_change_wq_data.dev_name_read_write_wrap_ack = 0;
	state->dpst_change_wq_data.dev_name_write++;
	if (state->dpst_change_wq_data.dev_name_write ==
	    state->dpst_change_wq_data.dev_name_read) {
		state->dpst_change_wq_data.dev_name_write--;
		return IRQ_NONE;
	}
	if (state->dpst_change_wq_data.dev_name_write > DRM_DPST_RING_DEPTH_MAX) {
		state->dpst_change_wq_data.dev_name_write = 0;
		state->dpst_change_wq_data.dev_name_write_wrap = 1;
	}
	state->dpst_change_wq_data.hotplug_dev_list = state->list;
	queue_work(state->dpst_wq, &(state->dpst_change_wq_data.work));
	return IRQ_HANDLED;
}

EXPORT_SYMBOL(psb_dpst_notify_change_um);
/**
 *
 * psb_dpst_create_and_notify_um - create and notify user mode of new dev
 *
 * @name: name to give for new event / device
 * @state: dpst state instaces to associate event with
 *
 */
struct umevent_obj *psb_dpst_create_and_notify_um(const char *name,
						  struct dpst_state *state)
{
	return psb_create_umevent_obj(name, state->list);

}

EXPORT_SYMBOL(psb_dpst_create_and_notify_um);
/**
 * psb_dpst_device_pool_create_and_init - make new hotplug device pool
 *
 * @parent_kobj - parent kobject to associate dpst kset with
 * @state - dpst state instance to associate list with
 *
 */
struct umevent_list *psb_dpst_device_pool_create_and_init(struct kobject
							  *parent_kobj, struct dpst_state
							  *state)
{
	struct umevent_list *new_hotplug_dev_list = NULL;
	new_hotplug_dev_list = psb_umevent_create_list();
	if (new_hotplug_dev_list)
		psb_umevent_init(parent_kobj, new_hotplug_dev_list, "psb_dpst");

	state->dpst_wq = create_singlethread_workqueue("dpst-wq");

	if (!state->dpst_wq)
		return NULL;

	INIT_WORK(&state->dpst_change_wq_data.work, psb_dpst_dev_change_wq);

	state->dpst_change_wq_data.dev_name_read = 0;
	state->dpst_change_wq_data.dev_name_write = 0;
	state->dpst_change_wq_data.dev_name_write_wrap = 0;
	state->dpst_change_wq_data.dev_name_read_write_wrap_ack = 0;

	memset(&(state->dpst_change_wq_data.dev_name_arry_rw_status[0]),
	       0, sizeof(int) * DRM_DPST_RING_DEPTH);

	return new_hotplug_dev_list;
}

EXPORT_SYMBOL(psb_dpst_device_pool_create_and_init);
/**
 * psb_dpst_init - init dpst subsystem
 * @parent_kobj - parent kobject to associate dpst state with
 *
 */
struct dpst_state *psb_dpst_init(struct kobject *parent_kobj)
{
	struct dpst_state *state;
	struct umevent_obj *working_umevent;

	state = kzalloc(sizeof(struct dpst_state), GFP_KERNEL);
	if (!state)
		return NULL;

	state->list = NULL;
	state->list = psb_dpst_device_pool_create_and_init(parent_kobj, state);
	working_umevent = psb_dpst_create_and_notify_um("init", state);
	state->dpst_change_wq_data.dev_umevent_arry
	    [DPST_EVENT_INIT_COMPLETE] = &(working_umevent->head);
	working_umevent = psb_dpst_create_and_notify_um("hist_int", state);
	state->dpst_change_wq_data.dev_umevent_arry
	    [DPST_EVENT_HIST_INTERRUPT] = &(working_umevent->head);
	working_umevent = psb_dpst_create_and_notify_um("term", state);
	state->dpst_change_wq_data.dev_umevent_arry
	    [DPST_EVENT_TERMINATE] = &(working_umevent->head);
	working_umevent = psb_dpst_create_and_notify_um("phase_done", state);
	state->dpst_change_wq_data.dev_umevent_arry
	    [DPST_EVENT_PHASE_COMPLETE] = &(working_umevent->head);

	return state;
}

EXPORT_SYMBOL(psb_dpst_init);
/**
 * psb_dpst_device_pool_destroy - destroy all dpst related resources
 *
 * @state: dpst state instance to destroy
 *
 */
void psb_dpst_device_pool_destroy(struct dpst_state *state)
{
	int i;
	struct umevent_list *list;
	struct umevent_obj *umevent_test;

	if(state == NULL)
	{
		DRM_INFO("DPST state already NULL in psb_dpst_device_pool_destroy\n");
		return;
	}

	list = state->list;
	flush_workqueue(state->dpst_wq);
	destroy_workqueue(state->dpst_wq);
	for (i = 0; i < DRM_DPST_MAX_NUM_EVENTS; i++) {
		umevent_test =
		    list_entry((state->dpst_change_wq_data.dev_umevent_arry[i]),
			       struct umevent_obj, head);
		state->dpst_change_wq_data.dev_umevent_arry[i] = NULL;
	}
	psb_umevent_cleanup(list);
	kfree(state);
}

EXPORT_SYMBOL(psb_dpst_device_pool_destroy);
/**
 * psb_dpst_dev_change_wq - change workqueue implementation
 *
 * @work: work struct to use for kernel scheduling
 *
 */
void psb_dpst_dev_change_wq(struct work_struct *work)
{
	struct dpst_disp_workqueue_data *wq_data;
	int curr_event_index;
	wq_data = to_dpst_disp_workqueue_data(work);
	if (wq_data->dev_name_write_wrap == 1) {
		wq_data->dev_name_read_write_wrap_ack = 1;
		wq_data->dev_name_write_wrap = 0;
		while (wq_data->dev_name_read != DRM_DPST_RING_DEPTH) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_DPST_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_DPST_READ_COMPLETE;
				curr_event_index = wq_data->dpst_events
				    [wq_data->dev_name_read];
				// SH DPST psb_umevent_notify_change_gfxsock
				if(curr_event_index < DRM_DPST_MAX_NUM_EVENTS) {
					dpst_process_event
						(list_entry((wq_data->dev_umevent_arry
							[curr_event_index]),
							struct umevent_obj, head),
							DRM_DPST_SOCKET_GROUP_ID);
				} else {
					DRM_ERROR("Work queue event index out of bounds.\n");
				}
			}
			wq_data->dev_name_read++;
		}
		wq_data->dev_name_read = 0;
		while (wq_data->dev_name_read < wq_data->dev_name_write - 1) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_DPST_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_DPST_READ_COMPLETE;
				curr_event_index = wq_data->dpst_events
				    [wq_data->dev_name_read];
				// SH DPST psb_umevent_notify_change_gfxsock
				if (curr_event_index < DRM_DPST_MAX_NUM_EVENTS) {
					dpst_process_event
						(list_entry((wq_data->dev_umevent_arry
							[curr_event_index]),
							struct umevent_obj, head),
							DRM_DPST_SOCKET_GROUP_ID);
				} else {
					DRM_ERROR("Work queue event index out of bounds.\n");
				}
			}
			wq_data->dev_name_read++;
		}
	} else {
		while (wq_data->dev_name_read < wq_data->dev_name_write) {
			if (wq_data->dev_name_arry_rw_status
			    [wq_data->dev_name_read] ==
			    DRM_DPST_READY_TO_READ) {
				wq_data->dev_name_arry_rw_status
				    [wq_data->dev_name_read] =
				    DRM_DPST_READ_COMPLETE;
				curr_event_index = wq_data->dpst_events
				    [wq_data->dev_name_read];
				// SH DPST psb_umevent_notify_change_gfxsock
				if (curr_event_index < DRM_DPST_MAX_NUM_EVENTS) {
					dpst_process_event
						(list_entry((wq_data->dev_umevent_arry
							[curr_event_index]),
							struct umevent_obj, head),
							DRM_DPST_SOCKET_GROUP_ID);
				} else {
					DRM_ERROR("Work queue event index out of bounds.\n");
				}

			}
			wq_data->dev_name_read++;
		}
	}
	if (wq_data->dev_name_read > DRM_DPST_RING_DEPTH_MAX)
		wq_data->dev_name_read = 0;
}

EXPORT_SYMBOL(psb_dpst_dev_change_wq);
