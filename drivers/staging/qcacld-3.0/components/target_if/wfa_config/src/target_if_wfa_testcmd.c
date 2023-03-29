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
 * DOC: This file contains definitions for target_if wfa send test cmd.
 */

#include "qdf_types.h"
#include "target_if_wfa_testcmd.h"
#include "target_if.h"
#include "wlan_mlme_dbg.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_main.h"

static struct wmi_unified
*target_if_wfa_get_wmi_handle_from_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct wmi_unified *wmi_handle;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		target_if_err("PDEV is NULL");
		return NULL;
	}

	wmi_handle = get_wmi_unified_hdl_from_pdev(pdev);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return NULL;
	}

	return wmi_handle;
}

/**
 * target_if_wfa_send_cmd() - Send WFA test cmd to WMI
 * @vdev: VDEV object pointer
 * @wfa_test:  Pointer to WFA test params
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_wfa_send_cmd(struct wlan_objmgr_vdev *vdev,
		       struct set_wfatest_params *wfa_test)
{
	wmi_unified_t wmi_handle;

	wmi_handle = target_if_wfa_get_wmi_handle_from_vdev(vdev);
	if (!wmi_handle)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_wfa_test_cmd(wmi_handle, wfa_test);
}

QDF_STATUS
target_if_wfatestcmd_register_tx_ops(struct wlan_wfa_cmd_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("target if tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops->send_wfa_test_cmd = target_if_wfa_send_cmd;

	return QDF_STATUS_SUCCESS;
}
