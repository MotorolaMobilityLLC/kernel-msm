/*
 * Copyright (c) 2002-2013 The Linux Foundation. All rights reserved.
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

                     dfs_misc.c

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

#include "dfs.h"
/* TO DO DFS
#include <ieee80211_var.h>
*/
#ifndef UNINET
/* TO DO DFS
#include <ieee80211_channel.h>
*/
#endif
#ifdef ATH_SUPPORT_DFS

static int adjust_pri_per_chan_busy(int ext_chan_busy, int pri_margin)
{
    int adjust_pri=0;

    if(ext_chan_busy > DFS_EXT_CHAN_LOADING_THRESH) {

       adjust_pri = (ext_chan_busy - DFS_EXT_CHAN_LOADING_THRESH) * (pri_margin);
       adjust_pri /= 100;

    }
    return adjust_pri;
}

static int adjust_thresh_per_chan_busy(int ext_chan_busy, int thresh)
{
    int adjust_thresh=0;

    if(ext_chan_busy > DFS_EXT_CHAN_LOADING_THRESH) {

       adjust_thresh = (ext_chan_busy - DFS_EXT_CHAN_LOADING_THRESH) * thresh;
       adjust_thresh /= 100;

    }
    return adjust_thresh;
}
/* For the extension channel, if legacy traffic is present, we see a lot of false alarms,
so make the PRI margin narrower depending on the busy % for the extension channel.*/

int
dfs_get_pri_margin(struct ath_dfs *dfs, int is_extchan_detect,
    int is_fixed_pattern)
{

    int adjust_pri=0, ext_chan_busy=0;
    int pri_margin;

    if (is_fixed_pattern)
        pri_margin = DFS_DEFAULT_FIXEDPATTERN_PRI_MARGIN;
    else
        pri_margin = DFS_DEFAULT_PRI_MARGIN;

    if (IS_CHAN_HT40(dfs->ic->ic_curchan)) {
        ext_chan_busy = dfs->ic->ic_get_ext_busy(dfs->ic);
        if(ext_chan_busy >= 0) {
            dfs->dfs_rinfo.ext_chan_busy_ts = dfs->ic->ic_get_TSF64(dfs->ic);
            dfs->dfs_rinfo.dfs_ext_chan_busy = ext_chan_busy;
        } else {
            // Check to see if the cached value of ext_chan_busy can be used
            ext_chan_busy = 0;
            if (dfs->dfs_rinfo.dfs_ext_chan_busy) {
                if (dfs->dfs_rinfo.rn_lastfull_ts < dfs->dfs_rinfo.ext_chan_busy_ts) {
                    ext_chan_busy = dfs->dfs_rinfo.dfs_ext_chan_busy;
                    DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
                      " PRI Use cached copy of ext_chan_busy extchanbusy=%d \n", ext_chan_busy);
                }
            }
        }
        adjust_pri = adjust_pri_per_chan_busy(ext_chan_busy, pri_margin);

        pri_margin -= adjust_pri;
    }
    return pri_margin;
}

/* For the extension channel, if legacy traffic is present, we see a lot of false alarms,
so make the thresholds higher depending on the busy % for the extension channel.*/

int dfs_get_filter_threshold(struct ath_dfs *dfs, struct dfs_filter *rf,
    int is_extchan_detect)
{
    int ext_chan_busy=0;
    int thresh, adjust_thresh=0;

    thresh = rf->rf_threshold;

    if (IS_CHAN_HT40(dfs->ic->ic_curchan)) {
        ext_chan_busy = dfs->ic->ic_get_ext_busy(dfs->ic);
        if(ext_chan_busy >= 0) {
           dfs->dfs_rinfo.ext_chan_busy_ts = dfs->ic->ic_get_TSF64(dfs->ic);
            dfs->dfs_rinfo.dfs_ext_chan_busy = ext_chan_busy;
        } else {
            // Check to see if the cached value of ext_chan_busy can be used
            ext_chan_busy = 0;
            if (dfs->dfs_rinfo.dfs_ext_chan_busy) {
           if (dfs->dfs_rinfo.rn_lastfull_ts < dfs->dfs_rinfo.ext_chan_busy_ts) {
                        ext_chan_busy = dfs->dfs_rinfo.dfs_ext_chan_busy;
                        DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
                          " THRESH Use cached copy of ext_chan_busy extchanbusy=%d rn_lastfull_ts=%llu ext_chan_busy_ts=%llu\n",
                          ext_chan_busy,
                          (unsigned long long)dfs->dfs_rinfo.rn_lastfull_ts,
                          (unsigned long long)dfs->dfs_rinfo.ext_chan_busy_ts);
                }
            }
        }

        adjust_thresh = adjust_thresh_per_chan_busy(ext_chan_busy, thresh);

        DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
          " filterID=%d extchanbusy=%d adjust_thresh=%d\n",
          rf->rf_pulseid, ext_chan_busy, adjust_thresh);

        thresh += adjust_thresh;
    }
    return thresh;
}


u_int32_t dfs_round(int32_t val)
{
   u_int32_t ival,rem;

   if (val < 0)
      return 0;
   ival = val/100;
   rem = val-(ival*100);
   if (rem <50)
      return ival;
   else
      return(ival+1);
}

struct ieee80211_channel *
ieee80211_get_extchan(struct ieee80211com *ic)
{
           int chan_offset = 0;
           if (IEEE80211_IS_CHAN_HT40PLUS_CAPABLE(ic->ic_curchan)) {
                chan_offset = 20;
           } else if (IEEE80211_IS_CHAN_HT40MINUS_CAPABLE(ic->ic_curchan)) {
                chan_offset = -20;
           } else {
                return NULL;
           }
           return(ic->ic_find_channel(ic, ic->ic_curchan->ic_freq + chan_offset, IEEE80211_CHAN_11NA_HT20));
}

/*
 * Finds the radar state entry that matches the current channel
 */
struct dfs_state *
dfs_getchanstate(struct ath_dfs *dfs, u_int8_t *index, int ext_chan_flag)
{
   struct dfs_state *rs=NULL;
   int i;
        struct ieee80211_channel *cmp_ch;

   if (dfs == NULL) {
      printk("%s[%d]: sc_dfs is NULL\n",__func__,__LINE__);
               // DFS_DPRINTK(dfs, ATH_DEBUG_DFS,"%s: sc_dfs is NULL\n",__func__);
      return NULL;
   }

        if (ext_chan_flag) {
            cmp_ch = ieee80211_get_extchan(dfs->ic);
            if (cmp_ch) {
                DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "Extension channel freq = %u flags=0x%x\n", cmp_ch->ic_freq, cmp_ch->ic_flagext);
            } else {
                return NULL;
            }

        } else {
            cmp_ch = dfs->ic->ic_curchan;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "Primary channel freq = %u flags=0x%x\n", cmp_ch->ic_freq, cmp_ch->ic_flagext);
        }
   for (i=0;i<DFS_NUM_RADAR_STATES; i++) {
                if ((dfs->dfs_radar[i].rs_chan.ic_freq== cmp_ch->ic_freq) &&
          (dfs->dfs_radar[i].rs_chan.ic_flags == cmp_ch->ic_flags)) {
         if (index != NULL)
            *index = (u_int8_t) i;
         return &(dfs->dfs_radar[i]);
      }
   }
   /* No existing channel found, look for first free channel state entry */
   for (i=0; i<DFS_NUM_RADAR_STATES; i++) {
      if (dfs->dfs_radar[i].rs_chan.ic_freq == 0) {
         rs = &(dfs->dfs_radar[i]);
         /* Found one, set channel info and default thresholds */
         rs->rs_chan = *cmp_ch;

         /* Copy the parameters from the default set */
         ath_dfs_phyerr_param_copy(&rs->rs_param,
             &dfs->dfs_defaultparams);

         if (index != NULL)
            *index = (u_int8_t) i;
         return (rs);
      }
   }
   DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "%s: No more radar states left.\n", __func__);
   return(NULL);
}



#endif /* ATH_SUPPORT_DFS */
