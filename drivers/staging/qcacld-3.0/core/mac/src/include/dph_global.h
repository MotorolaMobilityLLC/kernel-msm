/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

/*

 *

 * Author:      Sandesh Goel

 * Date:        02/25/02

 * History:-

 * Date            Modified by    Modification Information

 * --------------------------------------------------------------------

 *

 */

#ifndef __DPH_GLOBAL_H__
#define __DPH_GLOBAL_H__

#include "lim_global.h"
#include "sir_mac_prot_def.h"
#include "sir_api.h"

/* DPH Hash Index for BSS(STA's Peer) on station. */
#define DPH_STA_HASH_INDEX_PEER   1

#ifdef WLAN_FEATURE_11W
/* DPH PMF SA Query state for station */
#define DPH_SA_QUERY_NOT_IN_PROGRESS      1
#define DPH_SA_QUERY_IN_PROGRESS          2
#define DPH_SA_QUERY_TIMED_OUT            3
#endif

typedef struct sDphQosParams {
	uint8_t addtsPresent;
	tSirAddtsReqInfo addts;
	tSirMacQosCapabilityStaIE capability;
	/*AP EDCA params, extracted from assoc resp*/
	tSirMacEdcaParamSetIE peer_edca_params;
} tDphQosParams;

/**
 * struct parsed_ies: Parsed IE's of BSS capability
 * @ht_caps: HT caps IE
 * @vht_caps: VHT caps IE
 * @ht_operation: HT operation IE
 * @vht_operation: VHT operation IE
 * @hs20vendor_ie: HS2.0 vendor IE
 *
 * This structure holds the parsed IE of connected BSS
 * and this is not the intersection of BSS and STA
 * capability. For example, if BSS supports 80 MHz
 * and STA connects to BSS in 20 MHz, this structure
 * holds 80 MHz as peer capability.
 */
struct parsed_ies {
	tDot11fIEHTCaps ht_caps;
	tDot11fIEVHTCaps vht_caps;
	tDot11fIEHTInfo ht_operation;
	tDot11fIEVHTOperation vht_operation;
	tDot11fIEhs20vendor_ie hs20vendor_ie;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_op he_operation;
#endif
};

/* STA state node */
typedef struct sDphHashNode {
	/*
	 * This STA valid or not
	 */
	uint8_t valid:1;
	uint8_t qosMode:1;
	uint8_t erpEnabled:1;
	/* This has been added to the dph hash table */
	uint8_t added:1;
	uint8_t shortPreambleEnabled:1;
	uint8_t shortSlotTimeEnabled:1;
	/* set if both ap and sta are wme capable */
	uint8_t wmeEnabled:1;
	/* set if both ap and sta are 11e capable */
	uint8_t lleEnabled:1;
	/* set if both ap and sta are wsm capable */
	uint8_t wsmEnabled:1;
	uint8_t fAniCount:1;
	uint8_t rmfEnabled:1;
	uint8_t ocv_enabled:1;
	/* LIM state */
	struct lim_sta_context mlmStaContext;
	/* qos parameter info */
	tDphQosParams qos;
	/*
	 * All the legacy and airgo supported rates.
	 */
	struct supported_rates supportedRates;
	/* MIMO Power Save */
	tSirMacHTMIMOPowerSaveState htMIMOPSState;
	uint8_t htGreenfield:1;
	uint8_t htShortGI40Mhz:1;
	uint8_t htShortGI20Mhz:1;
	/* DSSS/CCK at 40 MHz: Enabled 1 or Disabled */
	uint8_t htDsssCckRate40MHzSupport:1;
	/* L-SIG TXOP Protection used only if peer support available */
	uint8_t htLsigTXOPProtection:1;
	/*
	 * A-MPDU Density
	 * 000 - No restriction
	 * 001 - 1/8 usec
	 * 010 - 1/4 usec
	 * 011 - 1/2 usec
	 * 100 - 1 usec
	 * 101 - 2 usec
	 * 110 - 4 usec
	 * 111 - 8 usec
	 */
	uint8_t htAMpduDensity:3;
	/* Set to 0 for 3839 octets */
	/* Set to 1 for 7935 octets */
	uint8_t htMaxAmsduLength;
	/* */
	/* Maximum Rx A-MPDU factor */
	uint8_t htMaxRxAMpduFactor:3;
	/*
	 * Recommended Tx Width Set
	 * 0 - use 20 MHz channel (control channel)
	 * 1 - use 40 Mhz channel
	 */
	uint8_t htSupportedChannelWidthSet:1;
	uint8_t htSecondaryChannelOffset:2;
	uint16_t assocId;       /* Association ID */
	uint8_t staAddr[6];
	uint8_t staType;

	uint8_t vhtSupportedChannelWidthSet;
	enum phy_ch_width ch_width;
	uint8_t vhtSupportedRxNss;
	uint8_t vhtBeamFormerCapable;
	uint8_t vht_su_bfee_capable;
	uint8_t vht_mcs_10_11_supp;
	uint8_t vht_160mhz_nss;
	uint8_t vht_80p80mhz_nss;
	uint8_t vht_extended_nss_bw_cap;
#ifdef WLAN_FEATURE_11W
	TX_TIMER pmfSaQueryTimer;
	uint16_t pmfSaQueryCurrentTransId;
	uint16_t pmfSaQueryStartTransId;
	uint8_t pmfSaQueryState;
	uint8_t pmfSaQueryRetryCount;
#endif
	uint8_t htLdpcCapable;
	uint8_t vhtLdpcCapable;
#ifdef FEATURE_WLAN_TDLS
	uint16_t ht_caps;
	uint32_t vht_caps;
#endif
	uint8_t timingMeasCap;
	/* key installed for this STA or not in the firmware */
	uint8_t is_key_installed;
	uint8_t is_disassoc_deauth_in_progress;

	uint8_t nss;
	int8_t del_sta_ctx_rssi;
	bool sta_deletion_in_progress;
	/* Flag indicating connected STA doesn't support ECSA */
	uint8_t non_ecsa_capable;
	struct parsed_ies parsed_ies;

#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_cap he_config;
	uint16_t he_mcs_12_13_map;
	tDot11fIEhe_6ghz_band_cap he_6g_band_cap;
#endif
	/* Peer operation class, extracted from ASSOC request frame*/
	tDot11fIESuppOperatingClasses supp_operating_classes;
	/*
	 * When a station with already an existing dph entry tries to
	 * associate again, the old dph entry will be zeroed out except
	 * for the next pointer. The next pointer must be defined at the
	 * end of the structure.
	 */
	struct sDphHashNode *next;
} tDphHashNode, *tpDphHashNode;

#include "dph_hash_table.h"

/* ------------------------------------------------------------------- */
typedef struct sAniSirDph {
	/* The hash table object */
	struct dph_hash_table dphHashTable;
} tAniSirDph, *tpAniSirDph;

#endif
