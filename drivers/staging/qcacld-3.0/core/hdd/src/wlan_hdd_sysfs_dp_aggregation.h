/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_dp_aggregation.h
 *
 * implementation for creating sysfs files:
 *
 * dp_aggregation
 */

#ifndef _WLAN_HDD_SYSFS_DP_AGGREGATION_H
#define _WLAN_HDD_SYSFS_DP_AGGREGATION_H

#if defined(WLAN_SYSFS)
/**
 * hdd_sysfs_dp_aggregation_create() - API to create dp aggregation
 *  related sysfs entry
 * @driver_kobject: sysfs driver kobject
 *
 * file path: /sys/kernel/wifi/dp_aggregation
 *
 * usage:
 *      echo [0/1] > dp_aggregation
 *
 * Return: 0 on success and errno on failure
 */
int
hdd_sysfs_dp_aggregation_create(struct kobject *drv_kobj);

/**
 * hdd_sysfs_dp_aggregation_destroy() - API to destroy dp aggregation
 *  related sysfs entry
 * @driver_kobject: sysfs driver kobject
 *
 * Return: None
 */
void
hdd_sysfs_dp_aggregation_destroy(struct kobject *drv_kobj);
#else
static inline int
hdd_sysfs_dp_aggregation_create(struct kobject *drv_kobj)
{
	return 0;
}

static inline void
hdd_sysfs_dp_aggregation_destroy(struct kobject *drv_kobj)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_DP_AGGREGATION_H */
