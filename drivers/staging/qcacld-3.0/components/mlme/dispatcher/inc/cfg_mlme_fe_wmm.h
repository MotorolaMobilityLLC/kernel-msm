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

#ifndef __CFG_MLME_FE_WMM_H
#define __CFG_MLME_FE_WMM_H

#define CFG_QOS_ENABLED CFG_BOOL( \
		"qos_enabled", \
		0, \
		"QOS Enabled")

#define CFG_WME_ENABLED CFG_BOOL( \
		"wme_enabled", \
		1, \
		"WME Enabled")

#define CFG_MAX_SP_LENGTH CFG_UINT( \
		"max_sp_length", \
		0, \
		3, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"MAX sp length")

#define CFG_WSM_ENABLED CFG_BOOL( \
		"wsm_enabled", \
		0, \
		"WSM Enabled")

#define CFG_EDCA_PROFILE CFG_UINT( \
		"edca_profile", \
		0, \
		4, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"Edca Profile")

/* default TSPEC parameters for AC_VO */
/*
 * <ini>
 * InfraDirAcVo - Set TSPEC direction for VO
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for VO
 *
 * 0 - uplink
 * 1 - direct link
 * 2 - down link
 * 3 - bidirectional link
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_DIR_AC_VO CFG_INI_UINT( \
		"InfraDirAcVo", \
		0, \
		3, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"direction for vo")

/*
 * <ini>
 * InfraNomMsduSizeAcVo - Set normal MSDU size for VO
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x80D0
 *
 * This ini is used to set normal MSDU size for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_NOM_MSDU_SIZE_AC_VO CFG_INI_UINT( \
		"InfraNomMsduSizeAcVo", \
		0x0, \
		0xFFFF, \
		0x80D0, \
		CFG_VALUE_OR_DEFAULT, \
		"MSDU size for VO")

/*
 * <ini>
 * InfraMeanDataRateAcVo - Set mean data rate for VO
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x14500
 *
 * This ini is used to set mean data rate for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MEAN_DATA_RATE_AC_VO CFG_INI_UINT( \
		"InfraMeanDataRateAcVo", \
		0x0, \
		0xFFFFFFFF, \
		0x14500, \
		CFG_VALUE_OR_DEFAULT, \
		"mean data rate for VO")

/*
 * <ini>
 * InfraMinPhyRateAcVo - Set min PHY rate for VO
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MIN_PHY_RATE_AC_VO CFG_INI_UINT( \
		"InfraMinPhyRateAcVo", \
		0x0, \
		0xFFFFFFFF, \
		0x5B8D80, \
		CFG_VALUE_OR_DEFAULT, \
		"min PHY rate for VO")

/*
 * <ini>
 * InfraSbaAcVo - Set surplus bandwidth allowance for VO
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for VO
 *
 * Related: None.
 *
 * Supported Feature: WMM
*
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_SBA_AC_VO CFG_INI_UINT( \
		"InfraSbaAcVo", \
		0x2001, \
		0xFFFF, \
		0x2001, \
		CFG_VALUE_OR_DEFAULT, \
		"surplus bandwidth allowance for VO")
/*
 * <ini>
 * InfraDirAcVi - Set TSPEC direction for VI
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_DIR_AC_VI CFG_INI_UINT( \
		"InfraDirAcVi", \
		0, \
		3, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"TSPEC direction for VI")

/*
 * <ini>
 * InfraNomMsduSizeAcVi - Set normal MSDU size for VI
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_NOM_MSDU_SIZE_AC_VI CFG_INI_UINT( \
		"InfraNomMsduSizeAcVi", \
		0x0, \
		0xFFFF, \
		0x85DC, \
		CFG_VALUE_OR_DEFAULT, \
		"MSDU size for VI")

/*
 * <ini>
 * InfraMeanDataRateAcVi - Set mean data rate for VI
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x57E40
 *
 * This ini is used to set mean data rate for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MEAN_DATA_RATE_AC_VI CFG_INI_UINT( \
		"InfraMeanDataRateAcVi", \
		0x0, \
		0xFFFFFFFF, \
		0x57E40, \
		CFG_VALUE_OR_DEFAULT, \
		"data rate for VI")

/*
 * <ini>
 * InfraMinPhyRateAcVi - Set min PHY rate for VI
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MIN_PHY_RATE_AC_VI CFG_INI_UINT( \
		"InfraMinPhyRateAcVi", \
		0x0, \
		0xFFFFFFFF, \
		0x5B8D80, \
		CFG_VALUE_OR_DEFAULT, \
		"min PHY rate for VI")

/*
 * <ini>
 * InfraSbaAcVi - Set surplus bandwidth allowance for VI
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for VI
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_QOS_WMM_SBA_AC_VI CFG_INI_UINT( \
		"InfraSbaAcVi", \
		0x2001, \
		0xFFFF, \
		0x2001, \
		CFG_VALUE_OR_DEFAULT, \
		"surplus bandwidth allowance for VI")

/*
 * <ini>
 * InfraUapsdVoSrvIntv - Set Uapsd service interval for voice
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 20
 *
 * This ini is used to set Uapsd service interval(in ms) for voice.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_VO_SRV_INTV CFG_INI_UINT( \
		"InfraUapsdVoSrvIntv", \
		0, \
		4294967295UL, \
		20, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd vo srv intv")

/*
 * <ini>
 * InfraUapsdVoSuspIntv - Set Uapsd suspension interval for voice
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval(in ms) for voice.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_VO_SUS_INTV CFG_INI_UINT( \
		"InfraUapsdVoSuspIntv", \
		0, \
		4294967295UL, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd vo sus intv")

/*
 * <ini>
 * InfraUapsdViSrvIntv - Set Uapsd service interval for video
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval(in ms) for video.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_QOS_WMM_UAPSD_VI_SRV_INTV CFG_INI_UINT( \
		"InfraUapsdViSrvIntv", \
		0, \
		4294967295UL, \
		300, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd vi srv intv")

/*
 * <ini>
 * InfraUapsdViSuspIntv - Set Uapsd suspension interval for video
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval(in ms) for video
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_VI_SUS_INTV CFG_INI_UINT( \
		"InfraUapsdViSuspIntv", \
		0, \
		4294967295UL, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd vi sus intv")

/*
 * <ini>
 * InfraDirAcBe - Set TSPEC direction for BE
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for BE
 *
 * 0 - uplink
 * 1 - direct link
 * 2 - down link
 * 3 - bidirectional link
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_DIR_AC_BE CFG_INI_UINT( \
		"InfraDirAcBe", \
		0, \
		3, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"TSPEC direction for BE")

/*
 * <ini>
 * InfraNomMsduSizeAcBe - Set normal MSDU size for BE
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_NOM_MSDU_SIZE_AC_BE CFG_INI_UINT( \
		"InfraNomMsduSizeAcBe", \
		0x0, \
		0xFFFF, \
		0x85DC, \
		CFG_VALUE_OR_DEFAULT, \
		"MSDU size for BE")

/*
 * <ini>
 * InfraMeanDataRateAcBe - Set mean data rate for BE
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x493E0
 *
 * This ini is used to set mean data rate for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MEAN_DATA_RATE_AC_BE CFG_INI_UINT( \
		"InfraMeanDataRateAcBe", \
		0x0, \
		0xFFFFFFFF, \
		0x493E0, \
		CFG_VALUE_OR_DEFAULT, \
		"data rate for BE")

/*
 * <ini>
 * InfraMinPhyRateAcBe - Set min PHY rate for BE
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MIN_PHY_RATE_AC_BE CFG_INI_UINT( \
		"InfraMinPhyRateAcBe", \
		0x0, \
		0xFFFFFFFF, \
		0x5B8D80, \
		CFG_VALUE_OR_DEFAULT, \
		"min PHY rate for BE")

/*
 * <ini>
 * InfraSbaAcBe - Set surplus bandwidth allowance for BE
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_SBA_AC_BE CFG_INI_UINT( \
		"InfraSbaAcBe", \
		0x2001, \
		0xFFFF, \
		0x2001, \
		CFG_VALUE_OR_DEFAULT, \
		"surplus bandwidth allowance for BE")

/*
 * <ini>
 * InfraUapsdBeSrvIntv - Set Uapsd service interval for BE
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval(in ms) for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_BE_SRV_INTV CFG_INI_UINT( \
		"InfraUapsdBeSrvIntv", \
		0, \
		4294967295UL, \
		300, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd be srv intv")

/*
 * <ini>
 * InfraUapsdBeSuspIntv - Set Uapsd suspension interval for BE
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval(in ms) for BE
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_BE_SUS_INTV CFG_INI_UINT( \
		"InfraUapsdBeSuspIntv", \
		0, \
		4294967295UL, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd vi sus intv")

/*
 * <ini>
 * InfraDirAcBk - Set TSPEC direction for BK
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set TSPEC direction for BK
 *
 * 0 - uplink
 * 1 - direct link
 * 2 - down link
 * 3 - bidirectional link
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_DIR_AC_BK CFG_INI_UINT( \
		"InfraDirAcBk", \
		0, \
		3, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"TSPEC direction for BK")

/*
 * <ini>
 * InfraNomMsduSizeAcBk - Set normal MSDU size for BK
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0x85DC
 *
 * This ini is used to set normal MSDU size for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_NOM_MSDU_SIZE_AC_BK CFG_INI_UINT( \
		"InfraNomMsduSizeAcBk", \
		0x0, \
		0xFFFF, \
		0x85DC, \
		CFG_VALUE_OR_DEFAULT, \
		"MSDU size for BK")

/*
 * <ini>
 * InfraMeanDataRateAcBk - Set mean data rate for BK
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x493E0
 *
 * This ini is used to set mean data rate for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MEAN_DATA_RATE_AC_BK CFG_INI_UINT( \
		"InfraMeanDataRateAcBk", \
		0x0, \
		0xFFFFFFFF, \
		0x493E0, \
		CFG_VALUE_OR_DEFAULT, \
		"data rate for BK")

/*
 * <ini>
 * InfraMinPhyRateAcBk - Set min PHY rate for BK
 * @Min: 0x0
 * @Max: 0xFFFFFFFF
 * @Default: 0x5B8D80
 *
 * This ini is used to set min PHY rate for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MIN_PHY_RATE_AC_BK CFG_INI_UINT( \
		"InfraMinPhyRateAcBk", \
		0x0, \
		0xFFFFFFFF, \
		0x5B8D80, \
		CFG_VALUE_OR_DEFAULT, \
		"min PHY rate for BK")

/*
 * <ini>
 * InfraSbaAcBk - Set surplus bandwidth allowance for BK
 * @Min: 0x2001
 * @Max: 0xFFFF
 * @Default: 0x2001
 *
 * This ini is used to set surplus bandwidth allowance for BK
 *
 * The 13 least significant bits (LSBs) indicate the decimal part while the
 * three MSBs indicate the integer part of the number.
 *
 * A value of 1 indicates that no additional allocation of time is requested.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_SBA_AC_BK CFG_INI_UINT( \
		"InfraSbaAcBk", \
		0x2001, \
		0xFFFF, \
		0x2001, \
		CFG_VALUE_OR_DEFAULT, \
		"surplus bandwidth allowance for BK")

/*
 * <ini>
 * InfraUapsdBkSrvIntv - Set Uapsd service interval for BK
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 300
 *
 * This ini is used to set Uapsd service interval(in ms) for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_BK_SRV_INTV CFG_INI_UINT( \
		"InfraUapsdBkSrvIntv", \
		0, \
		4294967295UL, \
		300, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd bk srv intv")

/*
 * <ini>
 * InfraUapsdBkSuspIntv - Set Uapsd suspension interval for BK
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 2000
 *
 * This ini is used to set Uapsd suspension interval(in ms) for BK
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_BK_SUS_INTV CFG_INI_UINT( \
		"InfraUapsdBkSuspIntv", \
		0, \
		4294967295UL, \
		2000, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra uapsd bk sus intv")

/* WMM configuration */
/*
 * <ini>
 * WmmIsEnabled - Enable WMM feature
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to enable/disable WMM.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_MODE CFG_INI_UINT( \
		"WmmIsEnabled", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Enable WMM feature")

/*
 * <ini>
 * 80211eIsEnabled - Enable 802.11e feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable 802.11e.
 *
 * Related: None.
 *
 * Supported Feature: 802.11e
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_80211E_ENABLED CFG_INI_BOOL( \
		"80211eIsEnabled", \
		0, \
		"Enable 802.11e feature")

/*
 * <ini>
 * UapsdMask - To setup U-APSD mask for ACs
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x00
 *
 * This ini is used to setup U-APSD mask for ACs.
 *
 * Bit 0 set, Voice both deliver/trigger enabled
 * Bit 1 set, Video both deliver/trigger enabled
 * Bit 2 set, Background both deliver/trigger enabled
 * Bit 3 set, Best Effort both deliver/trigger enabled
 * others, reserved
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_UAPSD_MASK CFG_INI_UINT( \
		"UapsdMask", \
		0x00, \
		0xFF, \
		0x00, \
		CFG_VALUE_OR_DEFAULT, \
		"setup U-APSD mask for ACs")

/*
 * <ini>
 * ImplicitQosIsEnabled - Enableimplicit QOS
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable implicit QOS.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_IMPLICIT_SETUP_ENABLED CFG_INI_BOOL( \
		"ImplicitQosIsEnabled", \
		0, \
		"Enable implicit QOS")

#ifdef FEATURE_WLAN_ESE
/*
 * <ini>
 * InfraInactivityInterval - To setup Infra Inactivity Interval for ACs
 * @Min: 0
 * @Max: 4294967295UL
 * @Default: 0
 *
 * This ini is used to setup Infra Inactivity Interval for
 * ACs.
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_INACTIVITY_INTERVAL CFG_INI_UINT( \
		"InfraInactivityInterval", \
		0, \
		4294967295UL, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Infra Inactivity Interval")

#define QOS_CFG CFG(CFG_QOS_WMM_INACTIVITY_INTERVAL)
#else

#define QOS_CFG

#endif /* FEATURE_WLAN_ESE */

/*
 * <ini>
 * burstSizeDefinition - Set TS burst size
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set TS burst size
 *
 * 0 - burst is disabled
 * 1 - burst is enabled
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_BURST_SIZE_DEFN CFG_INI_BOOL( \
		"burstSizeDefinition", \
		0, \
		"burst size definition")

/*
 * <ini>
 * tsInfoAckPolicy - Set TS ack policy
 * @Min: 0x00
 * @Max: 0x01
 * @Default: 0x00
 *
 * This ini is used to set TS ack policy
 *
 * TS Info Ack Policy can be either of the following values:
 *
 * 0 - normal ack
 * 1 - HT immediate block ack
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_WMM_TS_INFO_ACK_POLICY CFG_INI_UINT( \
		"tsInfoAckPolicy", \
		0, \
		1, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"ts info ack policy")

/*
 * <ini>
 * gAddTSWhenACMIsOff - Set ACM value for AC
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set ACM value for AC
 *
 * Related: None.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_QOS_ADDTS_WHEN_ACM_IS_OFF CFG_INI_BOOL( \
		"gAddTSWhenACMIsOff", \
		0, \
		"ACM value for AC")

/*
 * <ini>
 * DelayedTriggerFrmInt - UAPSD delay interval
 * @Min: 1
 * @Max: 4294967295
 * @Default: 3000
 *
 * This parameter controls the delay interval(in ms) of UAPSD auto trigger.
 *
 * Supported Feature: WMM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TL_DELAYED_TRGR_FRM_INTERVAL CFG_INI_UINT( \
		"DelayedTriggerFrmInt", \
		1, \
		4294967295UL, \
		3000, \
		CFG_VALUE_OR_DEFAULT, \
		"UAPSD auto trigger Interval")

#define CFG_WMM_PARAMS_ALL \
	CFG(CFG_QOS_ENABLED) \
	CFG(CFG_WME_ENABLED) \
	CFG(CFG_MAX_SP_LENGTH) \
	CFG(CFG_WSM_ENABLED) \
	CFG(CFG_EDCA_PROFILE) \
	CFG(CFG_QOS_WMM_DIR_AC_VO) \
	CFG(CFG_QOS_WMM_NOM_MSDU_SIZE_AC_VO) \
	CFG(CFG_QOS_WMM_MEAN_DATA_RATE_AC_VO) \
	CFG(CFG_QOS_WMM_MIN_PHY_RATE_AC_VO) \
	CFG(CFG_QOS_WMM_SBA_AC_VO) \
	CFG(CFG_QOS_WMM_UAPSD_VO_SRV_INTV) \
	CFG(CFG_QOS_WMM_UAPSD_VO_SUS_INTV) \
	CFG(CFG_QOS_WMM_DIR_AC_VI) \
	CFG(CFG_QOS_WMM_NOM_MSDU_SIZE_AC_VI) \
	CFG(CFG_QOS_WMM_MEAN_DATA_RATE_AC_VI) \
	CFG(CFG_QOS_WMM_MIN_PHY_RATE_AC_VI) \
	CFG(CFG_QOS_WMM_SBA_AC_VI) \
	CFG(CFG_QOS_WMM_UAPSD_VI_SRV_INTV) \
	CFG(CFG_QOS_WMM_UAPSD_VI_SUS_INTV) \
	CFG(CFG_QOS_WMM_DIR_AC_BE) \
	CFG(CFG_QOS_WMM_NOM_MSDU_SIZE_AC_BE) \
	CFG(CFG_QOS_WMM_MEAN_DATA_RATE_AC_BE) \
	CFG(CFG_QOS_WMM_MIN_PHY_RATE_AC_BE) \
	CFG(CFG_QOS_WMM_SBA_AC_BE) \
	CFG(CFG_QOS_WMM_UAPSD_BE_SRV_INTV) \
	CFG(CFG_QOS_WMM_UAPSD_BE_SUS_INTV) \
	CFG(CFG_QOS_WMM_DIR_AC_BK) \
	CFG(CFG_QOS_WMM_NOM_MSDU_SIZE_AC_BK) \
	CFG(CFG_QOS_WMM_MEAN_DATA_RATE_AC_BK) \
	CFG(CFG_QOS_WMM_MIN_PHY_RATE_AC_BK) \
	CFG(CFG_QOS_WMM_SBA_AC_BK) \
	CFG(CFG_QOS_WMM_UAPSD_BK_SRV_INTV) \
	CFG(CFG_QOS_WMM_UAPSD_BK_SUS_INTV) \
	CFG(CFG_QOS_WMM_MODE) \
	CFG(CFG_QOS_WMM_80211E_ENABLED) \
	CFG(CFG_QOS_WMM_UAPSD_MASK) \
	CFG(CFG_QOS_WMM_IMPLICIT_SETUP_ENABLED) \
	QOS_CFG \
	CFG(CFG_QOS_WMM_BURST_SIZE_DEFN) \
	CFG(CFG_QOS_WMM_TS_INFO_ACK_POLICY) \
	CFG(CFG_QOS_ADDTS_WHEN_ACM_IS_OFF) \
	CFG(CFG_TL_DELAYED_TRGR_FRM_INTERVAL)

#endif /* __CFG_MLME_FE_WMM_H */
