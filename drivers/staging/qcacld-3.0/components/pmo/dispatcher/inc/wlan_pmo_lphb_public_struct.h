/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * pmo lphb offload feature.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_LPHB_PUBLIC_STRUCT_H_
#define _WLAN_PMO_LPHB_PUBLIC_STRUCT_H_

#include "wlan_pmo_common_public_struct.h"

#define PMO_SIR_LPHB_FILTER_LEN   64

/**
 * enum lphb_ind_type -Low power heart beat indication type
 * @pmo_lphb_set_en_param_indid: lphb enable indication
 * @pmo_lphb_set_tcp_pararm_indid: lphb tcp param indication
 * @pmo_lphb_set_tcp_pkt_filter_indid: lphb tcp packet filter indication
 * @pmo_lphb_set_udp_pararm_indid: lphb udp param indication
 * @pmo_lphb_set_udp_pkt_filter_indid: lphb udp packet filter indication
 * @pmo_lphb_set_network_info_indid: lphb network information indication
 */
enum lphb_ind_type {
	pmo_lphb_set_en_param_indid,
	pmo_lphb_set_tcp_pararm_indid,
	pmo_lphb_set_tcp_pkt_filter_indid,
	pmo_lphb_set_udp_pararm_indid,
	pmo_lphb_set_udp_pkt_filter_indid,
	pmo_lphb_set_network_info_indid,
};

/**
 * struct pmo_lphb_enable_req -Low power heart beat enable request
 * @enable: lphb enable request
 * @item: request item
 * @session: lphb session
 */
struct pmo_lphb_enable_req {
	uint8_t enable;
	uint8_t item;
	uint8_t session;
};

/**
 * struct pmo_lphb_tcp_params - Low power heart beat tcp params
 * @srv_ip: source ip address
 * @dev_ip: destination ip address
 * @src_port: source port
 * @dst_port: destination port
 * @timeout: tcp timeout value
 * @session: session on which lphb needs to be configured
 * @gateway_mac: gateway mac address
 * @time_period_sec: time period in seconds
 * @tcp_sn: tcp sequence number
 */
struct pmo_lphb_tcp_params {
	uint32_t srv_ip;
	uint32_t dev_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t timeout;
	uint8_t session;
	struct qdf_mac_addr gateway_mac;
	uint16_t time_period_sec;
	uint32_t tcp_sn;
};

/**
 * struct pmo_lphb_tcp_filter_req - Low power heart beat tcp filter request
 * @length: length of filter
 * @offset: offset of filter
 * @session: session on which lphb needs to be configured
 * @filter: filter buffer
 */
struct pmo_lphb_tcp_filter_req {
	uint16_t length;
	uint8_t offset;
	uint8_t session;
	uint8_t filter[PMO_SIR_LPHB_FILTER_LEN];
};

/**
 * struct pmo_lphb_udp_params - Low power heart beat udp params
 * @srv_ip: source ip address
 * @dev_ip: destination ip address
 * @src_port: source port
 * @dst_port: destination port
 * @timeout: tcp timeout value
 * @session: session on which lphb needs to be configured
 * @gateway_mac: gateway mac address
 * @time_period_sec: time period in seconds
 */
struct pmo_lphb_udp_params {
	uint32_t srv_ip;
	uint32_t dev_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t interval;
	uint16_t timeout;
	uint8_t session;
	struct qdf_mac_addr gateway_mac;
};

/**
 * struct pmo_lphb_udp_filter_req - Low power heart beat udp filter request
 * @length: length of filter
 * @offset: offset of filter
 * @session: session on which lphb needs to be configured
 * @filter: filter buffer
 */
struct pmo_lphb_udp_filter_req {
	uint16_t length;
	uint8_t offset;
	uint8_t session;
	uint8_t filter[PMO_SIR_LPHB_FILTER_LEN];
};

/**
 * struct pmo_lphb_req - Low power heart beat request
 * @cmd: lphb command type
 * @dummy: whether dummy or not
 * @params: based on command lphb request buffer
 */
struct pmo_lphb_req {
	uint16_t cmd;
	uint16_t dummy;
	union {
		struct pmo_lphb_enable_req lphb_enable_req;
		struct pmo_lphb_tcp_params lphb_tcp_params;
		struct pmo_lphb_tcp_filter_req lphb_tcp_filter_req;
		struct pmo_lphb_udp_params lphb_udp_params;
		struct pmo_lphb_udp_filter_req lphb_udp_filter_req;
	} params;
};

/**
 * struct pmo_lphb_rsp - Low power heart beat response
 * @session_idx: session id
 * @protocol_type: tell protocol type
 * @event_reason: carry reason of lphb event
 */
struct pmo_lphb_rsp {
	uint8_t session_idx;
	uint8_t protocol_type;   /*TCP or UDP */
	uint8_t event_reason;
};

/*
 * Define typedef for lphb callback when fwr send response
 */
typedef
void (*pmo_lphb_callback)(void *cb_ctx, struct pmo_lphb_rsp *ind_param);

#endif /* end  of _WLAN_PMO_LPHB_PUBLIC_STRUCT_H_ */

