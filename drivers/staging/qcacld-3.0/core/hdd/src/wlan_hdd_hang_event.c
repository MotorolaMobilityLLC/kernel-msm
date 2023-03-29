/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
#include <qdf_hang_event_notifier.h>
#include <qdf_notifier.h>
#include <wlan_hdd_hang_event.h>
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_hdd_object_manager.h"
#include <qdf_types.h>

struct hdd_hang_event_fixed_param  {
	uint16_t tlv_header;
	uint8_t vdev_id;
	uint8_t vdev_opmode;
	uint8_t vdev_state;
	uint8_t vdev_substate;
} qdf_packed;

struct hdd_scan_fixed_param {
	uint16_t tlv_header;
	uint8_t last_scan_reject_vdev_id;
	enum scan_reject_states last_scan_reject_reason;
	unsigned long last_scan_reject_timestamp;
	uint8_t scan_reject_cnt;
} qdf_packed;

static int wlan_hdd_recovery_notifier_call(struct notifier_block *block,
					   unsigned long state,
					   void *data)
{
	qdf_notif_block *notif_block = qdf_container_of(block, qdf_notif_block,
							notif_block);
	struct hdd_context *hdd_ctx;
	struct qdf_notifer_data *hdd_hang_data = data;
	uint8_t *hdd_buf_ptr;
	struct hdd_adapter *adapter, *next_adapter = NULL;
	uint32_t total_len;
	struct wlan_objmgr_vdev *vdev;
	struct hdd_hang_event_fixed_param *cmd;
	struct hdd_scan_fixed_param *cmd_scan;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_RECOVERY_NOTIFIER_CALL;

	if (!data)
		return NOTIFY_STOP_MASK;

	hdd_ctx = notif_block->priv_data;
	if (!hdd_ctx)
		return NOTIFY_STOP_MASK;

	if (state == QDF_SCAN_ATTEMPT_FAILURES) {
		total_len = sizeof(*cmd_scan);
		hdd_buf_ptr = hdd_hang_data->hang_data + hdd_hang_data->offset;
		if (hdd_hang_data->offset + total_len > QDF_WLAN_HANG_FW_OFFSET)
			return NOTIFY_STOP_MASK;
		cmd_scan = (struct hdd_scan_fixed_param *)hdd_buf_ptr;
		QDF_HANG_EVT_SET_HDR(&cmd_scan->tlv_header,
				     HANG_EVT_TAG_OS_IF_SCAN,
		QDF_HANG_GET_STRUCT_TLVLEN(struct hdd_scan_fixed_param));
		cmd_scan->last_scan_reject_vdev_id =
					hdd_ctx->last_scan_reject_vdev_id;
		cmd_scan->last_scan_reject_reason =
					hdd_ctx->last_scan_reject_reason;
		cmd_scan->scan_reject_cnt =
					hdd_ctx->scan_reject_cnt;
		hdd_hang_data->offset += total_len;
	}

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		vdev = hdd_objmgr_get_vdev(adapter);
		if (!vdev) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}
		total_len = sizeof(*cmd);
		hdd_buf_ptr = hdd_hang_data->hang_data + hdd_hang_data->offset;
		if (hdd_hang_data->offset + total_len >
				QDF_WLAN_HANG_FW_OFFSET) {
			hdd_objmgr_put_vdev(vdev);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return NOTIFY_STOP_MASK;
		}
		cmd = (struct hdd_hang_event_fixed_param *)hdd_buf_ptr;
		QDF_HANG_EVT_SET_HDR(&cmd->tlv_header,
				     HANG_EVT_TAG_OS_IF,
		QDF_HANG_GET_STRUCT_TLVLEN(struct hdd_hang_event_fixed_param));
		cmd->vdev_id = wlan_vdev_get_id(vdev);
		cmd->vdev_opmode = wlan_vdev_mlme_get_opmode(vdev);
		cmd->vdev_state = wlan_vdev_mlme_get_state(vdev);
		cmd->vdev_substate = wlan_vdev_mlme_get_substate(vdev);
		hdd_hang_data->offset += total_len;
		hdd_objmgr_put_vdev(vdev);
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	return NOTIFY_OK;
}

static qdf_notif_block hdd_recovery_notifier = {
	.notif_block.notifier_call = wlan_hdd_recovery_notifier_call,
};

QDF_STATUS wlan_hdd_hang_event_notifier_register(struct hdd_context *hdd_ctx)
{
	hdd_recovery_notifier.priv_data = hdd_ctx;
	return qdf_hang_event_register_notifier(&hdd_recovery_notifier);
}

QDF_STATUS wlan_hdd_hang_event_notifier_unregister(void)
{
	return qdf_hang_event_unregister_notifier(&hdd_recovery_notifier);
}
