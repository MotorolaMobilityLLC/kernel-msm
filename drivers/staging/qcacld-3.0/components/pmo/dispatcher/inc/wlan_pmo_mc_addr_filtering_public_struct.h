/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
 * pmo mc address filterign related features.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_MC_ADDR_FILTERING_STRUCT_H_
#define _WLAN_PMO_MC_ADDR_FILTERING_STRUCT_H_

#include "wlan_pmo_common_public_struct.h"

#define PMO_MAX_MC_ADDR_LIST 32
#define PMO_MAX_NUM_MULTICAST_ADDRESS 240

/**
 * struct pmo_mc_addr_list_params -pmo mc address list request params
 * @psoc: objmgr psoc
 * @vdev_id: vdev id on which arp offload needed
 * @count: multicast address count
 * @mc_addr: multicast address array
 */
struct pmo_mc_addr_list_params {
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	uint8_t count;
	struct qdf_mac_addr mc_addr[PMO_MAX_MC_ADDR_LIST];
};

/**
 * struct pmo_mc_addr_list -pmo mc address list params for vdev
 * @is_filter_applied: is mc list filter applied on vdev
 * @mc_cnt: mc address count
 * @mc_addr:mc address list
 */
struct pmo_mc_addr_list {
	uint8_t is_filter_applied;
	uint8_t mc_cnt;
	struct qdf_mac_addr mc_addr[PMO_MAX_MC_ADDR_LIST];
};

/**
 * struct mcast_filter_params - mcast filter parameters
 * @multicast_addr_cnt: num of addresses
 * @multicast_addr: address array
 * @action: operation to perform
 */
struct pmo_mcast_filter_params {
	uint32_t multicast_addr_cnt;
	struct qdf_mac_addr multicast_addr[PMO_MAX_NUM_MULTICAST_ADDRESS];
	uint8_t action;
};
#endif /* end  of _WLAN_PMO_MC_ADDR_FILTERING_STRUCT_H_ */
