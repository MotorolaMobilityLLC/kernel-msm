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
 * DOC: wlan_hdd_sysfs_suspend_resume.h
 *
 * implementation for creating sysfs file crash_inject
 */

#ifndef _WLAN_HDD_SYSFS_SUSPEND_RESUME_H
#define _WLAN_HDD_SYSFS_SUSPEND_RESUME_H

#if defined(WLAN_SYSFS) && defined(WLAN_SUSPEND_RESUME_TEST)
/**
 * hdd_sysfs_suspend_create() - API to create wlan_suspend
 * @adapter: hdd adapter
 *
 * this file is created per adapter.
 * file path: /sys/class/net/wlan_xx/wlan_suspend
 *                (wlan_xx is adapter name)
 * usage:
 *      echo [arg_0] [arg_1] > wlan_suspend
 *            arg_0 from enum wow_interface_pause
 *            arg_1 from enum wow_resume_trigger
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_suspend_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_suspend_destroy() -
 *   API to destroy wlan_suspend sys file
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_suspend_destroy(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_resume_create() - API to create wlan_resume
 * @adapter: hdd adapter
 *
 * this file is created per adapter.
 * file path: /sys/class/net/wlan_xx/wlan_resume
 *                (wlan_xx is adapter name)
 * usage:
 *		echo > wlan_resume
 *
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_resume_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_resume_destroy() -
 *   API to destroy wlan_resume sys file
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_resume_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_suspend_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_suspend_destroy(struct hdd_adapter *adapter)
{
}

static inline int
hdd_sysfs_resume_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_resume_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_SUSPEND_RESUME_H */
