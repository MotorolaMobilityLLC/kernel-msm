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

/*===========================================================================

                     wma_dfs_interface.c

  OVERVIEW:

  Source code borrowed from QCA_MAIN DFS module

  DEPENDENCIES:

  Are listed for each API below.

===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when        who     what, where, why
----------    ---    --------------------------------------------------------

===========================================================================*/



#include "wma.h"
#include "ath_dfs_structs.h"
#include "wma_dfs_interface.h"

#ifndef ATH_SUPPORT_DFS
#define ATH_SUPPORT_DFS 1
#endif

int
ol_if_dfs_attach(struct ieee80211com *ic, void *ptr, void *radar_info)
{
    struct ath_dfs_caps *pCap = (struct ath_dfs_caps *) ptr;

    adf_os_print("%s: called; ptr=%p, radar_info=%p\n",
                  __func__, ptr, radar_info);

    pCap->ath_chip_is_bb_tlv = 1;
    pCap->ath_dfs_combined_rssi_ok = 0;
    pCap->ath_dfs_ext_chan_ok = 0;
    pCap->ath_dfs_use_enhancement = 0;
    pCap->ath_strong_signal_diversiry = 0;
    pCap->ath_fastdiv_val = 0;

    return(0);
}

/*
 * Place Holder API
 * We get the tsf from Firmware.
 */
u_int64_t
ol_if_get_tsf64(struct ieee80211com *ic)
{
    return (0);
}

/*
 * ic_dfs_disable is just a place holder
 * function since firmware takes care of
 * disabling the dfs phyerrors disabling.
 */
int
ol_if_dfs_disable(struct ieee80211com *ic)
{
    return (0);
}


/*
 * Locate a channel given a frequency+flags.  We cache
 * the previous lookup to optimize swithing between two
 * channels--as happens with dynamic turbo.
 * This verifies that found channels have not been excluded because of 11d.
 */
struct ieee80211_channel *
ieee80211_find_channel(struct ieee80211com *ic, int freq, u_int32_t flags)
{
    struct ieee80211_channel *c;
    int i;

    flags &= IEEE80211_CHAN_ALLTURBO;
    /* brute force search */
    for (i = 0; i < ic->ic_nchans; i++)
    {
        c = &ic->ic_channels[i];

        if ((! IEEE80211_IS_CHAN_11D_EXCLUDED(c)) &&
            (c->ic_freq == freq) &&
            ((c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags))
        {
            return c;
        }
    }

    return NULL;
}


/*
 * ic_dfs_enable - enable DFS
 * For offload solutions, radar PHY errors will be enabled by the target
 * firmware when DFS is requested for the current channel.
 */
int ol_if_dfs_enable(struct ieee80211com *ic, int *is_fastclk, void *pe)
{
    /*
     * For peregrine, treat fastclk as the "oversampling" mode.
     * It's on by default.  This may change at some point, so
     * we should really query the firmware to find out what
     * the current configuration is.
     */
    (* is_fastclk) = 1;

    return (0);
}

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int32_t
ieee80211_ieee2mhz(u_int32_t chan, u_int32_t flags)
{
    if (flags & IEEE80211_CHAN_2GHZ)
    {
        /* 2GHz band */
        if (chan == 14)
            return 2484;
        if (chan < 14)
            return 2407 + chan*5;
        else
            return 2512 + ((chan-15)*20);
    }
    else if (flags & IEEE80211_CHAN_5GHZ)
    {
        /* 5Ghz band */
        return 5000 + (chan*5);
    }
    else
    {
        /* either, guess */
        if (chan == 14)
            return 2484;
        if (chan < 14) /* 0-13 */
            return 2407 + chan*5;
        if (chan < 27) /* 15-26 */
            return 2512 + ((chan-15)*20);
        return 5000 + (chan*5);
    }
}

/*
 * Place holder function ic_get_ext_busy
 */
int
ol_if_dfs_get_ext_busy(struct ieee80211com *ic)
{
    return (0);
}

/*
 * ic_get_mib_cycle_counts_pct
 */
int
ol_if_dfs_get_mib_cycle_counts_pct(struct ieee80211com *ic,
    u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt)
{
    return (0);
}

u_int16_t
ol_if_dfs_usenol(struct ieee80211com *ic)
{
#if ATH_SUPPORT_DFS
    return(dfs_usenol(ic));
#else
    return (0);
#endif  /* ATH_SUPPORT_DFS */
    return 0;
}

/*
 * Function to indicate Radar on the current
 * SAP operating channel.This indication will
 * be posted to SAP to select a new channel
 * randomly and issue a vdev restart to
 * operate on the new channel.
 */
void
ieee80211_mark_dfs(struct ieee80211com *ic, struct ieee80211_channel *ichan)
{
    int status;
    status = wma_dfs_indicate_radar(ic, ichan);
}
