/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

                     ath_dfs_structs.h

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



#ifndef _DFS__STRUCTS_H_
#define _DFS__STRUCTS_H_
#include <adf_os_mem.h>

#ifdef ANDROID
#include <linux/string.h>
#endif

/*
 * For the dfs_nol_clist_update() method - this is the
 * update command.
 */
enum {
       DFS_NOL_CLIST_CMD_NONE      = 0x0,
       DFS_NOL_CLIST_CMD_UPDATE    = 0x1,
};

struct ath_dfs_caps {
    u_int32_t
        ath_dfs_ext_chan_ok:1, /* Can radar be detected on the extension chan? */
        ath_dfs_combined_rssi_ok:1,  /* Can use combined radar RSSI? */
        /* the following flag is used to indicate if radar detection scheme */
        /* should use enhanced chirping detection algorithm. This flag also */
        /* determines if certain radar data should be discarded to minimize */
        /* false detection of radar.                                        */
        ath_dfs_use_enhancement:1,
        ath_strong_signal_diversiry:1,
        ath_chip_is_bb_tlv:1;

         /*
          * goes with ath_strong_signal_diversiry:
       * If we have fast diversity capability, read off
       * Strong Signal fast diversity count set in the ini
       * file, and store so we can restore the value when
       * radar is disabled
       */
    u_int32_t ath_fastdiv_val;
};

/*
 * These are defined in the HAL for now, and must be migrated outside
 * of there in order to be used by the new partial offload data path.
 */

struct  dfs_pulse {
    u_int32_t    rp_numpulses;     /* Num of pulses in radar burst */
    u_int32_t    rp_pulsedur;      /* Duration of each pulse in usecs */
    u_int32_t    rp_pulsefreq;     /* Frequency of pulses in burst */
    u_int32_t    rp_max_pulsefreq; /* Frequency of pulses in burst */
    u_int32_t    rp_patterntype;   /* fixed or variable pattern type*/
    u_int32_t    rp_pulsevar;      /* Time variation of pulse duration for
                                      matched filter (single-sided) in usecs */
    u_int32_t    rp_threshold;     /* Threshold for MF output to indicateC
                                      radar match */
    u_int32_t    rp_mindur;        /* Min pulse duration to be considered for
                                      this pulse type */
    u_int32_t    rp_maxdur;        /* Max pusle duration to be considered for
                                      this pulse type */
    u_int32_t    rp_rssithresh;    /* Minimum rssi to be considered a radar pulse */
    u_int32_t    rp_meanoffset;    /* Offset for timing adjustment */
    int32_t      rp_rssimargin;    /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
                                   /* lower than in non TURBO mode. This will be used to offset that diff.*/
    u_int32_t    rp_ignore_pri_window;
    u_int32_t    rp_pulseid;       /* Unique ID for identifying filter */
};

struct  dfs_staggered_pulse {
    u_int32_t    rp_numpulses;        /* Num of pulses in radar burst */
    u_int32_t    rp_pulsedur;         /* Duration of each pulse in usecs */
    u_int32_t    rp_min_pulsefreq;    /* Frequency of pulses in burst */
    u_int32_t    rp_max_pulsefreq;    /* Frequency of pulses in burst */
    u_int32_t    rp_patterntype;      /* fixed or variable pattern type*/
    u_int32_t    rp_pulsevar;         /* Time variation of pulse duration for
                                         matched filter (single-sided) in usecs */
    u_int32_t    rp_threshold;        /* Thershold for MF output to indicateC
                                         radar match */
    u_int32_t    rp_mindur;           /* Min pulse duration to be considered for
                                         this pulse type */
    u_int32_t    rp_maxdur;           /* Max pusle duration to be considered for
                                         this pulse type */
    u_int32_t    rp_rssithresh;       /* Minimum rssi to be considered a radar pulse */
    u_int32_t    rp_meanoffset;       /* Offset for timing adjustment */
    int32_t      rp_rssimargin;       /* rssi threshold margin. In Turbo Mode HW reports rssi 3dBm */
                                      /* lower than in non TURBO mode. This will be used to offset that diff.*/
    u_int32_t    rp_pulseid;          /* Unique ID for identifying filter */
};

struct dfs_bin5pulse {
        u_int32_t       b5_threshold;          /* Number of bin5 pulses to indicate detection */
        u_int32_t       b5_mindur;             /* Min duration for a bin5 pulse */
        u_int32_t       b5_maxdur;             /* Max duration for a bin5 pulse */
        u_int32_t       b5_timewindow;         /* Window over which to count bin5 pulses */
        u_int32_t       b5_rssithresh;         /* Min rssi to be considered a pulse */
        u_int32_t       b5_rssimargin;         /* rssi threshold margin. In Turbo Mode HW reports rssi 3dB */
};

/*
 * DFS NOL representation.
 *
 * This is used to represent the DFS NOL information between the
 * NOL code in lmac/dfs/dfs_nol.c and any driver layer wishing
 * to use it.
 */
struct dfs_nol_chan_entry {
   u_int32_t   nol_chfreq; /* Centre frequency, MHz */
   u_int32_t   nol_chwidth;   /* Width, MHz */
   unsigned long  nol_start_ticks;  /* start ticks, OS specific */
   u_int32_t   nol_timeout_ms;   /* timeout, mS */
};

//HAL_PHYERR_PARAM;

/*
 * This represents the general case of the radar PHY configuration,
 * across all chips.
 *
 * It's then up to each chip layer to translate to/from this
 * (eg to HAL_PHYERR_PARAM for the HAL case.)
 */

#define  ATH_DFS_PHYERR_PARAM_NOVAL 0xFFFF
#define  ATH_DFS_PHYERR_PARAM_ENABLE   0x8000

struct ath_dfs_phyerr_param {
   int32_t         pe_firpwr;      /* FIR pwr out threshold */
   int32_t         pe_rrssi;       /* Radar rssi thresh */
   int32_t         pe_height;      /* Pulse height thresh */
   int32_t         pe_prssi;       /* Pulse rssi thresh */
   int32_t         pe_inband;      /* Inband thresh */

   /* The following params are only for AR5413 and later */
   /*
    * Relative power threshold in 0.5dB steps
    */
   u_int32_t       pe_relpwr;

   /*
    * Pulse Relative step threshold in 0.5dB steps
    */
   u_int32_t       pe_relstep;

   /*
    * Max length of radar sign in 0.8us units
    */
   u_int32_t       pe_maxlen;

   /*
    * Use the average in-band power measured over 128 cycles
    */
   bool        pe_usefir128;

   /*
    * Enable to block radar check if pkt detect is done via OFDM
    * weak signal detect or pkt is detected immediately after tx
    * to rx transition
    */
   bool        pe_blockradar;

   /*
    * Enable to use the max rssi instead of the last rssi during
    * fine gain changes for radar detection
    */
   bool        pe_enmaxrssi;
};

static inline void ath_dfs_phyerr_param_copy(struct ath_dfs_phyerr_param *dst,
    struct ath_dfs_phyerr_param *src)
{
   adf_os_mem_copy(dst, src, sizeof(*dst));
}

static inline void ath_dfs_phyerr_init_noval(struct ath_dfs_phyerr_param *pe)
{
   pe->pe_firpwr = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_rrssi = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_height = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_prssi = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_inband = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_relpwr = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_relstep = ATH_DFS_PHYERR_PARAM_NOVAL;
   pe->pe_maxlen = ATH_DFS_PHYERR_PARAM_NOVAL;

   /* XXX what about usefir128, blockradar, enmaxrssi? */
}

struct ath_dfs_radar_tab_info {
    u_int32_t dfsdomain;
    int numradars;
    struct dfs_pulse *dfs_radars;
    int numb5radars;
    struct dfs_bin5pulse *b5pulses;
    struct ath_dfs_phyerr_param dfs_defaultparams;
    int dfs_pri_multiplier;
};
#endif  /* _DFS__STRUCTS_H_ */
