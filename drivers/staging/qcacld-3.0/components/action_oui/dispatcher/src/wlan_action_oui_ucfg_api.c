/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: Public API implementation of action_oui called by north bound HDD/OSIF.
 */

#include "wlan_action_oui_ucfg_api.h"
#include "wlan_action_oui_main.h"
#include "wlan_action_oui_main.h"
#include "target_if_action_oui.h"
#include "wlan_action_oui_tgt_api.h"
#include <qdf_str.h>

QDF_STATUS ucfg_action_oui_init(void)
{
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_create_notification, NULL);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to register psoc create handler");
		goto exit;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_destroy_notification, NULL);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_debug("psoc create/delete notifications registered");
		goto exit;
	}

	action_oui_err("Failed to register psoc delete handler");
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_ACTION_OUI,
			action_oui_psoc_create_notification, NULL);

exit:
	ACTION_OUI_EXIT();
	return status;
}

void ucfg_action_oui_deinit(void)
{
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_create_notification, NULL);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to unregister psoc create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_ACTION_OUI,
				action_oui_psoc_destroy_notification,
				NULL);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to unregister psoc delete handler");

	ACTION_OUI_EXIT();
}

QDF_STATUS
ucfg_action_oui_parse(struct wlan_objmgr_psoc *psoc,
		      const uint8_t *in_str,
		      enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint8_t *oui_str;
	int len;

	ACTION_OUI_ENTER();

	if (!psoc) {
		action_oui_err("psoc is NULL");
		goto exit;
	}

	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	len = qdf_str_len(in_str);
	if (len <= 0 || len > ACTION_OUI_MAX_STR_LEN - 1) {
		action_oui_err("Invalid string length: %u", action_id);
		goto exit;
	}

	oui_str = qdf_mem_malloc(len + 1);
	if (!oui_str) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	qdf_mem_copy(oui_str, in_str, len);
	oui_str[len] = '\0';

	status = action_oui_parse(psoc_priv, oui_str, action_id);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to parse: %u", action_id);

	qdf_mem_free(oui_str);

exit:
	ACTION_OUI_EXIT();
	return status;
}

QDF_STATUS ucfg_action_oui_send(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t id;

	if (!psoc) {
		action_oui_err("psoc is NULL");
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	for (id = 0; id < ACTION_OUI_MAXIMUM_ID; id++) {
		if (id >= ACTION_OUI_HOST_ONLY)
			continue;
		status = action_oui_send(psoc_priv, id);
		if (!QDF_IS_STATUS_SUCCESS(status))
			action_oui_err("Failed to send: %u", id);
	}

exit:

	return status;
}

bool ucfg_action_oui_search(struct wlan_objmgr_psoc *psoc,
			    struct action_oui_search_attr *attr,
			    enum action_oui_id action_id)
{
	struct action_oui_psoc_priv *psoc_priv;
	bool found = false;

	if (!psoc || !attr) {
		action_oui_err("Invalid psoc or search attrs");
		goto exit;
	}

	if (action_id >= ACTION_OUI_MAXIMUM_ID) {
		action_oui_err("Invalid action_oui id: %u", action_id);
		goto exit;
	}

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	found = action_oui_search(psoc_priv, attr, action_id);

exit:

	return found;
}
