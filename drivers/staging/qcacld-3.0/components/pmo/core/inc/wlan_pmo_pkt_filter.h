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
 * DOC: Declare packet filter feature API's
 */

#ifndef _WLAN_PMO_PKT_FILTER_H_
#define _WLAN_PMO_PKT_FILTER_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_pkt_filter_public_struct.h"

struct wlan_objmgr_psoc;

/**
 * pmo_get_num_packet_filters() - get the number of packet filters
 * @psoc: the psoc to query
 *
 * Return: number of packet filters
 */
uint32_t pmo_get_num_packet_filters(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_set_pkt_fltr_req() - Set packet filter
 * @psoc: objmgr psoc
 * @pmo_set_pkt_fltr_req: packet filter set param
 * @vdev_id: vdev id
 *
 *  API to set packet filter
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_set_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
			uint8_t vdev_id);

/**
 * pmo_core_clear_pkt_filter() - Clear packet filter
 * @psoc: objmgr psoc
 * @pmo_clr_pkt_fltr_param: packet filter clear param
 * @vdev_id: vdev id
 *
 *  API to clear packet filter
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_clear_pkt_filter(struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* _WLAN_PMO_PKT_FILTER_H_ */

