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
 * DOC: wlan_hdd_debugfs_unit_test.h
 *
 * WLAN Host Device Driver implementation to create debugfs
 * unit_test_host/unit_test_target
 */

#ifndef _WLAN_HDD_DEBUGFS_UNIT_TEST_H
#define _WLAN_HDD_DEBUGFS_UNIT_TEST_H

#if defined(WLAN_DEBUGFS) && defined(WLAN_UNIT_TEST)
/**
 * hdd_debugfs_unit_test_host_create() - API to create unit_test_target
 * @hdd_ctx: hdd context
 *
 * this file is created per driver.
 * file path:  /sys/kernel/debug/wlan_xx/unit_test_host
 *                (wlan_xx is driver name)
 * usage:
 *      echo 'all'>unit_test_host
 *      echo 'qdf_periodic_work'>unit_test_host
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_debugfs_unit_test_host_create(struct hdd_context *hdd_ctx);
#else
static inline int
wlan_hdd_debugfs_unit_test_host_create(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif
#endif /* _WLAN_HDD_DEBUGFS_UNIT_TEST_H */
