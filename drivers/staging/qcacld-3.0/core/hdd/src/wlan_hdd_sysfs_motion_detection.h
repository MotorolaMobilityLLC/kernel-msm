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
 * DOC: wlan_hdd_sysfs_motion_detection.h
 *
 * implementation for creating sysfs file motion_detection
 */

#ifndef _WLAN_HDD_SYSFS_MOTION_DETECTION_H
#define _WLAN_HDD_SYSFS_MOTION_DETECTION_H

#if defined(WLAN_SYSFS) && defined(WLAN_FEATURE_MOTION_DETECTION)
/**
 * wlan_hdd_sysfs_motion_detection_create() - API to create motion
 * detection sysfs attributes
 * @adapter: hdd adapter
 *
 * This API creates the following sysfs attributes:
 * 1. mt_bl_config
 * 2. mt_bl_start
 * 3. mt_config
 * 4. mt_start
 *
 * mt_bl_config: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/mt_bl_config
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] [arg_1] [arg_2] [arg_3] > mt_bl_config
 *
 * mt_bl_start: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/mt_bl_start
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > mt_bl_start
 *
 * mt_config: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/mt_config
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] [arg_1] [arg_2] [arg_4] [arg_5] [arg_6] [arg_7] [arg_8]
 *      [arg_9] [arg_10] [arg_11] [arg_12] [arg_13] [arg_14] > mt_config
 *
 * mt_start: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/mt_start
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > mt_start
 *
 * Return: None
 */
void hdd_sysfs_motion_detection_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_motion_detection_destroy() -
 *   API to destroy motion detection sysfs attributes
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_motion_detection_destroy(struct hdd_adapter *adapter);

#else
static inline void
hdd_sysfs_motion_detection_create(struct hdd_adapter *adapter)
{
}

static inline void
hdd_sysfs_motion_detection_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_MOTION_DETECTION_H */
