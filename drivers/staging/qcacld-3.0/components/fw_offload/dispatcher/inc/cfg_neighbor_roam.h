/*
 * Copyright (c) 2012 - 2018 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_NR_H
#define __CFG_NR_H

/*
 * <ini>
 * 11k_offload_enable_bitmask - 11K offload bitmask feature control
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Disabled when 0 and enabled when 1
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_11K_ENABLE_BITMASK CFG_INI_BOOL( \
		"11k_offload_enable_bitmask", \
		1, \
		"11K offload bitmask feature control")

/*
 * <ini>
 * nr_offload_params_bitmask - bitmask to specify which of the
 * neighbor report offload params are valid in the ini
 * frame
 * @Min: 0
 * @Max: 63
 * @Default: 63
 *
 * This ini specifies which of the neighbor report offload params are valid
 * and should be considered by the FW. The bitmask is as follows
 * B0: nr_offload_time_offset
 * B1: nr_offload_low_rssi_offset
 * B2: nr_offload_bmiss_count_trigger
 * B3: nr_offload_per_threshold_offset
 * B4: nr_offload_cache_timeout
 * B5: nr_offload_max_req_cap
 * B6-B7: Reserved
 *
 * Related : 11k_offload_enable_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK CFG_INI_UINT( \
			"nr_offload_params_bitmask", \
			0, \
			63, \
			63, \
			CFG_VALUE_OR_DEFAULT, \
			"Neighbor report offload params validity bitmask")

#define OFFLOAD_11K_BITMASK_NEIGHBOR_REPORT_REQUEST  0x1

/*
 * <ini>
 * nr_offload_time_offset - time interval in seconds after the
 * neighbor report offload command to send the first neighbor report request
 * frame
 * @Min: 0
 * @Max: 3600
 * @Default: 30
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET CFG_INI_UINT( \
		"nr_offload_time_offset", \
		0, \
		3600, \
		30, \
		CFG_VALUE_OR_DEFAULT, \
		"Neighbor report time offset")

/*
 * <ini>
 * nr_offload_low_rssi_offset - offset from the roam RSSI threshold
 * to trigger the neighbor report request frame (in dBm)
 * @Min: 4
 * @Max: 10
 * @Default: 4
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET CFG_INI_UINT( \
		"nr_offload_low_rssi_offset", \
		4, \
		10, \
		4, \
		CFG_VALUE_OR_DEFAULT, \
		"Neighbor report low RSSI offset")

/*
 * <ini>
 * nr_offload_bmiss_count_trigger - Number of beacon miss events to
 * trigger a neighbor report request frame
 * @Min: 1
 * @Max: 5
 * @Default: 1
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER CFG_INI_UINT( \
		"nr_offload_bmiss_count_trigger", \
		1, \
		5, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"Beacon miss count trigger for neighbor report req frame")

/*
 * <ini>
 * nr_offload_per_threshold_offset - offset from PER threshold to
 * trigger a neighbor report request frame (in %)
 * @Min: 5
 * @Max: 20
 * @Default: 5
 *
 * This ini is used to set the neighbor report offload parameter:
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET CFG_INI_UINT( \
		"nr_offload_per_threshold_offset", \
		5, \
		20, \
		5, \
		CFG_VALUE_OR_DEFAULT, \
		"PER threshold offset to trigger neighbor report req frame")

/*
 * <ini>
 * nr_offload_cache_timeout - time in seconds after which the
 * neighbor report cache is marked as timed out and any of the triggers would
 * cause a neighbor report request frame to be sent.
 * @Min: 5
 * @Max: 86400
 * @Default: 1200
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT CFG_INI_UINT( \
		"nr_offload_cache_timeout", \
		5, \
		86400, \
		1200, \
		CFG_VALUE_OR_DEFAULT, \
		"Neighbor report cache timeout to trigger report req frame")

/*
 * <ini>
 * nr_offload_max_req_cap - Max number of neighbor
 * report requests that can be sent to a connected peer in the current session.
 * This counter is reset once a successful roam happens or at cache timeout
 * @Min: 3
 * @Max: 300
 * @Default: 3
 *
 * Related : nr_offload_params_bitmask
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP CFG_INI_UINT( \
		"nr_offload_max_req_cap", \
		3, \
		300, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Max neighbor report request to be sent to connected peer")

#define CFG_11K_ALL \
	CFG(CFG_OFFLOAD_11K_ENABLE_BITMASK) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_PARAMS_BITMASK) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_TIME_OFFSET) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_LOW_RSSI_OFFSET) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_BMISS_COUNT_TRIGGER) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_PER_THRESHOLD_OFFSET) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_CACHE_TIMEOUT) \
	CFG(CFG_OFFLOAD_NEIGHBOR_REPORT_MAX_REQ_CAP)

#endif
