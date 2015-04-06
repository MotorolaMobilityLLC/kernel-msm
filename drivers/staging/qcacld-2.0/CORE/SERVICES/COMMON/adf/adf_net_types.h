/*
 * Copyright (c) 2011,2013 The Linux Foundation. All rights reserved.
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


/**
 * @ingroup adf_net_public
 * @file adf_net_types.h
 * This file defines types used in the networking stack abstraction.
 */

#ifndef _ADF_NET_TYPES_H
#define _ADF_NET_TYPES_H

#include <adf_os_types.h> /* a_uint8_t, etc. */

/**
 * @brief These control/get info from the device
 */
#define ADF_NET_CMD(_x)           \
    ADF_NET_CMD_GET_##_x,        \
    ADF_NET_CMD_SET_##_x

/**
 * @brief Get/Set commands from anet to adf_drv
 */
typedef enum {
    ADF_NET_CMD(LINK_INFO),
    ADF_NET_CMD(POLL_INFO),
    ADF_NET_CMD(CKSUM_INFO),
    ADF_NET_CMD(RING_INFO),
    ADF_NET_CMD(MAC_ADDR),
    ADF_NET_CMD(MTU),
    ADF_NET_CMD_GET_DMA_INFO,
    ADF_NET_CMD_GET_OFFLOAD_CAP,
    ADF_NET_CMD_GET_STATS,
    ADF_NET_CMD_ADD_VID,
    ADF_NET_CMD_DEL_VID,
    ADF_NET_CMD_SET_MCAST,
    ADF_NET_CMD_GET_MCAST_CAP
}adf_net_cmd_t;



/**
 * @brief Indicates what features are supported by the interface.
 */
#define ADF_NET_LINK_SUPP_10baseT_Half      (1 << 0)
#define ADF_NET_LINK_SUPP_10baseT_Full      (1 << 1)
#define ADF_NET_LINK_SUPP_100baseT_Half     (1 << 2)
#define ADF_NET_LINK_SUPP_100baseT_Full     (1 << 3)
#define ADF_NET_LINK_SUPP_1000baseT_Half    (1 << 4)
#define ADF_NET_LINK_SUPP_1000baseT_Full    (1 << 5)
#define ADF_NET_LINK_SUPP_Autoneg           (1 << 6)
#define ADF_NET_LINK_SUPP_Pause             (1 << 7)
#define ADF_NET_LINK_SUPP_Asym_Pause        (1 << 8)

#define ADF_NET_LINK_SUPP_100               (ADF_NET_LINK_SUPP_10baseT_Half  |   \
                                          ADF_NET_LINK_SUPP_10baseT_Full  |   \
                                          ADF_NET_LINK_SUPP_100baseT_Half |   \
                                          ADF_NET_LINK_SUPP_100baseT_Full)

#define ADF_NET_LINK_SUPP_1000               (ADF_NET_LINK_SUPP_100 |             \
                                           ADF_NET_LINK_SUPP_1000baseT_Full)

/**
 * @brief Indicates what features are advertised by the interface.
 */
#define ADF_NET_LINK_ADV_10baseT_Half     (1 << 0)
#define ADF_NET_LINK_ADV_10baseT_Full     (1 << 1)
#define ADF_NET_LINK_ADV_100baseT_Half    (1 << 2)
#define ADF_NET_LINK_ADV_100baseT_Full    (1 << 3)
#define ADF_NET_LINK_ADV_1000baseT_Half   (1 << 4)
#define ADF_NET_LINK_ADV_1000baseT_Full   (1 << 5)
#define ADF_NET_LINK_ADV_Autoneg          (1 << 6)
#define ADF_NET_LINK_ADV_Pause            (1 << 7)
#define ADF_NET_LINK_ADV_Asym_Pause       (1 << 8)

#define ADF_NET_LINK_ADV_100             (ADF_NET_LINK_ADV_10baseT_Half  |  \
                                         ADF_NET_LINK_ADV_10baseT_Full  |   \
                                         ADF_NET_LINK_ADV_100baseT_Half |   \
                                         ADF_NET_LINK_ADV_100baseT_Full)

#define ADF_NET_LINK_ADV_1000            (ADF_NET_LINK_ADV_100 |            \
                                          ADF_NET_LINK_ADV_1000baseT_Full)

/**
 * @brief The forced/current speed/duplex/autoneg
 */
#define ADF_NET_LINK_SPEED_10        10
#define ADF_NET_LINK_SPEED_100       100
#define ADF_NET_LINK_SPEED_1000      1000

#define ADF_NET_LINK_DUPLEX_HALF     0x00
#define ADF_NET_LINK_DUPLEX_FULL     0x01

#define ADF_NET_LINK_AUTONEG_DISABLE 0x00
#define ADF_NET_LINK_AUTONEG_ENABLE  0x01

#define ADF_NET_MAC_ADDR_MAX_LEN 6
#define ADF_NET_IF_NAME_SIZE    64
#define ADF_NET_ETH_LEN         ADF_NET_MAC_ADDR_MAX_LEN
#define ADF_NET_MAX_MCAST_ADDR  64


/**
 * @brief Extended Traffic ID  passed to target if the TID is unknown
 */
#define ADF_NBUF_TX_EXT_TID_INVALID     0x1f


/**
 * @brief Specify whether to encrypt frame or not
 */
enum adf_nbuf_exemption_type {
    ADF_NBUF_EXEMPT_NO_EXEMPTION = 0,
    ADF_NBUF_EXEMPT_ALWAYS,
    ADF_NBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE
};

typedef enum adf_net_flags{
    ADF_NET_IS_RUNNING = 0x0001,
    ADF_NET_IS_UP = 0x0002,
    ADF_NET_IS_ALLMULTI = 0x0004,
    ADF_NET_IS_PROMISC = 0x0008
}adf_net_flags_t;
/**
 * @brief link info capability/parameters for the device
 * Note the flags below
 */
typedef struct {
    a_uint32_t  supported;   /*RO Features this if supports*/
    a_uint32_t  advertized;  /*Features this interface advertizes*/
    a_int16_t   speed;       /*Force speed 10M, 100M, gigE*/
    a_int8_t    duplex;      /*duplex full or half*/
    a_uint8_t   autoneg;     /*Enabled/disable autoneg*/
}adf_net_cmd_link_info_t;

typedef struct adf_net_ethaddr{
        a_uint8_t   addr[ADF_NET_ETH_LEN];
} adf_net_ethaddr_t;
typedef struct {
    a_uint8_t   ether_dhost[ADF_NET_ETH_LEN];   /* destination eth addr */
    a_uint8_t   ether_shost[ADF_NET_ETH_LEN];   /* source ether addr    */
    a_uint16_t  ether_type;                     /* packet type ID field */
}adf_net_ethhdr_t;

typedef struct {
#if defined (ADF_LITTLE_ENDIAN_MACHINE)
    a_uint8_t       ip_hl:4,
                    ip_version:4;
#elif defined (ADF_BIG_ENDIAN_MACHINE)
    a_uint8_t       ip_version:4,
                    ip_hl:4;
#else
#error  "Please fix"
#endif
    a_uint8_t       ip_tos;

    a_uint16_t      ip_len;
    a_uint16_t      ip_id;
    a_uint16_t      ip_frag_off;
    a_uint8_t       ip_ttl;
    a_uint8_t       ip_proto;
    a_uint16_t      ip_check;
    a_uint32_t      ip_saddr;
    a_uint32_t      ip_daddr;
    /*The options start here. */
 }adf_net_iphdr_t;

typedef struct {
    a_uint16_t      src_port;
    a_uint16_t      dst_port;
    a_uint16_t      udp_len;
    a_uint16_t      udp_cksum;
 }adf_net_udphdr_t;

typedef struct {
    a_uint8_t       dhcp_msg_type;
    a_uint8_t       dhcp_hw_type;
    a_uint8_t       dhcp_hw_addr_len;
    a_uint8_t       dhcp_num_hops;
    a_uint32_t      dhcp_transc_id;
    a_uint16_t      dhcp_secs_elapsed;
    a_uint16_t      dhcp_flags;
    a_uint32_t      dhcp_ciaddr; /* client IP (if client is in BOUND, RENEW or REBINDING state) */
    a_uint32_t      dhcp_yiaddr; /* 'your' (client) IP address */
    /* IP address of next server to use in bootstrap, returned in DHCPOFFER, DHCPACK by server */
    a_uint32_t      dhcp_siaddr_nip;
    a_uint32_t      dhcp_gateway_nip; /* relay agent IP address */
    a_uint8_t       dhcp_chaddr[16];  /* link-layer client hardware address (MAC) */
    a_uint8_t       dhcp_sname[64];   /* server host name (ASCIZ) */
    a_uint8_t       dhcp_file[128];   /* boot file name (ASCIZ) */
    a_uint8_t       dhcp_cookie[4];   /* fixed first four option bytes (99,130,83,99 dec) */
 } adf_net_dhcphdr_t;

/* @brief V3 group record types [grec_type] */
#define IGMPV3_MODE_IS_INCLUDE		1
#define IGMPV3_MODE_IS_EXCLUDE		2
#define IGMPV3_CHANGE_TO_INCLUDE	3
#define IGMPV3_CHANGE_TO_EXCLUDE	4
#define IGMPV3_ALLOW_NEW_SOURCES	5
#define IGMPV3_BLOCK_OLD_SOURCES	6

typedef struct {
	a_uint8_t   igmp_type;
	a_uint8_t   igmp_code;		/* For newer IGMP */
	a_uint16_t  igmp_csum;
	a_uint32_t  igmp_group;
} adf_net_igmphdr_t;

typedef struct {
	a_uint8_t	grec_type;
	a_uint8_t	grec_auxwords;
	a_uint16_t	grec_nsrcs;
	a_uint32_t	grec_mca;
	a_uint32_t	grec_src[1];
} adf_net_igmpv3_grec_t;

/**
 * @brief IGMP version 3 specific datatypes
 */
typedef struct {
	a_uint8_t               igmpv3_type;
	a_uint8_t               igmpv3_resv1;
	a_uint16_t              igmpv3_csum;
	a_uint16_t              igmpv3_resv2;
	a_uint16_t              igmpv3_ngrec;
	adf_net_igmpv3_grec_t   igmpv3_grec[1];
} adf_net_igmpv3_report_t;


/**
 * @brief Vlan header
 */
typedef struct adf_net_vlanhdr{
    a_uint16_t      tpid;
#if defined (ADF_LITTLE_ENDIAN_MACHINE)
    a_uint16_t      vid:12; /* Vlan id*/
    a_uint8_t       cfi:1; /* reserved for CFI, don't use*/
    a_uint8_t       prio:3; /* Priority*/
#elif defined (ADF_BIG_ENDIAN_MACHINE)
    a_uint8_t       prio:3; /* Priority*/
    a_uint8_t       cfi:1; /* reserved for CFI, don't use*/
    a_uint16_t      vid:12; /* Vlan id*/
#else
#error  "Please fix"
#endif
}adf_net_vlanhdr_t;

typedef struct adf_net_vid{
#if defined (ADF_LITTLE_ENDIAN_MACHINE)
    a_uint16_t      val:12;
    a_uint8_t       res:4;
#elif defined (ADF_BIG_ENDIAN_MACHINE)
    a_uint8_t      res:4;
    a_uint16_t      val:12;
#else
#error  "Please fix"
#endif
}adf_net_vid_t;


/**
 * @brief Command for setting ring paramters.
 */
typedef struct {
    a_uint32_t rx_bufsize;  /*Ro field. For shim's that maintain a pool*/
    a_uint32_t rx_ndesc;
    a_uint32_t tx_ndesc;
}adf_net_cmd_ring_info_t;

/**
 * @brief Whether the interface is polled or not. If so, the polling bias (number of
 * packets it wants to process per invocation
 */
typedef struct {
    a_bool_t    polled;
    a_uint32_t  poll_wt;
}adf_net_cmd_poll_info_t;

/**
 * @brief Basic device info
 */
typedef struct {
    a_uint8_t      if_name[ADF_NET_IF_NAME_SIZE];
    a_uint8_t       dev_addr[ADF_NET_MAC_ADDR_MAX_LEN];
    a_uint16_t       header_len;
    a_uint16_t       mtu_size;
}adf_net_dev_info_t;

typedef struct adf_dma_info {
    adf_os_dma_mask_t   dma_mask;
    a_uint32_t          sg_nsegs; /**< scatter segments */
}adf_net_cmd_dma_info_t;

/**
 * @brief Defines the TX and RX checksumming capabilities/state of the device
 * The actual checksum handling happens on an adf_nbuf
 * If offload capability command not supported, all offloads are assumed to be
 * none.
 */
typedef enum {
    ADF_NET_CKSUM_NONE,           /*Cannot do any checksum*/
    ADF_NET_CKSUM_TCP_UDP_IPv4,   /*tcp/udp on ipv4 with pseudo hdr*/
    ADF_NET_CKSUM_TCP_UDP_IPv6,   /*tcp/udp on ipv6*/
}adf_net_cksum_type_t;

typedef struct {
    adf_net_cksum_type_t tx_cksum;
    adf_net_cksum_type_t rx_cksum;
}adf_net_cksum_info_t;

typedef adf_net_cksum_info_t adf_net_cmd_cksum_info_t;    /*XXX needed?*/

/**
 * @brief Command for set/unset vid
 */
typedef a_uint16_t adf_net_cmd_vid_t ;        /*get/set vlan id*/

typedef enum {
    ADF_NET_TSO_NONE,
    ADF_NET_TSO_IPV4,     /**< for tsp ipv4 only*/
    ADF_NET_TSO_ALL,      /**< ip4 & ipv6*/
}adf_net_tso_type_t;

/**
 * @brief Command for getting offloading capabilities of a device
 */
typedef struct {
    adf_net_cksum_info_t cksum_cap;
    adf_net_tso_type_t   tso;
    a_uint8_t         vlan_supported;
}adf_net_cmd_offload_cap_t;

/**
 * @brief Command for getting general stats from a device
 */
typedef struct {
    a_uint32_t tx_packets;  /**< total packets transmitted*/
    a_uint32_t rx_packets;  /**< total packets recieved*/
    a_uint32_t tx_bytes;    /**< total bytes transmitted*/
    a_uint32_t rx_bytes;    /**< total bytes recieved*/
    a_uint32_t tx_dropped;  /**< total tx dropped because of lack of buffers*/
    a_uint32_t rx_dropped;  /**< total rx dropped because of lack of buffers*/
    a_uint32_t rx_errors;   /**< bad packet recieved*/
    a_uint32_t tx_errors;   /**< transmisison problems*/
}adf_net_cmd_stats_t;

typedef enum adf_net_cmd_mcast_cap{
    ADF_NET_MCAST_SUP=0,
    ADF_NET_MCAST_NOTSUP
}adf_net_cmd_mcast_cap_t;

typedef struct adf_net_devaddr{
    a_uint32_t              num; /**< No. of mcast addresses*/
    a_uint8_t               *da_addr[ADF_NET_MAX_MCAST_ADDR];
}adf_net_devaddr_t;

typedef adf_net_devaddr_t       adf_net_cmd_mcaddr_t;
typedef adf_net_ethaddr_t       adf_net_cmd_macaddr_t;

typedef union {
    adf_net_cmd_link_info_t     link_info;
    adf_net_cmd_poll_info_t     poll_info;
    adf_net_cmd_cksum_info_t    cksum_info;
    adf_net_cmd_ring_info_t     ring_info;
    adf_net_cmd_dma_info_t      dma_info;
    adf_net_cmd_vid_t           vid;
    adf_net_cmd_offload_cap_t   offload_cap;
    adf_net_cmd_stats_t         stats;
    adf_net_cmd_mcaddr_t        mcast_info;
    adf_net_cmd_mcast_cap_t     mcast_cap;
    adf_net_cmd_macaddr_t       mac_addr;
}adf_net_cmd_data_t;

/**
 * @brief For polled devices, adf_drv responds with one of the following status in
 * its poll function.
 */
typedef enum {
    ADF_NET_POLL_DONE,
    ADF_NET_POLL_NOT_DONE,
    ADF_NET_POLL_OOM,
}adf_net_poll_resp_t;

/**
 * @brief For transmit checksum API
 */

typedef enum {
    ADF_NBUF_TX_CKSUM_NONE,         /* No ckecksum offload */
    ADF_NBUF_TX_CKSUM_IP,           /* IP header ckecksum offload */
    ADF_NBUF_TX_CKSUM_TCP_UDP,      /* TCP/UDP ckecksum offload */
    ADF_NBUF_TX_CKSUM_TCP_UDP_IP,   /* TCP/UDP checksum and IP header ckecksum offload */

}adf_nbuf_tx_cksum_t;

/**
 * @brief For recieve checksum API
 */
typedef enum {
    ADF_NBUF_RX_CKSUM_TCP                   = 0x0001,
    ADF_NBUF_RX_CKSUM_UDP                   = 0x0002,
    ADF_NBUF_RX_CKSUM_TCPIPV6               = 0x0010,
    ADF_NBUF_RX_CKSUM_UDPIPV6               = 0x0020,
    ADF_NBUF_RX_CKSUM_TCP_NOPSEUDOHEADER    = 0x0100,
    ADF_NBUF_RX_CKSUM_UDP_NOPSEUDOHEADER    = 0x0200,
    ADF_NBUF_RX_CKSUM_TCPSUM16              = 0x1000,
}adf_nbuf_l4_rx_cksum_type_t;

typedef enum {
    ADF_NBUF_RX_CKSUM_NONE                  = 0x0000,        /* device failed to ckecksum */
    ADF_NBUF_RX_CKSUM_TCP_UDP_HW            = 0x0010,        /* TCP/UDP cksum successful and value returned */
    ADF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY   = 0x0020,        /* TCP/UDP cksum successful, no value */
}adf_nbuf_l4_rx_cksum_result_t;

typedef struct {
    adf_nbuf_l4_rx_cksum_type_t    l4_type;
    adf_nbuf_l4_rx_cksum_result_t  l4_result;
    a_uint32_t                  val;
}adf_nbuf_rx_cksum_t;

/**
 * @brief For TCP large Segment Offload
 */
typedef struct {
    adf_net_tso_type_t  type;
    a_uint16_t          mss;
    a_uint8_t           hdr_off;
}adf_nbuf_tso_t;

/**
 * @brief Wireless events
 * ADF_IEEE80211_ASSOC = station associate (bss mode)
 * ADF_IEEE80211_REASSOC = station re-associate (bss mode)
 * ADF_IEEE80211_DISASSOC = station disassociate (bss mode)
 * ADF_IEEE80211_JOIN = station join (ap mode)
 * ADF_IEEE80211_LEAVE = station leave (ap mode)
 * ADF_IEEE80211_SCAN = scan complete, results available
 * ADF_IEEE80211_REPLAY = sequence counter replay detected
 * ADF_IEEE80211_MICHAEL = Michael MIC failure detected
 * ADF_IEEE80211_REJOIN = station re-associate (ap mode)
 * ADF_CUSTOM_PUSH_BUTTON =
 */
typedef enum adf_net_wireless_events{
    ADF_IEEE80211_ASSOC = __ADF_IEEE80211_ASSOC,
    ADF_IEEE80211_REASSOC = __ADF_IEEE80211_REASSOC,
    ADF_IEEE80211_DISASSOC = __ADF_IEEE80211_DISASSOC,
    ADF_IEEE80211_JOIN = __ADF_IEEE80211_JOIN,
    ADF_IEEE80211_LEAVE = __ADF_IEEE80211_LEAVE,
    ADF_IEEE80211_SCAN = __ADF_IEEE80211_SCAN,
    ADF_IEEE80211_REPLAY = __ADF_IEEE80211_REPLAY,
    ADF_IEEE80211_MICHAEL = __ADF_IEEE80211_MICHAEL,
    ADF_IEEE80211_REJOIN = __ADF_IEEE80211_REJOIN,
    ADF_CUSTOM_PUSH_BUTTON = __ADF_CUSTOM_PUSH_BUTTON
}adf_net_wireless_event_t;

#define ADF_ARP_REQ		  1 /* ARP request */
#define ADF_ARP_RSP		  2 /* ARP response */
#define ADF_ARP_RREQ		  3 /* RARP request */
#define ADF_ARP_RRSP		  4 /* RARP response */

#define ADF_NEXTHDR_ICMP	 58 /* ICMP for IPv6. */

/* Neighbor Discovery */
#define ADF_ND_RSOL		133 /* Router Solicitation */
#define ADF_ND_RADVT		134 /* Router Advertisement */
#define ADF_ND_NSOL		135 /* Neighbor Solicitation */
#define ADF_ND_NADVT		136 /* Neighbor Advertisement */

/**
 * @brief IPv6 Address
 */
typedef struct {
	union {
		a_uint8_t	u6_addr8[16];
		a_uint16_t	u6_addr16[8];
		a_uint32_t	u6_addr32[4];
	} in6_u;
//#define s6_addr			in6_u.u6_addr8
//#define s6_addr16		in6_u.u6_addr16
//#define s6_addr32		in6_u.u6_addr32
} adf_net_ipv6_addr_t;

/**
 * @brief IPv6 Header
 */
typedef struct {
#if defined(ADF_LITTLE_ENDIAN_MACHINE)
	a_uint8_t		ipv6_priority:4,
				ipv6_version:4;
#elif defined(ADF_BIG_ENDIAN_MACHINE)
	a_uint8_t		ipv6_version:4,
				ipv6_priority:4;
#else
#error  "Please fix"
#endif
	a_uint8_t		ipv6_flow_lbl[3];

	a_uint16_t		ipv6_payload_len;
	a_uint8_t		ipv6_nexthdr,
				ipv6_hop_limit;

	adf_net_ipv6_addr_t	ipv6_saddr,
				ipv6_daddr;
} adf_net_ipv6hdr_t;

/**
 * @brief ICMPv6 Header
 */
typedef struct {

	a_uint8_t	icmp6_type;
	a_uint8_t	icmp6_code;
	a_uint16_t	icmp6_cksum;

	union {
		a_uint32_t	un_data32[1];
		a_uint16_t	un_data16[2];
		a_uint8_t	un_data8[4];

		struct {
			a_uint16_t	identifier;
			a_uint16_t	sequence;
		} u_echo;

		struct {
#if defined(ADF_LITTLE_ENDIAN_MACHINE)
			a_uint32_t	reserved:5,
					override:1,
					solicited:1,
					router:1,
					reserved2:24;
#elif defined(ADF_BIG_ENDIAN_MACHINE)
			a_uint32_t	router:1,
					solicited:1,
					override:1,
					reserved:29;
#else
#error  "Please fix"
#endif
		} u_nd_advt;

		struct {
			a_uint8_t	hop_limit;
#if defined(ADF_LITTLE_ENDIAN_MACHINE)
			a_uint8_t	reserved:6,
					other:1,
					managed:1;

#elif defined(ADF_BIG_ENDIAN_MACHINE)
			a_uint8_t	managed:1,
					other:1,
					reserved:6;
#else
#error  "Please fix"
#endif
			a_uint16_t	rt_lifetime;
		} u_nd_ra;

	} icmp6_dataun;

} adf_net_icmpv6hdr_t;

/**
 * @brief Neighbor Discovery Message
 */
typedef struct {
	adf_net_icmpv6hdr_t	nd_icmph;
	adf_net_ipv6_addr_t	nd_target;
	a_uint8_t		nd_opt[0];
} adf_net_nd_msg_t;

#define adf_csum_ipv6(s, d, l, p, sum)			\
	csum_ipv6_magic((struct in6_addr *)s,		\
			(struct in6_addr *)d, l, p, sum)
/**
 * @brief
 *
 * @param addr1
 * @param addr2
 *
 * @return Compare two ethernet addresses, returns A_TRUE if equal
 *
 */
static adf_os_inline a_bool_t
adf_net_cmp_ether_addr(const a_uint8_t *addr1,const  a_uint8_t *addr2)
{
    const a_uint16_t *a = (const a_uint16_t *) addr1;
    const a_uint16_t *b = (const a_uint16_t *) addr2;

    return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

#endif /*_ADF_NET_TYPES_H*/
