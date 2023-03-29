/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains configuration definitions for MLME OBSS HT40.
 */
#ifndef CFG_MLME_OBSS_HT40_H__
#define CFG_MLME_OBSS_HT40_H__

/*
 * <ini>
 * obss_active_dwelltime - Set obss active dwelltime
 * @Min: 10
 * @Max: 1000
 * @Default: 10
 *
 * This ini is used to set dwell time in secs for active
 * obss scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME CFG_INI_UINT( \
	"obss_active_dwelltime", \
	10, \
	1000, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Set obss active dwelltime")

/*
 * <ini>
 * obss_passive_dwelltime - Set obss passive dwelltime
 * @Min: 5
 * @Max: 1000
 * @Default: 20
 *
 * This ini is used to set dwell time in secs for passive
 * obss scan
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME CFG_INI_UINT( \
	"obss_passive_dwelltime", \
	5, \
	1000, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"Set obss passive dwelltime")

/*
 * <ini>
 * obss_width_trigger_interval - Set obss trigger interval
 * @Min: 10
 * @Max: 900
 * @Default: 200
 *
 * This ini is used during an OBSS scan operation,
 * where each channel in the set is scanned at least
 * once per configured trigger interval time. Unit is TUs.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL CFG_INI_UINT( \
	"obss_width_trigger_interval", \
	10, \
	900, \
	200, \
	CFG_VALUE_OR_DEFAULT, \
	"Set obss trigger interval")

/*
 * obss_passive_total_per_channel - Set obss scan passive total per channel
 * @Min: 200
 * @Max: 10000
 * @Default: 200
 *
 * FW can perform multiple scans with in a OBSS scan interval. This cfg is for
 * the total per channel dwell time of passive scans. Unit is TUs.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 */
#define CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL CFG_UINT( \
	"obss_passive_total_per_channel", \
	200, \
	10000, \
	200, \
	CFG_VALUE_OR_DEFAULT, \
	"obss passive total per channel")

/*
 * obss_active_total_per_channel - Set obss scan active total per channel
 * @Min: 20
 * @Max: 10000
 * @Default: 20
 *
 * FW can perform multiple scans with in a OBSS scan interval. This cfg is for
 * the total per channel dwell time of active scans. Unit is TUs.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 */
#define CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL CFG_UINT( \
	"obss_active_total_per_channel", \
	20, \
	10000, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"obss active total per channel")

/*
 * obss_scan_activity_thre - Set obss scan activity threshold
 * @Min: 0
 * @Max: 100
 * @Default: 25
 *
 * This cfg sets obss scan activity threshold.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 */
#define CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD CFG_UINT( \
	"obss_scan_activity_thre", \
	0, \
	100, \
	25, \
	CFG_VALUE_OR_DEFAULT, \
	"obss ht40 scan activity threshold")

/*
 * obss_width_transition_delay - Set obss width transition delay
 * @Min: 5
 * @Max: 100
 * @Default: 5
 *
 * This cfg sets obss width transition delay, it is used to check exemption
 * from scan. The unit is TUs.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: Internal
 *
 */
#define CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY CFG_UINT( \
	"obss_width_transition_delay", \
	5, \
	100, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"obss ht40 width transition delay")

/*
 * <ini>
 * override_ht20_40_24g - Use channel bonding in 2.4GHz from supplicant
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini used to set whether use channel Bonding in 2.4GHz from supplicant
 * if gChannelBondingMode24GHz is set
 *
 * Related: gChannelBondingMode24GHz
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_OBSS_HT40_OVERRIDE_HT40_20_24GHZ CFG_INI_BOOL( \
		"override_ht20_40_24g", \
		0, \
		"Use channel bonding in 24 GHz")

/*
 * <cfg>
 * obss_detection_offload - Enable OBSS detection offload
 * @Min: 0
 * @Max: 1
 * @Default: 0
 */
#define CFG_OBSS_DETECTION_OFFLOAD CFG_BOOL( \
		"obss_detection_offload", \
		0, \
		"Enable OBSS detection offload")

/*
 * <cfg>
 * obss_color_collision_offload - Enable obss color collision offload
 * @Min: 0
 * @Max: 1
 * @Default: 0
 */
#define CFG_OBSS_COLOR_COLLISION_OFFLOAD CFG_BOOL( \
		"obss_color_collision_offload", \
		0, \
		"Enable obss color collision offload")

/*
 * <ini>
 * bss_color_collision_det_sta - Enables BSS color collision detection in STA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini used to enable or disable the BSS color collision detection in
 * STA mode if obss_color_collision_offload is enabled.
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BSS_CLR_COLLISION_DETCN_STA CFG_INI_BOOL( \
		"bss_color_collision_det_sta", \
		1, \
		"BSS color collision detection in STA")

#define CFG_OBSS_HT40_ALL \
	CFG(CFG_OBSS_HT40_SCAN_ACTIVE_DWELL_TIME) \
	CFG(CFG_OBSS_HT40_SCAN_PASSIVE_DWELL_TIME) \
	CFG(CFG_OBSS_HT40_SCAN_WIDTH_TRIGGER_INTERVAL) \
	CFG(CFG_OBSS_HT40_SCAN_PASSIVE_TOTAL_PER_CHANNEL) \
	CFG(CFG_OBSS_HT40_SCAN_ACTIVE_TOTAL_PER_CHANNEL) \
	CFG(CFG_OBSS_HT40_SCAN_ACTIVITY_THRESHOLD) \
	CFG(CFG_OBSS_HT40_WIDTH_CH_TRANSITION_DELAY) \
	CFG(CFG_OBSS_HT40_OVERRIDE_HT40_20_24GHZ) \
	CFG(CFG_OBSS_DETECTION_OFFLOAD) \
	CFG(CFG_BSS_CLR_COLLISION_DETCN_STA) \
	CFG(CFG_OBSS_COLOR_COLLISION_OFFLOAD)

#endif /* CFG_MLME_OBSS_HT40_H__ */
