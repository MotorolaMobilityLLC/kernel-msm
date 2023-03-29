/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Declare private API which shall be used internally only
 * in ftm_time_sync component. This file shall include prototypes of
 * various notification handlers and logging functions.
 *
 * Note: This API should be never accessed out of ftm_time_sync component.
 */

#ifndef _FTM_TIME_SYNC_MAIN_H_
#define _FTM_TIME_SYNC_MAIN_H_

#include <qdf_types.h>
#include <qdf_delayed_work.h>
#include "ftm_time_sync_priv.h"
#include "ftm_time_sync_objmgr.h"

#define ftm_time_sync_log(level, args...) \
	QDF_TRACE(QDF_MODULE_ID_FTM_TIME_SYNC, level, ## args)

#define ftm_time_sync_logfl(level, format, args...) \
	ftm_time_sync_log(level, FL(format), ## args)

#define ftm_time_sync_fatal(format, args...) \
		ftm_time_sync_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define ftm_time_sync_err(format, args...) \
		ftm_time_sync_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define ftm_time_sync_warn(format, args...) \
		ftm_time_sync_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define ftm_time_sync_info(format, args...) \
		ftm_time_sync_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define ftm_time_sync_debug(format, args...) \
		ftm_time_sync_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#define FTM_TIME_SYNC_ENTER() ftm_time_sync_debug("enter")
#define FTM_TIME_SYNC_EXIT() ftm_time_sync_debug("exit")

/**
 * ftm_time_sync_vdev_create_notification() - Handler for vdev create notify.
 * @vdev: vdev which is going to be created by objmgr
 * @arg: argument for notification handler
 *
 * Allocate and attach vdev private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ftm_time_sync_vdev_create_notification(struct wlan_objmgr_vdev *vdev,
						  void *arg);

/**
 * ftm_time_sync_vdev_destroy_notification() - Handler for vdev destroy notify.
 * @vdev: vdev which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach vdev private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ftm_time_sync_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev,
					void *arg);

/**
 * ftm_time_sync_psoc_create_notification() - Handler for psoc create notify.
 * @psoc: psoc which is going to be created by objmgr
 * @arg: argument for notification handler.
 *
 * Allocate and attach psoc private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ftm_time_sync_psoc_create_notification(struct wlan_objmgr_psoc *psoc,
				       void *arg);

/**
 * ftm_time_sync_psoc_destroy_notification() -  Handler for psoc destroy notify.
 * @psoc: psoc which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach psoc private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ftm_time_sync_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc,
					void *arg);

/**
 * ftm_time_sync_is_enable() - Function to advertise feature is enabled or not
 * @psoc: psoc context
 *
 * This function advertises whether the feature is enabled or not.
 *
 * Return: true if enable, false if disable
 */
bool ftm_time_sync_is_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ftm_time_sync_set_enable() - Handler to enable the feature
 * @psoc: psoc context
 * @value: value to be set
 *
 * This function is used to enable the ftm time sync feature.
 * The feature is enabled iff both ini and wmi service is advertised by
 * firmware.
 *
 * Return: None
 */
void ftm_time_sync_set_enable(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * ftm_time_sync_get_mode() - API to get the ftm time sync mode
 * @psoc: psoc context
 *
 * Return: enum ftm_time_sync_mode
 */
enum ftm_time_sync_mode ftm_time_sync_get_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ftm_time_sync_get_role() -  API to get the ftm time sync role
 * @psoc: psoc context
 *
 * Return: enum ftm_time_sync_role
 */
enum ftm_time_sync_role ftm_time_sync_get_role(struct wlan_objmgr_psoc *psoc);

/**
 * ftm_time_sync_send_trigger() - Handler for sending trigger cmd to FW
 * @vdev: vdev for which FTM time_sync trigger cmd to be send
 *
 * This function sends the ftm trigger cmd to target.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ftm_time_sync_send_trigger(struct wlan_objmgr_vdev *vdev);

/**
 * ftm_time_sync_stop() - Handler for stopping the FTM time sync
 * @vdev: vdev for which FTM time_sync feature to be stopped
 *
 * This function stops the ftm time sync functionality.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ftm_time_sync_stop(struct wlan_objmgr_vdev *vdev);

/**
 * ftm_time_sync_show() - Handler to print the offset derived
 * @vdev: vdev for which offset is to be shown
 * @buf: buffer in which the values to be printed
 *
 * Return: the number of bytes written in buf
 */
ssize_t ftm_time_sync_show(struct wlan_objmgr_vdev *vdev, char *buf);

/**
 * ftm_time_sync_update_bssid() - Update the bssid info
 * @vdev: vdev context
 * @bssid: bssid of connected AP
 *
 * Return: None
 */
void ftm_time_sync_update_bssid(struct wlan_objmgr_vdev *vdev,
				struct qdf_mac_addr bssid);

#endif /* end of _FTM_TIME_SYNC_MAIN_H_ */
