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
 * DOC: wlan_hdd_unit_test.h
 *
 * config wlan_hdd_unit_test
 */

#ifndef _WLAN_HDD_UNIT_TEST_H
#define _WLAN_HDD_UNIT_TEST_H

#ifdef WLAN_UNIT_TEST
/**
 * wlan_hdd_unit_test() - API to begin unit test on host side
 * @hdd_ctx: hdd context
 * @name: test item name
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_unit_test(struct hdd_context *hdd_ctx, const char *name);
#else
static inline int
wlan_hdd_unit_test(struct hdd_context *hdd_ctx, const char *name)
{
	return -EOPNOTSUPP;
}
#endif

#endif /* _WLAN_HDD_UNIT_TEST_H */
