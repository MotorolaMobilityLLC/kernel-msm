/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_llstat.h
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with Link Layer statistics
 */

#ifndef _WLAN_HDD_DEBUGFS_LLSTAT_H
#define _WLAN_HDD_DEBUGFS_LLSTAT_H

#define DEBUGFS_LLSTATS_BUF_SIZE 12288
#define DEBUGFS_LLSTATS_REQID   4294967295UL
#define DEBUGFS_LLSTATS_REQMASK 0x7

#include <wlan_hdd_main.h>

#if defined(WLAN_FEATURE_LINK_LAYER_STATS) && defined(WLAN_DEBUGFS)
/**
 * hdd_debugfs_process_peer_stats() - Parse Peer stats and add it to buffer
 * @adapter: Pointer to device adapter
 * @data: Pointer to stats data
 *
 * Receiving Link Layer peer statistics from FW. This function stores the
 * firmware data in a buffer to be written into debugfs.
 *
 * Return: None
 */
void hdd_debugfs_process_peer_stats(struct hdd_adapter *adapter, void *data);

/**
 * hdd_debugfs_process_radio_stats() - Parse Radio stats and add it to buffer
 * @adapter: Pointer to device adapter
 * @more_data: More data
 * @data: Pointer to stats data
 * @num_radio: Number of radios
 *
 * Receiving Link Layer Radio statistics from FW. This function stores the
 * firmware data in a buffer to be written into debugfs.
 *
 * Return: None
 */
void hdd_debugfs_process_radio_stats(struct hdd_adapter *adapter,
		uint32_t more_data, void *data, uint32_t num_radio);

/**
 * hdd_link_layer_process_iface_stats() - This function is called after
 * @adapter: Pointer to device adapter
 * @data: Pointer to stats data
 * @num_peers: Number of peers
 *
 * Receiving Link Layer Interface statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 *
 * Return: None
 */
void hdd_debugfs_process_iface_stats(struct hdd_adapter *adapter,
		void *data, uint32_t num_peers);

/**
 * wlan_hdd_create_ll_stats_file() - API to create Link Layer stats file
 * @adapter: interface adapter pointer
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_create_ll_stats_file(struct hdd_adapter *adapter);
#else
static inline void hdd_debugfs_process_peer_stats(struct hdd_adapter *adapter,
						  void *data)
{
}

static inline void hdd_debugfs_process_radio_stats(
			struct hdd_adapter *adapter,
			uint32_t more_data, void *data, uint32_t num_radio)
{
}

static inline void hdd_debugfs_process_iface_stats(
				struct hdd_adapter *adapter,
				void *data, uint32_t num_peers)
{
}
static inline int wlan_hdd_create_ll_stats_file(struct hdd_adapter *adapter)
{
	return 0;
}
#endif
#endif /* #ifndef _WLAN_HDD_DEBUGFS_LLSTAT_H */
