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
 * DOC: This file contains definitions of Data Path configuration.
 */

#ifndef _CFG_IPA_H_
#define _CFG_IPA_H_

#include "cfg_define.h"

/* DP INI Declerations */
/*
 * IPA Offload configuration - Each bit enables a feature
 * bit0 - IPA Enable
 * bit1 - IPA Pre filter enable
 * bit2 - IPv6 enable
 * bit3 - IPA Resource Manager (RM) enable
 * bit4 - IPA Clock scaling enable
 */

/*
 * <ini>
 * gIPAConfig - IPA configuration
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0
 *
 * This ini specifies the IPA configuration
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_OFFLOAD_CONFIG \
		CFG_INI_UINT("gIPAConfig", \
		0, \
		0xFFFFFFFF, \
		0, \
		CFG_VALUE_OR_DEFAULT, "IPA offload configuration")

/*
 * <ini>
 * gIPADescSize - IPA descriptor size
 * @Min: 800
 * @Max: 8000
 * @Default: 800
 *
 * This ini specifies the IPA descriptor size
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_IPA_DESC_SIZE \
		CFG_INI_UINT("gIPADescSize", \
		800, \
		8000, \
		800, \
		CFG_VALUE_OR_DEFAULT, "IPA DESC SIZE")

/*
 * <ini>
 * gIPAHighBandwidthMbps - IPA high bw threshold
 * @Min: 200
 * @Max: 1000
 * @Default: 400
 *
 * This ini specifies the IPA high bw threshold
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_IPA_HIGH_BANDWIDTH_MBPS \
		CFG_INI_UINT("gIPAHighBandwidthMbps", \
		200, \
		1000, \
		400, \
		CFG_VALUE_OR_DEFAULT, "IPA high bw threshold")

/*
 * <ini>
 * gIPAMediumBandwidthMbps - IPA medium bw threshold
 * @Min: 100
 * @Max: 400
 * @Default: 200
 *
 * This ini specifies the IPA medium bw threshold
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_IPA_MEDIUM_BANDWIDTH_MBPS \
		CFG_INI_UINT("gIPAMediumBandwidthMbps", \
		100, \
		400, \
		200, \
		CFG_VALUE_OR_DEFAULT, "IPA medium bw threshold")

/*
 * <ini>
 * gIPALowBandwidthMbps - IPA low bw threshold
 * @Min: 0
 * @Max: 100
 * @Default: 100
 *
 * This ini specifies the IPA low bw threshold
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_IPA_LOW_BANDWIDTH_MBPS \
		CFG_INI_UINT("gIPALowBandwidthMbps", \
		0, \
		100, \
		100, \
		CFG_VALUE_OR_DEFAULT, "IPA low bw threshold")

/*
 * <ini>
 * gIPAForceVotingEnable - IPA force voting enable
 * @Default: false
 *
 * This ini specifies to enable IPA force voting
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_ENABLE_FORCE_VOTING \
		CFG_INI_BOOL("gIPAForceVotingEnable", \
		false, "Ctrl to enable force voting")

/*
 * <ini>
 * IpaUcTxBufCount - IPA tx buffer count
 * @Min: 0
 * @Max: 2048
 * @Default: 512
 *
 * This ini specifies the IPA tx buffer count
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_IPA_UC_TX_BUF_COUNT \
		CFG_INI_UINT("IpaUcTxBufCount", \
		0, \
		2048, \
		512, \
		CFG_VALUE_OR_DEFAULT, "IPA tx buffer count")

#define CFG_IPA \
	CFG(CFG_DP_IPA_OFFLOAD_CONFIG) \
	CFG(CFG_DP_IPA_DESC_SIZE) \
	CFG(CFG_DP_IPA_HIGH_BANDWIDTH_MBPS) \
	CFG(CFG_DP_IPA_MEDIUM_BANDWIDTH_MBPS) \
	CFG(CFG_DP_IPA_LOW_BANDWIDTH_MBPS) \
	CFG(CFG_DP_IPA_ENABLE_FORCE_VOTING) \
	CFG(CFG_DP_IPA_UC_TX_BUF_COUNT)

#endif /* _CFG_IPA_H_ */
