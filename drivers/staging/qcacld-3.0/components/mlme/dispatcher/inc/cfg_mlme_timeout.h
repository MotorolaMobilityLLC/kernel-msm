/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_TIMEOUT_H
#define __CFG_MLME_TIMEOUT_H

/*
 * <ini>
 * join_failure_timeout - Join failure timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 3000
 *
 * This cfg is used to configure the join failure timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_JOIN_FAILURE_TIMEOUT CFG_INI_UINT( \
		"join_failure_timeout", \
		0, \
		65535, \
		3000, \
		CFG_VALUE_OR_DEFAULT, \
		"Join failure timeout")

/*
 * <ini>
 * auth_failure_timeout - Auth failure timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 1000
 *
 * This cfg is used to configure the auth failure timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_AUTH_FAILURE_TIMEOUT CFG_INI_UINT( \
		"auth_failure_timeout", \
		500, \
		5000, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"auth failure timeout")

/*
 * <ini>
 * auth_rsp_timeout - Auth response timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 1000
 *
 * This cfg is used to configure the auth response timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_AUTH_RSP_TIMEOUT CFG_INI_UINT( \
		"auth_rsp_timeout", \
		0, \
		65535, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"auth rsp timeout")

/*
 * <ini>
 * assoc_failure_timeout - Assoc failure timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 2000
 *
 * This cfg is used to configure the assoc failure timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ASSOC_FAILURE_TIMEOUT CFG_INI_UINT( \
		"assoc_failure_timeout", \
		0, \
		65535, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"assoc failure timeout")

/*
 * <ini>
 * reassoc_failure_timeout - Re-Assoc failure timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 1000
 *
 * This cfg is used to configure the re-assoc failure timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_REASSOC_FAILURE_TIMEOUT CFG_INI_UINT( \
		"reassoc_failure_timeout", \
		0, \
		65535, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"reassoc failure timeout")

/*
 * <ini>
 * probe_after_hb_fail_timeout - Probe after HB failure timeout value
 * @Min: 10
 * @Max: 10000
 * @Default: 70
 *
 * This cfg is used to configure the Probe after HB failure timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_PROBE_AFTER_HB_FAIL_TIMEOUT CFG_INI_UINT( \
		"probe_after_hb_fail_timeout", \
		10, \
		10000, \
		70, \
		CFG_VALUE_OR_DEFAULT, \
		"probe after HB fail timeout")

/*
 * <ini>
 * olbc_detect_timeout - olbc detect timeout value
 * @Min: 1000
 * @Max: 30000
 * @Default: 10000
 *
 * This cfg is used to configure the olbc detect timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_OLBC_DETECT_TIMEOUT CFG_INI_UINT( \
		"olbc_detect_timeout", \
		1000, \
		30000, \
		10000, \
		CFG_VALUE_OR_DEFAULT, \
		"OLBC detect timeout")

/*
 * <ini>
 * addts_rsp_timeout - addts response timeout value
 * @Min: 0
 * @Max: 65535
 * @Default: 1000
 *
 * This cfg is used to configure the addts response timeout.
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ADDTS_RSP_TIMEOUT CFG_INI_UINT( \
		"addts_rsp_timeout", \
		0, \
		65535, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"ADDTS RSP timeout")

/*
 * <ini>
 * gHeartbeat24 - Heart beat threashold value
 * @Min: 0
 * @Max: 65535
 * @Default: 40
 *
 * This cfg is used to configure the Heart beat threashold.
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_HEART_BEAT_THRESHOLD CFG_INI_UINT( \
		"gHeartbeat24", \
		0, \
		65535, \
		40, \
		CFG_VALUE_OR_DEFAULT, \
		"Heart beat threashold")

/*
 * <ini>
 * gApKeepAlivePeriod - AP keep alive period
 * @Min: 1
 * @Max: 65535
 * @Default: 20
 *
 * This ini is used to set keep alive period(in seconds) of AP
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_KEEP_ALIVE_TIMEOUT CFG_INI_UINT( \
		"gApKeepAlivePeriod", \
		1, \
		65535, \
		20, \
		CFG_VALUE_OR_DEFAULT, \
		"AP keep alive timeout")

/*
 * <ini>
 * gApLinkMonitorPeriod - AP keep alive period
 * @Min: 3
 * @Max: 50
 * @Default: 10
 *
 * This ini is used to configure AP link monitor timeout value
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_LINK_MONITOR_TIMEOUT CFG_INI_UINT( \
		"gApLinkMonitorPeriod", \
		3, \
		50, \
		10, \
		CFG_VALUE_OR_DEFAULT, \
		"AP link monitor timeout")

/*
 * <ini>
 * gDataInactivityTimeout - Data inactivity timeout for non wow mode.
 * @Min: 1
 * @Max: 255
 * @Default: 200
 *
 * This ini is used to set data inactivity timeout value, in milliseconds, of
 * non wow mode.
 *
 * Supported Feature: inactivity timeout in non wow mode
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PS_DATA_INACTIVITY_TIMEOUT CFG_INI_UINT( \
		"gDataInactivityTimeout", \
		1, \
		255, \
		200, \
		CFG_VALUE_OR_DEFAULT, \
		"PS data inactivity timeout")

/*
 * <ini>
 * wmi_wq_watchdog - Sets timeout period for wmi watchdog bite.
 * @Min: 0
 * @Max: 30
 * @Default: 20
 *
 * This ini is used to set timeout period for wmi watchdog bite. If it is
 * 0 then wmi watchdog bite is disabled.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WMI_WQ_WATCHDOG CFG_INI_UINT( \
		"wmi_wq_watchdog", \
		0, \
		30, \
		20, \
		CFG_VALUE_OR_DEFAULT, \
		"timeout period for wmi watchdog bite")

/*
 * <ini>
 * sae_auth_failure_timeout - SAE Auth failure timeout value in msec
 * @Min: 500
 * @Max: 1000
 * @Default: 1000
 *
 * This cfg is used to configure the SAE auth failure timeout.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAE_AUTH_FAILURE_TIMEOUT CFG_INI_UINT( \
		"sae_auth_failure_timeout", \
		500, \
		1000, \
		1000, \
		CFG_VALUE_OR_DEFAULT, \
		"SAE auth failure timeout")

#define CFG_TIMEOUT_ALL \
	CFG(CFG_JOIN_FAILURE_TIMEOUT) \
	CFG(CFG_AUTH_FAILURE_TIMEOUT) \
	CFG(CFG_AUTH_RSP_TIMEOUT) \
	CFG(CFG_ASSOC_FAILURE_TIMEOUT) \
	CFG(CFG_REASSOC_FAILURE_TIMEOUT) \
	CFG(CFG_PROBE_AFTER_HB_FAIL_TIMEOUT) \
	CFG(CFG_OLBC_DETECT_TIMEOUT) \
	CFG(CFG_ADDTS_RSP_TIMEOUT) \
	CFG(CFG_HEART_BEAT_THRESHOLD) \
	CFG(CFG_AP_KEEP_ALIVE_TIMEOUT) \
	CFG(CFG_AP_LINK_MONITOR_TIMEOUT) \
	CFG(CFG_WMI_WQ_WATCHDOG) \
	CFG(CFG_PS_DATA_INACTIVITY_TIMEOUT)\
	CFG(CFG_SAE_AUTH_FAILURE_TIMEOUT)

#endif /* __CFG_MLME_TIMEOUT_H */
