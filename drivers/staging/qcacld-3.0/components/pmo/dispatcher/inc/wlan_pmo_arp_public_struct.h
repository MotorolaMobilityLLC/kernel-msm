/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Declare various struct, macros which shall be used in
 * pmo arp offload feature.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_ARP_PUBLIC_STRUCT_H_
#define _WLAN_PMO_ARP_PUBLIC_STRUCT_H_

#include "wlan_pmo_common_public_struct.h"

/**
 * struct pmo_arp_req - pmo arp request
 * @psoc: objmgr psoc
 * @vdev_id: vdev id on which arp offload needed
 * @ipv4_addr: ipv4 address for the interface
 * @trigger: context from where arp offload triggered
 */
struct pmo_arp_req {
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	uint32_t ipv4_addr;
	enum pmo_offload_trigger trigger;
};

/**
 * struct pmo_arp_req - pmo arp offload param for target interface
 * @enable: true when arp offload is enabled else false
 * @host_ipv4_addr: host interface ipv4 address
 * @bssid: peer ap address
 */
struct pmo_arp_offload_params {
	uint8_t enable;
	uint8_t host_ipv4_addr[QDF_IPV4_ADDR_SIZE];
	struct qdf_mac_addr bssid;
	bool is_offload_applied;
};

#endif /* end  of _WLAN_PMO_ARP_PUBLIC_STRUCT_H_ */
