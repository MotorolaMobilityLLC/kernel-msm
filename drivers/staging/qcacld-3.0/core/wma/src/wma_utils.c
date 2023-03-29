/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 *  DOC:    wma_utis.c
 *  This file contains utilities and stats related functions.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "wma_internal.h"
#include "wlan_policy_mgr_api.h"
#include "wmi_unified_param.h"
#include "linux/ieee80211.h"
#include <cdp_txrx_handle.h>
#include <cdp_txrx_peer_ops.h>
#include "cds_reg_service.h"
#include "target_if.h"
#include <wlan_utility.h>
#include <wlan_mlme_main.h>
#include "host_diag_core_log.h"
#include <wlan_mlme_api.h>
#include <../../core/src/vdev_mgr_ops.h>
#include "cdp_txrx_misc.h"
#include <cdp_txrx_host_stats.h>
#include "wlan_mlme_ucfg_api.h"
#include <wlan_cp_stats_mc_tgt_api.h>

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static struct index_data_rate_type mcs_nss1[] = {
	/* MCS L20  S20   L40   S40 */
	{0,  {65,  72},  {135,  150 } },
	{1,  {130, 144}, {270,  300 } },
	{2,  {195, 217}, {405,  450 } },
	{3,  {260, 289}, {540,  600 } },
	{4,  {390, 433}, {815,  900 } },
	{5,  {520, 578}, {1080, 1200} },
	{6,  {585, 650}, {1215, 1350} },
	{7,  {650, 722}, {1350, 1500} }
};

/* HT MCS parameters with Nss = 2 */
static struct index_data_rate_type mcs_nss2[] = {
	/* MCS L20  S20    L40   S40 */
	{0,  {130,  144},  {270,  300 } },
	{1,  {260,  289},  {540,  600 } },
	{2,  {390,  433},  {810,  900 } },
	{3,  {520,  578},  {1080, 1200} },
	{4,  {780,  867},  {1620, 1800} },
	{5,  {1040, 1156}, {2160, 2400} },
	{6,  {1170, 1300}, {2430, 2700} },
	{7,  {1300, 1440}, {2700, 3000} }
};

/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static struct index_vht_data_rate_type vht_mcs_nss1[] = {
	/* MCS L20  S20    L40   S40    L80   S80    L160  S160*/
	{0,  {65,   72 }, {135,  150},  {293,  325}, {585, 650} },
	{1,  {130,  144}, {270,  300},  {585,  650}, {1170, 1300} },
	{2,  {195,  217}, {405,  450},  {878,  975}, {1755, 1950} },
	{3,  {260,  289}, {540,  600},  {1170, 1300}, {2340, 2600} },
	{4,  {390,  433}, {810,  900},  {1755, 1950}, {3510, 3900} },
	{5,  {520,  578}, {1080, 1200}, {2340, 2600}, {4680, 5200} },
	{6,  {585,  650}, {1215, 1350}, {2633, 2925}, {5265, 5850} },
	{7,  {650,  722}, {1350, 1500}, {2925, 3250}, {5850, 6500} },
	{8,  {780,  867}, {1620, 1800}, {3510, 3900}, {7020, 7800} },
	{9,  {865,  960}, {1800, 2000}, {3900, 4333}, {7800, 8667} }
};

/*MCS parameters with Nss = 2*/
static struct index_vht_data_rate_type vht_mcs_nss2[] = {
	/* MCS L20  S20    L40    S40    L80    S80    L160   S160*/
	{0,  {130,  144},  {270,  300},  { 585,  650}, {1170, 1300} },
	{1,  {260,  289},  {540,  600},  {1170, 1300}, {2340, 2600} },
	{2,  {390,  433},  {810,  900},  {1755, 1950}, {3510, 3900} },
	{3,  {520,  578},  {1080, 1200}, {2340, 2600}, {4680, 5200} },
	{4,  {780,  867},  {1620, 1800}, {3510, 3900}, {7020, 7800} },
	{5,  {1040, 1156}, {2160, 2400}, {4680, 5200}, {9360, 10400} },
	{6,  {1170, 1300}, {2430, 2700}, {5265, 5850}, {10530, 11700} },
	{7,  {1300, 1444}, {2700, 3000}, {5850, 6500}, {11700, 13000} },
	{8,  {1560, 1733}, {3240, 3600}, {7020, 7800}, {14040, 15600} },
	{9,  {1730, 1920}, {3600, 4000}, {7800, 8667}, {15600, 17333} }
};

#ifdef WLAN_FEATURE_11AX
/* MCS Based HE rate table */
/* MCS parameters with Nss = 1*/
static struct index_he_data_rate_type he_mcs_nss1[] = {
/* MCS,  {dcm0:0.8/1.6/3.2}, {dcm1:0.8/1.6/3.2} */
	{0,  {{86,   81,   73  }, {43,   40,  36 } }, /* HE20 */
	     {{172,  163,  146 }, {86,   81,  73 } }, /* HE40 */
	     {{360,  340,  306 }, {180,  170, 153} }, /* HE80 */
	     {{721,  681,  613 }, {360,  340, 306} } }, /* HE160/HE80+80 */
	{1,  {{172,  163,  146 }, {86,   81,  73 } },
	     {{344,  325,  293 }, {172,  163, 146} },
	     {{721,  681,  613 }, {360,  340, 306} },
	     {{1441, 1361, 1225}, {721,  681, 613} } },
	{2,  {{258,  244,  219 }, {0} },
	     {{516,  488,  439 }, {0} },
	     {{1081, 1021, 919 }, {0} },
	     {{2162, 2042, 1838}, {0} } },
	{3,  {{344,  325,  293 }, {172,  163, 146} },
	     {{688,  650,  585 }, {344,  325, 293} },
	     {{1441, 1361, 1225}, {721,  681, 613} },
	     {{2882, 2722, 2450}, {1441, 1361, 1225} } },
	{4,  {{516,  488,  439 }, {258,  244, 219} },
	     {{1032, 975,  878 }, {516,  488, 439} },
	     {{2162, 2042, 1838}, {1081, 1021, 919} },
	     {{4324, 4083, 3675}, {2162, 2042, 1838} } },
	{5,  {{688,  650,  585 }, {0} },
	     {{1376, 1300, 1170}, {0} },
	     {{2882, 2722, 2450}, {0} },
	     {{5765, 5444, 4900}, {0} } },
	{6,  {{774,  731,  658 }, {0} },
	     {{1549, 1463, 1316}, {0} },
	     {{3243, 3063, 2756}, {0} },
	     {{6485, 6125, 5513}, {0} } },
	{7,  {{860,  813,  731 }, {0} },
	     {{1721, 1625, 1463}, {0} },
	     {{3603, 3403, 3063}, {0} },
	     {{7206, 6806, 6125}, {0} } },
	{8,  {{1032, 975,  878 }, {0} },
	     {{2065, 1950, 1755}, {0} },
	     {{4324, 4083, 3675}, {0} },
	     {{8647, 8167, 7350}, {0} } },
	{9,  {{1147, 1083, 975 }, {0} },
	     {{2294, 2167, 1950}, {0} },
	     {{4804, 4537, 4083}, {0} },
	     {{9607, 9074, 8166}, {0} } },
	{10, {{1290, 1219, 1097}, {0} },
	     {{2581, 2438, 2194}, {0} },
	     {{5404, 5104, 4594}, {0} },
	     {{10809, 10208, 9188}, {0} } },
	{11, {{1434, 1354, 1219}, {0} },
	     {{2868, 2708, 2438}, {0} },
	     {{6004, 5671, 5104}, {0} },
	     {{12010, 11342, 10208}, {0} } },
	{12, {{1549, 1463, 1316}, {0} },
	     {{3097, 2925, 2633}, {0} },
	     {{6485, 6125, 5513}, {0} },
	     {{12971, 12250, 11025}, {0} } },
	{13, {{1721, 1625, 1463}, {0} },
	     {{3441, 3250, 2925}, {0} },
	     {{7206, 6806, 6125}, {0} },
	     {{14412, 13611, 12250}, {0} } }
};

/*MCS parameters with Nss = 2*/
static struct index_he_data_rate_type he_mcs_nss2[] = {
/* MCS,  {dcm0:0.8/1.6/3.2}, {dcm1:0.8/1.6/3.2} */
	{0,  {{172,   163,   146 }, {86,   81,   73 } }, /* HE20 */
	     {{344,   325,   293 }, {172,  163,  146} }, /* HE40 */
	     {{721,   681,   613 }, {360,  340,  306} }, /* HE80 */
	     {{1441, 1361, 1225},   {721, 681, 613} } }, /* HE160/HE80+80 */
	{1,  {{344,   325,   293 }, {172,  163,  146} },
	     {{688,   650,   585 }, {344,  325,  293} },
	     {{1441,  1361,  1225}, {721,  681,  613} },
	     {{2882, 2722, 2450},   {1441, 1361, 1225} } },
	{2,  {{516,   488,   439 }, {0} },
	     {{1032,  975,   878 }, {0} },
	     {{2162,  2042,  1838}, {0} },
	     {{4324, 4083, 3675}, {0} } },
	{3,  {{688,   650,   585 }, {344,  325,  293 } },
	     {{1376,  1300,  1170}, {688,  650,  585  } },
	     {{2882,  2722,  2450}, {1441, 1361, 1225} },
	     {{5765, 5444, 4900}, {2882, 2722, 2450} } },
	{4,  {{1032,  975,   878 }, {516,  488,  439 } },
	     {{2065,  1950,  1755}, {1032, 975,  878 } },
	     {{4324,  4083,  3675}, {2162, 2042, 1838} },
	     {{8647, 8167, 7350}, {4324, 4083, 3675} } },
	{5,  {{1376,  1300,  1170}, {0} },
	     {{2753,  2600,  2340}, {0} },
	     {{5765,  5444,  4900}, {0} },
	     {{11529, 10889, 9800}, {0} } },
	{6,  {{1549,  1463,  1316}, {0} },
	     {{3097,  2925,  2633}, {0} },
	     {{6485,  6125,  5513}, {0} },
	     {{12971, 12250, 11025}, {0} } },
	{7,  {{1721,  1625,  1463}, {0} },
	     {{3441,  3250,  2925}, {0} },
	     {{7206,  6806,  6125}, {0} },
	     {{14412, 13611, 12250}, {0} } },
	{8,  {{2065,  1950,  1755}, {0} },
	     {{4129,  3900,  3510}, {0} },
	     {{8647,  8167,  7350}, {0} },
	     {{17294, 16333, 14700}, {0} } },
	{9,  {{2294,  2167,  1950}, {0} },
	     {{4588,  4333,  3900}, {0} },
	     {{9607,  9074,  8166}, {0} },
	     {{19215, 18148, 16333}, {0} } },
	{10, {{2581,  2438,  2194}, {0} },
	     {{5162,  4875,  4388}, {0} },
	     {{10809, 10208, 9188}, {0} },
	     {{21618, 20417, 18375}, {0} } },
	{11, {{2868,  2708,  2438}, {0} },
	     {{5735,  5417,  4875}, {0} },
	     {{12010, 11343, 10208}, {0} },
	     {{24019, 22685, 20416}, {0} } },
	{12, {{3097,  2925,  2633}, {0} },
	     {{6194,  5850,  5265}, {0} },
	     {{12971, 12250, 11025}, {0} },
	     {{25941, 24500, 22050}, {0} } },
	{13, {{3441,  3250,  2925}, {0} },
	     {{6882,  6500,  5850}, {0} },
	     {{14412, 13611, 12250}, {0} },
	     {{28824, 27222, 24500}, {0} } }
};
#endif

#ifdef BIG_ENDIAN_HOST

/* ############# function definitions ############ */

/**
 * wma_swap_bytes() - swap bytes
 * @pv: buffer
 * @n: swap bytes
 *
 * Return: none
 */
void wma_swap_bytes(void *pv, uint32_t n)
{
	int32_t no_words;
	int32_t i;
	uint32_t *word_ptr;

	no_words = n / sizeof(uint32_t);
	word_ptr = (uint32_t *) pv;
	for (i = 0; i < no_words; i++)
		*(word_ptr + i) = __cpu_to_le32(*(word_ptr + i));
}

#define SWAPME(x, len) wma_swap_bytes(&x, len)
#endif /* BIG_ENDIAN_HOST */

/**
 * wma_mcs_rate_match() - find the match mcs rate
 * @raw_rate: the rate to look up
 * @is_he: if it is he rate
 * @nss1_rate: the nss1 rate
 * @nss2_rate: the nss2 rate
 * @nss: the nss in use
 * @guard_interval: to get guard interval from rate
 *
 * This is a helper function to find the match of the tx_rate
 * and return nss/guard interval.
 *
 * Return: the found rate or 0 otherwise
 */
static inline uint16_t wma_mcs_rate_match(uint16_t raw_rate,
					  bool is_he,
					  uint16_t *nss1_rate,
					  uint16_t *nss2_rate,
					  uint8_t *nss,
					  enum txrate_gi *guard_interval)
{
	uint8_t gi_index;
	uint8_t gi_index_max = 2;
	uint16_t ret_rate = 0;

	if (is_he)
		gi_index_max = 3;

	for (gi_index = 0; gi_index < gi_index_max; gi_index++) {
		if (raw_rate == nss1_rate[gi_index]) {
			*nss = 1;
			ret_rate = nss1_rate[gi_index];
			break;
		}
		if (*nss == 2 && raw_rate == nss2_rate[gi_index]) {
			ret_rate = nss2_rate[gi_index];
			break;
		}
	}

	if (ret_rate) {
		if (gi_index == 1)
			*guard_interval =
				is_he ? TXRATE_GI_1_6_US : TXRATE_GI_0_4_US;
		else if (is_he && gi_index == 2)
			*guard_interval = TXRATE_GI_3_2_US;
		else
			*guard_interval = TXRATE_GI_0_8_US;
	}

	return ret_rate;
}

#ifdef WLAN_FEATURE_11AX
/**
 * wma_match_he_rate() - match he rate
 * @raw_rate: raw rate from fw
 * @rate_flags: rate flags
 * @is_he_mcs_12_13_supported: is he mcs12/13 supported
 * @nss: nss
 * @dcm: dcm
 * @guard_interval: guard interval
 * @mcs_rate_flag: mcs rate flags
 * @p_index: index for matched rate
 *
 *  Return: return match rate if found, else 0
 */
static uint16_t wma_match_he_rate(uint16_t raw_rate,
				  enum tx_rate_info rate_flags,
				  bool is_he_mcs_12_13_supported,
				  uint8_t *nss, uint8_t *dcm,
				  enum txrate_gi *guard_interval,
				  enum tx_rate_info *mcs_rate_flag,
				  uint8_t *p_index)
{
	uint8_t index = 0, max_he_mcs_idx;
	uint8_t dcm_index_max = 1;
	uint8_t dcm_index = 0;
	uint16_t match_rate = 0;
	uint16_t *nss1_rate;
	uint16_t *nss2_rate;

	*p_index = 0;
	if (!(rate_flags & (TX_RATE_HE160 | TX_RATE_HE80 | TX_RATE_HE40 |
		TX_RATE_HE20)))
		return 0;

	if (is_he_mcs_12_13_supported)
		max_he_mcs_idx = MAX_HE_MCS12_13_IDX;
	else
		max_he_mcs_idx = MAX_HE_MCS_IDX;

	for (index = 0; index < max_he_mcs_idx; index++) {
		dcm_index_max = IS_MCS_HAS_DCM_RATE(index) ? 2 : 1;

		for (dcm_index = 0; dcm_index < dcm_index_max;
			 dcm_index++) {
			if (rate_flags & TX_RATE_HE160) {
				nss1_rate = &he_mcs_nss1[index].
					supported_he160_rate[dcm_index][0];
				nss2_rate = &he_mcs_nss2[index].
					supported_he160_rate[dcm_index][0];
				/* check for he160 nss1/2 rate set */
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);
				if (match_rate)
					goto rate_found;
			}

			if (rate_flags & (TX_RATE_HE80 | TX_RATE_HE160)) {
				nss1_rate = &he_mcs_nss1[index].
					supported_he80_rate[dcm_index][0];
				nss2_rate = &he_mcs_nss2[index].
					supported_he80_rate[dcm_index][0];
				/* check for he80 nss1/2 rate set */
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);
				if (match_rate) {
					*mcs_rate_flag &= ~TX_RATE_HE160;
					goto rate_found;
				}
			}

			if (rate_flags & (TX_RATE_HE40 | TX_RATE_HE80 |
					  TX_RATE_HE160)) {
				nss1_rate = &he_mcs_nss1[index].
					supported_he40_rate[dcm_index][0];
				nss2_rate = &he_mcs_nss2[index].
					supported_he40_rate[dcm_index][0];
				/* check for he40 nss1/2 rate set */
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);

				if (match_rate) {
					*mcs_rate_flag &= ~(TX_RATE_HE80 |
							    TX_RATE_HE160);
					goto rate_found;
				}
			}

			if (rate_flags & (TX_RATE_HE80 | TX_RATE_HE40 |
				TX_RATE_HE20 | TX_RATE_HE160)) {
				nss1_rate = &he_mcs_nss1[index].
					supported_he20_rate[dcm_index][0];
				nss2_rate = &he_mcs_nss2[index].
					supported_he20_rate[dcm_index][0];
				/* check for he20 nss1/2 rate set */
				match_rate = wma_mcs_rate_match(raw_rate, 1,
								nss1_rate,
								nss2_rate,
								nss,
								guard_interval);

				if (match_rate) {
					*mcs_rate_flag &= TX_RATE_HE20;
					goto rate_found;
				}
			}
		}
	}

rate_found:
	if (match_rate) {
		if (dcm_index == 1)
			*dcm = 1;
		*p_index = index;
	}
	return match_rate;
}
#else
static uint16_t wma_match_he_rate(uint16_t raw_rate,
				  enum tx_rate_info rate_flags,
				  bool is_he_mcs_12_13_supported,
				  uint8_t *nss, uint8_t *dcm,
				  enum txrate_gi *guard_interval,
				  enum tx_rate_info *mcs_rate_flag,
				  uint8_t *p_index)
{
		return 0;
}
#endif

uint8_t wma_get_mcs_idx(uint16_t raw_rate, enum tx_rate_info rate_flags,
			bool is_he_mcs_12_13_supported,
			uint8_t *nss, uint8_t *dcm,
			enum txrate_gi *guard_interval,
			enum tx_rate_info *mcs_rate_flag)
{
	uint8_t  index = 0;
	uint16_t match_rate = 0;
	uint16_t *nss1_rate;
	uint16_t *nss2_rate;

	wma_debug("Rates from FW:  raw_rate:%d rate_flgs: 0x%x is_he_mcs_12_13_supported: %d nss: %d",
		  raw_rate, rate_flags, is_he_mcs_12_13_supported, *nss);

	*mcs_rate_flag = rate_flags;

	match_rate = wma_match_he_rate(raw_rate, rate_flags,
				       is_he_mcs_12_13_supported,
				       nss, dcm, guard_interval,
				       mcs_rate_flag, &index);
	if (match_rate)
		goto rate_found;

	for (index = 0; index < MAX_VHT_MCS_IDX; index++) {
		if (rate_flags & TX_RATE_VHT160) {
			nss1_rate = &vht_mcs_nss1[index].ht160_rate[0];
			nss2_rate = &vht_mcs_nss2[index].ht160_rate[0];
			/* check for vht160 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate)
				goto rate_found;
		}
		if (rate_flags & (TX_RATE_VHT80 | TX_RATE_VHT160)) {
			nss1_rate = &vht_mcs_nss1[index].ht80_rate[0];
			nss2_rate = &vht_mcs_nss2[index].ht80_rate[0];
			/* check for vht80 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate) {
				*mcs_rate_flag &= ~TX_RATE_VHT160;
				goto rate_found;
			}
		}
		if (rate_flags & (TX_RATE_VHT40 | TX_RATE_VHT80 |
				TX_RATE_VHT160)) {
			nss1_rate = &vht_mcs_nss1[index].ht40_rate[0];
			nss2_rate = &vht_mcs_nss2[index].ht40_rate[0];
			/* check for vht40 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate) {
				*mcs_rate_flag &= ~TX_RATE_VHT80;
				goto rate_found;
			}
		}
		if (rate_flags & (TX_RATE_VHT20 | TX_RATE_VHT40 |
			TX_RATE_VHT80 | TX_RATE_VHT160)) {
			nss1_rate = &vht_mcs_nss1[index].ht20_rate[0];
			nss2_rate = &vht_mcs_nss2[index].ht20_rate[0];
			/* check for vht20 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate) {
				*mcs_rate_flag &= ~(TX_RATE_VHT80 |
						TX_RATE_VHT40);
				goto rate_found;
			}
		}
	}
	for (index = 0; index < MAX_HT_MCS_IDX; index++) {
		if (rate_flags & TX_RATE_HT40) {
			nss1_rate = &mcs_nss1[index].ht40_rate[0];
			nss2_rate = &mcs_nss2[index].ht40_rate[0];
			/* check for ht40 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate) {
				*mcs_rate_flag = TX_RATE_HT40;
				if (*nss == 2)
					index += MAX_HT_MCS_IDX;
				goto rate_found;
			}
		}
		if (rate_flags & (TX_RATE_HT20 | TX_RATE_HT40)) {
			nss1_rate = &mcs_nss1[index].ht20_rate[0];
			nss2_rate = &mcs_nss2[index].ht20_rate[0];
			/* check for ht20 nss1/2 rate set */
			match_rate = wma_mcs_rate_match(raw_rate, 0,
							nss1_rate,
							nss2_rate,
							nss, guard_interval);
			if (match_rate) {
				*mcs_rate_flag = TX_RATE_HT20;
				if (*nss == 2)
					index += MAX_HT_MCS_IDX;
				goto rate_found;
			}
		}
	}

rate_found:

	/* set SGI flag only if this is SGI rate */
	if (match_rate && *guard_interval == TXRATE_GI_0_4_US)
		*mcs_rate_flag |= TX_RATE_SGI;
	else
		*mcs_rate_flag &= ~TX_RATE_SGI;

	wma_debug("Matched rate in table: %d index: %d"
		 " mcs_rate_flag: 0x%x nss %d guard interval %d",
		 match_rate, index, *mcs_rate_flag,
		 *nss, *guard_interval);

	return match_rate ? index : INVALID_MCS_IDX;
}

void wma_lost_link_info_handler(tp_wma_handle wma, uint32_t vdev_id,
					int32_t rssi)
{
	struct sir_lost_link_info *lost_link_info;
	QDF_STATUS qdf_status;
	struct scheduler_msg sme_msg = {0};

	if (vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", vdev_id);
		return;
	}

	/* report lost link information only for STA mode */
	if (wma_is_vdev_up(vdev_id) &&
	    (WMI_VDEV_TYPE_STA == wma->interfaces[vdev_id].type) &&
	    (0 == wma->interfaces[vdev_id].sub_type)) {
		lost_link_info = qdf_mem_malloc(sizeof(*lost_link_info));
		if (!lost_link_info)
			return;

		lost_link_info->vdev_id = vdev_id;
		lost_link_info->rssi = rssi;
		sme_msg.type = eWNI_SME_LOST_LINK_INFO_IND;
		sme_msg.bodyptr = lost_link_info;
		sme_msg.bodyval = 0;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_SME,
						    &sme_msg);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			wma_err("fail to post msg to SME");
			qdf_mem_free(lost_link_info);
		}
	}
}

/**
 * host_map_smps_mode() - map fw smps mode to enum eSmpsModeValue
 * @fw_smps_mode: fw smps mode
 *
 * Return: return enum eSmpsModeValue
 */
enum eSmpsModeValue host_map_smps_mode(A_UINT32 fw_smps_mode)
{
	enum eSmpsModeValue smps_mode = SMPS_MODE_DISABLED;

	switch (fw_smps_mode) {
	case WMI_SMPS_FORCED_MODE_STATIC:
		smps_mode = STATIC_SMPS_MODE;
		break;
	case WMI_SMPS_FORCED_MODE_DYNAMIC:
		smps_mode = DYNAMIC_SMPS_MODE;
		break;
	default:
		smps_mode = SMPS_MODE_DISABLED;
	}

	return smps_mode;
}

/**
 * wma_smps_mode_to_force_mode_param() - Map smps mode to force
 * mode commmand param
 * @smps_mode: SMPS mode according to the protocol
 *
 * Return: int > 0 for success else failure
 */
int wma_smps_mode_to_force_mode_param(uint8_t smps_mode)
{
	int param = -EINVAL;

	switch (smps_mode) {
	case STATIC_SMPS_MODE:
		param = WMI_SMPS_FORCED_MODE_STATIC;
		break;
	case DYNAMIC_SMPS_MODE:
		param = WMI_SMPS_FORCED_MODE_DYNAMIC;
		break;
	case SMPS_MODE_DISABLED:
		param = WMI_SMPS_FORCED_MODE_DISABLED;
		break;
	default:
		wma_err("smps mode cannot be mapped :%d", smps_mode);
	}
	return param;
}

#ifdef WLAN_FEATURE_STATS_EXT
#ifdef FEATURE_STATS_EXT_V2
int wma_stats_ext_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	WMI_STATS_EXT_EVENTID_param_tlvs *param_buf;
	tSirStatsExtEvent *stats_ext_event;
	wmi_stats_ext_event_fixed_param *stats_ext_info;
	QDF_STATUS status;
	struct scheduler_msg cds_msg = {0};
	uint8_t *buf_ptr;
	uint32_t alloc_len;
	struct cdp_txrx_ext_stats ext_stats = {0};
	struct cdp_soc_t *soc_hdl = cds_get_context(QDF_MODULE_ID_SOC);

	wma_debug("Posting stats ext event to SME");

	param_buf = (WMI_STATS_EXT_EVENTID_param_tlvs *)event_buf;
	if (!param_buf) {
		wma_err("Invalid stats ext event buf");
		return -EINVAL;
	}

	stats_ext_info = param_buf->fixed_param;
	buf_ptr = (uint8_t *)stats_ext_info;

	alloc_len = sizeof(tSirStatsExtEvent);
	alloc_len += stats_ext_info->data_len;
	alloc_len += sizeof(struct cdp_txrx_ext_stats);

	if (stats_ext_info->data_len > (WMI_SVC_MSG_MAX_SIZE -
	    WMI_TLV_HDR_SIZE - sizeof(*stats_ext_info)) ||
	    stats_ext_info->data_len > param_buf->num_data) {
		wma_err("Excess data_len:%d, num_data:%d",
			stats_ext_info->data_len, param_buf->num_data);
		return -EINVAL;
	}
	stats_ext_event = qdf_mem_malloc(alloc_len);
	if (!stats_ext_event)
		return -ENOMEM;

	buf_ptr += sizeof(wmi_stats_ext_event_fixed_param) + WMI_TLV_HDR_SIZE;

	stats_ext_event->vdev_id = stats_ext_info->vdev_id;
	stats_ext_event->event_data_len = stats_ext_info->data_len;
	qdf_mem_copy(stats_ext_event->event_data,
		     buf_ptr, stats_ext_event->event_data_len);

	cdp_txrx_ext_stats_request(soc_hdl, OL_TXRX_PDEV_ID, &ext_stats);
	qdf_mem_copy(stats_ext_event->event_data +
		     stats_ext_event->event_data_len,
		     &ext_stats, sizeof(struct cdp_txrx_ext_stats));

	stats_ext_event->event_data_len += sizeof(struct cdp_txrx_ext_stats);

	cds_msg.type = eWNI_SME_STATS_EXT_EVENT;
	cds_msg.bodyptr = (void *)stats_ext_event;
	cds_msg.bodyval = 0;

	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &cds_msg);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(stats_ext_event);
		return -EFAULT;
	}

	wma_debug("stats ext event Posted to SME");
	return 0;
}
#else
int wma_stats_ext_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	WMI_STATS_EXT_EVENTID_param_tlvs *param_buf;
	tSirStatsExtEvent *stats_ext_event;
	wmi_stats_ext_event_fixed_param *stats_ext_info;
	QDF_STATUS status;
	struct scheduler_msg cds_msg = {0};
	uint8_t *buf_ptr;
	uint32_t alloc_len;

	wma_debug("Posting stats ext event to SME");

	param_buf = (WMI_STATS_EXT_EVENTID_param_tlvs *)event_buf;
	if (!param_buf) {
		wma_err("Invalid stats ext event buf");
		return -EINVAL;
	}

	stats_ext_info = param_buf->fixed_param;
	buf_ptr = (uint8_t *)stats_ext_info;

	alloc_len = sizeof(tSirStatsExtEvent);
	alloc_len += stats_ext_info->data_len;

	if (stats_ext_info->data_len > (WMI_SVC_MSG_MAX_SIZE -
	    WMI_TLV_HDR_SIZE - sizeof(*stats_ext_info)) ||
	    stats_ext_info->data_len > param_buf->num_data) {
		wma_err("Excess data_len:%d, num_data:%d",
			stats_ext_info->data_len, param_buf->num_data);
		return -EINVAL;
	}
	stats_ext_event = qdf_mem_malloc(alloc_len);
	if (!stats_ext_event)
		return -ENOMEM;

	buf_ptr += sizeof(wmi_stats_ext_event_fixed_param) + WMI_TLV_HDR_SIZE;

	stats_ext_event->vdev_id = stats_ext_info->vdev_id;
	stats_ext_event->event_data_len = stats_ext_info->data_len;
	qdf_mem_copy(stats_ext_event->event_data,
		     buf_ptr, stats_ext_event->event_data_len);

	cds_msg.type = eWNI_SME_STATS_EXT_EVENT;
	cds_msg.bodyptr = (void *)stats_ext_event;
	cds_msg.bodyval = 0;

	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &cds_msg);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(stats_ext_event);
		return -EFAULT;
	}

	wma_debug("stats ext event Posted to SME");
	return 0;
}
#endif
#endif /* WLAN_FEATURE_STATS_EXT */

/**
 * wma_profile_data_report_event_handler() - fw profiling handler
 * @handle:     wma handle
 * @event_buf:  event buffer received from fw
 * @len:        length of data
 *
 * Return: 0 for success or error code
 */
int wma_profile_data_report_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	WMI_WLAN_PROFILE_DATA_EVENTID_param_tlvs *param_buf;
	wmi_wlan_profile_ctx_t *profile_ctx;
	wmi_wlan_profile_t *profile_data;
	uint32_t i = 0;
	uint32_t entries;
	uint8_t *buf_ptr;
	char temp_str[150];

	param_buf = (WMI_WLAN_PROFILE_DATA_EVENTID_param_tlvs *) event_buf;
	if (!param_buf) {
		wma_err("Invalid profile data event buf");
		return -EINVAL;
	}
	profile_ctx = param_buf->profile_ctx;
	buf_ptr = (uint8_t *)profile_ctx;
	buf_ptr = buf_ptr + sizeof(wmi_wlan_profile_ctx_t) + WMI_TLV_HDR_SIZE;
	profile_data = (wmi_wlan_profile_t *) buf_ptr;
	entries = profile_ctx->bin_count;

	if (entries > param_buf->num_profile_data) {
		wma_err("FW bin count %d more than data %d in TLV hdr",
			 entries,
			 param_buf->num_profile_data);
		return -EINVAL;
	}

	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
				"Profile data stats\n");
	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
		"TOT: %d\n"
		"tx_msdu_cnt: %d\n"
		"tx_mpdu_cnt: %d\n"
		"tx_ppdu_cnt: %d\n"
		"rx_msdu_cnt: %d\n"
		"rx_mpdu_cnt: %d\n"
		"bin_count: %d\n",
		profile_ctx->tot,
		profile_ctx->tx_msdu_cnt,
		profile_ctx->tx_mpdu_cnt,
		profile_ctx->tx_ppdu_cnt,
		profile_ctx->rx_msdu_cnt,
		profile_ctx->rx_mpdu_cnt,
		profile_ctx->bin_count);

	QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
		  "Profile ID: Count: TOT: Min: Max: hist_intvl: hist[0]: hist[1]:hist[2]");

	for (i = 0; i < entries; i++) {
		if (i == WMI_WLAN_PROFILE_MAX_BIN_CNT)
			break;
		snprintf(temp_str, sizeof(temp_str),
			 " %d : %d : %d : %d : %d : %d : %d : %d : %d",
			profile_data[i].id,
			profile_data[i].cnt,
			profile_data[i].tot,
			profile_data[i].min,
			profile_data[i].max,
			profile_data[i].hist_intvl,
			profile_data[i].hist[0],
			profile_data[i].hist[1],
			profile_data[i].hist[2]);
		QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_ERROR,
			"%s", temp_str);
	}

	return 0;
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

#define WMA_FILL_TX_STATS(eve, msg)   do {\
	(msg)->msdus = (eve)->tx_msdu_cnt;\
	(msg)->mpdus = (eve)->tx_mpdu_cnt;\
	(msg)->ppdus = (eve)->tx_ppdu_cnt;\
	(msg)->bytes = (eve)->tx_bytes;\
	(msg)->drops = (eve)->tx_msdu_drop_cnt;\
	(msg)->drop_bytes = (eve)->tx_drop_bytes;\
	(msg)->retries = (eve)->tx_mpdu_retry_cnt;\
	(msg)->failed = (eve)->tx_mpdu_fail_cnt;\
} while (0)

#define WMA_FILL_RX_STATS(eve, msg)       do {\
	(msg)->mpdus = (eve)->mac_rx_mpdu_cnt;\
	(msg)->bytes = (eve)->mac_rx_bytes;\
	(msg)->ppdus = (eve)->phy_rx_ppdu_cnt;\
	(msg)->ppdu_bytes = (eve)->phy_rx_bytes;\
	(msg)->mpdu_retry = (eve)->rx_mpdu_retry_cnt;\
	(msg)->mpdu_dup = (eve)->rx_mpdu_dup_cnt;\
	(msg)->mpdu_discard = (eve)->rx_mpdu_discard_cnt;\
} while (0)

/**
 * wma_get_ll_stats_ext_buf() - alloc result buffer for MAC counters
 * @len: buffer length output
 * @peer_num: peer number
 * @fixed_param: fixed parameters in WMI event
 *
 * Structure of the stats message
 * LL_EXT_STATS
 *  |
 *  |--Channel stats[1~n]
 *  |--Peer[1~n]
 *      |
 *      +---Signal
 *      +---TX
 *      |    +---BE
 *      |    +---BK
 *      |    +---VI
 *      |    +---VO
 *      |
 *      +---RX
 *           +---BE
 *           +---BK
 *           +---VI
 *           +---VO
 * For each Access Category, the arregation and mcs
 * stats are as this:
 *  TX
 *   +-BE/BK/VI/VO
 *         +----tx_mpdu_aggr_array
 *         +----tx_succ_mcs_array
 *         +----tx_fail_mcs_array
 *         +----tx_delay_array
 *  RX
 *   +-BE/BK/VI/VO
 *         +----rx_mpdu_aggr_array
 *         +----rx_mcs_array
 *
 * return: Address for result buffer.
 */
static tSirLLStatsResults *wma_get_ll_stats_ext_buf(uint32_t *len,
						    uint32_t peer_num,
			wmi_report_stats_event_fixed_param *fixed_param)
{
	tSirLLStatsResults *buf;
	uint32_t buf_len;
	uint32_t total_array_len, total_peer_len;
	bool excess_data = false;

	if (!len || !fixed_param) {
		wma_err("Invalid input parameters");
		return NULL;
	}

	/*
	 * Result buffer has a structure like this:
	 *     ---------------------------------
	 *     |      trigger_cond_i           |
	 *     +-------------------------------+
	 *     |      cca_chgd_bitmap          |
	 *     +-------------------------------+
	 *     |      sig_chgd_bitmap          |
	 *     +-------------------------------+
	 *     |      tx_chgd_bitmap           |
	 *     +-------------------------------+
	 *     |      rx_chgd_bitmap           |
	 *     +-------------------------------+
	 *     |      peer_num                 |
	 *     +-------------------------------+
	 *     |      channel_num              |
	 *     +-------------------------------+
	 *     |      tx_mpdu_aggr_array_len   |
	 *     +-------------------------------+
	 *     |      tx_succ_mcs_array_len    |
	 *     +-------------------------------+
	 *     |      tx_fail_mcs_array_len    |
	 *     +-------------------------------+
	 *     |      tx_delay_array_len       |
	 *     +-------------------------------+
	 *     |      rx_mpdu_aggr_array_len   |
	 *     +-------------------------------+
	 *     |      rx_mcs_array_len         |
	 *     +-------------------------------+
	 *     |      pointer to CCA stats     |
	 *     +-------------------------------+
	 *     |      CCA stats                |
	 *     +-------------------------------+
	 *     |      peer_stats               |----+
	 *     +-------------------------------+    |
	 *     | TX aggr/mcs parameters array  |    |
	 *     | Length of this buffer is      |    |
	 *     | not fixed.                    |<-+ |
	 *     +-------------------------------+  | |
	 *     |      per peer tx stats        |--+ |
	 *     |         BE                    | <--+
	 *     |         BK                    |    |
	 *     |         VI                    |    |
	 *     |         VO                    |    |
	 *     +-------------------------------+    |
	 *     | TX aggr/mcs parameters array  |    |
	 *     | Length of this buffer is      |    |
	 *     | not fixed.                    |<-+ |
	 *     +-------------------------------+  | |
	 *     |      peer peer rx stats       |--+ |
	 *     |         BE                    | <--+
	 *     |         BK                    |
	 *     |         VI                    |
	 *     |         VO                    |
	 *     ---------------------------------
	 */

	buf_len = sizeof(tSirLLStatsResults) +
		  sizeof(struct sir_wifi_ll_ext_stats);
	do {
		if (fixed_param->num_chan_cca_stats > (WMI_SVC_MSG_MAX_SIZE /
		    sizeof(struct sir_wifi_chan_cca_stats))) {
			excess_data = true;
			break;
		}
		buf_len += (fixed_param->num_chan_cca_stats *
				sizeof(struct sir_wifi_chan_cca_stats));
		if (fixed_param->tx_mpdu_aggr_array_len >
		    WMI_SVC_MSG_MAX_SIZE) {
			excess_data = true;
			break;
		} else {
			total_array_len = fixed_param->tx_mpdu_aggr_array_len;
		}
		if (fixed_param->tx_succ_mcs_array_len >
		    (WMI_SVC_MSG_MAX_SIZE - total_array_len)) {
			excess_data = true;
			break;
		} else {
			total_array_len += fixed_param->tx_succ_mcs_array_len;
		}
		if (fixed_param->tx_fail_mcs_array_len >
		    (WMI_SVC_MSG_MAX_SIZE - total_array_len)) {
			excess_data = true;
			break;
		} else {
			total_array_len += fixed_param->tx_fail_mcs_array_len;
		}
		if (fixed_param->tx_ppdu_delay_array_len >
		    (WMI_SVC_MSG_MAX_SIZE - total_array_len)) {
			excess_data = true;
			break;
		} else {
			total_array_len += fixed_param->tx_ppdu_delay_array_len;
		}
		if (fixed_param->rx_mpdu_aggr_array_len >
		    (WMI_SVC_MSG_MAX_SIZE - total_array_len)) {
			excess_data = true;
			break;
		} else {
			total_array_len += fixed_param->rx_mpdu_aggr_array_len;
		}
		if (fixed_param->rx_mcs_array_len >
		    (WMI_SVC_MSG_MAX_SIZE - total_array_len)) {
			excess_data = true;
			break;
		} else {
			total_array_len += fixed_param->rx_mcs_array_len;
		}

		if (total_array_len > (WMI_SVC_MSG_MAX_SIZE /
		    (sizeof(uint32_t) * WLAN_MAX_AC))) {
			excess_data = true;
			break;
		} else {
			total_peer_len = (sizeof(uint32_t) * WLAN_MAX_AC *
					 total_array_len) +
					 (WLAN_MAX_AC *
					 (sizeof(struct sir_wifi_tx) +
					 sizeof(struct sir_wifi_rx)));
		}
		if (total_peer_len > WMI_SVC_MSG_MAX_SIZE) {
			excess_data = true;
			break;
		}
		if (peer_num > WMI_SVC_MSG_MAX_SIZE / (total_peer_len +
		    sizeof(struct sir_wifi_ll_ext_peer_stats))) {
			excess_data = true;
			break;
		} else {
			buf_len += peer_num *
				   (sizeof(struct sir_wifi_ll_ext_peer_stats) +
				    total_peer_len);
		}
	} while (0);

	if (excess_data || (buf_len > WMI_SVC_MSG_MAX_SIZE)) {
		wma_err("excess wmi buffer: peer %d cca %d tx_mpdu %d tx_succ%d tx_fail %d tx_ppdu %d rx_mpdu %d rx_mcs %d",
			 peer_num, fixed_param->num_chan_cca_stats,
			 fixed_param->tx_mpdu_aggr_array_len,
			 fixed_param->tx_succ_mcs_array_len,
			 fixed_param->tx_fail_mcs_array_len,
			 fixed_param->tx_ppdu_delay_array_len,
			 fixed_param->rx_mpdu_aggr_array_len,
			 fixed_param->rx_mcs_array_len);
		return NULL;
	}

	buf = qdf_mem_malloc(buf_len);
	if (!buf)
		*len = 0;
	else
		*len = buf_len;

	return buf;
}

/**
 * wma_fill_tx_stats() - Fix TX stats into result buffer
 * @ll_stats: LL stats buffer
 * @fix_param: parameters with fixed length in WMI event
 * @param_buf: parameters without fixed length in WMI event
 * @buf: buffer for TLV parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_fill_tx_stats(struct sir_wifi_ll_ext_stats *ll_stats,
		  wmi_report_stats_event_fixed_param *fix_param,
		  WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf,
		  uint8_t **buf, uint32_t *buf_length)
{
	uint8_t *result;
	uint32_t i, j, k;
	wmi_peer_ac_tx_stats *wmi_peer_tx;
	wmi_tx_stats *wmi_tx;
	struct sir_wifi_tx *tx_stats;
	struct sir_wifi_ll_ext_peer_stats *peer_stats;
	uint32_t *tx_mpdu_aggr, *tx_succ_mcs, *tx_fail_mcs, *tx_delay;
	uint32_t len, dst_len, param_len, tx_mpdu_aggr_array_len,
		 tx_succ_mcs_array_len, tx_fail_mcs_array_len,
		 tx_delay_array_len;

	result = *buf;
	dst_len = *buf_length;
	tx_mpdu_aggr_array_len = fix_param->tx_mpdu_aggr_array_len;
	ll_stats->tx_mpdu_aggr_array_len = tx_mpdu_aggr_array_len;
	tx_succ_mcs_array_len = fix_param->tx_succ_mcs_array_len;
	ll_stats->tx_succ_mcs_array_len = tx_succ_mcs_array_len;
	tx_fail_mcs_array_len = fix_param->tx_fail_mcs_array_len;
	ll_stats->tx_fail_mcs_array_len = tx_fail_mcs_array_len;
	tx_delay_array_len = fix_param->tx_ppdu_delay_array_len;
	ll_stats->tx_delay_array_len = tx_delay_array_len;
	wmi_peer_tx = param_buf->peer_ac_tx_stats;
	wmi_tx = param_buf->tx_stats;

	len = fix_param->num_peer_ac_tx_stats *
		WLAN_MAX_AC * tx_mpdu_aggr_array_len * sizeof(uint32_t);
	param_len = param_buf->num_tx_mpdu_aggr * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->tx_mpdu_aggr) {
		tx_mpdu_aggr = (uint32_t *)result;
		qdf_mem_copy(tx_mpdu_aggr, param_buf->tx_mpdu_aggr, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("TX_MPDU_AGGR invalid arg, %d, %d, %d",
			len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	len = fix_param->num_peer_ac_tx_stats * WLAN_MAX_AC *
		tx_succ_mcs_array_len * sizeof(uint32_t);
	param_len = param_buf->num_tx_succ_mcs * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->tx_succ_mcs) {
		tx_succ_mcs = (uint32_t *)result;
		qdf_mem_copy(tx_succ_mcs, param_buf->tx_succ_mcs, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("TX_SUCC_MCS invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	len = fix_param->num_peer_ac_tx_stats * WLAN_MAX_AC *
		tx_fail_mcs_array_len * sizeof(uint32_t);
	param_len = param_buf->num_tx_fail_mcs * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->tx_fail_mcs) {
		tx_fail_mcs = (uint32_t *)result;
		qdf_mem_copy(tx_fail_mcs, param_buf->tx_fail_mcs, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("TX_FAIL_MCS invalid arg, %d, %d %d",
			 len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	len = fix_param->num_peer_ac_tx_stats *
		WLAN_MAX_AC * tx_delay_array_len * sizeof(uint32_t);
	param_len = param_buf->num_tx_ppdu_delay * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->tx_ppdu_delay) {
		tx_delay = (uint32_t *)result;
		qdf_mem_copy(tx_delay, param_buf->tx_ppdu_delay, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("TX_DELAY invalid arg, %d, %d, %d",
			 len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	/* per peer tx stats */
	peer_stats = ll_stats->peer_stats;
	if (!wmi_peer_tx || !wmi_tx || !peer_stats) {
		wma_err("Invalid arg, peer_tx %pK, wmi_tx %pK stats %pK",
			 wmi_peer_tx, wmi_tx, peer_stats);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < fix_param->num_peer_ac_tx_stats; i++) {
		uint32_t peer_id = wmi_peer_tx[i].peer_id;
		struct sir_wifi_tx *ac;
		wmi_tx_stats *wmi_tx_stats;

		for (j = 0; j < ll_stats->peer_num; j++) {
			peer_stats += j;
			if (peer_stats->peer_id == WIFI_INVALID_PEER_ID ||
			    peer_stats->peer_id == peer_id)
				break;
		}

		if (j < ll_stats->peer_num) {
			peer_stats->peer_id = wmi_peer_tx[i].peer_id;
			peer_stats->vdev_id = wmi_peer_tx[i].vdev_id;
			tx_stats = (struct sir_wifi_tx *)result;
			for (k = 0; k < WLAN_MAX_AC; k++) {
				wmi_tx_stats = &wmi_tx[i * WLAN_MAX_AC + k];
				ac = &tx_stats[k];
				WMA_FILL_TX_STATS(wmi_tx_stats, ac);
				ac->mpdu_aggr_size = tx_mpdu_aggr;
				ac->aggr_len = tx_mpdu_aggr_array_len *
							sizeof(uint32_t);
				ac->success_mcs_len = tx_succ_mcs_array_len *
							sizeof(uint32_t);
				ac->success_mcs = tx_succ_mcs;
				ac->fail_mcs = tx_fail_mcs;
				ac->fail_mcs_len = tx_fail_mcs_array_len *
							sizeof(uint32_t);
				ac->delay = tx_delay;
				ac->delay_len = tx_delay_array_len *
							sizeof(uint32_t);
				peer_stats->ac_stats[k].tx_stats = ac;
				peer_stats->ac_stats[k].type = k;
				tx_mpdu_aggr += tx_mpdu_aggr_array_len;
				tx_succ_mcs += tx_succ_mcs_array_len;
				tx_fail_mcs += tx_fail_mcs_array_len;
				tx_delay += tx_delay_array_len;
			}
			result += WLAN_MAX_AC * sizeof(struct sir_wifi_tx);
		} else {
			/*
			 * Buffer for Peer TX counter overflow.
			 * There is peer ID mismatch between TX, RX,
			 * signal counters.
			 */
			wma_err("One peer TX info is dropped");

			tx_mpdu_aggr += tx_mpdu_aggr_array_len * WLAN_MAX_AC;
			tx_succ_mcs += tx_succ_mcs_array_len * WLAN_MAX_AC;
			tx_fail_mcs += tx_fail_mcs_array_len * WLAN_MAX_AC;
			tx_delay += tx_delay_array_len * WLAN_MAX_AC;
		}
	}
	*buf = result;
	*buf_length = dst_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_fill_rx_stats() - Fix RX stats into result buffer
 * @ll_stats: LL stats buffer
 * @fix_param: parameters with fixed length in WMI event
 * @param_buf: parameters without fixed length in WMI event
 * @buf: buffer for TLV parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_fill_rx_stats(struct sir_wifi_ll_ext_stats *ll_stats,
		  wmi_report_stats_event_fixed_param *fix_param,
		  WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf,
		  uint8_t **buf, uint32_t *buf_length)
{
	uint8_t *result;
	uint32_t i, j, k;
	uint32_t *rx_mpdu_aggr, *rx_mcs;
	wmi_rx_stats *wmi_rx;
	wmi_peer_ac_rx_stats *wmi_peer_rx;
	struct sir_wifi_rx *rx_stats;
	struct sir_wifi_ll_ext_peer_stats *peer_stats;
	uint32_t len, dst_len, param_len,
		 rx_mpdu_aggr_array_len, rx_mcs_array_len;

	rx_mpdu_aggr_array_len = fix_param->rx_mpdu_aggr_array_len;
	ll_stats->rx_mpdu_aggr_array_len = rx_mpdu_aggr_array_len;
	rx_mcs_array_len = fix_param->rx_mcs_array_len;
	ll_stats->rx_mcs_array_len = rx_mcs_array_len;
	wmi_peer_rx = param_buf->peer_ac_rx_stats;
	wmi_rx = param_buf->rx_stats;

	result = *buf;
	dst_len = *buf_length;
	len = sizeof(uint32_t) * (fix_param->num_peer_ac_rx_stats *
				  WLAN_MAX_AC * rx_mpdu_aggr_array_len);
	param_len = param_buf->num_rx_mpdu_aggr * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->rx_mpdu_aggr) {
		rx_mpdu_aggr = (uint32_t *)result;
		qdf_mem_copy(rx_mpdu_aggr, param_buf->rx_mpdu_aggr, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("RX_MPDU_AGGR invalid arg %d, %d, %d",
			 len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	len = sizeof(uint32_t) * (fix_param->num_peer_ac_rx_stats *
				  WLAN_MAX_AC * rx_mcs_array_len);
	param_len = param_buf->num_rx_mcs * sizeof(uint32_t);
	if (len <= dst_len && len <= param_len && param_buf->rx_mcs) {
		rx_mcs = (uint32_t *)result;
		qdf_mem_copy(rx_mcs, param_buf->rx_mcs, len);
		result += len;
		dst_len -= len;
	} else {
		wma_err("RX_MCS invalid arg %d, %d, %d",
			 len, dst_len, param_len);
		return QDF_STATUS_E_FAILURE;
	}

	/* per peer rx stats */
	peer_stats = ll_stats->peer_stats;
	if (!wmi_peer_rx || !wmi_rx || !peer_stats) {
		wma_err("Invalid arg, peer_rx %pK, wmi_rx %pK stats %pK",
			 wmi_peer_rx, wmi_rx, peer_stats);
		return QDF_STATUS_E_FAILURE;
	}
	for (i = 0; i < fix_param->num_peer_ac_rx_stats; i++) {
		uint32_t peer_id = wmi_peer_rx[i].peer_id;
		struct sir_wifi_rx *ac;
		wmi_rx_stats *wmi_rx_stats;

		for (j = 0; j < ll_stats->peer_num; j++) {
			peer_stats += j;
			if ((peer_stats->peer_id == WIFI_INVALID_PEER_ID) ||
			    (peer_stats->peer_id == peer_id))
				break;
		}

		if (j < ll_stats->peer_num) {
			peer_stats->peer_id = wmi_peer_rx[i].peer_id;
			peer_stats->vdev_id = wmi_peer_rx[i].vdev_id;
			peer_stats->sta_ps_inds = wmi_peer_rx[i].sta_ps_inds;
			peer_stats->sta_ps_durs = wmi_peer_rx[i].sta_ps_durs;
			peer_stats->rx_probe_reqs =
						wmi_peer_rx[i].rx_probe_reqs;
			peer_stats->rx_oth_mgmts = wmi_peer_rx[i].rx_oth_mgmts;
			rx_stats = (struct sir_wifi_rx *)result;

			for (k = 0; k < WLAN_MAX_AC; k++) {
				wmi_rx_stats = &wmi_rx[i * WLAN_MAX_AC + k];
				ac = &rx_stats[k];
				WMA_FILL_RX_STATS(wmi_rx_stats, ac);
				ac->mpdu_aggr = rx_mpdu_aggr;
				ac->aggr_len = rx_mpdu_aggr_array_len *
							sizeof(uint32_t);
				ac->mcs = rx_mcs;
				ac->mcs_len = rx_mcs_array_len *
							sizeof(uint32_t);
				peer_stats->ac_stats[k].rx_stats = ac;
				peer_stats->ac_stats[k].type = k;
				rx_mpdu_aggr += rx_mpdu_aggr_array_len;
				rx_mcs += rx_mcs_array_len;
			}
			result += WLAN_MAX_AC * sizeof(struct sir_wifi_rx);
		} else {
			/*
			 * Buffer for Peer RX counter overflow.
			 * There is peer ID mismatch between TX, RX,
			 * signal counters.
			 */
			wma_err("One peer RX info is dropped");
			rx_mpdu_aggr += rx_mpdu_aggr_array_len * WLAN_MAX_AC;
			rx_mcs += rx_mcs_array_len * WLAN_MAX_AC;
		}
	}
	*buf = result;
	*buf_length = dst_len;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_ll_stats_evt_handler() - handler for MAC layer counters.
 * @handle - wma handle
 * @event - FW event
 * @len - length of FW event
 *
 * return: 0 success.
 */
static int wma_ll_stats_evt_handler(void *handle, u_int8_t *event,
				    u_int32_t len)
{
	WMI_REPORT_STATS_EVENTID_param_tlvs *param_buf;
	wmi_report_stats_event_fixed_param *fixed_param;
	tSirLLStatsResults *link_stats_results;
	wmi_chan_cca_stats *wmi_cca_stats;
	wmi_peer_signal_stats *wmi_peer_signal;
	struct sir_wifi_ll_ext_stats *ll_stats;
	struct sir_wifi_ll_ext_peer_stats *peer_stats;
	struct sir_wifi_chan_cca_stats *cca_stats;
	struct sir_wifi_peer_signal_stats *peer_signal;
	uint8_t *result;
	uint32_t i, peer_num, result_size, dst_len;
	struct mac_context *mac;
	struct scheduler_msg sme_msg = { 0 };
	QDF_STATUS qdf_status;

	mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_ext_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	wma_debug("Posting MAC counters event to HDD");

	param_buf = (WMI_REPORT_STATS_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_debug("param_buf is null");
		return -EINVAL;
	}
	fixed_param = param_buf->fixed_param;
	if (!fixed_param) {
		wma_debug("fixed_param is null");
		return -EINVAL;
	}
	wmi_cca_stats = param_buf->chan_cca_stats;
	wmi_peer_signal = param_buf->peer_signal_stats;
	if (fixed_param->num_peer_signal_stats >
		param_buf->num_peer_signal_stats ||
		fixed_param->num_peer_ac_tx_stats >
		param_buf->num_peer_ac_tx_stats ||
		fixed_param->num_peer_ac_rx_stats >
		param_buf->num_peer_ac_rx_stats) {
		wma_err("excess num_peer_signal_stats:%d, num_peer_ac_tx_stats:%d, num_peer_ac_rx_stats:%d",
			fixed_param->num_peer_signal_stats,
			fixed_param->num_peer_ac_tx_stats,
			fixed_param->num_peer_ac_rx_stats);
		return -EINVAL;
	}

	/* Get the MAX of three peer numbers */
	peer_num = fixed_param->num_peer_signal_stats >
			fixed_param->num_peer_ac_tx_stats ?
			fixed_param->num_peer_signal_stats :
			fixed_param->num_peer_ac_tx_stats;
	peer_num = peer_num > fixed_param->num_peer_ac_rx_stats ?
			peer_num : fixed_param->num_peer_ac_rx_stats;

	if (peer_num == 0)
		return -EINVAL;

	link_stats_results = wma_get_ll_stats_ext_buf(&result_size,
						      peer_num,
						      fixed_param);
	if (!link_stats_results) {
		wma_err("Fail to allocate stats buffer");
		return -EINVAL;
	}
	link_stats_results->paramId = WMI_LL_STATS_EXT_MAC_COUNTER;
	link_stats_results->num_peers = peer_num;
	link_stats_results->peer_event_number = 1;
	link_stats_results->moreResultToFollow = 0;

	ll_stats = (struct sir_wifi_ll_ext_stats *)link_stats_results->results;
	ll_stats->trigger_cond_id = fixed_param->trigger_cond_id;
	ll_stats->cca_chgd_bitmap = fixed_param->cca_chgd_bitmap;
	ll_stats->sig_chgd_bitmap = fixed_param->sig_chgd_bitmap;
	ll_stats->tx_chgd_bitmap = fixed_param->tx_chgd_bitmap;
	ll_stats->rx_chgd_bitmap = fixed_param->rx_chgd_bitmap;
	ll_stats->channel_num = fixed_param->num_chan_cca_stats;
	ll_stats->peer_num = peer_num;

	result = (uint8_t *)ll_stats->stats;
	if (!result) {
		wma_err("result is null");
		qdf_mem_free(link_stats_results);
		return -EINVAL;
	}
	peer_stats = (struct sir_wifi_ll_ext_peer_stats *)result;
	ll_stats->peer_stats = peer_stats;

	for (i = 0; i < peer_num && peer_stats; i++) {
		peer_stats[i].peer_id = WIFI_INVALID_PEER_ID;
		peer_stats[i].vdev_id = WIFI_INVALID_VDEV_ID;
	}

	/* Per peer signal */
	result_size -= sizeof(struct sir_wifi_ll_ext_stats);
	dst_len = sizeof(struct sir_wifi_peer_signal_stats);
	for (i = 0;
	     i < fixed_param->num_peer_signal_stats &&
	     peer_stats && wmi_peer_signal;
	     i++) {
		peer_stats[i].peer_id = wmi_peer_signal->peer_id;
		peer_stats[i].vdev_id = wmi_peer_signal->vdev_id;
		peer_signal = &peer_stats[i].peer_signal_stats;

		wma_debug("%d antennas for peer %d",
			 wmi_peer_signal->num_chains_valid,
			 wmi_peer_signal->peer_id);
		if (dst_len <= result_size && peer_signal) {
			peer_signal->vdev_id = wmi_peer_signal->vdev_id;
			peer_signal->peer_id = wmi_peer_signal->peer_id;
			peer_signal->num_chain =
					wmi_peer_signal->num_chains_valid;
			qdf_mem_copy(peer_signal->per_ant_snr,
				     wmi_peer_signal->per_chain_snr,
				     sizeof(peer_signal->per_ant_snr));
			qdf_mem_copy(peer_signal->nf,
				     wmi_peer_signal->per_chain_nf,
				     sizeof(peer_signal->nf));
			qdf_mem_copy(peer_signal->per_ant_rx_mpdus,
				     wmi_peer_signal->per_antenna_rx_mpdus,
				     sizeof(peer_signal->per_ant_rx_mpdus));
			qdf_mem_copy(peer_signal->per_ant_tx_mpdus,
				     wmi_peer_signal->per_antenna_tx_mpdus,
				     sizeof(peer_signal->per_ant_tx_mpdus));
			result_size -= dst_len;
		} else {
			wma_err("Invalid length of PEER signal");
		}
		wmi_peer_signal++;
	}

	result += peer_num * sizeof(struct sir_wifi_ll_ext_peer_stats);
	cca_stats = (struct sir_wifi_chan_cca_stats *)result;
	ll_stats->cca = cca_stats;
	dst_len = sizeof(*cca_stats);
	for (i = 0;
	     i < ll_stats->channel_num && cca_stats && wmi_cca_stats;
	     i++) {
		if (dst_len <= result_size) {
			cca_stats->vdev_id = wmi_cca_stats->vdev_id;
			cca_stats->idle_time = wmi_cca_stats->idle_time;
			cca_stats->tx_time = wmi_cca_stats->tx_time;
			cca_stats->rx_in_bss_time =
				wmi_cca_stats->rx_in_bss_time;
			cca_stats->rx_out_bss_time =
				wmi_cca_stats->rx_out_bss_time;
			cca_stats->rx_busy_time = wmi_cca_stats->rx_busy_time;
			cca_stats->rx_in_bad_cond_time =
				wmi_cca_stats->rx_in_bad_cond_time;
			cca_stats->tx_in_bad_cond_time =
				wmi_cca_stats->tx_in_bad_cond_time;
			cca_stats->wlan_not_avail_time =
				wmi_cca_stats->wlan_not_avail_time;
			result_size -= dst_len;
		} else {
			wma_err("Invalid length of CCA");
		}
		cca_stats++;
	}

	result += i * sizeof(struct sir_wifi_chan_cca_stats);
	qdf_status = wma_fill_tx_stats(ll_stats, fixed_param, param_buf,
				       &result, &result_size);
	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		qdf_status = wma_fill_rx_stats(ll_stats, fixed_param, param_buf,
					       &result, &result_size);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		sme_msg.type = eWMI_SME_LL_STATS_IND;
		sme_msg.bodyptr = (void *)link_stats_results;
		sme_msg.bodyval = 0;
		qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
						    QDF_MODULE_ID_SME,
						    QDF_MODULE_ID_SME,
						    &sme_msg);
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		qdf_mem_free(link_stats_results);
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_unified_link_peer_stats_event_handler() - peer stats event handler
 * @handle:          wma handle
 * @cmd_param_info:  data received with event from fw
 * @len:             length of data
 *
 * Return: 0 for success or error code
 */
static int wma_unified_link_peer_stats_event_handler(void *handle,
						     uint8_t *cmd_param_info,
						     uint32_t len)
{
	WMI_PEER_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_peer_stats_event_fixed_param *fixed_param;
	wmi_peer_link_stats *peer_stats, *temp_peer_stats;
	wmi_rate_stats *rate_stats;
	tSirLLStatsResults *link_stats_results;
	uint8_t *results, *t_peer_stats, *t_rate_stats;
	uint32_t count, rate_cnt;
	uint32_t total_num_rates = 0;
	uint32_t next_res_offset, next_peer_offset, next_rate_offset;
	size_t peer_info_size, peer_stats_size, rate_stats_size;
	size_t link_stats_results_size;
	bool excess_data = false;
	uint32_t buf_len = 0;
	struct cdp_peer_stats *dp_stats;
	void *dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;
	uint8_t mcs_index;

	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	param_tlvs = (WMI_PEER_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}
	/*
	 * cmd_param_info contains
	 * wmi_peer_stats_event_fixed_param fixed_param;
	 * num_peers * size of(struct wmi_peer_link_stats)
	 * total_num_rates * size of(struct wmi_rate_stats)
	 * total_num_rates is the sum of the rates of all the peers.
	 */
	fixed_param = param_tlvs->fixed_param;
	peer_stats = param_tlvs->peer_stats;
	rate_stats = param_tlvs->peer_rate_stats;

	if (!fixed_param || !peer_stats ||
	    (peer_stats->num_rates && !rate_stats)) {
		wma_err("Invalid param_tlvs for Peer Stats");
		return -EINVAL;
	}

	do {
		if (fixed_param->num_peers >
		    WMI_SVC_MSG_MAX_SIZE/sizeof(wmi_peer_link_stats) ||
		    fixed_param->num_peers > param_tlvs->num_peer_stats) {
			excess_data = true;
			break;
		} else {
			buf_len = fixed_param->num_peers *
				sizeof(wmi_peer_link_stats);
		}
		temp_peer_stats = (wmi_peer_link_stats *) peer_stats;
		for (count = 0; count < fixed_param->num_peers; count++) {
			if (temp_peer_stats->num_rates >
			    WMI_SVC_MSG_MAX_SIZE / sizeof(wmi_rate_stats)) {
				excess_data = true;
				break;
			} else {
				total_num_rates += temp_peer_stats->num_rates;
				if (total_num_rates >
				    WMI_SVC_MSG_MAX_SIZE /
				    sizeof(wmi_rate_stats) || total_num_rates >
				    param_tlvs->num_peer_rate_stats) {
					excess_data = true;
					break;
				}
				buf_len += temp_peer_stats->num_rates *
					sizeof(wmi_rate_stats);
			}
			temp_peer_stats++;
		}
	} while (0);

	if (excess_data ||
	    (buf_len > WMI_SVC_MSG_MAX_SIZE - sizeof(*fixed_param))) {
		wma_err("excess wmi buffer: rates:%d, peers:%d",
			peer_stats->num_rates, fixed_param->num_peers);
		return -EINVAL;
	}

	peer_stats_size = sizeof(struct wifi_peer_stat);
	peer_info_size = sizeof(struct wifi_peer_info);
	rate_stats_size = sizeof(struct wifi_rate_stat);
	link_stats_results_size =
		sizeof(*link_stats_results) + peer_stats_size +
		(fixed_param->num_peers * peer_info_size) +
		(total_num_rates * rate_stats_size);

	link_stats_results = qdf_mem_malloc(link_stats_results_size);
	if (!link_stats_results)
		return -ENOMEM;

	qdf_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId = WMI_LINK_STATS_ALL_PEER;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = 0;
	link_stats_results->num_peers = fixed_param->num_peers;
	link_stats_results->peer_event_number = fixed_param->peer_event_number;
	link_stats_results->moreResultToFollow = fixed_param->more_data;

	qdf_mem_copy(link_stats_results->results,
		     &fixed_param->num_peers, peer_stats_size);
	dp_stats = qdf_mem_malloc(sizeof(*dp_stats));
	if (!dp_stats) {
		qdf_mem_free(link_stats_results);
		return -ENOMEM;
	}

	results = (uint8_t *) link_stats_results->results;
	t_peer_stats = (uint8_t *) peer_stats;
	t_rate_stats = (uint8_t *) rate_stats;
	next_res_offset = peer_stats_size;
	next_peer_offset = WMI_TLV_HDR_SIZE;
	next_rate_offset = WMI_TLV_HDR_SIZE;
	for (rate_cnt = 0; rate_cnt < fixed_param->num_peers; rate_cnt++) {
		qdf_mem_copy(results + next_res_offset,
			     t_peer_stats + next_peer_offset, peer_info_size);
		next_res_offset += peer_info_size;

		status = cdp_host_get_peer_stats(dp_soc,
				       link_stats_results->ifaceId,
				       (uint8_t *)&peer_stats->peer_mac_address,
				       dp_stats);

		/* Copy rate stats associated with this peer */
		for (count = 0; count < peer_stats->num_rates; count++) {
			mcs_index = RATE_STAT_GET_MCS_INDEX(rate_stats->rate);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				if (mcs_index < MAX_MCS)
					rate_stats->rx_mpdu =
					    dp_stats->rx.rx_mpdu_cnt[mcs_index];
				else
					rate_stats->rx_mpdu = 0;
			}
			rate_stats++;

			qdf_mem_copy(results + next_res_offset,
				     t_rate_stats + next_rate_offset,
				     rate_stats_size);
			next_res_offset += rate_stats_size;
			next_rate_offset += sizeof(*rate_stats);
		}
		next_peer_offset += sizeof(*peer_stats);
		peer_stats++;
	}

	qdf_mem_free(dp_stats);
	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	mac->sme.link_layer_stats_cb(mac->hdd_handle,
				     WMA_LINK_LAYER_STATS_RESULTS_RSP,
				     link_stats_results,
				     mac->sme.ll_stats_context);
	qdf_mem_free(link_stats_results);

	return 0;
}

/**
 * wma_unified_link_stats_results_mem_free() - Free link stats results memory
 * #link_stats_results: pointer to link stats result
 *
 * Return: 0 on success, error number otherwise.
 */
void wma_unified_link_stats_results_mem_free(
			tSirLLStatsResults *link_stats_results)
{
	struct wifi_radio_stats *rs_results;
	uint32_t i = 0;

	if (!link_stats_results)
		return;

	rs_results = (struct wifi_radio_stats *)
				&link_stats_results->results[0];
	for (i = 0; i < link_stats_results->num_radio; i++) {
		if (rs_results->tx_time_per_power_level) {
			qdf_mem_free(rs_results->tx_time_per_power_level);
			rs_results->tx_time_per_power_level = NULL;
		}

		if (rs_results->channels) {
			qdf_mem_free(rs_results->channels);
			rs_results->channels = NULL;
		}
		rs_results++;
	}
}

/**
 * wma_unified_radio_tx_mem_free() - Free radio tx power stats memory
 * @handle: WMI handle
 *
 * Return: 0 on success, error number otherwise.
 */
int wma_unified_radio_tx_mem_free(void *handle)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle->link_stats_results)
		return 0;

	wma_unified_link_stats_results_mem_free(wma_handle->link_stats_results);

	qdf_mem_free(wma_handle->link_stats_results);
	wma_handle->link_stats_results = NULL;

	return 0;
}

/**
 * wma_unified_radio_tx_power_level_stats_event_handler() - tx power level stats
 * @handle: WMI handle
 * @cmd_param_info: command param info
 * @len: Length of @cmd_param_info
 *
 * This is the WMI event handler function to receive radio stats tx
 * power level stats.
 *
 * Return: 0 on success, error number otherwise.
 */
static int wma_unified_radio_tx_power_level_stats_event_handler(void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_tx_power_level_stats_evt_fixed_param *fixed_param;
	uint8_t *tx_power_level_values;
	tSirLLStatsResults *link_stats_results;
	struct wifi_radio_stats *rs_results;
	uint32_t max_total_num_tx_power_levels = MAX_TPC_LEVELS * NUM_OF_BANDS *
						MAX_SPATIAL_STREAM_ANY_V3;

	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	param_tlvs = (WMI_RADIO_TX_POWER_LEVEL_STATS_EVENTID_param_tlvs *)
								cmd_param_info;
	if (!param_tlvs) {
		wma_err("Invalid tx power level stats event");
		return -EINVAL;
	}

	fixed_param = param_tlvs->fixed_param;
	if (!fixed_param) {
		wma_err("Invalid param_tlvs for Radio tx_power level Stats");
		return -EINVAL;
	}

	link_stats_results = wma_handle->link_stats_results;
	if (!link_stats_results) {
		wma_err("link_stats_results is NULL");
		return -EINVAL;
	}

	if (fixed_param->num_tx_power_levels > ((WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*fixed_param)) / sizeof(uint32_t)) ||
	    fixed_param->num_tx_power_levels >
	    param_tlvs->num_tx_time_per_power_level) {
		wma_err("excess tx_power buffers:%d, num_tx_time_per_power_level:%d",
			fixed_param->num_tx_power_levels,
			param_tlvs->num_tx_time_per_power_level);
		return -EINVAL;
	}

	if (fixed_param->radio_id >= link_stats_results->num_radio) {
		wma_err("Invalid radio_id %d num_radio %d",
			 fixed_param->radio_id,
			 link_stats_results->num_radio);
		return -EINVAL;
	}

	if (fixed_param->total_num_tx_power_levels >
	    max_total_num_tx_power_levels) {
		wma_debug("Invalid total_num_tx_power_levels %d",
			 fixed_param->total_num_tx_power_levels);
		return -EINVAL;
	}

	rs_results = (struct wifi_radio_stats *) &link_stats_results->results[0] +
							 fixed_param->radio_id;
	tx_power_level_values = (uint8_t *) param_tlvs->tx_time_per_power_level;

	if (rs_results->total_num_tx_power_levels &&
	    fixed_param->total_num_tx_power_levels >
		rs_results->total_num_tx_power_levels) {
		wma_err("excess tx_power buffers:%d, total_num_tx_power_levels:%d",
			 fixed_param->total_num_tx_power_levels,
			 rs_results->total_num_tx_power_levels);
		return -EINVAL;
	}

	rs_results->total_num_tx_power_levels =
				fixed_param->total_num_tx_power_levels;
	if (!rs_results->total_num_tx_power_levels) {
		link_stats_results->nr_received++;
		goto post_stats;
	}

	if ((fixed_param->power_level_offset >
	    rs_results->total_num_tx_power_levels) ||
	    (fixed_param->num_tx_power_levels >
	    rs_results->total_num_tx_power_levels -
	    fixed_param->power_level_offset)) {
		wma_err("Invalid offset %d total_num %d num %d",
			 fixed_param->power_level_offset,
			 rs_results->total_num_tx_power_levels,
			 fixed_param->num_tx_power_levels);
		return -EINVAL;
	}

	if (rs_results->tx_time_per_power_level) {
		qdf_mem_free(rs_results->tx_time_per_power_level);
		rs_results->tx_time_per_power_level = NULL;
	}

	rs_results->tx_time_per_power_level =
		qdf_mem_malloc(sizeof(uint32_t) *
			       rs_results->total_num_tx_power_levels);
	if (!rs_results->tx_time_per_power_level) {
		/* In error case, atleast send the radio stats without
		 * tx_power_level stats
		 */
		rs_results->total_num_tx_power_levels = 0;
		link_stats_results->nr_received++;
		goto post_stats;
	}

	qdf_mem_copy(&rs_results->tx_time_per_power_level[
					fixed_param->power_level_offset],
		tx_power_level_values,
		sizeof(uint32_t) * fixed_param->num_tx_power_levels);
	if (rs_results->total_num_tx_power_levels ==
	   (fixed_param->num_tx_power_levels +
					 fixed_param->power_level_offset)) {
		link_stats_results->moreResultToFollow = 0;
		link_stats_results->nr_received++;
	}
	wma_debug("num tx pwr lvls %u num tx pwr lvls %u pwr lvl offset %u radio_id %u moretofollow: %u nr_received: %u",
		 fixed_param->total_num_tx_power_levels,
		 fixed_param->num_tx_power_levels,
		 fixed_param->power_level_offset, fixed_param->radio_id,
		 link_stats_results->moreResultToFollow,
		 link_stats_results->nr_received);

	/* If still data to receive, return from here */
	if (link_stats_results->moreResultToFollow)
		return 0;

post_stats:
	if (link_stats_results->num_radio != link_stats_results->nr_received) {
		/* Not received all radio stats yet, don't post yet */
		return 0;
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	mac->sme.link_layer_stats_cb(mac->hdd_handle,
		WMA_LINK_LAYER_STATS_RESULTS_RSP,
		link_stats_results,
		mac->sme.ll_stats_context);
	wma_unified_radio_tx_mem_free(handle);

	return 0;
}

static int wma_copy_chan_stats(uint32_t num_chan,
			       struct wifi_channel_stats *channels,
			       struct wifi_radio_stats *rs_results)
{
	uint32_t num_chan_in_prev_event = rs_results->num_channels;
	struct wifi_channel_stats *channels_in_prev_event =
							rs_results->channels;
	if (!rs_results->channels) {
		/* It means this is the first event for this radio */
		rs_results->num_channels = num_chan;
		rs_results->channels = channels;
		return 0;
	}
	if (rs_results->num_channels + num_chan > NUM_CHANNELS) {
		wma_err("total chan stats num unexpected %d new %d",
			rs_results->num_channels, num_chan);
		/* do not add more */
		qdf_mem_free(channels);
		return 0;
	}

	rs_results->num_channels += num_chan;
	rs_results->channels = qdf_mem_malloc(rs_results->num_channels *
					      sizeof(*channels));
	if (!rs_results->channels) {
		qdf_mem_free(channels);
		qdf_mem_free(channels_in_prev_event);
		return -ENOMEM;
	}

	/* copy the previous event's information */
	qdf_mem_copy(rs_results->channels, channels_in_prev_event,
		     num_chan_in_prev_event * sizeof(*channels_in_prev_event));

	/* copy the current event's information */
	qdf_mem_copy(rs_results->channels + num_chan_in_prev_event, channels,
		     num_chan * sizeof(*channels));

	qdf_mem_free(channels);
	qdf_mem_free(channels_in_prev_event);
	return 0;
}

/**
 * wma_unified_link_radio_stats_event_handler() - radio link stats event handler
 * @handle:          wma handle
 * @cmd_param_info:  data received with event from fw
 * @len:             length of data
 *
 * Return: 0 for success or error code
 */
static int wma_unified_link_radio_stats_event_handler(void *handle,
						      uint8_t *cmd_param_info,
						      uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_radio_link_stats_event_fixed_param *fixed_param;
	wmi_radio_link_stats *radio_stats;
	wmi_channel_stats *channel_stats;
	tSirLLStatsResults *link_stats_results;
	uint8_t *results, *t_radio_stats, *t_channel_stats;
	uint32_t next_chan_offset, count;
	uint32_t num_chan_in_this_event = 0;
	size_t radio_stats_size, chan_stats_size;
	size_t link_stats_results_size;
	struct wifi_radio_stats *rs_results = NULL;
	struct wifi_channel_stats *chn_results;
	struct wifi_channel_stats *channels_in_this_event;
	int32_t status;

	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	param_tlvs = (WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_radio_link_stats_event_fixed_param fixed_param;
	 * size of(struct wmi_radio_link_stats);
	 * num_channels * size of(struct wmi_channel_stats)
	 */
	fixed_param = param_tlvs->fixed_param;
	if (fixed_param && !fixed_param->num_radio &&
	    !fixed_param->more_radio_events) {
		wma_debug("FW indicates dummy link radio stats");
		if (!wma_handle->link_stats_results) {
			wma_handle->link_stats_results = qdf_mem_malloc(
						sizeof(*link_stats_results));
			if (!wma_handle->link_stats_results)
				return -ENOMEM;
		}

		/*
		 * Free the already allocated memory, if any, before setting
		 * the num_radio to 0
		 */
		wma_unified_link_stats_results_mem_free(
					wma_handle->link_stats_results);

		link_stats_results = wma_handle->link_stats_results;
		link_stats_results->num_radio = fixed_param->num_radio;
		goto link_radio_stats_cb;
	}

	radio_stats = param_tlvs->radio_stats;
	channel_stats = param_tlvs->channel_stats;

	if (!fixed_param || !radio_stats ||
	    (radio_stats->num_channels && !channel_stats)) {
		wma_err("Invalid param_tlvs for Radio Stats");
		return -EINVAL;
	}
	if (radio_stats->num_channels > NUM_CHANNELS ||
	    radio_stats->num_channels > param_tlvs->num_channel_stats) {
		wma_err("Too many channels %d", radio_stats->num_channels);
		return -EINVAL;
	}

	radio_stats_size = sizeof(struct wifi_radio_stats);
	chan_stats_size = sizeof(struct wifi_channel_stats);
	if (fixed_param->num_radio >
		(UINT_MAX - sizeof(*link_stats_results))/radio_stats_size) {
		wma_err("excess num_radio %d is leading to int overflow",
			fixed_param->num_radio);
		return -EINVAL;
	}
	link_stats_results_size = sizeof(*link_stats_results) +
				  fixed_param->num_radio * radio_stats_size;

	if (radio_stats->radio_id >= fixed_param->num_radio) {
		wma_err("invalid radio id:%d, num radio:%d",
			radio_stats->radio_id,
			fixed_param->num_radio);
		return -EINVAL;
	}

	if (!wma_handle->link_stats_results) {
		wma_handle->link_stats_results = qdf_mem_malloc(
						link_stats_results_size);
		if (!wma_handle->link_stats_results)
			return -ENOMEM;
	}
	link_stats_results = wma_handle->link_stats_results;
	if (link_stats_results->num_radio == 0) {
		link_stats_results->num_radio = fixed_param->num_radio;
	} else if (link_stats_results->num_radio < fixed_param->num_radio) {
		/*
		 * The link stats results size allocated based on num_radio of
		 * first event must be same as following events. Otherwise these
		 * events may be spoofed. Drop all of them and report error.
		 */
		wma_err("Invalid following WMI_RADIO_LINK_STATS_EVENTID. Discarding this set");
		wma_unified_radio_tx_mem_free(handle);
		return -EINVAL;
	}

	wma_debug("Radio stats Fixed Param: req_id: %u num_radio: %u more_radio_events: %u more_channels %u",
		  fixed_param->request_id, fixed_param->num_radio,
		  fixed_param->more_radio_events, fixed_param->more_channels);

	results = (uint8_t *) link_stats_results->results;
	t_radio_stats = (uint8_t *) radio_stats;
	t_channel_stats = (uint8_t *) channel_stats;

	rs_results = (struct wifi_radio_stats *) &results[0] + radio_stats->radio_id;

	/*
	 * If more channels is true, means this is the second event for the
	 * same radio so no need to process radio stats again as the second
	 * event will only contain remaining channel stats.
	 */
	if (!rs_results->more_channels) {
		rs_results->radio = radio_stats->radio_id;
		rs_results->on_time = radio_stats->on_time;
		rs_results->tx_time = radio_stats->tx_time;
		rs_results->rx_time = radio_stats->rx_time;
		rs_results->on_time_scan = radio_stats->on_time_scan;
		rs_results->on_time_nbd = radio_stats->on_time_nbd;
		rs_results->on_time_gscan = radio_stats->on_time_gscan;
		rs_results->on_time_roam_scan = radio_stats->on_time_roam_scan;
		rs_results->on_time_pno_scan = radio_stats->on_time_pno_scan;
		rs_results->on_time_hs20 = radio_stats->on_time_hs20;
		rs_results->on_time_host_scan = radio_stats->on_time_host_scan;
		rs_results->on_time_lpi_scan = radio_stats->on_time_lpi_scan;
		if (rs_results->channels) {
			qdf_mem_free(rs_results->channels);
			rs_results->channels = NULL;
		}
	}

	rs_results->total_num_tx_power_levels = 0;
	if (rs_results->tx_time_per_power_level) {
		qdf_mem_free(rs_results->tx_time_per_power_level);
		rs_results->tx_time_per_power_level = NULL;
	}

	rs_results->more_channels = fixed_param->more_channels;
	num_chan_in_this_event = radio_stats->num_channels;

	if (num_chan_in_this_event) {
		channels_in_this_event = qdf_mem_malloc(
					radio_stats->num_channels *
					chan_stats_size);
		if (!channels_in_this_event) {
			wma_unified_radio_tx_mem_free(handle);
			return -ENOMEM;
		}

		chn_results =
			(struct wifi_channel_stats *)&channels_in_this_event[0];
		next_chan_offset = WMI_TLV_HDR_SIZE;
		wma_debug("Channel Stats Info, radio id %d",
			  radio_stats->radio_id);
		for (count = 0; count < radio_stats->num_channels; count++) {
			wma_nofl_debug("freq %u width %u freq0 %u freq1 %u awake time %u cca busy time %u",
				       channel_stats->center_freq,
				       channel_stats->channel_width,
				       channel_stats->center_freq0,
				       channel_stats->center_freq1,
				       channel_stats->radio_awake_time,
				       channel_stats->cca_busy_time);
			if (wmi_service_enabled(
			      wma_handle->wmi_handle,
			      wmi_service_ll_stats_per_chan_rx_tx_time)) {
				wma_nofl_debug("tx time %u rx time %u",
					       channel_stats->tx_time,
					       channel_stats->rx_time);
			} else {
				wma_nofl_debug("LL Stats per channel tx time and rx time are not supported.");
			}

			channel_stats++;

			qdf_mem_copy(chn_results,
				     t_channel_stats + next_chan_offset,
				     chan_stats_size);
			chn_results++;
			next_chan_offset += sizeof(*channel_stats);
		}

		status = wma_copy_chan_stats(num_chan_in_this_event,
					     channels_in_this_event,
					     rs_results);
		if (status) {
			wma_unified_radio_tx_mem_free(handle);
			return status;
		}
	}

link_radio_stats_cb:
	link_stats_results->paramId = WMI_LINK_STATS_RADIO;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = 0;
	link_stats_results->peer_event_number = 0;

	/*
	 * Backward compatibility:
	 * There are firmware(s) which will send Radio stats only with
	 * more_radio_events set to 0 and firmware which sends Radio stats
	 * followed by tx_power level stats with more_radio_events set to 1.
	 * if more_radio_events is set to 1, buffer the radio stats and
	 * wait for tx_power_level stats.
	 */
	link_stats_results->moreResultToFollow = fixed_param->more_radio_events;

	if ((rs_results && rs_results->more_channels) ||
	    link_stats_results->moreResultToFollow) {
		/* More results coming, don't post yet */
		return 0;
	}
	if (link_stats_results->num_radio) {
		link_stats_results->nr_received++;

		if (link_stats_results->num_radio !=
		    link_stats_results->nr_received) {
			/* Not received all radio stats yet, don't post yet */
			return 0;
		}
	}

	mac->sme.link_layer_stats_cb(mac->hdd_handle,
				     WMA_LINK_LAYER_STATS_RESULTS_RSP,
				     link_stats_results,
				     mac->sme.ll_stats_context);
	wma_unified_radio_tx_mem_free(handle);

	return 0;
}

#ifdef WLAN_PEER_PS_NOTIFICATION
/**
 * wma_peer_ps_evt_handler() - handler for PEER power state change.
 * @handle: wma handle
 * @event: FW event
 * @len: length of FW event
 *
 * Once peer STA power state changes, an event will be indicated by
 * FW. This function send a link layer state change msg to HDD. HDD
 * link layer callback will converts the event to NL msg.
 *
 * Return: 0 Success. Others fail.
 */
static int wma_peer_ps_evt_handler(void *handle, u_int8_t *event,
				   u_int32_t len)
{
	WMI_PEER_STA_PS_STATECHG_EVENTID_param_tlvs *param_buf;
	wmi_peer_sta_ps_statechange_event_fixed_param *fixed_param;
	struct wifi_peer_stat *peer_stat;
	struct wifi_peer_info *peer_info;
	tSirLLStatsResults *link_stats_results;
	tSirMacAddr mac_address;
	uint32_t result_len;
	cds_msg_t sme_msg = { 0 };
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_ext_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	wma_debug("Posting Peer Stats PS event to HDD");

	param_buf = (WMI_PEER_STA_PS_STATECHG_EVENTID_param_tlvs *)event;
	fixed_param = param_buf->fixed_param;

	result_len = sizeof(tSirLLStatsResults) +
			sizeof(struct wifi_peer_stat) +
			sizeof(struct wifi_peer_info);
	link_stats_results = qdf_mem_malloc(result_len);
	if (!link_stats_results)
		return -EINVAL;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&fixed_param->peer_macaddr, &mac_address[0]);
	wma_debug("Peer power state change event from FW");
	wma_debug("Fixed Param:");
	wma_nofl_debug("MAC address: %2x:%2x:%2x:%2x:%2x:%2x, Power state: %d",
		 mac_address[0], mac_address[1], mac_address[2],
		 mac_address[3], mac_address[4], mac_address[5],
		 fixed_param->peer_ps_state);

	link_stats_results->paramId            = WMI_LL_STATS_EXT_PS_CHG;
	link_stats_results->num_peers          = 1;
	link_stats_results->peer_event_number  = 1;
	link_stats_results->moreResultToFollow = 0;

	peer_stat = (struct wifi_peer_stat *)link_stats_results->results;
	peer_stat->numPeers = 1;
	peer_info = (struct wifi_peer_info *)peer_stat->peer_info;
	qdf_mem_copy(&peer_info->peer_macaddr,
		     &mac_address,
		     sizeof(tSirMacAddr));
	peer_info->power_saving = fixed_param->peer_ps_state;

	sme_msg.type = eWMI_SME_LL_STATS_IND;
	sme_msg.bodyptr = link_stats_results;
	sme_msg.bodyval = 0;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Fail to post ps change ind msg");
		qdf_mem_free(link_stats_results);
	}

	return 0;
}
#else
/**
 * wma_peer_ps_evt_handler() - handler for PEER power state change.
 * @handle: wma handle
 * @event: FW event
 * @len: length of FW event
 *
 * Once peer STA power state changes, an event will be indicated by
 * FW. This function send a link layer state change msg to HDD. HDD
 * link layer callback will converts the event to NL msg.
 *
 * Return: 0 Success. Others fail.
 */
static inline int wma_peer_ps_evt_handler(void *handle, u_int8_t *event,
					  u_int32_t len)
{
	return 0;
}
#endif

/**
 * wma_register_ll_stats_event_handler() - register link layer stats related
 *                                         event handler
 * @wma_handle: wma handle
 *
 * Return: none
 */
void wma_register_ll_stats_event_handler(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_iface_link_stats_event_id,
				wma_unified_link_iface_stats_event_handler,
				WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_peer_link_stats_event_id,
				wma_unified_link_peer_stats_event_handler,
				WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_radio_link_stats_link,
				wma_unified_link_radio_stats_event_handler,
				WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_radio_tx_power_level_stats_event_id,
			wma_unified_radio_tx_power_level_stats_event_handler,
			WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_sta_ps_statechg_event_id,
					   wma_peer_ps_evt_handler,
					   WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_report_stats_event_id,
					   wma_ll_stats_evt_handler,
					   WMA_RX_WORK_CTX);

}

QDF_STATUS wma_process_ll_stats_clear_req(tp_wma_handle wma,
				 const tpSirLLStatsClearReq clearReq)
{
	uint8_t *addr;
	struct ll_stats_clear_params cmd = {0};
	int ret;
	struct wlan_objmgr_vdev *vdev;

	if (!clearReq || !wma) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wma->interfaces[clearReq->staId].vdev;
	if (!vdev) {
		wma_err("vdev is NULL for vdev_%d", clearReq->staId);
		return QDF_STATUS_E_FAILURE;
	}

	cmd.stop_req = clearReq->stopReq;
	cmd.vdev_id = clearReq->staId;
	cmd.stats_clear_mask = clearReq->statsClearReqMask;

	vdev = wma->interfaces[clearReq->staId].vdev;
	if (!vdev) {
		wma_err("Failed to get vdev for vdev_%d", clearReq->staId);
		return QDF_STATUS_E_FAILURE;
	}
	addr = wlan_vdev_mlme_get_macaddr(vdev);
	if (!addr) {
		wma_err("Failed to get macaddr for vdev_%d", clearReq->staId);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(cmd.peer_macaddr.bytes, addr, QDF_MAC_ADDR_SIZE);
	ret = wmi_unified_process_ll_stats_clear_cmd(wma->wmi_handle, &cmd);
	if (ret) {
		wma_err("Failed to send clear link stats req");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_ll_stats_set_req() - link layer stats set request
 * @wma:       wma handle
 * @setReq:  ll stats set request command params
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_process_ll_stats_set_req(tp_wma_handle wma,
				 const tpSirLLStatsSetReq setReq)
{
	struct ll_stats_set_params cmd = {0};
	int ret;

	if (!setReq || !wma) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cmd.mpdu_size_threshold = setReq->mpduSizeThreshold;
	cmd.aggressive_statistics_gathering =
		setReq->aggressiveStatisticsGathering;

	ret = wmi_unified_process_ll_stats_set_cmd(wma->wmi_handle,
					 &cmd);
	if (ret) {
		wma_err("Failed to send set link stats request");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
static QDF_STATUS
wma_send_ll_stats_get_cmd(tp_wma_handle wma_handle,
			  struct ll_stats_get_params *cmd)
{
	if (!(cfg_get(wma_handle->psoc, CFG_CLUB_LL_STA_AND_GET_STATION) &&
	      wmi_service_enabled(wma_handle->wmi_handle,
				  wmi_service_get_station_in_ll_stats_req) &&
	      wma_handle->interfaces[cmd->vdev_id].type == WMI_VDEV_TYPE_STA))
		return wmi_unified_process_ll_stats_get_cmd(
						wma_handle->wmi_handle, cmd);

	return wmi_process_unified_ll_stats_get_sta_cmd(wma_handle->wmi_handle,
							cmd);
}
#else
static QDF_STATUS
wma_send_ll_stats_get_cmd(tp_wma_handle wma_handle,
			  struct ll_stats_get_params *cmd)
{
	return wmi_unified_process_ll_stats_get_cmd(wma_handle->wmi_handle,
						    cmd);
}
#endif

QDF_STATUS wma_process_ll_stats_get_req(tp_wma_handle wma,
				 const tpSirLLStatsGetReq getReq)
{
	struct wlan_objmgr_vdev *vdev;
	uint8_t *addr;
	struct ll_stats_get_params cmd = {0};
	int ret;

	if (!getReq || !wma) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wma_is_vdev_valid(getReq->staId)) {
		wma_err("vdev:%d not created yet", getReq->staId);
		return QDF_STATUS_E_FAILURE;
	}

	cmd.req_id = getReq->reqId;
	cmd.param_id_mask = getReq->paramIdMask;
	cmd.vdev_id = getReq->staId;

	vdev = wma->interfaces[getReq->staId].vdev;
	if (!vdev) {
		wma_err("Failed to get vdev for vdev_%d", getReq->staId);
		return QDF_STATUS_E_FAILURE;
	}
	addr = wlan_vdev_mlme_get_macaddr(vdev);
	if (!addr) {
		wma_err("Failed to get macaddr for vdev_%d", getReq->staId);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(cmd.peer_macaddr.bytes, addr, QDF_MAC_ADDR_SIZE);
	ret = wma_send_ll_stats_get_cmd(wma, &cmd);
	if (ret) {
		wma_err("Failed to send get link stats request");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_unified_link_iface_stats_event_handler() - link iface stats event handler
 * @wma:wma handle
 * @cmd_param_info: data from event
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_unified_link_iface_stats_event_handler(void *handle,
					       uint8_t *cmd_param_info,
					       uint32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_iface_link_stats_event_fixed_param *fixed_param;
	wmi_iface_link_stats *link_stats, *iface_link_stats;
	wmi_wmm_ac_stats *ac_stats, *iface_ac_stats;
	wmi_iface_offload_stats *offload_stats, *iface_offload_stats;
	tSirLLStatsResults *link_stats_results;
	struct wifi_interface_stats *iface_stat;
	uint32_t count;
	size_t link_stats_size, ac_stats_size, iface_info_size;
	size_t link_stats_results_size, offload_stats_size;
	size_t total_ac_size, total_offload_size;
	bool db2dbm_enabled;

	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_debug("NULL mac ptr. Exiting");
		return -EINVAL;
	}

	if (!mac->sme.link_layer_stats_cb) {
		wma_debug("HDD callback is null");
		return -EINVAL;
	}

	param_tlvs = (WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_iface_link_stats_event_fixed_param fixed_param;
	 * wmi_iface_link_stats iface_link_stats;
	 * iface_link_stats->num_ac * size of(struct wmi_wmm_ac_stats)
	 * fixed_param->num_offload_stats * size of(wmi_iface_offload_stats);
	 */
	fixed_param = param_tlvs->fixed_param;
	link_stats = param_tlvs->iface_link_stats;
	ac_stats = param_tlvs->ac;
	offload_stats = param_tlvs->iface_offload_stats;

	if (!fixed_param || !link_stats || (link_stats->num_ac && !ac_stats) ||
	    (fixed_param->num_offload_stats && !offload_stats)) {
		wma_err("Invalid param_tlvs for Iface Stats");
		return -EINVAL;
	}
	if (link_stats->num_ac > WIFI_AC_MAX || link_stats->num_ac >
	    param_tlvs->num_ac) {
		wma_err("Excess data received from firmware num_ac %d, param_tlvs->num_ac %d",
			link_stats->num_ac, param_tlvs->num_ac);
		return -EINVAL;
	}
	if (fixed_param->num_offload_stats > WMI_OFFLOAD_STATS_TYPE_MAX ||
	    fixed_param->num_offload_stats >
	    param_tlvs->num_iface_offload_stats) {
		wma_err("Excess num offload stats recvd from fw: %d, um_iface_offload_stats: %d",
			fixed_param->num_offload_stats,
			param_tlvs->num_iface_offload_stats);
		return -EINVAL;
	}

	link_stats_size = sizeof(struct wifi_interface_stats);
	iface_info_size = sizeof(struct wifi_interface_info);

	ac_stats_size = sizeof(wmi_wmm_ac_stats);
	offload_stats_size = sizeof(wmi_iface_offload_stats);

	total_ac_size = ac_stats_size * WIFI_AC_MAX;
	total_offload_size = offload_stats_size * WMI_OFFLOAD_STATS_TYPE_MAX +
			      member_size(struct wifi_interface_stats,
					  num_offload_stats);

	link_stats_results_size = sizeof(*link_stats_results) +	link_stats_size;

	link_stats_results = qdf_mem_malloc(link_stats_results_size);
	if (!link_stats_results)
		return -ENOMEM;

	qdf_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId = WMI_LINK_STATS_IFACE;
	link_stats_results->rspId = fixed_param->request_id;
	link_stats_results->ifaceId = fixed_param->vdev_id;
	link_stats_results->num_peers = link_stats->num_peers;
	link_stats_results->peer_event_number = 0;
	link_stats_results->moreResultToFollow = 0;

	/* results is copied to struct wifi_interface_stats in upper layer
	 *   struct wifi_interface_stats
	 *    - struct wifi_interface_info (all fields except roaming is
	 *                             filled by host in the upper layer)
	 *    - various members of struct wifi_interface_stats (from
	 *                             wmi_iface_link_stats)
	 *    - ACs information (from wmi_wmm_ac_stats)
	 *    - num_offload_stats (from fixed param)
	 *    - offload stats (from wmi_iface_offload_stats)
	 */

	iface_stat = (struct wifi_interface_stats *)link_stats_results->results;

	iface_link_stats = &iface_stat->link_stats;
	*iface_link_stats = *link_stats;
	db2dbm_enabled = wmi_service_enabled(wma_handle->wmi_handle,
					     wmi_service_hw_db2dbm_support);
	if (!db2dbm_enabled) {
		/* FW doesn't indicate support for HW db2dbm conversion */
		iface_link_stats->rssi_mgmt += WMA_TGT_NOISE_FLOOR_DBM;
		iface_link_stats->rssi_data += WMA_TGT_NOISE_FLOOR_DBM;
		iface_link_stats->rssi_ack += WMA_TGT_NOISE_FLOOR_DBM;
	}
	wma_debug("db2dbm: %d, rssi_mgmt: %d, rssi_data: %d, rssi_ack: %d",
		 db2dbm_enabled, iface_link_stats->rssi_mgmt,
		 iface_link_stats->rssi_data, iface_link_stats->rssi_ack);

	/* Copy roaming state */
	iface_stat->info.roaming = link_stats->roam_state;

	iface_ac_stats = &iface_stat->ac_stats[0];
	for (count = 0; count < link_stats->num_ac; count++) {
		*iface_ac_stats = *ac_stats;
		ac_stats++;
		iface_ac_stats++;
	}

	/* Copy wmi_iface_offload_stats to wifi_iface_offload_stat */
	iface_stat->num_offload_stats = fixed_param->num_offload_stats;
	iface_offload_stats = &iface_stat->offload_stats[0];
	for (count = 0; count < fixed_param->num_offload_stats; count++) {
		*iface_offload_stats = *offload_stats;
		offload_stats++;
		iface_offload_stats++;
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	mac->sme.link_layer_stats_cb(mac->hdd_handle,
				     WMA_LINK_LAYER_STATS_RESULTS_RSP,
				     link_stats_results,
				     mac->sme.ll_stats_context);
	qdf_mem_free(link_stats_results);

	return 0;
}

/**
 * wma_config_stats_ext_threshold - set threthold for MAC counters
 * @wma: wma handler
 * @threshold: threhold for MAC counters
 *
 * For each MAC layer counter, FW holds two copies. One is the current value.
 * The other is the last report. Once a current counter's increment is larger
 * than the threshold, FW will indicate that counter to host even if the
 * monitoring timer does not expire.
 *
 * Return: None
 */
void wma_config_stats_ext_threshold(tp_wma_handle wma,
				    struct sir_ll_ext_stats_threshold *thresh)
{
	QDF_STATUS status;
	uint32_t len, tag, hdr_len;
	uint8_t *buf_ptr;
	wmi_buf_t buf;
	wmi_pdev_set_stats_threshold_cmd_fixed_param *cmd;
	wmi_chan_cca_stats_thresh *cca;
	wmi_peer_signal_stats_thresh *signal;
	wmi_tx_stats_thresh *tx;
	wmi_rx_stats_thresh *rx;

	if (!thresh) {
		wma_err("Invalid threshold input");
		return;
	}

	len = sizeof(wmi_pdev_set_stats_threshold_cmd_fixed_param) +
	      sizeof(wmi_chan_cca_stats_thresh) +
	      sizeof(wmi_peer_signal_stats_thresh) +
	      sizeof(wmi_tx_stats_thresh) +
	      sizeof(wmi_rx_stats_thresh) +
	      5 * WMI_TLV_HDR_SIZE;
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return;

	buf_ptr = (u_int8_t *)wmi_buf_data(buf);
	tag = WMITLV_TAG_STRUC_wmi_pdev_set_stats_threshold_cmd_fixed_param;
	hdr_len = WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_set_stats_threshold_cmd_fixed_param);
	wma_debug("Setting fixed parameters. tag=%d, len=%d", tag, hdr_len);
	cmd = (wmi_pdev_set_stats_threshold_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header, tag, hdr_len);
	cmd->enable_thresh = thresh->enable;
	cmd->use_thresh_bitmap = thresh->enable_bitmap;
	cmd->gbl_thresh = thresh->global_threshold;
	cmd->cca_thresh_enable_bitmap = thresh->cca_bitmap;
	cmd->signal_thresh_enable_bitmap = thresh->signal_bitmap;
	cmd->tx_thresh_enable_bitmap = thresh->tx_bitmap;
	cmd->rx_thresh_enable_bitmap = thresh->rx_bitmap;
	len = sizeof(wmi_pdev_set_stats_threshold_cmd_fixed_param);

	tag = WMITLV_TAG_STRUC_wmi_chan_cca_stats_thresh,
	hdr_len = WMITLV_GET_STRUCT_TLVLEN(wmi_chan_cca_stats_thresh);
	cca = (wmi_chan_cca_stats_thresh *)(buf_ptr + len);
	WMITLV_SET_HDR(&cca->tlv_header, tag, hdr_len);
	wma_debug("Setting cca parameters. tag=%d, len=%d", tag, hdr_len);
	cca->idle_time = thresh->cca.idle_time;
	cca->tx_time = thresh->cca.tx_time;
	cca->rx_in_bss_time = thresh->cca.rx_in_bss_time;
	cca->rx_out_bss_time = thresh->cca.rx_out_bss_time;
	cca->rx_busy_time = thresh->cca.rx_busy_time;
	cca->rx_in_bad_cond_time = thresh->cca.rx_in_bad_cond_time;
	cca->tx_in_bad_cond_time = thresh->cca.tx_in_bad_cond_time;
	cca->wlan_not_avail_time = thresh->cca.wlan_not_avail_time;
	wma_debug("idle time=%d, tx_time=%d, in_bss=%d, out_bss=%d",
		 cca->idle_time, cca->tx_time,
		 cca->rx_in_bss_time, cca->rx_out_bss_time);
	wma_debug("rx_busy=%d, rx_bad=%d, tx_bad=%d, not_avail=%d",
		 cca->rx_busy_time, cca->rx_in_bad_cond_time,
		 cca->tx_in_bad_cond_time, cca->wlan_not_avail_time);
	len += sizeof(wmi_chan_cca_stats_thresh);

	signal = (wmi_peer_signal_stats_thresh *)(buf_ptr + len);
	tag = WMITLV_TAG_STRUC_wmi_peer_signal_stats_thresh;
	hdr_len = WMITLV_GET_STRUCT_TLVLEN(wmi_peer_signal_stats_thresh);
	wma_debug("Setting signal parameters. tag=%d, len=%d", tag, hdr_len);
	WMITLV_SET_HDR(&signal->tlv_header, tag, hdr_len);
	signal->per_chain_snr = thresh->signal.snr;
	signal->per_chain_nf = thresh->signal.nf;
	wma_debug("snr=%d, nf=%d", signal->per_chain_snr,
		 signal->per_chain_nf);
	len += sizeof(wmi_peer_signal_stats_thresh);

	tx = (wmi_tx_stats_thresh *)(buf_ptr + len);
	tag = WMITLV_TAG_STRUC_wmi_tx_stats_thresh;
	hdr_len = WMITLV_GET_STRUCT_TLVLEN(wmi_tx_stats_thresh);
	wma_debug("Setting TX parameters. tag=%d, len=%d", tag, len);
	WMITLV_SET_HDR(&tx->tlv_header, tag, hdr_len);
	tx->tx_msdu_cnt = thresh->tx.msdu;
	tx->tx_mpdu_cnt = thresh->tx.mpdu;
	tx->tx_ppdu_cnt = thresh->tx.ppdu;
	tx->tx_bytes = thresh->tx.bytes;
	tx->tx_msdu_drop_cnt = thresh->tx.msdu_drop;
	tx->tx_drop_bytes = thresh->tx.byte_drop;
	tx->tx_mpdu_retry_cnt = thresh->tx.mpdu_retry;
	tx->tx_mpdu_fail_cnt = thresh->tx.mpdu_fail;
	tx->tx_ppdu_fail_cnt = thresh->tx.ppdu_fail;
	tx->tx_mpdu_aggr = thresh->tx.aggregation;
	tx->tx_succ_mcs = thresh->tx.succ_mcs;
	tx->tx_fail_mcs = thresh->tx.fail_mcs;
	tx->tx_ppdu_delay = thresh->tx.delay;
	wma_debug("msdu=%d, mpdu=%d, ppdu=%d, bytes=%d, msdu_drop=%d",
		 tx->tx_msdu_cnt, tx->tx_mpdu_cnt, tx->tx_ppdu_cnt,
		 tx->tx_bytes, tx->tx_msdu_drop_cnt);
	wma_debug("byte_drop=%d, mpdu_retry=%d, mpdu_fail=%d, ppdu_fail=%d",
		 tx->tx_drop_bytes, tx->tx_mpdu_retry_cnt,
		 tx->tx_mpdu_fail_cnt, tx->tx_ppdu_fail_cnt);
	wma_debug("aggr=%d, succ_mcs=%d, fail_mcs=%d, delay=%d",
		 tx->tx_mpdu_aggr, tx->tx_succ_mcs, tx->tx_fail_mcs,
		 tx->tx_ppdu_delay);
	len += sizeof(wmi_tx_stats_thresh);

	rx = (wmi_rx_stats_thresh *)(buf_ptr + len);
	tag = WMITLV_TAG_STRUC_wmi_rx_stats_thresh,
	hdr_len = WMITLV_GET_STRUCT_TLVLEN(wmi_rx_stats_thresh);
	WMITLV_SET_HDR(&rx->tlv_header, tag, hdr_len);
	wma_debug("Setting RX parameters. tag=%d, len=%d", tag, hdr_len);
	rx->mac_rx_mpdu_cnt = thresh->rx.mpdu;
	rx->mac_rx_bytes = thresh->rx.bytes;
	rx->phy_rx_ppdu_cnt = thresh->rx.ppdu;
	rx->phy_rx_bytes = thresh->rx.ppdu_bytes;
	rx->rx_disorder_cnt = thresh->rx.disorder;
	rx->rx_mpdu_retry_cnt = thresh->rx.mpdu_retry;
	rx->rx_mpdu_dup_cnt = thresh->rx.mpdu_dup;
	rx->rx_mpdu_discard_cnt = thresh->rx.mpdu_discard;
	rx->rx_mpdu_aggr = thresh->rx.aggregation;
	rx->rx_mcs = thresh->rx.mcs;
	rx->sta_ps_inds = thresh->rx.ps_inds;
	rx->sta_ps_durs = thresh->rx.ps_durs;
	rx->rx_probe_reqs = thresh->rx.probe_reqs;
	rx->rx_oth_mgmts = thresh->rx.other_mgmt;
	wma_debug("rx_mpdu=%d, rx_bytes=%d, rx_ppdu=%d, rx_pbytes=%d",
		 rx->mac_rx_mpdu_cnt, rx->mac_rx_bytes,
		 rx->phy_rx_ppdu_cnt, rx->phy_rx_bytes);
	wma_debug("disorder=%d, rx_dup=%d, rx_aggr=%d, rx_mcs=%d",
		 rx->rx_disorder_cnt, rx->rx_mpdu_dup_cnt,
		 rx->rx_mpdu_aggr, rx->rx_mcs);
	wma_debug("rx_ind=%d, rx_dur=%d, rx_probe=%d, rx_mgmt=%d",
		 rx->sta_ps_inds, rx->sta_ps_durs,
		 rx->rx_probe_reqs, rx->rx_oth_mgmts);
	len += sizeof(wmi_rx_stats_thresh);

	wma_alert("WMA --> WMI_PDEV_SET_STATS_THRESHOLD_CMDID(0x%x), length=%d",
		 WMI_PDEV_SET_STATS_THRESHOLD_CMDID, len);
	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				      WMI_PDEV_SET_STATS_THRESHOLD_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

/**
 * wma_post_link_status() - post link status to SME
 * @pGetLinkStatus: SME Link status
 * @link_status: Link status
 *
 * Return: none
 */
void wma_post_link_status(tAniGetLinkStatus *pGetLinkStatus,
			  uint8_t link_status)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct scheduler_msg sme_msg = { 0 };

	pGetLinkStatus->linkStatus = link_status;
	sme_msg.type = eWNI_SME_LINK_STATUS_IND;
	sme_msg.bodyptr = pGetLinkStatus;
	sme_msg.bodyval = 0;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Fail to post link status ind msg");
		qdf_mem_free(pGetLinkStatus);
	}
}

int wma_link_status_event_handler(void *handle, uint8_t *cmd_param_info,
				  uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_vdev_rate_stats_event_fixed_param *event;
	wmi_vdev_rate_ht_info *ht_info;
	struct wma_txrx_node *intr = wma->interfaces;
	uint8_t link_status = LINK_STATUS_LEGACY;
	uint32_t i, rate_flag;
	QDF_STATUS status;

	param_buf =
	      (WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	event = (wmi_vdev_rate_stats_event_fixed_param *)
						param_buf->fixed_param;
	ht_info = (wmi_vdev_rate_ht_info *) param_buf->ht_info;

	if (!ht_info) {
		wma_err("Invalid ht_info");
		return -EINVAL;
	}

	wma_debug("num_vdev_stats: %d", event->num_vdev_stats);

	if (event->num_vdev_stats > ((WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*event)) / sizeof(*ht_info)) ||
	    event->num_vdev_stats > param_buf->num_ht_info) {
		wma_err("excess vdev_stats buffers:%d, num_ht_info:%d",
			event->num_vdev_stats,
			param_buf->num_ht_info);
		return -EINVAL;
	}

	if (!wma_is_vdev_valid(ht_info->vdevid)) {
		wma_err("Invalid vdevid %d", ht_info->vdevid);
		return -EINVAL;
	}

	if (!intr[ht_info->vdevid].vdev) {
		wma_err("Vdev is NULL");
		return -EINVAL;
	}

	status = wma_get_vdev_rate_flag(intr[ht_info->vdevid].vdev, &rate_flag);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get rate flag");
		return -EINVAL;
	}

	for (i = 0; (i < event->num_vdev_stats) && ht_info; i++) {
		wma_debug("vdevId:%d  tx_nss:%d rx_nss:%d tx_preamble:%d rx_preamble:%d",
			ht_info->vdevid, ht_info->tx_nss,
			ht_info->rx_nss, ht_info->tx_preamble,
			ht_info->rx_preamble);
		if (ht_info->vdevid < wma->max_bssid
		    && intr[ht_info->vdevid].plink_status_req) {
			if (ht_info->tx_nss || ht_info->rx_nss)
				link_status = LINK_STATUS_MIMO;

			if ((ht_info->tx_preamble == LINK_RATE_VHT) ||
			    (ht_info->rx_preamble == LINK_RATE_VHT))
				link_status |= LINK_STATUS_VHT;

			if (intr[ht_info->vdevid].nss == 2)
				link_status |= LINK_SUPPORT_MIMO;

			if (rate_flag &
				(TX_RATE_VHT20 | TX_RATE_VHT40 |
				TX_RATE_VHT80))
				link_status |= LINK_SUPPORT_VHT;

			wma_post_link_status(
					intr[ht_info->vdevid].plink_status_req,
					link_status);
			intr[ht_info->vdevid].plink_status_req = NULL;
			link_status = LINK_STATUS_LEGACY;
		}

		ht_info++;
	}

	return 0;
}

int wma_rso_cmd_status_event_handler(wmi_roam_event_fixed_param *wmi_event)
{
	struct rso_cmd_status *rso_status;
	struct scheduler_msg sme_msg = {0};
	QDF_STATUS qdf_status;

	rso_status = qdf_mem_malloc(sizeof(*rso_status));
	if (!rso_status)
		return -ENOMEM;

	rso_status->vdev_id = wmi_event->vdev_id;
	if (WMI_ROAM_NOTIF_SCAN_MODE_SUCCESS == wmi_event->notif)
		rso_status->status = true;
	else if (WMI_ROAM_NOTIF_SCAN_MODE_FAIL == wmi_event->notif)
		rso_status->status = false;
	sme_msg.type = eWNI_SME_RSO_CMD_STATUS_IND;
	sme_msg.bodyptr = rso_status;
	sme_msg.bodyval = 0;
	wma_debug("Post RSO cmd status to SME");

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("fail to post RSO cmd status to SME");
		qdf_mem_free(rso_status);
	}
	return 0;
}

/**
 * wma_send_link_speed() - send link speed to SME
 * @link_speed: link speed
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_send_link_speed(uint32_t link_speed)
{
	struct mac_context *mac_ctx;
	struct link_speed_info *ls_ind;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		wma_debug("NULL mac ptr. Exiting");
		return QDF_STATUS_E_INVAL;
	}

	ls_ind = qdf_mem_malloc(sizeof(*ls_ind));
	if (!ls_ind)
		return QDF_STATUS_E_NOMEM;

	ls_ind->estLinkSpeed = link_speed;
	if (mac_ctx->sme.link_speed_cb)
		mac_ctx->sme.link_speed_cb(ls_ind,
					   mac_ctx->sme.link_speed_context);
	else
		wma_debug("link_speed_cb is null");
	qdf_mem_free(ls_ind);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_link_speed_event_handler() - link speed event handler
 * @handle: wma handle
 * @cmd_param_info: event data
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_link_speed_event_handler(void *handle, uint8_t *cmd_param_info,
				 uint32_t len)
{
	WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *param_buf;
	wmi_peer_estimated_linkspeed_event_fixed_param *event;
	QDF_STATUS qdf_status;

	param_buf = (WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *)
							 cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid linkspeed event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	qdf_status = wma_send_link_speed(event->est_linkspeed_kbps);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		return -EINVAL;
	return 0;
}

#define BIG_ENDIAN_MAX_DEBUG_BUF   500
/**
 * wma_unified_debug_print_event_handler() - debug print event handler
 * @handle: wma handle
 * @datap: data pointer
 * @len: length
 *
 * Return: 0 for success or error code
 */
int wma_unified_debug_print_event_handler(void *handle, uint8_t *datap,
					  uint32_t len)
{
	WMI_DEBUG_PRINT_EVENTID_param_tlvs *param_buf;
	uint8_t *data;
	uint32_t datalen;

	param_buf = (WMI_DEBUG_PRINT_EVENTID_param_tlvs *) datap;
	if (!param_buf || !param_buf->data) {
		wma_err("Get NULL point message from FW");
		return -ENOMEM;
	}
	data = param_buf->data;
	datalen = param_buf->num_data;
	if (datalen > WMI_SVC_MSG_MAX_SIZE) {
		wma_err("Received data len %d exceeds max value %d",
			datalen, WMI_SVC_MSG_MAX_SIZE);
		return QDF_STATUS_E_FAILURE;
	}
	data[datalen - 1] = '\0';

#ifdef BIG_ENDIAN_HOST
	{
		if (datalen >= BIG_ENDIAN_MAX_DEBUG_BUF) {
			wma_err("Invalid data len %d, limiting to max",
				datalen);
			datalen = BIG_ENDIAN_MAX_DEBUG_BUF - 1;
		}
		char dbgbuf[BIG_ENDIAN_MAX_DEBUG_BUF] = { 0 };

		memcpy(dbgbuf, data, datalen);
		SWAPME(dbgbuf, datalen);
		wma_debug("FIRMWARE:%s", dbgbuf);
		return 0;
	}
#else
	wma_debug("FIRMWARE:%s", data);
	return 0;
#endif /* BIG_ENDIAN_HOST */
}

enum wlan_phymode
wma_peer_phymode(tSirNwType nw_type, uint8_t sta_type,
		 uint8_t is_ht, uint8_t ch_width,
		 uint8_t is_vht, bool is_he)
{
	enum wlan_phymode phymode = WLAN_PHYMODE_AUTO;

	switch (nw_type) {
	case eSIR_11B_NW_TYPE:
#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == sta_type) {
		if (is_he)
			phymode = WLAN_PHYMODE_11AXG_HE20;
		else if (is_vht)
			phymode = WLAN_PHYMODE_11AC_VHT20_2G;
		else if (is_ht)
			phymode = WLAN_PHYMODE_11NG_HT20;
		else
			phymode = WLAN_PHYMODE_11B;
	} else
#endif /* FEATURE_WLAN_TDLS */
	{
		phymode = WLAN_PHYMODE_11B;
		if (is_ht || is_vht || is_he)
			wma_err("HT/VHT is enabled with 11B NW type");
	}
		break;
	case eSIR_11G_NW_TYPE:
		if (!(is_ht || is_vht || is_he)) {
			phymode = WLAN_PHYMODE_11G;
			break;
		}
		if (CH_WIDTH_40MHZ < ch_width)
			wma_err("80/160 MHz BW sent in 11G, configured 40MHz");
		if (ch_width)
			phymode = (is_he) ? WLAN_PHYMODE_11AXG_HE40 : (is_vht) ?
					WLAN_PHYMODE_11AC_VHT40_2G :
					WLAN_PHYMODE_11NG_HT40;
		else
			phymode = (is_he) ? WLAN_PHYMODE_11AXG_HE20 : (is_vht) ?
					WLAN_PHYMODE_11AC_VHT20_2G :
					WLAN_PHYMODE_11NG_HT20;
		break;
	case eSIR_11A_NW_TYPE:
		if (!(is_ht || is_vht || is_he)) {
			phymode = WLAN_PHYMODE_11A;
			break;
		}
		if (is_he) {
			if (ch_width == CH_WIDTH_160MHZ)
				phymode = WLAN_PHYMODE_11AXA_HE160;
			else if (ch_width == CH_WIDTH_80P80MHZ)
				phymode = WLAN_PHYMODE_11AXA_HE80_80;
			else if (ch_width == CH_WIDTH_80MHZ)
				phymode = WLAN_PHYMODE_11AXA_HE80;
			else
				phymode = (ch_width) ?
					  WLAN_PHYMODE_11AXA_HE40 :
					  WLAN_PHYMODE_11AXA_HE20;
		} else if (is_vht) {
			if (ch_width == CH_WIDTH_160MHZ)
				phymode = WLAN_PHYMODE_11AC_VHT160;
			else if (ch_width == CH_WIDTH_80P80MHZ)
				phymode = WLAN_PHYMODE_11AC_VHT80_80;
			else if (ch_width == CH_WIDTH_80MHZ)
				phymode = WLAN_PHYMODE_11AC_VHT80;
			else
				phymode = (ch_width) ?
					  WLAN_PHYMODE_11AC_VHT40 :
					  WLAN_PHYMODE_11AC_VHT20;
		} else
			phymode = (ch_width) ? WLAN_PHYMODE_11NA_HT40 :
						WLAN_PHYMODE_11NA_HT20;
		break;
	default:
		wma_err("Invalid nw type %d", nw_type);
		break;
	}
	wma_debug("nw_type %d is_ht %d ch_width %d is_vht %d is_he %d phymode %d",
		 nw_type, is_ht, ch_width, is_vht, is_he, phymode);

	return phymode;
}

/**
 * wma_txrx_fw_stats_reset() - reset txrx fw statistics
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: 0 for success or return error
 */
int32_t wma_txrx_fw_stats_reset(tp_wma_handle wma_handle,
				uint8_t vdev_id, uint32_t value)
{
	struct ol_txrx_stats_req req;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!soc) {
		wma_err("SOC context is NULL");
		return -EINVAL;
	}

	qdf_mem_zero(&req, sizeof(req));
	req.stats_type_reset_mask = value;
	cdp_fw_stats_get(soc, vdev_id, &req, false, false);

	return 0;
}

#ifdef HELIUMPLUS
#define SET_UPLOAD_MASK(_mask, _rate_info)	\
	((_mask) = 1 << (_rate_info ## _V2))
#else  /* !HELIUMPLUS */
#define SET_UPLOAD_MASK(_mask, _rate_info)	\
	((_mask) = 1 << (_rate_info))
#endif

#if defined(HELIUMPLUS) || defined(QCN7605_SUPPORT)
static bool wma_is_valid_fw_stats_cmd(uint32_t value)
{
	if (value > (HTT_DBG_NUM_STATS + 1) ||
		value == (HTT_DBG_STATS_RX_RATE_INFO + 1) ||
		value == (HTT_DBG_STATS_TX_RATE_INFO + 1) ||
		value == (HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT + 1)) {
		wma_err("Not supported");
		return false;
	}
	return true;
}
#else
static bool wma_is_valid_fw_stats_cmd(uint32_t value)
{
	if (value > (HTT_DBG_NUM_STATS + 1) ||
		value == (HTT_DBG_STATS_RX_RATE_INFO_V2 + 1) ||
		value == (HTT_DBG_STATS_TX_RATE_INFO_V2 + 1) ||
		value == (HTT_DBG_STATS_TXBF_MUSU_NDPA_PKT + 1)) {
		wma_err("Not supported");
		return false;
	}
	return true;
}
#endif

/**
 * wma_set_txrx_fw_stats_level() - set txrx fw stats level
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @value: value
 *
 * Return: 0 for success or return error
 */
int32_t wma_set_txrx_fw_stats_level(tp_wma_handle wma_handle,
				    uint8_t vdev_id, uint32_t value)
{
	struct ol_txrx_stats_req req;
	uint32_t l_up_mask;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!soc) {
		wma_err("SOC context is NULL");
		return -EINVAL;
	}

	if (wma_is_valid_fw_stats_cmd(value) == false)
		return -EINVAL;

	qdf_mem_zero(&req, sizeof(req));
	req.print.verbose = 1;

	/* TODO: Need to check how to avoid mem leak*/
	l_up_mask = 1 << (value - 1);
	req.stats_type_upload_mask = l_up_mask;

	cdp_fw_stats_get(soc, vdev_id, &req, false, true);

	return 0;
}

/**
 * wma_get_cca_stats() - send request to fw to get CCA
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS wma_get_cca_stats(tp_wma_handle wma_handle,
				uint8_t vdev_id)
{
	if (wmi_unified_congestion_request_cmd(wma_handle->wmi_handle,
			vdev_id)) {
		wma_err("Failed to congestion request to fw");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_beacon_buffer_by_vdev_id() - get the beacon buffer from vdev ID
 * @vdev_id: vdev id
 * @buffer_size: size of buffer
 *
 * Return: none
 */
void *wma_get_beacon_buffer_by_vdev_id(uint8_t vdev_id, uint32_t *buffer_size)
{
	tp_wma_handle wma;
	struct beacon_info *beacon;
	uint8_t *buf;
	uint32_t buf_size;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid WMA handle");
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %u", vdev_id);
		return NULL;
	}

	if (!wma_is_vdev_in_ap_mode(wma, vdev_id)) {
		wma_err("vdevid %d is not in AP mode", vdev_id);
		return NULL;
	}

	beacon = wma->interfaces[vdev_id].beacon;

	if (!beacon) {
		wma_err("beacon invalid");
		return NULL;
	}

	qdf_spin_lock_bh(&beacon->lock);

	buf_size = qdf_nbuf_len(beacon->buf);
	buf = qdf_mem_malloc(buf_size);
	if (!buf) {
		qdf_spin_unlock_bh(&beacon->lock);
		return NULL;
	}

	qdf_mem_copy(buf, qdf_nbuf_data(beacon->buf), buf_size);

	qdf_spin_unlock_bh(&beacon->lock);

	if (buffer_size)
		*buffer_size = buf_size;

	return buf;
}

uint8_t *wma_get_vdev_address_by_vdev_id(uint8_t vdev_id)
{
	tp_wma_handle wma;
	struct wlan_objmgr_vdev *vdev;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid WMA handle");
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %u", vdev_id);
		return NULL;
	}
	vdev = wma->interfaces[vdev_id].vdev;
	if (!vdev) {
		wma_err("Invalid vdev for vdev_id %u", vdev_id);
		return NULL;
	}
	return wlan_vdev_mlme_get_macaddr(vdev);
}

QDF_STATUS wma_get_connection_info(uint8_t vdev_id,
		struct policy_mgr_vdev_entry_info *conn_table_entry)
{
	struct wma_txrx_node *wma_conn_table_entry;

	wma_conn_table_entry = wma_get_interface_by_vdev_id(vdev_id);
	if (!wma_conn_table_entry) {
		wma_err("can't find vdev_id %d in WMA table", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	conn_table_entry->chan_width = wma_conn_table_entry->chan_width;
	conn_table_entry->mac_id = wma_conn_table_entry->mac_id;
	conn_table_entry->mhz = wma_conn_table_entry->ch_freq;
	conn_table_entry->sub_type = wma_conn_table_entry->sub_type;
	conn_table_entry->type = wma_conn_table_entry->type;
	conn_table_entry->ch_flagext = wma_conn_table_entry->ch_flagext;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_ndi_update_connection_info(uint8_t vdev_id,
				struct nan_datapath_channel_info *ndp_chan_info)
{
	struct wma_txrx_node *wma_iface_entry;

	wma_iface_entry = wma_get_interface_by_vdev_id(vdev_id);
	if (!wma_iface_entry) {
		wma_err("can't find vdev_id %d in WMA table", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (WMI_VDEV_TYPE_NDI != wma_iface_entry->type) {
		wma_err("Given vdev id(%d) not of type NDI!", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!ndp_chan_info) {
		wma_err("Provided chan info is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	wma_iface_entry->chan_width = ndp_chan_info->ch_width;
	wma_iface_entry->ch_freq = ndp_chan_info->freq;
	wma_iface_entry->nss = ndp_chan_info->nss;
	wma_iface_entry->mac_id = ndp_chan_info->mac_id;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_interface_by_vdev_id() - lookup interface entry using vdev ID
 * @vdev_id: vdev id
 *
 * Return: entry from vdev table
 */
struct wma_txrx_node  *wma_get_interface_by_vdev_id(uint8_t vdev_id)
{
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid WMA handle");
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %u", vdev_id);
		return NULL;
	}

	return &wma->interfaces[vdev_id];
}

#ifdef WLAN_FEATURE_PKT_CAPTURE
int wma_get_rmf_status(uint8_t vdev_id)
{
	struct wma_txrx_node *iface;

	iface = wma_get_interface_by_vdev_id(vdev_id);
	if (!iface) {
		wma_err("Unable to get wma interface");
		return -EINVAL;
	}

	return iface->rmfEnabled;
}
#endif

/**
 * wma_update_intf_hw_mode_params() - Update WMA params
 * @vdev_id: VDEV id whose params needs to be updated
 * @mac_id: MAC id to be updated
 * @cfgd_hw_mode_index: HW mode index from which Tx and Rx SS will be updated
 *
 * Updates the MAC id, tx spatial stream, rx spatial stream in WMA
 *
 * Return: None
 */
void wma_update_intf_hw_mode_params(uint32_t vdev_id, uint32_t mac_id,
				    uint32_t cfgd_hw_mode_index)
{
	tp_wma_handle wma;
	struct policy_mgr_hw_mode_params hw_mode;
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid WMA handle");
		return;
	}

	if (!wma->interfaces) {
		wma_err("Interface is NULL");
		return;
	}

	status = policy_mgr_get_hw_mode_from_idx(wma->psoc, cfgd_hw_mode_index,
						 &hw_mode);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("cfgd_hw_mode_index %d not found",
			 cfgd_hw_mode_index);
		return;
	}
	wma->interfaces[vdev_id].mac_id = mac_id;
	if (mac_id == 0)
		wma->interfaces[vdev_id].tx_streams =
			hw_mode.mac0_tx_ss;
	else
		wma->interfaces[vdev_id].tx_streams =
			hw_mode.mac1_tx_ss;

	wma_debug("vdev %d, update tx ss:%d mac %d hw_mode_id %d",
		 vdev_id,
		 wma->interfaces[vdev_id].tx_streams,
		 mac_id,
		 cfgd_hw_mode_index);
}

/**
 * wma_get_vht_ch_width - return vht channel width
 *
 * Return: return vht channel width
 */
uint32_t wma_get_vht_ch_width(void)
{
	uint32_t fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
	tp_wma_handle wm_hdl = cds_get_context(QDF_MODULE_ID_WMA);
	struct target_psoc_info *tgt_hdl;
	int vht_cap_info;

	if (!wm_hdl)
		return fw_ch_wd;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wm_hdl->psoc);
	if (!tgt_hdl)
		return fw_ch_wd;

	vht_cap_info = target_if_get_vht_cap_info(tgt_hdl);
	if (vht_cap_info & WMI_VHT_CAP_CH_WIDTH_80P80_160MHZ)
		fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
	else if (vht_cap_info & WMI_VHT_CAP_CH_WIDTH_160MHZ)
		fw_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;

	return fw_ch_wd;
}

/**
 * wma_get_num_of_setbits_from_bitmask() - to get num of setbits from bitmask
 * @mask: given bitmask
 *
 * This helper function should return number of setbits from bitmask
 *
 * Return: number of setbits from bitmask
 */
uint32_t wma_get_num_of_setbits_from_bitmask(uint32_t mask)
{
	uint32_t num_of_setbits = 0;

	while (mask) {
		mask &= (mask - 1);
		num_of_setbits++;
	}
	return num_of_setbits;
}

/**
 * wma_is_csa_offload_enabled - checks fw CSA offload capability
 *
 * Return: true or false
 */

bool wma_is_csa_offload_enabled(void)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma)
		return false;

	return wmi_service_enabled(wma->wmi_handle,
				   wmi_service_csa_offload);
}

bool wma_is_mbssid_enabled(void)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma)
		return false;

	return wmi_service_enabled(wma->wmi_handle,
				   wmi_service_infra_mbssid);
}

#ifdef FEATURE_FW_LOG_PARSING
/**
 * wma_config_debug_module_cmd - set debug log config
 * @wmi_handle: wmi layer handle
 * @param: debug log parameter
 * @val: debug log value
 * @module_id_bitmap: debug module id bitmap
 * @bitmap_len:  debug module bitmap length
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
wma_config_debug_module_cmd(wmi_unified_t wmi_handle, A_UINT32 param,
			    A_UINT32 val, A_UINT32 *module_id_bitmap,
			    A_UINT32 bitmap_len)
{
	struct dbglog_params dbg_param;

	dbg_param.param = param;
	dbg_param.val = val;
	dbg_param.module_id_bitmap = module_id_bitmap;
	dbg_param.bitmap_len = bitmap_len;

	return wmi_unified_dbglog_cmd_send(wmi_handle, &dbg_param);
}
#endif
#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * wma_is_p2p_lo_capable() - if driver is capable of p2p listen offload
 *
 * This function checks if driver is capable of p2p listen offload
 *    true: capable of p2p offload
 *    false: not capable
 *
 * Return: true - capable, false - not capable
 */
bool wma_is_p2p_lo_capable(void)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		return wmi_service_enabled
				(wma->wmi_handle,
				 wmi_service_p2p_listen_offload_support);
	}

	return 0;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS wma_get_roam_scan_ch(wmi_unified_t wmi_handle,
				uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct roam_scan_ch_resp *roam_ch;
	struct scheduler_msg sme_msg = {0};

	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("vdev_id: %d is not active", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_get_roam_scan_ch_list(wmi_handle, vdev_id);
	if (QDF_IS_STATUS_SUCCESS(status))
		return status;
	roam_ch = qdf_mem_malloc(sizeof(struct roam_scan_ch_resp));
	if (!roam_ch)
		return QDF_STATUS_E_INVAL;

	roam_ch->command_resp = 1;
	roam_ch->num_channels = 0;
	roam_ch->chan_list = NULL;
	roam_ch->vdev_id = vdev_id;
	sme_msg.type = eWNI_SME_GET_ROAM_SCAN_CH_LIST_EVENT;
	sme_msg.bodyptr = roam_ch;

	if (scheduler_post_message(QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_SME, &sme_msg)) {
		wma_err("Failed to post msg to SME");
		qdf_mem_free(roam_ch);
		return QDF_STATUS_E_INVAL;
	}

	return status;
}
#endif

bool wma_capability_enhanced_mcast_filter(void)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		return wmi_service_enabled(wma->wmi_handle,
					   wmi_service_enhanced_mcast_filter);
	}

	return 0;
}


bool wma_is_vdev_up(uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	bool is_up = false;

	if (!wma) {
		wma_err("WMA context is invalid!");
		return is_up;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma->psoc, vdev_id,
			WLAN_LEGACY_WMA_ID);
	if (vdev) {
		is_up = QDF_IS_STATUS_SUCCESS(wlan_vdev_is_up(vdev));
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	}
	return is_up;
}

void wma_acquire_wakelock(qdf_wake_lock_t *wl, uint32_t msec)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	cds_host_diag_log_work(wl, msec, WIFI_POWER_EVENT_WAKELOCK_WMI_CMD_RSP);
	qdf_wake_lock_timeout_acquire(wl, msec);
	qdf_runtime_pm_prevent_suspend(&wma->wmi_cmd_rsp_runtime_lock);
}

void wma_release_wakelock(qdf_wake_lock_t *wl)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	qdf_wake_lock_release(wl, WIFI_POWER_EVENT_WAKELOCK_WMI_CMD_RSP);
	qdf_runtime_pm_allow_suspend(&wma->wmi_cmd_rsp_runtime_lock);
}

QDF_STATUS wma_send_vdev_stop_to_fw(t_wma_handle *wma, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];
	struct vdev_mlme_obj *vdev_mlme = NULL;

	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("Invalid vdev id:%d", vdev_id);
		return status;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!vdev_mlme) {
		wma_err("Failed to get vdev mlme obj for vdev id %d", vdev_id);
		return status;
	}

	/*
	 * Reset the dynamic nss chains config to the ini values, as when the
	 * vdev gets its started again, this would be a fresh connection,
	 * and we dont want the config of previous connection to affect the
	 * current connection.
	 */
	qdf_mem_copy(mlme_get_dynamic_vdev_config(iface->vdev),
		     mlme_get_ini_vdev_config(iface->vdev),
		     sizeof(struct wlan_mlme_nss_chains));

	status = vdev_mgr_stop_send(vdev_mlme);

	return status;
}

QDF_STATUS wma_get_rcpi_req(WMA_HANDLE handle,
			    struct sme_rcpi_req *rcpi_request)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct rcpi_req  cmd = {0};
	struct wma_txrx_node *iface;
	struct sme_rcpi_req *node_rcpi_req;

	wma_debug("Enter");
	iface = &wma_handle->interfaces[rcpi_request->session_id];
	/* command is in progress */
	if (iface->rcpi_req) {
		wma_err("previous rcpi request is pending");
		return QDF_STATUS_SUCCESS;
	}

	node_rcpi_req = qdf_mem_malloc(sizeof(*node_rcpi_req));
	if (!node_rcpi_req)
		return QDF_STATUS_E_NOMEM;

	*node_rcpi_req = *rcpi_request;
	iface->rcpi_req = node_rcpi_req;

	cmd.vdev_id = rcpi_request->session_id;
	qdf_mem_copy(cmd.mac_addr, &rcpi_request->mac_addr, QDF_MAC_ADDR_SIZE);
	cmd.measurement_type = rcpi_request->measurement_type;

	if (wmi_unified_send_request_get_rcpi_cmd(wma_handle->wmi_handle,
						  &cmd)) {
		wma_err("Failed to send WMI_REQUEST_RCPI_CMDID");
		iface->rcpi_req = NULL;
		qdf_mem_free(node_rcpi_req);
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Exit");

	return QDF_STATUS_SUCCESS;
}

int wma_rcpi_event_handler(void *handle, uint8_t *cmd_param_info,
			   uint32_t len)
{
	struct rcpi_res res = {0};
	struct sme_rcpi_req *rcpi_req;
	struct qdf_mac_addr qdf_mac;
	struct wma_txrx_node *iface;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma_handle = (tp_wma_handle)handle;

	status = wmi_extract_rcpi_response_event(wma_handle->wmi_handle,
						 cmd_param_info, &res);
	if (status == QDF_STATUS_E_INVAL)
		return -EINVAL;

	if (res.vdev_id >= wma_handle->max_bssid) {
		wma_err("received invalid vdev_id %d", res.vdev_id);
		return -EINVAL;
	}

	iface = &wma_handle->interfaces[res.vdev_id];
	if (!iface->rcpi_req) {
		wmi_err("rcpi_req buffer not available");
		return 0;
	}

	rcpi_req = iface->rcpi_req;
	if (!rcpi_req->rcpi_callback) {
		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
		return 0;
	}

	if ((res.measurement_type == RCPI_MEASUREMENT_TYPE_INVALID) ||
	    (res.vdev_id != rcpi_req->session_id) ||
	    (res.measurement_type != rcpi_req->measurement_type) ||
	    (qdf_mem_cmp(res.mac_addr, &rcpi_req->mac_addr,
			 QDF_MAC_ADDR_SIZE))) {
		wmi_err("Invalid rcpi_response");
		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
		return 0;
	}

	qdf_mem_copy(&qdf_mac, res.mac_addr, QDF_MAC_ADDR_SIZE);
	(rcpi_req->rcpi_callback)(rcpi_req->rcpi_context, qdf_mac,
				  res.rcpi_value, status);
	iface->rcpi_req = NULL;
	qdf_mem_free(rcpi_req);

	return 0;
}

QDF_STATUS wma_send_vdev_down_to_fw(t_wma_handle *wma, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];
	struct vdev_mlme_obj *vdev_mlme;

	if (!wma_is_vdev_valid(vdev_id)) {
		wma_err("Invalid vdev id:%d", vdev_id);
		return status;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!vdev_mlme) {
		wma_err("Failed to get vdev mlme obj for vdev id %d", vdev_id);
		return status;
	}

	wma->interfaces[vdev_id].roaming_in_progress = false;

	status = vdev_mgr_down_send(vdev_mlme);

	return status;
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
tSirWifiPeerType wmi_to_sir_peer_type(enum wmi_peer_type type)
{
	switch (type) {
	case WMI_PEER_TYPE_DEFAULT:
		return WIFI_PEER_STA;
	case WMI_PEER_TYPE_BSS:
		return WIFI_PEER_AP;
	case WMI_PEER_TYPE_TDLS:
		return WIFI_PEER_TDLS;
	case WMI_PEER_TYPE_NAN_DATA:
		return WIFI_PEER_NAN;
	default:
		wma_err("Cannot map wmi_peer_type %d to HAL peer type", type);
		return WIFI_PEER_INVALID;
	}
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef FEATURE_WLAN_DYNAMIC_CVM
/**
 * wma_set_vc_mode_config() - set voltage corner mode config to FW.
 * @wma_handle:	pointer to wma handle.
 * @vc_bitmap:	value needs to set to firmware.
 *
 * At the time of driver startup, set operating voltage corner mode
 * for differenet phymode and bw configurations.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_vc_mode_config(void *wma_handle,
		uint32_t vc_bitmap)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wma_handle;
	struct pdev_params pdevparam;

	pdevparam.param_id = WMI_PDEV_UPDATE_WDCVS_ALGO;
	pdevparam.param_value = vc_bitmap;

	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
			&pdevparam,
			WMA_WILDCARD_PDEV_ID);
	if (ret) {
		wma_err("Fail to Set Voltage Corner config (0x%x)",
			vc_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Successfully Set Voltage Corner config (0x%x)",
		vc_bitmap);

	return QDF_STATUS_SUCCESS;
}
#endif

int wma_chip_power_save_failure_detected_handler(void *handle,
						 uint8_t  *cmd_param_info,
						 uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID_param_tlvs *param_buf;
	wmi_chip_power_save_failure_detected_fixed_param  *event;
	struct chip_pwr_save_fail_detected_params  pwr_save_fail_params;
	struct mac_context *mac = (struct mac_context *)cds_get_context(
						QDF_MODULE_ID_PE);
	if (!wma) {
		wma_err("wma_handle is NULL");
		return -EINVAL;
	}
	if (!mac) {
		wma_err("Invalid mac context");
		return -EINVAL;
	}
	if (!mac->sme.chip_power_save_fail_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	param_buf =
	(WMI_PDEV_CHIP_POWER_SAVE_FAILURE_DETECTED_EVENTID_param_tlvs *)
	cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid pwr_save_fail_params breached event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	pwr_save_fail_params.failure_reason_code =
				event->power_save_failure_reason_code;
	pwr_save_fail_params.wake_lock_bitmap[0] =
				event->protocol_wake_lock_bitmap[0];
	pwr_save_fail_params.wake_lock_bitmap[1] =
				event->protocol_wake_lock_bitmap[1];
	pwr_save_fail_params.wake_lock_bitmap[2] =
				event->protocol_wake_lock_bitmap[2];
	pwr_save_fail_params.wake_lock_bitmap[3] =
				event->protocol_wake_lock_bitmap[3];
	mac->sme.chip_power_save_fail_cb(mac->hdd_handle,
				&pwr_save_fail_params);

	wma_debug("Invoke HDD pwr_save_fail callback");
	return 0;
}

int wma_roam_scan_stats_event_handler(void *handle, uint8_t *event,
				      uint32_t len)
{
	tp_wma_handle wma_handle;
	wmi_unified_t wmi_handle;
	struct sir_roam_scan_stats *roam_scan_stats_req = NULL;
	struct wma_txrx_node *iface = NULL;
	struct wmi_roam_scan_stats_res *res = NULL;
	int ret = 0;
	uint32_t vdev_id;
	QDF_STATUS status;

	wma_handle = handle;
	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return -EINVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return -EINVAL;
	}

	status = wmi_extract_roam_scan_stats_res_evt(wmi_handle, event,
						     &vdev_id,
						     &res);

	/* vdev_id can be invalid though status is success, hence validate */
	if (vdev_id >= wma_handle->max_bssid) {
		wma_err("Received invalid vdev_id: %d", vdev_id);
		ret  = -EINVAL;
		goto free_res;
	}

	/* Get interface for valid vdev_id */
	iface = &wma_handle->interfaces[vdev_id];
	if (!iface) {
		wmi_err("Interface not available for vdev_id: %d", vdev_id);
		ret  = -EINVAL;
		goto free_res;
	}

	roam_scan_stats_req = iface->roam_scan_stats_req;
	iface->roam_scan_stats_req = NULL;
	if (!roam_scan_stats_req) {
		wmi_err("No pending request vdev_id: %d", vdev_id);
		ret  = -EINVAL;
		goto free_res;
	}

	if (!QDF_IS_STATUS_SUCCESS(status) ||
	    !roam_scan_stats_req->cb ||
	    roam_scan_stats_req->vdev_id != vdev_id) {
		wmi_err("roam_scan_stats buffer not available");
		ret = -EINVAL;
		goto free_roam_scan_stats_req;
	}

	roam_scan_stats_req->cb(roam_scan_stats_req->context, res);

free_roam_scan_stats_req:
	qdf_mem_free(roam_scan_stats_req);
	roam_scan_stats_req = NULL;

free_res:
	qdf_mem_free(res);
	res = NULL;

	return ret;
}

QDF_STATUS wma_get_roam_scan_stats(WMA_HANDLE handle,
				   struct sir_roam_scan_stats *req)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	struct wmi_roam_scan_stats_req cmd = {0};
	struct wma_txrx_node *iface;
	struct sir_roam_scan_stats *node_req = NULL;

	wma_debug("Enter");
	iface = &wma_handle->interfaces[req->vdev_id];
	/* command is in progress */
	if (iface->roam_scan_stats_req) {
		wma_err("previous roam scan stats req is pending");
		return QDF_STATUS_SUCCESS;
	}

	node_req = qdf_mem_malloc(sizeof(*node_req));
	if (!node_req)
		return QDF_STATUS_E_NOMEM;

	*node_req = *req;
	iface->roam_scan_stats_req = node_req;
	cmd.vdev_id = req->vdev_id;

	if (wmi_unified_send_roam_scan_stats_cmd(wma_handle->wmi_handle,
						 &cmd)) {
		wma_err("Failed to send WMI_REQUEST_ROAM_SCAN_STATS_CMDID");
		iface->roam_scan_stats_req = NULL;
		qdf_mem_free(node_req);
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Exit");

	return QDF_STATUS_SUCCESS;
}

void wma_remove_bss_peer_on_vdev_start_failure(tp_wma_handle wma,
					       uint8_t vdev_id)
{
	uint8_t pdev_id = WMI_PDEV_ID_SOC;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;
	struct qdf_mac_addr bss_peer;
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[vdev_id];

	status = mlme_get_vdev_bss_peer_mac_addr(iface->vdev, &bss_peer);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get bssid");
		return;
	}

	wma_err("ADD BSS failure for vdev %d", vdev_id);

	if (!cdp_find_peer_exist(soc, pdev_id, bss_peer.bytes)) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(bss_peer.bytes));
		return;
	}

	wma_remove_peer(wma, bss_peer.bytes, vdev_id, false);
}

QDF_STATUS wma_sta_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				uint16_t data_len, void *data)
{
	uint8_t vdev_id;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS status;
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev_mlme->vdev);
	iface = &wma->interfaces[vdev_id];
	vdev_mlme->proto.sta.assoc_id = iface->aid;

	status = vdev_mgr_up_send(vdev_mlme);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send vdev up cmd: vdev %d", vdev_id);
		status = QDF_STATUS_E_FAILURE;
	} else {
		wma_set_vdev_mgmt_rate(wma, vdev_id);
		if (iface->beacon_filter_enabled)
			wma_add_beacon_filter(
					wma,
					&iface->beacon_filter);
	}

	return QDF_STATUS_SUCCESS;
}

bool wma_get_hidden_ssid_restart_in_progress(struct wma_txrx_node *iface)
{
	if (!iface)
		return false;

	return ap_mlme_is_hidden_ssid_restart_in_progress(iface->vdev);
}

bool wma_get_channel_switch_in_progress(struct wma_txrx_node *iface)
{
	if (!iface)
		return false;

	return mlme_is_chan_switch_in_progress(iface->vdev);
}

static QDF_STATUS wma_vdev_send_start_resp(tp_wma_handle wma,
					  struct add_bss_rsp *add_bss_rsp)
{
	wma_debug("Sending add bss rsp to umac(vdev %d status %d)",
		 add_bss_rsp->vdev_id, add_bss_rsp->status);
	lim_handle_add_bss_rsp(wma->mac_context, add_bss_rsp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_sta_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	enum vdev_assoc_type assoc_type;

	if (!wma) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}
	if (mlme_is_chan_switch_in_progress(vdev_mlme->vdev)) {
		mlme_set_chan_switch_in_progress(vdev_mlme->vdev, false);
		lim_process_switch_channel_rsp(wma->mac_context, data);
		return QDF_STATUS_SUCCESS;
	}

	assoc_type = mlme_get_assoc_type(vdev_mlme->vdev);
	switch (assoc_type) {
	case VDEV_ASSOC:
	case VDEV_REASSOC:
		lim_process_switch_channel_rsp(wma->mac_context, data);
		break;
	case VDEV_FT_REASSOC:
		lim_handle_add_bss_rsp(wma->mac_context, data);
		break;
	default:
		wma_err("assoc_type %d is invalid", assoc_type);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_sta_mlme_vdev_roam_notify(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data)
{
	tp_wma_handle wma;
	int ret;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	ret = wma_mlme_roam_synch_event_handler_cb(wma, data, data_len);
	if (ret != 0) {
		wma_err("Failed to process roam synch event");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS wma_ap_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					   uint16_t data_len, void *data)
{
	tp_wma_handle wma;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
	uint8_t vdev_id;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (mlme_is_chan_switch_in_progress(vdev)) {
		mlme_set_chan_switch_in_progress(vdev, false);
		lim_process_switch_channel_rsp(wma->mac_context, data);
	} else if (ap_mlme_is_hidden_ssid_restart_in_progress(vdev)) {
		vdev_id = vdev->vdev_objmgr.vdev_id;
		lim_process_mlm_update_hidden_ssid_rsp(wma->mac_context,
						       vdev_id);
		ap_mlme_set_hidden_ssid_restart_in_progress(vdev, false);
	} else {
		status = wma_vdev_send_start_resp(wma, data);
	}

	return status;
}

QDF_STATUS wma_mlme_vdev_stop_continue(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	return __wma_handle_vdev_stop_rsp(
			(struct vdev_stop_response *)data);
}

QDF_STATUS wma_ap_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wma_send_vdev_down(wma, data);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wma_mlme_vdev_notify_down_complete(struct vdev_mlme_obj *vdev_mlme,
				   uint16_t data_len, void *data)
{
	tp_wma_handle wma;
	QDF_STATUS status;
	uint32_t vdev_stop_type;
	struct del_bss_resp *resp = (struct del_bss_resp *)data;

	if (mlme_is_connection_fail(vdev_mlme->vdev) ||
	    mlme_get_vdev_start_failed(vdev_mlme->vdev)) {
		wma_debug("Vdev start req failed, no action required");
		mlme_set_connection_fail(vdev_mlme->vdev, false);
		mlme_set_vdev_start_failed(vdev_mlme->vdev, false);
		return QDF_STATUS_SUCCESS;
	}

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handle is NULL");
		status = QDF_STATUS_E_INVAL;
		goto end;
	}

	status = mlme_get_vdev_stop_type(wma->interfaces[resp->vdev_id].vdev,
					 &vdev_stop_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get msg_type");
		status = QDF_STATUS_E_INVAL;
		goto end;
	}

	if (vdev_stop_type == WMA_DELETE_BSS_HO_FAIL_REQ) {
		resp->status = QDF_STATUS_SUCCESS;
		wma_send_msg_high_priority(wma, WMA_DELETE_BSS_HO_FAIL_RSP,
					   (void *)resp, 0);
		return QDF_STATUS_SUCCESS;
	}

	if (vdev_stop_type == WMA_SET_LINK_STATE) {
		lim_join_result_callback(wma->mac_context,
					 wlan_vdev_get_id(vdev_mlme->vdev));
	} else {
		wma_send_del_bss_response(wma, resp);
		return QDF_STATUS_SUCCESS;
	}

end:
	qdf_mem_free(resp);

	return status;
}

QDF_STATUS wma_ap_mlme_vdev_stop_start_send(struct vdev_mlme_obj *vdev_mlme,
					    enum vdev_cmd_type type,
					    uint16_t data_len, void *data)
{
	tp_wma_handle wma;
	struct add_bss_rsp *add_bss_rsp = data;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (wma_send_vdev_stop_to_fw(wma, add_bss_rsp->vdev_id))
		wma_err("Failed to send vdev stop for vdev id %d",
			 add_bss_rsp->vdev_id);

	wma_remove_bss_peer_on_vdev_start_failure(wma, add_bss_rsp->vdev_id);

	return wma_vdev_send_start_resp(wma, add_bss_rsp);
}

#ifdef QCA_SUPPORT_CP_STATS
QDF_STATUS wma_mon_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct request_info info = {0};
	uint8_t interval = 1;
	QDF_STATUS status;
	int pdev;

	if (!wma)
		return QDF_STATUS_E_INVAL;

	if (mlme_is_chan_switch_in_progress(vdev_mlme->vdev))
		mlme_set_chan_switch_in_progress(vdev_mlme->vdev, false);

	pdev = target_if_mc_cp_get_mac_id(vdev_mlme);

	/* Cancel periodic pdev stats update if running for other mac */
	status = wma_cli_set_command(vdev_mlme->vdev->vdev_objmgr.vdev_id,
				     WMI_PDEV_PARAM_PDEV_STATS_UPDATE_PERIOD,
				     0, PDEV_CMD);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("failed to clear fw stats request = %d", status);

	/* send periodic fw stats to get chan noise floor for monitor mode */
	info.vdev_id = vdev_mlme->vdev->vdev_objmgr.vdev_id;
	info.pdev_id = pdev;
	status = tgt_send_mc_cp_stats_req((wlan_vdev_get_psoc(vdev_mlme->vdev)),
					  TYPE_STATION_STATS,
					  &info);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("failed to send fw stats request = %d", status);

	status = wma_cli_set2_command(vdev_mlme->vdev->vdev_objmgr.vdev_id,
				      WMI_PDEV_PARAM_PDEV_STATS_UPDATE_PERIOD,
				      interval * 2000, pdev, PDEV_CMD);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("failed to send fw stats request = %d", status);

	lim_process_switch_channel_rsp(wma->mac_context, data);

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS wma_mon_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (mlme_is_chan_switch_in_progress(vdev_mlme->vdev))
		mlme_set_chan_switch_in_progress(vdev_mlme->vdev, false);

	lim_process_switch_channel_rsp(wma->mac_context, data);

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_SUPPORT_CP_STATS */

QDF_STATUS wma_mon_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				     uint16_t data_len, void *data)
{
	uint8_t vdev_id;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS status;
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev_mlme->vdev);
	iface = &wma->interfaces[vdev_id];
	vdev_mlme->proto.sta.assoc_id = 0;

	status = vdev_mgr_up_send(vdev_mlme);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send vdev up cmd: vdev %d", vdev_id);

	return status;
}

QDF_STATUS wma_mon_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	uint8_t vdev_id;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS status;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev_mlme->vdev);

	status = wma_send_vdev_stop_to_fw(wma, vdev_id);

	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send vdev stop cmd: vdev %d", vdev_id);

	wlan_vdev_mlme_sm_deliver_evt(vdev_mlme->vdev,
				      WLAN_VDEV_SM_EV_MLME_DOWN_REQ,
				      0,
				      NULL);

	return status;
}

QDF_STATUS wma_mon_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	uint8_t vdev_id;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS status;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = wlan_vdev_get_id(vdev_mlme->vdev);

	status = wma_send_vdev_down_to_fw(wma, vdev_id);

	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send vdev down cmd: vdev %d", vdev_id);

	wlan_vdev_mlme_sm_deliver_evt(vdev_mlme->vdev,
				      WLAN_VDEV_SM_EV_DOWN_COMPLETE,
				      0,
				      NULL);

	return status;
}

#ifdef FEATURE_WLM_STATS
int wma_wlm_stats_req(int vdev_id, uint32_t bitmask, uint32_t max_size,
		      wma_wlm_stats_cb cb, void *cookie)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	wmi_unified_t wmi_handle;
	wmi_buf_t wmi_buf;
	uint32_t buf_len, tlv_tag, tlv_len;
	wmi_request_wlm_stats_cmd_fixed_param *cmd;
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return -EINVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for wlm_stats_event_handler");
		return -EINVAL;
	}

	if (!wmi_service_enabled(wmi_handle, wmi_service_wlm_stats_support)) {
		wma_err("Feature not supported by firmware");
		return -ENOTSUPP;
	}

	wma_handle->wlm_data.wlm_stats_cookie = cookie;
	wma_handle->wlm_data.wlm_stats_callback = cb;
	wma_handle->wlm_data.wlm_stats_max_size = max_size;

	buf_len = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, buf_len);
	if (!wmi_buf)
		return -EINVAL;

	cmd = (void *)wmi_buf_data(wmi_buf);

	tlv_tag = WMITLV_TAG_STRUC_wmi_request_wlm_stats_cmd_fixed_param;
	tlv_len =
		WMITLV_GET_STRUCT_TLVLEN(wmi_request_wlm_stats_cmd_fixed_param);
	WMITLV_SET_HDR(&cmd->tlv_header, tlv_tag, tlv_len);

	cmd->vdev_id = vdev_id;
	cmd->request_bitmask = bitmask;
	status = wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, buf_len,
				      WMI_REQUEST_WLM_STATS_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_buf_free(wmi_buf);
		return -EINVAL;
	}
	/* info logging per test team request */
	wma_info("---->sent request for vdev:%d", vdev_id);

	return 0;
}

int wma_wlm_stats_rsp(void *wma_ctx, uint8_t *event, uint32_t evt_len)
{
	WMI_WLM_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_wlm_stats_event_fixed_param *param;
	tp_wma_handle wma_handle = wma_ctx;
	char *data;
	void *cookie;
	uint32_t *raw_data;
	uint32_t len, buffer_size, raw_data_num, i;

	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return -EINVAL;
	}
	if (!wma_handle->wlm_data.wlm_stats_callback) {
		wma_err("No callback registered");
		return -EINVAL;
	}

	param_tlvs = (WMI_WLM_STATS_EVENTID_param_tlvs *)event;
	param = param_tlvs->fixed_param;
	if (!param) {
		wma_err("Fix size param is not present, something is wrong");
		return -EINVAL;
	}

	/* info logging per test team request */
	wma_info("---->Received response for vdev:%d", param->vdev_id);

	raw_data = param_tlvs->data;
	raw_data_num = param_tlvs->num_data;

	len = 0;
	buffer_size = wma_handle->wlm_data.wlm_stats_max_size;
	data = qdf_mem_malloc(buffer_size);
	if (!data)
		return -ENOMEM;

	len += qdf_scnprintf(data + len, buffer_size - len, "\n%x ",
			     param->request_bitmask);
	len += qdf_scnprintf(data + len, buffer_size - len, "%x ",
			     param->vdev_id);
	len += qdf_scnprintf(data + len, buffer_size - len, "%x ",
			     param->timestamp);
	len += qdf_scnprintf(data + len, buffer_size - len, "%x ",
			     param->req_interval);
	if (!raw_data)
		goto send_data;

	len += qdf_scnprintf(data + len, buffer_size - len, "\ndata:\n");

	for (i = 0; i < raw_data_num; i++)
		len += qdf_scnprintf(data + len, buffer_size - len, "%x ",
				     *raw_data++);

send_data:
	cookie = wma_handle->wlm_data.wlm_stats_cookie;
	wma_handle->wlm_data.wlm_stats_callback(cookie, data);

	qdf_mem_free(data);

	return 0;
}
#endif /* FEATURE_WLM_STATS */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
static QDF_STATUS wma_send_cold_boot_cal_data(uint8_t *data,
		wmi_cold_boot_cal_data_fixed_param *event)
{
	struct host_log_cold_boot_cal_data_type *log_ptr = NULL;

	WLAN_HOST_DIAG_LOG_ALLOC(log_ptr,
				 struct host_log_cold_boot_cal_data_type,
				 LOG_WLAN_COLD_BOOT_CAL_DATA_C);

	if (!log_ptr)
		return QDF_STATUS_E_NOMEM;

	log_ptr->version = VERSION_LOG_WLAN_COLD_BOOT_CAL_DATA_C;
	log_ptr->cb_cal_data_len = event->data_len;
	log_ptr->flags = event->flags;
	qdf_mem_copy(log_ptr->cb_cal_data, data, log_ptr->cb_cal_data_len);

	WLAN_HOST_DIAG_LOG_REPORT(log_ptr);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wma_send_cold_boot_cal_data(uint8_t *data,
		wmi_cold_boot_cal_data_fixed_param *event)
{
	return QDF_STATUS_SUCCESS;
}
#endif

int wma_cold_boot_cal_event_handler(void *wma_ctx, uint8_t *event_buff,
				    uint32_t len)
{
	WMI_PDEV_COLD_BOOT_CAL_DATA_EVENTID_param_tlvs *param_buf;
	wmi_cold_boot_cal_data_fixed_param *event;
	QDF_STATUS status;
	tp_wma_handle wma_handle = (tp_wma_handle)wma_ctx;

	if (!wma_handle) {
		wma_err("NULL wma handle");
		return -EINVAL;
	}

	param_buf =
		   (WMI_PDEV_COLD_BOOT_CAL_DATA_EVENTID_param_tlvs *)event_buff;
	if (!param_buf) {
		wma_err("Invalid Cold Boot Cal Event");
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	if ((event->data_len > param_buf->num_data) ||
	    (param_buf->num_data > HOST_LOG_MAX_COLD_BOOT_CAL_DATA_SIZE)) {
		wma_err("Excess data_len:%d, num_data:%d", event->data_len,
			param_buf->num_data);
		return -EINVAL;
	}

	status = wma_send_cold_boot_cal_data((uint8_t *)param_buf->data, event);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Cold Boot Cal Diag log not sent");
		return -ENOMEM;
	}

	return 0;
}

#ifdef FEATURE_OEM_DATA
int wma_oem_event_handler(void *wma_ctx, uint8_t *event_buff, uint32_t len)
{
	WMI_OEM_DATA_EVENTID_param_tlvs *param_buf;
	struct mac_context *pmac =
		(struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	wmi_oem_data_event_fixed_param *event;
	struct oem_data oem_event_data;

	if (!pmac) {
		wma_err("NULL mac handle");
		return -EINVAL;
	}

	if (!pmac->sme.oem_data_event_handler_cb) {
		wma_err("oem data handler cb is not registered");
		return -EINVAL;
	}

	param_buf =
		   (WMI_OEM_DATA_EVENTID_param_tlvs *)event_buff;
	if (!param_buf) {
		wma_err("Invalid oem data Event");
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	if (!event) {
		wma_err("Invalid fixed param in oem data Event");
		return -EINVAL;
	}

	if (event->data_len > param_buf->num_data) {
		wma_err("Invalid data len %d num_data %d", event->data_len,
			param_buf->num_data);
		return -EINVAL;
	}

	oem_event_data.data_len = event->data_len;
	oem_event_data.data = param_buf->data;
	pmac->sme.oem_data_event_handler_cb(&oem_event_data,
					    pmac->sme.oem_data_vdev_id);

	return QDF_STATUS_SUCCESS;
}
#endif
