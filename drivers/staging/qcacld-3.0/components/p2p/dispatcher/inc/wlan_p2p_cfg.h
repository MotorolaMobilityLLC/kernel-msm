/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#if !defined(CONFIG_P2P_H__)
#define CONFIG_P2P_H__

#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"

/*
 * <ini>
 * gGoKeepAlivePeriod - P2P GO keep alive period.
 * @Min: 1
 * @Max: 65535
 * @Default: 20
 *
 * This is P2P GO keep alive period.
 *
 * Related: None.
 *
 * Supported Feature: P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GO_KEEP_ALIVE_PERIOD CFG_INI_UINT( \
	"gGoKeepAlivePeriod", \
	1, \
	65535, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"P2P GO keep alive period")

/*
 * <ini>
 * gGoLinkMonitorPeriod - period where link is idle and where
 * we send NULL frame
 * @Min: 3
 * @Max: 50
 * @Default: 10
 *
 * This is period where link is idle and where we send NULL frame for P2P GO.
 *
 * Related: None.
 *
 * Supported Feature: P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_GO_LINK_MONITOR_PERIOD CFG_INI_UINT( \
	"gGoLinkMonitorPeriod", \
	3, \
	50, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"period where link is idle and where we send NULL frame")

/*
 * <ini>
 * isP2pDeviceAddrAdministrated - Enables to derive the P2P MAC address from
 * the primary MAC address
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable to derive the P2P MAC address from the
 * primary MAC address.
 *
 * Related: None.
 *
 * Supported Feature: P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED CFG_INI_BOOL( \
	"isP2pDeviceAddrAdministrated", \
	1, \
	"derive the P2P MAC address from the primary MAC address")

#define CFG_P2P_ALL \
	CFG(CFG_GO_KEEP_ALIVE_PERIOD) \
	CFG(CFG_GO_LINK_MONITOR_PERIOD) \
	CFG(CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED)

#endif
