/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
#define IEEE80211_CHAN_NO_80MHZ		1<<7
#endif

static v_REGDOMAIN_t cur_reg_domain = REGDOMAIN_COUNT;
static char linux_reg_cc[2] = {0, 0};
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

/*
 * This is a set of common rules used by our world regulatory domains.
 * We have 12 world regulatory domains. To save space we consolidate
 * the regulatory domains in 5 structures by frequency and change
 * the flags on our reg_notifier() on a case by case basis.
 */

/* Only these channels all allow active scan on all world regulatory domains */
#define REG_RULE_2GHZ_CH01_11	REG_RULE(2412-10, 2462+10, 40, 0, 20, 0)

/* We enable active scan on these a case by case basis by regulatory domain */
#define REG_RULE_2GHZ_CH12_13	REG_RULE(2467-10, 2472+10, 40, 0, 20,\
      NL80211_RRF_PASSIVE_SCAN)
#define REG_RULE_2GHZ_CH14		REG_RULE(2484-10, 2484+10, 40, 0, 20,\
      NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_OFDM)

/* We allow IBSS on these on a case by case basis by regulatory domain */
#define REG_RULE_5GHZ_5150_5350	REG_RULE(5150-10, 5350+10, 80, 0, 30,\
      NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)
#define REG_RULE_5GHZ_5470_5850	REG_RULE(5470-10, 5850+10, 80, 0, 30,\
      NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)
#define REG_RULE_5GHZ_5725_5850	REG_RULE(5725-10, 5850+10, 80, 0, 30,\
      NL80211_RRF_PASSIVE_SCAN | NL80211_RRF_NO_IBSS)

#define REG_RULE_2GHZ_ALL		REG_RULE_2GHZ_CH01_11, \
   REG_RULE_2GHZ_CH12_13, \
REG_RULE_2GHZ_CH14

#define REG_RULE_5GHZ_ALL		REG_RULE_5GHZ_5150_5350, \
   REG_RULE_5GHZ_5470_5850

/* This one skips what we call "mid band" */
#define REG_RULE_5GHZ_NO_MIDBAND	REG_RULE_5GHZ_5150_5350, \
   REG_RULE_5GHZ_5725_5850
#define WORLD_SKU_MASK          0x00F0
#define WORLD_SKU_PREFIX        0x0060

/* Can be used for:
 * 0x60, 0x61, 0x62 */
static const struct ieee80211_regdomain vos_world_regdom_60_61_62 = {
   .n_reg_rules = 5,
   .alpha2 =  "99",
   .reg_rules = {
      REG_RULE_2GHZ_ALL,
      REG_RULE_5GHZ_ALL,
   }
};

/* Can be used by 0x63 and 0x65 */
static const struct ieee80211_regdomain vos_world_regdom_63_65 = {
   .n_reg_rules = 4,
   .alpha2 =  "99",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_2GHZ_CH12_13,
      REG_RULE_5GHZ_NO_MIDBAND,
   }
};

/* Can be used by 0x64 only */
static const struct ieee80211_regdomain vos_world_regdom_64 = {
   .n_reg_rules = 3,
   .alpha2 =  "99",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_5GHZ_NO_MIDBAND,
   }
};

/* Can be used by 0x66 and 0x69 */
static const struct ieee80211_regdomain vos_world_regdom_66_69 = {
   .n_reg_rules = 3,
   .alpha2 =  "99",
   .reg_rules = {
      REG_RULE_2GHZ_CH01_11,
      REG_RULE_5GHZ_ALL,
   }
};

/* Can be used by 0x67, 0x68, 0x6A and 0x6C */
static const struct ieee80211_regdomain vos_world_regdom_67_68_6A_6C = {
   .n_reg_rules = 4,
   .alpha2 =  "99",
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
      {REGDOMAIN_FCC, {'I', 'D'}},  //INDONESIA
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
      {REGDOMAIN_FCC, {'T', 'Z'}}, //TANZANIA, UNITED REPUBLIC OF
      {REGDOMAIN_ETSI, {'U', 'A'}}, //UKRAINE
      {REGDOMAIN_ETSI, {'U', 'G'}}, //UGANDA
      {REGDOMAIN_FCC, {'U', 'S'}}, //USA
      {REGDOMAIN_ETSI, {'U', 'Y'}}, //URUGUAY
      {REGDOMAIN_FCC, {'U', 'Z'}}, //UZBEKISTAN
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
    { 4920, 240, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_240,
    { 4940, 244, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_244,
    { 4960, 248, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_248,
    { 4980, 252, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_252,
    { 5040, 208, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_208,
    { 5060, 212, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_212,
    { 5080, 216, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_216,
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
    { 2422, 3  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_3,
    { 2427, 4  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_4,
    { 2432, 5  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_5,
    { 2437, 6  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_6,
    { 2442, 7  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_7,
    { 2447, 8  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_8,
    { 2452, 9  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_9,
    { 2457, 10 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_10,
    { 2462, 11 , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_11,
    { 4930, 242, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_242,
    { 4950, 246, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_246,
    { 4970, 250, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_250,
    { 5050, 210, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_210,
    { 5070, 214, NUM_RF_SUBBANDS},           //RF_CHAN_BOND_214,
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


/* Frequency is one where radar detection is required */
static bool vos_is_radar_freq(u16 center_freq)
{
   return (center_freq >= 5260 && center_freq <= 5700);
}

/*
 * N.B: These exception rules do not apply radar freqs.
 *
 * - We enable adhoc (or beaconing) if allowed by 11d
 * - We enable active scan if the channel is allowed by 11d
 * - If no country IE has been processed and a we determine we have
 *   received a beacon on a channel we can enable active scan and
 *   adhoc (or beaconing).
 */
static void
vos_reg_apply_beaconing_flags(struct wiphy *wiphy,
      enum nl80211_reg_initiator initiator)
{
   enum ieee80211_band band;
   struct ieee80211_supported_band *sband;
   const struct ieee80211_reg_rule *reg_rule;
   struct ieee80211_channel *ch;
   unsigned int i;

   for (band = 0; band < IEEE80211_NUM_BANDS; band++) {

      if (!wiphy->bands[band])
         continue;

      sband = wiphy->bands[band];

      for (i = 0; i < sband->n_channels; i++) {

         ch = &sband->channels[i];

         if (vos_is_radar_freq(ch->center_freq) ||
               (ch->flags & IEEE80211_CHAN_RADAR))
            continue;

         if (initiator == NL80211_REGDOM_SET_BY_COUNTRY_IE) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
            freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq),
                        0, &reg_rule);
#else
            reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq));
#endif
            if (IS_ERR(reg_rule))
               continue;
            /*
             * If 11d had a rule for this channel ensure
             * we enable adhoc/beaconing if it allows us to
             * use it. Note that we would have disabled it
             * by applying our static world regdomain by
             * default during init, prior to calling our
             * regulatory_hint().
             */
            if (!(reg_rule->flags &
                     NL80211_RRF_NO_IBSS))
               ch->flags &=
                  ~IEEE80211_CHAN_NO_IBSS;
            if (!(reg_rule->flags &
                     NL80211_RRF_PASSIVE_SCAN))
               ch->flags &=
                  ~IEEE80211_CHAN_PASSIVE_SCAN;
         } else {
            if (ch->beacon_found)
               ch->flags &= ~(IEEE80211_CHAN_NO_IBSS |
                     IEEE80211_CHAN_PASSIVE_SCAN);
         }
      }
   }
}

/* Allows active scan scan on Ch 12 and 13 */
static void
vos_reg_apply_active_scan_flags(struct wiphy *wiphy,
      enum nl80211_reg_initiator initiator)
{
   struct ieee80211_supported_band *sband;
   struct ieee80211_channel *ch;
   const struct ieee80211_reg_rule *reg_rule;

   sband = wiphy->bands[IEEE80211_BAND_2GHZ];
   if (!sband)
      return;

   /*
    * If no country IE has been received always enable active scan
    * on these channels. This is only done for specific regulatory SKUs
    */
   if (initiator != NL80211_REGDOM_SET_BY_COUNTRY_IE) {
      ch = &sband->channels[11]; /* CH 12 */
      if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
         ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
      ch = &sband->channels[12]; /* CH 13 */
      if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
         ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
      return;
   }

   /*
    * If a country IE has been received check its rule for this
    * channel first before enabling active scan. The passive scan
    * would have been enforced by the initial processing of our
    * custom regulatory domain.
    */

   ch = &sband->channels[11]; /* CH 12 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
            freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq),
                        0, &reg_rule);
#else
   reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq));
#endif

   if (!IS_ERR(reg_rule)) {
      if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
         if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
            ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
   }

   ch = &sband->channels[12]; /* CH 13 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
            freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq),
                        0, &reg_rule);
#else
   reg_rule = freq_reg_info(wiphy, MHZ_TO_KHZ(ch->center_freq));
#endif
   if (!IS_ERR(reg_rule)) {
      if (!(reg_rule->flags & NL80211_RRF_PASSIVE_SCAN))
         if (ch->flags & IEEE80211_CHAN_PASSIVE_SCAN)
            ch->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
   }
}

/* Always apply Radar/DFS rules on freq range 5260 MHz - 5700 MHz */
static void vos_reg_apply_radar_flags(struct wiphy *wiphy)
{
   struct ieee80211_supported_band *sband;
   struct ieee80211_channel *ch;
   unsigned int i;

   if (!wiphy->bands[IEEE80211_BAND_5GHZ])
      return;

   sband = wiphy->bands[IEEE80211_BAND_5GHZ];

   for (i = 0; i < sband->n_channels; i++) {
      ch = &sband->channels[i];
      if (!vos_is_radar_freq(ch->center_freq))
         continue;
      /* We always enable radar detection/DFS on this
       * frequency range. Additionally we also apply on
       * this frequency range:
       * - If STA mode does not yet have DFS supports disable
       * active scanning
       * - If adhoc mode does not support DFS yet then
       *   disable adhoc in the frequency.
       * - If AP mode does not yet support radar detection/DFS
       *   do not allow AP mode
       */
      if (!(ch->flags & IEEE80211_CHAN_DISABLED))
         ch->flags |= IEEE80211_CHAN_RADAR |
            IEEE80211_CHAN_NO_IBSS |
            IEEE80211_CHAN_PASSIVE_SCAN;
   }
}

static void vos_reg_apply_world_flags(struct wiphy *wiphy,
      enum nl80211_reg_initiator initiator,
      struct regulatory *reg)
{
   REG_DMN_PAIR_MAPPING *regpair;
   regpair = (REG_DMN_PAIR_MAPPING *)reg->regpair;
   switch (regpair->regDmnEnum) {
      case 0x60:
      case 0x63:
      case 0x66:
      case 0x67:
      case 0x6C:
         vos_reg_apply_beaconing_flags(wiphy, initiator);
         break;
      case 0x68:
         vos_reg_apply_beaconing_flags(wiphy, initiator);
         vos_reg_apply_active_scan_flags(wiphy, initiator);
         break;
   }
}

static int regd_init_wiphy(hdd_context_t *pHddCtx, struct regulatory *reg,
      struct wiphy *wiphy)
{
   const struct ieee80211_regdomain *regd;

   if  (pHddCtx->cfg_ini->fRegChangeDefCountry) {
       regd = vos_custom_world_regdomain();
       wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
   }
   else if (is_world_regd(reg->reg_domain))
   {
       regd = vos_world_regdomain(reg);
       wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
   }
   else
   {
       regd = vos_default_world_regdomain();
       wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
   }
   wiphy_apply_custom_regulatory(wiphy, regd);
   vos_reg_apply_radar_flags(wiphy);
   vos_reg_apply_world_flags(wiphy, NL80211_REGDOM_SET_BY_DRIVER, reg);
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
   for (i = 0; i < countryInfoTable.countryCount; i++)
   {
      memcpy( pBuffer, countryInfoTable.countryInfo[i].countryCode, VOS_COUNTRY_CODE_LEN );
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
        for( i = RF_CHAN_36; i <= RF_CHAN_165; i++ )
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
            for( i = RF_CHAN_36; i <= RF_CHAN_165; i++ )
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
   memcpy(&tableData->defaultCountryTable, &pnvEFSTable->halnv.tables.defaultCountryTable, sizeof(sDefaultCountry));
   pr_info("DefaultCountry is %c%c\n",
            tableData->defaultCountryTable.countryCode[0],
            tableData->defaultCountryTable.countryCode[1]);
   return status;
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

   for(channelLoop = 0; channelLoop <= RF_CHAN_165; channelLoop++)
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

/******************************************************************
 Add CRDA regulatory support
*******************************************************************/

static int bw20_ch_index_to_bw40_ch_index(int k)
{
   int m = -1;
   if (k >= RF_CHAN_1 && k <= RF_CHAN_14)
   {
      m = k - RF_CHAN_1 + RF_CHAN_BOND_3 ;
      if (m > RF_CHAN_BOND_11)
         m = RF_CHAN_BOND_11;
   }
   else if (k >= RF_CHAN_240 && k <= RF_CHAN_216)
   {
      m = k - RF_CHAN_240 + RF_CHAN_BOND_242 ;
      if (m > RF_CHAN_BOND_214)
         m = RF_CHAN_BOND_214;
   }
   else if (k >= RF_CHAN_36 && k <= RF_CHAN_64)
   {
      m = k - RF_CHAN_36 + RF_CHAN_BOND_38;
      if (m > RF_CHAN_BOND_62)
         m = RF_CHAN_BOND_62;
   }
#ifdef FEATURE_WLAN_CH144
   else if (k >= RF_CHAN_100 && k <= RF_CHAN_144)
#else
   else if (k >= RF_CHAN_100 && k <= RF_CHAN_140)
#endif /* FEATURE_WLAN_CH144 */
   {
      m = k - RF_CHAN_100 + RF_CHAN_BOND_102;
#ifdef FEATURE_WLAN_CH144
      if (m > RF_CHAN_BOND_142)
         m = RF_CHAN_BOND_142;
#else
      if (m > RF_CHAN_BOND_138)
         m = RF_CHAN_BOND_138;
#endif /* FEATURE_WLAN_CH144 */
   }
   else if (k >= RF_CHAN_149 && k <= RF_CHAN_165)
   {
      m = k - RF_CHAN_149 + RF_CHAN_BOND_151;
      if (m > RF_CHAN_BOND_163)
         m = RF_CHAN_BOND_163;
   }
   return m;
}

static int create_linux_regulatory_entry(struct wiphy *wiphy,
                                         v_U8_t nBandCapability);

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
    int wait_result;

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

        /* the country was not found in the driver database */
        /* so we will return the REGDOMAIN_WORLD to SME/CSR */

        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Country does not map to any Regulatory domain"));

        temp_reg_domain = REGDOMAIN_WORLD;
    }

    if (COUNTRY_QUERY == source)
    {
        *pRegDomain = temp_reg_domain;
         return VOS_STATUS_SUCCESS;
    }

    wiphy = pHddCtx->wiphy;

    if (false == wiphy->registered)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("wiphy is not yet registered with the kernel") );
        return VOS_STATUS_E_FAULT;
    }

    /* We need to query the kernel to get the regulatory information
       for this country */


    /* First compare the country code with the existing current country code
       . If both are same there is no need to query any database */

    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   ("regdomain request"));

    if ((country_code[0] == linux_reg_cc[0]) &&
        (country_code[1] == linux_reg_cc[1])) {

        /* country code already exists */

        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   (" country code already exists"));

        *pRegDomain = cur_reg_domain;

        return VOS_STATUS_SUCCESS;
    }
    else {

        /* get the regulatory information from the kernel
           database */

        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
                   (" get country information from kernel db"));


        if ((COUNTRY_INIT == source) && (VOS_FALSE == init_by_reg_core))
        {
            init_by_driver = VOS_TRUE;

            INIT_COMPLETION(pHddCtx->linux_reg_req);
            regulatory_hint(wiphy, country_code);
            wait_result = wait_for_completion_timeout(
                &pHddCtx->linux_reg_req,
                msecs_to_jiffies(LINUX_REG_WAIT_TIME));

            /* if the country information does not exist with the kernel,
               then the driver callback would not be called */

            if (wait_result > 0) {

                /* the driver callback was called. this means the country
                   regulatory information was found in the kernel database.
                   The callback would have updated the internal database. Here
                   update the country and the return value for the regulatory
                   domain */

                VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                           ("init time regulatory hint callback got called"));

                *pRegDomain = temp_reg_domain;
                cur_reg_domain = temp_reg_domain;
                linux_reg_cc[0] = country_code[0];
                linux_reg_cc[1] = country_code[1];

                return VOS_STATUS_SUCCESS;
            }
            else {

                /* the country information has not been found in the kernel
                   database, return failure */

                VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                           ("init time reg hint callback not called"));

                /* Set to world only if kernel never respnded before*/
                if ((linux_reg_cc[0] == 0) && (linux_reg_cc[1] == 0))
                {
                   temp_reg_domain = REGDOMAIN_WORLD;
                   cur_reg_domain = temp_reg_domain;

                   if (create_linux_regulatory_entry(wiphy,
                            pHddCtx->cfg_ini->nBandCapability) != 0)
                   {
                      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                            ("Error while creating regulatory entry"));
                      return VOS_STATUS_E_FAULT;
                   }
                }
                *pRegDomain = temp_reg_domain;

                return VOS_STATUS_SUCCESS;
            }
        }
        else if (COUNTRY_IE == source || COUNTRY_USER == source)
        {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
            regulatory_hint_user(country_code,NL80211_USER_REG_HINT_USER);
#else
            regulatory_hint_user(country_code);
#endif
            *pRegDomain = temp_reg_domain;
        }
        else if (COUNTRY_INIT == source)
        {
            *pRegDomain = temp_reg_domain;
        }

   }

   return VOS_STATUS_SUCCESS;
}


/* create_linux_regulatory_entry to populate internal structures from wiphy */
static int create_linux_regulatory_entry(struct wiphy *wiphy,
                v_U8_t nBandCapability)
{
    int i, j, m;
    int k = 0, n = 0;
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
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
    }

    /* 20MHz channels */
    if (nBandCapability == eCSR_BAND_24)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "BandCapability is set to 2G only");

    for (i = 0, m = 0; i<IEEE80211_NUM_BANDS; i++)
    {
        /* 5G only */
        if (i == IEEE80211_BAND_2GHZ && nBandCapability == eCSR_BAND_5G)
            continue;

        /* 2G only */
        else if (i == IEEE80211_BAND_5GHZ && nBandCapability == eCSR_BAND_24)
            continue;

        if (wiphy->bands[i] == NULL)
        {

            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "error: wiphy->bands is NULL, i = %d", i);
            continue;
        }

        /* internal channels[] is one continous array for both 2G and 5G bands
           m is internal starting channel index for each band */

        if (i == 0)
            m = 0;
        else
            m = wiphy->bands[i-1]->n_channels + m;

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            /* k = (m + j) is internal current channel index for 20MHz channel
              n is internal channel index for corresponding 40MHz channel */

            k = m + j;
            n = bw20_ch_index_to_bw40_ch_index(k);

            if (n == -1)
                return -1;

            /* If the regulatory rules for a country do not explicilty
             * require a passive scan on a frequency, lift the passive
             * scan restriction
             */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
            reg_rule = freq_reg_info(wiphy,
                                     MHZ_TO_KHZ(wiphy->bands[i]->channels[j].center_freq));
#else
            err = freq_reg_info(wiphy,
                                MHZ_TO_KHZ(wiphy->bands[i]->channels[j].center_freq),
                                0, &reg_rule);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
            if (!IS_ERR(reg_rule))
#else
            if (0 == err)
#endif
            {
                if  (wiphy->flags & WIPHY_FLAG_CUSTOM_REGULATORY) {

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

            if (wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_DISABLED)
            {
                if (pnvEFSTable == NULL)
                {
                    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                              "error: pnvEFSTable is NULL, probably not parsed nv.bin yet");
                    return -1;
                }
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].enabled =
                    NV_CHANNEL_DISABLE;
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                    NV_CHANNEL_DISABLE;
            }

            /* nv cannot distinguish between DFS and passive channels */
            else if (wiphy->bands[i]->channels[j].flags &
                    (IEEE80211_CHAN_RADAR | IEEE80211_CHAN_PASSIVE_SCAN))
            {
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].enabled =
                    NV_CHANNEL_DFS;

                /* max_power is in mBm = 100 * dBm */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].pwrLimit =
                    (tANI_S8) ((wiphy->bands[i]->channels[j].max_power));

                /* Disable the center channel if neither HT40+ nor HT40- is allowed
                 */
                if ((wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_NO_HT40) ==
                                                             IEEE80211_CHAN_NO_HT40 )
                {
                   pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_DISABLE;
                }
                else
                {
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_DFS;

                    /* 40MHz channel power is half of 20MHz (-3dB) ?? */
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].pwrLimit =
                        (tANI_S8) (((wiphy->bands[i]->channels[j].max_power))-3);
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
            else /* Enable is only last flag we support */
            {
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].
                    channels[k].enabled = NV_CHANNEL_ENABLE;

                /* max_power is in dBm */
                pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[k].pwrLimit =
                    (tANI_S8) ((wiphy->bands[i]->channels[j].max_power));

                /* Disable the center channel if neither HT40+ nor HT40- is allowed
                 */
                if ((wiphy->bands[i]->channels[j].flags & IEEE80211_CHAN_NO_HT40) ==
                                                             IEEE80211_CHAN_NO_HT40 )
                {
                   pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_DISABLE;
                }
                else
                {
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].enabled =
                        NV_CHANNEL_ENABLE;
                    /* 40MHz channel power is half of 20MHz (-3dB) */
                    pnvEFSTable->halnv.tables.regDomains[temp_reg_domain].channels[n].pwrLimit =
                        (tANI_S8) (((wiphy->bands[i]->channels[j].max_power))-3);
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

            /* ignore CRDA max_antenna_gain typical is 3dBi, nv.bin antennaGain
               is real gain which should be provided by the real design */
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

    return 0;
}

/*
 * Function: wlan_hdd_linux_reg_notifier
 * This function is called from cfg80211 core to provide regulatory settings
 * after new country is requested or intersected (init, user input or 11d)
 * This function is used to create a CRDA regulatory settings entry into internal
 * regulatory setting table.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
void wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
#else
int wlan_hdd_linux_reg_notifier(struct wiphy *wiphy,
                struct regulatory_request *request)
#endif
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    eCsrBand nBandCapability = eCSR_BAND_ALL;
    v_COUNTRYCODE_t country_code;
    int i,j;
    v_BOOL_t isVHT80Allowed;

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
               "cfg80211 reg notifier callback for country for initiator %d", request->initiator);

    if (TRUE == isWDresetInProgress())
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("SSR is in progress") );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
       return;
#else
       return 0;
#endif
    }

    if (NULL == pHddCtx)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   ("Invalid pHddCtx pointer") );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
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

        if ( VOS_TRUE == init_by_driver)
        {
            isVHT80Allowed = pHddCtx->isVHT80Allowed;
            if (create_linux_regulatory_entry(wiphy, nBandCapability) == 0)
            {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                          (" regulatory entry created"));
            }
            if (pHddCtx->isVHT80Allowed != isVHT80Allowed)
            {
                hdd_checkandupdate_phymode( pHddCtx);
            }
            complete(&pHddCtx->linux_reg_req);
            break;
        }

        /* we purposely want to fall thru since the processing is same
           as other 2 conditions */

    case NL80211_REGDOM_SET_BY_CORE:
    case NL80211_REGDOM_SET_BY_USER:

        /* first lookup the country in the local database */
        country_code[0] = request->alpha2[0];
        country_code[1] = request->alpha2[1];

        pHddCtx->reg.alpha2[0] = request->alpha2[0];
        pHddCtx->reg.alpha2[1] = request->alpha2[1];

        if (NL80211_REGDOM_SET_BY_CORE == request->initiator)
            pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_CORE;
        else if (NL80211_REGDOM_SET_BY_DRIVER == request->initiator)
            pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_DRIVER;
        else pHddCtx->reg.cc_src = COUNTRY_CODE_SET_BY_USER;

        vos_update_reg_info(pHddCtx);
        vos_reg_apply_world_flags(wiphy, request->initiator, &pHddCtx->reg);

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
        if (create_linux_regulatory_entry(wiphy,
                                          nBandCapability) == 0)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                      (" regulatory entry created"));

        }
        if (pHddCtx->isVHT80Allowed != isVHT80Allowed)
        {
            hdd_checkandupdate_phymode( pHddCtx);
        }

        cur_reg_domain = temp_reg_domain;
        linux_reg_cc[0] = country_code[0];
        linux_reg_cc[1] = country_code[1];

        if ((VOS_TRUE == init_by_reg_core) || (VOS_TRUE == init_by_driver)) {
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
        }

        if ((VOS_FALSE == init_by_driver) &&
            (request->initiator != NL80211_REGDOM_SET_BY_CORE))
            init_by_reg_core = VOS_TRUE;


        /* send CTL info to firmware */
        regdmn_set_regval(&pHddCtx->reg);
    default:
        break;
    }

    /* Mark channels 36-48 as passive for US CC */

    if ((request->initiator == NL80211_REGDOM_SET_BY_DRIVER) ||
        (request->initiator == NL80211_REGDOM_SET_BY_CORE) ||
        (request->initiator == NL80211_REGDOM_SET_BY_USER))
    {
        if (pHddCtx->cfg_ini->gEnableStrictRegulatoryForFCC &&
            wiphy->bands[IEEE80211_BAND_5GHZ])
        {
            for (j=0; j<wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels; j++)
            {
                // UNII-1 band channels are passive when domain is FCC.
                if ((wiphy->bands[IEEE80211_BAND_5GHZ ]->channels[j].center_freq == 5180 ||
                     wiphy->bands[IEEE80211_BAND_5GHZ]->channels[j].center_freq == 5200 ||
                     wiphy->bands[IEEE80211_BAND_5GHZ]->channels[j].center_freq == 5220 ||
                     wiphy->bands[IEEE80211_BAND_5GHZ]->channels[j].center_freq == 5240) &&
                    (request->alpha2[0]== 'U' && request->alpha2[1]=='S'))
                {
                    wiphy->bands[IEEE80211_BAND_5GHZ]->channels[j].flags |= IEEE80211_CHAN_PASSIVE_SCAN;
                }
            }
        }
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
    return;
#else
    return 0;
#endif
}

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
               pHddCtx->cfg_ini->nBandCapability) != 0) {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               ("Error while creating regulatory entry"));
         return VOS_STATUS_E_FAULT;
      }
   }

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
        wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
    }
    else if (REGDOMAIN_WORLD ==
	     pnvEFSTable->halnv.tables.defaultCountryTable.regDomain) {

        reg_domain = pnvEFSTable->halnv.tables.defaultCountryTable.regDomain;
        wiphy->flags |= WIPHY_FLAG_CUSTOM_REGULATORY;
    }
    else {

        reg_domain = pnvEFSTable->halnv.tables.defaultCountryTable.regDomain;
        wiphy->flags |= WIPHY_FLAG_STRICT_REGULATORY;
    }
    m = 0;
    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {

        if (wiphy->bands[i] == NULL)
        {
            pr_info("error: wiphy->bands[i] is NULL, i = %d\n", i);
            continue;
        }

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

