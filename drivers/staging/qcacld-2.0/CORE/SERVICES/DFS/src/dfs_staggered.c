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

                     dfs_staggered.c

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

static int is_pri_multiple(u_int32_t sample_pri, u_int32_t refpri)
{
#define MAX_ALLOWED_MISSED 3
    int i;

    if (sample_pri < refpri || (!refpri))
        return 0;

    for (i=1; i<= MAX_ALLOWED_MISSED; i++) {
        if((sample_pri%(i*refpri) <= 5)) {
            //printk("sample_pri=%d is a multiple of refpri=%d\n", sample_pri, refpri);
            return 1;
        }
    }
    return 0;
#undef MAX_ALLOWED_MISSED
}

static int is_unique_pri(u_int32_t highestpri , u_int32_t midpri,
        u_int32_t lowestpri , u_int32_t refpri )
{
#define DFS_STAGGERED_PRI_MARGIN_MIN  20
#define DFS_STAGGERED_PRI_MARGIN_MAX  400
    if ((DFS_DIFF(lowestpri, refpri) >= DFS_STAGGERED_PRI_MARGIN_MIN) &&
       (DFS_DIFF(midpri, refpri) >= DFS_STAGGERED_PRI_MARGIN_MIN) &&
       (DFS_DIFF(highestpri, refpri) >= DFS_STAGGERED_PRI_MARGIN_MIN)) {
        return 1;
    } else {
        if ((is_pri_multiple(refpri, highestpri)) || (is_pri_multiple(refpri, lowestpri)) ||
           (is_pri_multiple(refpri, midpri)))
        return 0;
    }
    return 0;
#undef DFS_STAGGERED_PRI_MARGIN_MIN
#undef DFS_STAGGERED_PRI_MARGIN_MAX
}


int dfs_staggered_check(struct ath_dfs *dfs, struct dfs_filter *rf,
                             u_int32_t deltaT, u_int32_t width)
{
        u_int32_t refpri, refdur, searchpri=0, deltapri;//, averagerefpri;
        u_int32_t n, i, primargin, durmargin;
        int score[DFS_MAX_DL_SIZE], delayindex, dindex, found=0;
        struct dfs_delayline *dl;
        u_int32_t scoreindex, lowpriindex= 0, lowpri = 0xffff;
#if 0
        int numpulses=0;
#endif
        int higherthan, lowerthan, numscores;
   int numpulseshigh=0, numpulsesmid=0, numpulsestemp=0;
        u_int32_t lowestscore=0, lowestscoreindex=0, lowestpri=0;
        u_int32_t midscore=0, midscoreindex=0, midpri=0;
        u_int32_t highestscore=0, highestscoreindex=0, highestpri=0;

        dl = &rf->rf_dl;
        if( dl->dl_numelems < (rf->rf_threshold-1)) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
           "numelems %d < threshold for filter %d\n",
      dl->dl_numelems, rf->rf_pulseid);
            return 0;
        }
        if( deltaT > rf->rf_filterlen) {
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
           "numelems %d < threshold for filter %d\n",
      dl->dl_numelems, rf->rf_pulseid);
            return 0;
        }
        primargin = 6;
        if(rf->rf_maxdur < 10) {
                durmargin = 4;
        }
        else {
                durmargin = 6;
        }

        OS_MEMZERO(score, sizeof(int)*DFS_MAX_DL_SIZE);
        /* find out the lowest pri */
        for (n=0;n<dl->dl_numelems; n++) {
                delayindex = (dl->dl_firstelem + n) & DFS_MAX_DL_MASK;
                refpri = dl->dl_elems[delayindex].de_time;
                if( refpri == 0)
                        continue;
                else if(refpri < lowpri) {
                        lowpri = dl->dl_elems[delayindex].de_time;
                        lowpriindex = n;
                }
 }
        /* find out the each delay element's pri score */
        for (n=0;n<dl->dl_numelems; n++) {
                delayindex = (dl->dl_firstelem + n) & DFS_MAX_DL_MASK;
                refpri = dl->dl_elems[delayindex].de_time;
                if( refpri == 0) {
                    continue;
                }

                if ( (refpri > rf->rf_maxpri) || (refpri < rf->rf_minpri) ) {
                    score[n] = 0;
                    continue;
                }

                for (i=0;i<dl->dl_numelems; i++) {
                        dindex = (dl->dl_firstelem + i) & DFS_MAX_DL_MASK;
                        searchpri = dl->dl_elems[dindex].de_time;
                        deltapri = DFS_DIFF(searchpri, refpri);
                        if( deltapri < primargin)
                                score[n]++;
                }
        }
        for (n=0;n<dl->dl_numelems; n++) {
            delayindex = (dl->dl_firstelem + n) & DFS_MAX_DL_MASK;
            refdur = dl->dl_elems[delayindex].de_time;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "score[%d]=%d pri=%d\n", n, score[n], refdur);
        }

        /* find out the 2 or 3 highest scorers */
        scoreindex = 0;
        highestscore=0;
        highestscoreindex=0;
        highestpri=0; numscores=0; lowestscore=0;

        for (n=0;n<dl->dl_numelems; n++) {
                higherthan=0;
                lowerthan=0;
                delayindex = (dl->dl_firstelem + n) & DFS_MAX_DL_MASK;
                refpri = dl->dl_elems[delayindex].de_time;

                if ((score[n] >= highestscore) &&
                        (is_unique_pri(highestpri, midpri, lowestpri, refpri))) {
                            lowestscore = midscore;
                            lowestpri = midpri;
                            lowestscoreindex = midscoreindex;
                            midscore = highestscore;
                            midpri = highestpri;
                            midscoreindex = highestscoreindex;
                            highestscore = score[n];
                            highestpri = refpri;
                            highestscoreindex = n;
                    } else {
                        if ((score[n] >= midscore) &&
                            (is_unique_pri(highestpri, midpri, lowestpri, refpri))) {
                            lowestscore = midscore;
                            lowestpri = midpri;
                            lowestscoreindex = midscoreindex;
                            midscore = score[n];
                            midpri = refpri;
                            midscoreindex = n;
                        } else if ((score[n] >= lowestscore) &&
                            (is_unique_pri(highestpri, midpri, lowestpri, refpri))) {
                            lowestscore = score[n];
                            lowestpri = refpri;
                            lowestscoreindex = n;
                        }
                    }

                }

       if (midscore == 0) {
               // This means we have only 1 pulse type. It can not be staggered!
                    return 0;
       }

            DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
           "FINAL highestscore=%d highestscoreindex=%d highestpri=%d\n",
      highestscore, highestscoreindex, highestpri);
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
           "FINAL lowestscore=%d lowestscoreindex=%d lowpri=%d\n",
      lowestscore, lowestscoreindex, lowestpri);
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
           "FINAL midscore=%d midscoreindex=%d midpri=%d\n",
      midscore, midscoreindex, midpri);


            delayindex = (dl->dl_firstelem + highestscoreindex) & DFS_MAX_DL_MASK;
            refdur = dl->dl_elems[delayindex].de_dur;
            refpri = dl->dl_elems[delayindex].de_time;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
           "highscoreindex=%d refdur=%d refpri=%d\n",
      highestscoreindex, refdur, refpri);

            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, highestscore, refpri, refdur,0, highestpri);
       numpulseshigh = numpulsestemp;
            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, highestscore, refpri, refdur,0, highestpri + midpri);
       if (numpulsestemp > numpulseshigh) {
          numpulseshigh = numpulsestemp;
       }
            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, highestscore, refpri, refdur,0, highestpri + midpri + lowestpri);
       if (numpulsestemp > numpulseshigh) {
          numpulseshigh = numpulsestemp;
       }


            delayindex = (dl->dl_firstelem + midscoreindex) & DFS_MAX_DL_MASK;
            refdur = dl->dl_elems[delayindex].de_dur;
            refpri = dl->dl_elems[delayindex].de_time;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS1,
           "midscoreindex=%d refdur=%d refpri=%d\n",
      midscoreindex, refdur, refpri);

            //numpulsesmid = dfs_bin_pri_check(dfs, rf, dl, midscore, refpri, refdur,0, 1);
            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, midscore, refpri, refdur,0, midpri);
       numpulsesmid = numpulsestemp;
            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, midscore, refpri, refdur,0, highestpri + midpri);
       if (numpulsestemp > numpulsesmid) {
          numpulsesmid = numpulsestemp;
       }
            numpulsestemp = dfs_bin_pri_check(dfs, rf, dl, midscore, refpri, refdur,0, highestpri + midpri + lowestpri);
       if (numpulsestemp > numpulsesmid) {
          numpulsesmid = numpulsestemp;
       }


            /*delayindex = (dl->dl_firstelem + lowestscoreindex) & DFS_MAX_DL_MASK;
            refdur = dl->dl_elems[delayindex].de_dur;
            refpri = dl->dl_elems[delayindex].de_time;
            DFS_DPRINTK(ic, ATH_DEBUG_DFS1, "lowestscoreindex=%d refdur=%d refpri=%d\n", lowestscoreindex, refdur, refpri);

            numpulseslow = dfs_bin_pri_check(dfs, rf, dl, lowestscore, refpri, refdur,0, 1);
       */
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2,
           "numpulseshigh=%d, numpulsesmid=%d\n",
      numpulseshigh, numpulsesmid);
//            printf("numpulseshigh=%d, numpulsesmid=%d, numpulseslow %d\n",numpulseshigh, numpulsesmid, numpulseslow);

   if ( (numpulseshigh >= rf->rf_threshold) && (numpulsesmid >= rf->rf_threshold) ) {
        /*if (numpulses >= rf->rf_threshold) {*/
            found = 1;
            DFS_DPRINTK(dfs, ATH_DEBUG_DFS2, "MATCH filter=%u numpulseshigh=%u numpulsesmid= %u thresh=%u\n", rf->rf_pulseid, numpulseshigh, numpulsesmid, rf->rf_threshold);
        }
        return found;
    }

#endif /* ATH_SUPPORT_DFS */
