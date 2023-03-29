/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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
 * DOC: Declare public API related to the action_oui called by north bound
 * HDD/OSIF/LIM
 */

#ifndef _WLAN_ACTION_OUI_UCFG_API_H_
#define _WLAN_ACTION_OUI_UCFG_API_H_

#include <qdf_status.h>
#include <qdf_types.h>
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_objmgr.h"

#ifdef WLAN_FEATURE_ACTION_OUI

/**
 * ucfg_action_oui_init() - Register notification handlers.
 *
 * This function registers action_oui notification handlers which are
 * invoked from psoc create/destroy handlers.
 *
 * Return: For successful registration - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
QDF_STATUS ucfg_action_oui_init(void);

/**
 * ucfg_action_oui_deinit() - Unregister notification handlers.
 *
 * This function Unregisters action_oui notification handlers which are
 * invoked from psoc create/destroy handlers.
 *
 * Return: None
 */
void ucfg_action_oui_deinit(void);

/**
 * ucfg_action_oui_parse() - Parse input string and extract extensions.
 * @psoc: objmgr psoc object
 * @in_str: input string to be parsed
 * @action_id: action to which given string corresponds
 *
 * This is a wrapper function which invokes internal function
 * action_oui_extract() to extract OUIs and related attributes.
 *
 * Return: For successful parse - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
QDF_STATUS
ucfg_action_oui_parse(struct wlan_objmgr_psoc *psoc,
		      const uint8_t *in_str,
		      enum action_oui_id action_id);

/**
 * ucfg_action_oui_send() - Send action_oui and related attributes to Fw.
 * @psoc: objmgr psoc object
 *
 * This is a wrapper function which invokes internal function
 * action_oui_send() to send OUIs and related attributes to firmware.
 *
 * Return: For successful send - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
QDF_STATUS ucfg_action_oui_send(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_action_oui_enabled() - State of action_oui component
 *
 * Return: When action_oui component is present return true
 *	   else return false.
 */
static inline bool ucfg_action_oui_enabled(void)
{
	return true;
}

/**
 * ucfg_action_oui_search() - Check for OUIs and related info in IE data.
 * @psoc: objmgr psoc object
 * @attr: pointer to structure containing type of action, beacon IE data etc.,
 * @action_id: type of action to be checked
 *
 * This is a wrapper function which invokes internal function to search
 * for OUIs and related info (specified from ini file) in vendor specific
 * data of beacon IE for given action.
 *
 * Return: If search is successful return true else false.
 */
bool ucfg_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id);

#else

/**
 * ucfg_action_oui_init() - Register notification handlers.
 *
 * This function registers action_oui notification handlers which are
 * invoked from psoc create/destroy handlers.
 *
 * Return: For successful registration - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
static inline
QDF_STATUS ucfg_action_oui_init(void)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_action_oui_deinit() - Unregister notification handlers.
 *
 * This function Unregisters action_oui notification handlers which are
 * invoked from psoc create/destroy handlers.
 *
 * Return: None
 */
static inline
void ucfg_action_oui_deinit(void)
{
}

/**
 * ucfg_action_oui_parse() - Parse input string of action_id specified.
 * @psoc: objmgr psoc object
 * @in_str: input string to be parsed
 * @action_id: action to which given string corresponds
 *
 * This is a wrapper function which invokes internal function
 * action_oui_extract() to extract OUIs and related attributes.
 *
 * Return: For successful parse - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
static inline QDF_STATUS
ucfg_action_oui_parse(struct wlan_objmgr_psoc *psoc,
		      const uint8_t *in_str,
		      enum action_oui_id action_id)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_action_oui_send() - Send action_oui and related attributes to Fw.
 * @psoc: objmgr psoc object
 *
 * This is a wrapper function which invokes internal function
 * action_oui_send() to send OUIs and related attributes to firmware.
 *
 * Return: For successful send - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
static inline
QDF_STATUS ucfg_action_oui_send(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_action_oui_enabled() - State of action_oui component
 *
 * Return: When action_oui component is present return true
 *	   else return false.
 */
static inline bool ucfg_action_oui_enabled(void)
{
	return false;
}

/**
 * ucfg_action_oui_search() - Check for OUIs and related info in IE data.
 * @psoc: objmgr psoc object
 * @attr: pointer to structure containing type of action, beacon IE data etc.,
 * @action_id: type of action to be checked
 *
 * This is a wrapper function which invokes internal function to search
 * for OUIs and related info (specified from ini file) in vendor specific
 * data of beacon IE for given action.
 *
 * Return: If search is successful return true else false.
 */
static inline
bool ucfg_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id)
{
	return false;
}

#endif /* WLAN_FEATURE_ACTION_OUI */

#endif /* _WLAN_ACTION_OUI_UCFG_API_H_ */
