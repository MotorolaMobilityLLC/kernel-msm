/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_PMO_RUNTIME_PM_CFG_H__
#define WLAN_PMO_RUNTIME_PM_CFG_H__

#ifdef FEATURE_RUNTIME_PM
/*
 * <ini>
 * gRuntimePMDelay - Set runtime pm's inactivity timer
 * @Min: 100
 * @Max: 10000
 * @Default: 500
 *
 * This ini is used to set runtime pm's inactivity timer value.
 * the wlan driver will wait for this number of milliseconds of
 * inactivity before performing a runtime suspend.
 *
 * Related: gRuntimePM
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_RUNTIME_PM_DELAY CFG_INI_UINT( \
	"gRuntimePMDelay", \
	100, \
	10000, \
	500, \
	CFG_VALUE_OR_DEFAULT, \
	"Set runtime pm's inactivity timer")

#define CFG_RUNTIME_PM_ALL \
	CFG(CFG_PMO_RUNTIME_PM_DELAY)
#else
#define CFG_RUNTIME_PM_ALL
#endif /* FEATURE_RUNTIME_PM */
#endif /* WLAN_PMO_RUNTIME_PM_CFG_H__ */
