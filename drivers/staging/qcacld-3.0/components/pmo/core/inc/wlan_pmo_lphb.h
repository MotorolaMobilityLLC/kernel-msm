/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: Declare low power heart beat offload feature API's
 */

#ifndef _WLAN_PMO_LPHB_H_
#define _WLAN_PMO_LPHB_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_lphb_public_struct.h"

/**
 * pmo_core_lphb_config_req() - API to configure lphb request
 * @psoc: objmgr psoc handle
 * @lphb_req: low power heart beat configuration request
 * @lphb_cb_ctx: low power heart beat context
 * @callback: osif callback which need to be called when host get lphb event
 *
 * API to configure lphb request
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_lphb_config_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_req *lphb_req, void *lphb_cb_ctx,
		pmo_lphb_callback callback);

/**
 * pmo_core_apply_lphb(): apply cached LPHB settings
 * @psoc: objmgr psoc handle
 *
 * LPHB cache, if any item was enabled, should be
 * applied.
 */
void pmo_core_apply_lphb(struct wlan_objmgr_psoc *psoc);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_LPHB_H_ */
