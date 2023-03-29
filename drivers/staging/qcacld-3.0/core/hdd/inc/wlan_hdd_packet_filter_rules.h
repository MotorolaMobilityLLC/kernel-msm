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

#if !defined(WLAN_HDD_PACKET_FILTER_RULES_H__)
#define WLAN_HDD_PACKET_FILTER_RULES_H__

/**
 * DOC: wlan_hdd_packet_filter_rules.h
 *
 */

/* Include files */

#define MAX_NUM_PACKET_FILTERS 6

/**
 * @filter_action: Filter action, set/clear the filter
 * Ex: filter_action = 1 set the filter
 *     filter_action = 2 clear the filter
 * @filter_id: Filter id  Ex: 1/2/3/4/5 .... MAX_NUM_FILTERS
 * @num_params: Number of parameters Ex: 1/2/3/4/5
 * @params_data: Packet filter parameters details
 *
 * @protocol_layer: the type of protocol layer header to which the data
 *                  being configured correspond
 * Ex: protocol_layer = 1 - MAC Header
 *     protocol_layer = 2 - ARP Header
 *     protocol_layer = 3 - IP Header
 * @compare_flag: comparison type
 * EX: compare_flag = 0 - comparison is invalid
 *     compare_flag = 1 - compare for equality of the data present in received
 *                        packet to the corresponding  configured data
 *     compare_flag = 2 - compare for equality of the data present in received
 *                        packet to the corresponding configured data after
 *                        applying the mask
 *     compare_flag = 3 - compare for non-equality of the data present in
 *                        received packet to the corresponding configured data
 *     compare_flag = 4 - compare for non-equality of the data present in
 *                        received packet to the corresponding configured data
 *                        after applying the mask
 * @data_fffset: Offset of the data to compare from the respective protocol
 *               layer header start (as per the respective protocol
 *               specification) in terms of bytes
 * @data_length: length of data to compare
 * @compare_data: Array of 8 bytes
 * @data_mask: Mask to be applied on the received packet data (Array of 8 bytes)
 */
static struct pkt_filter_cfg
		packet_filter_default_rules[MAX_NUM_PACKET_FILTERS] = {
	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 3,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 6,
		   .data_length = 2,
		   .compare_data = {134, 221, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 3,
		   .compare_flag = 4,
		   .data_offset = 24,
		   .data_length = 2,
		   .compare_data = {255, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {255, 0, 0, 0, 0, 0, 0, 0} } } },

	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 3,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 6,
		   .data_length = 2,
		   .compare_data = {8, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 3,
		   .compare_flag = 4,
		   .data_offset = 16,
		   .data_length = 1,
		   .compare_data = {224, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {240, 0, 0, 0, 0, 0, 0, 0} } } },
	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 3,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 6,
		   .data_length = 2,
		   .compare_data = {8, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 3,
		   .compare_flag = 4,
		   .data_offset = 18,
		   .data_length = 2,
		   .compare_data = {0, 255, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 255, 0, 0, 0, 0, 0, 0} } } },
	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 2,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 8,
		   .data_length = 4,
		   .compare_data = {0, 1, 175, 129, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} } } },
	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 2,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 6,
		   .data_length = 2,
		   .compare_data = {0, 39, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} } } },
	{ .filter_action = 1,
	  .filter_id = 0,
	  .num_params = 2,
	  .params_data = {
		 { .protocol_layer = 1,
		   .compare_flag = 5,
		   .data_offset = 0,
		   .data_length = 1,
		   .compare_data = {1, 0, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} },
		 { .protocol_layer = 2,
		   .compare_flag = 3,
		   .data_offset = 4,
		   .data_length = 2,
		   .compare_data = {0, 12, 0, 0, 0, 0, 0, 0},
		   .data_mask = {0, 0, 0, 0, 0, 0, 0, 0} } } } };
#endif
