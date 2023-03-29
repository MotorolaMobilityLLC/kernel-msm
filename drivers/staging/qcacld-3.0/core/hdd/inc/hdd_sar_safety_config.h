/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

#ifndef __HDD_SAR_SAFETY_CONFIG_H
#define __HDD_SAR_SAFETY_CONFIG_H

#ifdef SAR_SAFETY_FEATURE

/*
 * <ini>
 * gSarSafetyTimeout - Specify SAR safety timeout value in milliseconds
 *
 * @Min: 120000
 * @Max: 600000
 * Default: 300000
 *
 * This ini is used to define SAR safety timeout value in milliseconds.
 * This timer is started when the QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * is received first time.
 * SAR safety timer will wait for the gSarSafetyTimeout for
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and if
 * SAR safety timer timeouts host will configure the gSarSafetyIndex
 * to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_TIMEOUT  CFG_INI_UINT( \
			"gSarSafetyTimeout", \
			120000, \
			600000, \
			300000, \
			CFG_VALUE_OR_DEFAULT, \
			"Timeout value for SAR safety timer")

/*
 * <ini>
 * gSarSafetyUnsolicitedTimeout - Specify SAR safety unsolicited timeout value
 * in milliseconds
 *
 * @Min: 5000
 * @Max: 30000
 * Default: 15000
 *
 * This ini is used to define SAR safety unsolicited timeout value in
 * milliseconds. This timer is started on first data tx.
 * SAR unsolicited timer will wait for the
 * gSarSafetyUnsolicitedTimeout for QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * vendor command and if SAR unsolicited timer timeouts host will indicate
 * user space with QCA_NL80211_VENDOR_SUBCMD_REQUEST_SAR_LIMITS_EVENT to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT  CFG_INI_UINT( \
			"gSarSafetyUnsolicitedTimeout", \
			5000, \
			30000, \
			15000, \
			CFG_VALUE_OR_DEFAULT, \
			"Timeout value for SAR Unsolicited timer")

/*
 * <ini>
 * gSarSafetyReqRespTimeout - Specify SAR safety request response timeout value
 * in milliseconds
 *
 * @Min: 500
 * @Max: 3000
 * Default: 1000
 *
 * This ini is used to define SAR request-response timeout value
 * in milliseconds. SAR request-response timer will wait for the
 * gSarSafetyReqRespTimeout for QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS
 * vendor command and if SAR request-response timer timeouts host will
 * indicate user space with QCA_NL80211_VENDOR_SUBCMD_REQUEST_SAR_LIMITS_EVENT
 * for gSarSafetyReqRespRetry number of times to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and still if host
 * does not get QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command, host
 * will configure the gSarSafetyIndex to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_REQ_RESP_TIMEOUT  CFG_INI_UINT( \
			"gSarSafetyReqRespTimeout", \
			500, \
			1000, \
			1000, \
			CFG_VALUE_OR_DEFAULT, \
			"Timeout value for SAR safety request response timer")

/*
 * <ini>
 * gSarSafetyReqRespRetry - Specify SAR request response retries value
 *
 * @Min: 1
 * @Max: 10
 * Default: 5
 *
 * This ini is used to define SAR request-response retries value.
 * SAR request-response timer will wait for the gSarReqRespTimeout for
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and if
 * SAR request-response timer timeouts host will indicate user space
 * for gSarSafetyReqRespRetry number of times to send
 * QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor command and still if
 * host does not get QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS vendor
 * command, host will configure the gSarSafetyIndex to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_REQ_RESP_RETRIES  CFG_INI_UINT( \
			"gSarSafetyReqRespRetry", \
			1, \
			5, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"Max Number of SAR Request Response Retries")

/*
 * <ini>
 * gSarSafetyIndex - Specify SAR safety index
 *
 * @Min: 0
 * @Max: 11
 * Default: 11
 *
 * This ini is used to define SAR safety index, when sar safety timer
 * timeouts or sar request response timer timeouts for gSarSafetyReqRespRetry
 * number of times, host will configure gSarSafetyIndex value to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_INDEX  CFG_INI_UINT( \
			"gSarSafetyIndex", \
			0, \
			11, \
			11, \
			CFG_VALUE_OR_DEFAULT, \
			"SAR safety index value")
/*
 * <ini>
 * gSarSafetySleepIndex - Specify SAR Safety sleep index
 *
 * @Min: 0
 * @Max: 11
 * Default: 11
 *
 * This ini is used to define SAR sleep index, when device goes into the
 * sleep mode, before going into the sleep mode host configures
 * gSarSafetySleepIndex value to the FW.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAR_SAFETY_SLEEP_INDEX  CFG_INI_UINT( \
			"gSarSafetySleepIndex", \
			0, \
			11, \
			11, \
			CFG_VALUE_OR_DEFAULT, \
			"SAR safety sleep index value")

/*
 * <ini>
 * gEnableSarSafety - Enable/Disable SAR safety feature
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This ini is used to enable/disable SAR safety feature
 * Value 1 of this ini enables SAR safety feature and
 * value 0 of this ini disables SAR safety feature
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_SAR_SAFETY_FEATURE CFG_INI_BOOL( \
			"gEnableSarSafety", \
			0, \
			"Enable/Disable SAR safety feature")

/*
 * <ini>
 * gConfigSarSafetySleepIndex - Enable/Disable SAR Safety sleep index
 *
 * @Min: 0
 * @Max: 1
 * Default: 0
 *
 * This Configuration is to decide that before going to
 * sleep mode whether to maintain high RF power
 * (SAR disable) or to configure SAR sleep mode index
 *
 * Value 0 for this ini indicates to maintain high
 * RF power (SAR disable)
 * Value 1 for this ini indicates to configure SAR
 * sleep mode index.
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX CFG_INI_BOOL( \
			"gConfigSarSafetySleepIndex", \
			0, \
			"Config SAR sleep Index")

#define SAR_SAFETY_FEATURE_ALL \
	CFG(CFG_SAR_SAFETY_TIMEOUT) \
	CFG(CFG_SAR_SAFETY_UNSOLICITED_TIMEOUT) \
	CFG(CFG_SAR_SAFETY_REQ_RESP_TIMEOUT) \
	CFG(CFG_SAR_SAFETY_REQ_RESP_RETRIES) \
	CFG(CFG_SAR_SAFETY_INDEX) \
	CFG(CFG_SAR_SAFETY_SLEEP_INDEX) \
	CFG(CFG_ENABLE_SAR_SAFETY_FEATURE) \
	CFG(CFG_CONFIG_SAR_SAFETY_SLEEP_MODE_INDEX) \

#else
#define SAR_SAFETY_FEATURE_ALL
#endif

#endif
