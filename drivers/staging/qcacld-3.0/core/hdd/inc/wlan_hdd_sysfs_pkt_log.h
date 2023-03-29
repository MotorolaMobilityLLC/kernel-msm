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
#ifndef WLAN_HDD_SYSFS_PKT_LOG_H
#define WLAN_HDD_SYSFS_PKT_LOG_H

 #if defined(WLAN_SYSFS) && !defined(REMOVE_PKT_LOG)

/**
 * hdd_sysfs_pktlog_create(): Initialize pktlog specific sysfs file
 * @driver_kobject: Driver Kobject
 *
 * Function to initialize pktlog specific mode syfs files.
 *
 * Return: NONE
 */
void hdd_sysfs_pktlog_create(struct kobject *driver_kobject);

/**
 * hdd_sysfs_pktlog_destroy(): Remove pktlog sysfs file
 * @driver_kobject: Driver Kobject
 *
 * Function to remove pktlog specific mode syfs files.
 *
 * Return: NONE
 */
void hdd_sysfs_pktlog_destroy(struct kobject *driver_kobject);
#else
static inline
void hdd_sysfs_pktlog_create(struct kobject *driver_kobject)
{
}

static inline
void hdd_sysfs_pktlog_destroy(struct kobject *driver_kobject)
{
}
#endif
#endif
