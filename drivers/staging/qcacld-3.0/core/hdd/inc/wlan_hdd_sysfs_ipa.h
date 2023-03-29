/*
 * Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
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
#ifndef WLAN_HDD_SYSFS_IPA_H
#define WLAN_HDD_SYSFS_IPA_H

#if defined(WLAN_SYSFS) && defined(IPA_OFFLOAD)

enum ipa_debug_cmd {
	IPA_UC_STAT = 1,
	IPA_UC_INFO,
	IPA_UC_RT_DEBUG_HOST_DUMP,
	IPA_DUMP_INFO,
};

/**
 * hdd_sysfs_ipa_create(): Initialize ipa specific sysfs file
 * @adapter: os if adapter
 *
 * Function to initialize ipa specific mode syfs files.
 *
 * Return: NONE
 */
void hdd_sysfs_ipa_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_ipa_destroy(): Remove ipucstat specific sysfs file
 * @adapter: os if adapter
 *
 * Function to remove ipa specific mode syfs files.
 *
 * Return: NONE
 */
void hdd_sysfs_ipa_destroy(struct hdd_adapter *adapter);
#else
static inline
void hdd_sysfs_ipa_create(struct hdd_adapter *adapter)
{
}

static inline
void hdd_sysfs_ipa_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif
