/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains roam specific SCORING related CFG/INI Items.
 */

#ifndef __CFG_MLME_ROAM_SCORING_H
#define __CFG_MLME_ROAM_SCORING_H

/*
 * <ini>
 * roam_score_delta_bitmap - bitmap to enable roam triggers on
 * which roam score delta is to be applied during roam candidate
 * selection
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * Bitmap value of the following roam triggers:
 * ROAM_TRIGGER_REASON_NONE       - B0,
 * ROAM_TRIGGER_REASON_PER        - B1,
 * ROAM_TRIGGER_REASON_BMISS      - B2,
 * ROAM_TRIGGER_REASON_LOW_RSSI   - B3,
 * ROAM_TRIGGER_REASON_HIGH_RSSI  - B4,
 * ROAM_TRIGGER_REASON_PERIODIC   - B5,
 * ROAM_TRIGGER_REASON_MAWC       - B6,
 * ROAM_TRIGGER_REASON_DENSE      - B7,
 * ROAM_TRIGGER_REASON_BACKGROUND - B8,
 * ROAM_TRIGGER_REASON_FORCED     - B9,
 * ROAM_TRIGGER_REASON_BTM        - B10,
 * ROAM_TRIGGER_REASON_UNIT_TEST  - B11,
 * ROAM_TRIGGER_REASON_BSS_LOAD   - B12
 * ROAM_TRIGGER_REASON_DISASSOC   - B13
 * ROAM_TRIGGER_REASON_IDLE_ROAM  - B14
 *
 * When the bit corresponding to a particular roam trigger reason
 * is set, the value of "roam_score_delta" is expected over the
 * roam score of the current connected AP, for that triggered roam
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCORE_DELTA_TRIGGER_BITMAP CFG_INI_UINT( \
			"roam_score_delta_bitmap", \
			0, \
			0xFFFFFFFF, \
			0xFFFFFFFF, \
			CFG_VALUE_OR_DEFAULT, \
			"Bitmap for various roam triggers")

/*
 * <ini>
 * roam_score_delta - Percentage increment in roam score value
 * that is expected from a roaming candidate AP.
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to provide the percentage increment value over roam
 * score for the candidate APs so that they can be preferred over current
 * AP for roaming.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCORE_DELTA CFG_INI_UINT( \
			"roam_score_delta", \
			0, \
			100, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"candidate AP's percentage roam score delta")

/*
 * <ini>
 * min_roam_score_delta - Difference of roam score values between connected
 * AP and roam candidate AP.
 * @Min: 0
 * @Max: 10000
 * @Default: 0
 *
 * This ini is used during CU and low rssi based roam triggers, consider
 * AP as roam candidate only if its roam score is better than connected
 * AP score by at least min_roam_score_delta.
 * If user configured "roam_score_delta" and "min_roam_score_delta" both,
 * then firmware selects roam candidate AP by considering values of both
 * INIs.
 * Example: If DUT is connected with AP1 and roam candidate AP2 has roam
 * score greater than roam_score_delta and min_roam_score_delta then only
 * firmware will trigger roaming to AP2.
 *
 * Related: roam_score_delta
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_CAND_MIN_ROAM_SCORE_DELTA CFG_INI_UINT( \
			"min_roam_score_delta", \
			0, \
			10000, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Diff between connected AP's and candidate AP's roam score")

/*
 * <ini>
 * enable_scoring_for_roam - enable/disable scoring logic in FW for candidate
 * selection during roaming
 *
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable scoring logic in FW for candidate
 * selection during roaming.
 *
 * Supported Feature: STA Candidate selection by FW during roaming based on
 * scoring logic.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SCORING_FOR_ROAM CFG_INI_BOOL( \
		"enable_scoring_for_roam", \
		1, \
		"Enable Scoring for Roam")

/*
 * <cfg>
 * apsd_enabled - Enable automatic power save delivery
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Supported Feature: Power save
 *
 * Usage: Internal
 *
 * </cfg>
 */
#define CFG_APSD_ENABLED CFG_BOOL( \
		"apsd_enabled", \
		0, \
		"Enable APSD")

/*
 * <ini>
 * candidate_min_rssi_for_disconnect - Candidate AP minimum RSSI in
 * idle roam trigger(in dBm).
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * Minimum RSSI value of the candidate AP to consider it as candidate for
 * roaming when roam trigger is Deauthentication/Disconnection from current
 * AP. This value will be sent to firmware over the WMI_ROAM_AP_PROFILE
 * wmi command in the roam_min_rssi_param_list tlv.
 *
 * Related: enable_idle_roam.
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI CFG_INI_INT( \
		"candidate_min_rssi_for_disconnect", \
		-120, \
		0, \
		-75, \
		CFG_VALUE_OR_DEFAULT, \
		"Minimum RSSI of candidate AP for Disconnect roam trigger")

/*
 * <ini>
 * candidate_min_rssi_for_beacon_miss - Candidate AP minimum RSSI for beacon
 * miss roam trigger (in dBm)
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * Minimum RSSI value of the candidate AP to consider it as candidate for
 * roaming when roam trigger is disconnection from current AP due to beacon
 * miss. This value will be sent to firmware over the WMI_ROAM_AP_PROFILE
 * wmi command in the roam_min_rssi_param_list tlv.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BMISS_ROAM_MIN_RSSI CFG_INI_INT( \
	"candidate_min_rssi_for_beacon_miss", \
	-120, \
	0, \
	-75, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum RSSI of candidate AP for Bmiss roam trigger")

/*
 * <ini>
 * min_rssi_for_2g_to_5g_roam - Candidate AP minimum RSSI for
 * 2G to 5G roam trigger (in dBm)
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * Minimum RSSI value of the candidate AP to consider it as candidate
 * for 2G to 5G roam.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_2G_TO_5G_ROAM_MIN_RSSI CFG_INI_INT( \
	"min_rssi_for_2g_to_5g_roam", \
	-120, \
	0, \
	-70, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum RSSI of candidate AP for 2G to 5G roam trigger")

/*
 * <ini>
 * idle_roam_score_delta - Roam score delta value in percentage for idle roam.
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to configure the minimum change in roam score
 * value of the AP to consider it as candidate for
 * roaming when roam trigger is due to idle state of sta.
 * This value will be sent to firmware over the WMI_ROAM_AP_PROFILE wmi
 * command in the roam_score_delta_param_list tlv.
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IDLE_ROAM_SCORE_DELTA CFG_INI_UINT( \
		"idle_roam_score_delta", \
		0, \
		100, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Roam score delta for Idle roam trigger")

/*
 * <ini>
 * btm_roam_score_delta - Roam score delta value in percentage for BTM triggered
 * roaming.
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to configure the minimum change in roam score
 * value of the AP to consider it as candidate when the sta is disconnected
 * from the current AP due to BTM kickout.
 * This value will be sent to firmware over the WMI_ROAM_AP_PROFILE wmi
 * command in the roam_score_delta_param_list tlv.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_ROAM_SCORE_DELTA CFG_INI_UINT( \
	"btm_roam_score_delta", \
	0, \
	100, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Roam score delta for BTM roam trigger")

#define CFG_ROAM_SCORING_ALL \
	CFG(CFG_ROAM_SCORE_DELTA_TRIGGER_BITMAP) \
	CFG(CFG_ROAM_SCORE_DELTA) \
	CFG(CFG_CAND_MIN_ROAM_SCORE_DELTA) \
	CFG(CFG_ENABLE_SCORING_FOR_ROAM) \
	CFG(CFG_APSD_ENABLED) \
	CFG(CFG_DISCONNECT_ROAM_TRIGGER_MIN_RSSI) \
	CFG(CFG_BMISS_ROAM_MIN_RSSI) \
	CFG(CFG_2G_TO_5G_ROAM_MIN_RSSI) \
	CFG(CFG_IDLE_ROAM_SCORE_DELTA) \
	CFG(CFG_BTM_ROAM_SCORE_DELTA)

#endif /* __CFG_MLME_ROAM_SCORING_H */
