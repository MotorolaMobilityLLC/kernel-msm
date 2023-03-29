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
 * by ipa component for wmi cmd (tx path)
 */

#ifndef _TARGET_IF_IPA_H_
#define _TARGET_IF_IPA_H_

#ifdef IPA_OFFLOAD

#include "wlan_ipa_public_struct.h"

/**
 * target_if_ipa_register_tx_ops() - Register IPA component TX OPS
 * @ipa_tx_op: IPA if transmit op
 *
 * Return: None
 */
void target_if_ipa_register_tx_ops(ipa_uc_offload_control_req *ipa_tx_op);

#endif /* IPA_OFFLOAD */
#endif /* _TARGET_IF_IPA_H_ */

