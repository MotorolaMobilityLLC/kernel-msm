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

                              dfs_fcc_bin5.c

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

/*
 * Reject the pulse if:
 * + It's outside the RSSI threshold;
 * + It's outside the pulse duration;
 * + It's been verified by HW/SW chirp checking
 *   and neither of those found a chirp.
 */
int
dfs_bin5_check_pulse(struct ath_dfs *dfs, struct dfs_event *re,
    struct dfs_bin5radars *br)
{
   int b5_rssithresh = br->br_pulse.b5_rssithresh;

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_PULSE,
       "%s: re_dur=%d, rssi=%d, check_chirp=%d, "
       "hw_chirp=%d, sw_chirp=%d\n",
       __func__,
       (int) re->re_dur,
       (int) re->re_rssi,
       !! (re->re_flags & DFS_EVENT_CHECKCHIRP),
       !! (re->re_flags & DFS_EVENT_HW_CHIRP),
       !! (re->re_flags & DFS_EVENT_SW_CHIRP));

   /*
    * If the sw/hw chirp detection says to fail the pulse,
    * do so.
    */
   if (DFS_EVENT_NOTCHIRP(re)) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
          "%s: rejecting chirp: ts=%llu, dur=%d, rssi=%d "
          "checkchirp=%d, hwchirp=%d, swchirp=%d\n",
          __func__,
          (unsigned long long) re->re_full_ts,
          (int) re->re_dur,
          (int) re->re_rssi,
          !! (re->re_flags & DFS_EVENT_CHECKCHIRP),
          !! (re->re_flags & DFS_EVENT_HW_CHIRP),
          !! (re->re_flags & DFS_EVENT_SW_CHIRP));
      return (0);
   }

   /* Adjust the filter threshold for rssi in non TURBO mode */
   if( ! (dfs->ic->ic_curchan->ic_flags & CHANNEL_TURBO))
      b5_rssithresh += br->br_pulse.b5_rssimargin;

   /*
    * Check if the pulse is within duration and rssi
    * thresholds.
    */
   if ((re->re_dur >= br->br_pulse.b5_mindur) &&
       (re->re_dur <= br->br_pulse.b5_maxdur) &&
       (re->re_rssi >= b5_rssithresh)) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
          "%s: dur=%d, rssi=%d - adding!\n",
          __func__, (int) re->re_dur, (int) re->re_rssi);
      return (1);
   }

   DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
       "%s: too low to be Bin5 pulse tsf=%llu, dur=%d, rssi=%d\n",
       __func__,
       (unsigned long long) re->re_full_ts,
       (int) re->re_dur,
       (int) re->re_rssi);

   return (0);
}


int dfs_bin5_addpulse(struct ath_dfs *dfs, struct dfs_bin5radars *br,
        struct dfs_event *re, u_int64_t thists)
{
   u_int32_t index,stop;
   u_int64_t tsDelta;

   /* Check if this pulse is a valid pulse in terms of repetition,
    * if not, return without adding it to the queue.
    * PRI : Pulse Repitetion Interval
    * BRI : Burst Repitetion Interval */
   if( br->br_numelems != 0){
      index = br->br_lastelem;
      tsDelta = thists - br->br_elems[index].be_ts;
      if( (tsDelta < DFS_BIN5_PRI_LOWER_LIMIT) ||
         ( (tsDelta > DFS_BIN5_PRI_HIGHER_LIMIT) &&
           (tsDelta < DFS_BIN5_BRI_LOWER_LIMIT))) {
             return 0;
      }
   }
   /* Circular buffer of size 2^n */
   index = (br->br_lastelem +1) & DFS_MAX_B5_MASK;
   br->br_lastelem = index;
        if (br->br_numelems == DFS_MAX_B5_SIZE)
      br->br_firstelem = (br->br_firstelem+1)&DFS_MAX_B5_MASK;
   else
      br->br_numelems++;
   br->br_elems[index].be_ts = thists;
   br->br_elems[index].be_rssi = re->re_rssi;
   br->br_elems[index].be_dur = re->re_dur;  /* please note that this is in u-sec */
   stop = 0;
   index = br->br_firstelem;
   while ((!stop) && (br->br_numelems-1) > 0) {
      if ((thists - br->br_elems[index].be_ts) >
          ((u_int64_t) br->br_pulse.b5_timewindow)) {
         br->br_numelems--;
         br->br_firstelem = (br->br_firstelem +1) & DFS_MAX_B5_MASK;
         index = br->br_firstelem;
      } else
         stop = 1;
   }
   return 1;
}

/*
 * If the dfs structure is NULL (which should be illegal if everyting is working
 * properly, then signify that a bin5 radar was found
 */

int
dfs_bin5_check(struct ath_dfs *dfs)
{
   struct dfs_bin5radars *br;
        int index[DFS_MAX_B5_SIZE];
   u_int32_t n = 0, i = 0, i1 = 0, this = 0, prev = 0, rssi_diff = 0, width_diff = 0, bursts= 0;
        u_int32_t total_diff=0, average_diff=0, total_width=0, average_width=0, numevents=0;
   u_int64_t pri;


   if (dfs == NULL) {
      DFS_PRINTK("%s: ic_dfs is NULL\n", __func__);
      return 1;
   }
   for (n=0;n<dfs->dfs_rinfo.rn_numbin5radars; n++) {
      br = &(dfs->dfs_b5radars[n]);
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
         "Num elems = %d\n", br->br_numelems);

        /* find a valid bin 5 pulse and use it as reference */
        for(i1=0;i1 < br->br_numelems; i1++) {
         this = ((br->br_firstelem +i1) & DFS_MAX_B5_MASK);
            if ((br->br_elems[this].be_dur >= MIN_BIN5_DUR_MICROSEC) &&
                (br->br_elems[this].be_dur <= MAX_BIN5_DUR_MICROSEC)) {
                break;
            }
        }

        prev = this;
        for(i = i1 + 1; i < br->br_numelems; i++){
         this = ((br->br_firstelem +i) & DFS_MAX_B5_MASK);

            /* first make sure it is a bin 5 pulse by checking the duration */
            if ((br->br_elems[this].be_dur < MIN_BIN5_DUR_MICROSEC) || (br->br_elems[this].be_dur > MAX_BIN5_DUR_MICROSEC)) {
                continue;
            }

         /* Rule 1: 1000 <= PRI <= 2000 + some margin */
         if( br->br_elems[this].be_ts >= br->br_elems[prev].be_ts ) {
            pri = br->br_elems[this].be_ts - br->br_elems[prev].be_ts;
         }
         else {//roll over case
            //pri = (0xffffffffffffffff - br->br_elems[prev].be_ts) + br->br_elems[this].be_ts;
            pri = br->br_elems[this].be_ts;
         }
              DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
             " pri=%llu this.ts=%llu this.dur=%d this.rssi=%d prev.ts=%llu\n",
             (unsigned long long)pri,
             (unsigned long long)br->br_elems[this].be_ts,
             (int) br->br_elems[this].be_dur,
             (int) br->br_elems[this].be_rssi,
             (unsigned long long)br->br_elems[prev].be_ts);
         if(( (pri >= DFS_BIN5_PRI_LOWER_LIMIT) && (pri <= DFS_BIN5_PRI_HIGHER_LIMIT))) {  //pri: pulse repitition interval in us
            /* Rule 2: pulse width of the pulses in the burst should be same (+/- margin) */
            if( br->br_elems[this].be_dur >= br->br_elems[prev].be_dur) {
               width_diff = br->br_elems[this].be_dur - br->br_elems[prev].be_dur;
            }
            else {
               width_diff = br->br_elems[prev].be_dur - br->br_elems[this].be_dur;
            }
            if( width_diff <= DFS_BIN5_WIDTH_MARGIN ) {
               /* Rule 3: RSSI of the pulses in the burst should be same (+/- margin) */
               if( br->br_elems[this].be_rssi >= br->br_elems[prev].be_rssi) {
                  rssi_diff = br->br_elems[this].be_rssi - br->br_elems[prev].be_rssi;
               }
               else {
                  rssi_diff = br->br_elems[prev].be_rssi - br->br_elems[this].be_rssi;
               }
               if( rssi_diff <= DFS_BIN5_RSSI_MARGIN ) {
                  bursts++;
                                                /* Save the indexes of this pair for later width variance check */
                                                if( numevents >= 2 ) {
                                                        /* make sure the event is not duplicated,
                                                         * possible in a 3 pulse burst */
                                                        if( index[numevents-1] != prev) {
                                                                index[numevents++] = prev;
                                                        }
                                                }
                                                else {
                                                        index[numevents++] = prev;                                                }
                                                index[numevents++] = this;
               } else {
                                       DFS_DPRINTK(dfs,
                       ATH_DEBUG_DFS_BIN5,
                  "%s %d Bin5 rssi_diff=%d\n",
                  __func__, __LINE__, rssi_diff);
                                            }
                      } else {
               DFS_DPRINTK(dfs,
                   ATH_DEBUG_DFS_BIN5,
                   "%s %d Bin5 width_diff=%d\n",
                   __func__, __LINE__,
                   width_diff);
                                        }
         } else if ((pri >= DFS_BIN5_BRI_LOWER_LIMIT) &&
                       (pri <= DFS_BIN5_BRI_UPPER_LIMIT)) {
                // check pulse width to make sure it is in range of bin 5
                //if ((br->br_elems[this].be_dur >= MIN_BIN5_DUR_MICROSEC) && (br->br_elems[this].be_dur <= MAX_BIN5_DUR_MICROSEC)) {
                    bursts++;
                //}
            } else{
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
          "%s %d Bin5 PRI check fail pri=%llu\n",
          __func__, __LINE__, (unsigned long long)pri);
            }
          prev = this;
      }

                DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5, "bursts=%u numevents=%u\n", bursts, numevents);
      if ( bursts >= br->br_pulse.b5_threshold) {
                        if( (br->br_elems[br->br_lastelem].be_ts - br->br_elems[br->br_firstelem].be_ts) < 3000000 ) {
                                return 0;
                        }
                        else {
            /*
            * don't do this check since not all the cases have this kind of burst width variation.
            *
                                for (i=0; i<bursts; i++){
                                        total_width += br->br_elems[index[i]].be_dur;
                                }
                                average_width = total_width/bursts;
                                for (i=0; i<bursts; i++){
                                        total_diff += DFS_DIFF(br->br_elems[index[i]].be_dur, average_width);
                                }
                                average_diff = total_diff/bursts;
                                if( average_diff > DFS_BIN5_WIDTH_MARGIN ) {
                                        return 1;
                                } else {

                                    DFS_DPRINTK(ic, ATH_DEBUG_DFS_BIN5, "bursts=%u numevents=%u total_width=%d average_width=%d total_diff=%d average_diff=%d\n", bursts, numevents, total_width, average_width, total_diff, average_diff);

                                }
            */
                           DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5, "bursts=%u numevents=%u total_width=%d average_width=%d total_diff=%d average_diff=%d\n", bursts, numevents, total_width, average_width, total_diff, average_diff);
                           DFS_PRINTK("bin 5 radar detected, bursts=%d\n", bursts);
                           return 1;
                        }
      }
   }

   return 0;
}


/* Return TRUE if chirping pulse, FALSE if not.
   Decision is made based on processing the FFT data included with the PHY error.
   Calculate the slope using the maximum bin index reported in the FFT data.
   Calculate slope between FFT packet 0 and packet n-1. Also calculate slope between
   packet 1 and packet n.
   If a pulse is chirping, a slope of 5 and greater is seen.
   Non-chirping pulses have slopes of 0, 1, 2 or 3.
*/

/*
 * Chirp detection for Sowl/Howl.
 */
static int
dfs_check_chirping_sowl(struct ath_dfs *dfs, void *buf,
    u_int16_t datalen, int is_ctl, int is_ext, int *slope, int *is_dc)
{
#define FFT_LEN 70
#define FFT_LOWER_BIN_MAX_INDEX_BYTE 66
#define FFT_UPPER_BIN_MAX_INDEX_BYTE 69
#define MIN_CHIRPING_SLOPE 4
    int is_chirp=0;
    int p, num_fft_packets=0;
    int ctl_slope=0, ext_slope=0;
    int ctl_high0, ctl_low0, ctl_slope0=0, ext_high0, ext_low0, ext_slope0=0;
    int ctl_high1, ctl_low1, ctl_slope1=0, ext_high1, ext_low1, ext_slope1=0;
    u_int8_t *fft_data_ptr;

    *slope = 0;
    *is_dc = 0;

    num_fft_packets = datalen / FFT_LEN;
    fft_data_ptr = ((u_int8_t*)buf);

    /* DEBUG - Print relevant portions of the FFT data*/
    for (p=0; p < num_fft_packets; p++) {

        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT, "fft_data_ptr=0x%p\t", fft_data_ptr);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "[66]=%d [69]=%d\n",
          *(fft_data_ptr+FFT_LOWER_BIN_MAX_INDEX_BYTE) >> 2,
          *(fft_data_ptr+FFT_UPPER_BIN_MAX_INDEX_BYTE) >> 2);

        fft_data_ptr += FFT_LEN;
    }

    DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT, "datalen=%d num_fft_packets=%d\n", datalen, num_fft_packets);

    /* There is not enough FFT data to figure out whether the pulse is chirping or not*/
    if (num_fft_packets < 4) {
        return 0;
    }

    fft_data_ptr = ((u_int8_t*)buf);

    if (is_ctl) {

        fft_data_ptr = ((u_int8_t*)buf);
        ctl_low0 = *(fft_data_ptr+FFT_LOWER_BIN_MAX_INDEX_BYTE) >>  2;
        fft_data_ptr += FFT_LEN;
        ctl_low1 = *(fft_data_ptr+FFT_LOWER_BIN_MAX_INDEX_BYTE) >>  2;

        // last packet with first packet
        fft_data_ptr = ((u_int8_t*)buf) + (FFT_LEN*(num_fft_packets - 1));
        ctl_high1 = *(fft_data_ptr+FFT_LOWER_BIN_MAX_INDEX_BYTE) >> 2;

        // second last packet with 0th packet
        fft_data_ptr = ((u_int8_t*)buf) + (FFT_LEN*(num_fft_packets - 2));
        ctl_high0 = *(fft_data_ptr+FFT_LOWER_BIN_MAX_INDEX_BYTE) >> 2;

        ctl_slope0 = ctl_high0 - ctl_low0;
        if (ctl_slope0 < 0) ctl_slope0 *= (-1);

        ctl_slope1 = ctl_high1 - ctl_low1;
        if (ctl_slope1 < 0) ctl_slope1 *= (-1);

        ctl_slope = ((ctl_slope0 > ctl_slope1) ? ctl_slope0: ctl_slope1);
        *slope = ctl_slope;
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT, "ctl_slope0=%d ctl_slope1=%d ctl_slope=%d\n",
            ctl_slope0, ctl_slope1, ctl_slope);

    } else if (is_ext) {

        fft_data_ptr = ((u_int8_t*)buf);
        ext_low0 = *(fft_data_ptr+FFT_UPPER_BIN_MAX_INDEX_BYTE) >>  2;

        fft_data_ptr += FFT_LEN;
        ext_low1 = *(fft_data_ptr+FFT_UPPER_BIN_MAX_INDEX_BYTE) >>  2;

        fft_data_ptr = ((u_int8_t*)buf) + (FFT_LEN*(num_fft_packets - 1));
        ext_high1 = *(fft_data_ptr+FFT_UPPER_BIN_MAX_INDEX_BYTE) >> 2;
        fft_data_ptr = ((u_int8_t*)buf) + (FFT_LEN*(num_fft_packets - 2));
        ext_high0 = *(fft_data_ptr+FFT_UPPER_BIN_MAX_INDEX_BYTE) >> 2;

        ext_slope0 = ext_high0 - ext_low0;
        if (ext_slope0 < 0) ext_slope0 *= (-1);

        ext_slope1 = ext_high1 - ext_low1;
        if (ext_slope1 < 0) ext_slope1 *= (-1);

        ext_slope = ((ext_slope0 > ext_slope1) ? ext_slope0: ext_slope1);
        *slope = ext_slope;
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT | ATH_DEBUG_DFS_BIN5,
          "ext_slope0=%d ext_slope1=%d ext_slope=%d\n",
          ext_slope0, ext_slope1, ext_slope);
    } else
        return 0;

    if ((ctl_slope >= MIN_CHIRPING_SLOPE) || (ext_slope >= MIN_CHIRPING_SLOPE)) {
        is_chirp = 1;
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5 | ATH_DEBUG_DFS_BIN5_FFT |
          ATH_DEBUG_DFS_PHYERR_SUM,
          "is_chirp=%d is_dc=%d\n", is_chirp, *is_dc);
    }
    return is_chirp;

#undef FFT_LEN
#undef FFT_LOWER_BIN_MAX_INDEX_BYTE
#undef FFT_UPPER_BIN_MAX_INDEX_BYTE
#undef MIN_CHIRPING_SLOPE
}

/*
 * Merlin (and Osprey, etc) chirp radar chirp detection.
 */
static int
dfs_check_chirping_merlin(struct ath_dfs *dfs, void *buf, u_int16_t datalen,
    int is_ctl, int is_ext, int *slope, int *is_dc)
{
#define ABS_DIFF(_x, _y) ((int)_x > (int)_y ? (int)_x - (int)_y : (int)_y - (int)_x)
#define ABS(_x) ((int)_x > 0 ? (int)_x : - (int)_x)

#define DELTA_STEP              1   /* This should be between 1 and 3. Default is 1.  */
#define NUM_DIFFS               3   /* Number of Diffs to compute. valid range is 2-4 */
#define MAX_DIFF                2   /* Threshold for difference of delta peaks        */
#define BIN_COUNT_MAX           6   /* Max. number of strong bins for narrow band     */


/*
 * Dynamic 20/40 mode FFT packet format related definition
 */

#define NUM_FFT_BYTES_HT40      70
#define NUM_BIN_BYTES_HT40      64
#define NUM_SUBCHAN_BINS_HT40   64
#define LOWER_INDEX_BYTE_HT40   66
#define UPPER_INDEX_BYTE_HT40   69
#define LOWER_WEIGHT_BYTE_HT40  64
#define UPPER_WEIGHT_BYTE_HT40  67
#define LOWER_MAG_BYTE_HT40     65
#define UPPER_MAG_BYTE_HT40     68

/*
 * Static 20 mode FFT packet format related definition
 */

#define NUM_FFT_BYTES_HT20      31
#define NUM_BIN_BYTES_HT20      28
#define NUM_SUBCHAN_BINS_HT20   56
#define LOWER_INDEX_BYTE_HT20   30
#define UPPER_INDEX_BYTE_HT20   30
#define LOWER_WEIGHT_BYTE_HT20  28
#define UPPER_WEIGHT_BYTE_HT20  28
#define LOWER_MAG_BYTE_HT20     29
#define UPPER_MAG_BYTE_HT20     29

    int num_fft_packets;    /* number of FFT packets reported to software */
    int num_fft_bytes;
    int num_bin_bytes;
    int num_subchan_bins;
    int lower_index_byte;
    int upper_index_byte;
    int lower_weight_byte;
    int upper_weight_byte;
    int lower_mag_byte;
    int upper_mag_byte;

    int max_index_lower [DELTA_STEP + NUM_DIFFS];
    int max_index_upper [DELTA_STEP + NUM_DIFFS];
    int max_mag_lower   [DELTA_STEP + NUM_DIFFS];
    int max_mag_upper   [DELTA_STEP + NUM_DIFFS];
    int bin_wt_lower    [DELTA_STEP + NUM_DIFFS];
    int bin_wt_upper    [DELTA_STEP + NUM_DIFFS];
    int max_mag_sel     [DELTA_STEP + NUM_DIFFS];
    int max_mag         [DELTA_STEP + NUM_DIFFS];
    int max_index       [DELTA_STEP + NUM_DIFFS];



    int max_d[] = {10, 19, 28};
    int min_d[] = {1, 2, 3};

    u_int8_t *ptr;          /* pointer to FFT data */
    int i;
    int fft_start;
    int chirp_found;
    int delta_peak[NUM_DIFFS];
    int j;
    int bin_count;
    int bw_mask;
    int delta_diff;
    int same_sign;
    int temp;

    if (IS_CHAN_HT40(dfs->ic->ic_curchan)) {
        num_fft_bytes     = NUM_FFT_BYTES_HT40;
        num_bin_bytes     = NUM_BIN_BYTES_HT40;
        num_subchan_bins  = NUM_SUBCHAN_BINS_HT40;
        lower_index_byte  = LOWER_INDEX_BYTE_HT40;
        upper_index_byte  = UPPER_INDEX_BYTE_HT40;
        lower_weight_byte = LOWER_WEIGHT_BYTE_HT40;
        upper_weight_byte = UPPER_WEIGHT_BYTE_HT40;
        lower_mag_byte    = LOWER_MAG_BYTE_HT40;
        upper_mag_byte    = UPPER_MAG_BYTE_HT40;

        /* if we are in HT40MINUS then swap primary and extension */
        if (IS_CHAN_HT40_MINUS(dfs->ic->ic_curchan)) {
            temp   = is_ctl;
            is_ctl = is_ext;
            is_ext = temp;
        }

    } else {
        num_fft_bytes     = NUM_FFT_BYTES_HT20;
        num_bin_bytes     = NUM_BIN_BYTES_HT20;
        num_subchan_bins  = NUM_SUBCHAN_BINS_HT20;
        lower_index_byte  = LOWER_INDEX_BYTE_HT20;
        upper_index_byte  = UPPER_INDEX_BYTE_HT20;
        lower_weight_byte = LOWER_WEIGHT_BYTE_HT20;
        upper_weight_byte = UPPER_WEIGHT_BYTE_HT20;
        lower_mag_byte    = LOWER_MAG_BYTE_HT20;
        upper_mag_byte    = UPPER_MAG_BYTE_HT20;
    }

    ptr     = (u_int8_t*)buf;
    /*
     * sanity check for FFT buffer
     */

    if ((ptr == NULL) || (datalen == 0)) {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT, "%s: FFT buffer pointer is null or size is 0\n", __func__);
        return 0;
    }

    num_fft_packets = (datalen - 3) / num_fft_bytes;
    if (num_fft_packets < (NUM_DIFFS + DELTA_STEP)) {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT, "datalen = %d, num_fft_packets = %d, too few packets... (exiting)\n", datalen, num_fft_packets);
        return 0;
    }

    if ((((datalen - 3) % num_fft_bytes) == 2) && (datalen > num_fft_bytes))   {
        // FIXME !!!
        ptr += 2;
        datalen -= 2;
    }

    for (i = 0; i < (NUM_DIFFS + DELTA_STEP); i++) {
        fft_start = i * num_fft_bytes;
        bin_wt_lower[i] = ptr[fft_start + lower_weight_byte] & 0x3f;
        bin_wt_upper[i] = ptr[fft_start + upper_weight_byte] & 0x3f;

        max_index_lower[i] = ptr[fft_start + lower_index_byte] >> 2;
        max_index_upper[i] = (ptr[fft_start + upper_index_byte] >> 2) + num_subchan_bins;

        if (!IS_CHAN_HT40(dfs->ic->ic_curchan)) {
            /*
             * for HT20 mode indices are 6 bit signed number
             */
            max_index_lower[i] ^= 0x20;
            max_index_upper[i] = 0;
        }
        /*
         * Reconstruct the maximum magnitude for each sub-channel. Also select
         * and flag the max overall magnitude between the two sub-channels.
         */

        max_mag_lower[i] = ((ptr[fft_start + lower_index_byte] & 0x03) << 8) +
                           ptr[fft_start + lower_mag_byte];
        max_mag_upper[i] = ((ptr[fft_start + upper_index_byte] & 0x03) << 8) +
                            ptr[fft_start + upper_mag_byte];
        bw_mask = ((bin_wt_lower[i] == 0) ? 0 : is_ctl) +
                  (((bin_wt_upper[i] == 0) ? 0 : is_ext) << 1);

        /*
         * Limit the max bin based on channel bandwidth
         * If the upper sub-channel max index is stuck at '1', the signal is dominated
         * by residual DC (or carrier leak) and should be ignored.
         */

        if (bw_mask == 1) {
            max_mag_sel[i] = 0;
            max_mag[i] = max_mag_lower[i];
            max_index[i] = max_index_lower[i];
        } else if(bw_mask == 2) {
            max_mag_sel[i] = 1;
            max_mag[i] = max_mag_upper[i];
            max_index[i] = max_index_upper[i];
        } else if(max_index_upper[i] == num_subchan_bins) {
            max_mag_sel[i] = 0;  /* Ignore DC bin. */
            max_mag[i] = max_mag_lower[i];
            max_index[i] = max_index_lower[i];
        } else {
            if (max_mag_upper[i] > max_mag_lower[i]) {
                max_mag_sel[i] = 1;
                max_mag[i] = max_mag_upper[i];
                max_index[i] = max_index_upper[i];
            } else {
                max_mag_sel[i] = 0;
                max_mag[i] = max_mag_lower[i];
                max_index[i] = max_index_lower[i];
            }
        }
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "i=%d, max_index[i]=%d, max_index_lower[i]=%d, "
          "max_index_upper[i]=%d\n",
          i, max_index[i], max_index_lower[i], max_index_upper[i]);
    }



    chirp_found = 1;
    delta_diff  = 0;
    same_sign   = 1;

    /*
        delta_diff computation -- look for movement in peak.
        make sure that the chirp direction (i.e. sign) is always the same,
        i.e. sign of the two peaks should be same.
    */
    for (i = 0; i < NUM_DIFFS; i++) {
        delta_peak[i] = max_index[i + DELTA_STEP] - max_index[i];
        if (i > 0) {
            delta_diff = delta_peak[i] - delta_peak[i-1];
            same_sign  = !((delta_peak[i] & 0x80) ^ (delta_peak[i-1] & 0x80));
        }
        chirp_found &= (ABS(delta_peak[i]) >= min_d[DELTA_STEP - 1]) &&
                       (ABS(delta_peak[i]) <= max_d[DELTA_STEP - 1]) &&
                       same_sign &&
                       (ABS(delta_diff) <= MAX_DIFF);
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "i=%d, delta_peak[i]=%d, delta_diff=%d\n",
          i, delta_peak[i], delta_diff);
    }


    if (chirp_found)
    {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "%s: CHIRPING_BEFORE_STRONGBIN_YES\n", __func__);
    } else {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "%s: CHIRPING_BEFORE_STRONGBIN_NO\n", __func__);
    }

    /*
        Work around for potential hardware data corruption bug. Check for
        wide band signal by counting strong bins indicated by bitmap flags.
        This check is done if chirp_found is true. We do this as a final check
        to weed out corrupt FFTs bytes. This looks expensive but in most cases it
        will exit early.
    */

    for (i = 0; (i < (NUM_DIFFS + DELTA_STEP)) && (chirp_found == 1); i++) {
        bin_count = 0;
        /* point to the start of the 1st byte of the selected sub-channel. */
        fft_start = (i * num_fft_bytes) + (max_mag_sel[i] ? (num_subchan_bins >> 1) : 0);
        for (j = 0; j < (num_subchan_bins >> 1); j++)
        {
            /*
            * If either bin is flagged "strong", accumulate the bin_count.
            * It's not accurate, but good enough...
            */
            bin_count += (ptr[fft_start + j] & 0x88) ? 1 : 0;
        }
        chirp_found &= (bin_count > BIN_COUNT_MAX) ? 0 : 1;
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT,
          "i=%d, computed bin_count=%d\n", i, bin_count);
    }

    if (chirp_found)
    {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT | ATH_DEBUG_DFS_PHYERR_SUM,
          "%s: CHIRPING_YES\n", __func__);
    } else {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5_FFT | ATH_DEBUG_DFS_PHYERR_SUM,
          "%s: CHIRPING_NO\n", __func__);
    }
    return chirp_found;

#undef ABS_DIFF
#undef ABS
#undef DELTA_STEP
#undef NUM_DIFFS
#undef MAX_DIFF
#undef BIN_COUNT_MAX

#undef NUM_FFT_BYTES_HT40
#undef NUM_BIN_BYTES_HT40
#undef NUM_SUBCHAN_BINS_HT40
#undef LOWER_INDEX_BYTE_HT40
#undef UPPER_INDEX_BYTE_HT40
#undef LOWER_WEIGHT_BYTE_HT40
#undef UPPER_WEIGHT_BYTE_HT40
#undef LOWER_MAG_BYTE_HT40
#undef UPPER_MAG_BYTE_HT40

#undef NUM_FFT_BYTES_HT40
#undef NUM_BIN_BYTES_HT40
#undef NUM_SUBCHAN_BINS_HT40
#undef LOWER_INDEX_BYTE_HT40
#undef UPPER_INDEX_BYTE_HT40
#undef LOWER_WEIGHT_BYTE_HT40
#undef UPPER_WEIGHT_BYTE_HT40
#undef LOWER_MAG_BYTE_HT40
#undef UPPER_MAG_BYTE_HT40
}

int
dfs_check_chirping(struct ath_dfs *dfs, void *buf,
    u_int16_t datalen, int is_ctl, int is_ext, int *slope, int *is_dc)
{

   if (dfs->dfs_caps.ath_dfs_use_enhancement)
      return dfs_check_chirping_merlin(dfs, buf, datalen, is_ctl,
          is_ext, slope, is_dc);
   else
      return dfs_check_chirping_sowl(dfs, buf, datalen, is_ctl,
          is_ext, slope, is_dc);
}

u_int8_t
dfs_retain_bin5_burst_pattern(struct ath_dfs *dfs, u_int32_t diff_ts,
    u_int8_t old_dur)
{

   /*
    * Pulses may get split into 2 during chirping, this print is only
    * to show that it happened, we do not handle this condition if we
    * cannot detect the chirping.
    */
   /*
    * SPLIT pulses will have a time stamp difference of < 50
    */
   if (diff_ts < 50) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS_BIN5,
          "%s SPLIT pulse diffTs=%u dur=%d (old_dur=%d)\n",
          __func__, diff_ts,
          dfs->dfs_rinfo.dfs_last_bin5_dur, old_dur);
   }
   /*
    * Check if this is the 2nd or 3rd pulse in the same burst,
    * PRI will be between 1000 and 2000 us
    */
   if (((diff_ts >= DFS_BIN5_PRI_LOWER_LIMIT) &&
       (diff_ts <= DFS_BIN5_PRI_HIGHER_LIMIT))) {
      /*
       * This pulse belongs to the same burst as the pulse before,
       * so return the same random duration for it
       */
      DFS_DPRINTK(dfs,ATH_DEBUG_DFS_BIN5,
          "%s this pulse belongs to the same burst as before, give "
          "it same dur=%d (old_dur=%d)\n",
          __func__, dfs->dfs_rinfo.dfs_last_bin5_dur, old_dur);

      return (dfs->dfs_rinfo.dfs_last_bin5_dur);
   }
   /*
    * This pulse does not belong to this burst, return unchanged
    * duration.
    */
   return old_dur;
}

/*
 * Chirping pulses may get cut off at DC and report lower durations.
 *
 * This function will compute a suitable random duration for each pulse.
 * Duration must be between 50 and 100 us, but remember that in
 * ath_process_phyerr() which calls this function, we are dealing with the
 * HW reported duration (unconverted). dfs_process_radarevent() will
 * actually convert the duration into the correct value.
 *
 * XXX This function doesn't take into account whether the hardware
 * is operating in 5GHz fast clock mode or not.
 *
 * XXX And this function doesn't take into account whether the hardware
 * is peregrine or not.  Grr.
 */
int
dfs_get_random_bin5_dur(struct ath_dfs *dfs, u_int64_t tstamp)
{
    u_int8_t new_dur=MIN_BIN5_DUR;
    int range;

    get_random_bytes(&new_dur, sizeof(u_int8_t));

    range = (MAX_BIN5_DUR - MIN_BIN5_DUR + 1);

    new_dur %= range;

    new_dur += MIN_BIN5_DUR;

    return new_dur;
}

#endif /* ATH_SUPPORT_DFS */
