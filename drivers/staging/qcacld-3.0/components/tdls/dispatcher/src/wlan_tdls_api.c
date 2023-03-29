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

/*
 * DOC: contains tdls link teardown definitions
 */

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_tdls_api.h"
#include "../../core/src/wlan_tdls_main.h"
#include "../../core/src/wlan_tdls_ct.h"
#include "../../core/src/wlan_tdls_mgmt.h"
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>

static QDF_STATUS tdls_teardown_flush_cb(struct scheduler_msg *msg)
{
	struct tdls_link_teardown *tdls_teardown = msg->bodyptr;
	struct wlan_objmgr_psoc *psoc = tdls_teardown->psoc;

	wlan_objmgr_psoc_release_ref(psoc, WLAN_TDLS_SB_ID);
	qdf_mem_free(tdls_teardown);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0, };
	struct tdls_link_teardown *link_teardown;

	link_teardown = qdf_mem_malloc(sizeof(*link_teardown));
	if (!link_teardown)
		return QDF_STATUS_E_NOMEM;

	wlan_objmgr_psoc_get_ref(psoc, WLAN_TDLS_SB_ID);
	link_teardown->psoc = psoc;
	msg.bodyptr = link_teardown;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_teardown_flush_cb;
	msg.type = TDLS_CMD_TEARDOWN_LINKS;

	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post msg fail, %d", status);
		wlan_objmgr_psoc_release_ref(psoc, WLAN_TDLS_SB_ID);
		qdf_mem_free(link_teardown);
	}

	return status;
}

void  wlan_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_vdev_priv_obj *vdev_priv_obj;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (!vdev)
		return;

	vdev_priv_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!vdev_priv_obj) {
		tdls_err("vdev priv is NULL");
		goto release_ref;
	}

	qdf_event_reset(&vdev_priv_obj->tdls_teardown_comp);

	status = wlan_tdls_teardown_links(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("wlan_tdls_teardown_links failed err %d", status);
		goto release_ref;
	}

	tdls_debug("Wait for tdls teardown completion. Timeout %u ms",
		   WAIT_TIME_FOR_TDLS_TEARDOWN_LINKS);

	status = qdf_wait_for_event_completion(
					&vdev_priv_obj->tdls_teardown_comp,
					WAIT_TIME_FOR_TDLS_TEARDOWN_LINKS);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err(" Teardown Completion timed out %d", status);
		goto release_ref;
	}

	tdls_debug("TDLS teardown completion status %d ", status);

release_ref:
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
}
