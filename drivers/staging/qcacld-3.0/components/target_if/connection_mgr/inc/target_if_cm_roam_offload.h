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
 * DOC: This file contains definitions for target_if roaming offload.
 */

#ifndef TARGET_IF_CM_ROAM_OFFLOAD_H__
#define TARGET_IF_CM_ROAM_OFFLOAD_H__

#include "wlan_cm_roam_public_struct.h"

/**
 * target_if_cm_roam_register_tx_ops  - Target IF API to register roam
 * related tx op.
 * @tx_ops: Pointer to tx ops fp struct
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_cm_roam_register_tx_ops(struct wlan_cm_roam_tx_ops *tx_ops);
#endif
