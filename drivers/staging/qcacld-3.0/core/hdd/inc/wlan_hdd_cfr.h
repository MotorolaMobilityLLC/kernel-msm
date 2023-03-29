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
 * DOC : wlan_hdd_cfr.h
 *
 * WLAN Host Device Driver cfr capture implementation
 *
 */

#if !defined(_WLAN_HDD_CFR_H)
#define _WLAN_HDD_CFR_H

#ifdef WLAN_CFR_ENABLE

#include "wlan_cfr_utils_api.h"

#define HDD_INVALID_GROUP_ID MAX_TA_RA_ENTRIES
#define LEGACY_CFR_VERSION 1
#define ENHANCED_CFR_VERSION 2

/**
 * wlan_hdd_cfg80211_peer_cfr_capture_cfg() - configure peer cfr capture
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts CFR capture
 *
 * Return: 0 on success and errno on failure
 */
int
wlan_hdd_cfg80211_peer_cfr_capture_cfg(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len);

#ifdef WLAN_ENH_CFR_ENABLE
/**
 * hdd_cfr_disconnect() - Handle disconnection event in CFR
 * @vdev: Pointer to vdev object
 *
 * Handle disconnection event in CFR. Stop CFR if it started and get
 * disconnection event.
 *
 * Return: QDF status
 */
QDF_STATUS hdd_cfr_disconnect(struct wlan_objmgr_vdev *vdev);
#else
static inline QDF_STATUS
hdd_cfr_disconnect(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif
extern const struct nla_policy cfr_config_policy[
			QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX + 1];

#define FEATURE_CFR_VENDOR_COMMANDS \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PEER_CFR_CAPTURE_CFG, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
			WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_peer_cfr_capture_cfg, \
	vendor_command_policy(cfr_config_policy, \
			      QCA_WLAN_VENDOR_ATTR_PEER_CFR_MAX) \
},
#else
#define FEATURE_CFR_VENDOR_COMMANDS
static inline QDF_STATUS
hdd_cfr_disconnect(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_CFR_ENABLE */
#endif /* _WLAN_HDD_CFR_H */

