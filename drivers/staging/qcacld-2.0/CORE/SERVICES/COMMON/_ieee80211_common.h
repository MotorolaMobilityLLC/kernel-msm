/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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


#ifndef _COMMON__IEEE80211_H_
#define _COMMON__IEEE80211_H_

/* These defines should match the table from ah_internal.h */
typedef enum {
	DFS_UNINIT_DOMAIN	= 0,	/* Uninitialized dfs domain */
	DFS_FCC_DOMAIN		= 1,	/* FCC3 dfs domain */
	DFS_ETSI_DOMAIN		= 2,	/* ETSI dfs domain */
	DFS_MKK4_DOMAIN		= 3	/* Japan dfs domain */
}HAL_DFS_DOMAIN;

/* XXX not really a mode; there are really multiple PHY's */
enum ieee80211_phymode {
    IEEE80211_MODE_AUTO             = 0,    /* autoselect */
    IEEE80211_MODE_11A              = 1,    /* 5GHz, OFDM */
    IEEE80211_MODE_11B              = 2,    /* 2GHz, CCK */
    IEEE80211_MODE_11G              = 3,    /* 2GHz, OFDM */
    IEEE80211_MODE_FH               = 4,    /* 2GHz, GFSK */
    IEEE80211_MODE_TURBO_A          = 5,    /* 5GHz, OFDM, 2x clock dynamic turbo */
    IEEE80211_MODE_TURBO_G          = 6,    /* 2GHz, OFDM, 2x clock dynamic turbo */
    IEEE80211_MODE_11NA_HT20        = 7,    /* 5Ghz, HT20 */
    IEEE80211_MODE_11NG_HT20        = 8,    /* 2Ghz, HT20 */
    IEEE80211_MODE_11NA_HT40PLUS    = 9,    /* 5Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NA_HT40MINUS   = 10,   /* 5Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40PLUS    = 11,   /* 2Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NG_HT40MINUS   = 12,   /* 2Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40        = 13,   /* 2Ghz, Auto HT40 */
    IEEE80211_MODE_11NA_HT40        = 14,   /* 2Ghz, Auto HT40 */
    IEEE80211_MODE_11AC_VHT20       = 15,   /* 5Ghz, VHT20 */
    IEEE80211_MODE_11AC_VHT40PLUS   = 16,   /* 5Ghz, VHT40 (Ext ch +1) */
    IEEE80211_MODE_11AC_VHT40MINUS  = 17,   /* 5Ghz  VHT40 (Ext ch -1) */
    IEEE80211_MODE_11AC_VHT40       = 18,   /* 5Ghz, VHT40 */
    IEEE80211_MODE_11AC_VHT80       = 19,   /* 5Ghz, VHT80 */
    IEEE80211_MODE_2G_AUTO          = 20,    /* 2G 11 b/g/n  autoselect */
    IEEE80211_MODE_5G_AUTO          = 21,    /* 5G 11 a/n/ac autoselect */
};
#define IEEE80211_MODE_MAX      (IEEE80211_MODE_11AC_VHT80 + 1)

enum ieee80211_opmode {
    IEEE80211_M_STA         = 1,                 /* infrastructure station */
    IEEE80211_M_IBSS        = 0,                 /* IBSS (adhoc) station */
    IEEE80211_M_AHDEMO      = 3,                 /* Old lucent compatible adhoc demo */
    IEEE80211_M_HOSTAP      = 6,                 /* Software Access Point */
    IEEE80211_M_MONITOR     = 8,                 /* Monitor mode */
    IEEE80211_M_WDS         = 2,                 /* WDS link */
    IEEE80211_M_BTAMP       = 9,                 /* VAP for BT AMP */

    IEEE80211_M_P2P_GO      = 33,                /* P2P GO */
    IEEE80211_M_P2P_CLIENT  = 34,                /* P2P Client */
    IEEE80211_M_P2P_DEVICE  = 35,                /* P2P Device */


    IEEE80211_OPMODE_MAX    = IEEE80211_M_BTAMP, /* Highest numbered opmode in the list */

    IEEE80211_M_ANY         = 0xFF               /* Any of the above; used by NDIS 6.x */
};

/*
 * 802.11n
 */
#define IEEE80211_CWM_EXTCH_BUSY_THRESHOLD 30

enum ieee80211_cwm_mode {
    IEEE80211_CWM_MODE20,
    IEEE80211_CWM_MODE2040,
    IEEE80211_CWM_MODE40,
    IEEE80211_CWM_MODEMAX

};

enum ieee80211_cwm_extprotspacing {
    IEEE80211_CWM_EXTPROTSPACING20,
    IEEE80211_CWM_EXTPROTSPACING25,
    IEEE80211_CWM_EXTPROTSPACINGMAX
};

enum ieee80211_cwm_width {
    IEEE80211_CWM_WIDTH20,
    IEEE80211_CWM_WIDTH40,
    IEEE80211_CWM_WIDTH80,
    IEEE80211_CWM_WIDTHINVALID = 0xff    /* user invalid value */
};

enum ieee80211_cwm_extprotmode {
    IEEE80211_CWM_EXTPROTNONE,      /* no protection */
    IEEE80211_CWM_EXTPROTCTSONLY,   /* CTS to self */
    IEEE80211_CWM_EXTPROTRTSCTS,    /* RTS-CTS */
    IEEE80211_CWM_EXTPROTMAX
};

enum ieee80211_fixed_rate_mode {
    IEEE80211_FIXED_RATE_NONE  = 0,
    IEEE80211_FIXED_RATE_MCS   = 1,  /* HT rates */
    IEEE80211_FIXED_RATE_LEGACY  = 2,   /* legacy rates */
    IEEE80211_FIXED_RATE_VHT  = 3   /* VHT rates */
};

/* Holds the fixed rate information for each VAP */
struct ieee80211_fixed_rate {
    enum ieee80211_fixed_rate_mode  mode;
    u_int32_t                       series;
    u_int32_t                       retries;
};

/*
 * 802.11g protection mode.
 */
enum ieee80211_protmode {
    IEEE80211_PROT_NONE     = 0,    /* no protection */
    IEEE80211_PROT_CTSONLY  = 1,    /* CTS to self */
    IEEE80211_PROT_RTSCTS   = 2,    /* RTS-CTS */
};

/*
 * Roaming mode is effectively who controls the operation
 * of the 802.11 state machine when operating as a station.
 * State transitions are controlled either by the driver
 * (typically when management frames are processed by the
 * hardware/firmware), the host (auto/normal operation of
 * the 802.11 layer), or explicitly through ioctl requests
 * when applications like wpa_supplicant want control.
 */
enum ieee80211_roamingmode {
    IEEE80211_ROAMING_DEVICE= 0,    /* driver/hardware control */
    IEEE80211_ROAMING_AUTO  = 1,    /* 802.11 layer control */
    IEEE80211_ROAMING_MANUAL= 2,    /* application control */
};

/*
 * Scanning mode controls station scanning work; this is
 * used only when roaming mode permits the host to select
 * the bss to join/channel to use.
 */
enum ieee80211_scanmode {
    IEEE80211_SCAN_DEVICE   = 0,    /* driver/hardware control */
    IEEE80211_SCAN_BEST     = 1,    /* 802.11 layer selects best */
    IEEE80211_SCAN_FIRST    = 2,    /* take first suitable candidate */
};

#define	IEEE80211_NWID_LEN	32
#define IEEE80211_CHAN_MAX      255
#define IEEE80211_CHAN_BYTES    32      /* howmany(IEEE80211_CHAN_MAX, NBBY) */
#define IEEE80211_CHAN_ANY      (-1)    /* token for ``any channel'' */
#define IEEE80211_CHAN_ANYC \
        ((struct ieee80211_channel *) IEEE80211_CHAN_ANY)

#define IEEE80211_CHAN_DEFAULT          11
#define IEEE80211_CHAN_DEFAULT_11A      52
#define IEEE80211_CHAN_ADHOC_DEFAULT1   10
#define IEEE80211_CHAN_ADHOC_DEFAULT2   11

#define IEEE80211_RADAR_11HCOUNT        5
#define IEEE80211_RADAR_TEST_MUTE_CHAN_11A      36      /* Move to channel 36 for mute test */
#define IEEE80211_RADAR_TEST_MUTE_CHAN_11NHT20  36
#define IEEE80211_RADAR_TEST_MUTE_CHAN_11NHT40U 36
#define IEEE80211_RADAR_TEST_MUTE_CHAN_11NHT40D 40      /* Move to channel 40 for HT40D mute test */
#define IEEE80211_RADAR_DETECT_DEFAULT_DELAY    60000   /* STA ignore AP beacons during this period in millisecond */

#define IEEE80211_2GCSA_TBTTCOUNT        3

/* bits 0-3 are for private use by drivers */
/* channel attributes */
#define IEEE80211_CHAN_TURBO            0x00000010 /* Turbo channel */
#define IEEE80211_CHAN_CCK              0x00000020 /* CCK channel */
#define IEEE80211_CHAN_OFDM             0x00000040 /* OFDM channel */
#define IEEE80211_CHAN_2GHZ             0x00000080 /* 2 GHz spectrum channel. */
#define IEEE80211_CHAN_5GHZ             0x00000100 /* 5 GHz spectrum channel */
#define IEEE80211_CHAN_PASSIVE          0x00000200 /* Only passive scan allowed */
#define IEEE80211_CHAN_DYN              0x00000400 /* Dynamic CCK-OFDM channel */
#define IEEE80211_CHAN_GFSK             0x00000800 /* GFSK channel (FHSS PHY) */
#define IEEE80211_CHAN_RADAR_DFS        0x00001000 /* Radar found on channel */
#define IEEE80211_CHAN_STURBO           0x00002000 /* 11a static turbo channel only */
#define IEEE80211_CHAN_HALF             0x00004000 /* Half rate channel */
#define IEEE80211_CHAN_QUARTER          0x00008000 /* Quarter rate channel */
#define IEEE80211_CHAN_HT20             0x00010000 /* HT 20 channel */
#define IEEE80211_CHAN_HT40PLUS         0x00020000 /* HT 40 with extension channel above */
#define IEEE80211_CHAN_HT40MINUS        0x00040000 /* HT 40 with extension channel below */
#define IEEE80211_CHAN_HT40INTOL        0x00080000 /* HT 40 Intolerant */
#define IEEE80211_CHAN_VHT20            0x00100000 /* VHT 20 channel */
#define IEEE80211_CHAN_VHT40PLUS        0x00200000 /* VHT 40 with extension channel above */
#define IEEE80211_CHAN_VHT40MINUS       0x00400000 /* VHT 40 with extension channel below */
#define IEEE80211_CHAN_VHT80            0x00800000 /* VHT 80 channel */

/* flagext */
#define	IEEE80211_CHAN_RADAR_FOUND    0x01
#define IEEE80211_CHAN_DFS              0x0002  /* DFS required on channel */
#define IEEE80211_CHAN_DFS_CLEAR        0x0008  /* if channel has been checked for DFS */
#define IEEE80211_CHAN_11D_EXCLUDED     0x0010  /* excluded in 11D */
#define IEEE80211_CHAN_CSA_RECEIVED     0x0020  /* Channel Switch Announcement received on this channel */
#define IEEE80211_CHAN_DISALLOW_ADHOC   0x0040  /* ad-hoc is not allowed */
#define IEEE80211_CHAN_DISALLOW_HOSTAP  0x0080  /* Station only channel */

/*
 * Useful combinations of channel characteristics.
 */
#define IEEE80211_CHAN_FHSS \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_GFSK)
#define IEEE80211_CHAN_A \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_B \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define IEEE80211_CHAN_PUREG \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_G \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)
#define IEEE80211_CHAN_108A \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)
#define IEEE80211_CHAN_108G \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)
#define IEEE80211_CHAN_ST \
    (IEEE80211_CHAN_108A | IEEE80211_CHAN_STURBO)

#define IEEE80211_IS_CHAN_11AC_2G(_c) \
    (IEEE80211_IS_CHAN_2GHZ((_c)) && IEEE80211_IS_CHAN_VHT((_c)))
#define IEEE80211_CHAN_11AC_VHT20_2G \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_VHT20)
#define IEEE80211_CHAN_11AC_VHT40_2G \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS)
#define IEEE80211_CHAN_11AC_VHT80_2G \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_VHT80)

#define IEEE80211_IS_CHAN_11AC_VHT20_2G(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT20_2G) == IEEE80211_CHAN_11AC_VHT20_2G)
#define IEEE80211_IS_CHAN_11AC_VHT40_2G(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT40_2G) != 0)
#define IEEE80211_IS_CHAN_11AC_VHT80_2G(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT80_2G) == IEEE80211_CHAN_11AC_VHT80_2G)

#define IEEE80211_CHAN_11NG_HT20 \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_HT20)
#define IEEE80211_CHAN_11NA_HT20 \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_HT20)
#define IEEE80211_CHAN_11NG_HT40PLUS \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_HT40PLUS)
#define IEEE80211_CHAN_11NG_HT40MINUS \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_HT40MINUS)
#define IEEE80211_CHAN_11NA_HT40PLUS \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_HT40PLUS)
#define IEEE80211_CHAN_11NA_HT40MINUS \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_HT40MINUS)

#define IEEE80211_CHAN_ALL \
    (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_GFSK | \
    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_DYN | \
    IEEE80211_CHAN_HT20 | IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS | \
    IEEE80211_CHAN_VHT20 | IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS | IEEE80211_CHAN_VHT80 | \
    IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER)
#define IEEE80211_CHAN_ALLTURBO \
    (IEEE80211_CHAN_ALL | IEEE80211_CHAN_TURBO | IEEE80211_CHAN_STURBO)

#define IEEE80211_IS_CHAN_FHSS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define IEEE80211_IS_CHAN_A(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define IEEE80211_IS_CHAN_B(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define IEEE80211_IS_CHAN_PUREG(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define IEEE80211_IS_CHAN_G(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define IEEE80211_IS_CHAN_ANYG(_c) \
    (IEEE80211_IS_CHAN_PUREG(_c) || IEEE80211_IS_CHAN_G(_c))
#define IEEE80211_IS_CHAN_ST(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_ST) == IEEE80211_CHAN_ST)
#define IEEE80211_IS_CHAN_108A(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_108A) == IEEE80211_CHAN_108A)
#define IEEE80211_IS_CHAN_108G(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_108G) == IEEE80211_CHAN_108G)

#define IEEE80211_IS_CHAN_2GHZ(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_2GHZ) != 0)
#define IEEE80211_IS_CHAN_5GHZ(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_5GHZ) != 0)
#define IEEE80211_IS_CHAN_OFDM(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_OFDM) != 0)
#define IEEE80211_IS_CHAN_CCK(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_CCK) != 0)
#define IEEE80211_IS_CHAN_GFSK(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_GFSK) != 0)
#define IEEE80211_IS_CHAN_TURBO(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_TURBO) != 0)
#define IEEE80211_IS_CHAN_WEATHER_RADAR(_c) \
    ((((_c)->ic_freq >= 5600) && ((_c)->ic_freq <= 5650)) \
     || (((_c)->ic_flags & IEEE80211_CHAN_HT40PLUS) && (5580 == (_c)->ic_freq)))
#define IEEE80211_IS_CHAN_STURBO(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_STURBO) != 0)
#define IEEE80211_IS_CHAN_DTURBO(_c) \
    (((_c)->ic_flags & \
    (IEEE80211_CHAN_TURBO | IEEE80211_CHAN_STURBO)) == IEEE80211_CHAN_TURBO)
#define IEEE80211_IS_CHAN_HALF(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HALF) != 0)
#define IEEE80211_IS_CHAN_QUARTER(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_QUARTER) != 0)
#define IEEE80211_IS_CHAN_PASSIVE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_PASSIVE) != 0)

#define IEEE80211_IS_CHAN_DFS(_c) \
    (((_c)->ic_flagext & (IEEE80211_CHAN_DFS|IEEE80211_CHAN_DFS_CLEAR)) == IEEE80211_CHAN_DFS)
#define IEEE80211_IS_CHAN_DFSFLAG(_c) \
    (((_c)->ic_flagext & IEEE80211_CHAN_DFS) == IEEE80211_CHAN_DFS)
#define IEEE80211_IS_CHAN_DISALLOW_ADHOC(_c) \
    (((_c)->ic_flagext & IEEE80211_CHAN_DISALLOW_ADHOC) != 0)
#define IEEE80211_IS_CHAN_11D_EXCLUDED(_c) \
    (((_c)->ic_flagext & IEEE80211_CHAN_11D_EXCLUDED) != 0)
#define IEEE80211_IS_CHAN_CSA(_c) \
    (((_c)->ic_flagext & IEEE80211_CHAN_CSA_RECEIVED) != 0)
#define IEEE80211_IS_CHAN_ODD(_c) \
    (((_c)->ic_freq == 5170) || ((_c)->ic_freq == 5190) || \
     ((_c)->ic_freq == 5210) || ((_c)->ic_freq == 5230))
#define IEEE80211_IS_CHAN_DISALLOW_HOSTAP(_c) \
    (((_c)->ic_flagext & IEEE80211_CHAN_DISALLOW_HOSTAP) != 0)

#define IEEE80211_IS_CHAN_11NG_HT20(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NG_HT20) == IEEE80211_CHAN_11NG_HT20)
#define IEEE80211_IS_CHAN_11NA_HT20(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NA_HT20) == IEEE80211_CHAN_11NA_HT20)
#define IEEE80211_IS_CHAN_11NG_HT40PLUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NG_HT40PLUS) == IEEE80211_CHAN_11NG_HT40PLUS)
#define IEEE80211_IS_CHAN_11NG_HT40MINUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NG_HT40MINUS) == IEEE80211_CHAN_11NG_HT40MINUS)
#define IEEE80211_IS_CHAN_11NA_HT40PLUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NA_HT40PLUS) == IEEE80211_CHAN_11NA_HT40PLUS)
#define IEEE80211_IS_CHAN_11NA_HT40MINUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11NA_HT40MINUS) == IEEE80211_CHAN_11NA_HT40MINUS)

#define IEEE80211_IS_CHAN_11N(_c) \
    (((_c)->ic_flags & (IEEE80211_CHAN_HT20 | IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS)) != 0)
#define IEEE80211_IS_CHAN_11N_HT20(_c) \
    (((_c)->ic_flags & (IEEE80211_CHAN_HT20)) != 0)
#define IEEE80211_IS_CHAN_11N_HT40(_c) \
    (((_c)->ic_flags & (IEEE80211_CHAN_HT40PLUS | IEEE80211_CHAN_HT40MINUS)) != 0)
#define IEEE80211_IS_CHAN_11NG(_c) \
    (IEEE80211_IS_CHAN_2GHZ((_c)) && IEEE80211_IS_CHAN_11N((_c)))
#define IEEE80211_IS_CHAN_11NA(_c) \
    (IEEE80211_IS_CHAN_5GHZ((_c)) && IEEE80211_IS_CHAN_11N((_c)))
#define IEEE80211_IS_CHAN_11N_HT40PLUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40PLUS) != 0)
#define IEEE80211_IS_CHAN_11N_HT40MINUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40MINUS) != 0)

#define IEEE80211_IS_CHAN_HT20_CAPABLE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT20) == IEEE80211_CHAN_HT20)
#define IEEE80211_IS_CHAN_HT40PLUS_CAPABLE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40PLUS) == IEEE80211_CHAN_HT40PLUS)
#define IEEE80211_IS_CHAN_HT40MINUS_CAPABLE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40MINUS) == IEEE80211_CHAN_HT40MINUS)
#define IEEE80211_IS_CHAN_HT40_CAPABLE(_c) \
    (IEEE80211_IS_CHAN_HT40PLUS_CAPABLE(_c) || IEEE80211_IS_CHAN_HT40MINUS_CAPABLE(_c))
#define IEEE80211_IS_CHAN_HT_CAPABLE(_c) \
    (IEEE80211_IS_CHAN_HT20_CAPABLE(_c) || IEEE80211_IS_CHAN_HT40_CAPABLE(_c))
#define IEEE80211_IS_CHAN_11N_CTL_CAPABLE(_c)  IEEE80211_IS_CHAN_HT20_CAPABLE(_c)
#define IEEE80211_IS_CHAN_11N_CTL_U_CAPABLE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40PLUS) == IEEE80211_CHAN_HT40PLUS)
#define IEEE80211_IS_CHAN_11N_CTL_L_CAPABLE(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40MINUS) == IEEE80211_CHAN_HT40MINUS)
#define IEEE80211_IS_CHAN_11N_CTL_40_CAPABLE(_c) \
    (IEEE80211_IS_CHAN_11N_CTL_U_CAPABLE((_c)) || IEEE80211_IS_CHAN_11N_CTL_L_CAPABLE((_c)))


#define IEEE80211_IS_CHAN_VHT(_c) \
    (((_c)->ic_flags & (IEEE80211_CHAN_VHT20 | \
      IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS | IEEE80211_CHAN_VHT80)) != 0)
#define IEEE80211_IS_CHAN_11AC(_c) \
    ( IEEE80211_IS_CHAN_5GHZ((_c)) && IEEE80211_IS_CHAN_VHT((_c)) )
#define IEEE80211_CHAN_11AC_VHT20 \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT20)
#define IEEE80211_CHAN_11AC_VHT40 \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS )
#define IEEE80211_CHAN_11AC_VHT40PLUS \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT40PLUS)
#define IEEE80211_CHAN_11AC_VHT40MINUS \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT40MINUS)
#define IEEE80211_CHAN_11AC_VHT80 \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT80)
#define IEEE80211_IS_CHAN_11AC_VHT20(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT20) == IEEE80211_CHAN_11AC_VHT20)

#define IEEE80211_IS_CHAN_11AC_VHT40(_c) \
    (((_c)->ic_flags & (IEEE80211_CHAN_VHT40PLUS | IEEE80211_CHAN_VHT40MINUS)) !=0)
#define IEEE80211_IS_CHAN_11AC_VHT40PLUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT40PLUS) == IEEE80211_CHAN_11AC_VHT40PLUS)
#define IEEE80211_IS_CHAN_11AC_VHT40MINUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT40MINUS) == IEEE80211_CHAN_11AC_VHT40MINUS)
#define IEEE80211_IS_CHAN_11AC_VHT80(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT80) == IEEE80211_CHAN_11AC_VHT80)

#define IEEE80211_IS_CHAN_RADAR(_c)    \
    (((_c)->ic_flags & IEEE80211_CHAN_RADAR_DFS) == IEEE80211_CHAN_RADAR_DFS)
#define IEEE80211_CHAN_SET_RADAR(_c)    \
    ((_c)->ic_flags |= IEEE80211_CHAN_RADAR_DFS)
#define IEEE80211_CHAN_CLR_RADAR(_c)    \
    ((_c)->ic_flags &= ~IEEE80211_CHAN_RADAR_DFS)
#define IEEE80211_CHAN_SET_DISALLOW_ADHOC(_c)   \
    ((_c)->ic_flagext |= IEEE80211_CHAN_DISALLOW_ADHOC)
#define IEEE80211_CHAN_SET_DISALLOW_HOSTAP(_c)   \
    ((_c)->ic_flagext |= IEEE80211_CHAN_DISALLOW_HOSTAP)
#define IEEE80211_CHAN_SET_DFS(_c)  \
    ((_c)->ic_flagext |= IEEE80211_CHAN_DFS)
#define IEEE80211_CHAN_SET_DFS_CLEAR(_c)  \
    ((_c)->ic_flagext |= IEEE80211_CHAN_DFS_CLEAR)
#define IEEE80211_CHAN_EXCLUDE_11D(_c)  \
    ((_c)->ic_flagext |= IEEE80211_CHAN_11D_EXCLUDED)

/* channel encoding for FH phy */
#define IEEE80211_FH_CHANMOD            80
#define IEEE80211_FH_CHAN(set,pat)      (((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define IEEE80211_FH_CHANSET(chan)      ((chan)/IEEE80211_FH_CHANMOD+1)
#define IEEE80211_FH_CHANPAT(chan)      ((chan)%IEEE80211_FH_CHANMOD)

/*
 * 802.11 rate set.
 */
#define IEEE80211_RATE_SIZE     8               /* 802.11 standard */
#define IEEE80211_RATE_MAXSIZE  36              /* max rates we'll handle */
#define IEEE80211_HT_RATE_SIZE  128
#define IEEE80211_RATE_SINGLE_STREAM_MCS_MAX     7  /* MCS7 */

#define IEEE80211_RATE_MCS      0x8000
#define IEEE80211_RATE_MCS_VAL  0x7FFF

#define IEEE80211_RATE_IDX_ENTRY(val, idx) (((val&(0xff<<(idx*8)))>>(idx*8)))

/*
 * RSSI range
 */
#define IEEE80211_RSSI_MAX           -10   /* in db */
#define IEEE80211_RSSI_MIN           -200

/*
 * 11n A-MPDU & A-MSDU limits
 */
#define IEEE80211_AMPDU_LIMIT_MIN           (1 * 1024)
#define IEEE80211_AMPDU_LIMIT_MAX           (64 * 1024 - 1)
#define IEEE80211_AMPDU_LIMIT_DEFAULT       IEEE80211_AMPDU_LIMIT_MAX
#define IEEE80211_AMPDU_SUBFRAME_MIN        2
#define IEEE80211_AMPDU_SUBFRAME_MAX        64
#define IEEE80211_AMPDU_SUBFRAME_DEFAULT    32
#define IEEE80211_AMSDU_LIMIT_MAX           4096
#define IEEE80211_RIFS_AGGR_DIV             10
#define IEEE80211_MAX_AMPDU_MIN             0
#define IEEE80211_MAX_AMPDU_MAX             3

/*
 * 11ac A-MPDU limits
 */
#define IEEE80211_VHT_MAX_AMPDU_MIN         0
#define IEEE80211_VHT_MAX_AMPDU_MAX         7



struct ieee80211_rateset {
    u_int8_t                rs_nrates;
    u_int8_t                rs_rates[IEEE80211_RATE_MAXSIZE];
};

struct ieee80211_beacon_info{
    u_int8_t    essid[IEEE80211_NWID_LEN+1];
    u_int8_t    esslen;
    u_int8_t	rssi_ctl_0;
    u_int8_t	rssi_ctl_1;
    u_int8_t	rssi_ctl_2;
    int         numchains;
};

struct ieee80211_ibss_peer_list{
    u_int8_t    bssid[6];
};

struct ieee80211_roam {
    int8_t                  rssi11a;        /* rssi thresh for 11a bss */
    int8_t                  rssi11b;        /* for 11g sta in 11b bss */
    int8_t                  rssi11bOnly;    /* for 11b sta */
    u_int8_t                pad1;
    u_int8_t                rate11a;        /* rate thresh for 11a bss */
    u_int8_t                rate11b;        /* for 11g sta in 11b bss */
    u_int8_t                rate11bOnly;    /* for 11b sta */
    u_int8_t                pad2;
};

#define IEEE80211_TID_SIZE      17 /* total number of TIDs */
#define IEEE80211_NON_QOS_SEQ   16 /* index for non-QoS (including management) sequence number space */
#define IEEE80211_SEQ_MASK      0xfff   /* sequence generator mask*/
#define MIN_SW_SEQ              0x100   /* minimum sequence for SW generate packect*/

#define IEEE80211_ADDR_LEN  6       /* size of 802.11 address */

/* crypto related defines*/
#define IEEE80211_KEYBUF_SIZE   16
#define IEEE80211_MICBUF_SIZE   (8+8)   /* space for both tx+rx keys */

enum ieee80211_clist_cmd {
    CLIST_UPDATE,
    CLIST_DFS_UPDATE,
    CLIST_NEW_COUNTRY,
    CLIST_NOL_UPDATE
};

enum ieee80211_nawds_param {
    IEEE80211_NAWDS_PARAM_NUM = 0,
    IEEE80211_NAWDS_PARAM_MODE,
    IEEE80211_NAWDS_PARAM_DEFCAPS,
    IEEE80211_NAWDS_PARAM_OVERRIDE,
};

struct ieee80211_mib_cycle_cnts {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int8_t    is_rx_active;
    u_int8_t    is_tx_active;
};

struct ieee80211_chanutil_info {
    u_int32_t    rx_clear_count;
    u_int32_t    cycle_count;
    u_int8_t     value;
    u_int32_t    beacon_count;
    u_int8_t     beacon_intervals;
};

#endif /* _COMMON__IEEE80211_H_ */
