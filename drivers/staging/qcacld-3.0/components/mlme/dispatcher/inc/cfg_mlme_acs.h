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
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_ACS_H
#define __CFG_MLME_ACS_H

/*
 * <ini>
 * acs_with_more_param- Enable acs calculation with more param.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable acs calculation with more param.
 *
 * Related: NA
 *
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ACS_WITH_MORE_PARAM CFG_INI_BOOL( \
		"acs_with_more_param", \
		0, \
		"Enable ACS with more param")

/*
 * <ini>
 * AutoChannelSelectWeight - ACS channel weight
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x000000FF
 *
 * This ini is used to adjust weight of factors in
 * acs algorithm.
 *
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * bits 0-3:   rssi weight
 * bits 4-7:   bss count weight
 * bits 8-11:  noise floor weight
 * bits 12-15: channel free weight
 * bits 16-19: tx power range weight
 * bits 20-23: tx power throughput weight
 * bits 24-31: reserved
 *
 * </ini>
 */

#define CFG_AUTO_CHANNEL_SELECT_WEIGHT CFG_INI_UINT( \
		"AutoChannelSelectWeight", \
		0, \
		0xFFFFFFFF, \
		0x000000FF, \
		CFG_VALUE_OR_DEFAULT, \
		"Adjust weight factor in ACS")

/*
 * <ini>
 * gvendor_acs_support - vendor based channel selection manager
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Enabling this parameter will force driver to use user application based
 * channel selection algo instead of driver based auto channel selection
 * logic.
 *
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_USER_AUTO_CHANNEL_SELECTION CFG_INI_BOOL( \
		"gvendor_acs_support", \
		0, \
		"Vendor channel selection manager")

/*
 * <ini>
 * gacs_support_for_dfs_lte_coex - acs support for lte coex and dfs event
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Enabling this parameter will force driver to use user application based
 * channel selection algo for channel selection in case of dfs and lte
 * coex event.
 *
 * Supported Feature: ACS
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_USER_ACS_DFS_LTE CFG_INI_BOOL( \
		"gacs_support_for_dfs_lte_coex", \
		0, \
		"Acs support for lte coex and dfs")

/*
 * <ini>
 * acs_policy - External ACS policy control
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Values are per enum hdd_external_acs_policy.
 *
 * This ini is used to control the external ACS policy.
 *
 * 0 -Preferable for ACS to select a
 *    channel with non-zero pcl weight.
 * 1 -Mandatory for ACS to select a
 *    channel with non-zero pcl weight.
 *
 * Related: None
 *
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_EXTERNAL_ACS_POLICY CFG_INI_BOOL( \
		"acs_policy", \
		1, \
		"External ACS Policy Control")

#define ACS_WEIGHT_MAX_STR_LEN            500

/*
 * <ini>
 * normalize_acs_weight - Used to control the ACS channel weightage.
 *
 * This ini is used to specify the weight percentage of the channel. Channel
 * weights can be controlled by user to prioritize or de-prioritize channels.
 *
 * Related: ACS
 *
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NORMALIZE_ACS_WEIGHT CFG_INI_STRING( \
		"normalize_acs_weight", \
		0, \
		ACS_WEIGHT_MAX_STR_LEN, \
		"5945-7125=0, 5975=100, 6055=100, 6135=100, 6215=100, 6295=100, 6375=100, 6615=100, 6695=100, 6775=100, 6855=100", \
		"Used to specify the channel weights")

/*
 * <ini>
 * force_start_sap- Enable the SAP even if no channel is suitable for SAP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable the SAP even if no channel is found suitable
 * for SAP by ACS.
 *
 * Related: NA
 *
 * Supported Feature: ACS
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ACS_FORCE_START_SAP CFG_INI_BOOL( \
		"force_start_sap", \
		0, \
		"Force start SAP")

/*
 * <ini>
 * np_chan_weight - chan weightage for non preferred channels
 * @Min: 0x00000000
 * @Max: 0x64646464
 * @Default: 0x00000000
 *
 * This INI give percentage value of weights to be considered in the ACS algo
 * for the non preferred channels. the distribution of the channel type is:-
 * Example:- If the percentage of lets say DFS channels is set to 50%, and
 * the weight comes out to be x, then we would increase the weight of DFS
 * channels by 50% ( 100 - y% set in INI), so that it gets de-prioritized in
 * the ACS sorted channel list, the lesser the weight, the better the channel.
 * So the channel with more weight is less likely to be selected. So by default
 * the np chan weightage for DFS is set to 0, that is it will be assigned max
 * weightage, so no probality of getting selected, as for standlaone, DFS is not
 * recommended (it takes 60 sec/10min to start depending upon channel type).
 *
 * Indexes are defined in this way.
 *     0 Index (BITS 0-7): DFS - Def 1%
 *     1 Index (BITS 8-15): Reserved
 *     2 Index (BITS 16-23): Reserved
 *     3 Index (BITS 24-31): Reserved
 * These percentage values are stored in HEX. Max can be 0x64
 * Supported Feature: ACS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ACS_NP_CHAN_WEIGHT CFG_INI_UINT( \
		"np_chan_weight", \
		0x00000000, \
		0x64646464, \
		0x00000001, \
		CFG_VALUE_OR_DEFAULT, \
		"np chan weight")

#define CFG_ACS_ALL \
	CFG(CFG_ACS_WITH_MORE_PARAM) \
	CFG(CFG_AUTO_CHANNEL_SELECT_WEIGHT) \
	CFG(CFG_USER_AUTO_CHANNEL_SELECTION) \
	CFG(CFG_USER_ACS_DFS_LTE) \
	CFG(CFG_EXTERNAL_ACS_POLICY) \
	CFG(CFG_NORMALIZE_ACS_WEIGHT) \
	CFG(CFG_ACS_FORCE_START_SAP) \
	CFG(CFG_ACS_NP_CHAN_WEIGHT)

#endif /* __CFG_MLME_ACS_H */
