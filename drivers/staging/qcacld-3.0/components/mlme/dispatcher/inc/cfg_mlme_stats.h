/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_STATS_H
#define __CFG_MLME_STATS_H

enum mlme_stats_link_speed_rpt_type {
	CFG_STATS_LINK_SPEED_REPORT_ACTUAL = 0,
	CFG_STATS_LINK_SPEED_REPORT_MAX = 1,
	CFG_STATS_LINK_SPEED_REPORT_MAX_SCALED = 2,
};

/*
 * <ini>
 * periodic_stats_display_time - time(seconds) after which stats will be printed
 * @Min: 0
 * @Max: 256
 * @Default: 10
 *
 * This values specifies the recurring time period after which stats will be
 * printed in wlan driver logs.
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_PERIODIC_STATS_DISPLAY_TIME CFG_INI_UINT( \
		"periodic_stats_display_time", \
		0, \
		256, \
		10, \
		CFG_VALUE_OR_DEFAULT, \
		"time after which stats will be printed")

/*
 * <ini>
 * gLinkSpeedRssiMed - Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 * @Min: -127
 * @Max: 0
 * @Default: -65
 *
 * This ini is used to set medium rssi link speed
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_LINK_SPEED_RSSI_MID CFG_INI_INT( \
		"gLinkSpeedRssiMed", \
		-127, \
		0, \
		-65, \
		CFG_VALUE_OR_DEFAULT, \
		"medium rssi link speed")

/*
 * <ini>
 * gReportMaxLinkSpeed - Max link speed
 * @Min: CFG_STATS_LINK_SPEED_REPORT_ACTUAL
 * @Max: CFG_STATS_LINK_SPEED_REPORT_MAX_SCALED
 * @Default: CFG_STATS_LINK_SPEED_REPORT_ACTUAL
 *
 * This ini is used to set Max link speed
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_REPORT_MAX_LINK_SPEED CFG_INI_UINT( \
		"gReportMaxLinkSpeed", \
		CFG_STATS_LINK_SPEED_REPORT_ACTUAL, \
		CFG_STATS_LINK_SPEED_REPORT_MAX_SCALED, \
		CFG_STATS_LINK_SPEED_REPORT_ACTUAL, \
		CFG_VALUE_OR_DEFAULT, \
		"Max link speed")

/*
 * <ini>
 * gLinkSpeedRssiLow - Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 * @Min: -127
 * @Max: 0
 * @Default: -80
 *
 * This ini is used to set low rssi link speed
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_LINK_SPEED_RSSI_LOW CFG_INI_INT( \
		"gLinkSpeedRssiLow", \
		-127, \
		0, \
		-80, \
		CFG_VALUE_OR_DEFAULT, \
		"low rssi link speed")

/*
 * <ini>
 * gLinkSpeedRssiHigh - Report the max possible speed with RSSI scaling
 * @Min: -127
 * @Max: 0
 * @Default: -55
 *
 * This ini is used to set default eHDD_LINK_SPEED_REPORT
 * Used when eHDD_LINK_SPEED_REPORT_SCALED is selected
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal / External
 *
 * </ini>
 */
#define CFG_LINK_SPEED_RSSI_HIGH CFG_INI_INT( \
		"gLinkSpeedRssiHigh", \
		-127, \
		0, \
		-55, \
		CFG_VALUE_OR_DEFAULT, \
		"max possible rssi link speed")

#define CFG_STATS_ALL \
	CFG(CFG_PERIODIC_STATS_DISPLAY_TIME) \
	CFG(CFG_LINK_SPEED_RSSI_HIGH) \
	CFG(CFG_LINK_SPEED_RSSI_MID) \
	CFG(CFG_LINK_SPEED_RSSI_LOW) \
	CFG(CFG_REPORT_MAX_LINK_SPEED)

#endif /* __CFG_MLME_STATS_H */
