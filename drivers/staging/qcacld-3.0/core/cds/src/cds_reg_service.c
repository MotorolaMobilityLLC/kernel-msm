/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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

/*============================================================================
   FILE:         cds_reg_service.c
   OVERVIEW:     This source file contains definitions for CDS regulatory APIs
   DEPENDENCIES: None
   ============================================================================*/

#include "qdf_types.h"
#include "qdf_trace.h"
#include <wlan_objmgr_pdev_obj.h>
#include "wlan_reg_services_api.h"
#include "cds_reg_service.h"
#include "cds_ieee80211_common_i.h"
#include "cds_config.h"
#include "cds_utils.h"
#include "wlan_reg_services_api.h"

uint32_t cds_get_vendor_reg_flags(struct wlan_objmgr_pdev *pdev,
		qdf_freq_t freq, uint16_t bandwidth,
		bool is_ht_enabled, bool is_vht_enabled,
		uint8_t sub_20_channel_width)
{
	uint32_t flags = 0;
	enum channel_state state;
	struct ch_params ch_params;
	qdf_freq_t sec_freq;

	state = wlan_reg_get_channel_state_for_freq(pdev, freq);
	if (state == CHANNEL_STATE_INVALID)
		return flags;
	if (state == CHANNEL_STATE_DFS) {
		flags |= IEEE80211_CHAN_PASSIVE;
	}
	if (state == CHANNEL_STATE_DISABLE)
		flags |= IEEE80211_CHAN_BLOCKED;

	if (wlan_reg_is_24ghz_ch_freq(freq)) {
		if ((bandwidth == CH_WIDTH_80P80MHZ) ||
		    (bandwidth == CH_WIDTH_160MHZ) ||
		    (bandwidth == CH_WIDTH_80MHZ)) {
			bandwidth = CH_WIDTH_40MHZ;
		}
		flags |= IEEE80211_CHAN_2GHZ;
	} else
		flags |= IEEE80211_CHAN_5GHZ;

	switch (bandwidth) {
	case CH_WIDTH_80P80MHZ:
		if (wlan_reg_get_5g_bonded_channel_state_for_freq(pdev, freq,
								  bandwidth) !=
		    CHANNEL_STATE_INVALID) {
			if (is_vht_enabled)
				flags |= IEEE80211_CHAN_VHT80_80;
		}
		bandwidth = CH_WIDTH_160MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_160MHZ:
		if (wlan_reg_get_5g_bonded_channel_state_for_freq(pdev, freq,
								  bandwidth) !=
		    CHANNEL_STATE_INVALID) {
			if (is_vht_enabled)
				flags |= IEEE80211_CHAN_VHT160;
		}
		bandwidth = CH_WIDTH_80MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_80MHZ:
		if (wlan_reg_get_5g_bonded_channel_state_for_freq(pdev, freq,
								  bandwidth) !=
		    CHANNEL_STATE_INVALID) {
			if (is_vht_enabled)
				flags |= IEEE80211_CHAN_VHT80;
		}
		bandwidth = CH_WIDTH_40MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_40MHZ:
		qdf_mem_zero(&ch_params, sizeof(ch_params));
		ch_params.ch_width = bandwidth;
		wlan_reg_set_channel_params_for_freq(pdev, freq, 0, &ch_params);

		if (ch_params.sec_ch_offset == LOW_PRIMARY_CH)
			sec_freq = freq + 20;
		else if (ch_params.sec_ch_offset == HIGH_PRIMARY_CH)
			sec_freq = freq - 20;
		else
			sec_freq = 0;

		if (wlan_reg_get_bonded_channel_state_for_freq(pdev, freq,
							       bandwidth,
							       sec_freq) !=
		    CHANNEL_STATE_INVALID) {
			if (ch_params.sec_ch_offset == LOW_PRIMARY_CH) {
				flags |= IEEE80211_CHAN_HT40PLUS;
				if (is_vht_enabled)
					flags |= IEEE80211_CHAN_VHT40PLUS;
			} else if (ch_params.sec_ch_offset ==
					HIGH_PRIMARY_CH) {
				flags |= IEEE80211_CHAN_HT40MINUS;
				if (is_vht_enabled)
					flags |= IEEE80211_CHAN_VHT40MINUS;
			}
		}
		bandwidth = CH_WIDTH_20MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_20MHZ:
		if (is_vht_enabled)
			flags |= IEEE80211_CHAN_VHT20;
		if (is_ht_enabled)
			flags |= IEEE80211_CHAN_HT20;
		bandwidth = CH_WIDTH_10MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_10MHZ:
		if (wlan_reg_get_bonded_channel_state_for_freq(pdev, freq,
							       bandwidth,
							       0) !=
		     CHANNEL_STATE_INVALID &&
		     sub_20_channel_width == WLAN_SUB_20_CH_WIDTH_10)
			flags |= IEEE80211_CHAN_HALF;
		bandwidth = CH_WIDTH_5MHZ;
	/* FALLTHROUGH */
	case CH_WIDTH_5MHZ:
		if (wlan_reg_get_bonded_channel_state_for_freq(pdev, freq,
							       bandwidth,
							       0) !=
		    CHANNEL_STATE_INVALID &&
		    sub_20_channel_width == WLAN_SUB_20_CH_WIDTH_5)
			flags |= IEEE80211_CHAN_QUARTER;
		break;
	default:
		cds_info("invalid channel width value %d", bandwidth);
	}

	return flags;
}

