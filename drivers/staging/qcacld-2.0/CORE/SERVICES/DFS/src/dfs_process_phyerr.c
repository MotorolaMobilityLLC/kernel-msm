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

                     dfs_process_phyerr.c

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
#include "dfs_phyerr.h" /* For chip-specific phyerr function declarations */
/* TO DO DFS
#include <ieee80211_var.h>
*/
#ifndef UNINET
/* TO DO DFS
#include <ieee80211_channel.h>
*/
#endif
#ifdef ATH_SUPPORT_DFS

/*
 * Return the frequency width for the current operating channel.
 *
 * This isn't the channel width - it's how wide the reported event
 * may be.  For HT20 this is 20MHz. For HT40 on Howl and later it'll
 * still be 20MHz - the hardware returns either pri or ext channel.
 */
static inline int
dfs_get_event_freqwidth(struct ath_dfs *dfs)
{

   /* Handle edge cases during startup/transition, shouldn't happen! */
   if (dfs == NULL)
      return (0);
   if (dfs->ic == NULL || dfs->ic->ic_curchan == NULL)
      return (0);

   /*
    * XXX For now, assume 20MHz wide - but this is incorrect when
    * operating in half/quarter mode!
    */
   return (20);
}

/*
 * Return the centre frequency for the current operating channel and
 * event.
 *
 * This is for post-Owl 11n chips which report pri/extension channel
 * events.
 */
static inline uint16_t
dfs_get_event_freqcentre(struct ath_dfs *dfs, int is_pri, int is_ext, int is_dc)
{
   struct ieee80211com *ic;
   int chan_offset = 0, chan_width;
   uint16_t freq;

   /* Handle edge cases during startup/transition, shouldn't happen! */
   if (dfs == NULL)
      return (0);
   if (dfs->ic == NULL || dfs->ic->ic_curchan == NULL)
      return (0);

   ic = dfs->ic;

   /*
    *
    * For wide channels, DC and ext frequencies need a bit of hand-holding
    * based on whether it's an upper or lower channel.
    */
   chan_width = dfs_get_event_freqwidth(dfs);

   adf_os_spin_lock_bh(&ic->chan_lock);
   if (IEEE80211_IS_CHAN_11N_HT40PLUS(ic->ic_curchan))
      chan_offset = chan_width;
   else if (IEEE80211_IS_CHAN_11N_HT40MINUS(ic->ic_curchan))
      chan_offset = -chan_width;
   else
      chan_offset = 0;
   adf_os_spin_unlock_bh(&ic->chan_lock);

   /*
    * Check for DC events first - the sowl code may just set all
    * the bits together..
    */
   if (is_dc) {
      /*
       * XXX TODO: Should DC events be considered 40MHz wide here?
       */
      adf_os_spin_lock_bh(&ic->chan_lock);
      freq = (ieee80211_chan2freq(ic, ic->ic_curchan) +
          (chan_offset / 2));
      adf_os_spin_unlock_bh(&ic->chan_lock);
      return freq;
   }

   /*
    * For non-wide channels, the centre frequency is just ic_freq.
    * The centre frequency for pri events is still ic_freq.
    */
   if (is_pri) {
      adf_os_spin_lock_bh(&ic->chan_lock);
      freq = (ieee80211_chan2freq(ic, ic->ic_curchan));
      adf_os_spin_unlock_bh(&ic->chan_lock);
      return freq;
   }

   if (is_ext) {
      adf_os_spin_lock_bh(&ic->chan_lock);
      freq = (ieee80211_chan2freq(ic, ic->ic_curchan) + chan_width);
      adf_os_spin_unlock_bh(&ic->chan_lock);
      return freq;
   }

   /* XXX shouldn't get here */
   adf_os_spin_lock_bh(&ic->chan_lock);
   freq = (ieee80211_chan2freq(ic, ic->ic_curchan));
   adf_os_spin_unlock_bh(&ic->chan_lock);

   return freq;
}

/*
 * Process an Owl-style phy error.
 *
 * Return 1 on success or 0 on failure.
 */
int
dfs_process_phyerr_owl(struct ath_dfs *dfs, void *buf, u_int16_t datalen,
    u_int8_t rssi, u_int8_t ext_rssi, u_int32_t rs_tstamp, u_int64_t fulltsf,
    struct dfs_phy_err *e)
{
   const char *cbuf = (const char *) buf;
   u_int8_t dur;
   int event_width;

   /* XXX this shouldn't be kept count here */
   dfs->ath_dfs_stats.owl_phy_errors++;

   /*
    * HW cannot detect extension channel radar so it only passes us
    * primary channel radar data
    */
   if (datalen == 0)
      dur = 0;
   else
      dur = ((u_int8_t *) cbuf)[0];

   /*
    * This is a spurious event; toss.
    */
   if (rssi == 0 && dur == 0)
      dfs->ath_dfs_stats.datalen_discards++;
      return (0);

   /*
    * Fill out dfs_phy_err with the information we have
    * at hand.
    */
   OS_MEMSET(e, 0, sizeof(*e));
   e->rssi = rssi;
   e->dur = dur;
   e->is_pri = 1;
   e->is_ext = 0;
   e->is_dc = 0;
   e->is_early = 1;
   e->fulltsf = fulltsf;
   e->rs_tstamp = rs_tstamp;

   /*
    * Owl only ever reports events on the primary channel;
    * it doesn't even see events on the secondary channel.
    */
   event_width = dfs_get_event_freqwidth(dfs);
   e->freq = dfs_get_event_freqcentre(dfs, 1, 0, 0) * 1000;
   e->freq_lo = e->freq - (event_width / 2) * 1000;
   e->freq_hi = e->freq + (event_width / 2) * 1000;

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR_SUM,
       "%s: rssi=%u dur=%u, freq=%d MHz, freq_lo=%d MHz, "
       "freq_hi=%d MHz\n",
       __func__,
       rssi,
       dur,
       e->freq / 1000,
       e->freq_lo / 1000,
       e->freq_hi / 1000);

   return (1);
}

/*
 * Process a Sowl/Howl style phy error.
 */
int
dfs_process_phyerr_sowl(struct ath_dfs *dfs, void *buf, u_int16_t datalen,
    u_int8_t rssi, u_int8_t ext_rssi, u_int32_t rs_tstamp, u_int64_t fulltsf,
    struct dfs_phy_err *e)
{
#define EXT_CH_RADAR_FOUND 0x02
#define PRI_CH_RADAR_FOUND 0x01
#define EXT_CH_RADAR_EARLY_FOUND 0x04
   const char *cbuf = (const char *) buf;
   u_int8_t dur = 0;
   u_int8_t pulse_bw_info, pulse_length_ext, pulse_length_pri;
   int pri_found = 0, ext_found = 0;
   int early_ext = 0;
   int event_width;

   /*
    * If radar can be detected on the extension channel, datalen zero
    * pulses are bogus, discard them.
    */
   if (!datalen) {
      dfs->ath_dfs_stats.datalen_discards++;
      return (0);
   }

   /* Ensure that we have at least three bytes of payload */
   if (datalen < 3) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS,
      "%s: short error frame (%d bytes)\n",
          __func__, datalen);
      dfs->ath_dfs_stats.datalen_discards++;
      return (0);
   }

   /*
    * Fetch the payload directly - the compiler will happily generate
    * byte-read instructions with a const char * cbuf pointer.
    */
   pulse_length_pri = cbuf[datalen - 3];
   pulse_length_ext = cbuf[datalen - 2];
   pulse_bw_info = cbuf[datalen - 1];

   /*
    * Only the last 3 bits of the BW info are relevant, they indicate
         * which channel the radar was detected in.
    */
        pulse_bw_info &= 0x07;

   /*
    * If pulse on DC, both primary and extension flags will be set
    */
   if (((pulse_bw_info & EXT_CH_RADAR_FOUND) &&
       (pulse_bw_info & PRI_CH_RADAR_FOUND))) {
      /*
       * Conducted testing, when pulse is on DC, both
       * pri and ext durations are reported to be same.
       *
       * Radiated testing, when pulse is on DC, different
       * pri and ext durations are reported, so take the
       * larger of the two
       */
      if (pulse_length_ext >= pulse_length_pri) {
         dur = pulse_length_ext;
         ext_found = 1;
      } else {
         dur = pulse_length_pri;
         pri_found = 1;
      }
      dfs->ath_dfs_stats.dc_phy_errors++;
   } else {
      if (pulse_bw_info & EXT_CH_RADAR_FOUND) {
         dur = pulse_length_ext;
         pri_found = 0;
         ext_found = 1;
         dfs->ath_dfs_stats.ext_phy_errors++;
      }
      if (pulse_bw_info & PRI_CH_RADAR_FOUND) {
         dur = pulse_length_pri;
         pri_found = 1;
         ext_found = 0;
         dfs->ath_dfs_stats.pri_phy_errors++;
      }
      if (pulse_bw_info & EXT_CH_RADAR_EARLY_FOUND) {
         dur = pulse_length_ext;
         pri_found = 0;
         ext_found = 1;
         early_ext = 1;
         dfs->ath_dfs_stats.early_ext_phy_errors++;
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
             "EARLY ext channel dur=%u rssi=%u datalen=%d\n",
             dur, rssi, datalen);
      }
      if (! pulse_bw_info) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
             "ERROR channel dur=%u rssi=%u pulse_bw_info=0x%x "
             "datalen MOD 4 = %d\n",
             dur, rssi, pulse_bw_info,
             (datalen & 0x3));
         /*
          * Bogus bandwidth info received in descriptor,
          * so ignore this PHY error
          */
         dfs->ath_dfs_stats.bwinfo_errors++;
         return (0);
      }
   }

   /*
    * Always use combined RSSI reported, unless RSSI reported on
    * extension is stronger
    */
   if ((ext_rssi > rssi) && (ext_rssi < 128)) {
      rssi = ext_rssi;
   }

   /*
    * Fill out the rssi/duration fields from above.
    */
   OS_MEMSET(e, 0, sizeof(*e));
   e->rssi = rssi;
   e->dur = dur;
   e->is_pri = pri_found;
   e->is_ext = ext_found;
   e->is_dc = !! (((pulse_bw_info & EXT_CH_RADAR_FOUND) &&
       (pulse_bw_info & PRI_CH_RADAR_FOUND)));
   e->is_early = early_ext;
   e->fulltsf = fulltsf;
   e->rs_tstamp = rs_tstamp;

   /*
    * Sowl and later can report pri/ext events.
    */
   event_width = dfs_get_event_freqwidth(dfs);
   e->freq =
       dfs_get_event_freqcentre(dfs, e->is_pri, e->is_ext, e->is_dc)
       * 1000;
   e->freq_lo = e->freq - (event_width / 2) * 1000;
   e->freq_hi = e->freq + (event_width / 2) * 1000;

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR_SUM,
       "%s: pulse_bw_info=0x%x pulse_length_ext=%u pulse_length_pri=%u "
       "rssi=%u ext_rssi=%u, freq=%d MHz, freq_lo=%d MHz, "
       "freq_hi=%d MHz\n",
       __func__,
       pulse_bw_info,
       pulse_length_ext,
       pulse_length_pri,
       rssi,
       ext_rssi,
       e->freq / 1000,
       e->freq_lo / 1000,
       e->freq_hi / 1000);

   return (1);
}

/*
 * Process a Merlin/Osprey style phy error.
 */
int
dfs_process_phyerr_merlin(struct ath_dfs *dfs, void *buf,
    u_int16_t datalen, u_int8_t rssi, u_int8_t ext_rssi, u_int32_t rs_tstamp,
    u_int64_t fulltsf, struct dfs_phy_err *e)
{
   const char *cbuf = (const char *) buf;
   u_int8_t pulse_bw_info = 0;

   /*
    * Process using the sowl code
    */
   if (! dfs_process_phyerr_sowl(dfs, buf, datalen, rssi, ext_rssi,
       rs_tstamp, fulltsf, e)) {
      return (0);
   }

   /*
    * For osprey (and Merlin) bw_info has implication for selecting
    * RSSI value.  So re-fetch the bw_info field so the RSSI values
    * can be appropriately overridden.
    */
   pulse_bw_info = cbuf[datalen - 1];

   switch (pulse_bw_info & 0x03) {
   case 0x00:
      /* No radar in ctrl or ext channel */
      rssi = 0;
      break;
   case 0x01:
      /* radar in ctrl channel */
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
          "RAW RSSI: rssi=%u ext_rssi=%u\n",
          rssi, ext_rssi);
      if (ext_rssi >= (rssi + 3)) {
         /*
          * cannot use ctrl channel RSSI if
          * extension channel is stronger
          */
         rssi = 0;
      }
      break;
   case 0x02:
      /* radar in extension channel */
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
          "RAW RSSI: rssi=%u ext_rssi=%u\n",
          rssi, ext_rssi);
      if (rssi >= (ext_rssi + 12)) {
         /*
          * cannot use extension channel RSSI if control channel
          * is stronger
          */
         rssi = 0;
      } else {
         rssi = ext_rssi;
      }
      break;
   case 0x03:
      /* when both are present use stronger one */
      if (rssi < ext_rssi) {
         rssi = ext_rssi;
      }
      break;
   }

   /*
    * Override the rssi decision made by the sowl code.
    * The rest of the fields (duration, timestamp, etc)
    * are left untouched.
    */
   e->rssi = rssi;

   return(1);
}

static void
dump_phyerr_contents(const char *d, int len)
{
#ifdef CONFIG_ENABLE_DUMP_PHYERR_CONTENTS
   int i, n, bufsize = 64;

   /*
    * This is statically sized for a 4-digit address + 16 * 2 digit
    * data string.
    *
    * It's done so the printk() passed to the kernel is an entire
    * line, so the kernel logging code will atomically print it.
    * Otherwise we'll end up with interleaved lines with output
    * from other kernel threads.
    */
   char buf[64];

   /* Initial conditions */
   buf[0] = '\n';
   n = 0;

   for (i = 0; i < len; i++) {
      if (i % 16 == 0) {
         n += snprintf(buf + n, bufsize - n,
             "%04x: ", i);
      }
      n += snprintf(buf + n, bufsize - n, "%02x ", d[i] & 0xff);
      if (i % 16 == 15) {
         n = 0;
         buf[0] = '\0';
      }
   }

   /*
    * Print the final line if we didn't print it above.
    */
   if (n != 0)
      VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s: %s\n", __func__, buf);
#endif /* def CONFIG_ENABLE_DUMP_PHYERR_CONTENTS */
}

void
dfs_process_phyerr(struct ieee80211com *ic, void *buf, u_int16_t datalen,
                   u_int8_t r_rssi, u_int8_t r_ext_rssi, u_int32_t r_rs_tstamp,
                   u_int64_t r_fulltsf, bool enable_log)
{
   struct ath_dfs *dfs = (struct ath_dfs *)ic->ic_dfs;
   struct ieee80211_channel *chan=ic->ic_curchan;
   struct dfs_event *event;
   struct dfs_phy_err e;
   int empty;

   if (dfs == NULL) {
      VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                      "%s: sc_dfs is NULL\n", __func__);
      return;
   }

   dfs->dfs_phyerr_count++;
   dump_phyerr_contents(buf, datalen);
   /*
    * XXX The combined_rssi_ok support has been removed.
    * This was only clear for Owl.
    *
    * XXX TODO: re-add this; it requires passing in the ctl/ext
    * RSSI set from the RX status descriptor.
    *
    * XXX TODO TODO: this may be done for us from the legacy
    * phy error path in ath_dev; please review that code.
    */

   /*
    * At this time we have a radar pulse that we need to examine and
    * queue. But if dfs_process_radarevent already detected radar and set
    * CHANNEL_INTERFERENCE flag then do not queue any more radar data.
    * When we are in a new channel this flag will be clear and we will
    * start queueing data for new channel. (EV74162)
    */
   if (dfs->dfs_debug_mask & ATH_DEBUG_DFS_PHYERR_PKT)
      dump_phyerr_contents(buf, datalen);

   if (chan == NULL) {
      VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
              "%s: chan is NULL\n", __func__);
      return;
   }

   adf_os_spin_lock_bh(&ic->chan_lock);
   if (IEEE80211_IS_CHAN_RADAR(chan)) {
         adf_os_spin_unlock_bh(&ic->chan_lock);
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
          "%s: Radar already found in the channel, "
          " do not queue radar data\n",
          __func__);
      return;
   }

   adf_os_spin_unlock_bh(&ic->chan_lock);
   dfs->ath_dfs_stats.total_phy_errors++;
   DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
       "%s[%d] phyerr %d len %d\n",
       __func__, __LINE__,
       dfs->ath_dfs_stats.total_phy_errors, datalen);

   /*
    * hardware stores this as 8 bit signed value.
    * we will cap it at 0 if it is a negative number
    */
   if (r_rssi & 0x80)
      r_rssi = 0;

   if (r_ext_rssi & 0x80)
      r_ext_rssi = 0;

   OS_MEMSET(&e, 0, sizeof(e));

   /*
    * This is a bit evil - instead of just passing in
    * the chip version, the existing code uses a set
    * of HAL capability bits to determine what is
    * possible.
    *
    * The way I'm decoding it is thus:
    *
    * + DFS enhancement? Merlin or later
    * + DFS extension channel? Sowl or later. (Howl?)
    * + otherwise, Owl (and legacy.)
    */
   if (dfs->dfs_caps.ath_chip_is_bb_tlv) {
      if (dfs_process_phyerr_bb_tlv(dfs, buf, datalen, r_rssi, r_ext_rssi,
                                    r_rs_tstamp, r_fulltsf,
                                    &e, enable_log) == 0) {
                        dfs->dfs_phyerr_reject_count++;
         return;
                    } else {
                        if (dfs->dfs_phyerr_freq_min > e.freq)
                            dfs->dfs_phyerr_freq_min = e. freq;

                        if (dfs->dfs_phyerr_freq_max < e.freq)
                            dfs->dfs_phyerr_freq_max = e. freq;
                    }
   } else if (dfs->dfs_caps.ath_dfs_use_enhancement) {
      if (dfs_process_phyerr_merlin(dfs, buf, datalen, r_rssi,
          r_ext_rssi, r_rs_tstamp, r_fulltsf, &e) == 0){
                  return;
                }
   } else if (dfs->dfs_caps.ath_dfs_ext_chan_ok) {
      if (dfs_process_phyerr_sowl(dfs, buf, datalen, r_rssi,
          r_ext_rssi, r_rs_tstamp, r_fulltsf, &e) == 0){
                        return;
                }
   } else {
      if (dfs_process_phyerr_owl(dfs, buf, datalen, r_rssi,
          r_ext_rssi, r_rs_tstamp, r_fulltsf, &e) == 0){
         return;
                }
   }

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "\n %s: Frequency at which the phyerror was injected = %d",
             __func__, e.freq);

   /*
    * If the hardware supports radar reporting on the extension channel
    * it will supply FFT data for longer radar pulses.
    *
    * TLV chips don't go through this software check - the hardware
    * check should be enough.  If we want to do software checking
    * later on then someone will have to craft an FFT parser
    * suitable for the TLV FFT data format.
    */
   if ((! dfs->dfs_caps.ath_chip_is_bb_tlv) &&
       dfs->dfs_caps.ath_dfs_ext_chan_ok) {
      /*
       * HW has a known issue with chirping pulses injected at or
       * around DC in 40MHz mode. Such pulses are reported with
       * much lower durations and SW then discards them because
       * they do not fit the minimum bin5 pulse duration.
       *
       * To work around this issue, if a pulse is within a 10us
       * range of the bin5 min duration, check if the pulse is
       * chirping. If the pulse is chirping, bump up the duration
       * to the minimum bin5 duration.
       *
       * This makes sure that a valid chirping pulse will not be
       * discarded because of incorrect low duration.
       *
       * TBD - Is it possible to calculate the 'real' duration of
       * the pulse using the slope of the FFT data?
       *
       * TBD - Use FFT data to differentiate between radar pulses
       * and false PHY errors.
       * This will let us reduce the number of false alarms seen.
       *
       * BIN 5 chirping pulses are only for FCC or Japan MMK4 domain
       */
      if (((dfs->dfsdomain == DFS_FCC_DOMAIN) ||
          (dfs->dfsdomain == DFS_MKK4_DOMAIN)) &&
          (e.dur >= MAYBE_BIN5_DUR) && (e.dur < MAX_BIN5_DUR)) {
         int add_dur;
         int slope = 0, dc_found = 0;

         /*
          * Set the event chirping flags; as we're doing
          * an actual chirp check.
          */
         e.do_check_chirp = 1;
         e.is_hw_chirp = 0;
         e.is_sw_chirp = 0;

         /*
          * dfs_check_chirping() expects is_pri and is_ext
          * to be '1' for true and '0' for false for now,
          * as the function itself uses these values in
          * constructing things rather than testing them
          * for 'true' or 'false'.
          */
         add_dur = dfs_check_chirping(dfs, buf, datalen,
             (e.is_pri ? 1 : 0),
             (e.is_ext ? 1 : 0),
             &slope, &dc_found);
         if (add_dur) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "old dur %d slope =%d\n", e.dur, slope);
            e.is_sw_chirp = 1;
            // bump up to a random bin5 pulse duration
            if (e.dur < MIN_BIN5_DUR) {
               e.dur = dfs_get_random_bin5_dur(dfs,
                   e.fulltsf);
            }
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "new dur %d\n", e.dur);
         } else {
            /* set the duration so that it is rejected */
            e.is_sw_chirp = 0;
            e.dur = MAX_BIN5_DUR + 100;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "is_chirping = %d dur=%d \n",
                add_dur, e.dur);
         }
      } else {
         /*
          * We have a pulse that is either bigger than
          * MAX_BIN5_DUR or * less than MAYBE_BIN5_DUR
          */
         if ((dfs->dfsdomain == DFS_FCC_DOMAIN) ||
             (dfs->dfsdomain == DFS_MKK4_DOMAIN)) {
            /*
             * XXX Would this result in very large pulses
             *     wrapping around to become short pulses?
             */
            if (e.dur >= MAX_BIN5_DUR) {
               /*
                * set the duration so that it is
                * rejected
                */
               e.dur = MAX_BIN5_DUR + 50;
            }
         }
      }
   }

   /*
    * Add the parsed, checked and filtered entry to the radar pulse
    * event list.  This is then checked by dfs_radar_processevent().
    *
    * XXX TODO: some filtering is still done below this point - fix
    * XXX this!
    */
   ATH_DFSEVENTQ_LOCK(dfs);
   empty = STAILQ_EMPTY(&(dfs->dfs_eventq));
   if (empty) {
      ATH_DFSEVENTQ_UNLOCK(dfs);
      return;
   }
   ATH_DFSEVENTQ_UNLOCK(dfs);
   /*
    * If the channel is a turbo G channel, then the event is
    * for the adaptive radio (AR) pattern matching rather than
    * radar detection.
    */
   adf_os_spin_lock_bh(&ic->chan_lock);
   if ((chan->ic_flags & CHANNEL_108G) == CHANNEL_108G) {
      adf_os_spin_unlock_bh(&ic->chan_lock);
      if (!(dfs->dfs_proc_phyerr & DFS_AR_EN)) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
             "%s: DFS_AR_EN not enabled\n",
             __func__);
         return;
      }
      ATH_DFSEVENTQ_LOCK(dfs);
      event = STAILQ_FIRST(&(dfs->dfs_eventq));
      if (event == NULL) {
         ATH_DFSEVENTQ_UNLOCK(dfs);
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS,
             "%s: no more events space left\n",
             __func__);
         return;
      }
      STAILQ_REMOVE_HEAD(&(dfs->dfs_eventq), re_list);
      ATH_DFSEVENTQ_UNLOCK(dfs);
      event->re_rssi = e.rssi;
      event->re_dur = e.dur;
      event->re_full_ts = e.fulltsf;
      event->re_ts = (e.rs_tstamp) & DFS_TSMASK;
      event->re_chanindex = dfs->dfs_curchan_radindex;
      event->re_flags = 0;
      event->sidx = e.sidx;

      /*
       * Handle chirp flags.
       */
      if (e.do_check_chirp) {
         event->re_flags |= DFS_EVENT_CHECKCHIRP;
         if (e.is_hw_chirp)
            event->re_flags |= DFS_EVENT_HW_CHIRP;
         if (e.is_sw_chirp)
            event->re_flags |= DFS_EVENT_SW_CHIRP;
      }

      ATH_ARQ_LOCK(dfs);
      STAILQ_INSERT_TAIL(&(dfs->dfs_arq), event, re_list);
      ATH_ARQ_UNLOCK(dfs);
   } else {
      if (IEEE80211_IS_CHAN_DFS(chan)) {
         adf_os_spin_unlock_bh(&ic->chan_lock);
         if (!(dfs->dfs_proc_phyerr & DFS_RADAR_EN)) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS3,
                "%s: DFS_RADAR_EN not enabled\n",
                __func__);
                                return;
         }
         /*
          * rssi is not accurate for short pulses, so do
          * not filter based on that for short duration pulses
          *
          * XXX do this filtering above?
          */
         if (dfs->dfs_caps.ath_dfs_ext_chan_ok) {
            if ((e.rssi < dfs->dfs_rinfo.rn_minrssithresh &&
                (e.dur > 4)) ||
                e.dur > (dfs->dfs_rinfo.rn_maxpulsedur) ) {
               dfs->ath_dfs_stats.rssi_discards++;
               DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
                   "Extension channel pulse is "
                   "discarded: dur=%d, "
                   "maxpulsedur=%d, rssi=%d, "
                   "minrssi=%d\n",
                  e.dur,
                  dfs->dfs_rinfo.rn_maxpulsedur,
                  e.rssi,
                  dfs->dfs_rinfo.rn_minrssithresh);
               return;
            }
         } else {
            if (e.rssi < dfs->dfs_rinfo.rn_minrssithresh ||
                e.dur > dfs->dfs_rinfo.rn_maxpulsedur) {
               /* XXX TODO add a debug statement? */
               dfs->ath_dfs_stats.rssi_discards++;
               return;
            }
         }

         /*
          * Add the event to the list, if there's space.
          */
         ATH_DFSEVENTQ_LOCK(dfs);
         event = STAILQ_FIRST(&(dfs->dfs_eventq));
         if (event == NULL) {
            ATH_DFSEVENTQ_UNLOCK(dfs);
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS,
                "%s: no more events space left\n",
                __func__);
            return;
         }
         STAILQ_REMOVE_HEAD(&(dfs->dfs_eventq), re_list);
         ATH_DFSEVENTQ_UNLOCK(dfs);

                        dfs->dfs_phyerr_queued_count++;
                        dfs->dfs_phyerr_w53_counter++;

         event->re_dur = e.dur;
         event->re_full_ts = e.fulltsf;
         event->re_ts = (e.rs_tstamp) & DFS_TSMASK;
         event->re_rssi = e.rssi;
         event->sidx = e.sidx;

         /*
          * Handle chirp flags.
          */
         if (e.do_check_chirp) {
            event->re_flags |= DFS_EVENT_CHECKCHIRP;
            if (e.is_hw_chirp)
               event->re_flags |= DFS_EVENT_HW_CHIRP;
            if (e.is_sw_chirp)
               event->re_flags |= DFS_EVENT_SW_CHIRP;
         }

         /*
          * Correctly set which channel is being reported on
          */
         if (e.is_pri) {
            event->re_chanindex = dfs->dfs_curchan_radindex;
         } else {
            if (dfs->dfs_extchan_radindex == -1) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "%s - phyerr on ext channel\n", __func__);
            }
            event->re_chanindex = dfs->dfs_extchan_radindex;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "%s New extension channel event is added "
                "to queue\n",__func__);
         }
         ATH_DFSQ_LOCK(dfs);
         STAILQ_INSERT_TAIL(&(dfs->dfs_radarq), event, re_list);
         ATH_DFSQ_UNLOCK(dfs);
      } else {
        adf_os_spin_unlock_bh(&ic->chan_lock);
      }
   }

   /*
    * Schedule the radar/AR task as appropriate.
    *
    * XXX isn't a lock needed for ath_radar_tasksched?
    */

/*
*  Commenting out the dfs_process_ar_event() since the function is never
*  called at run time as dfs_arq will be empty and the function
*  dfs_process_ar_event is obsolete and function definition is removed
*  as part of dfs_ar.c file
*
*  if (!STAILQ_EMPTY(&dfs->dfs_arq))
*     // XXX shouldn't this be a task/timer too?
*     dfs_process_ar_event(dfs, ic->ic_curchan);
*/

   if (!STAILQ_EMPTY(&dfs->dfs_radarq) && !dfs->ath_radar_tasksched) {
      dfs->ath_radar_tasksched = 1;
      OS_SET_TIMER(&dfs->ath_dfs_task_timer, 0);
   }
#undef   EXT_CH_RADAR_FOUND
#undef   PRI_CH_RADAR_FOUND
#undef   EXT_CH_RADAR_EARLY_FOUND
}
#endif /* ATH_SUPPORT_DFS */
