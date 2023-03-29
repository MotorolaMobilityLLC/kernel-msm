/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains centralized definitions of QOS related
 * converged configurations.
 */

#ifndef __CFG_MLME_MBO_H
#define __CFG_MLME_MBO_H

/*
 * <ini>
 * g_mbo_candidate_rssi_thres - Candidate AP's minimum RSSI to accept
 * @Min: -120
 * @Max: 0
 * @Default: -72
 *
 * This ini specifies the minimum RSSI value a candidate should have to accept
 * it as a target for transition.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CANDIDATE_RSSI_THRESHOLD CFG_INI_INT( \
			"g_mbo_candidate_rssi_thres", \
			-120, \
			0, \
			-72, \
			CFG_VALUE_OR_DEFAULT, \
			"candidate AP rssi threshold")
/*
 * <ini>
 * g_mbo_current_rssi_thres - Connected AP's RSSI threshold to consider a
 * transition
 * @Min: -120
 * @Max: 0
 * @Default: -65
 *
 * This ini is used to configure connected AP's RSSI threshold value to consider
 * a transition.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CURRENT_RSSI_THRESHOLD CFG_INI_INT( \
			"g_mbo_current_rssi_thres", \
			-120, \
			0, \
			-65, \
			CFG_VALUE_OR_DEFAULT, \
			"current AP rssi threshold")

/*
 * <ini>
 * g_mbo_current_rssi_mcc_thres - connected AP's RSSI threshold value to prefer
 * against a MCC
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure connected AP's minimum RSSI threshold that is
 * preferred against a MCC case, if the candidate can cause MCC.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CUR_RSSI_MCC_THRESHOLD CFG_INI_INT( \
		"g_mbo_current_rssi_mcc_thres", \
		-120, \
		0, \
		-75, \
		CFG_VALUE_OR_DEFAULT, \
		"current AP mcc rssi threshold")

/*
 * <ini>
 * g_mbo_candidate_rssi_btc_thres -  Candidate AP's minimum RSSI threshold to
 * prefer it even in case of BT coex
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * This ini is used to configure candidate AP's minimum RSSI threshold to prefer
 * it for transition even in case of BT coex.
 *
 * Related: N/A
 *
 * Supported Feature: MBO
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MBO_CAND_RSSI_BTC_THRESHOLD CFG_INI_INT( \
		"g_mbo_candidate_rssi_btc_thres", \
		-120, \
		0, \
		-70, \
		CFG_VALUE_OR_DEFAULT, \
		"candidate AP rssi threshold")

#define CFG_MBO_ALL \
	CFG(CFG_MBO_CANDIDATE_RSSI_THRESHOLD) \
	CFG(CFG_MBO_CURRENT_RSSI_THRESHOLD) \
	CFG(CFG_MBO_CUR_RSSI_MCC_THRESHOLD) \
	CFG(CFG_MBO_CAND_RSSI_BTC_THRESHOLD)

#endif /* __CFG_MLME_MBO_H */
