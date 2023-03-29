/*
 * Copyright (c) 2011-2014, 2016-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_timer_utils.h contains the utility definitions
 * LIM uses for timer handling.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_TIMER_UTILS_H
#define __LIM_TIMER_UTILS_H

#include "lim_types.h"

/* Timer related functions */
enum limtimertype {
	eLIM_MIN_CHANNEL_TIMER,
	eLIM_MAX_CHANNEL_TIMER,
	eLIM_JOIN_FAIL_TIMER,
	eLIM_AUTH_FAIL_TIMER,
	eLIM_AUTH_RESP_TIMER,
	eLIM_ASSOC_FAIL_TIMER,
	eLIM_REASSOC_FAIL_TIMER,
	eLIM_PRE_AUTH_CLEANUP_TIMER,
	eLIM_CNF_WAIT_TIMER,
	eLIM_AUTH_RSP_TIMER,
	eLIM_UPDATE_OLBC_CACHE_TIMER,
	eLIM_PROBE_AFTER_HB_TIMER,
	eLIM_ADDTS_RSP_TIMER,
	eLIM_CHANNEL_SWITCH_TIMER,
	eLIM_WPS_OVERLAP_TIMER,
	eLIM_FT_PREAUTH_RSP_TIMER,
	eLIM_REMAIN_CHN_TIMER,
	eLIM_DISASSOC_ACK_TIMER,
	eLIM_DEAUTH_ACK_TIMER,
	eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER,
	eLIM_INSERT_SINGLESHOT_NOA_TIMER,
	eLIM_AUTH_RETRY_TIMER,
	eLIM_AUTH_SAE_TIMER
};

#define LIM_DISASSOC_DEAUTH_ACK_TIMEOUT         500

/* Timer Handler functions */
uint32_t lim_create_timers(struct mac_context *);
void lim_timer_handler(void *, uint32_t);
void lim_auth_response_timer_handler(void *, uint32_t);
void lim_assoc_failure_timer_handler(void *, uint32_t);

void lim_deactivate_and_change_timer(struct mac_context *, uint32_t);
void lim_cnf_wait_tmer_handler(void *, uint32_t);
void lim_deactivate_and_change_per_sta_id_timer(struct mac_context *, uint32_t, uint16_t);
void lim_activate_cnf_timer(struct mac_context *, uint16_t, struct pe_session *);
void lim_activate_auth_rsp_timer(struct mac_context *, tLimPreAuthNode *);
void lim_update_olbc_cache_timer_handler(void *, uint32_t);
void lim_addts_response_timer_handler(void *, uint32_t);
#endif /* __LIM_TIMER_UTILS_H */
