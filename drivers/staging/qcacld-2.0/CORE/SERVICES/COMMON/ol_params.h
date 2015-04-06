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


/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_OL_PARAMS_H
#define _DEV_OL_PARAMS_H
#include "ol_txrx_stats.h"
#include "wlan_defs.h" /* for wlan statst definitions */
/*
** Enumeration of PDEV Configuration parameter
*/

typedef enum _ol_ath_param_t {
    OL_ATH_PARAM_TXCHAINMASK           = 0,
    OL_ATH_PARAM_RXCHAINMASK,
    OL_ATH_PARAM_TXCHAINMASKLEGACY,
    OL_ATH_PARAM_RXCHAINMASKLEGACY,
    OL_ATH_PARAM_CHAINMASK_SEL,
    OL_ATH_PARAM_AMPDU,
    OL_ATH_PARAM_AMPDU_LIMIT,
    OL_ATH_PARAM_AMPDU_SUBFRAMES,
    OL_ATH_PARAM_LDPC,
    OL_ATH_PARAM_NON_AGG_SW_RETRY_TH,
    OL_ATH_PARAM_AGG_SW_RETRY_TH,
    OL_ATH_PARAM_STA_KICKOUT_TH,
    OL_ATH_PARAM_WLAN_PROF_ENABLE,
    OL_ATH_PARAM_LTR_ENABLE,
    OL_ATH_PARAM_LTR_AC_LATENCY_BE,
    OL_ATH_PARAM_LTR_AC_LATENCY_BK,
    OL_ATH_PARAM_LTR_AC_LATENCY_VI,
    OL_ATH_PARAM_LTR_AC_LATENCY_VO,
    OL_ATH_PARAM_LTR_AC_LATENCY_TIMEOUT,
    OL_ATH_PARAM_LTR_TX_ACTIVITY_TIMEOUT,
    OL_ATH_PARAM_LTR_SLEEP_OVERRIDE,
    OL_ATH_PARAM_LTR_RX_OVERRIDE,
    OL_ATH_PARAM_L1SS_ENABLE,
    OL_ATH_PARAM_DSLEEP_ENABLE,
    OL_ATH_PARAM_PCIELP_TXBUF_FLUSH,
    OL_ATH_PARAM_PCIELP_TXBUF_WATERMARK,
    OL_ATH_PARAM_PCIELP_TXBUF_TMO_EN,
    OL_ATH_PARAM_PCIELP_TXBUF_TMO_VALUE,
    OL_ATH_PARAM_BCN_BURST,
	OL_ATH_PARAM_ARP_AC_OVERRIDE,
	OL_ATH_PARAM_TXPOWER_LIMIT2G,
    OL_ATH_PARAM_TXPOWER_LIMIT5G,
    OL_ATH_PARAM_TXPOWER_SCALE,
    OL_ATH_PARAM_DCS,
    OL_ATH_PARAM_ANI_ENABLE,
    OL_ATH_PARAM_ANI_POLL_PERIOD,
    OL_ATH_PARAM_ANI_LISTEN_PERIOD,
    OL_ATH_PARAM_ANI_OFDM_LEVEL,
    OL_ATH_PARAM_ANI_CCK_LEVEL,
    OL_ATH_PARAM_PROXYSTA,
    OL_ATH_PARAM_DYN_TX_CHAINMASK,
    OL_ATH_PARAM_VOW_EXT_STATS,
    OL_ATH_PARAM_PWR_GATING_ENABLE,
    OL_ATH_PARAM_CHATTER,
} ol_ath_param_t;

/*
** Enumeration of PDEV Configuration parameter
*/

typedef enum _ol_hal_param_t {
    OL_HAL_CONFIG_DMA_BEACON_RESPONSE_TIME         = 0
} ol_hal_param_t;


/*
** structure to hold all stats information
** for offload device interface
*/
struct ol_stats {
    int txrx_stats_level;
    struct ol_txrx_stats txrx_stats;
    struct wlan_dbg_stats stats;
};
#endif /* _DEV_OL_PARAMS_H  */
