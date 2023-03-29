/*
 * Copyright (c) 2013-2017,2019 The Linux Foundation. All rights reserved.
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

#ifndef CDS_COMMON__IEEE80211_I_H_
#define CDS_COMMON__IEEE80211_I_H_

/**
 * enum ieee80211_phymode - not really a mode; there are really multiple PHY's
 * @IEEE80211_MODE_AUTO - autoselect
 * @IEEE80211_MODE_11A - 5GHz, OFDM
 * @IEEE80211_MODE_11B - 2GHz, CCK
 * @IEEE80211_MODE_11G - 2GHz, OFDM
 * @IEEE80211_MODE_FH - 2GHz, GFSK
 * @IEEE80211_MODE_TURBO_A - 5GHz, OFDM, 2x clock dynamic turbo
 * @IEEE80211_MODE_TURBO_G - 2GHz, OFDM, 2x clock dynamic turbo
 * @IEEE80211_MODE_11NA_HT20 - 5Ghz, HT20
 * @IEEE80211_MODE_11NG_HT20 - 2Ghz, HT20
 * @IEEE80211_MODE_11NA_HT40PLUS - 5Ghz, HT40 (ext ch +1)
 * @IEEE80211_MODE_11NA_HT40MINUS - 5Ghz, HT40 (ext ch -1)
 * @IEEE80211_MODE_11NG_HT40PLUS - 2Ghz, HT40 (ext ch +1)
 * @IEEE80211_MODE_11NG_HT40MINUS - 2Ghz, HT40 (ext ch -1)
 * @IEEE80211_MODE_11NG_HT40 - 2Ghz, Auto HT40
 * @IEEE80211_MODE_11NA_HT40 - 2Ghz, Auto HT40
 * @IEEE80211_MODE_11AC_VHT20 - 5Ghz, VHT20
 * @IEEE80211_MODE_11AC_VHT40PLUS - 5Ghz, VHT40 (Ext ch +1)
 * @IEEE80211_MODE_11AC_VHT40MINUS - 5Ghz VHT40 (Ext ch -1)
 * @IEEE80211_MODE_11AC_VHT40 - 5Ghz, VHT40
 * @IEEE80211_MODE_11AC_VHT80 - 5Ghz, VHT80
 * @IEEE80211_MODE_2G_AUTO - 2G 11 b/g/n autoselect
 * @IEEE80211_MODE_5G_AUTO - 5G 11 a/n/ac autoselect
 * @IEEE80211_MODE_11AGN - Support 11N in both 2G and 5G
 * @IEEE80211_MODE_11AX_HE20 - HE20
 * @IEEE80211_MODE_11AX_HE40 - HE40
 * @IEEE80211_MODE_11AX_HE40PLUS - HE40 (ext ch +1)
 * @IEEE80211_MODE_11AX_HE40MINUS - HE40 (ext ch -1)
 * @IEEE80211_MODE_11AX_HE80 - HE80
 * @IEEE80211_MODE_11AX_HE80P80 - HE 80P80
 * @IEEE80211_MODE_11AX_HE160 - HE160
 * @IEEE80211_MODE_MAX - Maximum possible value
 */
enum ieee80211_phymode {
	IEEE80211_MODE_AUTO = 0,
	IEEE80211_MODE_11A = 1,
	IEEE80211_MODE_11B = 2,
	IEEE80211_MODE_11G = 3,
	IEEE80211_MODE_FH = 4,
	IEEE80211_MODE_TURBO_A = 5,
	IEEE80211_MODE_TURBO_G = 6,
	IEEE80211_MODE_11NA_HT20 = 7,
	IEEE80211_MODE_11NG_HT20 = 8,
	IEEE80211_MODE_11NA_HT40PLUS = 9,
	IEEE80211_MODE_11NA_HT40MINUS = 10,
	IEEE80211_MODE_11NG_HT40PLUS = 11,
	IEEE80211_MODE_11NG_HT40MINUS = 12,
	IEEE80211_MODE_11NG_HT40 = 13,
	IEEE80211_MODE_11NA_HT40 = 14,
	IEEE80211_MODE_11AC_VHT20 = 15,
	IEEE80211_MODE_11AC_VHT40PLUS = 16,
	IEEE80211_MODE_11AC_VHT40MINUS = 17,
	IEEE80211_MODE_11AC_VHT40 = 18,
	IEEE80211_MODE_11AC_VHT80 = 19,
	IEEE80211_MODE_2G_AUTO = 20,
	IEEE80211_MODE_5G_AUTO = 21,
	IEEE80211_MODE_11AGN = 22,
	IEEE80211_MODE_11AX_HE20 = 23,
	IEEE80211_MODE_11AX_HE40 = 24,
	IEEE80211_MODE_11AX_HE40PLUS = 25,
	IEEE80211_MODE_11AX_HE40MINUS = 26,
	IEEE80211_MODE_11AX_HE80 = 27,
	IEEE80211_MODE_11AX_HE80P80 = 28,
	IEEE80211_MODE_11AX_HE160 = 29,

	/* Do not add after this line */
	IEEE80211_MODE_MAX = IEEE80211_MODE_11AX_HE160,
};

/*
 * 802.11g protection mode.
 */
enum ieee80211_protmode {
	IEEE80211_PROT_NONE = 0,        /* no protection */
	IEEE80211_PROT_CTSONLY = 1,     /* CTS to self */
	IEEE80211_PROT_RTSCTS = 2,      /* RTS-CTS */
};

/* bits 0-3 are for private use by drivers */
/* channel attributes */
#define IEEE80211_CHAN_TURBO            0x00000010      /* Turbo channel */
#define IEEE80211_CHAN_CCK              0x00000020      /* CCK channel */
#define IEEE80211_CHAN_OFDM             0x00000040      /* OFDM channel */
#define IEEE80211_CHAN_2GHZ             0x00000080      /* 2 GHz spectrum channel. */
#define IEEE80211_CHAN_5GHZ             0x00000100      /* 5 GHz spectrum channel */
#define IEEE80211_CHAN_PASSIVE          0x00000200      /* Only passive scan allowed */
#define IEEE80211_CHAN_DYN              0x00000400      /* Dynamic CCK-OFDM channel */
#define IEEE80211_CHAN_GFSK             0x00000800      /* GFSK channel (FHSS PHY) */
#define IEEE80211_CHAN_RADAR_DFS        0x00001000      /* Radar found on channel */
#define IEEE80211_CHAN_STURBO           0x00002000      /* 11a static turbo channel only */
#define IEEE80211_CHAN_HALF             0x00004000      /* Half rate channel */
#define IEEE80211_CHAN_QUARTER          0x00008000      /* Quarter rate channel */
#define IEEE80211_CHAN_HT20             0x00010000      /* HT 20 channel */
#define IEEE80211_CHAN_HT40PLUS         0x00020000      /* HT 40 with extension channel above */
#define IEEE80211_CHAN_HT40MINUS        0x00040000      /* HT 40 with extension channel below */
#define IEEE80211_CHAN_HT40INTOL        0x00080000      /* HT 40 Intolerant */
#define IEEE80211_CHAN_VHT20            0x00100000      /* VHT 20 channel */
#define IEEE80211_CHAN_VHT40PLUS        0x00200000      /* VHT 40 with extension channel above */
#define IEEE80211_CHAN_VHT40MINUS       0x00400000      /* VHT 40 with extension channel below */
#define IEEE80211_CHAN_VHT80            0x00800000      /* VHT 80 channel */
/* channel temporarily blocked due to noise */
#define IEEE80211_CHAN_BLOCKED          0x02000000
/* VHT 160 channel */
#define IEEE80211_CHAN_VHT160           0x04000000
/* VHT 80_80 channel */
#define IEEE80211_CHAN_VHT80_80         0x08000000

/* flagext */
#define IEEE80211_CHAN_DFS              0x0002  /* DFS required on channel */
/* DFS required on channel for 2nd band of 80+80*/
#define IEEE80211_CHAN_DFS_CFREQ2       0x0004

#define IEEE80211_SEQ_MASK      0xfff   /* sequence generator mask */
#define MIN_SW_SEQ              0x100   /* minimum sequence for SW generate packect */

#endif /* CDS_COMMON__IEEE80211_I_H_ */
