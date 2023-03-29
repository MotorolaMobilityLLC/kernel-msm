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
 * DOC: Declare various struct, macros which are used for object mgmt in disa.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_DISA_OBJ_MGMT_PUBLIC_STRUCT_H_
#define _WLAN_DISA_OBJ_MGMT_PUBLIC_STRUCT_H_

struct wlan_objmgr_psoc;
struct disa_encrypt_decrypt_req_params;
struct disa_encrypt_decrypt_resp_params;

/**
 * struct wlan_disa_tx_ops - structure of tx operation function
 *	pointers for disa component
 * @disa_encrypt_decrypt_req: send encrypt/decrypt request
 * @disa_register_ev_handlers: register disa event handlers
 * @disa_unregister_ev_handlers: unregister disa event handlers
 */
struct wlan_disa_tx_ops {
	QDF_STATUS (*disa_encrypt_decrypt_req)(struct wlan_objmgr_psoc *psoc,
			struct disa_encrypt_decrypt_req_params *req);
	QDF_STATUS (*disa_register_ev_handlers)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*disa_unregister_ev_handlers)
			(struct wlan_objmgr_psoc *psoc);
};

/**
 * struct wlan_disa_rx_ops - structure of rx operation function
 *	pointers for disa component
 * @encrypt_decrypt_msg_resp: send response of encrypt/decrypt request
 */
struct wlan_disa_rx_ops {
	QDF_STATUS (*encrypt_decrypt_msg_resp)(struct wlan_objmgr_psoc *psoc,
			struct disa_encrypt_decrypt_resp_params *resp);
};
#endif /* end  of _WLAN_DISA_OBJ_MGMT_PUBLIC_STRUCT_H_ */
