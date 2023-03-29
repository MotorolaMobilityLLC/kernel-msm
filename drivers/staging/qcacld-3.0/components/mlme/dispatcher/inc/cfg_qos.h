/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __CFG_MLME_QOS_H
#define __CFG_MLME_QOS_H

#define ADDBA_TXAGGR_SIZE 256

/*
 * <ini>
 * gTxAggregationSize - Gives an option to configure Tx aggregation size
 * in no of MPDUs
 * @Min: 0
 * @Max: ADDBA_TXAGGR_SIZE
 * @Default: ADDBA_TXAGGR_SIZE
 *
 * gTxAggregationSize gives an option to configure Tx aggregation size
 * in no of MPDUs.This can be useful in debugging throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_AGGREGATION_SIZE CFG_INI_UINT( \
			"gTxAggregationSize", \
			0, \
			ADDBA_TXAGGR_SIZE, \
			ADDBA_TXAGGR_SIZE, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx Aggregation size value")

/*
 * <ini>
 * gTxAggregationSizeBE - To configure Tx aggregation size for BE queue
 * in no of MPDUs
 * @Min: 0
 * @Max: ADDBA_TXAGGR_SIZE
 * @Default: 0
 *
 * gTxAggregationSizeBE gives an option to configure Tx aggregation size
 * for BE queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGREGATION_SIZEBE CFG_INI_UINT( \
			"gTxAggregationSizeBE", \
			0, \
			ADDBA_TXAGGR_SIZE, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx Aggregation size value BE")

/*
 * <ini>
 * gTxAggregationSizeBK - To configure Tx aggregation size for BK queue
 * in no of MPDUs
 * @Min: 0
 * @Max: ADDBA_TXAGGR_SIZE
 * @Default: 0
 *
 * gTxAggregationSizeBK gives an option to configure Tx aggregation size
 * for BK queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGREGATION_SIZEBK CFG_INI_UINT( \
			"gTxAggregationSizeBK", \
			0, \
			ADDBA_TXAGGR_SIZE, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx Aggregation size value BK")

/*
 * <ini>
 * gTxAggregationSizeVI - To configure Tx aggregation size for VI queue
 * in no of MPDUs
 * @Min: 0
 * @Max: ADDBA_TXAGGR_SIZE
 * @Default: 0
 *
 * gTxAggregationSizeVI gives an option to configure Tx aggregation size
 * for VI queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGREGATION_SIZEVI CFG_INI_UINT( \
			"gTxAggregationSizeVI", \
			0, \
			ADDBA_TXAGGR_SIZE, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx Aggregation size value for VI")

/*
 * <ini>
 * gTxAggregationSizeVO - To configure Tx aggregation size for VO queue
 * in no of MPDUs
 * @Min: 0
 * @Max: ADDBA_TXAGGR_SIZE
 * @Default: 0
 *
 * gTxAggregationSizeVO gives an option to configure Tx aggregation size
 * for BE queue in no of MPDUs.This can be useful in debugging
 * throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGREGATION_SIZEVO CFG_INI_UINT( \
			"gTxAggregationSizeVO", \
			0, \
			ADDBA_TXAGGR_SIZE, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx Aggregation size value for VO")

/*
 * <ini>
 * gRxAggregationSize - Gives an option to configure Rx aggregation size
 * in no of MPDUs
 * @Min: 1
 * @Max: 256
 * @Default: 256
 *
 * gRxAggregationSize gives an option to configure Rx aggregation size
 * in no of MPDUs. This can be useful in debugging throughput issues
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_AGGREGATION_SIZE CFG_INI_UINT( \
			"gRxAggregationSize", \
			1, \
			256, \
			256, \
			CFG_VALUE_OR_DEFAULT, \
			"Rx Aggregation size value")

/*
 * <ini>
 * gTxAggSwRetryBE - Configure Tx aggregation sw retry for BE
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryBE gives an option to configure Tx aggregation sw
 * retry for BE. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGR_SW_RETRY_BE CFG_INI_UINT( \
			"gTxAggSwRetryBE", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx aggregation retry value for BE")

/*
 * <ini>
 * gTxAggSwRetryBK - Configure Tx aggregation sw retry for BK
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryBK gives an option to configure Tx aggregation sw
 * retry for BK. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGR_SW_RETRY_BK CFG_INI_UINT( \
			"gTxAggSwRetryBK", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx aggregation retry value for BK")

/*
 * <ini>
 * gTxAggSwRetryVI - Configure Tx aggregation sw retry for VI
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryVI gives an option to configure Tx aggregation sw
 * retry for VI. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGR_SW_RETRY_VI CFG_INI_UINT( \
			"gTxAggSwRetryVI", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx aggregation retry value for VI")

/*
 * <ini>
 * gTxAggSwRetryVO - Configure Tx aggregation sw retry for VO
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxAggSwRetryVO gives an option to configure Tx aggregation sw
 * retry for VO. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_AGGR_SW_RETRY_VO CFG_INI_UINT( \
			"gTxAggSwRetryVO", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx aggregation retry value for VO")

/*
 * <ini>
 * gTxNonAggSwRetryBE - Configure Tx non aggregation sw retry for BE
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryBE gives an option to configure Tx non aggregation sw
 * retry for BE. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_NON_AGGR_SW_RETRY_BE CFG_INI_UINT( \
			"gTxNonAggSwRetryBE", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx non aggregation retry value for BE")

/*
 * <ini>
 * gTxNonAggSwRetryBK - Configure Tx non aggregation sw retry for BK
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryBK gives an option to configure Tx non aggregation sw
 * retry for BK. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_NON_AGGR_SW_RETRY_BK CFG_INI_UINT( \
			"gTxNonAggSwRetryBK", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx non aggregation retry value for BK")

/*
 * <ini>
 * gTxNonAggSwRetryVI - Configure Tx non aggregation sw retry for VI
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryVI gives an option to configure Tx non aggregation sw
 * retry for VI. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_NON_AGGR_SW_RETRY_VI CFG_INI_UINT( \
			"gTxNonAggSwRetryVI", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx non aggregation retry value for VI")

/*
 * <ini>
 * gTxNonAggSwRetryVO - Configure Tx non aggregation sw retry for VO
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * gTxNonAggSwRetryVO gives an option to configure Tx non aggregation sw
 * retry for VO. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_NON_AGGR_SW_RETRY_VO CFG_INI_UINT( \
			"gTxNonAggSwRetryVO", \
			0, \
			64, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx non aggregation retry value for VO")
/*
 * <ini>
 * gTxAggSwRetry - Configure Tx aggregation sw retry
 * @Min: 0
 * @Max: 64
 * @Default: 16
 *
 * gTxAggSwRetry gives an option to configure Tx aggregation sw
 * retry. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_AGGR_SW_RETRY CFG_INI_UINT( \
			"gTxAggSwRetry", \
			0, \
			64, \
			16, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx aggregation retry value")
/*
 * <ini>
 * gTxNonAggSwRetry - Configure Tx non aggregation sw retry
 * @Min: 0
 * @Max: 64
 * @Default: 16
 *
 * gTxNonAggSwRetry gives an option to configure Tx non aggregation sw
 * retry. This can be useful in debugging throughput issues.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_NON_AGGR_SW_RETRY CFG_INI_UINT( \
			"gTxNonAggSwRetry", \
			0, \
			64, \
			16, \
			CFG_VALUE_OR_DEFAULT, \
			"Tx non aggregation retry value")
/*
 * <ini>
 * gSapMaxInactivityOverride - Configure
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This parameter will avoid updating ap_sta_inactivity from hostapd.conf
 * file. If a station does not send anything in ap_max_inactivity seconds, an
 * empty data frame is sent to it in order to verify whether it is
 * still in range. If this frame is not ACKed, the station will be
 * disassociated and then deauthenticated. This feature is used to
 * clear station table of old entries when the STAs move out of the
 * range.
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: External
 * </ini>
 */
#define CFG_SAP_MAX_INACTIVITY_OVERRIDE CFG_INI_BOOL( \
			"gSapMaxInactivityOverride", \
			0, \
			"SAP maximum inactivity override flag")

/*
 * <ini>
 * gEnableApUapsd - Enable/disable UAPSD for SoftAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to setup setup U-APSD for Acs at association
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_QOS_UAPSD CFG_INI_BOOL( \
			"gEnableApUapsd", \
			1, \
			"Enable UAPSD for SAP")

#define IOT_AGGR_INFO_MAX_LEN 500
#define IOT_AGGR_INFO_MAX_NUM 32
#define IOT_AGGR_MSDU_MAX_NUM 6
#define IOT_AGGR_MPDU_MAX_NUM 512
/*
 * <ini>
 * cfg_tx_iot_aggr - OUI based tx aggr size for msdu/mpdu
 *
 * This ini gives an option to configure Tx aggregation size
 * in no. of MPDUs/MSDUs for specified OUI.
 * This can be useful for IOT issues.
 *
 * Format of the configuration:
 * cfg_tx_iot_aggr=<OUI-1>,<MSDU-1>,<MPDU-1>,<OUI-2>,<MSDU-2>,<MPDU-2>...
 * MSDU: 0..IOT_AGGR_MSDU_MAX_NUM, the max tx aggregation size in no. of MSDUs,
 *       0 means not specified.
 * MPDU: 0..IOT_AGGR_MPDU_MAX_NUM, the max tx aggregation size in no. of MPDUs,
 *       0 means not specified.
 * Note: MSDU-x/MPDU-x are the max values, FW will take decision for actual
 *   AMSDU/AMPDU size on different platforms.
 *
 * For example:
 *   cfg_tx_iot_aggr=112233,2,0,445566,3,32,778899,0,64
 *   If vendor OUI-1("\x11\x22\x33") is found in assoc resp,
 *   set tx amsdu size to 2;
 *   If vendor OUI-2("\x44\x55\x66") is found in assoc resp,
 *   set tx amsdu size to 3, set tx ampdu size to 32;
 *   If vendor OUI-3("\x77\x88\x99") is found in assoc resp,
 *   set tx ampdu size to 64.
 *
 * Related: IOT
 *
 * Supported Feature: IOT
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_IOT_AGGR CFG_INI_STRING( \
		"cfg_tx_iot_aggr", \
		0, \
		IOT_AGGR_INFO_MAX_LEN, \
		"", \
		"Used to configure OUI based tx aggr size for msdu/mpdu")

#define CFG_QOS_ALL \
	CFG(CFG_SAP_MAX_INACTIVITY_OVERRIDE) \
	CFG(CFG_TX_AGGREGATION_SIZE) \
	CFG(CFG_TX_AGGREGATION_SIZEBE) \
	CFG(CFG_TX_AGGREGATION_SIZEBK) \
	CFG(CFG_TX_AGGREGATION_SIZEVI) \
	CFG(CFG_TX_AGGREGATION_SIZEVO) \
	CFG(CFG_RX_AGGREGATION_SIZE) \
	CFG(CFG_TX_AGGR_SW_RETRY_BE) \
	CFG(CFG_TX_AGGR_SW_RETRY_BK) \
	CFG(CFG_TX_AGGR_SW_RETRY_VI) \
	CFG(CFG_TX_AGGR_SW_RETRY_VO) \
	CFG(CFG_TX_AGGR_SW_RETRY) \
	CFG(CFG_TX_NON_AGGR_SW_RETRY_BE) \
	CFG(CFG_TX_NON_AGGR_SW_RETRY_BK) \
	CFG(CFG_TX_NON_AGGR_SW_RETRY_VI) \
	CFG(CFG_TX_NON_AGGR_SW_RETRY_VO) \
	CFG(CFG_TX_NON_AGGR_SW_RETRY) \
	CFG(CFG_SAP_QOS_UAPSD) \
	CFG(CFG_TX_IOT_AGGR)

#endif /* __CFG_MLME_QOS_H */
