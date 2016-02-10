/*
 * Copyright (c) 2002-2014 The Linux Foundation. All rights reserved.
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

                     dfs_radarevent.c

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
#ifdef ATH_SUPPORT_DFS

#define FREQ_5500_MHZ  5500
#define FREQ_5500_MHZ       5500

#define DFS_MAX_FREQ_SPREAD            1375 * 1

static char debug_dup[33];
static int debug_dup_cnt;

/*
 * Convert the hardware provided duration to TSF ticks (usecs)
 * taking the clock (fast or normal) into account.
 *
 * Legacy (pre-11n, Owl, Sowl, Howl) operate 5GHz using a 40MHz
 * clock.  Later 11n chips (Merlin, Osprey, etc) operate 5GHz using
 * a 44MHz clock, so the reported pulse durations are different.
 *
 * Peregrine reports the pulse duration in microseconds regardless
 * of the operating mode. (XXX TODO: verify this, obviously.)
 */
static inline u_int8_t
dfs_process_pulse_dur(struct ath_dfs *dfs, u_int8_t re_dur)
{
   /*
    * Short pulses are sometimes returned as having a duration of 0,
    * so round those up to 1.
    *
    * XXX This holds true for BB TLV chips too, right?
    */
   if (re_dur == 0)
      return 1;

   /*
    * For BB TLV chips, the hardware always returns microsecond
    * pulse durations.
    */
   if (dfs->dfs_caps.ath_chip_is_bb_tlv)
      return re_dur;

   /*
    * This is for 11n and legacy chips, which may or may not
    * use the 5GHz fast clock mode.
    */
   /* Convert 0.8us durations to TSF ticks (usecs) */
   return (u_int8_t)dfs_round((int32_t)((dfs->dur_multiplier)*re_dur));
}

/*
 * Process a radar event.
 *
 * If a radar event is found, return 1.  Otherwise, return 0.
 *
 * There is currently no way to specify that a radar event has occured on
 * a specific channel, so the current methodology is to mark both the pri
 * and ext channels as being unavailable.  This should be fixed for 802.11ac
 * or we'll quickly run out of valid channels to use.
 */
int
dfs_process_radarevent(struct ath_dfs *dfs, struct ieee80211_channel *chan)
{
//commenting for now to validate radar indication msg to SAP
//#if 0
    struct dfs_event re,*event;
    struct dfs_state *rs=NULL;
    struct dfs_filtertype *ft;
    struct dfs_filter *rf;
    int found, retval = 0, p, empty;
    int events_processed = 0;
    u_int32_t tabledepth, index;
    u_int64_t deltafull_ts = 0, this_ts, deltaT;
    struct ieee80211_channel *thischan;
    struct dfs_pulseline *pl;
    static u_int32_t  test_ts  = 0;
    static u_int32_t  diff_ts  = 0;
    int ext_chan_event_flag = 0;
#if 0
    int pri_multiplier = 2;
#endif
    int i;

   if (dfs == NULL) {
      VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                      "%s[%d]: dfs is NULL", __func__, __LINE__);
      return 0;
   }
    pl = dfs->pulses;
   adf_os_spin_lock_bh(&dfs->ic->chan_lock);
   if ( !(IEEE80211_IS_CHAN_DFS(dfs->ic->ic_curchan))) {
           adf_os_spin_unlock_bh(&dfs->ic->chan_lock);
           DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "%s: radar event on non-DFS chan",
                        __func__);
                dfs_reset_radarq(dfs);
                dfs_reset_alldelaylines(dfs);
         return 0;
        }
   adf_os_spin_unlock_bh(&dfs->ic->chan_lock);
#ifndef ATH_DFS_RADAR_DETECTION_ONLY
   /* TEST : Simulate radar bang, make sure we add the channel to NOL (bug 29968) */
        if (dfs->dfs_bangradar) {
                    /* bangradar will always simulate radar found on the primary channel */
           rs = &dfs->dfs_radar[dfs->dfs_curchan_radindex];
           dfs->dfs_bangradar = 0; /* reset */
                DFS_DPRINTK(dfs, ATH_DEBUG_DFS, "%s: bangradar", __func__);
           retval = 1;
                     goto dfsfound;
    }
#endif

        /*
          The HW may miss some pulses especially with high channel loading.
          This is true for Japan W53 where channel loaoding is 50%. Also
          for ETSI where channel loading is 30% this can be an issue too.
          To take care of missing pulses, we introduce pri_margin multiplie.
          This is normally 2 but can be higher for W53.
        */

        if ((dfs->dfsdomain  == DFS_MKK4_DOMAIN) &&
            (dfs->dfs_caps.ath_chip_is_bb_tlv) &&
            (chan->ic_freq < FREQ_5500_MHZ)) {

            dfs->dfs_pri_multiplier = dfs->dfs_pri_multiplier_ini;

            /* do not process W53 pulses,
               unless we have a minimum number of them
             */
            if (dfs->dfs_phyerr_w53_counter >= 5) {
               DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
                       "%s: w53_counter=%d, freq_max=%d, "
                       "freq_min=%d, pri_multiplier=%d",
                       __func__,
                       dfs->dfs_phyerr_w53_counter,
                       dfs->dfs_phyerr_freq_max,
                       dfs->dfs_phyerr_freq_min,
                       dfs->dfs_pri_multiplier);
                dfs->dfs_phyerr_freq_min     = 0x7fffffff;
                dfs->dfs_phyerr_freq_max     = 0;
            } else {
                return 0;
            }
        }
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
                    "%s: pri_multiplier=%d",
                    __func__,
                    dfs->dfs_pri_multiplier);

   ATH_DFSQ_LOCK(dfs);
   empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
   ATH_DFSQ_UNLOCK(dfs);

   while ((!empty) && (!retval) && (events_processed < MAX_EVENTS)) {
      ATH_DFSQ_LOCK(dfs);
      event = STAILQ_FIRST(&(dfs->dfs_radarq));
      if (event != NULL)
         STAILQ_REMOVE_HEAD(&(dfs->dfs_radarq), re_list);
      ATH_DFSQ_UNLOCK(dfs);

      if (event == NULL) {
         empty = 1;
         VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "%s[%d]: event is NULL ",__func__,__LINE__);
                        break;
      }
                events_processed++;
                re = *event;

      OS_MEMZERO(event, sizeof(struct dfs_event));
      ATH_DFSEVENTQ_LOCK(dfs);
      STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), event, re_list);
      ATH_DFSEVENTQ_UNLOCK(dfs);

      found = 0;

      adf_os_spin_lock_bh(&dfs->ic->chan_lock);
      if (dfs->ic->disable_phy_err_processing) {
         ATH_DFSQ_LOCK(dfs);
         empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
         ATH_DFSQ_UNLOCK(dfs);
         adf_os_spin_unlock_bh(&dfs->ic->chan_lock);
         continue;
      }

      adf_os_spin_unlock_bh(&dfs->ic->chan_lock);

      if (re.re_chanindex < DFS_NUM_RADAR_STATES)
         rs = &dfs->dfs_radar[re.re_chanindex];
      else {
         ATH_DFSQ_LOCK(dfs);
         empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
         ATH_DFSQ_UNLOCK(dfs);
         continue;
      }
      if (rs->rs_chan.ic_flagext & CHANNEL_INTERFERENCE) {
         ATH_DFSQ_LOCK(dfs);
         empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
         ATH_DFSQ_UNLOCK(dfs);
         continue;
      }

      if (dfs->dfs_rinfo.rn_lastfull_ts == 0) {
         /*
          * Either not started, or 64-bit rollover exactly to zero
          * Just prepend zeros to the 15-bit ts
          */
         dfs->dfs_rinfo.rn_ts_prefix = 0;
      } else {
                         /* WAR 23031- patch duplicate ts on very short pulses */
                        /* This pacth has two problems in linux environment.
                         * 1)The time stamp created and hence PRI depends entirely on the latency.
                         *   If the latency is high, it possibly can split two consecutive
                         *   pulses in the same burst so far away (the same amount of latency)
                         *   that make them look like they are from differenct bursts. It is
                         *   observed to happen too often. It sure makes the detection fail.
                         * 2)Even if the latency is not that bad, it simply shifts the duplicate
                         *   timestamps to a new duplicate timestamp based on how they are processed.
                         *   This is not worse but not good either.
                         *
                         *   Take this pulse as a good one and create a probable PRI later
                         */
                        if (re.re_dur == 0 && re.re_ts == dfs->dfs_rinfo.rn_last_unique_ts) {
                                debug_dup[debug_dup_cnt++] = '1';
                                DFS_DPRINTK(dfs, ATH_DEBUG_DFS1, " %s deltaT is 0 ", __func__);
                        } else {
                                dfs->dfs_rinfo.rn_last_unique_ts = re.re_ts;
                                debug_dup[debug_dup_cnt++] = '0';
                        }
                        if (debug_dup_cnt >= 32){
                                 debug_dup_cnt = 0;
                        }


         if (re.re_ts <= dfs->dfs_rinfo.rn_last_ts) {
            dfs->dfs_rinfo.rn_ts_prefix +=
               (((u_int64_t) 1) << DFS_TSSHIFT);
            /* Now, see if it's been more than 1 wrap */
            deltafull_ts = re.re_full_ts - dfs->dfs_rinfo.rn_lastfull_ts;
            if (deltafull_ts >
                ((u_int64_t)((DFS_TSMASK - dfs->dfs_rinfo.rn_last_ts) + 1 + re.re_ts)))
               deltafull_ts -= (DFS_TSMASK - dfs->dfs_rinfo.rn_last_ts) + 1 + re.re_ts;
            deltafull_ts = deltafull_ts >> DFS_TSSHIFT;
            if (deltafull_ts > 1) {
               dfs->dfs_rinfo.rn_ts_prefix +=
                  ((deltafull_ts - 1) << DFS_TSSHIFT);
            }
         } else {
            deltafull_ts = re.re_full_ts - dfs->dfs_rinfo.rn_lastfull_ts;
            if (deltafull_ts > (u_int64_t) DFS_TSMASK) {
               deltafull_ts = deltafull_ts >> DFS_TSSHIFT;
               dfs->dfs_rinfo.rn_ts_prefix +=
                  ((deltafull_ts - 1) << DFS_TSSHIFT);
            }
         }
      }
      /*
       * At this stage rn_ts_prefix has either been blanked or
       * calculated, so it's safe to use.
       */
      this_ts = dfs->dfs_rinfo.rn_ts_prefix | ((u_int64_t) re.re_ts);
      dfs->dfs_rinfo.rn_lastfull_ts = re.re_full_ts;
      dfs->dfs_rinfo.rn_last_ts = re.re_ts;

      /*
       * The hardware returns the duration in a variety of formats,
       * so it's converted from the hardware format to TSF (usec)
       * values here.
       *
       * XXX TODO: this should really be done when the PHY error
       * is processed, rather than way out here..
       */
      re.re_dur = dfs_process_pulse_dur(dfs, re.re_dur);

      /*
       * Calculate the start of the radar pulse.
       *
       * The TSF is stamped by the MAC upon reception of the
       * event, which is (typically?) at the end of the event.
       * But the pattern matching code expects the event timestamps
       * to be at the start of the event.  So to fake it, we
       * subtract the pulse duration from the given TSF.
       *
       * This is done after the 64-bit timestamp has been calculated
       * so long pulses correctly under-wrap the counter.  Ie, if
       * this was done on the 32 (or 15!) bit TSF when the TSF
       * value is closed to 0, it will underflow to 0xfffffXX, which
       * would mess up the logical "OR" operation done above.
       *
       * This isn't valid for Peregrine as the hardware gives us
       * the actual TSF offset of the radar event, not just the MAC
       * TSF of the completed receive.
       *
       * XXX TODO: ensure that the TLV PHY error processing
       * code will correctly calculate the TSF to be the start
       * of the radar pulse.
       *
       * XXX TODO TODO: modify the TLV parsing code to subtract
       * the duration from the TSF, based on the current fast clock
       * value.
       */
      if ((! dfs->dfs_caps.ath_chip_is_bb_tlv) && re.re_dur != 1) {
         this_ts -= re.re_dur;
      }

              /* Save the pulse parameters in the pulse buffer(pulse line) */
                index = (pl->pl_lastelem + 1) & DFS_MAX_PULSE_BUFFER_MASK;
                if (pl->pl_numelems == DFS_MAX_PULSE_BUFFER_SIZE)
                        pl->pl_firstelem = (pl->pl_firstelem+1) & DFS_MAX_PULSE_BUFFER_MASK;
                else
                        pl->pl_numelems++;
                pl->pl_lastelem = index;
                pl->pl_elems[index].p_time = this_ts;
                pl->pl_elems[index].p_dur = re.re_dur;
                pl->pl_elems[index].p_rssi = re.re_rssi;
                diff_ts = (u_int32_t)this_ts - test_ts;
                test_ts = (u_int32_t)this_ts;
                DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,"ts%u %u %u diff %u pl->pl_lastelem.p_time=%llu",(u_int32_t)this_ts, re.re_dur, re.re_rssi, diff_ts, (unsigned long long)pl->pl_elems[index].p_time);
                if (dfs->dfs_event_log_on) {
                        i = dfs->dfs_event_log_count % DFS_EVENT_LOG_SIZE;
                        dfs->radar_log[i].ts      = this_ts;
                        dfs->radar_log[i].diff_ts = diff_ts;
                        dfs->radar_log[i].rssi    = re.re_rssi;
                        dfs->radar_log[i].dur     = re.re_dur;
                        dfs->dfs_event_log_count++;
                }
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:xxxxx ts =%u re.re_dur=%u re.re_rssi =%u diff =%u pl->pl_lastelem.p_time=%llu xxxxx",__func__,__LINE__,(u_int32_t)this_ts, re.re_dur, re.re_rssi, diff_ts, (unsigned long long)pl->pl_elems[index].p_time);



                /* If diff_ts is very small, we might be getting false pulse detects
                 * due to heavy interference. We might be getting spectral splatter
                 * from adjacent channel. In order to prevent false alarms we
                 * clear the delay-lines. This might impact positive detections under
                 * harsh environments, but helps with false detects. */

         if (diff_ts < 100) {
            dfs_reset_alldelaylines(dfs);
            dfs_reset_radarq(dfs);
         }

         found = 0;

         /*
          * Use this fix only when device is not in test mode, as
          * it drops some valid phyerrors.
          * In FCC or JAPAN domain,if the follwing signature matches
          * its likely that this is a false radar pulse pattern
          * so process the next pulse in the queue.
          */
         if ((dfs->disable_dfs_ch_switch == VOS_FALSE) &&
             (DFS_FCC_DOMAIN == dfs->dfsdomain ||
              DFS_MKK4_DOMAIN == dfs->dfsdomain) &&
             (re.re_dur >= 11 && re.re_dur <= 20) &&
             (diff_ts > 500 || diff_ts <= 305) &&
             (re.sidx == -4)) {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
            "\n%s: Rejecting on Peak Index = %d,re.re_dur = %d,diff_ts = %d\n",
            __func__,re.sidx, re.re_dur, diff_ts);

            ATH_DFSQ_LOCK(dfs);
            empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
            ATH_DFSQ_UNLOCK(dfs);
            continue;
         }

         /*
          * Modifying the pulse duration for FCC Type 4
          * or JAPAN W56 Type 6 radar pulses when the
          * following condition is reported in radar
          * summary report.
          */
         if ((DFS_FCC_DOMAIN == dfs->dfsdomain ||
              DFS_MKK4_DOMAIN == dfs->dfsdomain) &&
             ((chan->ic_flags & IEEE80211_CHAN_VHT80) ==
              IEEE80211_CHAN_VHT80) &&
             (chan->ic_pri_freq_center_freq_mhz_separation ==
                                DFS_WAR_PLUS_30_MHZ_SEPARATION ||
              chan->ic_pri_freq_center_freq_mhz_separation ==
                                DFS_WAR_MINUS_30_MHZ_SEPARATION) &&
             (re.sidx == DFS_WAR_PEAK_INDEX_ZERO) &&
             (re.re_dur > DFS_TYPE4_WAR_PULSE_DURATION_LOWER_LIMIT &&
              re.re_dur < DFS_TYPE4_WAR_PULSE_DURATION_UPPER_LIMIT) &&
             (diff_ts > DFS_TYPE4_WAR_PRI_LOWER_LIMIT &&
              diff_ts < DFS_TYPE4_WAR_PRI_UPPER_LIMIT)) {
             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       "\n%s:chan->ic_flags=0x%x, Pri Chan MHz Separation=%d\n",
                       __func__, chan->ic_flags,
                       chan->ic_pri_freq_center_freq_mhz_separation);

             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "\n%s: Reported Peak Index = %d,re.re_dur = %d,diff_ts = %d\n",
                 __func__, re.sidx, re.re_dur, diff_ts);

             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "\n%s: Modifying the pulse duration to fit the valid range \n",
                 __func__);

             re.re_dur = DFS_TYPE4_WAR_VALID_PULSE_DURATION;
         }

         /*
          * Modifying the pulse duration for ETSI Type 2
          * and ETSI type 3 radar pulses when the following
          * condition is reported in radar summary report.
          */
         if ((DFS_ETSI_DOMAIN == dfs->dfsdomain) &&
             ((chan->ic_flags & IEEE80211_CHAN_VHT80) ==
              IEEE80211_CHAN_VHT80) &&
             (chan->ic_pri_freq_center_freq_mhz_separation ==
                                DFS_WAR_PLUS_30_MHZ_SEPARATION ||
              chan->ic_pri_freq_center_freq_mhz_separation ==
                                DFS_WAR_MINUS_30_MHZ_SEPARATION) &&
             (re.sidx == DFS_WAR_PEAK_INDEX_ZERO) &&
             (re.re_dur > DFS_ETSI_TYPE2_TYPE3_WAR_PULSE_DUR_LOWER_LIMIT &&
              re.re_dur < DFS_ETSI_TYPE2_TYPE3_WAR_PULSE_DUR_UPPER_LIMIT) &&
             ((diff_ts > DFS_ETSI_TYPE2_WAR_PRI_LOWER_LIMIT &&
               diff_ts < DFS_ETSI_TYPE2_WAR_PRI_UPPER_LIMIT) ||
              (diff_ts > DFS_ETSI_TYPE3_WAR_PRI_LOWER_LIMIT &&
               diff_ts < DFS_ETSI_TYPE3_WAR_PRI_UPPER_LIMIT ))) {
             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                       "\n%s:chan->ic_flags=0x%x, Pri Chan MHz Separation=%d\n",
                       __func__, chan->ic_flags,
                       chan->ic_pri_freq_center_freq_mhz_separation);

             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "\n%s: Reported Peak Index = %d,re.re_dur = %d,diff_ts = %d\n",
                 __func__, re.sidx, re.re_dur, diff_ts);

             VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                "\n%s: Modifying the ETSI pulse dur to fit the valid range \n",
                 __func__);

             re.re_dur  = DFS_ETSI_WAR_VALID_PULSE_DURATION;
         }

      /* BIN5 pulses are FCC and Japan specific */

      if ((dfs->dfsdomain == DFS_FCC_DOMAIN) || (dfs->dfsdomain == DFS_MKK4_DOMAIN)) {
         for (p=0; (p < dfs->dfs_rinfo.rn_numbin5radars) && (!found); p++) {
            struct dfs_bin5radars *br;

            br = &(dfs->dfs_b5radars[p]);
            if (dfs_bin5_check_pulse(dfs, &re, br)) {
               // This is a valid Bin5 pulse, check if it belongs to a burst
               re.re_dur = dfs_retain_bin5_burst_pattern(dfs, diff_ts, re.re_dur);
               // Remember our computed duration for the next pulse in the burst (if needed)
               dfs->dfs_rinfo.dfs_bin5_chirp_ts = this_ts;
               dfs->dfs_rinfo.dfs_last_bin5_dur = re.re_dur;

               if( dfs_bin5_addpulse(dfs, br, &re, this_ts) ) {
                  found |= dfs_bin5_check(dfs);
               }
            } else{
               DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_PULSE,
                   "%s not a BIN5 pulse (dur=%d)",
                   __func__, re.re_dur);
                                 }
         }
      }

      if (found) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS, "%s: Found bin5 radar", __func__);
         retval |= found;
         goto dfsfound;
      }

      tabledepth = 0;
      rf = NULL;
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,"  *** chan freq (%d): ts %llu dur %u rssi %u",
         rs->rs_chan.ic_freq, (unsigned long long)this_ts, re.re_dur, re.re_rssi);

      while ((tabledepth < DFS_MAX_RADAR_OVERLAP) &&
             ((dfs->dfs_radartable[re.re_dur])[tabledepth] != -1) &&
             (!retval)) {
         ft = dfs->dfs_radarf[((dfs->dfs_radartable[re.re_dur])[tabledepth])];
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,"  ** RD (%d): ts %x dur %u rssi %u",
                   rs->rs_chan.ic_freq,
                   re.re_ts, re.re_dur, re.re_rssi);

         if (re.re_rssi < ft->ft_rssithresh && re.re_dur > 4) {
               DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,"%s : Rejecting on rssi rssi=%u thresh=%u", __func__, re.re_rssi, ft->ft_rssithresh);
                           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]: Rejecting on rssi rssi=%u thresh=%u",__func__,__LINE__,re.re_rssi, ft->ft_rssithresh);
                     tabledepth++;
            ATH_DFSQ_LOCK(dfs);
            empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
            ATH_DFSQ_UNLOCK(dfs);
            continue;
         }
         deltaT = this_ts - ft->ft_last_ts;
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,"deltaT = %lld (ts: 0x%llx) (last ts: 0x%llx)",(unsigned long long)deltaT, (unsigned long long)this_ts, (unsigned long long)ft->ft_last_ts);
         if ((deltaT < ft->ft_minpri) && (deltaT !=0)){
                                /* This check is for the whole filter type. Individual filters
                                 will check this again. This is first line of filtering.*/
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "%s: Rejecting on pri pri=%lld minpri=%u", __func__, (unsigned long long)deltaT, ft->ft_minpri);
                                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:Rejecting on pri pri=%lld minpri=%u",__func__,__LINE__,(unsigned long long)deltaT,ft->ft_minpri);
                                tabledepth++;
            continue;
         }
         for (p=0, found = 0; (p<ft->ft_numfilters) && (!found); p++) {
                                    rf = &(ft->ft_filters[p]);
                                    if ((re.re_dur >= rf->rf_mindur) && (re.re_dur <= rf->rf_maxdur)) {
                                        /* The above check is probably not necessary */
                                        deltaT = (this_ts < rf->rf_dl.dl_last_ts) ?
                                            (int64_t) ((DFS_TSF_WRAP - rf->rf_dl.dl_last_ts) + this_ts + 1) :
                                            this_ts - rf->rf_dl.dl_last_ts;

                                        if ((deltaT < rf->rf_minpri) && (deltaT != 0)) {
                                                /* Second line of PRI filtering. */
                                                DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
                                                "filterID %d : Rejecting on individual filter min PRI deltaT=%lld rf->rf_minpri=%u",
                                                rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:filterID= %d::Rejecting on individual filter min PRI deltaT=%lld rf->rf_minpri=%u",__func__,__LINE__,rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                continue;
                                        }

                                        if (rf->rf_ignore_pri_window > 0) {
                                           if (deltaT < rf->rf_minpri) {
                                                DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
                                                "filterID %d : Rejecting on individual filter max PRI deltaT=%lld rf->rf_minpri=%u",
                                                rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:filterID= %d :: Rejecting on individual filter max PRI deltaT=%lld rf->rf_minpri=%u",__func__,__LINE__,rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                /* But update the last time stamp */
                                                rf->rf_dl.dl_last_ts = this_ts;
                                                continue;
                                           }
                                        } else {

                                        /*
                                            The HW may miss some pulses especially with high channel loading.
                                            This is true for Japan W53 where channel loaoding is 50%. Also
                                            for ETSI where channel loading is 30% this can be an issue too.
                                            To take care of missing pulses, we introduce pri_margin multiplie.
                                            This is normally 2 but can be higher for W53.
                                        */

                                        if ( (deltaT > (dfs->dfs_pri_multiplier * rf->rf_maxpri) ) || (deltaT < rf->rf_minpri) ) {
                                                DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
                                                "filterID %d : Rejecting on individual filter max PRI deltaT=%lld rf->rf_minpri=%u",
                                                rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:filterID= %d :: Rejecting on individual filter max PRI deltaT=%lld rf->rf_minpri=%u",__func__,__LINE__,rf->rf_pulseid, (unsigned long long)deltaT, rf->rf_minpri);
                                                /* But update the last time stamp */
                                                rf->rf_dl.dl_last_ts = this_ts;
                                                continue;
                                        }
                                        }
                                        dfs_add_pulse(dfs, rf, &re, deltaT, this_ts);


                                       /* If this is an extension channel event, flag it for false alarm reduction */
                                        if (re.re_chanindex == dfs->dfs_extchan_radindex) {
                                            ext_chan_event_flag = 1;
                                        }
                                        if (rf->rf_patterntype == 2) {
                                            found = dfs_staggered_check(dfs, rf, (u_int32_t) deltaT, re.re_dur);
                                        } else {
                                            found = dfs_bin_check(dfs, rf, (u_int32_t) deltaT, re.re_dur, ext_chan_event_flag);
                                        }
                                        if (dfs->dfs_debug_mask & ATH_DEBUG_DFS2) {
                                                dfs_print_delayline(dfs, &rf->rf_dl);
                                        }
                                        rf->rf_dl.dl_last_ts = this_ts;
                                }
                            }
         ft->ft_last_ts = this_ts;
         retval |= found;
         if (found) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS3,
               "Found on channel minDur = %d, filterId = %d",ft->ft_mindur,
               rf != NULL ? rf->rf_pulseid : -1);
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                 "%s[%d]:### Found on channel minDur = %d, filterId = %d ###",
                 __func__,__LINE__,ft->ft_mindur,
                 rf != NULL ? rf->rf_pulseid : -1);
         }
         tabledepth++;
      }
      ATH_DFSQ_LOCK(dfs);
      empty = STAILQ_EMPTY(&(dfs->dfs_radarq));
      ATH_DFSQ_UNLOCK(dfs);
   }
dfsfound:
   if (retval) {
      /* Collect stats */
      dfs->ath_dfs_stats.num_radar_detects++;
      thischan = &rs->rs_chan;
   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "%s[%d]: ### RADAR FOUND ON CHANNEL %d (%d MHz) ###",__func__,__LINE__,thischan->ic_ieee,
thischan->ic_freq);
      DFS_PRINTK("Radar found on channel %d (%d MHz)",
          thischan->ic_ieee,
          thischan->ic_freq);

#if 0 //UMACDFS : TODO
      /* Disable radar for now */
      rfilt = ath_hal_getrxfilter(ah);
      rfilt &= ~HAL_RX_FILTER_PHYRADAR;
      ath_hal_setrxfilter(ah, rfilt);
#endif
      dfs_reset_radarq(dfs);
      dfs_reset_alldelaylines(dfs);
      /* XXX Should we really enable again? Maybe not... */
/* No reason to re-enable so far - Ajay*/
#if 0
      pe.pe_firpwr = rs->rs_firpwr;
      pe.pe_rrssi = rs->rs_radarrssi;
      pe.pe_height = rs->rs_height;
      pe.pe_prssi = rs->rs_pulserssi;
      pe.pe_inband = rs->rs_inband;
      /* 5413 specific */
      pe.pe_relpwr = rs->rs_relpwr;
      pe.pe_relstep = rs->rs_relstep;
      pe.pe_maxlen = rs->rs_maxlen;

      ath_hal_enabledfs(ah, &pe);
      rfilt |= HAL_RX_FILTER_PHYRADAR;
      ath_hal_setrxfilter(ah, rfilt);
#endif
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
             "Primary channel freq = %u flags=0x%x",
             chan->ic_freq, chan->ic_flagext);
         adf_os_spin_lock_bh(&dfs->ic->chan_lock);
         if ((dfs->ic->ic_curchan->ic_freq!= thischan->ic_freq)) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
             "Ext channel freq = %u flags=0x%x",
             thischan->ic_freq, thischan->ic_flagext);
      }

      adf_os_spin_unlock_bh(&dfs->ic->chan_lock);
                dfs->dfs_phyerr_freq_min     = 0x7fffffff;
                dfs->dfs_phyerr_freq_max     = 0;
                dfs->dfs_phyerr_w53_counter  = 0;
   }
        //VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "IN FUNC %s[%d]: retval = %d ",__func__,__LINE__,retval);
   return retval;
//#endif
//        return 1;
}

#endif /* ATH_SUPPORT_DFS */
