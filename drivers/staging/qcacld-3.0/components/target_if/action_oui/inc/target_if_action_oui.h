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
 * DOC: Declare various api/struct which shall be used
 * by action_oui component for wmi command path.
 */

#ifndef _TARGET_IF_ACTION_OUI_H_
#define _TARGET_IF_ACTION_OUI_H_

#include "target_if.h"
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include "wlan_action_oui_tgt_api.h"
#include "wlan_action_oui_public_struct.h"

/**
 * target_if_action_oui_register_tx_ops() - Register action_oui component TX OPS
 * @tx_ops: action_oui transmit ops
 *
 * Return: None
 */
void
target_if_action_oui_register_tx_ops(struct action_oui_tx_ops *tx_ops);

#endif /* _TARGET_IF_ACTION_OUI_H_ */
