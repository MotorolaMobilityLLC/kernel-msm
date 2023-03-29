/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 *  DOC:    wma_scan_roam.c
 *  This file contains functions related to scan and
 *  roaming functionality.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_cfg.h>
#include <cdp_txrx_ctrl.h>

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"
#include "wlan_blm_api.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"
#include "wlan_policy_mgr_api.h"
#include <wlan_utility.h>

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "wma_internal.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include "wlan_reg_services_api.h"
#include "wlan_roam_debug.h"
#include "wlan_mlme_public_struct.h"

/* This is temporary, should be removed */
#include "ol_htt_api.h"
#include <cdp_txrx_handle.h>
#include "wma_he.h"
#include <wlan_scan_public_structs.h>
#include <wlan_scan_ucfg_api.h>
#include "wma_nan_datapath.h"
#include "wlan_mlme_api.h"
#include <wlan_mlme_main.h>
#include <wlan_crypto_global_api.h>
#include <cdp_txrx_mon.h>
#include <cdp_txrx_ctrl.h>
#include "wlan_blm_api.h"
#include "wlan_cm_roam_api.h"
#ifdef FEATURE_WLAN_DIAG_SUPPORT    /* FEATURE_WLAN_DIAG_SUPPORT */
#include "host_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_EXTSCAN
#define WMA_EXTSCAN_CYCLE_WAKE_LOCK_DURATION WAKELOCK_DURATION_RECOMMENDED

/*
 * Maximum number of entires that could be present in the
 * WMI_EXTSCAN_HOTLIST_MATCH_EVENT buffer from the firmware
 */
#define WMA_EXTSCAN_MAX_HOTLIST_ENTRIES 10
#endif

static inline wmi_host_channel_width
wma_map_phy_ch_bw_to_wmi_channel_width(enum phy_ch_width ch_width)
{
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		return WMI_HOST_CHAN_WIDTH_20;
	case CH_WIDTH_40MHZ:
		return WMI_HOST_CHAN_WIDTH_40;
	case CH_WIDTH_80MHZ:
		return WMI_HOST_CHAN_WIDTH_80;
	case CH_WIDTH_160MHZ:
		return WMI_HOST_CHAN_WIDTH_160;
	case CH_WIDTH_5MHZ:
		return WMI_HOST_CHAN_WIDTH_5;
	case CH_WIDTH_10MHZ:
		return WMI_HOST_CHAN_WIDTH_10;
	default:
		return WMI_HOST_CHAN_WIDTH_20;
	}
}

#define WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ      0
#define WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ         1
#define WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ        2
#define WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ 3

/**
 * wma_update_channel_list() - update channel list
 * @handle: wma handle
 * @chan_list: channel list
 *
 * Function is used to update the support channel list in fw.
 *
 * Return: QDF status
 */
QDF_STATUS wma_update_channel_list(WMA_HANDLE handle,
				   tSirUpdateChanList *chan_list)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	int i, len;
	struct scan_chan_list_params *scan_ch_param;
	struct channel_param *chan_p;
	struct ch_params ch_params;

	len = sizeof(struct channel_param) * chan_list->numChan +
		offsetof(struct scan_chan_list_params, ch_param[0]);
	scan_ch_param = qdf_mem_malloc(len);
	if (!scan_ch_param)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(scan_ch_param, len);
	wma_debug("no of channels = %d", chan_list->numChan);
	chan_p = &scan_ch_param->ch_param[0];
	scan_ch_param->nallchans = chan_list->numChan;
	scan_ch_param->max_bw_support_present = true;
	wma_handle->saved_chan.num_channels = chan_list->numChan;
	wma_debug("ht %d, vht %d, vht_24 %d", chan_list->ht_en,
		  chan_list->vht_en, chan_list->vht_24_en);

	for (i = 0; i < chan_list->numChan; ++i) {
		chan_p->mhz = chan_list->chanParam[i].freq;
		chan_p->cfreq1 = chan_p->mhz;
		chan_p->cfreq2 = 0;
		wma_handle->saved_chan.ch_freq_list[i] =
					chan_list->chanParam[i].freq;

		if (chan_list->chanParam[i].dfsSet) {
			chan_p->is_chan_passive = 1;
			chan_p->dfs_set = 1;
		}

		if (chan_list->chanParam[i].nan_disabled)
			chan_p->nan_disabled = 1;

		if (chan_p->mhz < WMA_2_4_GHZ_MAX_FREQ) {
			chan_p->phy_mode = MODE_11G;
			if (chan_list->vht_en && chan_list->vht_24_en)
				chan_p->allow_vht = 1;
		} else {
			chan_p->phy_mode = MODE_11A;
			if (chan_list->vht_en &&
			    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_p->mhz)))
				chan_p->allow_vht = 1;
		}

		if (chan_list->ht_en &&
		    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_p->mhz)))
			chan_p->allow_ht = 1;

		if (chan_list->he_en)
			chan_p->allow_he = 1;

		if (chan_list->chanParam[i].half_rate)
			chan_p->half_rate = 1;
		else if (chan_list->chanParam[i].quarter_rate)
			chan_p->quarter_rate = 1;

		if (wlan_reg_is_6ghz_psc_chan_freq(
			    chan_p->mhz))
			chan_p->psc_channel = 1;

		/*TODO: Set WMI_SET_CHANNEL_MIN_POWER */
		/*TODO: Set WMI_SET_CHANNEL_ANTENNA_MAX */
		/*TODO: WMI_SET_CHANNEL_REG_CLASSID */
		chan_p->maxregpower = chan_list->chanParam[i].pwr;

		ch_params.ch_width = CH_WIDTH_160MHZ;
		wlan_reg_set_channel_params_for_freq(wma_handle->pdev,
						     chan_p->mhz, 0,
						     &ch_params);

		chan_p->max_bw_supported =
		     wma_map_phy_ch_bw_to_wmi_channel_width(ch_params.ch_width);
		chan_p++;
	}

	qdf_status = wmi_unified_scan_chan_list_cmd_send(wma_handle->wmi_handle,
				scan_ch_param);

	if (QDF_IS_STATUS_ERROR(qdf_status))
		wma_err("Failed to send WMI_SCAN_CHAN_LIST_CMDID");

	qdf_mem_free(scan_ch_param);

	return qdf_status;
}

/**
 * wma_handle_disconnect_reason() - Send del sta msg to lim on receiving
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @reason: disconnection reason from fw
 *
 * Return: None
 */
static void wma_handle_disconnect_reason(tp_wma_handle wma_handle,
					 uint32_t vdev_id, uint32_t reason)
{
	tpDeleteStaContext del_sta_ctx;

	del_sta_ctx = qdf_mem_malloc(sizeof(tDeleteStaContext));
	if (!del_sta_ctx)
		return;

	del_sta_ctx->vdev_id = vdev_id;
	del_sta_ctx->reasonCode = reason;
	wma_send_msg(wma_handle, SIR_LIM_DELETE_STA_CONTEXT_IND,
		     (void *)del_sta_ctx, 0);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
int wma_roam_vdev_disconnect_event_handler(void *handle, uint8_t *event,
					   uint32_t len)
{
	WMI_VDEV_DISCONNECT_EVENTID_param_tlvs *param_buf;
	wmi_vdev_disconnect_event_fixed_param *roam_vdev_disc_ev;
	tp_wma_handle wma = (tp_wma_handle)handle;

	if (!event) {
		wma_err("received null event from target");
		return -EINVAL;
	}

	param_buf = (WMI_VDEV_DISCONNECT_EVENTID_param_tlvs *)event;

	roam_vdev_disc_ev = param_buf->fixed_param;
	if (!roam_vdev_disc_ev) {
		wma_err("roam cap event is NULL");
		return -EINVAL;
	}
	if (roam_vdev_disc_ev->vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev id %d", roam_vdev_disc_ev->vdev_id);
		return -EINVAL;
	}

	wma_debug("Received disconnect roam event on vdev_id : %d, reason:%d",
		 roam_vdev_disc_ev->vdev_id, roam_vdev_disc_ev->reason);

	switch (roam_vdev_disc_ev->reason) {
	case WLAN_DISCONNECT_REASON_CSA_SA_QUERY_TIMEOUT:
		wma_handle_disconnect_reason(wma, roam_vdev_disc_ev->vdev_id,
			HAL_DEL_STA_REASON_CODE_SA_QUERY_TIMEOUT);
		break;
	case WLAN_DISCONNECT_REASON_MOVE_TO_CELLULAR:
		wma_handle_disconnect_reason(wma, roam_vdev_disc_ev->vdev_id,
			HAL_DEL_STA_REASON_CODE_BTM_DISASSOC_IMMINENT);
		break;
	default:
		return 0;
	}

	return 0;
}
#endif

/**
 * e_csr_auth_type_to_rsn_authmode() - map csr auth type to rsn authmode
 * @authtype: CSR authtype
 * @encr: CSR Encryption
 *
 * Map CSR's authentication type into RSN auth mode used by firmware
 *
 * Return: WMI RSN auth mode
 */
A_UINT32 e_csr_auth_type_to_rsn_authmode(enum csr_akm_type authtype,
					 eCsrEncryptionType encr)
{
	switch (authtype) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		return WMI_AUTH_OPEN;
	case eCSR_AUTH_TYPE_WPA:
		return WMI_AUTH_WPA;
	case eCSR_AUTH_TYPE_WPA_PSK:
		return WMI_AUTH_WPA_PSK;
	case eCSR_AUTH_TYPE_RSN:
		return WMI_AUTH_RSNA;
	case eCSR_AUTH_TYPE_RSN_PSK:
		return WMI_AUTH_RSNA_PSK;
	case eCSR_AUTH_TYPE_FT_RSN:
		return WMI_AUTH_FT_RSNA;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return WMI_AUTH_FT_RSNA_PSK;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		return WMI_AUTH_WAPI;
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		return WMI_AUTH_WAPI_PSK;
#endif /* FEATURE_WLAN_WAPI */
#ifdef FEATURE_WLAN_ESE
	case eCSR_AUTH_TYPE_CCKM_WPA:
		return WMI_AUTH_CCKM_WPA;
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return WMI_AUTH_CCKM_RSNA;
#endif /* FEATURE_WLAN_ESE */
#ifdef WLAN_FEATURE_11W
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return WMI_AUTH_RSNA_PSK_SHA256;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return WMI_AUTH_RSNA_8021X_SHA256;
#endif /* WLAN_FEATURE_11W */
	case eCSR_AUTH_TYPE_NONE:
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		/* In case of WEP and other keys, NONE means OPEN auth */
		if (encr == eCSR_ENCRYPT_TYPE_WEP40_STATICKEY ||
		    encr == eCSR_ENCRYPT_TYPE_WEP104_STATICKEY ||
		    encr == eCSR_ENCRYPT_TYPE_WEP40 ||
		    encr == eCSR_ENCRYPT_TYPE_WEP104 ||
		    encr == eCSR_ENCRYPT_TYPE_TKIP ||
		    encr == eCSR_ENCRYPT_TYPE_AES ||
		    encr == eCSR_ENCRYPT_TYPE_AES_GCMP ||
		    encr == eCSR_ENCRYPT_TYPE_AES_GCMP_256) {
			return WMI_AUTH_OPEN;
		}
		return WMI_AUTH_NONE;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		return WMI_AUTH_SHARED;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return WMI_AUTH_RSNA_FILS_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return WMI_AUTH_RSNA_FILS_SHA384;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return WMI_AUTH_FT_RSNA_FILS_SHA256;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return WMI_AUTH_FT_RSNA_FILS_SHA384;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return WMI_AUTH_RSNA_SUITE_B_8021X_SHA256;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return WMI_AUTH_RSNA_SUITE_B_8021X_SHA384;
	case eCSR_AUTH_TYPE_OWE:
		return WMI_AUTH_WPA3_OWE;
	case eCSR_AUTH_TYPE_SAE:
		return WMI_AUTH_WPA3_SAE;
	case eCSR_AUTH_TYPE_FT_SAE:
		return WMI_AUTH_FT_RSNA_SAE;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return WMI_AUTH_FT_RSNA_SUITE_B_8021X_SHA384;
	default:
		return WMI_AUTH_NONE;
	}
}

/**
 * e_csr_encryption_type_to_rsn_cipherset() - map csr enc type to ESN cipher
 * @encr: CSR Encryption
 *
 * Map CSR's encryption type into RSN cipher types used by firmware
 *
 * Return: WMI RSN cipher
 */
A_UINT32 e_csr_encryption_type_to_rsn_cipherset(eCsrEncryptionType encr)
{

	switch (encr) {
	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:
	case eCSR_ENCRYPT_TYPE_WEP104:
		return WMI_CIPHER_WEP;
	case eCSR_ENCRYPT_TYPE_TKIP:
		return WMI_CIPHER_TKIP;
	case eCSR_ENCRYPT_TYPE_AES:
		return WMI_CIPHER_AES_CCM;
	/* FWR will use key length to distinguish GCMP 128 or 256 */
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		return WMI_CIPHER_AES_GCM;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
		return WMI_CIPHER_WAPI;
#endif /* FEATURE_WLAN_WAPI */
	case eCSR_ENCRYPT_TYPE_ANY:
		return WMI_CIPHER_ANY;
	case eCSR_ENCRYPT_TYPE_NONE:
	default:
		return WMI_CIPHER_NONE;
	}
}

/**
 * wma_process_set_pdev_ie_req() - process the pdev set IE req
 * @wma: Pointer to wma handle
 * @ie_params: Pointer to IE data.
 *
 * Sends the WMI req to set the IE to FW.
 *
 * Return: None
 */
void wma_process_set_pdev_ie_req(tp_wma_handle wma,
				 struct set_ie_param *ie_params)
{
	if (ie_params->ie_type == DOT11_HT_IE)
		wma_process_set_pdev_ht_ie_req(wma, ie_params);
	if (ie_params->ie_type == DOT11_VHT_IE)
		wma_process_set_pdev_vht_ie_req(wma, ie_params);

	qdf_mem_free(ie_params->ie_ptr);
}

/**
 * wma_process_set_pdev_ht_ie_req() - sends HT IE data to FW
 * @wma: Pointer to wma handle
 * @ie_params: Pointer to IE data.
 * @nss: Nss values to prepare the HT IE.
 *
 * Sends the WMI req to set the HT IE to FW.
 *
 * Return: None
 */
void wma_process_set_pdev_ht_ie_req(tp_wma_handle wma,
				    struct set_ie_param *ie_params)
{
	QDF_STATUS status;
	wmi_pdev_set_ht_ie_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len;
	uint16_t ie_len_pad;
	uint8_t *buf_ptr;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	ie_len_pad = roundup(ie_params->ie_len, sizeof(uint32_t));
	len += ie_len_pad;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return;

	cmd = (wmi_pdev_set_ht_ie_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_pdev_set_ht_ie_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->ie_len = ie_params->ie_len;
	cmd->tx_streams = ie_params->nss;
	cmd->rx_streams = ie_params->nss;
	wma_debug("Setting pdev HT ie with Nss = %u", ie_params->nss);
	buf_ptr = (uint8_t *)cmd + sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_pad);
	if (ie_params->ie_len) {
		qdf_mem_copy(buf_ptr + WMI_TLV_HDR_SIZE,
			     (uint8_t *)ie_params->ie_ptr,
			     ie_params->ie_len);
	}

	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				      WMI_PDEV_SET_HT_CAP_IE_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);
}

/**
 * wma_process_set_pdev_vht_ie_req() - sends VHT IE data to FW
 * @wma: Pointer to wma handle
 * @ie_params: Pointer to IE data.
 * @nss: Nss values to prepare the VHT IE.
 *
 * Sends the WMI req to set the VHT IE to FW.
 *
 * Return: None
 */
void wma_process_set_pdev_vht_ie_req(tp_wma_handle wma,
				     struct set_ie_param *ie_params)
{
	QDF_STATUS status;
	wmi_pdev_set_vht_ie_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint16_t len;
	uint16_t ie_len_pad;
	uint8_t *buf_ptr;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	ie_len_pad = roundup(ie_params->ie_len, sizeof(uint32_t));
	len += ie_len_pad;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return;

	cmd = (wmi_pdev_set_vht_ie_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
					wmi_pdev_set_vht_ie_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->ie_len = ie_params->ie_len;
	cmd->tx_streams = ie_params->nss;
	cmd->rx_streams = ie_params->nss;
	wma_debug("Setting pdev VHT ie with Nss = %u", ie_params->nss);
	buf_ptr = (uint8_t *)cmd + sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_pad);
	if (ie_params->ie_len) {
		qdf_mem_copy(buf_ptr + WMI_TLV_HDR_SIZE,
			     (uint8_t *)ie_params->ie_ptr, ie_params->ie_len);
	}

	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				      WMI_PDEV_SET_VHT_CAP_IE_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);
}

/**
 * wma_roam_scan_bmiss_cnt() - set bmiss count to fw
 * @wma_handle: wma handle
 * @first_bcnt: first bmiss count
 * @final_bcnt: final bmiss count
 * @vdev_id: vdev id
 *
 * set first & final biss count to fw.
 *
 * Return: QDF status
 */
QDF_STATUS wma_roam_scan_bmiss_cnt(tp_wma_handle wma_handle,
				   A_INT32 first_bcnt,
				   A_UINT32 final_bcnt, uint32_t vdev_id)
{
	QDF_STATUS status;

	wma_debug("first_bcnt: %d, final_bcnt: %d", first_bcnt, final_bcnt);

	status = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				    WMI_VDEV_PARAM_BMISS_FIRST_BCNT,
				    first_bcnt);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("wma_vdev_set_param WMI_VDEV_PARAM_BMISS_FIRST_BCNT returned Error %d",
			status);
		return status;
	}

	status = wma_vdev_set_param(wma_handle->wmi_handle, vdev_id,
				    WMI_VDEV_PARAM_BMISS_FINAL_BCNT,
				    final_bcnt);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("wma_vdev_set_param WMI_VDEV_PARAM_BMISS_FINAL_BCNT returned Error %d",
			status);
		return status;
	}

	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void
wma_send_roam_preauth_status(tp_wma_handle wma_handle,
			     struct wmi_roam_auth_status_params *params)
{
	QDF_STATUS status;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, cannot send roam prauth status");
		return;
	}

	status = wmi_unified_send_roam_preauth_status(wma_handle->wmi_handle,
						      params);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to send disconnect roam preauth status");
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD

/**
 * wma_process_roam_invoke() - send roam invoke command to fw.
 * @handle: wma handle
 * @roaminvoke: roam invoke command
 *
 * Send roam invoke command to fw for fastreassoc.
 *
 * Return: none
 */
void wma_process_roam_invoke(WMA_HANDLE handle,
		struct wma_roam_invoke_cmd *roaminvoke)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint32_t ch_hz;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not send roam invoke");
		goto free_frame_buf;
	}

	if (!wma_is_vdev_valid(roaminvoke->vdev_id)) {
		wma_err("Invalid vdev id:%d", roaminvoke->vdev_id);
		goto free_frame_buf;
	}
	ch_hz = roaminvoke->ch_freq;
	wmi_unified_roam_invoke_cmd(wma_handle->wmi_handle,
				(struct wmi_roam_invoke_cmd *)roaminvoke,
				ch_hz);
free_frame_buf:
	if (roaminvoke->frame_len) {
		qdf_mem_free(roaminvoke->frame_buf);
		roaminvoke->frame_buf = NULL;
	}
}

/**
 * wma_process_roam_synch_fail() -roam synch failure handle
 * @handle: wma handle
 * @synch_fail: roam synch fail parameters
 *
 * Return: none
 */
void wma_process_roam_synch_fail(WMA_HANDLE handle,
				 struct roam_offload_synch_fail *synch_fail)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not clean-up roam synch");
		return;
	}
	wlan_roam_debug_log(synch_fail->session_id,
			    DEBUG_ROAM_SYNCH_FAIL,
			    DEBUG_INVALID_PEER_ID, NULL, NULL, 0, 0);
}

/**
 * wma_free_roam_synch_frame_ind() - Free the bcn_probe_rsp, reassoc_req,
 * reassoc_rsp received as part of the ROAM_SYNC_FRAME event
 *
 * @iface - interaface corresponding to a vdev
 *
 * This API is used to free the buffer allocated during the ROAM_SYNC_FRAME
 * event
 *
 */
static void wma_free_roam_synch_frame_ind(struct wma_txrx_node *iface)
{
	if (iface->roam_synch_frame_ind.bcn_probe_rsp) {
		qdf_mem_free(iface->roam_synch_frame_ind.bcn_probe_rsp);
		iface->roam_synch_frame_ind.bcn_probe_rsp_len = 0;
		iface->roam_synch_frame_ind.bcn_probe_rsp = NULL;
	}
	if (iface->roam_synch_frame_ind.reassoc_req) {
		qdf_mem_free(iface->roam_synch_frame_ind.reassoc_req);
		iface->roam_synch_frame_ind.reassoc_req_len = 0;
		iface->roam_synch_frame_ind.reassoc_req = NULL;
	}
	if (iface->roam_synch_frame_ind.reassoc_rsp) {
		qdf_mem_free(iface->roam_synch_frame_ind.reassoc_rsp);
		iface->roam_synch_frame_ind.reassoc_rsp_len = 0;
		iface->roam_synch_frame_ind.reassoc_rsp = NULL;
	}
}

/**
 * wma_fill_data_synch_frame_event() - Fill the the roam sync data buffer using
 * synch frame event data
 * @wma: Global WMA Handle
 * @roam_synch_ind_ptr: Buffer to be filled
 * @param_buf: Source buffer
 *
 * Firmware sends all the required information required for roam
 * synch propagation as TLV's and stored in param_buf. These
 * parameters are parsed and filled into the roam synch indication
 * buffer which will be used at different layers for propagation.
 *
 * Return: None
 */
static void wma_fill_data_synch_frame_event(tp_wma_handle wma,
				struct roam_offload_synch_ind *roam_synch_ind_ptr,
				struct wma_txrx_node *iface)
{
	uint8_t *bcn_probersp_ptr;
	uint8_t *reassoc_rsp_ptr;
	uint8_t *reassoc_req_ptr;

	/* Beacon/Probe Rsp data */
	roam_synch_ind_ptr->beaconProbeRespOffset =
		sizeof(struct roam_offload_synch_ind);
	bcn_probersp_ptr = (uint8_t *) roam_synch_ind_ptr +
		roam_synch_ind_ptr->beaconProbeRespOffset;
	roam_synch_ind_ptr->beaconProbeRespLength =
		iface->roam_synch_frame_ind.bcn_probe_rsp_len;
	qdf_mem_copy(bcn_probersp_ptr,
		iface->roam_synch_frame_ind.bcn_probe_rsp,
		roam_synch_ind_ptr->beaconProbeRespLength);
	qdf_mem_free(iface->roam_synch_frame_ind.bcn_probe_rsp);
		iface->roam_synch_frame_ind.bcn_probe_rsp = NULL;

	/* ReAssoc Rsp data */
	roam_synch_ind_ptr->reassocRespOffset =
		sizeof(struct roam_offload_synch_ind) +
		roam_synch_ind_ptr->beaconProbeRespLength;
	roam_synch_ind_ptr->reassocRespLength =
		iface->roam_synch_frame_ind.reassoc_rsp_len;
	reassoc_rsp_ptr = (uint8_t *) roam_synch_ind_ptr +
			  roam_synch_ind_ptr->reassocRespOffset;
	qdf_mem_copy(reassoc_rsp_ptr,
		     iface->roam_synch_frame_ind.reassoc_rsp,
		     roam_synch_ind_ptr->reassocRespLength);
	qdf_mem_free(iface->roam_synch_frame_ind.reassoc_rsp);
	iface->roam_synch_frame_ind.reassoc_rsp = NULL;

	/* ReAssoc Req data */
	roam_synch_ind_ptr->reassoc_req_offset =
		sizeof(struct roam_offload_synch_ind) +
		roam_synch_ind_ptr->beaconProbeRespLength +
		roam_synch_ind_ptr->reassocRespLength;
	roam_synch_ind_ptr->reassoc_req_length =
		iface->roam_synch_frame_ind.reassoc_req_len;
	reassoc_req_ptr = (uint8_t *) roam_synch_ind_ptr +
			  roam_synch_ind_ptr->reassoc_req_offset;
	qdf_mem_copy(reassoc_req_ptr,
		     iface->roam_synch_frame_ind.reassoc_req,
		     roam_synch_ind_ptr->reassoc_req_length);
	qdf_mem_free(iface->roam_synch_frame_ind.reassoc_req);
	iface->roam_synch_frame_ind.reassoc_req = NULL;
}

/**
 * wma_fill_data_synch_event() - Fill the the roam sync data buffer using synch
 * event data
 * @wma: Global WMA Handle
 * @roam_synch_ind_ptr: Buffer to be filled
 * @param_buf: Source buffer
 *
 * Firmware sends all the required information required for roam
 * synch propagation as TLV's and stored in param_buf. These
 * parameters are parsed and filled into the roam synch indication
 * buffer which will be used at different layers for propagation.
 *
 * Return: None
 */
static void wma_fill_data_synch_event(tp_wma_handle wma,
				struct roam_offload_synch_ind *roam_synch_ind_ptr,
				WMI_ROAM_SYNCH_EVENTID_param_tlvs *param_buf)
{
	uint8_t *bcn_probersp_ptr;
	uint8_t *reassoc_rsp_ptr;
	uint8_t *reassoc_req_ptr;
	wmi_roam_synch_event_fixed_param *synch_event;

	synch_event = param_buf->fixed_param;

	/* Beacon/Probe Rsp data */
	roam_synch_ind_ptr->beaconProbeRespOffset =
		sizeof(struct roam_offload_synch_ind);
	bcn_probersp_ptr = (uint8_t *) roam_synch_ind_ptr +
		roam_synch_ind_ptr->beaconProbeRespOffset;
	roam_synch_ind_ptr->beaconProbeRespLength =
		synch_event->bcn_probe_rsp_len;
	qdf_mem_copy(bcn_probersp_ptr, param_buf->bcn_probe_rsp_frame,
		     roam_synch_ind_ptr->beaconProbeRespLength);
	/* ReAssoc Rsp data */
	roam_synch_ind_ptr->reassocRespOffset =
		sizeof(struct roam_offload_synch_ind) +
		roam_synch_ind_ptr->beaconProbeRespLength;
	roam_synch_ind_ptr->reassocRespLength = synch_event->reassoc_rsp_len;
	reassoc_rsp_ptr = (uint8_t *) roam_synch_ind_ptr +
			  roam_synch_ind_ptr->reassocRespOffset;
	qdf_mem_copy(reassoc_rsp_ptr,
		     param_buf->reassoc_rsp_frame,
		     roam_synch_ind_ptr->reassocRespLength);

	/* ReAssoc Req data */
	roam_synch_ind_ptr->reassoc_req_offset =
		sizeof(struct roam_offload_synch_ind) +
		roam_synch_ind_ptr->beaconProbeRespLength +
		roam_synch_ind_ptr->reassocRespLength;
	roam_synch_ind_ptr->reassoc_req_length = synch_event->reassoc_req_len;
	reassoc_req_ptr = (uint8_t *) roam_synch_ind_ptr +
			  roam_synch_ind_ptr->reassoc_req_offset;
	qdf_mem_copy(reassoc_req_ptr, param_buf->reassoc_req_frame,
		     roam_synch_ind_ptr->reassoc_req_length);
}

/**
 * wma_fill_roam_synch_buffer() - Fill the the roam sync buffer
 * @wma: Global WMA Handle
 * @roam_synch_ind_ptr: Buffer to be filled
 * @param_buf: Source buffer
 *
 * Firmware sends all the required information required for roam
 * synch propagation as TLV's and stored in param_buf. These
 * parameters are parsed and filled into the roam synch indication
 * buffer which will be used at different layers for propagation.
 *
 * Return: Success or Failure
 */
static int wma_fill_roam_synch_buffer(tp_wma_handle wma,
				struct roam_offload_synch_ind *roam_synch_ind_ptr,
				WMI_ROAM_SYNCH_EVENTID_param_tlvs *param_buf)
{
	wmi_roam_synch_event_fixed_param *synch_event;
	wmi_channel *chan;
	wmi_key_material *key;
	wmi_key_material_ext *key_ft;
	struct wma_txrx_node *iface = NULL;
	wmi_roam_fils_synch_tlv_param *fils_info;
	wmi_roam_pmk_cache_synch_tlv_param *pmk_cache_info;
	int status = -EINVAL;
	uint8_t kck_len;
	uint8_t kek_len;

	synch_event = param_buf->fixed_param;
	roam_synch_ind_ptr->roamed_vdev_id = synch_event->vdev_id;
	roam_synch_ind_ptr->authStatus = synch_event->auth_status;
	roam_synch_ind_ptr->roamReason = synch_event->roam_reason;
	roam_synch_ind_ptr->rssi = synch_event->rssi;
	iface = &wma->interfaces[synch_event->vdev_id];
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&synch_event->bssid,
				   roam_synch_ind_ptr->bssid.bytes);
	wma_debug("roamedVdevId %d authStatus %d roamReason %d rssi %d isBeacon %d",
		 roam_synch_ind_ptr->roamed_vdev_id,
		 roam_synch_ind_ptr->authStatus, roam_synch_ind_ptr->roamReason,
		 roam_synch_ind_ptr->rssi, roam_synch_ind_ptr->isBeacon);

	if (!QDF_IS_STATUS_SUCCESS(
		wma->csr_roam_synch_cb(wma->mac_context, roam_synch_ind_ptr,
				       NULL, SIR_ROAMING_DEREGISTER_STA))) {
		wma_err("LFR3: CSR Roam synch cb failed");
		wma_free_roam_synch_frame_ind(iface);
		return status;
	}

	/*
	 * If lengths of bcn_probe_rsp, reassoc_req and reassoc_rsp are zero in
	 * synch_event driver would have received bcn_probe_rsp, reassoc_req
	 * and reassoc_rsp via the event WMI_ROAM_SYNCH_FRAME_EVENTID
	 */
	if ((!synch_event->bcn_probe_rsp_len) &&
		(!synch_event->reassoc_req_len) &&
		(!synch_event->reassoc_rsp_len)) {
		if (!iface->roam_synch_frame_ind.bcn_probe_rsp) {
			wma_err("LFR3: bcn_probe_rsp is NULL");
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   bcn_probe_rsp);
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		if (!iface->roam_synch_frame_ind.reassoc_rsp) {
			wma_err("LFR3: reassoc_rsp is NULL");
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   reassoc_rsp);
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		if (!iface->roam_synch_frame_ind.reassoc_req) {
			wma_err("LFR3: reassoc_req is NULL");
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   reassoc_req);
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		wma_fill_data_synch_frame_event(wma, roam_synch_ind_ptr, iface);
	} else {
		wma_fill_data_synch_event(wma, roam_synch_ind_ptr, param_buf);
	}
	chan = param_buf->chan;
	if (chan) {
		roam_synch_ind_ptr->chan_freq = chan->mhz;
		roam_synch_ind_ptr->phy_mode =
			wma_fw_to_host_phymode(WMI_GET_CHANNEL_MODE(chan));
	} else {
		roam_synch_ind_ptr->phy_mode = WLAN_PHYMODE_AUTO;
	}

	key = param_buf->key;
	key_ft = param_buf->key_ext;
	if (key) {
		roam_synch_ind_ptr->kck_len = SIR_KCK_KEY_LEN;
		qdf_mem_copy(roam_synch_ind_ptr->kck, key->kck,
			     SIR_KCK_KEY_LEN);
		roam_synch_ind_ptr->kek_len = SIR_KEK_KEY_LEN;
		qdf_mem_copy(roam_synch_ind_ptr->kek, key->kek,
			     SIR_KEK_KEY_LEN);
		qdf_mem_copy(roam_synch_ind_ptr->replay_ctr,
			     key->replay_counter, SIR_REPLAY_CTR_LEN);
	} else if (key_ft) {
		/*
		 * For AKM 00:0F:AC (FT suite-B-SHA384)
		 * KCK-bits:192 KEK-bits:256
		 * Firmware sends wmi_key_material_ext tlv now only if
		 * auth is FT Suite-B SHA-384 auth. If further new suites
		 * are added, add logic to get kck, kek bits based on
		 * akm protocol
		 */
		kck_len = KCK_192BIT_KEY_LEN;
		kek_len = KEK_256BIT_KEY_LEN;

		roam_synch_ind_ptr->kck_len = kck_len;
		qdf_mem_copy(roam_synch_ind_ptr->kck,
			     key_ft->key_buffer, kck_len);

		roam_synch_ind_ptr->kek_len = kek_len;
		qdf_mem_copy(roam_synch_ind_ptr->kek,
			     (key_ft->key_buffer + kck_len),
			     kek_len);

		qdf_mem_copy(roam_synch_ind_ptr->replay_ctr,
			     (key_ft->key_buffer + kek_len + kck_len),
			     SIR_REPLAY_CTR_LEN);
	}

	if (param_buf->hw_mode_transition_fixed_param)
		wma_process_pdev_hw_mode_trans_ind(wma,
		    param_buf->hw_mode_transition_fixed_param,
		    param_buf->wmi_pdev_set_hw_mode_response_vdev_mac_mapping,
		    &roam_synch_ind_ptr->hw_mode_trans_ind);
	else
		wma_debug("hw_mode transition fixed param is NULL");

	fils_info = param_buf->roam_fils_synch_info;
	if (fils_info) {
		if ((fils_info->kek_len > SIR_KEK_KEY_LEN_FILS) ||
		    (fils_info->pmk_len > SIR_PMK_LEN)) {
			wma_err("Invalid kek_len %d or pmk_len %d",
				 fils_info->kek_len,
				 fils_info->pmk_len);
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}

		roam_synch_ind_ptr->kek_len = fils_info->kek_len;
		qdf_mem_copy(roam_synch_ind_ptr->kek, fils_info->kek,
			     fils_info->kek_len);

		roam_synch_ind_ptr->pmk_len = fils_info->pmk_len;
		qdf_mem_copy(roam_synch_ind_ptr->pmk, fils_info->pmk,
			     fils_info->pmk_len);

		qdf_mem_copy(roam_synch_ind_ptr->pmkid, fils_info->pmkid,
			     PMKID_LEN);

		roam_synch_ind_ptr->update_erp_next_seq_num =
				fils_info->update_erp_next_seq_num;
		roam_synch_ind_ptr->next_erp_seq_num =
				fils_info->next_erp_seq_num;

		wma_debug("Update ERP Seq Num %d, Next ERP Seq Num %d",
			 roam_synch_ind_ptr->update_erp_next_seq_num,
			 roam_synch_ind_ptr->next_erp_seq_num);
	}

	pmk_cache_info = param_buf->roam_pmk_cache_synch_info;
	if (pmk_cache_info && (pmk_cache_info->pmk_len)) {
		if (pmk_cache_info->pmk_len > SIR_PMK_LEN) {
			wma_err("Invalid pmk_len %d",
				 pmk_cache_info->pmk_len);
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}

		roam_synch_ind_ptr->pmk_len = pmk_cache_info->pmk_len;
		qdf_mem_copy(roam_synch_ind_ptr->pmk,
			     pmk_cache_info->pmk, pmk_cache_info->pmk_len);
		qdf_mem_copy(roam_synch_ind_ptr->pmkid,
			     pmk_cache_info->pmkid, PMKID_LEN);
	}
	wma_free_roam_synch_frame_ind(iface);
	return 0;
}

static void wma_update_roamed_peer_unicast_cipher(tp_wma_handle wma,
						  uint32_t uc_cipher,
						  uint32_t cipher_cap,
						  uint8_t *peer_mac)
{
	struct wlan_objmgr_peer *peer;

	if (!peer_mac) {
		wma_err("wma ctx or peer mac is NULL");
		return;
	}

	peer = wlan_objmgr_get_peer(wma->psoc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    peer_mac, WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			QDF_MAC_ADDR_REF(peer_mac));
		return;
	}
	wlan_crypto_set_peer_param(peer, WLAN_CRYPTO_PARAM_CIPHER_CAP,
				   cipher_cap);
	wlan_crypto_set_peer_param(peer, WLAN_CRYPTO_PARAM_UCAST_CIPHER,
				   uc_cipher);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	wma_debug("Set unicast cipher %x and cap %x for "QDF_MAC_ADDR_FMT,
		  uc_cipher, cipher_cap, QDF_MAC_ADDR_REF(peer_mac));
}

static void wma_get_peer_uc_cipher(tp_wma_handle wma, uint8_t *peer_mac,
				   uint32_t *uc_cipher, uint32_t *cipher_cap)
{
	int32_t cipher, cap;
	struct wlan_objmgr_peer *peer;

	if (!peer_mac) {
		wma_err("wma ctx or peer_mac is NULL");
		return;
	}
	peer = wlan_objmgr_get_peer(wma->psoc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    peer_mac, WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			 QDF_MAC_ADDR_REF(peer_mac));
		return;
	}

	cipher = wlan_crypto_get_peer_param(peer,
					    WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	cap = wlan_crypto_get_peer_param(peer, WLAN_CRYPTO_PARAM_CIPHER_CAP);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	if (cipher < 0 || cap < 0) {
		wma_err("Invalid mgmt cipher");
		return;
	}

	if (uc_cipher)
		*uc_cipher = cipher;
	if (cipher_cap)
		*cipher_cap = cap;
}

/**
 * wma_roam_update_vdev() - Update the STA and BSS
 * @wma: Global WMA Handle
 * @roam_synch_ind_ptr: Information needed for roam sync propagation
 *
 * This function will perform all the vdev related operations with
 * respect to the self sta and the peer after roaming and completes
 * the roam synch propagation with respect to WMA layer.
 *
 * Return: None
 */
static void
wma_roam_update_vdev(tp_wma_handle wma,
		     struct roam_offload_synch_ind *roam_synch_ind_ptr)
{
	tDeleteStaParams *del_sta_params;
	tAddStaParams *add_sta_params;
	uint32_t uc_cipher = 0, cipher_cap = 0;
	uint8_t vdev_id, *bssid;

	vdev_id = roam_synch_ind_ptr->roamed_vdev_id;
	wma->interfaces[vdev_id].nss = roam_synch_ind_ptr->nss;

	del_sta_params = qdf_mem_malloc(sizeof(*del_sta_params));
	if (!del_sta_params) {
		return;
	}

	add_sta_params = qdf_mem_malloc(sizeof(*add_sta_params));
	if (!add_sta_params) {
		qdf_mem_free(del_sta_params);
		return;
	}

	qdf_mem_zero(del_sta_params, sizeof(*del_sta_params));
	qdf_mem_zero(add_sta_params, sizeof(*add_sta_params));

	del_sta_params->smesessionId = vdev_id;
	add_sta_params->staType = STA_ENTRY_SELF;
	add_sta_params->smesessionId = vdev_id;
	qdf_mem_copy(&add_sta_params->bssId, &roam_synch_ind_ptr->bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	add_sta_params->assocId = roam_synch_ind_ptr->aid;

	bssid = wma_get_vdev_bssid(wma->interfaces[vdev_id].vdev);
	if (!bssid) {
		wma_err("Failed to get bssid for vdev_%d", vdev_id);
		return;
	}

	/*
	 * Get uc cipher of old peer to update new peer as it doesnt
	 * change in roaming
	 */
	wma_get_peer_uc_cipher(wma, bssid,
			       &uc_cipher, &cipher_cap);

	wma_delete_sta(wma, del_sta_params);
	wma_delete_bss(wma, vdev_id);
	wma_create_peer(wma, roam_synch_ind_ptr->bssid.bytes,
			WMI_PEER_TYPE_DEFAULT, vdev_id);

	/* Update new peer's uc cipher */
	wma_update_roamed_peer_unicast_cipher(wma, uc_cipher, cipher_cap,
					      roam_synch_ind_ptr->bssid.bytes);
	wma_add_bss_lfr3(wma, roam_synch_ind_ptr->add_bss_params);
	wma_add_sta(wma, add_sta_params);
	qdf_mem_copy(bssid, roam_synch_ind_ptr->bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	lim_fill_roamed_peer_twt_caps(wma->mac_context, vdev_id,
				      roam_synch_ind_ptr);
	qdf_mem_free(add_sta_params);
}

static void wma_update_phymode_on_roam(tp_wma_handle wma, uint8_t *bssid,
				       wmi_channel *chan,
				       struct wma_txrx_node *iface)
{
	enum wlan_phymode bss_phymode;
	struct wlan_channel *des_chan;
	struct wlan_channel *bss_chan;
	struct vdev_mlme_obj *vdev_mlme;
	uint8_t channel;
	struct wlan_objmgr_pdev *pdev;
	qdf_freq_t sec_ch_2g_freq = 0;
	struct ch_params ch_params = {0};

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!vdev_mlme)
		return;

	pdev = wlan_vdev_get_pdev(vdev_mlme->vdev);

	channel = wlan_freq_to_chan(iface->ch_freq);
	if (chan)
		bss_phymode =
			wma_fw_to_host_phymode(WMI_GET_CHANNEL_MODE(chan));
	else
		wma_get_phy_mode_cb(iface->ch_freq,
				    iface->chan_width, &bss_phymode);

	/* Update vdev mlme channel info after roaming */
	des_chan = wlan_vdev_mlme_get_des_chan(iface->vdev);
	bss_chan = wlan_vdev_mlme_get_bss_chan(iface->vdev);
	des_chan->ch_phymode = bss_phymode;
	des_chan->ch_width = iface->chan_width;
	if (chan) {
		des_chan->ch_freq = chan->mhz;
		ch_params.ch_width = des_chan->ch_width;
		if (wlan_reg_is_24ghz_ch_freq(des_chan->ch_freq) &&
		    des_chan->ch_width == CH_WIDTH_40MHZ &&
		    chan->band_center_freq1) {
			if (des_chan->ch_freq < chan->band_center_freq1)
				sec_ch_2g_freq = des_chan->ch_freq + 20;
			else
				sec_ch_2g_freq = des_chan->ch_freq - 20;
		}
		wlan_reg_set_channel_params_for_freq(pdev, des_chan->ch_freq,
						     sec_ch_2g_freq,
						     &ch_params);
		if (ch_params.ch_width != des_chan->ch_width ||
		    ch_params.mhz_freq_seg0 != chan->band_center_freq1 ||
		    ch_params.mhz_freq_seg1 != chan->band_center_freq2)
			wma_err("ch mismatch host & fw bw (%d %d) seg0 (%d, %d) seg1 (%d, %d)",
				ch_params.ch_width, des_chan->ch_width,
				ch_params.mhz_freq_seg0,
				chan->band_center_freq1,
				ch_params.mhz_freq_seg1,
				chan->band_center_freq2);
		des_chan->ch_cfreq1 = ch_params.mhz_freq_seg0;
		des_chan->ch_cfreq2 = ch_params.mhz_freq_seg1;
		des_chan->ch_width = ch_params.ch_width;
	} else {
		wma_err("LFR3: invalid chan");
	}
	qdf_mem_copy(bss_chan, des_chan, sizeof(struct wlan_channel));

	/* Till conversion is not done in WMI we need to fill fw phy mode */
	vdev_mlme->mgmt.generic.phy_mode = wma_host_to_fw_phymode(bss_phymode);

	/* update new phymode to peer */
	wma_objmgr_set_peer_mlme_phymode(wma, bssid, bss_phymode);

	wma_debug("LFR3: new phymode %d freq %d (bw %d, %d %d)",
		  bss_phymode, des_chan->ch_freq, des_chan->ch_width,
		  des_chan->ch_cfreq1, des_chan->ch_cfreq2);
}

static void wma_post_roam_sync_failure(tp_wma_handle wma, uint8_t vdev_id)
{
	wlan_cm_roam_stop_req(wma->psoc, vdev_id, REASON_ROAM_SYNCH_FAILED);
	wma_debug("In cleanup: RSO Command:%d, reason %d vdev %d",
		  ROAM_SCAN_OFFLOAD_STOP, REASON_ROAM_SYNCH_FAILED, vdev_id);
}

int wma_mlme_roam_synch_event_handler_cb(void *handle, uint8_t *event,
					 uint32_t len)
{
	WMI_ROAM_SYNCH_EVENTID_param_tlvs *param_buf = NULL;
	wmi_roam_synch_event_fixed_param *synch_event = NULL;
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct roam_offload_synch_ind *roam_synch_ind_ptr = NULL;
	struct bss_description *bss_desc_ptr = NULL;
	uint16_t ie_len = 0;
	int status = -EINVAL;
	qdf_time_t roam_synch_received = qdf_get_system_timestamp();
	uint32_t roam_synch_data_len;
	A_UINT32 bcn_probe_rsp_len;
	A_UINT32 reassoc_rsp_len;
	A_UINT32 reassoc_req_len;

	wma_debug("LFR3: Received WMA_ROAM_OFFLOAD_SYNCH_IND");
	if (!event) {
		wma_err("event param null");
		goto cleanup_label;
	}

	param_buf = (WMI_ROAM_SYNCH_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("received null buf from target");
		goto cleanup_label;
	}

	synch_event = param_buf->fixed_param;
	if (!synch_event) {
		wma_err("received null event data from target");
		goto cleanup_label;
	}

	if (synch_event->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", synch_event->vdev_id);
		return status;
	}

	/*
	 * This flag is set during ROAM_START and once this event is being
	 * executed which is a run to completion, no other event can interrupt
	 * this in MC thread context. So, it is OK to reset the flag here as
	 * soon as we receive this event.
	 */
	wma->interfaces[synch_event->vdev_id].roaming_in_progress = false;

	if (synch_event->bcn_probe_rsp_len >
	    param_buf->num_bcn_probe_rsp_frame ||
	    synch_event->reassoc_req_len >
	    param_buf->num_reassoc_req_frame ||
	    synch_event->reassoc_rsp_len >
	    param_buf->num_reassoc_rsp_frame) {
		wma_debug("Invalid synch payload: LEN bcn:%d, req:%d, rsp:%d",
			synch_event->bcn_probe_rsp_len,
			synch_event->reassoc_req_len,
			synch_event->reassoc_rsp_len);
		goto cleanup_label;
	}

	wlan_roam_debug_log(synch_event->vdev_id, DEBUG_ROAM_SYNCH_IND,
			    DEBUG_INVALID_PEER_ID, NULL, NULL,
			    synch_event->bssid.mac_addr31to0,
			    synch_event->bssid.mac_addr47to32);
	DPTRACE(qdf_dp_trace_record_event(QDF_DP_TRACE_EVENT_RECORD,
		synch_event->vdev_id, QDF_TRACE_DEFAULT_PDEV_ID,
		QDF_PROTO_TYPE_EVENT, QDF_ROAM_SYNCH));

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, synch_event->vdev_id)) {
		wma_err("Ignoring RSI since one is already in progress");
		goto cleanup_label;
	}

	/*
	 * All below length fields are unsigned and hence positive numbers.
	 * Maximum number during the addition would be (3 * MAX_LIMIT(UINT32) +
	 * few fixed fields).
	 */
	wma_debug("synch payload: LEN bcn:%d, req:%d, rsp:%d",
			synch_event->bcn_probe_rsp_len,
			synch_event->reassoc_req_len,
			synch_event->reassoc_rsp_len);

	/*
	 * If lengths of bcn_probe_rsp, reassoc_req and reassoc_rsp are zero in
	 * synch_event driver would have received bcn_probe_rsp, reassoc_req
	 * and reassoc_rsp via the event WMI_ROAM_SYNCH_FRAME_EVENTID
	 */
	if ((!synch_event->bcn_probe_rsp_len) &&
		(!synch_event->reassoc_req_len) &&
		(!synch_event->reassoc_rsp_len)) {
		bcn_probe_rsp_len = wma->interfaces[synch_event->vdev_id].
				      roam_synch_frame_ind.
				      bcn_probe_rsp_len;
		reassoc_req_len = wma->interfaces[synch_event->vdev_id].
				      roam_synch_frame_ind.reassoc_req_len;
		reassoc_rsp_len = wma->interfaces[synch_event->vdev_id].
				      roam_synch_frame_ind.reassoc_rsp_len;

		roam_synch_data_len = bcn_probe_rsp_len + reassoc_rsp_len +
			reassoc_req_len + sizeof(struct roam_offload_synch_ind);

		wma_debug("Updated synch payload: LEN bcn:%d, req:%d, rsp:%d",
			 bcn_probe_rsp_len,
			 reassoc_req_len,
			 reassoc_rsp_len);
	} else {
		bcn_probe_rsp_len = synch_event->bcn_probe_rsp_len;
		reassoc_req_len = synch_event->reassoc_req_len;
		reassoc_rsp_len = synch_event->reassoc_rsp_len;

		if (synch_event->bcn_probe_rsp_len > WMI_SVC_MSG_MAX_SIZE)
			goto cleanup_label;
		if (synch_event->reassoc_rsp_len >
		    (WMI_SVC_MSG_MAX_SIZE - synch_event->bcn_probe_rsp_len))
			goto cleanup_label;
		if (synch_event->reassoc_req_len >
		    WMI_SVC_MSG_MAX_SIZE - (synch_event->bcn_probe_rsp_len +
			synch_event->reassoc_rsp_len))
			goto cleanup_label;

		roam_synch_data_len = bcn_probe_rsp_len +
			reassoc_rsp_len + reassoc_req_len;

		/*
		 * Below is the check for the entire size of the message
		 * received from the firmware.
		 */
		if (roam_synch_data_len > WMI_SVC_MSG_MAX_SIZE -
			(sizeof(*synch_event) + sizeof(wmi_channel) +
			 sizeof(wmi_key_material) + sizeof(uint32_t)))
			goto cleanup_label;

		roam_synch_data_len += sizeof(struct roam_offload_synch_ind);
	}

	cds_host_diag_log_work(&wma->roam_ho_wl,
			       WMA_ROAM_HO_WAKE_LOCK_DURATION,
			       WIFI_POWER_EVENT_WAKELOCK_WOW);
	qdf_wake_lock_timeout_acquire(&wma->roam_ho_wl,
				      WMA_ROAM_HO_WAKE_LOCK_DURATION);

	roam_synch_ind_ptr = qdf_mem_malloc(roam_synch_data_len);
	if (!roam_synch_ind_ptr) {
		QDF_ASSERT(roam_synch_ind_ptr);
		status = -ENOMEM;
		goto cleanup_label;
	}
	qdf_mem_zero(roam_synch_ind_ptr, roam_synch_data_len);
	status = wma_fill_roam_synch_buffer(wma,
			roam_synch_ind_ptr, param_buf);
	if (status != 0)
		goto cleanup_label;
	/* 24 byte MAC header and 12 byte to ssid IE */
	if (roam_synch_ind_ptr->beaconProbeRespLength >
			(SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET)) {
		ie_len = roam_synch_ind_ptr->beaconProbeRespLength -
			(SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET);
	} else {
		wma_err("LFR3: Invalid Beacon Length");
		goto cleanup_label;
	}
	bss_desc_ptr = qdf_mem_malloc(sizeof(struct bss_description) + ie_len);
	if (!bss_desc_ptr) {
		QDF_ASSERT(bss_desc_ptr);
		status = -ENOMEM;
		goto cleanup_label;
	}
	qdf_mem_zero(bss_desc_ptr, sizeof(struct bss_description) + ie_len);
	if (QDF_IS_STATUS_ERROR(wma->pe_roam_synch_cb(wma->mac_context,
			roam_synch_ind_ptr, bss_desc_ptr,
			SIR_ROAM_SYNCH_PROPAGATION))) {
		wma_err("LFR3: PE roam synch propagation failed");
		status = -EBUSY;
		goto cleanup_label;
	}

	wma_roam_update_vdev(wma, roam_synch_ind_ptr);

	if (QDF_IS_STATUS_ERROR(wma->csr_roam_synch_cb(wma->mac_context,
				roam_synch_ind_ptr,
				bss_desc_ptr, SIR_ROAM_SYNCH_PROPAGATION))) {
		wma_err("CSR roam synch propagation failed, abort roam");
		status = -EINVAL;
		goto cleanup_label;
	}

	wma_process_roam_synch_complete(wma, synch_event->vdev_id);

	/* update freq and channel width */
	wma->interfaces[synch_event->vdev_id].ch_freq =
		roam_synch_ind_ptr->chan_freq;
	if (roam_synch_ind_ptr->join_rsp)
		wma->interfaces[synch_event->vdev_id].chan_width =
			roam_synch_ind_ptr->join_rsp->vht_channel_width;
	/*
	 * update phy_mode in wma to avoid mismatch in phymode between host and
	 * firmware. The phymode stored in peer->peer_mlme.phymode is
	 * sent to firmware as part of opmode update during either - vht opmode
	 * action frame received or during opmode change detected while
	 * processing beacon. Any mismatch of this value with firmware phymode
	 * results in firmware assert.
	 */
	wma_update_phymode_on_roam(wma, roam_synch_ind_ptr->bssid.bytes,
				   param_buf->chan,
				   &wma->interfaces[synch_event->vdev_id]);

	wma->csr_roam_synch_cb(wma->mac_context, roam_synch_ind_ptr,
			       bss_desc_ptr, SIR_ROAM_SYNCH_COMPLETE);
	wma->interfaces[synch_event->vdev_id].roam_synch_delay =
		qdf_get_system_timestamp() - roam_synch_received;
	wma_debug("LFR3: roam_synch_delay:%d",
		 wma->interfaces[synch_event->vdev_id].roam_synch_delay);
	wma->csr_roam_synch_cb(wma->mac_context, roam_synch_ind_ptr,
			       bss_desc_ptr, SIR_ROAM_SYNCH_NAPI_OFF);

	status = 0;

cleanup_label:
	if (status != 0) {
		if (roam_synch_ind_ptr)
			wma->csr_roam_synch_cb(wma->mac_context,
					       roam_synch_ind_ptr, NULL,
					       SIR_ROAMING_ABORT);
		if (synch_event)
			wma_post_roam_sync_failure(wma, synch_event->vdev_id);
	}
	if (roam_synch_ind_ptr && roam_synch_ind_ptr->join_rsp)
		qdf_mem_free(roam_synch_ind_ptr->join_rsp);
	if (roam_synch_ind_ptr)
		qdf_mem_free(roam_synch_ind_ptr);
	if (bss_desc_ptr)
		qdf_mem_free(bss_desc_ptr);

	return status;
}

int wma_roam_synch_frame_event_handler(void *handle, uint8_t *event,
					uint32_t len)
{
	WMI_ROAM_SYNCH_FRAME_EVENTID_param_tlvs *param_buf = NULL;
	wmi_roam_synch_frame_event_fixed_param *synch_frame_event = NULL;
	tp_wma_handle wma = (tp_wma_handle) handle;
	A_UINT32 vdev_id;
	struct wma_txrx_node *iface = NULL;
	int status = -EINVAL;

	if (!event) {
		wma_err("event param null");
		return status;
	}

	param_buf = (WMI_ROAM_SYNCH_FRAME_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("received null buf from target");
		return status;
	}

	synch_frame_event = param_buf->fixed_param;
	if (!synch_frame_event) {
		wma_err("received null event data from target");
		return status;
	}

	if (synch_frame_event->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d",
			 synch_frame_event->vdev_id);
		return status;
	}

	if (synch_frame_event->bcn_probe_rsp_len >
	    param_buf->num_bcn_probe_rsp_frame ||
	    synch_frame_event->reassoc_req_len >
	    param_buf->num_reassoc_req_frame ||
	    synch_frame_event->reassoc_rsp_len >
	    param_buf->num_reassoc_rsp_frame) {
		wma_err("fixed/actual len err: bcn:%d/%d req:%d/%d rsp:%d/%d",
			 synch_frame_event->bcn_probe_rsp_len,
			 param_buf->num_bcn_probe_rsp_frame,
			 synch_frame_event->reassoc_req_len,
			 param_buf->num_reassoc_req_frame,
			 synch_frame_event->reassoc_rsp_len,
			 param_buf->num_reassoc_rsp_frame);
		return status;
	}

	vdev_id = synch_frame_event->vdev_id;
	iface = &wma->interfaces[vdev_id];

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, vdev_id)) {
		wma_err("Ignoring this event as it is unexpected");
		wma_free_roam_synch_frame_ind(iface);
		return status;
	}

	wma_debug("LFR3: Roam synch frame payload: LEN bcn:%d, req:%d, rsp:%d morefrag: %d",
		  synch_frame_event->bcn_probe_rsp_len,
		  synch_frame_event->reassoc_req_len,
		  synch_frame_event->reassoc_rsp_len,
		  synch_frame_event->more_frag);

	if (synch_frame_event->bcn_probe_rsp_len) {
		iface->roam_synch_frame_ind.bcn_probe_rsp_len =
				synch_frame_event->bcn_probe_rsp_len;
		iface->roam_synch_frame_ind.is_beacon =
				synch_frame_event->is_beacon;

		if (iface->roam_synch_frame_ind.bcn_probe_rsp)
			qdf_mem_free(iface->roam_synch_frame_ind.
				     bcn_probe_rsp);
		iface->roam_synch_frame_ind.bcn_probe_rsp =
			qdf_mem_malloc(iface->roam_synch_frame_ind.
				       bcn_probe_rsp_len);
		if (!iface->roam_synch_frame_ind.bcn_probe_rsp) {
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   bcn_probe_rsp);
			status = -ENOMEM;
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		qdf_mem_copy(iface->roam_synch_frame_ind.
			bcn_probe_rsp,
			param_buf->bcn_probe_rsp_frame,
			iface->roam_synch_frame_ind.bcn_probe_rsp_len);
	}

	if (synch_frame_event->reassoc_req_len) {
		iface->roam_synch_frame_ind.reassoc_req_len =
				synch_frame_event->reassoc_req_len;

		if (iface->roam_synch_frame_ind.reassoc_req)
			qdf_mem_free(iface->roam_synch_frame_ind.reassoc_req);
		iface->roam_synch_frame_ind.reassoc_req =
			qdf_mem_malloc(iface->roam_synch_frame_ind.
				       reassoc_req_len);
		if (!iface->roam_synch_frame_ind.reassoc_req) {
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   reassoc_req);
			status = -ENOMEM;
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		qdf_mem_copy(iface->roam_synch_frame_ind.reassoc_req,
			     param_buf->reassoc_req_frame,
			     iface->roam_synch_frame_ind.reassoc_req_len);
	}

	if (synch_frame_event->reassoc_rsp_len) {
		iface->roam_synch_frame_ind.reassoc_rsp_len =
				synch_frame_event->reassoc_rsp_len;

		if (iface->roam_synch_frame_ind.reassoc_rsp)
			qdf_mem_free(iface->roam_synch_frame_ind.reassoc_rsp);

		iface->roam_synch_frame_ind.reassoc_rsp =
			qdf_mem_malloc(iface->roam_synch_frame_ind.
				       reassoc_rsp_len);
		if (!iface->roam_synch_frame_ind.reassoc_rsp) {
			QDF_ASSERT(iface->roam_synch_frame_ind.
				   reassoc_rsp);
			status = -ENOMEM;
			wma_free_roam_synch_frame_ind(iface);
			return status;
		}
		qdf_mem_copy(iface->roam_synch_frame_ind.reassoc_rsp,
			     param_buf->reassoc_rsp_frame,
			     iface->roam_synch_frame_ind.reassoc_rsp_len);
	}
	return 0;
}

/**
 * __wma_roam_synch_event_handler() - roam synch event handler
 * @handle: wma handle
 * @event: event data
 * @len: length of data
 *
 * This function is roam synch event handler.It sends roam
 * indication for upper layer.
 *
 * Return: Success or Failure status
 */
int wma_roam_synch_event_handler(void *handle, uint8_t *event,
				 uint32_t len)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	int status = -EINVAL;
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct wma_txrx_node *iface = NULL;
	wmi_roam_synch_event_fixed_param *synch_event = NULL;
	WMI_ROAM_SYNCH_EVENTID_param_tlvs *param_buf = NULL;
	struct vdev_mlme_obj *mlme_obj;

	if (!event) {
		wma_err_rl("event param null");
		return status;
	}

	param_buf = (WMI_ROAM_SYNCH_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_err_rl("received null buf from target");
		return status;
	}
	synch_event = param_buf->fixed_param;
	if (!synch_event) {
		wma_err_rl("received null event data from target");
		return status;
	}

	if (synch_event->vdev_id >= wma->max_bssid) {
		wma_err_rl("received invalid vdev_id %d",
			   synch_event->vdev_id);
		return status;
	}

	iface = &wma->interfaces[synch_event->vdev_id];
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (mlme_obj)
		mlme_obj->mgmt.generic.tx_pwrlimit =
				synch_event->max_allowed_tx_power;

	qdf_status = wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
						   WLAN_VDEV_SM_EV_ROAM,
						   len,
						   event);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("Failed to send the EV_ROAM");
		wma_post_roam_sync_failure(wma, synch_event->vdev_id);
		return status;
	}
	wma_debug("Posted EV_ROAM to VDEV SM");
	return 0;
}

int wma_roam_auth_offload_event_handler(WMA_HANDLE handle, uint8_t *event,
					uint32_t len)
{
	QDF_STATUS status;
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct mac_context *mac_ctx;
	wmi_roam_preauth_start_event_fixed_param *rso_auth_start_ev;
	WMI_ROAM_PREAUTH_START_EVENTID_param_tlvs *param_buf;
	struct qdf_mac_addr ap_bssid;
	uint8_t vdev_id;

	if (!event) {
		wma_err_rl("received null event from target");
		return -EINVAL;
	}

	param_buf = (WMI_ROAM_PREAUTH_START_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err_rl("received null buf from target");
		return -EINVAL;
	}

	rso_auth_start_ev = param_buf->fixed_param;
	if (!rso_auth_start_ev) {
		wma_err_rl("received null event data from target");
		return -EINVAL;
	}

	if (rso_auth_start_ev->vdev_id > wma->max_bssid) {
		wma_err_rl("received invalid vdev_id %d",
			   rso_auth_start_ev->vdev_id);
		return -EINVAL;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		wma_err("NULL mac ptr");
		QDF_ASSERT(0);
		return -EINVAL;
	}

	cds_host_diag_log_work(&wma->roam_preauth_wl,
			       WMA_ROAM_PREAUTH_WAKE_LOCK_DURATION,
			       WIFI_POWER_EVENT_WAKELOCK_WOW);
	qdf_wake_lock_timeout_acquire(&wma->roam_ho_wl,
				      WMA_ROAM_HO_WAKE_LOCK_DURATION);

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&rso_auth_start_ev->candidate_ap_bssid,
				   ap_bssid.bytes);
	if (qdf_is_macaddr_zero(&ap_bssid) ||
	    qdf_is_macaddr_broadcast(&ap_bssid) ||
	    qdf_is_macaddr_group(&ap_bssid)) {
		wma_err_rl("Invalid bssid");
		return -EINVAL;
	}

	vdev_id = rso_auth_start_ev->vdev_id;
	wma_debug("Received Roam auth offload event for bss:"QDF_MAC_ADDR_FMT" vdev_id:%d",
		  QDF_MAC_ADDR_REF(ap_bssid.bytes), vdev_id);

	lim_sae_auth_cleanup_retry(mac_ctx, vdev_id);
	status = wma->csr_roam_auth_event_handle_cb(mac_ctx, vdev_id, ap_bssid);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err_rl("Trigger pre-auth failed");
		return -EINVAL;
	}

	return 0;
}

int wma_roam_scan_chan_list_event_handler(WMA_HANDLE handle,
					  uint8_t *event,
					  uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_ROAM_SCAN_CHANNEL_LIST_EVENTID_param_tlvs *param_buf;
	wmi_roam_scan_channel_list_event_fixed_param *fixed_param;
	uint8_t vdev_id, i = 0, num_ch = 0;
	struct roam_scan_ch_resp *resp;
	struct scheduler_msg sme_msg = {0};

	param_buf = (WMI_ROAM_SCAN_CHANNEL_LIST_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_err_rl("NULL event received from target");
		return -EINVAL;
	}

	fixed_param = param_buf->fixed_param;
	if (!fixed_param) {
		wma_err_rl(" NULL fixed param");
		return -EINVAL;
	}

	vdev_id = fixed_param->vdev_id;
	if (vdev_id >= wma->max_bssid) {
		wma_err_rl("Invalid vdev_id %d", vdev_id);
		return -EINVAL;
	}

	num_ch = (param_buf->num_channel_list <
		WNI_CFG_VALID_CHANNEL_LIST_LEN) ?
		param_buf->num_channel_list :
		WNI_CFG_VALID_CHANNEL_LIST_LEN;

	resp = qdf_mem_malloc(sizeof(struct roam_scan_ch_resp) +
		num_ch * sizeof(param_buf->channel_list[0]));
	if (!resp)
		return -EINVAL;

	resp->chan_list = (uint32_t *)(resp + 1);
	resp->vdev_id = vdev_id;
	resp->command_resp = fixed_param->command_response;
	resp->num_channels = param_buf->num_channel_list;

	for (i = 0; i < num_ch; i++)
		resp->chan_list[i] = param_buf->channel_list[i];

	sme_msg.type = eWNI_SME_GET_ROAM_SCAN_CH_LIST_EVENT;
	sme_msg.bodyptr = resp;

	if (scheduler_post_message(QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_SME, &sme_msg)) {
		wma_err("Failed to post msg to SME");
		qdf_mem_free(sme_msg.bodyptr);
		return -EINVAL;
	}

	return 0;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wma_get_trigger_detail_str  - Return roam trigger string from the
 * enum WMI_ROAM_TRIGGER_REASON
 * @roam_info: Pointer to the roam trigger info
 * @buf:       Destination buffer to write the reason string
 *
 * Return: None
 */
static void
wma_get_trigger_detail_str(struct wmi_roam_trigger_info *roam_info, char *buf)
{
	uint16_t buf_cons, buf_left = MAX_ROAM_DEBUG_BUF_SIZE;
	char *temp = buf;

	buf_cons = qdf_snprint(temp, buf_left, "Reason: \"%s\" ",
			mlme_get_roam_trigger_str(roam_info->trigger_reason));
	temp += buf_cons;
	buf_left -= buf_cons;

	if (roam_info->trigger_sub_reason) {
		buf_cons = qdf_snprint(
			    temp, buf_left, "Sub-Reason: %s",
			    mlme_get_sub_reason_str(roam_info->trigger_sub_reason));
		temp += buf_cons;
		buf_left -= buf_cons;
	}

	switch (roam_info->trigger_reason) {
	case WMI_ROAM_TRIGGER_REASON_PER:
	case WMI_ROAM_TRIGGER_REASON_BMISS:
	case WMI_ROAM_TRIGGER_REASON_HIGH_RSSI:
	case WMI_ROAM_TRIGGER_REASON_MAWC:
	case WMI_ROAM_TRIGGER_REASON_DENSE:
	case WMI_ROAM_TRIGGER_REASON_BACKGROUND:
	case WMI_ROAM_TRIGGER_REASON_IDLE:
	case WMI_ROAM_TRIGGER_REASON_FORCED:
	case WMI_ROAM_TRIGGER_REASON_UNIT_TEST:
		return;
	case WMI_ROAM_TRIGGER_REASON_BTM:
		buf_cons = qdf_snprint(temp, buf_left,
				       "Req_mode: %d Disassoc_timer: %d",
				       roam_info->btm_trig_data.btm_request_mode,
				       roam_info->btm_trig_data.disassoc_timer);
		temp += buf_cons;
		buf_left -= buf_cons;

		buf_cons = qdf_snprint(temp, buf_left,
			    "validity_interval: %d candidate_list_cnt: %d resp_status: %d, bss_termination_timeout: %d, mbo_assoc_retry_timeout: %d",
			    roam_info->btm_trig_data.validity_interval,
			    roam_info->btm_trig_data.candidate_list_count,
			    roam_info->btm_trig_data.btm_resp_status,
			    roam_info->btm_trig_data.
					btm_bss_termination_timeout,
			    roam_info->btm_trig_data.
					btm_mbo_assoc_retry_timeout);
		buf_left -= buf_cons;
		temp += buf_cons;
		return;
	case WMI_ROAM_TRIGGER_REASON_BSS_LOAD:
		buf_cons = qdf_snprint(temp, buf_left, "CU: %d %% ",
				       roam_info->cu_trig_data.cu_load);
		temp += buf_cons;
		buf_left -= buf_cons;
		return;
	case WMI_ROAM_TRIGGER_REASON_DEAUTH:
		buf_cons = qdf_snprint(temp, buf_left, "Type: %d Reason: %d ",
				       roam_info->deauth_trig_data.type,
				       roam_info->deauth_trig_data.reason);
		temp += buf_cons;
		buf_left -= buf_cons;
		return;
	case WMI_ROAM_TRIGGER_REASON_LOW_RSSI:
	case WMI_ROAM_TRIGGER_REASON_PERIODIC:
		/*
		 * Use roam_info->current_rssi get the RSSI of current AP after
		 * roam scan is triggered. This avoids discrepency with the
		 * next rssi threshold value printed in roam scan details.
		 * roam_info->rssi_trig_data.threshold gives the rssi threshold
		 * for the Low Rssi/Periodic scan trigger.
		 */
		buf_cons = qdf_snprint(temp, buf_left,
				       " Cur_Rssi threshold:%d Current AP RSSI: %d",
				       roam_info->rssi_trig_data.threshold,
				       roam_info->current_rssi);
		temp += buf_cons;
		buf_left -= buf_cons;
		return;
	case WMI_ROAM_TRIGGER_REASON_WTC_BTM:
		if (roam_info->wtc_btm_trig_data.wtc_candi_rssi_ext_present) {
			buf_cons = qdf_snprint(temp, buf_left, "Roaming Mode: %d, Trigger Reason: %d, Sub code:%d, wtc mode:%d, wtc scan mode:%d, wtc rssi th:%d, wtc candi rssi th_2g:%d, wtc_candi_rssi_th_5g:%d, wtc_candi_rssi_th_6g:%d",
			     roam_info->wtc_btm_trig_data.roaming_mode,
			     roam_info->wtc_btm_trig_data.vsie_trigger_reason,
			     roam_info->wtc_btm_trig_data.sub_code,
			     roam_info->wtc_btm_trig_data.wtc_mode,
			     roam_info->wtc_btm_trig_data.wtc_scan_mode,
			     roam_info->wtc_btm_trig_data.wtc_rssi_th,
			     roam_info->wtc_btm_trig_data.wtc_candi_rssi_th,
			     roam_info->wtc_btm_trig_data.wtc_candi_rssi_th_5g,
			     roam_info->wtc_btm_trig_data.wtc_candi_rssi_th_6g);
		} else {
			buf_cons = qdf_snprint(temp, buf_left, "Roaming Mode: %d, Trigger Reason: %d, Sub code:%d, wtc mode:%d, wtc scan mode:%d, wtc rssi th:%d, wtc candi rssi th:%d",
			       roam_info->wtc_btm_trig_data.roaming_mode,
			       roam_info->wtc_btm_trig_data.vsie_trigger_reason,
			       roam_info->wtc_btm_trig_data.sub_code,
			       roam_info->wtc_btm_trig_data.wtc_mode,
			       roam_info->wtc_btm_trig_data.wtc_scan_mode,
			       roam_info->wtc_btm_trig_data.wtc_rssi_th,
			       roam_info->wtc_btm_trig_data.wtc_candi_rssi_th);
		}

		temp += buf_cons;
		buf_left -= buf_cons;
		return;
	default:
		return;
	}
}

/**
 * wma_rso_print_trigger_info  - Roam trigger related details
 * @data:    Pointer to the roam trigger data
 * @vdev_id: Vdev ID
 *
 * Prints the vdev, roam trigger reason, time of the day at which roaming
 * was triggered.
 *
 * Return: None
 */
static void
wma_rso_print_trigger_info(struct wmi_roam_trigger_info *data, uint8_t vdev_id)
{
	char *buf;
	char time[TIME_STRING_LEN];

	buf = qdf_mem_malloc(MAX_ROAM_DEBUG_BUF_SIZE);
	if (!buf)
		return;

	wma_get_trigger_detail_str(data, buf);
	mlme_get_converted_timestamp(data->timestamp, time);
	wma_nofl_info("%s [ROAM_TRIGGER]: VDEV[%d] %s", time, vdev_id, buf);

	qdf_mem_free(buf);
}

/**
 * wma_rso_print_btm_rsp_info - BTM RSP related details
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 *
 * Prints the vdev, btm status, target_bssid and vsie reason
 *
 * Return: None
 */
static void
wma_rso_print_btm_rsp_info(struct roam_btm_response_data *data,
			   uint8_t vdev_id)
{
	char time[TIME_STRING_LEN];

	mlme_get_converted_timestamp(data->timestamp, time);
	wma_nofl_info("%s [BTM RSP]: VDEV[%d], Status: %d, VSIE reason: %d, BSSID: " QDF_MAC_ADDR_FMT,
		      time, vdev_id, data->btm_status, data->vsie_reason,
		      QDF_MAC_ADDR_REF(data->target_bssid.bytes));
}

/**
 * wma_rso_print_roam_initial_info - Roaming related initial details
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 *
 * Prints the vdev, roam_full_scan_count, channel and rssi
 * utilization threhold and timer
 *
 * Return: None
 */
static void
wma_rso_print_roam_initial_info(struct roam_initial_data *data,
				uint8_t vdev_id)
{
	wma_nofl_info("[ROAM INIT INFO]: VDEV[%d], roam_full_scan_count: %d, rssi_th: %d, cu_th: %d, fw_cancel_timer_bitmap: %d",
		      vdev_id, data->roam_full_scan_count, data->rssi_th,
		      data->cu_th, data->fw_cancel_timer_bitmap);
}

/**
 * wma_rso_print_roam_msg_info - Roaming related message details
 * @data:    Pointer to the btm rsp data
 * @vdev_id: vdev id
 *
 * Prints the vdev, msg_id, msg_param1, msg_param2 and timer
 *
 * Return: None
 */
static void wma_rso_print_roam_msg_info(struct roam_msg_info *data,
					uint8_t vdev_id)
{
	char time[TIME_STRING_LEN];
	static const char msg_id1_str[] = "Roam RSSI TH Reset";

	if (data->msg_id == WMI_ROAM_MSG_RSSI_RECOVERED) {
		mlme_get_converted_timestamp(data->timestamp, time);
		wma_info("%s [ROAM MSG INFO]: VDEV[%d] %s, Current rssi: %d dbm, next_rssi_threshold: %d dbm",
			 time, vdev_id, msg_id1_str, data->msg_param1,
			 data->msg_param2);
	}
}

/**
 * wma_log_roam_scan_candidates  - Print roam scan candidate AP info
 * @ap:           Pointer to the candidate AP list
 * @num_entries:  Number of candidate APs
 *
 * Print the RSSI, CU load, Cu score, RSSI score, total score, BSSID
 * and time stamp at which the candidate was found details.
 *
 * Return: None
 */
static void
wma_log_roam_scan_candidates(struct wmi_roam_candidate_info *ap,
			     uint8_t num_entries)
{
	uint16_t i;
	char time[TIME_STRING_LEN], time2[TIME_STRING_LEN];

	wma_nofl_info("%62s%62s", LINE_STR, LINE_STR);
	wma_nofl_info("%13s %16s %8s %4s %4s %5s/%3s %3s/%3s %7s %7s %6s %12s %20s",
		      "AP BSSID", "TSTAMP", "CH", "TY", "ETP", "RSSI",
		      "SCR", "CU%", "SCR", "TOT_SCR", "BL_RSN", "BL_SRC",
		      "BL_TSTAMP", "BL_TIMEOUT(ms)");
	wma_nofl_info("%62s%62s", LINE_STR, LINE_STR);

	if (num_entries > MAX_ROAM_CANDIDATE_AP)
		num_entries = MAX_ROAM_CANDIDATE_AP;

	for (i = 0; i < num_entries; i++) {
		mlme_get_converted_timestamp(ap->timestamp, time);
		mlme_get_converted_timestamp(ap->bl_timestamp, time2);
		wma_nofl_info(QDF_MAC_ADDR_FMT " %17s %4d %-4s %4d %3d/%-4d %2d/%-4d %5d %7d %7d   %17s %9d",
			      QDF_MAC_ADDR_REF(ap->bssid.bytes), time,
			      ap->freq,
			      ((ap->type == 0) ? "C_AP" :
			       ((ap->type == 2) ? "R_AP" : "P_AP")),
			      ap->etp, ap->rssi, ap->rssi_score, ap->cu_load,
			      ap->cu_score, ap->total_score, ap->bl_reason,
			      ap->bl_source, time2, ap->bl_original_timeout);
		ap++;
	}
}

/**
 * wma_rso_print_scan_info  - Print the roam scan details and candidate AP
 * details
 * @scan:      Pointer to the received tlv after sanitization
 * @vdev_id:   Vdev ID
 * @trigger:   Roam scan trigger reason
 * @timestamp: Host timestamp in millisecs
 *
 * Prinst the roam scan details with time of the day when the scan was
 * triggered and roam candidate AP with score details
 *
 * Return: None
 */
static void
wma_rso_print_scan_info(struct wmi_roam_scan_data *scan, uint8_t vdev_id,
			uint32_t trigger, uint32_t timestamp)
{
	uint16_t num_ch = scan->num_chan;
	uint16_t buf_cons = 0, buf_left = ROAM_CHANNEL_BUF_SIZE;
	uint8_t i;
	char *buf, *buf1, *tmp;
	char time[TIME_STRING_LEN];

	buf = qdf_mem_malloc(ROAM_CHANNEL_BUF_SIZE);
	if (!buf)
		return;

	tmp = buf;
	/* For partial scans, print the channel info */
	if (!scan->type) {
		buf_cons = qdf_snprint(tmp, buf_left, "{");
		buf_left -= buf_cons;
		tmp += buf_cons;

		for (i = 0; i < num_ch; i++) {
			buf_cons = qdf_snprint(tmp, buf_left, "%d ",
					       scan->chan_freq[i]);
			buf_left -= buf_cons;
			tmp += buf_cons;
		}
		buf_cons = qdf_snprint(tmp, buf_left, "}");
		buf_left -= buf_cons;
		tmp += buf_cons;
	}

	buf1 = qdf_mem_malloc(ROAM_FAILURE_BUF_SIZE);
	if (!buf1) {
		qdf_mem_free(buf);
		return;
	}

	if (WMI_ROAM_TRIGGER_REASON_LOW_RSSI == trigger ||
	    WMI_ROAM_TRIGGER_REASON_PERIODIC == trigger)
		qdf_snprint(buf1, ROAM_FAILURE_BUF_SIZE,
			    "next_rssi_threshold: %d dBm",
			    scan->next_rssi_threshold);

	mlme_get_converted_timestamp(timestamp, time);
	wma_nofl_info("%s [ROAM_SCAN]: VDEV[%d] Scan_type: %s %s %s",
		      time, vdev_id, mlme_get_roam_scan_type_str(scan->type),
		      buf1, buf);
	wma_log_roam_scan_candidates(scan->ap, scan->num_ap);

	qdf_mem_free(buf);
	qdf_mem_free(buf1);
}

/**
 * wma_rso_print_roam_result()  - Print roam result related info
 * @res:     Roam result strucure pointer
 * @vdev_id: Vdev id
 *
 * Print roam result and failure reason if roaming failed.
 *
 * Return: None
 */
static void
wma_rso_print_roam_result(struct wmi_roam_result *res,
			  uint8_t vdev_id)
{
	char *buf;
	char time[TIME_STRING_LEN];

	buf = qdf_mem_malloc(ROAM_FAILURE_BUF_SIZE);
	if (!buf)
		return;

	if (res->status == 1)
		qdf_snprint(buf, ROAM_FAILURE_BUF_SIZE, "Reason: %s",
			    mlme_get_roam_fail_reason_str(res->fail_reason));

	mlme_get_converted_timestamp(res->timestamp, time);
	wma_nofl_info("%s [ROAM_RESULT]: VDEV[%d] %s %s",
		      time, vdev_id, mlme_get_roam_status_str(res->status),
		      buf);

	qdf_mem_free(buf);
}

/**
 * wma_rso_print_11kv_info  - Print neighbor report/BTM related data
 * @neigh_rpt: Pointer to the extracted TLV structure
 * @vdev_id:   Vdev ID
 *
 * Print BTM/neighbor report info that is sent by firmware after
 * connection/roaming to an AP.
 *
 * Return: none
 */
static void
wma_rso_print_11kv_info(struct wmi_neighbor_report_data *neigh_rpt,
			uint8_t vdev_id)
{
	char time[TIME_STRING_LEN], time1[TIME_STRING_LEN];
	char *buf, *tmp;
	uint8_t type = neigh_rpt->req_type, i;
	uint16_t buf_left = ROAM_CHANNEL_BUF_SIZE, buf_cons;
	uint8_t num_ch = neigh_rpt->num_freq;

	if (!type)
		return;

	buf = qdf_mem_malloc(ROAM_CHANNEL_BUF_SIZE);
	if (!buf)
		return;

	tmp = buf;
	if (num_ch) {
		buf_cons = qdf_snprint(tmp, buf_left, "{ ");
		buf_left -= buf_cons;
		tmp += buf_cons;

		for (i = 0; i < num_ch; i++) {
			buf_cons = qdf_snprint(tmp, buf_left, "%d ",
					       neigh_rpt->freq[i]);
			buf_left -= buf_cons;
			tmp += buf_cons;
		}

		buf_cons = qdf_snprint(tmp, buf_left, "}");
		buf_left -= buf_cons;
		tmp += buf_cons;
	}

	mlme_get_converted_timestamp(neigh_rpt->req_time, time);
	wma_nofl_info("%s [%s] VDEV[%d]", time,
		      (type == 1) ? "BTM_QUERY" : "NEIGH_RPT_REQ", vdev_id);

	if (neigh_rpt->resp_time) {
		mlme_get_converted_timestamp(neigh_rpt->resp_time, time1);
		wma_nofl_info("%s [%s] VDEV[%d] %s", time1,
			      (type == 1) ? "BTM_REQ" : "NEIGH_RPT_RSP",
			      vdev_id,
			      (num_ch > 0) ? buf : "NO Ch update");
	} else {
		wma_nofl_info("%s No response received from AP",
			      (type == 1) ? "BTM" : "NEIGH_RPT");
	}
	qdf_mem_free(buf);
}

void wma_report_real_time_roam_stats(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     enum roam_rt_stats_type events,
				     struct mlme_roam_debug_info *roam_info,
				     uint32_t value)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct mlme_roam_debug_info *roam_event = NULL;

	if (!wlan_cm_get_roam_rt_stats(psoc, ROAM_RT_STATS_ENABLE)) {
		wma_err_rl("Roam events stats is disabled");
		return;
	}

	switch (events) {
	case ROAM_RT_STATS_TYPE_SCAN_STATE:
		roam_event = qdf_mem_malloc(sizeof(*roam_event));
		if (!roam_event)
			return;

		if (value == WMI_ROAM_NOTIF_SCAN_START)
			roam_event->roam_event_param.roam_scan_state =
					QCA_WLAN_VENDOR_ROAM_SCAN_STATE_START;
		else if (value == WMI_ROAM_NOTIF_SCAN_END)
			roam_event->roam_event_param.roam_scan_state =
					QCA_WLAN_VENDOR_ROAM_SCAN_STATE_END;

		if (mac && mac->sme.roam_rt_stats_cb) {
			wma_debug("Invoke HDD roam events callback for "
				  "roam scan notif");
			roam_event->roam_event_param.vdev_id = vdev_id;
			mac->sme.roam_rt_stats_cb(mac->hdd_handle, roam_event);
		}
		qdf_mem_free(roam_event);
		break;
	case ROAM_RT_STATS_TYPE_INVOKE_FAIL_REASON:
		roam_event = qdf_mem_malloc(sizeof(*roam_event));
		if (!roam_event)
			return;

		roam_event->roam_event_param.roam_invoke_fail_reason = value;

		if (mac && mac->sme.roam_rt_stats_cb) {
			wma_debug("Invoke HDD roam events callback for "
				  "roam invoke fail");
			roam_event->roam_event_param.vdev_id = vdev_id;
			mac->sme.roam_rt_stats_cb(mac->hdd_handle, roam_event);
		}
		qdf_mem_free(roam_event);
		break;
	case ROAM_RT_STATS_TYPE_ROAM_SCAN_INFO:
		if ((roam_info->scan.present || roam_info->trigger.present ||
		     (roam_info->result.present &&
		      roam_info->result.fail_reason)) &&
		    mac && mac->sme.roam_rt_stats_cb) {
			wma_debug("Invoke HDD roam events callback for roam "
				  "stats event");
			roam_info->roam_event_param.vdev_id = vdev_id;
			mac->sme.roam_rt_stats_cb(mac->hdd_handle, roam_info);
		}
		break;
	default:
		break;
	}
}

int wma_roam_stats_event_handler(WMA_HANDLE handle, uint8_t *event,
				 uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_ROAM_STATS_EVENTID_param_tlvs *param_buf;
	wmi_roam_stats_event_fixed_param *fixed_param;
	struct mlme_roam_debug_info *roam_info = NULL;
	uint8_t vdev_id, i;
	uint8_t num_tlv = 0, num_chan = 0, num_ap = 0, num_rpt = 0, rem_tlv = 0;
	uint32_t rem_len;
	QDF_STATUS status;

	param_buf = (WMI_ROAM_STATS_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_err_rl("NULL event received from target");
		goto err;
	}

	fixed_param = param_buf->fixed_param;
	if (!fixed_param) {
		wma_err_rl(" NULL fixed param");
		goto err;
	}

	vdev_id = fixed_param->vdev_id;
	if (vdev_id >= wma->max_bssid) {
		wma_err_rl("Invalid vdev_id %d", vdev_id);
		goto err;
	}

	num_tlv = fixed_param->roam_scan_trigger_count;
	if (num_tlv > MAX_ROAM_SCAN_STATS_TLV) {
		wma_err_rl("Limiting roam triggers to 5");
		num_tlv = MAX_ROAM_SCAN_STATS_TLV;
	}

	rem_len = len - sizeof(*fixed_param);
	if (rem_len < num_tlv * sizeof(wmi_roam_trigger_reason)) {
		wma_err_rl("Invalid roam trigger data");
		goto err;
	}

	rem_len -= num_tlv * sizeof(wmi_roam_trigger_reason);
	if (rem_len < num_tlv * sizeof(wmi_roam_scan_info)) {
		wma_err_rl("Invalid roam scan data");
		goto err;
	}

	rem_len -= num_tlv * sizeof(wmi_roam_scan_info);
	if (rem_len < num_tlv * sizeof(wmi_roam_result)) {
		wma_err_rl("Invalid roam result data");
		goto err;
	}

	rem_len -= num_tlv * sizeof(wmi_roam_result);
	if (rem_len < (num_tlv * sizeof(wmi_roam_neighbor_report_info))) {
		wma_err_rl("Invalid roam neighbor report data");
		goto err;
	}

	rem_len -= num_tlv * sizeof(wmi_roam_neighbor_report_info);
	if (rem_len < (param_buf->num_roam_scan_chan_info *
		       sizeof(wmi_roam_scan_channel_info))) {
		wma_err_rl("Invalid roam chan data num_tlv:%d",
			   param_buf->num_roam_scan_chan_info);
		goto err;
	}

	rem_len -= param_buf->num_roam_scan_chan_info *
		   sizeof(wmi_roam_scan_channel_info);

	if (rem_len < (param_buf->num_roam_ap_info *
		       sizeof(wmi_roam_ap_info))) {
		wma_err_rl("Invalid roam ap data num_tlv:%d",
			   param_buf->num_roam_ap_info);
		goto err;
	}

	rem_len -= param_buf->num_roam_ap_info * sizeof(wmi_roam_ap_info);
	if (rem_len < (param_buf->num_roam_neighbor_report_chan_info *
		       sizeof(wmi_roam_neighbor_report_channel_info))) {
		wma_err_rl("Invalid roam neigb rpt chan data num_tlv:%d",
			   param_buf->num_roam_neighbor_report_chan_info);
		goto err;
	}

	rem_len -= param_buf->num_roam_neighbor_report_chan_info *
			sizeof(wmi_roam_neighbor_report_channel_info);
	if (rem_len < param_buf->num_roam_btm_response_info *
	    sizeof(wmi_roam_btm_response_info)) {
		wma_err_rl("Invalid btm rsp data");
		goto err;
	}

	rem_len -= param_buf->num_roam_btm_response_info *
			sizeof(wmi_roam_btm_response_info);
	if (rem_len < param_buf->num_roam_initial_info *
	    sizeof(wmi_roam_initial_info)) {
		wma_err_rl("Invalid Initial roam info");
		goto err;
	}

	rem_len -= param_buf->num_roam_initial_info *
			sizeof(wmi_roam_initial_info);
	if (rem_len < param_buf->num_roam_msg_info *
	    sizeof(wmi_roam_msg_info)) {
		wma_err_rl("Invalid roam msg info");
		goto err;
	}

	for (i = 0; i < num_tlv; i++) {
		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info)
			return -ENOMEM;

		/*
		 * Roam Trigger id and that specific roam trigger related
		 * details.
		 */
		status = wmi_unified_extract_roam_trigger_stats(
						wma->wmi_handle, event,
						&roam_info->trigger, i);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Extract roam trigger stats failed vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}

		if (roam_info->trigger.present) {
			wma_rso_print_trigger_info(&roam_info->trigger,
						   vdev_id);
			wlan_cm_update_roam_states(wma->psoc, vdev_id,
					roam_info->trigger.trigger_reason,
					ROAM_TRIGGER_REASON);
		}

		/* Roam scan related details - Scan channel, scan type .. */
		status = wmi_unified_extract_roam_scan_stats(
							wma->wmi_handle, event,
							&roam_info->scan, i,
							num_chan, num_ap);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam scan stats extract failed vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}
		num_chan += roam_info->scan.num_chan;
		num_ap += roam_info->scan.num_ap;

		if (roam_info->scan.present && roam_info->trigger.present)
			wma_rso_print_scan_info(&roam_info->scan, vdev_id,
					roam_info->trigger.trigger_reason,
					roam_info->trigger.timestamp);

		/* Roam result - Success/Failure status, failure reason */
		status = wmi_unified_extract_roam_result_stats(
							wma->wmi_handle, event,
							&roam_info->result, i);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam result stats extract failed vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}
		if (roam_info->result.present) {
			wma_rso_print_roam_result(&roam_info->result, vdev_id);
			wlan_cm_update_roam_states(wma->psoc, vdev_id,
						roam_info->result.fail_reason,
						ROAM_FAIL_REASON);
		}
		/* BTM resp info */
		status = wlan_cm_roam_extract_btm_response(wma->wmi_handle,
							   event,
							   &roam_info->btm_rsp,
							   i);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam btm rsp stats extract fail vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}

		/*
		 * Print BTM resp TLV info (wmi_roam_btm_response_info) only
		 * when trigger reason is BTM or WTC_BTM. As for other roam
		 * triggers this TLV contains zeros, so host should not print.
		 */
		if ((roam_info->btm_rsp.present) &&
		    (roam_info->trigger.present &&
		    (roam_info->trigger.trigger_reason ==
		     WMI_ROAM_TRIGGER_REASON_WTC_BTM ||
		     roam_info->trigger.trigger_reason ==
		     WMI_ROAM_TRIGGER_REASON_BTM)))
			wma_rso_print_btm_rsp_info(&roam_info->btm_rsp,
						   vdev_id);

		/* Initial Roam info */
		status = wlan_cm_roam_extract_roam_initial_info(
				wma->wmi_handle, event,
				&roam_info->roam_init_info, i);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Initial roam stats extract fail vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}
		if (roam_info->roam_init_info.present)
			wma_rso_print_roam_initial_info(
					&roam_info->roam_init_info, vdev_id);

		/* Roam message info */
		status = wlan_cm_roam_extract_roam_msg_info(
				wma->wmi_handle, event,
				&roam_info->roam_msg_info, i);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("roam msg stats extract fail vdev %d",
				vdev_id);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}
		if (roam_info->roam_msg_info.present) {
			rem_tlv++;
			wma_rso_print_roam_msg_info(
					&roam_info->roam_msg_info, vdev_id);

		/* BTM req/resp or Neighbor report/response info */
		status = wmi_unified_extract_roam_11kv_stats(
				wma->wmi_handle, event,
				&roam_info->data_11kv, i, num_rpt);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam 11kv stats extract failed vdev %d at %d iteration",
				     vdev_id, i);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}
		num_rpt += roam_info->data_11kv.num_freq;
		if (roam_info->data_11kv.present)
			wma_rso_print_11kv_info(&roam_info->data_11kv, vdev_id);
		}

		wma_report_real_time_roam_stats(
					wma->psoc, vdev_id,
					ROAM_RT_STATS_TYPE_ROAM_SCAN_INFO,
					roam_info, 0);

		qdf_mem_free(roam_info);
	}

	if (!num_tlv) {
		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info)
			return -ENOMEM;
		status = wmi_unified_extract_roam_11kv_stats(
						wma->wmi_handle, event,
						&roam_info->data_11kv, 0, 0);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam 11kv stats extract failed vdev %d",
				     vdev_id);
			qdf_mem_free(roam_info);
			goto err;
		}

		if (roam_info->data_11kv.present)
			wma_rso_print_11kv_info(&roam_info->data_11kv, vdev_id);

		status = wmi_unified_extract_roam_trigger_stats(
						wma->wmi_handle, event,
						&roam_info->trigger, 0);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Extract roam trigger stats failed vdev %d",
				     vdev_id);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}

		if (roam_info->trigger.present)
			wma_rso_print_trigger_info(&roam_info->trigger,
						   vdev_id);

		status = wmi_unified_extract_roam_scan_stats(wma->wmi_handle,
							     event,
							     &roam_info->scan,
							     0, 0, 0);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam scan stats extract failed vdev %d",
				     vdev_id);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}

		if (roam_info->scan.present && roam_info->trigger.present)
			wma_rso_print_scan_info(&roam_info->scan, vdev_id,
					roam_info->trigger.trigger_reason,
					roam_info->trigger.timestamp);

		status = wlan_cm_roam_extract_btm_response(wma->wmi_handle,
							   event,
							   &roam_info->btm_rsp,
							   0);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_debug_rl("Roam btm rsp stats extract fail vdev %d",
				     vdev_id);
			qdf_mem_free(roam_info);
			return -EINVAL;
		}

		if (roam_info->btm_rsp.present)
			wma_rso_print_btm_rsp_info(&roam_info->btm_rsp,
						   vdev_id);

		qdf_mem_free(roam_info);
	}

	if (param_buf->roam_msg_info && param_buf->num_roam_msg_info &&
	    param_buf->num_roam_msg_info - rem_tlv) {
		for (i = 0; i < (param_buf->num_roam_msg_info - rem_tlv); i++) {
			roam_info = qdf_mem_malloc(sizeof(*roam_info));
			if (!roam_info)
				return -ENOMEM;
			status = wlan_cm_roam_extract_roam_msg_info(
					wma->wmi_handle, event,
					&roam_info->roam_msg_info, rem_tlv + i);
			if (QDF_IS_STATUS_ERROR(status)) {
				wma_err("roam msg stats extract fail vdev %d",
					vdev_id);
				qdf_mem_free(roam_info);
				return -EINVAL;
			}

			if (roam_info->roam_msg_info.present)
				wma_rso_print_roam_msg_info(
						&roam_info->roam_msg_info,
						vdev_id);
			qdf_mem_free(roam_info);
		}
	}

	return 0;

err:
	return -EINVAL;
}
#endif

/**
 * wma_set_ric_req() - set ric request element
 * @wma: wma handle
 * @msg: message
 * @is_add_ts: is addts required
 *
 * This function sets ric request element for 11r roaming.
 *
 * Return: none
 */
void wma_set_ric_req(tp_wma_handle wma, void *msg, uint8_t is_add_ts)
{
	if (!wma) {
		wma_err("wma handle is NULL");
		return;
	}

	wmi_unified_set_ric_req_cmd(wma->wmi_handle, msg, is_add_ts);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#ifdef FEATURE_RSSI_MONITOR
QDF_STATUS wma_set_rssi_monitoring(tp_wma_handle wma,
				   struct rssi_monitor_param *req)
{
	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_set_rssi_monitoring_cmd(wma->wmi_handle, req);
}

/**
 * wma_rssi_breached_event_handler() - rssi breached event handler
 * @handle: wma handle
 * @cmd_param_info: event handler data
 * @len: length of @cmd_param_info
 *
 * Return: 0 on success; error number otherwise
 */
int wma_rssi_breached_event_handler(void *handle,
				u_int8_t  *cmd_param_info, u_int32_t len)
{
	WMI_RSSI_BREACH_EVENTID_param_tlvs *param_buf;
	wmi_rssi_breach_event_fixed_param  *event;
	struct rssi_breach_event  rssi;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!mac || !wma) {
		wma_err("Invalid mac/wma context");
		return -EINVAL;
	}
	if (!mac->sme.rssi_threshold_breached_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_RSSI_BREACH_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid rssi breached event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;

	rssi.request_id = event->request_id;
	rssi.session_id = event->vdev_id;
	if (wmi_service_enabled(wma->wmi_handle,
				wmi_service_hw_db2dbm_support))
		rssi.curr_rssi = event->rssi;
	else
		rssi.curr_rssi = event->rssi + WMA_TGT_NOISE_FLOOR_DBM;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->bssid, rssi.curr_bssid.bytes);

	wma_debug("req_id: %u vdev_id: %d curr_rssi: %d",
		 rssi.request_id, rssi.session_id, rssi.curr_rssi);
	wma_debug("curr_bssid: "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(rssi.curr_bssid.bytes));

	mac->sme.rssi_threshold_breached_cb(mac->hdd_handle, &rssi);
	wma_debug("Invoke HDD rssi breached callback");
	return 0;
}
#endif /* FEATURE_RSSI_MONITOR */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wma_roam_ho_fail_handler() - LFR3.0 roam hand off failed handler
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Return: none
 */
static void
wma_roam_ho_fail_handler(tp_wma_handle wma, uint32_t vdev_id,
			 struct qdf_mac_addr bssid)
{
	struct handoff_failure_ind *ho_failure_ind;
	struct scheduler_msg sme_msg = { 0 };
	QDF_STATUS qdf_status;

	ho_failure_ind = qdf_mem_malloc(sizeof(*ho_failure_ind));
	if (!ho_failure_ind)
		return;

	ho_failure_ind->vdev_id = vdev_id;
	ho_failure_ind->bssid = bssid;

	sme_msg.type = eWNI_SME_HO_FAIL_IND;
	sme_msg.bodyptr = ho_failure_ind;
	sme_msg.bodyval = 0;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Fail to post eWNI_SME_HO_FAIL_IND msg to SME");
		qdf_mem_free(ho_failure_ind);
		return;
	}
}

/**
 * wma_process_roam_synch_complete() - roam synch complete command to fw.
 * @handle: wma handle
 * @synchcnf: offload synch confirmation params
 *
 * This function sends roam synch complete event to fw.
 *
 * Return: none
 */
void wma_process_roam_synch_complete(WMA_HANDLE handle, uint8_t vdev_id)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue roam synch cnf");
		return;
	}

	if (!wma_is_vdev_valid(vdev_id))
		return;

	if (wmi_unified_roam_synch_complete_cmd(wma_handle->wmi_handle,
						vdev_id))
		return;

	DPTRACE(qdf_dp_trace_record_event(QDF_DP_TRACE_EVENT_RECORD,
		vdev_id, QDF_TRACE_DEFAULT_PDEV_ID,
		QDF_PROTO_TYPE_EVENT, QDF_ROAM_COMPLETE));

	wma_info("LFR3: vdev[%d] Sent ROAM_SYNCH_COMPLETE", vdev_id);
	wlan_roam_debug_log(vdev_id, DEBUG_ROAM_SYNCH_CNF,
			    DEBUG_INVALID_PEER_ID, NULL, NULL, 0, 0);

}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

QDF_STATUS wma_pre_chan_switch_setup(uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *intr;
	uint16_t beacon_interval_ori;
	bool restart;
	uint16_t reduced_beacon_interval;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;

	if (!wma) {
		pe_err("wma is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	intr = &wma->interfaces[vdev_id];
	if (!intr) {
		pe_err("wma txrx node is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = intr->vdev;
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	restart =
		wma_get_channel_switch_in_progress(intr);
	if (restart && intr->beacon_filter_enabled)
		wma_remove_beacon_filter(wma, &intr->beacon_filter);

	reduced_beacon_interval =
		wma->mac_context->sap.SapDfsInfo.reduced_beacon_interval;
	if (wma_is_vdev_in_ap_mode(wma, vdev_id) && reduced_beacon_interval) {


		/* Reduce the beacon interval just before the channel switch.
		 * This would help in reducing the downtime on the STA side
		 * (which is waiting for beacons from the AP to resume back
		 * transmission). Switch back the beacon_interval to its
		 * original value after the channel switch based on the
		 * timeout. This would ensure there are atleast some beacons
		 * sent with increased frequency.
		 */

		wma_debug("Changing beacon interval to %d",
			 reduced_beacon_interval);

		/* Add a timer to reset the beacon interval back*/
		beacon_interval_ori = mlme_obj->proto.generic.beacon_interval;
		mlme_obj->proto.generic.beacon_interval =
			reduced_beacon_interval;
		if (wma_fill_beacon_interval_reset_req(wma,
			vdev_id,
			beacon_interval_ori,
			RESET_BEACON_INTERVAL_TIMEOUT)) {

			wma_debug("Failed to fill beacon interval reset req");
		}
	}

	status = wma_vdev_pre_start(vdev_id, restart);

	return status;
}

QDF_STATUS wma_post_chan_switch_setup(uint8_t vdev_id)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *intr;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_channel *des_chan;
	cdp_config_param_type val;

	if (!wma) {
		pe_err("wma is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	intr = &wma->interfaces[vdev_id];
	if (!intr) {
		pe_err("wma txrx node is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * Record monitor mode channel here in case HW
	 * indicate RX PPDU TLV with invalid channel number.
	 */
	if (intr->type == WMI_VDEV_TYPE_MONITOR) {
		des_chan = intr->vdev->vdev_mlme.des_chan;
		val.cdp_pdev_param_monitor_chan = des_chan->ch_ieee;
		cdp_txrx_set_pdev_param(soc,
					wlan_objmgr_pdev_get_pdev_id(wma->pdev),
					CDP_MONITOR_CHANNEL, val);
		val.cdp_pdev_param_mon_freq = des_chan->ch_freq;
		cdp_txrx_set_pdev_param(soc,
					wlan_objmgr_pdev_get_pdev_id(wma->pdev),
					CDP_MONITOR_FREQUENCY, val);
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
/**
 * wma_plm_start() - plm start request
 * @wma: wma handle
 * @params: plm request parameters
 *
 * This function request FW to start PLM.
 *
 * Return: QDF status
 */
static QDF_STATUS wma_plm_start(tp_wma_handle wma,
				struct plm_req_params *params)
{
	QDF_STATUS status;

	wma_debug("PLM Start");

	status = wmi_unified_plm_start_cmd(wma->wmi_handle, params);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma->interfaces[params->vdev_id].plm_in_progress = true;

	wma_debug("Plm start request sent successfully for vdev %d",
		 params->vdev_id);

	return status;
}

/**
 * wma_plm_stop() - plm stop request
 * @wma: wma handle
 * @params: plm request parameters
 *
 * This function request FW to stop PLM.
 *
 * Return: QDF status
 */
static QDF_STATUS wma_plm_stop(tp_wma_handle wma,
			       struct plm_req_params *params)
{
	QDF_STATUS status;

	if (!wma->interfaces[params->vdev_id].plm_in_progress) {
		wma_err("No active plm req found, skip plm stop req");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("PLM Stop");

	status = wmi_unified_plm_stop_cmd(wma->wmi_handle, params);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma->interfaces[params->vdev_id].plm_in_progress = false;

	wma_debug("Plm stop request sent successfully for vdev %d",
		 params->vdev_id);

	return status;
}

/**
 * wma_config_plm() - config PLM
 * @wma: wma handle
 * @params: plm request parameters
 *
 * Return: none
 */
void wma_config_plm(tp_wma_handle wma, struct plm_req_params *params)
{
	QDF_STATUS ret;

	if (!params || !wma)
		return;

	if (params->enable)
		ret = wma_plm_start(wma, params);
	else
		ret = wma_plm_stop(wma, params);

	if (ret)
		wma_err("PLM %s failed %d",
			params->enable ? "start" : "stop", ret);
}
#endif

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * wma_extscan_wow_event_callback() - extscan wow event callback
 * @handle: WMA handle
 * @event: event buffer
 * @len: length of @event buffer
 *
 * In wow case, the wow event is followed by the payload of the event
 * which generated the wow event.
 * payload is 4 bytes of length followed by event buffer. the first 4 bytes
 * of event buffer is common tlv header, which is a combination
 * of tag (higher 2 bytes) and length (lower 2 bytes). The tag is used to
 * identify the event which triggered wow event.
 * Payload is extracted and converted into generic tlv structure before
 * being passed to this function.
 *
 * @Return: Errno
 */
int wma_extscan_wow_event_callback(void *handle, void *event, uint32_t len)
{
	uint32_t tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(event));

	switch (tag) {
	case WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param:
		return wma_extscan_start_stop_event_handler(handle, event, len);

	case WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param:
		return wma_extscan_operations_event_handler(handle, event, len);

	case WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param:
		return wma_extscan_table_usage_event_handler(handle, event,
							     len);

	case WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param:
		return wma_extscan_cached_results_event_handler(handle, event,
								len);

	case WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param:
		return wma_extscan_change_results_event_handler(handle, event,
								len);

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param:
		return wma_extscan_hotlist_match_event_handler(handle,	event,
							       len);

	case WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param:
		return wma_extscan_capabilities_event_handler(handle, event,
							      len);

	default:
		wma_err("Unknown tag: %d", tag);
		return 0;
	}
}

/**
 * wma_register_extscan_event_handler() - register extscan event handler
 * @wma_handle: wma handle
 *
 * This function register extscan related event handlers.
 *
 * Return: none
 */
void wma_register_extscan_event_handler(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		wma_err("extscan wma_handle is NULL");
		return;
	}
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_extscan_start_stop_event_id,
					   wma_extscan_start_stop_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					wmi_extscan_capabilities_event_id,
					wma_extscan_capabilities_event_handler,
					WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_extscan_hotlist_match_event_id,
				wma_extscan_hotlist_match_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_extscan_wlan_change_results_event_id,
				wma_extscan_change_results_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_extscan_operation_event_id,
				wma_extscan_operations_event_handler,
				WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_extscan_table_usage_event_id,
				wma_extscan_table_usage_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_extscan_cached_results_event_id,
				wma_extscan_cached_results_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_passpoint_match_event_id,
			wma_passpoint_match_event_handler,
			WMA_RX_SERIALIZER_CTX);
}

/**
 * wma_extscan_start_stop_event_handler() -  extscan start/stop event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: data length
 *
 * This function handles different extscan related commands
 * like start/stop/get results etc and indicate to upper layers.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_start_stop_event_handler(void *handle,
					 uint8_t *cmd_param_info,
					 uint32_t len)
{
	WMI_EXTSCAN_START_STOP_EVENTID_param_tlvs *param_buf;
	wmi_extscan_start_stop_event_fixed_param *event;
	struct sir_extscan_generic_response   *extscan_ind;
	uint16_t event_type;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_START_STOP_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid extscan event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	extscan_ind = qdf_mem_malloc(sizeof(*extscan_ind));
	if (!extscan_ind)
		return -ENOMEM;

	switch (event->command) {
	case WMI_EXTSCAN_START_CMDID:
		event_type = eSIR_EXTSCAN_START_RSP;
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		break;
	case WMI_EXTSCAN_STOP_CMDID:
		event_type = eSIR_EXTSCAN_STOP_RSP;
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		break;
	case WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP)
			event_type =
				eSIR_EXTSCAN_RESET_SIGNIFICANT_WIFI_CHANGE_RSP;
		else
			event_type =
				eSIR_EXTSCAN_SET_SIGNIFICANT_WIFI_CHANGE_RSP;
		break;
	case WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP)
			event_type = eSIR_EXTSCAN_RESET_BSSID_HOTLIST_RSP;
		else
			event_type = eSIR_EXTSCAN_SET_BSSID_HOTLIST_RSP;
		break;
	case WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		event_type = eSIR_EXTSCAN_CACHED_RESULTS_RSP;
		break;
	case WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP)
			event_type =
				eSIR_EXTSCAN_RESET_SSID_HOTLIST_RSP;
		else
			event_type =
				eSIR_EXTSCAN_SET_SSID_HOTLIST_RSP;
		break;
	default:
		wma_err("Unknown event(%d) from target", event->status);
		qdf_mem_free(extscan_ind);
		return -EINVAL;
	}
	mac->sme.ext_scan_ind_cb(mac->hdd_handle, event_type, extscan_ind);
	wma_debug("sending event to umac for requestid %u with status %d",
		 extscan_ind->request_id, extscan_ind->status);
	qdf_mem_free(extscan_ind);
	return 0;
}

/**
 * wma_extscan_operations_event_handler() - extscan operation event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length
 *
 * This function handles different operations related event and indicate
 * upper layers with appropriate callback.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_operations_event_handler(void *handle,
					 uint8_t *cmd_param_info,
					 uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_EXTSCAN_OPERATION_EVENTID_param_tlvs *param_buf;
	wmi_extscan_operation_event_fixed_param *oprn_event;
	tSirExtScanOnScanEventIndParams *oprn_ind;
	uint32_t cnt;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_OPERATION_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid scan operation event");
		return -EINVAL;
	}
	oprn_event = param_buf->fixed_param;
	oprn_ind = qdf_mem_malloc(sizeof(*oprn_ind));
	if (!oprn_ind)
		return -ENOMEM;

	oprn_ind->requestId = oprn_event->request_id;

	switch (oprn_event->event) {
	case WMI_EXTSCAN_BUCKET_COMPLETED_EVENT:
		oprn_ind->status = 0;
		goto exit_handler;
	case WMI_EXTSCAN_CYCLE_STARTED_EVENT:
		wma_debug("received WMI_EXTSCAN_CYCLE_STARTED_EVENT");

		if (oprn_event->num_buckets > param_buf->num_bucket_id) {
			wma_err("FW mesg num_buk %d more than TLV hdr %d",
				 oprn_event->num_buckets,
				 param_buf->num_bucket_id);
			qdf_mem_free(oprn_ind);
			return -EINVAL;
		}

		cds_host_diag_log_work(&wma->extscan_wake_lock,
				       WMA_EXTSCAN_CYCLE_WAKE_LOCK_DURATION,
				       WIFI_POWER_EVENT_WAKELOCK_EXT_SCAN);
		qdf_wake_lock_timeout_acquire(&wma->extscan_wake_lock,
				      WMA_EXTSCAN_CYCLE_WAKE_LOCK_DURATION);
		oprn_ind->scanEventType = WIFI_EXTSCAN_CYCLE_STARTED_EVENT;
		oprn_ind->status = 0;
		oprn_ind->buckets_scanned = 0;
		for (cnt = 0; cnt < oprn_event->num_buckets; cnt++)
			oprn_ind->buckets_scanned |=
				(1 << param_buf->bucket_id[cnt]);
		wma_debug("num_buckets %u request_id %u buckets_scanned %u",
			oprn_event->num_buckets, oprn_ind->requestId,
			oprn_ind->buckets_scanned);
		break;
	case WMI_EXTSCAN_CYCLE_COMPLETED_EVENT:
		wma_debug("received WMI_EXTSCAN_CYCLE_COMPLETED_EVENT");
		qdf_wake_lock_release(&wma->extscan_wake_lock,
				      WIFI_POWER_EVENT_WAKELOCK_EXT_SCAN);
		oprn_ind->scanEventType = WIFI_EXTSCAN_CYCLE_COMPLETED_EVENT;
		oprn_ind->status = 0;
		/* Set bucket scanned mask to zero on cycle complete */
		oprn_ind->buckets_scanned = 0;
		break;
	case WMI_EXTSCAN_BUCKET_STARTED_EVENT:
		wma_debug("received WMI_EXTSCAN_BUCKET_STARTED_EVENT");
		oprn_ind->scanEventType = WIFI_EXTSCAN_BUCKET_STARTED_EVENT;
		oprn_ind->status = 0;
		goto exit_handler;
	case WMI_EXTSCAN_THRESHOLD_NUM_SCANS:
		wma_debug("received WMI_EXTSCAN_THRESHOLD_NUM_SCANS");
		oprn_ind->scanEventType = WIFI_EXTSCAN_THRESHOLD_NUM_SCANS;
		oprn_ind->status = 0;
		break;
	case WMI_EXTSCAN_THRESHOLD_PERCENT:
		wma_debug("received WMI_EXTSCAN_THRESHOLD_PERCENT");
		oprn_ind->scanEventType = WIFI_EXTSCAN_THRESHOLD_PERCENT;
		oprn_ind->status = 0;
		break;
	default:
		wma_err("Unknown event(%d) from target", oprn_event->event);
		qdf_mem_free(oprn_ind);
		return -EINVAL;
	}
	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_SCAN_PROGRESS_EVENT_IND, oprn_ind);
	wma_debug("sending scan progress event to hdd");
exit_handler:
	qdf_mem_free(oprn_ind);
	return 0;
}

/**
 * wma_extscan_table_usage_event_handler() - extscan table usage event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length
 *
 * This function handles table usage related event and indicate
 * upper layers with appropriate callback.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_table_usage_event_handler(void *handle,
					  uint8_t *cmd_param_info,
					  uint32_t len)
{
	WMI_EXTSCAN_TABLE_USAGE_EVENTID_param_tlvs *param_buf;
	wmi_extscan_table_usage_event_fixed_param *event;
	tSirExtScanResultsAvailableIndParams *tbl_usg_ind;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_TABLE_USAGE_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid table usage event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	tbl_usg_ind = qdf_mem_malloc(sizeof(*tbl_usg_ind));
	if (!tbl_usg_ind)
		return -ENOMEM;

	tbl_usg_ind->requestId = event->request_id;
	tbl_usg_ind->numResultsAvailable = event->entries_in_use;
	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_SCAN_RES_AVAILABLE_IND,
				tbl_usg_ind);
	wma_debug("sending scan_res available event to hdd");
	qdf_mem_free(tbl_usg_ind);
	return 0;
}

/**
 * wma_extscan_capabilities_event_handler() - extscan capabilities event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length
 *
 * This function handles capabilities event and indicate
 * upper layers with registered callback.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_capabilities_event_handler(void *handle,
					   uint8_t *cmd_param_info,
					   uint32_t len)
{
	WMI_EXTSCAN_CAPABILITIES_EVENTID_param_tlvs *param_buf;
	wmi_extscan_capabilities_event_fixed_param *event;
	wmi_extscan_cache_capabilities *src_cache;
	wmi_extscan_hotlist_monitor_capabilities *src_hotlist;
	wmi_extscan_wlan_change_monitor_capabilities *src_change;
	struct ext_scan_capabilities_response  *dest_capab;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_CAPABILITIES_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid capabilities event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_cache = param_buf->extscan_cache_capabilities;
	src_hotlist = param_buf->hotlist_capabilities;
	src_change = param_buf->wlan_change_capabilities;

	if (!src_cache || !src_hotlist || !src_change) {
		wma_err("Invalid capabilities list");
		return -EINVAL;
	}
	dest_capab = qdf_mem_malloc(sizeof(*dest_capab));
	if (!dest_capab)
		return -ENOMEM;

	dest_capab->requestId = event->request_id;
	dest_capab->max_scan_buckets = src_cache->max_buckets;
	dest_capab->max_scan_cache_size = src_cache->scan_cache_entry_size;
	dest_capab->max_ap_cache_per_scan = src_cache->max_bssid_per_scan;
	dest_capab->max_scan_reporting_threshold =
		src_cache->max_table_usage_threshold;

	dest_capab->max_hotlist_bssids = src_hotlist->max_hotlist_entries;
	dest_capab->max_rssi_sample_size =
					src_change->max_rssi_averaging_samples;
	dest_capab->max_bssid_history_entries =
		src_change->max_rssi_history_entries;
	dest_capab->max_significant_wifi_change_aps =
		src_change->max_wlan_change_entries;
	dest_capab->max_hotlist_ssids =
				event->num_extscan_hotlist_ssid;
	dest_capab->max_number_epno_networks =
				event->num_epno_networks;
	dest_capab->max_number_epno_networks_by_ssid =
				event->num_epno_networks;
	dest_capab->max_number_of_white_listed_ssid =
				event->num_roam_ssid_whitelist;
	dest_capab->max_number_of_black_listed_bssid =
				event->num_roam_bssid_blacklist;
	dest_capab->status = 0;

	wma_debug("request_id: %u status: %d",
		 dest_capab->requestId, dest_capab->status);

	wma_debug("Capabilities: max_scan_buckets: %d, max_hotlist_bssids: %d, max_scan_cache_size: %d, max_ap_cache_per_scan: %d",
		 dest_capab->max_scan_buckets,
		 dest_capab->max_hotlist_bssids, dest_capab->max_scan_cache_size,
		 dest_capab->max_ap_cache_per_scan);
	wma_debug("max_scan_reporting_threshold: %d, max_rssi_sample_size: %d, max_bssid_history_entries: %d, max_significant_wifi_change_aps: %d",
		 dest_capab->max_scan_reporting_threshold,
		 dest_capab->max_rssi_sample_size,
		 dest_capab->max_bssid_history_entries,
		 dest_capab->max_significant_wifi_change_aps);

	wma_debug("Capabilities: max_hotlist_ssids: %d, max_number_epno_networks: %d, max_number_epno_networks_by_ssid: %d",
		 dest_capab->max_hotlist_ssids,
		 dest_capab->max_number_epno_networks,
		 dest_capab->max_number_epno_networks_by_ssid);
	wma_debug("max_number_of_white_listed_ssid: %d, max_number_of_black_listed_bssid: %d",
		 dest_capab->max_number_of_white_listed_ssid,
		 dest_capab->max_number_of_black_listed_bssid);

	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_GET_CAPABILITIES_IND, dest_capab);
	qdf_mem_free(dest_capab);
	return 0;
}

/**
 * wma_extscan_hotlist_match_event_handler() - hotlist match event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length
 *
 * This function handles hotlist match event and indicate
 * upper layers with registered callback.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_hotlist_match_event_handler(void *handle,
					    uint8_t *cmd_param_info,
					    uint32_t len)
{
	WMI_EXTSCAN_HOTLIST_MATCH_EVENTID_param_tlvs *param_buf;
	wmi_extscan_hotlist_match_event_fixed_param *event;
	struct extscan_hotlist_match *dest_hotlist;
	tSirWifiScanResult *dest_ap;
	wmi_extscan_wlan_descriptor *src_hotlist;
	uint32_t numap;
	int j, ap_found = 0;
	uint32_t buf_len;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_HOTLIST_MATCH_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid hotlist match event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_hotlist = param_buf->hotlist_match;
	numap = event->total_entries;

	if (!src_hotlist || !numap) {
		wma_err("Hotlist AP's list invalid");
		return -EINVAL;
	}
	if (numap > param_buf->num_hotlist_match) {
		wma_err("Invalid no of total enteries %d", numap);
		return -EINVAL;
	}
	if (numap > WMA_EXTSCAN_MAX_HOTLIST_ENTRIES) {
		wma_err("Total Entries %u greater than max", numap);
		numap = WMA_EXTSCAN_MAX_HOTLIST_ENTRIES;
	}

	buf_len = sizeof(wmi_extscan_hotlist_match_event_fixed_param) +
		  WMI_TLV_HDR_SIZE +
		  (numap * sizeof(wmi_extscan_wlan_descriptor));

	if (buf_len > len) {
		wma_err("Invalid buf len from FW %d numap %d", len, numap);
		return -EINVAL;
	}

	dest_hotlist = qdf_mem_malloc(sizeof(*dest_hotlist) +
				      sizeof(*dest_ap) * numap);
	if (!dest_hotlist)
		return -ENOMEM;

	dest_ap = &dest_hotlist->ap[0];
	dest_hotlist->numOfAps = event->total_entries;
	dest_hotlist->requestId = event->config_request_id;

	if (event->first_entry_index +
		event->num_entries_in_page < event->total_entries)
		dest_hotlist->moreData = 1;
	else
		dest_hotlist->moreData = 0;

	wma_debug("Hotlist match: requestId: %u numOfAps: %d",
		 dest_hotlist->requestId, dest_hotlist->numOfAps);

	/*
	 * Currently firmware sends only one bss information in-case
	 * of both hotlist ap found and lost.
	 */
	for (j = 0; j < numap; j++) {
		dest_ap->rssi = 0;
		dest_ap->channel = src_hotlist->channel;
		dest_ap->ts = src_hotlist->tstamp;
		ap_found = src_hotlist->flags & WMI_HOTLIST_FLAG_PRESENCE;
		dest_ap->rtt = src_hotlist->rtt;
		dest_ap->rtt_sd = src_hotlist->rtt_sd;
		dest_ap->beaconPeriod = src_hotlist->beacon_interval;
		dest_ap->capability = src_hotlist->capabilities;
		dest_ap->ieLength = src_hotlist->ie_length;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_hotlist->bssid,
					   dest_ap->bssid.bytes);
		if (src_hotlist->ssid.ssid_len > WLAN_SSID_MAX_LEN) {
			wma_err("Invalid SSID len %d, truncating",
				src_hotlist->ssid.ssid_len);
			src_hotlist->ssid.ssid_len = WLAN_SSID_MAX_LEN;
		}
		qdf_mem_copy(dest_ap->ssid, src_hotlist->ssid.ssid,
			     src_hotlist->ssid.ssid_len);
		dest_ap->ssid[src_hotlist->ssid.ssid_len] = '\0';
		dest_ap++;
		src_hotlist++;
	}
	dest_hotlist->ap_found = ap_found;
	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_HOTLIST_MATCH_IND, dest_hotlist);
	wma_debug("sending hotlist match event to hdd");
	qdf_mem_free(dest_hotlist);
	return 0;
}

/** wma_extscan_find_unique_scan_ids() - find unique scan ids
 * @cmd_param_info: event data.
 *
 * This utility function parses the input bss table of information
 * and find the unique number of scan ids
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_extscan_find_unique_scan_ids(const u_int8_t *cmd_param_info)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	int prev_scan_id, scan_ids_cnt, i;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;

	/* Find the unique number of scan_id's for grouping */
	prev_scan_id = src_rssi->scan_cycle_id;
	scan_ids_cnt = 1;
	for (i = 1; i < param_buf->num_rssi_list; i++) {
		src_rssi++;

		if (prev_scan_id != src_rssi->scan_cycle_id) {
			scan_ids_cnt++;
			prev_scan_id = src_rssi->scan_cycle_id;
		}
	}

	return scan_ids_cnt;
}

/** wma_fill_num_results_per_scan_id() - fill number of bss per scan id
 * @cmd_param_info: event data.
 * @scan_id_group: pointer to scan id group.
 *
 * This utility function parses the input bss table of information
 * and finds how many bss are there per unique scan id.
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_fill_num_results_per_scan_id(const u_int8_t *cmd_param_info,
			struct extscan_cached_scan_result *scan_id_group)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	struct extscan_cached_scan_result *t_scan_id_grp;
	int i, prev_scan_id;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	t_scan_id_grp = scan_id_group;

	prev_scan_id = src_rssi->scan_cycle_id;

	t_scan_id_grp->scan_id = src_rssi->scan_cycle_id;
	t_scan_id_grp->flags = src_rssi->flags;
	t_scan_id_grp->buckets_scanned = src_rssi->buckets_scanned;
	t_scan_id_grp->num_results = 1;
	for (i = 1; i < param_buf->num_rssi_list; i++) {
		src_rssi++;
		if (prev_scan_id == src_rssi->scan_cycle_id) {
			t_scan_id_grp->num_results++;
		} else {
			t_scan_id_grp++;
			prev_scan_id = t_scan_id_grp->scan_id =
				src_rssi->scan_cycle_id;
			t_scan_id_grp->flags = src_rssi->flags;
			t_scan_id_grp->buckets_scanned =
				src_rssi->buckets_scanned;
			t_scan_id_grp->num_results = 1;
		}
	}
	return 0;
}

/** wma_group_num_bss_to_scan_id() - group bss to scan id table
 * @cmd_param_info: event data.
 * @cached_result: pointer to cached table.
 *
 * This function reads the bss information from the format
 * ------------------------------------------------------------------------
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_1 | flags |
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_2 | flags |
 * ........................................................................
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_N | flags |
 * ------------------------------------------------------------------------
 *
 * and converts it into the below format and store it
 *
 * ------------------------------------------------------------------------
 * | scan id_1 | -> bss info_1 -> bss info_2 -> .... bss info_M1
 * | scan id_2 | -> bss info_1 -> bss info_2 -> .... bss info_M2
 * ......................
 * | scan id_N | -> bss info_1 -> bss info_2 -> .... bss info_Mn
 * ------------------------------------------------------------------------
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_group_num_bss_to_scan_id(const u_int8_t *cmd_param_info,
			struct extscan_cached_scan_results *cached_result)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	struct extscan_cached_scan_results *t_cached_result;
	struct extscan_cached_scan_result *t_scan_id_grp;
	int i, j;
	tSirWifiScanResult *ap;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	t_cached_result = cached_result;
	t_scan_id_grp = &t_cached_result->result[0];

	if ((t_cached_result->num_scan_ids *
	     QDF_MIN(t_scan_id_grp->num_results,
		     param_buf->num_bssid_list)) > param_buf->num_bssid_list) {
		wma_err("num_scan_ids %d, num_results %d num_bssid_list %d",
			 t_cached_result->num_scan_ids,
			 t_scan_id_grp->num_results,
			 param_buf->num_bssid_list);
		return -EINVAL;
	}

	wma_debug("num_scan_ids:%d",
			t_cached_result->num_scan_ids);
	for (i = 0; i < t_cached_result->num_scan_ids; i++) {
		wma_debug("num_results:%d", t_scan_id_grp->num_results);
		t_scan_id_grp->ap = qdf_mem_malloc(t_scan_id_grp->num_results *
						sizeof(*ap));
		if (!t_scan_id_grp->ap)
			return -ENOMEM;

		ap = &t_scan_id_grp->ap[0];
		for (j = 0; j < QDF_MIN(t_scan_id_grp->num_results,
					param_buf->num_bssid_list); j++) {
			ap->channel = src_hotlist->channel;
			ap->ts = WMA_MSEC_TO_USEC(src_rssi->tstamp);
			ap->rtt = src_hotlist->rtt;
			ap->rtt_sd = src_hotlist->rtt_sd;
			ap->beaconPeriod = src_hotlist->beacon_interval;
			ap->capability = src_hotlist->capabilities;
			ap->ieLength = src_hotlist->ie_length;

			/* Firmware already applied noise floor adjustment and
			 * due to WMI interface "UINT32 rssi", host driver
			 * receives a positive value, hence convert to
			 * signed char to get the absolute rssi.
			 */
			ap->rssi = (signed char) src_rssi->rssi;
			WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_hotlist->bssid,
						   ap->bssid.bytes);

			if (src_hotlist->ssid.ssid_len >
			    WLAN_SSID_MAX_LEN) {
				wma_debug("Invalid SSID len %d, truncating",
					 src_hotlist->ssid.ssid_len);
				src_hotlist->ssid.ssid_len =
						WLAN_SSID_MAX_LEN;
			}
			qdf_mem_copy(ap->ssid, src_hotlist->ssid.ssid,
					src_hotlist->ssid.ssid_len);
			ap->ssid[src_hotlist->ssid.ssid_len] = '\0';
			ap++;
			src_rssi++;
			src_hotlist++;
		}
		t_scan_id_grp++;
	}
	return 0;
}

/**
 * wma_extscan_cached_results_event_handler() - cached results event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length of @cmd_param_info
 *
 * This function handles cached results event and indicate
 * cached results to upper layer.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_cached_results_event_handler(void *handle,
					     uint8_t *cmd_param_info,
					     uint32_t len)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param *event;
	struct extscan_cached_scan_results *dest_cachelist;
	struct extscan_cached_scan_result *dest_result;
	struct extscan_cached_scan_results empty_cachelist;
	wmi_extscan_wlan_descriptor *src_hotlist;
	wmi_extscan_rssi_info *src_rssi;
	int i, moredata, scan_ids_cnt, buf_len, status;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint32_t total_len;
	bool excess_data = false;

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid cached results event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	wma_debug("Total_entries: %u first_entry_index: %u num_entries_in_page: %d",
		 event->total_entries,
		 event->first_entry_index,
		 event->num_entries_in_page);

	if (!src_hotlist || !src_rssi || !event->num_entries_in_page) {
		wma_warn("Cached results empty, send 0 results");
		goto noresults;
	}

	if (event->num_entries_in_page >
	    (WMI_SVC_MSG_MAX_SIZE - sizeof(*event))/sizeof(*src_hotlist) ||
	    event->num_entries_in_page > param_buf->num_bssid_list) {
		wma_err("excess num_entries_in_page %d in WMI event. num_bssid_list %d",
			 event->num_entries_in_page, param_buf->num_bssid_list);
		return -EINVAL;
	} else {
		total_len = sizeof(*event) +
			(event->num_entries_in_page * sizeof(*src_hotlist));
	}
	for (i = 0; i < event->num_entries_in_page; i++) {
		if (src_hotlist[i].ie_length >
		    WMI_SVC_MSG_MAX_SIZE - total_len) {
			excess_data = true;
			break;
		} else {
			total_len += src_hotlist[i].ie_length;
			wma_debug("total len IE: %d", total_len);
		}

		if (src_hotlist[i].number_rssi_samples >
		    (WMI_SVC_MSG_MAX_SIZE - total_len) / sizeof(*src_rssi)) {
			excess_data = true;
			break;
		} else {
			total_len += (src_hotlist[i].number_rssi_samples *
					sizeof(*src_rssi));
			wma_debug("total len RSSI samples: %d", total_len);
		}
	}
	if (excess_data) {
		wma_err("excess data in WMI event");
		return -EINVAL;
	}

	if (event->first_entry_index +
	    event->num_entries_in_page < event->total_entries)
		moredata = 1;
	else
		moredata = 0;

	dest_cachelist = qdf_mem_malloc(sizeof(*dest_cachelist));
	if (!dest_cachelist)
		return -ENOMEM;

	qdf_mem_zero(dest_cachelist, sizeof(*dest_cachelist));
	dest_cachelist->request_id = event->request_id;
	dest_cachelist->more_data = moredata;

	scan_ids_cnt = wma_extscan_find_unique_scan_ids(cmd_param_info);
	wma_debug("scan_ids_cnt %d", scan_ids_cnt);
	dest_cachelist->num_scan_ids = scan_ids_cnt;

	buf_len = sizeof(*dest_result) * scan_ids_cnt;
	dest_cachelist->result = qdf_mem_malloc(buf_len);
	if (!dest_cachelist->result) {
		qdf_mem_free(dest_cachelist);
		return -ENOMEM;
	}

	dest_result = dest_cachelist->result;
	wma_fill_num_results_per_scan_id(cmd_param_info, dest_result);

	status = wma_group_num_bss_to_scan_id(cmd_param_info, dest_cachelist);
	if (!status)
	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_CACHED_RESULTS_IND,
				dest_cachelist);
	else
		wma_debug("wma_group_num_bss_to_scan_id failed, not calling callback");

	dest_result = dest_cachelist->result;
	for (i = 0; i < dest_cachelist->num_scan_ids; i++) {
		if (dest_result->ap)
		qdf_mem_free(dest_result->ap);
		dest_result++;
	}
	qdf_mem_free(dest_cachelist->result);
	qdf_mem_free(dest_cachelist);
	return status;

noresults:
	empty_cachelist.request_id = event->request_id;
	empty_cachelist.more_data = 0;
	empty_cachelist.num_scan_ids = 0;

	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_EXTSCAN_CACHED_RESULTS_IND,
				&empty_cachelist);
	return 0;
}

/**
 * wma_extscan_change_results_event_handler() - change results event handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: length
 *
 * This function handles change results event and indicate
 * change results to upper layer.
 *
 * Return: 0 for success or error code.
 */
int wma_extscan_change_results_event_handler(void *handle,
					     uint8_t *cmd_param_info,
					     uint32_t len)
{
	WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_wlan_change_results_event_fixed_param *event;
	tSirWifiSignificantChangeEvent *dest_chglist;
	tSirWifiSignificantChange *dest_ap;
	wmi_extscan_wlan_change_result_bssid *src_chglist;

	uint32_t numap;
	int i, k;
	uint8_t *src_rssi;
	int count = 0;
	int moredata;
	uint32_t rssi_num = 0;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint32_t buf_len;
	bool excess_data = false;

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}
	param_buf = (WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID_param_tlvs *)
		    cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid change monitor event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_chglist = param_buf->bssid_signal_descriptor_list;
	src_rssi = param_buf->rssi_list;
	numap = event->num_entries_in_page;

	if (!src_chglist || !numap) {
		wma_err("Results invalid");
		return -EINVAL;
	}
	if (numap > param_buf->num_bssid_signal_descriptor_list) {
		wma_err("Invalid num of entries in page: %d", numap);
		return -EINVAL;
	}
	for (i = 0; i < numap; i++) {
		if (src_chglist->num_rssi_samples > (UINT_MAX - rssi_num)) {
			wma_err("Invalid num of rssi samples %d numap %d rssi_num %d",
				 src_chglist->num_rssi_samples,
				 numap, rssi_num);
			return -EINVAL;
		}
		rssi_num += src_chglist->num_rssi_samples;
		src_chglist++;
	}
	src_chglist = param_buf->bssid_signal_descriptor_list;

	if (event->first_entry_index +
	    event->num_entries_in_page < event->total_entries) {
		moredata = 1;
	} else {
		moredata = 0;
	}

	do {
		if (event->num_entries_in_page >
			(WMI_SVC_MSG_MAX_SIZE - sizeof(*event))/
			sizeof(*src_chglist)) {
			excess_data = true;
			break;
		} else {
			buf_len =
				sizeof(*event) + (event->num_entries_in_page *
						sizeof(*src_chglist));
		}
		if (rssi_num >
			(WMI_SVC_MSG_MAX_SIZE - buf_len)/sizeof(int32_t)) {
			excess_data = true;
			break;
		}
	} while (0);

	if (excess_data) {
		wma_err("buffer len exceeds WMI payload,numap:%d, rssi_num:%d",
			numap, rssi_num);
		QDF_ASSERT(0);
		return -EINVAL;
	}
	dest_chglist = qdf_mem_malloc(sizeof(*dest_chglist) +
				      sizeof(*dest_ap) * numap +
				      sizeof(int32_t) * rssi_num);
	if (!dest_chglist)
		return -ENOMEM;

	dest_ap = &dest_chglist->ap[0];
	for (i = 0; i < numap; i++) {
		dest_ap->channel = src_chglist->channel;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_chglist->bssid,
					   dest_ap->bssid.bytes);
		dest_ap->numOfRssi = src_chglist->num_rssi_samples;
		if (dest_ap->numOfRssi) {
			if ((dest_ap->numOfRssi + count) >
			    param_buf->num_rssi_list) {
				wma_err("Invalid num in rssi list: %d",
					dest_ap->numOfRssi);
				qdf_mem_free(dest_chglist);
				return -EINVAL;
			}
			for (k = 0; k < dest_ap->numOfRssi; k++) {
				dest_ap->rssi[k] = WMA_TGT_NOISE_FLOOR_DBM +
						   src_rssi[count++];
			}
		}
		dest_ap = (tSirWifiSignificantChange *)((char *)dest_ap +
					dest_ap->numOfRssi * sizeof(int32_t) +
					sizeof(*dest_ap));
		src_chglist++;
	}
	dest_chglist->requestId = event->request_id;
	dest_chglist->moreData = moredata;
	dest_chglist->numResults = numap;

	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
			eSIR_EXTSCAN_SIGNIFICANT_WIFI_CHANGE_RESULTS_IND,
			dest_chglist);
	wma_debug("sending change monitor results");
	qdf_mem_free(dest_chglist);
	return 0;
}

/**
 * wma_passpoint_match_event_handler() - passpoint match found event handler
 * @handle: WMA handle
 * @cmd_param_info: event data
 * @len: event data length
 *
 * This is the passpoint match found event handler; it reads event data from
 * @cmd_param_info and fill in the destination buffer and sends indication
 * up layer.
 *
 * Return: 0 on success; error number otherwise
 */
int wma_passpoint_match_event_handler(void *handle,
				     uint8_t  *cmd_param_info,
				     uint32_t len)
{
	WMI_PASSPOINT_MATCH_EVENTID_param_tlvs *param_buf;
	wmi_passpoint_event_hdr  *event;
	struct wifi_passpoint_match  *dest_match;
	tSirWifiScanResult      *dest_ap;
	uint8_t *buf_ptr;
	uint32_t buf_len = 0;
	bool excess_data = false;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac");
		return -EINVAL;
	}
	if (!mac->sme.ext_scan_ind_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	param_buf = (WMI_PASSPOINT_MATCH_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid passpoint match event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	buf_ptr = (uint8_t *)param_buf->fixed_param;

	do {
		if (event->ie_length > (WMI_SVC_MSG_MAX_SIZE)) {
			excess_data = true;
			break;
		} else {
			buf_len = event->ie_length;
		}

		if (event->anqp_length > (WMI_SVC_MSG_MAX_SIZE)) {
			excess_data = true;
			break;
		} else {
			buf_len += event->anqp_length;
		}

	} while (0);

	if (excess_data || buf_len > (WMI_SVC_MSG_MAX_SIZE - sizeof(*event)) ||
	    buf_len > (WMI_SVC_MSG_MAX_SIZE - sizeof(*dest_match)) ||
	    (event->ie_length + event->anqp_length) > param_buf->num_bufp) {
		wma_err("IE Length: %u or ANQP Length: %u is huge, num_bufp: %u",
			event->ie_length, event->anqp_length,
			param_buf->num_bufp);
		return -EINVAL;
	}

	if (event->ssid.ssid_len > WLAN_SSID_MAX_LEN) {
		wma_debug("Invalid ssid len %d, truncating",
			 event->ssid.ssid_len);
		event->ssid.ssid_len = WLAN_SSID_MAX_LEN;
	}

	dest_match = qdf_mem_malloc(sizeof(*dest_match) + buf_len);
	if (!dest_match)
		return -EINVAL;

	dest_ap = &dest_match->ap;
	dest_match->request_id = 0;
	dest_match->id = event->id;
	dest_match->anqp_len = event->anqp_length;
	wma_info("passpoint match: id: %u anqp length %u",
		 dest_match->id, dest_match->anqp_len);

	dest_ap->channel = event->channel_mhz;
	dest_ap->ts = event->timestamp;
	dest_ap->rtt = event->rtt;
	dest_ap->rssi = event->rssi;
	dest_ap->rtt_sd = event->rtt_sd;
	dest_ap->beaconPeriod = event->beacon_period;
	dest_ap->capability = event->capability;
	dest_ap->ieLength = event->ie_length;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->bssid, dest_ap->bssid.bytes);
	qdf_mem_copy(dest_ap->ssid, event->ssid.ssid,
				event->ssid.ssid_len);
	dest_ap->ssid[event->ssid.ssid_len] = '\0';
	qdf_mem_copy(dest_ap->ieData, buf_ptr + sizeof(*event) +
			WMI_TLV_HDR_SIZE, dest_ap->ieLength);
	qdf_mem_copy(dest_match->anqp, buf_ptr + sizeof(*event) +
			WMI_TLV_HDR_SIZE + dest_ap->ieLength,
			dest_match->anqp_len);

	mac->sme.ext_scan_ind_cb(mac->hdd_handle,
				eSIR_PASSPOINT_NETWORK_FOUND_IND,
				dest_match);
	wma_debug("sending passpoint match event to hdd");
	qdf_mem_free(dest_match);
	return 0;
}

QDF_STATUS wma_start_extscan(tp_wma_handle wma,
			     struct wifi_scan_cmd_req_params *params)
{
	QDF_STATUS status;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	if (!params) {
		wma_err("NULL param");
		return QDF_STATUS_E_NOMEM;
	}

	status = wmi_unified_start_extscan_cmd(wma->wmi_handle, params);
	if (QDF_IS_STATUS_SUCCESS(status))
		wma->interfaces[params->vdev_id].extscan_in_progress = true;

	wma_debug("Exit, vdev %d, status %d", params->vdev_id, status);

	return status;
}

QDF_STATUS wma_stop_extscan(tp_wma_handle wma,
			    struct extscan_stop_req_params *params)
{
	QDF_STATUS status;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, cannot issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	status = wmi_unified_stop_extscan_cmd(wma->wmi_handle, params);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma->interfaces[params->vdev_id].extscan_in_progress = false;
	wma_debug("Extscan stop request sent successfully for vdev %d",
		 params->vdev_id);

	return status;
}

QDF_STATUS wma_extscan_start_hotlist_monitor(tp_wma_handle wma,
			struct extscan_bssid_hotlist_set_params *params)
{
	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue hotlist cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (!params) {
		wma_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_extscan_start_hotlist_monitor_cmd(wma->wmi_handle,
							     params);
}

QDF_STATUS wma_extscan_stop_hotlist_monitor(tp_wma_handle wma,
		    struct extscan_bssid_hotlist_reset_params *params)
{
	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (!params) {
		wma_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle,
				 wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_extscan_stop_hotlist_monitor_cmd(wma->wmi_handle,
							    params);
}

QDF_STATUS
wma_extscan_start_change_monitor(tp_wma_handle wma,
			struct extscan_set_sig_changereq_params *params)
{
	QDF_STATUS status;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed,can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (!params) {
		wma_err("NULL params");
		return QDF_STATUS_E_NOMEM;
	}

	status = wmi_unified_extscan_start_change_monitor_cmd(wma->wmi_handle,
							      params);
	return status;
}

QDF_STATUS wma_extscan_stop_change_monitor(tp_wma_handle wma,
			struct extscan_capabilities_reset_params *params)
{
	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle,
				    wmi_service_extscan)) {
		wma_err("ext scan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_extscan_stop_change_monitor_cmd(wma->wmi_handle,
							   params);
}

QDF_STATUS
wma_extscan_get_cached_results(tp_wma_handle wma,
			       struct extscan_cached_result_params *params)
{
	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, cannot issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_extscan_get_cached_results_cmd(wma->wmi_handle,
							  params);
}

QDF_STATUS
wma_extscan_get_capabilities(tp_wma_handle wma,
			     struct extscan_capabilities_params *params)
{
	if (!wma || !wma->wmi_handle) {
		wma_er("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_extscan_get_capabilities_cmd(wma->wmi_handle,
							params);
}

QDF_STATUS wma_set_epno_network_list(tp_wma_handle wma,
				     struct wifi_enhanced_pno_params *req)
{
	QDF_STATUS status;

	wma_debug("Enter");

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	status = wmi_unified_set_epno_network_list_cmd(wma->wmi_handle, req);
	wma_debug("Exit, vdev %d, status %d", req->vdev_id, status);

	return status;
}

QDF_STATUS
wma_set_passpoint_network_list(tp_wma_handle wma,
			       struct wifi_passpoint_req_param *params)
{
	QDF_STATUS status;

	wma_debug("Enter");

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_FAILURE;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	status = wmi_unified_set_passpoint_network_list_cmd(wma->wmi_handle,
							    params);
	wma_debug("Exit, vdev %d, status %d", params->vdev_id, status);

	return status;
}

QDF_STATUS
wma_reset_passpoint_network_list(tp_wma_handle wma,
				 struct wifi_passpoint_req_param *params)
{
	QDF_STATUS status;

	wma_debug("Enter");

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_FAILURE;
	}
	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_extscan)) {
		wma_err("extscan not enabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	status = wmi_unified_reset_passpoint_network_list_cmd(wma->wmi_handle,
							      params);
	wma_debug("Exit, vdev %d, status %d", params->vdev_id, status);

	return status;
}

#endif

QDF_STATUS wma_scan_probe_setoui(tp_wma_handle wma,
				 struct scan_mac_oui *set_oui)
{
	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_is_vdev_valid(set_oui->vdev_id)) {
		wma_err("vdev_id: %d is not active", set_oui->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_scan_probe_setoui_cmd(wma->wmi_handle, set_oui);
}

/**
 * wma_roam_better_ap_handler() - better ap event handler
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Handler for WMI_ROAM_REASON_BETTER_AP event from roam firmware in Rome.
 * This event means roam algorithm in Rome has found a better matching
 * candidate AP. The indication is sent to SME.
 *
 * Return: none
 */
void wma_roam_better_ap_handler(tp_wma_handle wma, uint32_t vdev_id)
{
	struct scheduler_msg cds_msg = {0};
	tSirSmeCandidateFoundInd *candidate_ind;
	QDF_STATUS status;

	candidate_ind = qdf_mem_malloc(sizeof(tSirSmeCandidateFoundInd));
	if (!candidate_ind)
		return;

	wma->interfaces[vdev_id].roaming_in_progress = true;
	candidate_ind->messageType = eWNI_SME_CANDIDATE_FOUND_IND;
	candidate_ind->sessionId = vdev_id;
	candidate_ind->length = sizeof(tSirSmeCandidateFoundInd);

	cds_msg.type = eWNI_SME_CANDIDATE_FOUND_IND;
	cds_msg.bodyptr = candidate_ind;
	cds_msg.callback = sme_mc_process_handler;
	wma_debug("Posting candidate ind to SME, vdev %d", vdev_id);

	status = scheduler_post_message(QDF_MODULE_ID_WMA, QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SCAN,  &cds_msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(candidate_ind);
}

/**
 * wma_handle_hw_mode_in_roam_fail() - Fill hw mode info if present in policy
 * manager.
 * @wma: wma handle
 * @param: roam event params
 *
 * Return: None
 */
static int wma_handle_hw_mode_transition(tp_wma_handle wma,
					 WMI_ROAM_EVENTID_param_tlvs *param)
{
	struct sir_hw_mode_trans_ind *hw_mode_trans_ind;
	struct scheduler_msg sme_msg = {0};
	QDF_STATUS status;

	if (param->hw_mode_transition_fixed_param) {
		hw_mode_trans_ind = qdf_mem_malloc(sizeof(*hw_mode_trans_ind));
		if (!hw_mode_trans_ind)
			return -ENOMEM;
		wma_process_pdev_hw_mode_trans_ind(wma,
		    param->hw_mode_transition_fixed_param,
		    param->wmi_pdev_set_hw_mode_response_vdev_mac_mapping,
		    hw_mode_trans_ind);

		wma_debug("Update HW mode");
		sme_msg.type = eWNI_SME_HW_MODE_TRANS_IND;
		sme_msg.bodyptr = hw_mode_trans_ind;

		status = scheduler_post_message(QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_SME,
						QDF_MODULE_ID_SME, &sme_msg);
		if (QDF_IS_STATUS_ERROR(status))
			qdf_mem_free(hw_mode_trans_ind);
	} else {
		wma_debug("hw_mode transition fixed param is NULL");
	}

	return 0;
}

/**
 * wma_invalid_roam_reason_handler() - Handle Invalid roam notification
 * @wma: wma handle
 * @vdev_id: vdev id
 * @op_code: Operation to be done by the callback
 *
 * This function calls pe and csr callbacks with proper op_code
 *
 * Return: None
 */
static void wma_invalid_roam_reason_handler(tp_wma_handle wma_handle,
					    uint32_t vdev_id,
					    uint32_t notif)
{
	struct roam_offload_synch_ind *roam_synch_data;
	enum sir_roam_op_code op_code;

	if (notif == WMI_ROAM_NOTIF_ROAM_START) {
		wma_handle->interfaces[vdev_id].roaming_in_progress = true;
		op_code = SIR_ROAMING_START;
	} else if (notif == WMI_ROAM_NOTIF_ROAM_ABORT) {
		wma_handle->interfaces[vdev_id].roaming_in_progress = false;
		op_code = SIR_ROAMING_ABORT;
		lim_sae_auth_cleanup_retry(wma_handle->mac_context, vdev_id);
	} else {
		wma_debug("Invalid notif %d", notif);
		return;
	}

	roam_synch_data = qdf_mem_malloc(sizeof(*roam_synch_data));
	if (!roam_synch_data)
		return;

	roam_synch_data->roamed_vdev_id = vdev_id;
	if (notif != WMI_ROAM_NOTIF_ROAM_START)
		wma_handle->pe_roam_synch_cb(wma_handle->mac_context,
					     roam_synch_data, NULL, op_code);

	wma_handle->csr_roam_synch_cb(wma_handle->mac_context, roam_synch_data,
				      NULL, op_code);
	qdf_mem_free(roam_synch_data);
}

void wma_handle_roam_sync_timeout(tp_wma_handle wma_handle,
				  struct roam_sync_timeout_timer_info *info)
{
	wma_invalid_roam_reason_handler(wma_handle, info->vdev_id,
					WMI_ROAM_NOTIF_ROAM_ABORT);
}

static char *wma_get_roam_event_reason_string(uint32_t reason)
{
	switch (reason) {
	case WMI_ROAM_REASON_INVALID:
		return "Default";
	case WMI_ROAM_REASON_BETTER_AP:
		return "Better AP";
	case WMI_ROAM_REASON_BMISS:
		return "BMISS";
	case WMI_ROAM_REASON_LOW_RSSI:
		return "Low Rssi";
	case WMI_ROAM_REASON_SUITABLE_AP:
		return "Suitable AP";
	case WMI_ROAM_REASON_HO_FAILED:
		return "Hand-off Failed";
	case WMI_ROAM_REASON_INVOKE_ROAM_FAIL:
		return "Roam Invoke failed";
	case WMI_ROAM_REASON_RSO_STATUS:
		return "RSO status";
	case WMI_ROAM_REASON_BTM:
		return "BTM";
	case WMI_ROAM_REASON_DEAUTH:
		return "Deauth";
	default:
		return "Invalid";
	}

	return "Invalid";
}

/**
 * wma_roam_event_callback() - roam event callback
 * @handle: wma handle
 * @event_buf: event buffer
 * @len: buffer length
 *
 * Handler for all events from roam engine in firmware
 *
 * Return: 0 for success or error code
 */
int wma_roam_event_callback(WMA_HANDLE handle, uint8_t *event_buf,
				   uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_ROAM_EVENTID_param_tlvs *param_buf;
	wmi_roam_event_fixed_param *wmi_event;
	struct roam_offload_synch_ind *roam_synch_data;
	uint8_t *frame = NULL;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	struct qdf_mac_addr bssid;
#endif

	param_buf = (WMI_ROAM_EVENTID_param_tlvs *) event_buf;
	if (!param_buf) {
		wma_err("Invalid roam event buffer");
		return -EINVAL;
	}

	wmi_event = param_buf->fixed_param;
	if (wmi_event->vdev_id >= wma_handle->max_bssid) {
		wma_err("Invalid vdev id from firmware");
		return -EINVAL;
	}
	wlan_roam_debug_log(wmi_event->vdev_id, DEBUG_ROAM_EVENT,
			    DEBUG_INVALID_PEER_ID, NULL, NULL,
			    wmi_event->reason,
			    (wmi_event->reason == WMI_ROAM_REASON_INVALID) ?
				wmi_event->notif : wmi_event->rssi);

	DPTRACE(qdf_dp_trace_record_event(QDF_DP_TRACE_EVENT_RECORD,
		wmi_event->vdev_id, QDF_TRACE_DEFAULT_PDEV_ID,
		QDF_PROTO_TYPE_EVENT, QDF_ROAM_EVENTID));

	wma_debug("FW_ROAM_EVT: Reason:%s[%d], Notif %x for vdevid %x, rssi %d",
		  wma_get_roam_event_reason_string(wmi_event->reason),
		  wmi_event->reason,
		  wmi_event->notif, wmi_event->vdev_id, wmi_event->rssi);

	switch (wmi_event->reason) {
	case WMI_ROAM_REASON_BTM:
		/*
		 * This event is received from firmware if firmware is unable to
		 * find candidate AP after roam scan and BTM request from AP
		 * has disassoc imminent bit set.
		 */
		wma_debug("Kickout due to btm request");
		wma_sta_kickout_event(HOST_STA_KICKOUT_REASON_BTM,
				      wmi_event->vdev_id, NULL);
		wma_handle_disconnect_reason(wma_handle, wmi_event->vdev_id,
				HAL_DEL_STA_REASON_CODE_BTM_DISASSOC_IMMINENT);
		break;
	case WMI_ROAM_REASON_BMISS:
		/*
		 * WMI_ROAM_REASON_BMISS can get called in soft IRQ context, so
		 * avoid using CSR/PE structure directly
		 */
		wma_debug("Beacon Miss for vdevid %x", wmi_event->vdev_id);
		wma_beacon_miss_handler(wma_handle, wmi_event->vdev_id,
					wmi_event->rssi);
		wma_sta_kickout_event(HOST_STA_KICKOUT_REASON_BMISS,
						wmi_event->vdev_id, NULL);
		break;
	case WMI_ROAM_REASON_BETTER_AP:
		/*
		 * WMI_ROAM_REASON_BETTER_AP can get called in soft IRQ context,
		 * so avoid using CSR/PE structure directly.
		 */
		wma_debug("Better AP found for vdevid %x, rssi %d",
			 wmi_event->vdev_id, wmi_event->rssi);
		mlme_set_roam_reason_better_ap(
			wma_handle->interfaces[wmi_event->vdev_id].vdev, false);
		wma_roam_better_ap_handler(wma_handle, wmi_event->vdev_id);
		break;
	case WMI_ROAM_REASON_SUITABLE_AP:
		/*
		 * WMI_ROAM_REASON_SUITABLE_AP can get called in soft IRQ
		 * context, so avoid using CSR/PE structure directly.
		 */
		mlme_set_roam_reason_better_ap(
			wma_handle->interfaces[wmi_event->vdev_id].vdev, true);
		mlme_set_hb_ap_rssi(
			wma_handle->interfaces[wmi_event->vdev_id].vdev,
			wmi_event->rssi);
		wma_debug("Bmiss scan AP found for vdevid %x, rssi %d",
			 wmi_event->vdev_id, wmi_event->rssi);
		wma_roam_better_ap_handler(wma_handle, wmi_event->vdev_id);
		break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case WMI_ROAM_REASON_HO_FAILED:
		/*
		 * WMI_ROAM_REASON_HO_FAILED can get called in soft IRQ context,
		 * so avoid using CSR/PE structure directly.
		 */
		wma_err("LFR3:Hand-Off Failed for vdevid %x",
			 wmi_event->vdev_id);
		bssid.bytes[0] = wmi_event->notif_params >> 0 & 0xFF;
		bssid.bytes[1] = wmi_event->notif_params >> 8 & 0xFF;
		bssid.bytes[2] = wmi_event->notif_params >> 16 & 0xFF;
		bssid.bytes[3] = wmi_event->notif_params >> 24 & 0xFF;
		bssid.bytes[4] = wmi_event->notif_params1 >> 0 & 0xFF;
		bssid.bytes[5] = wmi_event->notif_params1 >> 8 & 0xFF;
		wma_debug("mac addr to avoid "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(bssid.bytes));
		wma_handle_hw_mode_transition(wma_handle, param_buf);
		wma_roam_ho_fail_handler(wma_handle, wmi_event->vdev_id, bssid);
		lim_sae_auth_cleanup_retry(wma_handle->mac_context,
					   wmi_event->vdev_id);
		break;
#endif
	case WMI_ROAM_REASON_INVALID:
		wma_invalid_roam_reason_handler(wma_handle, wmi_event->vdev_id,
						wmi_event->notif);
		if (wmi_event->notif == WMI_ROAM_NOTIF_SCAN_START ||
		    wmi_event->notif == WMI_ROAM_NOTIF_SCAN_END)
			wma_report_real_time_roam_stats(
						wma_handle->psoc,
						wmi_event->vdev_id,
						ROAM_RT_STATS_TYPE_SCAN_STATE,
						NULL, wmi_event->notif);
		break;
	case WMI_ROAM_REASON_RSO_STATUS:
		wma_rso_cmd_status_event_handler(wmi_event);
		break;
	case WMI_ROAM_REASON_INVOKE_ROAM_FAIL:
		wma_handle_hw_mode_transition(wma_handle, param_buf);
		roam_synch_data = qdf_mem_malloc(sizeof(*roam_synch_data));
		if (!roam_synch_data)
			return -ENOMEM;

		lim_sae_auth_cleanup_retry(wma_handle->mac_context,
					   wmi_event->vdev_id);
		roam_synch_data->roamed_vdev_id = wmi_event->vdev_id;
		wma_handle->csr_roam_synch_cb(wma_handle->mac_context,
					      roam_synch_data, NULL,
					      SIR_ROAMING_INVOKE_FAIL);
		wlan_cm_update_roam_states(wma_handle->psoc, wmi_event->vdev_id,
					   wmi_event->notif_params,
					   ROAM_INVOKE_FAIL_REASON);
		wma_report_real_time_roam_stats(
					wma_handle->psoc, wmi_event->vdev_id,
					ROAM_RT_STATS_TYPE_INVOKE_FAIL_REASON,
					NULL, wmi_event->notif_params);
		qdf_mem_free(roam_synch_data);
		break;
	case WMI_ROAM_REASON_DEAUTH:
		wma_debug("Received disconnect roam event reason:%d",
			 wmi_event->notif_params);
		if (wmi_event->notif_params1)
			frame = param_buf->deauth_disassoc_frame;
		wma_handle->pe_disconnect_cb(wma_handle->mac_context,
					     wmi_event->vdev_id,
					     frame, wmi_event->notif_params1,
					     wmi_event->notif_params);
		roam_synch_data = qdf_mem_malloc(sizeof(*roam_synch_data));
		if (!roam_synch_data)
			return -ENOMEM;

		roam_synch_data->roamed_vdev_id = wmi_event->vdev_id;
		wma_handle->csr_roam_synch_cb(
				wma_handle->mac_context,
				roam_synch_data, NULL, SIR_ROAMING_DEAUTH);
		qdf_mem_free(roam_synch_data);
		break;
	default:
		wma_debug("Unhandled Roam Event %x for vdevid %x",
			 wmi_event->reason, wmi_event->vdev_id);
		break;
	}
	return 0;
}

#ifdef FEATURE_LFR_SUBNET_DETECTION
QDF_STATUS wma_set_gateway_params(tp_wma_handle wma,
				  struct gateway_update_req_param *req)
{
	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_set_gateway_params_cmd(wma->wmi_handle, req);
}
#endif /* FEATURE_LFR_SUBNET_DETECTION */

/**
 * wma_ht40_stop_obss_scan() - ht40 obss stop scan
 * @wma: WMA handel
 * @vdev_id: vdev identifier
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS wma_ht40_stop_obss_scan(tp_wma_handle wma, int32_t vdev_id)
{
	QDF_STATUS status;
	wmi_buf_t buf;
	wmi_obss_scan_disable_cmd_fixed_param *cmd;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	wma_debug("cmd %x vdev_id %d", WMI_OBSS_SCAN_DISABLE_CMDID, vdev_id);

	cmd = (wmi_obss_scan_disable_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_obss_scan_disable_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_obss_scan_disable_cmd_fixed_param));

	cmd->vdev_id = vdev_id;
	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				      WMI_OBSS_SCAN_DISABLE_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wma_send_ht40_obss_scanind() - ht40 obss start scan indication
 * @wma: WMA handel
 * @req: start scan request
 *
 * Return: Return QDF_STATUS, otherwise appropriate failure code
 */
QDF_STATUS wma_send_ht40_obss_scanind(tp_wma_handle wma,
				struct obss_ht40_scanind *req)
{
	QDF_STATUS status;
	wmi_buf_t buf;
	wmi_obss_scan_enable_cmd_fixed_param *cmd;
	int len = 0;
	uint8_t *buf_ptr, i;
	uint8_t *channel_list;
	uint32_t *chan_freq_list;

	len += sizeof(wmi_obss_scan_enable_cmd_fixed_param);

	len += WMI_TLV_HDR_SIZE;
	len += qdf_roundup(sizeof(uint8_t) * req->channel_count,
				sizeof(uint32_t));

	len += WMI_TLV_HDR_SIZE;
	len += qdf_roundup(sizeof(uint8_t) * 1, sizeof(uint32_t));

	/* length calculation for chan_freqs */
	len += WMI_TLV_HDR_SIZE;
	len += sizeof(uint32_t) * req->channel_count;

	wma_debug("cmdlen %d vdev_id %d channel count %d iefield_len %d",
		 len, req->bss_id, req->channel_count, req->iefield_len);

	wma_debug("scantype %d active_time %d passive %d Obss interval %d",
		 req->scan_type, req->obss_active_dwelltime,
		 req->obss_passive_dwelltime,
		 req->obss_width_trigger_interval);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_obss_scan_enable_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_obss_scan_enable_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_obss_scan_enable_cmd_fixed_param));

	buf_ptr = (uint8_t *) cmd;

	cmd->vdev_id = req->bss_id;
	cmd->scan_type = req->scan_type;
	cmd->obss_scan_active_dwell =
		req->obss_active_dwelltime;
	cmd->obss_scan_passive_dwell =
		req->obss_passive_dwelltime;
	cmd->bss_channel_width_trigger_scan_interval =
		req->obss_width_trigger_interval;
	cmd->bss_width_channel_transition_delay_factor =
		req->bsswidth_ch_trans_delay;
	cmd->obss_scan_active_total_per_channel =
		req->obss_active_total_per_channel;
	cmd->obss_scan_passive_total_per_channel =
		req->obss_passive_total_per_channel;
	cmd->obss_scan_activity_threshold =
		req->obss_activity_threshold;

	cmd->channel_len = req->channel_count;
	cmd->forty_mhz_intolerant =  req->fortymhz_intolerent;
	cmd->current_operating_class = req->current_operatingclass;
	cmd->ie_len = req->iefield_len;

	buf_ptr += sizeof(wmi_obss_scan_enable_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		qdf_roundup(req->channel_count, sizeof(uint32_t)));

	buf_ptr += WMI_TLV_HDR_SIZE;
	channel_list = (uint8_t *) buf_ptr;

	for (i = 0; i < req->channel_count; i++) {
		channel_list[i] =
		  wlan_reg_freq_to_chan(wma->pdev, req->chan_freq_list[i]);
		wma_nofl_debug("Ch[%d]: %d ", i, channel_list[i]);
	}

	buf_ptr += qdf_roundup(sizeof(uint8_t) * req->channel_count,
				sizeof(uint32_t));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
			qdf_roundup(1, sizeof(uint32_t)));
	buf_ptr += WMI_TLV_HDR_SIZE;

	buf_ptr += qdf_roundup(sizeof(uint8_t) * 1, sizeof(uint32_t));

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       sizeof(uint32_t) * req->channel_count);
	buf_ptr += WMI_TLV_HDR_SIZE;

	chan_freq_list = (uint32_t *)buf_ptr;
	for (i = 0; i < req->channel_count; i++) {
		chan_freq_list[i] = req->chan_freq_list[i];
		wma_nofl_debug("freq[%u]: %u ", i, chan_freq_list[i]);
	}

	buf_ptr += sizeof(uint32_t) * req->channel_count;

	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				      WMI_OBSS_SCAN_ENABLE_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

static enum blm_reject_ap_reason wma_get_reject_reason(uint32_t reason)
{
	switch(reason) {
	case WMI_BL_REASON_NUD_FAILURE:
		return REASON_NUD_FAILURE;
	case WMI_BL_REASON_STA_KICKOUT:
		return REASON_STA_KICKOUT;
	case WMI_BL_REASON_ROAM_HO_FAILURE:
		return REASON_ROAM_HO_FAILURE;
	case WMI_BL_REASON_ASSOC_REJECT_POOR_RSSI:
		return REASON_ASSOC_REJECT_POOR_RSSI;
	case WMI_BL_REASON_ASSOC_REJECT_OCE:
		return REASON_ASSOC_REJECT_OCE;
	case WMI_BL_REASON_USERSPACE_BL:
		return REASON_USERSPACE_BL;
	case WMI_BL_REASON_USERSPACE_AVOID_LIST:
		return REASON_USERSPACE_AVOID_LIST;
	case WMI_BL_REASON_BTM_DIASSOC_IMMINENT:
		return REASON_BTM_DISASSOC_IMMINENT;
	case WMI_BL_REASON_BTM_BSS_TERMINATION:
		return REASON_BTM_BSS_TERMINATION;
	case WMI_BL_REASON_BTM_MBO_RETRY:
		return REASON_BTM_MBO_RETRY;
	case WMI_BL_REASON_REASSOC_RSSI_REJECT:
		return REASON_REASSOC_RSSI_REJECT;
	case WMI_BL_REASON_REASSOC_NO_MORE_STAS:
		return REASON_REASSOC_NO_MORE_STAS;
	default:
		return REASON_UNKNOWN;
	}
}

int wma_handle_btm_blacklist_event(void *handle, uint8_t *cmd_param_info,
				   uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_ROAM_BLACKLIST_EVENTID_param_tlvs *param_buf;
	wmi_roam_blacklist_event_fixed_param *resp_event;
	wmi_roam_blacklist_with_timeout_tlv_param *src_list;
	struct roam_blacklist_event *dst_list;
	struct roam_blacklist_timeout *roam_blacklist;
	uint32_t num_entries, i;

	param_buf = (WMI_ROAM_BLACKLIST_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid event buffer");
		return -EINVAL;
	}

	resp_event = param_buf->fixed_param;
	if (!resp_event) {
		wma_err("received null event data from target");
		return -EINVAL;
	}

	if (resp_event->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", resp_event->vdev_id);
		return -EINVAL;
	}

	num_entries = param_buf->num_blacklist_with_timeout;
	if (num_entries == 0) {
		/* no aps to blacklist just return*/
		wma_err("No APs in blacklist received");
		return 0;
	}

	if (num_entries > MAX_RSSI_AVOID_BSSID_LIST) {
		wma_err("num blacklist entries:%d exceeds maximum value",
			num_entries);
		return -EINVAL;
	}

	src_list = param_buf->blacklist_with_timeout;
	if (len < (sizeof(*resp_event) + (num_entries * sizeof(*src_list)))) {
		wma_err("Invalid length:%d", len);
		return -EINVAL;
	}

	dst_list = qdf_mem_malloc(sizeof(struct roam_blacklist_event) +
				 (sizeof(struct roam_blacklist_timeout) *
				 num_entries));
	if (!dst_list)
		return -ENOMEM;

	roam_blacklist = &dst_list->roam_blacklist[0];
	for (i = 0; i < num_entries; i++) {
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_list->bssid,
					   roam_blacklist->bssid.bytes);
		roam_blacklist->timeout = src_list->timeout;
		roam_blacklist->received_time = src_list->timestamp;
		roam_blacklist->original_timeout = src_list->original_timeout;
		roam_blacklist->reject_reason =
				wma_get_reject_reason(src_list->reason);
		roam_blacklist->source = src_list->source;
		roam_blacklist++;
		src_list++;
	}

	dst_list->num_entries = num_entries;
	wma_send_msg(wma, WMA_ROAM_BLACKLIST_MSG, (void *)dst_list, 0);
	return 0;
}

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_FIPS)
void wma_register_pmkid_req_event_handler(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		wma_err("pmkid req wma_handle is NULL");
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_pmkid_request_event_id,
					   wma_roam_pmkid_request_event_handler,
					   WMA_RX_SERIALIZER_CTX);
}

int wma_roam_pmkid_request_event_handler(void *handle, uint8_t *event,
					 uint32_t len)
{
	WMI_ROAM_PMKID_REQUEST_EVENTID_param_tlvs *param_buf;
	wmi_roam_pmkid_request_event_fixed_param *roam_pmkid_req_ev;
	wmi_roam_pmkid_request_tlv_param *src_list;
	tp_wma_handle wma = (tp_wma_handle)handle;
	struct roam_pmkid_req_event *dst_list;
	struct qdf_mac_addr *roam_bsslist;
	uint32_t num_entries, i;
	QDF_STATUS status;

	if (!event) {
		wma_err("received null event from target");
		return -EINVAL;
	}

	param_buf = (WMI_ROAM_PMKID_REQUEST_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_err("received null buf from target");
		return -EINVAL;
	}

	roam_pmkid_req_ev = param_buf->fixed_param;
	if (!roam_pmkid_req_ev) {
		wma_err("received null event data from target");
		return -EINVAL;
	}

	if (roam_pmkid_req_ev->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", roam_pmkid_req_ev->vdev_id);
		return -EINVAL;
	}

	num_entries = param_buf->num_pmkid_request;
	if (num_entries > MAX_RSSI_AVOID_BSSID_LIST) {
		wma_err("num bssid entries:%d exceeds maximum value",
			num_entries);
		return -EINVAL;
	}

	src_list = param_buf->pmkid_request;
	if (len < (sizeof(*roam_pmkid_req_ev) +
		(num_entries * sizeof(*src_list)))) {
		wma_err("Invalid length: %d", len);
		return -EINVAL;
	}

	dst_list = qdf_mem_malloc(sizeof(struct roam_pmkid_req_event) +
				 (sizeof(struct qdf_mac_addr) * num_entries));
	if (!dst_list)
		return -ENOMEM;

	for (i = 0; i < num_entries; i++) {
		roam_bsslist = &dst_list->ap_bssid[i];
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_list->bssid,
					   roam_bsslist->bytes);
		if (qdf_is_macaddr_zero(roam_bsslist) ||
		    qdf_is_macaddr_broadcast(roam_bsslist) ||
		    qdf_is_macaddr_group(roam_bsslist)) {
			wma_err("Invalid bssid");
			qdf_mem_free(dst_list);
			return -EINVAL;
		}
		wma_debug("Received pmkid fallback for bssid: "QDF_MAC_ADDR_FMT" vdev_id:%d",
			  QDF_MAC_ADDR_REF(roam_bsslist->bytes),
			 roam_pmkid_req_ev->vdev_id);
		src_list++;
	}
	dst_list->num_entries = num_entries;

	status = wma->csr_roam_pmkid_req_cb(roam_pmkid_req_ev->vdev_id,
					    dst_list);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Pmkid request failed");
		qdf_mem_free(dst_list);
		return -EINVAL;
	}

	qdf_mem_free(dst_list);
	return 0;
}
#endif
