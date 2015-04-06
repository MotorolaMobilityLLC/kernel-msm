/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_OPTS_H_
#define _WLAN_OPTS_H_

/* ATH_DEBUG -
 * Control whether debug features (printouts, assertions) are compiled
 * into the driver.
 */
#ifndef ATH_DEBUG
#define ATH_DEBUG 1 /* default: include debug code */
#endif

#if ATH_DEBUG
#define DEBUG_VAR_DECL_INIT(_var,_type,_value) _type (_var)=_value
#else
#define DEBUG_VAR_DECL_INIT(_var,_type,_value)
#endif

/* ATH_SUPPORT_WIRESHARK -
 * Control whether code that adds a radiotap packet header for consumption
 * by wireshark are compiled into the driver.
 */
#ifndef ATH_SUPPORT_WIRESHARK
#define ATH_SUPPORT_WIRESHARK            1 /* default: include radiotap/wireshark code */
#endif

#ifndef ATH_SUPPORT_ATHVAP_INFO
#define ATH_SUPPORT_ATHVAP_INFO          1
#endif

/* ATH_SUPPORT_IBSS -
 * Control whether Adhoc support is compiled into the driver.
 */
#ifndef ATH_SUPPORT_IBSS
#define ATH_SUPPORT_IBSS                 1 /* default: include radiotap/wireshark code */
#endif

/* ATH_SLOW_ANT_DIV -
 * Control whether Slow Antenna Diversity support is compiled into the driver.
 */
#ifndef ATH_SLOW_ANT_DIV
#define ATH_SLOW_ANT_DIV                 0 /* default: do not include Slow Antenna Diversity code */
#endif

#ifndef ATH_SUPPORT_CWM
#define ATH_SUPPORT_CWM                  1 /* on unless explicitly disabled */
#endif

#ifndef ATH_SUPPORT_MULTIPLE_SCANS
#define ATH_SUPPORT_MULTIPLE_SCANS       0 /* default: do not include suport for multiple simultaneous scans, including Scan Scheduler code */
#endif

#ifndef ATH_BUILD_VISTA_ONLY
#define ATH_BUILD_VISTA_ONLY             0 /* default: build VISTA and WIN7 as the same binary */
#endif

#ifndef IEEE80211_DEBUG_REFCNT_SE
#define IEEE80211_DEBUG_REFCNT_SE        0 /*default: do not include scan_entry reference count debug info */
#endif

#ifndef ATH_OSPREY_UAPSDDEFERRED
#define ATH_OSPREY_UAPSDDEFERRED         0 /* default: handle Osprey UAPSD in ISR */
#endif

#ifndef ATH_SUPPORT_STATS_APONLY
#define ATH_SUPPORT_STATS_APONLY		0 /*default: update all stats*/
#endif

#ifndef ATH_RESET_SERIAL
#define ATH_RESET_SERIAL                0 /*default: do not run ath_reset always in passive_level */
#endif

#ifndef ATH_LOW_PRIORITY_CALIBRATE
#define ATH_LOW_PRIORITY_CALIBRATE      0 /*default: do not run ath_calibrate in workitem. */
#endif

#endif /* _WLAN_OPTS_H_ */
