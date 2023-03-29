/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_tgt_api.c
 *
 * TDLS south bound interface definitions
 */

#include "qdf_status.h"
#include <wlan_tdls_tgt_api.h>
#include "../../core/src/wlan_tdls_main.h"
#include "../../core/src/wlan_tdls_cmds_process.h"
#include "../../core/src/wlan_tdls_mgmt.h"

static inline struct wlan_lmac_if_tdls_tx_ops *
wlan_psoc_get_tdls_txops(struct wlan_objmgr_psoc *psoc)
{
	return &psoc->soc_cb.tx_ops->tdls_tx_ops;
}

static inline struct wlan_lmac_if_tdls_rx_ops *
wlan_psoc_get_tdls_rxops(struct wlan_objmgr_psoc *psoc)
{
	return &psoc->soc_cb.rx_ops->tdls_rx_ops;
}

QDF_STATUS tgt_tdls_set_fw_state(struct wlan_objmgr_psoc *psoc,
				 struct tdls_info *tdls_param)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_ops = NULL;

	tdls_ops = wlan_psoc_get_tdls_txops(psoc);
	if (tdls_ops && tdls_ops->update_fw_state)
		return tdls_ops->update_fw_state(psoc, tdls_param);
	else
		return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_tdls_set_peer_state(struct wlan_objmgr_psoc *psoc,
				   struct tdls_peer_update_state *peer_param)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_ops = NULL;

	tdls_ops = wlan_psoc_get_tdls_txops(psoc);
	if (tdls_ops && tdls_ops->update_peer_state)
		return tdls_ops->update_peer_state(psoc, peer_param);
	else
		return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				     struct tdls_channel_switch_params *param)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_ops = NULL;

	tdls_ops = wlan_psoc_get_tdls_txops(psoc);
	if (tdls_ops && tdls_ops->set_offchan_mode)
		return tdls_ops->set_offchan_mode(psoc, param);
	else
		return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_tdls_send_mgmt_tx_completion(struct scheduler_msg *pmsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pmsg || !pmsg->bodyptr) {
		tdls_err("msg: 0x%pK", pmsg);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tdls_send_mgmt_tx_completion(pmsg->bodyptr);

	return status;
}

QDF_STATUS tgt_tdls_send_mgmt_rsp(struct scheduler_msg *pmsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pmsg || !pmsg->bodyptr) {
		tdls_err("msg: 0x%pK", pmsg);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tdls_process_send_mgmt_rsp(pmsg->bodyptr);

	return status;
}

QDF_STATUS tgt_tdls_add_peer_rsp(struct scheduler_msg *pmsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pmsg || !pmsg->bodyptr) {
		tdls_err("msg: 0x%pK", pmsg);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tdls_process_add_peer_rsp(pmsg->bodyptr);

	return status;
}

QDF_STATUS tgt_tdls_del_peer_rsp(struct scheduler_msg *pmsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pmsg || !pmsg->bodyptr) {
		tdls_err("msg: 0x%pK", pmsg);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = tdls_process_del_peer_rsp(pmsg->bodyptr);

	return status;
}

QDF_STATUS tgt_tdls_register_ev_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_ops = NULL;

	tdls_ops = wlan_psoc_get_tdls_txops(psoc);
	if (tdls_ops && tdls_ops->tdls_reg_ev_handler)
		return tdls_ops->tdls_reg_ev_handler(psoc, NULL);
	else
		return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_tdls_unregister_ev_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_ops = NULL;

	tdls_ops = wlan_psoc_get_tdls_txops(psoc);
	if (tdls_ops->tdls_unreg_ev_handler)
		return tdls_ops->tdls_unreg_ev_handler(psoc, NULL);
	else
		return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tgt_tdls_event_flush_cb(struct scheduler_msg *msg)
{
	struct tdls_event_notify *notify;

	notify = msg->bodyptr;
	if (notify && notify->vdev) {
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_SB_ID);
		qdf_mem_free(notify);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
tgt_tdls_event_handler(struct wlan_objmgr_psoc *psoc,
		       struct tdls_event_info *info)
{
	struct scheduler_msg msg = {0,};
	struct tdls_event_notify *notify;
	uint8_t vdev_id;
	QDF_STATUS status;

	if (!psoc || !info) {
		tdls_err("psoc: 0x%pK, info: 0x%pK", psoc, info);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("vdev: %d, type: %d, reason: %d" QDF_MAC_ADDR_FMT,
		   info->vdev_id, info->message_type, info->peer_reason,
		   QDF_MAC_ADDR_REF(info->peermac.bytes));
	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify)
		return QDF_STATUS_E_NOMEM;

	vdev_id = info->vdev_id;
	notify->vdev =
		wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						     vdev_id, WLAN_TDLS_SB_ID);
	if (!notify->vdev) {
		tdls_err("null vdev, vdev_id: %d, psoc: 0x%pK", vdev_id, psoc);
		return QDF_STATUS_E_INVAL;
	}
	qdf_mem_copy(&notify->event, info, sizeof(*info));

	msg.bodyptr = notify;
	msg.callback = tdls_process_evt;
	msg.flush_callback = tgt_tdls_event_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't post msg to handle tdls event");
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_SB_ID);
		qdf_mem_free(notify);
	}

	return status;
}

static QDF_STATUS tgt_tdls_mgmt_frame_rx_flush_cb(struct scheduler_msg *msg)
{
	struct tdls_rx_mgmt_event *rx_mgmt_event;

	rx_mgmt_event = msg->bodyptr;

	if (rx_mgmt_event) {
		if (rx_mgmt_event->rx_mgmt)
			qdf_mem_free(rx_mgmt_event->rx_mgmt);

		qdf_mem_free(rx_mgmt_event);
	}
	msg->bodyptr = NULL;

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS tgt_tdls_mgmt_frame_process_rx_cb(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_peer *peer,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			enum mgmt_frame_type frm_type)
{
	struct tdls_rx_mgmt_frame *rx_mgmt;
	struct tdls_rx_mgmt_event *rx_mgmt_event;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct scheduler_msg msg = {0};
	struct wlan_objmgr_vdev *vdev;
	uint32_t vdev_id;
	uint8_t *pdata;
	QDF_STATUS status;

	tdls_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
			WLAN_UMAC_COMP_TDLS);
	if (!tdls_soc_obj) {
		tdls_err("tdls ctx is NULL, drop this frame");
		return QDF_STATUS_E_FAILURE;
	}

	if (!peer) {
		vdev = tdls_get_vdev(psoc, WLAN_TDLS_SB_ID);
		if (!vdev) {
			tdls_err("current tdls vdev is null, can't get vdev id");
			return QDF_STATUS_E_FAILURE;
		}
		vdev_id = wlan_vdev_get_id(vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
	} else {
		vdev = wlan_peer_get_vdev(peer);
		if (!vdev) {
			tdls_err("vdev is NULL in peer, drop this frame");
			return QDF_STATUS_E_FAILURE;
		}
		vdev_id = wlan_vdev_get_id(vdev);
	}

	rx_mgmt_event = qdf_mem_malloc_atomic(sizeof(*rx_mgmt_event));
	if (!rx_mgmt_event)
		return QDF_STATUS_E_NOMEM;

	rx_mgmt = qdf_mem_malloc_atomic(sizeof(*rx_mgmt) +
			mgmt_rx_params->buf_len);
	if (!rx_mgmt) {
		tdls_debug_rl("Failed to allocate rx mgmt frame");
		qdf_mem_free(rx_mgmt_event);
		return QDF_STATUS_E_NOMEM;
	}

	pdata = (uint8_t *)qdf_nbuf_data(buf);
	rx_mgmt->frame_len = mgmt_rx_params->buf_len;
	rx_mgmt->rx_freq = mgmt_rx_params->chan_freq;
	rx_mgmt->vdev_id = vdev_id;
	rx_mgmt->frm_type = frm_type;
	rx_mgmt->rx_rssi = mgmt_rx_params->rssi;

	rx_mgmt_event->rx_mgmt = rx_mgmt;
	rx_mgmt_event->tdls_soc_obj = tdls_soc_obj;
	qdf_mem_copy(rx_mgmt->buf, pdata, mgmt_rx_params->buf_len);
	msg.type = TDLS_EVENT_RX_MGMT;
	msg.bodyptr = rx_mgmt_event;
	msg.callback = tdls_process_rx_frame;
	msg.flush_callback = tgt_tdls_mgmt_frame_rx_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(rx_mgmt);
		qdf_mem_free(rx_mgmt_event);
	}

	qdf_nbuf_free(buf);

	return status;
}

QDF_STATUS tgt_tdls_mgmt_frame_rx_cb(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_peer *peer,
			qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			enum mgmt_frame_type frm_type)
{
	QDF_STATUS status;

	tdls_debug("psoc:%pK, peer:%pK, type:%d", psoc, peer, frm_type);


	if (!buf) {
		tdls_err("rx frame buff is null buf:%pK", buf);
		return QDF_STATUS_E_INVAL;
	}

	if (!mgmt_rx_params || !psoc) {
		tdls_err("input is NULL mgmt_rx_params:%pK psoc:%pK, peer:%pK",
			  mgmt_rx_params, psoc, peer);
		status = QDF_STATUS_E_INVAL;
		goto release_nbuf;
	}

	status = wlan_objmgr_peer_try_get_ref(peer, WLAN_TDLS_SB_ID);
	if (QDF_STATUS_SUCCESS != status)
		goto release_nbuf;

	status = tgt_tdls_mgmt_frame_process_rx_cb(psoc, peer, buf,
						   mgmt_rx_params, frm_type);

	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_SB_ID);

	if (QDF_STATUS_SUCCESS != status)
release_nbuf:
		qdf_nbuf_free(buf);
	return status;
}

void tgt_tdls_peers_deleted_notification(struct wlan_objmgr_psoc *psoc,
					 uint32_t session_id)
{
	tdls_peers_deleted_notification(psoc, session_id);
}

void tgt_tdls_delete_all_peers_indication(struct wlan_objmgr_psoc *psoc,
					  uint32_t session_id)
{

	tdls_delete_all_peers_indication(psoc, session_id);
}
