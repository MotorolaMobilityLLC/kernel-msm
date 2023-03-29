/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_hostapd_wext.c
 *
 * Linux Wireless Extensions Implementation
 */

#include "osif_sync.h"
#include <wlan_hdd_hostapd_wext.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include <wlan_hdd_green_ap.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_ioctl.h>
#include <wlan_hdd_stats.h>
#include "wlan_hdd_p2p.h"
#include "wma.h"
#ifdef WLAN_DEBUG
#include "wma_api.h"
#endif
#include "wlan_hdd_power.h"
#include "wlan_policy_mgr_ucfg.h"
#include <ol_defines.h>
#include <cdp_txrx_stats.h>
#include "wlan_dfs_utils_api.h"
#include <wlan_ipa_ucfg_api.h>
#include <wlan_cfg80211_mc_cp_stats.h>
#include "wlan_mlme_ucfg_api.h"
#include "wlan_reg_ucfg_api.h"
#include "wlan_hdd_sta_info.h"
#include "wlan_hdd_object_manager.h"

#define WE_WLAN_VERSION     1

/* WEXT limitation: MAX allowed buf len for any *
 * IW_PRIV_TYPE_CHAR is 2Kbytes *
 */
#define WE_SAP_MAX_STA_INFO 0x7FF

#define RC_2_RATE_IDX(_rc)        ((_rc) & 0x7)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define RC_2_RATE_IDX_11AC(_rc)        ((_rc) & 0xf)
#define HT_RC_2_STREAMS_11AC(_rc)    ((((_rc) & 0x30) >> 4) + 1)

static int hdd_sap_get_chan_width(struct hdd_adapter *adapter, int *value)
{
	struct sap_context *sap_ctx;
	struct hdd_hostapd_state *hostapdstate;

	hdd_enter();
	hostapdstate = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	if (hostapdstate->bss_state != BSS_START) {
		*value = -EINVAL;
		return -EINVAL;
	}

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);

	*value = wlansap_get_chan_width(sap_ctx);
	hdd_debug("chan_width = %d", *value);

	return 0;
}

int
static __iw_softap_get_ini_cfg(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	hdd_debug("Printing CLD global INI Config");
	hdd_cfg_get_global_config(hdd_ctx, extra, QCSAP_IOCTL_MAX_STR_LEN);
	wrqu->data.length = strlen(extra) + 1;

	return 0;
}

int
static iw_softap_get_ini_cfg(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_get_ini_cfg(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * iw_softap_set_two_ints_getnone() - Generic "set two integer" ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_softap_set_two_ints_getnone(struct net_device *dev,
					    struct iw_request_info *info,
					    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int ret;
	int *value = (int *)extra;
	int sub_cmd = value[0];
	struct hdd_context *hdd_ctx;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct cdp_txrx_stats_req req = {0};
	struct hdd_station_info *sta_info;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	if (qdf_unlikely(!soc)) {
		hdd_err("soc is NULL");
		return -EINVAL;
	}

	switch (sub_cmd) {
	case QCSAP_PARAM_SET_TXRX_STATS:
	{
		req.stats = value[1];
		req.mac_id = value[2];
		hdd_info("QCSAP_PARAM_SET_TXRX_STATS stats_id: %d mac_id: %d",
			req.stats, req.mac_id);

		if (value[1] == CDP_TXRX_STATS_28) {
			req.peer_addr = (char *)&adapter->mac_addr;
			ret = cdp_txrx_stats_request(soc, adapter->vdev_id,
						     &req);

			hdd_for_each_sta_ref(
					adapter->sta_info_list, sta_info,
					STA_INFO_SAP_SET_TWO_INTS_GETNONE) {
				hdd_debug("bss_id: " QDF_MAC_ADDR_FMT,
					  QDF_MAC_ADDR_REF(
					  sta_info->sta_mac.bytes));

				req.peer_addr = (char *)
					&sta_info->sta_mac;
				ret = cdp_txrx_stats_request(
					soc, adapter->vdev_id, &req);
				hdd_put_sta_info_ref(
					&adapter->sta_info_list, &sta_info,
					true,
					STA_INFO_SAP_SET_TWO_INTS_GETNONE);
			}
		} else {
			ret = cdp_txrx_stats_request(soc, adapter->vdev_id,
						     &req);
		}

		break;
	}

	/* Firmware debug log */
	case QCSAP_IOCTL_SET_FW_CRASH_INJECT:
		ret = hdd_crash_inject(adapter, value[1], value[2]);
		break;

	case QCSAP_IOCTL_DUMP_DP_TRACE_LEVEL:
		hdd_set_dump_dp_trace(value[1], value[2]);
		break;

	case QCSAP_ENABLE_FW_PROFILE:
		hdd_debug("QCSAP_ENABLE_FW_PROFILE: %d %d",
		       value[1], value[2]);
		ret = wma_cli_set2_command(adapter->vdev_id,
				 WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID,
					value[1], value[2], DBG_CMD);
		break;

	case QCSAP_SET_FW_PROFILE_HIST_INTVL:
		hdd_debug("QCSAP_SET_FW_PROFILE_HIST_INTVL: %d %d",
		       value[1], value[2]);
		ret = wma_cli_set2_command(adapter->vdev_id,
					WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID,
					value[1], value[2], DBG_CMD);
		break;

	case QCSAP_SET_WLAN_SUSPEND:
		hdd_info("SAP unit-test suspend(%d, %d)", value[1], value[2]);
		ret = hdd_wlan_fake_apps_suspend(hdd_ctx->wiphy, dev,
						 value[1], value[2]);
		break;

	case QCSAP_SET_WLAN_RESUME:
		ret = hdd_wlan_fake_apps_resume(hdd_ctx->wiphy, dev);
		break;

	case QCSAP_SET_BA_AGEING_TIMEOUT:
		hdd_info("QCSAP_SET_BA_AGEING_TIMEOUT: AC[%d] timeout[%d]",
			 value[1], value[2]);
		/*
		 *  value[1] : suppose to be access class, value between[0-3]
		 *  value[2]: suppose to be duration in seconds
		 */
		cdp_set_ba_timeout(soc, value[1], value[2]);
		break;

	default:
		hdd_err("Invalid IOCTL command: %d", sub_cmd);
		break;
	}

	return ret;
}

static int iw_softap_set_two_ints_getnone(struct net_device *dev,
					  struct iw_request_info *info,
					  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_set_two_ints_getnone(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static void print_mac_list(struct qdf_mac_addr *macList, uint8_t size)
{
	int i;
	uint8_t *macArray;

	for (i = 0; i < size; i++) {
		macArray = (macList + i)->bytes;
		pr_info("ACL entry %i - "QDF_MAC_ADDR_FMT"\n",
			i, QDF_MAC_ADDR_REF(macArray));
	}
}

static QDF_STATUS hdd_print_acl(struct hdd_adapter *adapter)
{
	eSapMacAddrACL acl_mode;
	struct qdf_mac_addr maclist[MAX_ACL_MAC_ADDRESS];
	uint8_t listnum;
	struct sap_context *sap_ctx;

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	qdf_mem_zero(&maclist[0], sizeof(maclist));
	if (QDF_STATUS_SUCCESS == wlansap_get_acl_mode(sap_ctx, &acl_mode)) {
		pr_info("******** ACL MODE *********\n");
		switch (acl_mode) {
		case eSAP_ACCEPT_UNLESS_DENIED:
			pr_info("ACL Mode = ACCEPT_UNLESS_DENIED\n");
			break;
		case eSAP_DENY_UNLESS_ACCEPTED:
			pr_info("ACL Mode = DENY_UNLESS_ACCEPTED\n");
			break;
		case eSAP_SUPPORT_ACCEPT_AND_DENY:
			pr_info("ACL Mode = ACCEPT_AND_DENY\n");
			break;
		case eSAP_ALLOW_ALL:
			pr_info("ACL Mode = ALLOW_ALL\n");
			break;
		default:
			pr_info("Invalid SAP ACL Mode = %d\n", acl_mode);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS == wlansap_get_acl_accept_list(sap_ctx,
							      &maclist[0],
							      &listnum)) {
		pr_info("******* WHITE LIST ***********\n");
		if (listnum <= MAX_ACL_MAC_ADDRESS)
			print_mac_list(&maclist[0], listnum);
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS == wlansap_get_acl_deny_list(sap_ctx,
							    &maclist[0],
							    &listnum)) {
		pr_info("******* BLACK LIST ***********\n");
		if (listnum <= MAX_ACL_MAC_ADDRESS)
			print_mac_list(&maclist[0], listnum);
	} else {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

int
static __iw_softap_setparam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	mac_handle_t mac_handle;
	//BEGIN MOT a19110 IKDREL3KK-11113 Fix iwpriv panic
	int *value;
	uint8_t *mot_value; //MOT a19110 IKLOCSEN-3014 Use copy from user
	int sub_cmd;
	int set_value;
	int *tmp = (int *) extra;
	//END IKDREL3KK-11113
	QDF_STATUS status;
	int ret = 0;
	struct hdd_context *hdd_ctx;
	bool bval = false;

	hdd_enter_dev(dev);
	//BEGIN MOT a19110 IKDREL3KK-11113 Fix iwpriv panic to 8998
	//BEGIN MOT a19110 IKLOCSEN-3014 use copy_from_user to avoid junk value
	if (tmp[0] < 0 || tmp[0] > QCSAP_SET_BTCOEX_LOW_RSSI_THRESHOLD) {
		mot_value = (uint8_t *)kmalloc(wrqu->data.length+1, GFP_KERNEL);
		if(copy_from_user((uint8_t *) mot_value, (uint8_t *)(wrqu->data.pointer), wrqu->data.length)) {
			hdd_err("%s -- copy_from_user --data pointer failed! bailing",
	                    __func__);
			kfree(mot_value);
			return -EFAULT;
		}
		sub_cmd = (int )(*(mot_value + 0));
		set_value = (int )(*(mot_value + 1));
		kfree(mot_value);
	} else {
		value = (int *)extra;
		sub_cmd = value[0];
		set_value = value[1];
	}
	//END IKLOCSEN-3014
	if (!adapter || !adapter->hdd_ctx) {
		hdd_err("Either hostpad adapter is null or Hal ctx is null");
		return -EINVAL;
	}
	//END IKDREL3KK-11113

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	mac_handle = hdd_ctx->mac_handle;
	if (!mac_handle) {
		hdd_err("mac handle is null");
		return -EINVAL;
	}

	switch (sub_cmd) {
	case QCASAP_SET_RADAR_DBG:
		hdd_debug("QCASAP_SET_RADAR_DBG called with: value: %x",
				set_value);
		wlan_sap_enable_phy_error_logs(mac_handle, set_value);
		break;

	case QCSAP_PARAM_CLR_ACL:
		if (QDF_STATUS_SUCCESS != wlansap_clear_acl(
		    WLAN_HDD_GET_SAP_CTX_PTR(adapter))) {
			ret = -EIO;
		}
		break;

	case QCSAP_PARAM_ACL_MODE:
		if ((eSAP_ALLOW_ALL < (eSapMacAddrACL) set_value) ||
		    (eSAP_ACCEPT_UNLESS_DENIED > (eSapMacAddrACL) set_value)) {
			hdd_err("Invalid ACL Mode value: %d", set_value);
			ret = -EINVAL;
		} else {
			wlansap_set_acl_mode(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter),
				set_value);
		}
		break;

	case QCSAP_PARAM_SET_CHANNEL_CHANGE:
		if ((QDF_SAP_MODE == adapter->device_mode) ||
		   (QDF_P2P_GO_MODE == adapter->device_mode)) {
			wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc,
						    adapter->vdev_id,
						    CSA_REASON_USER_INITIATED);
			hdd_debug("SET Channel Change to new channel= %d",
			       set_value);
			if (set_value <= wlan_reg_max_5ghz_ch_num())
				set_value = wlan_reg_legacy_chan_to_freq(
								hdd_ctx->pdev,
								set_value);

			ret = hdd_softap_set_channel_change(dev, set_value,
							    CH_WIDTH_MAX,
							    false);
		} else {
			hdd_err("Channel Change Failed, Device in test mode");
			ret = -EINVAL;
		}
		break;

	case QCSAP_PARAM_CONC_SYSTEM_PREF:
		hdd_debug("New preference: %d", set_value);
		ucfg_policy_mgr_set_sys_pref(hdd_ctx->psoc, set_value);
		break;

	case QCSAP_PARAM_MAX_ASSOC:
		if (WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value) {
			hdd_err("Invalid setMaxAssoc value %d",
			       set_value);
			ret = -EINVAL;
		} else {
			if (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value) {
				hdd_warn("setMaxAssoc %d > max allowed %d.",
				       set_value,
				       WNI_CFG_ASSOC_STA_LIMIT_STAMAX);
				hdd_warn("Setting it to max allowed and continuing");
				set_value = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
			}
			if (ucfg_mlme_set_assoc_sta_limit(hdd_ctx->psoc,
							  set_value) !=
			    QDF_STATUS_SUCCESS) {
				hdd_err("CFG_ASSOC_STA_LIMIT failed");
				ret = -EIO;
			}

		}
		break;

	case QCSAP_PARAM_HIDE_SSID:
	{
		QDF_STATUS status;

		/*
		 * Reject hidden ssid param update  if reassoc in progress on
		 * any adapter. sme_is_any_session_in_middle_of_roaming is for
		 * LFR2 and hdd_is_roaming_in_progress is for LFR3
		 */
		if (hdd_is_roaming_in_progress(hdd_ctx) ||
		    sme_is_any_session_in_middle_of_roaming(mac_handle)) {
			hdd_info("Reassociation in progress");
			return -EINVAL;
		}

		/*
		 * Disable Roaming on all adapters before start of
		 * start of Hidden ssid connection
		 */
		wlan_hdd_disable_roaming(adapter, RSO_START_BSS);

		status = sme_update_session_param(mac_handle,
				adapter->vdev_id,
				SIR_PARAM_SSID_HIDDEN, set_value);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("QCSAP_PARAM_HIDE_SSID failed");
			wlan_hdd_enable_roaming(adapter, RSO_START_BSS);
			return -EIO;
		}
		break;
	}
	case QCSAP_PARAM_SET_MC_RATE:
	{
		tSirRateUpdateInd rate_update = {0};

		hdd_debug("MC Target rate %d", set_value);
		qdf_copy_macaddr(&rate_update.bssid,
				 &adapter->mac_addr);
		status = ucfg_mlme_get_vht_enable2x2(hdd_ctx->psoc, &bval);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("unable to get vht_enable2x2");
			ret = -1;
		}
		rate_update.nss = (bval == 0) ? 0 : 1;

		rate_update.dev_mode = adapter->device_mode;
		rate_update.mcastDataRate24GHz = set_value;
		rate_update.mcastDataRate24GHzTxFlag = 1;
		rate_update.mcastDataRate5GHz = set_value;
		rate_update.bcastDataRate = -1;
		status = sme_send_rate_update_ind(mac_handle, &rate_update);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("SET_MC_RATE failed");
			ret = -1;
		}
		break;
	}

	case QCSAP_PARAM_SET_TXRX_FW_STATS:
	{
		hdd_debug("QCSAP_PARAM_SET_TXRX_FW_STATS val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID,
					  set_value, VDEV_CMD);
		break;
	}

	/* Firmware debug log */
	case QCSAP_DBGLOG_LOG_LEVEL:
	{
		hdd_debug("QCSAP_DBGLOG_LOG_LEVEL val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_LOG_LEVEL,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_VAP_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_VAP_ENABLE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_VAP_ENABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_VAP_DISABLE:
	{
		hdd_debug("QCSAP_DBGLOG_VAP_DISABLE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_VAP_DISABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MODULE_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_MODULE_ENABLE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_MODULE_ENABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MODULE_DISABLE:
	{
		hdd_debug("QCSAP_DBGLOG_MODULE_DISABLE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_MODULE_DISABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MOD_LOG_LEVEL:
	{
		hdd_debug("QCSAP_DBGLOG_MOD_LOG_LEVEL val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_MOD_LOG_LEVEL,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_TYPE:
	{
		hdd_debug("QCSAP_DBGLOG_TYPE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_TYPE,
					  set_value, DBG_CMD);
		break;
	}
	case QCSAP_DBGLOG_REPORT_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_REPORT_ENABLE val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_DBGLOG_REPORT_ENABLE,
					  set_value, DBG_CMD);
		break;
	}
	case QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY:
	{
		wlan_hdd_set_mcc_latency(adapter, set_value);
		break;
	}

	case QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA:
	{
		hdd_debug("iwpriv cmd to set MCC quota value %dms",
		       set_value);
		ret = wlan_hdd_go_set_mcc_p2p_quota(adapter,
						    set_value);
		break;
	}

	case QCASAP_TXRX_FWSTATS_RESET:
	{
		hdd_debug("WE_TXRX_FWSTATS_RESET val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
					  set_value, VDEV_CMD);
		break;
	}

	case QCSAP_PARAM_RTSCTS:
	{
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_ENABLE_RTSCTS,
					  set_value, VDEV_CMD);
		if (ret) {
			hdd_err("FAILED TO SET RTSCTS at SAP");
			ret = -EIO;
		}
		break;
	}
	case QCASAP_SET_11N_RATE:
	{
		uint8_t preamble = 0, nss = 0, rix = 0;
		struct sap_config *config =
			&adapter->session.ap.sap_config;

		hdd_debug("SET_HT_RATE val %d", set_value);

		if (set_value != 0xff) {
			rix = RC_2_RATE_IDX(set_value);
			if (set_value & 0x80) {
				if (config->SapHw_mode ==
				    eCSR_DOT11_MODE_11b
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_11b_ONLY
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_11g
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_11g_ONLY
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_abg
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_11a) {
					hdd_err("Not valid mode for HT");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_HT;
				nss = HT_RC_2_STREAMS(set_value) - 1;
			} else if (set_value & 0x10) {
				if (config->SapHw_mode ==
				    eCSR_DOT11_MODE_11a) {
					hdd_err("Not valid for cck");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_CCK;
				/* Enable Short preamble always
				 * for CCK except 1mbps
				 */
				if (rix != 0x3)
					rix |= 0x4;
			} else {
				if (config->SapHw_mode ==
				    eCSR_DOT11_MODE_11b
				    || config->SapHw_mode ==
				    eCSR_DOT11_MODE_11b_ONLY) {
					hdd_err("Not valid for OFDM");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_OFDM;
			}
			set_value = hdd_assemble_rate_code(preamble, nss, rix);
		}
		hdd_debug("SET_HT_RATE val %d rix %d preamble %x nss %d",
		       set_value, rix, preamble, nss);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_FIXED_RATE,
					  set_value, VDEV_CMD);
		break;
	}

	case QCASAP_SET_VHT_RATE:
	{
		uint8_t preamble = 0, nss = 0, rix = 0;
		struct sap_config *config =
			&adapter->session.ap.sap_config;

		if (config->SapHw_mode != eCSR_DOT11_MODE_11ac &&
		    config->SapHw_mode != eCSR_DOT11_MODE_11ac_ONLY) {
			hdd_err("SET_VHT_RATE: SapHw_mode= 0x%x, ch_freq: %d",
			       config->SapHw_mode, config->chan_freq);
			ret = -EIO;
			break;
		}

		if (set_value != 0xff) {
			rix = RC_2_RATE_IDX_11AC(set_value);
			preamble = WMI_RATE_PREAMBLE_VHT;
			nss = HT_RC_2_STREAMS_11AC(set_value) - 1;

			set_value = hdd_assemble_rate_code(preamble, nss, rix);
		}
		hdd_debug("SET_VHT_RATE val %d rix %d preamble %x nss %d",
		       set_value, rix, preamble, nss);

		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_FIXED_RATE,
					  set_value, VDEV_CMD);
		break;
	}

	case QCASAP_SHORT_GI:
	{
		hdd_debug("QCASAP_SET_SHORT_GI val %d", set_value);
		ret = hdd_we_set_short_gi(adapter, set_value);
		if (ret)
			hdd_err("Failed to set ShortGI value ret: %d", ret);
		break;
	}

	case QCSAP_SET_AMPDU:
	{
		hdd_debug("QCSAP_SET_AMPDU %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  GEN_VDEV_PARAM_AMPDU,
					  set_value, GEN_CMD);
		break;
	}

	case QCSAP_SET_AMSDU:
	{
		hdd_debug("QCSAP_SET_AMSDU %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  GEN_VDEV_PARAM_AMSDU,
					  set_value, GEN_CMD);
		break;
	}
	case QCSAP_GTX_HT_MCS:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_HT_MCS %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_HT_MCS,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_VHT_MCS:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_VHT_MCS %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_VHT_MCS,
						set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_USRCFG:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_USR_CFG %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_USR_CFG,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_THRE:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_THRE %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_THRE,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_MARGIN:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_MARGIN %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_MARGIN,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_STEP:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_STEP %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_STEP,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_MINTPC:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_MINTPC %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_MINTPC,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_BWMASK:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_BWMASK %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_GTX_BW_MASK,
					  set_value, GTX_CMD);
		break;
	}

	case QCASAP_SET_TM_LEVEL:
	{
		hdd_debug("Set Thermal Mitigation Level %d", set_value);
		(void)sme_set_thermal_level(mac_handle, set_value);
		break;
	}

	case QCASAP_SET_DFS_IGNORE_CAC:
	{
		hdd_debug("Set Dfs ignore CAC  %d", set_value);

		if (adapter->device_mode != QDF_SAP_MODE)
			return -EINVAL;

		ret = wlansap_set_dfs_ignore_cac(mac_handle, set_value);
		break;
	}

	case QCASAP_SET_DFS_TARGET_CHNL:
	{
		hdd_debug("Set Dfs target channel  %d", set_value);

		if (adapter->device_mode != QDF_SAP_MODE)
			return -EINVAL;

		ret = wlansap_set_dfs_target_chnl(mac_handle,
						  wlan_reg_legacy_chan_to_freq(hdd_ctx->pdev,
									       set_value));
		break;
	}

	case QCASAP_SET_HE_BSS_COLOR:
		if (adapter->device_mode != QDF_SAP_MODE)
			return -EINVAL;

		status = sme_set_he_bss_color(mac_handle, adapter->vdev_id,
				set_value);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("SET_HE_BSS_COLOR failed");
			return -EIO;
		}
		break;
	case QCASAP_SET_DFS_NOL:
		wlansap_set_dfs_nol(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter),
			(eSapDfsNolType) set_value);
		break;

	case QCASAP_SET_RADAR_CMD:
	{
		struct hdd_ap_ctx *ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
		struct wlan_objmgr_pdev *pdev;
		struct radar_found_info radar;

		hdd_debug("Set QCASAP_SET_RADAR_CMD val %d", set_value);

		pdev = hdd_ctx->pdev;
		if (!pdev) {
			hdd_err("null pdev");
			return -EINVAL;
		}

		qdf_mem_zero(&radar, sizeof(radar));
		if (policy_mgr_get_dfs_beaconing_session_id(hdd_ctx->psoc) !=
		    WLAN_UMAC_VDEV_ID_MAX)
			tgt_dfs_process_radar_ind(pdev, &radar);
		else
			hdd_debug("Ignore set radar, op ch_freq(%d) is not dfs",
				  ap_ctx->operating_chan_freq);

		break;
	}
	case QCASAP_TX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_TX_CHAINMASK_CMD val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_PDEV_PARAM_TX_CHAIN_MASK,
					  set_value, PDEV_CMD);
		ret = hdd_set_antenna_mode(adapter, hdd_ctx, set_value);
		break;
	}

	case QCASAP_RX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_RX_CHAINMASK_CMD val %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_PDEV_PARAM_RX_CHAIN_MASK,
					  set_value, PDEV_CMD);
		ret = hdd_set_antenna_mode(adapter, hdd_ctx, set_value);
		break;
	}

	case QCASAP_NSS_CMD:
	{
		hdd_debug("QCASAP_NSS_CMD val %d", set_value);
		hdd_update_nss(adapter, set_value, set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_NSS,
					  set_value, VDEV_CMD);
		break;
	}

	case QCSAP_IPA_UC_STAT:
	{
		/* If input value is non-zero get stats */
		switch (set_value) {
		case 1:
			ucfg_ipa_uc_stat(hdd_ctx->pdev);
			break;
		case 2:
			ucfg_ipa_uc_info(hdd_ctx->pdev);
			break;
		case 3:
			ucfg_ipa_uc_rt_debug_host_dump(hdd_ctx->pdev);
			break;
		case 4:
			ucfg_ipa_dump_info(hdd_ctx->pdev);
			break;
		default:
			/* place holder for stats clean up
			 * Stats clean not implemented yet on FW and IPA
			 */
			break;
		}
		return ret;
	}

	case QCASAP_SET_PHYMODE:
		ret = hdd_we_update_phymode(adapter, set_value);
		break;

	case QCASAP_DUMP_STATS:
	{
		hdd_debug("QCASAP_DUMP_STATS val %d", set_value);
		ret = hdd_wlan_dump_stats(adapter, set_value);
		break;
	}
	case QCASAP_CLEAR_STATS:
	{
		void *soc = cds_get_context(QDF_MODULE_ID_SOC);

		hdd_debug("QCASAP_CLEAR_STATS val %d", set_value);
		switch (set_value) {
		case CDP_HDD_STATS:
			memset(&adapter->stats, 0,
						sizeof(adapter->stats));
			memset(&adapter->hdd_stats, 0,
					sizeof(adapter->hdd_stats));
			break;
		case CDP_TXRX_HIST_STATS:
			wlan_hdd_clear_tx_rx_histogram(hdd_ctx);
			break;
		case CDP_HDD_NETIF_OPER_HISTORY:
			wlan_hdd_clear_netif_queue_history(hdd_ctx);
			break;
		case CDP_HIF_STATS:
			hdd_clear_hif_stats();
			break;
		default:
			if (soc)
				cdp_clear_stats(soc, OL_TXRX_PDEV_ID,
						set_value);
		}
		break;
	}
	case QCSAP_START_FW_PROFILING:
		hdd_debug("QCSAP_START_FW_PROFILING %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					WMI_WLAN_PROFILE_TRIGGER_CMDID,
					set_value, DBG_CMD);
		break;
	case QCASAP_PARAM_LDPC:
		ret = hdd_set_ldpc(adapter, set_value);
		break;
	case QCASAP_PARAM_TX_STBC:
		ret = hdd_set_tx_stbc(adapter, set_value);
		break;
	case QCASAP_PARAM_RX_STBC:
		ret = hdd_set_rx_stbc(adapter, set_value);
		break;
	case QCASAP_SET_11AX_RATE:
		ret = hdd_set_11ax_rate(adapter, set_value,
					&adapter->session.ap.
					sap_config);
		break;
	case QCASAP_PARAM_DCM:
		hdd_debug("Set WMI_VDEV_PARAM_HE_DCM: %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_HE_DCM, set_value,
					  VDEV_CMD);
		break;
	case QCASAP_PARAM_RANGE_EXT:
		hdd_debug("Set WMI_VDEV_PARAM_HE_RANGE_EXT: %d", set_value);
		ret = wma_cli_set_command(adapter->vdev_id,
					  WMI_VDEV_PARAM_HE_RANGE_EXT,
					  set_value, VDEV_CMD);
		break;
	case QCSAP_SET_DEFAULT_AMPDU:
		hdd_debug("QCSAP_SET_DEFAULT_AMPDU val %d", set_value);
		ret = wma_cli_set_command((int)adapter->vdev_id,
				(int)WMI_PDEV_PARAM_MAX_MPDUS_IN_AMPDU,
				set_value, PDEV_CMD);
		break;
	case QCSAP_ENABLE_RTS_BURSTING:
		hdd_debug("QCSAP_ENABLE_RTS_BURSTING val %d", set_value);
		ret = wma_cli_set_command((int)adapter->vdev_id,
				(int)WMI_PDEV_PARAM_ENABLE_RTS_SIFS_BURSTING,
				set_value, PDEV_CMD);
		break;
	case QCSAP_SET_BTCOEX_MODE:
		ret =  wlan_hdd_set_btcoex_mode(adapter, set_value);
		break;
	case QCSAP_SET_BTCOEX_LOW_RSSI_THRESHOLD:
		ret =  wlan_hdd_set_btcoex_rssi_threshold(adapter, set_value);
		break;
	default:
		hdd_err("Invalid setparam command %d value %d",
		       sub_cmd, set_value);
		ret = -EINVAL;
		break;
	}
	hdd_exit();
	return ret;
}

/**
 * __iw_softap_get_three() - return three value to upper layer.
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/out
 *
 * Return: execute result
 */
static int __iw_softap_get_three(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu, char *extra)
{
	uint32_t *value = (uint32_t *)extra;
	uint32_t sub_cmd = value[0];
	int ret = 0; /* success */
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	switch (sub_cmd) {
	case QCSAP_GET_TSF:
		ret = hdd_indicate_tsf(adapter, value, 3);
		break;
	default:
		hdd_err("Invalid getparam command: %d", sub_cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}


/**
 * iw_softap_get_three() - return three value to upper layer.
 *
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/Output
 *
 * Return: execute result
 */
static int iw_softap_get_three(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_get_three(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int
static iw_softap_setparam(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_setparam(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int
static __iw_softap_getparam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	int *value = (int *)extra;
	int sub_cmd = value[0];
	int ret;
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	switch (sub_cmd) {
	case QCSAP_PARAM_MAX_ASSOC:
		if (ucfg_mlme_get_assoc_sta_limit(hdd_ctx->psoc, value) !=
		    QDF_STATUS_SUCCESS) {
			hdd_err("CFG_ASSOC_STA_LIMIT failed");
			ret = -EIO;
		}

		break;

	case QCSAP_PARAM_GET_WLAN_DBG:
	{
		qdf_trace_display();
		*value = 0;
		break;
	}

	case QCSAP_PARAM_RTSCTS:
	{
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_ENABLE_RTSCTS,
					     VDEV_CMD);
		break;
	}

	case QCASAP_SHORT_GI:
	{
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_SGI,
					     VDEV_CMD);
		break;
	}

	case QCSAP_GTX_HT_MCS:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_HT_MCS");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_HT_MCS,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_VHT_MCS:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_VHT_MCS");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_VHT_MCS,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_USRCFG:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_USR_CFG");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_USR_CFG,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_THRE:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_THRE");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_THRE,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_MARGIN:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_MARGIN");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_MARGIN,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_STEP:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_STEP");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_STEP,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_MINTPC:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_MINTPC");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_MINTPC,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_BWMASK:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_BW_MASK");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_GTX_BW_MASK,
					     GTX_CMD);
		break;
	}

	case QCASAP_GET_DFS_NOL:
	{
		struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
		struct wlan_objmgr_pdev *pdev;

		pdev = hdd_ctx->pdev;
		if (!pdev) {
			hdd_err("null pdev");
			return -EINVAL;
		}

		utils_dfs_print_nol_channels(pdev);
	}
	break;

	case QCSAP_GET_ACL:
	{
		hdd_debug("QCSAP_GET_ACL");
		if (hdd_print_acl(adapter) !=
		    QDF_STATUS_SUCCESS) {
			hdd_err("QCSAP_GET_ACL returned Error: not completed");
		}
		*value = 0;
		break;
	}

	case QCASAP_TX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_TX_CHAINMASK_CMD");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_PDEV_PARAM_TX_CHAIN_MASK,
					     PDEV_CMD);
		break;
	}

	case QCASAP_RX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_RX_CHAINMASK_CMD");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_PDEV_PARAM_RX_CHAIN_MASK,
					     PDEV_CMD);
		break;
	}

	case QCASAP_NSS_CMD:
	{
		hdd_debug("QCASAP_NSS_CMD");
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_NSS,
					     VDEV_CMD);
		break;
	}
	case QCSAP_CAP_TSF:
		ret = hdd_capture_tsf(adapter, (uint32_t *)value, 1);
		break;
	case QCASAP_GET_TEMP_CMD:
	{
		hdd_debug("QCASAP_GET_TEMP_CMD");
		ret = wlan_hdd_get_temperature(adapter, value);
		break;
	}
	case QCSAP_GET_FW_PROFILE_DATA:
		hdd_debug("QCSAP_GET_FW_PROFILE_DATA");
		ret = wma_cli_set_command(adapter->vdev_id,
				WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID,
				0, DBG_CMD);
		break;
	case QCASAP_PARAM_LDPC:
	{
		ret = hdd_get_ldpc(adapter, value);
		break;
	}
	case QCASAP_PARAM_TX_STBC:
	{
		ret = hdd_get_tx_stbc(adapter, value);
		break;
	}
	case QCASAP_PARAM_RX_STBC:
	{
		ret = hdd_get_rx_stbc(adapter, value);
		break;
	}
	case QCSAP_PARAM_CHAN_WIDTH:
	{
		ret = hdd_sap_get_chan_width(adapter, value);
		break;
	}
	case QCASAP_PARAM_DCM:
	{
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_HE_DCM,
					     VDEV_CMD);
		break;
	}
	case QCASAP_PARAM_RANGE_EXT:
	{
		*value = wma_cli_get_command(adapter->vdev_id,
					     WMI_VDEV_PARAM_HE_RANGE_EXT,
					     VDEV_CMD);
		break;
	}
	default:
		hdd_err("Invalid getparam command: %d", sub_cmd);
		ret = -EINVAL;
		break;

	}
	hdd_exit();
	return ret;
}

int
static iw_softap_getparam(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_getparam(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/* Usage:
 *  BLACK_LIST  = 0
 *  WHITE_LIST  = 1
 *  ADD MAC = 0
 *  REMOVE MAC  = 1
 *
 *  mac addr will be accepted as a 6 octet mac address with each octet
 *  inputted in hex for e.g. 00:0a:f5:11:22:33 will be represented as
 *  0x00 0x0a 0xf5 0x11 0x22 0x33 while using this ioctl
 *
 *  Syntax:
 *  iwpriv softap.0 modify_acl
 *  <6 octet mac addr> <list type> <cmd type>
 *
 *  Examples:
 *  eg 1. to add a mac addr 00:0a:f5:89:89:90 to the black list
 *  iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 0 0
 *  eg 2. to delete a mac addr 00:0a:f5:89:89:90 from white list
 *  iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 1 1
 */
static
int __iw_softap_modify_acl(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	uint8_t *value = (uint8_t *) kmalloc(wrqu->data.length+1, GFP_KERNEL); //MOT IKLOCSEN-3014
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	int list_type, cmd, i;
	int ret;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret) {
	    kfree(value); //MOT IKLOCSEN-3014
	    return ret;
	}
	//BEGIN MOT a19110 IKLOCSEN-3014 use copy_from_user to avoid junk value
	if(copy_from_user((uint8_t *) value, (uint8_t *)(wrqu->data.pointer), wrqu->data.length)) {
	    hdd_err("%s -- copy_from_user --data pointer failed! bailing",
	                       __func__);
	    kfree(value);
	    return -EFAULT;
	}
        //END IKLOCSEN-3014

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	for (i = 0; i < QDF_MAC_ADDR_SIZE; i++)
		peer_mac[i] = *(value + i);

	list_type = (int)(*(value + i));
	i++;
	cmd = (int)(*(value + i));

        kfree(value);//MOT IKLOCSEN-3014
	hdd_debug("Modify ACL mac:" QDF_MAC_ADDR_FMT " type: %d cmd: %d",
	       QDF_MAC_ADDR_REF(peer_mac), list_type, cmd);

	qdf_status = wlansap_modify_acl(
		WLAN_HDD_GET_SAP_CTX_PTR(adapter),
		peer_mac, (eSapACLType) list_type, (eSapACLCmdType) cmd);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Modify ACL failed");
		ret = -EIO;
	}
	hdd_exit();
	return ret;
}

static
int iw_softap_modify_acl(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_modify_acl(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int
static __iw_softap_getchannel(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	struct hdd_ap_ctx *ap_ctx;
	int *value = (int *)extra;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	*value = 0;
	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags))
		*value = wlan_reg_freq_to_chan(
				hdd_ctx->pdev,
				ap_ctx->operating_chan_freq);
	hdd_exit();
	return 0;
}

int
static iw_softap_getchannel(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_getchannel(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int
static __iw_softap_set_max_tx_power(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	uint8_t *mot_value;
	int set_value;
	int ret;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_BCAST_INIT;
	struct qdf_mac_addr selfMac = QDF_MAC_ADDR_BCAST_INIT;

	hdd_enter_dev(dev);
        mot_value = (uint8_t*)kmalloc(wrqu->data.length+1, GFP_KERNEL);
	if (NULL == mot_value)
		return -ENOMEM;
        if(copy_from_user((uint8_t *)mot_value, (uint8_t *)(wrqu->data.pointer), wrqu->data.length)) {
            hdd_err("%s -- copy from user -- data pointer failed! bailing", __func__);
            kfree(mot_value);
            return -EFAULT;
        }
        set_value = (int)(*(mot_value + 0));
        kfree(mot_value);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	/* Assign correct self MAC address */
	qdf_copy_macaddr(&bssid, &adapter->mac_addr);
	qdf_copy_macaddr(&selfMac, &adapter->mac_addr);

        if (QDF_STATUS_SUCCESS !=
	    sme_set_max_tx_power(hdd_ctx->mac_handle, bssid,
				 selfMac, set_value)) {
		hdd_err("Setting maximum tx power failed");
		return -EIO;
	}
	hdd_exit();
	return 0;
}

int
static iw_softap_set_max_tx_power(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_set_max_tx_power(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#ifndef REMOVE_PKT_LOG
int
static __iw_softap_set_pktlog(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct hdd_context *hdd_ctx;
	int *value = (int *)extra;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	if (wrqu->data.length < 1 || wrqu->data.length > 2) {
		hdd_err("pktlog: either 1 or 2 parameters are required");
		return -EINVAL;
	}

	return hdd_process_pktlog_command(hdd_ctx, value[0], value[1]);
}

int
static iw_softap_set_pktlog(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_set_pktlog(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
#else
int
static iw_softap_set_pktlog(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	return -EINVAL;
}
#endif

int
static __iw_softap_set_tx_power(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	int *value = (int *)extra;
	int set_value;
	struct qdf_mac_addr bssid;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	qdf_copy_macaddr(&bssid, &adapter->mac_addr);

	set_value = value[0];
	if (QDF_STATUS_SUCCESS !=
	    sme_set_tx_power(hdd_ctx->mac_handle, adapter->vdev_id, bssid,
			     adapter->device_mode, set_value)) {
		hdd_err("Setting tx power failed");
		return -EIO;
	}
	hdd_exit();
	return 0;
}

int
static iw_softap_set_tx_power(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_set_tx_power(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int
static __iw_softap_getassoc_stamacaddr(struct net_device *dev,
				       struct iw_request_info *info,
				       union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_station_info *sta_info;
	struct hdd_context *hdd_ctx;
	char *buf;
	int left;
	int ret;
	/* maclist_index must be u32 to match userspace */
	u32 maclist_index;

	hdd_enter_dev(dev);

	/*
	 * NOTE WELL: this is a "get" ioctl but it uses an even ioctl
	 * number, and even numbered iocts are supposed to have "set"
	 * semantics.  Hence the wireless extensions support in the kernel
	 * won't correctly copy the result to userspace, so the ioctl
	 * handler itself must copy the data.  Output format is 32-bit
	 * record length, followed by 0 or more 6-byte STA MAC addresses.
	 *
	 * Further note that due to the incorrect semantics, the "iwpriv"
	 * userspace application is unable to correctly invoke this API,
	 * hence it is not registered in the hostapd_private_args.  This
	 * API can only be invoked by directly invoking the ioctl() system
	 * call.
	 */

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	/* make sure userspace allocated a reasonable buffer size */
	if (wrqu->data.length < sizeof(maclist_index)) {
		hdd_err("invalid userspace buffer");
		return -EINVAL;
	}

	/* allocate local buffer to build the response */
	buf = qdf_mem_malloc(wrqu->data.length);
	if (!buf)
		return -ENOMEM;

	/* start indexing beyond where the record count will be written */
	maclist_index = sizeof(maclist_index);
	left = wrqu->data.length - maclist_index;

	hdd_for_each_sta_ref(adapter->sta_info_list, sta_info,
			     STA_INFO_SAP_GETASSOC_STAMACADDR) {
		if (!qdf_is_macaddr_broadcast(&sta_info->sta_mac)) {
			memcpy(&buf[maclist_index], &sta_info->sta_mac,
			       QDF_MAC_ADDR_SIZE);
			maclist_index += QDF_MAC_ADDR_SIZE;
			left -= QDF_MAC_ADDR_SIZE;
		}
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SAP_GETASSOC_STAMACADDR);
	}

	*((u32 *) buf) = maclist_index;
	wrqu->data.length = maclist_index;
	if (copy_to_user(wrqu->data.pointer, buf, maclist_index)) {
		hdd_err("failed to copy response to user buffer");
		ret = -EFAULT;
	}
	qdf_mem_free(buf);
	hdd_exit();
	return ret;
}

int
static iw_softap_getassoc_stamacaddr(struct net_device *dev,
				     struct iw_request_info *info,
				     union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_getassoc_stamacaddr(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/* Usage:
 *  mac addr will be accepted as a 6 octet mac address with each octet
 *  inputted in hex for e.g. 00:0a:f5:11:22:33 will be represented as
 *  0x00 0x0a 0xf5 0x11 0x22 0x33 while using this ioctl
 *
 *  Syntax:
 *  iwpriv softap.0 disassoc_sta <6 octet mac address>
 *
 *  e.g.
 *  disassociate sta with mac addr 00:0a:f5:11:22:33 from softap
 *  iwpriv softap.0 disassoc_sta 0x00 0x0a 0xf5 0x11 0x22 0x33
 */

int
static __iw_softap_disassoc_sta(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	uint8_t *peer_macaddr = (uint8_t *)kmalloc(wrqu->data.length+1, GFP_KERNEL); //MOT IKLOCSEN-3014
	int ret;
	struct csr_del_sta_params del_sta_params;

	hdd_enter_dev(dev);

	if (!capable(CAP_NET_ADMIN)) {
		hdd_err("permission check failed");
		kfree(peer_macaddr); //MOT IKLOCSEN-3014
		return -EPERM;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret){
		kfree(peer_macaddr); //MOT IKLOCSEN-3014
		return ret;
        }
	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret){
		kfree(peer_macaddr); //MOT IKLOCSEN-3014
		return ret;
	}

	/* iwpriv tool or framework calls this ioctl with
	 * data passed in extra (less than 16 octets);
	 */
       //BEGIN MOT a19110 IKLOCSEN-3014 use copy_from_user to avoid junk value
       if(copy_from_user((uint8_t *) peer_macaddr, (uint8_t *)(wrqu->data.pointer), wrqu->data.length)) {
               hdd_err("%s -- copy_from_user --data pointer failed! bailing",
                         __func__);
               kfree(peer_macaddr);
               return -EFAULT;
       }
       //END IKLOCSEN-3014

	hdd_debug("data " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(peer_macaddr));
	wlansap_populate_del_sta_params(peer_macaddr,
					REASON_DEAUTH_NETWORK_LEAVING,
					SIR_MAC_MGMT_DISASSOC,
					&del_sta_params);
	hdd_softap_sta_disassoc(adapter, &del_sta_params);
        kfree(peer_macaddr); //MOT IKLOCSEN-3014
	hdd_exit();
	return 0;
}

int
static iw_softap_disassoc_sta(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_disassoc_sta(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * iw_get_char_setnone() - Generic "get char" private ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_get_char_setnone(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int ret;
	int sub_cmd = wrqu->data.flags;
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	switch (sub_cmd) {
	case QCSAP_GET_STATS:
		hdd_wlan_get_stats(adapter, &(wrqu->data.length),
					extra, WE_MAX_STR_LEN);
		break;
	case QCSAP_LIST_FW_PROFILE:
		hdd_wlan_list_fw_profile(&(wrqu->data.length),
					extra, WE_MAX_STR_LEN);
		break;
	}

	hdd_exit();
	return ret;
}

static int iw_get_char_setnone(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_get_char_setnone(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int iw_get_channel_list(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	uint32_t num_channels = 0;
	uint8_t i = 0;
	struct hdd_adapter *hostapd_adapter = (netdev_priv(dev));
	struct channel_list_info *channel_list =
		(struct channel_list_info *)extra;
	struct regulatory_channel *cur_chan_list = NULL;
	struct hdd_context *hdd_ctx;
	int ret;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(hostapd_adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	cur_chan_list = qdf_mem_malloc(sizeof(*cur_chan_list) * NUM_CHANNELS);
	if (!cur_chan_list)
		return -ENOMEM;

	status = ucfg_reg_get_current_chan_list(hdd_ctx->pdev, cur_chan_list);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err_rl("Failed to get the current channel list");
		qdf_mem_free(cur_chan_list);
		return -EIO;
	}

	for (i = 0; i < NUM_CHANNELS; i++) {
		/*
		 * current channel list includes all channels. do not report
		 * disabled channels
		 */
		if (cur_chan_list[i].chan_flags & REGULATORY_CHAN_DISABLED)
			continue;

		/*
		 * do not include 6 GHz channels since they are ambiguous with
		 * 2.4 GHz and 5 GHz channels. 6 GHz-aware applications should
		 * not be using this interface, but instead should be using the
		 * frequency-based interface
		 */
		if (wlan_reg_is_6ghz_chan_freq(cur_chan_list[i].center_freq))
			continue;
		channel_list->channels[num_channels] =
						cur_chan_list[i].chan_num;
		num_channels++;

	}

	qdf_mem_free(cur_chan_list);
	hdd_debug_rl("number of channels %d", num_channels);
	channel_list->num_channels = num_channels;
	wrqu->data.length = num_channels + 1;
	hdd_exit();

	return 0;
}

int iw_get_channel_list_with_cc(struct net_device *dev,
				mac_handle_t mac_handle,
				struct iw_request_info *info,
				union iwreq_data *wrqu,
				char *extra)
{
	uint8_t i, len;
	char *buf;
	uint8_t ubuf[REG_ALPHA2_LEN + 1] = {0};
	uint8_t ubuf_len = REG_ALPHA2_LEN + 1;
	struct channel_list_info channel_list;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	hdd_enter_dev(dev);

	memset(&channel_list, 0, sizeof(channel_list));

	if (0 != iw_get_channel_list(dev, info, wrqu, (char *)&channel_list)) {
		hdd_err_rl("GetChannelList Failed!!!");
		return -EINVAL;
	}

	/*
	 * Maximum buffer needed =
	 * [4: 3 digits of num_chn + 1 space] +
	 * [REG_ALPHA2_LEN: REG_ALPHA2_LEN digits] +
	 * [4 * num_chn: (1 space + 3 digits of chn[i]) * num_chn] +
	 * [1: Terminator].
	 *
	 * Check if sufficient buffer is available and then
	 * proceed to fill the buffer.
	 */
	if (WE_MAX_STR_LEN <
	    (4 + REG_ALPHA2_LEN + 4 * channel_list.num_channels + 1)) {
		hdd_err_rl("Insufficient Buffer to populate channel list");
		return -EINVAL;
	}

	buf = extra;
	len = scnprintf(buf, WE_MAX_STR_LEN, "%u ", channel_list.num_channels);
	wlan_reg_get_cc_and_src(mac->psoc, ubuf);
	/* Printing Country code in getChannelList(break at '\0') */
	for (i = 0; i < (ubuf_len - 1) && ubuf[i] != 0; i++)
		len += scnprintf(buf + len, WE_MAX_STR_LEN - len, "%c", ubuf[i]);

	for (i = 0; i < channel_list.num_channels; i++)
		len += scnprintf(buf + len, WE_MAX_STR_LEN - len, " %u",
				 channel_list.channels[i]);

	wrqu->data.length = strlen(extra) + 1;

	hdd_exit();
	return 0;
}

static
int __iw_get_genie(struct net_device *dev,
		   struct iw_request_info *info,
		   union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	int ret;
	QDF_STATUS status;
	uint32_t length = DOT11F_IE_RSN_MAX_LEN;
	uint8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	/*
	 * Actually retrieve the RSN IE from CSR.
	 * (We previously sent it down in the CSR Roam Profile.)
	 */
	status = wlan_sap_getstation_ie_information(
		WLAN_HDD_GET_SAP_CTX_PTR(adapter),
		&length, genIeBytes);
	if (status == QDF_STATUS_SUCCESS) {
		wrqu->data.length = length;
		if (length > DOT11F_IE_RSN_MAX_LEN) {
			hdd_err("Invalid buffer length: %d", length);
			return -E2BIG;
		}
		qdf_mem_copy(extra, genIeBytes, length);
		hdd_debug(" RSN IE of %d bytes returned",
				wrqu->data.length);
	} else {
		wrqu->data.length = 0;
		hdd_debug(" RSN IE failed to populate");
	}

	hdd_exit();
	return 0;
}

static
int iw_get_genie(struct net_device *dev,
		 struct iw_request_info *info,
		 union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_get_genie(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int
__iw_softap_stopbss(struct net_device *dev,
		    struct iw_request_info *info,
		    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		struct hdd_hostapd_state *hostapd_state =
			WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

		qdf_event_reset(&hostapd_state->qdf_stop_bss_event);
		status = wlansap_stop_bss(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter));
		if (QDF_IS_STATUS_SUCCESS(status)) {
			status =
				qdf_wait_for_event_completion(&hostapd_state->
					qdf_stop_bss_event,
					SME_CMD_STOP_BSS_TIMEOUT);

			if (!QDF_IS_STATUS_SUCCESS(status)) {
				hdd_err("wait for single_event failed!!");
				QDF_ASSERT(0);
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &adapter->event_flags);
		policy_mgr_decr_session_set_pcl(hdd_ctx->psoc,
					     adapter->device_mode,
					     adapter->vdev_id);
		hdd_green_ap_start_state_mc(hdd_ctx, adapter->device_mode,
					    false);
		ret = qdf_status_to_os_return(status);
	}
	hdd_exit();
	return ret;
}

static int iw_softap_stopbss(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_stopbss(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int
__iw_softap_version(struct net_device *dev,
		    struct iw_request_info *info,
		    union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	wrqu->data.length = hdd_wlan_get_version(hdd_ctx, WE_MAX_STR_LEN,
						 extra);
	hdd_exit();
	return 0;
}

static int iw_softap_version(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_version(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int hdd_softap_get_sta_info(struct hdd_adapter *adapter,
				   uint8_t *buf,
				   int size)
{
	int written;
	struct hdd_station_info *sta;

	hdd_enter();

	written = scnprintf(buf, size, "\nstaId staAddress\n");

	hdd_for_each_sta_ref(adapter->sta_info_list, sta,
			     STA_INFO_SOFTAP_GET_STA_INFO) {
		if (written >= size - 1) {
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &sta, true,
					     STA_INFO_SOFTAP_GET_STA_INFO);
			break;
		}

		if (QDF_IS_ADDR_BROADCAST(sta->sta_mac.bytes)) {
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &sta, true,
					     STA_INFO_SOFTAP_GET_STA_INFO);
			continue;
		}

		written += scnprintf(buf + written, size - written,
				     QDF_FULL_MAC_FMT
				     " ecsa=%d\n",
				     QDF_FULL_MAC_REF(sta->sta_mac.bytes),
				     sta->ecsa_capable);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta, true,
				     STA_INFO_SOFTAP_GET_STA_INFO);
	}

	hdd_exit();

	return 0;
}

static int __iw_softap_get_channel_list(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu,
					char *extra)
{
	int ret;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	if (hdd_validate_adapter(adapter))
		return -ENODEV;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	mac_handle = hdd_ctx->mac_handle;

	ret = iw_get_channel_list_with_cc(dev, mac_handle,
					  info, wrqu, extra);

	if (0 != ret)
		return -EINVAL;

	hdd_exit();
	return 0;
}

static int iw_softap_get_channel_list(struct net_device *dev,
				      struct iw_request_info *info,
				      union iwreq_data *wrqu,
				      char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_get_channel_list(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int __iw_softap_get_sta_info(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	adapter = netdev_priv(dev);
	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = hdd_check_private_wext_control(hdd_ctx, info);
	if (errno)
		return errno;

	errno = hdd_softap_get_sta_info(adapter, extra, WE_SAP_MAX_STA_INFO);
	if (errno) {
		hdd_err("Failed to get sta info; errno:%d", errno);
		return errno;
	}

	wrqu->data.length = strlen(extra);

	hdd_exit();

	return 0;
}

static int iw_softap_get_sta_info(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu,
				  char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_get_sta_info(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static int __iw_softap_get_ba_timeout(struct net_device *dev,
				      struct iw_request_info *info,
				      union iwreq_data *wrqu, char *extra)
{
	int errno;
	uint32_t i;
	enum qca_wlan_ac_type duration[QCA_WLAN_AC_ALL];
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;

	hdd_enter_dev(dev);

	adapter = netdev_priv(dev);
	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	if (!soc) {
		hdd_err("Invalid handle");
		return -EINVAL;
	}

	for (i = 0; i < QCA_WLAN_AC_ALL; i++)
		cdp_get_ba_timeout(soc, i, &duration[i]);

	snprintf(extra, WE_SAP_MAX_STA_INFO,
		 "\n|------------------------------|\n"
		 "|AC | BA aging timeout duration |\n"
		 "|--------------------------------|\n"
		 "|VO |  %d        |\n"
		 "|VI |  %d        |\n"
		 "|BK |  %d        |\n"
		 "|BE |  %d        |\n"
		 "|--------------------------------|\n",
		duration[QCA_WLAN_AC_VO], duration[QCA_WLAN_AC_VI],
		duration[QCA_WLAN_AC_BK], duration[QCA_WLAN_AC_BE]);

	wrqu->data.length = strlen(extra) + 1;
	hdd_exit();

	return 0;
}

static int iw_softap_get_ba_timeout(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu,
				    char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_softap_get_ba_timeout(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static
int __iw_get_softap_linkspeed(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter = (netdev_priv(dev));
	struct hdd_context *hdd_ctx;
	char *out_link_speed = (char *)extra;
	uint32_t link_speed = 0;
	int len = sizeof(uint32_t) + 1;
	struct qdf_mac_addr mac_address;
	char macaddr_string[MAC_ADDRESS_STR_LEN + 1];
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int rc, ret;

	hdd_enter_dev(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_check_private_wext_control(hdd_ctx, info);
	if (0 != ret)
		return ret;

	hdd_debug("wrqu->data.length(%d)", wrqu->data.length);

	/* Linkspeed is allowed only for P2P mode */
	if (adapter->device_mode != QDF_P2P_GO_MODE) {
		hdd_err("Link Speed is not allowed in Device mode %s(%d)",
			qdf_opmode_str(adapter->device_mode),
			adapter->device_mode);
		return -ENOTSUPP;
	}

	if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1) {
		if (copy_from_user(macaddr_string,
				   wrqu->data.pointer, MAC_ADDRESS_STR_LEN)) {
			hdd_err("failed to copy data to user buffer");
			return -EFAULT;
		}
		macaddr_string[MAC_ADDRESS_STR_LEN - 1] = '\0';

		if (!mac_pton(macaddr_string, mac_address.bytes)) {
			hdd_err("String to Hex conversion Failed");
			return -EINVAL;
		}
	}
	/* If no mac address is passed and/or its length is less than 17,
	 * link speed for first connected client will be returned.
	 */
	if (wrqu->data.length < 17 || !QDF_IS_STATUS_SUCCESS(status)) {
		struct hdd_station_info *sta_info;

		hdd_for_each_sta_ref(adapter->sta_info_list, sta_info,
				     STA_INFO_GET_SOFTAP_LINKSPEED) {
			if (!qdf_is_macaddr_broadcast(&sta_info->sta_mac)) {
				qdf_copy_macaddr(&mac_address,
						 &sta_info->sta_mac);
				status = QDF_STATUS_SUCCESS;
				hdd_put_sta_info_ref(
						&adapter->sta_info_list,
						&sta_info, true,
						STA_INFO_GET_SOFTAP_LINKSPEED);
				break;
			}
			hdd_put_sta_info_ref(&adapter->sta_info_list,
					     &sta_info, true,
					     STA_INFO_GET_SOFTAP_LINKSPEED);
		}
	}
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Invalid peer macaddress");
		return -EINVAL;
	}
	rc = wlan_hdd_get_linkspeed_for_peermac(adapter, &mac_address,
						&link_speed);
	if (rc) {
		hdd_err("Unable to retrieve SME linkspeed");
		return rc;
	}

	/* linkspeed in units of 500 kbps */
	link_speed = link_speed / 500;
	wrqu->data.length = len;
	rc = snprintf(out_link_speed, len, "%u", link_speed);
	if ((rc < 0) || (rc >= len)) {
		/* encoding or length error? */
		hdd_err("Unable to encode link speed");
		return -EIO;
	}
	hdd_exit();
	return 0;
}

static int
iw_get_softap_linkspeed(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu,
			char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_get_softap_linkspeed(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call wlan_cfg80211_mc_cp_stats_get_peer_rssi
 * to get rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
__iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
		   union iwreq_data *wrqu, char *extra)
{
	int ret, i;
	struct hdd_context *hddctx;
	struct stats_event *rssi_info;
	char macaddrarray[MAC_ADDRESS_STR_LEN];
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct qdf_mac_addr macaddress = QDF_MAC_ADDR_BCAST_INIT;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	hddctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hddctx);
	if (ret != 0)
		return ret;

	ret = hdd_check_private_wext_control(hddctx, info);
	if (0 != ret)
		return ret;

	hdd_debug("wrqu->data.length= %d", wrqu->data.length);

	if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1) {
		if (copy_from_user(macaddrarray,
				   wrqu->data.pointer,
				   MAC_ADDRESS_STR_LEN - 1)) {
			hdd_info("failed to copy data from user buffer");
			return -EFAULT;
		}

		macaddrarray[MAC_ADDRESS_STR_LEN - 1] = '\0';
		hdd_debug("%s", macaddrarray);

		if (!mac_pton(macaddrarray, macaddress.bytes))
			hdd_err("String to Hex conversion Failed");
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	rssi_info = wlan_cfg80211_mc_cp_stats_get_peer_rssi(vdev,
							    macaddress.bytes,
							    &ret);
	hdd_objmgr_put_vdev(vdev);
	if (ret || !rssi_info) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
		return ret;
	}

	wrqu->data.length = scnprintf(extra, IW_PRIV_SIZE_MASK, "\n");
	for (i = 0; i < rssi_info->num_peer_stats; i++)
		wrqu->data.length +=
			scnprintf(extra + wrqu->data.length,
				  IW_PRIV_SIZE_MASK - wrqu->data.length,
				  "["QDF_FULL_MAC_FMT"] [%d]\n",
				  QDF_FULL_MAC_REF(rssi_info->peer_stats[i].peer_macaddr),
				  rssi_info->peer_stats[i].peer_rssi);

	wrqu->data.length++;
	wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
	hdd_exit();

	return 0;
}

/**
 * iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call __iw_get_peer_rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
		 union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __iw_get_peer_rssi(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/*
 * Note that the following ioctls were defined with semantics which
 * cannot be handled by the "iwpriv" userspace application and hence
 * they are not included in the hostapd_private_args array
 *     QCSAP_IOCTL_ASSOC_STA_MACADDR
 */

static const struct iw_priv_args hostapd_private_args[] = {
	{
		QCSAP_IOCTL_SETPARAM,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam"
	}, {
		QCSAP_IOCTL_SETPARAM,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""
	}, {
		QCSAP_PARAM_MAX_ASSOC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMaxAssoc"
	}, {
		QCSAP_PARAM_HIDE_SSID,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hideSSID"
	}, {
		QCSAP_PARAM_SET_MC_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setMcRate"
	}, {
		QCSAP_PARAM_SET_TXRX_FW_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"txrx_fw_stats"
	}, {
		QCSAP_PARAM_SET_TXRX_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0,
		"txrx_stats"
	}, {
		QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMccLatency"
	}, {
		QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMccQuota"
	}, {
		QCSAP_PARAM_SET_CHANNEL_CHANGE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setChanChange"
	}, {
		QCSAP_PARAM_CONC_SYSTEM_PREF,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setConcSysPref"
	},
#ifdef FEATURE_FW_LOG_PARSING
	/* Sub-cmds DBGLOG specific commands */
	{
		QCSAP_DBGLOG_LOG_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_loglevel"
	}, {
		QCSAP_DBGLOG_VAP_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_vapon"
	}, {
		QCSAP_DBGLOG_VAP_DISABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_vapoff"
	}, {
		QCSAP_DBGLOG_MODULE_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_modon"
	}, {
		QCSAP_DBGLOG_MODULE_DISABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_modoff"
	}, {
		QCSAP_DBGLOG_MOD_LOG_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_mod_loglevel"
	}, {
		QCSAP_DBGLOG_TYPE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_type"
	}, {
		QCSAP_DBGLOG_REPORT_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_report"
	},
#endif /* FEATURE_FW_LOG_PARSING */
	{

		QCASAP_TXRX_FWSTATS_RESET,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "txrx_fw_st_rst"
	}, {
		QCSAP_PARAM_RTSCTS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "enablertscts"
	}, {
		QCASAP_SET_11N_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set11NRates"
	}, {
		QCASAP_SET_VHT_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set11ACRates"
	}, {
		QCASAP_SHORT_GI,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "enable_short_gi"
	}, {
		QCSAP_SET_AMPDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ampdu"
	}, {
		QCSAP_SET_AMSDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "amsdu"
	}, {
		QCSAP_GTX_HT_MCS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxHTMcs"
	}, {
		QCSAP_GTX_VHT_MCS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxVHTMcs"
	}, {
		QCSAP_GTX_USRCFG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxUsrCfg"
	}, {
		QCSAP_GTX_THRE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxThre"
	}, {
		QCSAP_GTX_MARGIN,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxMargin"
	}, {
		QCSAP_GTX_STEP,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxStep"
	}, {
		QCSAP_GTX_MINTPC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxMinTpc"
	}, {
		QCSAP_GTX_BWMASK,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxBWMask"
	}, {
		QCSAP_PARAM_CLR_ACL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setClearAcl"
	}, {
		QCSAP_PARAM_ACL_MODE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setAclMode"
	},
	{
		QCASAP_SET_TM_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setTmLevel"
	}, {
		QCASAP_SET_DFS_IGNORE_CAC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setDfsIgnoreCAC"
	}, {
		QCASAP_SET_DFS_NOL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setdfsnol"
	}, {
		QCASAP_SET_DFS_TARGET_CHNL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setNextChnl"
	}, {
		QCASAP_SET_RADAR_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setRadar"
	},
	{
		QCSAP_IPA_UC_STAT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ipaucstat"
	},
	{
		QCASAP_TX_CHAINMASK_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_txchainmask"
	}, {
		QCASAP_RX_CHAINMASK_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_rxchainmask"
	}, {
		QCASAP_SET_HE_BSS_COLOR,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_he_bss_clr"
	}, {
		QCASAP_NSS_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_nss"
	}, {
		QCASAP_SET_PHYMODE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setphymode"
	}, {
		QCASAP_DUMP_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dumpStats"
	}, {
		QCASAP_CLEAR_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "clearStats"
	}, {
		QCSAP_START_FW_PROFILING,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "startProfile"
	}, {
		QCASAP_PARAM_LDPC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "ldpc"
	}, {
		QCASAP_PARAM_TX_STBC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_tx_stbc"
	}, {
		QCASAP_PARAM_RX_STBC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_rx_stbc"
	}, {
		QCSAP_IOCTL_GETPARAM, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getparam"
	}, {
		QCSAP_IOCTL_GETPARAM, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""
	}, {
		QCSAP_PARAM_MAX_ASSOC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getMaxAssoc"
	}, {
		QCSAP_PARAM_GET_WLAN_DBG, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getwlandbg"
	}, {
		QCSAP_GTX_BWMASK, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxBWMask"
	}, {
		QCSAP_GTX_MINTPC, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxMinTpc"
	}, {
		QCSAP_GTX_STEP, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxStep"
	}, {
		QCSAP_GTX_MARGIN, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxMargin"
	}, {
		QCSAP_GTX_THRE, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxThre"
	}, {
		QCSAP_GTX_USRCFG, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxUsrCfg"
	}, {
		QCSAP_GTX_VHT_MCS, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxVHTMcs"
	}, {
		QCSAP_GTX_HT_MCS, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxHTMcs"
	}, {
		QCASAP_SHORT_GI, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_short_gi"
	}, {
		QCSAP_PARAM_RTSCTS, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rtscts"
	}, {
		QCASAP_GET_DFS_NOL, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdfsnol"
	}, {
		QCSAP_GET_ACL, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_acl_list"
	}, {
		QCASAP_PARAM_LDPC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_ldpc"
	}, {
		QCASAP_PARAM_TX_STBC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_tx_stbc"
	}, {
		QCASAP_PARAM_RX_STBC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_rx_stbc"
	}, {
		QCSAP_PARAM_CHAN_WIDTH, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_chwidth"
	}, {
		QCASAP_TX_CHAINMASK_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_txchainmask"
	}, {
		QCASAP_RX_CHAINMASK_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_rxchainmask"
	}, {
		QCASAP_NSS_CMD, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_nss"
	}, {
		QCSAP_CAP_TSF, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"cap_tsf"
	}, {
		QCSAP_IOCTL_SET_NONE_GET_THREE, 0, IW_PRIV_TYPE_INT |
		IW_PRIV_SIZE_FIXED | 3,    ""
	}, {
		QCSAP_GET_TSF, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
		"get_tsf"
	}, {
		QCASAP_GET_TEMP_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_temp"
	}, {
		QCSAP_GET_FW_PROFILE_DATA, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getProfileData"
	}, {
		QCSAP_IOCTL_GET_STAWPAIE,
		0, IW_PRIV_TYPE_BYTE | DOT11F_IE_RSN_MAX_LEN,
		"get_staWPAIE"
	}, {
		QCSAP_IOCTL_STOPBSS, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED, 0,
		"stopbss"
	}, {
		QCSAP_IOCTL_VERSION, 0, IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN,
		"version"
	}, {
		QCSAP_IOCTL_GET_STA_INFO, 0,
		IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_sta_info"
	}, {
		QCSAP_IOCTL_GET_CHANNEL, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getchannel"
	}, {
		QCSAP_IOCTL_GET_BA_AGEING_TIMEOUT, 0,
		IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_ba_timeout"
	}, {
		QCSAP_IOCTL_DISASSOC_STA,
		IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6, 0,
		"disassoc_sta"
	}
	/* handler for main ioctl */
	, {
		QCSAP_PRIV_GET_CHAR_SET_NONE, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, ""
	}
	/* handler for sub-ioctl */
	, {
		QCSAP_GET_STATS, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "getStats"
	}
	, {
		QCSAP_LIST_FW_PROFILE, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "listProfile"
	}
	, {
		QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED,
		IW_PRIV_TYPE_CHAR | 18,
		IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed"
	}
	, {
		QCSAP_IOCTL_PRIV_GET_RSSI,
		IW_PRIV_TYPE_CHAR | 18,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "getRSSI"
	}
	, {
		QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, ""
	}
	,
	/* handlers for sub-ioctl */
	{
		WE_SET_WLAN_DBG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "setwlandbg"
	}
	,
#ifdef CONFIG_DP_TRACE
	/* handlers for sub-ioctl */
	{
		WE_SET_DP_TRACE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_dp_trace"
	}
	,
#endif
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0, ""
	}
	, {
		WE_P2P_NOA_CMD, IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0, "SetP2pPs"
	}
	, {
		WE_UNIT_TEST_CMD, IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0,
		"setUnitTestCmd"
	}
#ifdef WLAN_DEBUG
	,
	{
		WE_SET_CHAN_AVOID,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
		0,
		"ch_avoid"
	}
#endif
	,
	{
		QCSAP_SET_BTCOEX_MODE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_btc_mode"
	}
	,
	{
		QCSAP_SET_BTCOEX_LOW_RSSI_THRESHOLD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_btc_rssi"
	}
	,
#ifdef FW_THERMAL_THROTTLE_SUPPORT
	{
		WE_SET_THERMAL_THROTTLE_CFG,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
		0, "set_thermal_cfg"
	}
	,
#endif /* FW_THERMAL_THROTTLE_SUPPORT */
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_MODIFY_ACL,
		IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 8, 0, "modify_acl"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_GET_CHANNEL_LIST,
		0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN,
		"getChannelList"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_SET_TX_POWER,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setTxPower"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_SET_MAX_TX_POWER,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setTxMaxPower"
	}
	,
	{
		QCSAP_IOCTL_SET_PKTLOG,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
		0, "pktlog"
	}
	,
	/* Get HDD CFG Ini param */
	{
		QCSAP_IOCTL_GET_INI_CFG,
		0, IW_PRIV_TYPE_CHAR | QCSAP_IOCTL_MAX_STR_LEN, "getConfig"
	}
	,
	/* handlers for main ioctl */
	{
	/* handlers for main ioctl */
		QCSAP_IOCTL_SET_TWO_INT_GET_NONE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, ""
	}
	,
	/* handlers for sub-ioctl */
#ifdef CONFIG_WLAN_DEBUG_CRASH_INJECT
	{
		QCSAP_IOCTL_SET_FW_CRASH_INJECT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "crash_inject"
	}
	,
#endif
	{
		QCASAP_SET_RADAR_DBG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0,  "setRadarDbg"
	}
	,
#ifdef CONFIG_DP_TRACE
	/* dump dp trace - descriptor or dp trace records */
	{
		QCSAP_IOCTL_DUMP_DP_TRACE_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "dump_dp_trace"
	}
	,
#endif
	{
		QCSAP_ENABLE_FW_PROFILE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "enableProfile"
	}
	,
	{
		QCSAP_SET_FW_PROFILE_HIST_INTVL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "set_hist_intvl"
	}
	,
#ifdef WLAN_SUSPEND_RESUME_TEST
	{
		QCSAP_SET_WLAN_SUSPEND,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "wlan_suspend"
	}
	,
	{
		QCSAP_SET_WLAN_RESUME,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "wlan_resume"
	}
	,
#endif
	{
		QCSAP_SET_BA_AGEING_TIMEOUT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "set_ba_timeout"
	}
	,
	{
		QCASAP_SET_11AX_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_11ax_rate"
	}
	,
	{
		QCASAP_PARAM_DCM,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "enable_dcm"
	}
	,
	{
		QCASAP_PARAM_RANGE_EXT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "range_ext"
	}
	,
	{	QCSAP_SET_DEFAULT_AMPDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "def_ampdu"
	}
	,
	{	QCSAP_ENABLE_RTS_BURSTING,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "rts_bursting"
	}
	,
};

static const iw_handler hostapd_private[] = {
	/* set priv ioctl */
	[QCSAP_IOCTL_SETPARAM - SIOCIWFIRSTPRIV] = iw_softap_setparam,
	/* get priv ioctl */
	[QCSAP_IOCTL_GETPARAM - SIOCIWFIRSTPRIV] = iw_softap_getparam,
	[QCSAP_IOCTL_SET_NONE_GET_THREE - SIOCIWFIRSTPRIV] =
							iw_softap_get_three,
	/* get station genIE */
	[QCSAP_IOCTL_GET_STAWPAIE - SIOCIWFIRSTPRIV] = iw_get_genie,
	/* stop bss */
	[QCSAP_IOCTL_STOPBSS - SIOCIWFIRSTPRIV] = iw_softap_stopbss,
	/* get driver version */
	[QCSAP_IOCTL_VERSION - SIOCIWFIRSTPRIV] = iw_softap_version,
	[QCSAP_IOCTL_GET_CHANNEL - SIOCIWFIRSTPRIV] =
		iw_softap_getchannel,
	[QCSAP_IOCTL_ASSOC_STA_MACADDR - SIOCIWFIRSTPRIV] =
		iw_softap_getassoc_stamacaddr,
	[QCSAP_IOCTL_DISASSOC_STA - SIOCIWFIRSTPRIV] =
		iw_softap_disassoc_sta,
	[QCSAP_PRIV_GET_CHAR_SET_NONE - SIOCIWFIRSTPRIV] =
		iw_get_char_setnone,
	[QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE -
	 SIOCIWFIRSTPRIV] =
		iw_set_three_ints_getnone,
	[QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE -
	 SIOCIWFIRSTPRIV] =
		iw_set_var_ints_getnone,
	[QCSAP_IOCTL_MODIFY_ACL - SIOCIWFIRSTPRIV] =
		iw_softap_modify_acl,
	[QCSAP_IOCTL_GET_CHANNEL_LIST - SIOCIWFIRSTPRIV] =
		iw_softap_get_channel_list,
	[QCSAP_IOCTL_GET_STA_INFO - SIOCIWFIRSTPRIV] =
		iw_softap_get_sta_info,
	[QCSAP_IOCTL_GET_BA_AGEING_TIMEOUT - SIOCIWFIRSTPRIV] =
		iw_softap_get_ba_timeout,
	[QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED -
	 SIOCIWFIRSTPRIV] =
		iw_get_softap_linkspeed,
	[QCSAP_IOCTL_PRIV_GET_RSSI - SIOCIWFIRSTPRIV] =
		iw_get_peer_rssi,
	[QCSAP_IOCTL_SET_TX_POWER - SIOCIWFIRSTPRIV] =
		iw_softap_set_tx_power,
	[QCSAP_IOCTL_SET_MAX_TX_POWER - SIOCIWFIRSTPRIV] =
		iw_softap_set_max_tx_power,
	[QCSAP_IOCTL_SET_PKTLOG - SIOCIWFIRSTPRIV] =
		iw_softap_set_pktlog,
	[QCSAP_IOCTL_GET_INI_CFG - SIOCIWFIRSTPRIV] =
		iw_softap_get_ini_cfg,
	[QCSAP_IOCTL_SET_TWO_INT_GET_NONE - SIOCIWFIRSTPRIV] =
		iw_softap_set_two_ints_getnone,
};

const struct iw_handler_def hostapd_handler_def = {
	.num_standard = 0,
	.num_private = QDF_ARRAY_SIZE(hostapd_private),
	.num_private_args = QDF_ARRAY_SIZE(hostapd_private_args),
	.standard = NULL,
	.private = (iw_handler *)hostapd_private,
	.private_args = hostapd_private_args,
	.get_wireless_stats = NULL,
};

/**
 * hdd_register_wext() - register wext context
 * @dev: net device handle
 *
 * Registers wext interface context for a given net device
 *
 * Returns: 0 on success, errno on failure
 */
void hdd_register_hostapd_wext(struct net_device *dev)
{
	hdd_enter_dev(dev);
	/* Register as a wireless device */
	dev->wireless_handlers = (struct iw_handler_def *)&hostapd_handler_def;

	hdd_exit();
}

