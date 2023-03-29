/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_PMO_PACKET_FILTER_CFG_H__
#define WLAN_PMO_PACKET_FILTER_CFG_H__

/*
 * <ini>
 * gDisablePacketFilter - Disable packet filter disable
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_DISABLE_PKT_FILTER CFG_INI_BOOL( \
	"gDisablePacketFilter", \
	true, \
	"Disable packet filter feature")

#ifdef WLAN_FEATURE_PACKET_FILTERING
/*
 * <ini>
 * g_enable_packet_filter_bitmap - Packet filters configuration
 * @Min: 0
 * @Max: 63
 * @Default: 0
 *
 * To enable packet filters when target goes to suspend, clear when resume:
 * bit-0 : IPv6 multicast
 * bit-1 : IPv4 multicast
 * bit-2 : IPv4 broadcast
 * bit-3 : XID - Exchange station Identification packet, solicits the
 *         identification of the receiving station
 * bit-4 : STP - Spanning Tree Protocol, builds logical loop free topology
 * bit-5 : DTP/LLC/CDP
 *         DTP - Dynamic Trunking Protocol is used by Cisco switches to
 *               negotiate whether an interconnection between two switches
 *               should be put into access or trunk mode
 *         LLC - Logical link control, used for multiplexing, flow & error
 *               control
 *         CDP - Cisco Discovery Protocol packet contains information
 *               about the cisco devices in the network
 *
 * Supported Feature: Packet filtering
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PMO_PKT_FILTER CFG_INI_UINT( \
	"g_enable_packet_filter_bitmap", \
	0, 63, 0, \
	CFG_VALUE_OR_DEFAULT, \
	"Packet filter bitmap configure")

#define CFG_PACKET_FILTER_ALL \
	CFG(CFG_PMO_PKT_FILTER) \
	CFG(CFG_PMO_DISABLE_PKT_FILTER)
#else
#define CFG_PACKET_FILTER_ALL \
	CFG(CFG_PMO_DISABLE_PKT_FILTER)
#endif /* WLAN_FEATURE_PACKET_FILTERING */
#endif /* WLAN_PMO_PACKET_FILTER_CFG_H__ */
