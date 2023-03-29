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
 * DOC: wlan_hdd_sysfs_dl_modes.h
 *
 * implementation for creating sysfs file dl_modes
 */

#ifndef _WLAN_HDD_SYSFS_DL_MODES_H
#define _WLAN_HDD_SYSFS_DL_MODES_H

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_DL_MODES)
/**
 * wlan_hdd_sysfs_dl_modes_create() - API to create motion
 * detection sysfs attributes
 * @adapter: hdd adapter
 *
 * This API creates the following sysfs attributes:
 * 1. dl_loglevel
 * 2. dl_mod_loglevel
 * 3. dl_modoff
 * 4. dl_modon
 * 5. dl_report
 * 6. dl_type
 * 7. dl_vapoff
 * 8. dl_vapon
 *
 * dl_loglevel: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_loglevel
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_loglevel
 *
 * dl_mod_loglevel: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_mod_loglevel
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_mod_loglevel
 *
 * dl_modoff: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_modoff
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > mt_config
 *
 * dl_modon: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_modon
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_modon
 *
 * dl_report: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_report
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_report
 *
 * dl_type: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_type
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_type
 *
 * dl_vapoff: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_vapoff
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_vapoff
 *
 * dl_vapon: this file is created per adapter.
 * file path: /sys/class/net/wlanxx/dl_vapon
 *                (wlanxx is adapter name)
 * usage:
 *      echo [arg_0] > dl_vapon
 *
 * Return: None
 */
void hdd_sysfs_dl_modes_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_dl_modes_destroy() -
 *   API to destroy dl mode sysfs attributes
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_dl_modes_destroy(struct hdd_adapter *adapter);

#else
static inline void
hdd_sysfs_dl_modes_create(struct hdd_adapter *adapter)
{
}

static inline void
hdd_sysfs_dl_modes_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_DL_MODES_H */
