/*
 * Copyright (c) 2012-2014, 2017 The Linux Foundation. All rights reserved.
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

#ifndef _IPV6__H_
#define _IPV6__H_

#if defined(ATH_TARGET)
#include <osapi.h>              /* A_UINT8 */
#else
#include <a_types.h>            /* A_UINT8 */
#endif

/* utilities for converting between network byte order and native endianness */
#ifndef BYTESWAP32
#define BYTESWAP32(x) \
	((((x) & 0x000000ff) << 24) /* byte 0 -> byte 3 */ | \
	 (((x) & 0x0000ff00) <<  8) /* byte 1 -> byte 2 */ | \
	 (((x) & 0x00ff0000) >>  8) /* byte 2 -> byte 1 */ | \
	 (((x) & 0xff000000) >> 24) /* byte 3 -> byte 0 */)
#endif /* BYTESWAP32 */

#ifndef BE_TO_CPU32
#if defined(ATH_TARGET)
/* assume target is little-endian */
#define BE_TO_CPU32(x) BYTESWAP32(x)
#else
#ifdef BIG_ENDIAN_HOST
#define BE_TO_CPU32(x) (x)
#else
#define BE_TO_CPU32(x) BYTESWAP32(x)
#endif
#endif
#endif /* BE_TO_CPU32 */

/* IPv6 header definition */

#define IPV6_ADDR_LEN 4         /* bytes */
struct ipv6_hdr_t {
	/* version, traffic class, and flow label */
	A_UINT32 ver_tclass_flowlabel;
	A_UINT8 pyld_len[2];    /* payload length */
	A_UINT8 next_hdr;
	A_UINT8 hop_limit;
	A_UINT8 src_addr[IPV6_ADDR_LEN];
	A_UINT8 dst_addr[IPV6_ADDR_LEN];
};

#define IPV6_HDR_LEN (sizeof(struct ipv6_hdr_t))
#define IPV6_HDR_OFFSET_NEXT_HDR (offsetof(struct ipv6_hdr_t, next_hdr))
#define IPV6_HDR_OFFSET_DST_ADDR (offsetof(struct ipv6_hdr_t, dst_addr[0]))

/* IPv6 header field access macros */

#define IPV6_HDR_VERSION_M       0xF0000000
#define IPV6_HDR_VERSION_S       28

#define IPV6_HDR_TRAFFIC_CLASS_M 0x0FF00000
#define IPV6_HDR_TRAFFIC_CLASS_S 20

#define IPV6_HDR_FLOW_LABEL_M    0x000FFFFF
#define IPV6_HDR_FLOW_LABEL_S    0

static inline A_UINT8 ipv6_version(struct ipv6_hdr_t *ipv6_hdr)
{
	return (BE_TO_CPU32(ipv6_hdr->ver_tclass_flowlabel) &
	       IPV6_HDR_VERSION_M) >> IPV6_HDR_VERSION_S;
}

static inline A_UINT8 ipv6_traffic_class(struct ipv6_hdr_t *ipv6_hdr)
{
	return (A_UINT8)((BE_TO_CPU32(ipv6_hdr->ver_tclass_flowlabel) &
	       IPV6_HDR_TRAFFIC_CLASS_M) >> IPV6_HDR_TRAFFIC_CLASS_S);
}

static inline A_UINT32 ipv6_flow_label(struct ipv6_hdr_t *ipv6_hdr)
{
	return (BE_TO_CPU32(ipv6_hdr->ver_tclass_flowlabel) &
	       IPV6_HDR_FLOW_LABEL_M) >> IPV6_HDR_FLOW_LABEL_S;
}

#endif /* _IPV6__H_ */
