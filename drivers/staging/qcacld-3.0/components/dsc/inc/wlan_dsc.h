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
 * DOC: Driver Synchronization Core (DSC) APIs for use by the driver
 * orchestration layer.
 *
 * This infrastructure accomplishes two high level goals:
 * 1) Replace ad-hoc locking/flags (hdd_init_deinit_lock,
 *    iface_change_lock, con_mode_flag, etc., etc., etc.)
 * 2) Make cds_ssr_protect() and driver state checking atomic
 *
 * These two goals are commplished in DSC via two corollary concepts:
 * 1) Transitions (as in driver state machine transitions)
 *    These are mutually exclusive, and replace ad-hoc locking
 * 2) Operations (as in operations the driver is currently servicing)
 *    These execute concurrently with other operations, and replace
 *    cds_ssr_protect(). Any active transition causes new operations to be
 *    rejected, in the same way as cds_ssr_protect/hdd_validate_context would.
 *
 * Transitions and operations are split into 3 distinct levels: driver, psoc,
 * and vdev. These levels are arranged into a tree, with a single driver at
 * the root, zero or more psocs per driver, and zero or more vdevs per psoc.
 *
 * High level transitions block transitions and operations at the same level,
 * down-tree, and up-tree. So a driver transition effectively prevents any new
 * activity in the system, while a vdev transition prevents transtitions and
 * operations on the same vdev, its parent psoc, and the driver. This also means
 * that sibling nodes can transition at the same time, e.g. one vdev going up at
 * the same time another is going down.
 */

#ifndef __WLAN_DSC_H
#define __WLAN_DSC_H

#include "wlan_dsc_driver.h"
#include "wlan_dsc_psoc.h"
#include "wlan_dsc_vdev.h"

#endif /* __WLAN_DSC_H */
