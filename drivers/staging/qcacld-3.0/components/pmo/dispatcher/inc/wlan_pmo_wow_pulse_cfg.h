/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_PMO_WOW_PULSE_CFG_H__
#define WLAN_PMO_WOW_PULSE_CFG_H__

#ifdef WLAN_FEATURE_WOW_PULSE
/*
 * <ini>
 * gwow_pulse_support - WOW pulse feature configuration
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * When set to 1 WOW pulse feature will be enabled.
 *
 * Related: gwow_pulse_pin, gwow_pulse_interval_low, gwow_pulse_interval_high
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_ENABLE CFG_INI_BOOL("gwow_pulse_support", \
					      0, \
					      "Enable wow pulse")

/*
 * <ini>
 * gwow_pulse_pin - GPIO pin for WOW pulse
 * @Min: 0
 * @Max: 254
 * @Default: 35
 *
 * Which PIN to send the Pulse
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_PIN CFG_INI_UINT("gwow_pulse_pin", \
					   0, 254, 35, \
					   CFG_VALUE_OR_DEFAULT, \
					   "Pin for wow pulse")

/*
 * <ini>
 * gwow_pulse_interval_low - Pulse interval low
 * @Min: 160
 * @Max: 480
 * @Default: 180
 *
 * The interval of low level in the pulse
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_LOW CFG_INI_UINT("gwow_pulse_interval_low", \
					   160, 480, 180, \
					   CFG_VALUE_OR_DEFAULT, \
					   "Interval of low pulse")

/*
 * <ini>
 * gwow_pulse_interval_high - Pulse interval high
 * @Min: 20
 * @Max: 40
 * @Default: 20
 *
 * The interval of high level in the pulse
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_HIGH CFG_INI_UINT("gwow_pulse_interval_high", \
					    20, 40, 20, \
					    CFG_VALUE_OR_DEFAULT, \
					    "Interval of high pulse")

/*
 * <ini>
 * gwow_pulse_repeat_count - wow pulse repetition count
 * @Min: 1
 * @Max: 0xffffffff
 * @Default: 1
 *
 * The repeat count of wow pin wave.
 * Level low to level high is one time, 0xffffffff means endless.
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_REPEAT CFG_INI_UINT("gwow_pulse_repeat_count", \
					      1, 0xffffffff, 1, \
					      CFG_VALUE_OR_DEFAULT, \
					      "Pulse repetition count")

/*
 * <ini>
 * gwow_pulse_init_state - wow pulse init level
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * The init level of wow pin, 1 is high level, 0 is low level.
 *
 * Supported Feature: WOW pulse
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_WOW_PULSE_INIT CFG_INI_UINT("gwow_pulse_init_state", \
					    0, 1, 1, \
					    CFG_VALUE_OR_DEFAULT, \
					    "Pulse init level")

#define CFG_WOW_PULSE_ALL \
	CFG(CFG_PMO_WOW_PULSE_ENABLE) \
	CFG(CFG_PMO_WOW_PULSE_PIN) \
	CFG(CFG_PMO_WOW_PULSE_LOW) \
	CFG(CFG_PMO_WOW_PULSE_HIGH) \
	CFG(CFG_PMO_WOW_PULSE_REPEAT) \
	CFG(CFG_PMO_WOW_PULSE_INIT)
#else
#define CFG_WOW_PULSE_ALL
#endif /* WLAN_FEATURE_WOW_PULSE */
#endif /* WLAN_PMO_WOW_PULSE_CFG_H__ */
