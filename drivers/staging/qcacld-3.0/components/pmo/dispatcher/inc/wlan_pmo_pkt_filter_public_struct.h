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
 * pmo packet filter feature.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_PKT_FILTER_PUBLIC_STRUCT_H_
#define _WLAN_PMO_PKT_FILTER_PUBLIC_STRUCT_H_

#include "qdf_types.h"

#define    PMO_MAX_FILTER_TEST_DATA_LEN       8
#define    PMO_MAX_NUM_TESTS_PER_FILTER      10

/**
 * enum pmo_rcv_pkt_fltr_type: Receive Filter Parameters
 * @PMO_RCV_FILTER_TYPE_INVALID: invalied filter type
 * @PMO_RCV_FILTER_TYPE_FILTER_PKT: packet filter
 * @PMO_RCV_FILTER_TYPE_BUFFER_PKT: buffer packet
 * @PMO_RCV_FILTER_TYPE_MAX_ENUM_SIZE: max filter
 */
enum pmo_rcv_pkt_fltr_type {
	PMO_RCV_FILTER_TYPE_INVALID,
	PMO_RCV_FILTER_TYPE_FILTER_PKT,
	PMO_RCV_FILTER_TYPE_BUFFER_PKT,
	PMO_RCV_FILTER_TYPE_MAX_ENUM_SIZE
};

/**
 * enum pmo_rcv_pkt_fltr_flag_type: Receive Filter flags
 * @PMO_FILTER_CMP_TYPE_INVALID: invalied flag
 * @PMO_FILTER_CMP_TYPE_EQUAL: equal
 * @PMO_FILTER_CMP_TYPE_MASK_EQUAL: mask
 * @PMO_FILTER_CMP_TYPE_NOT_EQUAL: not equal
 * @PMO_FILTER_CMP_TYPE_MASK_NOT_EQUAL: mask not equal
 * @PMO_FILTER_CMP_TYPE_MAX: max size of flag
 */
enum pmo_rcv_pkt_fltr_flag_type {
	PMO_FILTER_CMP_TYPE_INVALID,
	PMO_FILTER_CMP_TYPE_EQUAL,
	PMO_FILTER_CMP_TYPE_MASK_EQUAL,
	PMO_FILTER_CMP_TYPE_NOT_EQUAL,
	PMO_FILTER_CMP_TYPE_MASK_NOT_EQUAL,
	PMO_FILTER_CMP_TYPE_MAX
};

/**
 * enum pmo_rcv_pkt_fltr_protocol_params: Receive Filter protocal parameters
 * @PMO_FILTER_HDR_TYPE_INVALID: invalied type
 * @PMO_FILTER_HDR_TYPE_MAC: mac protocol
 * @PMO_FILTER_HDR_TYPE_ARP: arp protocol
 * @PMO_FILTER_HDR_TYPE_IPV4: ipv4 protocol
 * @PMO_FILTER_HDR_TYPE_IPV6: ipv6 protocol
 * @PMO_FILTER_HDR_TYPE_UDP: udp protocol
 * @PMO_FILTER_HDR_TYPE_MAX: max of type of protocol
 */
enum pmo_rcv_pkt_fltr_protocol_params {
	PMO_FILTER_HDR_TYPE_INVALID,
	PMO_FILTER_HDR_TYPE_MAC,
	PMO_FILTER_HDR_TYPE_ARP,
	PMO_FILTER_HDR_TYPE_IPV4,
	PMO_FILTER_HDR_TYPE_IPV6,
	PMO_FILTER_HDR_TYPE_UDP,
	PMO_FILTER_HDR_TYPE_MAX
};

/**
 * struct pmo_rcv_pkt_fltr_field_params  - pmo packet filter field parameters
 * @protocol_layer: Protocol layer
 * @compare_flag: Comparison flag
 * @data_length: Length of the data to compare
 * @data_offset:  from start of the respective frame header
 * @reserved: Reserved field
 * @compare_data: Data to compare
 * @data_mask: Mask to be applied on the received packet data before compare
 */
struct pmo_rcv_pkt_fltr_field_params {
	enum pmo_rcv_pkt_fltr_protocol_params protocol_layer;
	enum pmo_rcv_pkt_fltr_flag_type compare_flag;
	uint16_t data_length;
	uint8_t data_offset;
	uint8_t reserved;
	uint8_t compare_data[PMO_MAX_FILTER_TEST_DATA_LEN];
	uint8_t data_mask[PMO_MAX_FILTER_TEST_DATA_LEN];
};

/**
 * struct pmo_rcv_pkt_fltr_cfg - pmo packet filter config
 * @filter_id: filter id
 * @filter_type: filter type
 * @num_params: number of parameters
 * @coalesce_time: time
 * @self_macaddr: mac address
 * @bssid: Bssid of the connected AP
 * @params_data: data
 */
struct pmo_rcv_pkt_fltr_cfg {
	uint8_t filter_id;
	enum pmo_rcv_pkt_fltr_type filter_type;
	uint32_t num_params;
	uint32_t coalesce_time;
	struct qdf_mac_addr self_macaddr;
	struct qdf_mac_addr bssid;
	struct pmo_rcv_pkt_fltr_field_params
		params_data[PMO_MAX_NUM_TESTS_PER_FILTER];
};

/**
 * struct pmo_rcv_pkt_fltr_cfg - pmo receive Filter Clear Parameters
 * @status:  only valid for response message
 * @filter_id:
 * @self_macaddr:
 * @bssid: peer ap address
 */
struct pmo_rcv_pkt_fltr_clear_param {
	uint32_t status;
	uint8_t filter_id;
	struct qdf_mac_addr self_macaddr;
	struct qdf_mac_addr bssid;
};

#endif /* end of _WLAN_PMO_PKT_FILTER_PUBLIC_STRUCT_H_ */
