/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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



//#ifndef _NET_IF_ETHERSUBR_H_
//#define _NET_IF_ETHERSUBR_H_
#ifndef _NET_IF_UPPERPROTO_H_
#define _NET_IF_UPPERPROTO_H_

#define ETHER_ADDR_LEN    6    /* length of an Ethernet address */
#define ETHER_TYPE_LEN    2    /* length of the Ethernet type field */
#define ETHER_CRC_LEN     4    /* length of the Ethernet CRC */
#define ETHER_HDR_LEN     (ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)
#define ETHER_MAX_LEN     1518

#define ETHERMTU          (ETHER_MAX_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)

/*
 * Structure of a 10Mb/s Ethernet header.
 */
#ifndef _NET_ETHERNET_H_
struct ether_header {
    u_int8_t    ether_dhost[ETHER_ADDR_LEN];
    u_int8_t    ether_shost[ETHER_ADDR_LEN];
    u_int16_t   ether_type;
} __packed;
#endif
#ifndef _LINUX_IF_VLAN_H_
struct vlan_hdr {
    u_int16_t  h_vlan_TCI;
    u_int16_t  h_vlan_encapsulated_proto;
} __packed;
#define VLAN_VID_MASK   0xfff
#endif

#ifndef ETHERTYPE_PAE
#define ETHERTYPE_PAE    0x888e    /* EAPOL PAE/802.1x */
#endif
#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP     0x0800    /* IP protocol */
#endif
#ifndef ETHERTYPE_AARP
#define	ETHERTYPE_AARP	0x80f3		/* Appletalk AARP protocol */
#endif
#ifndef ETHERTYPE_IPX
#define ETHERTYPE_IPX    0x8137    /* IPX over DIX protocol */
#endif
#ifndef ETHERTYPE_ARP
#define ETHERTYPE_ARP    0x0806    /* ARP protocol */
#endif
#ifndef ETHERTYPE_IPV6
#define ETHERTYPE_IPV6   0x86dd    /* IPv6 */
#endif
#ifndef ETHERTYPE_8021Q
#define	ETHERTYPE_8021Q	0x8100		/* 802.1Q vlan protocol */
#endif
#ifndef ETHERTYPE_VLAN
#define ETHERTYPE_VLAN  0x8100          /* VLAN TAG protocol */
#endif
#ifndef TX_QUEUE_FOR_EAPOL_FRAME
#define TX_QUEUE_FOR_EAPOL_FRAME  0x7        /* queue eapol frame to queue 7 to avoid aggregation disorder */
#endif

/*
 * define WAI ethertype
 */
#ifndef ETHERTYPE_WAI
#define ETHERTYPE_WAI    0x88b4     /* WAI/WAPI */
#endif

/*
 * Structure of a 48-bit Ethernet address.
 */
#if 0
#ifndef _NET_ETHERNET_H_
struct ether_addr {
    u_int8_t octet[ETHER_ADDR_LEN];
} __packed;
#endif
#endif

#define ETHER_IS_MULTICAST(addr) (*(addr) & 0x01) /* is address mcast/bcast? */

#define VLAN_PRI_SHIFT  13    /* Shift to find VLAN user priority */
#define VLAN_PRI_MASK    7    /* Mask for user priority bits in VLAN */

/*
 * Structure of the IP frame
 */
struct ip_header {
    u_int8_t    version_ihl;
    u_int8_t    tos;
    u_int16_t   tot_len;
    u_int16_t   id;
    u_int16_t   frag_off;
    u_int8_t    ttl;
    u_int8_t    protocol;
    u_int16_t   check;
    u_int32_t   saddr;
    u_int32_t   daddr;
    /*The options start here. */
};
#ifndef IP_PROTO_TCP
#define IP_PROTO_TCP    0x6    /* TCP protocol */
#endif
#ifndef IP_PROTO_UDP
#define IP_PROTO_UDP 17
#endif

/*
 *   IGMP protocol structures
 */

/* IGMP record type */
#define IGMP_QUERY_TYPE       0x11
#define IGMPV1_REPORT_TYPE    0x12
#define IGMPV2_REPORT_TYPE    0x16
#define IGMPV2_LEAVE_TYPE     0x17
#define IGMPV3_REPORT_TYPE    0x22

/* Is packet type is either leave or report */
#define IS_IGMP_REPORT_LEAVE_PACKET(type) (\
                                              (IGMPV1_REPORT_TYPE == type)\
                                           || (IGMPV2_REPORT_TYPE == type)\
                                           || (IGMPV2_LEAVE_TYPE  == type)\
                                           || (IGMPV3_REPORT_TYPE == type)\
                                          )
/*
 *    Header in on cable format
 */

struct igmp_header
{
    u_int8_t type;
    u_int8_t code;        /* For newer IGMP */
    u_int16_t csum;
    u_int32_t group;
};

/* V3 group record types [grec_type] */
#define IGMPV3_MODE_IS_INCLUDE        1
#define IGMPV3_MODE_IS_EXCLUDE        2
#define IGMPV3_CHANGE_TO_INCLUDE      3
#define IGMPV3_CHANGE_TO_EXCLUDE      4
#define IGMPV3_ALLOW_NEW_SOURCES      5
#define IGMPV3_BLOCK_OLD_SOURCES      6

/*  Group record format
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  Record Type  |  Aux Data Len |     Number of Sources (N)     |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Multicast Address                       |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Source Address [1]                      |
      +-                                                             -+
      |                       Source Address [2]                      |
      +-                                                             -+
      .                               .                               .
      .                               .                               .
      .                               .                               .
      +-                                                             -+
      |                       Source Address [N]                      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      .                                                               .
      .                         Auxiliary Data                        .
      .                                                               .
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct igmp_v3_grec {
    u_int8_t    grec_type;
    u_int8_t    grec_auxwords;
    u_int16_t    grec_nsrcs;
    u_int32_t    grec_mca;
};

/* IGMPv3 report format
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  Type = 0x22  |    Reserved   |           Checksum            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |           Reserved            |  Number of Group Records (M)  |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      .                                                               .
      .                        Group Record [1]                       .
      .                                                               .
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      .                                                               .
      .                        Group Record [2]                       .
      .                                                               .
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                               .                               |
      .                               .                               .
      |                               .                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      .                                                               .
      .                        Group Record [M]                       .
      .                                                               .
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct igmp_v3_report {
    u_int8_t     type;
    u_int8_t     resv1;
    u_int16_t    csum;
    u_int16_t    resv2;
    u_int16_t    ngrec;
};

/* Calculate the group record length*/
#define IGMPV3_GRP_REC_LEN(x) (8 + (4 * x->grec_nsrcs) + (4 * x->grec_auxwords) )

#endif /* _NET_IF_ETHERSUBR_H_ */
