/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Operating System Interface, Synchronization (osif_sync)
 *
 * osif_sync is a collection of APIs for synchronizing and safely interacting
 * with external processes/threads via various operating system interfaces. It
 * relies heavily on the Driver Synchronization Core, which defines the
 * synchronization primitives and behavior.
 *
 * While DSC is great at doing the required synchronization, it (by design) does
 * not address the gap between receiving a callback and involing the appropriate
 * DSC protections. For example, given an input net_device pointer from the
 * kernel, how does one safely aquire the appropriate DSC context? osif_sync
 * implements this logic via wrapping DSC APIs with a registration mechanism.
 *
 * For example, after the creation of a new dsc_vdev context, osif_sync allows
 * it to be registered with a specific net_device pointer as a key. Future
 * operations against this net_device can use this pointer to atomically lookup
 * the DSC context, and start either a DSC transition or DSC operation. If this
 * is successful, the operation continues and is guaranteed protected from major
 * state changes. Otherwise, the operation attempt is rejected.
 *
 * See also: wlan_dsc.h
 */

#ifndef __OSIF_SYNC_H
#define __OSIF_SYNC_H

#include "osif_driver_sync.h"
#include "osif_psoc_sync.h"
#include "osif_vdev_sync.h"

/**
 * osif_sync_init() - global initializer
 *
 * Return: None
 */
void osif_sync_init(void);

/**
 * osif_sync_deinit() - global de-initializer
 *
 * Return: None
 */
void osif_sync_deinit(void);

#endif /* __OSIF_SYNC_H */

