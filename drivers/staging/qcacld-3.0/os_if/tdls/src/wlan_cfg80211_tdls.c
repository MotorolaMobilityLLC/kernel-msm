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
 * DOC: defines driver functions interfacing with linux kernel
 */

#include <qdf_list.h>
#include <qdf_status.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <net/cfg80211.h>
#include <wlan_cfg80211.h>
#include <wlan_cfg80211_tdls.h>
#include <wlan_osif_priv.h>
#include <wlan_tdls_public_structs.h>
#include <wlan_tdls_ucfg_api.h>
#include <qdf_mem.h>
#include <wlan_utility.h>
#include <wlan_reg_services_api.h>
#include "wlan_cfg80211_mc_cp_stats.h"
#include "sir_api.h"
#include "wlan_tdls_ucfg_api.h"

#define TDLS_MAX_NO_OF_2_4_CHANNELS 14

static int wlan_cfg80211_tdls_validate_mac_addr(const uint8_t *mac)
{
	static const uint8_t temp_mac[QDF_MAC_ADDR_SIZE] = {0};

	if (!qdf_mem_cmp(mac, temp_mac, QDF_MAC_ADDR_SIZE)) {
		osif_debug("Invalid Mac address " QDF_MAC_ADDR_FMT
			   " cmd declined.",
			   QDF_MAC_ADDR_REF(mac));
		return -EINVAL;
	}

	return 0;
}

QDF_STATUS wlan_cfg80211_tdls_osif_priv_init(struct wlan_objmgr_vdev *vdev)
{
	struct osif_tdls_vdev *tdls_priv;
	struct vdev_osif_priv *osif_priv;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is NULL!");
		return QDF_STATUS_E_FAULT;
	}

	osif_debug("initialize tdls os if layer private structure");
	tdls_priv = qdf_mem_malloc(sizeof(*tdls_priv));
	if (!tdls_priv)
		return QDF_STATUS_E_NOMEM;

	init_completion(&tdls_priv->tdls_add_peer_comp);
	init_completion(&tdls_priv->tdls_del_peer_comp);
	init_completion(&tdls_priv->tdls_mgmt_comp);
	init_completion(&tdls_priv->tdls_link_establish_req_comp);
	init_completion(&tdls_priv->tdls_teardown_comp);
	init_completion(&tdls_priv->tdls_user_cmd_comp);
	init_completion(&tdls_priv->tdls_antenna_switch_comp);

	osif_priv->osif_tdls = tdls_priv;

	return QDF_STATUS_SUCCESS;
}

void wlan_cfg80211_tdls_osif_priv_deinit(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is NULL!");
		return;
	}

	osif_debug("deinitialize tdls os if layer private structure");
	if (osif_priv->osif_tdls)
		qdf_mem_free(osif_priv->osif_tdls);
	osif_priv->osif_tdls = NULL;
}

void hdd_notify_tdls_reset_adapter(struct wlan_objmgr_vdev *vdev)
{
	ucfg_tdls_notify_reset_adapter(vdev);
}

void
hdd_notify_sta_connect(uint8_t session_id,
		       bool tdls_chan_swit_prohibited,
		       bool tdls_prohibited,
		       struct wlan_objmgr_vdev *vdev)
{
	struct tdls_sta_notify_params notify_info = {0};
	QDF_STATUS status;

	if (!vdev) {
		osif_err("vdev is NULL");
		return;
	}
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("can't get vdev");
		return;
	}

	notify_info.session_id = session_id;
	notify_info.vdev = vdev;
	notify_info.tdls_chan_swit_prohibited = tdls_chan_swit_prohibited;
	notify_info.tdls_prohibited = tdls_prohibited;
	ucfg_tdls_notify_sta_connect(&notify_info);
}

void hdd_notify_sta_disconnect(uint8_t session_id,
			       bool lfr_roam,
			       bool user_disconnect,
			       struct wlan_objmgr_vdev *vdev)
{
	struct tdls_sta_notify_params notify_info = {0};
	QDF_STATUS status;

	if (!vdev) {
		osif_err("vdev is NULL");
		return;
	}

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("can't get vdev");
		return;
	}

	notify_info.session_id = session_id;
	notify_info.lfr_roam = lfr_roam;
	notify_info.tdls_chan_swit_prohibited = false;
	notify_info.tdls_prohibited = false;
	notify_info.vdev = vdev;
	notify_info.user_disconnect = user_disconnect;
	ucfg_tdls_notify_sta_disconnect(&notify_info);
}

int wlan_cfg80211_tdls_add_peer(struct wlan_objmgr_vdev *vdev,
				const uint8_t *mac)
{
	struct tdls_add_peer_params *add_peer_req;
	int status;
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	unsigned long rc;

	status = wlan_cfg80211_tdls_validate_mac_addr(mac);

	if (status)
		return status;

	osif_debug("Add TDLS peer " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(mac));

	add_peer_req = qdf_mem_malloc(sizeof(*add_peer_req));
	if (!add_peer_req)
		return -EINVAL;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv || !osif_priv->osif_tdls) {
		osif_err("osif_tdls_vdev or osif_priv is NULL for the current vdev");
		status = -EINVAL;
		goto error;
	}
	tdls_priv = osif_priv->osif_tdls;
	add_peer_req->vdev_id = wlan_vdev_get_id(vdev);

	qdf_mem_copy(add_peer_req->peer_addr, mac, QDF_MAC_ADDR_SIZE);

	reinit_completion(&tdls_priv->tdls_add_peer_comp);
	status = ucfg_tdls_add_peer(vdev, add_peer_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("ucfg_tdls_add_peer returned err %d", status);
		status = -EIO;
		goto error;
	}

	rc = wait_for_completion_timeout(
	    &tdls_priv->tdls_add_peer_comp,
	    msecs_to_jiffies(WAIT_TIME_TDLS_ADD_STA));
	if (!rc) {
		osif_err("timeout for tdls add peer indication %ld", rc);
		status = -EPERM;
		goto error;
	}

	if (QDF_IS_STATUS_ERROR(tdls_priv->tdls_add_peer_status)) {
		osif_err("tdls add peer failed, status:%d",
			 tdls_priv->tdls_add_peer_status);
		status = -EPERM;
	}
error:
	qdf_mem_free(add_peer_req);
	return status;
}

static bool
is_duplicate_channel(uint8_t *arr, int index, uint8_t match)
{
	int i;

	for (i = 0; i < index; i++) {
		if (arr[i] == match)
			return true;
	}
	return false;
}

static void
tdls_calc_channels_from_staparams(struct tdls_update_peer_params *req_info,
				  struct station_parameters *params)
{
	int i = 0, j = 0, k = 0, no_of_channels = 0;
	int num_unique_channels;
	int next;
	uint8_t *dest_chans;
	const uint8_t *src_chans;

	dest_chans = req_info->supported_channels;
	src_chans = params->supported_channels;

	/* Convert (first channel , number of channels) tuple to
	 * the total list of channels. This goes with the assumption
	 * that if the first channel is < 14, then the next channels
	 * are an incremental of 1 else an incremental of 4 till the number
	 * of channels.
	 */
	for (i = 0; i < params->supported_channels_len &&
	     j < WLAN_MAC_MAX_SUPP_CHANNELS; i += 2) {
		int wifi_chan_index;

		if (!is_duplicate_channel(dest_chans, j, src_chans[i]))
			dest_chans[j] = src_chans[i];
		else
			continue;

		wifi_chan_index = ((dest_chans[j] <= WLAN_CHANNEL_14) ? 1 : 4);
		no_of_channels = src_chans[i + 1];

		osif_debug("i:%d,j:%d,k:%d,[%d]:%d,index:%d,chans_num: %d",
			   i, j, k, j,
			   dest_chans[j],
			   wifi_chan_index,
			   no_of_channels);

		for (k = 1; k <= no_of_channels &&
		     j < WLAN_MAC_MAX_SUPP_CHANNELS - 1; k++) {
			next = dest_chans[j] + wifi_chan_index;

			if (!is_duplicate_channel(dest_chans, j + 1, next))
				dest_chans[j + 1] = next;
			else
				continue;

			osif_debug("i: %d, j: %d, k: %d, [%d]: %d",
				   i, j, k, j + 1, dest_chans[j + 1]);
			j += 1;
		}
	}
	num_unique_channels = j + 1;
	osif_debug("Unique Channel List: supported_channels ");
	for (i = 0; i < num_unique_channels; i++)
		osif_debug("[%d]: %d,", i, dest_chans[i]);

	if (num_unique_channels > NUM_CHANNELS)
		num_unique_channels = NUM_CHANNELS;
	req_info->supported_channels_len = num_unique_channels;
	osif_debug("After removing duplcates supported_channels_len: %d",
		   req_info->supported_channels_len);
}

#ifdef WLAN_FEATURE_11AX
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void
wlan_cfg80211_tdls_extract_6ghz_params(struct tdls_update_peer_params *req_info,
				       struct station_parameters *params)
{
	if (!params->he_6ghz_capa) {
		osif_debug("6 Ghz he_capa not present");
		return;
	}

	qdf_mem_copy(&req_info->he_6ghz_cap, params->he_6ghz_capa,
		     sizeof(params->he_6ghz_capa));
}
#else
static void
wlan_cfg80211_tdls_extract_6ghz_params(struct tdls_update_peer_params *req_info,
				       struct station_parameters *params)
{
	osif_debug("kernel don't support tdls 6 ghz band");
}
#endif

static void
wlan_cfg80211_tdls_extract_he_params(struct tdls_update_peer_params *req_info,
				     struct station_parameters *params)
{
	if (params->he_capa_len < MIN_TDLS_HE_CAP_LEN) {
		osif_debug("he_capa_len %d less than MIN_TDLS_HE_CAP_LEN",
			   params->he_capa_len);
		return;
	}

	if (!params->he_capa) {
		osif_debug("he_capa not present");
		return;
	}

	req_info->he_cap_len = params->he_capa_len;
	if (req_info->he_cap_len > MAX_TDLS_HE_CAP_LEN)
		req_info->he_cap_len = MAX_TDLS_HE_CAP_LEN;

	qdf_mem_copy(&req_info->he_cap, params->he_capa,
		     req_info->he_cap_len);

	wlan_cfg80211_tdls_extract_6ghz_params(req_info, params);

	return;
}

#else
static void
wlan_cfg80211_tdls_extract_he_params(struct tdls_update_peer_params *req_info,
				     struct station_parameters *params)
{
}
#endif

static void
wlan_cfg80211_tdls_extract_params(struct tdls_update_peer_params *req_info,
				  struct station_parameters *params,
				  bool tdls_11ax_support)
{
	int i;

	osif_debug("sta cap %d, uapsd_queue %d, max_sp %d",
		   params->capability,
		   params->uapsd_queues, params->max_sp);

	if (!req_info) {
		osif_err("reg_info is NULL");
		return;
	}
	req_info->capability = params->capability;
	req_info->uapsd_queues = params->uapsd_queues;
	req_info->max_sp = params->max_sp;

	if (params->supported_channels_len)
		tdls_calc_channels_from_staparams(req_info, params);

	if (params->supported_oper_classes_len > WLAN_MAX_SUPP_OPER_CLASSES) {
		osif_debug("received oper classes:%d, resetting it to max supported: %d",
			   params->supported_oper_classes_len,
			   WLAN_MAX_SUPP_OPER_CLASSES);
		params->supported_oper_classes_len = WLAN_MAX_SUPP_OPER_CLASSES;
	}

	qdf_mem_copy(req_info->supported_oper_classes,
		     params->supported_oper_classes,
		     params->supported_oper_classes_len);
	req_info->supported_oper_classes_len =
		params->supported_oper_classes_len;

	if (params->ext_capab_len)
		qdf_mem_copy(req_info->extn_capability, params->ext_capab,
			     sizeof(req_info->extn_capability));

	if (params->ht_capa) {
		req_info->htcap_present = 1;
		qdf_mem_copy(&req_info->ht_cap, params->ht_capa,
			     sizeof(struct htcap_cmn_ie));
	}

	req_info->supported_rates_len = params->supported_rates_len;

	/* Note : The Maximum sizeof supported_rates sent by the Supplicant is
	 * 32. The supported_rates array , for all the structures propogating
	 * till Add Sta to the firmware has to be modified , if the supplicant
	 * (ieee80211) is modified to send more rates.
	 */

	/* To avoid Data Currption , set to max length to SIR_MAC_MAX_SUPP_RATES
	 */
	if (req_info->supported_rates_len > WLAN_MAC_MAX_SUPP_RATES)
		req_info->supported_rates_len = WLAN_MAC_MAX_SUPP_RATES;

	if (req_info->supported_rates_len) {
		qdf_mem_copy(req_info->supported_rates,
			     params->supported_rates,
			     req_info->supported_rates_len);
		osif_debug("Supported Rates with Length %d",
			   req_info->supported_rates_len);

		for (i = 0; i < req_info->supported_rates_len; i++)
			osif_debug("[%d]: %0x", i,
				   req_info->supported_rates[i]);
	}

	if (params->vht_capa) {
		req_info->vhtcap_present = 1;
		qdf_mem_copy(&req_info->vht_cap, params->vht_capa,
			     sizeof(struct vhtcap));
	}

	if (params->ht_capa || params->vht_capa ||
	    (params->sta_flags_set & BIT(NL80211_STA_FLAG_WME)))
		req_info->is_qos_wmm_sta = true;
	if (params->sta_flags_set & BIT(NL80211_STA_FLAG_MFP)) {
		osif_debug("TDLS peer pmf capable");
		req_info->is_pmf = 1;
	}

	if (tdls_11ax_support)
		wlan_cfg80211_tdls_extract_he_params(req_info, params);
	else
		osif_debug("tdls ax disabled");
}

int wlan_cfg80211_tdls_update_peer(struct wlan_objmgr_vdev *vdev,
				   const uint8_t *mac,
				   struct station_parameters *params)
{
	struct tdls_update_peer_params *req_info;
	int status;
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	unsigned long rc;
	struct wlan_objmgr_psoc *psoc;
	bool tdls_11ax_support = false;

	status = wlan_cfg80211_tdls_validate_mac_addr(mac);

	if (status)
		return status;

	osif_debug("Update TDLS peer " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(mac));

	req_info = qdf_mem_malloc(sizeof(*req_info));
	if (!req_info)
		return -EINVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("Invalid psoc");
		return -EINVAL;
	}

	tdls_11ax_support = ucfg_tdls_is_fw_11ax_capable(psoc);
	wlan_cfg80211_tdls_extract_params(req_info, params, tdls_11ax_support);

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv || !osif_priv->osif_tdls) {
		osif_err("osif priv or tdls priv is NULL");
		status = -EINVAL;
		goto error;
	}
	tdls_priv = osif_priv->osif_tdls;
	req_info->vdev_id = wlan_vdev_get_id(vdev);
	qdf_mem_copy(req_info->peer_addr, mac, QDF_MAC_ADDR_SIZE);

	reinit_completion(&tdls_priv->tdls_add_peer_comp);
	status = ucfg_tdls_update_peer(vdev, req_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("ucfg_tdls_update_peer returned err %d", status);
		status = -EIO;
		goto error;
	}

	rc = wait_for_completion_timeout(
		&tdls_priv->tdls_add_peer_comp,
		msecs_to_jiffies(WAIT_TIME_TDLS_ADD_STA));
	if (!rc) {
		osif_err("timeout for tdls update peer indication %ld", rc);
		status = -EPERM;
		goto error;
	}

	if (QDF_IS_STATUS_ERROR(tdls_priv->tdls_add_peer_status)) {
		osif_err("tdls update peer failed, status:%d",
			 tdls_priv->tdls_add_peer_status);
		status = -EPERM;
	}
error:
	qdf_mem_free(req_info);
	return status;
}

static char *tdls_oper_to_str(enum nl80211_tdls_operation oper)
{
	switch (oper) {
	case NL80211_TDLS_ENABLE_LINK:
		return "TDLS_ENABLE_LINK";
	case NL80211_TDLS_DISABLE_LINK:
		return "TDLS_DISABLE_LINK";
	case NL80211_TDLS_TEARDOWN:
		return "TDLS_TEARDOWN";
	case NL80211_TDLS_SETUP:
		return "TDLS_SETUP";
	default:
		return "UNKNOWN:ERR";
	}
}

static enum tdls_command_type tdls_oper_to_cmd(enum nl80211_tdls_operation oper)
{
	if (oper == NL80211_TDLS_ENABLE_LINK)
		return TDLS_CMD_ENABLE_LINK;
	else if (oper == NL80211_TDLS_DISABLE_LINK)
		return TDLS_CMD_DISABLE_LINK;
	else if (oper == NL80211_TDLS_TEARDOWN)
		return TDLS_CMD_REMOVE_FORCE_PEER;
	else if (oper == NL80211_TDLS_SETUP)
		return TDLS_CMD_CONFIG_FORCE_PEER;
	else
		return 0;
}

int wlan_cfg80211_tdls_configure_mode(struct wlan_objmgr_vdev *vdev,
						uint32_t trigger_mode)
{
	enum tdls_feature_mode tdls_mode;
	struct tdls_set_mode_params set_mode_params;
	int status;

	if (!vdev)
		return -EINVAL;

	switch (trigger_mode) {
	case WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT:
		tdls_mode = TDLS_SUPPORT_EXP_TRIG_ONLY;
		return 0;
	case WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL:
		tdls_mode = TDLS_SUPPORT_EXT_CONTROL;
		break;
	case WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT:
		tdls_mode = TDLS_SUPPORT_IMP_MODE;
		return 0;
	default:
		osif_err("Invalid TDLS trigger mode");
		return -EINVAL;
	}

	osif_notice("cfg80211 tdls trigger mode %d", trigger_mode);
	set_mode_params.source = TDLS_SET_MODE_SOURCE_USER;
	set_mode_params.tdls_mode = tdls_mode;
	set_mode_params.update_last = false;
	set_mode_params.vdev = vdev;

	status = ucfg_tdls_set_operating_mode(&set_mode_params);
	return status;
}

int wlan_cfg80211_tdls_oper(struct wlan_objmgr_vdev *vdev,
			    const uint8_t *peer,
			    enum nl80211_tdls_operation oper)
{
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	int status;
	unsigned long rc;
	enum tdls_command_type cmd;

	status = wlan_cfg80211_tdls_validate_mac_addr(peer);

	if (status)
		return status;

	if (NL80211_TDLS_DISCOVERY_REQ == oper) {
		osif_warn(
			"We don't support in-driver setup/teardown/discovery");
		return -ENOTSUPP;
	}

	osif_debug("%s start", tdls_oper_to_str(oper));
	cmd = tdls_oper_to_cmd(oper);
	switch (oper) {
	case NL80211_TDLS_ENABLE_LINK:
	case NL80211_TDLS_TEARDOWN:
	case NL80211_TDLS_SETUP:
		status = ucfg_tdls_oper(vdev, peer, cmd);
		if (QDF_IS_STATUS_ERROR(status)) {
			osif_err("%s fail %d",
				 tdls_oper_to_str(oper), status);
			status = -EIO;
			goto error;
		}
		break;
	case NL80211_TDLS_DISABLE_LINK:
		osif_priv = wlan_vdev_get_ospriv(vdev);

		if (!osif_priv || !osif_priv->osif_tdls) {
			osif_err("osif priv or tdls priv is NULL");
			status = -EINVAL;
			goto error;
		}
		tdls_priv = osif_priv->osif_tdls;
		reinit_completion(&tdls_priv->tdls_del_peer_comp);
		status = ucfg_tdls_oper(vdev, peer, cmd);
		if (QDF_IS_STATUS_ERROR(status)) {
			osif_err("ucfg_tdls_disable_link fail %d", status);
			status = -EIO;
			goto error;
		}

		rc = wait_for_completion_timeout(
			&tdls_priv->tdls_del_peer_comp,
			msecs_to_jiffies(WAIT_TIME_TDLS_DEL_STA));
		if (!rc) {
			osif_err("timeout for tdls disable link %ld", rc);
			status = -EPERM;
		}
		break;
	default:
		osif_err("unsupported event %d", oper);
		status = -ENOTSUPP;
	}

error:
	return status;
}

void wlan_cfg80211_tdls_rx_callback(void *user_data,
	struct tdls_rx_mgmt_frame *rx_frame)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		rx_frame->vdev_id, WLAN_TDLS_NB_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wdev is null");
		goto fail;
	}

	osif_notice("Indicate frame over nl80211, vdev id:%d, idx:%d",
		    rx_frame->vdev_id, wdev->netdev->ifindex);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED, GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE */
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
}

static void wlan_cfg80211_update_tdls_peers_rssi(struct wlan_objmgr_vdev *vdev)
{
	int ret = 0, i;
	struct stats_event *rssi_info;
	struct qdf_mac_addr bcast_mac = QDF_MAC_ADDR_BCAST_INIT;

	rssi_info = wlan_cfg80211_mc_cp_stats_get_peer_rssi(
			vdev, bcast_mac.bytes,
			&ret);
	if (ret || !rssi_info) {
		osif_err("get peer rssi fail");
		wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
		return;
	}

	for (i = 0; i < rssi_info->num_peer_stats; i++)
		ucfg_tdls_set_rssi(vdev, rssi_info->peer_stats[i].peer_macaddr,
				   rssi_info->peer_stats[i].peer_rssi);

	wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
}

int wlan_cfg80211_tdls_get_all_peers(struct wlan_objmgr_vdev *vdev,
				char *buf, int buflen)
{
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	int32_t len;
	QDF_STATUS status;
	unsigned long rc;

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv || !osif_priv->osif_tdls) {
		osif_err("osif_tdls_vdev or osif_priv is NULL for the current vdev");
		return -EINVAL;
	}

	tdls_priv = osif_priv->osif_tdls;

	wlan_cfg80211_update_tdls_peers_rssi(vdev);

	reinit_completion(&tdls_priv->tdls_user_cmd_comp);
	status = ucfg_tdls_get_all_peers(vdev, buf, buflen);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("ucfg_tdls_get_all_peers failed err %d", status);
		len = scnprintf(buf, buflen,
				"\nucfg_tdls_send_mgmt failed\n");
		goto error_get_tdls_peers;
	}

	osif_debug("Wait for tdls_user_cmd_comp. Timeout %u ms",
		   WAIT_TIME_FOR_TDLS_USER_CMD);

	rc = wait_for_completion_timeout(
		&tdls_priv->tdls_user_cmd_comp,
		msecs_to_jiffies(WAIT_TIME_FOR_TDLS_USER_CMD));

	if (0 == rc) {
		osif_err("TDLS user cmd get all peers timed out rc %ld",
			 rc);
		len = scnprintf(buf, buflen,
				"\nTDLS user cmd get all peers timed out\n");
		goto error_get_tdls_peers;
	}

	len = tdls_priv->tdls_user_cmd_len;

error_get_tdls_peers:
	return len;
}

int wlan_cfg80211_tdls_mgmt(struct wlan_objmgr_vdev *vdev,
			    const uint8_t *peer_mac,
			    uint8_t action_code, uint8_t dialog_token,
			    uint16_t status_code, uint32_t peer_capability,
			    const uint8_t *buf, size_t len)
{
	struct tdls_action_frame_request mgmt_req;
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	int status;
	unsigned long rc;
	struct tdls_set_responder_req set_responder;

	status = wlan_cfg80211_tdls_validate_mac_addr(peer_mac);

	if (status)
		return status;

	osif_priv = wlan_vdev_get_ospriv(vdev);

	if (!osif_priv || !osif_priv->osif_tdls) {
		osif_err("osif priv or tdls priv is NULL");
		return -EINVAL;
	}

	tdls_priv = osif_priv->osif_tdls;

	/* make sure doesn't call send_mgmt() while it is pending */
	if (TDLS_VDEV_MAGIC == tdls_priv->mgmt_tx_completion_status) {
		osif_err(QDF_MAC_ADDR_FMT " action %d couldn't sent, as one is pending. return EBUSY",
			 QDF_MAC_ADDR_REF(peer_mac), action_code);
		return -EBUSY;
	}

	/* Reset TDLS VDEV magic */
	tdls_priv->mgmt_tx_completion_status = TDLS_VDEV_MAGIC;


	/*prepare the request */

	/* Validate the management Request */
	mgmt_req.chk_frame.action_code = action_code;
	qdf_mem_copy(mgmt_req.chk_frame.peer_mac, peer_mac, QDF_MAC_ADDR_SIZE);
	mgmt_req.chk_frame.dialog_token = dialog_token;
	mgmt_req.chk_frame.action_code = action_code;
	mgmt_req.chk_frame.status_code = status_code;
	mgmt_req.chk_frame.len = len;

	mgmt_req.vdev = vdev;
	mgmt_req.vdev_id = wlan_vdev_get_id(vdev);
	mgmt_req.session_id = mgmt_req.vdev_id;
	/* populate management req params */
	qdf_mem_copy(mgmt_req.tdls_mgmt.peer_mac.bytes,
		     peer_mac, QDF_MAC_ADDR_SIZE);
	mgmt_req.tdls_mgmt.dialog = dialog_token;
	mgmt_req.tdls_mgmt.frame_type = action_code;
	mgmt_req.tdls_mgmt.len = len;
	mgmt_req.tdls_mgmt.peer_capability = peer_capability;
	mgmt_req.tdls_mgmt.status_code = mgmt_req.chk_frame.status_code;

	/*populate the additional IE's */
	mgmt_req.cmd_buf = buf;
	mgmt_req.len = len;

	reinit_completion(&tdls_priv->tdls_mgmt_comp);
	status = ucfg_tdls_send_mgmt_frame(&mgmt_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("ucfg_tdls_send_mgmt failed err %d", status);
		status = -EIO;
		tdls_priv->mgmt_tx_completion_status = false;
		goto error_mgmt_req;
	}

	osif_debug("Wait for tdls_mgmt_comp. Timeout %u ms",
		   WAIT_TIME_FOR_TDLS_MGMT);

	rc = wait_for_completion_timeout(
		&tdls_priv->tdls_mgmt_comp,
		msecs_to_jiffies(WAIT_TIME_FOR_TDLS_MGMT));

	if ((0 == rc) || (QDF_STATUS_SUCCESS !=
				tdls_priv->mgmt_tx_completion_status)) {
		osif_err("%s rc %ld mgmtTxCompletionStatus %u",
			 !rc ? "Mgmt Tx Completion timed out" :
			 "Mgmt Tx Completion failed",
			 rc, tdls_priv->mgmt_tx_completion_status);

		tdls_priv->mgmt_tx_completion_status = false;
		status = -EINVAL;
		goto error_mgmt_req;
	}

	osif_debug("Mgmt Tx Completion status %ld TxCompletion %u",
		   rc, tdls_priv->mgmt_tx_completion_status);

	if (TDLS_SETUP_RESPONSE == action_code ||
	    TDLS_SETUP_CONFIRM == action_code) {
		qdf_mem_copy(set_responder.peer_mac, peer_mac,
			     QDF_MAC_ADDR_SIZE);
		set_responder.vdev = vdev;
		if (TDLS_SETUP_RESPONSE == action_code)
			set_responder.responder = false;
		if (TDLS_SETUP_CONFIRM == action_code)
			set_responder.responder = true;
		ucfg_tdls_responder(&set_responder);
	}

error_mgmt_req:
	return status;
}

int wlan_tdls_antenna_switch(struct wlan_objmgr_vdev *vdev, uint32_t mode)
{
	struct vdev_osif_priv *osif_priv;
	struct osif_tdls_vdev *tdls_priv;
	int ret;
	unsigned long rc;

	if (!vdev) {
		osif_err("vdev is NULL");
		return -EAGAIN;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv || !osif_priv->osif_tdls) {
		osif_err("osif priv or tdls priv is NULL");
		ret = -EINVAL;
		goto error;
	}
	tdls_priv = osif_priv->osif_tdls;

	reinit_completion(&tdls_priv->tdls_antenna_switch_comp);
	ret = ucfg_tdls_antenna_switch(vdev, mode);
	if (QDF_IS_STATUS_ERROR(ret)) {
		osif_err("ucfg_tdls_antenna_switch failed err %d", ret);
		ret = -EAGAIN;
		goto error;
	}

	rc = wait_for_completion_timeout(
		&tdls_priv->tdls_antenna_switch_comp,
		msecs_to_jiffies(WAIT_TIME_FOR_TDLS_ANTENNA_SWITCH));
	if (!rc) {
		osif_err("timeout for tdls antenna switch %ld", rc);
		ret = -EAGAIN;
		goto error;
	}

	ret = tdls_priv->tdls_antenna_switch_status;
	osif_debug("tdls antenna switch status:%d", ret);
error:
	return ret;
}

static void
wlan_cfg80211_tdls_indicate_discovery(struct tdls_osif_indication *ind)
{
	struct vdev_osif_priv *osif_vdev;

	osif_vdev = wlan_vdev_get_ospriv(ind->vdev);

	cfg80211_tdls_oper_request(osif_vdev->wdev->netdev,
				   ind->peer_mac, NL80211_TDLS_DISCOVERY_REQ,
				   false, GFP_KERNEL);
}

static void
wlan_cfg80211_tdls_indicate_setup(struct tdls_osif_indication *ind)
{
	struct vdev_osif_priv *osif_vdev;

	osif_vdev = wlan_vdev_get_ospriv(ind->vdev);

	osif_debug("Indication to request TDLS setup");
	cfg80211_tdls_oper_request(osif_vdev->wdev->netdev,
				   ind->peer_mac, NL80211_TDLS_SETUP, false,
				   GFP_KERNEL);
}

static void
wlan_cfg80211_tdls_indicate_teardown(struct tdls_osif_indication *ind)
{
	struct vdev_osif_priv *osif_vdev;

	osif_vdev = wlan_vdev_get_ospriv(ind->vdev);

	osif_debug("Teardown reason %d", ind->reason);
	cfg80211_tdls_oper_request(osif_vdev->wdev->netdev,
				   ind->peer_mac, NL80211_TDLS_TEARDOWN,
				   ind->reason, GFP_KERNEL);
}

void wlan_cfg80211_tdls_event_callback(void *user_data,
				       enum tdls_event_type type,
				       struct tdls_osif_indication *ind)
{
	struct vdev_osif_priv *osif_vdev;
	struct osif_tdls_vdev *tdls_priv;

	if (!ind || !ind->vdev) {
		osif_err("ind: %pK", ind);
		return;
	}
	osif_vdev = wlan_vdev_get_ospriv(ind->vdev);

	if (!osif_vdev || !osif_vdev->osif_tdls) {
		osif_err("osif priv or tdls priv is NULL");
		return;
	}

	tdls_priv = osif_vdev->osif_tdls;

	switch (type) {
	case TDLS_EVENT_MGMT_TX_ACK_CNF:
		tdls_priv->mgmt_tx_completion_status = ind->status;
		complete(&tdls_priv->tdls_mgmt_comp);
		break;
	case TDLS_EVENT_ADD_PEER:
		tdls_priv->tdls_add_peer_status = ind->status;
		complete(&tdls_priv->tdls_add_peer_comp);
		break;
	case TDLS_EVENT_DEL_PEER:
		complete(&tdls_priv->tdls_del_peer_comp);
		break;
	case TDLS_EVENT_DISCOVERY_REQ:
		wlan_cfg80211_tdls_indicate_discovery(ind);
		break;
	case TDLS_EVENT_TEARDOWN_REQ:
		wlan_cfg80211_tdls_indicate_teardown(ind);
		break;
	case TDLS_EVENT_SETUP_REQ:
		wlan_cfg80211_tdls_indicate_setup(ind);
		break;
	case TDLS_EVENT_USER_CMD:
		tdls_priv->tdls_user_cmd_len = ind->status;
		complete(&tdls_priv->tdls_user_cmd_comp);
		break;

	case TDLS_EVENT_ANTENNA_SWITCH:
		tdls_priv->tdls_antenna_switch_status = ind->status;
		complete(&tdls_priv->tdls_antenna_switch_comp);
	default:
		break;
	}
}
