/*
 * Copyright (c) 2011-2015, 2017-2020 The Linux Foundation. All rights reserved.
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
 *
 * This file lim_security_utils.h contains the utility definitions
 * related to WEP encryption/decryption etc.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SECURITY_UTILS_H
#define __LIM_SECURITY_UTILS_H
#include "sir_mac_prot_def.h"      /* for tSirMacAuthFrameBody */

#define LIM_ENCR_AUTH_BODY_LEN  (SIR_MAC_AUTH_FRAME_INFO_LEN + \
				 SIR_MAC_AUTH_CHALLENGE_BODY_LEN + \
				 SIR_MAC_WEP_IV_LENGTH + \
				 SIR_MAC_WEP_ICV_LENGTH)

#define LIM_ENCR_AUTH_BODY_LEN_SAP  (SIR_MAC_AUTH_FRAME_INFO_LEN + \
					 SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH + \
					 SIR_MAC_CHALLENGE_ID_LEN + \
					 SIR_MAC_WEP_IV_LENGTH + \
					 SIR_MAC_WEP_ICV_LENGTH)

#define LIM_ENCR_AUTH_INFO_LEN	(SIR_MAC_AUTH_FRAME_INFO_LEN +\
				 SIR_MAC_WEP_IV_LENGTH + \
				 SIR_MAC_WEP_ICV_LENGTH + \
				 SIR_MAC_CHALLENGE_ID_LEN)


struct tLimPreAuthNode;

uint8_t lim_is_auth_algo_supported(struct mac_context *, tAniAuthType,
				   struct pe_session *);

/* MAC based authentication related functions */
void lim_init_pre_auth_list(struct mac_context *);
void lim_delete_pre_auth_list(struct mac_context *);
struct tLimPreAuthNode *lim_search_pre_auth_list(struct mac_context *,
						 tSirMacAddr);
void lim_add_pre_auth_node(struct mac_context *, struct tLimPreAuthNode *);
void lim_delete_pre_auth_node(struct mac_context *, tSirMacAddr);
void lim_release_pre_auth_node(struct mac_context *mac,
			       tpLimPreAuthNode pAuthNode);
void lim_restore_from_auth_state(struct mac_context *,
				tSirResultCodes, uint16_t, struct pe_session *);
uint8_t lim_delete_open_auth_pre_auth_node(struct mac_context *mac_ctx);

/* Encryption/Decryption related functions */
void lim_compute_crc32(uint8_t *, uint8_t *, uint16_t);
void lim_rc4(uint8_t *, uint8_t *, uint8_t *, uint32_t, uint16_t);
void lim_encrypt_auth_frame(struct mac_context *, uint8_t, uint8_t *, uint8_t *,
			    uint8_t *, uint32_t);
uint8_t lim_decrypt_auth_frame(struct mac_context *, uint8_t *, uint8_t *,
			       uint8_t *, uint32_t, uint16_t);

void lim_post_sme_set_keys_cnf(struct mac_context *, tLimMlmSetKeysReq *,
			       tLimMlmSetKeysCnf *);

#define  PTAPS  0xedb88320

static inline uint32_t lim_crc_update(uint32_t crc, uint8_t x)
{

	/* Update CRC computation for 8 bits contained in x */
	/* */
	uint32_t z;
	uint32_t fb;
	int i;

	z = crc ^ x;
	for (i = 0; i < 8; i++) {
		fb = z & 1;
		z >>= 1;
		if (fb)
			z ^= PTAPS;
	}
	return z;
}

#endif /* __LIM_SECURITY_UTILS_H */
