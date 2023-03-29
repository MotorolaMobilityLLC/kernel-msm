/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains configuration definitions for MLME STA.
 */

#ifndef CFG_MLME_STA_H__
#define CFG_MLME_STA_H__

#include "wlan_mlme_public_struct.h"

/*
 * <ini>
 * gStaKeepAlivePeriod - STA keep alive period
 *
 *
 * @Min: 0
 * @Max: 1000
 * @Default: 30
 *
 * This ini is used to control how frequently STA should send NULL frames to AP
 * (period in seconds) to notify AP of its existence.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_INFRA_STA_KEEP_ALIVE_PERIOD CFG_INI_UINT( \
	"gStaKeepAlivePeriod", \
	0, \
	1000, \
	30, \
	CFG_VALUE_OR_DEFAULT, \
	"send default NULL frame to AP")

/*
 * <ini>
 * tgt_gtx_usr_cfg - target gtx user config
 * @Min: 0
 * @Max: 32
 * @Default: 32
 *
 * This ini is used to set target gtx user config.
 *
 * Related: None
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TGT_GTX_USR_CFG CFG_INI_UINT( \
	"tgt_gtx_usr_cfg", \
	0, \
	32, \
	32, \
	CFG_VALUE_OR_DEFAULT, \
	"target gtx user config")

/*
 * <ini>
 * pmkidModes - Enable PMKID modes
 * This INI is used to enable PMKID feature options
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PMKID_MODES CFG_INI_UINT( \
	"pmkidModes", \
	0, \
	3, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"PMKID feature options")

/*
 * <ini>
 * gIgnorePeerErpInfo - Ignore peer information
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to ignore default peer info
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_IGNORE_PEER_ERP_INFO CFG_INI_BOOL( \
	"gIgnorePeerErpInfo", \
	0, \
	"ignore default peer info")

/*
 * <ini>
 * gStaPrefer80MHzOver160MHz - set sta preference to connect in 80HZ/160HZ
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set sta preference to connect in 80HZ/160HZ
 *
 * 0 - Connects in 160MHz 1x1 when AP is 160MHz 2x2
 * 1 - Connects in 80MHz 2x2 when AP is 160MHz 2x2
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STA_PREFER_80MHZ_OVER_160MHZ CFG_INI_BOOL( \
	"gStaPrefer80MHzOver160MHz", \
	0, \
	"Sta preference to connect in 80HZ/160HZ")

/*
 * <ini>
 * gEnable5gEBT - Enables/disables 5G early beacon termination. When enabled
 *                 terminate the reception of beacon if the TIM element is
 *                 clear for the power saving
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default 5G early beacon termination
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PPS_ENABLE_5G_EBT CFG_INI_BOOL( \
	"gEnable5gEBT", \
	1, \
	"5G early beacon termination")

/*
 * <ini>
 * gSendDeauthBeforeCon - Send deauth before connection or not
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set whether send deauth before connection or
 * not. If last disconnection was due to HB failure and we reconnect
 * to same AP next time, send deauth before starting connection.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_DEAUTH_BEFORE_CONNECTION CFG_INI_BOOL( \
	"gSendDeauthBeforeCon", \
	0, \
	"send deauth before connection")

/*
 * <ini>
 * deauth_retry_cnt- No. of deauth retries if the Tx is failed
 * @Min: 0
 * @Max: 4
 * @Default: 2
 *
 * This ini is used to set retry deauth if Tx is not success.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DEAUTH_RETRY_CNT CFG_INI_UINT( \
	"deauth_retry_cnt", \
	0, \
	4, \
	2, \
	CFG_VALUE_OR_DEFAULT, \
	"Set Deauth retry count")

/*
 * <ini>
 * gDot11PMode - 802.11p mode
 * @Min: CFG_11P_DISABLED
 * @Max: CFG_11P_CONCURRENT
 * @Default: CFG_11P_DISABLED
 *
 * This ini used to set 802.11p mode.
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_DOT11P_MODE CFG_INI_UINT( \
	"gDot11PMode", \
	CFG_11P_DISABLED, \
	CFG_11P_CONCURRENT, \
	CFG_11P_DISABLED, \
	CFG_VALUE_OR_DEFAULT, \
	"802.11p mode")

/*
 * <ini>
 * gEnable_go_cts2self_for_sta - Indicate firmware to stop NOA and
 * start using cts2self
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * When gEnable_go_cts2self_for_sta is enabled then if a legacy
 * client connects to P2P GO, Host will send a WMI VDEV command
 * to FW to stop using NOA for P2P GO
 * and start using CTS2SELF.
 *
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_GO_CTS2SELF_FOR_STA CFG_INI_BOOL( \
	"gEnable_go_cts2self_for_sta", \
	0, \
	"firmware to stop NOA and start using cts2self")

/*
 * <ini>
 * g_qcn_ie_support - QCN IE support
 * @Min: 0 (disabled)
 * @Max: 1 (enabled)
 * @Default: 1 (enabled)
 *
 * This config item is used to support QCN IE in probe/assoc/reassoc request
 * for STA mode. QCN IE support is not added for SAP mode.
 *
 * Related: N/A
 *
 * Supported Feature: N/A
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_QCN_IE_SUPPORT CFG_INI_BOOL( \
	"g_qcn_ie_support", \
	1, \
	"QCN IE support")

/*
 * <ini>
 * g_fils_max_chan_guard_time - Set maximum channel guard time(ms)
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to set maximum channel guard time in milliseconds.
 *
 * Related: None
 *
 * Supported Feature: FILS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FILS_MAX_CHAN_GUARD_TIME CFG_INI_UINT( \
	"g_fils_max_chan_guard_time", \
	0, \
	10, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Set maximum channel guard time")

/*
 * <ini>
 * SingleTIDRC - Set replay counter for all TID's
 * @Min: 0       Separate replay counter for all TID
 * @Max: 1       Single replay counter for all TID
 * @Default: 1
 *
 * This ini is used to set replay counter for all TID's
 *
 * 0 - Separate replay counter for all TID
 * 1 - Single replay counter for all TID
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SINGLE_TID_RC CFG_INI_BOOL( \
	"SingleTIDRC", \
	1, \
	"replay counter for all TID")

/*
 * wait_cnf_timeout - Wait assoc cnf timeout
 * @Min: 10
 * @Max: 3000
 * @Default: 1000
 *
 * This is internal configure for waiting assoc cnf timeout
 *
 * Related: None
 *
 * Usage: Internal
 *
 */
#define CFG_WT_CNF_TIMEOUT CFG_UINT( \
	"wait_cnf_timeout", \
	10, \
	3000, \
	1000, \
	CFG_VALUE_OR_DEFAULT, \
	"Wait confirm timeout")

/*
 * <ini>
 * gStaMiracastMccRestTimeVal - Rest time when Miracast is running.
 * @Min: 100
 * @Max: 500
 * @Default: 400
 *
 * This ini is used to set rest time for home channel for Miracast before
 * going for scan.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_STA_MCAST_MCC_REST_TIME CFG_INI_UINT( \
	"gStaMiracastMccRestTimeVal", \
	100, \
	500, \
	400, \
	CFG_VALUE_OR_DEFAULT, \
	"Rest time when Miracast is running")

/*
 * current_rssi - current rssi
 * @Min: 0
 * @Max: 127
 * @Default: 0
 *
 * This is internal configure for current rssi
 *
 * Related: None
 *
 * Usage: Internal
 *
 */
#define CFG_CURRENT_RSSI CFG_UINT( \
	"current_rssi", \
	0, \
	127, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Current RSSI")

/*
 * <ini>
 * gAllowTPCfromAP - Support for AP power constraint
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini controls driver to honor/dishonor power constraint from AP.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TX_POWER_CTRL CFG_INI_BOOL( \
	"gAllowTPCfromAP", \
	1, \
	"Support for AP power constraint")

/*
 * <ini>
 * gStaKeepAliveMethod - Which keepalive method to use
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini determines which keepalive method to use for station interfaces
 *	 1) Use null data packets
 *	 2) Use gratuitous ARP packets
 *
 * Related: gStaKeepAlivePeriod, gApKeepAlivePeriod, gGoKeepAlivePeriod
 *
 * Supported Feature: STA, Keepalive
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STA_KEEPALIVE_METHOD CFG_INI_INT( \
			"gStaKeepAliveMethod", \
			MLME_STA_KEEPALIVE_NULL_DATA, \
			MLME_STA_KEEPALIVE_COUNT - 1, \
			MLME_STA_KEEPALIVE_GRAT_ARP, \
			CFG_VALUE_OR_DEFAULT, \
			"Which keepalive method to use")

/*
 * <ini>
 * gMaxLIModulatedDTIM - Set MaxLIModulate Dtim
 * @Min: 1
 * @Max: 10
 * @Default: 5
 *
 * This ini is used to set default MaxLIModulatedDTIM
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_MAX_LI_MODULATED_DTIM CFG_INI_UINT( \
			"gMaxLIModulatedDTIM", \
			1, \
			10, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"Max modulated dtim")

#define CFG_STA_ALL \
	CFG(CFG_INFRA_STA_KEEP_ALIVE_PERIOD) \
	CFG(CFG_TGT_GTX_USR_CFG) \
	CFG(CFG_PMKID_MODES) \
	CFG(CFG_IGNORE_PEER_ERP_INFO) \
	CFG(CFG_STA_PREFER_80MHZ_OVER_160MHZ) \
	CFG(CFG_PPS_ENABLE_5G_EBT) \
	CFG(CFG_ENABLE_DEAUTH_BEFORE_CONNECTION) \
	CFG(CFG_DOT11P_MODE) \
	CFG(CFG_DEAUTH_RETRY_CNT) \
	CFG(CFG_ENABLE_GO_CTS2SELF_FOR_STA) \
	CFG(CFG_QCN_IE_SUPPORT) \
	CFG(CFG_STA_MCAST_MCC_REST_TIME) \
	CFG(CFG_FILS_MAX_CHAN_GUARD_TIME) \
	CFG(CFG_SINGLE_TID_RC) \
	CFG(CFG_STA_KEEPALIVE_METHOD) \
	CFG(CFG_WT_CNF_TIMEOUT) \
	CFG(CFG_CURRENT_RSSI) \
	CFG(CFG_TX_POWER_CTRL)

#endif /* CFG_MLME_STA_H__ */
