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

                                 dfs_init.c

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

extern int domainoverride;

/*
 * Clear all delay lines for all filter types
 *
 * This may be called before any radar pulses are configured
 * (eg on a non-DFS channel, with radar PHY errors still showing up.)
 * In that case, just drop out early.
 */
void dfs_reset_alldelaylines(struct ath_dfs *dfs)
{
        struct dfs_filtertype *ft = NULL;
        struct dfs_filter *rf;
        struct dfs_delayline *dl;
        struct dfs_pulseline *pl;
        int i,j;

        if (dfs == NULL) {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                                "%s[%d]: sc_dfs is NULL", __func__, __LINE__);
            return;
        }
        pl = dfs->pulses;

        if (pl == NULL) {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                            "%s[%d]:  pl==NULL, dfs=%p", __func__, __LINE__, dfs);
            return;
        }

        if (dfs->dfs_b5radars == NULL) {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
            "%s[%d]: pl==NULL, b5radars=%p", __func__, __LINE__, dfs->dfs_b5radars);
            return;
        }

         /* reset the pulse log */
        pl->pl_firstelem = pl->pl_numelems = 0;
        pl->pl_lastelem = DFS_MAX_PULSE_BUFFER_MASK;

        for (i=0; i<DFS_MAX_RADAR_TYPES; i++) {
            if (dfs->dfs_radarf[i] != NULL) {
                ft = dfs->dfs_radarf[i];
                if (NULL != ft) {
                    for (j = 0; j < ft->ft_numfilters; j++) {
                        rf = &(ft->ft_filters[j]);
                        dl = &(rf->rf_dl);
                        if (dl != NULL) {
                            OS_MEMZERO(dl, sizeof(struct dfs_delayline));
                            dl->dl_lastelem = (0xFFFFFFFF) & DFS_MAX_DL_MASK;
                        }
                    }
                }
            }
        }
        for (i = 0; i < dfs->dfs_rinfo.rn_numbin5radars; i++) {
            OS_MEMZERO(&(dfs->dfs_b5radars[i].br_elems[0]), sizeof(struct dfs_bin5elem)*DFS_MAX_B5_SIZE);
            dfs->dfs_b5radars[i].br_firstelem = 0;
            dfs->dfs_b5radars[i].br_numelems = 0;
            dfs->dfs_b5radars[i].br_lastelem = (0xFFFFFFFF)&DFS_MAX_B5_MASK;
        }
}
/*
 * Clear only a single delay line
 */

void dfs_reset_delayline(struct dfs_delayline *dl)
{
   OS_MEMZERO(&(dl->dl_elems[0]), sizeof(dl->dl_elems));
   dl->dl_lastelem = (0xFFFFFFFF)&DFS_MAX_DL_MASK;
}

void dfs_reset_filter_delaylines(struct dfs_filtertype *dft)
{
        int i;
        struct dfs_filter *df;
        for (i=0; i< DFS_MAX_NUM_RADAR_FILTERS; i++) {
                df = &dft->ft_filters[i];
                dfs_reset_delayline(&(df->rf_dl));
        }
}

void
dfs_reset_radarq(struct ath_dfs *dfs)
{
   struct dfs_event *event;
   if (dfs == NULL) {
      DFS_DPRINTK(dfs, ATH_DEBUG_DFS, "%s: sc_dfs is NULL", __func__);
      return;
   }
   ATH_DFSQ_LOCK(dfs);
   ATH_DFSEVENTQ_LOCK(dfs);
   while (!STAILQ_EMPTY(&(dfs->dfs_radarq))) {
      event = STAILQ_FIRST(&(dfs->dfs_radarq));
      STAILQ_REMOVE_HEAD(&(dfs->dfs_radarq), re_list);
      OS_MEMZERO(event, sizeof(struct dfs_event));
      STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), event, re_list);
   }
   ATH_DFSEVENTQ_UNLOCK(dfs);
   ATH_DFSQ_UNLOCK(dfs);
}


/* This function Initialize the radar filter tables
 * if the ath dfs domain is uninitalized or
 *    ath dfs domain is different from hal dfs domain
 */
int dfs_init_radar_filters(struct ieee80211com *ic,
  struct ath_dfs_radar_tab_info *radar_info)
{
    u_int32_t T, Tmax;
    int numpulses,p,n, i;
    int numradars = 0, numb5radars = 0;
    struct ath_dfs *dfs = (struct ath_dfs *)ic->ic_dfs;
    struct dfs_filtertype *ft = NULL;
    struct dfs_filter *rf=NULL;
    struct dfs_pulse *dfs_radars;
    struct dfs_bin5pulse *b5pulses=NULL;
    int32_t min_rssithresh=DFS_MAX_RSSI_VALUE;
    u_int32_t max_pulsedur=0;

    if (dfs == NULL) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        "%s[%d]: dfs is NULL", __func__, __LINE__);
        return DFS_STATUS_FAIL;
    }
    /* clear up the dfs domain flag first */
#ifndef ATH_DFS_RADAR_DETECTION_ONLY
    dfs->ath_dfs_isdfsregdomain = 0;
#endif

    /*
     * If radar_info is NULL or dfsdomain is NULL, treat
     * the rest of the radar configuration as suspect.
     */
    if (radar_info == NULL || radar_info->dfsdomain == 0) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                        "%s[%d]: Unknown dfs domain %d ",
                        __func__, __LINE__, dfs->dfsdomain);
        /* Disable radar detection since we don't have a radar domain */
        dfs->dfs_proc_phyerr &= ~DFS_RADAR_EN;
        /* Returning success though we are not completing init. A failure
         * will fail dfs_attach also.
         */
        return DFS_STATUS_SUCCESS;
    }

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                    "%s[%d]:dfsdomain=%d, numradars=%d, numb5radars=%d",
                    __func__, __LINE__, radar_info->dfsdomain,
                    radar_info->numradars, radar_info->numb5radars);
    dfs->dfsdomain = radar_info->dfsdomain;
    dfs_radars = radar_info->dfs_radars;
    numradars = radar_info->numradars;
    b5pulses = radar_info->b5pulses;
    numb5radars = radar_info->numb5radars;

    /* XXX this should be an explicit copy of some sort! */
    dfs->dfs_defaultparams = radar_info->dfs_defaultparams;

#ifndef ATH_DFS_RADAR_DETECTION_ONLY
    dfs->ath_dfs_isdfsregdomain = 1;
#endif

    dfs->dfs_rinfo.rn_numradars = 0;
    /* Clear filter type table */
    for (n = 0; n < 256; n++) {
        for (i=0;i<DFS_MAX_RADAR_OVERLAP; i++)
            (dfs->dfs_radartable[n])[i] = -1;
    }
    /* Now, initialize the radar filters */
    for (p=0; p<numradars; p++) {
    ft = NULL;
    for (n=0; n<dfs->dfs_rinfo.rn_numradars; n++) {
        if ((dfs_radars[p].rp_pulsedur == dfs->dfs_radarf[n]->ft_filterdur) &&
           (dfs_radars[p].rp_numpulses == dfs->dfs_radarf[n]->ft_numpulses) &&
           (dfs_radars[p].rp_mindur == dfs->dfs_radarf[n]->ft_mindur) &&
           (dfs_radars[p].rp_maxdur == dfs->dfs_radarf[n]->ft_maxdur)) {
           ft = dfs->dfs_radarf[n];
           break;
        }
    }
    if (ft == NULL) {
        /* No filter of the appropriate dur was found */
        if ((dfs->dfs_rinfo.rn_numradars+1) >DFS_MAX_RADAR_TYPES) {
         DFS_DPRINTK(dfs, ATH_DEBUG_DFS, "%s: Too many filter types",
         __func__);
         goto bad4;
        }
        ft = dfs->dfs_radarf[dfs->dfs_rinfo.rn_numradars];
        ft->ft_numfilters = 0;
        ft->ft_numpulses = dfs_radars[p].rp_numpulses;
        ft->ft_patterntype = dfs_radars[p].rp_patterntype;
        ft->ft_mindur = dfs_radars[p].rp_mindur;
        ft->ft_maxdur = dfs_radars[p].rp_maxdur;
        ft->ft_filterdur = dfs_radars[p].rp_pulsedur;
        ft->ft_rssithresh = dfs_radars[p].rp_rssithresh;
        ft->ft_rssimargin = dfs_radars[p].rp_rssimargin;
        ft->ft_minpri = 1000000;

        if (ft->ft_rssithresh < min_rssithresh)
        min_rssithresh = ft->ft_rssithresh;
        if (ft->ft_maxdur > max_pulsedur)
        max_pulsedur = ft->ft_maxdur;
        for (i=ft->ft_mindur; i<=ft->ft_maxdur; i++) {
               u_int32_t stop=0,tableindex=0;
               while ((tableindex < DFS_MAX_RADAR_OVERLAP) && (!stop)) {
                   if ((dfs->dfs_radartable[i])[tableindex] == -1)
                  stop = 1;
                   else
                  tableindex++;
               }
               if (stop) {
                   (dfs->dfs_radartable[i])[tableindex] =
                  (int8_t) (dfs->dfs_rinfo.rn_numradars);
               } else {
                   DFS_DPRINTK(dfs, ATH_DEBUG_DFS,
                  "%s: Too many overlapping radar filters",
                  __func__);
                   goto bad4;
               }
        }
        dfs->dfs_rinfo.rn_numradars++;
    }
    rf = &(ft->ft_filters[ft->ft_numfilters++]);
    dfs_reset_delayline(&rf->rf_dl);
    numpulses = dfs_radars[p].rp_numpulses;

    rf->rf_numpulses = numpulses;
    rf->rf_patterntype = dfs_radars[p].rp_patterntype;
    rf->rf_pulseid = dfs_radars[p].rp_pulseid;
    rf->rf_mindur = dfs_radars[p].rp_mindur;
    rf->rf_maxdur = dfs_radars[p].rp_maxdur;
    rf->rf_numpulses = dfs_radars[p].rp_numpulses;
    rf->rf_ignore_pri_window = dfs_radars[p].rp_ignore_pri_window;
    T = (100000000/dfs_radars[p].rp_max_pulsefreq) -
         100*(dfs_radars[p].rp_meanoffset);
            rf->rf_minpri =
            dfs_round((int32_t)T - (100*(dfs_radars[p].rp_pulsevar)));
    Tmax = (100000000/dfs_radars[p].rp_pulsefreq) -
            100*(dfs_radars[p].rp_meanoffset);
      rf->rf_maxpri =
         dfs_round((int32_t)Tmax + (100*(dfs_radars[p].rp_pulsevar)));

        if( rf->rf_minpri < ft->ft_minpri )
        ft->ft_minpri = rf->rf_minpri;

    rf->rf_fixed_pri_radar_pulse = ( dfs_radars[p].rp_max_pulsefreq == dfs_radars[p].rp_pulsefreq ) ? 1 : 0;
    rf->rf_threshold = dfs_radars[p].rp_threshold;
    rf->rf_filterlen = rf->rf_maxpri * rf->rf_numpulses;

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]: minprf = %d maxprf = %d pulsevar = %d thresh=%d",
            __func__,__LINE__,dfs_radars[p].rp_pulsefreq, dfs_radars[p].rp_max_pulsefreq,
            dfs_radars[p].rp_pulsevar, rf->rf_threshold);
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO, "%s[%d]:minpri = %d maxpri = %d filterlen = %d filterID = %d",__func__,__LINE__,
            rf->rf_minpri, rf->rf_maxpri, rf->rf_filterlen, rf->rf_pulseid);

    }

#ifdef DFS_DEBUG
    dfs_print_filters(ic);
#endif
    dfs->dfs_rinfo.rn_numbin5radars  = numb5radars;
    if (dfs->dfs_b5radars != NULL)
        OS_FREE(dfs->dfs_b5radars);

    dfs->dfs_b5radars = (struct dfs_bin5radars *)OS_MALLOC(NULL,
      numb5radars * sizeof(struct dfs_bin5radars), GFP_KERNEL);
    if (dfs->dfs_b5radars == NULL) {
        DFS_DPRINTK(dfs, ATH_DEBUG_DFS,
        "%s: cannot allocate memory for bin5 radars",
        __func__);
        goto bad4;
    }
    for (n=0; n<numb5radars; n++) {
        dfs->dfs_b5radars[n].br_pulse = b5pulses[n];
        dfs->dfs_b5radars[n].br_pulse.b5_timewindow *= 1000000;
        if (dfs->dfs_b5radars[n].br_pulse.b5_rssithresh < min_rssithresh)
            min_rssithresh = dfs->dfs_b5radars[n].br_pulse.b5_rssithresh;
        if (dfs->dfs_b5radars[n].br_pulse.b5_maxdur > max_pulsedur)
            max_pulsedur = dfs->dfs_b5radars[n].br_pulse.b5_maxdur;
    }
    dfs_reset_alldelaylines(dfs);
    dfs_reset_radarq(dfs);
    dfs->dfs_curchan_radindex = -1;
    dfs->dfs_extchan_radindex = -1;
    dfs->dfs_rinfo.rn_minrssithresh = min_rssithresh;
    /* Convert durations to TSF ticks */
    dfs->dfs_rinfo.rn_maxpulsedur = dfs_round((int32_t)((max_pulsedur*100/80)*100));
    /* relax the max pulse duration a little bit due to inaccuracy caused by chirping. */
    dfs->dfs_rinfo.rn_maxpulsedur = dfs->dfs_rinfo.rn_maxpulsedur +20;
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                    "%s[%d]: DFS min filter rssiThresh = %d",
                    __func__, __LINE__, min_rssithresh);
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
                    "%s[%d]:DFS max pulse dur = %d ticks",
                    __func__ ,__LINE__, dfs->dfs_rinfo.rn_maxpulsedur);
    return DFS_STATUS_SUCCESS;

 bad4:
     return DFS_STATUS_FAIL;
}

void
dfs_clear_stats(struct ieee80211com *ic)
{
    struct ath_dfs *dfs = (struct ath_dfs *)ic->ic_dfs;
    if (dfs == NULL)
            return;
    OS_MEMZERO(&dfs->ath_dfs_stats, sizeof (struct dfs_stats));
    dfs->ath_dfs_stats.last_reset_tstamp = ic->ic_get_TSF64(ic);
}

#endif /* ATH_SUPPORT_DFS */
