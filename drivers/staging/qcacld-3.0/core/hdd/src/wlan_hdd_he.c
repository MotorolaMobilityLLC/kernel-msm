/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_he.c
 *
 * WLAN Host Device Driver file for 802.11ax (High Efficiency) support.
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_he.h"
#include "osif_sync.h"
#include "wma_he.h"
#include "wlan_utility.h"
#include "wlan_mlme_ucfg_api.h"

void hdd_update_tgt_he_cap(struct hdd_context *hdd_ctx,
			   struct wma_tgt_cfg *cfg)
{
	QDF_STATUS status;
	tDot11fIEhe_cap he_cap_ini = {0};
	uint8_t value = 0;

	ucfg_mlme_update_tgt_he_cap(hdd_ctx->psoc, cfg);

	status = ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp(hdd_ctx->psoc,
							&value);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("unable to get tx_bfee_ant_supp");

	he_cap_ini.bfee_sts_lt_80 = value;
	sme_update_tgt_he_cap(hdd_ctx->mac_handle, cfg, &he_cap_ini);
}

void wlan_hdd_check_11ax_support(struct hdd_beacon_data *beacon,
				 struct sap_config *config)
{
	const uint8_t *ie;

	ie = wlan_get_ext_ie_ptr_from_ext_id(HE_CAP_OUI_TYPE, HE_CAP_OUI_SIZE,
					    beacon->tail, beacon->tail_len);
	if (ie)
		config->SapHw_mode = eCSR_DOT11_MODE_11ax;
}

int hdd_update_he_cap_in_cfg(struct hdd_context *hdd_ctx)
{
	uint32_t val;
	uint32_t val1 = 0;
	QDF_STATUS status;
	int ret;
	uint8_t enable_ul_ofdma, enable_ul_mimo;

	status = ucfg_mlme_cfg_get_he_ul_mumimo(hdd_ctx->psoc, &val);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("could not get CFG_HE_UL_MUMIMO");
		return qdf_status_to_os_return(status);
	}

	/* In val,
	 * Bit 1 - corresponds to UL MIMO
	 * Bit 2 - corresponds to UL OFDMA
	 */
	ret = ucfg_mlme_cfg_get_enable_ul_mimo(hdd_ctx->psoc,
					       &enable_ul_mimo);
	if (ret)
		return ret;
	ret = ucfg_mlme_cfg_get_enable_ul_ofdm(hdd_ctx->psoc,
					       &enable_ul_ofdma);
	if (ret)
		return ret;
	if (val & 0x1 || (val >> 1) & 0x1)
		val1 = enable_ul_mimo & 0x1;

	if ((val >> 1) & 0x1)
		val1 |= ((enable_ul_ofdma & 0x1) << 1);

	ret = ucfg_mlme_cfg_set_he_ul_mumimo(hdd_ctx->psoc, val1);

	return ret;
}

/*
 * __wlan_hdd_cfg80211_get_he_cap() - get HE Capabilities
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
static int
__wlan_hdd_cfg80211_get_he_cap(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data,
			       int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int ret;
	QDF_STATUS status;
	struct sk_buff *reply_skb;
	uint32_t nl_buf_len;
	struct he_capability he_cap;
	uint8_t he_supported = 0;

	hdd_enter();
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	nl_buf_len = NLMSG_HDRLEN;
	if (sme_is_feature_supported_by_fw(DOT11AX)) {
		he_supported = 1;

		status = wma_get_he_capabilities(&he_cap);
		if (QDF_STATUS_SUCCESS != status)
			return -EINVAL;
	} else {
		hdd_info("11AX: HE not supported, send only QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED");
	}

	if (he_supported) {
		nl_buf_len += NLA_HDRLEN + sizeof(he_supported) +
			      NLA_HDRLEN + sizeof(he_cap.phy_cap) +
			      NLA_HDRLEN + sizeof(he_cap.mac_cap) +
			      NLA_HDRLEN + sizeof(he_cap.mcs) +
			      NLA_HDRLEN + sizeof(he_cap.ppet.numss_m1) +
			      NLA_HDRLEN + sizeof(he_cap.ppet.ru_bit_mask) +
			      NLA_HDRLEN +
				sizeof(he_cap.ppet.ppet16_ppet8_ru3_ru0);
	} else {
		nl_buf_len += NLA_HDRLEN + sizeof(he_supported);
	}

	hdd_info("11AX: he_supported: %d", he_supported);

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, nl_buf_len);

	if (!reply_skb) {
		hdd_err("Allocate reply_skb failed");
		return -EINVAL;
	}

	if (nla_put_u8(reply_skb,
		       QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED, he_supported))
		goto nla_put_failure;

	/* No need to populate other attributes if HE is not supported */
	if (0 == he_supported)
		goto end;

	if (nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_MAC_CAPAB, he_cap.mac_cap) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_HE_MCS, he_cap.mcs) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_NUM_SS, he_cap.ppet.numss_m1) ||
	    nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_RU_IDX_MASK,
			he_cap.ppet.ru_bit_mask) ||
	    nla_put(reply_skb,
		    QCA_WLAN_VENDOR_ATTR_PHY_CAPAB,
		    sizeof(u32) * HE_MAX_PHY_CAP_SIZE, he_cap.phy_cap) ||
	    nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_PPE_THRESHOLD,
		    sizeof(u32) * PSOC_HOST_MAX_NUM_SS,
		    he_cap.ppet.ppet16_ppet8_ru3_ru0))
		goto nla_put_failure;
end:
	ret = cfg80211_vendor_cmd_reply(reply_skb);
	hdd_exit();
	return ret;

nla_put_failure:
	hdd_err("nla put fail");
	kfree_skb(reply_skb);
	return -EINVAL;
}

int wlan_hdd_cfg80211_get_he_cap(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_he_cap(wiphy, wdev, data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}
