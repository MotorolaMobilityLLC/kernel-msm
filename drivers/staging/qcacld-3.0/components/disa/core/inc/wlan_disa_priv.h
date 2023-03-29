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
  * DOC: Declare various struct, macros which are used privately in DISA
  * component.
  *
  * Note: This file shall not contain public API's prototype/declarations.
  *
  */

#ifndef _WLAN_DISA_PRIV_STRUCT_H_
#define _WLAN_DISA_PRIV_STRUCT_H_

#include <qdf_lock.h>
#include "wlan_disa_public_struct.h"

/**
 * struct disa_psoc_priv_obj -psoc specific user configuration required for disa
 *
 * @disa_rx_ops: rx operations for disa
 * @disa_tx_ops: tx operations for disa
 * @disa_psoc_lock: spin lock for disa psoc priv ctx
 */
struct disa_psoc_priv_obj {
	struct wlan_disa_rx_ops disa_rx_ops;
	struct wlan_disa_tx_ops disa_tx_ops;
	qdf_spinlock_t lock;
};

/**
 * struct wlan_disa_ctx - disa context for single command
 *
 * @callback: hdd callback for disa encrypt/decrypt resp
 * @callback_context: context for the callback
 * @lock: spin lock for disa context
 */
struct wlan_disa_ctx {
	encrypt_decrypt_resp_callback callback;
	void *callback_context;
	bool request_active;
	qdf_spinlock_t lock;
};

#endif /* end  of _WLAN_DISA_PRIV_STRUCT_H_ */
