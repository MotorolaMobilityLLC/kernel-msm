/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/*============================================================================
  FILE:         vos_nvitem.c
  OVERVIEW:     This source file contains definitions for vOS NV Item APIs
  DEPENDENCIES: NV, remote API client, WinCE REX
                Copyright (c) 2008 QUALCOMM Incorporated.
                All Rights Reserved.
                Qualcomm Confidential and Proprietary
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
 
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define VALIDITY_BITMAP_NV_ID    NV_WLAN_VALIDITY_BITMAP_I
#define VALIDITY_BITMAP_SIZE     32
#define MAX_COUNTRY_COUNT        300
//To be removed when NV support is fully functional
#define VOS_HARD_CODED_MAC    {0, 0x0a, 0xf5, 4, 5, 6}
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
    254,
    {
        { REGDOMAIN_FCC, {'U', 'S'}},       // USA - must be the first country code
        { REGDOMAIN_WORLD, {'A', 'D'}},     //ANDORRA
        { REGDOMAIN_WORLD,{'A', 'E'}},   //UAE
        { REGDOMAIN_WORLD, {'A', 'F'}},     //AFGHANISTAN
        { REGDOMAIN_NO_5GHZ, {'A', 'G'}},     //ANTIGUA AND BARBUDA
        { REGDOMAIN_NO_5GHZ, {'A', 'I'}},     //ANGUILLA
        { REGDOMAIN_WORLD, {'A', 'L'}},     //ALBANIA
        { REGDOMAIN_NO_5GHZ, {'A', 'M'}},     //ARMENIA
        { REGDOMAIN_WORLD, { 'A', 'N'}},     //NETHERLANDS ANTILLES
        { REGDOMAIN_NO_5GHZ, { 'A', 'O'}},     //ANGOLA
        { REGDOMAIN_WORLD, { 'A', 'Q'}},     //ANTARCTICA
        { REGDOMAIN_HI_5GHZ,{ 'A', 'R'}},   //ARGENTINA
        { REGDOMAIN_FCC, { 'A', 'S'}},     //AMERICAN SOMOA
        { REGDOMAIN_ETSI, { 'A', 'T'}},      //AUSTRIA
        { REGDOMAIN_APAC, { 'A', 'U'}},      //AUSTRALIA
        { REGDOMAIN_NO_5GHZ, { 'A', 'W'}},     //ARUBA
        { REGDOMAIN_WORLD, { 'A', 'X'}},     //ALAND ISLANDS
        { REGDOMAIN_NO_5GHZ, { 'A', 'Z'}},     //AZERBAIJAN
        { REGDOMAIN_WORLD, { 'B', 'A'}},     //BOSNIA AND HERZEGOVINA
        { REGDOMAIN_WORLD, { 'B', 'B'}},     //BARBADOS
        { REGDOMAIN_WORLD, { 'B', 'D'}},     //BANGLADESH
        { REGDOMAIN_ETSI,  {'B', 'E'}},      //BELGIUM
        { REGDOMAIN_WORLD, { 'B', 'F'}},     //BURKINA FASO
        { REGDOMAIN_HI_5GHZ, {'B', 'G'}},      //BULGARIA
        { REGDOMAIN_WORLD, { 'B', 'H'}},     //BAHRAIN
        { REGDOMAIN_WORLD, { 'B', 'I'}},     //BURUNDI
        { REGDOMAIN_WORLD, { 'B', 'J'}},     //BENIN
        { REGDOMAIN_ETSI, { 'B', 'M'}},     //BERMUDA
        { REGDOMAIN_WORLD, { 'B', 'N'}},     //BRUNEI DARUSSALAM
        { REGDOMAIN_WORLD,{ 'B', 'O'}},   //BOLIVIA
        { REGDOMAIN_WORLD, {'B', 'R'}},       //BRAZIL
        { REGDOMAIN_WORLD, { 'B', 'S'}},     //BAHAMAS
        { REGDOMAIN_WORLD, { 'B', 'T'}},     //BHUTAN
        { REGDOMAIN_WORLD, { 'B', 'V'}},     //BOUVET ISLAND
        { REGDOMAIN_WORLD, { 'B', 'W'}},     //BOTSWANA
        { REGDOMAIN_WORLD, { 'B', 'Y'}},     //BELARUS
        { REGDOMAIN_WORLD, { 'B', 'Z'}},     //BELIZE
        { REGDOMAIN_FCC, {'C', 'A'}},       //CANADA
        { REGDOMAIN_WORLD, { 'C', 'C'}},     //COCOS (KEELING) ISLANDS
        { REGDOMAIN_WORLD, { 'C', 'D'}},     //CONGO, THE DEMOCRATIC REPUBLIC OF THE
        { REGDOMAIN_WORLD, { 'C', 'F'}},     //CENTRAL AFRICAN REPUBLIC
        { REGDOMAIN_WORLD, { 'C', 'G'}},     //CONGO
        { REGDOMAIN_ETSI, {'C', 'H'}},      //SWITZERLAND
        { REGDOMAIN_WORLD, { 'C', 'I'}},     //COTE D'IVOIRE
        { REGDOMAIN_WORLD, { 'C', 'K'}},     //COOK ISLANDS
        { REGDOMAIN_WORLD, {'C', 'L'}},       //CHILE
        { REGDOMAIN_WORLD, { 'C', 'M'}},     //CAMEROON
        { REGDOMAIN_HI_5GHZ, {'C', 'N'}},   //CHINA
        { REGDOMAIN_WORLD, {'C', 'O'}},       //COLOMBIA
        { REGDOMAIN_WORLD, {'C', 'R'}},       //COSTA RICA
        { REGDOMAIN_WORLD, { 'C', 'U'}},     //CUBA
        { REGDOMAIN_WORLD, { 'C', 'V'}},     //CAPE VERDE
        { REGDOMAIN_WORLD, { 'C', 'X'}},     //CHRISTMAS ISLAND
        { REGDOMAIN_WORLD, {'C', 'Y'}},      //CYPRUS
        { REGDOMAIN_HI_5GHZ, {'C', 'Z'}},      //CZECH REPUBLIC
        { REGDOMAIN_ETSI, {'D', 'E'}},      //GERMANY
        { REGDOMAIN_WORLD, { 'D', 'J'}},     //DJIBOUTI
        { REGDOMAIN_ETSI, {'D', 'K'}},      //DENMARK
        { REGDOMAIN_WORLD, { 'D', 'M'}},     //DOMINICA
        { REGDOMAIN_NO_5GHZ,{ 'D', 'O'}},   //DOMINICAN REPUBLIC
        { REGDOMAIN_WORLD, { 'D', 'Z'}},     //ALGERIA
        { REGDOMAIN_WORLD,{ 'E', 'C'}},   //ECUADOR
        { REGDOMAIN_HI_5GHZ, {'E', 'E'}},      //ESTONIA
        { REGDOMAIN_WORLD, { 'E', 'G'}},     //EGYPT
        { REGDOMAIN_WORLD, { 'E', 'H'}},     //WESTERN SAHARA
        { REGDOMAIN_WORLD, { 'E', 'R'}},     //ERITREA
        { REGDOMAIN_ETSI, {'E', 'S'}},      //SPAIN
        { REGDOMAIN_WORLD, { 'E', 'T'}},     //ETHIOPIA
        { REGDOMAIN_WORLD, {'F', 'I'}},      //FINLAND
        { REGDOMAIN_WORLD, { 'F', 'J'}},     //FIJI
        { REGDOMAIN_WORLD, { 'F', 'K'}},     //FALKLAND ISLANDS (MALVINAS)
        { REGDOMAIN_WORLD, { 'F', 'M'}},     //MICRONESIA, FEDERATED STATES OF
        { REGDOMAIN_WORLD, { 'F', 'O'}},     //FAROE ISLANDS
        { REGDOMAIN_ETSI, {'F', 'R'}},      //FRANCE
        { REGDOMAIN_WORLD, { 'G', 'A'}},     //GABON
        { REGDOMAIN_ETSI, {'G', 'B'}},      //UNITED KINGDOM
        { REGDOMAIN_WORLD, { 'G', 'D'}},     //GRENADA
        { REGDOMAIN_HI_5GHZ, { 'G', 'E'}},     //GEORGIA
        { REGDOMAIN_WORLD, { 'G', 'F'}},     //FRENCH GUIANA
        { REGDOMAIN_ETSI, {'G', 'G'}},      //GUERNSEY
        { REGDOMAIN_WORLD, { 'G', 'H'}},     //GHANA
        { REGDOMAIN_WORLD, {'G', 'I'}},      //GIBRALTAR
        { REGDOMAIN_WORLD, { 'G', 'L'}},     //GREENLAND
        { REGDOMAIN_WORLD, { 'G', 'M'}},     //GAMBIA
        { REGDOMAIN_WORLD, { 'G', 'N'}},     //GUINEA
        { REGDOMAIN_WORLD, { 'G', 'P'}},     //GUADELOUPE
        { REGDOMAIN_WORLD, { 'G', 'Q'}},     //EQUATORIAL GUINEA
        { REGDOMAIN_ETSI, {'G', 'R'}},      //GREECE
        { REGDOMAIN_WORLD, { 'G', 'S'}},     //SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS
        { REGDOMAIN_WORLD,{ 'G', 'T'}},   //GUATEMALA
        { REGDOMAIN_WORLD, { 'G', 'U'}},     //GUAM
        { REGDOMAIN_WORLD, { 'G', 'W'}},     //GUINEA-BISSAU
        { REGDOMAIN_WORLD, { 'G', 'Y'}},     //GUYANA
        { REGDOMAIN_WORLD, {'H', 'K'}},      //HONGKONG
        { REGDOMAIN_WORLD, { 'H', 'M'}},     //HEARD ISLAND AND MCDONALD ISLANDS
        { REGDOMAIN_WORLD,{'H', 'N'}},   //HONDURAS
        { REGDOMAIN_HI_5GHZ, {'H', 'R'}},      //CROATIA
        { REGDOMAIN_WORLD, { 'H', 'T'}},     //HAITI
        { REGDOMAIN_HI_5GHZ, {'H', 'U'}},      //HUNGARY
        { REGDOMAIN_APAC, { 'I', 'D'}},     //INDONESIA
        { REGDOMAIN_ETSI, {'I', 'E'}},     //IRELAND
        { REGDOMAIN_WORLD, {'I', 'L'}},        //ISREAL
        { REGDOMAIN_ETSI, {'I', 'M'}},      //ISLE OF MAN
        { REGDOMAIN_APAC, {'I', 'N'}},      //INDIA
        { REGDOMAIN_ETSI, { 'I', 'O'}},     //BRITISH INDIAN OCEAN TERRITORY
        { REGDOMAIN_WORLD, { 'I', 'Q'}},     //IRAQ
        { REGDOMAIN_WORLD, { 'I', 'R'}},     //IRAN, ISLAMIC REPUBLIC OF
        { REGDOMAIN_WORLD, {'I', 'S'}},      //ICELAND
        { REGDOMAIN_ETSI, {'I', 'T'}},      //ITALY
        { REGDOMAIN_ETSI, {'J', 'E'}},      //JERSEY
        { REGDOMAIN_WORLD, { 'J', 'M'}},     //JAMAICA
        { REGDOMAIN_WORLD, { 'J', 'O'}},     //JORDAN
        { REGDOMAIN_JAPAN, {'J', 'P'}},     //JAPAN
        { REGDOMAIN_WORLD, { 'K', 'E'}},     //KENYA
        { REGDOMAIN_WORLD, { 'K', 'G'}},     //KYRGYZSTAN
        { REGDOMAIN_WORLD, { 'K', 'H'}},     //CAMBODIA
        { REGDOMAIN_WORLD, { 'K', 'I'}},     //KIRIBATI
        { REGDOMAIN_WORLD, { 'K', 'M'}},     //COMOROS
        { REGDOMAIN_WORLD, { 'K', 'N'}},     //SAINT KITTS AND NEVIS
        { REGDOMAIN_KOREA, { 'K', 'P'}},     //KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF
        { REGDOMAIN_KOREA, {'K', 'R'}},     //KOREA, REPUBLIC OF
        { REGDOMAIN_WORLD, { 'K', 'W'}},     //KUWAIT
        { REGDOMAIN_WORLD, { 'K', 'Y'}},     //CAYMAN ISLANDS
        { REGDOMAIN_WORLD, { 'K', 'Z'}},     //KAZAKHSTAN
        { REGDOMAIN_WORLD, { 'L', 'A'}},     //LAO PEOPLE'S DEMOCRATIC REPUBLIC
        { REGDOMAIN_WORLD, { 'L', 'B'}},     //LEBANON
        { REGDOMAIN_WORLD, { 'L', 'C'}},     //SAINT LUCIA
        { REGDOMAIN_ETSI, {'L', 'I'}},      //LIECHTENSTEIN
        { REGDOMAIN_WORLD, { 'L', 'K'}},     //SRI LANKA
        { REGDOMAIN_WORLD, { 'L', 'R'}},     //LIBERIA
        { REGDOMAIN_WORLD, { 'L', 'S'}},     //LESOTHO
        { REGDOMAIN_HI_5GHZ, {'L', 'T'}},      //LITHUANIA
        { REGDOMAIN_ETSI, {'L', 'U'}},      //LUXEMBOURG
        { REGDOMAIN_HI_5GHZ, {'L', 'V'}},      //LATVIA
        { REGDOMAIN_WORLD, { 'L', 'Y'}},     //LIBYAN ARAB JAMAHIRIYA
        { REGDOMAIN_WORLD, { 'M', 'A'}},     //MOROCCO
        { REGDOMAIN_ETSI, {'M', 'C'}},      //MONACO
        { REGDOMAIN_WORLD, { 'M', 'D'}},     //MOLDOVA, REPUBLIC OF
        { REGDOMAIN_WORLD, { 'M', 'E'}},     //MONTENEGRO
        { REGDOMAIN_WORLD, { 'M', 'G'}},     //MADAGASCAR
        { REGDOMAIN_WORLD, { 'M', 'H'}},     //MARSHALL ISLANDS
        { REGDOMAIN_WORLD, { 'M', 'K'}},     //MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF
        { REGDOMAIN_WORLD, { 'M', 'L'}},     //MALI
        { REGDOMAIN_WORLD, { 'M', 'M'}},     //MYANMAR
        { REGDOMAIN_HI_5GHZ, { 'M', 'N'}},     //MONGOLIA
        { REGDOMAIN_WORLD, { 'M', 'O'}},     //MACAO
        { REGDOMAIN_WORLD, { 'M', 'P'}},     //NORTHERN MARIANA ISLANDS
        { REGDOMAIN_WORLD, { 'M', 'Q'}},     //MARTINIQUE
        { REGDOMAIN_WORLD, { 'M', 'R'}},     //MAURITANIA
        { REGDOMAIN_WORLD, { 'M', 'S'}},     //MONTSERRAT
        { REGDOMAIN_WORLD, {'M', 'T'}},      //MALTA
        { REGDOMAIN_WORLD, { 'M', 'U'}},     //MAURITIUS
        { REGDOMAIN_WORLD, { 'M', 'V'}},     //MALDIVES
        { REGDOMAIN_WORLD, { 'M', 'W'}},     //MALAWI
        { REGDOMAIN_WORLD, {'M', 'X'}},       //MEXICO
        { REGDOMAIN_HI_5GHZ,{ 'M', 'Y'}},   //MALAYSIA
        { REGDOMAIN_WORLD, { 'M', 'Z'}},     //MOZAMBIQUE
        { REGDOMAIN_WORLD, { 'N', 'A'}},     //NAMIBIA
        { REGDOMAIN_WORLD, { 'N', 'C'}},     //NEW CALEDONIA
        { REGDOMAIN_WORLD, { 'N', 'E'}},     //NIGER
        { REGDOMAIN_WORLD, { 'N', 'F'}},     //NORFOLD ISLAND
        { REGDOMAIN_WORLD, { 'N', 'G'}},     //NIGERIA
        { REGDOMAIN_WORLD,{ 'N', 'I'}},       //NICARAGUA
        { REGDOMAIN_ETSI, {'N', 'L'}},      //NETHERLANDS
        { REGDOMAIN_WORLD, {'N', 'O'}},      //NORWAY
        { REGDOMAIN_WORLD, { 'N', 'P'}},     //NEPAL
        { REGDOMAIN_WORLD, { 'N', 'R'}},     //NAURU
        { REGDOMAIN_WORLD, { 'N', 'U'}},     //NIUE
        { REGDOMAIN_ETSI, {'N', 'Z'}},      //NEW ZEALAND
        { REGDOMAIN_WORLD, { 'O', 'M'}},     //OMAN
        { REGDOMAIN_WORLD, {'P', 'A'}},       //PANAMA
        { REGDOMAIN_WORLD,{ 'P', 'E'}},   //PERU
        { REGDOMAIN_WORLD, { 'P', 'F'}},     //FRENCH POLYNESIA
        { REGDOMAIN_WORLD, { 'P', 'G'}},     //PAPUA NEW GUINEA
        { REGDOMAIN_WORLD, {'P', 'H'}},      //PHILIPPINES
        { REGDOMAIN_WORLD, { 'P', 'K'}},     //PAKISTAN
        { REGDOMAIN_ETSI, {'P', 'L'}},      //POLAND
        { REGDOMAIN_WORLD, { 'P', 'M'}},     //SAINT PIERRE AND MIQUELON
        { REGDOMAIN_WORLD, { 'P', 'N'}},     //WORLDPITCAIRN
        { REGDOMAIN_FCC, {'P', 'R'}},       //PUERTO RICO
        { REGDOMAIN_WORLD, {'P', 'S'}},        //PALESTINIAN TERRITORY, OCCUPIED
        { REGDOMAIN_ETSI, {'P', 'T'}},      //PORTUGAL
        { REGDOMAIN_WORLD, { 'P', 'W'}},     //PALAU
        { REGDOMAIN_WORLD, { 'P', 'Y'}},     //PARAGUAY
        { REGDOMAIN_WORLD, { 'Q', 'A'}},     //QATAR
        { REGDOMAIN_WORLD, { 'R', 'E'}},     //REUNION
        { REGDOMAIN_HI_5GHZ, {'R', 'O'}},      //ROMANIA
        { REGDOMAIN_HI_5GHZ, {'R', 'S'}},      //SERBIA
        { REGDOMAIN_WORLD, {'R', 'U'}},   //RUSSIA
        { REGDOMAIN_WORLD, { 'R', 'W'}},     //RWANDA
        { REGDOMAIN_WORLD, {'S', 'A'}},      //SAUDI ARABIA
        { REGDOMAIN_WORLD, { 'S', 'B'}},     //SOLOMON ISLANDS
        { REGDOMAIN_ETSI, {'S', 'C'}},      //SEYCHELLES
        { REGDOMAIN_WORLD, { 'S', 'D'}},     //SUDAN
        { REGDOMAIN_ETSI, {'S', 'E'}},      //SWEDEN
        { REGDOMAIN_APAC, {'S', 'G'}},      //SINGAPORE
        { REGDOMAIN_WORLD, { 'S', 'H'}},     //SAINT HELENA
        { REGDOMAIN_HI_5GHZ, {'S', 'I'}},      //SLOVENNIA
        { REGDOMAIN_WORLD, { 'S', 'J'}},     //SVALBARD AND JAN MAYEN
        { REGDOMAIN_ETSI, {'S', 'K'}},      //SLOVAKIA
        { REGDOMAIN_WORLD, { 'S', 'L'}},     //SIERRA LEONE
        { REGDOMAIN_WORLD, { 'S', 'M'}},     //SAN MARINO
        { REGDOMAIN_WORLD, { 'S', 'N'}},     //SENEGAL
        { REGDOMAIN_WORLD, { 'S', 'O'}},     //SOMALIA
        { REGDOMAIN_WORLD, { 'S', 'R'}},     //SURINAME
        { REGDOMAIN_WORLD, { 'S', 'T'}},     //SAO TOME AND PRINCIPE
        { REGDOMAIN_WORLD, {'S', 'V'}},       //EL SALVADOR
        { REGDOMAIN_WORLD, { 'S', 'Y'}},     //SYRIAN ARAB REPUBLIC
        { REGDOMAIN_WORLD, { 'S', 'Z'}},     //SWAZILAND
        { REGDOMAIN_WORLD, { 'T', 'C'}},     //TURKS AND CAICOS ISLANDS
        { REGDOMAIN_WORLD, { 'T', 'D'}},     //CHAD
        { REGDOMAIN_WORLD, { 'T', 'F'}},     //FRENCH SOUTHERN TERRITORIES
        { REGDOMAIN_WORLD, { 'T', 'G'}},     //TOGO
        { REGDOMAIN_WORLD,{ 'T', 'H'}},   //THAILAND
        { REGDOMAIN_WORLD, { 'T', 'J'}},     //TAJIKISTAN
        { REGDOMAIN_WORLD, { 'T', 'K'}},     //TOKELAU
        { REGDOMAIN_WORLD, { 'T', 'L'}},     //TIMOR-LESTE
        { REGDOMAIN_WORLD, { 'T', 'M'}},     //TURKMENISTAN
        { REGDOMAIN_WORLD, { 'T', 'N'}},     //TUNISIA
        { REGDOMAIN_WORLD, { 'T', 'O'}},     //TONGA
        { REGDOMAIN_WORLD, {'T', 'R'}},      //TURKEY
        { REGDOMAIN_WORLD, { 'T', 'T'}},     //TRINIDAD AND TOBAGO
        { REGDOMAIN_WORLD, { 'T', 'V'}},     //TUVALU
        { REGDOMAIN_HI_5GHZ,{ 'T', 'W'}},   //TAIWAN, PROVINCE OF CHINA
        { REGDOMAIN_WORLD, { 'T', 'Z'}},     //TANZANIA, UNITED REPUBLIC OF
        { REGDOMAIN_HI_5GHZ,{ 'U', 'A'}},   //UKRAINE
        { REGDOMAIN_WORLD, { 'U', 'G'}},     //UGANDA
        { REGDOMAIN_FCC, {'U', 'M'}},       //UNITED STATES MINOR OUTLYING ISLANDS
        { REGDOMAIN_WORLD,{ 'U', 'Y'}},   //URUGUAY
        { REGDOMAIN_HI_5GHZ, { 'U', 'Z'}},     //UZBEKISTAN
        { REGDOMAIN_ETSI, {'V', 'A'}},      //HOLY SEE (VATICAN CITY STATE)
        { REGDOMAIN_WORLD, { 'V', 'C'}},     //SAINT VINCENT AND THE GRENADINES
        { REGDOMAIN_HI_5GHZ,{ 'V', 'E'}},   //VENEZUELA
        { REGDOMAIN_ETSI, {'V', 'G'}},       //VIRGIN ISLANDS, BRITISH
        { REGDOMAIN_FCC, {'V', 'I'}},       //VIRGIN ISLANDS, US
        { REGDOMAIN_WORLD, {'V', 'N'}},      //VIET NAM
        { REGDOMAIN_WORLD, { 'V', 'U'}},     //VANUATU
        { REGDOMAIN_WORLD, { 'W', 'F'}},     //WALLIS AND FUTUNA
        { REGDOMAIN_WORLD, { 'W', 'S'}},     //SOMOA
        { REGDOMAIN_WORLD, { 'Y', 'E'}},     //YEMEN
        { REGDOMAIN_WORLD, { 'Y', 'T'}},     //MAYOTTE
        { REGDOMAIN_WORLD, {'Z', 'A'}},      //SOUTH AFRICA
        { REGDOMAIN_WORLD, { 'Z', 'M'}},     //ZAMBIA
        { REGDOMAIN_WORLD, { 'Z', 'W'}},     //ZIMBABWE
        { REGDOMAIN_KOREA, {'K', '1'}},     //Korea alternate 1
        { REGDOMAIN_KOREA, {'K', '2'}},     //Korea alternate 2
        { REGDOMAIN_KOREA, {'K', '3'}},     //Korea alternate 3
        { REGDOMAIN_KOREA, {'K', '4'}},      //Korea alternate 4
        { REGDOMAIN_ETSI, {'E', 'U'}},       //Europe (SSGFI)
        { REGDOMAIN_JAPAN, {'J', '1'}},     //Japan alternate 1
        { REGDOMAIN_JAPAN, {'J', '2'}},     //Japan alternate 2
        { REGDOMAIN_JAPAN, {'J', '3'}},     //Japan alternate 3
        { REGDOMAIN_JAPAN, {'J', '4'}},     //Japan alternate 4
        { REGDOMAIN_JAPAN, {'J', '5'}}      //Japan alternate 5
    }
};
typedef struct nvEFSTable_s
{
   v_U32_t    nvValidityBitmap;
   sHalNv     halnv;
} nvEFSTable_t;
nvEFSTable_t *gnvEFSTable=NULL;
/* EFS Table  to send the NV structure to HAL*/ 
static nvEFSTable_t *pnvEFSTable =NULL;

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
#ifdef FEATURE_WLAN_INTEGRATED_SOC
    { 0,    240, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_240,
    { 0,    244, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_244,
    { 0,    248, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_248,
    { 0,    252, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_252,
    { 0,    208, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_208,
    { 0,    212, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_212,
    { 0,    216, RF_SUBBAND_4_9_GHZ},        //RF_CHAN_216,
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
    { 5745, 149, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_149,
    { 5765, 153, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_153,
    { 5785, 157, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_157,
    { 5805, 161, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_161,
    { 5825, 165, RF_SUBBAND_5_HIGH_GHZ},     //RF_CHAN_165,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_3,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_4,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_5,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_6,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_7,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_8,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_9,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_10,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_11,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_242,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_246,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_250,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_210,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_214,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_38,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_42,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_46,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_50,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_54,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_58,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_62,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_102,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_106,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_110,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_114,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_118,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_122,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_126,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_130,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_134,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_138,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_151,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_155,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_159,
    { 0   , 0  , NUM_RF_SUBBANDS},           //RF_CHAN_BOND_163,
#endif
};

extern const sHalNv nvDefaults;

const sRegulatoryChannel * regChannels = nvDefaults.tables.regDomains[0].channels;

/*----------------------------------------------------------------------------
   Function Definitions and Documentation
 * -------------------------------------------------------------------------*/
VOS_STATUS wlan_write_to_efs (v_U8_t *pData, v_U16_t data_len);
/**------------------------------------------------------------------------
  \brief vos_nv_init() - initialize the NV module
  The \a vos_nv_init() initializes the NV module.  This read the binary
  file for country code and regulatory domain information.
  \return VOS_STATUS_SUCCESS - module is initialized successfully
          otherwise  - module is not initialized
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_init(void)
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_nv_open(void)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    v_CONTEXT_t pVosContext= NULL;
    v_SIZE_t bufSize;
    v_BOOL_t itemIsValid = VOS_FALSE;
    
    /*Get the global context */
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
    bufSize = sizeof(nvEFSTable_t);
    status = hdd_request_firmware(WLAN_NV_FILE,
                                  ((VosContextType*)(pVosContext))->pHDDContext,
                                  (v_VOID_t**)&gnvEFSTable, &bufSize);

    if ( (!VOS_IS_STATUS_SUCCESS( status )) || !gnvEFSTable)
    {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                   "%s: unable to download NV file %s",
                   __FUNCTION__, WLAN_NV_FILE);
         return VOS_STATUS_E_RESOURCES;
    }

     /* Copying the read nv data to the globa NV EFS table */
    {
        /* Allocate memory to global NV table */
        pnvEFSTable = (nvEFSTable_t *)vos_mem_malloc(sizeof(nvEFSTable_t));
        if (NULL == pnvEFSTable)
        {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s : failed to allocate memory for NV", __FUNCTION__);
            return VOS_STATUS_E_NOMEM;
        }

        /*Copying the NV defaults */
        vos_mem_copy(&(pnvEFSTable->halnv),&nvDefaults,sizeof(sHalNv));
        pnvEFSTable->nvValidityBitmap = gnvEFSTable->nvValidityBitmap;

        /* Copy the valid fields to the NV Global structure */ 
        if (vos_nv_getValidity(VNV_FIELD_IMAGE, &itemIsValid) == 
           VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE) {

                if(vos_nv_read( VNV_FIELD_IMAGE, (v_VOID_t *)&pnvEFSTable->halnv.fields,
                   NULL, sizeof(sNvFields) ) != VOS_STATUS_SUCCESS)
                   return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_RATE_TO_POWER_TABLE, &itemIsValid) == 
             VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_RATE_TO_POWER_TABLE, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.pwrOptimum[0],
                  NULL, sizeof(tRateGroupPwr) * NUM_RF_SUBBANDS ) != VOS_STATUS_SUCCESS)
               return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_REGULARTORY_DOMAIN_TABLE, &itemIsValid) == 
               VOS_STATUS_SUCCESS)
        {
    
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_REGULARTORY_DOMAIN_TABLE,
                (v_VOID_t *)&pnvEFSTable->halnv.tables.regDomains[0],
                 NULL, sizeof(sRegulatoryDomains) * NUM_REG_DOMAINS ) != VOS_STATUS_SUCCESS)
                    return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_DEFAULT_LOCATION, &itemIsValid) == 
            VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_DEFAULT_LOCATION,
                (v_VOID_t *)&pnvEFSTable->halnv.tables.defaultCountryTable,
                NULL, sizeof(sDefaultCountry) ) !=  VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
    
        if (vos_nv_getValidity(VNV_TPC_POWER_TABLE, &itemIsValid) == 
            VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_TPC_POWER_TABLE, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.plutCharacterized[0],
                  NULL, sizeof(tTpcPowerTable) * NUM_RF_CHANNELS ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
    
        if (vos_nv_getValidity(VNV_TPC_PDADC_OFFSETS, &itemIsValid) == 
            VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_TPC_PDADC_OFFSETS,
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.plutPdadcOffset[0],
                  NULL, sizeof(tANI_U16) * NUM_RF_CHANNELS ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
        if (vos_nv_getValidity(VNV_RSSI_CHANNEL_OFFSETS, &itemIsValid) == 
           VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_RSSI_CHANNEL_OFFSETS,
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.rssiChanOffsets[0],
                  NULL, sizeof(sRssiChannelOffsets) * 2 ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
    
        if (vos_nv_getValidity(VNV_RF_CAL_VALUES, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_RF_CAL_VALUES, (v_VOID_t *)&pnvEFSTable->halnv
    .tables.rFCalValues, NULL, sizeof(sRFCalValues) ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_ANTENNA_PATH_LOSS, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_ANTENNA_PATH_LOSS,
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.antennaPathLoss[0], NULL, 
                sizeof(tANI_S16)*NUM_RF_CHANNELS ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
        if (vos_nv_getValidity(VNV_PACKET_TYPE_POWER_LIMITS, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_PACKET_TYPE_POWER_LIMITS, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.pktTypePwrLimits[0], NULL, 
                sizeof(tANI_S16)*NUM_802_11_MODES*NUM_RF_CHANNELS ) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_OFDM_CMD_PWR_OFFSET, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
                if(vos_nv_read( VNV_OFDM_CMD_PWR_OFFSET, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.ofdmCmdPwrOffset, NULL, 
                                sizeof(sOfdmCmdPwrOffset)) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }

        if (vos_nv_getValidity(VNV_TX_BB_FILTER_MODE, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
               if(vos_nv_read(VNV_TX_BB_FILTER_MODE, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.txbbFilterMode, NULL, 
                sizeof(sTxBbFilterMode)) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
        if (vos_nv_getValidity(VNV_TABLE_VIRTUAL_RATE, &itemIsValid) == 
         VOS_STATUS_SUCCESS)
        {
            if (itemIsValid == VOS_TRUE)
            {
               if(vos_nv_read(VNV_TABLE_VIRTUAL_RATE, 
                  (v_VOID_t *)&pnvEFSTable->halnv.tables.pwrOptimum_virtualRate, NULL, 
                sizeof(gnvEFSTable->halnv.tables.pwrOptimum_virtualRate)) != VOS_STATUS_SUCCESS)
                     return (eHAL_STATUS_FAILURE);
            }
        }
    }

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_nv_close(void)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    v_CONTEXT_t pVosContext= NULL;
         /*Get the global context */
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
    status = hdd_release_firmware(WLAN_NV_FILE, ((VosContextType*)(pVosContext))->pHDDContext);
    if ( !VOS_IS_STATUS_SUCCESS( status ))
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                         "%s : vos_open failed\n",__func__);
        return VOS_STATUS_E_FAILURE;
    }
    vos_mem_free(pnvEFSTable);
    gnvEFSTable=NULL;
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
  \return VOS_STATUS_SUCCESS - regulatory domain is found for the given country
          VOS_STATUS_E_FAULT - invalid pointer error
          VOS_STATUS_E_EMPTY - country code table is empty
          VOS_STATUS_E_EXISTS - given country code does not exist in table
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getRegDomainFromCountryCode( v_REGDOMAIN_t *pRegDomain,
      const v_COUNTRYCODE_t countryCode )
{
   int i;
   // sanity checks
   if (NULL == pRegDomain)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            ("Invalid reg domain pointer\r\n") );
      return VOS_STATUS_E_FAULT;
   }
   *pRegDomain = REGDOMAIN_COUNT;

   if (NULL == countryCode)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            ("Country code array is NULL\r\n") );
      return VOS_STATUS_E_FAULT;
   }
   if (0 == countryInfoTable.countryCount)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            ("Reg domain table is empty\r\n") );
      return VOS_STATUS_E_EMPTY;
   }
   // iterate the country info table until end of table or the country code
   // is found
   for (i = 0; i < countryInfoTable.countryCount &&
         REGDOMAIN_COUNT == *pRegDomain; i++)
   {
      if (memcmp(countryCode, countryInfoTable.countryInfo[i].countryCode,
               VOS_COUNTRY_CODE_LEN) == 0)
      {
         // country code is found
         *pRegDomain = countryInfoTable.countryInfo[i].regDomain;
      }
   }
   if (REGDOMAIN_COUNT != *pRegDomain)
   {
      return VOS_STATUS_SUCCESS;
   }
   else
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
            ("country code is not found\r\n"));
      return VOS_STATUS_E_EXISTS;
   }
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
            ("Insufficient memory for country code list\r\n"));
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
  \brief vos_nv_readTxAntennaCount() - return number of TX antenna
  \param pTxAntennaCount   - antenna count
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readTxAntennaCount( v_U8_t *pTxAntennaCount )
{
   sNvFields fieldImage;
   VOS_STATUS status;
   status = vos_nv_read( VNV_FIELD_IMAGE, &fieldImage, NULL,
         sizeof(fieldImage) );
   if (VOS_STATUS_SUCCESS == status)
   {
      *pTxAntennaCount = fieldImage.numOfTxChains;
   }
   return status;
}
/**------------------------------------------------------------------------
  \brief vos_nv_readRxAntennaCount() - return number of RX antenna
  \param pRxAntennaCount   - antenna count
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readRxAntennaCount( v_U8_t *pRxAntennaCount )
{
   sNvFields fieldImage;
   VOS_STATUS status;
   status = vos_nv_read( VNV_FIELD_IMAGE, &fieldImage, NULL,
         sizeof(fieldImage) );
   if (VOS_STATUS_SUCCESS == status)
   {
      *pRxAntennaCount = fieldImage.numOfRxChains;
   }
   return status;
}

/**------------------------------------------------------------------------
  \brief vos_nv_readMacAddress() - return the MAC address
  \param pMacAddress - MAC address
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readMacAddress( v_MAC_ADDRESS_t pMacAddress )
{
   sNvFields fieldImage;
   VOS_STATUS status;
   status = vos_nv_read( VNV_FIELD_IMAGE, &fieldImage, NULL,
         sizeof(fieldImage) );
   if (VOS_STATUS_SUCCESS == status)
   {
      memcpy( pMacAddress, fieldImage.macAddr, VOS_MAC_ADDRESS_LEN );
   }
   else
   {
      //This part of the code can be removed when NV is programmed
      const v_U8_t macAddr[VOS_MAC_ADDRESS_LEN] = VOS_HARD_CODED_MAC;
      memcpy( pMacAddress, macAddr, VOS_MAC_ADDRESS_LEN );
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
          " fail to get MAC address from NV, hardcoded to %02X-%02X-%02X-%02X-%02X%02X",
          macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
      status = VOS_STATUS_SUCCESS;
   }
   return status;
}

/**------------------------------------------------------------------------

  \brief vos_nv_readMultiMacAddress() - return the Multiple MAC addresses

  \param pMacAddress - MAC address
  \param macCount - Count of valid MAC addresses to get from NV field

  \return status of the NV read operation

  \sa

  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_readMultiMacAddress( v_U8_t *pMacAddress,
                                              v_U8_t  macCount )
{
   sNvFields   fieldImage;
   VOS_STATUS  status;
   v_U8_t      countLoop;
   v_U8_t     *pNVMacAddress;

   if((0 == macCount) || (VOS_MAX_CONCURRENCY_PERSONA < macCount) ||
      (NULL == pMacAddress))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          " Invalid Parameter from NV Client macCount %d, pMacAddress 0x%x",
          macCount, pMacAddress);
   }

   status = vos_nv_read( VNV_FIELD_IMAGE, &fieldImage, NULL,
                         sizeof(fieldImage) );
   if (VOS_STATUS_SUCCESS == status)
   {
      pNVMacAddress = fieldImage.macAddr;
      for(countLoop = 0; countLoop < macCount; countLoop++)
      {
         vos_mem_copy(pMacAddress + (countLoop * VOS_MAC_ADDRESS_LEN),
                      pNVMacAddress + (countLoop * VOS_MAC_ADDRESS_LEN),
                      VOS_MAC_ADDRESS_LEN);
      }
   }
   else
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "vos_nv_readMultiMacAddress Get NV Field Fail");
   }

   return status;
}

/**------------------------------------------------------------------------
  \brief vos_nv_setValidity() - set the validity of an NV item.
  The \a vos_nv_setValidity() validates and invalidates an NV item.  The
  validity information is stored in NV memory.
  One would get the VOS_STATUS_E_EXISTS error when reading an invalid item.
  An item becomes valid when one has written to it successfully.
  \param type        - NV item type
  \param itemIsValid - boolean value indicating the item's validity
  \return VOS_STATUS_SUCCESS - validity is set successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAILURE - unknown error
  \sa
  -------------------------------------------------------------------------*/
#ifndef WLAN_FTM_STUB

VOS_STATUS vos_nv_setValidity( VNV_TYPE type, v_BOOL_t itemIsValid )
{
   v_U32_t lastNvValidityBitmap;
   v_U32_t newNvValidityBitmap;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   // check if the current NV type is valid
   if (VNV_TYPE_COUNT <= type)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("%s: invalid type=%d"), __FUNCTION__, type );
      return VOS_STATUS_E_INVAL;
   }
   // read the validity bitmap
   lastNvValidityBitmap = gnvEFSTable->nvValidityBitmap;
   // modify the validity bitmap
   if (itemIsValid)
   {
       newNvValidityBitmap = lastNvValidityBitmap | (1 << type);
              // commit to NV store if bitmap has been modified
       if (newNvValidityBitmap != lastNvValidityBitmap)
       {
           gnvEFSTable->nvValidityBitmap = newNvValidityBitmap;
       }
   }
   else
   {
       newNvValidityBitmap = lastNvValidityBitmap & (~(1 << type));
       if (newNvValidityBitmap != lastNvValidityBitmap)
       {
           gnvEFSTable->nvValidityBitmap = newNvValidityBitmap;
           status = wlan_write_to_efs((v_U8_t*)gnvEFSTable,sizeof(nvEFSTable_t));
           if (! VOS_IS_STATUS_SUCCESS(status)) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, ("vos_nv_write_to_efs failed!!!\r\n"));
               status = VOS_STATUS_E_FAULT;
           }
       }
   }

   return status;
}
#endif
/**------------------------------------------------------------------------
  \brief vos_nv_getValidity() - get the validity of an NV item.
  The \a vos_nv_getValidity() indicates if an NV item is valid.  The
  validity information is stored in NV memory.
  One would get the VOS_STATUS_E_EXISTS error when reading an invalid item.
  An item becomes valid when one has written to it successfully.
  \param type        - NV item type
  \param pItemIsValid- pointer to the boolean value indicating the item's
                       validity
  \return VOS_STATUS_SUCCESS - validity is determined successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAILURE - unknown error
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getValidity( VNV_TYPE type, v_BOOL_t *pItemIsValid )
{
   v_U32_t nvValidityBitmap = gnvEFSTable->nvValidityBitmap;
   // check if the current NV type is valid
   if (VNV_TYPE_COUNT <= type)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("%s: invalid type=%d"), __FUNCTION__, type );
      return VOS_STATUS_E_INVAL;
   }
   *pItemIsValid = (v_BOOL_t)((nvValidityBitmap >> type) & 1);
   return VOS_STATUS_SUCCESS;
}
/**------------------------------------------------------------------------
  \brief vos_nv_read() - read a NV item to an output buffer
  The \a vos_nv_read() reads a NV item to an output buffer.  If the item is
  an array, this function would read the entire array. One would get a
  VOS_STATUS_E_EXISTS error when reading an invalid item.
  For error conditions of VOS_STATUS_E_EXISTS and VOS_STATUS_E_FAILURE,
  if a default buffer is provided (with a non-NULL value),
  the default buffer content is copied to the output buffer.
  \param type  - NV item type
  \param outputBuffer   - output buffer
  \param defaultBuffer  - default buffer
  \param bufferSize  - output buffer size
  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - defaultBuffer point is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE - unknown error
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_read( VNV_TYPE type, v_VOID_t *outputVoidBuffer,
      v_VOID_t *defaultBuffer, v_SIZE_t bufferSize )
{
    VOS_STATUS status             = VOS_STATUS_SUCCESS;
    v_SIZE_t itemSize;
    v_BOOL_t itemIsValid = VOS_TRUE;

    // sanity check
    if (VNV_TYPE_COUNT <= type)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("%s: invalid type=%d"), __FUNCTION__, type );
       return VOS_STATUS_E_INVAL;
    }
    if (NULL == outputVoidBuffer)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             ("Buffer provided is NULL\r\n") );
       return VOS_STATUS_E_FAULT;
    }
    if (0 == bufferSize)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             ("NV type=%d is invalid\r\n"), type );
       return VOS_STATUS_E_INVAL;
    }
    // check if the NV item has valid data
    status = vos_nv_getValidity( type, &itemIsValid );
   if (!itemIsValid)
   {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_WARN,
            "NV type=%d does not have valid data\r\n", type );
       return VOS_STATUS_E_EMPTY;
   }
   switch(type)
   {
       case VNV_FIELD_IMAGE:
           itemSize = sizeof(gnvEFSTable->halnv.fields);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.fields,bufferSize);
           }
           break;
       case VNV_RATE_TO_POWER_TABLE:
           itemSize = sizeof(gnvEFSTable->halnv.tables.pwrOptimum);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.pwrOptimum[0],bufferSize);
           }
           break;
       case VNV_REGULARTORY_DOMAIN_TABLE:
           itemSize = sizeof(gnvEFSTable->halnv.tables.regDomains);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.regDomains[0],bufferSize);
           }
           break;
       case VNV_DEFAULT_LOCATION:
           itemSize = sizeof(gnvEFSTable->halnv.tables.defaultCountryTable);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.defaultCountryTable,bufferSize);
           }
           break;
       case VNV_TPC_POWER_TABLE:
           itemSize = sizeof(gnvEFSTable->halnv.tables.plutCharacterized);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.plutCharacterized[0],bufferSize);
           }
           break;
       case VNV_TPC_PDADC_OFFSETS:
           itemSize = sizeof(gnvEFSTable->halnv.tables.plutPdadcOffset);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.plutPdadcOffset[0],bufferSize);
           }
           break;
       case VNV_RSSI_CHANNEL_OFFSETS:

           itemSize = sizeof(gnvEFSTable->halnv.tables.rssiChanOffsets);

           if(bufferSize != itemSize) {

               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.rssiChanOffsets[0],bufferSize);
           }
           break;
       case VNV_RF_CAL_VALUES:

           itemSize = sizeof(gnvEFSTable->halnv.tables.rFCalValues);

           if(bufferSize != itemSize) {

               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.rFCalValues,bufferSize);
           }
           break;
       case VNV_ANTENNA_PATH_LOSS:
           itemSize = sizeof(gnvEFSTable->halnv.tables.antennaPathLoss);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.antennaPathLoss[0],bufferSize);
           }
           break;
       case VNV_PACKET_TYPE_POWER_LIMITS:
           itemSize = sizeof(gnvEFSTable->halnv.tables.pktTypePwrLimits);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.pktTypePwrLimits[0][0],bufferSize);
           }
           break;
       case VNV_OFDM_CMD_PWR_OFFSET:
           itemSize = sizeof(gnvEFSTable->halnv.tables.ofdmCmdPwrOffset);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.ofdmCmdPwrOffset,bufferSize);
           }
           break;
       case VNV_TX_BB_FILTER_MODE:
           itemSize = sizeof(gnvEFSTable->halnv.tables.txbbFilterMode);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.txbbFilterMode,bufferSize);
           }
           break;

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
       case VNV_FREQUENCY_FOR_1_3V_SUPPLY:
           itemSize = sizeof(gnvEFSTable->halnv.tables.freqFor1p3VSupply);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.freqFor1p3VSupply,bufferSize);
           }
           break;
#endif /* FEATURE_WLAN_NON_INTEGRATED_SOC */

       case VNV_TABLE_VIRTUAL_RATE:
           itemSize = sizeof(gnvEFSTable->halnv.tables.pwrOptimum_virtualRate);
           if(bufferSize != itemSize) {
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                 itemSize);
               status = VOS_STATUS_E_INVAL;
           }
           else {
               memcpy(outputVoidBuffer,&gnvEFSTable->halnv.tables.pwrOptimum_virtualRate,bufferSize);
           }
           break;

       default:
         break;
   }
   return status;
}
#ifndef WLAN_FTM_STUB

/**------------------------------------------------------------------------
  \brief vos_nv_write() - write to a NV item from an input buffer
  The \a vos_nv_write() writes to a NV item from an input buffer. This would
  validate the NV item if the write operation is successful.
  \param type - NV item type
  \param inputBuffer - input buffer
  \param inputBufferSize - input buffer size
  \return VOS_STATUS_SUCCESS - NV item is read successfully
          VOS_STATUS_E_INVAL - one of the parameters is invalid
          VOS_STATUS_E_FAULT - outputBuffer pointer is NULL
          VOS_STATUS_E_EXISTS - NV type is unsupported
          VOS_STATUS_E_FAILURE   - unknown error
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_write( VNV_TYPE type, v_VOID_t *inputVoidBuffer,
      v_SIZE_t bufferSize )
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    v_SIZE_t itemSize;

    // sanity check
    if (VNV_TYPE_COUNT <= type)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                  ("%s: invalid type=%d"), __FUNCTION__, type );
       return VOS_STATUS_E_INVAL;
    }
    if (NULL == inputVoidBuffer)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             ("Buffer provided is NULL\r\n") );
       return VOS_STATUS_E_FAULT;
    }
    if (0 == bufferSize)
    {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             ("NV type=%d is invalid\r\n"), type );
       return VOS_STATUS_E_INVAL;
    }
    switch(type)
    {
        case VNV_FIELD_IMAGE:
            itemSize = sizeof(gnvEFSTable->halnv.fields);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.fields,inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_RATE_TO_POWER_TABLE:
            itemSize = sizeof(gnvEFSTable->halnv.tables.pwrOptimum);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.pwrOptimum[0],inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_REGULARTORY_DOMAIN_TABLE:
            itemSize = sizeof(gnvEFSTable->halnv.tables.regDomains);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.regDomains[0],inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_DEFAULT_LOCATION:
            itemSize = sizeof(gnvEFSTable->halnv.tables.defaultCountryTable);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.defaultCountryTable,inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_TPC_POWER_TABLE:
            itemSize = sizeof(gnvEFSTable->halnv.tables.plutCharacterized);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.plutCharacterized[0],inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_TPC_PDADC_OFFSETS:
            itemSize = sizeof(gnvEFSTable->halnv.tables.plutPdadcOffset);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.plutPdadcOffset[0],inputVoidBuffer,bufferSize);
            }
            break;
         case VNV_RSSI_CHANNEL_OFFSETS:

            itemSize = sizeof(gnvEFSTable->halnv.tables.rssiChanOffsets);

            if(bufferSize != itemSize) {

                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.rssiChanOffsets[0],inputVoidBuffer,bufferSize);
            }
            break;
         case VNV_RF_CAL_VALUES:

            itemSize = sizeof(gnvEFSTable->halnv.tables.rFCalValues);

            if(bufferSize != itemSize) {

                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.rFCalValues,inputVoidBuffer,bufferSize);
            }
            break;
        case VNV_ANTENNA_PATH_LOSS:
            itemSize = sizeof(gnvEFSTable->halnv.tables.antennaPathLoss);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.antennaPathLoss[0],inputVoidBuffer,bufferSize);
            }
            break;

        case VNV_PACKET_TYPE_POWER_LIMITS:
            itemSize = sizeof(gnvEFSTable->halnv.tables.pktTypePwrLimits);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.pktTypePwrLimits[0][0],inputVoidBuffer,bufferSize);
            }
            break;

        case VNV_OFDM_CMD_PWR_OFFSET:
            itemSize = sizeof(gnvEFSTable->halnv.tables.ofdmCmdPwrOffset);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.ofdmCmdPwrOffset,inputVoidBuffer,bufferSize);
            }
            break;

        case VNV_TX_BB_FILTER_MODE:
            itemSize = sizeof(gnvEFSTable->halnv.tables.txbbFilterMode);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.txbbFilterMode,inputVoidBuffer,bufferSize);
            }
            break;
            
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
        case VNV_FREQUENCY_FOR_1_3V_SUPPLY:
            itemSize = sizeof(gnvEFSTable->halnv.tables.freqFor1p3VSupply);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.freqFor1p3VSupply,inputVoidBuffer,bufferSize);
            }
            break;
#endif /* FEATURE_WLAN_NON_INTEGRATED_SOC */

        case VNV_TABLE_VIRTUAL_RATE:
            itemSize = sizeof(gnvEFSTable->halnv.tables.pwrOptimum_virtualRate);
            if(bufferSize != itemSize) {
                VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 ("type = %d buffer size=%d is less than data size=%d\r\n"),type, bufferSize,
                  itemSize);
                status = VOS_STATUS_E_INVAL;
            }
            else {
                memcpy(&gnvEFSTable->halnv.tables.pwrOptimum_virtualRate,inputVoidBuffer,bufferSize);
            }
            break;

        default:
          break;
    }
   if (VOS_STATUS_SUCCESS == status)
   {
      // set NV item to have valid data
      status = vos_nv_setValidity( type, VOS_TRUE );
      if (! VOS_IS_STATUS_SUCCESS(status)) {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, ("vos_nv_setValidity failed!!!\r\n"));
          status = VOS_STATUS_E_FAULT;
      }
      status = wlan_write_to_efs((v_U8_t*)gnvEFSTable,sizeof(nvEFSTable_t));

      if (! VOS_IS_STATUS_SUCCESS(status)) {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, ("vos_nv_write_to_efs failed!!!\r\n"));
          status = VOS_STATUS_E_FAULT;
      }
   }
   return status;
}
#endif
  
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
#ifdef FEATURE_WLAN_INTEGRATED_SOC
        for( i = RF_CHAN_36; i <= RF_CHAN_165; i++ )
        {
            if( regChannels[i].enabled )
            {
                channels20MHz[count].chanId = rfChannels[i].channelNum;
                channels20MHz[count++].pwr  = regChannels[i].pwrLimit;
            }
        }
#endif
        *num20MHzChannelsFound = (tANI_U8)count;
    }

    if( channels40MHz && num40MHzChannelsFound )
    {
        count = 0;
#ifdef FEATURE_WLAN_INTEGRATED_SOC
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
#endif
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
#ifdef FEATURE_WLAN_INTEGRATED_SOC
            for( i = RF_CHAN_36; i <= RF_CHAN_165; i++ )
            {
                p20MhzChannels[count++] = rfChannels[i].channelNum;
            }
#endif
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
   
   VOS_STATUS status;

   status = vos_nv_read( VNV_DEFAULT_LOCATION, tableData, NULL, sizeof(sDefaultCountry) );

   return status;
}

/**------------------------------------------------------------------------
  \brief vos_nv_getBuffer - 
  \param pBuffer  - to return the buffer address
              pSize     - buffer size.
  \return status of the NV read operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_getNVBuffer(v_VOID_t **pNvBuffer,v_SIZE_t *pSize)
{

   /* Send the NV structure and size */
   *pNvBuffer = (v_VOID_t *)(&pnvEFSTable->halnv);
   *pSize = sizeof(sHalNv);

   return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_INTEGRATED_SOC
/**------------------------------------------------------------------------
  \brief vos_nv_setRegDomain - 
  \param clientCtxt  - Client Context, Not used for PRIMA
              regId  - Regulatory Domain ID
  \return status set REG domain operation
  \sa
  -------------------------------------------------------------------------*/
VOS_STATUS vos_nv_setRegDomain(void * clientCtxt, v_REGDOMAIN_t regId)
{
   /* Client Context Argumant not used for PRIMA */
   if(regId >= REGDOMAIN_COUNT)
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
                "vos_nv_getChannelEnabledState, invalid cahnnel %d", rfChannel);
      return NV_CHANNEL_INVALID;
   }

   return regChannels[channelEnum].enabled;
}
#endif /* FEATURE_WLAN_NON_INTEGRATED_SOC */
