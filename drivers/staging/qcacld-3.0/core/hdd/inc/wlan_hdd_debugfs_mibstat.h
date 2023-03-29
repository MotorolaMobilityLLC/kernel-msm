/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_mibstat.h
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with MIB statistics
 */

#ifndef _WLAN_HDD_DEBUGFS_MIBSTAT_H
#define _WLAN_HDD_DEBUGFS_MIBSTAT_H

#define DEBUGFS_MIBSTATS_BUF_SIZE 4096

#include <wlan_hdd_main.h>

#if defined(WLAN_FEATURE_MIB_STATS) && defined(WLAN_DEBUGFS)
/**
 * hdd_debugfs_process_mib_stats() - Process mib stats from fw
 *
 * This function is used to store mib stats to global variable mib_stats.
 *
 * Return: None
 */
void hdd_debugfs_process_mib_stats(struct hdd_adapter *adapter,
				   struct stats_event *stats);

/**
 * wlan_hdd_create_mib_stats_file() - API to create MIB stats file
 * @adapter: interface adapter pointer
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_create_mib_stats_file(struct hdd_adapter *adapter);

/**
 * wlan_hdd_create_mib_stats_lock() - API to create MIB stats lock
 *
 * Return: No return
 */
void wlan_hdd_create_mib_stats_lock(void);

/**
 * wlan_hdd_destroy_mib_stats_lock() - API to destroy MIB stats lock
 *
 * Return: No return
 */
void wlan_hdd_destroy_mib_stats_lock(void);
#else
static inline int wlan_hdd_create_mib_stats_file(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void wlan_hdd_create_mib_stats_lock(void)
{
}

static inline void wlan_hdd_destroy_mib_stats_lock(void)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_DEBUGFS_MIBSTAT_H */
