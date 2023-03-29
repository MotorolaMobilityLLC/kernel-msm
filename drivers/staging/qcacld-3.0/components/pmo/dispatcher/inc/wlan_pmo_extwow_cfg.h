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

#ifndef WLAN_PMO_EXTWOW_CFG_H__
#define WLAN_PMO_EXTWOW_CFG_H__

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/*
 * <ini>
 * gExtWoWgotoSuspend - Enable/Disable Extended WoW
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable Extended WoW.
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_GOTO_SUSPEND CFG_INI_BOOL("gExtWoWgotoSuspend", \
					     1, \
					     "Enable Ext WoW goto support")
/*
 * <ini>
 * gExtWowApp1WakeupPinNumber - Set wakeup1 PIN number
 * @Min: 0
 * @Max: 255
 * @Default: 12
 *
 * This ini is used to set EXT WOW APP1 wakeup PIN number
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_APP1_WAKE_PIN_NUMBER \
		CFG_INI_UINT("gExtWowApp1WakeupPinNumber", \
			     0, 255, 12, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set wakeup1 PIN number")
/*
 * <ini>
 * gExtWowApp2WakeupPinNumber - Set wakeup2 PIN number
 * @Min: 0
 * @Max: 255
 * @Default: 16
 *
 * This ini is used to set EXT WOW APP2 wakeup PIN number
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_APP2_WAKE_PIN_NUMBER \
		CFG_INI_UINT("gExtWowApp2WakeupPinNumber", \
			     0, 255, 16, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set wakeup2 PIN number")
/*
 * <ini>
 * gExtWoWApp2KAInitPingInterval - Set Keep Alive Init Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 240
 *
 * This ini is used to set Keep Alive Init Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_INIT_PING_INTERVAL \
		CFG_INI_UINT("gExtWoWApp2KAInitPingInterval", \
			     0, 0xffffffff, 240, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set Keep Alive Init Ping Interval")
/*
 * <ini>
 * gExtWoWApp2KAMinPingInterval - Set Keep Alive Minimum Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 240
 *
 * This ini is used to set Keep Alive Minimum Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_MIN_PING_INTERVAL \
		CFG_INI_UINT("gExtWoWApp2KAMinPingInterval", \
			     0, 0xffffffff, 240, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set Keep Alive Minimum Ping Interval")
/*
 * <ini>
 * gExtWoWApp2KAMaxPingInterval - Set Keep Alive Maximum Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 1280
 *
 * This ini is used to set Keep Alive Maximum Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_MAX_PING_INTERVAL \
		CFG_INI_UINT("gExtWoWApp2KAMaxPingInterval", \
			     0, 0xffffffff, 1280, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set Keep Alive Maximum Ping Interval")
/*
 * <ini>
 * gExtWoWApp2KAIncPingInterval - Set Keep Alive increment of Ping Interval
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 4
 *
 * This ini is used to set Keep Alive increment of Ping Interval for EXT WOW
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_KA_INC_PING_INTERVAL \
		CFG_INI_UINT("gExtWoWApp2KAIncPingInterval", \
			     0, 0xffffffff, 4, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set Keep Alive increment of Ping Interval")
/*
 * <ini>
 * gExtWoWApp2TcpSrcPort - Set TCP source port
 * @Min: 0
 * @Max: 65535
 * @Default: 5000
 *
 * This ini is used to set TCP source port when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_SRC_PORT \
		CFG_INI_UINT("gExtWoWApp2TcpSrcPort", \
			     0, 65535, 5000, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set TCP source port")
/*
 * <ini>
 * gExtWoWApp2TcpDstPort - Set TCP Destination port
 * @Min: 0
 * @Max: 65535
 * @Default: 5001
 *
 * This ini is used to set TCP Destination port when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_DST_PORT \
		CFG_INI_UINT("gExtWoWApp2TcpDstPort", \
			     0, 65535, 5001, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set TCP Destination port")
/*
 * <ini>
 * gExtWoWApp2TcpTxTimeout - Set TCP tx timeout
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 200
 *
 * This ini is used to set TCP Tx timeout when EXT WOW is enabled
 *
 * Related: None
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_TX_TIMEOUT \
		CFG_INI_UINT("gExtWoWApp2TcpTxTimeout", \
			     0, 0xffffffff, 200, \
			     CFG_VALUE_OR_DEFAULT, \
			     "Set TCP tx timeout")

/*
 * <ini>
 * gExtWoWApp2TcpRxTimeout - Set TCP rx timeout
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 200
 *
 * This ini is used to set TCP Rx timeout when EXT WOW is enabled
 *
 * Supported Feature: Power Save
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EXTWOW_TCP_RX_TIMEOUT \
		CFG_INI_UINT("gExtWoWApp2TcpRxTimeout", \
			     0, 0xffffffff, 200, \
			     CFG_VALUE_OR_DEFAULT, \
			     "ExtWow App2 tcp rx timeout")

#define CFG_EXTWOW_ALL \
	CFG(CFG_EXTWOW_GOTO_SUSPEND) \
	CFG(CFG_EXTWOW_APP1_WAKE_PIN_NUMBER) \
	CFG(CFG_EXTWOW_APP2_WAKE_PIN_NUMBER) \
	CFG(CFG_EXTWOW_KA_INIT_PING_INTERVAL) \
	CFG(CFG_EXTWOW_KA_MIN_PING_INTERVAL) \
	CFG(CFG_EXTWOW_KA_MAX_PING_INTERVAL) \
	CFG(CFG_EXTWOW_KA_INC_PING_INTERVAL) \
	CFG(CFG_EXTWOW_TCP_SRC_PORT) \
	CFG(CFG_EXTWOW_TCP_DST_PORT) \
	CFG(CFG_EXTWOW_TCP_TX_TIMEOUT) \
	CFG(CFG_EXTWOW_TCP_RX_TIMEOUT)
#else
#define CFG_EXTWOW_ALL
#endif /* WLAN_FEATURE_EXTWOW_SUPPORT */
#endif /* WLAN_PMO_EXTWOW_CFG_H_ */
