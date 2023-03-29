/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * DOC: declare utility API related to the disa component
 * called by other components
 */

#ifndef _WLAN_DISA_OBJ_MGMT_API_H_
#define _WLAN_DISA_OBJ_MGMT_API_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;

/**
 * disa_init() - register disa notification handlers.
 *
 * This function registers disa related notification handlers.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
#ifdef WLAN_FEATURE_DISA
QDF_STATUS disa_init(void);
#else
static inline QDF_STATUS disa_init(void)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * disa_deinit() - unregister disa notification handlers.
 *
 * This function unregisters disa related notification handlers.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
#ifdef WLAN_FEATURE_DISA
QDF_STATUS disa_deinit(void);
#else
static inline QDF_STATUS disa_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * disa_psoc_enable() - Trigger psoc enable for DISA
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
#ifdef WLAN_FEATURE_DISA
QDF_STATUS disa_psoc_enable(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS disa_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * disa_psoc_disable() - Trigger psoc disable for DISA
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
#ifdef WLAN_FEATURE_DISA
QDF_STATUS disa_psoc_disable(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS disa_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * disa_psoc_object_created_notification(): disa psoc create handler
 * @psoc: psoc which is going to created by objmgr
 * @arg: argument for psoc create handler
 *
 * Attach psoc private object, register rx/tx ops and event handlers
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS disa_psoc_object_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * disa_psoc_object_destroyed_notification(): disa psoc destroy handler
 * @psoc: objmgr object corresponding to psoc which is going to be destroyed
 * @arg: argument for psoc destroy handler
 *
 * Detach and free psoc private object, unregister event handlers
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS disa_psoc_object_destroyed_notification(
		struct wlan_objmgr_psoc *psoc, void *arg);

#endif /* end  of _WLAN_DISA_OBJ_MGMT_API_H_ */
