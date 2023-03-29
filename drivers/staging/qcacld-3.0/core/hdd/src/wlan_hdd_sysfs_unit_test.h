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
 * DOC: wlan_hdd_sysfs_unit_test.h
 *
 * implementation for creating sysfs file unit_test_target
 */

#ifndef _WLAN_HDD_SYSFS_UNIT_TEST_H
#define _WLAN_HDD_SYSFS_UNIT_TEST_H

#if defined(WLAN_SYSFS)
/**
 * hdd_sysfs_unit_test_target_create() - API to create unit_test_target file
 * @adapter: hdd adapter
 *
 * this file is created per adapter.
 * file path: /sys/class/net/wlan_xx/unit_test_target
 *                (wlan_xx is adapter name)
 * usage:
 *      echo [module_id] [arg_num] [arg_0] [arg_xx] ... >unit_test_target
 *      echo 5 2 2 1 >unit_test_target
 *
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_unit_test_target_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_unit_test_target_destroy() -
 *   API to destroy unit_test_target sys file
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_unit_test_target_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_unit_test_target_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_unit_test_target_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_UNIT_TEST_H */
