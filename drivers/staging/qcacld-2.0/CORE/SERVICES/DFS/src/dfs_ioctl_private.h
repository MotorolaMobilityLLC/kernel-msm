/*
 * Copyright (c) 2010-2013 The Linux Foundation. All rights reserved.
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

                        dfs_ioctl_private.h

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

/*
 * ioctl defines
 */

#ifndef _DFS_IOCTL_PRIVATE_H_
#define _DFS_IOCTL_PRIVATE_H_

/*
 * Assert that the NOVAL values match.
 */
#if (ATH_DFS_PHYERR_PARAM_NOVAL != DFS_IOCTL_PARAM_NOVAL)
#error "ATH_DFS_PHYERR_PARAM_NOVAL != DFS_IOCTL_PARAM_NOVAL"
#endif

/*
 * Assert that the ENABLE values match.
 */
#if (ATH_DFS_PHYERR_PARAM_ENABLE != DFS_IOCTL_PARAM_ENABLE)
#error "ATH_DFS_PHYERR_PARAM_ENABLE != DFS_IOCTL_PARAM_ENABLE"
#endif

/*
 * These two methods are used by the lmac glue to copy between
 * the DFS and HAL PHY configuration.
 *
 * I'm "cheating" here and assuming that the ENABLE and NOVAL
 * values match - see the above macros.
 */
static inline void
ath_dfs_ioctlparam_to_dfsparam(const struct dfs_ioctl_params *src,
    struct ath_dfs_phyerr_param *dst)
{

    dst->pe_firpwr = src->dfs_firpwr;
    dst->pe_rrssi = src->dfs_rrssi;
    dst->pe_height = src->dfs_height;
    dst->pe_prssi = src->dfs_prssi;
    dst->pe_inband = src->dfs_inband;
    dst->pe_relpwr = src->dfs_relpwr;
    dst->pe_relstep = src->dfs_relstep;
    dst->pe_maxlen = src->dfs_maxlen;
}

static inline void
ath_dfs_dfsparam_to_ioctlparam(struct ath_dfs_phyerr_param *src,
    struct dfs_ioctl_params *dst)
{

    dst->dfs_firpwr = src->pe_firpwr;
    dst->dfs_rrssi = src->pe_rrssi;
    dst->dfs_height = src->pe_height;
    dst->dfs_prssi = src->pe_prssi;
    dst->dfs_inband = src->pe_inband;
    dst->dfs_relpwr = src->pe_relpwr;
    dst->dfs_relstep = src->pe_relstep;
    dst->dfs_maxlen = src->pe_maxlen;
}

#endif  /* _DFS_IOCTL_PRIVATE_H_ */
