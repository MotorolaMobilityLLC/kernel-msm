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
 * DOC : wlan_hdd_he.h
 *
 * WLAN Host Device Driver file for 802.11ax (High Efficiency) support.
 *
 */

#if !defined(WLAN_HDD_HE_H)
#define WLAN_HDD_HE_H

struct hdd_context;
struct wma_tgt_cfg;
struct hdd_beacon_data;
struct sap_config;

#ifdef WLAN_FEATURE_11AX
/**
 * enum qca_wlan_vendor_attr_get_he_capabilities - attributes for HE caps.
 *						  vendor command.
 * @QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_INVALID - invalid
 * @QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED - to check if HE capabilities is supported
 * @QCA_WLAN_VENDOR_ATTR_PHY_CAPAB - to get HE PHY capabilities
 * @QCA_WLAN_VENDOR_ATTR_MAC_CAPAB - to get HE MAC capabilities
 * @QCA_WLAN_VENDOR_ATTR_HE_MCS - to get HE MCS
 * @QCA_WLAN_VENDOR_ATTR_NUM_SS - to get NUM SS
 * @QCA_WLAN_VENDOR_ATTR_RU_IDX_MASK - to get RU index mask
 * @QCA_WLAN_VENDOR_ATTR_RU_COUNT - to get RU count,
 * @QCA_WLAN_VENDOR_ATTR_PPE_THRESHOLD - to get PPE Threshold,
 * @QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_AFTER_LAST - next to last valid enum
 * @QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_MAX - max value supported
 *
 * enum values are used for NL attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_GET_HE_CAPABILITIES sub command.
 */
enum qca_wlan_vendor_attr_get_he_capabilities {
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_HE_SUPPORTED,
	QCA_WLAN_VENDOR_ATTR_PHY_CAPAB,
	QCA_WLAN_VENDOR_ATTR_MAC_CAPAB,
	QCA_WLAN_VENDOR_ATTR_HE_MCS,
	QCA_WLAN_VENDOR_ATTR_NUM_SS = 5,
	QCA_WLAN_VENDOR_ATTR_RU_IDX_MASK,
	QCA_WLAN_VENDOR_ATTR_PPE_THRESHOLD,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_MAX =
	QCA_WLAN_VENDOR_ATTR_HE_CAPABILITIES_AFTER_LAST - 1,
};

/**
 * hdd_update_tgt_he_cap() - Update HE related capabilities
 * @hdd_ctx: HDD context
 * @he_cap: Target HE capabilities
 *
 * This function updaates WNI CFG with Target capabilities received as part of
 * Default values present in WNI CFG are the values supported by FW/HW.
 * INI should be introduced if user control is required to control the value.
 *
 * Return: None
 */
void hdd_update_tgt_he_cap(struct hdd_context *hdd_ctx,
			   struct wma_tgt_cfg *cfg);

/**
 * wlan_hdd_check_11ax_support() - check if beacon IE and update hw mode
 * @beacon: beacon IE buffer
 * @config: pointer to sap config
 *
 * Check if HE cap IE is present in beacon IE, if present update hw mode
 * to 11ax.
 *
 * Return: None
 */
void wlan_hdd_check_11ax_support(struct hdd_beacon_data *beacon,
				 struct sap_config *config);

/**
 * hdd_update_he_cap_in_cfg() - update HE cap in global CFG
 * @hdd_ctx: pointer to hdd context
 *
 * This API will update the HE config in CFG after taking intersection
 * of INI and firmware capabilities provided reading CFG
 *
 * Return: 0 on success and errno on failure
 */
int hdd_update_he_cap_in_cfg(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_cfg80211_get_he_cap() - get HE Capabilities
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
int wlan_hdd_cfg80211_get_he_cap(struct wiphy *wiphy,
				 struct wireless_dev *wdev, const void *data,
				 int data_len);
#define FEATURE_11AX_VENDOR_COMMANDS                                    \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_HE_CAPABILITIES,   \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		 WIPHY_VENDOR_CMD_NEED_NETDEV,                          \
	.doit = wlan_hdd_cfg80211_get_he_cap,                           \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                   \
},

#else
static inline void hdd_update_tgt_he_cap(struct hdd_context *hdd_ctx,
					 struct wma_tgt_cfg *cfg)
{
}

static inline void wlan_hdd_check_11ax_support(struct hdd_beacon_data *beacon,
					       struct sap_config *config)
{
}

static inline int hdd_update_he_cap_in_cfg(struct hdd_context *hdd_ctx)
{
	return 0;
}

/* dummy definition */
#define FEATURE_11AX_VENDOR_COMMANDS

#endif
#endif /* if !defined(WLAN_HDD_HE_H)*/
