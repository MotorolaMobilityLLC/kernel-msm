/*
 * Copyright (c) 2013-2016, 2018, 2019-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__SME_FTAPI_H)
#define __SME_FTAPI_H

typedef enum eFTIEState {
	eFT_START_READY,        /* Start before and after 11r assoc */
	eFT_AUTH_REQ_READY,     /* When we have recvd the 1st or nth auth req */
	/*
	 * Sent auth1 and waiting auth2 We are now ready for FT phase,
	 * send auth1, recd auth2
	 */
	eFT_WAIT_AUTH2,
	eFT_AUTH_COMPLETE,
	/* Now we have sent Auth Rsp to the supplicant and waiting */
	/* Reassoc Req from the supplicant. */
	eFT_REASSOC_REQ_WAIT,
	/*
	 * We have received the Reassoc request from supplicant.
	 * Waiting for the keys.
	 */
	eFT_SET_KEY_WAIT,
} tFTIEStates;

/* FT neighbor roam callback user context */
typedef struct sFTRoamCallbackUsrCtx {
	struct mac_context *mac;
	uint8_t sessionId;
} tFTRoamCallbackUsrCtx, *tpFTRoamCallbackUsrCtx;

typedef struct sFTSMEContext {
	/* Received and processed during pre-auth */
	uint8_t *auth_ft_ies;
	uint32_t auth_ft_ies_length;
	/* Received and processed during re-assoc */
	uint8_t *reassoc_ft_ies;
	uint16_t reassoc_ft_ies_length;
	/* Pre-Auth info */
	tFTIEStates FTState;    /* The state of FT in the current 11rAssoc */
	tSirMacAddr preAuthbssId;       /* BSSID to preauth to */
	uint32_t vdev_id;
	/* Saved pFTPreAuthRsp */
	tpSirFTPreAuthRsp psavedFTPreAuthRsp;
	bool setFTPreAuthState;
	/* Time to trigger reassoc once pre-auth is successful */
	qdf_mc_timer_t preAuthReassocIntvlTimer;
	bool addMDIE;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint32_t r0kh_id_len;
	uint8_t r0kh_id[SIR_ROAM_R0KH_ID_MAX_LEN];
#endif
	/* User context for the timer callback */
	tpFTRoamCallbackUsrCtx pUsrCtx;
} tftSMEContext, *tpftSMEContext;

/*--------------------------------------------------------------------------
  Prototype functions
  ------------------------------------------------------------------------*/
void sme_ft_open(mac_handle_t mac_handle, uint32_t sessionId);
void sme_ft_close(mac_handle_t mac_handle, uint32_t sessionId);
void sme_ft_reset(mac_handle_t mac_handle, uint32_t sessionId);
void sme_set_ft_ies(mac_handle_t mac_handle, uint32_t sessionId,
		    const uint8_t *ft_ies, uint16_t ft_ies_length);
QDF_STATUS sme_ft_update_key(mac_handle_t mac_handle, uint32_t sessionId,
			     tCsrRoamSetKey *pFTKeyInfo);
void sme_get_ft_pre_auth_response(mac_handle_t mac_handle, uint32_t sessionId,
				  uint8_t *ft_ies, uint32_t ft_ies_ip_len,
				  uint16_t *ft_ies_length);
void sme_get_rici_es(mac_handle_t mac_handle, uint32_t sessionId,
		     uint8_t *ric_ies,
		     uint32_t ric_ies_ip_len, uint32_t *ric_ies_length);
/**
 * sme_check_ft_status() - Check for key wait status in FT mode
 * @mac_handle: MAC handle
 * @session_id: vdev identifier
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_check_ft_status(mac_handle_t mac_handle, uint32_t session_id);

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * sme_ft_key_ready_for_install() - API to check ft key ready for install
 * @mac_handle: MAC handle
 * @session_id: vdev identifier
 *
 * It is only applicable for LFR2.0 enabled
 *
 * Return: true when ft key is ready otherwise false
 */
bool sme_ft_key_ready_for_install(mac_handle_t mac_handle, uint32_t session_id);
#else
static inline bool
sme_ft_key_ready_for_install(mac_handle_t mac_handle, uint32_t session_id)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * sme_reset_key() -Reset key information
 * @mac_handle: MAC handle
 * @vdev_id: vdev identifier
 *
 * Return: None
 */
void sme_reset_key(mac_handle_t mac_handle, uint32_t vdev_id);
#else
static inline void sme_reset_key(mac_handle_t mac_handle, uint32_t vdev_id)
{
}
#endif

void sme_preauth_reassoc_intvl_timer_callback(void *context);
void sme_set_ft_pre_auth_state(mac_handle_t mac_handle, uint32_t sessionId,
			       bool state);
bool sme_get_ft_pre_auth_state(mac_handle_t mac_handle, uint32_t sessionId);
#endif
