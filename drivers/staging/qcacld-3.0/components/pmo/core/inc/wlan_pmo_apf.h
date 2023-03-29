/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC: Android Packet Filter (APF) headers for PMO
 */

#ifndef __WLAN_PMO_APF_H
#define __WLAN_PMO_APF_H

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "qdf_types.h"
#include "wlan_objmgr_psoc_obj.h"

/**
 * pmo_get_apf_instruction_size() - get the current APF instruction size
 * @psoc: the psoc to query
 *
 * Return: APF instruction size
 */
uint32_t pmo_get_apf_instruction_size(struct wlan_objmgr_psoc *psoc);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* __WLAN_PMO_APF_H */
