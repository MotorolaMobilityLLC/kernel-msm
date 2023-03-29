/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_dp_trace.h
 *
 * Implementation for creating sysfs files:
 *
 * dp_trace
 * dump_dp_trace
 * clear_dp_trace
 */

#ifndef _WLAN_HDD_SYSFS_DP_TRACE_H
#define _WLAN_HDD_SYSFS_DP_TRACE_H

#if defined(WLAN_SYSFS) && defined(WLAN_SYSFS_DP_TRACE)

#define HDD_SYSFS_DISABLE_DP_TRACE_LIVE_MODE 0
#define HDD_SYSFS_ENABLE_DP_TRACE_LIVE_MODE 1
#define HDD_SYSFS_DUMP_DP_TRACE 2

/**
 * hdd_sysfs_dp_trace_create() - API to create dp trace related files
 * @driver_kobject: sysfs driver kobject
 *
 * file path: /sys/kernel/wifi/dp_trace
 *            /sys/kernel/wifi/dump_dp_trace
 *            /sys/kernel/wifi/clear_dp_trace
 *
 * usage:
 *      echo [arg_0] [arg_1] [arg_2]> dp_trace
 *      echo [0/1] > dump_dp_trace
 *      echo 2 [count] > dump_dp_trace
 *      cat dump_dp_trace
 *      echo 1 > clear_dp_trace
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_dp_trace_create(struct kobject *driver_kobject);

/**
 * hdd_sysfs_dp_trace_destroy() -
 *   API to destroy dp trace related files
 *
 * Return: none
 */
void
hdd_sysfs_dp_trace_destroy(struct kobject *driver_kobject);
#else
static inline int
hdd_sysfs_dp_trace_create(struct kobject *driver_kobject)
{
	return 0;
}

static inline void
hdd_sysfs_dp_trace_destroy(struct kobject *driver_kobject)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_DP_TRACE_H */
