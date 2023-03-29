/*
 * Copyright (c) 2018, 2019 The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains ipa public structure definitions
 */

#ifndef _WLAN_IPA_PUBLIC_STRUCT_H_
#define _WLAN_IPA_PUBLIC_STRUCT_H_

#include <wlan_cmn.h>
#include <qdf_status.h>
#include <wlan_objmgr_psoc_obj.h>

/**
 * struct wlan_ipa_config
 * @ipa_config: IPA config
 * @desc_size: IPA descriptor size
 * @txbuf_count: TX buffer count
 * @bus_bw_high: Bus bandwidth high threshold
 * @bus_bw_medium: Bus bandwidth medium threshold
 * @bus_bw_low: Bus bandwidth low threshold
 * @ipa_bw_high: IPA bandwidth high threshold
 * @ipa_bw_medium: IPA bandwidth medium threshold
 * @ipa_bw_low: IPA bandwidth low threshold
 * @ipa_force_voting: support force bw voting
 */
struct wlan_ipa_config {
	uint32_t ipa_config;
	uint32_t desc_size;
	uint32_t txbuf_count;
	uint32_t bus_bw_high;
	uint32_t bus_bw_medium;
	uint32_t bus_bw_low;
	uint32_t ipa_bw_high;
	uint32_t ipa_bw_medium;
	uint32_t ipa_bw_low;
	bool ipa_force_voting;
};

/**
 * enum wlan_ipa_wlan_event - WLAN IPA events
 * @WLAN_IPA_CLIENT_CONNECT: Client Connects
 * @WLAN_IPA_CLIENT_DISCONNECT: Client Disconnects
 * @WLAN_IPA_AP_CONNECT: SoftAP is started
 * @WLAN_IPA_AP_DISCONNECT: SoftAP is stopped
 * @WLAN_IPA_STA_CONNECT: STA associates to AP
 * @WLAN_IPA_STA_DISCONNECT: STA dissociates from AP
 * @WLAN_IPA_CLIENT_CONNECT_EX: Peer associates/re-associates to softap
 * @WLAN_IPA_WLAN_EVENT_MAX: Max value for the enum
 */
enum wlan_ipa_wlan_event {
	WLAN_IPA_CLIENT_CONNECT,
	WLAN_IPA_CLIENT_DISCONNECT,
	WLAN_IPA_AP_CONNECT,
	WLAN_IPA_AP_DISCONNECT,
	WLAN_IPA_STA_CONNECT,
	WLAN_IPA_STA_DISCONNECT,
	WLAN_IPA_CLIENT_CONNECT_EX,
	WLAN_IPA_WLAN_EVENT_MAX
};

/**
 * struct ipa_uc_offload_control_params - ipa offload control params
 * @offload_type: ipa offload type
 * @vdev_id: vdev id
 * @enable: ipa offload enable/disable
 */
struct ipa_uc_offload_control_params {
	uint32_t offload_type;
	uint32_t vdev_id;
	uint32_t enable;
};

/* fp to send IPA UC offload cmd */
typedef QDF_STATUS (*ipa_uc_offload_control_req)(struct wlan_objmgr_psoc *psoc,
				struct ipa_uc_offload_control_params *req);
#endif /* end  of _WLAN_IPA_PUBLIC_STRUCT_H_ */
