/*
 * Copyright (c) 2011-2016, 2018-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_aid_mgmt.c contains the functions related to
 * AID pool management like initialization, assignment etc.
 * Author:        Chandra Modumudi
 * Date:          03/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "cds_api.h"
#include "wni_cfg.h"
#include "ani_global.h"
#include "sir_params.h"
#include "lim_utils.h"
#include "lim_timer_utils.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_session_utils.h"

#define LIM_START_PEER_IDX   1

/**
 * lim_init_peer_idxpool() -- initializes peer index pool
 * @mac: mac context
 * @pe_session: session entry
 *
 * This function is called while starting a BSS at AP
 * to initialize AID pool.
 *
 * Return: None
 */

void lim_init_peer_idxpool(struct mac_context *mac, struct pe_session *pe_session)
{
	uint8_t i;
	uint8_t maxAssocSta = mac->lim.max_sta_of_pe_session;

	pe_session->gpLimPeerIdxpool[0] = 0;

#ifdef FEATURE_WLAN_TDLS
	/*
	* In station role, DPH_STA_HASH_INDEX_PEER (index 1) is reserved
	* for peer station index corresponding to AP. Avoid choosing that index
	* and get index starting from (DPH_STA_HASH_INDEX_PEER + 1)
	* (index 2) for TDLS stations;
	*/
	if (LIM_IS_STA_ROLE(pe_session)) {
		pe_session->freePeerIdxHead = DPH_STA_HASH_INDEX_PEER + 1;
	} else
#endif
	{
		pe_session->freePeerIdxHead = LIM_START_PEER_IDX;
	}

	for (i = pe_session->freePeerIdxHead; i < maxAssocSta; i++) {
		pe_session->gpLimPeerIdxpool[i] = i + 1;
	}
	pe_session->gpLimPeerIdxpool[i] = 0;

	pe_session->freePeerIdxTail = i;

}

/**
 * lim_assign_peer_idx()
 *
 ***FUNCTION:
 * This function is called to get a peer station index. This index is
 * used during Association/Reassociation
 * frame handling to assign association ID (aid) to a STA.
 * In case of TDLS, this is used to assign a index into the Dph hash entry.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @return peerIdx  - assigned peer Station IDx for STA
 */

uint16_t lim_assign_peer_idx(struct mac_context *mac, struct pe_session *pe_session)
{
	uint16_t peerId;

	/* make sure we haven't exceeded the configurable limit on associations */
	/* This count is global to ensure that it doesn't exceed the hardware limits. */
	if (pe_get_current_stas_count(mac) >=
	    mac->mlme_cfg->sap_cfg.assoc_sta_limit) {
		/* too many associations already active */
		return 0;
	}

	/* return head of free list */

	if (pe_session->freePeerIdxHead) {
		peerId = pe_session->freePeerIdxHead;
		pe_session->freePeerIdxHead =
			pe_session->gpLimPeerIdxpool[pe_session->
							freePeerIdxHead];
		if (pe_session->freePeerIdxHead == 0)
			pe_session->freePeerIdxTail = 0;
		pe_session->gLimNumOfCurrentSTAs++;
		return peerId;
	}

	return 0;               /* no more free peer index */
}

/**
 * lim_release_peer_idx()
 *
 ***FUNCTION:
 * This function is called when a STA context is removed
 * at AP (or TDLS) to return peer Index
 * to free pool.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  peerIdx - peer station index that need to return to free pool
 *
 * @return None
 */

void
lim_release_peer_idx(struct mac_context *mac, uint16_t peerIdx,
		     struct pe_session *pe_session)
{
	pe_session->gLimNumOfCurrentSTAs--;

	/* insert at tail of free list */
	if (pe_session->freePeerIdxTail) {
		pe_session->gpLimPeerIdxpool[pe_session->
						freePeerIdxTail] =
			(uint8_t) peerIdx;
		pe_session->freePeerIdxTail = (uint8_t) peerIdx;
	} else {
		pe_session->freePeerIdxTail =
			pe_session->freePeerIdxHead = (uint8_t) peerIdx;
	}
	pe_session->gpLimPeerIdxpool[(uint8_t) peerIdx] = 0;
}
