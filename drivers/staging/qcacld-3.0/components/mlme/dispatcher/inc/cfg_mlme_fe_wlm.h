/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_FE_WLM_H
#define __CFG_MLME_FE_WLM_H

/*
 * |31  18|  17  |  16    |15        8|7    2|    1     |     0     |
 * +------+------+--------+-----------+------+----------+-----------+
 * | RSVD | HBB  | PM-QOS |  RSVD     | RSVD | REO Ring | RX Thread |
 * +------+------+--------+-----------+------+----------+-----------+
 * |       common         |  TX Path  |        RX Path              |
 *
 * bit 0-7: Rx path related optimization
 * bit 0: disable rx_thread for vdev
 * bit 1: Reduce REO ring timer threshold to 32us(applicable for all REO ring)
 * bit 2-7: Reserved
 * bit 8-15: Tx path related optimization
 * bit 8-15: Reserved
 * bit 16-31: common changes
 * bit 16: Request for pm_qos vote
 * bit 17: Request for high ddr bus bandwidth
 */

#define WLM_HOST_RX_THREAD_FLAG         (1 << 0)
#define WLM_HOST_REO_RING_FLAG          (1 << 1)
#define WLM_HOST_PM_QOS_FLAG            (1 << 16)
#define WLM_HOST_HBB_FLAG               (1 << 17)

/*
 * <ini>
 * wlm_latency_enable - WLM latency Enable
 *
 * @min: 0
 * @max: 1
 * @default: 1
 *
 * 0 - disable
 * 1 - enable
 *
 * </ini>
 */
#define CFG_LATENCY_ENABLE CFG_INI_BOOL("wlm_latency_enable", \
					1, \
					"WLM latency Enable")

/*
 * <ini>
 * wlm_latency_reset_on_disconnect - WLM latency level reset on disconnect
 *
 * @min: 0
 * @max: 1
 * @default: 0
 *
 * 0 - disable
 * 1 - enable
 *
 * </ini>
 */
#define CFG_LATENCY_RESET CFG_INI_BOOL("wlm_latency_reset_on_disconnect", \
				       0, \
				       "WLM latency reset on disconnect")

/*
 * <ini>
 * wlm_latency_level - WLM latency level
 * Define 4 latency level to gain latency
 *
 * @min: 0
 * @max: 3
 * @defalut: 0
 *
 * 0 - normal
 * 1 - moderate
 * 2 - low
 * 3 - ultralow
 *
 * </ini>
 */
#define CFG_LATENCY_LEVEL CFG_INI_UINT("wlm_latency_level", \
				       0, \
				       3, \
				       0, \
				       CFG_VALUE_OR_DEFAULT, \
				       "WLM latency level")

/*
 * <ini>
 * wlm_latency_flags_normal - WLM flags setting for normal level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x0
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_NORMAL CFG_INI_UINT("wlm_latency_flags_normal", \
					      0, \
					      0xffffffff, \
					      0, \
					      CFG_VALUE_OR_DEFAULT, \
					      "WLM flag setting for normal lvl")

/*
 * <ini>
 * wlm_latency_flags_moderate - WLM flags setting for moderate level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x8
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_MOD CFG_INI_UINT("wlm_latency_flags_moderate", \
					   0, \
					   0xffffffff, \
					   0x8, \
					   CFG_VALUE_OR_DEFAULT, \
					   "WLM flag setting for moderate lvl")

/*
 * <ini>
 * wlm_latency_flags_low - WLM flags setting for low level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0xa
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_LOW CFG_INI_UINT("wlm_latency_flags_low", \
					   0, \
					   0xffffffff, \
					   0xa, \
					   CFG_VALUE_OR_DEFAULT, \
					   "WLM flags setting for low level")

/*
 * <ini>
 * wlm_latency_flags_ultralow - WLM flags setting for ultralow level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0xc83
 *
 * |31  12|  11  |  10  |9    8|7    6|5    4|3    2|  1  |  0  |
 * +------+------+------+------+------+------+------+-----+-----+
 * | RSVD | SSLP | CSLP | RSVD | Roam | RSVD | DWLT | DFS | SUP |
 * +------+-------------+-------------+-------------------------+
 * |  WAL |      PS     |     Roam    |         Scan            |
 *
 * bit 0: Avoid scan request from HLOS if setting
 * bit 1: Skip DFS channel SCAN if setting
 * bit 2-3: Define policy of dwell time/duration for each foreign channel
 *     (b2 b3)
 *     (0  0 ): Default scan dwell time
 *     (0  1 ): Reserve
 *     (1  0 ): Shrink off channel dwell time
 *     (1  1 ): Reserve
 * bit 4-5: Reserve for scan
 * bit 6-7: Define roaming policy
 *     (b6 b7)
 *     (0  0 ): Default roaming behavior, allow roaming in all scenarios
 *     (0  1 ): Disallow all roaming
 *     (1  0 ): Allow roaming when final bmissed
 *     (1  1 ): Reserve
 * bit 8-9: Reserve for roaming
 * bit 10: Disable css power collapse if setting
 * bit 11: Disable sys sleep if setting
 * bit 12-31: Reserve for future useage
 *
 * </ini>
 */
#define CFG_LATENCY_FLAGS_ULTLOW CFG_INI_UINT("wlm_latency_flags_ultralow",\
					      0, \
					      0xffffffff, \
					      0xc83, \
					      CFG_VALUE_OR_DEFAULT, \
					      "WLM flags for ultralow level")

/*
 * <ini>
 * wlm_latency_host_flags_normal - WLM Host flags setting for normal level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x0
 *
 * This ini decides which host latency features gets enabled
 * in normal latency mode.
 *
 * Usage: External
 *
 * |31  18|  17  |  16    |15        8|7    2|    1     |     0     |
 * +------+------+--------+-----------+------+----------+-----------+
 * | RSVD | HBB  | PM-QOS |  RSVD     | RSVD | REO Ring | RX Thread |
 * +------+------+--------+-----------+------+----------+-----------+
 * |       common         |  TX Path  |        RX Path              |
 *
 * bit 0-7: Rx path related optimization
 * bit 0: disable rx_thread for vdev
 * bit 1: Reduce REO ring timer threshold to 32us(applicable for all REO ring)
 * bit 2-7: Reserved
 * bit 8-15: Tx path related optimization
 * bit 8-15: Reserved
 * bit 16-31: common changes
 * bit 16: Request for pm_qos vote
 * bit 17: Request for high ddr bus bandwidth
 *
 * </ini>
 */
#define CFG_LATENCY_HOST_FLAGS_NORMAL \
		CFG_INI_UINT("wlm_latency_host_flags_normal",\
			     0, \
			     0xffffffff, \
			     0, \
			     CFG_VALUE_OR_DEFAULT, \
			     "WLM Host flags for normal level")

/*
 * <ini>
 * wlm_latency_host_flags_moderate - WLM Host flags setting for moderate level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x0
 *
 * This ini decides which host latency features gets enabled
 * in moderate latency mode.
 *
 * Usage: External
 *
 * |31  18|  17  |  16    |15        8|7    2|    1     |     0     |
 * +------+------+--------+-----------+------+----------+-----------+
 * | RSVD | HBB  | PM-QOS |  RSVD     | RSVD | REO Ring | RX Thread |
 * +------+------+--------+-----------+------+----------+-----------+
 * |       common         |  TX Path  |        RX Path              |
 *
 * bit 0-7: Rx path related optimization
 * bit 0: disable rx_thread for vdev
 * bit 1: Reduce REO ring timer threshold to 32us(applicable for all REO ring)
 * bit 2-7: Reserved
 * bit 8-15: Tx path related optimization
 * bit 8-15: Reserved
 * bit 16-31: common changes
 * bit 16: Request for pm_qos vote
 * bit 17: Request for high ddr bus bandwidth
 *
 * </ini>
 */
#define CFG_LATENCY_HOST_FLAGS_MOD \
		CFG_INI_UINT("wlm_latency_host_flags_moderate",\
			     0, \
			     0xffffffff, \
			     0, \
			     CFG_VALUE_OR_DEFAULT, \
			     "WLM Host flags for moderate level")
/*
 * <ini>
 * wlm_latency_host_flags_low - WLM Host flags setting for low level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x0
 *
 * This ini decides which host latency features gets enabled
 * in low latency mode.
 *
 * Usage: External
 *
 * |31  18|  17  |  16    |15        8|7    2|    1     |     0     |
 * +------+------+--------+-----------+------+----------+-----------+
 * | RSVD | HBB  | PM-QOS |  RSVD     | RSVD | REO Ring | RX Thread |
 * +------+------+--------+-----------+------+----------+-----------+
 * |       common         |  TX Path  |        RX Path              |
 *
 * bit 0-7: Rx path related optimization
 * bit 0: disable rx_thread for vdev
 * bit 1: Reduce REO ring timer threshold to 32us(applicable for all REO ring)
 * bit 2-7: Reserved
 * bit 8-15: Tx path related optimization
 * bit 8-15: Reserved
 * bit 16-31: common changes
 * bit 16: Request for pm_qos vote
 * bit 17: Request for high ddr bus bandwidth
 *
 * </ini>
 */
#define CFG_LATENCY_HOST_FLAGS_LOW CFG_INI_UINT("wlm_latency_host_flags_low",\
						0, \
						0xffffffff, \
						0, \
						CFG_VALUE_OR_DEFAULT, \
						"WLM Host flags for low level")

/*
 * <ini>
 * wlm_latency_host_flags_ultralow - WLM Host flags setting for ultralow level
 *
 * @min: 0x0
 * @max: 0xffffffff
 * @default: 0x0
 *
 * This ini decides which host latency features gets enabled
 * in ultra low latency mode.
 *
 * Usage: External
 *
 * |31  18|  17  |  16    |15        8|7    2|    1     |     0     |
 * +------+------+--------+-----------+------+----------+-----------+
 * | RSVD | HBB  | PM-QOS |  RSVD     | RSVD | REO Ring | RX Thread |
 * +------+------+--------+-----------+------+----------+-----------+
 * |       common         |  TX Path  |        RX Path              |
 *
 * bit 0-7: Rx path related optimization
 * bit 0: disable rx_thread for vdev
 * bit 1: Reduce REO ring timer threshold to 32us(applicable for all REO ring)
 * bit 2-7: Reserved
 * bit 8-15: Tx path related optimization
 * bit 8-15: Reserved
 * bit 16-31: common changes
 * bit 16: Request for pm_qos vote
 * bit 17: Request for high ddr bus bandwidth
 *
 * </ini>
 */
#define CFG_LATENCY_HOST_FLAGS_ULTLOW \
		CFG_INI_UINT("wlm_latency_host_flags_ultralow",\
			     0, \
			     0xffffffff, \
			     0, \
			     CFG_VALUE_OR_DEFAULT, \
			     "WLM Host flags for ultralow level")

#define CFG_FE_WLM_ALL \
	CFG(CFG_LATENCY_ENABLE) \
	CFG(CFG_LATENCY_RESET) \
	CFG(CFG_LATENCY_LEVEL) \
	CFG(CFG_LATENCY_FLAGS_NORMAL) \
	CFG(CFG_LATENCY_FLAGS_MOD) \
	CFG(CFG_LATENCY_FLAGS_LOW) \
	CFG(CFG_LATENCY_FLAGS_ULTLOW) \
	CFG(CFG_LATENCY_HOST_FLAGS_NORMAL) \
	CFG(CFG_LATENCY_HOST_FLAGS_MOD) \
	CFG(CFG_LATENCY_HOST_FLAGS_LOW) \
	CFG(CFG_LATENCY_HOST_FLAGS_ULTLOW)

#endif /* __CFG_MLME_FE_WLM_H */
