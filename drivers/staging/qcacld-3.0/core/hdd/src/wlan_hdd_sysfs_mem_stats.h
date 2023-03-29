/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_mem_stats.h
 *
 * Implementation to add sysfs node wlan_mem_stats
 */

#ifndef _WLAN_HDD_SYSFS_MEM_STATS
#define _WLAN_HDD_SYSFS_MEM_STATS

#if defined(WLAN_SYSFS) && defined(CONFIG_WLAN_SYSFS_MEM_STATS)
/**
 * hdd_sysfs_mem_stats_create() - Function to create
 * wlan_mem_stats sysfs node to capture host driver memory usage
 * @wlan_kobject: sysfs wlan kobject
 *
 * file path: /sys/kernel/wifi/wlan/wlan_mem_stats
 *
 * usage: cat /sys/kernel/wifi/wlan/wlan_mem_stats
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_mem_stats_create(struct kobject *wlan_kobject);

/**
 * hdd_sysfs_mem_stats_destroy() - API to destroy
 * wlan_mem_stats
 *
 * Return: none
 */
void hdd_sysfs_mem_stats_destroy(struct kobject *wlan_kobject);
#else
static inline int
hdd_sysfs_mem_stats_create(struct kobject *wlan_kobject)
{
	return 0;
}

static inline void
hdd_sysfs_mem_stats_destroy(struct kobject *wlan_kobject)
{
}
#endif /* WLAN_SYSFS && CONFIG_WLAN_SYSFS_MEM_STATS */
#endif /* _WLAN_HDD_SYSFS_MEM_STATS */

