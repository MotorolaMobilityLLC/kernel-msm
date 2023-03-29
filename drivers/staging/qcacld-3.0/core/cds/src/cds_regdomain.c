/*
 * Copyright (c) 2011,2013-2019 The Linux Foundation. All rights reserved.
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
 */

#include "qdf_types.h"
#include "wma.h"
#include "cds_regdomain.h"


static const struct reg_dmn_pair g_reg_dmn_pairs[] = {
	{NO_ENUMRD, FCC8, FCCA, CTRY_DEFAULT},
	{NULL1_WORLD, NULL1, WORLD, CTRY_DEFAULT},
	{FCC1_FCCA, FCC1, FCCA, CTRY_DEFAULT},
	{FCC1_WORLD, FCC1, WORLD, CTRY_DEFAULT},
	{FCC2_WORLD, FCC2, WORLD, CTRY_DEFAULT},
	{FCC2_ETSIC, FCC2, ETSIC, CTRY_DEFAULT},
	{FCC2_FCCA, FCC2, FCCA, CTRY_DEFAULT},
	{FCC3_FCCA, FCC3, FCCA, CTRY_DEFAULT},
	{FCC3_WORLD, FCC3, WORLD, CTRY_DEFAULT},
	{FCC3_ETSIC, FCC3, ETSIC, CTRY_DEFAULT},
	{FCC4_FCCA, FCC4, FCCA, CTRY_DEFAULT},
	{FCC5_FCCA, FCC5, FCCA, CTRY_DEFAULT},
	{FCC6_FCCA, FCC6, FCCA, CTRY_DEFAULT},
	{FCC7_FCCA, FCC7, FCCA, CTRY_DEFAULT},
	{FCC8_FCCA, FCC8, FCCA, CTRY_DEFAULT},
	{FCC6_WORLD, FCC6, WORLD, CTRY_DEFAULT},
	{FCC9_FCCA, FCC9, FCCA, CTRY_DEFAULT},
	{FCC10_FCCA, FCC10, FCCA, CTRY_DEFAULT},
	{FCC11_WORLD, FCC11, WORLD, CTRY_DEFAULT},
	{FCC13_WORLD, FCC13, WORLD, CTRY_DEFAULT},
	{FCC14_FCCB, FCC14, FCCB, CTRY_DEFAULT},
	{ETSI1_WORLD, ETSI1, WORLD, CTRY_DEFAULT},
	{ETSI3_WORLD, ETSI3, WORLD, CTRY_DEFAULT},
	{ETSI4_WORLD, ETSI4, WORLD, CTRY_DEFAULT},
	{ETSI7_WORLD, ETSI4, WORLD, CTRY_DEFAULT},
	{ETSI8_WORLD, ETSI8, WORLD, CTRY_DEFAULT},
	{ETSI9_WORLD, ETSI9, WORLD, CTRY_DEFAULT},
	{APL4_WORLD, APL4, WORLD, CTRY_DEFAULT},
	{APL2_WORLD, APL2, WORLD, CTRY_DEFAULT},
	{APL2_FCCA, APL2, FCCA, CTRY_DEFAULT},
	{APL2_ETSIC, APL2, ETSIC, CTRY_DEFAULT},
	{APL1_WORLD, APL1, WORLD, CTRY_DEFAULT},
	{APL1_ETSIC, APL1, ETSIC, CTRY_DEFAULT},
	{APL6_WORLD, APL6, WORLD, CTRY_DEFAULT},
	{APL7_FCCA, APL7, FCCA, CTRY_DEFAULT},
	{APL8_WORLD, APL8, WORLD, CTRY_DEFAULT},
	{APL9_WORLD, APL9, WORLD, CTRY_DEFAULT},
	{APL10_WORLD, APL10, WORLD, CTRY_DEFAULT},
	{APL12_WORLD, APL12, WORLD, CTRY_DEFAULT},
	{APL13_WORLD, APL13, WORLD, CTRY_DEFAULT},
	{APL14_WORLD, APL14, WORLD, CTRY_DEFAULT},
	{APL15_WORLD, APL15, WORLD, CTRY_DEFAULT},
	{APL16_WORLD, APL16, WORLD, CTRY_DEFAULT},
	{APL17_ETSID, APL17, WORLD, CTRY_DEFAULT},
	{APL20_WORLD, APL20, WORLD, CTRY_DEFAULT},
	{APL23_WORLD, APL23, WORLD, CTRY_DEFAULT},
	{WOR0_WORLD, WOR0_WORLD, WOR0_WORLD, CTRY_DEFAULT},
	{WOR1_WORLD, WOR1_WORLD, WOR1_WORLD, CTRY_DEFAULT},
	{WOR2_WORLD, WOR2_WORLD, WOR2_WORLD, CTRY_DEFAULT},
	{WOR3_WORLD, WOR3_WORLD, WOR3_WORLD, CTRY_DEFAULT},
	{WOR4_FCCA, WOR4_FCCA, WOR4_FCCA, CTRY_DEFAULT},
	{WOR5_ETSIC, WOR5_ETSIC, WOR5_ETSIC, CTRY_DEFAULT},
	{WOR01_WORLD, WOR01_WORLD, WOR01_WORLD, CTRY_DEFAULT},
	{WOR02_WORLD, WOR02_WORLD, WOR02_WORLD, CTRY_DEFAULT},
	{EU1_WORLD, EU1_WORLD, EU1_WORLD, CTRY_DEFAULT},
	{WOR9_WORLD, WOR9_WORLD, WOR9_WORLD, CTRY_DEFAULT},
	{WORA_WORLD, WORA_WORLD, WORA_WORLD, CTRY_DEFAULT},
	{WORB_WORLD, WORB_WORLD, WORB_WORLD, CTRY_DEFAULT},
	{WORC_WORLD, WORC_WORLD, WORC_WORLD, CTRY_DEFAULT},
	{MKK5_MKKC, MKK5, MKKC, CTRY_JAPAN15},
	{MKK5_MKKA2, MKK5, MKKA, CTRY_DEFAULT},
};

static const struct country_code_to_reg_dmn g_all_countries[] = {
	{CTRY_AFGHANISTAN, ETSI1_WORLD, "AF", "AFGHANISTAN"},
	{CTRY_ALBANIA, ETSI1_WORLD, "AL", "ALBANIA"},
	{CTRY_ALGERIA, APL13_WORLD, "DZ", "ALGERIA"},
	{CTRY_AMERICAN_SAMOA, FCC3_FCCA, "AS", "AMERICAN SAMOA"},
	{CTRY_ANGUILLA, ETSI1_WORLD, "AI", "ANGUILLA"},
	{CTRY_ARGENTINA, APL17_ETSID, "AR", "ARGENTINA"},
	{CTRY_ARMENIA, ETSI4_WORLD, "AM", "ARMENIA"},
	{CTRY_ARUBA, ETSI1_WORLD, "AW", "ARUBA"},
	{CTRY_AUSTRALIA, FCC6_WORLD, "AU", "AUSTRALIA"},
	{CTRY_AUSTRIA, ETSI1_WORLD, "AT", "AUSTRIA"},
	{CTRY_AZERBAIJAN, ETSI4_WORLD, "AZ", "AZERBAIJAN"},
	{CTRY_BAHAMAS, FCC3_WORLD, "BS", "BAHAMAS"},
	{CTRY_BAHRAIN, APL15_WORLD, "BH", "BAHRAIN"},
	{CTRY_BANGLADESH, APL1_WORLD, "BD", "BANGLADESH"},
	{CTRY_BARBADOS, FCC2_WORLD, "BB", "BARBADOS"},
	{CTRY_BELARUS, ETSI1_WORLD, "BY", "BELARUS"},
	{CTRY_BELGIUM, ETSI1_WORLD, "BE", "BELGIUM"},
	{CTRY_BELIZE, ETSI8_WORLD, "BZ", "BELIZE"},
	{CTRY_BERMUDA, FCC3_FCCA, "BM", "BERMUDA"},
	{CTRY_BHUTAN, ETSI1_WORLD, "BT", "BHUTAN"},
	{CTRY_BOLIVIA, APL8_WORLD, "BO", "BOLIVIA"},
	{CTRY_BOSNIA_HERZ, ETSI1_WORLD, "BA", "BOSNIA AND HERZEGOVINA"},
	{CTRY_BRAZIL, FCC3_ETSIC, "BR", "BRAZIL"},
	{CTRY_BRUNEI_DARUSSALAM, APL6_WORLD, "BN", "BRUNEI DARUSSALAM"},
	{CTRY_BULGARIA, ETSI1_WORLD, "BG", "BULGARIA"},
	{CTRY_BURKINA_FASO, FCC3_WORLD, "BF", "BURKINA-FASO"},
	{CTRY_CAMBODIA, ETSI1_WORLD, "KH", "CAMBODIA"},
	{CTRY_CANADA, FCC3_FCCA, "CA", "CANADA"},
	{CTRY_CAYMAN_ISLANDS, FCC3_WORLD, "KY", "CAYMAN ISLANDS"},
	{CTRY_CENTRAL_AFRICA_REPUBLIC, FCC3_WORLD, "CF", "AFRICA REPUBLIC"},
	{CTRY_CHAD, ETSI1_WORLD, "TD", "CHAD"},
	{CTRY_CHILE, APL23_WORLD, "CL", "CHILE"},
	{CTRY_CHINA, APL14_WORLD, "CN", "CHINA"},
	{CTRY_CHRISTMAS_ISLAND, FCC3_WORLD, "CX", "CHRISTMAS ISLAND"},
	{CTRY_COLOMBIA, FCC3_WORLD, "CO", "COLOMBIA"},
	{CTRY_COSTA_RICA, FCC3_WORLD, "CR", "COSTA RICA"},
	{CTRY_COTE_DIVOIRE, FCC3_WORLD, "CI", "COTE DIVOIRE"},
	{CTRY_CROATIA, ETSI1_WORLD, "HR", "CROATIA"},
	{CTRY_CYPRUS, ETSI1_WORLD, "CY", "CYPRUS"},
	{CTRY_CZECH, ETSI1_WORLD, "CZ", "CZECH REPUBLIC"},
	{CTRY_DENMARK, ETSI1_WORLD, "DK", "DENMARK"},
	{CTRY_DOMINICA, FCC2_FCCA, "DM", "DOMINICA"},
	{CTRY_DOMINICAN_REPUBLIC, FCC2_FCCA, "DO", "DOMINICAN REPUBLIC"},
	{CTRY_ECUADOR, FCC3_WORLD, "EC", "ECUADOR"},
	{CTRY_EGYPT, ETSI3_WORLD, "EG", "EGYPT"},
	{CTRY_EL_SALVADOR, FCC2_WORLD, "SV", "EL SALVADOR"},
	{CTRY_ESTONIA, ETSI1_WORLD, "EE", "ESTONIA"},
	{CTRY_ETHIOPIA, ETSI1_WORLD, "ET", "ETHIOPIA"},
	{CTRY_FINLAND, ETSI1_WORLD, "FI", "FINLAND"},
	{CTRY_FRANCE, ETSI1_WORLD, "FR", "FRANCE"},
	{CTRY_FRENCH_GUIANA, ETSI1_WORLD, "GF", "FRENCH GUIANA"},
	{CTRY_FRENCH_POLYNESIA, ETSI1_WORLD, "PF", "FRENCH POLYNESIA"},
	{CTRY_GEORGIA, ETSI4_WORLD, "GE", "GEORGIA"},
	{CTRY_GERMANY, ETSI1_WORLD, "DE", "GERMANY"},
	{CTRY_GHANA, FCC3_WORLD, "GH", "GHANA"},
	{CTRY_GIBRALTAR, ETSI1_WORLD, "GI", "GIBRALTAR"},
	{CTRY_GREECE, ETSI1_WORLD, "GR", "GREECE"},
	{CTRY_GREENLAND, ETSI1_WORLD, "GL", "GREENLAND"},
	{CTRY_GRENADA, FCC3_FCCA, "GD", "GRENADA"},
	{CTRY_GUADELOUPE, ETSI1_WORLD, "GP", "GUADELOUPE"},
	{CTRY_GUAM, FCC3_FCCA, "GU", "GUAM"},
	{CTRY_GUATEMALA, ETSI1_WORLD, "GT", "GUATEMALA"},
	{CTRY_GUYANA, APL1_ETSIC, "GY", "GUYANA"},
	{CTRY_HAITI, FCC3_FCCA, "HT", "HAITI"},
	{CTRY_HONDURAS, FCC13_WORLD, "HN", "HONDURAS"},
	{CTRY_HONG_KONG, FCC3_WORLD, "HK", "HONG KONG"},
	{CTRY_HUNGARY, ETSI1_WORLD, "HU", "HUNGARY"},
	{CTRY_ICELAND, ETSI1_WORLD, "IS", "ICELAND"},
	{CTRY_INDIA, APL15_WORLD, "IN", "INDIA"},
	{CTRY_INDONESIA, APL2_ETSIC, "ID", "INDONESIA"},
	{CTRY_IRAQ, ETSI1_WORLD, "IQ", "IRAQ"},
	{CTRY_IRELAND, ETSI1_WORLD, "IE", "IRELAND"},
	{CTRY_ISRAEL, ETSI3_WORLD, "IL", "ISRAEL"},
	{CTRY_ITALY, ETSI1_WORLD, "IT", "ITALY"},
	{CTRY_JAMAICA, FCC13_WORLD, "JM", "JAMAICA"},
	{CTRY_JORDAN, APL4_WORLD, "JO", "JORDAN"},
	{CTRY_KAZAKHSTAN, NULL1_WORLD, "KZ", "KAZAKHSTAN"},
	{CTRY_KENYA, APL12_WORLD, "KE", "KENYA"},
	{CTRY_KOREA_ROC, APL9_WORLD, "KR", "KOREA REPUBLIC"},
	{CTRY_KUWAIT, ETSI3_WORLD, "KW", "KUWAIT"},
	{CTRY_LATVIA, ETSI1_WORLD, "LV", "LATVIA"},
	{CTRY_LEBANON, FCC3_WORLD, "LB", "LEBANON"},
	{CTRY_LESOTHO, ETSI1_WORLD, "LS", "LESOTHO"},
	{CTRY_LIECHTENSTEIN, ETSI1_WORLD, "LI", "LIECHTENSTEIN"},
	{CTRY_LITHUANIA, ETSI1_WORLD, "LT", "LITHUANIA"},
	{CTRY_LUXEMBOURG, ETSI1_WORLD, "LU", "LUXEMBOURG"},
	{CTRY_MACAU, FCC3_WORLD, "MO", "MACAU SAR"},
	{CTRY_MACEDONIA, ETSI1_WORLD, "MK", "MACEDONIA, FYRO"},
	{CTRY_MALAWI, ETSI1_WORLD, "MW", "MALAWI"},
	{CTRY_MALAYSIA, FCC11_WORLD, "MY", "MALAYSIA"},
	{CTRY_MALDIVES, APL6_WORLD, "MV", "MALDIVES"},
	{CTRY_MALTA, ETSI1_WORLD, "MT", "MALTA"},
	{CTRY_MARSHALL_ISLANDS, FCC3_FCCA, "MH", "MARSHALL ISLANDS"},
	{CTRY_MARTINIQUE, ETSI1_WORLD, "MQ", "MARTINIQUE"},
	{CTRY_MAURITANIA, ETSI1_WORLD, "MR", "MAURITANA"},
	{CTRY_MAURITIUS, FCC3_WORLD, "MU", "MAURITIUS"},
	{CTRY_MAYOTTE, ETSI1_WORLD, "YT", "MAYOTTE"},
	{CTRY_MEXICO, FCC3_ETSIC, "MX", "MEXICO"},
	{CTRY_MICRONESIA, FCC3_FCCA, "FM", "MICRONESIA"},
	{CTRY_MOLDOVA, ETSI1_WORLD, "MD", "MOLDOVA"},
	{CTRY_MONACO, ETSI1_WORLD, "MC", "MONACO"},
	{CTRY_MONGOLIA, FCC3_WORLD, "MN", "MONGOLIA"},
	{CTRY_MONTENEGRO, ETSI1_WORLD, "ME", "MONTENEGRO"},
	{CTRY_MOROCCO, ETSI3_WORLD, "MA", "MOROCCO"},
	{CTRY_NAMIBIA, APL20_WORLD, "NA", "NAMIBIA"},
	{CTRY_NEPAL, APL23_WORLD, "NP", "NEPAL"},
	{CTRY_NETHERLANDS, ETSI1_WORLD, "NL", "NETHERLANDS"},
	{CTRY_NETHERLANDS_ANTILLES, ETSI1_WORLD, "AN", "NETHERLANDS ANTILLES"},
	{CTRY_NEW_ZEALAND, FCC3_ETSIC, "NZ", "NEW ZEALAND"},
	{CTRY_NIGERIA, APL8_WORLD, "NG", "NIGERIA"},
	{CTRY_NORTHERN_MARIANA_ISLANDS, FCC3_FCCA, "MP", "MARIANA ISLANDS"},
	{CTRY_NICARAGUA, FCC3_FCCA, "NI", "NICARAGUA"},
	{CTRY_NORWAY, ETSI1_WORLD, "NO", "NORWAY"},
	{CTRY_OMAN, ETSI1_WORLD, "OM", "OMAN"},
	{CTRY_PAKISTAN, APL1_ETSIC, "PK", "PAKISTAN"},
	{CTRY_PALAU, FCC3_FCCA, "PW", "PALAU"},
	{CTRY_PANAMA, FCC14_FCCB, "PA", "PANAMA"},
	{CTRY_PAPUA_NEW_GUINEA, FCC3_WORLD, "PG", "PAPUA NEW GUINEA"},
	{CTRY_PARAGUAY, FCC3_WORLD, "PY", "PARAGUAY"},
	{CTRY_PERU, FCC3_WORLD, "PE", "PERU"},
	{CTRY_PHILIPPINES, FCC3_WORLD, "PH", "PHILIPPINES"},
	{CTRY_POLAND, ETSI1_WORLD, "PL", "POLAND"},
	{CTRY_PORTUGAL, ETSI1_WORLD, "PT", "PORTUGAL"},
	{CTRY_PUERTO_RICO, FCC3_FCCA, "PR", "PUERTO RICO"},
	{CTRY_QATAR, APL1_WORLD, "QA", "QATAR"},
	{CTRY_REUNION, ETSI1_WORLD, "RE", "REUNION"},
	{CTRY_ROMANIA, ETSI1_WORLD, "RO", "ROMANIA"},
	{CTRY_RUSSIA, ETSI8_WORLD, "RU", "RUSSIA"},
	{CTRY_RWANDA, FCC3_WORLD, "RW", "RWANDA"},
	{CTRY_SAINT_BARTHELEMY, ETSI1_WORLD, "BL", "SAINT BARTHELEMY"},
	{CTRY_SAINT_KITTS_AND_NEVIS, APL10_WORLD, "KN", "SAINT KITTS"},
	{CTRY_SAINT_LUCIA, APL10_WORLD, "LC", "SAINT LUCIA"},
	{CTRY_SAINT_MARTIN, ETSI1_WORLD, "MF", "SAINT MARTIN"},
	{CTRY_SAINT_PIERRE_AND_MIQUELON, ETSI1_WORLD, "PM", "SAINT PIERRE"},
	{CTRY_SAINT_VINCENT_AND_THE_GRENADIENS, ETSI1_WORLD, "VC", "VINCENT"},
	{CTRY_SAMOA, ETSI1_WORLD, "WS", "SAMOA"},
	{CTRY_SAUDI_ARABIA, ETSI1_WORLD, "SA", "SAUDI ARABIA"},
	{CTRY_SENEGAL, FCC13_WORLD, "SN", "SENEGAL"},
	{CTRY_SERBIA, ETSI1_WORLD, "RS", "REPUBLIC OF SERBIA"},
	{CTRY_SINGAPORE, FCC3_WORLD, "SG", "SINGAPORE"},
	{CTRY_SLOVAKIA, ETSI1_WORLD, "SK", "SLOVAKIA"},
	{CTRY_SLOVENIA, ETSI1_WORLD, "SI", "SLOVENIA"},
	{CTRY_SOUTH_AFRICA, FCC3_WORLD, "ZA", "SOUTH AFRICA"},
	{CTRY_SPAIN, ETSI1_WORLD, "ES", "SPAIN"},
	{CTRY_SURINAME, ETSI1_WORLD, "SR", "SURINAME"},
	{CTRY_SRI_LANKA, FCC3_WORLD, "LK", "SRI LANKA"},
	{CTRY_SWEDEN, ETSI1_WORLD, "SE", "SWEDEN"},
	{CTRY_SWITZERLAND, ETSI1_WORLD, "CH", "SWITZERLAND"},
	{CTRY_TAIWAN, FCC3_FCCA, "TW", "TAIWAN"},
	{CTRY_TANZANIA, APL1_WORLD, "TZ", "TANZANIA"},
	{CTRY_THAILAND, FCC3_WORLD, "TH", "THAILAND"},
	{CTRY_TOGO, ETSI1_WORLD, "TG", "TOGO"},
	{CTRY_TRINIDAD_Y_TOBAGO, FCC3_WORLD, "TT", "TRINIDAD AND TOBAGO"},
	{CTRY_TUNISIA, ETSI3_WORLD, "TN", "TUNISIA"},
	{CTRY_TURKEY, ETSI1_WORLD, "TR", "TURKEY"},
	{CTRY_TURKS_AND_CAICOS, FCC3_WORLD, "TC" "TURKS AND CAICOS"},
	{CTRY_UGANDA, FCC3_WORLD, "UG", "UGANDA"},
	{CTRY_UKRAINE, ETSI9_WORLD, "UA", "UKRAINE"},
	{CTRY_UAE, FCC3_WORLD, "AE", "UNITED ARAB EMIRATES"},
	{CTRY_UNITED_KINGDOM, ETSI1_WORLD, "GB", "UNITED KINGDOM"},
	{CTRY_UNITED_STATES, FCC8_FCCA, "US", "UNITED STATES"},
	{CTRY_URUGUAY, FCC2_WORLD, "UY", "URUGUAY"},
	{CTRY_UZBEKISTAN, ETSI3_WORLD, "UZ", "UZBEKISTAN"},
	{CTRY_VANUATU, FCC3_WORLD, "VU", "VANUATU"},
	{CTRY_VENEZUELA, FCC2_ETSIC, "VE", "VENEZUELA"},
	{CTRY_VIET_NAM, FCC3_WORLD, "VN", "VIETNAM"},
	{CTRY_VIRGIN_ISLANDS, FCC3_FCCA, "VI", "VIRGIN ISLANDS"},
	{CTRY_WALLIS_AND_FUTUNA, ETSI1_WORLD, "WF" "WALLIS"},
	{CTRY_YEMEN, NULL1_WORLD, "YE", "YEMEN"},
	{CTRY_ZIMBABWE, ETSI1_WORLD, "ZW", "ZIMBABWE"},
	{CTRY_JAPAN15, MKK5_MKKC, "JP", "JAPAN"},
	{CTRY_XA, MKK5_MKKA2, "XA", "JAPAN PASSIVE"}
};

static const struct reg_dmn g_reg_dmns[] = {
	{FCC1, CTL_FCC},
	{FCC2, CTL_FCC},
	{FCC3, CTL_FCC},
	{FCC4, CTL_FCC},
	{FCC5, CTL_FCC},
	{FCC6, CTL_FCC},
	{FCC7, CTL_FCC},
	{FCC8, CTL_FCC},
	{FCC9, CTL_FCC},
	{FCC10, CTL_FCC},
	{FCC11, CTL_FCC},
	{FCC13, CTL_FCC},
	{FCC14, CTL_FCC},
	{ETSI1, CTL_ETSI},
	{ETSI2, CTL_ETSI},
	{ETSI3, CTL_ETSI},
	{ETSI4, CTL_ETSI},
	{ETSI5, CTL_ETSI},
	{ETSI6, CTL_ETSI},
	{ETSI8, CTL_ETSI},
	{ETSI9, CTL_ETSI},
	{ETSI10, CTL_ETSI},
	{ETSI11, CTL_ETSI},
	{APL1, CTL_ETSI},
	{APL2, CTL_ETSI},
	{APL3, CTL_ETSI},
	{APL4, CTL_ETSI},
	{APL5, CTL_ETSI},
	{APL6, CTL_ETSI},
	{APL7, CTL_ETSI},
	{APL8, CTL_ETSI},
	{APL9, CTL_ETSI},
	{APL10, CTL_ETSI},
	{APL11, CTL_ETSI},
	{APL12, CTL_ETSI},
	{APL13, CTL_ETSI},
	{APL14, CTL_FCC},
	{APL15, CTL_FCC},
	{APL16, CTL_FCC},
	{APL17, CTL_FCC},
	{APL20, CTL_ETSI},
	{APL23, CTL_ETSI},
	{NULL1, CTL_NONE},
	{MKK3, CTL_MKK},
	{MKK5, CTL_MKK},
	{MKK11, CTL_MKK},
	{WORLD, CTL_ETSI},
	{FCCA, CTL_FCC},
	{MKKA, CTL_MKK},
	{MKKC, CTL_MKK},
	{ETSIC, CTL_ETSI},
	{WOR0_WORLD, CTL_NONE},
	{WOR1_WORLD, CTL_NONE},
	{WOR2_WORLD, CTL_NONE},
	{WOR3_WORLD, CTL_NONE},
	{WOR4_FCCA, CTL_NONE},
	{WOR5_ETSIC, CTL_NONE},
	{WOR01_WORLD, CTL_NONE},
	{WOR02_WORLD, CTL_NONE},
	{EU1_WORLD, CTL_NONE},
	{WOR9_WORLD, CTL_NONE},
	{WORA_WORLD, CTL_NONE},
	{WORB_WORLD, CTL_NONE},
	{WORC_WORLD, CTL_NONE},
};


struct reg_dmn_tables g_reg_dmn_tbl = {
	g_reg_dmn_pairs,
	g_all_countries,
	g_reg_dmns,
	QDF_ARRAY_SIZE(g_reg_dmn_pairs),
	QDF_ARRAY_SIZE(g_all_countries),
	QDF_ARRAY_SIZE(g_reg_dmns),
};

/*
 *  ETSI is updating EN 301 893, which specifies 5 GHz channel access
 *  in Europe
 */
static const char etsi_europe_country[][2] = {
	{'A', 'T'},
	{'B', 'E'},
	{'B', 'G'},
	{'C', 'Z'},
	{'D', 'K'},
	{'E', 'E'},
	{'F', 'R'},

	{'D', 'E'},
	{'I', 'S'},
	{'I', 'E'},
	{'I', 'T'},
	{'E', 'L'},
	{'E', 'S'},
	{'C', 'Y'},

	{'L', 'V'},
	{'L', 'I'},
	{'L', 'T'},
	{'L', 'U'},
	{'H', 'U'},
	{'M', 'T'},
	{'N', 'L'},

	{'N', 'O'},
	{'P', 'L'},
	{'P', 'T'},
	{'R', 'O'},
	{'S', 'I'},
	{'S', 'K'},
	{'T', 'R'},

	{'F', 'I'},
	{'S', 'E'},
	{'C', 'H'},
	{'U', 'K'},
	{'H', 'R'},
};

bool cds_is_etsi_europe_country(uint8_t *country)
{
	int32_t i;

	for (i = 0; i < QDF_ARRAY_SIZE(etsi_europe_country); i++) {
		if (country[0] == etsi_europe_country[i][0] &&
		    country[1] == etsi_europe_country[i][1])
			return true;
	}

	return false;
}

/**
 * get_bdf_reg_dmn() - get regulatory domain from BDF
 * @reg_dmn: BDF regulatory domain
 *
 * Return: regulatory domain
 */
static uint16_t get_bdf_reg_dmn(uint16_t reg_dmn)
{
	return reg_dmn & ~WORLD_ROAMING_FLAG;
}

/**
 * is_reg_dmn_valid() - is regulatory domain valid
 * @reg_dmn: regulatory domain
 *
 * Return: true or false
 */
static bool is_reg_dmn_valid(uint16_t reg_dmn)
{
	int32_t i;

	if (reg_dmn & CTRY_FLAG) {
		uint16_t cc = reg_dmn & ~CTRY_FLAG;

		for (i = 0; i < g_reg_dmn_tbl.all_countries_cnt; i++)
			if (g_reg_dmn_tbl.all_countries[i].country_code == cc)
				return true;
	} else {
		for (i = 0; i < g_reg_dmn_tbl.reg_dmn_pairs_cnt; i++)
			if (g_reg_dmn_tbl.reg_dmn_pairs[i].reg_dmn_pair
			    == reg_dmn)
				return true;
	}

	cds_err("invalid regulatory domain/country code 0x%x", reg_dmn);

	return false;
}

/**
 * find_country() - find country data
 * @country_code: country code
 *
 * Return: country code data pointer
 */
static const struct country_code_to_reg_dmn *find_country(uint16_t country_code)
{
	int32_t i;

	for (i = 0; i < g_reg_dmn_tbl.all_countries_cnt; i++) {
		if (g_reg_dmn_tbl.all_countries[i].country_code == country_code)
			return &g_reg_dmn_tbl.all_countries[i];
	}

	return NULL;
}

/**
 * cds_get_country_from_alpha2() - get country from alpha2
 * @alpha2: country code alpha2
 *
 * Return: country code
 */
int32_t cds_get_country_from_alpha2(uint8_t *alpha2)
{
	int32_t i;

	for (i = 0; i < g_reg_dmn_tbl.all_countries_cnt; i++) {
		if (g_reg_dmn_tbl.all_countries[i].alpha2[0] == alpha2[0] &&
		    g_reg_dmn_tbl.all_countries[i].alpha2[1] == alpha2[1])
			return g_reg_dmn_tbl.all_countries[i].country_code;
	}

	return CTRY_DEFAULT;
}

/**
 * reg_dmn_get_default_country() - get default country for regulatory domain
 * @reg_dmn: regulatory domain
 *
 * Return: default country
 */
static uint16_t reg_dmn_get_default_country(uint16_t reg_dmn)
{
	int32_t i;
	const struct country_code_to_reg_dmn *country = NULL;
	uint16_t cc = reg_dmn & ~CTRY_FLAG;

	if (reg_dmn & CTRY_FLAG) {
		country = find_country(cc);
		if (country)
			return cc;
	}

	for (i = 0; i < g_reg_dmn_tbl.reg_dmn_pairs_cnt; i++) {
		if (g_reg_dmn_tbl.reg_dmn_pairs[i].reg_dmn_pair == reg_dmn) {
			if (g_reg_dmn_tbl.reg_dmn_pairs[i].single_cc != 0)
				return g_reg_dmn_tbl.reg_dmn_pairs[i].single_cc;
			else
				i = g_reg_dmn_tbl.reg_dmn_pairs_cnt;
		}
	}

	return CTRY_DEFAULT;
}

/**
 * get_reg_dmn_pair() - get regulatory domain pair pointer
 * @reg_dmn: regulatory domain
 *
 * Return: pointer to regulatory domain pair data
 */
static const struct reg_dmn_pair *get_reg_dmn_pair(uint16_t reg_dmn)
{
	int32_t i;

	for (i = 0; i < g_reg_dmn_tbl.reg_dmn_pairs_cnt; i++) {
		if (g_reg_dmn_tbl.reg_dmn_pairs[i].reg_dmn_pair == reg_dmn)
			return &g_reg_dmn_tbl.reg_dmn_pairs[i];
	}

	return NULL;
}

/**
 * get_reg_dmn() - get regulatory domain pointer
 * @reg_dmn: regulatory domain
 *
 * Return: pointer to regulatory domain data
 */
static const struct reg_dmn *get_reg_dmn(uint16_t reg_dmn)
{
	int32_t i;

	for (i = 0; i < g_reg_dmn_tbl.reg_dmns_cnt; i++) {
		if (g_reg_dmn_tbl.reg_dmns[i].reg_dmn == reg_dmn)
			return &g_reg_dmn_tbl.reg_dmns[i];
	}

	return NULL;
}

/**
 * get_country_from_rd() - get country from regulatory domain
 * @reg_dmn: regulatory domain
 *
 * Return: country code enum
 */
static const struct country_code_to_reg_dmn *get_country_from_rd(
	uint16_t reg_dmn)
{
	int32_t i;

	for (i = 0; i < g_reg_dmn_tbl.all_countries_cnt; i++) {
		if (g_reg_dmn_tbl.all_countries[i].reg_dmn_pair == reg_dmn)
			return &g_reg_dmn_tbl.all_countries[i];
	}

	return NULL;
}

/**
 * reg_dmn_sanitize() - sanitize regulatory domain
 * @reg: regulatory data structure
 *
 * Return: none
 */
static void reg_dmn_sanitize(struct regulatory *reg)
{
	if (reg->reg_domain != CTRY_FLAG)
		return;

	reg->reg_domain = WOR0_WORLD;
}

/**
 * cds_fill_some_regulatory_info() - fill regulatory information
 * @reg: regulatory data structure
 *
 * Return: error code
 */
int32_t cds_fill_some_regulatory_info(struct regulatory *reg)
{
	uint16_t country_code;
	uint16_t reg_dmn, rd;
	const struct country_code_to_reg_dmn *country = NULL;

	reg_dmn_sanitize(reg);
	rd = reg->reg_domain;

	if (!is_reg_dmn_valid(rd))
		return -EINVAL;

	reg_dmn = get_bdf_reg_dmn(rd);

	country_code = reg_dmn_get_default_country(reg_dmn);
	if (country_code == CTRY_DEFAULT && reg_dmn == CTRY_DEFAULT)
		country_code = CTRY_UNITED_STATES;

	if (country_code != CTRY_DEFAULT) {
		country = find_country(country_code);
		if (!country) {
			cds_err("not a valid country code");
			return -EINVAL;
		}

		reg_dmn = country->reg_dmn_pair;
	}

	reg->regpair = get_reg_dmn_pair(reg_dmn);
	if (!reg->regpair) {
		cds_err("no regpair is found, can not proceeed");
		return -EINVAL;
	}

	reg->country_code = country_code;

	if (!country)
		country = get_country_from_rd(reg_dmn);

	if (country) {
		reg->alpha2[0] = country->alpha2[0];
		reg->alpha2[1] = country->alpha2[1];
	} else {
		reg->alpha2[0] = '0';
		reg->alpha2[1] = '0';
	}

	return 0;
}

/**
 * cds_fill_and_send_ctl_to_fw() - fill and send ctl to firmware
 * @reg: the regulatory handle
 *
 * Return: none
 */
void cds_fill_and_send_ctl_to_fw(struct regulatory *reg)
{
	const struct reg_dmn *reg_dmn_2g = NULL;
	const struct reg_dmn *reg_dmn_5g = NULL;
	uint8_t ctl_2g, ctl_5g;
	const struct reg_dmn_pair *regpair;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		cds_err("unable to get WMA handle");
		return;
	}

	if (!reg->regpair) {
		cds_err(FL("no regpair is found, can not proceed"));
		return;
	}
	regpair = reg->regpair;
	reg_dmn_2g = get_reg_dmn(regpair->reg_dmn_2ghz);
	if (!reg_dmn_2g) {
		cds_err("failed to get regdmn 2G");
		return;
	}

	reg_dmn_5g = get_reg_dmn(regpair->reg_dmn_5ghz);
	if (!reg_dmn_5g) {
		cds_err("failed to get regdmn 5G");
		return;
	}

	ctl_2g = reg_dmn_2g->conformance_test_limit;
	ctl_5g = reg_dmn_5g->conformance_test_limit;


	reg->ctl_5g = ctl_5g;
	reg->ctl_2g = ctl_2g;

	wma_send_regdomain_info_to_fw(reg->reg_domain, regpair->reg_dmn_2ghz,
				      regpair->reg_dmn_5ghz, ctl_2g, ctl_5g);
}
