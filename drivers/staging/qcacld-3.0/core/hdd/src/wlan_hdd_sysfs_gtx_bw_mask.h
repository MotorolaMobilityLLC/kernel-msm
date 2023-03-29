/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_gtx_bw_mask.h
 *
 * implementation for creating sysfs file gtx_bw_mask
 */

#ifndef _WLAN_HDD_SYSFS_GTX_BW_MASK_H
#define _WLAN_HDD_SYSFS_GTX_BW_MASK_H

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_GTX_BW_MASK)
/**
 * wlan_hdd_sysfs_gtx_bw_mask_create() - API to create gtx_bw_mask
 * @adapter: hdd adapter
 *
 * this file is created per adapter.
 * file path: /sys/class/net/wlanxx/gtx_bw_mask
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > gtx_bw_mask
 *      cat gtx_bw_mask
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_gtx_bw_mask_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_gtx_bw_mask_destroy() -
 *   API to destroy gtx_bw_mask
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_gtx_bw_mask_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_gtx_bw_mask_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_gtx_bw_mask_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_GTX_BW_MASK_H */
