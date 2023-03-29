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

#ifndef __CFG_POLICY_MGR
#define __CFG_POLICY_MGR
#include "qdf_types.h"

/*
 * <ini>
 * gWlanMccToSccSwitchMode - Control SAP channel.
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * This ini is used to override SAP channel.
 * If gWlanMccToSccSwitchMode = 0: disabled.
 * If gWlanMccToSccSwitchMode = 1: Enable switch.
 * If gWlainMccToSccSwitchMode = 2: Force switch with SAP restart.
 * If gWlainMccToSccSwitchMode = 3: Force switch without SAP restart.
 * If gWlainMccToSccSwitchMode = 4: Switch using
 * 					fav channel(s)without SAP restart.
 * If gWlainMccToSccSwitchMode = 5: Force switch without SAP restart.MCC allowed
 *					in exceptional cases.
 * If gWlainMccToSccSwitchMode = 6: Force Switch without SAP restart only in
					user preffered band.
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MCC_TO_SCC_SWITCH CFG_INI_UINT(\
					"gWlanMccToSccSwitchMode", \
					QDF_MCC_TO_SCC_SWITCH_DISABLE, \
					QDF_MCC_TO_SCC_SWITCH_MAX - 1, \
					QDF_MCC_TO_SCC_SWITCH_DISABLE, \
					CFG_VALUE_OR_DEFAULT, \
					"Provides MCC to SCC switch mode")
/*
 * <ini>
 * gSystemPref - Configure wlan system preference for PCL.
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to configure wlan system preference option to help
 * policy manager decide on Preferred Channel List for a new connection.
 * For possible values refer to enum hdd_conc_priority_mode
 *
 * Related: None.
 *
 * Supported Feature: DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CONC_SYS_PREF CFG_INI_UINT(\
					"gSystemPref", 0, 2, 0, \
					CFG_VALUE_OR_DEFAULT, \
					"System preference to predict PCL")
/*
 * <ini>
 * gMaxConcurrentActiveSessions - Maximum number of concurrent connections.
 * @Min: 1
 * @Max: 4
 * @Default: 3
 *
 * This ini is used to configure the maximum number of concurrent connections.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_CONC_CXNS CFG_INI_UINT(\
					"gMaxConcurrentActiveSessions", \
					1, 4, 3, \
					CFG_VALUE_OR_DEFAULT, \
					"Config max num allowed connections")

#define POLICY_MGR_CH_SELECT_POLICY_DEF         0x00000003

/*
 * <ini>
 * channel_select_logic_conc - Set channel selection logic
 * for different concurrency combinations to DBS or inter band
 * MCC. Default is DBS for STA+STA and STA+P2P.
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * 0 - inter-band MCC
 * 1 - DBS
 *
 * BIT 0: STA+STA
 * BIT 1: STA+P2P
 * BIT 2-31: Reserved
 *
 * Supported Feature: STA+STA, STA+P2P
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHNL_SELECT_LOGIC_CONC CFG_INI_UINT(\
					"channel_select_logic_conc",\
					0x00000000, \
					0xFFFFFFFF, \
					POLICY_MGR_CH_SELECT_POLICY_DEF, \
					CFG_VALUE_OR_DEFAULT, \
					"Set channel selection policy for various concurrency")
/*
 * <ini>
 * dbs_selection_policy - Configure dbs selection policy.
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 *  set band preference or Vdev preference.
 *      bit[0] = 0: 5G 2x2 preferred to select 2x2 5G + 1x1 2G DBS mode.
 *      bit[0] = 1: 2G 2x2 preferred to select 2x2 2G + 1x1 5G DBS mode.
 *      bit[1] = 1: vdev priority enabled. The INI "vdev_priority_list" will
 * specify the vdev priority.
 *      bit[1] = 0: vdev priority disabled.
 * This INI only take effect for Genoa dual DBS hw.
 *
 * Supported Feature: DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DBS_SELECTION_PLCY CFG_INI_UINT(\
					    "dbs_selection_policy", \
					    0, 3, 0, \
					    CFG_VALUE_OR_DEFAULT, \
					    "Configure dbs selection policy")
/*
 * <ini>
 * vdev_priority_list - Configure vdev priority list.
 * @Min: 0
 * @Max: 0x4444
 * @Default: 0x4321
 *
 * @vdev_priority_list: vdev priority list
 *      bit[0-3]: pri_id (policy_mgr_pri_id) of highest priority
 *      bit[4-7]: pri_id (policy_mgr_pri_id) of second priority
 *      bit[8-11]: pri_id (policy_mgr_pri_id) of third priority
 *      bit[12-15]: pri_id (policy_mgr_pri_id) of fourth priority
 *      example: 0x4321 - CLI < GO < SAP < STA
 *      vdev priority id mapping:
 *        PM_STA_PRI_ID = 1,
 *        PM_SAP_PRI_ID = 2,
 *        PM_P2P_GO_PRI_ID = 3,
 *        PM_P2P_CLI_PRI_ID = 4,
 * When the previous INI "dbs_selection_policy" bit[1]=1, which means
 * the vdev 2x2 prioritization enabled. Then this INI will be used to
 * specify the vdev type priority list. For example :
 * dbs_selection_policy=0x2
 * vdev_priority_list=0x4312
 * means: default preference 2x2 band is 5G, vdev 2x2 prioritization enabled.
 * And the priority list is CLI < GO < STA < SAP
 *
 * This INI only take effect for Genoa dual DBS hw.
 *
 * Supported Feature: DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VDEV_CUSTOM_PRIORITY_LIST CFG_INI_UINT(\
					"vdev_priority_list", \
					0, 0x4444, 0x4321, \
					CFG_VALUE_OR_DEFAULT, \
					"Configure vdev priority list")
/*
 * <ini>
 * gEnableCustomConcRule1 - Enable custom concurrency rule1.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable custom concurrency rule1.
 * If SAP comes up first and STA comes up later then SAP needs to follow STA's
 * channel.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_CONC_RULE1 CFG_INI_UINT(\
					"gEnableCustomConcRule1", \
					0, 1, 0, \
					CFG_VALUE_OR_DEFAULT, \
					"Enable custom concurrency rule 1")
/*
 * <ini>
 * gEnableCustomConcRule2 - Enable custom concurrency rule2.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable custom concurrency rule2.
 * If P2PGO comes up first and STA comes up later then P2PGO need to follow
 * STA's channel in 5Ghz. In following if condition we are just adding sanity
 * check to make sure that by this time P2PGO's channel is same as STA's
 * channel.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_CONC_RULE2 CFG_INI_UINT(\
					"gEnableCustomConcRule2", \
					0, 1, 0, \
					CFG_VALUE_OR_DEFAULT, \
					"Enable custom concurrency rule 2")
/*
 * <ini>
 * gEnableMCCAdaptiveScheduler - MCC Adaptive Scheduler feature.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable MCC Adaptive Scheduler feature.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_MCC_ADAPTIVE_SCH_ENABLED_NAME CFG_INI_BOOL(\
					"gEnableMCCAdaptiveScheduler", \
					true, \
					"Enable/Disable MCC Adaptive Scheduler")

/*
 * <ini>
 * gEnableStaConnectionIn5Ghz - To enable/disable STA connection in 5G
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable STA connection in 5G band
 *
 * Related: STA
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_STA_CONNECTION_IN_5GHZ CFG_INI_UINT(\
					"gEnableStaConnectionIn5Ghz", \
					0, 1, 1, \
					CFG_VALUE_OR_DEFAULT, \
					"Enable/Disable STA connection in 5G")

/*
 * <ini>
 * gAllowMCCGODiffBI - Allow GO in MCC mode to accept different beacon interval
 * than STA's.
 * @Min: 0
 * @Max: 4
 * @Default: 4
 *
 * This ini is used to allow GO in MCC mode to accept different beacon interval
 * than STA's.
 * Added for Wi-Fi Cert. 5.1.12
 * If gAllowMCCGODiffBI = 1
 *	Set to 1 for WFA certification. GO Beacon interval is not changed.
 *	MCC GO doesn't work well in optimized way. In worst scenario, it may
 *	invite STA disconnection.
 * If gAllowMCCGODiffBI = 2
 *	If set to 2 workaround 1 disassoc all the clients and update beacon
 *	Interval.
 * If gAllowMCCGODiffBI = 3
 *	If set to 3 tear down the P2P link in auto/Non-autonomous -GO case.
 * If gAllowMCCGODiffBI = 4
 *	If set to 4 don't disconnect the P2P client in autonomous/Non-auto-
 *	nomous -GO case update the BI dynamically
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ALLOW_MCC_GO_DIFF_BI \
CFG_INI_UINT("gAllowMCCGODiffBI", 0, 4, 4, CFG_VALUE_OR_DEFAULT, \
	     "Allow GO in MCC mode to accept different BI than STA's")

/*
 * <ini>
 * gEnableOverLapCh - Enables Overlap Channel. If set, allow overlapping
 *                    channels to be selected for the SoftAP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set Overlap Channel
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_OVERLAP_CH \
CFG_INI_UINT("gEnableOverLapCh", 0, 1, 0, CFG_VALUE_OR_DEFAULT, \
	     "Overlap channels are allowed for SAP when flag is set")

/*
 *
 * <ini>
 * gDualMacFeatureDisable - Disable Dual MAC feature.
 * @Min: 0
 * @Max: 6
 * @Default: 6
 *
 * This ini is used to enable/disable dual MAC feature.
 * 0 - enable DBS
 * 1 - disable DBS
 * 2 - disable DBS for connection but keep DBS for scan
 * 3 - disable DBS for connection but keep DBS scan with async
 * scan policy disabled
 * 4 - enable DBS for connection as well as for scan with async
 * scan policy disabled
 * 5 - enable DBS for connection but disable DBS for scan.
 * 6 - enable DBS for connection but disable simultaneous scan
 * from upper layer (DBS scan remains enabled in FW).
 *
 * Note: INI item value should match 'enum dbs_support'
 *
 * Related: None.
 *
 * Supported Feature: DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DUAL_MAC_FEATURE_DISABLE \
CFG_INI_UINT("gDualMacFeatureDisable", 0, 6, 6, CFG_VALUE_OR_DEFAULT, \
	     "This INI is used to enable/disable Dual MAC feature")

/*
 * <ini>
 * g_sta_sap_scc_on_dfs_chan - Allow STA+SAP SCC on DFS channel with master
 * mode support disabled.
 * @Min: 0
 * @Max: 2
 * @Default: 2
 *
 * This ini is used to allow STA+SAP SCC on DFS channel with master mode
 * support disabled, the value is defined by enum PM_AP_DFS_MASTER_MODE.
 * 0 - Disallow STA+SAP SCC on DFS channel
 * 1 - Allow STA+SAP SCC on DFS channel with master mode disabled
 *       This needs gEnableDFSMasterCap enabled to allow SAP SCC with
 *       STA on DFS but dfs master mode disabled. Single SAP is not allowed
 *       on DFS.
 * 2 - enhance "1" with below requirement
 *	 a. Allow single SAP (GO) start on DFS channel.
 *	 b. Allow CAC process on DFS channel in single SAP (GO) mode
 *	 c. Allow DFS radar event process in single SAP (GO) mode
 *	 d. Disallow CAC and radar event process in SAP (GO) + STA mode.
 *	 The value 2 of this ini requires master mode to be enabled so it is
 *	 mandatory to enable the dfs master mode ini gEnableDFSMasterCap
 *	 along with it.
 *
 * Related: None.
 *
 * Supported Feature: Non-DBS, DBS
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_STA_SAP_SCC_ON_DFS_CHAN \
CFG_INI_UINT("g_sta_sap_scc_on_dfs_chan", 0, 2, 2, CFG_VALUE_OR_DEFAULT, \
	     "Allow STA+SAP SCC on DFS channel with master mode disable")

/*
 * <ini>
 * gForce1x1Exception - force 1x1 when connecting to certain peer
 * @Min: 0
 * @Max: 2
 * @Default: 2
 *
 * This INI when enabled will force 1x1 connection with certain peer.
 * The implementation for this ini would be as follows:-
 * Value 0: Even if the AP is present in OUI, 1x1 will not be forced
 * Value 1: If antenna sharing supported, then only do 1x1.
 * Value 2: If AP present in OUI, force 1x1 connection.

 *
 * Related: None
 *
 * Supported Feature: connection
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_FORCE_1X1_FEATURE \
CFG_INI_UINT("gForce1x1Exception", 0, 2, 1, CFG_VALUE_OR_DEFAULT, \
	     "force 1x1 when connecting to certain peer")

/*
 * <ini>
 * gEnableSAPManadatoryChanList - Enable SAP Mandatory channel list
 * Options.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the SAP manadatory chan list
 * 0 - Disable SAP mandatory chan list
 * 1 - Enable SAP mandatory chan list
 *
 * Supported Feature: SAP
 *
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_SAP_MANDATORY_CHAN_LIST \
CFG_INI_UINT("gEnableSAPManadatoryChanList", 0, 1, 0, CFG_VALUE_OR_DEFAULT, \
	     "Enable SAP Mandatory channel list")

/*
 * <ini>
 * g_nan_sap_scc_on_lte_coex_chan - Allow NAN+SAP SCC on LTE coex channel
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to allow NAN+SAP SCC on LTE coex channel
 * 0 - Disallow NAN+SAP SCC on LTE coex channel
 * 1 - Allow NAN+SAP SCC on LTE coex channel
 *
 * Related: Depends on gWlanMccToSccSwitchMode config.
 *
 * Supported Feature: Non-DBS, DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_NAN_SAP_SCC_ON_LTE_COEX_CHAN \
CFG_INI_BOOL("g_nan_sap_scc_on_lte_coex_chan", 1, \
	     "Allow NAN+SAP SCC on LTE coex channel")

/*
 * <ini>
 * g_sta_sap_scc_on_lte_coex_chan - Allow STA+SAP SCC on LTE coex channel
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to allow STA+SAP SCC on LTE coex channel
 * 0 - Disallow STA+SAP SCC on LTE coex channel
 * 1 - Allow STA+SAP SCC on LTE coex channel
 *
 * Related: None.
 *
 * Supported Feature: Non-DBS, DBS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN \
CFG_INI_UINT("g_sta_sap_scc_on_lte_coex_chan", 0, 1, 1, CFG_VALUE_OR_DEFAULT, \
	     "Allow STA+SAP SCC on LTE coex channel")

/*
 * <ini>
 * g_mark_sap_indoor_as_disable - Enable/Disable Indoor channel
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to mark the Indoor channel as
 * disable when SAP start and revert it on SAP stop,
 * so SAP will not turn on indoor channel and
 * sta will not scan/associate and roam on indoor
 * channels.
 *
 * Related: If g_mark_sap_indoor_as_disable set, turn the
 * indoor channels to disable and update Wiphy & fw.
 *
 * Supported Feature: SAP/STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_MARK_INDOOR_AS_DISABLE_FEATURE \
CFG_INI_UINT("g_mark_sap_indoor_as_disable", 0, 1, 0, CFG_VALUE_OR_DEFAULT, \
	     "Enable/Disable Indoor channel")

/*
 * <ini>
 * g_enable_go_force_scc - Enable/Disable force SCC on P2P GO
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini and along with "gWlanMccToSccSwitchMode" is used to enable
 * force SCC on P2P GO interface.
 *
 * Supported Feature: P2P GO
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_P2P_GO_ENABLE_FORCE_SCC \
CFG_INI_UINT("g_enable_go_force_scc", 0, 1, 0, CFG_VALUE_OR_DEFAULT, \
	     "Enable/Disable P2P GO force SCC")

/**
 * <ini>
 * g_pcl_band_priority - Set 5G/6G Channel order
 * Options.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set preference between 5G and 6G channels during
 * PCL population.
 * 0 - Prefer 5G channels, 5G channels will be placed before the 6G channels
 *	in PCL.
 * 1 - Prefer 6G channels, 6G channels will be placed before the 5G channels
 *	in PCL.
 *
 * Supported Feature: STA, SAP
 *
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_PCL_BAND_PRIORITY \
CFG_INI_UINT("g_pcl_band_priority", 0, 1, 0, CFG_VALUE_OR_DEFAULT, \
	     "Set 5G and 6G Channel order")

/*
 * <ini>
 * g_prefer_5g_scc_to_dbs - prefer 5g scc to dbs
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0
 *
 * This ini is used to give higher priority for 5g scc than dbs.
 * It is bitmap per enum policy_mgr_con_mode.
 * For example in GO+STA(5G) mode, when TPUT is onfigured as wlan system
 * preference option, If 5G SCC needs higher priority than dbs, set it as 0x8.
 *
 * Supported Feature: P2P GO
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PREFER_5G_SCC_TO_DBS \
CFG_INI_UINT("g_prefer_5g_scc_to_dbs", 0, 0xFFFFFFFF, 0, CFG_VALUE_OR_DEFAULT, \
	     "5G SCC has higher priority than DBS")

#define CFG_POLICY_MGR_ALL \
		CFG(CFG_MCC_TO_SCC_SWITCH) \
		CFG(CFG_CONC_SYS_PREF) \
		CFG(CFG_MAX_CONC_CXNS) \
		CFG(CFG_DBS_SELECTION_PLCY) \
		CFG(CFG_VDEV_CUSTOM_PRIORITY_LIST) \
		CFG(CFG_CHNL_SELECT_LOGIC_CONC) \
		CFG(CFG_ENABLE_CONC_RULE1) \
		CFG(CFG_ENABLE_CONC_RULE2) \
		CFG(CFG_ENABLE_MCC_ADAPTIVE_SCH_ENABLED_NAME)\
		CFG(CFG_ENABLE_STA_CONNECTION_IN_5GHZ)\
		CFG(CFG_ENABLE_OVERLAP_CH)\
		CFG(CFG_DUAL_MAC_FEATURE_DISABLE)\
		CFG(CFG_STA_SAP_SCC_ON_DFS_CHAN)\
		CFG(CFG_FORCE_1X1_FEATURE)\
		CFG(CFG_ENABLE_SAP_MANDATORY_CHAN_LIST)\
		CFG(CFG_STA_SAP_SCC_ON_LTE_COEX_CHAN)\
		CFG(CFG_NAN_SAP_SCC_ON_LTE_COEX_CHAN) \
		CFG(CFG_MARK_INDOOR_AS_DISABLE_FEATURE)\
		CFG(CFG_ALLOW_MCC_GO_DIFF_BI) \
		CFG(CFG_P2P_GO_ENABLE_FORCE_SCC) \
		CFG(CFG_PCL_BAND_PRIORITY) \
		CFG(CFG_PREFER_5G_SCC_TO_DBS)
#endif
