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
 * DOC: This file contains centralized definitions of power save related
 * converged configurations.
 */

#ifndef __CFG_MLME_POWERSAVE_H
#define __CFG_MLME_POWERSAVE_H

/*
 * <ini>
 * gEnableImps - Enable/Disable IMPS
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/Disable IMPS(IdleModePowerSave) Mode
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_IMPS CFG_INI_BOOL( \
		"gEnableImps", \
		1,\
		"Enable/disable IMPS")

/*
 * <ini>
 * gEnableBmps - Enable/Disable BMPS
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/Disable BMPS(BeaconModePowerSave) Mode
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PS  CFG_INI_BOOL( \
		"gEnableBmps", \
		1,\
		"Enable/disable BMPS")

/*
 * <ini>
 * gAutoBmpsTimerValue - Set Auto BMPS Timer value
 * @Min: 0
 * @Max: 1000
 * @Default: 600
 *
 * This ini is used to set Auto BMPS Timer value in seconds
 *
 * Related: gEnableBmps
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_AUTO_BMPS_ENABLE_TIMER CFG_INI_UINT( \
		"gAutoBmpsTimerValue", \
		0, \
		1000, \
		600, \
		CFG_VALUE_OR_DEFAULT, \
		"Auto BMPS Timer value")

/*
 * <ini>
 * gBmpsMinListenInterval - Set BMPS Minimum Listen Interval
 * @Min: 1
 * @Max: 65535
 * @Default: 1
 *
 * This ini is used to set BMPS Minimum Listen Interval. If gPowerUsage
 * is set "Min", this INI need to be set.
 *
 * Related: gEnableBmps, gPowerUsage
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMPS_MINIMUM_LI CFG_INI_UINT( \
		"gBmpsMinListenInterval", \
		1, \
		65535, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"BMPS Minimum Listen Interval")

/*
 * <ini>
 * gBmpsMaxListenInterval - Set BMPS Maximum Listen Interval
 * @Min: 1
 * @Max: 65535
 * @Default: 1
 *
 * This ini is used to set BMPS Maximum Listen Interval. If gPowerUsage
 * is set "Max", this INI need to be set.
 *
 * Related: gEnableBmps, gPowerUsage
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BMPS_MAXIMUM_LI CFG_INI_UINT( \
		"gBmpsMaxListenInterval", \
		1, \
		65535, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"BMPS Maximum Listen Interval")

/*
 * <ini>
 * gEnableDTIMSelectionDiversity - Enable/Disable chain
 * selection optimization for one chain dtim
 * @Min: 0
 * @Max: 30
 * @Default: 5
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DTIM_SELECTION_DIVERSITY CFG_INI_UINT( \
		"gEnableDTIMSelectionDiversity", \
		0, \
		30, \
		5, \
		CFG_VALUE_OR_DEFAULT, \
		"Chain selection diversity value")

#define CFG_POWERSAVE_ALL \
	CFG(CFG_ENABLE_IMPS) \
	CFG(CFG_ENABLE_PS) \
	CFG(CFG_AUTO_BMPS_ENABLE_TIMER) \
	CFG(CFG_BMPS_MINIMUM_LI) \
	CFG(CFG_BMPS_MAXIMUM_LI) \
	CFG(CFG_DTIM_SELECTION_DIVERSITY)

#endif /* __CFG_MLME_POWERSAVE_H */
