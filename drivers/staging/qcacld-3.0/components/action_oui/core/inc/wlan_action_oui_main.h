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
 * DOC: Declare private API which shall be used internally only
 * in action_oui component. This file shall include prototypes of
 * various notification handlers and logging functions.
 *
 * Note: This API should be never accessed out of action_oui component.
 */

#ifndef _WLAN_ACTION_OUI_MAIN_H_
#define _WLAN_ACTION_OUI_MAIN_H_

#include <qdf_types.h>
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_priv.h"
#include "wlan_action_oui_objmgr.h"

#define action_oui_log(level, args...) \
	QDF_TRACE(QDF_MODULE_ID_ACTION_OUI, level, ## args)

#define action_oui_logfl(level, format, args...) \
	action_oui_log(level, FL(format), ## args)

#define action_oui_fatal(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define action_oui_err(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define action_oui_warn(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define action_oui_info(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define action_oui_debug(format, args...) \
		action_oui_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#define ACTION_OUI_ENTER() action_oui_debug("enter")
#define ACTION_OUI_EXIT() action_oui_debug("exit")

/**
 * action_oui_psoc_create_notification(): Handler for psoc create notify.
 * @psoc: psoc which is going to be created by objmgr
 * @arg: argument for notification handler.
 *
 * Allocate and attach psoc private object.
 *
 * Return: QDF_STATUS status in case of success else return error.
 */
QDF_STATUS
action_oui_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * action_oui_psoc_destroy_notification(): Handler for psoc destroy notify.
 * @psoc: psoc which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach psoc private object.
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS
action_oui_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg);

#endif /* end  of _WLAN_ACTION_OUI_MAIN_H_ */
