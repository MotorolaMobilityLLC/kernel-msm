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

#ifndef _WLAN_HDD_SYSFS_SWLM_H_
#define _WLAN_HDD_SYSFS_SWLM_H_

#ifdef WLAN_DP_FEATURE_SW_LATENCY_MGR
/**
 * hdd_sysfs_dp_swlm_create() - Create SWLM specific sysfs entry
 * @driver_kobject: Driver kobject
 *
 * Returns: none
 */
int hdd_sysfs_dp_swlm_create(struct kobject *driver_kobject);

/**
 * hdd_sysfs_dp_swlm_destroy() - Destroy SWLM specific sysfs entry
 * @driver_kobject: Driver kobject
 *
 * Returns: none
 */
void hdd_sysfs_dp_swlm_destroy(struct kobject *driver_kobject);
#else
static inline int hdd_sysfs_dp_swlm_create(struct kobject *driver_kobject)
{
	return 0;
}

static inline void hdd_sysfs_dp_swlm_destroy(struct kobject *driver_kobject)
{
}
#endif /* WLAN_DP_FEATURE_SW_LATENCY_MGR */

#endif
