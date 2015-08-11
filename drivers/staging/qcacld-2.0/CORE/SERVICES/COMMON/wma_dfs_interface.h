/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#include "ath_dfs_structs.h"
#include "_ieee80211_common.h"
#include <vos_lock.h>
#define IEEE80211_CHAN_MAX      255

/* channel attributes */

/* Turbo channel */
#define IEEE80211_CHAN_TURBO            0x00000010
/* CCK channel */
#define IEEE80211_CHAN_CCK              0x00000020
/* OFDM channel */
#define IEEE80211_CHAN_OFDM             0x00000040
/* 2 GHz spectrum channel. */
#define IEEE80211_CHAN_2GHZ             0x00000080
/* 5 GHz spectrum channel */
#define IEEE80211_CHAN_5GHZ             0x00000100
/* Only passive scan allowed */
#define IEEE80211_CHAN_PASSIVE          0x00000200
/* Dynamic CCK-OFDM channel */
#define IEEE80211_CHAN_DYN              0x00000400
/* GFSK channel (FHSS PHY) */
#define IEEE80211_CHAN_GFSK             0x00000800
/* Radar found on channel */
#define IEEE80211_CHAN_RADAR            0x00001000
/* 11a static turbo channel only */
#define IEEE80211_CHAN_STURBO           0x00002000
/* Half rate channel */
#define IEEE80211_CHAN_HALF             0x00004000
/* Quarter rate channel */
#define IEEE80211_CHAN_QUARTER          0x00008000
/* HT 20 channel */
#define IEEE80211_CHAN_HT20             0x00010000
/* HT 40 with extension channel above */
#define IEEE80211_CHAN_HT40PLUS         0x00020000
/* HT 40 with extension channel below */
#define IEEE80211_CHAN_HT40MINUS        0x00040000
/* HT 40 Intolerant */
#define IEEE80211_CHAN_HT40INTOL        0x00080000
/* VHT 20 channel */
#define IEEE80211_CHAN_VHT20            0x00100000
/* VHT 40 with extension channel above */
#define IEEE80211_CHAN_VHT40PLUS        0x00200000
/* VHT 40 with extension channel below */
#define IEEE80211_CHAN_VHT40MINUS       0x00400000
/* VHT 80 channel */
#define IEEE80211_CHAN_VHT80            0x00800000

/* token for ``any channel'' */
#define IEEE80211_CHAN_ANY      (-1)
#define IEEE80211_CHAN_ANYC \
        ((struct ieee80211_channel *) IEEE80211_CHAN_ANY)


#define IEEE80211_IS_CHAN_11N_HT40MINUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40MINUS) != 0)
#define IEEE80211_IS_CHAN_11N_HT40PLUS(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_HT40PLUS) != 0)
#define IEEE80211_CHAN_11AC_VHT80 \
    (IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_VHT80)


#define IEEE80211_IS_CHAN_11AC_VHT80(_c) \
    (((_c)->ic_flags & IEEE80211_CHAN_11AC_VHT80) == \
                                    IEEE80211_CHAN_11AC_VHT80)
#define CHANNEL_108G \
    (IEEE80211_CHAN_2GHZ|IEEE80211_CHAN_OFDM|IEEE80211_CHAN_TURBO)

/*
 * Software use: channel interference
 * used for as AR as well as RADAR
 * interference detection
*/
#define CHANNEL_INTERFERENCE    0x01
/* In case of VHT160, we can have 8 20Mhz channels */
#define IEE80211_MAX_20M_SUB_CH 8

struct ieee80211_channel
{
    u_int32_t       ic_freq;        /* setting in Mhz */
    u_int32_t       ic_flags;       /* see below */
    u_int8_t        ic_flagext;     /* see below */
    u_int8_t        ic_ieee;        /* IEEE channel number */

    /* maximum regulatory tx power in dBm */
    int8_t          ic_maxregpower;

    int8_t          ic_maxpower;    /* maximum tx power in dBm */
    int8_t          ic_minpower;    /* minimum tx power in dBm */
    u_int8_t        ic_regClassId;  /* regClassId of this channel */
    u_int8_t        ic_antennamax;  /* antenna gain max from regulatory */
    u_int32_t       ic_vhtop_ch_freq_seg1; /* Channel Center frequency */

    /* Channel Center frequency applicable*/
    u_int32_t       ic_vhtop_ch_freq_seg2;
};

struct ieee80211_channel_list
{
    int                         cl_nchans;
    struct ieee80211_channel    *cl_channels[IEE80211_MAX_20M_SUB_CH];
};

struct ieee80211_dfs_state
{
    int                            nol_event[IEEE80211_CHAN_MAX];
    os_timer_t                     nol_timer; /* NOL list processing */
    os_timer_t                     cac_timer; /* CAC timer */
    int                            cureps;    /* current events/second */
    const struct ieee80211_channel *lastchan; /* chan w/ last radar event */
    struct ieee80211_channel       *newchan;  /* chan selected next */
    /* overridden cac timeout */
    int                            cac_timeout_override;
    u_int8_t                       enable:1,
                                   cac_timer_running:1,
                                   ignore_dfs:1,
                                   ignore_cac:1;
};

typedef struct ieee80211com
{
    void (*ic_start_csa)(struct ieee80211com *ic,u_int8_t ieeeChan);
    void (*ic_get_ext_chan_info)(struct ieee80211com *ic,
                                        struct ieee80211_channel_list *chan);
    enum ieee80211_opmode ic_opmode;  /* operation mode */
    struct ieee80211_channel *(*ic_find_channel)(struct ieee80211com *ic,
                                                 int freq, u_int32_t flags);
    u_int64_t (*ic_get_TSF64)(struct ieee80211com *ic);
    unsigned int (*ic_ieee2mhz)(u_int chan, u_int flags);
    struct ieee80211_channel ic_channels[IEEE80211_CHAN_MAX+1];
    /* Number of Channels according to the Regulatory domain channels */
    int ic_nchans;
    struct ieee80211_channel *ic_curchan;   /* current channel */
    u_int8_t ic_isdfsregdomain; /* operating in DFS domain ? */
    int (*ic_get_dfsdomain)(struct ieee80211com *);
    u_int16_t (*ic_dfs_usenol)(struct ieee80211com *ic);
    u_int16_t (*ic_dfs_isdfsregdomain)(struct ieee80211com *ic);
    int (*ic_dfs_attached)(struct ieee80211com *ic);
    void *ic_dfs;
    struct ieee80211_dfs_state ic_dfs_state;
    int (*ic_dfs_attach)(struct ieee80211com *ic,
                                       void *pCap, void *radar_info);
    int (*ic_dfs_detach)(struct ieee80211com *ic);
    int (*ic_dfs_enable)(struct ieee80211com *ic, int *is_fastclk, void *);
    int (*ic_dfs_disable)(struct ieee80211com *ic);
    int (*ic_get_ext_busy)(struct ieee80211com *ic);
    int (*ic_get_mib_cycle_counts_pct)(struct ieee80211com *ic,
          u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt);
    int (*ic_dfs_get_thresholds)(struct ieee80211com *ic,void *pe);

    int (*ic_dfs_debug)(struct ieee80211com *ic, int type, void *data);
    /*
     * Update the channel list with the current set of DFS
     * NOL entries.
     *
     * + 'cmd' indicates what to do; for now it should just
     *   be DFS_NOL_CLIST_CMD_UPDATE which will update all
     *   channels, given the _entire_ NOL. (Rather than
     *   the earlier behaviour with clist_update, which
     *   was to either add or remove a set of channel
     *   entries.)
     */
    void (*ic_dfs_clist_update)(struct ieee80211com *ic, int cmd,
                                struct dfs_nol_chan_entry *, int nentries);
    void (*ic_dfs_notify_radar)(struct ieee80211com *ic,
                                   struct ieee80211_channel *chan);
    void (*ic_dfs_unmark_radar)(struct ieee80211com *ic,
                                   struct ieee80211_channel *chan);
    int (*ic_dfs_control)(struct ieee80211com *ic,
                          u_int id, void *indata, u_int32_t insize,
                          void *outdata, u_int32_t *outsize);
    HAL_DFS_DOMAIN current_dfs_regdomain;
    u_int8_t vdev_id;
    u_int8_t last_radar_found_chan;
    int32_t dfs_pri_multiplier;
    vos_lock_t chan_lock;
} IEEE80211COM, *PIEEE80211COM;

/*
 * Convert channel to frequency value.
 */
static INLINE u_int
ieee80211_chan2freq(struct ieee80211com *ic,
                                 const struct ieee80211_channel *c)
{
    if (c == NULL)
    {
        return 0;
    }
    return (c == IEEE80211_CHAN_ANYC ?  IEEE80211_CHAN_ANY : c->ic_freq);
}
