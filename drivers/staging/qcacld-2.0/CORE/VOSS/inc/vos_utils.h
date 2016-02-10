/*
 * Copyright (c) 2011-2012, 2014-2015 The Linux Foundation. All rights reserved.
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


#if !defined( __VOS_UTILS_H )
#define __VOS_UTILS_H

/**=========================================================================

  \file  vos_utils.h

  \brief virtual Operating System Services (vOSS) utility APIs

   Various utility functions

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
#include <vos_event.h>
#include "aniGlobal.h"

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define VOS_DIGEST_SHA1_SIZE    20
#define VOS_DIGEST_MD5_SIZE     16
#define VOS_BAND_2GHZ          1
#define VOS_BAND_5GHZ          2

#define VOS_24_GHZ_BASE_FREQ   2407
#define VOS_5_GHZ_BASE_FREQ    5000
#define VOS_24_GHZ_CHANNEL_14  14
#define VOS_24_GHZ_CHANNEL_15  15
#define VOS_24_GHZ_CHANNEL_27  27
#define VOS_5_GHZ_CHANNEL_170  170
#define VOS_CHAN_SPACING_5MHZ  5
#define VOS_CHAN_SPACING_20MHZ 20
#define VOS_CHAN_14_FREQ       2484
#define VOS_CHAN_15_FREQ       2512
#define VOS_CHAN_170_FREQ      5852
/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/

VOS_STATUS vos_crypto_init( v_U32_t *phCryptProv );

VOS_STATUS vos_crypto_deinit( v_U32_t hCryptProv );



/**
 * vos_rand_get_bytes

 * FUNCTION:
 * Returns cryptographically secure pseudo-random bytes.
 *
 *
 * @param pbBuf - the caller allocated location where the bytes should be copied
 * @param numBytes the number of bytes that should be generated and
 * copied
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
*/
VOS_STATUS vos_rand_get_bytes( v_U32_t handle, v_U8_t *pbBuf, v_U32_t numBytes );


/**
 * vos_sha1_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-SHA1 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant SHA1 HMAC (20B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */
VOS_STATUS vos_sha1_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *text, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *key, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_SHA1_SIZE]);/* caller digest to be filled in */

/**
 * vos_md5_hmac_str
 *
 * FUNCTION:
 * Generate the HMAC-MD5 of a string given a key.
 *
 * LOGIC:
 * Standard HMAC processing from RFC 2104. The code is provided in the
 * appendix of the RFC.
 *
 * ASSUMPTIONS:
 * The RFC is correct.
 *
 * @param text text to be hashed
 * @param textLen length of text
 * @param key key to use for HMAC
 * @param keyLen length of key
 * @param digest holds resultant MD5 HMAC (16B)
 *
 * @return VOS_STATUS_SUCCSS if the operation succeeds
 *
 */
VOS_STATUS vos_md5_hmac_str(v_U32_t cryptHandle, /* Handle */
           v_U8_t *text, /* pointer to data stream */
           v_U32_t textLen, /* length of data stream */
           v_U8_t *key, /* pointer to authentication key */
           v_U32_t keyLen, /* length of authentication key */
           v_U8_t digest[VOS_DIGEST_MD5_SIZE]);/* caller digest to be filled in */



VOS_STATUS vos_encrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pText, /* pointer to data stream */
                           v_U8_t *Encrypted,
                           v_U8_t *pKey); /* pointer to authentication key */


VOS_STATUS vos_decrypt_AES(v_U32_t cryptHandle, /* Handle */
                           v_U8_t *pText, /* pointer to data stream */
                           v_U8_t *pDecrypted,
                           v_U8_t *pKey); /* pointer to authentication key */

v_U32_t vos_chan_to_freq(v_U8_t chan);
v_U8_t vos_freq_to_chan(v_U32_t freq);
v_U8_t vos_chan_to_band(v_U32_t chan);
#ifdef WLAN_FEATURE_11W
v_BOOL_t vos_is_mmie_valid(v_U8_t *key, v_U8_t *ipn,
				v_U8_t* frm, v_U8_t* efrm);
v_BOOL_t vos_attach_mmie(v_U8_t *igtk, v_U8_t *ipn, u_int16_t key_id,
				v_U8_t* frm, v_U8_t* efrm, u_int16_t frmLen);
v_U8_t vos_get_mmie_size(void);
#endif /* WLAN_FEATURE_11W */

eHalStatus vos_send_flush_logs_cmd_to_fw(tpAniSirGlobal pMac);
#endif // #if !defined __VOSS_UTILS_H
