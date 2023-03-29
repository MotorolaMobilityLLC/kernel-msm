/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC:wlan_interop_issues_ap_tgt_api.c
 *
 * This file provide API definitions to update interop issues ap from interface
 */
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_pdev_obj.h>
#include <scheduler_api.h>
#include <wlan_interop_issues_ap_api.h>
#include <wlan_interop_issues_ap_tgt_api.h>

static QDF_STATUS wlan_interop_issues_ap_flush_cbk(struct scheduler_msg *msg)
{
	if (msg->bodyptr) {
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wlan_interop_issues_ap_info_cbk(struct scheduler_msg *msg)
{
	struct wlan_interop_issues_ap_event *data;
	struct wlan_interop_issues_ap_callbacks *cbs;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	data = msg->bodyptr;
	data->pdev = wlan_objmgr_get_pdev_by_id(data->psoc,
						data->pdev_id,
						WLAN_INTEROP_ISSUES_AP_ID);
	if (!data->pdev) {
		interop_issues_ap_err("pdev is null.");
		status = QDF_STATUS_E_FAILURE;
		goto err;
	}

	cbs = interop_issues_ap_psoc_get_cbs(data->psoc);
	if (cbs && cbs->os_if_interop_issues_ap_event_handler)
		cbs->os_if_interop_issues_ap_event_handler(msg->bodyptr);

	wlan_objmgr_pdev_release_ref(data->pdev, WLAN_INTEROP_ISSUES_AP_ID);
err:
	qdf_mem_free(data);
	msg->bodyptr = NULL;
	return status;
}

QDF_STATUS tgt_interop_issues_ap_info_callback(struct wlan_objmgr_psoc *psoc,
				      struct wlan_interop_issues_ap_event *rap)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;
	struct wlan_interop_issues_ap_event *data;

	data = qdf_mem_malloc(sizeof(*data));
	if (!data)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(data, rap, sizeof(*data));

	msg.bodyptr = data;
	msg.callback = wlan_interop_issues_ap_info_cbk;
	msg.flush_callback = wlan_interop_issues_ap_flush_cbk;

	status = scheduler_post_message(QDF_MODULE_ID_INTEROP_ISSUES_AP,
					QDF_MODULE_ID_INTEROP_ISSUES_AP,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("scheduler msg posting failed");
		qdf_mem_free(msg.bodyptr);
		msg.bodyptr = NULL;
	}

	return status;
}

QDF_STATUS tgt_set_interop_issues_ap_req(struct wlan_objmgr_psoc *psoc,
				struct wlan_interop_issues_ap_info *rap)
{
	struct interop_issues_ap_psoc_priv_obj *obj;

	obj = interop_issues_ap_get_psoc_priv_obj(psoc);
	if (!obj || !obj->tx_ops.set_rap_ps)
		return QDF_STATUS_E_NULL_VALUE;

	return obj->tx_ops.set_rap_ps(psoc, rap);
}
