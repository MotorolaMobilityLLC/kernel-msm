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

#ifndef WLAN_PMO_APF_CFG_H__
#define WLAN_PMO_APF_CFG_H__

/*
 * <ini>
 * gBpfFilterEnable - APF feature support configuration
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * When set to 1 APF feature will be enabled.
 *
 * Supported Feature: Android packet filter
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMO_APF_ENABLE CFG_INI_BOOL("gBpfFilterEnable", \
					1, \
					"Enable APF Support")

/*
 * <ini>
 * gActiveUcBpfMode - Control UC active APF mode
 * @Min: 0 (disabled)
 * @Max: 2 (adaptive)
 * @Default: 0 (disabled)
 *
 * This config item controls UC APF in active mode. There are 3 modes:
 *	0) disabled - APF is disabled in active mode
 *	1) enabled - APF is enabled for all packets in active mode
 *	2) adaptive - APF is enabled for packets up to some throughput threshold
 *
 * Related: gActiveMcBcBpfMode
 *
 * Supported Feature: Active Mode APF
 *
 * Usage: Internal/External
 * </ini>
 */
#define CFG_ACTIVE_UC_APF_MODE CFG_INI_UINT( \
	"gActiveUcBpfMode", \
	ACTIVE_APF_DISABLED, \
	(ACTIVE_APF_MODE_COUNT - 1), \
	ACTIVE_APF_DISABLED, \
	CFG_VALUE_OR_DEFAULT, \
	"Control UC active APF mode")

/*
 * <ini>
 * gActiveMcBcBpfMode - Control MC/BC active APF mode
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 0 (disabled)
 *
 * This config item controls MC/BC APF in active mode. There are 3 modes:
 *	0) disabled - APF is disabled in active mode
 *	1) enabled - APF is enabled for all packets in active mode
 *	2) adaptive - APF is enabled for packets up to some throughput threshold
 *
 * Related: gActiveUcBpfMode
 *
 * Supported Feature: Active Mode APF
 *
 * Usage: Internal/External
 * </ini>
 */
#define CFG_ACTIVE_MC_BC_APF_MODE CFG_INI_UINT( \
	"gActiveMcBcBpfMode", \
	ACTIVE_APF_DISABLED, \
	ACTIVE_APF_ENABLED, \
	ACTIVE_APF_DISABLED, \
	CFG_VALUE_OR_DEFAULT, \
	"Control MC/BC active APF mode")

#define CFG_PMO_APF_ALL \
	CFG(CFG_PMO_APF_ENABLE) \
	CFG(CFG_ACTIVE_UC_APF_MODE) \
	CFG(CFG_ACTIVE_MC_BC_APF_MODE)

#endif /* WLAN_PMO_APF_CFG_H__ */
