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

/**
 * DOC: wlan_hdd_sysfs_modify_acl.h
 *
 * implementation for creating sysfs file modify_acl
 */

#ifndef _WLAN_HDD_SYSFS_MODIFY_ACL_H
#define _WLAN_HDD_SYSFS_MODIFY_ACL_H

#if defined(WLAN_SYSFS)
/**
 * hdd_sysfs_modify_acl_create() - API to create modify_acl
 * @adapter: hdd adapter
 *
 * this file is created for sap adapter.
 * file path: /sys/class/net/wlan_xx/modify_acl
 *                (wlan_xx is adapter name)
 * usage:
 *      echo <6 octet mac addr> <list type> <cmd type> > modify_acl
 *      eg:  echo 0x00 0x11 0x22 0x33 0x44 0x55 0 0 >modify_acls
 * Return: 0 on success and errno on failure
 */
int hdd_sysfs_modify_acl_create(struct hdd_adapter *adapter);

/**
 * hdd_sysfs_modify_acl_destroy() -
 *   API to destroy modify_acl sys file
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_sysfs_modify_acl_destroy(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_modify_acl_create(struct hdd_adapter *adapter)
{
	return 0;
}

static inline void
hdd_sysfs_modify_acl_destroy(struct hdd_adapter *adapter)
{
}
#endif
#endif /* #ifndef _WLAN_HDD_SYSFS_MODIFY_ACL_H */
