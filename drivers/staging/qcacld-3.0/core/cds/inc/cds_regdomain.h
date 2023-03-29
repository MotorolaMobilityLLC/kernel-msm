/*
 * Copyright (c) 2011, 2014-2018, 2020 The Linux Foundation. All rights reserved.
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
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * This module contains the regulatory domain private structure definitions .
 *
 */

#ifndef __CDS_REGDOMAIN_H
#define __CDS_REGDOMAIN_H

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>

#define MIN_TX_PWR_CAP    8
#define MAX_TX_PWR_CAP    24

#define CTRY_DEFAULT          0
#define CTRY_FLAG             0x8000
#define WORLD_ROAMING_FLAG    0x4000
#define WORLD_ROAMING_MASK    0x00F0
#define WORLD_ROAMING_PREFIX  0x0060

enum country_code {
	CTRY_AFGHANISTAN = 4,
	CTRY_ALBANIA = 8,
	CTRY_ALGERIA = 12,
	CTRY_AMERICAN_SAMOA = 16,
	CTRY_ANGUILLA = 660,
	CTRY_ARGENTINA = 32,
	CTRY_ARGENTINA_AP = 5003,
	CTRY_ARMENIA = 51,
	CTRY_ARUBA = 533,
	CTRY_AUSTRALIA = 36,
	CTRY_AUSTRALIA_AP = 5000,
	CTRY_AUSTRIA = 40,
	CTRY_AZERBAIJAN = 31,
	CTRY_BAHAMAS = 44,
	CTRY_BAHRAIN = 48,
	CTRY_BANGLADESH = 50,
	CTRY_BARBADOS = 52,
	CTRY_BELARUS = 112,
	CTRY_BELGIUM = 56,
	CTRY_BELIZE = 84,
	CTRY_BERMUDA = 60,
	CTRY_BHUTAN = 64,
	CTRY_BOLIVIA = 68,
	CTRY_BOSNIA_HERZ = 70,
	CTRY_BRAZIL = 76,
	CTRY_BRUNEI_DARUSSALAM = 96,
	CTRY_BULGARIA = 100,
	CTRY_BURKINA_FASO = 854,
	CTRY_CAMBODIA = 116,
	CTRY_CANADA = 124,
	CTRY_CANADA_AP = 5001,
	CTRY_CAYMAN_ISLANDS = 136,
	CTRY_CENTRAL_AFRICA_REPUBLIC = 140,
	CTRY_CHAD = 148,
	CTRY_CHILE = 152,
	CTRY_CHINA = 156,
	CTRY_CHRISTMAS_ISLAND = 162,
	CTRY_COLOMBIA = 170,
	CTRY_COSTA_RICA = 188,
	CTRY_COTE_DIVOIRE = 384,
	CTRY_CROATIA = 191,
	CTRY_CYPRUS = 196,
	CTRY_CZECH = 203,
	CTRY_DENMARK = 208,
	CTRY_DOMINICA = 212,
	CTRY_DOMINICAN_REPUBLIC = 214,
	CTRY_ECUADOR = 218,
	CTRY_EGYPT = 818,
	CTRY_EL_SALVADOR = 222,
	CTRY_ESTONIA = 233,
	CTRY_ETHIOPIA = 231,
	CTRY_FINLAND = 246,
	CTRY_FRANCE = 250,
	CTRY_FRENCH_GUIANA = 254,
	CTRY_FRENCH_POLYNESIA = 258,
	CTRY_GEORGIA = 268,
	CTRY_GERMANY = 276,
	CTRY_GHANA = 288,
	CTRY_GIBRALTAR = 292,
	CTRY_GREECE = 300,
	CTRY_GREENLAND = 304,
	CTRY_GRENADA = 308,
	CTRY_GUADELOUPE = 312,
	CTRY_GUAM = 316,
	CTRY_GUATEMALA = 320,
	CTRY_GUYANA = 328,
	CTRY_HAITI = 332,
	CTRY_HONDURAS = 340,
	CTRY_HONG_KONG = 344,
	CTRY_HUNGARY = 348,
	CTRY_ICELAND = 352,
	CTRY_INDIA = 356,
	CTRY_INDONESIA = 360,
	CTRY_IRAQ = 368,
	CTRY_IRELAND = 372,
	CTRY_ISRAEL = 376,
	CTRY_ITALY = 380,
	CTRY_JAMAICA = 388,
	CTRY_JORDAN = 400,
	CTRY_KAZAKHSTAN = 398,
	CTRY_KENYA = 404,
	CTRY_KOREA_ROC = 410,
	CTRY_KOREA_ROC_AP = 412,
	CTRY_KUWAIT = 414,
	CTRY_LATVIA = 428,
	CTRY_LEBANON = 422,
	CTRY_LESOTHO = 426,
	CTRY_LIBYA = 434,
	CTRY_LIECHTENSTEIN = 438,
	CTRY_LITHUANIA = 440,
	CTRY_LUXEMBOURG = 442,
	CTRY_MACAU = 446,
	CTRY_MACEDONIA = 807,
	CTRY_MALAWI = 454,
	CTRY_MALAYSIA = 458,
	CTRY_MALDIVES = 462,
	CTRY_MALTA = 470,
	CTRY_MARSHALL_ISLANDS = 584,
	CTRY_MARTINIQUE = 474,
	CTRY_MAURITANIA = 478,
	CTRY_MAURITIUS = 480,
	CTRY_MAYOTTE = 175,
	CTRY_MEXICO = 484,
	CTRY_MICRONESIA = 583,
	CTRY_MOLDOVA = 498,
	CTRY_MONACO = 492,
	CTRY_MONGOLIA = 496,
	CTRY_MONTENEGRO = 499,
	CTRY_MOROCCO = 504,
	CTRY_NAMIBIA = 516,
	CTRY_NEPAL = 524,
	CTRY_NETHERLANDS = 528,
	CTRY_NETHERLANDS_ANTILLES = 530,
	CTRY_NEW_ZEALAND = 554,
	CTRY_NIGERIA = 566,
	CTRY_NORTHERN_MARIANA_ISLANDS = 580,
	CTRY_NICARAGUA = 558,
	CTRY_NORWAY = 578,
	CTRY_OMAN = 512,
	CTRY_PAKISTAN = 586,
	CTRY_PALAU = 585,
	CTRY_PANAMA = 591,
	CTRY_PAPUA_NEW_GUINEA = 598,
	CTRY_PARAGUAY = 600,
	CTRY_PERU = 604,
	CTRY_PHILIPPINES = 608,
	CTRY_POLAND = 616,
	CTRY_PORTUGAL = 620,
	CTRY_PUERTO_RICO = 630,
	CTRY_QATAR = 634,
	CTRY_REUNION = 638,
	CTRY_ROMANIA = 642,
	CTRY_RUSSIA = 643,
	CTRY_RWANDA = 646,
	CTRY_SAINT_BARTHELEMY = 652,
	CTRY_SAINT_KITTS_AND_NEVIS = 659,
	CTRY_SAINT_LUCIA = 662,
	CTRY_SAINT_MARTIN = 663,
	CTRY_SAINT_PIERRE_AND_MIQUELON = 666,
	CTRY_SAINT_VINCENT_AND_THE_GRENADIENS = 670,
	CTRY_SAMOA = 882,
	CTRY_SAUDI_ARABIA = 682,
	CTRY_SENEGAL = 686,
	CTRY_SERBIA = 688,
	CTRY_SINGAPORE = 702,
	CTRY_SLOVAKIA = 703,
	CTRY_SLOVENIA = 705,
	CTRY_SOUTH_AFRICA = 710,
	CTRY_SPAIN = 724,
	CTRY_SURINAME = 740,
	CTRY_SRI_LANKA = 144,
	CTRY_SWEDEN = 752,
	CTRY_SWITZERLAND = 756,
	CTRY_TAIWAN = 158,
	CTRY_TANZANIA = 834,
	CTRY_THAILAND = 764,
	CTRY_TOGO = 768,
	CTRY_TRINIDAD_Y_TOBAGO = 780,
	CTRY_TUNISIA = 788,
	CTRY_TURKEY = 792,
	CTRY_TURKS_AND_CAICOS = 796,
	CTRY_UGANDA = 800,
	CTRY_UKRAINE = 804,
	CTRY_UAE = 784,
	CTRY_UNITED_KINGDOM = 826,
	CTRY_UNITED_STATES = 840,
	CTRY_UNITED_STATES_AP = 841,
	CTRY_UNITED_STATES_AP2 = 843,
	CTRY_UNITED_STATES_PS = 842,
	CTRY_URUGUAY = 858,
	CTRY_UZBEKISTAN = 860,
	CTRY_VANUATU = 548,
	CTRY_VENEZUELA = 862,
	CTRY_VIET_NAM = 704,
	CTRY_VIRGIN_ISLANDS = 850,
	CTRY_WALLIS_AND_FUTUNA = 876,
	CTRY_YEMEN = 887,
	CTRY_ZIMBABWE = 716,
	CTRY_JAPAN9 = 4009,
	CTRY_JAPAN15 = 4015,
	CTRY_JAPAN48 = 4048,
	CTRY_JAPAN55 = 4055,
	CTRY_JAPAN60 = 4060,
	CTRY_XA = 4100,
};

enum reg_domain {
	NO_ENUMRD = 0x00,
	NULL1_WORLD = 0x03,
	NULL1_ETSIB = 0x07,
	NULL1_ETSIC = 0x08,

	FCC1_FCCA = 0x10,
	FCC1_WORLD = 0x11,
	FCC2_FCCA = 0x20,
	FCC2_WORLD = 0x21,
	FCC2_ETSIC = 0x22,
	FCC3_FCCA = 0x3A,
	FCC3_WORLD = 0x3B,
	FCC3_ETSIC = 0x3F,
	FCC4_FCCA = 0x12,
	FCC5_FCCA = 0x13,
	FCC6_FCCA = 0x14,
	FCC7_FCCA = 0x15,
	FCC8_FCCA = 0x16,
	FCC6_WORLD = 0x23,
	FCC9_FCCA = 0x17,
	FCC10_FCCA = 0x18,
	FCC11_WORLD = 0x19,
	FCC13_WORLD = 0xE4,
	FCC14_FCCB = 0xE6,

	ETSI1_WORLD = 0x37,
	ETSI3_ETSIA = 0x32,
	ETSI2_WORLD = 0x35,
	ETSI3_WORLD = 0x36,
	ETSI4_WORLD = 0x30,
	ETSI4_ETSIC = 0x38,
	ETSI5_WORLD = 0x39,
	ETSI6_WORLD = 0x34,
	ETSI_RESERVED = 0x33,
	FRANCE_RES = 0x31,
	ETSI7_WORLD = 0x3C,
	ETSI8_WORLD = 0x3D,
	ETSI9_WORLD = 0x3E,
	ETSI10_WORLD = 0x24,
	ETSI11_WORLD = 0x26,

	APL4_WORLD = 0x42,
	APL3_FCCA = 0x50,
	APL_RESERVED = 0x44,
	APL2_WORLD = 0x45,
	APL2_FCCA = 0x4D,
	APL2_APLC = 0x46,
	APL3_WORLD = 0x47,
	APL2_APLD = 0x49,
	APL1_WORLD = 0x52,
	APL1_FCCA = 0x53,
	APL1_APLA = 0x54,
	APL1_ETSIC = 0x55,
	APL2_ETSIC = 0x56,
	APL5_WORLD = 0x58,
	APL6_WORLD = 0x5B,
	APL7_FCCA = 0x5C,
	APL8_WORLD = 0x5D,
	APL9_WORLD = 0x5E,
	APL10_WORLD = 0x5F,
	APL11_FCCA = 0x4F,
	APL12_WORLD = 0x51,
	APL13_WORLD = 0x5A,
	APL14_WORLD = 0x57,
	APL15_WORLD = 0x59,
	APL16_WORLD = 0x70,
	APL17_ETSID = 0xE0,
	APL20_WORLD = 0xE5,
	APL23_WORLD = 0xE3,

	WOR0_WORLD = 0x60,
	WOR1_WORLD = 0x61,
	WOR2_WORLD = 0x62,
	WOR3_WORLD = 0x63,
	WOR4_FCCA = 0x64,
	WOR5_ETSIC = 0x65,
	WOR01_WORLD = 0x66,
	WOR02_WORLD = 0x67,
	EU1_WORLD = 0x68,
	WOR9_WORLD = 0x69,
	WORA_WORLD = 0x6A,
	WORB_WORLD = 0x6B,
	WORC_WORLD = 0x6C,

	MKK3_MKKB = 0x80,
	MKK3_MKKA2 = 0x81,
	MKK3_MKKC = 0x82,
	MKK4_MKKB = 0x83,
	MKK4_MKKA2 = 0x84,
	MKK4_MKKC = 0x85,
	MKK5_MKKA = 0x99,
	MKK5_FCCA = 0x9A,
	MKK5_MKKB = 0x86,
	MKK5_MKKA2 = 0x87,
	MKK5_MKKC = 0x88,
	MKK3_MKKA = 0xF0,
	MKK3_MKKA1 = 0xF1,
	MKK3_FCCA = 0xF2,
	MKK4_MKKA = 0xF3,
	MKK4_MKKA1 = 0xF4,
	MKK4_FCCA = 0xF5,
	MKK9_MKKA = 0xF6,
	MKK9_FCCA = 0xFC,
	MKK9_MKKA1 = 0xFD,
	MKK9_MKKC = 0xFE,
	MKK9_MKKA2 = 0xFF,
	MKK10_MKKA = 0xF7,
	MKK10_FCCA = 0xD0,
	MKK10_MKKA1 = 0xD1,
	MKK10_MKKC = 0xD2,
	MKK10_MKKA2 = 0xD3,
	MKK11_MKKA = 0xD4,
	MKK11_FCCA = 0xD5,
	MKK11_MKKA1 = 0xD6,
	MKK11_MKKC = 0xD7,
	MKK11_MKKA2 = 0xD8,
	MKK16_MKKC = 0xDF,

	FCC1 = 0x0110,
	FCC2 = 0x0120,
	FCC3 = 0x0160,
	FCC4 = 0x0165,
	FCC5 = 0x0510,
	FCC6 = 0x0610,
	FCC7 = 0x0710,
	FCC8 = 0x0810,
	FCC9 = 0x0910,
	FCC10 = 0x0B10,
	FCC11 = 0x0B20,
	FCC13 = 0x0B60,
	FCC14 = 0x0B70,

	ETSI1 = 0x0130,
	ETSI2 = 0x0230,
	ETSI3 = 0x0330,
	ETSI4 = 0x0430,
	ETSI5 = 0x0530,
	ETSI6 = 0x0630,
	ETSI8 = 0x0830,
	ETSI9 = 0x0930,
	ETSI10 = 0x0D30,
	ETSI11 = 0x0E30,

	APL1 = 0x0150,
	APL2 = 0x0250,
	APL3 = 0x0350,
	APL4 = 0x0450,
	APL5 = 0x0550,
	APL6 = 0x0650,
	APL7 = 0x0750,
	APL8 = 0x0850,
	APL9 = 0x0950,
	APL10 = 0x1050,
	APL11 = 0x1150,
	APL12 = 0x1160,
	APL13 = 0x1170,
	APL14 = 0x1180,
	APL15 = 0x1190,
	APL16 = 0x1200,
	APL17 = 0x1210,
	APL23 = 0x1280,
	APL20 = 0x1250,

	NULL1 = 0x0198,
	MKK3 = 0x0340,
	MKK5 = 0x0540,
	MKK11 = 0x1140,
	MKK16 =  0x1640,

	WORLD = 0x0199,
	FCCA = 0x0A10,
	FCCB = 0x0B90,
	MKKA = 0x0A40,
	MKKC = 0x0A50,
	ETSIC = 0x0C30,

};

/**
 * struct reg_dmn_pair: regulatory domain pair
 * @reg_dmn_pair: reg domain pair
 * @reg_dmn_5ghz: 5G reg domain
 * @reg_dmn_2ghz: 2G reg domain
 * @single_cc: country with this reg domain
 */
struct reg_dmn_pair {
	uint16_t reg_dmn_pair;
	uint16_t reg_dmn_5ghz;
	uint16_t reg_dmn_2ghz;
	uint16_t single_cc;
};

/**
 * struct country_code_to_reg_dmn: country code to reg domain mapping
 * @country_code: country code
 * @reg_dmn_pair: regulatory domain pair
 * @alpha2: country alpha2
 * @name: country name
 */
struct country_code_to_reg_dmn {
	uint16_t country_code;
	uint16_t reg_dmn_pair;
	const char *alpha2;
	const char *name;
};

/**
 * struct reg_dmn: regulatory domain structure
 * @reg_dmn: regulatory domain
 * @conformance_test_limit:  CTL limit
 */
struct reg_dmn {
	uint16_t reg_dmn;
	uint8_t conformance_test_limit;
};

/**
 * struct reg_dmn_tables: reg domain table
 * @reg_dmn_pairs: list of reg domain pairs
 * @all_countries: list of countries
 * @reg_dmns: list of reg domains
 * @reg_dmn_pairs_cnt: count of reg domain pairs
 * @all_countries_cnt: count of countries
 * @reg_dmns_cnt: count of reg domains
 */
struct reg_dmn_tables {
	const struct reg_dmn_pair *reg_dmn_pairs;
	const struct country_code_to_reg_dmn *all_countries;
	const struct reg_dmn *reg_dmns;
	uint16_t reg_dmn_pairs_cnt;
	uint16_t all_countries_cnt;
	uint16_t reg_dmns_cnt;
};

int32_t cds_fill_some_regulatory_info(struct regulatory *reg);
int32_t cds_get_country_from_alpha2(uint8_t *alpha2);
void cds_fill_and_send_ctl_to_fw(struct regulatory *reg);
/**
 * cds_is_etsi_europe_country - check ETSI Europe country or not
 * @country: country string with two Characters
 *
 * Return: true if country in ETSI Europe country list
 */
bool cds_is_etsi_europe_country(uint8_t *country);
#endif /* __CDS_REGDOMAIN_H */
