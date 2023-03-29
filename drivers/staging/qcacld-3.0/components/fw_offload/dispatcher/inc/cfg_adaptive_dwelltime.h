/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains adaptive dwell components.
 */

#ifndef __CFG_ADAPTIVE_DWELLTIME_H
#define __CFG_ADAPTIVE_DWELLTIME_H

/*
 * <ini>
 * adaptive_dwell_mode_enabled - enable/disable the adaptive dwell config.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 *
 * This ini will globally disable/enable the adaptive dwell config.
 * Following parameters will set different values of attributes for dwell
 * time optimization thus reducing total scan time.
 * Acceptable values for this:
 * 0: Config is disabled
 * 1: Config is enabled
 *
 * Related: None.
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_DWELL_MODE_ENABLED CFG_INI_BOOL(\
				"adaptive_dwell_mode_enabled",\
				1, \
				"enable the adaptive dwell config")

/*
 * <ini>
 * global_adapt_dwelltime_mode - set default adaptive mode.
 * @Min: 0
 * @Max: 4
 * @Default: 0
 *
 * This ini will set default adaptive mode, will be used if any of the
 * scan dwell mode is set to default.
 * For uses : see enum scan_dwelltime_adaptive_mode
 *
 * Related: None.
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GLOBAL_ADAPTIVE_DWELL_MODE CFG_INI_UINT(\
				"global_adapt_dwelltime_mode",\
				0, 4, 0,\
				CFG_VALUE_OR_DEFAULT, \
				"set default adaptive mode")

/*
 * <ini>
 * adapt_dwell_lpf_weight - weight to caclulate avg low pass filter.
 * @Min: 0
 * @Max: 100
 * @Default: 80
 *
 * This ini is used to set the weight to calculate
 * the average low pass filter for channel congestion.
 * Acceptable values for this: 0-100 (In %)
 *
 * Related: None.
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPT_DWELL_LPF_WEIGHT CFG_INI_UINT(\
				"adapt_dwell_lpf_weight",\
				0, 100, 80,\
				CFG_VALUE_OR_DEFAULT, \
				"weight to calc avg low pass filter")

/*
 * <ini>
 * adapt_dwell_passive_mon_intval - Interval to monitor passive scan in msec.
 * @Min: 0
 * @Max: 25
 * @Default: 10
 *
 * This ini is used to set interval to monitor wifi
 * activity in passive scan in milliseconds.
 *
 * Related: None.
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPT_DWELL_PASMON_INTVAL CFG_INI_UINT(\
				"adapt_dwell_passive_mon_intval",\
				0, 25, 10,\
				CFG_VALUE_OR_DEFAULT, \
				"interval to monitor passive scan")

/*
 * <ini>
 * adapt_dwell_wifi_act_threshold - % of wifi activity used in passive scan
 * @Min: 0
 * @Max: 100
 * @Default: 10
 *
 * This ini is used to set % of wifi activity used in passive scan
 * Acceptable values for this: 0-100 (in %)
 *
 * Related: None.
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPT_DWELL_WIFI_THRESH CFG_INI_UINT(\
				"adapt_dwell_wifi_act_threshold",\
				0, 100, 10,\
				CFG_VALUE_OR_DEFAULT, \
				"percent of wifi activity in pas scan")

#define CFG_ADAPTIVE_DWELLTIME_ALL \
	CFG(CFG_ADAPTIVE_DWELL_MODE_ENABLED) \
	CFG(CFG_GLOBAL_ADAPTIVE_DWELL_MODE) \
	CFG(CFG_ADAPT_DWELL_LPF_WEIGHT) \
	CFG(CFG_ADAPT_DWELL_PASMON_INTVAL) \
	CFG(CFG_ADAPT_DWELL_WIFI_THRESH)

#endif

