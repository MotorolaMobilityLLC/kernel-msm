/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_cfr.c
 *
 * WLAN Host Device Driver CFR capture Implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include "wlan_hdd_includes.h"
#include "osif_sync.h"
#include "wlan_hdd_cfr.h"
#include "wlan_cfr_ucfg_api.h"
#include "wlan_hdd_object_manager.h"

const struct nla_policy cfr_config_policy[
		QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE] = {.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_PERIODIC_CFR_CAPTURE_ENABLE] = {
						.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP] = {
						.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK] = {.type = NLA_U64},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT] = {
						.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE] = {
						.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_ENTRY] = {
						.type = NLA_NESTED},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK] =
		VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER] = {
						.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER] = {
						.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER] = {
						.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE] = {
						.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID] = {
						.type = NLA_U32},
};

static void
wlan_hdd_transport_mode_cfg(struct wlan_objmgr_pdev *pdev,
			    uint8_t vdev_id, uint32_t pid,
			    enum qca_wlan_vendor_cfr_data_transport_modes tx_mode)
{
	struct pdev_cfr *pa;

	if (!pdev) {
		hdd_err("failed to %s transport mode cb for cfr, pdev is NULL for vdev id %d",
			tx_mode ? "register" : "deregister", vdev_id);
		return;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_CFR);
	if (!pa) {
		hdd_err("cfr private obj is NULL for vdev id %d", vdev_id);
		return;
	}
	pa->nl_cb.vdev_id = vdev_id;
	pa->nl_cb.pid = pid;
	if (tx_mode == QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS)
		pa->nl_cb.cfr_nl_cb = hdd_cfr_data_send_nl_event;
	else
		pa->nl_cb.cfr_nl_cb = NULL;
}

#ifdef WLAN_ENH_CFR_ENABLE

#define DEFAULT_CFR_NSS 0xff
#define DEFAULT_CFR_BW  0xf
static QDF_STATUS
wlan_cfg80211_cfr_set_group_config(struct wlan_objmgr_vdev *vdev,
				   struct nlattr *tb[])
{
	struct cfr_wlanconfig_param params = { 0 };

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER]) {
		params.grp_id = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NUMBER]);
		hdd_debug("group_id %d", params.grp_id);
	}

	if (params.grp_id >= HDD_INVALID_GROUP_ID) {
		hdd_err("invalid group id");
		return QDF_STATUS_E_INVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA]) {
		nla_memcpy(&params.ta[0],
			   tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA],
			   QDF_MAC_ADDR_SIZE);
		hdd_debug("ta " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(&params.ta[0]));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK]) {
		nla_memcpy(&params.ta_mask[0],
			   tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TA_MASK],
			   QDF_MAC_ADDR_SIZE);
		hdd_debug("ta_mask " QDF_FULL_MAC_FMT,
			  QDF_FULL_MAC_REF(&params.ta_mask[0]));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA]) {
		nla_memcpy(&params.ra[0],
			   tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA],
			   QDF_MAC_ADDR_SIZE);
		hdd_debug("ra " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(&params.ra[0]));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK]) {
		nla_memcpy(&params.ra_mask[0],
			   tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_RA_MASK],
			   QDF_MAC_ADDR_SIZE);
		hdd_debug("ra_mask " QDF_FULL_MAC_FMT,
			  QDF_FULL_MAC_REF(&params.ra_mask[0]));
	}

	if (!qdf_is_macaddr_zero((struct qdf_mac_addr *)&params.ta) ||
	    !qdf_is_macaddr_zero((struct qdf_mac_addr *)&params.ra) ||
	    !qdf_is_macaddr_zero((struct qdf_mac_addr *)&params.ta_mask) ||
	    !qdf_is_macaddr_zero((struct qdf_mac_addr *)&params.ra_mask)) {
		hdd_debug("set tara config");
		ucfg_cfr_set_tara_config(vdev, &params);
	}

	params.nss = DEFAULT_CFR_NSS;
	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS]) {
		params.nss = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_NSS]);
		hdd_debug("nss %d", params.nss);
	}

	params.bw = DEFAULT_CFR_BW;
	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW]) {
		params.bw = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_BW]);
		hdd_debug("bw %d", params.bw);
	}

	if (params.nss || params.bw) {
		hdd_debug("set bw nss");
		ucfg_cfr_set_bw_nss(vdev, &params);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER]) {
		params.expected_mgmt_subtype = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_MGMT_FILTER]);
		hdd_debug("expected_mgmt_subtype %d(%x)",
			  params.expected_mgmt_subtype,
			  params.expected_mgmt_subtype);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER]) {
		params.expected_ctrl_subtype = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_CTRL_FILTER]);
		hdd_debug("expected_mgmt_subtype %d(%x)",
			  params.expected_ctrl_subtype,
			  params.expected_ctrl_subtype);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER]) {
		params.expected_data_subtype = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_DATA_FILTER]);
		hdd_debug("expected_mgmt_subtype %d(%x)",
			  params.expected_data_subtype,
			  params.expected_data_subtype);
	}

	if (!params.expected_mgmt_subtype ||
	    !params.expected_ctrl_subtype ||
		!params.expected_data_subtype) {
		hdd_debug("set frame type");
		ucfg_cfr_set_frame_type_subtype(vdev, &params);
	}

	return QDF_STATUS_SUCCESS;
}

static enum capture_type convert_vendor_cfr_capture_type(
			enum qca_wlan_vendor_cfr_capture_type type)
{
	switch (type) {
	case QCA_WLAN_VENDOR_CFR_DIRECT_FTM:
		return RCC_DIRECTED_FTM_FILTER;
	case QCA_WLAN_VENDOR_CFR_ALL_FTM_ACK:
		return RCC_ALL_FTM_ACK_FILTER;
	case QCA_WLAN_VENDOR_CFR_DIRECT_NDPA_NDP:
		return RCC_DIRECTED_NDPA_NDP_FILTER;
	case QCA_WLAN_VENDOR_CFR_TA_RA:
		return RCC_TA_RA_FILTER;
	case QCA_WLAN_VENDOR_CFR_ALL_PACKET:
		return RCC_NDPA_NDP_ALL_FILTER;
	default:
		hdd_err("invalid capture type");
		return RCC_DIS_ALL_MODE;
	}
}

static int
wlan_cfg80211_cfr_set_config(struct wlan_objmgr_vdev *vdev,
			     struct nlattr *tb[])
{
	struct nlattr *group[QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1];
	struct nlattr *group_list;
	struct cfr_wlanconfig_param params = { 0 };
	enum capture_type type;
	enum qca_wlan_vendor_cfr_capture_type vendor_capture_type;
	int rem = 0;
	int maxtype;
	int attr;
	uint64_t ul_mu_user_mask = 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION]) {
		params.cap_dur = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_DURATION]);
		ucfg_cfr_set_capture_duration(vdev, &params);
		hdd_debug("params.cap_dur %d", params.cap_dur);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL]) {
		params.cap_intvl = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_INTERVAL]);
		ucfg_cfr_set_capture_interval(vdev, &params);
		hdd_debug("params.cap_intvl %d", params.cap_intvl);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE]) {
		vendor_capture_type = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_CAPTURE_TYPE]);
		if ((vendor_capture_type < QCA_WLAN_VENDOR_CFR_DIRECT_FTM) ||
		    (vendor_capture_type > QCA_WLAN_VENDOR_CFR_ALL_PACKET)) {
			hdd_err_rl("invalid capture type %d",
				   vendor_capture_type);
			return -EINVAL;
		}
		type = convert_vendor_cfr_capture_type(vendor_capture_type);
		ucfg_cfr_set_rcc_mode(vdev, type, 1);
		hdd_debug("type %d", type);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK]) {
		ul_mu_user_mask = nla_get_u64(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_UL_MU_MASK]);
		hdd_debug("ul_mu_user_mask_lower %d",
			  params.ul_mu_user_mask_lower);
	}

	if (ul_mu_user_mask) {
		params.ul_mu_user_mask_lower =
				(uint32_t)(ul_mu_user_mask & 0xffffffff);
		params.ul_mu_user_mask_lower =
				(uint32_t)(ul_mu_user_mask >> 32);
		hdd_debug("set ul mu user maks");
		ucfg_cfr_set_ul_mu_user_mask(vdev, &params);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT]) {
		params.freeze_tlv_delay_cnt_thr = nla_get_u32(tb[
		QCA_WLAN_VENDOR_ATTR_PEER_CFR_FREEZE_TLV_DELAY_COUNT]);
		if (params.freeze_tlv_delay_cnt_thr) {
			params.freeze_tlv_delay_cnt_en = 1;
			ucfg_cfr_set_freeze_tlv_delay_cnt(vdev, &params);
			hdd_debug("freeze_tlv_delay_cnt_thr %d",
				  params.freeze_tlv_delay_cnt_thr);
		}
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE]) {
		maxtype = QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX;
		attr = QCA_WLAN_VENDOR_ATTR_PEER_CFR_GROUP_TABLE;
		nla_for_each_nested(group_list, tb[attr], rem) {
			if (wlan_cfg80211_nla_parse(group, maxtype,
						    nla_data(group_list),
						    nla_len(group_list),
						    cfr_config_policy)) {
				hdd_err("nla_parse failed for cfr config group");
				return -EINVAL;
			}
			wlan_cfg80211_cfr_set_group_config(vdev, group);
		}
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE]) {
		uint8_t transport_mode = 0xff;
		uint32_t pid = 0;

		if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID])
			pid = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_RECEIVER_PID]);
		else
			hdd_debug("No PID received");

		transport_mode = nla_get_u8(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_DATA_TRANSPORT_MODE]);

		hdd_debug("tx mode attr %d, pid %d", transport_mode, pid);
		if (transport_mode == QCA_WLAN_VENDOR_CFR_DATA_RELAY_FS ||
		    transport_mode == QCA_WLAN_VENDOR_CFR_DATA_NETLINK_EVENTS) {
			wlan_hdd_transport_mode_cfg(vdev->vdev_objmgr.wlan_pdev,
						    vdev->vdev_objmgr.vdev_id,
						    pid, transport_mode);
		} else {
			hdd_debug("invalid transport mode %d for vdev id %d",
				  transport_mode, vdev->vdev_objmgr.vdev_id);
		}
	}

	return 0;
}

#ifdef WLAN_CFR_ADRASTEA
static QDF_STATUS
wlan_cfg80211_peer_cfr_capture_cfg_adrastea(struct hdd_adapter *adapter,
					    struct nlattr **tb)
{
	struct cfr_capture_params params = { 0 };
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc;
	struct qdf_mac_addr peer_addr;
	bool is_start_capture = false;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!tb[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR]) {
		hdd_err("peer mac addr not given");
		return QDF_STATUS_E_INVAL;
	}

	nla_memcpy(peer_addr.bytes, tb[QCA_WLAN_VENDOR_ATTR_CFR_PEER_MAC_ADDR],
		   QDF_MAC_ADDR_SIZE);

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE]) {
		is_start_capture = nla_get_flag(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE]);
	}

	vdev = adapter->vdev;
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_CFR_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get vdev");
		return status;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		hdd_err("failed to get pdev");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CFR_ID);
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		hdd_err("Failed to get psoc");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CFR_ID);
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_addr.bytes, WLAN_CFR_ID);
	if (!peer) {
		hdd_err("No peer object found");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CFR_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (is_start_capture) {
		if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY]) {
			params.period = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_PEER_CFR_PERIODICITY]);
			hdd_debug("params.periodicity %d", params.period);
			/* Set the periodic CFR */
			if (params.period)
				ucfg_cfr_set_timer(pdev, params.period);
		}

		if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD]) {
			params.method = nla_get_u8(tb[
				QCA_WLAN_VENDOR_ATTR_PEER_CFR_METHOD]);
			/* Adrastea supports only QOS NULL METHOD */
			if (params.method !=
					QCA_WLAN_VENDOR_CFR_METHOD_QOS_NULL) {
				hdd_err_rl("invalid capture method %d",
					   params.method);
				status = QDF_STATUS_E_INVAL;
				goto exit;
			}
		}

		if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH]) {
			params.bandwidth = nla_get_u8(tb[
				QCA_WLAN_VENDOR_ATTR_PEER_CFR_BANDWIDTH]);
			/* Adrastea supports only 20Mhz bandwidth CFR capture */
			if (params.bandwidth != NL80211_CHAN_WIDTH_20_NOHT) {
				hdd_err_rl("invalid capture bandwidth %d",
					   params.bandwidth);
				status = QDF_STATUS_E_INVAL;
				goto exit;
			}
		}
		ucfg_cfr_start_capture(pdev, peer, &params);
	} else {
		/* Disable the periodic CFR if enabled */
		if (ucfg_cfr_get_timer(pdev))
			ucfg_cfr_set_timer(pdev, 0);

		/* Disable the peer CFR capture */
		ucfg_cfr_stop_capture(pdev, peer);
	}
exit:
	wlan_objmgr_peer_release_ref(peer, WLAN_CFR_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CFR_ID);

	return status;
}
#else
static QDF_STATUS
wlan_cfg80211_peer_cfr_capture_cfg_adrastea(struct hdd_adapter *adapter,
					    struct nlattr **tb)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

static QDF_STATUS hdd_stop_enh_cfr(struct wlan_objmgr_vdev *vdev)
{
	if (!ucfg_cfr_get_rcc_enabled(vdev))
		return QDF_STATUS_SUCCESS;

	hdd_debug("cleanup rcc mode");
	wlan_objmgr_vdev_try_get_ref(vdev, WLAN_CFR_ID);
	ucfg_cfr_set_rcc_mode(vdev, RCC_DIS_ALL_MODE, 0);
	ucfg_cfr_subscribe_ppdu_desc(wlan_vdev_get_pdev(vdev),
				     false);
	ucfg_cfr_committed_rcc_config(vdev);
	ucfg_cfr_stop_indication(vdev);
	ucfg_cfr_suspend(wlan_vdev_get_pdev(vdev));
	hdd_debug("stop indication done");
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CFR_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_cfr_disconnect(struct wlan_objmgr_vdev *vdev)
{
	return hdd_stop_enh_cfr(vdev);
}

static int
wlan_cfg80211_peer_enh_cfr_capture(struct hdd_adapter *adapter,
				   struct nlattr **tb)
{
	struct cfr_wlanconfig_param params = { 0 };
	struct wlan_objmgr_vdev *vdev;
	bool is_start_capture = false;
	int ret = 0;

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE]) {
		is_start_capture = nla_get_flag(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE]);
	}

	if (is_start_capture &&
	    !tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP]) {
		hdd_err("Invalid group bitmap");
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}

	if (is_start_capture) {
		ret = wlan_cfg80211_cfr_set_config(vdev, tb);
		if (ret) {
			hdd_err("set config failed");
			goto out;
		}
		params.en_cfg = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_ENABLE_GROUP_BITMAP]);
		hdd_debug("params.en_cfg %d", params.en_cfg);
		ucfg_cfr_set_en_bitmap(vdev, &params);
		ucfg_cfr_resume(wlan_vdev_get_pdev(vdev));
		ucfg_cfr_subscribe_ppdu_desc(wlan_vdev_get_pdev(vdev),
					     true);
		ucfg_cfr_committed_rcc_config(vdev);
	} else {
		hdd_stop_enh_cfr(vdev);
	}

out:
	hdd_objmgr_put_vdev(vdev);
	return ret;
}
#else
static int
wlan_cfg80211_peer_enh_cfr_capture(struct hdd_adapter *adapter,
				   struct nlattr **tb)
{
	return 0;
}
#endif

static int
wlan_cfg80211_peer_cfr_capture_cfg(struct wiphy *wiphy,
				   struct hdd_adapter *adapter,
				   const void *data,
				   int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1];
	uint8_t version = 0;
	QDF_STATUS status;

	if (wlan_cfg80211_nla_parse(
			tb,
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX,
			data,
			data_len,
			cfr_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION]) {
		version = nla_get_u8(tb[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_VERSION]);
		hdd_debug("version %d", version);
		if (version == LEGACY_CFR_VERSION) {
			status = wlan_cfg80211_peer_cfr_capture_cfg_adrastea(
								adapter, tb);
			return qdf_status_to_os_return(status);
		} else if (version != ENHANCED_CFR_VERSION) {
			hdd_err("unsupported version");
			return -EFAULT;
		}
	}

	return wlan_cfg80211_peer_enh_cfr_capture(adapter, tb);
}

static int __wlan_hdd_cfg80211_peer_cfr_capture_cfg(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    const void *data,
						    int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	wlan_cfg80211_peer_cfr_capture_cfg(wiphy, adapter,
					   data, data_len);

	hdd_exit();

	return ret;
}

int wlan_hdd_cfg80211_peer_cfr_capture_cfg(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_peer_cfr_capture_cfg(wiphy, wdev,
							 data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}
