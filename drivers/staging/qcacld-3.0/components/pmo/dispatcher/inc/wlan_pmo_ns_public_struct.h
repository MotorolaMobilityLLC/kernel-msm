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
 * pmo ns offload feature.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_NS_PUBLIC_STRUCT_H_
#define _WLAN_PMO_NS_PUBLIC_STRUCT_H_

#include "wlan_pmo_common_public_struct.h"

/**
 * enum pmo_ns_addr_scope - Internal identification of IPv6 addr scope
 * @PMO_NS_ADDR_SCOPE_INVALID: invalid scope
 * @PMO_NS_ADDR_SCOPE_NODELOCAL: node local scope
 * @PMO_NS_ADDR_SCOPE_LINKLOCAL: link local scope
 * @PMO_NS_ADDR_SCOPE_SITELOCAL: site local scope
 * @PMO_NS_ADDR_SCOPE_ORGLOCAL: org local scope
 * @PMO_NS_ADDR_SCOPE_GLOBAL: global scope
 */
enum pmo_ns_addr_scope {
	PMO_NS_ADDR_SCOPE_INVALID = 0,
	PMO_NS_ADDR_SCOPE_NODELOCAL = 1,
	PMO_NS_ADDR_SCOPE_LINKLOCAL = 2,
	PMO_NS_ADDR_SCOPE_SITELOCAL = 3,
	PMO_NS_ADDR_SCOPE_ORGLOCAL = 4,
	PMO_NS_ADDR_SCOPE_GLOBAL = 5
};

/**
 * struct pmo_ns_offload_params - pmo ns offload parameters
 * @enable: true when ns offload enable
 * @num_ns_offload_count: total ns entries
 * @src_ipv6_addr: in request source ipv 6 address
 * @self_ipv6_addr: self ipv6 address
 * @target_ipv6_addr: target ipv6 address
 * @self_macaddr: self mac address
 * @src_ipv6_addr_valid: true if source ipv6 address is valid else false
 * @target_ipv6_addr_valid: target ipv6 address are valid or not
 * @target_ipv6_addr_ac_type: target ipv6 address type (unicast or anycast)
 * @slot_idx: slot index
 */
struct pmo_ns_offload_params {
	uint8_t enable;
	uint32_t num_ns_offload_count;
	uint8_t src_ipv6_addr[QDF_IPV6_ADDR_SIZE];
	uint8_t self_ipv6_addr[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA]
		[QDF_IPV6_ADDR_SIZE];
	uint8_t target_ipv6_addr[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA]
		[QDF_IPV6_ADDR_SIZE];
	struct qdf_mac_addr self_macaddr;
	uint8_t src_ipv6_addr_valid;
	uint8_t target_ipv6_addr_valid[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
	uint8_t target_ipv6_addr_ac_type[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
	uint8_t slot_idx;
	struct qdf_mac_addr bssid;
	enum pmo_ns_addr_scope scope[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
	bool is_offload_applied;
};

/**
 * struct pmo_ns_req - pmo ns request
 * @psoc: objmgr psoc
 * @vdev_id: vdev id on which arp offload needed
 * @trigger: context from where arp offload triggered
 * @count: ns entries count
 * @ipv6_addr: ipv6 address array
 * @ipv6_addr_type: ipv6 address type (unicast/anycast) array
 */
struct pmo_ns_req {
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	enum pmo_offload_trigger trigger;
	uint32_t count;
	uint8_t ipv6_addr[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA]
				[QDF_IPV6_ADDR_SIZE];
	uint8_t ipv6_addr_type[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
	enum pmo_ns_addr_scope scope[PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA];
};
#endif /* end  of _WLAN_PMO_NS_PUBLIC_STRUCT_H_ */
