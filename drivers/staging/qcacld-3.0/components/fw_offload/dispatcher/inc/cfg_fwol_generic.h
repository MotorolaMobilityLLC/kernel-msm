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

#ifndef __CFG_FWOL_GENERIC_H
#define __CFG_FWOL_GENERIC_H


/*
 *
 * <ini>
 * gEnableANI - Enable Adaptive Noise Immunity
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable Adaptive Noise Immunity.
 *
 * Related: None
 *
 * Supported Feature: ANI
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_ANI CFG_INI_BOOL( \
		"gEnableANI", \
		1, \
		"Enable/Disable Adaptive Noise Immunity")

/*
 * <ini>
 * gSetRTSForSIFSBursting - set rts for sifs bursting
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini set rts for sifs bursting
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_RTS_FOR_SIFS_BURSTING CFG_INI_BOOL( \
		"gSetRTSForSIFSBursting", \
		0, \
		"Set rts for sifs bursting")

/*
 * <ini>
 * sifs_burst_mask - Set sifs burst mask
 * @Min: 0
 * @Max: 3
 * @Default: 1
 *
 * This ini is used to set 11n and legacy(non 11n/wmm)
 * sifs burst. Especially under running multi stream
 * traffic test case, it can be useful to let the low
 * priority AC, or legacy mode device, or the specified
 * AC to aggressively contend air medium, then have a
 * obvious improvement of throughput. Bit0 is the switch
 * of sifs burst, it must be set if want to enable sifs
 * burst, Bit1 is for legacy mode.
 * Supported configuration:
 * 0: disabled
 * 1: enabled, but disabled for legacy mode
 * 3: all enabled
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_SIFS_BURST CFG_INI_UINT( \
		"sifs_burst_mask", \
		0, \
		3, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"Set SIFS burst mask")

/*
 * <ini>
 * gMaxMPDUsInAMPDU - max mpdus in ampdu
 * @Min: 0
 * @Max: 64
 * @Default: 0
 *
 * This ini configure max mpdus in ampdu
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_MPDUS_IN_AMPDU CFG_INI_INT( \
		"gMaxMPDUsInAMPDU", \
		0, \
		64, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"This ini configure max mpdus in ampdu")

/*
 * <ini>
 * gEnableFastPwrTransition - Configuration for fast power transition
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini supported values:
 * 0x0: Phy register retention disabled (Higher timeline, Good for power)
 * 0x1: Phy register retention statically enabled
 * 0x2: Phy register retention enabled/disabled dynamically
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_PHY_REG CFG_INI_UINT( \
		"gEnableFastPwrTransition", \
		0x0, \
		0x2, \
		0x0, \
		CFG_VALUE_OR_DEFAULT, \
		"Configuration for fast power transition")

/*
 * <ini>
 * gUpperBrssiThresh - Sets Upper threshold for beacon RSSI
 * @Min: 36
 * @Max: 66
 * @Default: 46
 *
 * This ini sets Upper beacon threshold for beacon RSSI in FW
 * Used to reduced RX chainmask in FW, once this threshold is
 * reached FW will switch to 1X1 (Single chain).
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_UPPER_BRSSI_THRESH CFG_INI_UINT( \
		"gUpperBrssiThresh", \
		36, \
		66, \
		46, \
		CFG_VALUE_OR_DEFAULT, \
		"Sets Upper threshold for beacon RSSI")

/*
 * <ini>
 * gLowerBrssiThresh - Sets Lower threshold for beacon RSSI
 * @Min: 6
 * @Max: 36
 * @Default: 26
 *
 * This ini sets Lower beacon threshold for beacon RSSI in FW
 * Used to increase RX chainmask in FW, once this threshold is
 * reached FW will switch to 2X2 chain.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LOWER_BRSSI_THRESH CFG_INI_UINT( \
		"gLowerBrssiThresh", \
		6, \
		36, \
		26, \
		CFG_VALUE_OR_DEFAULT, \
		"Sets Lower threshold for beacon RSSI")

/*
 * <ini>
 * gDtim1ChRxEnable - Enable/Disable DTIM 1Chrx feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini Enables or Disables DTIM 1CHRX feature in FW
 * If this flag is set FW enables shutting off one chain
 * while going to power save.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DTIM_1CHRX_ENABLE CFG_INI_BOOL( \
		"gDtim1ChRxEnable", \
		1, \
		"Enable/Disable DTIM 1Chrx feature")

/*
 * <ini>
 * gEnableAlternativeChainmask - Enable Co-Ex Alternative Chainmask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the Co-ex Alternative Chainmask
 * feature via the WMI_PDEV_PARAM_ALTERNATIVE_CHAINMASK_SCHEME
 * firmware parameter.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_COEX_ALT_CHAINMASK CFG_INI_BOOL( \
		"gEnableAlternativeChainmask", \
		0, \
		"Enable Co-Ex Alternative Chainmask")

/*
 * <ini>
 * gEnableSmartChainmask - Enable Smart Chainmask
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the Smart Chainmask feature via
 * the WMI_PDEV_PARAM_SMART_CHAINMASK_SCHEME firmware parameter.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_SMART_CHAINMASK CFG_INI_BOOL( \
		"gEnableSmartChainmask", \
		0, \
		"Enable/disable the Smart Chainmask feature")

/*
 * <ini>
 * gEnableRTSProfiles - It will use configuring different RTS profiles
 * @Min: 0
 * @Max: 66
 * @Default: 33
 *
 * This ini used for configuring different RTS profiles
 * to firmware.
 * Following are the valid values for the rts profile:
 * RTSCTS_DISABLED				0
 * NOT_ALLOWED					1
 * NOT_ALLOWED					2
 * RTSCTS_DISABLED				16
 * RTSCTS_ENABLED_4_SECOND_RATESERIES		17
 * CTS2SELF_ENABLED_4_SECOND_RATESERIES		18
 * RTSCTS_DISABLED				32
 * RTSCTS_ENABLED_4_SWRETRIES			33
 * CTS2SELF_ENABLED_4_SWRETRIES			34
 * NOT_ALLOWED					48
 * NOT_ALLOWED					49
 * NOT_ALLOWED					50
 * RTSCTS_DISABLED				64
 * RTSCTS_ENABLED_4_ALL_RATESERIES		65
 * CTS2SELF_ENABLED_4_ALL_RATESERIES		66
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_ENABLE_FW_RTS_PROFILE CFG_INI_INT( \
		"gEnableRTSProfiles", \
		0, \
		66, \
		33, \
		CFG_VALUE_OR_DEFAULT, \
		"It is used to configure different RTS profiles")

/* <ini>
 * gFwDebugLogLevel - Firmware debug log level
 * @Min: 0
 * @Max: 255
 * @Default: 3
 *
 * This option controls the level of firmware debug log. Default value is
 * DBGLOG_WARN, which is to enable error and warning logs.
 *
 * Related: None
 *
 * Supported Features: Debugging
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_FW_DEBUG_LOG_LEVEL CFG_INI_INT( \
		"gFwDebugLogLevel", \
		0, \
		255, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"enable error and warning logs by default")

/* <ini>
 * gFwDebugLogType - Firmware debug log type
 * @Min: 0
 * @Max: 255
 * @Default: 3
 *
 * This option controls how driver is to give the firmware logs to net link
 * when cnss_diag service is started.
 *
 * Related: None
 *
 * Supported Features: Debugging
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_ENABLE_FW_LOG_TYPE CFG_INI_INT( \
		"gFwDebugLogType", \
		0, \
		255, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Default value to be given to the net link cnss_diag service")

/*
 * <ini>
 * gFwDebugModuleLoglevel - modulized firmware debug log level
 * @Min: N/A
 * @Max: N/A
 * @Default: N/A
 *
 * This ini is used to set modulized firmware debug log level.
 * FW module log level input string format looks like below:
 * gFwDebugModuleLoglevel="<FW Module ID>,<Log Level>,..."
 * For example:
 * gFwDebugModuleLoglevel="1,0,2,1,3,2,4,3,5,4,6,5,7,6"
 * The above input string means:
 * For FW module ID 1 enable log level 0
 * For FW module ID 2 enable log level 1
 * For FW module ID 3 enable log level 2
 * For FW module ID 4 enable log level 3
 * For FW module ID 5 enable log level 4
 * For FW module ID 6 enable log level 5
 * For FW module ID 7 enable log level 6
 * For valid values of log levels check enum DBGLOG_LOG_LVL and
 * for valid values of module ids check enum WLAN_MODULE_ID.
 *
 * Related: None
 *
 * Supported Feature: Debugging
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define FW_MODULE_LOG_LEVEL_STRING_LENGTH  (512)
#define CFG_ENABLE_FW_MODULE_LOG_LEVEL CFG_INI_STRING( \
	"gFwDebugModuleLoglevel", \
	0, \
	FW_MODULE_LOG_LEVEL_STRING_LENGTH, \
	"1,1,2,1,3,1,4,1,5,1,8,1,9,1,13,1,14,1,17,1,18,1,19,1,22,1,26,1,28,1," \
	"29,1,31,1,36,1,38,1,46,1,47,1,50,1,52,1,53,1,56,1,60,1,61,1", \
	"Set modulized firmware debug log level")

/*
 * <ini>
 * gFwDebugWowModuleLoglevel - modulized firmware wow debug log level
 * @Min: N/A
 * @Max: N/A
 * @Default: N/A
 *
 * This ini is used to set modulized firmware wow debug log level.
 * FW module log level input string format looks like below:
 * gFwDebugWowModuleLoglevel="<FW Module ID>,<Log Level>,..."
 * For example:
 * gFwDebugWowModuleLoglevel="1,0,2,1,3,2,4,3,5,4,6,5,7,6"
 * The above input string means:
 * For FW module ID 1 enable log level 0
 * For FW module ID 2 enable log level 1
 * For FW module ID 3 enable log level 2
 * For FW module ID 4 enable log level 3
 * For FW module ID 5 enable log level 4
 * For FW module ID 6 enable log level 5
 * For FW module ID 7 enable log level 6
 * For valid values of log levels check enum DBGLOG_LOG_LVL and
 * for valid values of module ids check enum WLAN_MODULE_ID.
 *
 * Related: None
 *
 * Supported Feature: Debugging
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_FW_WOW_MODULE_LOG_LEVEL CFG_INI_STRING( \
	"gFwDebugWowModuleLoglevel", \
	0, \
	FW_MODULE_LOG_LEVEL_STRING_LENGTH, \
	"5,3,18,3,31,3,36,3", \
	"Set modulized firmware wow debug log level")

#ifdef FEATURE_WLAN_RA_FILTERING
/* <ini>
 * gRAFilterEnable
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_RA_FILTER_ENABLE CFG_INI_BOOL( \
		"gRAFilterEnable", \
		1, \
		"Enable RA Filter")
#else
#define CFG_RA_FILTER_ENABLE
#endif

/* <ini>
 * gtsf_gpio_pin
 * @Min: 0
 * @Max: 254
 * @Default: 255
 *
 * GPIO pin to toggle when capture tsf
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SET_TSF_GPIO_PIN CFG_INI_INT( \
		"gtsf_gpio_pin", \
		0, \
		255, \
		255, \
		CFG_VALUE_OR_DEFAULT, \
		"GPIO pin to toggle when capture tsf")

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ
/* <ini>
 * gtsf_irq_host_gpio_pin
 * @Min: 0
 * @Max: 254
 * @Default: 255
 *
 * TSF irq GPIO pin of host platform
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SET_TSF_IRQ_HOST_GPIO_PIN CFG_INI_INT( \
		"gtsf_irq_host_gpio_pin", \
		0, \
		255, \
		255, \
		CFG_VALUE_OR_DEFAULT, \
		"TSF irq GPIO pin of host platform")

#define __CFG_SET_TSF_IRQ_HOST_GPIO_PIN CFG(CFG_SET_TSF_IRQ_HOST_GPIO_PIN)
#else
#define __CFG_SET_TSF_IRQ_HOST_GPIO_PIN
#endif

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
/*
 * <ini>
 * gtsf_sync_host_gpio_pin
 * @Min: 0
 * @Max: 254
 * @Default: 255
 *
 * TSF sync GPIO pin of host platform
 *
 * The driver will use this gpio on host platform
 * to drive the TSF sync pin on wlan chip.
 * Toggling this gpio  will generate a strobe to fw
 * for latching TSF.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SET_TSF_SYNC_HOST_GPIO_PIN CFG_INI_UINT( \
		"gtsf_sync_host_gpio_pin", \
		0, \
		254, \
		255, \
		CFG_VALUE_OR_DEFAULT, \
		"TSF sync GPIO pin of host platform")

#define __CFG_SET_TSF_SYNC_HOST_GPIO_PIN CFG(CFG_SET_TSF_SYNC_HOST_GPIO_PIN)
#else
#define __CFG_SET_TSF_SYNC_HOST_GPIO_PIN
#endif

#if defined(WLAN_FEATURE_TSF) && defined(WLAN_FEATURE_TSF_PLUS)
/* <ini>
 * gtsf_ptp_options: TSF Plus feature options
 * @Min: 0
 * @Max: 0xff
 * @Default: 0xf
 *
 * CFG_SET_TSF_PTP_OPT_RX                    (0x1)
 * CFG_SET_TSF_PTP_OPT_TX                    (0x2)
 * CFG_SET_TSF_PTP_OPT_RAW                   (0x4)
 * CFG_SET_TSF_DBG_FS                        (0x8)
 * CFG_SET_TSF_PTP_OPT_TSF64_TX              (0x10)
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SET_TSF_PTP_OPT_RX                    (0x1)
#define CFG_SET_TSF_PTP_OPT_TX                    (0x2)
#define CFG_SET_TSF_PTP_OPT_RAW                   (0x4)
#define CFG_SET_TSF_DBG_FS                        (0x8)
#define CFG_SET_TSF_PTP_OPT_TSF64_TX              (0x10)

#define CFG_SET_TSF_PTP_OPT CFG_INI_UINT( \
		"gtsf_ptp_options", \
		0, \
		0xff, \
		0xf, \
		CFG_VALUE_OR_DEFAULT, \
		"TSF Plus feature options")

#define __CFG_SET_TSF_PTP_OPT CFG(CFG_SET_TSF_PTP_OPT)
#else
#define __CFG_SET_TSF_PTP_OPT
#endif

#ifdef DHCP_SERVER_OFFLOAD
/* <ini>
 * gDHCPServerOffloadEnable
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * DHCP Server offload support
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DHCP_SERVER_OFFLOAD_SUPPORT CFG_INI_BOOL( \
		"gDHCPServerOffloadEnable", \
		0, \
		"DHCP Server offload support")

/* <ini>
 * gDHCPMaxNumClients
 * @Min: 1
 * @Max: 8
 * @Default: 8
 *
 * Number of DHCP server offload clients
 *
 * Related: None
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT CFG_INI_INT( \
		"gDHCPMaxNumClients", \
		1, \
		8, \
		8, \
		CFG_VALUE_OR_DEFAULT, \
		"Number of DHCP server offload clients")

#define CFG_FWOL_DHCP \
		CFG(CFG_DHCP_SERVER_OFFLOAD_SUPPORT) \
		CFG(CFG_DHCP_SERVER_OFFLOAD_NUM_CLIENT)

#else
#define CFG_FWOL_DHCP
#endif

/*
 * <ini>
 * gEnableLPRx - Enable/Disable LPRx
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini Enables or disables the LPRx in FW
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_LPRX CFG_INI_BOOL( \
		"gEnableLPRx", \
		1, \
		"LPRx control")

#ifdef WLAN_FEATURE_SAE
/*
 * <ini>
 * sae_enabled - Enable/Disable SAE support in driver
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SAE support in driver
 * Driver will update config to supplicant based on this config.
 *
 * Related: None
 *
 * Supported Feature: SAE
 * Usage: External
 *
 * </ini>
 */

#define CFG_IS_SAE_ENABLED CFG_INI_BOOL( \
		"sae_enabled", \
		1, \
		"SAE feature control")
#define __CFG_IS_SAE_ENABLED CFG(CFG_IS_SAE_ENABLED)
#else
#define __CFG_IS_SAE_ENABLED
#endif

/*
 * <ini>
 * gcmp_enabled - ini to enable/disable GCMP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Currently Firmware update the sequence number for each TID with 2^3
 * because of security issues. But with this PN mechanism, throughput drop
 * is observed. With this ini FW takes the decision to trade off between
 * security and throughput
 *
 * Supported Feature: STA/SAP/P2P
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_GCMP CFG_INI_BOOL( \
		"gcmp_enabled", \
		1, \
		"GCMP Feature control param")

/*
 * <ini>
 * gTxSchDelay - Enable/Disable Tx sch delay
 * @Min: 0
 * @Max: 5
 * @Default: 0
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TX_SCH_DELAY CFG_INI_UINT( \
		"gTxSchDelay", \
		0, \
		5, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Enable/Disable Tx sch delay")

/*
 * <ini>
 * gEnableSecondaryRate - Enable/Disable Secondary Retry Rate feature subset
 *
 * @Min: 0x0
 * @Max: 0x3F
 * @Default: 0x17
 *
 * It is a 32 bit value such that the various bits represent as below -
 * Bit-0 : is Enable/Disable Control for "PPDU Secondary Retry Support"
 * Bit-1 : is Enable/Disable Control for "RTS Black/White-listing Support"
 * Bit-2 : is Enable/Disable Control for "Higher MCS retry restriction
 *         on XRETRY failures"
 * Bit 3-5 : is "Xretry threshold" to use
 * Bit 3~31 : reserved for future use.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SECONDARY_RATE CFG_INI_UINT( \
		"gEnableSecondaryRate", \
		0, \
		0x3f, \
		0x17, \
		CFG_VALUE_OR_DEFAULT, \
		"Secondary Retry Rate feature subset control")

/*
 * <ini>
 * sap_xlna_bypass - Enable/Disable xLNA bypass
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable SAP xLNA bypass in the FW
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_SET_SAP_XLNA_BYPASS CFG_INI_BOOL( \
		"xlna_bypass", \
		0, \
		"SAP xLNA bypass control")

/*
 * <ini>
 * g_enable_ilp - ILP HW Block Configuration
 * @Min: 0
 * @Max: 3
 * @Default: 2
 *
 * This ini is used to configure ILP HW block with various options
 * 0: disable
 * 1: perf settings
 * 2: max power saving
 * 3: balanced settings
 *
 * Related: none
 *
 * Supported Feature: STA/SAP
 *
 * Usage: Internal
 *
 * <ini>
 */

#define CFG_SET_ENABLE_ILP CFG_INI_UINT( \
		"g_enable_ilp", \
		0, \
		3, \
		2, \
		CFG_VALUE_OR_DEFAULT, \
		"ILP configuration")

/*
 * <ini>
 * g_disable_hw_assist - Flag to disable HW assist feature
 * @Default: 0
 *
 * This ini is used to enable/disable the HW assist feature in FW
 *
 * Related: none
 *
 * Supported Feature: STA/SAP
 *
 * Usage: External
 *
 * <ini>
 */

#define CFG_DISABLE_HW_ASSIST CFG_INI_BOOL( \
		"g_disable_hw_assist", \
		0, \
		"Disable HW assist feature in FW")

/*
 * <ini>
 * g_enable_pci_gen - To enable pci gen switch
 * @Default: 0
 *
 * Related: None
 *
 * Supported Feature: PCI
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_PCI_GEN CFG_INI_BOOL( \
		"g_enable_pci_gen", \
		0, \
		"enable pci gen")

#define CFG_FWOL_GENERIC_ALL \
	CFG_FWOL_DHCP \
	CFG(CFG_ENABLE_ANI) \
	CFG(CFG_SET_RTS_FOR_SIFS_BURSTING) \
	CFG(CFG_SET_SIFS_BURST) \
	CFG(CFG_MAX_MPDUS_IN_AMPDU) \
	CFG(CFG_ENABLE_PHY_REG) \
	CFG(CFG_UPPER_BRSSI_THRESH) \
	CFG(CFG_LOWER_BRSSI_THRESH) \
	CFG(CFG_DTIM_1CHRX_ENABLE) \
	CFG(CFG_ENABLE_COEX_ALT_CHAINMASK) \
	CFG(CFG_ENABLE_SMART_CHAINMASK) \
	CFG(CFG_ENABLE_FW_RTS_PROFILE) \
	CFG(CFG_ENABLE_FW_DEBUG_LOG_LEVEL) \
	CFG(CFG_ENABLE_FW_LOG_TYPE) \
	CFG(CFG_ENABLE_FW_MODULE_LOG_LEVEL) \
	CFG(CFG_RA_FILTER_ENABLE) \
	CFG(CFG_SET_TSF_GPIO_PIN) \
	__CFG_SET_TSF_IRQ_HOST_GPIO_PIN \
	__CFG_SET_TSF_SYNC_HOST_GPIO_PIN \
	__CFG_SET_TSF_PTP_OPT \
	CFG(CFG_LPRX) \
	__CFG_IS_SAE_ENABLED \
	CFG(CFG_ENABLE_GCMP) \
	CFG(CFG_TX_SCH_DELAY) \
	CFG(CFG_ENABLE_SECONDARY_RATE) \
	CFG(CFG_SET_SAP_XLNA_BYPASS) \
	CFG(CFG_SET_ENABLE_ILP) \
	CFG(CFG_ENABLE_FW_WOW_MODULE_LOG_LEVEL) \
	CFG(CFG_DISABLE_HW_ASSIST) \
	CFG(CFG_ENABLE_PCI_GEN)

#endif
