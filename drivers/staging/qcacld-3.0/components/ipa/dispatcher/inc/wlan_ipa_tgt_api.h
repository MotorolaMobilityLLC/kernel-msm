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
 * DOC: Declare public API for wlan ipa to interact with target/WMI
 */

#ifndef _WLAN_IPA_TGT_API_H_
#define _WLAN_IPA_TGT_API_H_

#include "wlan_ipa_public_struct.h"

/**
 * tgt_ipa_uc_offload_enable_disable() - send ipa offload control to target if
 * @pdev: objmgr pdev object
 * @req: ipa offload control request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_ipa_uc_offload_enable_disable(struct wlan_objmgr_pdev *pdev,
				struct ipa_uc_offload_control_params *req);
#endif /* _WLAN_IPA_TGT_API_H_ */
