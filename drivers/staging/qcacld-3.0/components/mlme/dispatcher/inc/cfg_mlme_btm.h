/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains MLME BTM related CFG/INI Items.
 */

#ifndef CFG_MLME_BTM_H_
#define CFG_MLME_BTM_H_

/*
 * <ini>
 * prefer_btm_query - Prefer btm query over 11k neighbor report
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable the STA to send BTM query instead of
 * 11k neighbor report.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PREFER_BTM_QUERY CFG_INI_BOOL( \
			"prefer_btm_query", \
			1, \
			"prefer btm query over 11k neighbor report")

/*
 * <ini>
 * prefer_roam_score_for_candidate_selection - choose to sort the candidates on
 * roam score or preferred AP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable the the firmware to sort the candidates
 * based on the roam score rather than selecting APs as per the order
 * of the APs sent by the connected AP.
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BTM_ABRIDGE CFG_INI_BOOL( \
			"prefer_roam_score_for_candidate_selection", \
			1, \
			"sort candidate based on roam score")

/*
 * <ini>
 * btm_offload_config - Configure BTM
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x000001c1
 *
 * This ini is used to configure BTM
 *
 * Bit 0: Enable/Disable the BTM offload. Set this to 1 will
 * enable and 0 will disable BTM offload.
 *
 * BIT 2, 1: Action on non matching candidate with cache. If a BTM request
 * is received from AP then the candidate AP's may/may-not be present in
 * the firmware scan cache . Based on below config firmware will decide
 * whether to forward BTM frame to host or consume with firmware and proceed
 * with Roaming to candidate AP.
 * 00 scan and consume
 * 01 no scan and forward to host
 * 10, 11 reserved
 *
 * BIT 5, 4, 3: Roaming handoff decisions on multiple candidates match
 * for unsolicited BTM request.
 * 000 match if exact BSSIDs are found
 * 001 match if at least one top priority BSSID only
 * 010, 011, 100, 101, 110, 111 reserved
 * In case of solicited request intersection of Roam cache/candidates
 * with candidates in BTM request is taken into consideration.
 *
 * BIT 6: Set this to 1 will send BTM query frame and 0 not sent.
 *
 * BIT 7: Roam BTM candidates based on the roam score instead of BTM preferred
 * value
 *
 * BIT 8: BTM query preference over 11k neighbor report request
 *
 * BIT 9: BTM query with candidate list
 *
 * BIT 10: When this bit is set, Firmware will forward BTM Request Frame to
 * driver when the frame contains MBO assoc retry attribute. Driver will send
 * this frame to supplicant and supplicant will use the frame info for
 * blacklisting the AP so for the next connection framework will avoid this AP.
 *
 * BIT 11-31: Reserved
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_ENABLE CFG_INI_UINT( \
	"btm_offload_config", \
	0x00000000, \
	0xffffffff, \
	0x000001c1, \
	CFG_VALUE_OR_DEFAULT, \
	"configure btm offload")

/*
 * <ini>
 * btm_solicited_timeout - timeout value for waiting BTM request
 * @Min: 1
 * @Max: 10000
 * @Default: 100
 *
 * This ini is used to configure timeout value for waiting BTM request.
 * Unit: millionsecond
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_SOLICITED_TIMEOUT CFG_INI_UINT( \
	"btm_solicited_timeout", \
	1, \
	10000, \
	100, \
	CFG_VALUE_OR_DEFAULT, \
	"configure timeout value for waiting BTM request")

/*
 * <ini>
 * btm_max_attempt_cnt - Maximum attempt for sending BTM query to ESS
 * @Min: 1
 * @Max: 0xFFFFFFFF
 * @Default: 3
 *
 * This ini is used to configure maximum attempt for sending BTM query to ESS.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_MAX_ATTEMPT_CNT CFG_INI_UINT( \
	"btm_max_attempt_cnt", \
	1, \
	0xFFFFFFFF, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"configure maximum attempt for sending BTM query to ESS")

/*
 * <ini>
 * btm_sticky_time - Stick time after roaming to new AP by BTM
 * @Min: 0
 * @Max: 0x0000FFFF
 * @Default: 0
 *
 * This ini is used to configure Stick time after roaming to new AP by BTM.
 * Unit: seconds
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_STICKY_TIME CFG_INI_UINT( \
	"btm_sticky_time", \
	0, \
	0x0000FFFF, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"configure Stick time after roaming to new AP by BTM")

/*
 * <ini>
 * roam_candidate_validity_timer - roam cache entries validity timer
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 0xffffffff
 *
 * This value is the timeout values for the cached roam candidate
 * entries in firmware. If this value is 0, then that entry is not
 * valid
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_VALIDITY_TIMER CFG_INI_UINT( \
			"roam_candidate_validity_timer", \
			0, \
			0xffffffff, \
			0xffffffff, \
			CFG_VALUE_OR_DEFAULT, \
			"BTM validity timer")

/*
 * <ini>
 * btm_disassoc_timer_threshold - Disassociation timer threshold to wait
 * after which the full scan for roaming can be started after the AP has sent
 * the disassoc imminent
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 10000
 *
 * When AP sends, BTM request with disassoc imminent bit set, the STA should
 * roam to a new AP within the disassc timeout provided by the ap. If the Roam
 * scan period is less than the disassoc timeout value, then instead of
 * triggering the roam scan immediately, STA can wait for this
 * btm_disassoc_timer_threshold and then start roaming.
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_DISASSOC_TIMER_THRESHOLD CFG_INI_UINT( \
			"btm_disassoc_timer_threshold", \
			0, \
			0xffffffff, \
			10000, \
			CFG_VALUE_OR_DEFAULT, \
			"BTM disassociation timer threshold")

/*
 * <ini>
 * btm_query_bitmask - To send BTM query with candidate list on various roam
 * scans reasons
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x8
 *
 * This new ini is introduced to configure the bitmask for various roam scan
 * reasons. Fw sends "BTM query with preferred candidate list" only for those
 * roam scans which are enable through this bitmask.

 * For Example:
 * Bitmask : 0x8 (LOW_RSSI) refer enum WMI_ROAM_TRIGGER_REASON_ID
 * Bitmask : 0xDA (PER, LOW_RSSI, HIGH_RSSI, MAWC, DENSE)
 * refer enum WMI_ROAM_TRIGGER_REASON_ID
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTM_QUERY_BITMASK CFG_INI_UINT( \
			"btm_query_bitmask", \
			0, \
			0xFFFFFFFF, \
			0x8, \
			CFG_VALUE_OR_DEFAULT, \
			"btm query with candidate list bitmask")

/*
 * <ini>
 * minimum_btm_candidate_score - Consider the AP as roam candidate only if
 * its score is greater than minimum_btm_candidate_score.
 * @Min: 0
 * @Max: 10000
 * @Default: 2600
 *
 * This ini is applicable only for candidate selection during BTM roam trigger.
 * For this roam_score_delta_bitmap bit 10 should be set to 1.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MIN_BTM_CANDIDATE_SCORE CFG_INI_UINT( \
	"minimum_btm_candidate_score", \
	0, \
	10000, \
	2600, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum BTM candidate score")

#define CFG_BTM_ALL \
	CFG(CFG_PREFER_BTM_QUERY) \
	CFG(CFG_ENABLE_BTM_ABRIDGE) \
	CFG(CFG_BTM_ENABLE) \
	CFG(CFG_BTM_SOLICITED_TIMEOUT) \
	CFG(CFG_BTM_MAX_ATTEMPT_CNT) \
	CFG(CFG_BTM_STICKY_TIME) \
	CFG(CFG_BTM_VALIDITY_TIMER) \
	CFG(CFG_BTM_DISASSOC_TIMER_THRESHOLD) \
	CFG(CFG_BTM_QUERY_BITMASK) \
	CFG(CFG_MIN_BTM_CANDIDATE_SCORE)

#endif /* CFG_MLME_BTM_H_ */
