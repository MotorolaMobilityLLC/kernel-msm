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
 * DOC: Declare private API which shall be used internally only
 * in action_oui component. This file shall include prototypes of
 * action_oui parsing and send logic.
 *
 * Note: This API should be never accessed out of action_oui component.
 */

#ifndef _WLAN_ACTION_OUI_PRIV_STRUCT_H_
#define _WLAN_ACTION_OUI_PRIV_STRUCT_H_

#include <qdf_list.h>
#include <qdf_types.h>
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_tgt_api.h"
#include "wlan_action_oui_objmgr.h"

/**
 * enum action_oui_token_type - String token types expected.
 * @ACTION_OUI_TOKEN: oui string
 * @ACTION_OUI_DATA_LENGTH_TOKEN: data length string
 * @ACTION_OUI_DATA_TOKEN: OUI data string
 * @ACTION_OUI_DATA_MASK_TOKEN: data mask string
 * @ACTION_OUI_INFO_MASK_TOKEN: info mask string
 * @ACTION_OUI_MAC_ADDR_TOKEN: mac addr string
 * @ACTION_OUI_MAC_MASK_TOKEN: mac mask string
 * @ACTION_OUI_CAPABILITY_TOKEN: capability string
 * @ACTION_OUI_END_TOKEN: end of one oui extension
 */
enum action_oui_token_type {
	ACTION_OUI_TOKEN = 1 << 0,
	ACTION_OUI_DATA_LENGTH_TOKEN = 1 << 1,
	ACTION_OUI_DATA_TOKEN = 1 << 2,
	ACTION_OUI_DATA_MASK_TOKEN = 1 << 3,
	ACTION_OUI_INFO_MASK_TOKEN = 1 << 4,
	ACTION_OUI_MAC_ADDR_TOKEN = 1 << 5,
	ACTION_OUI_MAC_MASK_TOKEN = 1 << 6,
	ACTION_OUI_CAPABILITY_TOKEN = 1 << 7,
	ACTION_OUI_END_TOKEN = 1 << 8,
};

/**
 * struct action_oui_extension_priv - Private contents of extension.
 * @item: list element
 * @extension: Extension contents
 *
 * This structure encapsulates action_oui_extension and list item.
 */
struct action_oui_extension_priv {
	qdf_list_node_t item;
	struct action_oui_extension extension;
};

/**
 * struct action_oui_priv - Each action info.
 * @id: type of action
 * @extension_list: list of extensions
 * @extension_lock: lock to control access to @extension_list
 *
 * All extensions of action specified by action_id are stored
 * at @extension_list as linked list.
 */
struct action_oui_priv {
	enum action_oui_id id;
	qdf_list_t extension_list;
	qdf_mutex_t extension_lock;
};

/**
 * struct action_oui_psoc_priv - Private object to be stored in psoc
 * @psoc: pointer to psoc object
 * @total_extensions: total count of extensions from all actions
 * @oui_priv: array of pointers used to refer each action info
 * @tx_ops: call-back functions to send OUIs to firmware
 */
struct action_oui_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
	uint32_t total_extensions;
	struct action_oui_priv *oui_priv[ACTION_OUI_MAXIMUM_ID];
	struct action_oui_tx_ops tx_ops;
};

/**
 * action_oui_parse() - Parse action oui string
 * @psoc_priv: pointer to action_oui psoc priv obj
 * @oui_string: string to be parsed
 * @ation_id: type of the action to be parsed
 *
 * This function parses the action oui string, extracts extensions and
 * stores them @action_oui_priv using list data structure.
 *
 * Return: QDF_STATUS
 *
 */
QDF_STATUS
action_oui_parse(struct action_oui_psoc_priv *psoc_priv,
		 uint8_t *oui_string, enum action_oui_id action_id);

/**
 * action_oui_send() - Send action oui extensions to target_if.
 * @psoc_priv: pointer to action_oui psoc priv obj
 * @ation_id: type of the action to be send
 *
 * This function sends action oui extensions to target_if.
 *
 * Return: QDF_STATUS
 *
 */
QDF_STATUS
action_oui_send(struct action_oui_psoc_priv *psoc_priv,
		enum action_oui_id action_id);

/**
 * action_oui_search() - Check if Vendor OUIs are present in IE buffer
 * @psoc_priv: pointer to action_oui psoc priv obj
 * @attr: pointer to structure containing type of action, beacon IE data etc.,
 * @action_id: type of action to be checked
 *
 * This function parses the IE buffer and finds if any of the vendor OUI
 * and related attributes are present in it.
 *
 * Return: If vendor OUI is present return true else false
 */
bool
action_oui_search(struct action_oui_psoc_priv *psoc_priv,
		  struct action_oui_search_attr *attr,
		  enum action_oui_id action_id);

#endif /* End  of _WLAN_ACTION_OUI_PRIV_STRUCT_H_ */
