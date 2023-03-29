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
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_OCE_H
#define __CFG_MLME_OCE_H

/*
 * <ini>
 * g_enable_bcast_probe_rsp - Enable Broadcast probe response.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable broadcast probe response.
 * If this is disabled then OCE ini oce_sta_enable will also be
 * disabled and OCE IE will not be sent in frames.
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BCAST_PROBE_RESP CFG_INI_BOOL( \
		"g_enable_bcast_probe_rsp", \
		1, \
		"Enable Broadcast probe response")

/*
 * <ini>
 * oce_sta_enable - Enable/disable oce feature for STA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable oce feature for STA
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_STA CFG_INI_BOOL( \
		"oce_sta_enable", \
		1, \
		"Enable/disable oce feature for STA")

/*
 * <ini>
 * oce_sap_enable - Enable/disable oce feature for SAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable oce feature for SAP
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_SAP CFG_INI_BOOL( \
		"oce_sap_enable", \
		1, \
		"Enable/disable oce feature for SAP")

/*
 * <ini>
 * oce_enable_rssi_assoc_reject - Enable/disable rssi based assoc rejection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable rssi based assoc rejection. If this is
 * disabled then OCE ini oce_sta_enable will also be disabled and OCE IE will
 * not be sent in frames.
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT CFG_INI_BOOL( \
		"oce_enable_rssi_assoc_reject", \
		1, \
		"Enable/disable rssi based assoc rejection")

/*
 * <ini>
 * oce_enable_probe_req_rate - Set probe request rate
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set probe request rate to 5.5Mbps as per OCE requirement
 * in 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_PROBE_REQ_RATE CFG_INI_BOOL( \
		"oce_enable_probe_req_rate", \
		1, \
		"Set probe request rate for OCE")

/*
 * <ini>
 * oce_enable_probe_resp_rate - Set probe response rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set probe response rate to 5.5Mbps as per OCE requirement
 * in 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_PROBE_RSP_RATE CFG_INI_BOOL( \
		"oce_enable_probe_resp_rate", \
		0, \
		"Set probe response rate for OCE")

/*
 * <ini>
 * oce_enable_beacon_rate - Set beacon rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set beacon rate to 5.5Mbps as per OCE requirement in
 * 2.4G band
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OCE_BEACON_RATE CFG_INI_BOOL( \
		"oce_enable_beacon_rate", \
		0, \
		"Set Beacon rate for OCE")
/*
 * <ini>
 * oce_enable_probe_req_deferral - Enable/disable probe request deferral
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable probe request deferral as per OCE spec
 *
 * Related: None
 *
 * Supported Feature: OCE
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PROBE_REQ_DEFERRAL CFG_INI_BOOL( \
		"oce_enable_probe_req_deferral", \
		1, \
		"Enable/disable probe request deferral for OCE")

/*
 * <ini>
 * oce_enable_fils_discovery_sap - Enable/disable fils discovery in sap mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable fils discovery in sap mode
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FILS_DISCOVERY_SAP CFG_INI_BOOL( \
		"oce_enable_fils_discovery_sap", \
		1, \
		"Enable/disable fils discovery in sap mode")

/*
 * <ini>
 * enable_esp_for_roam - Enable/disable esp feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable ESP(Estimated service parameters) IE
 * parsing and decides whether firmware will include this in its scoring algo.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ESP_FEATURE CFG_INI_BOOL( \
		"enable_esp_for_roam", \
		1, \
		"Enable/disable esp feature")

/*
 * <ini>
 * g_is_fils_enabled - Enable/Disable FILS support in driver
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable FILS support in driver
 * Driver will update config to supplicant based on this config.
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IS_FILS_ENABLED CFG_INI_BOOL( \
		"g_is_fils_enabled", \
		1, \
		"Enable/disable support")

#define CFG_OCE_ALL \
	CFG(CFG_ENABLE_BCAST_PROBE_RESP) \
	CFG(CFG_OCE_ENABLE_STA) \
	CFG(CFG_OCE_ENABLE_SAP) \
	CFG(CFG_OCE_ENABLE_RSSI_BASED_ASSOC_REJECT) \
	CFG(CFG_OCE_PROBE_REQ_RATE) \
	CFG(CFG_OCE_PROBE_RSP_RATE) \
	CFG(CFG_OCE_BEACON_RATE) \
	CFG(CFG_ENABLE_PROBE_REQ_DEFERRAL) \
	CFG(CFG_ENABLE_FILS_DISCOVERY_SAP) \
	CFG(CFG_ENABLE_ESP_FEATURE) \
	CFG(CFG_IS_FILS_ENABLED)
#endif /* __CFG_MLME_OCE_H */
