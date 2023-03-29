/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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
   FILE:         cds_utils.c

   OVERVIEW:     This source file contains definitions for CDS crypto APIs
   The four APIs mentioned in this file are used for
   initializing, and de-initializing a crypto context, and
   obtaining truly random data (for keys), as well as
   SHA1 HMAC, and AES encrypt and decrypt routines.

   The routines include:
   cds_crypto_init() - Initializes Crypto module
   cds_crypto_deinit() - De-initializes Crypto module
   cds_rand_get_bytes() - Generates random byte

   DEPENDENCIES:
   ============================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/

#include "qdf_trace.h"
#include "cds_utils.h"
#include "qdf_mem.h"
#include <linux/err.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
#include <linux/ieee80211.h>
#include <crypto/hash.h>
#include <crypto/aes.h>

#include "cds_ieee80211_common.h"
#include <qdf_crypto.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
   Function Definitions and Documentation
 * -------------------------------------------------------------------------*/

#ifdef WLAN_FEATURE_11W
uint8_t cds_get_mmie_size(void)
{
	return sizeof(struct ieee80211_mmie);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
uint8_t cds_get_gmac_mmie_size(void)
{
	return sizeof(struct ieee80211_mmie_16);
}
#else
uint8_t cds_get_gmac_mmie_size(void)
{
	return 0;
}
#endif

#endif /* WLAN_FEATURE_11W */

uint32_t cds_chan_to_freq(uint8_t chan)
{
	if (chan < CDS_24_GHZ_CHANNEL_14)       /* ch 0 - ch 13 */
		return CDS_24_GHZ_BASE_FREQ + chan * CDS_CHAN_SPACING_5MHZ;
	else if (chan == CDS_24_GHZ_CHANNEL_14) /* ch 14 */
		return CDS_CHAN_14_FREQ;
	else if (chan < CDS_24_GHZ_CHANNEL_27)  /* ch 15 - ch 26 */
		return CDS_CHAN_15_FREQ +
		       (chan - CDS_24_GHZ_CHANNEL_15) * CDS_CHAN_SPACING_20MHZ;
	else if (chan == CDS_5_GHZ_CHANNEL_170)
		return CDS_CHAN_170_FREQ;
	else
		return CDS_5_GHZ_BASE_FREQ + chan * CDS_CHAN_SPACING_5MHZ;
}

uint8_t cds_freq_to_chan(uint32_t freq)
{
	uint8_t chan;

	if (freq > CDS_24_GHZ_BASE_FREQ && freq < CDS_CHAN_14_FREQ)
		chan = ((freq - CDS_24_GHZ_BASE_FREQ) / CDS_CHAN_SPACING_5MHZ);
	else if (freq == CDS_CHAN_14_FREQ)
		chan = CDS_24_GHZ_CHANNEL_14;
	else if ((freq > CDS_24_GHZ_BASE_FREQ) && (freq < CDS_5_GHZ_BASE_FREQ))
		chan = (((freq - CDS_CHAN_15_FREQ) / CDS_CHAN_SPACING_20MHZ) +
			CDS_24_GHZ_CHANNEL_15);
	else
		chan = (freq - CDS_5_GHZ_BASE_FREQ) / CDS_CHAN_SPACING_5MHZ;
	return chan;
}

enum cds_band_type cds_chan_to_band(uint32_t chan)
{
	if (chan <= CDS_24_GHZ_CHANNEL_14)
		return CDS_BAND_2GHZ;

	return CDS_BAND_5GHZ;
}

void cds_copy_hlp_info(struct qdf_mac_addr *input_dst_mac,
		       struct qdf_mac_addr *input_src_mac,
		       uint16_t input_hlp_data_len,
		       uint8_t *input_hlp_data,
		       struct qdf_mac_addr *output_dst_mac,
		       struct qdf_mac_addr *output_src_mac,
		       uint16_t *output_hlp_data_len,
		       uint8_t *output_hlp_data)
{
	if (!input_hlp_data_len) {
		cds_debug("Input HLP data len zero\n");
		return;
	}

	if (!input_dst_mac) {
		cds_debug("HLP destination mac NULL");
		return;
	}

	if (!input_src_mac) {
		cds_debug("HLP source mac NULL");
		return;
	}
	qdf_copy_macaddr(output_dst_mac, input_dst_mac);
	qdf_copy_macaddr(output_src_mac, input_src_mac);
	*output_hlp_data_len = input_hlp_data_len;
	qdf_mem_copy(output_hlp_data, input_hlp_data, input_hlp_data_len);
}
