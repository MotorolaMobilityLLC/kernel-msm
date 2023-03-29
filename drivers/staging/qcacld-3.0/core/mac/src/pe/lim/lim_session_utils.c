/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

   \file  lim_session_utils.c
   \brief implementation for lim Session Utility  APIs
   \author Sunit Bhatia
   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "ani_global.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_session_utils.h"
#include "lim_utils.h"

/**
 * lim_is_in_mcc() - check if device is in MCC
 * @mac_ctx: Global MAC context.
 *
 * Return: true - if in MCC.
 *         false - Not in MCC
 **/
uint8_t lim_is_in_mcc(struct mac_context *mac_ctx)
{
	uint8_t i;
	uint32_t freq = 0;
	uint32_t curr_oper_freq = 0;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		/*
		 * if another session is valid and it is on different channel
		 * it is an off channel operation.
		 */
		if ((mac_ctx->lim.gpSession[i].valid)) {
			curr_oper_freq =
				mac_ctx->lim.gpSession[i].curr_op_freq;
			if (curr_oper_freq == 0)
				continue;
			if (freq == 0)
				freq = curr_oper_freq;
			else if (freq != curr_oper_freq)
				return true;
		}
	}
	return false;
}

/**
 * pe_get_current_stas_count() - Total stations associated on all sessions.
 * @mac_ctx: Global MAC context.
 *
 * Return: true - Number of stations active on all sessions.
 **/
uint8_t pe_get_current_stas_count(struct mac_context *mac_ctx)
{
	uint8_t i;
	uint8_t stacount = 0;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++)
		if (mac_ctx->lim.gpSession[i].valid == true)
			stacount +=
				mac_ctx->lim.gpSession[i].gLimNumOfCurrentSTAs;
	return stacount;
}
