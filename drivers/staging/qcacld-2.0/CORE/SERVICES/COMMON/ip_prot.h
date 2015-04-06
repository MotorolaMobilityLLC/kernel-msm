/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef _IP_PROT__H_
#define _IP_PROT__H_


#define IP_PROTOCOL_ICMP        0x01 /* Internet Control Message Protocol */
#define IP_PROTOCOL_IGMP        0x02 /* Internet Group Management Protocol */
#define IP_PROTOCOL_IPV4        0x04 /* IPv4 (encapsulation) */
#define IP_PROTOCOL_TCP         0x06 /* Transmission Control Protocol */
#define IP_PROTOCOL_UDP         0x11 /* User Datagram Protocol */
#define IP_PROTOCOL_RDP         0x1B /* Reliable Datagram Protocol */
#define IP_PROTOCOL_IPV6        0x29 /* IPv6 (encapsulation) */
#define IP_PROTOCOL_IPV6_ROUTE  0x2B /* Routing Header for IPv6 */
#define IP_PROTOCOL_IPV6_FRAG   0x2C /* Fragment Header for IPv6 */
#define IP_PROTOCOL_RSVP        0x2E /* Resource Reservation Protocol */
#define IP_PROTOCOL_GRE         0x2F /* Generic Routing Encapsulation */
#define IP_PROTOCOL_MHRP        0x30 /* Mobile Host Routing Protocol */
#define IP_PROTOCOL_BNA         0x31 /* BNA */
#define IP_PROTOCOL_ESP         0x32 /* Encapsulating Security Payload */
#define IP_PROTOCOL_MOBILE      0x37 /* IP Mobility (Min Encap) */
#define IP_PROTOCOL_IPV6_ICMP   0x3A /* ICMP for IPv6 */
#define IP_PROTOCOL_IPV6_NONXT  0x3B /* No Next Header for IPv6 */
#define IP_PROTOCOL_IPV6_OPTS   0x3C /* Destination Options for IPv6 */
#define IP_PROTOCOL_IPCOMP      0x6C /* IP Payload Compression Protocol */
#define IP_PROTOCOL_L2TP        0x73 /* Layer Two Tunneling Protocol Version 3 */
#define IP_PROTOCOL_SMP         0x79 /* Simple Message Protocol */
#define IP_PROTOCOL_SCTP        0x84 /* Stream Control Transmission Protocol */
#define IP_PROTOCOL_SHIM6       0x8C /* Site Multihoming by IPv6 Intermediation */



/* IPv6 ICMP types */
#define IPV6_ICMP_TYPE_MLD 0x8F

#endif /* _IP_PROT__H_ */
