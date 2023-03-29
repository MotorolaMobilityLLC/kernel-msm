/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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

#ifndef __CDS_UTILS_H
#define __CDS_UTILS_H

/**=========================================================================

   \file  cds_utils.h

   \brief Connectivity driver services (CDS) utility APIs

   Various utility functions

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include <qdf_types.h>
#include <qdf_status.h>
#include <qdf_event.h>
#include <qdf_lock.h>
#include "ani_global.h"

/*--------------------------------------------------------------------------
   Preprocessor definitions and constants
   ------------------------------------------------------------------------*/
#define CDS_24_GHZ_BASE_FREQ   (2407)
#define CDS_5_GHZ_BASE_FREQ    (5000)
#define CDS_24_GHZ_CHANNEL_1   (1)
#define CDS_24_GHZ_CHANNEL_14  (14)
#define CDS_24_GHZ_CHANNEL_15  (15)
#define CDS_24_GHZ_CHANNEL_27  (27)
#define CDS_5_GHZ_CHANNEL_165  (165)
#define CDS_5_GHZ_CHANNEL_170  (170)
#define CDS_CHAN_SPACING_5MHZ  (5)
#define CDS_CHAN_SPACING_20MHZ (20)
#define CDS_CHAN_14_FREQ       (2484)
#define CDS_CHAN_15_FREQ       (2512)
#define CDS_CHAN_170_FREQ      (5852)

#define INVALID_SCAN_ID        0xFFFFFFFF

#define CDS_DBS_SCAN_CLIENTS_MAX           (7)
#define CDS_DBS_SCAN_PARAM_PER_CLIENT      (3)

#define cds_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_QDF, params)
#define cds_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_QDF, params)
#define cds_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_QDF, params)
#define cds_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_QDF, params)
#define cds_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_QDF, params)

#define cds_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_QDF, params)
#define cds_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_QDF, params)
#define cds_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_QDF, params)
#define cds_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_QDF, params)
#define cds_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_QDF, params)

#define cds_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_QDF, "enter")
#define cds_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_QDF, "exit")

/**
 * enum cds_band_type - Band type - 2g, 5g or all
 * CDS_BAND_ALL: Both 2G and 5G are valid.
 * CDS_BAND_2GHZ: only 2G is valid.
 * CDS_BAND_5GHZ: only 5G is valid.
 */
enum cds_band_type {
	CDS_BAND_ALL = 0,
	CDS_BAND_2GHZ = 1,
	CDS_BAND_5GHZ = 2
};

/*-------------------------------------------------------------------------
   Function declarations and documenation
   ------------------------------------------------------------------------*/

uint32_t cds_chan_to_freq(uint8_t chan);
uint8_t cds_freq_to_chan(uint32_t freq);
enum cds_band_type cds_chan_to_band(uint32_t chan);

#ifdef WLAN_FEATURE_11W
uint8_t cds_get_mmie_size(void);

/**
 * cds_get_gmac_mmie_size: Gives length of GMAC MMIE size
 *
 * Return: Size of MMIE for GMAC
 */
uint8_t cds_get_gmac_mmie_size(void);

#endif /* WLAN_FEATURE_11W */
static inline void cds_host_diag_log_work(qdf_wake_lock_t *lock, uint32_t msec,
			    uint32_t reason) {
	if (((cds_get_ring_log_level(RING_ID_WAKELOCK) >= WLAN_LOG_LEVEL_ACTIVE)
	     && (WIFI_POWER_EVENT_WAKELOCK_HOLD_RX == reason)) ||
	    (WIFI_POWER_EVENT_WAKELOCK_HOLD_RX != reason)) {
		host_diag_log_wlock(reason, qdf_wake_lock_name(lock),
				    msec, WIFI_POWER_EVENT_WAKELOCK_TAKEN);
	}
}

/**
 * cds_copy_hlp_info() - Copy HLP info
 * @input_dst_mac: input HLP destination MAC address
 * @input_src_mac: input HLP source MAC address
 * @input_hlp_data_len: input HLP data length
 * @input_hlp_data: Pointer to input HLP data
 * @output_dst_mac: output HLP destination MAC address
 * @output_src_mac: output HLP source MAC address
 * @output_hlp_data_len: Pointer to output HLP data length
 * @output_hlp_data: output Pointer to HLP data
 *
 * Util API to copy HLP info from input to output
 *
 * Return: None
 */
void cds_copy_hlp_info(struct qdf_mac_addr *input_dst_mac,
		       struct qdf_mac_addr *input_src_mac,
		       uint16_t input_hlp_data_len,
		       uint8_t *input_hlp_data,
		       struct qdf_mac_addr *output_dst_mac,
		       struct qdf_mac_addr *output_src_mac,
		       uint16_t *output_hlp_data_len,
		       uint8_t *output_hlp_data);
#endif /* #ifndef __CDS_UTILS_H */
