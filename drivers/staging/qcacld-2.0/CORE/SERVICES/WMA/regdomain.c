/*
 * Copyright (c) 2011,2013-2014 The Linux Foundation. All rights reserved.
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

#include <adf_os_types.h>
#include "wma.h"
#include "regdomain.h"
#include "regdomain_common.h"

#define N(a) (sizeof(a)/sizeof(a[0]))

static regdm_supp_op_classes regdm_curr_supp_opp_classes = {0};

/* Global Operating Classes */
regdm_op_class_map_t global_op_class[] = {
	{81, 25,  BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{82, 25,  BW20,      {14}},
	{83, 40,  BW40_LOW_PRIMARY,  {1, 2, 3, 4, 5, 6, 7, 8, 9}},
	{84, 40,  BW40_HIGH_PRIMARY, {5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{115, 20, BW20,      {36, 40, 44, 48}},
	{116, 40, BW40_LOW_PRIMARY,  {36, 44}},
	{117, 40, BW40_HIGH_PRIMARY, {40, 48}},
	{118, 20, BW20,      {52, 56, 60, 64}},
	{119, 40, BW40_LOW_PRIMARY,  {52, 60}},
	{120, 40, BW40_HIGH_PRIMARY, {56, 64}},
	{121, 20, BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
	{122, 40, BW40_LOW_PRIMARY,  {100, 108, 116, 124, 132}},
	{123, 40, BW40_HIGH_PRIMARY, {104, 112, 120, 128, 136}},
	{125, 20, BW20,      {149, 153, 157, 161, 165, 169}},
	{126, 40, BW40_LOW_PRIMARY,  {149, 157}},
	{127, 40, BW40_HIGH_PRIMARY, {153, 161}},
	{128, 80, BW80,      {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108,
				112, 116, 120, 124, 128, 132, 136, 140, 144,
				149, 153, 157, 161}},
	{0, 0, 0, {0}},
};

/* Operating Classes in US */
regdm_op_class_map_t us_op_class[] = {
	{1, 20,  BW20,      {36, 40, 44, 48}},
	{2, 20,  BW20,      {52, 56, 60, 64}},
	{4, 20,  BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140,
				144}},
	{5, 20,  BW20,      {149, 153, 157, 161, 165}},
	{12, 25, BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
	{22, 40, BW40_LOW_PRIMARY,  {36, 44}},
	{23, 40, BW40_LOW_PRIMARY,  {52, 60}},
	{24, 40, BW40_LOW_PRIMARY,  {100, 108, 116, 124, 132, 140}},
	{26, 40, BW40_LOW_PRIMARY,  {149, 157}},
	{27, 40, BW40_HIGH_PRIMARY, {40, 48}},
	{28, 40, BW40_HIGH_PRIMARY, {56, 64}},
	{29, 40, BW40_HIGH_PRIMARY, {104, 112, 120, 128, 136, 144}},
	{31, 40, BW40_HIGH_PRIMARY, {153, 161}},
	{32, 40, BW40_LOW_PRIMARY,  {1, 2, 3, 4, 5, 6, 7}},
	{33, 40, BW40_HIGH_PRIMARY, {5, 6, 7, 8, 9, 10, 11}},
	{128, 80, BW80,      {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108,
				112, 116, 120, 124, 128, 132, 136, 140, 144,
				149, 153, 157, 161}},
	{0, 0, 0, {0}},
};

/* Operating Classes in Europe */
regdm_op_class_map_t euro_op_class[] = {
	{1, 20,  BW20,      {36, 40, 44, 48}},
	{2, 20,  BW20,      {52, 56, 60, 64}},
	{3, 20,  BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
	{4, 25,  BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{5, 40,  BW40_LOW_PRIMARY,  {36, 44}},
	{6, 40,  BW40_LOW_PRIMARY,  {52, 60}},
	{7, 40,  BW40_LOW_PRIMARY,  {100, 108, 116, 124, 132}},
	{8, 40,  BW40_HIGH_PRIMARY, {40, 48}},
	{9, 40,  BW40_HIGH_PRIMARY, {56, 64}},
	{10, 40, BW40_HIGH_PRIMARY, {104, 112, 120, 128, 136}},
	{11, 40, BW40_LOW_PRIMARY,  {1, 2, 3, 4, 5, 6, 7, 8, 9}},
	{12, 40, BW40_HIGH_PRIMARY, {5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{17, 20, BW20,      {149, 153, 157, 161, 165, 169}},
	{128, 80, BW80,     {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112,
				116, 120, 124, 128}},
	{0, 0, 0, {0}},
};

/* Operating Classes in Japan */
regdm_op_class_map_t japan_op_class[] = {
	{1, 20,  BW20,      {36, 40, 44, 48}},
	{30, 25, BW20,      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}},
	{31, 25, BW20,      {14}},
	{32, 20, BW20,      {52, 56, 60, 64}},
	{34, 20, BW20,   {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140}},
	{36, 40, BW40_LOW_PRIMARY,  {36, 44}},
	{37, 40, BW40_LOW_PRIMARY,  {52, 60}},
	{39, 40, BW40_LOW_PRIMARY,  {100, 108, 116, 124, 132}},
	{41, 40, BW40_HIGH_PRIMARY, {40, 48}},
	{42, 40, BW40_HIGH_PRIMARY, {56, 64}},
	{44, 40, BW40_HIGH_PRIMARY, {104, 112, 120, 128, 136}},
	{128, 80, BW80,     {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112,
				116, 120, 124, 128}},
	{0, 0, 0, {0}},
};

/*
 * By default, the regdomain tables reference the common tables
 * from regdomain_common.h.  These default tables can be replaced
 * by calls to populate_regdomain_tables functions.
 */
HAL_REG_DMN_TABLES ol_regdmn_Rdt = {
	ahCmnRegDomainPairs,    /* regDomainPairs */
	ahCmnAllCountries,      /* allCountries */
	ahCmnRegDomains,        /* allRegDomains */
	N(ahCmnRegDomainPairs),    /* regDomainPairsCt */
	N(ahCmnAllCountries),      /* allCountriesCt */
	N(ahCmnRegDomains),        /* allRegDomainCt */
};

static u_int16_t get_eeprom_rd(u_int16_t rd)
{
	return rd & ~WORLDWIDE_ROAMING_FLAG;
}

/*
 * Return whether or not the regulatory domain/country in EEPROM
 * is acceptable.
 */
static bool regdmn_is_eeprom_valid(u_int16_t rd)
{
	int32_t i;

	if (rd & COUNTRY_ERD_FLAG) {
		u_int16_t cc = rd & ~COUNTRY_ERD_FLAG;
		for (i = 0; i < ol_regdmn_Rdt.allCountriesCt; i++)
			if (ol_regdmn_Rdt.allCountries[i].countryCode == cc)
				return true;
	} else {
		for (i = 0; i < ol_regdmn_Rdt.regDomainPairsCt; i++)
			if (ol_regdmn_Rdt.regDomainPairs[i].regDmnEnum == rd)
				return true;
	}
	/* TODO: Bring it under debug level */
	adf_os_print("%s: invalid regulatory domain/country code 0x%x\n",
		     __func__, rd);
	return false;
}

/*
 * Find the pointer to the country element in the country table
 * corresponding to the country code
 */
static const COUNTRY_CODE_TO_ENUM_RD *find_country(u_int16_t country_code)
{
	int32_t i;

	for (i = 0; i < ol_regdmn_Rdt.allCountriesCt; i++) {
		if (ol_regdmn_Rdt.allCountries[i].countryCode == country_code)
			return &ol_regdmn_Rdt.allCountries[i];
	}
	return NULL;        /* Not found */
}

int32_t regdmn_find_ctry_by_name(u_int8_t *alpha2)
{
	int32_t i;

	for (i = 0; i < ol_regdmn_Rdt.allCountriesCt; i++) {
		if (ol_regdmn_Rdt.allCountries[i].isoName[0] == alpha2[0] &&
			 ol_regdmn_Rdt.allCountries[i].isoName[1] == alpha2[1])
			return ol_regdmn_Rdt.allCountries[i].countryCode;
	}
	return CTRY_DEFAULT;        /* Not found */
}

static u_int16_t regdmn_get_default_country(u_int16_t rd)
{
	int32_t i;

	if (rd & COUNTRY_ERD_FLAG) {
		const COUNTRY_CODE_TO_ENUM_RD *country = NULL;
		u_int16_t cc = rd & ~COUNTRY_ERD_FLAG;

		country = find_country(cc);
		if (country)
			return cc;
	}

	/*
	 * Check reg domains that have only one country
	 */
	for (i = 0; i < ol_regdmn_Rdt.regDomainPairsCt; i++) {
		if (ol_regdmn_Rdt.regDomainPairs[i].regDmnEnum == rd) {
			if (ol_regdmn_Rdt.regDomainPairs[i].singleCC != 0)
				return ol_regdmn_Rdt.regDomainPairs[i].singleCC;
			else
				i = ol_regdmn_Rdt.regDomainPairsCt;
		}
	}
	return CTRY_DEFAULT;
}

static const REG_DMN_PAIR_MAPPING *get_regdmn_pair(u_int16_t reg_dmn)
{
	int32_t i;

	for (i = 0; i < ol_regdmn_Rdt.regDomainPairsCt; i++) {
		if (ol_regdmn_Rdt.regDomainPairs[i].regDmnEnum == reg_dmn)
			return &ol_regdmn_Rdt.regDomainPairs[i];
	}
	return NULL;
}

static const REG_DOMAIN *get_regdmn(u_int16_t reg_dmn)
{
	int32_t i;

	for (i = 0; i < ol_regdmn_Rdt.regDomainsCt; i++) {
		if (ol_regdmn_Rdt.regDomains[i].regDmnEnum == reg_dmn)
			return &ol_regdmn_Rdt.regDomains[i];
	}
	return NULL;
}

static const COUNTRY_CODE_TO_ENUM_RD *get_country_from_rd(u_int16_t regdmn)
{
	int32_t i;

	for (i = 0; i < ol_regdmn_Rdt.allCountriesCt; i++) {
		if (ol_regdmn_Rdt.allCountries[i].regDmnEnum == regdmn)
			return &ol_regdmn_Rdt.allCountries[i];
	}
	return NULL;        /* Not found */
}

/*
 * Some users have reported their EEPROM programmed with
 * 0x8000 set, this is not a supported regulatory domain
 * but since we have more than one user with it we need
 * a solution for them. We default to WOR0_WORLD
 */
static void regd_sanitize(struct regulatory *reg)
{
	if (reg->reg_domain != COUNTRY_ERD_FLAG)
		return;
	reg->reg_domain = WOR0_WORLD;
}

/*
 * Returns country string for the given regulatory domain.
 */
int32_t regdmn_get_country_alpha2(struct regulatory *reg)
{
	u_int16_t country_code;
	u_int16_t regdmn, rd;
	const COUNTRY_CODE_TO_ENUM_RD *country = NULL;

	regd_sanitize(reg);
	rd = reg->reg_domain;

	if (!regdmn_is_eeprom_valid(rd))
		return -EINVAL;

	regdmn = get_eeprom_rd(rd);

	country_code = regdmn_get_default_country(regdmn);
	if (country_code == CTRY_DEFAULT && regdmn == CTRY_DEFAULT) {
		/* Set to CTRY_UNITED_STATES for testing */
		country_code = CTRY_UNITED_STATES;
	}

	if (country_code != CTRY_DEFAULT) {
		country = find_country(country_code);
		if (!country) {
			/* TODO: Bring it under debug level */
			adf_os_print(KERN_ERR "Not a valid country code\n");
			return -EINVAL;
		}
		regdmn = country->regDmnEnum;
	}

	reg->regpair = get_regdmn_pair(regdmn);
	if (!reg->regpair) {
		/* TODO: Bring it under debug level */
		adf_os_print(KERN_ERR "No regpair is found, can not proceeed\n");
		return -EINVAL;
	}
	reg->country_code = country_code;

	if (!country)
		country = get_country_from_rd(regdmn);

	if (country) {
		reg->alpha2[0] = country->isoName[0];
		reg->alpha2[1] = country->isoName[1];
	} else {
		reg->alpha2[0] = '0';
		reg->alpha2[1] = '0';
	}

	return 0;
}

/*
 * Returns regulatory domain for given country string
 */
int32_t regdmn_get_regdmn_for_country(u_int8_t *alpha2)
{
	u_int8_t i;

	for (i = 0; i < ol_regdmn_Rdt.allCountriesCt; i++) {
		if ((ol_regdmn_Rdt.allCountries[i].isoName[0] == alpha2[0]) &&
		    (ol_regdmn_Rdt.allCountries[i].isoName[1] == alpha2[1]))
			return ol_regdmn_Rdt.allCountries[i].regDmnEnum;
	}
	return -1;
}

/*
 * Test to see if the bitmask array is all zeros
 */
static bool
isChanBitMaskZero(const u_int64_t *bitmask)
{
	int i;

	for (i = 0; i < BMLEN; i++) {
		if (bitmask[i] != 0)
			return false;
	}
	return true;
}

/*
 * Return the mask of available modes based on the hardware
 * capabilities and the specified country code and reg domain.
 */
u_int32_t regdmn_getwmodesnreg(u_int32_t modesAvail,
		const COUNTRY_CODE_TO_ENUM_RD *country,
		const REG_DOMAIN *rd5GHz)
{

	/* Check country regulations for allowed modes */
	if ((modesAvail & (REGDMN_MODE_11A_TURBO|REGDMN_MODE_TURBO)) &&
			(!country->allow11aTurbo))
		modesAvail &= ~(REGDMN_MODE_11A_TURBO | REGDMN_MODE_TURBO);

	if ((modesAvail & REGDMN_MODE_11G_TURBO) &&
			(!country->allow11gTurbo))
		modesAvail &= ~REGDMN_MODE_11G_TURBO;

	if ((modesAvail & REGDMN_MODE_11G) &&
			(!country->allow11g))
		modesAvail &= ~REGDMN_MODE_11G;

	if ((modesAvail & REGDMN_MODE_11A) &&
			(isChanBitMaskZero(rd5GHz->chan11a)))
		modesAvail &= ~REGDMN_MODE_11A;

	if ((modesAvail & REGDMN_MODE_11NG_HT20) &&
			(!country->allow11ng20))
		modesAvail &= ~REGDMN_MODE_11NG_HT20;

	if ((modesAvail & REGDMN_MODE_11NA_HT20) &&
			(!country->allow11na20))
		modesAvail &= ~REGDMN_MODE_11NA_HT20;

	if ((modesAvail & REGDMN_MODE_11NG_HT40PLUS) &&
			(!country->allow11ng40))
		modesAvail &= ~REGDMN_MODE_11NG_HT40PLUS;

	if ((modesAvail & REGDMN_MODE_11NG_HT40MINUS) &&
			(!country->allow11ng40))
		modesAvail &= ~REGDMN_MODE_11NG_HT40MINUS;

	if ((modesAvail & REGDMN_MODE_11NA_HT40PLUS) &&
			(!country->allow11na40))
		modesAvail &= ~REGDMN_MODE_11NA_HT40PLUS;

	if ((modesAvail & REGDMN_MODE_11NA_HT40MINUS) &&
			(!country->allow11na40))
		modesAvail &= ~REGDMN_MODE_11NA_HT40MINUS;

	if ((modesAvail & REGDMN_MODE_11AC_VHT20) &&
			(!country->allow11na20))
		modesAvail &= ~REGDMN_MODE_11AC_VHT20;

	if ((modesAvail & REGDMN_MODE_11AC_VHT40PLUS) &&
			(!country->allow11na40))
		modesAvail &= ~REGDMN_MODE_11AC_VHT40PLUS;

	if ((modesAvail & REGDMN_MODE_11AC_VHT40MINUS) &&
			(!country->allow11na40))
		modesAvail &= ~REGDMN_MODE_11AC_VHT40MINUS;

	if ((modesAvail & REGDMN_MODE_11AC_VHT80) &&
			(!country->allow11na80))
		modesAvail &= ~REGDMN_MODE_11AC_VHT80;

	if ((modesAvail & REGDMN_MODE_11AC_VHT20_2G) &&
			(!country->allow11ng20))
		modesAvail &= ~REGDMN_MODE_11AC_VHT20_2G;

	return modesAvail;
}

void regdmn_get_ctl_info(struct regulatory *reg, u_int32_t modesAvail,
                         u_int32_t modeSelect)
{
	const REG_DOMAIN *regdomain2G = NULL;
	const REG_DOMAIN *regdomain5G = NULL;
	int8_t ctl_2g, ctl_5g, ctl;
	const REG_DOMAIN *rd = NULL;
	const struct cmode *cm;
	const COUNTRY_CODE_TO_ENUM_RD *country;
	const REG_DMN_PAIR_MAPPING *regpair;

	regpair = reg->regpair;
	regdomain2G = get_regdmn(regpair->regDmn2GHz);
	if (!regdomain2G) {
		adf_os_print(KERN_ERR "Failed to get regdmn 2G");
		return;
	}

	regdomain5G = get_regdmn(regpair->regDmn5GHz);
	if (!regdomain5G) {
		adf_os_print(KERN_ERR "Failed to get regdmn 5G");
		return;
	}

	/* find first nible of CTL */
	ctl_2g = regdomain2G->conformance_test_limit;
	ctl_5g = regdomain5G->conformance_test_limit;

	/* find second nible of CTL */
	country = find_country(reg->country_code);
	if (country != NULL)
		modesAvail = regdmn_getwmodesnreg(modesAvail, country, regdomain5G);

	for (cm = modes; cm < &modes[N(modes)]; cm++) {

		if ((cm->mode & modeSelect) == 0)
			continue;

		if ((cm->mode & modesAvail) == 0)
			continue;

		switch (cm->mode) {
		case REGDMN_MODE_TURBO:
			rd = regdomain5G;
			ctl = rd->conformance_test_limit | CTL_TURBO;
			break;
		case REGDMN_MODE_11A:
		case REGDMN_MODE_11NA_HT20:
		case REGDMN_MODE_11NA_HT40PLUS:
		case REGDMN_MODE_11NA_HT40MINUS:
		case REGDMN_MODE_11AC_VHT20:
		case REGDMN_MODE_11AC_VHT40PLUS:
		case REGDMN_MODE_11AC_VHT40MINUS:
		case REGDMN_MODE_11AC_VHT80:
			rd = regdomain5G;
			ctl = rd->conformance_test_limit;
			break;
		case REGDMN_MODE_11B:
			rd = regdomain2G;
			ctl = rd->conformance_test_limit | CTL_11B;
			break;
		case REGDMN_MODE_11G:
		case REGDMN_MODE_11NG_HT20:
		case REGDMN_MODE_11NG_HT40PLUS:
		case REGDMN_MODE_11NG_HT40MINUS:
		case REGDMN_MODE_11AC_VHT20_2G:
		case REGDMN_MODE_11AC_VHT40_2G:
		case REGDMN_MODE_11AC_VHT80_2G:
			rd = regdomain2G;
			ctl = rd->conformance_test_limit | CTL_11G;
			break;
		case REGDMN_MODE_11G_TURBO:
			rd = regdomain2G;
			ctl = rd->conformance_test_limit | CTL_108G;
			break;
		case REGDMN_MODE_11A_TURBO:
			rd = regdomain5G;
			ctl = rd->conformance_test_limit | CTL_108G;
			break;
		default:
			adf_os_print(KERN_ERR "%s: Unkonwn HAL mode 0x%x\n",
					__func__, cm->mode);
			continue;
		}

		if (rd == regdomain2G)
			ctl_2g = ctl;

		if (rd == regdomain5G)
			ctl_5g = ctl;
	}

	/* save the ctl information for future reference */
	reg->ctl_5g = ctl_5g;
	reg->ctl_2g = ctl_2g;

	wma_send_regdomain_info(reg->reg_domain, regpair->regDmn2GHz,
				regpair->regDmn5GHz, ctl_2g, ctl_5g);
}

/* regdmn_set_dfs_region() - to set the dfs region to wma
 *
 * @reg: the regulatory handle
 *
 * Return: none
 */
void regdmn_set_dfs_region(struct regulatory *reg)
{
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);

	if (!wma) {
		WMA_LOGE("%s: Unable to get WMA handle", __func__);
		return;
	}

	WMA_LOGE("%s: dfs_region: %d", __func__, reg->dfs_region);
	wma_set_dfs_regdomain(wma, reg->dfs_region);
}

void regdmn_set_regval(struct regulatory *reg)
{
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);
	u_int32_t modeSelect = 0xFFFFFFFF;

	if (!wma) {
		WMA_LOGE("%s: Unable to get WMA handle", __func__);
		return;
	}

	wma_get_modeselect(wma, &modeSelect);

	regdmn_get_ctl_info(reg, wma->reg_cap.wireless_modes, modeSelect);
	return;
}

/* get the ctl from regdomain */
u_int8_t regdmn_get_ctl_for_regdmn(u_int32_t reg_dmn)
{
   u_int8_t i;
   u_int8_t default_regdmn_ctl = FCC;

   if (reg_dmn == CTRY_DEFAULT)
   {
      return default_regdmn_ctl;
   }
   else
   {
      for (i = 0; i < ol_regdmn_Rdt.regDomainsCt; i++)
      {
         if (ol_regdmn_Rdt.regDomains[i].regDmnEnum == reg_dmn)
            return ol_regdmn_Rdt.regDomains[i].conformance_test_limit;
      }
   }
   return -1;
}

/*
 * Get the 5G reg domain value for reg doamin
 */
u_int16_t get_regdmn_5g(u_int32_t reg_dmn)
{
	u_int16_t i;

	for (i = 0; i < ol_regdmn_Rdt.regDomainPairsCt; i++)
	{
		if (ol_regdmn_Rdt.regDomainPairs[i].regDmnEnum == reg_dmn)
		{
			return ol_regdmn_Rdt.regDomainPairs[i].regDmn5GHz;
		}
	}
	adf_os_print("%s: invalid regulatory domain/country code 0x%x\n",
		     __func__, reg_dmn);
	return 0;
}

/*
 *  Get channel width from a given operating class
 */
u_int16_t regdm_get_chanwidth_from_opclass(u_int8_t *country, u_int8_t channel,
       u_int8_t opclass)
{
	regdm_op_class_map_t *class = NULL;
	u_int16_t i;

	if (0 == adf_os_mem_cmp(country,"US", 2)) {
		class = us_op_class;
	} else if (0 == adf_os_mem_cmp(country,"EU", 2)) {
		class = euro_op_class;
	} else if (0 == adf_os_mem_cmp(country,"JP", 2)) {
		class = japan_op_class;
	} else {
		class = global_op_class;
	}

	while (class->op_class) {
		if (opclass == class->op_class) {
			for (i = 0;
			        (i < MAX_CHANNELS_PER_OPERATING_CLASS &&
			         class->channels[i]);
			         i++) {
				if (channel == class->channels[i])
					return class->ch_spacing;
			}
		}
		class++;
	}
	return 0;
}

/*
 * Get operating class for a given channel
 */
u_int16_t regdm_get_opclass_from_channel(u_int8_t *country, u_int8_t channel,
	u_int8_t offset)
{
	regdm_op_class_map_t *class = NULL;
	u_int16_t i = 0;

	if (0 == adf_os_mem_cmp(country,"US", 2)) {
		class = us_op_class;
	} else if (0 == adf_os_mem_cmp(country,"EU", 2)) {
		class = euro_op_class;
	} else if (0 == adf_os_mem_cmp(country,"JP", 2)) {
		class = japan_op_class;
	} else {
		class = global_op_class;
	}

	while (class->op_class) {
		if ((offset == class->offset) || (offset == BWALL)) {
			for (i = 0;
			     (i < MAX_CHANNELS_PER_OPERATING_CLASS &&
			      class->channels[i]);
			     i++) {
				if (channel == class->channels[i])
					return class->op_class;
			}
		}
		class++;
	}
	return 0;
}

/*
 * Set current operating classes per country, regdomain
 */
u_int16_t regdm_set_curr_opclasses(u_int8_t num_classes, u_int8_t *class)
{
	u_int8_t i;

	if (SIR_MAC_MAX_SUPP_OPER_CLASSES < num_classes) {
		adf_os_print(KERN_ERR "%s: Invalid numClasses (%d)\n",
		             __func__, num_classes);
		return -1;
	}

	for (i = 0 ; i < num_classes; i++) {
		regdm_curr_supp_opp_classes.classes[i] = class[i];
	}
	regdm_curr_supp_opp_classes.num_classes = num_classes;

	return 0;
}

/*
 * Get current operating classes
 */
u_int16_t regdm_get_curr_opclasses(u_int8_t *num_classes, u_int8_t *class)
{
	u_int8_t i;

	if (!num_classes || !class) {
		adf_os_print(KERN_ERR "%s: Either num_classes or class is null\n",
		             __func__);
		return -1;
	}

	for (i = 0 ; i < regdm_curr_supp_opp_classes.num_classes; i++) {
		class[i] = regdm_curr_supp_opp_classes.classes[i];
	}

	*num_classes = regdm_curr_supp_opp_classes.num_classes;

	return 0;
}
