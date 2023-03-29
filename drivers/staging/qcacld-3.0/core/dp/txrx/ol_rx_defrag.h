/*
 * Copyright (c) 2011-2017, 2019 The Linux Foundation. All rights reserved.
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

#ifndef _OL_RX_DEFRAG_H_
#define _OL_RX_DEFRAG_H_

#include <qdf_nbuf.h>
#include <cds_ieee80211_common.h>
#include <qdf_util.h>
#include <qdf_types.h>
#include <qdf_mem.h>
#include <ol_txrx_internal.h>
#include <ol_txrx_dbg.h>

#define DEFRAG_IEEE80211_KEY_LEN     8
#define DEFRAG_IEEE80211_FCS_LEN     4

struct ol_rx_defrag_cipher {
	const char *ic_name;
	uint16_t ic_header;
	uint8_t ic_trailer;
	uint8_t ic_miclen;
};

enum {
	OL_RX_DEFRAG_ERR,
	OL_RX_DEFRAG_OK,
	OL_RX_DEFRAG_PN_ERR
};

#define ol_rx_defrag_copydata(buf, offset, len, _to) \
	qdf_nbuf_copy_bits(buf, offset, len, _to)

#define ol_rx_defrag_len(buf) \
	qdf_nbuf_len(buf)

void
ol_rx_fraglist_insert(htt_pdev_handle htt_pdev,
		      qdf_nbuf_t *head_addr,
		      qdf_nbuf_t *tail_addr,
		      qdf_nbuf_t frag, uint8_t *all_frag_present);

void ol_rx_defrag_waitlist_add(struct ol_txrx_peer_t *peer, unsigned int tid);

void ol_rx_defrag_waitlist_remove(struct ol_txrx_peer_t *peer,
				  unsigned int tid);

void ol_rx_defrag_waitlist_flush(struct ol_txrx_pdev_t *pdev);

void
ol_rx_defrag(ol_txrx_pdev_handle pdev,
	     struct ol_txrx_peer_t *peer, unsigned int tid,
	     qdf_nbuf_t frag_list);

int
ol_rx_frag_tkip_decap(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t msdu, uint16_t hdrlen);

int
ol_rx_frag_wep_decap(ol_txrx_pdev_handle pdev,
		     qdf_nbuf_t nbuf, uint16_t hdrlen);

void ol_rx_defrag_nwifi_to_8023(ol_txrx_pdev_handle pdev, qdf_nbuf_t msdu);

void
ol_rx_defrag_qos_decap(ol_txrx_pdev_handle pdev,
		       qdf_nbuf_t nbuf, uint16_t hdrlen);

int
ol_rx_frag_tkip_demic(ol_txrx_pdev_handle pdev,
		      const uint8_t *key, qdf_nbuf_t msdu, uint16_t hdrlen);

int
ol_rx_frag_ccmp_decap(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t nbuf, uint16_t hdrlen);

int
ol_rx_frag_ccmp_demic(ol_txrx_pdev_handle pdev,
		      qdf_nbuf_t wbuf, uint16_t hdrlen);

uint16_t ol_rx_frag_hdrsize(const void *data);

void ol_rx_defrag_michdr(const struct ieee80211_frame *wh0, uint8_t hdr[]);

void
ol_rx_reorder_store_frag(ol_txrx_pdev_handle pdev,
			 struct ol_txrx_peer_t *peer,
			 unsigned int tid, uint16_t seq_num, qdf_nbuf_t frag);

qdf_nbuf_t
ol_rx_defrag_decap_recombine(htt_pdev_handle htt_pdev,
			     qdf_nbuf_t frag_list, uint16_t hdrsize);

int
ol_rx_defrag_mic(ol_txrx_pdev_handle pdev,
		 const uint8_t *key,
		 qdf_nbuf_t wbuf,
		 uint16_t off, uint16_t data_len, uint8_t mic[]);

void
ol_rx_reorder_flush_frag(htt_pdev_handle htt_pdev,
			 struct ol_txrx_peer_t *peer,
			 unsigned int tid, uint16_t seq_num);

static inline void xor_block(uint8_t *b, const uint8_t *a, qdf_size_t len)
{
	qdf_size_t i;

	for (i = 0; i < len; i++)
		b[i] ^= a[i];
}

static inline uint32_t rotl(uint32_t val, int bits)
{
	return (val << bits) | (val >> (32 - bits));
}

static inline uint32_t rotr(uint32_t val, int bits)
{
	return (val >> bits) | (val << (32 - bits));
}

static inline uint32_t xswap(uint32_t val)
{
	return ((val & 0x00ff00ff) << 8) | ((val & 0xff00ff00) >> 8);
}

static inline uint32_t
get_le32_split(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static inline uint32_t get_le32(const uint8_t *p)
{
	return get_le32_split(p[0], p[1], p[2], p[3]);
}

static inline void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (v) & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

static inline uint8_t ol_rx_defrag_concat(qdf_nbuf_t dst, qdf_nbuf_t src)
{
	/*
	 * Inside qdf_nbuf_cat, if it is necessary to reallocate dst
	 * to provide space for src, the headroom portion is copied from
	 * the original dst buffer to the larger new dst buffer.
	 * (This is needed, because the headroom of the dst buffer
	 * contains the rx desc.)
	 */
	if (qdf_nbuf_cat(dst, src))
		return OL_RX_DEFRAG_ERR;

	/* Free source buffer */
	qdf_nbuf_free(src);

	return OL_RX_DEFRAG_OK;
}

#define michael_block(l, r)	\
	do {					\
		r ^= rotl(l, 17);	\
		l += r;				\
		r ^= xswap(l);		\
		l += r;				\
		r ^= rotl(l, 3);	\
		l += r;				\
		r ^= rotr(l, 2);	\
		l += r;				\
	} while (0)

#endif
