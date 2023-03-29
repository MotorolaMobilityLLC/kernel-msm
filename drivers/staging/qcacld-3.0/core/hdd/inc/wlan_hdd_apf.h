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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * DOC: wlan_hdd_apf.h
 *
 * Android Packet Filter related API's and definitions
 */

#ifndef __WLAN_HDD_APF_H
#define __WLAN_HDD_APF_H

#ifdef FEATURE_WLAN_APF

#include <net/cfg80211.h>
#include "sir_api.h"
#include "wlan_hdd_main.h"
#include "wmi_unified_param.h"
#include "qca_vendor.h"

#define APF_CONTEXT_MAGIC 0x4575354

#define MAX_APF_MEMORY_LEN	4096

/* APF commands wait times in msec */
#define WLAN_WAIT_TIME_APF_READ_MEM     10000

/* QCA_NL80211_VENDOR_PACKET_FILTER policy */
extern const struct nla_policy wlan_hdd_apf_offload_policy[
			QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_MAX + 1];

#define FEATURE_APF_OFFLOAD_VENDOR_COMMANDS \
	{ \
		.info.vendor_id = QCA_NL80211_VENDOR_ID, \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_PACKET_FILTER, \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
			WIPHY_VENDOR_CMD_NEED_NETDEV | \
			WIPHY_VENDOR_CMD_NEED_RUNNING, \
		.doit = wlan_hdd_cfg80211_apf_offload, \
		vendor_command_policy(wlan_hdd_apf_offload_policy, \
				      QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_MAX) \
	},

/**
 * wlan_hdd_cfg80211_apf_offload() - SSR Wrapper to APF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */

int wlan_hdd_cfg80211_apf_offload(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len);

/**
 * hdd_apf_context_init - APF Context initialization operations
 * @adapter: hdd adapter
 *
 * Return: None
 */
void hdd_apf_context_init(struct hdd_adapter *adapter);

/**
 * hdd_apf_context_destroy - APF Context de-init operations
 * @adapter: hdd adapter
 *
 * Return: None
 */
void hdd_apf_context_destroy(struct hdd_adapter *adapter);

/**
 * hdd_get_apf_capabilities_cb() - Callback function to get APF capabilities
 * @hdd_context: pointer to the hdd context
 * @apf_get_offload: struct for get offload
 *
 * This function receives the response/data from the lower layer and
 * checks to see if the thread is still waiting then post the results to
 * upper layer, if the request has timed out then ignore.
 *
 * Return: None
 */
void hdd_get_apf_capabilities_cb(void *hdd_context,
				 struct sir_apf_get_offload *data);
#else /* FEATURE_WLAN_APF */

#define FEATURE_APF_OFFLOAD_VENDOR_COMMANDS

static inline void hdd_apf_context_init(struct hdd_adapter *adapter)
{
}

static inline void hdd_apf_context_destroy(struct hdd_adapter *adapter)
{
}

#endif /* FEATURE_WLAN_APF */

#endif /* WLAN_HDD_APF_H */
