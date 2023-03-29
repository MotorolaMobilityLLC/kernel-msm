/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Declare various struct, macros which shall be used in
 * pmo gtk related feature.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_GTK_PUBLIC_STRUCT_H
#define _WLAN_PMO_GTK_PUBLIC_STRUCT_H

#include "wlan_pmo_common_public_struct.h"

#define PMO_GTK_OFFLOAD_ENABLE  0
#define PMO_GTK_OFFLOAD_DISABLE 1
#define PMO_KEK_LEN 64
#define PMO_KCK_LEN 32
#define PMO_REPLAY_COUNTER_LEN 8
#define PMO_MAC_MAX_KEY_LENGTH 32
#define PMO_IGTK_PN_SIZE 6

/**
 * struct pmo_gtk_req - pmo gtk request
 * @flags: optional flags
 * @kck: Key confirmation key
 * @kck_len: Key confirmation key length
 * @kek: key encryption key
 * @kek_len: KEK Length
 * @replay_counter: replay_counter
 * @bssid: bssid
 * @is_fils_connection: is current connection with peer FILS or not.
 */
struct pmo_gtk_req {
	uint32_t flags;
	uint8_t kck[PMO_KCK_LEN];
	uint8_t kck_len;
	uint8_t kek[PMO_KEK_LEN];
	uint32_t kek_len;
	uint64_t replay_counter;
	struct qdf_mac_addr bssid;
	bool is_fils_connection;
};

/**
 * struct pmo_gtk_rsp_params - pmo gtk response
 * @psoc: objmgr psoc
 * @vdev_id: vdev id on which arp offload needed
 * @status_flag: status flags
 * @refresh_cnt: number of successful GTK refresh exchanges since SET operation
 * @igtk_key_index: igtk key index
 * @igtk_key_length: igtk key length
 * @igtk_key_rsc: igtk key index
 * @igtk_key: igtk key length
 */
struct pmo_gtk_rsp_params {
	uint8_t  vdev_id;
	uint32_t status_flag;
	uint32_t refresh_cnt;
	uint64_t replay_counter;
	uint8_t igtk_key_index;
	uint8_t igtk_key_length;
	uint8_t igtk_key_rsc[PMO_IGTK_PN_SIZE];
	uint8_t igtk_key[PMO_MAC_MAX_KEY_LENGTH];
	struct qdf_mac_addr bssid;
};

/**
 * typedef for gtk response callback
 */
typedef void (*pmo_gtk_rsp_callback)(void *callback_context,
		struct pmo_gtk_rsp_params *gtk_rsp);

/**
 * struct pmo_gtk_rsp_req -gtk respsonse request
 * @callback: client callback for providing gtk resposne when fwr send event
 * @callback_context: client callback response
 */
struct pmo_gtk_rsp_req {
	pmo_gtk_rsp_callback callback;
	void *callback_context;
};

#endif /* end  of _WLAN_PMO_GTK_PUBLIC_STRUCT_H */

