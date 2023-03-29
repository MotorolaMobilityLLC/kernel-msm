/*
 * Copyright (c) 2014-2017, 2019 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_HOST_OFFLOAD_H__
#define __WLAN_HDD_HOST_OFFLOAD_H__

/**
 * DOC: wlan_hdd_host_offload.h
 *
 * Android WLAN HDD Host Offload API
 */

/* Offload types. */
#define WLAN_IPV4_ARP_REPLY_OFFLOAD           0
#define WLAN_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD  1

/* Enable or disable offload. */
#define WLAN_OFFLOAD_DISABLE                     0
#define WLAN_OFFLOAD_ENABLE                      0x1
#define WLAN_OFFLOAD_BC_FILTER_ENABLE            0x2
#define WLAN_OFFLOAD_ARP_AND_BC_FILTER_ENABLE    \
			(WLAN_OFFLOAD_ENABLE | WLAN_OFFLOAD_BC_FILTER_ENABLE)

/* Offload request. */
struct host_offload_req {
	uint8_t offloadType;
	uint8_t enableOrDisable;
	union {
		uint8_t hostIpv4Addr[QDF_IPV4_ADDR_SIZE];
		uint8_t hostIpv6Addr[QDF_IPV6_ADDR_SIZE];
	} params;
	struct qdf_mac_addr bssId;
};

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void hdd_wlan_offload_event(uint8_t type, uint8_t state);
#else
static inline
void hdd_wlan_offload_event(uint8_t type, uint8_t state)
{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#endif /* __WLAN_HDD_HOST_OFFLOAD_H__ */
