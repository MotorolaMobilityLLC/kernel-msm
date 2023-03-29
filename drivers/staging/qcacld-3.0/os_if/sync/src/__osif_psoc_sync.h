/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef ____OSIF_PSOC_SYNC_H
#define ____OSIF_PSOC_SYNC_H

#include "linux/device.h"
#include "qdf_status.h"
#include "wlan_dsc_vdev.h"

/**
 * osif_psoc_sync_init() - global initializer
 *
 * Return: None
 */
void osif_psoc_sync_init(void);

/**
 * osif_psoc_sync_deinit() - global de-initializer
 *
 * Return: None
 */
void osif_psoc_sync_deinit(void);

/**
 * osif_psoc_sync_dsc_vdev_create() - create a dsc_vdev and attach it to a
 *	dsc_psoc keyed by @dev
 * @dev: the device to key off of
 * @out_dsc_vdev: output pointer parameter for the new dsc_vdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS osif_psoc_sync_dsc_vdev_create(struct device *dev,
					  struct dsc_vdev **out_dsc_vdev);

#endif /* ____OSIF_PSOC_SYNC_H */

