/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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
 * DOC: lim_roam_timer_utils.c
 *
 * Host based roaming timers implementation
 */

#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"

/**
 * lim_create_timers_host_roam() - Create timers used in host based roaming
 * @mac_ctx: Global MAC context
 *
 * Create reassoc and preauth timers
 *
 * Return: TX_SUCCESS or TX_TIMER_ERROR
 */
uint32_t lim_create_timers_host_roam(struct mac_context *mac_ctx)
{
	uint32_t cfg_value;

	cfg_value = SYS_MS_TO_TICKS(
			mac_ctx->mlme_cfg->timeouts.reassoc_failure_timeout);
	/* Create Association failure timer and activate it later */
	if (tx_timer_create(mac_ctx,
			&mac_ctx->lim.lim_timers.gLimReassocFailureTimer,
		    "REASSOC FAILURE TIMEOUT", lim_assoc_failure_timer_handler,
		    LIM_REASSOC, cfg_value, 0, TX_NO_ACTIVATE) != TX_SUCCESS) {
		pe_err("failed to create Reassoc timer");
		return TX_TIMER_ERROR;
	}
	cfg_value = 1000;
	cfg_value = SYS_MS_TO_TICKS(cfg_value);
	if (tx_timer_create(mac_ctx,
			&mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer,
			"FT PREAUTH RSP TIMEOUT",
			lim_timer_handler, SIR_LIM_FT_PREAUTH_RSP_TIMEOUT,
			cfg_value, 0, TX_NO_ACTIVATE) != TX_SUCCESS) {
		pe_err("failed to create Join fail timer");
		goto err_roam_timer;
	}
	return TX_SUCCESS;

err_roam_timer:
	tx_timer_delete(&mac_ctx->lim.lim_timers.gLimReassocFailureTimer);
	return TX_TIMER_ERROR;
}

void lim_delete_timers_host_roam(struct mac_context *mac_ctx)
{
	tLimTimers *lim_timer = &mac_ctx->lim.lim_timers;

	/* Delete Reassociation failure timer. */
	tx_timer_delete(&lim_timer->gLimReassocFailureTimer);
	/* Delete FT Preauth response timer */
	tx_timer_delete(&lim_timer->gLimFTPreAuthRspTimer);
}

void lim_deactivate_timers_host_roam(struct mac_context *mac_ctx)
{
	tLimTimers *lim_timer = &mac_ctx->lim.lim_timers;

	/* Deactivate Reassociation failure timer. */
	tx_timer_deactivate(&lim_timer->gLimReassocFailureTimer);
	/* Deactivate FT Preauth response timer */
	tx_timer_deactivate(&lim_timer->gLimFTPreAuthRspTimer);
}

/**
 * lim_deactivate_and_change_timer_host_roam() - Change timers in host roaming
 * @mac_ctx: Pointer to Global MAC structure
 * @timer_id: enum of timer to be deactivated and changed
 *
 * This function is called to deactivate and change a timer for future
 * re-activation for host roaming timers.
 *
 * Return: None
 */
void lim_deactivate_and_change_timer_host_roam(struct mac_context *mac_ctx,
		uint32_t timer_id)
{
	uint32_t val = 0;

	switch (timer_id) {
	case eLIM_REASSOC_FAIL_TIMER:
		if (tx_timer_deactivate
			(&mac_ctx->lim.lim_timers.gLimReassocFailureTimer) !=
				TX_SUCCESS)
			pe_warn("unable to deactivate Reassoc fail timer");

		val = SYS_MS_TO_TICKS(
			mac_ctx->mlme_cfg->timeouts.reassoc_failure_timeout);
		if (tx_timer_change
			(&mac_ctx->lim.lim_timers.gLimReassocFailureTimer, val,
			 0) != TX_SUCCESS)
			pe_warn("unable to change Reassoc fail timer");
		break;

	case eLIM_FT_PREAUTH_RSP_TIMER:
		if (tx_timer_deactivate
			(&mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer) !=
			TX_SUCCESS) {
			pe_err("Unable to deactivate Preauth Fail timer");
			return;
		}
		val = 1000;
		val = SYS_MS_TO_TICKS(val);
		if (tx_timer_change(
				&mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer,
				val, 0) != TX_SUCCESS) {
			pe_err("Unable to change Join Failure timer");
			return;
		}
		break;
	}
}

