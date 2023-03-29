/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains p2p south bound interface definitions
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <scheduler_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_p2p_tgt_api.h"
#include "wlan_p2p_public_struct.h"
#include "../../core/src/wlan_p2p_main.h"
#include "../../core/src/wlan_p2p_roc.h"
#include "../../core/src/wlan_p2p_off_chan_tx.h"

#define IEEE80211_FC0_TYPE_MASK              0x0c
#define P2P_NOISE_FLOOR_DBM_DEFAULT          (-96)

static inline struct wlan_lmac_if_p2p_tx_ops *
wlan_psoc_get_p2p_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &(psoc->soc_cb.tx_ops->p2p);
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
QDF_STATUS tgt_p2p_register_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_ops = wlan_psoc_get_p2p_tx_ops(psoc);
	if (p2p_ops && p2p_ops->reg_lo_ev_handler) {
		status = p2p_ops->reg_lo_ev_handler(psoc, NULL);
		p2p_debug("register lo event, status:%d", status);
	}

	return status;
}

QDF_STATUS tgt_p2p_unregister_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_ops = wlan_psoc_get_p2p_tx_ops(psoc);
	if (p2p_ops && p2p_ops->unreg_lo_ev_handler) {
		status = p2p_ops->unreg_lo_ev_handler(psoc, NULL);
		p2p_debug("unregister lo event, status:%d", status);
	}

	return status;
}

QDF_STATUS tgt_p2p_lo_event_cb(struct wlan_objmgr_psoc *psoc,
			       struct p2p_lo_event *event_info)
{
	struct p2p_lo_stop_event *lo_stop_event;
	struct scheduler_msg msg = {0};
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	p2p_debug("soc:%pK, event_info:%pK", psoc, event_info);

	if (!psoc) {
		p2p_err("psoc context passed is NULL");
		if (event_info)
			qdf_mem_free(event_info);
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
						WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc object is NULL");
		if (event_info)
			qdf_mem_free(event_info);
		return QDF_STATUS_E_INVAL;
	}

	if (!event_info) {
		p2p_err("invalid lo stop event information");
		return QDF_STATUS_E_INVAL;
	}

	lo_stop_event = qdf_mem_malloc(sizeof(*lo_stop_event));
	if (!lo_stop_event) {
		qdf_mem_free(event_info);
		return QDF_STATUS_E_NOMEM;
	}

	lo_stop_event->p2p_soc_obj = p2p_soc_obj;
	lo_stop_event->lo_event = event_info;
	msg.type = P2P_EVENT_LO_STOPPED;
	msg.bodyptr = lo_stop_event;
	msg.callback = p2p_process_evt;
	msg.flush_callback = p2p_event_flush_callback;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_TARGET_IF,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(lo_stop_event->lo_event);
		qdf_mem_free(lo_stop_event);
		p2p_err("post msg fail:%d", status);
	}

	return status;
}
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */

QDF_STATUS
tgt_p2p_add_mac_addr_status_event_cb(struct wlan_objmgr_psoc *psoc,
				     struct p2p_set_mac_filter_evt *event_info)
{
	struct p2p_mac_filter_rsp *mac_filter_rsp;
	struct scheduler_msg msg = {0};
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	if (!psoc) {
		p2p_err("random_mac:psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}
	if (!event_info) {
		p2p_err("random_mac:invalid event_info");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(
				psoc, WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("random_mac:p2p soc object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac_filter_rsp = qdf_mem_malloc(sizeof(*mac_filter_rsp));
	if (!mac_filter_rsp)
		return QDF_STATUS_E_NOMEM;

	mac_filter_rsp->p2p_soc_obj = p2p_soc_obj;
	mac_filter_rsp->vdev_id = event_info->vdev_id;
	mac_filter_rsp->status = event_info->status;

	msg.type = P2P_EVENT_ADD_MAC_RSP;
	msg.bodyptr = mac_filter_rsp;
	msg.callback = p2p_process_evt;
	status = scheduler_post_message(QDF_MODULE_ID_P2P, QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(mac_filter_rsp);

	return status;
}

QDF_STATUS tgt_p2p_register_macaddr_rx_filter_evt_handler(
	struct wlan_objmgr_psoc *psoc, bool reg)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_ops = wlan_psoc_get_p2p_tx_ops(psoc);
	if (p2p_ops && p2p_ops->reg_mac_addr_rx_filter_handler) {
		status = p2p_ops->reg_mac_addr_rx_filter_handler(psoc, reg);
		p2p_debug("register mac addr rx filter event,  register %d status:%d",
			  reg, status);
	}

	return status;
}

QDF_STATUS tgt_p2p_register_noa_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_ops = wlan_psoc_get_p2p_tx_ops(psoc);
	if (p2p_ops && p2p_ops->reg_noa_ev_handler) {
		status = p2p_ops->reg_noa_ev_handler(psoc, NULL);
		p2p_debug("register noa event, status:%d", status);
	}

	return status;
}

QDF_STATUS tgt_p2p_unregister_noa_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_ops = wlan_psoc_get_p2p_tx_ops(psoc);
	if (p2p_ops && p2p_ops->unreg_noa_ev_handler) {
		status = p2p_ops->unreg_noa_ev_handler(psoc, NULL);
		p2p_debug("unregister noa event, status:%d", status);
	}

	return status;
}

void tgt_p2p_scan_event_cb(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event, void *arg)
{
	p2p_scan_event_cb(vdev, event, arg);
}

QDF_STATUS tgt_p2p_mgmt_download_comp_cb(void *context,
	qdf_nbuf_t buf, bool free)
{
	p2p_debug("conext:%pK, buf:%pK, free:%d", context,
		qdf_nbuf_data(buf), free);

	qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_p2p_mgmt_ota_comp_cb(void *context, qdf_nbuf_t buf,
	uint32_t status, void *tx_compl_params)
{
	struct p2p_tx_conf_event *tx_conf_event;
	struct scheduler_msg msg = {0};
	QDF_STATUS ret;

	p2p_debug("context:%pK, buf:%pK, status:%d, tx complete params:%pK",
		context, buf, status, tx_compl_params);

	if (!context) {
		p2p_err("invalid context");
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_INVAL;
	}

	tx_conf_event = qdf_mem_malloc(sizeof(*tx_conf_event));
	if (!tx_conf_event) {
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_NOMEM;
	}

	tx_conf_event->status = status;
	tx_conf_event->nbuf = buf;
	tx_conf_event->p2p_soc_obj = (struct p2p_soc_priv_obj *)context;
	msg.type = P2P_EVENT_MGMT_TX_ACK_CNF;
	msg.bodyptr = tx_conf_event;
	msg.callback = p2p_process_evt;
	msg.flush_callback = p2p_event_flush_callback;
	ret = scheduler_post_message(QDF_MODULE_ID_P2P,
				     QDF_MODULE_ID_P2P,
				     QDF_MODULE_ID_TARGET_IF,
				     &msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(tx_conf_event);
		qdf_nbuf_free(buf);
		p2p_err("post msg fail:%d", status);
	}

	return ret;
}

QDF_STATUS tgt_p2p_mgmt_frame_rx_cb(struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_peer *peer, qdf_nbuf_t buf,
	struct mgmt_rx_event_params *mgmt_rx_params,
	enum mgmt_frame_type frm_type)
{
	struct p2p_rx_mgmt_frame *rx_mgmt;
	struct p2p_rx_mgmt_event *rx_mgmt_event;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct scheduler_msg msg = {0};
	struct wlan_objmgr_vdev *vdev;
	uint32_t vdev_id;
	uint8_t *pdata;
	QDF_STATUS status;

	p2p_debug("psoc:%pK, peer:%pK, type:%d", psoc, peer, frm_type);

	if (!mgmt_rx_params) {
		p2p_err("mgmt rx params is NULL");
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p ctx is NULL, drop this frame");
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	if (!peer) {
		if (p2p_soc_obj->cur_roc_vdev_id == P2P_INVALID_VDEV_ID) {
			p2p_debug("vdev id of current roc invalid");
			qdf_nbuf_free(buf);
			return QDF_STATUS_E_FAILURE;
		} else {
			vdev_id = p2p_soc_obj->cur_roc_vdev_id;
		}
	} else {
		vdev = wlan_peer_get_vdev(peer);
		if (!vdev) {
			p2p_err("vdev is NULL in peer, drop this frame");
			qdf_nbuf_free(buf);
			return QDF_STATUS_E_FAILURE;
		}
		vdev_id = wlan_vdev_get_id(vdev);
	}

	rx_mgmt_event = qdf_mem_malloc_atomic(sizeof(*rx_mgmt_event));
	if (!rx_mgmt_event) {
		p2p_debug_rl("Failed to allocate rx mgmt event");
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_NOMEM;
	}

	rx_mgmt = qdf_mem_malloc_atomic(sizeof(*rx_mgmt) +
			mgmt_rx_params->buf_len);
	if (!rx_mgmt) {
		p2p_debug_rl("Failed to allocate rx mgmt frame");
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_NOMEM;
	}

	pdata = (uint8_t *)qdf_nbuf_data(buf);
	rx_mgmt->frame_len = mgmt_rx_params->buf_len;
	rx_mgmt->rx_freq = mgmt_rx_params->chan_freq;
	rx_mgmt->vdev_id = vdev_id;
	rx_mgmt->frm_type = frm_type;
	rx_mgmt->rx_rssi = mgmt_rx_params->snr +
				P2P_NOISE_FLOOR_DBM_DEFAULT;
	rx_mgmt_event->rx_mgmt = rx_mgmt;
	rx_mgmt_event->p2p_soc_obj = p2p_soc_obj;
	qdf_mem_copy(rx_mgmt->buf, pdata, mgmt_rx_params->buf_len);
	msg.type = P2P_EVENT_RX_MGMT;
	msg.bodyptr = rx_mgmt_event;
	msg.callback = p2p_process_evt;
	msg.flush_callback = p2p_event_flush_callback;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_TARGET_IF,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(rx_mgmt_event->rx_mgmt);
		qdf_mem_free(rx_mgmt_event);
		p2p_err("post msg fail:%d", status);
	}
	qdf_nbuf_free(buf);

	return status;
}

QDF_STATUS  tgt_p2p_noa_event_cb(struct wlan_objmgr_psoc *psoc,
		struct p2p_noa_info *event_info)
{
	struct p2p_noa_event *noa_event;
	struct scheduler_msg msg = {0};
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	p2p_debug("soc:%pK, event_info:%pK", psoc, event_info);

	if (!psoc) {
		p2p_err("psoc context passed is NULL");
		if (event_info)
			qdf_mem_free(event_info);
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc object is NULL");
		if (event_info)
			qdf_mem_free(event_info);
		return QDF_STATUS_E_INVAL;
	}

	if (!event_info) {
		p2p_err("invalid noa event information");
		return QDF_STATUS_E_INVAL;
	}

	noa_event = qdf_mem_malloc(sizeof(*noa_event));
	if (!noa_event) {
		qdf_mem_free(event_info);
		return QDF_STATUS_E_NOMEM;
	}

	noa_event->p2p_soc_obj = p2p_soc_obj;
	noa_event->noa_info = event_info;
	msg.type = P2P_EVENT_NOA;
	msg.bodyptr = noa_event;
	msg.callback = p2p_process_evt;
	msg.flush_callback = p2p_event_flush_callback;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_TARGET_IF,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(noa_event->noa_info);
		qdf_mem_free(noa_event);
		p2p_err("post msg fail:%d", status);
	}

	return status;
}
