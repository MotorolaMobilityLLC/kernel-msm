/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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

/**=========================================================================

   Macros and Function prototypes FT and 802.11R purposes

   ========================================================================*/

#ifndef __LIMFTDEFS_H__
#define __LIMFTDEFS_H__

#include <cds_api.h>
#include "wma_if.h"

/*--------------------------------------------------------------------------
   Preprocessor definitions and constants
   ------------------------------------------------------------------------*/
#define MAX_FTIE_SIZE             384   /* Max size limited to 384, on acct. of IW custom events */

/* Time to dwell on preauth channel during roaming, in milliseconds */
#define LIM_FT_PREAUTH_SCAN_TIME 50

/*--------------------------------------------------------------------------
   Type declarations
   ------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------
   FT Pre Auth Req SME<->PE
   ------------------------------------------------------------------------*/
typedef struct sSirFTPreAuthReq {
	uint16_t messageType;   /* eWNI_SME_FT_PRE_AUTH_REQ */
	uint16_t length;
	uint32_t dot11mode;
	/*
	 * Track if response is processed for this request
	 * We expect only one response per request.
	 */
	bool bPreAuthRspProcessed;
	uint16_t pre_auth_channel_freq;
	/* BSSID currently associated to suspend the link */
	tSirMacAddr currbssId;
	tSirMacAddr preAuthbssId;       /* BSSID to preauth to */
	tSirMacAddr self_mac_addr;
	uint32_t scan_id;
	uint16_t ft_ies_length;
	uint8_t ft_ies[MAX_FTIE_SIZE];
	struct bss_description *pbssDescription;
} tSirFTPreAuthReq, *tpSirFTPreAuthReq;

/*-------------------------------------------------------------------------
   FT Pre Auth Rsp PE<->SME
   ------------------------------------------------------------------------*/
typedef struct sSirFTPreAuthRsp {
	uint16_t messageType;   /* eWNI_SME_FT_PRE_AUTH_RSP */
	uint16_t length;
	uint8_t vdev_id;
	tSirMacAddr preAuthbssId;       /* BSSID to preauth to */
	QDF_STATUS status;
	uint16_t ft_ies_length;
	uint8_t ft_ies[MAX_FTIE_SIZE];
	uint16_t ric_ies_length;
	uint8_t ric_ies[MAX_FTIE_SIZE];
} tSirFTPreAuthRsp, *tpSirFTPreAuthRsp;

/*--------------------------------------------------------------------------
   FT Pre Auth Rsp Key SME<->PE
   ------------------------------------------------------------------------*/
typedef struct sSirFTUpdateKeyInfo {
	uint16_t messageType;
	uint16_t length;
	uint32_t vdev_id;
	struct qdf_mac_addr bssid;
	tSirKeyMaterial keyMaterial;
} tSirFTUpdateKeyInfo, *tpSirFTUpdateKeyInfo;

/*-------------------------------------------------------------------------
   Global FT Information
   ------------------------------------------------------------------------*/
typedef struct sFTPEContext {
	tpSirFTPreAuthReq pFTPreAuthReq;        /* Saved FT Pre Auth Req */
	QDF_STATUS ftPreAuthStatus;
	uint16_t saved_auth_rsp_length;
	uint8_t saved_auth_rsp[MAX_FTIE_SIZE];
	/* Items created for the new FT, session */
	void *pAddBssReq;       /* Save add bss req */
	void *pAddStaReq;       /*Save add sta req  */
	uint32_t peSessionId;
	uint32_t smeSessionId;

	/* This flag is required to indicate on which session the preauth
	 * has taken place, since the auth response for preauth will come
	 * for a new BSSID for which there is no session yet. This flag
	 * will be used to extract the session from the session preauth
	 * has been initiated
	 */
	bool ftPreAuthSession;
} tftPEContext, *tpftPEContext;

#endif /* __LIMFTDEFS_H__ */
