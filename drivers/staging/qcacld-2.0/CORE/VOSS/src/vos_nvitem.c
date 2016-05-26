/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


/*============================================================================
  FILE:         vos_nvitem.c
  OVERVIEW:     This source file contains definitions for vOS NV Item APIs
  DEPENDENCIES: NV, remote API client, WinCE REX
============================================================================*/
/*============================================================================
  EDIT HISTORY FOR MODULE
============================================================================*/
// the following is used to disable warning for having too many labels in
// the 'nv_items_enum_type'

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "vos_types.h"
#include "aniGlobal.h"
#include "vos_nvitem.h"
#include "vos_trace.h"
#include "vos_api.h"
#include "wlan_hdd_misc.h"
#include "vos_sched.h"
#include "sme_Api.h"
#include "wlan_hdd_main.h"
#include <net/cfg80211.h>
#include "regdomain.h"
#include "regdomain_common.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)) && !defined(WITH_BACKPORTS)
#define IEEE80211_CHAN_NO_80MHZ		1<<7
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
#define IEEE80211_CHAN_PASSIVE_SCAN IEEE80211_CHAN_NO_IR
#define IEEE80211_CHAN_NO_IBSS IEEE80211_CHAN_NO_IR
#endif

static v_REGDOMAIN_t temp_reg_domain = REGDOMAIN_COUNT;
/* true if init happens thru init time driver hint */
static v_BOOL_t init_by_driver = VOS_FALSE;
/* true if init happens thru init time  callback from regulatory core.
   this should be set to true during driver reload */
static v_BOOL_t init_by_reg_core = VOS_FALSE;


/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define MAX_COUNTRY_COUNT        300
#define REG_WAIT_TIME            50
/*
 * This is a set of common rules used by our world regulatory domains.
 * We have 12 world regulatory domains. To save space we consolidate
 * the regulatory domains in 5 structures by frequency and change
 * the flags on our reg_notifier() on a case by case basis.
 */

#define REG_RULE_2412_2462    REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)

#define REG_RULE_2467_2472    REG_RULE(2467-10, 2472+10, 40, 0, 20, \
	NL80211_RRF_PASSIVE_SCAN)

#define REG_RULE_2484         REG_RULE(2484-10, 2484+10, 40, 0, 20, \
	NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_OFDM)

#define REG_RULE_5180_5320    REG_RULE(5180-10, 5320+10, 80, 0, 20, \
	NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5500_5720    REG_RULE(5500-10, 5720+10, 80, 0, 20, \
	NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_5745_5925    REG_RULE(5745-10, 5925+10, 80, 0, 20, \
	NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_2GHZ_CH01_11 REG_RULE_2412_2462

#define REG_RULE_2GHZ_CH12_13 REG_RULE_2467_2472

#define REG_RULE_2GHZ_ALL     REG_RULE_2412_2462,\
        REG_RULE_2467_2472,\
        REG_RULE_2484

#define REG_RULE_5GHZ_ALL     REG_RULE_5180_5320,\
        REG_RULE_5500_5720,\
        REG_RULE_5745_5925

#define REG_RULE_5GHZ_NO_MIDBAND   REG_RULE_5180_5320,\
        REG_RULE_5745_5925


#define WORLD_SKU_MASK          0x00F0
#define WORLD_SKU_PREFIX        0x0060

/**
 * struct bonded_chan
 * @start_ch: start channel
 * @end_ch: end channel
 */
struct bonded_chan {
	uint16_t start_ch;
	uint16_t end_ch;
};

static const struct bonded_chan bonded_chan_40mhz_array[] = {
	{36, 40},
	{44, 48},
	{52, 56},
	{60, 64},
	{100, 104},
	{108, 112},
	{116, 120},
	{124, 128},
	{132, 136},
	{140, 144},
	{149, 153},
	{157, 161}
};

static const struct bonded_chan bonded_chan_80mhz_array[] = {
	{36, 48},
	{52, 64},
	{100, 112},
	{116, 128},
	{132, 144},
	{149, 161}
};

static const enum phy_ch_width next_lower_bw[] = {
	[CH_WIDTH_80MHZ] = CH_WIDTH_40MHZ,
	[CH_WIDTH_40MHZ] = CH_WIDTH_20MHZ,
	[CH_WIDTH_20MHZ] = CH_WIDTH_10MHZ,
	[CH_WIDTH_10MHZ] = CH_WIDTH_5MHZ,
	[CH_WIDTH_5MHZ] = CH_WIDTH_INVALID
};

static const struct ieee80211_regdomain vos_world_regdom_60_61_62 = {
   .n_reg_rules = 6,
   .alpha2 =  "00",
   .reg_rules = {
      REG_RULE_2GHZ_ALL,
      REG_RULE_5GHZ_ALL,
   }
};

static const struct ieee80211_regdomain vos_world_regdom_63_65 = {
   .n_reg_rules = 4,
   .alpha2 =  "00",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_2GHZ_CH12_13,
      REG_RULE_5GHZ_NO_MIDBAND,
   }
};

static const struct ieee80211_regdomain vos_world_regdom_64 = {
   .n_reg_rules = 3,
   .alpha2 =  "00",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_5GHZ_NO_MIDBAND,
   }
};

static const struct ieee80211_regdomain vos_world_regdom_66_69 = {
   .n_reg_rules = 4,
   .alpha2 =  "00",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_5GHZ_ALL,
   }
};

static const struct ieee80211_regdomain vos_world_regdom_67_68_6A_6C = {
   .n_reg_rules = 5,
   .alpha2 =  "00",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_2GHZ_CH12_13,
      REG_RULE_5GHZ_ALL,
   }
};

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
// this wrapper structure is identical to nv_cmd_type except the
// data_ptr type is changed void* to avoid exceeding the debug information
// module size as there are too many elements within nv_items_type union

// structure for code and regulatory domain of a single country
typedef struct
{
   v_U8_t            regDomain;
   v_COUNTRYCODE_t   countryCode;
} CountryInfo_t;
// structure of table to map country code and regulatory domain
typedef struct
{
   v_U16_t           countryCount;
   CountryInfo_t     countryInfo[MAX_COUNTRY_COUNT];
} CountryInfoTable_t;
/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

static struct
chan_to_ht_40_index_map chan_to_ht_40_index[NUM_20MHZ_RF_CHANNELS] =
{
  /* ht_40_minus_index, ht_40_plus_index */
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_3},    //RF_CHAN_1,
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_4},    //RF_CHAN_2,
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_5},    //RF_CHAN_3,
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_6},    //RF_CHAN_4,
  {RF_CHAN_BOND_3, RF_CHAN_BOND_7},        //RF_CHAN_5,
  {RF_CHAN_BOND_4, RF_CHAN_BOND_8},        //RF_CHAN_6,
  {RF_CHAN_BOND_5, RF_CHAN_BOND_9},        //RF_CHAN_7,
  {RF_CHAN_BOND_6, RF_CHAN_BOND_10},       //RF_CHAN_8,
  {RF_CHAN_BOND_7, RF_CHAN_BOND_11},       //RF_CHAN_9,
  {RF_CHAN_BOND_8, INVALID_RF_CHANNEL},    //RF_CHAN_10,
  {RF_CHAN_BOND_9, INVALID_RF_CHANNEL},    //RF_CHAN_11,
  {RF_CHAN_BOND_10, INVALID_RF_CHANNEL},   //RF_CHAN_12,
  {RF_CHAN_BOND_11, INVALID_RF_CHANNEL},   //RF_CHAN_13,
  {INVALID_RF_CHANNEL, INVALID_RF_CHANNEL},//RF_CHAN_14,
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_38},   //RF_CHAN_36,
  {RF_CHAN_BOND_38, RF_CHAN_BOND_42},      //RF_CHAN_40,
  {RF_CHAN_BOND_42, RF_CHAN_BOND_46},      //RF_CHAN_44,
  {RF_CHAN_BOND_46, RF_CHAN_BOND_50},      //RF_CHAN_48,
  {RF_CHAN_BOND_50, RF_CHAN_BOND_54},      //RF_CHAN_52,
  {RF_CHAN_BOND_54, RF_CHAN_BOND_58},      //RF_CHAN_56,
  {RF_CHAN_BOND_58, RF_CHAN_BOND_62},      //RF_CHAN_60,
  {RF_CHAN_BOND_62, INVALID_RF_CHANNEL},   //RF_CHAN_64,
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_102},  //RF_CHAN_100,
  {RF_CHAN_BOND_102, RF_CHAN_BOND_106},    //RF_CHAN_104,
  {RF_CHAN_BOND_106, RF_CHAN_BOND_110},    //RF_CHAN_108,
  {RF_CHAN_BOND_110, RF_CHAN_BOND_114},    //RF_CHAN_112,
  {RF_CHAN_BOND_114, RF_CHAN_BOND_118},    //RF_CHAN_116,
  {RF_CHAN_BOND_118, RF_CHAN_BOND_122},    //RF_CHAN_120,
  {RF_CHAN_BOND_122, RF_CHAN_BOND_126},    //RF_CHAN_124,
  {RF_CHAN_BOND_126, RF_CHAN_BOND_130},    //RF_CHAN_128,
  {RF_CHAN_BOND_130, RF_CHAN_BOND_134},    //RF_CHAN_132,
  {RF_CHAN_BOND_134, RF_CHAN_BOND_138},    //RF_CHAN_136,
  {RF_CHAN_BOND_138, RF_CHAN_BOND_142},    //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
  {RF_CHAN_BOND_142, INVALID_RF_CHANNEL},  //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
  {INVALID_RF_CHANNEL, RF_CHAN_BOND_151},  //RF_CHAN_149,
  {RF_CHAN_BOND_151, RF_CHAN_BOND_155},    //RF_CHAN_153,
  {RF_CHAN_BOND_155, RF_CHAN_BOND_159},    //RF_CHAN_157,
  {RF_CHAN_BOND_159, RF_CHAN_BOND_163},    //RF_CHAN_161,
  {RF_CHAN_BOND_163, INVALID_RF_CHANNEL},  //RF_CHAN_165,
};

// cache of country info table;
// this is re-initialized from data on binary file
// loaded on driver initialization if available


static CountryInfoTable_t countryInfoTable =
{
    /* the first entry in the table is always the world domain */
    138,
    {
      {REGDOMAIN_WORLD, {'0', '0'}}, // WORLD DOMAIN
      {REGDOMAIN_FCC, {'A', 'D'}}, // ANDORRA
      {REGDOMAIN_ETSI, {'A', 'E'}}, //UAE
      {REGDOMAIN_ETSI, {'A', 'L'}}, //ALBANIA
      {REGDOMAIN_ETSI, {'A', 'M'}}, //ARMENIA
      {REGDOMAIN_ETSI, {'A', 'N'}}, //NETHERLANDS ANTILLES
      {REGDOMAIN_FCC, {'A', 'R'}}, //ARGENTINA
      {REGDOMAIN_FCC, {'A', 'S'}}, //AMERICAN SOMOA
      {REGDOMAIN_ETSI, {'A', 'T'}}, //AUSTRIA
      {REGDOMAIN_FCC, {'A', 'U'}}, //AUSTRALIA
      {REGDOMAIN_ETSI , {'A', 'W'}}, //ARUBA
      {REGDOMAIN_ETSI,  {'A', 'Z'}}, //AZERBAIJAN
      {REGDOMAIN_ETSI, {'B', 'A'}}, //BOSNIA AND HERZEGOVINA
      {REGDOMAIN_FCC, {'B', 'B'}}, //BARBADOS
      {REGDOMAIN_ETSI, {'B', 'D'}}, //BANGLADESH
      {REGDOMAIN_ETSI, { 'B', 'E'}}, //BELGIUM
      {REGDOMAIN_ETSI, {'B', 'G'}}, //BULGARIA
      {REGDOMAIN_ETSI, {'B', 'H'}}, //BAHRAIN
      {REGDOMAIN_ETSI, {'B', 'L'}}, //
      {REGDOMAIN_FCC, {'B', 'M'}}, //BERMUDA
      {REGDOMAIN_ETSI, {'B', 'N'}}, //BRUNEI DARUSSALAM
      {REGDOMAIN_ETSI, {'B', 'O'}}, //BOLIVIA
      {REGDOMAIN_ETSI, {'B', 'R'}}, //BRAZIL
      {REGDOMAIN_FCC, {'B', 'S'}}, //BAHAMAS
      {REGDOMAIN_ETSI, {'B', 'Y'}}, //BELARUS
      {REGDOMAIN_ETSI, {'B', 'Z'}}, //BELIZE
      {REGDOMAIN_FCC, {'C', 'A'}}, //CANADA
      {REGDOMAIN_ETSI, {'C', 'H'}}, //SWITZERLAND
      {REGDOMAIN_ETSI, {'C', 'L'}}, //CHILE
      {REGDOMAIN_FCC, {'C', 'N'}}, //CHINA
      {REGDOMAIN_FCC, {'C', 'O'}}, //COLOMBIA
      {REGDOMAIN_ETSI, {'C', 'R'}}, //COSTA RICA
      {REGDOMAIN_ETSI, {'C', 'S'}},
      {REGDOMAIN_ETSI, {'C', 'Y'}}, //CYPRUS
      {REGDOMAIN_ETSI, {'C', 'Z'}}, //CZECH REPUBLIC
      {REGDOMAIN_ETSI, {'D', 'E'}}, //GERMANY
      {REGDOMAIN_ETSI, {'D', 'K'}}, //DENMARK
      {REGDOMAIN_FCC, {'D', 'O'}}, //DOMINICAN REPUBLIC
      {REGDOMAIN_ETSI, {'D', 'Z'}}, //ALGERIA
      {REGDOMAIN_ETSI, {'E', 'C'}}, //ECUADOR
      {REGDOMAIN_ETSI, {'E', 'E'}}, //ESTONIA
      {REGDOMAIN_ETSI, {'E', 'G'}}, //EGYPT
      {REGDOMAIN_ETSI, {'E', 'S'}}, //SPAIN
      {REGDOMAIN_ETSI, {'F', 'I'}}, //FINLAND
      {REGDOMAIN_ETSI, {'F', 'R'}}, //FRANCE
      {REGDOMAIN_ETSI, {'G', 'B'}}, //UNITED KINGDOM
      {REGDOMAIN_FCC, {'G', 'D'}},  //GRENADA
      {REGDOMAIN_ETSI, {'G', 'E'}}, //GEORGIA
      {REGDOMAIN_ETSI, {'G', 'F'}}, //FRENCH GUIANA
      {REGDOMAIN_ETSI, {'G', 'L'}}, //GREENLAND
      {REGDOMAIN_ETSI, {'G', 'P'}}, //GUADELOUPE
      {REGDOMAIN_ETSI, {'G', 'R'}}, //GREECE
      {REGDOMAIN_FCC, {'G', 'T'}},  //GUATEMALA
      {REGDOMAIN_FCC, {'G', 'U'}},  //GUAM
      {REGDOMAIN_ETSI, {'H', 'U'}}, //HUNGARY
      {REGDOMAIN_ETSI, {'I', 'D'}},  //INDONESIA
      {REGDOMAIN_ETSI, {'I', 'E'}}, //IRELAND
      {REGDOMAIN_ETSI, {'I', 'L'}}, //ISRAEL
      {REGDOMAIN_ETSI, {'I', 'N'}}, //INDIA
      {REGDOMAIN_ETSI, {'I', 'R'}}, //IRAN, ISLAMIC REPUBLIC OF
      {REGDOMAIN_ETSI, {'I', 'S'}}, //ICELNAD
      {REGDOMAIN_ETSI, {'I', 'T'}}, //ITALY
      {REGDOMAIN_FCC, {'J', 'M'}},  //JAMAICA
      {REGDOMAIN_JAPAN, {'J', 'P'}}, //JAPAN
      {REGDOMAIN_ETSI, {'J', 'O'}}, //JORDAN
      {REGDOMAIN_ETSI, {'K', 'E'}}, //KENYA
      {REGDOMAIN_ETSI, {'K', 'H'}}, //CAMBODIA
      {REGDOMAIN_ETSI, {'K', 'P'}}, //KOREA, DEMOCRATIC PEOPLE's REPUBLIC OF
      {REGDOMAIN_ETSI, {'K', 'R'}}, //KOREA, REPUBLIC OF
      {REGDOMAIN_ETSI, {'K', 'W'}}, //KUWAIT
      {REGDOMAIN_ETSI, {'K', 'Z'}}, //KAZAKHSTAN
      {REGDOMAIN_ETSI, {'L', 'B'}}, //LEBANON
      {REGDOMAIN_ETSI, {'L', 'I'}}, //LIECHTENSTEIN
      {REGDOMAIN_ETSI, {'L', 'K'}}, //SRI-LANKA
      {REGDOMAIN_ETSI, {'L', 'T'}}, //LITHUANIA
      {REGDOMAIN_ETSI, {'L', 'U'}}, //LUXEMBOURG
      {REGDOMAIN_ETSI, {'L','V'}},  //LATVIA
      {REGDOMAIN_ETSI, {'M', 'A'}}, //MOROCCO
      {REGDOMAIN_ETSI, {'M', 'C'}}, //MONACO
      {REGDOMAIN_ETSI, {'M', 'K'}}, //MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF
      {REGDOMAIN_FCC, {'M','N'}}, //MONGOLIA
      {REGDOMAIN_FCC, {'M', 'O'}}, //MACAO
      {REGDOMAIN_FCC, {'M', 'P'}}, //NORTHERN MARIANA ISLANDS
      {REGDOMAIN_ETSI, {'M', 'Q'}}, //MARTINIQUE
      {REGDOMAIN_FCC, {'M', 'T'}}, //MALTA
      {REGDOMAIN_ETSI, {'M', 'U'}}, //MAURITIUS
      {REGDOMAIN_ETSI, {'M', 'W'}}, //MALAWI
      {REGDOMAIN_FCC, {'M', 'X'}}, //MEXICO
      {REGDOMAIN_ETSI, {'M', 'Y'}}, //MALAYSIA
      {REGDOMAIN_ETSI, {'N', 'G'}}, //NIGERIA
      {REGDOMAIN_FCC, {'N', 'I'}}, //NICARAGUA
      {REGDOMAIN_ETSI, {'N', 'L'}}, //NETHERLANDS
      {REGDOMAIN_ETSI, {'N', 'O'}}, //NORWAY
      {REGDOMAIN_ETSI, {'N', 'P'}}, //NEPAL
      {REGDOMAIN_FCC, {'N', 'Z'}}, //NEW-ZEALAND
      {REGDOMAIN_FCC, {'O', 'M'}}, //OMAN
      {REGDOMAIN_FCC, {'P', 'A'}}, //PANAMA
      {REGDOMAIN_ETSI, {'P', 'E'}}, //PERU
      {REGDOMAIN_ETSI, {'P', 'F'}}, //FRENCH POLYNESIA
      {REGDOMAIN_ETSI, {'P', 'G'}}, //PAPUA NEW GUINEA
      {REGDOMAIN_FCC, {'P', 'H'}}, //PHILIPPINES
      {REGDOMAIN_ETSI, {'P', 'K'}}, //PAKISTAN
      {REGDOMAIN_ETSI, {'P', 'L'}}, //POLAND
      {REGDOMAIN_FCC, {'P', 'R'}}, //PUERTO RICO
      {REGDOMAIN_FCC, {'P', 'S'}}, //PALESTINIAN TERRITORY, OCCUPIED
      {REGDOMAIN_ETSI, {'P', 'T'}}, //PORTUGAL
      {REGDOMAIN_FCC, {'P', 'Y'}}, //PARAGUAY
      {REGDOMAIN_ETSI, {'Q', 'A'}}, //QATAR
      {REGDOMAIN_ETSI, {'R', 'E'}}, //REUNION
      {REGDOMAIN_ETSI, {'R', 'O'}}, //ROMAINIA
      {REGDOMAIN_ETSI, {'R', 'S'}}, //SERBIA
      {REGDOMAIN_ETSI, {'R', 'U'}}, //RUSSIA
      {REGDOMAIN_FCC, {'R', 'W'}}, //RWANDA
      {REGDOMAIN_ETSI, {'S', 'A'}}, //SAUDI ARABIA
      {REGDOMAIN_ETSI, {'S', 'E'}}, //SWEDEN
      {REGDOMAIN_ETSI, {'S', 'G'}}, //SINGAPORE
      {REGDOMAIN_ETSI, {'S', 'I'}}, //SLOVENNIA
      {REGDOMAIN_ETSI, {'S', 'K'}}, //SLOVAKIA
      {REGDOMAIN_ETSI, {'S', 'V'}}, //EL SALVADOR
      {REGDOMAIN_ETSI, {'S', 'Y'}}, //SYRIAN ARAB REPUBLIC
      {REGDOMAIN_ETSI, {'T', 'H'}}, //THAILAND
      {REGDOMAIN_ETSI, {'T', 'N'}}, //TUNISIA
      {REGDOMAIN_ETSI, {'T', 'R'}}, //TURKEY
      {REGDOMAIN_ETSI, {'T', 'T'}}, //TRINIDAD AND TOBAGO
      {REGDOMAIN_FCC, {'T', 'W'}}, //TAIWAN, PRIVINCE OF CHINA
      {REGDOMAIN_ETSI, {'T', 'Z'}}, //TANZANIA, UNITED REPUBLIC OF
      {REGDOMAIN_ETSI, {'U', 'A'}}, //UKRAINE
      {REGDOMAIN_ETSI, {'U', 'G'}}, //UGANDA
      {REGDOMAIN_FCC, {'U', 'S'}}, //USA
      {REGDOMAIN_ETSI, {'U', 'Y'}}, //URUGUAY
      {REGDOMAIN_ETSI, {'U', 'Z'}}, //UZBEKISTAN
      {REGDOMAIN_ETSI, {'V', 'E'}}, //VENEZUELA
      {REGDOMAIN_FCC, {'V', 'I'}}, //VIRGIN ISLANDS, US
      {REGDOMAIN_ETSI, {'V', 'N'}}, //VIETNAM
      {REGDOMAIN_ETSI, {'Y', 'E'}}, //YEMEN
      {REGDOMAIN_ETSI, {'Y', 'T'}}, //MAYOTTE
      {REGDOMAIN_ETSI, {'Z', 'A'}}, //SOUTH AFRICA
      {REGDOMAIN_ETSI, {'Z', 'W'}}, //ZIMBABWE
    }
};

typedef struct nvEFSTable_s
{
   sHalNv     halnv;
} nvEFSTable_t;

static nvEFSTable_t *pnvEFSTable;

const tRfChannelProps rfChannels[NUM_RF_CHANNELS] =
{
    //RF_SUBBAND_2_4_GHZ
    //freq, chan#, band
    { 2412, 1  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_1,
    { 2417, 2  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_2,
    { 2422, 3  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_3,
    { 2427, 4  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_4,
    { 2432, 5  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_5,
    { 2437, 6  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_6,
    { 2442, 7  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_7,
    { 2447, 8  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_8,
    { 2452, 9  , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_9,
    { 2457, 10 , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_10,
    { 2462, 11 , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_11,
    { 2467, 12 , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_12,
    { 2472, 13 , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_13,
    { 2484, 14 , RF_SUBBAND_2_4_GHZ},        //RF_CHAN_14,
    { 5180, 36 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_36,
    { 5200, 40 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_40,
    { 5220, 44 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_44,
    { 5240, 48 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_48,
    { 5260, 52 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_52,
    { 5280, 56 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_56,
    { 5300, 60 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_60,
    { 5320, 64 , RF_SUBBAND_5_LOW_GHZ},      //RF_CHAN_64,
    { 5500, 100, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_100,
    { 5520, 104, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_104,
    { 5540, 108, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_108,
    { 5560, 112, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_112,
    { 5580, 116, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_116,
    { 5600, 120, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_120,
    { 5620, 124, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_124,
    { 5640, 128, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_128,
    { 5660, 132, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_132,
    { 5680, 136, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_136,
    { 5700, 140, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
    { 5720, 144, RF_SUBBAND_5_MID_GHZ},      //RF_CHAN_144,
#endif /* FEATURE_WLAN_CH144 */
    { 5745, 149, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_149,
    { 5765, 153, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_153,
    { 5785, 157, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_157,
    { 5805, 161, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_161,
    { 5825, 165, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_165,

    /* 5.9GHz 10 MHz bandwidth (802.11p) */
    { 5852, 170, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_170,
    { 5855, 171, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_171,
    { 5860, 172, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_172,
    { 5865, 173, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_173,
    { 5870, 174, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_174,
    { 5875, 175, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_175,
    { 5880, 176, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_176,
    { 5885, 177, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_177,
    { 5890, 178, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_178,
    { 5895, 179, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_179,
    { 5900, 180, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_180,
    { 5905, 181, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_181,
    { 5910, 182, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_182,
    { 5915, 183, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_183,
    { 5920, 184, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_184,

    { 2422, 3  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_3,
    { 2427, 4  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_4,
    { 2432, 5  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_5,
    { 2437, 6  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_6,
    { 2442, 7  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_7,
    { 2447, 8  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_8,
    { 2452, 9  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_9,
    { 2457, 10 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_10,
    { 2462, 11 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_11,
    { 5190, 38 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_38,
    { 5210, 42 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_42,
    { 5230, 46 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_46,
    { 5250, 50 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_50,
    { 5270, 54 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_54,
    { 5290, 58 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_58,
    { 5310, 62 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_62,
    { 5510, 102, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_102,
    { 5530, 106, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_106,
    { 5550, 110, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_110,
    { 5570, 114, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_114,
    { 5590, 118, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_118,
    { 5610, 122, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_122,
    { 5630, 126, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_126,
    { 5650, 130, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_130,
    { 5670, 134, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_134,
    { 5690, 138, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_138,
#ifdef FEATURE_WLAN_CH144
    { 5710, 142, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_142,
#endif /* FEATURE_WLAN_CH144 */
    { 5755, 151, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_151,
    { 5775, 155, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_155,
    { 5795, 159, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_159,
    { 5815, 163, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_163,
};

#define VOS_IS_CHANNEL_5GHZ(chan_num) \
	((chan_num >= rfChannels[RF_CHAN_36].channelNum) && \
	 (chan_num <= rfChannels[RF_CHAN_184].channelNum))
#define VOS_IS_CHANNEL_24GHZ(chan_num) \
	((chan_num >= rfChannels[RF_CHAN_1].channelNum) && \
	 (chan_num <= rfChannels[RF_CHAN_14].channelNum))

extern const sHalNv nvDefaults;

const sRegulatoryChannel * regChannels = nvDefaults.tables.regDomains[0].channels;

static inline bool is_wwr_sku(u16 regd)
{
   return ((regd & COUNTRY_ERD_FLAG) != COUNTRY_ERD_FLAG) &&
      (((regd & WORLD_SKU_MASK) == WORLD_SKU_PREFIX) ||
       (regd == WORLD));
}

bool is_world_regd(u_int32_t regd)
{
   return is_wwr_sku(regd & ~WORLDWIDE_ROAMING_FLAG);
}

static const struct ieee80211_regdomain *vos_default_world_regdomain(void)
{
   /* this is the most restrictive */
    return &vos_world_regdom_64;
}

static const struct ieee80211_regdomain *vos_custom_world_regdomain(void)
{
   /* this is the most restrictive */
    return &vos_world_regdom_60_61_62;
}

/**
 * voss_DomainIdtoString(): converts Reg domain enum to string.
 * @domainIdCurrent: Reg domain enum value.
 */
const char * voss_DomainIdtoString(v_U8_t domainIdCurrent)
{
    switch (domainIdCurrent)
    {
        CASE_RETURN_STRING( REGDOMAIN_FCC );
        CASE_RETURN_STRING( REGDOMAIN_ETSI );
        CASE_RETURN_STRING( REGDOMAIN_JAPAN );
        CASE_RETURN_STRING( REGDOMAIN_WORLD );
        CASE_RETURN_STRING( REGDOMAIN_N_AMER_EXC_FCC );
        CASE_RETURN_STRING( REGDOMAIN_APAC );
        CASE_RETURN_STRING( REGDOMAIN_KOREA );
        CASE_RETURN_STRING( REGDOMAIN_HI_5GHZ );
        CASE_RETURN_STRING( REGDOMAIN_NO_5GHZ );
        CASE_RETURN_STRING( REGDOMAIN_COUNT );
        default:
            return "Regulation Domain Unknown";
    }
}

static const
struct ieee80211_regdomain *vos_world_regdomain(struct regulatory *reg)
{
   REG_DMN_PAIR_MAPPING *regpair;
   regpair = (REG_DMN_PAIR_MAPPING *)reg->regpair;
   switch (regpair->regDmnEnum) {
      case 0x60:
      case 0x61:
      case 0x62:
         return &vos_world_regdom_60_61_62;
      case 0x63:
      case 0x65:
         return &vos_world_regdom_63_65;
      case 0x64:
         return &vos_world_regdom_64;
      case 0x66:
      case 0x69:
         return &vos_world_regdom_66_69;
      case 0x67:
      case 0x68:
      case 0x6A:
      case 0x6C:
         return &vos_world_regdom_67_68_6A_6C;
      default:
         WARN_ON(1);
         return vos_default_world_regdomain();
   }
}


static int regd_init_wiphy(hdd_context_t *pHddCtx, struct regulatory *reg,
			   struct wiphy *wiphy)
{
	const struct ieee80211_regdomain *regd;

	if (is_world_regd(reg->reg_domain)) {
		regd = vos_world_regdomain(reg);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
	} else if (pHddCtx->cfg_ini->fRegChangeDefCountry) {
		regd = vos_custom_world_regdomain();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
		wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif

	} else {
		regd = vos_default_world_regdomain();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
		wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
#else
		wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
#endif
	}

	/*
	 * save the original driver regulatory flags
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
	pHddCtx->reg.reg_flags = wiphy->regulatory_flags;
#else
	pHddCtx->reg.reg_flags = wiphy->flags;
#endif

	wiphy_apply_custom_regulatory(wiphy, regd);

	/*
	 * restore the driver regulatory flags since
	 * wiphy_apply_custom_regulatory may have
	 * changed them
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
	wiphy->regulatory_flags = pHddCtx->reg.reg_flags;
#else
	wiphy->flags = pHddCtx->reg.reg_flags;
#endif

	return 0;
}

static int reg_init_from_eeprom(hdd_context_t *pHddCtx, struct regulatory *reg,
				struct wiphy *wiphy)
{
	int ret_val = 0;

	ret_val = regdmn_get_country_alpha2(reg);
	if (ret_val) {
		adf_os_print(KERN_ERR "Error in getting country code\n");
		return ret_val;
	}

	reg->cc_src = COUNTRY_CODE_SET_BY_DRIVER;

	/* update default country code */
	pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[0] =
		reg->alpha2[0];
	pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[1] =
		reg->alpha2[1];

	regd_init_wiphy(pHddCtx, reg, wiphy);

	return ret_val;
}

static void vos_update_reg_info(hdd_context_t *pHddCtx)
{
    u_int32_t country_code;
    country_code = regdmn_find_ctry_by_name(pHddCtx->reg.alpha2);
    pHddCtx->reg.reg_domain = COUNTRY_ERD_FLAG;
    pHddCtx->reg.reg_domain |= country_code;
    regdmn_get_country_alpha2(&pHddCtx->reg);
    return;
}

/**------------------------------------------------------------------------
  \brief vos_nv_open() - Open NV operation
         Read NV bin file and prepare NV common structure
  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_open(void)
{
    /* Allocate memory to global NV table */
    pnvEFSTable = (nvEFSTable_t *)vos_mem_malloc(sizeof(nvEFSTable_t));
    if ( NULL == pnvEFSTable )
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s : failed to allocate memory for NV", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    /*Copying the NV defaults */
    vos_mem_copy(&(pnvEFSTable->halnv), &nvDefaults, sizeof(sHalNv));

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_nv_close(void)
{
    vos_mem_free(pnvEFSTable);
    pnvEFSTable=NULL;
    return VOS_STATUS_SUCCESS;
}

/**
 * vos_search_5g_bonded_chan_array() - get ptr to bonded channel
 * @oper_ch: operating channel number
 * @bonded_chan_ar: bonded channel array
 * @bonded_chan_ptr_ptr: bonded channel ptr ptr
 *
 * Return: eNVChannelEnabledType
 */
static eNVChannelEnabledType vos_search_5g_bonded_chan_array(
	uint32_t oper_chan, const struct bonded_chan bonded_chan_ar[],
	uint16_t array_size, const struct bonded_chan
	**bonded_chan_ptr_ptr)
{
	int i;
	uint8_t chan_num;
	const struct bonded_chan *bonded_chan_ptr = NULL;
	eNVChannelEnabledType chan_state = NV_CHANNEL_INVALID;
	eNVChannelEnabledType temp_chan_state;

	for (i = 0; i < array_size; i++) {
		if ((oper_chan >= bonded_chan_ar[i].start_ch) &&
		    (oper_chan <= bonded_chan_ar[i].end_ch)) {
			bonded_chan_ptr =  &(bonded_chan_ar[i]);
			break;
		}
	}

	if (NULL == bonded_chan_ptr)
		return chan_state;

	*bonded_chan_ptr_ptr = bonded_chan_ptr;
	chan_num =  bonded_chan_ptr->start_ch;
	while (chan_num <= bonded_chan_ptr->end_ch) {
		temp_chan_state = vos_nv_getChannelEnabledState(chan_num);
		if (temp_chan_state < chan_state)
			chan_state = temp_chan_state;
		chan_num = chan_num + 4;
	}

	return chan_state;
}

/**
 * vos_search_5g_bonded_channel() - get the 5G bonded channel state
 * @chan_num: channel number
 * @ch_width: channel width
 * @bonded_chan_ptr_ptr: bonded channel ptr ptr
 *
 * Return: channel state
 */
static eNVChannelEnabledType vos_search_5g_bonded_channel(uint32_t chan_num,
						enum phy_ch_width ch_width,
						const struct bonded_chan
						**bonded_chan_ptr_ptr)
{

	if (CH_WIDTH_80MHZ == ch_width)
		return vos_search_5g_bonded_chan_array(chan_num,
			bonded_chan_80mhz_array,
			ARRAY_SIZE(bonded_chan_80mhz_array),
			bonded_chan_ptr_ptr);
	else if (CH_WIDTH_40MHZ == ch_width)
		return vos_search_5g_bonded_chan_array(chan_num,
			bonded_chan_40mhz_array,
			ARRAY_SIZE(bonded_chan_40mhz_array),
			bonded_chan_ptr_ptr);
	else
		return vos_nv_getChannelEnabledState(chan_num);
}

/**
 * vos_get_5g_bonded_channel_state() - get the 5G bonded channel state
 * @chan_num: channel number
 * @ch_width: channel width
 *
 * Return: channel state
 */
eNVChannelEnabledType vos_get_5g_bonded_channel_state(
	uint16_t chan_num,
	enum phy_ch_width ch_width,
	const struct bonded_chan *bonded_chan_ptr)
{
	bool bw_enabled = false;
	uint32_t flags;

	if (CH_WIDTH_80MHZ < ch_width)
		return NV_CHANNEL_INVALID;

	flags = vos_nv_get_channel_flags(chan_num);

	if (CH_WIDTH_5MHZ == ch_width) {
		bw_enabled = true;
	} else if (CH_WIDTH_10MHZ == ch_width) {
		bw_enabled = !(flags &
				IEEE80211_CHAN_NO_10MHZ);
	} else if (CH_WIDTH_20MHZ == ch_width) {
		bw_enabled = !(flags &
				IEEE80211_CHAN_NO_20MHZ);
	} else if (CH_WIDTH_40MHZ == ch_width) {
		if (chan_num == bonded_chan_ptr->start_ch)
			bw_enabled =
				!(flags & IEEE80211_CHAN_NO_HT40PLUS);
		else
			bw_enabled =
				!(flags & IEEE80211_CHAN_NO_HT40MINUS);
	} else if (CH_WIDTH_80MHZ == ch_width) {
		bw_enabled = !(flags &
				IEEE80211_CHAN_NO_80MHZ);
	}

	if (bw_enabled)
		return NV_CHANNEL_ENABLE;
	else
		return NV_CHANNEL_DISABLE;
}

/**
 * vos_set_sec_chan_offset_vht80() - set sec channel offset for vht80
 * @oper_ch: operating channel
 * @center_chan: center channel for operating channel
 * @ch_params: channel parameters
 *
 * Return: void
 */
static inline void vos_set_sec_chan_offset_vht80(uint16_t oper_ch,
	uint16_t center_chan, struct ch_params_s *ch_params)
{
	if ((oper_ch + 2 ) == center_chan)
		ch_params->sec_ch_offset =
			PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
	else if ((oper_ch + 6 ) == center_chan)
		ch_params->sec_ch_offset =
			PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
	else if ((oper_ch - 2 ) == center_chan)
		ch_params->sec_ch_offset =
			PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
	else if ((oper_ch - 6 ) == center_chan)
		ch_params->sec_ch_offset =
			PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
}

/**
 * vos_set_5g_channel_params() - set the 5G bonded channel parameters
 * @oper_ch: operating channel
 * @ch_params: channel parameters
 *
 * Return: void
 */
static void vos_set_5g_channel_params(uint16_t oper_ch,
				      struct ch_params_s *ch_params)
{
	eNVChannelEnabledType chan_state = NV_CHANNEL_ENABLE;
	const struct bonded_chan *bonded_chan_ptr;
	uint16_t center_chan;

	if (CH_WIDTH_MAX <= ch_params->ch_width)
		ch_params->ch_width = CH_WIDTH_80MHZ;

	while (ch_params->ch_width < CH_WIDTH_INVALID) {
		chan_state = vos_search_5g_bonded_channel(oper_ch,
				ch_params->ch_width, &bonded_chan_ptr);
		if (((NV_CHANNEL_ENABLE != chan_state) &&
		   (NV_CHANNEL_DFS != chan_state)))
			goto next;

		chan_state = vos_get_5g_bonded_channel_state(oper_ch,
				ch_params->ch_width, bonded_chan_ptr);
		if (((NV_CHANNEL_ENABLE != chan_state) &&
		   (NV_CHANNEL_DFS != chan_state)))
			goto next;

		if (CH_WIDTH_20MHZ >= ch_params->ch_width) {
			ch_params->sec_ch_offset
				= PHY_SINGLE_CHANNEL_CENTERED;
			break;
		} else if (CH_WIDTH_40MHZ == ch_params->ch_width) {
			if (oper_ch == bonded_chan_ptr->start_ch)
				ch_params->sec_ch_offset =
					PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
			else
				ch_params->sec_ch_offset =
					PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		}

		center_chan = (bonded_chan_ptr->start_ch +
			 bonded_chan_ptr->end_ch) / 2;
		ch_params->center_freq_seg0 =
			vos_chan_to_freq(center_chan);

		if (CH_WIDTH_80MHZ == ch_params->ch_width)
			vos_set_sec_chan_offset_vht80(oper_ch,
					center_chan, ch_params);

		break;
next:
		ch_params->ch_width = next_lower_bw[ch_params->ch_width];
	}
}

/**
 * vos_get_2g_bonded_channel_state() - get the 2G bonded channel state
 * @oper_ch: operating channel
 * @ch_width: channel width
 * @sec_ch: secondary channel
 *
 * Return: channel state
 */
eNVChannelEnabledType vos_get_2g_bonded_channel_state(uint16_t oper_ch,
						  enum phy_ch_width ch_width,
						  uint16_t sec_ch)
{
	eNVChannelEnabledType chan_state;
	bool bw_enabled = false;
	uint32_t flags;
	bool ht_40_plus = false;

	if (CH_WIDTH_40MHZ < ch_width)
		return NV_CHANNEL_INVALID;

	if (CH_WIDTH_40MHZ == ch_width) {
		if ((sec_ch + 4 != oper_ch) &&
		    (oper_ch + 4 != sec_ch))
			return NV_CHANNEL_INVALID;
		ht_40_plus = (oper_ch < sec_ch)? true : false;
	}

	chan_state = vos_nv_getChannelEnabledState(oper_ch);
	if ((NV_CHANNEL_INVALID == chan_state) ||
	    (NV_CHANNEL_DISABLE == chan_state))
		return chan_state;

	flags = vos_nv_get_channel_flags(oper_ch);

	if (CH_WIDTH_5MHZ == ch_width) {
		bw_enabled = true;
	} else if (CH_WIDTH_10MHZ == ch_width) {
		bw_enabled = !(flags &
				IEEE80211_CHAN_NO_10MHZ);
	} else if (CH_WIDTH_20MHZ == ch_width) {
		bw_enabled = !(flags &
				IEEE80211_CHAN_NO_20MHZ);
	} else if (CH_WIDTH_40MHZ == ch_width) {
		if (ht_40_plus)
			bw_enabled =
				!(flags & IEEE80211_CHAN_NO_HT40PLUS);
		else
			bw_enabled =
				!(flags & IEEE80211_CHAN_NO_HT40MINUS);
	}

	if (bw_enabled)
		return chan_state;
	else
		return NV_CHANNEL_DISABLE;
}

/**
 * vos_set_2g_channel_params() - set the 2.4G bonded channel parameters
 * @oper_ch: operating channel
 * @ch_params: channel parameters
 * @sec_ch_2g: 2.4G secondary channel
 *
 * Return: void
 */
static void vos_set_2g_channel_params(uint16_t oper_ch,
		struct ch_params_s *ch_params,
		uint16_t sec_ch_2g)
{
	eNVChannelEnabledType chan_state = NV_CHANNEL_ENABLE;

	if (CH_WIDTH_MAX <= ch_params->ch_width)
		ch_params->ch_width = CH_WIDTH_40MHZ;

	while (ch_params->ch_width < CH_WIDTH_INVALID) {
		chan_state = vos_get_2g_bonded_channel_state(oper_ch,
							    ch_params->ch_width,
							    sec_ch_2g);
		if (NV_CHANNEL_ENABLE == chan_state) {
			if (CH_WIDTH_40MHZ == ch_params->ch_width) {
				if (oper_ch < sec_ch_2g)
					ch_params->sec_ch_offset =
						PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
				else
					ch_params->sec_ch_offset =
						PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
				ch_params->center_freq_seg0 =
					vos_chan_to_freq(
						(oper_ch + sec_ch_2g) / 2);
			} else {
				ch_params->sec_ch_offset =
					PHY_SINGLE_CHANNEL_CENTERED;
			}
			break;
		}

		ch_params->ch_width = next_lower_bw[ch_params->ch_width];
	}
}

/**
 * vos_set_channel_params() - set the bonded channel parameters
 * @oper_ch: operating channel
 * @sec_ch_2g: 2.4G secondary channel
 * @ch_params: chanel parameters
 *
 * Return: void
 */
void vos_set_channel_params(uint16_t oper_ch, uint16_t sec_ch_2g,
	struct ch_params_s *ch_params)
{
	if (VOS_IS_CHANNEL_5GHZ(oper_ch))
		vos_set_5g_channel_params(oper_ch, ch_params);
	else if (VOS_IS_CHANNEL_24GHZ(oper_ch))
		vos_set_2g_channel_params(oper_ch, ch_params, sec_ch_2g);
}

/**------------------------------------------------------------------------
  \brief vos_nv_getSupportedCountryCode() - get the list of supported
  country codes
  The \a vos_nv_getSupportedCountryCode() encodes the list of supported
  country codes with paddings in the provided buffer
  \param pBuffer     - pointer to buffer where supported country codes
                       and paddings are encoded; this may be set to NULL
                       if user wishes to query the required buffer size to
                       get the country code list
  \param pBufferSize - this is the provided buffer size on input;
                       this is the required or consumed buffer size on output
  \return VOS_STATUS_SUCCESS - country codes are successfully encoded
          VOS_STATUS_E_NOMEM - country codes are not encoded because either
                               the buffer is NULL or buffer size is
                               sufficient
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedCountryCode( v_BYTE_t *pBuffer, v_SIZE_t *pBufferSize,
      v_SIZE_t paddingSize )
{
   v_SIZE_t providedBufferSize = *pBufferSize;
   int i;
   // pBufferSize now points to the required buffer size
   *pBufferSize = countryInfoTable.countryCount * (VOS_COUNTRY_CODE_LEN + paddingSize );
   if ( NULL == pBuffer || providedBufferSize < *pBufferSize )
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            ("Insufficient memory for country code list"));
      return VOS_STATUS_E_NOMEM;
   }
   for (i = 0; i < countryInfoTable.countryCount; i++) {
      vos_mem_copy(pBuffer, countryInfoTable.countryInfo[i].countryCode,
                   VOS_COUNTRY_CODE_LEN);
      pBuffer += (VOS_COUNTRY_CODE_LEN + paddingSize );
   }
   return VOS_STATUS_SUCCESS;
}

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelListWithPower() - function to return the list of
          supported channels with the power limit info too.
  \param pChannels20MHz - list of 20 Mhz channels
  \param pNum20MHzChannelsFound - number of 20 Mhz channels
  \param pChannels40MHz - list of 20 Mhz channels
  \param pNum40MHzChannelsFound - number of 20 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getChannelListWithPower(tChannelListWithPower *channels20MHz /*[NUM_LEGIT_RF_CHANNELS] */,
                                          tANI_U8 *num20MHzChannelsFound,
                                          tChannelListWithPower *channels40MHz /*[NUM_CHAN_BOND_CHANNELS] */,
                                          tANI_U8 *num40MHzChannelsFound
                                          )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    int i, count;

    //TODO: Dont want to use pMac here...can we instead store the curRegDomain in NV
    // or pass it as a parameter to NV from SME?

    if( channels20MHz && num20MHzChannelsFound )
    {
        count = 0;
        for( i = 0; i <= RF_CHAN_14; i++ )
        {
            if( regChannels[i].enabled )
            {
                channels20MHz[count].chanId = rfChannels[i].channelNum;
                channels20MHz[count++].pwr  = regChannels[i].pwrLimit;
            }
        }
        for( i = RF_CHAN_36; i <= RF_CHAN_184; i++ )
        {
            if( regChannels[i].enabled )
            {
                channels20MHz[count].chanId = rfChannels[i].channelNum;
                channels20MHz[count++].pwr  = regChannels[i].pwrLimit;
            }
        }
        *num20MHzChannelsFound = (tANI_U8)count;
    }

    if( channels40MHz && num40MHzChannelsFound )
    {
        count = 0;
        //center channels for 2.4 Ghz 40 MHz channels
        for( i = RF_CHAN_BOND_3; i <= RF_CHAN_BOND_11; i++ )
        {

            if( regChannels[i].enabled )
            {
                channels40MHz[count].chanId = rfChannels[i].channelNum;
                channels40MHz[count++].pwr  = regChannels[i].pwrLimit;
            }
        }
        //center channels for 5 Ghz 40 MHz channels
        for( i = RF_CHAN_BOND_38; i <= RF_CHAN_BOND_163; i++ )
        {

            if( regChannels[i].enabled )
            {
                channels40MHz[count].chanId = rfChannels[i].channelNum;
                channels40MHz[count++].pwr  = regChannels[i].pwrLimit;
            }
        }
        *num40MHzChannelsFound = (tANI_U8)count;
    }
    return (status);
}

/**------------------------------------------------------------------------
  \brief vos_nv_getDefaultRegDomain() - return the default regulatory domain
  \return default regulatory domain
  \sa
  -------------------------------------------------------------------------*/

v_REGDOMAIN_t vos_nv_getDefaultRegDomain( void )
{
    return countryInfoTable.countryInfo[0].regDomain;
}

/**------------------------------------------------------------------------
  \brief vos_nv_getSupportedChannels() - function to return the list of
          supported channels
  \param p20MhzChannels - list of 20 Mhz channels
  \param pNum20MhzChannels - number of 20 Mhz channels
  \param p40MhzChannels - list of 40 Mhz channels
  \param pNum40MhzChannels - number of 40 Mhz channels
  \return status of the NV read operation
  \Note: 40Mhz not currently supported
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getSupportedChannels( v_U8_t *p20MhzChannels, int *pNum20MhzChannels,
                                        v_U8_t *p40MhzChannels, int *pNum40MhzChannels)
{
    VOS_STATUS status = VOS_STATUS_E_INVAL;
    int i, count = 0;

    if( p20MhzChannels && pNum20MhzChannels )
    {
        if( *pNum20MhzChannels >= NUM_RF_CHANNELS )
        {
            for( i = 0; i <= RF_CHAN_14; i++ )
            {
                p20MhzChannels[count++] = rfChannels[i].channelNum;
            }
            for( i = RF_CHAN_36; i <= RF_CHAN_184; i++ )
            {
                p20MhzChannels[count++] = rfChannels[i].channelNum;
            }
            status = VOS_STATUS_SUCCESS;
        }
        *pNum20MhzChannels = count;
    }

    return (status);
}

/**------------------------------------------------------------------------
  \brief vos_nv_readDefaultCountryTable() - return the default Country table
  \param table data - a union to return the default country table data in.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readDefaultCountryTable( uNvTables *tableData )
{

   VOS_STATUS status = VOS_STATUS_SUCCESS;
   vos_mem_copy(&tableData->defaultCountryTable,
                &pnvEFSTable->halnv.tables.defaultCountryTable,
                sizeof(sDefaultCountry));
   pr_info("DefaultCountry is %c%c\n",
            tableData->defaultCountryTable.countryCode[0],
            tableData->defaultCountryTable.countryCode[1]);
   return status;
}

/**
 * vos_nv_skip_dsrc_dfs_2g() - skip dsrc, dfs and 2g band channels
 * @rf_channel: input channel enum to know, whether to skip or add the channel
 * @skip_group: group to skip
 *
 * Return: true or false
 */
uint8_t vos_nv_skip_dsrc_dfs_2g(uint32_t rf_channel, int32_t skip_group)
{
	uint32_t channel_loop;
	eRfChannels channel_enum = INVALID_RF_CHANNEL;
	uint8_t ret = false;
	int32_t start_channel, end_channel;

	switch (skip_group){
	case NV_CHANNEL_SKIP_DSRC:
		start_channel = RF_CHAN_1;
		end_channel = RF_CHAN_165;
		break;
	case NV_CHANNEL_SKIP_2G:
		start_channel = RF_CHAN_36;
		end_channel = RF_CHAN_165;
		break;
	default:
		start_channel = RF_CHAN_1;
		end_channel = RF_CHAN_184;
		break;
	}

	for (channel_loop = start_channel;
	     channel_loop <= end_channel; channel_loop++) {
		if (rfChannels[channel_loop].channelNum == rf_channel) {
			channel_enum = (eRfChannels)channel_loop;
			break;
		}
	}

	if (INVALID_RF_CHANNEL == channel_enum) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
			  FL("Invalid channel %d"), rf_channel);
		ret = true;
		goto exit_ok;
	}

	if (NV_CHANNEL_DFS == regChannels[channel_enum].enabled)
		ret = true;
exit_ok:
	return ret;
}

/**------------------------------------------------------------------------
  \brief vos_nv_getChannelEnabledState -
  \param rfChannel  - input channel enum to know evabled state
  \return eNVChannelEnabledType enabled state for channel
             * enabled
             * disabled
             * DFS
  \sa
  -------------------------------------------------------------------------*/
eNVChannelEnabledType vos_nv_getChannelEnabledState
(
   v_U32_t     rfChannel
)
{
   v_U32_t       channelLoop;
   eRfChannels   channelEnum = INVALID_RF_CHANNEL;

   for(channelLoop = 0; channelLoop <= RF_CHAN_184; channelLoop++)
   {
      if(rfChannels[channelLoop].channelNum == rfChannel)
      {
         channelEnum = (eRfChannels)channelLoop;
         break;
      }
   }

   if(INVALID_RF_CHANNEL == channelEnum)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "vos_nv_getChannelEnabledState, invalid channel %d", rfChannel);
      return NV_CHANNEL_INVALID;
   }

   return regChannels[channelEnum].enabled;
}

/**
 * vos_nv_get_channel_flags: Get channel flags
 * @rf_channel: Channel number.
 * This function is called to know associated flags with channel
 *
 * Return: updated Wiphy struct
 */
uint32_t vos_nv_get_channel_flags
(
   uint32_t  rf_channel
)
{
	uint32_t       channel_loop;
	eRfChannels   channel_enum = INVALID_RF_CHANNEL;

	for(channel_loop = 0; channel_loop <= RF_CHAN_184; channel_loop++) {
		if(rfChannels[channel_loop].channelNum == rf_channel) {
			channel_enum = (eRfChannels)channel_loop;
			break;
		}
	}

	if (INVALID_RF_CHANNEL == channel_enum) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"vos_nv_get_channel_flags, invalid channel %d",
			rf_channel);
		return NV_CHANNEL_INVALID;
	}
	return regChannels[channel_enum].flags;
}

/******************************************************************
 Add CRDA regulatory support
*******************************************************************/

/**------------------------------------------------------------------------
  \brief vos_nv_setRegDomain -
  \param clientCtxt  - Client Context, Not used for PRIMA
              regId  - Regulatory Domain ID
              sendRegHint - send hint to nl80211
  \return status set REG domain operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setRegDomain(void * clientCtxt, v_REGDOMAIN_t regId,
                                                v_BOOL_t sendRegHint)
{

    if (regId >= REGDOMAIN_COUNT)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "VOS set reg domain, invalid REG domain ID %d", regId);
        return VOS_STATUS_E_INVAL;
    }

    /* Set correct channel information based on REG Domain */
    regChannels = pnvEFSTable->halnv.tables.regDomains[regId].channels;

    return VOS_STATUS_SUCCESS;
}

/* vos_nv_set_dfs_region() - set the dfs_region
 *
 * @dfs_region: the dfs_region to set
 *
 * Return: VOS_STATUS_SUCCESS if dfs_region set correctly
 *         VOS_STATUS_E_EXISTS if vos_context not found
 */
VOS_STATUS vos_nv_set_dfs_region(uint8_t dfs_region)
{
	v_CONTEXT_t vos_ctx_ptr = NULL;
	hdd_context_t *hdd_ctx_ptr= NULL;

	vos_ctx_ptr = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

	if (NULL == vos_ctx_ptr)
		return VOS_STATUS_E_EXISTS;

	hdd_ctx_ptr = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx_ptr);

	if (NULL == hdd_ctx_ptr)
		return VOS_STATUS_E_EXISTS;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)

	hdd_ctx_ptr->reg.dfs_region = dfs_region;

#else

	/* remap the ctl code to dfs region code */
	switch(hdd_ctx_ptr->reg.ctl_5g) {
	case FCC:
		hdd_ctx_ptr->reg.dfs_region = DFS_FCC_DOMAIN;
		break;
	case ETSI:
		hdd_ctx_ptr->reg.dfs_region = DFS_ETSI_DOMAIN;
		break;
	case MKK:
		hdd_ctx_ptr->reg.dfs_region = DFS_MKK4_DOMAIN;
		break;
	default:
		/* set default dfs_region to FCC */
		hdd_ctx_ptr->reg.dfs_region = DFS_FCC_DOMAIN;
		break;
	}
#endif
	return VOS_STATUS_SUCCESS;
}

/* vos_nv_get_dfs_region() - get the dfs_region
 *
 * @dfs_region: the dfs_region to return
 *
 * Return: VOS_STATUS_SUCCESS if dfs_region set correctly
 *         VOS_STATUS_E_EXISTS if vos_context not found
 */
VOS_STATUS vos_nv_get_dfs_region(uint8_t *dfs_region)
{
	v_CONTEXT_t vos_ctx_ptr = NULL;
	hdd_context_t *hdd_ctx_ptr = NULL;

	vos_ctx_ptr = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

	if (NULL == vos_ctx_ptr)
		return VOS_STATUS_E_EXISTS;

	hdd_ctx_ptr = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx_ptr);

	if (NULL == hdd_ctx_ptr)
		return VOS_STATUS_E_EXISTS;

	*dfs_region = hdd_ctx_ptr->reg.dfs_region;

	return VOS_STATUS_SUCCESS;
}

/**------------------------------------------------------------------------
  \brief vos_nv_getRegDomainFromCountryCode() - get the regulatory domain of
  a country given its country code
  The \a vos_nv_getRegDomainFromCountryCode() returns the regulatory domain of
  a country given its country code.  This is done from reading a cached
  copy of the binary file.
  \param pRegDomain  - pointer to regulatory domain
  \param countryCode - country code
  \param source      - source of the country code
  \return VOS_STATUS_SUCCESS - regulatory domain is found for the given country
          VOS_STATUS_E_FAULT - invalid pointer error
          VOS_STATUS_E_EMPTY - country code table is empty
          VOS_STATUS_E_EXISTS - given country code does not exist in table
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getRegDomainFromCountryCode( v_REGDOMAIN_t *pRegDomain,
      const v_COUNTRYCODE_t country_code, v_CountryInfoSource_t source)
{

    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;
    struct wiphy *wiphy = NULL;
    int i;

    /* sanity checks */
    if (NULL == pRegDomain)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Invalid reg domain pointer") );
        return VOS_STATUS_E_FAULT;
    }

    *pRegDomain = REGDOMAIN_COUNT;

    if (NULL == country_code)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Country code array is NULL"));
        return VOS_STATUS_E_FAULT;
    }

    if (0 == countryInfoTable.countryCount)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Reg domain table is empty") );
        return VOS_STATUS_E_EMPTY;
    }


    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL != pVosContext)
        pHddCtx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
    else
        return VOS_STATUS_E_EXISTS;

    if (NULL == pHddCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Invalid pHddCtx pointer") );
        return VOS_STATUS_E_FAULT;
    }

    if (pHddCtx->isLogpInProgress) {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   (" SSR in progress, return") );
        *pRegDomain = temp_reg_domain;
         return VOS_STATUS_SUCCESS;
    }

    wiphy = pHddCtx->wiphy;

    if (false == wiphy->registered) {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("wiphy is not yet registered with the kernel") );
        return VOS_STATUS_E_FAULT;
    }

    temp_reg_domain = REGDOMAIN_COUNT;
    /* lookup the country in the local database */
    for (i = 0; i < countryInfoTable.countryCount &&
             REGDOMAIN_COUNT == temp_reg_domain; i++)
    {
        if (memcmp(country_code, countryInfoTable.countryInfo[i].countryCode,
                   VOS_COUNTRY_CODE_LEN) == 0)
        {
            /* country code is found */
            /* record the temporary regulatory_domain as well */
            temp_reg_domain = countryInfoTable.countryInfo[i].regDomain;
            break;
        }
    }

    if (REGDOMAIN_COUNT == temp_reg_domain) {

        /*
         * The country was not found in the driver database
         * so we will return the REGDOMAIN_WORLD to SME/CSR
         */
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 ("Country %c%c does not map to any Regulatory domain"),
                  country_code[0], country_code[1]);

        temp_reg_domain = REGDOMAIN_WORLD;
    }

    if (COUNTRY_QUERY == source) {
        *pRegDomain = temp_reg_domain;
         return VOS_STATUS_SUCCESS;
    }

    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   ("regdomain request"));

    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
               (" get country information from kernel db"));

    if ((COUNTRY_INIT == source) && (VOS_FALSE == init_by_reg_core)) {
        init_by_driver = VOS_TRUE;

        if (('0' != country_code[0]) || ('0' != country_code[1])) {
            INIT_COMPLETION(pHddCtx->reg_init);
            regulatory_hint(wiphy, country_code);
            wait_for_completion_timeout(&pHddCtx->reg_init,
                                        msecs_to_jiffies(REG_WAIT_TIME));
        }

    } else if (COUNTRY_IE == source || COUNTRY_USER == source) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
        regulatory_hint_user(country_code, NL80211_USER_REG_HINT_USER);
#else
        regulatory_hint_user(country_code);
#endif
    }

    *pRegDomain = temp_reg_domain;
    return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_STATICALLY_ADD_11P_CHANNELS
#define DEFAULT_11P_POWER (30)
#endif

/* vos_is_dsrc_channel() - is the channel DSRC
 *
 * @center_freq: center freq of the channel
 *
 * Return: true if dsrc channel
 *         false otherwise
 */
bool vos_is_dsrc_channel(uint16_t center_freq)
{
    switch (center_freq) {
    case 5852:
    case 5860:
    case 5870:
    case 5880:
    case 5890:
    case 5900:
    case 5910:
    case 5920:
    case 5875:
    case 5905:
        return 1;
    }
    return 0;
}
/**
 * vos_update_band: Update the band
 * @eBand: Band value
 *
 * This function is called from the supplicant through a
 * private ioctl to change the band value.
 *
 * Return: updated Wiphy struct
 */
int vos_update_band(v_U8_t  band_capability)
{
	v_CONTEXT_t vos_ctx = NULL;
	hdd_context_t *hdd_ctx = NULL;
	struct wiphy *wiphy = NULL;
	int i, j;
	eNVChannelEnabledType channel_enabled_state;
	uint32_t flags;
	ENTER();
	vos_ctx = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

	if (NULL != vos_ctx)
		hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx);
	else
		return VOS_STATUS_E_EXISTS;

	if (NULL == hdd_ctx) {
		VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				("Invalid hdd_ctx pointer") );
		return VOS_STATUS_E_FAULT;
	}

	if (hdd_ctx->isLogpInProgress) {
		VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				(" SSR in progress, return") );
		return VOS_STATUS_SUCCESS;
	}

	wiphy = hdd_ctx->wiphy;

	if (false == wiphy->registered) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			("wiphy is not yet registered with the kernel"));
		return VOS_STATUS_E_FAULT;
	}

	for (i = 0; i < IEEE80211_NUM_BANDS; i++) {
		if (NULL == wiphy->bands[i])
			continue;

		for (j = 0; j < wiphy->bands[i]->n_channels; j++) {
			struct ieee80211_supported_band *band = wiphy->bands[i];

			flags = vos_nv_get_channel_flags(
					band->channels[j].hw_value);
			channel_enabled_state =
				vos_nv_getChannelEnabledState(
						band->channels[j].hw_value);
			/* 5G only */
			if (IEEE80211_BAND_2GHZ == i &&
					eCSR_BAND_5G == band_capability) {
#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
				/* Enable Social channels for P2P */
				if (WLAN_HDD_IS_SOCIAL_CHANNEL(
					band->channels[j].center_freq) &&
					NV_CHANNEL_ENABLE ==
					channel_enabled_state)
					band->channels[j].flags &=
						~IEEE80211_CHAN_DISABLED;
				else
#endif
					band->channels[j].flags |=
						IEEE80211_CHAN_DISABLED;
				continue;
			} else if (IEEE80211_BAND_5GHZ == i &&
					eCSR_BAND_24 == band_capability) {
				/* 2.4G only */
				band->channels[j].flags |=
					IEEE80211_CHAN_DISABLED;
				continue;
			}
			if (NV_CHANNEL_DISABLE != channel_enabled_state)
				band->channels[j].flags = flags;

		}
	}
	return 0;
}

/* create_linux_regulatory_entry to populate internal structures from wiphy */
static int create_linux_regulatory_entry(struct wiphy *wiphy,
                                         v_U8_t nBandCapability,
                                         bool reset)
{
    int i, j, m;
    int k = 0, n = 0;
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)) && !defined(WITH_BACKPORTS)
	 int err;
#endif
    const struct ieee80211_reg_rule *reg_rule;
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL != pVosContext)
    {
        pHddCtx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
        if (NULL == pHddCtx)
        {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       ("Invalid pHddCtx pointer") );
           return -1;
        }
        else
        {
           pHddCtx->isVHT80Allowed = 0;
        }
    }
    else
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  ("Invalid pVosContext pointer") );
       return -1;
    }

    /* 20MHz channels */
    if (nBandCapability == eCSR_BAND_24)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "BandCapability is set to 2G only");

    for (i = 0, m = 0; i<IEEE80211_NUM_BANDS; i++)
    {
        if (wiphy->bands[i] == NULL)
            continue;

        /* internal channels[] is one continous array for both 2G and 5G bands
           m is internal starting channel index for each band */

        if (i == 0)
            m = 0;
        else
            m = wiphy->bands[i-1]->n_channels + m;

        if (pnvEFSTable == NULL)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "error: pnvEFSTable is NULL, probably not parsed nv.bin yet");
            return -1;
        }

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            /* k = (m + j) is internal current channel index for 20MHz channel
              n is internal channel index for corresponding 40MHz channel */

            k = m + j;

            /* If the regulatory rules for a country do not explicilty
             * require a passive scan on a frequency, lift the passive
             * scan restriction
             * When getting the regulatory rule, specify the smallest bandwidth.
             * That's 5 MHz. A larger bandwidth may not fit into the frequency range. */

            if ((!reset) &&
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
                (wiphy->regulatory_flags & REGULATORY_CUSTOM_REG)) {
#else
                (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)) {
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
                reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(wiphy->bands[i]->
                                         channels[j].center_freq));
#else
                err = freq_reg_info(wiphy, MHZ_TO_KHZ(wiphy->bands[i]->
                                    channels[j].center_freq),
                                    0, &reg_rule);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
                if (!IS_ERR(reg_rule)) {
#else
                if (0 == err) {
#endif
                    wiphy->bands[i]->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;

                    if (!(reg_rule->flags & NL80211_RRF_DFS))
                    {
                        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                                  "%s: Remove passive scan restriction for %u",
                                  __func__, wiphy->bands[i]->channels[j].center_freq);
                        wiphy->bands[i]->channels[j].flags &= ~IEEE80211_CHAN_RADAR;
                    }

                    if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
                    {
                        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                                  "%s: Remove passive scan restriction for %u",
                                  __func__, wiphy->bands[i]->channels[j].center_freq);
                        wiphy->bands[i]->channels[j].flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
                    }

                    if (!(reg_rule->flags & NL80211_RRF_NO_IBSS))
                    {
                        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                                  "%s: Remove no ibss restriction for %u",
                                  __func__, wiphy->bands[i]->channels[j].center_freq);
                        wiphy->bands[i]->channels[j].flags &= ~IEEE80211_CHAN_NO_IBSS;
                    }

                    wiphy->bands[i]->channels[j].max_power =
                        (int) MBM_TO_DBM(reg_rule->power_rule.max_eirp);
                }
            }

#ifdef FEATURE_STATICALLY_ADD_11P_CHANNELS
            if (vos_is_dsrc_channel(wiphy->bands[i]->channels[j].center_freq))
            {
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                    channels[k].enabled = NV_CHANNEL_ENABLE;

                /* max_power is in dBm */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].pwrLimit =
                    DEFAULT_11P_POWER;
            }
            else
#endif
            if (wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_DISABLED)
            {
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].enabled =
                    NV_CHANNEL_DISABLE;
                n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                        chan_to_ht_40_index[k].ht_40_plus_index;
                if (n != INVALID_RF_CHANNEL)
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_DISABLE;

                n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                       chan_to_ht_40_index[k].ht_40_minus_index;
                if (n != INVALID_RF_CHANNEL)
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_DISABLE;
            } else if ((wiphy->bands[i]->channels[j].flags &
                     (IEEE80211_CHAN_RADAR | IEEE80211_CHAN_PASSIVE_SCAN)) ||
                      ((pHddCtx->cfg_ini->indoor_channel_support == FALSE) &&
                      (wiphy->bands[i]->channels[j].flags &
                       IEEE80211_CHAN_INDOOR_ONLY))) {
                /* nv cannot distinguish between DFS and passive channels */

                if ((wiphy->bands[i]->channels[j].flags &
                    IEEE80211_CHAN_INDOOR_ONLY) &&
                    (FALSE ==
                    pHddCtx->cfg_ini->indoor_channel_support))
                    wiphy->bands[i]->channels[j].flags |=
                        IEEE80211_CHAN_PASSIVE_SCAN;

                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].enabled =
                    NV_CHANNEL_DFS;

                /* max_power is in mBm = 100 * dBm */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].pwrLimit =
                    (tANI_S8) ((wiphy->bands[i]->channels[j].max_power));

                /* Disable the center channel if neither HT40+ nor HT40- is allowed
                 */
                if ((wiphy->bands[i]->channels[j].flags &
                        IEEE80211_CHAN_NO_HT40) == IEEE80211_CHAN_NO_HT40)
                {
                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                        chan_to_ht_40_index[k].ht_40_plus_index;
                    if (n != INVALID_RF_CHANNEL)
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DISABLE;

                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                       chan_to_ht_40_index[k].ht_40_minus_index;
                    if (n != INVALID_RF_CHANNEL)
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DISABLE;
                } else {
                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                        chan_to_ht_40_index[k].ht_40_plus_index;
                    if (!(wiphy->bands[i]->channels[j].flags &
                            IEEE80211_CHAN_NO_HT40PLUS) &&
                            (n != INVALID_RF_CHANNEL)) {
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DFS;
                        /* 40MHz channel power is half of 20MHz (-3dB) ?? */
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].pwrLimit = (tANI_S8)
                               (((wiphy->bands[i]->channels[j].max_power)) - 3);
                    }

                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                       chan_to_ht_40_index[k].ht_40_minus_index;
                    if (!(wiphy->bands[i]->channels[j].flags &
                            IEEE80211_CHAN_NO_HT40MINUS) &&
                            (n != INVALID_RF_CHANNEL)) {
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DFS;
                        /* 40MHz channel power is half of 20MHz (-3dB) ?? */
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].pwrLimit = (tANI_S8)
                               (((wiphy->bands[i]->channels[j].max_power)) - 3);
                    }

                }

                if ((wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_NO_80MHZ) == 0)
                {
                   if (NULL == pHddCtx)
                   {
                      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                                  ("Invalid pHddCtx pointer") );
                   }
                   else
                   {
                      pHddCtx->isVHT80Allowed = 1;
                   }
                }
            } else {
                /* Enable is only last flag we support */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                    channels[k].enabled = NV_CHANNEL_ENABLE;

                /* max_power is in dBm */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].pwrLimit =
                    (tANI_S8) ((wiphy->bands[i]->channels[j].max_power));

                /* Disable the center channel if neither HT40+ nor HT40- is allowed
                 */
                if ((wiphy->bands[i]->channels[j].flags &
                             IEEE80211_CHAN_NO_HT40) == IEEE80211_CHAN_NO_HT40)
                {
                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                        chan_to_ht_40_index[k].ht_40_plus_index;
                    if (n != INVALID_RF_CHANNEL)
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DISABLE;

                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                       chan_to_ht_40_index[k].ht_40_minus_index;
                    if (n != INVALID_RF_CHANNEL)
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_DISABLE;
                } else {
                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                        chan_to_ht_40_index[k].ht_40_plus_index;
                    if (!(wiphy->bands[i]->channels[j].flags &
                            IEEE80211_CHAN_NO_HT40PLUS) &&
                            (n != INVALID_RF_CHANNEL)) {
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_ENABLE;
                        /* 40MHz channel power is half of 20MHz (-3dB) ?? */
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].pwrLimit = (tANI_S8)
                               (((wiphy->bands[i]->channels[j].max_power)) - 3);
                    }

                    n = (k > RF_CHAN_165)? INVALID_RF_CHANNEL :
                                       chan_to_ht_40_index[k].ht_40_minus_index;
                    if (!(wiphy->bands[i]->channels[j].flags &
                            IEEE80211_CHAN_NO_HT40MINUS) &&
                            (n != INVALID_RF_CHANNEL)) {
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].enabled = NV_CHANNEL_ENABLE;
                        /* 40MHz channel power is half of 20MHz (-3dB) ?? */
                        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                            channels[n].pwrLimit = (tANI_S8)
                               (((wiphy->bands[i]->channels[j].max_power)) - 3);
                    }
                }

                if ((wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_NO_80MHZ) == 0)
                {
                   if (NULL == pHddCtx)
                   {
                      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                                  ("Invalid pHddCtx pointer") );
                   }
                   else
                   {
                      pHddCtx->isVHT80Allowed = 1;
                   }
                }

            }
            /* Copy wiphy flags in nv table */
            if (n != -1)
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                    channels[n].flags = wiphy->bands[i]->channels[j].flags;
            pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                channels[k].flags = wiphy->bands[i]->channels[j].flags;
        }
    }

#ifdef FEATURE_WLAN_CH144
     /* Disable  RF_CHAN_144 entry if FW does not support channel 144. */
     if (pHddCtx &&
        (0 == (pHddCtx->reg.eeprom_rd_ext & (1 << WHAL_REG_EXT_FCC_CH_144)))) {
        pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
           channels[RF_CHAN_144].enabled = NV_CHANNEL_DISABLE;
     }
#endif

    if (k == 0)
       return -1;

    vos_update_band(nBandCapability);
    return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)) && !defined(WITH_BACKPORTS)
/* restore_custom_reg_settings() - restore custom reg settings
 *
 * @wiphy: wiphy structure
 *
 * Return: void
 */
static void restore_custom_reg_settings(struct wiphy *wiphy)
{

	struct ieee80211_supported_band *sband;
	enum ieee80211_band band;
	struct ieee80211_channel *chan;
	int i;

	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;
		for (i = 0; i < sband->n_channels; i++) {
			chan = &sband->channels[i];
			chan->flags = chan->orig_flags;
			chan->max_antenna_gain = chan->orig_mag;
			chan->max_power = chan->orig_mpwr;
		}
	}
}
#endif

/*
 * Function: wlan_hdd_linux_reg_notifier
 * This function is called from cfg80211 core to provide regulatory settings
 * after new country is requested or intersected (init, user input or 11d)
 * This function is used to create a CRDA regulatory settings entry into internal
 * regulatory setting table.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
void __wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
#else
int __wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
#endif
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    eCsrBand nBandCapability = eCSR_BAND_ALL;
    v_COUNTRYCODE_t country_code;
    int i;
    v_BOOL_t isVHT80Allowed;
    bool reset = false;

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
              FL("country: %c%c, initiator %d, dfs_region: %d"),
              request->alpha2[0],
              request->alpha2[1],
              request->initiator,
              request->dfs_region);

    if (TRUE == isWDresetInProgress())
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("SSR is in progress") );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
       return;
#else
       return 0;
#endif
    }

    if (NULL == pHddCtx)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Invalid pHddCtx pointer") );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
       return;
#else
       return 0;
#endif
    }

    if (pHddCtx->isUnloadInProgress ||
        pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading or SSR in Progress, Ignore!!!", __func__);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
            return;
#else
            return 0;
#endif
    }

    sme_GetFreqBand(pHddCtx->hHal, &nBandCapability);

    /* first check if this callback is in response to the driver callback */

    switch (request->initiator)
    {
    case NL80211_REGDOM_SET_BY_DRIVER:
    case NL80211_REGDOM_SET_BY_CORE:
    case NL80211_REGDOM_SET_BY_USER:

        if ((VOS_FALSE == init_by_driver) &&
            (VOS_FALSE == init_by_reg_core)) {

            if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
                return;
#else
                return 0;
#endif
            }
            init_by_reg_core = VOS_TRUE;
        }

        if ((NL80211_REGDOM_SET_BY_DRIVER == request->initiator) &&
            (VOS_TRUE == init_by_driver)) {

            /*
             * restore the driver regulatory flags since
             * regulatory_hint may have
             * changed them
             */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
            wiphy->regulatory_flags = pHddCtx->reg.reg_flags;
#else
            wiphy->flags = pHddCtx->reg.reg_flags;
#endif
        }
        if (NL80211_REGDOM_SET_BY_CORE == request->initiator) {
            pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_CORE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
            if (wiphy->regulatory_flags & REGULATORY_CUSTOM_REG)
#else
            if (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY)
#endif
                reset = true;
        } else if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator) {
            pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_DRIVER;
        } else {
            pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_USER;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)) && !defined(WITH_BACKPORTS)
            if ((request->alpha2[0] == '0') &&
                (request->alpha2[1] == '0') &&
                (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY))
            {
                restore_custom_reg_settings(wiphy);
                reset = true;
            }
#endif
        }

        /* first lookup the country in the local database */
        country_code[0] = request->alpha2[0];
        country_code[1] = request->alpha2[1];

        pHddCtx->reg.alpha2[0] = request->alpha2[0];
        pHddCtx->reg.alpha2[1] = request->alpha2[1];

        vos_update_reg_info(pHddCtx);

        temp_reg_domain = REGDOMAIN_COUNT;
        for (i = 0; i < countryInfoTable.countryCount &&
                 REGDOMAIN_COUNT == temp_reg_domain; i++)
        {
            if (memcmp(country_code, countryInfoTable.countryInfo[i].countryCode,
                       VOS_COUNTRY_CODE_LEN) == 0)
            {
                /* country code is found */
                /* record the temporary regulatory_domain as well */
                temp_reg_domain = countryInfoTable.countryInfo[i].regDomain;
                break;
            }
        }

        if (REGDOMAIN_COUNT == temp_reg_domain)
            temp_reg_domain = REGDOMAIN_WORLD;

        isVHT80Allowed = pHddCtx->isVHT80Allowed;
        regChannels =
            pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels;
        if (create_linux_regulatory_entry(wiphy,
                                          nBandCapability,
                                          reset) == 0)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                      (" regulatory entry created"));
        }

        if (pHddCtx->isVHT80Allowed != isVHT80Allowed)
            hdd_checkandupdate_phymode( pHddCtx);

        if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
            complete(&pHddCtx->reg_init);

        /* now pass the new country information to sme */
        if (request->alpha2[0] == '0' && request->alpha2[1] == '0')
        {
            sme_GenericChangeCountryCode(pHddCtx->hHal, country_code,
                                         REGDOMAIN_COUNT);
        }
        else
        {
            sme_GenericChangeCountryCode(pHddCtx->hHal, country_code,
                                         temp_reg_domain);
        }

        /* send CTL info to firmware */
        regdmn_set_regval(&pHddCtx->reg);

        /* set dfs_region info */
        vos_nv_set_dfs_region(request->dfs_region);

        regdmn_set_dfs_region(&pHddCtx->reg);

    default:
        break;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)) || defined(WITH_BACKPORTS)
    return;
#else
    return 0;
#endif
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
void wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
{
    vos_ssr_protect(__func__);
    __wlan_hdd_linux_reg_notifier(wiphy, request);
    vos_ssr_unprotect(__func__);
    return;
}
#else
int wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_linux_reg_notifier(wiphy, request);
    vos_ssr_unprotect(__func__);
    return ret;
}
#endif

/* initialize wiphy from EEPROM */
VOS_STATUS vos_init_wiphy_from_eeprom(void)
{
   v_CONTEXT_t pVosContext = NULL;
   hdd_context_t *pHddCtx = NULL;
   struct wiphy *wiphy = NULL;

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if (!pVosContext)
      return VOS_STATUS_E_EXISTS;

   pHddCtx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if (!pHddCtx)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            ("Invalid pHddCtx pointer"));
      return VOS_STATUS_E_FAULT;
   }

   wiphy = pHddCtx->wiphy;

   if (reg_init_from_eeprom(pHddCtx, &pHddCtx->reg, wiphy))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            ("Error during regulatory init from EEPROM"));
      return VOS_STATUS_E_FAULT;
   }

   if (is_world_regd(pHddCtx->reg.reg_domain)) {
      temp_reg_domain = REGDOMAIN_WORLD;
      if (create_linux_regulatory_entry(wiphy,
                                        pHddCtx->cfg_ini->nBandCapability,
                                        true) != 0) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               ("Error while creating regulatory entry"));
         return VOS_STATUS_E_FAULT;
      }
   }

   init_completion(&pHddCtx->reg_init);

   /* send CTL info to firmware */
   regdmn_set_regval(&pHddCtx->reg);

   return VOS_STATUS_SUCCESS;
}

/* initialize wiphy from NV.bin */
VOS_STATUS vos_init_wiphy_from_nv_bin(void)
{
    int i, j, m;
    int k = 0;
    v_REGDOMAIN_t reg_domain;
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;
    struct wiphy *wiphy = NULL;

    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL != pVosContext)
        pHddCtx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
    else
        return VOS_STATUS_E_EXISTS;

    if (NULL == pHddCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Invalid pHddCtx pointer") );
        return VOS_STATUS_E_FAULT;
    }

    wiphy = pHddCtx->wiphy;

    /* Update regulatory structure in HDD */
    pHddCtx->reg.alpha2[0] =
       pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[0];
    pHddCtx->reg.alpha2[1] =
       pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[1];
    pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_DRIVER;

    vos_update_reg_info(pHddCtx);

    if  (('0' == pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[0])
         &&
         ('0' == pnvEFSTable->halnv.tables.defaultCountryTable.countryCode[1]))
    {
        /* default country is world roaming */

        reg_domain = REGDOMAIN_WORLD;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
        wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
        wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
    }
    else if (REGDOMAIN_WORLD ==
         pnvEFSTable->halnv.tables.defaultCountryTable.regDomain) {

        reg_domain = pnvEFSTable->halnv.tables.defaultCountryTable.regDomain;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
        wiphy->regulatory_flags |= REGULATORY_CUSTOM_REG;
#else
        wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
#endif
    }
    else {

        reg_domain = pnvEFSTable->halnv.tables.defaultCountryTable.regDomain;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || defined(WITH_BACKPORTS)
        wiphy->regulatory_flags |= REGULATORY_STRICT_REG;
#else
        wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
#endif
    }
    m = 0;
    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {

        if (wiphy->bands[i] == NULL)
            continue;

        /* internal channels[] is one continous array for both 2G and 5G bands
           m is internal starting channel index for each band */

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            /* k = (m + j) is internal current channel index */
            k = m + j;

            if (pnvEFSTable->halnv.tables.regDomains[reg_domain].channels[k].enabled ==
                NV_CHANNEL_DISABLE)
                wiphy->bands[i]->channels[j].flags |= IEEE80211_CHAN_DISABLED;

            else if (pnvEFSTable->halnv.tables.regDomains[reg_domain].channels[k].enabled ==
                     NV_CHANNEL_DFS) {

                wiphy->bands[i]->channels[j].flags |= IEEE80211_CHAN_PASSIVE_SCAN;

                wiphy->bands[i]->channels[j].max_power =
                    (pnvEFSTable->halnv.tables.regDomains[reg_domain].channels[k].pwrLimit)*100;
            }

            else if (pnvEFSTable->halnv.tables.regDomains[reg_domain].channels[k].enabled ==
                     NV_CHANNEL_ENABLE) {

                wiphy->bands[i]->channels[j].max_power =
                    (pnvEFSTable->halnv.tables.regDomains[reg_domain].channels[k].pwrLimit)*100;
            }
        }

        m += wiphy->bands[i]->n_channels;
    }

    return VOS_STATUS_SUCCESS;
}

