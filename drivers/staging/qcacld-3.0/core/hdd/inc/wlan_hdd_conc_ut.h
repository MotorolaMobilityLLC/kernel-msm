/*
 * Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_CONC_UT_H
#define __WLAN_HDD_CONC_UT_H

/* Include files */

#include "wlan_hdd_main.h"
#include "wlan_policy_mgr_api.h"
#ifdef MPC_UT_FRAMEWORK
void clean_report(struct hdd_context *hdd_ctx);
void fill_report(struct hdd_context *hdd_ctx, char *title,
	uint32_t first_persona, uint32_t second_persona, uint32_t third_persona,
	uint32_t chnl_1st_conn, uint32_t chnl_2nd_conn, uint32_t chnl_3rd_conn,
	bool status, enum policy_mgr_pcl_type pcl_type, char *reason,
	uint8_t *pcl);
void print_report(struct hdd_context *hdd_ctx);
void wlan_hdd_one_connection_scenario(struct hdd_context *hdd_ctx);
void wlan_hdd_two_connections_scenario(struct hdd_context *hdd_ctx,
	uint8_t first_chnl, enum policy_mgr_chain_mode first_chain_mask);
void wlan_hdd_three_connections_scenario(struct hdd_context *hdd_ctx,
	uint8_t first_chnl, uint8_t second_chnl,
	enum policy_mgr_chain_mode chain_mask, uint8_t use_same_mac);
#else
static inline
void clean_report(struct hdd_context *hdd_ctx)
{
}

static inline
void fill_report(struct hdd_context *hdd_ctx, char *title,
	uint32_t first_persona, uint32_t second_persona, uint32_t third_persona,
	uint32_t chnl_1st_conn, uint32_t chnl_2nd_conn, uint32_t chnl_3rd_conn,
	bool status, enum policy_mgr_pcl_type pcl_type, char *reason,
	uint8_t *pcl)
{
}

static inline
void print_report(struct hdd_context *hdd_ctx)
{
}

static inline
void wlan_hdd_one_connection_scenario(struct hdd_context *hdd_ctx)
{
}

static inline
void wlan_hdd_two_connections_scenario(struct hdd_context *hdd_ctx,
		uint8_t first_chnl, enum policy_mgr_chain_mode first_chain_mask)
{
}

static inline
void wlan_hdd_three_connections_scenario(struct hdd_context *hdd_ctx,
		uint8_t first_chnl, uint8_t second_chnl,
		enum policy_mgr_chain_mode chain_mask, uint8_t use_same_mac)
{
}
#endif
#endif
