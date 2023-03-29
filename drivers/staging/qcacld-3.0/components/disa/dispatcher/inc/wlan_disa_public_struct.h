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
 * DOC: Declare various struct, macros which shall be used in the DISA
 * component.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_DISA_PUBLIC_STRUCT_H_
#define _WLAN_DISA_PUBLIC_STRUCT_H_

#include <qdf_types.h>

#define MAC_MAX_KEY_LENGTH 32
#define MAC_PN_LENGTH 8
#define MAX_MAC_HEADER_LEN 32
#define MIN_MAC_HEADER_LEN 24

/**
 * struct disa_encrypt_decrypt_req_params - disa encrypt request
 * @vdev_id: virtual device id
 * @key_flag: This indicates firmware to encrypt/decrypt payload
 *    see ENCRYPT_DECRYPT_FLAG
 * @key_idx: Index used in storing key
 * @key_cipher: cipher used for encryption/decryption
 *    Eg: see WMI_CIPHER_AES_CCM for CCMP
 * @key_len: length of key data
 * @key_txmic_len: length of Tx MIC
 * @key_rxmic_len: length of Rx MIC
 * @key_data: Key
 * @pn: packet number
 * @mac_header: MAC header
 * @data_len: length of data
 * @data: pointer to payload
 */
struct disa_encrypt_decrypt_req_params {
	uint32_t vdev_id;
	uint8_t key_flag;
	uint32_t key_idx;
	uint32_t key_cipher;
	uint32_t key_len;
	uint32_t key_txmic_len;
	uint32_t key_rxmic_len;
	uint8_t key_data[MAC_MAX_KEY_LENGTH];
	uint8_t pn[MAC_PN_LENGTH];
	uint8_t mac_header[MAX_MAC_HEADER_LEN];
	uint32_t data_len;
	uint8_t *data;
};

/**
 * struct disa_encrypt_decrypt_resp_params - disa encrypt response
 * @vdev_id: vdev id
 * @status: status
 * @data_length: data length
 * @data: data pointer
 */
struct disa_encrypt_decrypt_resp_params {
	uint32_t vdev_id;
	int32_t status;
	uint32_t data_len;
	uint8_t *data;
};

typedef void (*encrypt_decrypt_resp_callback)(void *cookie,
	struct disa_encrypt_decrypt_resp_params *resp) ;
#endif /* end  of _WLAN_DISA_PUBLIC_STRUCT_H_ */

