/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains ini params for blacklist mgr component
 */

#ifndef __CFG_BLM_H_
#define __CFG_BLM_H_

#ifdef FEATURE_BLACKLIST_MGR

/*
 * <ini>
 * avoid_list_expiry_time - Config Param to move AP from avoid to monitor list.
 * @Min: 1 minutes
 * @Max: 300 minutes
 * @Default: 5 minutes
 *
 * This ini is used to specify the time after which the BSSID which is in the
 * avoid list should be moved to monitor list, assuming that the AP or the
 * gateway with which the data stall happenend might have recovered, and now
 * the STA can give another chance to connect to the AP.
 *
 * Supported Feature: Data Stall Recovery
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_AVOID_LIST_EXPIRY_TIME CFG_INI_UINT( \
				"avoid_list_expiry_time", \
				1, \
				300, \
				5, \
				CFG_VALUE_OR_DEFAULT, \
				"avoid list expiry")

/*
 * <ini>
 * bad_bssid_counter_thresh - Threshold to move the Ap from avoid to blacklist.
 * @Min: 2
 * @Max: 10
 * @Default: 3
 *
 * This ini is used to specify the threshld after which the BSSID which is in
 * the avoid list should be moved to black list, assuming that the AP or the
 * gateway with which the data stall happenend has no recovered, and now
 * the STA got the NUD failure again with the BSSID
 *
 * Supported Feature: Data Stall Recovery
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAD_BSSID_COUNTER_THRESHOLD CFG_INI_UINT( \
				"bad_bssid_counter_thresh", \
				2, \
				10, \
				3, \
				CFG_VALUE_OR_DEFAULT, \
				"bad bssid counter thresh")

/*
 * <ini>
 * black_list_expiry_time - Config Param to move AP from blacklist to monitor
 * list.
 * @Min: 1 minutes
 * @Max: 600 minutes
 * @Default: 10 minutes
 *
 * This ini is used to specify the time after which the BSSID which is in the
 * black list should be moved to monitor list, assuming that the AP or the
 * gateway with which the data stall happenend might have recovered, and now
 * the STA can give another chance to connect to the AP.
 *
 * Supported Feature: Data Stall Recovery
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BLACK_LIST_EXPIRY_TIME CFG_INI_UINT( \
				"black_list_expiry_time", \
				1, \
				600, \
				10, \
				CFG_VALUE_OR_DEFAULT, \
				"black list expiry")

/*
 * <ini>
 * bad_bssid_reset_time - Config Param to specify time after which AP would be
 * removed from monitor/avoid when connected.
 * @Min: 30 seconds
 * @Max: 1 minute
 * @Default: 30 seconds
 *
 * This ini is used to specify the time after which the BSSID which is in the
 * avoid or monitor list should be removed from the respective list, if the
 * data stall has not happened till the mentioned time after connection to the
 * AP. That means that the AP has recovered from the previous state where
 * data stall was observed with it, and was moved to avoid list.
 *
 * Supported Feature: Data Stall Recovery
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BAD_BSSID_RESET_TIME CFG_INI_UINT( \
				"bad_bssid_reset_time", \
				30, \
				60, \
				30, \
				CFG_VALUE_OR_DEFAULT, \
				"bad bssid reset time")

/*
 * <ini>
 * delta_rssi - RSSI threshold value, only when AP rssi improves
 * by threshold value entry would be removed from blacklist manager and assoc
 * req would be sent by FW.
 * @Min: 0
 * @Max: 10
 * @Default: 5
 *
 * This ini is used to specify the rssi threshold value, after rssi improves
 * by threshold the BSSID which is in the blacklist manager list should be
 * removed from the respective list.
 *
 * Supported Feature: Customer requirement
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BLACKLIST_RSSI_THRESHOLD CFG_INI_INT( \
			"delta_rssi", \
			0, \
			10, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"Configure delta RSSI")

#define CFG_BLACKLIST_MGR_ALL \
	CFG(CFG_AVOID_LIST_EXPIRY_TIME) \
	CFG(CFG_BAD_BSSID_COUNTER_THRESHOLD) \
	CFG(CFG_BLACK_LIST_EXPIRY_TIME) \
	CFG(CFG_BAD_BSSID_RESET_TIME) \
	CFG(CFG_BLACKLIST_RSSI_THRESHOLD)

#else

#define CFG_BLACKLIST_MGR_ALL

#endif /* FEATURE_BLACKLIST_MGR */

#endif /* __CFG_BLACKLIST_MGR */
