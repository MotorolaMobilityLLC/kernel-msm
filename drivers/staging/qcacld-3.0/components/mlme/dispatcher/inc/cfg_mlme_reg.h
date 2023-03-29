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
 * DOC: This file contains configuration definitions for MLME REG.
 */

#ifndef CFG_MLME_REG_H__
#define CFG_MLME_REG_H__

/*
 * <ini>
 * gSelfGenFrmPwr - self-generated frame power in tx chain mask
 * for CCK rates
 * @Min: 0
 * @Max: 0xffff
 * @Default: 0
 *
 * gSelfGenFrmPwr is to set self-generated frame power in tx chain mask
 * for CCK rates
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SELF_GEN_FRM_PWR CFG_INI_UINT( \
	"gSelfGenFrmPwr", \
	0, \
	0xffff, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"set the self gen power value")

/*
 * <ini>
 * enable_11d_in_world_mode - enable 11d in world mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini enables 11d in world mode, irrespective of value of
 * g11dSupportEnabled
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_11D_IN_WORLD_MODE CFG_INI_BOOL( \
	"enable_11d_in_world_mode", \
	0, \
	"enable 11d in world mode")

/*
 * <ini>
 * etsi_srd_chan_in_master_mode - Enable/disable ETSI SRD channels in
 * master mode PCL and ACS functionality
 * @Min: 0
 * @Max: 0xFF
 * @Default: 6
 *
 * etsi_srd_chan_in_master_mode is to enable/disable ETSI SRD channels in
 * master mode PCL and ACS functionality
 * Bit map for enabling the SRD mode in various modes are as follows:-
 * BIT 0:- Enable/Disable SRD channels for SAP.
 * BIT 1:- Enable/Disable SRD channels for P2P-GO.
 * BIT 2:- Enable/Disable SRD channels for NAN.
 * Rest of the bits are currently reserved for future SRD channel support for
 * other vdevs.
 *
 * Related: None
 *
 * Supported Feature: SAP/P2P-GO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ETSI_SRD_CHAN_IN_MASTER_MODE CFG_INI_UINT( \
	"etsi13_srd_chan_in_master_mode", \
	0, \
	0xff, \
	6, \
	CFG_VALUE_OR_DEFAULT, \
	"enable/disable ETSI SRD channels in master mode")

/*
 * <ini>
 * enable_nan_indoor_channel - Enable Indoor channels for NAN
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to support to indoor channels for NAN interface
 * Customer can config this item to enable/disable NAN in indoor channel
 *
 * Related: None
 *
 * Supported Feature: NAN
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_INDOOR_CHANNEL_SUPPORT_FOR_NAN CFG_INI_BOOL( \
	"enable_nan_indoor_channel", \
	0, \
	"enable/disable indoor channels for NAN")

/*
 * <ini>
 * fcc_5dot9_ghz_chan_in_master_mode - Enable/disable 5.9 GHz channels in
 * master mode for US
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * fcc_5dot9_ghz_chan_in_master_mode is to enable/disable 5.9 GHz channels
 * in master mode for FCC reg domain
 *
 * Related: None
 *
 * Supported Feature: SAP/P2P-GO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_FCC_5DOT9_GHZ_CHAN_IN_MASTER_MODE CFG_INI_BOOL( \
	"fcc_5dot9_ghz_chan_in_master_mode", \
	0, \
	"enable/disable FCC 5.9 GHz channels in master mode")

#ifdef SAP_AVOID_ACS_FREQ_LIST
#define SAP_AVOID_ACS_FREQ_LIST_DEFAULT ""

/*
 * <ini>
 * sap_avoid_acs_freq_list - Avoid configured frequencies from acs
 * @Default: No frequencies are configured, it means consider all
 * the frequencies for acs
 *
 * This ini is to configure the frequencies which needs to be
 * avoided during acs and sap will not come up on these channels
 * Ex: sap_avoid_acs_freq_list=2412,2417,2422,2427,2467,2472
 *
 * Related: Feature flag SAP_AVOID_ACS_FREQ_LIST
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SAP_AVOID_ACS_FREQ_LIST CFG_INI_STRING( \
	"sap_avoid_acs_freq_list", \
	0, \
	CFG_VALID_CHANNEL_LIST_STRING_LEN, \
	SAP_AVOID_ACS_FREQ_LIST_DEFAULT, \
	"Avoid configured frequencies during acs")
#define CFG_SAP_AVOID_ACS_FREQ_LIST_ALL CFG(CFG_SAP_AVOID_ACS_FREQ_LIST)
#else
#define CFG_SAP_AVOID_ACS_FREQ_LIST_ALL
#endif

/*
 * <ini>
 * restart_beaconing_on_chan_avoid_event - control the beaconing entity to move
 * away from active LTE channels
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * This ini is used to control the beaconing entity (SAP/GO) to move away from
 * active LTE channels when channel avoidance event is received
 * restart_beaconing_on_chan_avoid_event=0: Don't allow beaconing entity move
 * from active LTE channels
 * restart_beaconing_on_chan_avoid_event=1: Allow beaconing entity move from
 * active LTE channels
 * restart_beaconing_on_chan_avoid_event=2: Allow beaconing entity move from
 * 2.4G active LTE channels only
 *
 * Related: None
 *
 * Supported Feature: channel avoidance
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RESTART_BEACONING_ON_CH_AVOID CFG_INI_UINT( \
	"restart_beaconing_on_chan_avoid_event", \
	0, \
	2, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"control the beaconing entity to move away from active LTE channels")

/*
 * <ini>
 * gindoor_channel_support - support to start sap in indoor channel
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is to support to start sap in indoor channel.
 * Customer can config this item to enable/disable sap in indoor channel
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_INDOOR_CHANNEL_SUPPORT CFG_INI_BOOL( \
	"gindoor_channel_support", \
	0, \
	"enable/disable sap in indoor channel")

/*
 * <ini>
 * scan_11d_interval - 11d scan interval in ms
 * @Min: 1 sec
 * @Max: 10 hr
 * @Default: 1 hr
 *
 * This ini sets the 11d scan interval in FW
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_SCAN_11D_INTERVAL CFG_INI_UINT( \
	"scan_11d_interval", \
	1000, \
	36000000, \
	3600000, \
	CFG_VALUE_OR_DEFAULT, \
	"set the 11d scan interval in FW")

/*
 * <ini>
 * ignore_fw_reg_offload_ind - If set, Ignore the FW offload indication
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to ignore regdb offload indication from FW and
 * regulatory will be treated as non offload.
 *
 * Related: None
 *
 * Supported Feature: STA/AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IGNORE_FW_REG_OFFLOAD_IND CFG_INI_BOOL( \
		"ignore_fw_reg_offload_ind", \
		0, \
		"Ignore Regulatory offloads Indication from FW")

/*
 * <ini>
 * enable_pending_list_req - Sets Pending channel List Req.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This option enables/disables SCAN_CHAN_LIST_CMDID channel list command to FW
 * till the current scan is complete.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_PENDING_CHAN_LIST_REQ CFG_INI_BOOL( \
			"enable_pending_list_req", \
			0, \
			"Enable Pending list req")

/*
 * <ini>
 * retain_nol_across_regdmn - Retain NOL across reg domain
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set if NOL needs to be retained
 * on the reg domain change.
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RETAIN_NOL_ACROSS_REG_DOMAIN CFG_INI_BOOL( \
		"retain_nol_across_regdmn", \
		1, \
		"Retain NOL even if the regdomain changes")

#define CFG_REG_ALL \
	CFG(CFG_SELF_GEN_FRM_PWR) \
	CFG(CFG_ENABLE_PENDING_CHAN_LIST_REQ) \
	CFG(CFG_ENABLE_11D_IN_WORLD_MODE) \
	CFG(CFG_ETSI_SRD_CHAN_IN_MASTER_MODE) \
	CFG(CFG_INDOOR_CHANNEL_SUPPORT_FOR_NAN) \
	CFG(CFG_FCC_5DOT9_GHZ_CHAN_IN_MASTER_MODE) \
	CFG(CFG_RESTART_BEACONING_ON_CH_AVOID) \
	CFG(CFG_INDOOR_CHANNEL_SUPPORT) \
	CFG(CFG_SCAN_11D_INTERVAL) \
	CFG(CFG_IGNORE_FW_REG_OFFLOAD_IND) \
	CFG(CFG_RETAIN_NOL_ACROSS_REG_DOMAIN) \
	CFG_SAP_AVOID_ACS_FREQ_LIST_ALL

#endif /* CFG_MLME_REG_H__ */
