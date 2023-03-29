/*
 * Copyright (c) 2012-2018,2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_CONCURRENCY_MATRIX_H
#define __WLAN_HDD_CONCURRENCY_MATRIX_H

/**
 * DOC: wlan_hdd_concurrency_matrix_h
 *
 * WLAN Host Device Driver concurrency matrix API specification
 */

#ifdef FEATURE_CONCURRENCY_MATRIX

extern const struct nla_policy
wlan_hdd_get_concurrency_matrix_policy[
			QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX + 1];

/**
 * wlan_hdd_cfg80211_get_concurrency_matrix() - get concurrency matrix
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Retrieves the concurrency feature set matrix
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len);

#define FEATURE_CONCURRENCY_MATRIX_VENDOR_COMMANDS			\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX,\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,\
	.doit = wlan_hdd_cfg80211_get_concurrency_matrix,		\
	vendor_command_policy(wlan_hdd_get_concurrency_matrix_policy,   \
			      QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX)\
},
#else /* FEATURE_CONCURRENCY_MATRIX */
#define FEATURE_CONCURRENCY_MATRIX_VENDOR_COMMANDS
#endif /* FEATURE_CONCURRENCY_MATRIX */

#endif /* __WLAN_HDD_CONCURRENCY_MATRIX_H */

