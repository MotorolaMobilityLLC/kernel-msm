/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_GREEN_AP_H)
#define WLAN_HDD_GREEN_AP_H

#include "qdf_types.h"

struct hdd_context;

#ifdef WLAN_SUPPORT_GREEN_AP

/**
 * hdd_green_ap_add_sta() - Notify Green AP on STA association
 * @hdd_ctx: Global HDD context
 *
 * Call this function when new node is associated
 *
 * Return: void
 */
void hdd_green_ap_add_sta(struct hdd_context *hdd_ctx);

/**
 * hdd_green_ap_del_sta() - Notify Green AP on STA disassociation
 * @hdd_ctx: Global HDD context
 *
 * Call this function when new node is disassociated
 *
 * Return: void
 */
void hdd_green_ap_del_sta(struct hdd_context *hdd_ctx);

/**
 * hdd_green_ap_enable_egap() - Enable Enhanced Green AP
 * @hdd_ctx: Global HDD context
 *
 * This function will enable the Enhanced Green AP feature if it is supported
 * by the Green AP component.
 *
 * Return: 0 on success, negative errno on any failure
 */
int hdd_green_ap_enable_egap(struct hdd_context *hdd_ctx);

/**
 * hdd_green_ap_start_state_mc() - to start green AP state mc based on
 *        present concurrency and state of green AP state machine.
 * @hdd_ctx: hdd context
 * @mode: device mode
 * @is_session_start: BSS start/stop
 *
 * Return: 0 on success, negative errno on any failure
 */
int hdd_green_ap_start_state_mc(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE mode, bool is_session_start);

#else /* WLAN_SUPPORT_GREEN_AP */
static inline
void hdd_green_ap_add_sta(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_green_ap_del_sta(struct hdd_context *hdd_ctx)
{
}

static inline
int hdd_green_ap_enable_egap(struct hdd_context *hdd_ctx)
{
	return 0;
}

static inline
int hdd_green_ap_start_state_mc(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE mode, bool is_session_start)
{
	return 0;
}

#endif /* WLAN_SUPPORT_GREEN_AP */

#endif /* !defined(WLAN_HDD_GREEN_AP_H) */
