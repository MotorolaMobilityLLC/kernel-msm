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

#ifndef __CFG_LEGACY_DP
#define __CFG_LEGACY_DP

#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"

/*
 * <ini>
 * gEnableFlowSteering - Enable rx traffic flow steering
 * @Default: false
 *
 * Enable Rx traffic flow steering to enable Rx interrupts on multiple CEs based
 * on the flows. Different CEs<==>different IRQs<==>probably different CPUs.
 * Parallel Rx paths.
 * 1 - enable  0 - disable
 *
 * Usage: Internal
 *
 * </ini>
 */
 #define CFG_DP_FLOW_STEERING_ENABLED \
		CFG_INI_BOOL( \
		"gEnableFlowSteering", \
		false, \
		"")

/*
 * <ini>
 * maxMSDUsPerRxInd - Max number of MSDUs per HTT RX IN ORDER INDICATION msg.
 * Note that this has a direct impact on the size of source CE rings.
 * It is possible to go below 8, but would require testing; so we are
 * restricting the lower limit to 8 artificially
 *
 * It is recommended that this value is a POWER OF 2.
 *
 * Values lower than 8 are for experimental purposes only
 *
 * </ini>.
 */
#define CFG_DP_MAX_MSDUS_PER_RXIND \
	CFG_INI_UINT("maxMSDUsPerRxInd", \
	4, 32, 32, CFG_VALUE_OR_DEFAULT, \
	"Max number of MSDUs per HTT RX INORDER IND msg")

/*
 * <ini>
 * gEnableTxSchedWrrVO - Set TX sched parameters for VO
 * @Default:
 *
 * This key is mapping to VO defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for VO.
 * e.g., gEnableTxSchedWrrVO = 10, 9, 8, 1, 8
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TX_SCHED_WRR_VO \
	CFG_INI_STRING("gEnableTxSchedWrrVO", \
	0, 50, "", "et TX sched parameters for VO")

/*
 * <ini>
 * gEnableTxSchedWrrVI - Set TX sched parameters for VI
 * @Default:
 *
 * This key is mapping to VI defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for VI.
 * e.g., gEnableTxSchedWrrVI = 10, 9, 8, 1, 8
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TX_SCHED_WRR_VI \
	CFG_INI_STRING("gEnableTxSchedWrrVI", \
	0, 50, "", "Set TX sched parameters for VI")

/*
 * <ini>
 * gEnableTxSchedWrrBE - Set TX sched parameters for BE
 * @Default:
 *
 * This key is mapping to BE defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for BE.
 * e.g., gEnableTxSchedWrrBE = 10, 9, 8, 1, 8
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TX_SCHED_WRR_BE \
	CFG_INI_STRING("gEnableTxSchedWrrBE", \
	0, 50, "", "Set TX sched parameters for BE")

/*
 * <ini>
 * gEnableTxSchedWrrBK - Set TX sched parameters for BK
 * @Default:
 *
 * This key is mapping to BK defined in data path module through
 * OL_TX_SCHED_WRR_ADV_CAT_CFG_SPEC. The user can tune the
 * WRR TX sched parameters such as skip, credit, limit, credit, disc for BK.
 * e.g., gEnableTxSchedWrrBK = 10, 9, 8, 1, 8
 *
 * </ini>
 */
#define CFG_DP_ENABLE_TX_SCHED_WRR_BK \
	CFG_INI_STRING("gEnableTxSchedWrrBK", \
	0, 50, "", "Set TX sched parameters for BK")

#define CFG_DP_CE_CLASSIFY_ENABLE \
		CFG_INI_BOOL("gCEClassifyEnable", \
		true, "enable CE classify")

/*
 * <ini>
 * gEnablePeerUnmapConfSupport - Set PEER UNMAP confirmation support
 * Default: false
 * 1 - enable  0 - disable
 *
 * Enable peer unmap confirmation support in the Host. Host will send
 * this support to the FW only if FW support is enabled.
 *
 * </ini>
 */
#define CFG_DP_ENABLE_PEER_UMAP_CONF_SUPPORT \
		CFG_INI_BOOL("gEnablePeerUnmapConfSupport", \
		false, "enable PEER UNMAP CONF support")

#define CFG_LEGACY_DP_ALL \
	CFG(CFG_DP_FLOW_STEERING_ENABLED) \
	CFG(CFG_DP_CE_CLASSIFY_ENABLE) \
	CFG(CFG_DP_MAX_MSDUS_PER_RXIND) \
	CFG(CFG_DP_ENABLE_TX_SCHED_WRR_VO) \
	CFG(CFG_DP_ENABLE_TX_SCHED_WRR_VI) \
	CFG(CFG_DP_ENABLE_TX_SCHED_WRR_BE) \
	CFG(CFG_DP_ENABLE_TX_SCHED_WRR_BK) \
	CFG(CFG_DP_ENABLE_PEER_UMAP_CONF_SUPPORT)

#endif
