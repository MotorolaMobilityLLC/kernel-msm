/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_periodic_sta_stats.h
 *
 * WLAN Host Device Driver periodic STA statistics related implementation
 *
 */

#if !defined(WLAN_HDD_PERIODIC_STA_STATS_H)
#define WLAN_HDD_PERIODIC_STA_STATS_H

#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
/*
 * Used to get device name from the adapter
 */
#define WLAN_HDD_GET_DEV_NAME(adapter) ((adapter)->dev->name)

/**
 * hdd_periodic_sta_stats_config() - Initialize periodic stats configuration
 * @config: Pointer to hdd configuration
 * @psoc: Pointer to psoc
 *
 * Return: none
 */
void hdd_periodic_sta_stats_config(struct hdd_config *config,
				   struct wlan_objmgr_psoc *psoc);

/**
 * hdd_periodic_sta_stats_init() - Initialize periodic stats display flag
 * @adapter: Pointer to the station adapter
 *
 * Return: none
 */
void hdd_periodic_sta_stats_init(struct hdd_adapter *adapter);

/**
 * hdd_periodic_sta_stats_display() - Display periodic stats at STA
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void hdd_periodic_sta_stats_display(struct hdd_context *hdd_ctx);

/**
 * hdd_periodic_sta_stats_start() - Start displaying periodic stats for STA
 * @adapter: Pointer to the station adapter
 *
 * Return: none
 */
void hdd_periodic_sta_stats_start(struct hdd_adapter *adapter);

/**
 * hdd_periodic_sta_stats_stop() - Stop displaying periodic stats for STA
 * @adapter: Pointer to the station adapter
 *
 * Return: none
 */
void hdd_periodic_sta_stats_stop(struct hdd_adapter *adapter);

/**
 * hdd_periodic_sta_stats_mutex_create() - Create mutex for STA periodic stats
 * @adapter: Pointer to the station adapter
 *
 * Return: none
 */
void hdd_periodic_sta_stats_mutex_create(struct hdd_adapter *adapter);

/**
 * hdd_periodic_sta_stats_mutex_destroy() - Destroy STA periodic stats mutex
 * @adapter: Pointer to the station adapter
 *
 * Return: none
 */
void hdd_periodic_sta_stats_mutex_destroy(struct hdd_adapter *adapter);

#else
static inline void
hdd_periodic_sta_stats_display(struct hdd_context *hdd_ctx) {}

static inline void
hdd_periodic_sta_stats_config(struct hdd_config *config,
			      struct wlan_objmgr_psoc *psoc) {}

static inline void hdd_periodic_sta_stats_start(struct hdd_adapter *adapter) {}

static inline void hdd_periodic_sta_stats_stop(struct hdd_adapter *adapter) {}

static inline void
hdd_periodic_sta_stats_init(struct hdd_adapter *adapter) {}

static inline void
hdd_periodic_sta_stats_mutex_create(struct hdd_adapter *adapter) {}

static inline void
hdd_periodic_sta_stats_mutex_destroy(struct hdd_adapter *adapter) {}

#endif /* end #ifdef WLAN_FEATURE_PERIODIC_STA_STATS */

#endif /* end #if !defined(WLAN_HDD_PERIODIC_STA_STATS_H) */
