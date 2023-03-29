/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_monitor_mode_channel.h
 *
 * Implementation for creating sysfs file monitor_mode_channel
 */

#ifndef _WLAN_HDD_SYSFS_MONITOR_MODE_CHANNEL_H
#define _WLAN_HDD_SYSFS_MONITOR_MODE_CHANNEL_H

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_SYSFS_MONITOR_MODE_CHANNEL)
/**
 * hdd_sysfs_monitor_mode_channel_create() -
 *  API to create monitor_mode_channel
 * @adapter: hdd adapter
 *
 * this file is created per adapter.
 * file path: /sys/class/net/wlanxx/monitor_mode_channel
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] [arg_1] > monitor_mode_channel
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_monitor_mode_channel_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_monitor_mode_channel_destroy() -
 *   API to destroy monitor_mode_channel sysfs file
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_monitor_mode_channel_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_monitor_mode_channel_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_monitor_mode_channel_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_MONITOR_MODE_CHANNEL_H */
