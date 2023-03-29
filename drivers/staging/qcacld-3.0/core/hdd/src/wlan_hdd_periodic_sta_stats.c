/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC : wlan_hdd_periodic_sta_stats.c
 *
 * WLAN Host Device Driver periodic STA statistics related implementation
 *
 */

#include "wlan_hdd_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_hdd_periodic_sta_stats.h"

void hdd_periodic_sta_stats_config(struct hdd_config *config,
				   struct wlan_objmgr_psoc *psoc)
{
	config->periodic_stats_timer_interval =
		cfg_get(psoc, CFG_PERIODIC_STATS_TIMER_INTERVAL);
	config->periodic_stats_timer_duration =
		cfg_get(psoc, CFG_PERIODIC_STATS_TIMER_DURATION);
}

void hdd_periodic_sta_stats_init(struct hdd_adapter *adapter)
{
	adapter->is_sta_periodic_stats_enabled = false;
}

void hdd_periodic_sta_stats_display(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_stats sta_stats;
	struct hdd_config *hdd_cfg;
	char *dev_name;
	bool should_log;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_PERIODIC_STA_STATS_DISPLAY;

	if (!hdd_ctx)
		return;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		should_log = false;

		if (adapter->device_mode != QDF_STA_MODE) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		hdd_cfg = hdd_ctx->config;
		qdf_mutex_acquire(&adapter->sta_periodic_stats_lock);

		if (!adapter->is_sta_periodic_stats_enabled) {
			qdf_mutex_release(&adapter->sta_periodic_stats_lock);
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		adapter->periodic_stats_timer_counter++;
		if ((adapter->periodic_stats_timer_counter *
		    GET_BW_COMPUTE_INTV(hdd_cfg)) >=
				hdd_cfg->periodic_stats_timer_interval) {
			should_log = true;

			adapter->periodic_stats_timer_count--;
			if (adapter->periodic_stats_timer_count == 0)
				adapter->is_sta_periodic_stats_enabled = false;
			adapter->periodic_stats_timer_counter = 0;
		}
		qdf_mutex_release(&adapter->sta_periodic_stats_lock);

		if (should_log) {
			dev_name = WLAN_HDD_GET_DEV_NAME(adapter);
			sta_stats = adapter->hdd_stats;
			hdd_nofl_info("%s: Tx ARP requests: %d", dev_name,
				      sta_stats.hdd_arp_stats.tx_arp_req_count);
			hdd_nofl_info("%s: Rx ARP responses: %d", dev_name,
				      sta_stats.hdd_arp_stats.rx_arp_rsp_count);
			hdd_nofl_info("%s: Tx DNS requests: %d", dev_name,
				      sta_stats.hdd_dns_stats.tx_dns_req_count);
			hdd_nofl_info("%s: Rx DNS responses: %d", dev_name,
				      sta_stats.hdd_dns_stats.rx_dns_rsp_count);
		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

void hdd_periodic_sta_stats_start(struct hdd_adapter *adapter)
{
	struct hdd_config *hdd_cfg = adapter->hdd_ctx->config;

	if ((adapter->device_mode == QDF_STA_MODE) &&
	    (hdd_cfg->periodic_stats_timer_interval > 0)) {
		qdf_mutex_acquire(&adapter->sta_periodic_stats_lock);

		adapter->periodic_stats_timer_count =
			hdd_cfg->periodic_stats_timer_duration /
			hdd_cfg->periodic_stats_timer_interval;
		adapter->periodic_stats_timer_counter = 0;
		if (adapter->periodic_stats_timer_count > 0)
			adapter->is_sta_periodic_stats_enabled = true;

		qdf_mutex_release(&adapter->sta_periodic_stats_lock);
	}
}

void hdd_periodic_sta_stats_stop(struct hdd_adapter *adapter)
{
	struct hdd_config *hdd_cfg = adapter->hdd_ctx->config;

	if ((adapter->device_mode == QDF_STA_MODE) &&
	    (hdd_cfg->periodic_stats_timer_interval > 0)) {
		qdf_mutex_acquire(&adapter->sta_periodic_stats_lock);

		/* Stop the periodic ARP and DNS stats timer */
		adapter->periodic_stats_timer_count = 0;
		adapter->is_sta_periodic_stats_enabled = false;

		qdf_mutex_release(&adapter->sta_periodic_stats_lock);
	}
}

void hdd_periodic_sta_stats_mutex_create(struct hdd_adapter *adapter)
{
	qdf_mutex_create(&adapter->sta_periodic_stats_lock);
}

void hdd_periodic_sta_stats_mutex_destroy(struct hdd_adapter *adapter)
{
	qdf_mutex_destroy(&adapter->sta_periodic_stats_lock);
}

