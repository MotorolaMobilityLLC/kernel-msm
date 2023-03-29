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
 * DOC: This file contains off channel tx API definitions
 */

#include <wmi_unified_api.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_utility.h>
#include <scheduler_api.h>
#include "wlan_p2p_public_struct.h"
#include "wlan_p2p_tgt_api.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_p2p_roc.h"
#include "wlan_p2p_main.h"
#include "wlan_p2p_off_chan_tx.h"
#include "wlan_osif_request_manager.h"
#include <wlan_mlme_main.h>
#include "wlan_mlme_api.h"

/**
 * p2p_psoc_get_tx_ops() - get p2p tx ops
 * @psoc:        psoc object
 *
 * This function returns p2p tx ops callbacks.
 *
 * Return: wlan_lmac_if_p2p_tx_ops
 */
static inline struct wlan_lmac_if_p2p_tx_ops *
p2p_psoc_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &psoc->soc_cb.tx_ops->p2p;
}

/**
 * p2p_tx_context_check_valid() - check tx action context
 * @tx_ctx:         tx context
 *
 * This function check if tx action context and parameters are valid.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_tx_context_check_valid(struct tx_action_context *tx_ctx)
{
	struct wlan_objmgr_psoc *psoc;
	struct p2p_soc_priv_obj *p2p_soc_obj;

	if (!tx_ctx) {
		p2p_err("null tx action context");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	if (!p2p_soc_obj) {
		p2p_err("null p2p soc private object");
		return QDF_STATUS_E_INVAL;
	}

	psoc = p2p_soc_obj->soc;
	if (!psoc) {
		p2p_err("null p2p soc object");
		return QDF_STATUS_E_INVAL;
	}

	if (!tx_ctx->buf) {
		p2p_err("null tx buffer");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_vdev_check_valid() - check vdev and vdev mode
 * @tx_ctx:         tx context
 *
 * This function check if vdev and vdev mode are valid. It will drop
 * probe response in sta mode.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
#ifdef SUPPORT_P2P_BY_ONE_INTF_WLAN
static QDF_STATUS p2p_vdev_check_valid(struct tx_action_context *tx_ctx)
{
	enum QDF_OPMODE mode;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	psoc = p2p_soc_obj->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			psoc, tx_ctx->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("null vdev object");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);

	/* drop probe response/disassoc/deauth for go, sap */
	if ((mode == QDF_SAP_MODE ||
	     mode == QDF_P2P_GO_MODE) &&
	    ((tx_ctx->frame_info.sub_type == P2P_MGMT_PROBE_RSP) ||
	     (tx_ctx->frame_info.sub_type == P2P_MGMT_DISASSOC) ||
	     (tx_ctx->frame_info.sub_type == P2P_MGMT_DEAUTH))) {
		p2p_debug("drop frame, mode:%d, sub type:%d", mode,
			  tx_ctx->frame_info.sub_type);
		status = QDF_STATUS_E_FAILURE;
	}

	/* drop action frame for sap */
	if ((mode == QDF_SAP_MODE) &&
	    (tx_ctx->frame_info.sub_type == P2P_MGMT_ACTION) &&
	    (tx_ctx->frame_info.public_action_type ==
	     P2P_PUBLIC_ACTION_NOT_SUPPORT) &&
	    (tx_ctx->frame_info.action_type == P2P_ACTION_NOT_SUPPORT)) {
		p2p_debug("drop action frame for SAP");
		status = QDF_STATUS_E_FAILURE;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return status;
}
#else
static QDF_STATUS p2p_vdev_check_valid(struct tx_action_context *tx_ctx)
{
	enum QDF_OPMODE mode;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	psoc = p2p_soc_obj->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			psoc, tx_ctx->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("null vdev object");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);

	/* drop probe response/disassoc/deauth for sta, go, sap */
	if ((mode == QDF_STA_MODE &&
	     tx_ctx->frame_info.sub_type == P2P_MGMT_PROBE_RSP) ||
	    ((mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE) &&
	     ((tx_ctx->frame_info.sub_type == P2P_MGMT_PROBE_RSP) ||
	     (tx_ctx->frame_info.sub_type == P2P_MGMT_DISASSOC) ||
	     (tx_ctx->frame_info.sub_type == P2P_MGMT_DEAUTH)))) {
		p2p_debug("drop frame, mode:%d, sub type:%d", mode,
			  tx_ctx->frame_info.sub_type);
		status = QDF_STATUS_E_FAILURE;
	}

	/* drop ation frame for sap */
	if ((mode == QDF_SAP_MODE) &&
	    (tx_ctx->frame_info.sub_type == P2P_MGMT_ACTION) &&
	    (tx_ctx->frame_info.public_action_type ==
	     P2P_PUBLIC_ACTION_NOT_SUPPORT) &&
	    (tx_ctx->frame_info.action_type == P2P_ACTION_NOT_SUPPORT)) {
		p2p_debug("drop action frame for SAP");
		status = QDF_STATUS_E_FAILURE;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return status;
}
#endif /* SUPPORT_P2P_BY_ONE_INTF_WLAN */

/**
 * p2p_check_and_update_channel() - check and update tx channel
 * @tx_ctx:         tx context
 *
 * This function checks and updates tx channel if channel is 0 in tx context.
 * It will update channel to current roc channel if vdev mode is
 * P2P DEVICE/CLIENT/GO.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_check_and_update_channel(struct tx_action_context *tx_ctx)
{
	enum QDF_OPMODE mode;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *curr_roc_ctx;

	if (!tx_ctx || tx_ctx->chan) {
		p2p_err("NULL tx ctx or channel valid");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	psoc = p2p_soc_obj->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			psoc, tx_ctx->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("null vdev object");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	curr_roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);

	if (curr_roc_ctx &&
	    (mode == QDF_P2P_DEVICE_MODE ||
	     mode == QDF_P2P_CLIENT_MODE ||
	     mode == QDF_P2P_GO_MODE))
		tx_ctx->chan = curr_roc_ctx->chan;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_get_p2pie_ptr() - get the pointer to p2p ie
 * @ie:      source ie
 * @ie_len:  source ie length
 *
 * This function finds out p2p ie by p2p oui and return the pointer.
 *
 * Return: pointer to p2p ie
 */
const uint8_t *p2p_get_p2pie_ptr(const uint8_t *ie, uint16_t ie_len)
{
	return wlan_get_vendor_ie_ptr_from_oui(P2P_OUI,
			P2P_OUI_SIZE, ie, ie_len);
}

/**
 * p2p_get_p2pie_from_probe_rsp() - get the pointer to p2p ie from
 * probe response
 * @tx_ctx:      tx context
 *
 * This function finds out p2p ie and return the pointer if it is a
 * probe response frame.
 *
 * Return: pointer to p2p ie
 */
static const uint8_t *p2p_get_p2pie_from_probe_rsp(
	struct tx_action_context *tx_ctx)
{
	const uint8_t *ie;
	const uint8_t *p2p_ie;
	const uint8_t *tmp_p2p_ie = NULL;
	uint16_t ie_len;

	if (tx_ctx->buf_len <= PROBE_RSP_IE_OFFSET) {
		p2p_err("Invalid header len for probe response");
		return NULL;
	}

	ie = tx_ctx->buf + PROBE_RSP_IE_OFFSET;
	ie_len = tx_ctx->buf_len - PROBE_RSP_IE_OFFSET;
	p2p_ie = p2p_get_p2pie_ptr(ie, ie_len);
	while ((p2p_ie) &&
		(WLAN_MAX_IE_LEN == p2p_ie[1])) {
		ie_len = tx_ctx->buf_len - (p2p_ie - tx_ctx->buf);
		if (ie_len > 2) {
			ie = p2p_ie + WLAN_MAX_IE_LEN + 2;
			tmp_p2p_ie = p2p_get_p2pie_ptr(ie, ie_len);
		}

		if (tmp_p2p_ie) {
			p2p_ie = tmp_p2p_ie;
			tmp_p2p_ie = NULL;
		} else {
			break;
		}
	}

	return p2p_ie;
}

/**
 * p2p_get_presence_noa_attr() - get the pointer to noa attr
 * @pies:      source ie
 * @length:    source ie length
 *
 * This function finds out noa attr by noa eid and return the pointer.
 *
 * Return: pointer to noa attr
 */
static const uint8_t *p2p_get_presence_noa_attr(const uint8_t *pies, int length)
{
	int left = length;
	const uint8_t *ptr = pies;
	uint8_t elem_id;
	uint16_t elem_len;

	p2p_debug("pies:%pK, length:%d", pies, length);

	while (left >= 3) {
		elem_id = ptr[0];
		elem_len = ((uint16_t) ptr[1]) | (ptr[2] << 8);

		left -= 3;
		if (elem_len > left) {
			p2p_err("****Invalid IEs, elem_len=%d left=%d*****",
				elem_len, left);
			return NULL;
		}
		if (elem_id == P2P_NOA_ATTR)
			return ptr;

		left -= elem_len;
		ptr += (elem_len + 3);
	}

	return NULL;
}

/**
 * p2p_get_noa_attr_stream_in_mult_p2p_ies() - get the pointer to noa
 * attr from multi p2p ie
 * @noa_stream:      noa stream
 * @noa_len:         noa stream length
 * @overflow_len:    overflow length
 *
 * This function finds out noa attr from multi p2p ies.
 *
 * Return: noa length
 */
static uint8_t p2p_get_noa_attr_stream_in_mult_p2p_ies(uint8_t *noa_stream,
	uint8_t noa_len, uint8_t overflow_len)
{
	uint8_t overflow_p2p_stream[P2P_MAX_NOA_ATTR_LEN];

	p2p_debug("noa_stream:%pK, noa_len:%d, overflow_len:%d",
		noa_stream, noa_len, overflow_len);
	if ((noa_len <= (P2P_MAX_NOA_ATTR_LEN + P2P_IE_HEADER_LEN)) &&
	    (noa_len >= overflow_len) &&
	    (overflow_len <= P2P_MAX_NOA_ATTR_LEN)) {
		qdf_mem_copy(overflow_p2p_stream,
			     noa_stream + noa_len - overflow_len,
			     overflow_len);
		noa_stream[noa_len - overflow_len] =
			P2P_EID_VENDOR;
		noa_stream[noa_len - overflow_len + 1] =
			overflow_len + P2P_OUI_SIZE;
		qdf_mem_copy(noa_stream + noa_len - overflow_len + 2,
			P2P_OUI, P2P_OUI_SIZE);
		qdf_mem_copy(noa_stream + noa_len + 2 + P2P_OUI_SIZE -
			overflow_len, overflow_p2p_stream,
			overflow_len);
	}

	return noa_len + P2P_IE_HEADER_LEN;
}

/**
 * p2p_get_vdev_noa_info() - get vdev noa information
 * @tx_ctx:         tx context
 *
 * This function gets vdev noa information
 *
 * Return: pointer to noa information
 */
static struct p2p_noa_info *p2p_get_vdev_noa_info(
	struct tx_action_context *tx_ctx)
{
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	enum QDF_OPMODE mode;
	struct p2p_noa_info *noa_info = NULL;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	psoc = p2p_soc_obj->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
			tx_ctx->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("vdev obj is NULL");
		return NULL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);
	if (mode != QDF_P2P_GO_MODE) {
		p2p_debug("invalid p2p vdev mode:%d", mode);
		goto fail;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
			WLAN_UMAC_COMP_P2P);

	if (!p2p_vdev_obj || !(p2p_vdev_obj->noa_info)) {
		p2p_debug("null noa info");
		goto fail;
	}

	noa_info = p2p_vdev_obj->noa_info;

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return noa_info;
}

/**
 * p2p_get_noa_attr_stream() - get noa stream from p2p vdev object
 * @tx_ctx:         tx context
 * @pnoa_stream:    pointer to noa stream
 *
 * This function finds out noa stream from p2p vdev object
 *
 * Return: noa stream length
 */
static uint8_t p2p_get_noa_attr_stream(
	struct tx_action_context *tx_ctx, uint8_t *pnoa_stream)
{
	struct p2p_noa_info *noa_info;
	struct noa_descriptor *noa_desc_0;
	struct noa_descriptor *noa_desc_1;
	uint8_t *pbody = pnoa_stream;
	uint8_t len = 0;

	noa_info = p2p_get_vdev_noa_info(tx_ctx);
	if (!noa_info) {
		p2p_debug("not valid noa information");
		return 0;
	}

	noa_desc_0 = &(noa_info->noa_desc[0]);
	noa_desc_1 = &(noa_info->noa_desc[1]);
	if ((!(noa_desc_0->duration)) &&
		(!(noa_desc_1->duration)) &&
		(!noa_info->opps_ps)) {
		p2p_debug("opps ps and duration are 0");
		return 0;
	}

	pbody[0] = P2P_NOA_ATTR;
	pbody[3] = noa_info->index;
	pbody[4] = noa_info->ct_window | (noa_info->opps_ps << 7);
	len = 5;
	pbody += len;

	if (noa_desc_0->duration) {
		*pbody = noa_desc_0->type_count;
		pbody += 1;
		len += 1;

		*((uint32_t *) (pbody)) = noa_desc_0->duration;
		pbody += sizeof(uint32_t);
		len += 4;

		*((uint32_t *) (pbody)) = noa_desc_0->interval;
		pbody += sizeof(uint32_t);
		len += 4;

		*((uint32_t *) (pbody)) = noa_desc_0->start_time;
		pbody += sizeof(uint32_t);
		len += 4;
	}

	if (noa_desc_1->duration) {
		*pbody = noa_desc_1->type_count;
		pbody += 1;
		len += 1;

		*((uint32_t *) (pbody)) = noa_desc_1->duration;
		pbody += sizeof(uint32_t);
		len += 4;

		*((uint32_t *) (pbody)) = noa_desc_1->interval;
		pbody += sizeof(uint32_t);
		len += 4;

		*((uint32_t *) (pbody)) = noa_desc_1->start_time;
		pbody += sizeof(uint32_t);
		len += 4;
	}

	pbody = pnoa_stream + 1;
	 /* one byte for Attr and 2 bytes for length */
	*((uint16_t *) (pbody)) = len - 3;

	return len;
}

/**
 * p2p_update_noa_stream() - update noa stream
 * @tx_ctx:       tx context
 * @p2p_ie:       pointer to p2p ie
 * @noa_attr:     pointer to noa attr
 * @total_len:    pointer to total length of ie
 *
 * This function updates noa stream.
 *
 * Return: noa stream length
 */
static uint16_t p2p_update_noa_stream(struct tx_action_context *tx_ctx,
	uint8_t *p2p_ie, const uint8_t *noa_attr, uint32_t *total_len,
	uint8_t *noa_stream)
{
	uint16_t noa_len;
	uint16_t overflow_len;
	uint8_t orig_len;
	uint32_t nbytes_copy;
	uint32_t buf_len = *total_len;

	noa_len = p2p_get_noa_attr_stream(tx_ctx, noa_stream);
	if (noa_len <= 0) {
		p2p_debug("do not find out noa attr");
		return 0;
	}

	orig_len = p2p_ie[1];
	if (noa_attr) {
		noa_len = noa_attr[1] | (noa_attr[2] << 8);
		orig_len -= (noa_len + 1 + 2);
		buf_len -= (noa_len + 1 + 2);
		p2p_ie[1] = orig_len;
	}

	if ((p2p_ie[1] + noa_len) > WLAN_MAX_IE_LEN) {
		overflow_len = p2p_ie[1] + noa_len -
				WLAN_MAX_IE_LEN;
		noa_len = p2p_get_noa_attr_stream_in_mult_p2p_ies(
				noa_stream, noa_len, overflow_len);
		p2p_ie[1] = WLAN_MAX_IE_LEN;
	} else {
		/* increment the length of P2P IE */
		p2p_ie[1] += noa_len;
	}

	*total_len = buf_len;
	nbytes_copy = (p2p_ie + orig_len + 2) - tx_ctx->buf;

	p2p_debug("noa_len=%d orig_len=%d p2p_ie=%pK buf_len=%d nbytes copy=%d ",
		noa_len, orig_len, p2p_ie, buf_len, nbytes_copy);

	return noa_len;
}

/**
 * p2p_set_ht_caps() - set ht capability
 * @tx_ctx:         tx context
 * @num_bytes:      number bytes
 *
 * This function sets ht capability
 *
 * Return: None
 */
static void p2p_set_ht_caps(struct tx_action_context *tx_ctx,
	uint32_t num_bytes)
{
}

/**
 * p2p_populate_mac_header() - update sequence number
 * @tx_ctx:      tx context
 *
 * This function updates sequence number of this mgmt frame
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_populate_mac_header(
	struct tx_action_context *tx_ctx)
{
	struct wlan_seq_ctl *seq_ctl;
	struct wlan_frame_hdr *wh;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc;
	void *mac_addr;
	uint16_t seq_num;
	uint8_t pdev_id;
	struct wlan_objmgr_vdev *vdev;

	psoc = tx_ctx->p2p_soc_obj->soc;

	wh = (struct wlan_frame_hdr *)tx_ctx->buf;
	/*
	 * Remove the WEP bit if already set, p2p_populate_rmf_field will set it
	 * if required.
	 */
	wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
	mac_addr = wh->i_addr1;
	pdev_id = wlan_get_pdev_id_from_vdev_id(psoc, tx_ctx->vdev_id,
						WLAN_P2P_ID);
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr, WLAN_P2P_ID);
	if (!peer) {
		mac_addr = wh->i_addr2;
		peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
					    WLAN_P2P_ID);
	}
	if (!peer && tx_ctx->rand_mac_tx) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							    tx_ctx->vdev_id,
							    WLAN_P2P_ID);
		if (vdev) {
			mac_addr = wlan_vdev_mlme_get_macaddr(vdev);
			peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
						    WLAN_P2P_ID);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		}
	}
	if (!peer) {
		p2p_err("no valid peer");
		return QDF_STATUS_E_INVAL;
	}
	seq_num = (uint16_t)wlan_peer_mlme_get_next_seq_num(peer);
	seq_ctl = (struct wlan_seq_ctl *)(tx_ctx->buf +
			WLAN_SEQ_CTL_OFFSET);
	seq_ctl->seq_num_lo = (seq_num & WLAN_LOW_SEQ_NUM_MASK);
	seq_ctl->seq_num_hi = ((seq_num & WLAN_HIGH_SEQ_NUM_MASK) >>
				WLAN_HIGH_SEQ_NUM_OFFSET);
	p2p_debug("seq num: %d", seq_num);

	wlan_objmgr_peer_release_ref(peer, WLAN_P2P_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_get_frame_type_str() - parse frame type to string
 * @frame_info: frame information
 *
 * This function parse frame type to string.
 *
 * Return: command string
 */
static char *p2p_get_frame_type_str(struct p2p_frame_info *frame_info)
{
	if (frame_info->type == P2P_FRAME_NOT_SUPPORT)
		return "Not support frame";

	if (frame_info->sub_type == P2P_MGMT_NOT_SUPPORT)
		return "Not support sub frame";

	if (frame_info->action_type == P2P_ACTION_PRESENCE_REQ)
		return "P2P action presence request";
	if (frame_info->action_type == P2P_ACTION_PRESENCE_RSP)
		return "P2P action presence response";

	switch (frame_info->public_action_type) {
	case P2P_PUBLIC_ACTION_NEG_REQ:
		return "GO negotiation request frame";
	case P2P_PUBLIC_ACTION_NEG_RSP:
		return "GO negotiation response frame";
	case P2P_PUBLIC_ACTION_NEG_CNF:
		return "GO negotiation confirm frame";
	case P2P_PUBLIC_ACTION_INVIT_REQ:
		return "P2P invitation request";
	case P2P_PUBLIC_ACTION_INVIT_RSP:
		return "P2P invitation response";
	case P2P_PUBLIC_ACTION_DEV_DIS_REQ:
		return "Device discoverability request";
	case P2P_PUBLIC_ACTION_DEV_DIS_RSP:
		return "Device discoverability response";
	case P2P_PUBLIC_ACTION_PROV_DIS_REQ:
		return "Provision discovery request";
	case P2P_PUBLIC_ACTION_PROV_DIS_RSP:
		return "Provision discovery response";
	case P2P_PUBLIC_ACTION_GAS_INIT_REQ:
		return "GAS init request";
	case P2P_PUBLIC_ACTION_GAS_INIT_RSP:
		return "GAS init response";
	case P2P_PUBLIC_ACTION_GAS_COMB_REQ:
		return "GAS come back request";
	case P2P_PUBLIC_ACTION_GAS_COMB_RSP:
		return "GAS come back response";
	default:
		return "Other frame";
	}
}

/**
 * p2p_init_frame_info() - init frame information structure
 * @frame_info:     pointer to frame information
 *
 * This function init frame information structure.
 *
 * Return: None
 */
static void p2p_init_frame_info(struct p2p_frame_info *frame_info)
{
	frame_info->type = P2P_FRAME_NOT_SUPPORT;
	frame_info->sub_type = P2P_MGMT_NOT_SUPPORT;
	frame_info->public_action_type =
				P2P_PUBLIC_ACTION_NOT_SUPPORT;
	frame_info->action_type = P2P_ACTION_NOT_SUPPORT;
}

/**
 * p2p_get_frame_info() - get frame information from packet
 * @data_buf:          data buffer address
 * @length:            buffer length
 * @frame_info:        frame information
 *
 * This function gets frame information from packet.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_get_frame_info(uint8_t *data_buf, uint32_t length,
	struct p2p_frame_info *frame_info)
{
	uint8_t type;
	uint8_t sub_type;
	uint8_t action_type;
	uint8_t *buf = data_buf;

	p2p_init_frame_info(frame_info);

	if (length < P2P_ACTION_OFFSET + 1) {
		p2p_err("invalid p2p mgmt hdr len");
		return QDF_STATUS_E_INVAL;
	}

	type = P2P_GET_TYPE_FRM_FC(buf[0]);
	sub_type = P2P_GET_SUBTYPE_FRM_FC(buf[0]);
	if (type != P2P_FRAME_MGMT) {
		p2p_err("just support mgmt frame");
		return QDF_STATUS_E_FAILURE;
	}

	frame_info->type = type;
	frame_info->sub_type = sub_type;

	if (sub_type != P2P_MGMT_ACTION)
		return QDF_STATUS_SUCCESS;

	buf += P2P_ACTION_OFFSET;
	if (length > P2P_PUBLIC_ACTION_FRAME_TYPE_OFFSET &&
	    buf[0] == P2P_PUBLIC_ACTION_FRAME &&
	    buf[1] == P2P_PUBLIC_ACTION_VENDOR_SPECIFIC &&
	    !qdf_mem_cmp(&buf[2], P2P_OUI, P2P_OUI_SIZE)) {
		buf = data_buf +
			P2P_PUBLIC_ACTION_FRAME_TYPE_OFFSET;
		action_type = buf[0];
		if (action_type > P2P_PUBLIC_ACTION_PROV_DIS_RSP)
			frame_info->public_action_type =
				P2P_PUBLIC_ACTION_NOT_SUPPORT;
		else
			frame_info->public_action_type = action_type;
	} else if (length > P2P_ACTION_FRAME_TYPE_OFFSET &&
		   buf[0] == P2P_ACTION_VENDOR_SPECIFIC_CATEGORY &&
		   !qdf_mem_cmp(&buf[1], P2P_OUI, P2P_OUI_SIZE)) {
		buf = data_buf +
			P2P_ACTION_FRAME_TYPE_OFFSET;
		action_type = buf[0];
		if (action_type == P2P_ACTION_PRESENCE_REQ)
			frame_info->action_type =
				P2P_ACTION_PRESENCE_REQ;
		if (action_type == P2P_ACTION_PRESENCE_RSP)
			frame_info->action_type =
				P2P_ACTION_PRESENCE_RSP;
	} else {
		p2p_debug("this is not vendor specific p2p action frame");
		return QDF_STATUS_SUCCESS;
	}

	p2p_debug("%s", p2p_get_frame_type_str(frame_info));

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_P2P_DEBUG
/**
 * p2p_tx_update_connection_status() - Update P2P connection status
 * with tx frame
 * @p2p_soc_obj:        P2P soc private object
 * @tx_frame_info:      frame information
 * @mac_to:              Pointer to dest MAC address
 *
 * This function updates P2P connection status with tx frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_tx_update_connection_status(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct p2p_frame_info *tx_frame_info,
	uint8_t *mac_to)
{
	if (!p2p_soc_obj || !tx_frame_info || !mac_to) {
		p2p_err("invalid p2p_soc_obj:%pK or tx_frame_info:%pK or mac_to:%pK",
			p2p_soc_obj, tx_frame_info, mac_to);
		return QDF_STATUS_E_INVAL;
	}

	if (tx_frame_info->public_action_type !=
		P2P_PUBLIC_ACTION_NOT_SUPPORT)
		p2p_debug("%s ---> OTA to " QDF_MAC_ADDR_FMT,
			  p2p_get_frame_type_str(tx_frame_info),
			  QDF_MAC_ADDR_REF(mac_to));

	if ((tx_frame_info->public_action_type ==
	     P2P_PUBLIC_ACTION_PROV_DIS_REQ) &&
	    (p2p_soc_obj->connection_status == P2P_NOT_ACTIVE)) {
		p2p_soc_obj->connection_status = P2P_GO_NEG_PROCESS;
		p2p_debug("[P2P State]Inactive state to GO negotiation progress state");
	} else if ((tx_frame_info->public_action_type ==
		    P2P_PUBLIC_ACTION_NEG_CNF) &&
		   (p2p_soc_obj->connection_status ==
		    P2P_GO_NEG_PROCESS)) {
		p2p_soc_obj->connection_status = P2P_GO_NEG_COMPLETED;
		p2p_debug("[P2P State]GO nego progress to GO nego completed state");
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_rx_update_connection_status() - Update P2P connection status
 * with rx frame
 * @p2p_soc_obj:        P2P soc private object
 * @rx_frame_info:      frame information
 * @mac_from:            Pointer to source MAC address
 *
 * This function updates P2P connection status with rx frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_rx_update_connection_status(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct p2p_frame_info *rx_frame_info,
	uint8_t *mac_from)
{
	if (!p2p_soc_obj || !rx_frame_info || !mac_from) {
		p2p_err("invalid p2p_soc_obj:%pK or rx_frame_info:%pK, mac_from:%pK",
			p2p_soc_obj, rx_frame_info, mac_from);
		return QDF_STATUS_E_INVAL;
	}

	if (rx_frame_info->public_action_type !=
		P2P_PUBLIC_ACTION_NOT_SUPPORT)
		p2p_info("%s <--- OTA from " QDF_MAC_ADDR_FMT,
			  p2p_get_frame_type_str(rx_frame_info),
			  QDF_MAC_ADDR_REF(mac_from));

	if ((rx_frame_info->public_action_type ==
	     P2P_PUBLIC_ACTION_PROV_DIS_REQ) &&
	    (p2p_soc_obj->connection_status == P2P_NOT_ACTIVE)) {
		p2p_soc_obj->connection_status = P2P_GO_NEG_PROCESS;
		p2p_info("[P2P State]Inactive state to GO negotiation progress state");
	} else if ((rx_frame_info->public_action_type ==
		    P2P_PUBLIC_ACTION_NEG_CNF) &&
		   (p2p_soc_obj->connection_status ==
		    P2P_GO_NEG_PROCESS)) {
		p2p_soc_obj->connection_status = P2P_GO_NEG_COMPLETED;
		p2p_info("[P2P State]GO negotiation progress to GO negotiation completed state");
	} else if ((rx_frame_info->public_action_type ==
		    P2P_PUBLIC_ACTION_INVIT_REQ) &&
		   (p2p_soc_obj->connection_status == P2P_NOT_ACTIVE)) {
		p2p_soc_obj->connection_status = P2P_GO_NEG_COMPLETED;
		p2p_info("[P2P State]Inactive state to GO negotiation completed state Autonomous GO formation");
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS p2p_tx_update_connection_status(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct p2p_frame_info *tx_frame_info,
	uint8_t *mac_to)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS p2p_rx_update_connection_status(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct p2p_frame_info *rx_frame_info,
	uint8_t *mac_from)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * p2p_packet_alloc() - allocate qdf nbuf
 * @size:         buffe size
 * @data:         pointer to qdf nbuf data point
 * @ppPacket:     pointer to qdf nbuf point
 *
 * This function allocates qdf nbuf.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_packet_alloc(uint16_t size, void **data,
	qdf_nbuf_t *ppPacket)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	qdf_nbuf_t nbuf;

	nbuf = qdf_nbuf_alloc(NULL,
			roundup(size + P2P_TX_PKT_MIN_HEADROOM, 4),
			P2P_TX_PKT_MIN_HEADROOM, sizeof(uint32_t),
			false);

	if (nbuf) {
		qdf_nbuf_put_tail(nbuf, size);
		qdf_nbuf_set_protocol(nbuf, ETH_P_CONTROL);
		*ppPacket = nbuf;
		*data = qdf_nbuf_data(nbuf);
		qdf_mem_zero(*data, size);
		status = QDF_STATUS_SUCCESS;
	}

	return status;
}

/**
 * p2p_send_tx_conf() - send tx confirm
 * @tx_ctx:        tx context
 * @status:        tx status
 *
 * This function send tx confirm to osif
 *
 * Return: QDF_STATUS_SUCCESS - pointer to tx context
 */
static QDF_STATUS p2p_send_tx_conf(struct tx_action_context *tx_ctx,
	bool status)
{
	struct p2p_tx_cnf tx_cnf;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;

	if (!p2p_soc_obj || !(p2p_soc_obj->start_param)) {
		p2p_err("Invalid p2p soc object or start parameters");
		return QDF_STATUS_E_INVAL;
	}

	start_param = p2p_soc_obj->start_param;
	if (!(start_param->tx_cnf_cb)) {
		p2p_err("no tx confirm callback");
		return QDF_STATUS_E_INVAL;
	}

	if (tx_ctx->no_ack)
		tx_cnf.action_cookie = 0;
	else
		tx_cnf.action_cookie = (uint64_t)tx_ctx->id;

	tx_cnf.vdev_id = tx_ctx->vdev_id;
	tx_cnf.buf = tx_ctx->buf;
	tx_cnf.buf_len = tx_ctx->buf_len;
	tx_cnf.status = status ? 0 : 1;

	p2p_debug("soc:%pK, vdev_id:%d, action_cookie:%llx, len:%d, status:%d, buf:%pK",
		p2p_soc_obj->soc, tx_cnf.vdev_id,
		tx_cnf.action_cookie, tx_cnf.buf_len,
		tx_cnf.status, tx_cnf.buf);

	p2p_rand_mac_tx_done(p2p_soc_obj->soc, tx_ctx);

	start_param->tx_cnf_cb(start_param->tx_cnf_cb_data, &tx_cnf);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_get_hw_retry_count() - Get hw tx retry count from config store
 * @psoc:          psoc object
 * @tx_ctx:        tx context
 *
 * This function return the hw tx retry count for p2p action frame.
 * 0 value means target will use fw default mgmt tx retry count 15.
 *
 * Return: frame hw tx retry count
 */
static uint8_t p2p_get_hw_retry_count(struct wlan_objmgr_psoc *psoc,
				      struct tx_action_context *tx_ctx)
{
	if (tx_ctx->frame_info.type != P2P_FRAME_MGMT)
		return 0;

	if (tx_ctx->frame_info.sub_type != P2P_MGMT_ACTION)
		return 0;

	switch (tx_ctx->frame_info.public_action_type) {
	case P2P_PUBLIC_ACTION_NEG_REQ:
		return wlan_mlme_get_mgmt_hw_tx_retry_count(
					psoc,
					CFG_GO_NEGOTIATION_REQ_FRAME_TYPE);
	case P2P_PUBLIC_ACTION_INVIT_REQ:
		return wlan_mlme_get_mgmt_hw_tx_retry_count(
					psoc,
					CFG_P2P_INVITATION_REQ_FRAME_TYPE);
	case P2P_PUBLIC_ACTION_PROV_DIS_REQ:
		return wlan_mlme_get_mgmt_hw_tx_retry_count(
					psoc,
					CFG_PROVISION_DISCOVERY_REQ_FRAME_TYPE);
	default:
		return 0;
	}
}

#define GET_HW_RETRY_LIMIT(count) QDF_GET_BITS(count, 0, 4)
#define GET_HW_RETRY_LIMIT_EXT(count) QDF_GET_BITS(count, 4, 3)

/**
 * p2p_mgmt_set_hw_retry_count() - Set mgmt hw tx retry count
 * @psoc:          psoc object
 * @tx_ctx:        tx context
 * @mgmt_param:    mgmt frame tx parameter
 *
 * This function will set mgmt frame hw tx retry count to tx parameter
 *
 * Return: void
 */
static void
p2p_mgmt_set_hw_retry_count(struct wlan_objmgr_psoc *psoc,
			    struct tx_action_context *tx_ctx,
			    struct wmi_mgmt_params *mgmt_param)
{
	uint8_t retry_count = p2p_get_hw_retry_count(psoc, tx_ctx);

	mgmt_param->tx_param.retry_limit = GET_HW_RETRY_LIMIT(retry_count);
	mgmt_param->tx_param.retry_limit_ext =
					GET_HW_RETRY_LIMIT_EXT(retry_count);
	if (mgmt_param->tx_param.retry_limit ||
	    mgmt_param->tx_param.retry_limit_ext)
		mgmt_param->tx_params_valid = true;
}

/**
 * p2p_mgmt_tx() - call mgmt tx api
 * @tx_ctx:        tx context
 * @buf_len:       buffer length
 * @packet:        pointer to qdf nbuf
 * @frame:         pointer to qdf nbuf data
 *
 * This function call mgmt tx api to tx this action frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_mgmt_tx(struct tx_action_context *tx_ctx,
	uint32_t buf_len, qdf_nbuf_t packet, uint8_t *frame)
{
	QDF_STATUS status;
	mgmt_tx_download_comp_cb tx_comp_cb;
	mgmt_ota_comp_cb tx_ota_comp_cb;
	struct wlan_frame_hdr *wh;
	struct wlan_objmgr_peer *peer;
	struct wmi_mgmt_params mgmt_param = { 0 };
	struct wlan_objmgr_psoc *psoc;
	void *mac_addr;
	uint8_t pdev_id;
	struct wlan_objmgr_vdev *vdev;
	uint16_t chanfreq = 0;

	psoc = tx_ctx->p2p_soc_obj->soc;
	mgmt_param.tx_frame = packet;
	mgmt_param.frm_len = buf_len;
	mgmt_param.vdev_id = tx_ctx->vdev_id;
	mgmt_param.pdata = frame;
	if (tx_ctx->chan)
		chanfreq = (uint16_t)wlan_chan_to_freq(tx_ctx->chan);
	mgmt_param.chanfreq = chanfreq;

	mgmt_param.qdf_ctx = wlan_psoc_get_qdf_dev(psoc);
	if (!(mgmt_param.qdf_ctx)) {
		p2p_err("qdf ctx is null");
		return QDF_STATUS_E_INVAL;
	}
	p2p_mgmt_set_hw_retry_count(psoc, tx_ctx, &mgmt_param);

	wh = (struct wlan_frame_hdr *)frame;
	mac_addr = wh->i_addr1;
	pdev_id = wlan_get_pdev_id_from_vdev_id(psoc, tx_ctx->vdev_id,
						WLAN_P2P_ID);
	peer = wlan_objmgr_get_peer(psoc, pdev_id,  mac_addr, WLAN_P2P_ID);
	if (!peer) {
		mac_addr = wh->i_addr2;
		peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
					    WLAN_P2P_ID);
	}
	if (!peer && tx_ctx->rand_mac_tx) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				psoc, tx_ctx->vdev_id, WLAN_P2P_ID);
		if (vdev) {
			mac_addr = wlan_vdev_mlme_get_macaddr(vdev);
			peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr,
						    WLAN_P2P_ID);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		}
	}

	if (!peer) {
		p2p_err("no valid peer");
		return QDF_STATUS_E_INVAL;
	}

	if (tx_ctx->no_ack) {
		tx_comp_cb = tgt_p2p_mgmt_download_comp_cb;
		tx_ota_comp_cb = NULL;
	} else {
		tx_comp_cb = NULL;
		tx_ota_comp_cb = tgt_p2p_mgmt_ota_comp_cb;
	}

	p2p_debug("length:%d, chanfreq:%d retry count:%d(%d, %d)",
		  mgmt_param.frm_len, mgmt_param.chanfreq,
		  (mgmt_param.tx_param.retry_limit_ext << 4) |
		  mgmt_param.tx_param.retry_limit,
		  mgmt_param.tx_param.retry_limit,
		  mgmt_param.tx_param.retry_limit_ext);

	tx_ctx->nbuf = packet;

	status = wlan_mgmt_txrx_mgmt_frame_tx(peer, tx_ctx->p2p_soc_obj,
			packet, tx_comp_cb, tx_ota_comp_cb,
			WLAN_UMAC_COMP_P2P, &mgmt_param);

	wlan_objmgr_peer_release_ref(peer, WLAN_P2P_ID);

	return status;
}

/**
 * p2p_roc_req_for_tx_action() - new a roc request for tx
 * @tx_ctx:        tx context
 *
 * This function new a roc request for tx and call roc api to process
 * this new roc request.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_roc_req_for_tx_action(
	struct tx_action_context *tx_ctx)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *roc_ctx;
	QDF_STATUS status;

	roc_ctx = qdf_mem_malloc(sizeof(struct p2p_roc_context));
	if (!roc_ctx)
		return QDF_STATUS_E_NOMEM;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;
	roc_ctx->p2p_soc_obj = p2p_soc_obj;
	roc_ctx->vdev_id = tx_ctx->vdev_id;
	roc_ctx->chan = tx_ctx->chan;
	roc_ctx->duration = tx_ctx->duration;
	roc_ctx->roc_state = ROC_STATE_IDLE;
	roc_ctx->roc_type = OFF_CHANNEL_TX;
	roc_ctx->tx_ctx = tx_ctx;
	roc_ctx->id = tx_ctx->id;
	tx_ctx->roc_cookie = (uintptr_t)roc_ctx;

	p2p_debug("create roc request for off channel tx, tx ctx:%pK, roc ctx:%pK",
		tx_ctx, roc_ctx);

	status = p2p_process_roc_req(roc_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("request roc for tx action frrame fail");
		return status;
	}

	status = qdf_list_insert_back(&p2p_soc_obj->tx_q_roc,
		&tx_ctx->node);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to insert off chan tx context to wait roc req queue");

	return status;
}

/**
 * p2p_find_tx_ctx() - find tx context by cookie
 * @p2p_soc_obj:        p2p soc object
 * @cookie:          cookie to this p2p tx context
 * @is_roc_q:        it is in waiting for roc queue
 * @is_ack_q:        it is in waiting for ack queue
 *
 * This function finds out tx context by cookie.
 *
 * Return: pointer to tx context
 */
static struct tx_action_context *p2p_find_tx_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint64_t cookie,
	bool *is_roc_q, bool *is_ack_q)
{
	struct tx_action_context *cur_tx_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;
	*is_roc_q = false;
	*is_ack_q = false;

	p2p_debug("Start to find tx ctx, p2p soc_obj:%pK, cookie:%llx",
		p2p_soc_obj, cookie);

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_roc, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		cur_tx_ctx = qdf_container_of(p_node,
				struct tx_action_context, node);
		if ((uintptr_t) cur_tx_ctx == cookie) {
			*is_roc_q = true;
			p2p_debug("find tx ctx, cookie:%llx", cookie);
			return cur_tx_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_roc,
						p_node, &p_node);
	}

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_ack, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		cur_tx_ctx = qdf_container_of(p_node,
				struct tx_action_context, node);
		if ((uintptr_t) cur_tx_ctx == cookie) {
			*is_ack_q = true;
			p2p_debug("find tx ctx, cookie:%llx", cookie);
			return cur_tx_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_ack,
						p_node, &p_node);
	}

	return NULL;
}

/**
 * p2p_find_tx_ctx_by_roc() - find tx context by roc
 * @p2p_soc_obj:        p2p soc object
 * @cookie:          cookie to roc context
 *
 * This function finds out tx context by roc context.
 *
 * Return: pointer to tx context
 */
static struct tx_action_context *p2p_find_tx_ctx_by_roc(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint64_t cookie)
{
	struct tx_action_context *cur_tx_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	p2p_debug("Start to find tx ctx, p2p soc_obj:%pK, cookie:%llx",
		p2p_soc_obj, cookie);

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_roc, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		cur_tx_ctx = qdf_container_of(p_node,
					struct tx_action_context, node);
		if (cur_tx_ctx->roc_cookie == cookie) {
			p2p_debug("find tx ctx, cookie:%llx", cookie);
			return cur_tx_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_roc,
						p_node, &p_node);
	}

	return NULL;
}

/**
 * p2p_move_tx_context_to_ack_queue() - move tx context to tx_q_ack
 * @tx_ctx:        tx context
 *
 * This function moves tx context to waiting for ack queue.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_move_tx_context_to_ack_queue(
	struct tx_action_context *tx_ctx)
{
	bool is_roc_q = false;
	bool is_ack_q = false;
	struct p2p_soc_priv_obj *p2p_soc_obj = tx_ctx->p2p_soc_obj;
	struct tx_action_context *cur_tx_ctx;
	QDF_STATUS status;

	cur_tx_ctx = p2p_find_tx_ctx(p2p_soc_obj, (uintptr_t)tx_ctx,
					&is_roc_q, &is_ack_q);
	if (cur_tx_ctx) {
		if (is_roc_q) {
			p2p_debug("find in wait for roc queue");
			status = qdf_list_remove_node(
				&p2p_soc_obj->tx_q_roc,
				(qdf_list_node_t *)tx_ctx);
			if (status != QDF_STATUS_SUCCESS)
				p2p_err("Failed to remove off chan tx context from wait roc req queue");
		}

		if (is_ack_q) {
			p2p_debug("Already in waiting for ack queue");
			return QDF_STATUS_SUCCESS;
		}
	}

	status = qdf_list_insert_back(
				&p2p_soc_obj->tx_q_ack,
				&tx_ctx->node);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to insert off chan tx context to wait ack req queue");

	return status;
}

/**
 * p2p_extend_roc_timer() - extend roc timer
 * @p2p_soc_obj:   p2p soc private object
 * @frame_info:    pointer to frame information
 *
 * This function extends roc timer for some of p2p public action frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_extend_roc_timer(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct p2p_frame_info *frame_info)
{
	struct p2p_roc_context *curr_roc_ctx;
	uint32_t extend_time;

	curr_roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);
	if (!curr_roc_ctx) {
		p2p_debug("no running roc request currently");
		return QDF_STATUS_SUCCESS;
	}

	if (!frame_info) {
		p2p_err("invalid frame information");
		return QDF_STATUS_E_INVAL;
	}

	switch (frame_info->public_action_type) {
	case P2P_PUBLIC_ACTION_NEG_REQ:
	case P2P_PUBLIC_ACTION_NEG_RSP:
		extend_time = 2 * P2P_ACTION_FRAME_DEFAULT_WAIT;
		break;
	case P2P_PUBLIC_ACTION_INVIT_REQ:
	case P2P_PUBLIC_ACTION_DEV_DIS_REQ:
		extend_time = P2P_ACTION_FRAME_DEFAULT_WAIT;
		break;
	default:
		extend_time = 0;
		break;
	}

	if (extend_time) {
		p2p_debug("extend roc timer, duration:%d", extend_time);
		curr_roc_ctx->duration = extend_time;
		return p2p_restart_roc_timer(curr_roc_ctx);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_adjust_tx_wait() - adjust tx wait
 * @tx_ctx:        tx context
 *
 * This function adjust wait time of this tx context
 *
 * Return: None
 */
static void p2p_adjust_tx_wait(struct tx_action_context *tx_ctx)
{
	struct p2p_frame_info *frame_info;

	frame_info = &(tx_ctx->frame_info);
	switch (frame_info->public_action_type) {
	case P2P_PUBLIC_ACTION_NEG_RSP:
	case P2P_PUBLIC_ACTION_PROV_DIS_RSP:
		tx_ctx->duration += P2P_ACTION_FRAME_RSP_WAIT;
		break;
	case P2P_PUBLIC_ACTION_NEG_CNF:
	case P2P_PUBLIC_ACTION_INVIT_RSP:
		tx_ctx->duration += P2P_ACTION_FRAME_ACK_WAIT;
		break;
	default:
		break;
	}
}

/**
 * p2p_remove_tx_context() - remove tx ctx from queue
 * @tx_ctx:        tx context
 *
 * This function remove tx context from waiting for roc queue or
 * waiting for ack queue.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_remove_tx_context(
	struct tx_action_context *tx_ctx)
{
	bool is_roc_q = false;
	bool is_ack_q = false;
	struct tx_action_context *cur_tx_ctx;
	uint64_t cookie = (uintptr_t)tx_ctx;
	struct p2p_soc_priv_obj *p2p_soc_obj = tx_ctx->p2p_soc_obj;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	p2p_debug("tx context:%pK", tx_ctx);

	cur_tx_ctx = p2p_find_tx_ctx(p2p_soc_obj, cookie, &is_roc_q,
					&is_ack_q);

	/* for not off channel tx case, won't find from queue */
	if (!cur_tx_ctx) {
		p2p_debug("Do not find tx context from queue");
		goto end;
	}

	if (is_roc_q) {
		status = qdf_list_remove_node(
			&p2p_soc_obj->tx_q_roc,
			(qdf_list_node_t *)cur_tx_ctx);
		if (status != QDF_STATUS_SUCCESS)
			p2p_err("Failed to  tx context from wait roc req queue");
	}

	if (is_ack_q) {
		status = qdf_list_remove_node(
			&p2p_soc_obj->tx_q_ack,
			(qdf_list_node_t *)cur_tx_ctx);
		if (status != QDF_STATUS_SUCCESS)
			p2p_err("Failed to  tx context from wait ack req queue");
	}

end:
	if (!tx_ctx->roc_cookie)
		qdf_idr_remove(&p2p_soc_obj->p2p_idr, tx_ctx->id);
	qdf_mem_free(tx_ctx->buf);
	qdf_mem_free(tx_ctx);

	return status;
}

/**
 * p2p_tx_timeout() - Callback for tx timeout
 * @pdata: pointer to tx context
 *
 * This function is callback for tx time out.
 *
 * Return: None
 */
static void p2p_tx_timeout(void *pdata)
{
	QDF_STATUS status, ret;
	qdf_list_node_t *p_node;
	struct tx_action_context *tx_ctx;
	struct p2p_soc_priv_obj *p2p_soc_obj;

	p2p_info("pdata:%pK", pdata);
	p2p_soc_obj = (struct p2p_soc_priv_obj *)pdata;
	if (!p2p_soc_obj) {
		p2p_err("null p2p soc obj");
		return;
	}

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_ack, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		tx_ctx = qdf_container_of(p_node,
					  struct tx_action_context, node);
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_ack,
					    p_node, &p_node);
		if (QDF_TIMER_STATE_STOPPED ==
		    qdf_mc_timer_get_current_state(&tx_ctx->tx_timer)) {
			ret = qdf_list_remove_node(&p2p_soc_obj->tx_q_ack,
						   &tx_ctx->node);
			if (ret == QDF_STATUS_SUCCESS) {
				qdf_mc_timer_destroy(&tx_ctx->tx_timer);
				p2p_send_tx_conf(tx_ctx, false);
				qdf_mem_free(tx_ctx->buf);
				qdf_mem_free(tx_ctx);
			} else
				p2p_err("remove %pK from roc_q fail",
					tx_ctx);
		}
	}
}

/**
 * p2p_enable_tx_timer() - enable tx timer
 * @tx_ctx:         tx context
 *
 * This function enable tx timer for action frame required ota tx.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_enable_tx_timer(struct tx_action_context *tx_ctx)
{
	QDF_STATUS status;

	status = qdf_mc_timer_init(&tx_ctx->tx_timer,
				   QDF_TIMER_TYPE_SW, p2p_tx_timeout,
				   tx_ctx->p2p_soc_obj);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("failed to init tx timer tx_ctx:%pK", tx_ctx);
		return status;
	}

	status = qdf_mc_timer_start(&tx_ctx->tx_timer,
				    P2P_ACTION_FRAME_TX_TIMEOUT);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("tx timer start failed tx_ctx:%pK", tx_ctx);

	return status;
}

/**
 * p2p_disable_tx_timer() - disable tx timer
 * @tx_ctx:         tx context
 *
 * This function disable tx timer for action frame required ota tx.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_disable_tx_timer(struct tx_action_context *tx_ctx)
{
	QDF_STATUS status;

	p2p_debug("tx context:%pK", tx_ctx);

	status = qdf_mc_timer_stop(&tx_ctx->tx_timer);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to stop tx timer, status:%d", status);

	status = qdf_mc_timer_destroy(&tx_ctx->tx_timer);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to destroy tx timer, status:%d", status);

	return status;
}

/**
 * is_rmf_mgmt_action_frame() - check RMF action frame by category
 * @action_category: action frame actegory
 *
 * This function check the frame is robust mgmt action frame or not
 *
 * Return: true - if category is robust mgmt type
 */
static bool is_rmf_mgmt_action_frame(uint8_t action_category)
{
	switch (action_category) {
	case ACTION_CATEGORY_SPECTRUM_MGMT:
	case ACTION_CATEGORY_QOS:
	case ACTION_CATEGORY_DLS:
	case ACTION_CATEGORY_BACK:
	case ACTION_CATEGORY_RRM:
	case ACTION_FAST_BSS_TRNST:
	case ACTION_CATEGORY_SA_QUERY:
	case ACTION_CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION:
	case ACTION_CATEGORY_WNM:
	case ACTION_CATEGORY_MESH_ACTION:
	case ACTION_CATEGORY_MULTIHOP_ACTION:
	case ACTION_CATEGORY_DMG:
	case ACTION_CATEGORY_FST:
	case ACTION_CATEGORY_VENDOR_SPECIFIC_PROTECTED:
		return true;
	default:
		break;
	}
	return false;
}

/**
 * p2p_populate_rmf_field() - populate unicast rmf frame
 * @tx_ctx: tx_action_context
 * @size: input size of frame, and output new size
 * @ppbuf: input frame ptr, and output new frame
 * @ppkt: input pkt, output new pkt.
 *
 * This function allocates new pkt for rmf frame. The
 * new frame has extra space for ccmp field.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_populate_rmf_field(struct tx_action_context *tx_ctx,
	uint32_t *size, uint8_t **ppbuf, qdf_nbuf_t *ppkt)
{
	struct wlan_frame_hdr *wh, *rmf_wh;
	struct action_frm_hdr *action_hdr;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	qdf_nbuf_t pkt = NULL;
	uint8_t *frame;
	uint32_t frame_len;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	uint8_t action_category;

	p2p_soc_obj = tx_ctx->p2p_soc_obj;

	if (tx_ctx->frame_info.sub_type != P2P_MGMT_ACTION ||
	    !p2p_soc_obj->p2p_cb.is_mgmt_protected)
		return QDF_STATUS_SUCCESS;
	if (*size < (sizeof(struct wlan_frame_hdr) +
	    sizeof(struct action_frm_hdr))) {
		return QDF_STATUS_E_INVAL;
	}

	wh = (struct wlan_frame_hdr *)(*ppbuf);
	action_hdr = (struct action_frm_hdr *)(*ppbuf + sizeof(*wh));

	/*
	 * For Action frame which are not handled, the resp is sent back to the
	 * source without change, except that MSB of the Category set to 1, so
	 * to get the actual action category we need to ignore the MSB.
	 */
	action_category = action_hdr->action_category & 0x7f;
	if (!is_rmf_mgmt_action_frame(action_category)) {
		p2p_debug("non rmf act frame 0x%x category %x",
			  tx_ctx->frame_info.sub_type,
			  action_hdr->action_category);
		return QDF_STATUS_SUCCESS;
	}

	if (!p2p_soc_obj->p2p_cb.is_mgmt_protected(
		tx_ctx->vdev_id, wh->i_addr1)) {
		p2p_debug("non rmf connection vdev %d "QDF_MAC_ADDR_FMT,
			  tx_ctx->vdev_id, QDF_MAC_ADDR_REF(wh->i_addr1));
		return QDF_STATUS_SUCCESS;
	}
	if (!qdf_is_macaddr_group((struct qdf_mac_addr *)wh->i_addr1) &&
	    !qdf_is_macaddr_broadcast((struct qdf_mac_addr *)wh->i_addr1)) {
		uint8_t mic_len, mic_hdr_len, pdev_id;

		pdev_id =
			wlan_get_pdev_id_from_vdev_id(tx_ctx->p2p_soc_obj->soc,
						      tx_ctx->vdev_id,
						      WLAN_P2P_ID);
		status = mlme_get_peer_mic_len(p2p_soc_obj->soc, pdev_id,
					       wh->i_addr1, &mic_len,
					       &mic_hdr_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			p2p_err("Failed to get peer mic length.");
			return status;
		}

		frame_len = *size + mic_hdr_len + mic_len;
		status = p2p_packet_alloc((uint16_t)frame_len, (void **)&frame,
			 &pkt);
		if (status != QDF_STATUS_SUCCESS) {
			p2p_err("Failed to allocate %d bytes for rmf frame.",
				frame_len);
			return QDF_STATUS_E_NOMEM;
		}

		qdf_mem_copy(frame, wh, sizeof(*wh));
		qdf_mem_copy(frame + sizeof(*wh) + mic_hdr_len,
			     *ppbuf + sizeof(*wh),
			     *size - sizeof(*wh));
		rmf_wh = (struct wlan_frame_hdr *)frame;
		(rmf_wh)->i_fc[1] |= IEEE80211_FC1_WEP;
		p2p_debug("set protection 0x%x cat %d "QDF_MAC_ADDR_FMT,
			  tx_ctx->frame_info.sub_type,
			  action_hdr->action_category,
			  QDF_MAC_ADDR_REF(wh->i_addr1));

		qdf_nbuf_free(*ppkt);
		*ppbuf = frame;
		*ppkt = pkt;
		*size = frame_len;
		/*
		 * Some target which support sending mgmt frame based on htt
		 * would DMA write this PMF tx frame buffer, it may cause smmu
		 * check permission fault, set a flag to do bi-direction DMA
		 * map, normal tx unmap is enough for this case.
		 */
		QDF_NBUF_CB_TX_DMA_BI_MAP(pkt) = 1;
	}

	return status;
}

/**
 * p2p_execute_tx_action_frame() - execute tx action frame
 * @tx_ctx:        tx context
 *
 * This function modify p2p ie and tx this action frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_execute_tx_action_frame(
	struct tx_action_context *tx_ctx)
{
	uint8_t *frame;
	qdf_nbuf_t packet;
	QDF_STATUS status;
	uint8_t noa_len = 0;
	uint8_t noa_stream[P2P_NOA_STREAM_ARR_SIZE];
	uint8_t orig_len = 0;
	const uint8_t *ie;
	uint8_t ie_len;
	uint8_t *p2p_ie = NULL;
	const uint8_t *presence_noa_attr = NULL;
	uint32_t nbytes_copy;
	uint32_t buf_len = tx_ctx->buf_len;
	struct p2p_frame_info *frame_info;

	frame_info = &(tx_ctx->frame_info);
	if (frame_info->sub_type == P2P_MGMT_PROBE_RSP) {
		p2p_ie = (uint8_t *)p2p_get_p2pie_from_probe_rsp(tx_ctx);
	} else if (frame_info->action_type ==
			P2P_ACTION_PRESENCE_RSP) {
		ie = tx_ctx->buf +
			P2P_PUBLIC_ACTION_FRAME_TYPE_OFFSET;
		ie_len = tx_ctx->buf_len -
			P2P_PUBLIC_ACTION_FRAME_TYPE_OFFSET;
		p2p_ie = (uint8_t *)p2p_get_p2pie_ptr(ie, ie_len);
		if (p2p_ie) {
			/* extract the presence of NoA attribute inside
			 * P2P IE */
			ie = p2p_ie + P2P_IE_HEADER_LEN;
			ie_len = p2p_ie[1];
			presence_noa_attr = p2p_get_presence_noa_attr(
						ie, ie_len);
		}
	}

	if ((frame_info->sub_type != P2P_MGMT_NOT_SUPPORT) &&
		p2p_ie) {
		orig_len = p2p_ie[1];
		noa_len = p2p_update_noa_stream(tx_ctx, p2p_ie,
				presence_noa_attr, &buf_len,
				noa_stream);
		buf_len += noa_len;
	}

	if (frame_info->sub_type == P2P_MGMT_PROBE_RSP)
		p2p_set_ht_caps(tx_ctx, buf_len);

		/* Ok-- try to allocate some memory: */
	status = p2p_packet_alloc((uint16_t) buf_len, (void **)&frame,
			&packet);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to allocate %d bytes for a Probe Request.",
			buf_len);
		return status;
	}

	/*
	 * Add sequence number to action frames
	 * Frames are handed over in .11 format by supplicant already
	 */
	p2p_populate_mac_header(tx_ctx);

	if ((noa_len > 0) && p2p_ie
		&& (noa_len < (P2P_MAX_NOA_ATTR_LEN +
				P2P_IE_HEADER_LEN))) {
		/* Add 2 bytes for length and Arribute field */
		nbytes_copy = (p2p_ie + orig_len + 2) - tx_ctx->buf;
		qdf_mem_copy(frame, tx_ctx->buf, nbytes_copy);
		qdf_mem_copy((frame + nbytes_copy), noa_stream,
				noa_len);
		qdf_mem_copy((frame + nbytes_copy + noa_len),
			tx_ctx->buf + nbytes_copy,
			buf_len - nbytes_copy - noa_len);
	} else {
		qdf_mem_copy(frame, tx_ctx->buf, buf_len);
	}

	status = p2p_populate_rmf_field(tx_ctx, &buf_len, &frame, &packet);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("failed to populate rmf frame");
		qdf_nbuf_free(packet);
		return status;
	}
	status = p2p_mgmt_tx(tx_ctx, buf_len, packet, frame);
	if (status == QDF_STATUS_SUCCESS) {
		if (tx_ctx->no_ack) {
			p2p_send_tx_conf(tx_ctx, true);
			p2p_remove_tx_context(tx_ctx);
		} else {
			p2p_enable_tx_timer(tx_ctx);
			p2p_move_tx_context_to_ack_queue(tx_ctx);
		}
	} else {
		p2p_err("failed to tx mgmt frame");
		qdf_nbuf_free(packet);
	}

	return status;
}

struct tx_action_context *p2p_find_tx_ctx_by_nbuf(
	struct p2p_soc_priv_obj *p2p_soc_obj, void *nbuf)
{
	struct tx_action_context *cur_tx_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	if (!p2p_soc_obj) {
		p2p_err("invalid p2p soc object");
		return NULL;
	}

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_ack, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		cur_tx_ctx =
			qdf_container_of(p_node, struct tx_action_context, node);
		if (cur_tx_ctx->nbuf == nbuf) {
			p2p_debug("find tx ctx, nbuf:%pK", nbuf);
			return cur_tx_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_ack,
						p_node, &p_node);
	}

	return NULL;
}

void p2p_dump_tx_queue(struct p2p_soc_priv_obj *p2p_soc_obj)
{
	struct tx_action_context *tx_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	p2p_debug("dump tx queue wait for roc, p2p soc obj:%pK, size:%d",
		p2p_soc_obj, qdf_list_size(&p2p_soc_obj->tx_q_roc));

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_roc, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		tx_ctx = qdf_container_of(p_node,
				struct tx_action_context, node);
		p2p_debug("p2p soc object:%pK, tx ctx:%pK, vdev_id:%d, scan_id:%d, roc_cookie:%llx, chan:%d, buf:%pK, len:%d, off_chan:%d, cck:%d, ack:%d, duration:%d",
			p2p_soc_obj, tx_ctx,
			tx_ctx->vdev_id, tx_ctx->scan_id,
			tx_ctx->roc_cookie, tx_ctx->chan,
			tx_ctx->buf, tx_ctx->buf_len,
			tx_ctx->off_chan, tx_ctx->no_cck,
			tx_ctx->no_ack, tx_ctx->duration);

		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_roc,
						p_node, &p_node);
	}

	p2p_debug("dump tx queue wait for ack, size:%d",
		qdf_list_size(&p2p_soc_obj->tx_q_ack));
	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_ack, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		tx_ctx = qdf_container_of(p_node,
				struct tx_action_context, node);
		p2p_debug("p2p soc object:%pK, tx_ctx:%pK, vdev_id:%d, scan_id:%d, roc_cookie:%llx, chan:%d, buf:%pK, len:%d, off_chan:%d, cck:%d, ack:%d, duration:%d",
			p2p_soc_obj, tx_ctx,
			tx_ctx->vdev_id, tx_ctx->scan_id,
			tx_ctx->roc_cookie, tx_ctx->chan,
			tx_ctx->buf, tx_ctx->buf_len,
			tx_ctx->off_chan, tx_ctx->no_cck,
			tx_ctx->no_ack, tx_ctx->duration);

		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_ack,
						p_node, &p_node);
	}
}

QDF_STATUS p2p_ready_to_tx_frame(struct p2p_soc_priv_obj *p2p_soc_obj,
	uint64_t cookie)
{
	struct tx_action_context *cur_tx_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	cur_tx_ctx = p2p_find_tx_ctx_by_roc(p2p_soc_obj, cookie);

	while (cur_tx_ctx) {
		p2p_debug("tx_ctx:%pK", cur_tx_ctx);
		status = p2p_execute_tx_action_frame(cur_tx_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			p2p_send_tx_conf(cur_tx_ctx, false);
			p2p_remove_tx_context(cur_tx_ctx);
		}
		cur_tx_ctx = p2p_find_tx_ctx_by_roc(p2p_soc_obj, cookie);
	}

	return status;
}

QDF_STATUS p2p_cleanup_tx_sync(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct wlan_objmgr_vdev *vdev)
{
	struct scheduler_msg msg = {0};
	struct p2p_cleanup_param *param;
	QDF_STATUS status;
	uint32_t vdev_id;

	if (!p2p_soc_obj) {
		p2p_err("p2p soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_debug("p2p_soc_obj:%pK, vdev:%pK", p2p_soc_obj, vdev);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_NOMEM;

	param->p2p_soc_obj = p2p_soc_obj;
	if (vdev)
		vdev_id = (uint32_t)wlan_vdev_get_id(vdev);
	else
		vdev_id = P2P_INVALID_VDEV_ID;
	param->vdev_id = vdev_id;
	qdf_event_reset(&p2p_soc_obj->cleanup_tx_done);
	msg.type = P2P_CLEANUP_TX;
	msg.bodyptr = param;
	msg.callback = p2p_process_cmd;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_OS_IF, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(param);
		return status;
	}

	status = qdf_wait_single_event(
			&p2p_soc_obj->cleanup_tx_done,
			P2P_WAIT_CLEANUP_ROC);

	if (status != QDF_STATUS_SUCCESS)
		p2p_err("wait for cleanup tx timeout, %d", status);

	return status;
}

QDF_STATUS p2p_process_cleanup_tx_queue(struct p2p_cleanup_param *param)
{
	struct tx_action_context *curr_tx_ctx;
	qdf_list_node_t *p_node;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	uint32_t vdev_id;
	QDF_STATUS status, ret;

	if (!param || !(param->p2p_soc_obj)) {
		p2p_err("Invalid cleanup param");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_soc_obj = param->p2p_soc_obj;
	vdev_id = param->vdev_id;

	p2p_debug("clean up tx queue wait for roc, size:%d, vdev_id:%d",
		  qdf_list_size(&p2p_soc_obj->tx_q_roc), vdev_id);

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_roc, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		curr_tx_ctx = qdf_container_of(p_node,
					struct tx_action_context, node);
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_roc,
					    p_node, &p_node);
		if ((vdev_id == P2P_INVALID_VDEV_ID) ||
		    (vdev_id == curr_tx_ctx->vdev_id)) {
			ret = qdf_list_remove_node(&p2p_soc_obj->tx_q_roc,
						   &curr_tx_ctx->node);
			if (ret == QDF_STATUS_SUCCESS) {
				p2p_send_tx_conf(curr_tx_ctx, false);
				qdf_mem_free(curr_tx_ctx->buf);
				qdf_mem_free(curr_tx_ctx);
			} else
				p2p_err("remove %pK from roc_q fail",
					curr_tx_ctx);
		}
	}

	p2p_debug("clean up tx queue wait for ack, size:%d",
		  qdf_list_size(&p2p_soc_obj->tx_q_ack));

	status = qdf_list_peek_front(&p2p_soc_obj->tx_q_ack, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		curr_tx_ctx = qdf_container_of(p_node,
					struct tx_action_context, node);
		status = qdf_list_peek_next(&p2p_soc_obj->tx_q_ack,
					    p_node, &p_node);
		if ((vdev_id == P2P_INVALID_VDEV_ID) ||
		    (vdev_id == curr_tx_ctx->vdev_id)) {
			ret = qdf_list_remove_node(&p2p_soc_obj->tx_q_ack,
						   &curr_tx_ctx->node);
			if (ret == QDF_STATUS_SUCCESS) {
				p2p_disable_tx_timer(curr_tx_ctx);
				p2p_send_tx_conf(curr_tx_ctx, false);
				qdf_mem_free(curr_tx_ctx->buf);
				qdf_mem_free(curr_tx_ctx);
			} else
				p2p_err("remove %pK from roc_q fail",
					curr_tx_ctx);
		}
	}

	qdf_event_set(&p2p_soc_obj->cleanup_tx_done);

	return QDF_STATUS_SUCCESS;
}

bool p2p_check_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
			  uint8_t *random_mac_addr)
{
	uint32_t i = 0;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("vdev is null");
		return false;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("p2p vdev object is NULL");
		return false;
	}

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if ((p2p_vdev_obj->random_mac[i].in_use) &&
		    (!qdf_mem_cmp(p2p_vdev_obj->random_mac[i].addr,
				  random_mac_addr, QDF_MAC_ADDR_SIZE))) {
			qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
			return true;
		}
	}
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return false;
}

/**
 * find_action_frame_cookie() - Checks for action cookie in cookie list
 * @cookie_list: List of cookies
 * @rnd_cookie: Cookie to be searched
 *
 * Return: If search is successful return pointer to action_frame_cookie
 * object in which cookie item is encapsulated.
 */
static struct action_frame_cookie *
find_action_frame_cookie(qdf_list_t *cookie_list, uint64_t rnd_cookie)
{
	struct action_frame_cookie *action_cookie;

	qdf_list_for_each(cookie_list, action_cookie, cookie_node) {
		if (action_cookie->cookie == rnd_cookie)
			return action_cookie;
	}

	return NULL;
}

/**
 * allocate_action_frame_cookie() - Allocate and add action cookie to
 * given list
 * @cookie_list: List of cookies
 * @rnd_cookie: Cookie to be added
 *
 * Return: If allocation and addition is successful return pointer to
 * action_frame_cookie object in which cookie item is encapsulated.
 */
static struct action_frame_cookie *
allocate_action_frame_cookie(qdf_list_t *cookie_list, uint64_t rnd_cookie)
{
	struct action_frame_cookie *action_cookie;

	action_cookie = qdf_mem_malloc(sizeof(*action_cookie));
	if (!action_cookie)
		return NULL;

	action_cookie->cookie = rnd_cookie;
	qdf_list_insert_front(cookie_list, &action_cookie->cookie_node);

	return action_cookie;
}

/**
 * delete_action_frame_cookie() - Delete the cookie from given list
 * @cookie_list: List of cookies
 * @action_cookie: Cookie to be deleted
 *
 * This function deletes the cookie item from given list and corresponding
 * object in which it is encapsulated.
 *
 * Return: None
 */
static void
delete_action_frame_cookie(qdf_list_t *cookie_list,
			   struct action_frame_cookie *action_cookie)
{
	qdf_list_remove_node(cookie_list, &action_cookie->cookie_node);
	qdf_mem_free(action_cookie);
}

/**
 * delete_all_action_frame_cookie() - Delete all the cookies to given list
 * @cookie_list: List of cookies
 *
 * This function deletes all the cookies from from given list.
 *
 * Return: None
 */
static void
delete_all_action_frame_cookie(qdf_list_t *cookie_list)
{
	qdf_list_node_t *node = NULL;

	p2p_debug("Delete cookie list %pK, size %d", cookie_list,
		  qdf_list_size(cookie_list));

	while (!qdf_list_empty(cookie_list)) {
		qdf_list_remove_front(cookie_list, &node);
		qdf_mem_free(node);
	}
}

/**
 * append_action_frame_cookie() - Append action cookie to given list
 * @cookie_list: List of cookies
 * @rnd_cookie: Cookie to be append
 *
 * This is a wrapper function which invokes allocate_action_frame_cookie
 * if the cookie to be added is not duplicate
 *
 * Return: true - for successful case
 *             false - failed.
 */
static bool
append_action_frame_cookie(qdf_list_t *cookie_list, uint64_t rnd_cookie)
{
	struct action_frame_cookie *action_cookie;

	/*
	 * There should be no mac entry with empty cookie list,
	 * check and ignore if duplicate
	 */
	action_cookie = find_action_frame_cookie(cookie_list, rnd_cookie);
	if (action_cookie)
		/* random mac address is already programmed */
		return true;

	/* insert new cookie in cookie list */
	action_cookie = allocate_action_frame_cookie(cookie_list, rnd_cookie);
	if (!action_cookie)
		return false;

	return true;
}

/**
 * p2p_add_random_mac() - add or append random mac to given vdev rand mac list
 * @soc: soc object
 * @vdev_id: vdev id
 * @mac: mac addr to be added or append
 * @freq: frequency
 * @rnd_cookie: random mac mgmt tx cookie
 *
 * This function will add or append the mac addr entry to vdev random mac list.
 * Once the mac addr filter is not needed, it can be removed by
 * p2p_del_random_mac.
 *
 * Return: QDF_STATUS_E_EXISTS - append to existing list
 *             QDF_STATUS_SUCCESS - add a new entry.
 *             other : failed to add the mac address entry.
 */
static QDF_STATUS
p2p_add_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		   uint8_t *mac, uint32_t freq, uint64_t rnd_cookie)
{
	uint32_t i;
	uint32_t first_unused = MAX_RANDOM_MAC_ADDRS;
	struct action_frame_cookie *action_cookie;
	int32_t append_ret;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	p2p_debug("random_mac:vdev %d mac_addr:"QDF_MAC_ADDR_FMT" rnd_cookie=%llu freq = %u",
		  vdev_id, QDF_MAC_ADDR_REF(mac), rnd_cookie, freq);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("vdev is null");

		return QDF_STATUS_E_INVAL;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		p2p_debug("random_mac:p2p vdev object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	/*
	 * Following loop checks whether random mac entry is already
	 * present, if present get the index of matched entry else
	 * get the first unused slot to store this new random mac
	 */
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if (!p2p_vdev_obj->random_mac[i].in_use) {
			if (first_unused == MAX_RANDOM_MAC_ADDRS)
				first_unused = i;
			continue;
		}

		if (!qdf_mem_cmp(p2p_vdev_obj->random_mac[i].addr, mac,
				 QDF_MAC_ADDR_SIZE))
			break;
	}

	if (i != MAX_RANDOM_MAC_ADDRS) {
		append_ret = append_action_frame_cookie(
				&p2p_vdev_obj->random_mac[i].cookie_list,
				rnd_cookie);
		qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("random_mac:append %d vdev %d freq %d "QDF_MAC_ADDR_FMT" rnd_cookie %llu",
			  append_ret, vdev_id, freq, QDF_MAC_ADDR_REF(mac), rnd_cookie);
		if (!append_ret) {
			p2p_debug("random_mac:failed to append rnd_cookie");
			return QDF_STATUS_E_NOMEM;
		}

		return QDF_STATUS_E_EXISTS;
	}

	if (first_unused == MAX_RANDOM_MAC_ADDRS) {
		qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("random_mac:Reached the limit of Max random addresses");

		return QDF_STATUS_E_RESOURCES;
	}

	/* get the first unused buf and store new random mac */
	i = first_unused;

	action_cookie = allocate_action_frame_cookie(
				&p2p_vdev_obj->random_mac[i].cookie_list,
				rnd_cookie);
	if (!action_cookie) {
		qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_err("random_mac:failed to alloc rnd cookie");

		return QDF_STATUS_E_NOMEM;
	}
	qdf_mem_copy(p2p_vdev_obj->random_mac[i].addr, mac, QDF_MAC_ADDR_SIZE);
	p2p_vdev_obj->random_mac[i].in_use = true;
	p2p_vdev_obj->random_mac[i].freq = freq;
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
	p2p_debug("random_mac:add vdev %d freq %d "QDF_MAC_ADDR_FMT" rnd_cookie %llu",
		  vdev_id, freq, QDF_MAC_ADDR_REF(mac), rnd_cookie);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
p2p_del_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		   uint64_t rnd_cookie)
{
	uint32_t i;
	struct action_frame_cookie *action_cookie;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	p2p_debug("random_mac:vdev %d cookie %llu", vdev_id, rnd_cookie);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id,
						    WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("vdev is null");
		return QDF_STATUS_E_INVAL;
	}
	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("p2p vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		struct action_frame_random_mac *random_mac;
		qdf_freq_t freq;
		uint8_t addr[QDF_MAC_ADDR_SIZE];

		random_mac = &p2p_vdev_obj->random_mac[i];
		if (!random_mac->in_use)
			continue;

		action_cookie = find_action_frame_cookie(
				&random_mac->cookie_list, rnd_cookie);
		if (!action_cookie)
			continue;

		delete_action_frame_cookie(
			&random_mac->cookie_list,
			action_cookie);

		if (qdf_list_empty(&random_mac->cookie_list)) {
			random_mac->in_use = false;
			freq = random_mac->freq;
			qdf_mem_copy(addr, random_mac->addr, QDF_MAC_ADDR_SIZE);
			qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
			if (qdf_mc_timer_get_current_state(
					&random_mac->clear_timer) ==
			    QDF_TIMER_STATE_RUNNING) {
				p2p_debug("random_mac:stop timer on vdev %d addr " QDF_MAC_ADDR_FMT,
					  vdev_id, QDF_MAC_ADDR_REF(addr));
				qdf_mc_timer_stop(&random_mac->clear_timer);
			}

			p2p_clear_mac_filter(
					wlan_vdev_get_psoc(p2p_vdev_obj->vdev),
					vdev_id, addr, freq);

			qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
			p2p_debug("random_mac:noref on vdev %d addr "QDF_MAC_ADDR_FMT,
				  vdev_id, QDF_MAC_ADDR_REF(addr));
		}
		break;
	}
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
p2p_random_mac_handle_tx_done(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
			      uint64_t rnd_cookie, uint32_t duration)
{
	uint32_t i;
	struct action_frame_cookie *action_cookie;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	p2p_debug("random_mac:vdev %d cookie %llu duration %d", vdev_id,
		  rnd_cookie, duration);
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
						vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("p2p vdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		struct action_frame_random_mac *random_mac;
		qdf_freq_t freq;
		uint8_t addr[QDF_MAC_ADDR_SIZE];

		random_mac = &p2p_vdev_obj->random_mac[i];
		if (!random_mac->in_use)
			continue;
		action_cookie = find_action_frame_cookie(
					&random_mac->cookie_list, rnd_cookie);
		if (!action_cookie)
			continue;

		/* If duration is zero then remove the cookie and also remove
		 * the filter from firmware.
		 */
		if (!duration) {
			delete_action_frame_cookie(&random_mac->cookie_list,
						   action_cookie);
			p2p_debug("random mac:clear mac addr on vdev %d addr " QDF_MAC_ADDR_FMT,
				  vdev_id, QDF_MAC_ADDR_REF(random_mac->addr));

			if (qdf_list_empty(&random_mac->cookie_list)) {
				random_mac->in_use = false;
				freq = random_mac->freq;
				qdf_mem_copy(addr, random_mac->addr,
					     QDF_MAC_ADDR_SIZE);
				qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
				p2p_clear_mac_filter(
					wlan_vdev_get_psoc(p2p_vdev_obj->vdev),
					vdev_id, addr, freq);
				qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
			}
		} else {
			/* As duration is non zero start the timer for this
			 * duration. while the timer is running if tx cancel
			 * comes from supplicant then cookie will be removed
			 * and random mac filter will be removed from firmware.
			 * same thing will happen if timer expires without tx
			 * cancel from supplicant
			 */
			qdf_mem_copy(addr, random_mac->addr, QDF_MAC_ADDR_SIZE);

			qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
			if (qdf_mc_timer_get_current_state(
				&random_mac->clear_timer) ==
				QDF_TIMER_STATE_RUNNING)
				qdf_mc_timer_stop(&random_mac->clear_timer);
			qdf_mc_timer_start(&random_mac->clear_timer, duration);
			p2p_debug("random_mac:start timer on vdev %d addr " QDF_MAC_ADDR_FMT,
				  vdev_id, QDF_MAC_ADDR_REF(addr));

			qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
		}
	}
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return QDF_STATUS_SUCCESS;
}

void p2p_del_all_rand_mac_vdev(struct wlan_objmgr_vdev *vdev)
{
	int32_t i;
	uint32_t freq;
	uint8_t addr[QDF_MAC_ADDR_SIZE];
	struct p2p_vdev_priv_obj *p2p_vdev_obj;

	if (!vdev)
		return;
	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj)
		return;

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		struct action_frame_cookie *action_cookie;
		struct action_frame_cookie *action_cookie_next;

		if (!p2p_vdev_obj->random_mac[i].in_use)
			continue;

		/* empty the list and clear random addr */
		qdf_list_for_each_del(&p2p_vdev_obj->random_mac[i].cookie_list,
				      action_cookie, action_cookie_next,
				      cookie_node) {
			qdf_list_remove_node(
				&p2p_vdev_obj->random_mac[i].cookie_list,
				&action_cookie->cookie_node);
			qdf_mem_free(action_cookie);
		}

		p2p_vdev_obj->random_mac[i].in_use = false;
		freq = p2p_vdev_obj->random_mac[i].freq;
		qdf_mem_copy(addr, p2p_vdev_obj->random_mac[i].addr,
			     QDF_MAC_ADDR_SIZE);
		qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
		qdf_mc_timer_stop(&p2p_vdev_obj->random_mac[i].clear_timer);
		p2p_clear_mac_filter(wlan_vdev_get_psoc(vdev),
				     wlan_vdev_get_id(vdev), addr, freq);
		p2p_debug("random_mac:delall vdev %d freq %d addr "QDF_MAC_ADDR_FMT,
			  wlan_vdev_get_id(vdev), freq, QDF_MAC_ADDR_REF(addr));

		qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
	}
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
}

static void
p2p_del_rand_mac_vdev_enum_handler(struct wlan_objmgr_psoc *psoc,
				   void *obj, void *arg)
{
	struct wlan_objmgr_vdev *vdev = obj;

	if (!vdev) {
		p2p_err("random_mac:invalid vdev");
		return;
	}

	if (!p2p_is_vdev_support_rand_mac(vdev))
		return;

	p2p_del_all_rand_mac_vdev(vdev);
}

void p2p_del_all_rand_mac_soc(struct wlan_objmgr_psoc *soc)
{
	if (!soc) {
		p2p_err("random_mac:soc object is NULL");
		return;
	}

	wlan_objmgr_iterate_obj_list(soc, WLAN_VDEV_OP,
				     p2p_del_rand_mac_vdev_enum_handler,
				     NULL, 0, WLAN_P2P_ID);
}

/**
 * p2p_is_random_mac() - check mac addr is random mac for vdev
 * @soc: soc object
 * @vdev_id: vdev id
 * @mac: mac addr to be added or append
 *
 * This function will check the source mac addr same as vdev's mac addr or not.
 * If not same, then the source mac addr should be random mac addr.
 *
 * Return: true if mac is random mac, otherwise false
 */
static bool
p2p_is_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id, uint8_t *mac)
{
	bool ret = false;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("random_mac:vdev is null");
		return false;
	}

	if (qdf_mem_cmp(wlan_vdev_mlme_get_macaddr(vdev),
			mac, QDF_MAC_ADDR_SIZE))
		ret = true;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return ret;
}

static void p2p_set_mac_filter_callback(bool result, void *context)
{
	struct osif_request *request;
	struct random_mac_priv *priv;

	p2p_debug("random_mac:set random mac filter result %d", result);
	request = osif_request_get(context);
	if (!request) {
		p2p_err("random_mac:invalid response");
		return;
	}

	priv = osif_request_priv(request);
	priv->result = result;

	osif_request_complete(request);
	osif_request_put(request);
}

QDF_STATUS p2p_process_set_rand_mac_rsp(struct p2p_mac_filter_rsp *resp)
{
	struct wlan_objmgr_psoc *soc;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	if (!resp || !resp->p2p_soc_obj || !resp->p2p_soc_obj->soc) {
		p2p_debug("random_mac:set_filter_req is null");
		return QDF_STATUS_E_INVAL;
	}
	p2p_debug("random_mac:process rsp on vdev %d status %d", resp->vdev_id,
		  resp->status);
	soc = resp->p2p_soc_obj->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, resp->vdev_id,
						    WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("random_mac:vdev is null vdev %d", resp->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("random_mac:p2p_vdev_obj is null vdev %d",
			  resp->vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	if (!p2p_vdev_obj->pending_req.soc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		p2p_debug("random_mac:no pending set req for vdev %d",
			  resp->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	p2p_debug("random_mac:get pending req on vdev %d set %d mac filter "QDF_MAC_ADDR_FMT" freq %d",
		  p2p_vdev_obj->pending_req.vdev_id,
		  p2p_vdev_obj->pending_req.set,
		  QDF_MAC_ADDR_REF(p2p_vdev_obj->pending_req.mac),
		  p2p_vdev_obj->pending_req.freq);
	if (p2p_vdev_obj->pending_req.cb)
		p2p_vdev_obj->pending_req.cb(
			!!resp->status, p2p_vdev_obj->pending_req.req_cookie);

	qdf_mem_zero(&p2p_vdev_obj->pending_req,
		     sizeof(p2p_vdev_obj->pending_req));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
p2p_process_set_rand_mac(struct p2p_set_mac_filter_req *set_filter_req)
{
	struct wlan_objmgr_psoc *soc;
	struct wlan_lmac_if_p2p_tx_ops *p2p_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct p2p_set_mac_filter param;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_vdev *vdev;

	if (!set_filter_req || !set_filter_req->soc) {
		p2p_debug("random_mac:set_filter_req is null");
		return QDF_STATUS_E_INVAL;
	}
	p2p_debug("random_mac:vdev %d set %d mac filter "QDF_MAC_ADDR_FMT" freq %d",
		  set_filter_req->vdev_id, set_filter_req->set,
		  QDF_MAC_ADDR_REF(set_filter_req->mac), set_filter_req->freq);

	soc = set_filter_req->soc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
				soc, set_filter_req->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("random_mac:vdev is null vdev %d",
			  set_filter_req->vdev_id);
		goto get_vdev_failed;
	}
	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
				vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		p2p_debug("random_mac:p2p_vdev_obj is null vdev %d",
			  set_filter_req->vdev_id);
		goto get_p2p_obj_failed;
	}
	if (p2p_vdev_obj->pending_req.soc) {
		p2p_debug("random_mac:Busy on vdev %d set %d mac filter "QDF_MAC_ADDR_FMT" freq %d",
			  p2p_vdev_obj->pending_req.vdev_id,
			  p2p_vdev_obj->pending_req.set,
			  QDF_MAC_ADDR_REF(p2p_vdev_obj->pending_req.mac),
			  p2p_vdev_obj->pending_req.freq);
		goto get_p2p_obj_failed;
	}

	p2p_ops = p2p_psoc_get_tx_ops(soc);
	if (p2p_ops && p2p_ops->set_mac_addr_rx_filter_cmd) {
		qdf_mem_zero(&param,  sizeof(param));
		param.vdev_id = set_filter_req->vdev_id;
		qdf_mem_copy(param.mac, set_filter_req->mac,
			     QDF_MAC_ADDR_SIZE);
		param.freq = set_filter_req->freq;
		param.set = set_filter_req->set;
		status = p2p_ops->set_mac_addr_rx_filter_cmd(soc, &param);
		if (status == QDF_STATUS_SUCCESS && set_filter_req->set)
			qdf_mem_copy(&p2p_vdev_obj->pending_req,
				     set_filter_req, sizeof(*set_filter_req));
		p2p_debug("random_mac:p2p set mac addr rx filter, status:%d",
			  status);
	}

get_p2p_obj_failed:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

get_vdev_failed:
	if (status != QDF_STATUS_SUCCESS &&
	    set_filter_req->cb)
		set_filter_req->cb(false, set_filter_req->req_cookie);

	return status;
}

/**
 * p2p_set_mac_filter() - send set mac addr filter cmd
 * @soc: soc
 * @vdev_id: vdev id
 * @mac: mac addr
 * @freq: freq
 * @set: set or clear
 * @cb: callback func to be called when the request completed.
 * @req_cookie: cookie to be returned
 *
 * This function send set random mac addr filter command to p2p component
 * msg core
 *
 * Return: QDF_STATUS_SUCCESS - if sent successfully.
 *         otherwise : failed.
 */
static QDF_STATUS
p2p_set_mac_filter(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		   uint8_t *mac, uint32_t freq, bool set,
		   p2p_request_mgr_callback_t cb, void *req_cookie)
{
	struct p2p_set_mac_filter_req *set_filter_req;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	p2p_debug("random_mac:vdev %d freq %d set %d "QDF_MAC_ADDR_FMT,
		  vdev_id, freq, set, QDF_MAC_ADDR_REF(mac));

	set_filter_req = qdf_mem_malloc(sizeof(*set_filter_req));
	if (!set_filter_req)
		return QDF_STATUS_E_NOMEM;

	set_filter_req->soc = soc;
	set_filter_req->vdev_id = vdev_id;
	set_filter_req->freq = freq;
	qdf_mem_copy(set_filter_req->mac, mac, QDF_MAC_ADDR_SIZE);
	set_filter_req->set = set;
	set_filter_req->cb = cb;
	set_filter_req->req_cookie = req_cookie;

	msg.type = P2P_SET_RANDOM_MAC;
	msg.bodyptr = set_filter_req;
	msg.callback = p2p_process_cmd;
	status = scheduler_post_message(QDF_MODULE_ID_P2P, QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_OS_IF, &msg);
	if (status != QDF_STATUS_SUCCESS)
		qdf_mem_free(set_filter_req);

	return status;
}

QDF_STATUS
p2p_clear_mac_filter(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		     uint8_t *mac, uint32_t freq)
{
	return p2p_set_mac_filter(soc, vdev_id, mac, freq, false, NULL, NULL);
}

bool
p2p_is_vdev_support_rand_mac(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE mode;

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode == QDF_STA_MODE ||
	    mode == QDF_P2P_CLIENT_MODE ||
	    mode == QDF_P2P_DEVICE_MODE)
		return true;
	return false;
}

/**
 * p2p_is_vdev_support_rand_mac_by_id() - check vdev type support random mac
 * mgmt tx or not
 * @soc: soc obj
 * @vdev_id: vdev id
 *
 * Return: true: support random mac mgmt tx
 *             false: not support random mac mgmt tx.
 */
static bool
p2p_is_vdev_support_rand_mac_by_id(struct wlan_objmgr_psoc *soc,
				   uint32_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	bool ret = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev)
		return false;
	ret = p2p_is_vdev_support_rand_mac(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return ret;
}

/**
 * p2p_set_rand_mac() - set random mac address rx filter
 * @soc: soc
 * @vdev_id: vdev id
 * @mac: mac addr
 * @freq: freq
 * @rnd_cookie: cookie to be returned
 *
 * This function will post msg to p2p core to set random mac addr rx filter.
 * It will wait the respone and return the result to caller.
 *
 * Return: true: set successfully
 *             false: failed
 */
static bool
p2p_set_rand_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		 uint8_t *mac, uint32_t freq, uint64_t rnd_cookie)
{
	bool ret = false;
	int err;
	QDF_STATUS status;
	struct osif_request *request;
	static const struct osif_request_params params = {
		.priv_size = sizeof(struct random_mac_priv),
		.timeout_ms = WLAN_WAIT_TIME_SET_RND,
	};
	void *req_cookie;
	struct random_mac_priv *priv;

	request = osif_request_alloc(&params);
	if (!request) {
		p2p_err("Request allocation failure");
		return false;
	}

	req_cookie = osif_request_cookie(request);

	status = p2p_set_mac_filter(soc, vdev_id, mac, freq, true,
				    p2p_set_mac_filter_callback, req_cookie);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("random_mac:set mac fitler failure %d", status);
	} else {
		err = osif_request_wait_for_response(request);
		if (err) {
			p2p_err("random_mac:timeout for set mac fitler %d",
				err);
		} else {
			priv = osif_request_priv(request);
			ret = priv->result;
			p2p_debug("random_mac:vdev %d freq %d result %d "QDF_MAC_ADDR_FMT" rnd_cookie %llu",
				  vdev_id, freq, priv->result,
				  QDF_MAC_ADDR_REF(mac), rnd_cookie);
		}
	}
	osif_request_put(request);

	return ret;
}

/**
 * p2p_mac_clear_timeout() - clear random mac filter timeout
 * @context: timer context
 *
 * This function will clear the mac addr rx filter in target if no
 * reference to it.
 *
 * Return: void
 */
static void p2p_mac_clear_timeout(void *context)
{
	struct action_frame_random_mac *random_mac = context;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	uint32_t freq;
	uint8_t addr[QDF_MAC_ADDR_SIZE];
	uint32_t vdev_id;

	if (!random_mac || !random_mac->p2p_vdev_obj) {
		p2p_err("invalid context for mac_clear timeout");
		return;
	}
	p2p_vdev_obj = random_mac->p2p_vdev_obj;
	if (!p2p_vdev_obj || !p2p_vdev_obj->vdev)
		return;

	qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);

	delete_all_action_frame_cookie(&random_mac->cookie_list);
	random_mac->in_use = false;
	freq = random_mac->freq;
	qdf_mem_copy(addr, random_mac->addr, QDF_MAC_ADDR_SIZE);
	qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);

	vdev_id = wlan_vdev_get_id(p2p_vdev_obj->vdev);
	p2p_debug("random_mac:clear timeout vdev %d " QDF_MAC_ADDR_FMT " freq %d",
		  vdev_id, QDF_MAC_ADDR_REF(addr), freq);

	p2p_clear_mac_filter(wlan_vdev_get_psoc(p2p_vdev_obj->vdev),
			     vdev_id, addr, freq);
}

/**
 * p2p_request_random_mac() - request random mac mgmt tx
 * @soc: soc
 * @vdev_id: vdev id
 * @mac: mac addr
 * @freq: freq
 * @rnd_cookie: cookie to be returned
 * @duration: duration of tx timeout
 *
 * This function will add/append the random mac addr filter entry to vdev.
 * If it is new added entry, it will request to set filter in target.
 *
 * Return: QDF_STATUS_SUCCESS: request successfully
 *             other: failed
 */
static QDF_STATUS
p2p_request_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		       uint8_t *mac, uint32_t freq, uint64_t rnd_cookie,
		       uint32_t duration)
{
	QDF_STATUS status;
	uint32_t i;
	struct wlan_objmgr_vdev *vdev;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;

	status = p2p_add_random_mac(soc, vdev_id, mac, freq, rnd_cookie);
	if (status == QDF_STATUS_E_EXISTS)
		return QDF_STATUS_SUCCESS;

	else if (status != QDF_STATUS_SUCCESS)
		return status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(soc, vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_debug("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(
						vdev, WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		p2p_debug("random_mac:p2p vdev object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (!p2p_set_rand_mac(soc, vdev_id, mac, freq, rnd_cookie)) {
		p2p_debug("random mac: failed to set rand mac address");

		qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
		for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
			if (!qdf_mem_cmp(p2p_vdev_obj->random_mac[i].addr, mac,
					 QDF_MAC_ADDR_SIZE)) {
				qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
				p2p_mac_clear_timeout(
						&p2p_vdev_obj->random_mac[i]);
				status = QDF_STATUS_SUCCESS;
				qdf_spin_lock(&p2p_vdev_obj->random_mac_lock);
				break;
			}
		}
		qdf_spin_unlock(&p2p_vdev_obj->random_mac_lock);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
	return status;
}

void p2p_rand_mac_tx(struct  tx_action_context *tx_action)
{
	struct wlan_objmgr_psoc *soc;
	QDF_STATUS status;

	if (!tx_action || !tx_action->p2p_soc_obj ||
	    !tx_action->p2p_soc_obj->soc)
		return;
	soc = tx_action->p2p_soc_obj->soc;

	if (!tx_action->no_ack && tx_action->chan &&
	    tx_action->buf_len > MIN_MAC_HEADER_LEN &&
	    p2p_is_vdev_support_rand_mac_by_id(soc, tx_action->vdev_id) &&
	    p2p_is_random_mac(soc, tx_action->vdev_id,
			      &tx_action->buf[SRC_MAC_ADDR_OFFSET])) {
		status = p2p_request_random_mac(
					soc, tx_action->vdev_id,
					&tx_action->buf[SRC_MAC_ADDR_OFFSET],
					wlan_chan_to_freq(tx_action->chan),
					tx_action->id,
					tx_action->duration);
		if (status == QDF_STATUS_SUCCESS)
			tx_action->rand_mac_tx = true;
		else
			tx_action->rand_mac_tx = false;
	}
}

void
p2p_rand_mac_tx_done(struct wlan_objmgr_psoc *soc,
		     struct tx_action_context *tx_ctx)
{
	if (!tx_ctx || !tx_ctx->rand_mac_tx || !soc)
		return;

	p2p_random_mac_handle_tx_done(soc, tx_ctx->vdev_id, tx_ctx->id,
				      tx_ctx->duration);
}

void p2p_init_random_mac_vdev(struct p2p_vdev_priv_obj *p2p_vdev_obj)
{
	int32_t i;

	qdf_spinlock_create(&p2p_vdev_obj->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		qdf_mem_zero(&p2p_vdev_obj->random_mac[i],
			     sizeof(struct action_frame_random_mac));
		p2p_vdev_obj->random_mac[i].in_use = false;
		p2p_vdev_obj->random_mac[i].p2p_vdev_obj = p2p_vdev_obj;
		qdf_list_create(&p2p_vdev_obj->random_mac[i].cookie_list, 0);
		qdf_mc_timer_init(&p2p_vdev_obj->random_mac[i].clear_timer,
				  QDF_TIMER_TYPE_SW, p2p_mac_clear_timeout,
				  &p2p_vdev_obj->random_mac[i]);
	}
}

void p2p_deinit_random_mac_vdev(struct p2p_vdev_priv_obj *p2p_vdev_obj)
{
	int32_t i;

	p2p_del_all_rand_mac_vdev(p2p_vdev_obj->vdev);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		qdf_mc_timer_destroy(&p2p_vdev_obj->random_mac[i].clear_timer);
		qdf_list_destroy(&p2p_vdev_obj->random_mac[i].cookie_list);
	}
	qdf_spinlock_destroy(&p2p_vdev_obj->random_mac_lock);
}

QDF_STATUS p2p_process_mgmt_tx(struct tx_action_context *tx_ctx)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *curr_roc_ctx;
	uint8_t *mac_to;
	QDF_STATUS status;

	status = p2p_tx_context_check_valid(tx_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("invalid tx action context");
		if (tx_ctx) {
			if (tx_ctx->buf) {
				p2p_send_tx_conf(tx_ctx, false);
				qdf_mem_free(tx_ctx->buf);
			}
			qdf_mem_free(tx_ctx);
		}
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = tx_ctx->p2p_soc_obj;

	p2p_debug("soc:%pK, tx_ctx:%pK, vdev_id:%d, scan_id:%d, roc_cookie:%llx, chan:%d, buf:%pK, len:%d, off_chan:%d, cck:%d, ack:%d, duration:%d",
		p2p_soc_obj->soc, tx_ctx, tx_ctx->vdev_id,
		tx_ctx->scan_id, tx_ctx->roc_cookie, tx_ctx->chan,
		tx_ctx->buf, tx_ctx->buf_len, tx_ctx->off_chan,
		tx_ctx->no_cck, tx_ctx->no_ack, tx_ctx->duration);

	status = p2p_get_frame_info(tx_ctx->buf, tx_ctx->buf_len,
			&(tx_ctx->frame_info));
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("unsupport frame");
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	/* update P2P connection status with tx frame info */
	mac_to = &(tx_ctx->buf[DST_MAC_ADDR_OFFSET]);
	p2p_tx_update_connection_status(p2p_soc_obj,
		&(tx_ctx->frame_info), mac_to);

	status = p2p_vdev_check_valid(tx_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_debug("invalid vdev or vdev mode");
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	/* Do not wait for ack for probe response */
	if (tx_ctx->frame_info.sub_type == P2P_MGMT_PROBE_RSP &&
		!(tx_ctx->no_ack)) {
		p2p_debug("Force set no ack to 1");
		tx_ctx->no_ack = 1;
	}

	if (!tx_ctx->off_chan || !tx_ctx->chan) {
		if (!tx_ctx->chan)
			p2p_check_and_update_channel(tx_ctx);
		status = p2p_execute_tx_action_frame(tx_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			p2p_err("execute tx fail");
			goto fail;
		} else
			return QDF_STATUS_SUCCESS;
	}

	/* For off channel tx case */
	curr_roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);
	if (curr_roc_ctx && (curr_roc_ctx->chan == tx_ctx->chan)) {
		if ((curr_roc_ctx->roc_state == ROC_STATE_REQUESTED) ||
		    (curr_roc_ctx->roc_state == ROC_STATE_STARTED)) {
			tx_ctx->roc_cookie = (uintptr_t)curr_roc_ctx;
			status = qdf_list_insert_back(
					&p2p_soc_obj->tx_q_roc,
					&tx_ctx->node);
			if (status != QDF_STATUS_SUCCESS) {
				p2p_err("Failed to insert off chan tx context to wait roc req queue");
				goto fail;
			} else
				return QDF_STATUS_SUCCESS;
		} else if (curr_roc_ctx->roc_state == ROC_STATE_ON_CHAN) {
			p2p_adjust_tx_wait(tx_ctx);
			if (curr_roc_ctx->duration < tx_ctx->duration)
				curr_roc_ctx->duration = tx_ctx->duration;
			status = p2p_restart_roc_timer(curr_roc_ctx);
			curr_roc_ctx->tx_ctx = tx_ctx;
			if (status != QDF_STATUS_SUCCESS) {
				p2p_err("restart roc timer fail");
				goto fail;
			}
			status = p2p_execute_tx_action_frame(tx_ctx);
			if (status != QDF_STATUS_SUCCESS) {
				p2p_err("execute tx fail");
				goto fail;
			} else
				return QDF_STATUS_SUCCESS;
		}
	}

	curr_roc_ctx = p2p_find_roc_by_chan(p2p_soc_obj, tx_ctx->chan);
	if (curr_roc_ctx && (curr_roc_ctx->roc_state == ROC_STATE_IDLE)) {
		tx_ctx->roc_cookie = (uintptr_t)curr_roc_ctx;
		status = qdf_list_insert_back(
				&p2p_soc_obj->tx_q_roc,
				&tx_ctx->node);
		if (status != QDF_STATUS_SUCCESS) {
			p2p_err("Failed to insert off chan tx context to wait roc req queue");
			goto fail;
		} else {
			return QDF_STATUS_SUCCESS;
		}
	}

	if (!tx_ctx->duration) {
		tx_ctx->duration = P2P_ACTION_FRAME_DEFAULT_WAIT;
		p2p_debug("use default wait %d",
			  P2P_ACTION_FRAME_DEFAULT_WAIT);
	}
	status = p2p_roc_req_for_tx_action(tx_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to request roc before off chan tx");
		goto fail;
	}

	return QDF_STATUS_SUCCESS;

fail:
	p2p_send_tx_conf(tx_ctx, false);
	qdf_idr_remove(&p2p_soc_obj->p2p_idr, tx_ctx->id);
	qdf_mem_free(tx_ctx->buf);
	qdf_mem_free(tx_ctx);

	return status;
}

QDF_STATUS p2p_process_mgmt_tx_cancel(
	struct cancel_roc_context *cancel_tx)
{
	bool is_roc_q = false;
	bool is_ack_q = false;
	struct tx_action_context *cur_tx_ctx;
	struct p2p_roc_context *cur_roc_ctx;
	struct cancel_roc_context cancel_roc;

	if (!cancel_tx || !(cancel_tx->cookie)) {
		p2p_info("invalid cancel info");
		return QDF_STATUS_SUCCESS;
	}

	cur_tx_ctx = p2p_find_tx_ctx(cancel_tx->p2p_soc_obj,
			cancel_tx->cookie, &is_roc_q, &is_ack_q);
	if (cur_tx_ctx) {
		if (is_roc_q) {
			cancel_roc.p2p_soc_obj =
					cancel_tx->p2p_soc_obj;
			cancel_roc.cookie =
					cur_tx_ctx->roc_cookie;
			p2p_remove_tx_context(cur_tx_ctx);
			return p2p_process_cancel_roc_req(&cancel_roc);
		}
		if (is_ack_q) {
			/*Has tx action frame, waiting for ack*/
			p2p_debug("Waiting for ack, cookie %llx",
				cancel_tx->cookie);
		}
	} else {
		p2p_debug("Failed to find tx ctx by cookie, cookie %llx",
			cancel_tx->cookie);

		cur_roc_ctx = p2p_find_roc_by_tx_ctx(cancel_tx->p2p_soc_obj,
					cancel_tx->cookie);
		if (cur_roc_ctx) {
			p2p_debug("tx ctx:%llx, roc:%pK",
				cancel_tx->cookie, cur_roc_ctx);
			cancel_roc.p2p_soc_obj =
					cancel_tx->p2p_soc_obj;
			cancel_roc.cookie = (uintptr_t) cur_roc_ctx;
			return p2p_process_cancel_roc_req(&cancel_roc);
		}

		p2p_debug("Failed to find roc by tx ctx");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_process_mgmt_tx_ack_cnf(
	struct p2p_tx_conf_event *tx_cnf_event)
{
	struct p2p_tx_cnf tx_cnf;
	struct tx_action_context *tx_ctx;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;

	p2p_soc_obj = tx_cnf_event->p2p_soc_obj;

	if (!p2p_soc_obj || !(p2p_soc_obj->start_param)) {
		qdf_nbuf_free(tx_cnf_event->nbuf);
		p2p_err("Invalid p2p soc object or start parameters");
		return QDF_STATUS_E_INVAL;
	}

	tx_ctx = p2p_find_tx_ctx_by_nbuf(p2p_soc_obj, tx_cnf_event->nbuf);
	qdf_nbuf_free(tx_cnf_event->nbuf);
	if (!tx_ctx) {
		p2p_err("can't find tx_ctx, tx ack comes late");
		return QDF_STATUS_SUCCESS;
	}

	/* disable tx timer */
	p2p_disable_tx_timer(tx_ctx);
	tx_cnf.vdev_id = tx_ctx->vdev_id;
	tx_cnf.action_cookie = (uint64_t)tx_ctx->id;
	tx_cnf.buf = tx_ctx->buf;
	tx_cnf.buf_len = tx_ctx->buf_len;
	tx_cnf.status = tx_cnf_event->status;

	p2p_debug("soc:%pK, vdev_id:%d, action_cookie:%llx, len:%d, status:%d, buf:%pK",
		p2p_soc_obj->soc, tx_cnf.vdev_id,
		tx_cnf.action_cookie, tx_cnf.buf_len,
		tx_cnf.status, tx_cnf.buf);

	p2p_rand_mac_tx_done(p2p_soc_obj->soc, tx_ctx);

	start_param = p2p_soc_obj->start_param;
	if (start_param->tx_cnf_cb)
		start_param->tx_cnf_cb(start_param->tx_cnf_cb_data,
					&tx_cnf);
	else
		p2p_debug("Got tx conf, but no valid up layer callback");

	p2p_remove_tx_context(tx_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_process_rx_mgmt(
	struct p2p_rx_mgmt_event *rx_mgmt_event)
{
	struct p2p_rx_mgmt_frame *rx_mgmt;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;
	struct p2p_frame_info frame_info;
	uint8_t *mac_from;

	p2p_soc_obj = rx_mgmt_event->p2p_soc_obj;
	rx_mgmt = rx_mgmt_event->rx_mgmt;

	if (!p2p_soc_obj || !(p2p_soc_obj->start_param)) {
		p2p_err("Invalid psoc object or start parameters");
		qdf_mem_free(rx_mgmt);
		return QDF_STATUS_E_INVAL;
	}

	p2p_debug("soc:%pK, frame_len:%d, rx_freq:%d, vdev_id:%d, frm_type:%d, rx_rssi:%d, buf:%pK",
		  p2p_soc_obj->soc, rx_mgmt->frame_len,
		  rx_mgmt->rx_freq, rx_mgmt->vdev_id, rx_mgmt->frm_type,
		  rx_mgmt->rx_rssi, rx_mgmt->buf);

	if (rx_mgmt->frm_type == MGMT_ACTION_VENDOR_SPECIFIC) {
		p2p_get_frame_info(rx_mgmt->buf, rx_mgmt->frame_len,
				&frame_info);

		/* update P2P connection status with rx frame info */
		mac_from = &(rx_mgmt->buf[SRC_MAC_ADDR_OFFSET]);
		p2p_rx_update_connection_status(p2p_soc_obj,
						&frame_info, mac_from);

		p2p_debug("action_sub_type %u, action_type %d",
				frame_info.public_action_type,
				frame_info.action_type);

		if ((frame_info.public_action_type ==
			P2P_PUBLIC_ACTION_NOT_SUPPORT) &&
		   (frame_info.action_type ==
			P2P_ACTION_NOT_SUPPORT)) {
			p2p_debug("non-p2p frame, drop it");
			qdf_mem_free(rx_mgmt);
			return QDF_STATUS_SUCCESS;
		} else {
			p2p_debug("p2p frame, extend roc accordingly");
			p2p_extend_roc_timer(p2p_soc_obj, &frame_info);
		}
	}

	if (rx_mgmt->frm_type == MGMT_ACTION_CATEGORY_VENDOR_SPECIFIC)
		p2p_get_frame_info(rx_mgmt->buf, rx_mgmt->frame_len,
				&frame_info);

	start_param = p2p_soc_obj->start_param;
	if (start_param->rx_cb)
		start_param->rx_cb(start_param->rx_cb_data, rx_mgmt);
	else
		p2p_debug("rx mgmt, but no valid up layer callback");

	qdf_mem_free(rx_mgmt);

	return QDF_STATUS_SUCCESS;
}
