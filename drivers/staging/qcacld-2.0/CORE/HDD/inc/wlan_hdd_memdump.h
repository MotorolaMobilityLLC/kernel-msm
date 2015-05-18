/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * DOC : wlan_hdd_memdump.h
 *
 * WLAN Host Device Driver file for dumping firmware memory
 *
 */

#if !defined(WLAN_HDD_MEMDUMP_H)
#define WLAN_HDD_MEMDUMP_H

#include "wlan_hdd_main.h"

#ifdef WLAN_FEATURE_MEMDUMP
/**
 * enum qca_wlan_vendor_attr_memory_dump - values for memory dump attributes
 * @QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_INVALID - Invalid
 * @QCA_WLAN_VENDOR_ATTR_REQUEST_ID - Indicate request ID
 * @QCA_WLAN_VENDOR_ATTR_MEMDUMP_SIZE - Indicate size of the memory dump
 * @QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_AFTER_LAST - To keep track of the last enum
 * @QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_MAX - max value possible for this type
 *
 * enum values are used for NL attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP sub command.
 */
enum qca_wlan_vendor_attr_memory_dump {
	QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_REQUEST_ID = 1,
	QCA_WLAN_VENDOR_ATTR_MEMDUMP_SIZE = 2,

	QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_MAX =
		QCA_WLAN_VENDOR_ATTR_MEMORY_DUMP_AFTER_LAST - 1,
};

/* Size of fw memory dump is estimated to be 327680 bytes */
#define FW_MEM_DUMP_SIZE    327680
#define FW_DRAM_LOCATION    0x00400000
#define FW_MEM_DUMP_REQ_ID  1
#define FW_MEM_DUMP_NUM_SEG 1
#define MEMDUMP_COMPLETION_TIME_MS 5000

int memdump_init(void);
void memdump_deinit(void);
int wlan_hdd_cfg80211_get_fw_mem_dump(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len);
#else
static inline int memdump_init(void)
{
	return -ENOTSUPP;
}

static inline void memdump_deinit(void)
{
}

static inline int wlan_hdd_cfg80211_get_fw_mem_dump(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	return -ENOTSUPP;
}
#endif

#endif /* if !defined(WLAN_HDD_MEMDUMP_H)*/
