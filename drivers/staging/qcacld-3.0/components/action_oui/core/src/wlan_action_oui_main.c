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
 * DOC: Implement various notification handlers which are accessed
 * internally in action_oui component only.
 */

#include "wlan_action_oui_main.h"
#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_tgt_api.h"
#include "target_if_action_oui.h"

/**
 * action_oui_allocate() - Allocates memory for various actions.
 * @psoc_priv: pointer to action_oui psoc priv obj
 *
 * This function allocates memory for all the action_oui types
 * and initializes the respective lists to store extensions
 * extracted from action_oui_extract().
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
action_oui_allocate(struct action_oui_psoc_priv *psoc_priv)
{
	struct action_oui_priv *oui_priv;
	uint32_t i;
	uint32_t j;

	for (i = 0; i < ACTION_OUI_MAXIMUM_ID; i++) {
		oui_priv = qdf_mem_malloc(sizeof(*oui_priv));
		if (!oui_priv) {
			action_oui_err("Mem alloc failed for oui_priv id: %u",
					i);
			goto free_mem;
		}
		oui_priv->id = i;
		qdf_list_create(&oui_priv->extension_list,
				ACTION_OUI_MAX_EXTENSIONS);
		qdf_mutex_create(&oui_priv->extension_lock);
		psoc_priv->oui_priv[i] = oui_priv;
	}

	return QDF_STATUS_SUCCESS;

free_mem:
	for (j = 0; j < i; j++) {
		oui_priv = psoc_priv->oui_priv[j];
		if (!oui_priv)
			continue;

		qdf_list_destroy(&oui_priv->extension_list);
		qdf_mutex_destroy(&oui_priv->extension_lock);
		psoc_priv->oui_priv[j] = NULL;
	}

	return QDF_STATUS_E_NOMEM;
}

/**
 * action_oui_destroy() - Deallocates memory for various actions.
 * @psoc_priv: pointer to action_oui psoc priv obj
 *
 * This function Deallocates memory for all the action_oui types.
 * As a part of deallocate, all extensions are destroyed.
 *
 * Return: None
 */
static void
action_oui_destroy(struct action_oui_psoc_priv *psoc_priv)
{
	struct action_oui_priv *oui_priv;
	struct action_oui_extension_priv *ext_priv;
	qdf_list_t *ext_list;
	QDF_STATUS status;
	qdf_list_node_t *node = NULL;
	uint32_t i;

	psoc_priv->total_extensions = 0;
	for (i = 0; i < ACTION_OUI_MAXIMUM_ID; i++) {
		oui_priv = psoc_priv->oui_priv[i];
		psoc_priv->oui_priv[i] = NULL;
		if (!oui_priv)
			continue;

		ext_list = &oui_priv->extension_list;
		qdf_mutex_acquire(&oui_priv->extension_lock);
		while (!qdf_list_empty(ext_list)) {
			status = qdf_list_remove_front(ext_list, &node);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				action_oui_err("Invalid delete in action: %u",
						oui_priv->id);
				break;
			}
			ext_priv = qdf_container_of(node,
					struct action_oui_extension_priv,
					item);
			qdf_mem_free(ext_priv);
			ext_priv = NULL;
		}

		qdf_list_destroy(ext_list);
		qdf_mutex_release(&oui_priv->extension_lock);
		qdf_mutex_destroy(&oui_priv->extension_lock);
		qdf_mem_free(oui_priv);
		oui_priv = NULL;
	}
}

QDF_STATUS
action_oui_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct action_oui_psoc_priv *psoc_priv;
	QDF_STATUS status;

	ACTION_OUI_ENTER();

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
				WLAN_UMAC_COMP_ACTION_OUI,
				(void *)psoc_priv, QDF_STATUS_SUCCESS);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to attach priv with psoc");
		goto free_psoc_priv;
	}

	target_if_action_oui_register_tx_ops(&psoc_priv->tx_ops);
	psoc_priv->psoc = psoc;

	status = action_oui_allocate(psoc_priv);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		action_oui_err("Failed to alloc action_oui");
		goto detach_psoc_priv;
	}

	action_oui_debug("psoc priv attached");
	goto exit;

detach_psoc_priv:
	wlan_objmgr_psoc_component_obj_detach(psoc,
					      WLAN_UMAC_COMP_ACTION_OUI,
					      (void *)psoc_priv);
free_psoc_priv:
	qdf_mem_free(psoc_priv);
	status = QDF_STATUS_E_INVAL;
exit:
	ACTION_OUI_EXIT();
	return status;
}

QDF_STATUS
action_oui_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct action_oui_psoc_priv *psoc_priv = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ACTION_OUI_ENTER();

	psoc_priv = action_oui_psoc_get_priv(psoc);
	if (!psoc_priv) {
		action_oui_err("psoc priv is NULL");
		goto exit;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_ACTION_OUI,
					(void *)psoc_priv);
	if (!QDF_IS_STATUS_SUCCESS(status))
		action_oui_err("Failed to detach priv with psoc");

	action_oui_destroy(psoc_priv);
	qdf_mem_free(psoc_priv);

exit:
	ACTION_OUI_EXIT();
	return status;
}
