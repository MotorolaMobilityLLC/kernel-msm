/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_mpta_helper.h
 *
 * Add Vendor subcommand QCA_NL80211_VENDOR_SUBCMD_MPTA_HELPER_CONFIG
 */

#ifndef __WLAN_HDD_MPTA_HELPER_H
#define __WLAN_HDD_MPTA_HELPER_H

#include "wlan_fw_offload_main.h"

#ifdef FEATURE_MPTA_HELPER
#include <net/cfg80211.h>

/**
 * wlan_hdd_mpta_helper_enable() - enable/disable mpta helper
 * according to cfg from INI
 * @coex_cfg_params: pointer of coex config command params
 * @config: pointer of BTC config items
 *
 * Return: 0 on success; error number otherwise.
 *
 */
int
wlan_hdd_mpta_helper_enable(struct coex_config_params *coex_cfg_params,
			    struct wlan_fwol_coex_config *config);

/**
 * wlan_hdd_cfg80211_mpta_helper_config() - update
 * tri-radio coex status by mpta helper
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 on success; error number otherwise.
 *
 */
int wlan_hdd_cfg80211_mpta_helper_config(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len);

extern const struct nla_policy
qca_wlan_vendor_mpta_helper_attr[QCA_MPTA_HELPER_VENDOR_ATTR_MAX + 1];

#define FEATURE_MPTA_HELPER_COMMANDS				\
{								\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,		\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MPTA_HELPER_CONFIG,\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			\
		WIPHY_VENDOR_CMD_NEED_NETDEV |			\
		WIPHY_VENDOR_CMD_NEED_RUNNING,			\
	.doit = wlan_hdd_cfg80211_mpta_helper_config,	\
	vendor_command_policy(qca_wlan_vendor_mpta_helper_attr, \
			      QCA_MPTA_HELPER_VENDOR_ATTR_MAX)  \
},

#else /* FEATURE_MPTA_HELPER */
#define FEATURE_MPTA_HELPER_COMMANDS

static inline int
wlan_hdd_mpta_helper_enable(struct coex_config_params *coex_cfg_params,
			    struct wlan_fwol_coex_config *config)
{
	return 0;
}

#endif /* FEATURE_MPTA_HELPER */

#endif /* __WLAN_HDD_MPTA_HELPER_H */

