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
#ifndef WLAN_HDD_HANG_EVENT_H
#define WLAN_HDD_HANG_EVENT_H
#include <qdf_hang_event_notifier.h>
#include <wlan_hdd_main.h>

#ifdef WLAN_HANG_EVENT
/**
 * wlan_hdd_hang_event_notifier_register() - HDD hang event notifier register
 * @hdd_ctx: HDD context
 *
 * This function registers hdd layer notifier for the hang event notifier chain.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_hang_event_notifier_register(struct hdd_context *hdd_ctx);
/**
 * wlan_hdd_hang_event_notifier_unregister() - HDD hang event notifier
 * unregister
 * @hdd_ctx: HDD context
 *
 * This function unregisters hdd layer notifier for the hang event notifier
 * chain.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_hang_event_notifier_unregister(void);
#else
static inline
QDF_STATUS wlan_hdd_hang_event_notifier_register(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_hdd_hang_event_notifier_unregister(void)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
