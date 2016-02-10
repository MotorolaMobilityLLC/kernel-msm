/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

                     dfs_phyerr_tlv.c

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
/* TO DO DFS
#include <ieee80211_channel.h>
*/
#include "dfs_phyerr.h"
#include "dfs_phyerr_tlv.h"

/*
 * Parsed radar status
 */
struct rx_radar_status {
   uint32_t raw_tsf;
   uint32_t tsf_offset;
   int      rssi;
   int      pulse_duration;
   int      is_chirp:1;
   int      delta_peak;
   int      delta_diff;
   int      sidx;
   int      freq_offset;   /* in KHz */
};


struct rx_search_fft_report {
       uint32_t         total_gain_db;
       uint32_t         base_pwr_db;
       int              fft_chn_idx;
       int              peak_sidx;
       int              relpwr_db;
       int              avgpwr_db;
       int              peak_mag;
       int              num_str_bins_ib;
};


/*
 * XXX until "fastclk" is stored in the DFS configuration..
 */
#define  PERE_IS_OVERSAMPLING(_dfs) (1)

/*
 * XXX there _has_ to be a better way of doing this!
 * -adrian
 */
static int32_t
sign_extend_32(uint32_t v, int nb)
{
   uint32_t m = 1U << (nb - 1);

   /* Chop off high bits, just in case */
   v &= v & ((1U << nb) - 1);

   /* Extend */
   return (v ^ m) - m;
}

/*
 * Calculate the frequency offset from the given signed bin index
 * from the radar summary report.
 *
 * This takes the oversampling mode into account.
 *
 * For oversampling, each bin has resolution 44MHz/128.
 * For non-oversampling, each bin has resolution 40MHz/128.
 *
 * It returns kHz - ie, 1000th's of MHz.
 */
static int
calc_freq_offset(int sindex, int is_oversampling)
{

   if (is_oversampling)
      return (sindex * (44000 / 128));
   else
      return (sindex * (40000 / 128));
}

static void
radar_summary_print(struct ath_dfs *dfs,
                    struct rx_radar_status *rsu,
                    bool enable_log)
{
   if (!enable_log)
       return;

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             " \n ############ Radar Summary ############ \n");

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - pulsedur = %d\n",
             __func__, rsu->pulse_duration);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - rssi = %d\n",
              __func__, rsu->rssi);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - ischirp = %d\n",
             __func__,  rsu->is_chirp);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - sidx = %d\n",
             __func__,  rsu->sidx);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - delta_peak = %d\n",
             __func__,  rsu->delta_peak);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - delta_diff = %d\n",
             __func__,  rsu->delta_diff);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - raw tsf = %d\n",
             __func__,  rsu->raw_tsf);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - tsf_offset = %d\n",
             __func__,  rsu->tsf_offset);

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: Radar Summary - cooked tsf = %d \n",
             __func__, (rsu->raw_tsf - rsu->tsf_offset));

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
             "%s: frequency offset = %d.%d MHz (oversampling = %d) \n",
             __func__, (int) (rsu->freq_offset / 1000),
             (int) abs(rsu->freq_offset % 1000), PERE_IS_OVERSAMPLING(dfs));

   VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
            "\n ################################### \n");
}

/*
 * Parse the radar summary frame.
 *
 * The frame contents _minus_ the TLV are passed in.
 */
static void
radar_summary_parse(struct ath_dfs *dfs, const char *buf, size_t len,
    struct rx_radar_status *rsu)
{
   uint32_t rs[2];
   int freq_centre, freq;

   /* Drop out if we have < 2 DWORDs available */
   if (len < sizeof(rs)) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR |
          ATH_DEBUG_DFS_PHYERR_SUM,
          "%s: len (%zu) < expected (%zu)!",
          __func__,
          len,
          sizeof(rs));
   }

   /*
    * Since the TLVs may be unaligned for some reason
    * we take a private copy into aligned memory.
    * This enables us to use the HAL-like accessor macros
    * into the DWORDs to access sub-DWORD fields.
    */
   OS_MEMCPY(rs, buf, sizeof(rs));

        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: two 32 bit values are: %08x %08x", __func__, rs[0], rs[1]);
// DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR, "%s (p=%p):", __func__, buf);

   /* Populate the fields from the summary report */
   rsu->tsf_offset =
       MS(rs[RADAR_REPORT_PULSE_REG_2], RADAR_REPORT_PULSE_TSF_OFFSET);
   rsu->pulse_duration =
       MS(rs[RADAR_REPORT_PULSE_REG_2], RADAR_REPORT_PULSE_DUR);
   rsu->is_chirp =
       MS(rs[RADAR_REPORT_PULSE_REG_1], RADAR_REPORT_PULSE_IS_CHIRP);
   rsu->sidx = sign_extend_32(
           MS(rs[RADAR_REPORT_PULSE_REG_1], RADAR_REPORT_PULSE_SIDX),
             10);
   rsu->freq_offset =
           calc_freq_offset(rsu->sidx, PERE_IS_OVERSAMPLING(dfs));

   /* These are only relevant if the pulse is a chirp */
   rsu->delta_peak = sign_extend_32(
       MS(rs[RADAR_REPORT_PULSE_REG_1], RADAR_REPORT_PULSE_DELTA_PEAK),
           6);
   rsu->delta_diff =
       MS(rs[RADAR_REPORT_PULSE_REG_1], RADAR_REPORT_PULSE_DELTA_DIFF);

   /* WAR for FCC Type 4*/
   /*
    * HW is giving longer pulse duration (in case of VHT80, with traffic)
    * which fails to detect FCC type4 radar pulses. Added a work around to
    * fix the pulse duration and duration delta.
    *
    * IF VHT80
    *   && (primary_channel==30MHz || primary_channel== -30MHz)
    *   && -4 <= pulse_index <= 4
    *   && !chirp
    *   && pulse duration > 20 us
    * THEN
    *   Set pulse duration to 20 us
    */

   adf_os_spin_lock_bh(&dfs->ic->chan_lock);
   freq = ieee80211_chan2freq(dfs->ic, dfs->ic->ic_curchan);
   freq_centre = dfs->ic->ic_curchan->ic_vhtop_ch_freq_seg1;

   if ((IEEE80211_IS_CHAN_11AC_VHT80(dfs->ic->ic_curchan) &&
            (abs(freq - freq_centre) == 30) &&
            !rsu->is_chirp &&
            abs(rsu->sidx) <= 4 &&
            rsu->pulse_duration > 20)){
      rsu->pulse_duration = 20;
   }

   adf_os_spin_unlock_bh(&dfs->ic->chan_lock);
}

static void
radar_fft_search_report_parse(struct ath_dfs *dfs, const char *buf, size_t len,
    struct rx_search_fft_report *rsfr)
{
   uint32_t rs[2];

   /* Drop out if we have < 2 DWORDs available */
   if (len < sizeof(rs)) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR |
          ATH_DEBUG_DFS_PHYERR_SUM,
          "%s: len (%zu) < expected (%zu)!",
          __func__,
          len,
          sizeof(rs));
   }

   /*
    * Since the TLVs may be unaligned for some reason
    * we take a private copy into aligned memory.
    * This enables us to use the HAL-like accessor macros
    * into the DWORDs to access sub-DWORD fields.
    */
   OS_MEMCPY(rs, buf, sizeof(rs));
        rsfr->total_gain_db   = MS(rs[SEARCH_FFT_REPORT_REG_1], SEARCH_FFT_REPORT_TOTAL_GAIN_DB);
        rsfr->base_pwr_db     = MS(rs[SEARCH_FFT_REPORT_REG_1], SEARCH_FFT_REPORT_BASE_PWR_DB);
        rsfr->fft_chn_idx     = MS(rs[SEARCH_FFT_REPORT_REG_1], SEARCH_FFT_REPORT_FFT_CHN_IDX);
        rsfr->peak_sidx       = sign_extend_32(MS(rs[SEARCH_FFT_REPORT_REG_1], SEARCH_FFT_REPORT_PEAK_SIDX), 12);
        rsfr->relpwr_db       = MS(rs[SEARCH_FFT_REPORT_REG_2], SEARCH_FFT_REPORT_RELPWR_DB);
        rsfr->avgpwr_db       = MS(rs[SEARCH_FFT_REPORT_REG_2], SEARCH_FFT_REPORT_AVGPWR_DB);
        rsfr->peak_mag        = MS(rs[SEARCH_FFT_REPORT_REG_2], SEARCH_FFT_REPORT_PEAK_MAG);
        rsfr->num_str_bins_ib = MS(rs[SEARCH_FFT_REPORT_REG_2], SEARCH_FFT_REPORT_NUM_STR_BINS_IB);

        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: two 32 bit values are: %08x %08x", __func__, rs[0], rs[1]);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->total_gain_db = %d", __func__, rsfr->total_gain_db);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->base_pwr_db = %d", __func__, rsfr->base_pwr_db);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->fft_chn_idx = %d", __func__, rsfr->fft_chn_idx);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->peak_sidx = %d", __func__, rsfr->peak_sidx);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->relpwr_db = %d", __func__, rsfr->relpwr_db);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->avgpwr_db = %d", __func__, rsfr->avgpwr_db);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->peak_mag = %d", __func__, rsfr->peak_mag);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,"%s: rsfr->num_str_bins_ib = %d", __func__, rsfr->num_str_bins_ib);
}

/*
 * Parse a Peregrine BB TLV frame.
 *
 * This routine parses each TLV, prints out what's going on and
 * calls an appropriate sub-function.
 *
 * Since the TLV format doesn't _specify_ all TLV components are
 * DWORD aligned, we must treat them as not and access the fields
 * appropriately.
 */
static int
tlv_parse_frame(struct ath_dfs *dfs, struct rx_radar_status *rs,
    struct rx_search_fft_report *rsfr, const char *buf, size_t len, u_int8_t rssi)
{
   int i = 0;
   uint32_t tlv_hdr[1];
        bool first_tlv = true;
        bool false_detect = false;

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
       "%s: total length = %zu bytes", __func__, len);
   while ((i < len ) && (false_detect == false)) {
      /* Ensure we at least have four bytes */
      if ((len - i) < sizeof(tlv_hdr)) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR |
             ATH_DEBUG_DFS_PHYERR_SUM,
             "%s: ran out of bytes, len=%zu, i=%d",
             __func__, len, i);
         return (0);
      }

      /*
       * Copy the offset into the header,
       * so the DWORD style access macros
       * can be used.
       */
      OS_MEMCPY(&tlv_hdr, buf + i, sizeof(tlv_hdr));

      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
          "%s: HDR: TLV SIG=0x%x, TAG=0x%x, LEN=%d bytes",
          __func__,
         MS(tlv_hdr[TLV_REG], TLV_SIG),
          MS(tlv_hdr[TLV_REG], TLV_TAG),
          MS(tlv_hdr[TLV_REG], TLV_LEN));

      /*
       * Sanity check the length field is available in
       * the remaining frame.  Drop out if this isn't
       * the case - we can't trust the rest of the TLV
       * entries.
       */
      if (MS(tlv_hdr[TLV_REG], TLV_LEN) + i >= len) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
             "%s: TLV oversize: TLV LEN=%d, available=%zu, "
             "i=%d",
             __func__,
             MS(tlv_hdr[TLV_REG], TLV_LEN),
             len,
             i);
         break;
      }

      /* Skip the TLV header - one DWORD */
      i += sizeof(tlv_hdr);

      /* Handle the payload */
      /* XXX TODO! */
      switch (MS(tlv_hdr[TLV_REG], TLV_SIG)) {
         case TAG_ID_RADAR_PULSE_SUMMARY:    /* Radar pulse summary */
            /* XXX TODO verify return value */
            /* XXX TODO validate only seeing one of these */
            radar_summary_parse(dfs, buf + i,
                MS(tlv_hdr[TLV_REG], TLV_LEN), rs);
            break;
                        case TAG_ID_SEARCH_FFT_REPORT:
                                radar_fft_search_report_parse(dfs, buf + i,
                MS(tlv_hdr[TLV_REG], TLV_LEN), rsfr);
                                /*
                                   we examine search FFT report and make the following assumption
                                   as per algorithms group's input:
                                   (1) There may be multiple TLV
                                   (2) We make false detection decison solely based on the first TLV
                                   (3) If the first TLV is a serch FFT report then we check the peak_mag value.
                                       When RSSI is equal to dfs->ath_dfs_false_rssI_thres (default 50)
                                       and peak_mag is less than 2 * dfs->ath_dfs_peak_mag (default 40)
                                       we treat it as false detect.
                                       Please note that 50 is not a true RSSI estimate, but value indicated
                                       by HW for RF saturation event.
                                */

                                if ((first_tlv == true) &&
                                    (rssi ==  dfs->ath_dfs_false_rssi_thres) &&
                                    (rsfr->peak_mag <  (2 * dfs->ath_dfs_peak_mag))) {
                                    false_detect = true;
                               DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR, "%s: setting false_detect to TRUE", __func__);
                                }

                                break;
         default:
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR,
                "%s: unknown entry, SIG=0x%02x",
                __func__,
                MS(tlv_hdr[TLV_REG], TLV_SIG));
      }

      /* Skip the payload */
      i += MS(tlv_hdr[TLV_REG], TLV_LEN);
                first_tlv = false;
   }
   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR, "%s: done", __func__);

   return (false_detect? 0 : 1);
}

/*
 * Calculate the channel centre in MHz.
 */
static int
tlv_calc_freq_info(struct ath_dfs *dfs, struct rx_radar_status *rs)
{
   uint32_t chan_centre;
   uint32_t chan_width;
   int chan_offset;

   /*
    * For now, just handle up to VHT80 correctly.
    */
   if (dfs->ic == NULL || dfs->ic->ic_curchan == NULL) {
      DFS_PRINTK("%s: dfs->ic=%p, that or curchan is null?",
          __func__, dfs->ic);
      return (0);
   }

   adf_os_spin_lock_bh(&dfs->ic->chan_lock);
   /*
    * For now, the only 11ac channel with freq1/freq2 setup is
    * VHT80.
    *
    * XXX should have a flag macro to check this!
    */
   if (IEEE80211_IS_CHAN_11AC_VHT80(dfs->ic->ic_curchan)) {
      /* 11AC, so cfreq1/cfreq2 are setup */

      /*
       * XXX if it's 80+80 this won't work - need to use seg
       * appropriately!
       */

      chan_centre = dfs->ic->ic_curchan->ic_vhtop_ch_freq_seg1;
   } else {
      /* HT20/HT40 */

      /*
       * XXX this is hard-coded - it should be 5 or 10 for
       * half/quarter appropriately.
       */
      chan_width = 20;

      /* Grab default channel centre */
      chan_centre = ieee80211_chan2freq(dfs->ic,
          dfs->ic->ic_curchan);

      /* Calculate offset based on HT40U/HT40D and VHT40U/VHT40D */
      if (IEEE80211_IS_CHAN_11N_HT40PLUS(dfs->ic->ic_curchan) ||
          dfs->ic->ic_curchan->ic_flags & IEEE80211_CHAN_VHT40PLUS)
         chan_offset = chan_width;
      else if (IEEE80211_IS_CHAN_11N_HT40MINUS(dfs->ic->ic_curchan) ||
         dfs->ic->ic_curchan->ic_flags & IEEE80211_CHAN_VHT40MINUS)
         chan_offset = -chan_width;
      else
         chan_offset = 0;

      /* Calculate new _real_ channel centre */
      chan_centre += (chan_offset / 2);
   }
   adf_os_spin_unlock_bh(&dfs->ic->chan_lock);

   /*
    * XXX half/quarter rate support!
    */

   /* Return ev_chan_centre in MHz */
   return (chan_centre);
}

/*
 * Calculate the centre frequency and low/high range for a radar pulse event.
 *
 * XXX TODO: Handle half/quarter rates correctly!
 * XXX TODO: handle VHT160 correctly!
 * XXX TODO: handle VHT80+80 correctly!
 */
static int
tlv_calc_event_freq_pulse(struct ath_dfs *dfs, struct rx_radar_status *rs,
    uint32_t *freq_centre, uint32_t *freq_lo, uint32_t *freq_hi)
{
   int chan_width;
   int chan_centre;

   /* Fetch the channel centre frequency in MHz */
   chan_centre = tlv_calc_freq_info(dfs, rs);

   /* Convert to KHz */
   chan_centre *= 1000;

   /*
    * XXX hard-code event width to be 2 * bin size for now;
    * XXX this needs to take into account the core clock speed
    * XXX for half/quarter rate mode.
    */
   if (PERE_IS_OVERSAMPLING(dfs))
      chan_width = (44000 * 2 / 128);
   else
      chan_width = (40000 * 2 / 128);

   /* XXX adjust chan_width for half/quarter rate! */

   /*
    * Now we can do the math to figure out the correct channel range.
    */
   (*freq_centre) = (uint32_t) (chan_centre + rs->freq_offset);
   (*freq_lo) = (uint32_t) ((chan_centre + rs->freq_offset)
       - chan_width);
   (*freq_hi) = (uint32_t) ((chan_centre + rs->freq_offset)
       + chan_width);

   return (1);
}

/*
 * The chirp bandwidth in KHz is defined as:
 *
 * totalBW(KHz) = delta_peak(mean)
 *    * [ (bin resolution in KHz) / (radar_fft_long_period in uS) ]
 *    * pulse_duration (us)
 *
 * The bin resolution depends upon oversampling.
 *
 * For now, we treat the radar_fft_long_period as a hard-coded 8uS.
 */
static int
tlv_calc_event_freq_chirp(struct ath_dfs *dfs, struct rx_radar_status *rs,
    uint32_t *freq_centre, uint32_t *freq_lo, uint32_t *freq_hi)
{
   int32_t bin_resolution;    /* KHz * 100 */
   int32_t radar_fft_long_period = 8;  /* microseconds */
   int32_t delta_peak;
   int32_t pulse_duration;
   int32_t total_bw;
   int32_t chan_centre;
   int32_t freq_1, freq_2;

   /*
    * KHz isn't enough resolution here!
    * So treat it as deci-hertz (10Hz) and convert back to KHz
    * later.
    */
   if (PERE_IS_OVERSAMPLING(dfs))
      bin_resolution = (44000 * 100) / 128;
   else
      bin_resolution = (40000 * 100) / 128;

   delta_peak = rs->delta_peak;
   pulse_duration = rs->pulse_duration;

   total_bw = delta_peak * (bin_resolution / radar_fft_long_period) *
       pulse_duration;

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR | ATH_DEBUG_DFS_PHYERR_SUM,
       "%s: delta_peak=%d, pulse_duration=%d, bin_resolution=%d.%dKHz, "
           "radar_fft_long_period=%d, total_bw=%d.%ldKHz",
       __func__,
       delta_peak,
       pulse_duration,
       bin_resolution / 1000,
       bin_resolution % 1000,
       radar_fft_long_period,
       total_bw / 100,
       abs(total_bw % 100));

       total_bw /= 100; /* back to KHz */

   /* Grab the channel centre frequency in MHz */
   chan_centre = tlv_calc_freq_info(dfs, rs);

   /* Early abort! */
   if (chan_centre == 0) {
      (*freq_centre) = 0;
      return (0);
   }

   /* Convert to KHz */
   chan_centre *= 1000;

   /*
    * sidx is the starting frequency; total_bw is a signed value and
    * for negative chirps (ie, moving down in frequency rather than up)
    * the end frequency may be less than the start frequency.
    */
   if (total_bw > 0) {
      freq_1 = chan_centre + rs->freq_offset;
      freq_2 = chan_centre + rs->freq_offset + total_bw;
   } else {
      freq_1 = chan_centre + rs->freq_offset + total_bw;
      freq_2 = chan_centre + rs->freq_offset;
   }

   (*freq_lo) = (uint32_t)(freq_1);
   (*freq_hi) = (uint32_t)(freq_2);
   (*freq_centre) = (uint32_t) (freq_1 + (abs(total_bw) / 2));

   return (1);
}

/*
 * Calculate the centre and band edge frequencies of the given radar
 * event.
 */
static int
tlv_calc_event_freq(struct ath_dfs *dfs, struct rx_radar_status *rs,
    uint32_t *freq_centre, uint32_t *freq_lo, uint32_t *freq_hi)
{
   if (rs->is_chirp)
      return tlv_calc_event_freq_chirp(dfs, rs, freq_centre,
          freq_lo, freq_hi);
   else
      return tlv_calc_event_freq_pulse(dfs, rs, freq_centre,
          freq_lo, freq_hi);
}

/*
 * This is the public facing function which parses the PHY error
 * and populates the dfs_phy_err struct.
 */
int
dfs_process_phyerr_bb_tlv(struct ath_dfs *dfs, void *buf, u_int16_t datalen,
                          u_int8_t rssi, u_int8_t ext_rssi, u_int32_t rs_tstamp,
                          u_int64_t fulltsf, struct dfs_phy_err *e,
                          bool enable_log)
{
   struct rx_radar_status rs;
        struct rx_search_fft_report rsfr;
   static int invalid_phyerr_count = 0;

   OS_MEMZERO(&rs, sizeof(rs));

   /*
    * Add the ppdu_start/ppdu_end fields given to us by the upper
    * layers.  The firmware gives us a summary set of parameters rather
    * than the whole PPDU_START/PPDU_END descriptor contenst.
    */
   rs.rssi = rssi;
   rs.raw_tsf = rs_tstamp;
   /*
    * Try parsing the TLV set.
    */
   if (! tlv_parse_frame(dfs, &rs, &rsfr, buf, datalen, rssi)){
       invalid_phyerr_count++;
       /*
        * Print only at every 2 power times
        * to avoid flushing of the kernel
        * logs, since the frequency of
        * invalid phyerrors is very high
        * in noisy environments.
        */
       if ( !(invalid_phyerr_count & 0xFF) )
       {
           VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_DEBUG,
                     "%s[%d]:DFS-tlv parse failed invalid phyerror count = %d",
                     __func__,__LINE__, invalid_phyerr_count);
       }
       return (0);
   }
   /* For debugging, print what we have parsed */
   radar_summary_print(dfs, &rs, enable_log);

   /* Populate dfs_phy_err from rs */
   OS_MEMSET(e, 0, sizeof(*e));
   e->rssi = rs.rssi;
   e->dur = rs.pulse_duration;
   e->sidx = rs.sidx;
   e->is_pri = 1;    /* XXX always PRI for now */
   e->is_ext = 0;
   e->is_dc = 0;
   e->is_early = 0;
   /*
    * XXX TODO: add a "chirp detection enabled" capability or config
    * bit somewhere, in case for some reason the hardware chirp
    * detection AND FFTs are disabled.
    */
   /* For now, assume this hardware always does chirp detection */
   e->do_check_chirp = 1;
   e->is_hw_chirp = !! (rs.is_chirp);
   e->is_sw_chirp = 0;  /* We don't yet do software chirp checking */

   e->fulltsf = fulltsf;
   e->rs_tstamp = rs.raw_tsf - rs.tsf_offset;

   /* XXX error check */
   (void) tlv_calc_event_freq(dfs, &rs, &e->freq, &e->freq_lo,
       &e->freq_hi);

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_PHYERR_SUM,
       "%s: fbin=%d, freq=%d.%d MHz, raw tsf=%u, offset=%d, "
       "cooked tsf=%u, rssi=%d, dur=%d, is_chirp=%d, fulltsf=%llu, "
       "freq=%d.%d MHz, freq_lo=%d.%dMHz, freq_hi=%d.%d MHz",
       __func__,
       rs.sidx,
       (int) (rs.freq_offset / 1000),
       (int) abs(rs.freq_offset % 1000),
       rs.raw_tsf,
       rs.tsf_offset,
       e->rs_tstamp,
       rs.rssi,
       rs.pulse_duration,
       (int) rs.is_chirp,
       (unsigned long long) fulltsf,
       (int) e->freq / 1000,
       (int) abs(e->freq) % 1000,
       (int) e->freq_lo / 1000,
       (int) abs(e->freq_lo) % 1000,
       (int) e->freq_hi / 1000,
       (int) abs(e->freq_hi) % 1000);

   return (1);
}
