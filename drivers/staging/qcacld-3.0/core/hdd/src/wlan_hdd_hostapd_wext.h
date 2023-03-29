/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_HDD_HOSTAPD_WEXT_H
#define WLAN_HDD_HOSTAPD_WEXT_H

/**
 * DOC: wlan_hdd_hostapd_wext.h
 *
 * WLAN Host Device driver hostapd wext header file
 */

/* Include files */

#include <linux/netdevice.h>

/* Preprocessor definitions and constants */
#ifdef WLAN_WEXT_SUPPORT_ENABLE
void hdd_register_hostapd_wext(struct net_device *dev);
#else
static inline void hdd_register_hostapd_wext(struct net_device *dev)
{
}
#endif

#endif /* end #ifndef WLAN_HDD_HOSTAPD_H */
