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
/**
 * DOC: Declare public API for blacklist manager to interact with target/WMI
 */

#ifndef _WLAN_BLM_TGT_API_H
#define _WLAN_BLM_TGT_API_H

#include "wlan_blm_main.h"

/**
 * tgt_blm_send_reject_list_to_fw() - API to send the reject ap list to FW.
 * @pdev: pdev object
 * @reject_params: Reject params contains the bssid list, and num of bssids
 *
 * This API will send the reject AP list maintained by the blacklist manager
 * to the target.
 *
 * Return: QDF status
 */
QDF_STATUS
tgt_blm_send_reject_list_to_fw(struct wlan_objmgr_pdev *pdev,
			       struct reject_ap_params *reject_params);

#endif
