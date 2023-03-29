/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_disa.c
 *
 * WLAN Host Device Driver file for DISA certification
 *
 */

#include "wlan_hdd_disa.h"
#include "osif_sync.h"
#include "wlan_disa_ucfg_api.h"
#include "wlan_osif_request_manager.h"
#include "sme_api.h"
#include <qca_vendor.h>

#define WLAN_WAIT_TIME_ENCRYPT_DECRYPT 1000


/**
 * hdd_encrypt_decrypt_msg_context - hdd encrypt/decrypt message context
 * @status: status of response. 0: no error, -ENOMEM: unable to allocate
 *   memory for the response payload
 * @request: encrypt/decrypt request
 * @response: encrypt/decrypt response
 */
struct hdd_encrypt_decrypt_msg_context {
	int status;
	struct disa_encrypt_decrypt_req_params request;
	struct disa_encrypt_decrypt_resp_params response;
};

/**
 * hdd_encrypt_decrypt_msg_cb () - encrypt/decrypt response message handler
 * @cookie: hdd request cookie
 * @resp: encrypt/decrypt response parameters
 *
 * Return: none
 */
static void hdd_encrypt_decrypt_msg_cb(void *cookie,
	struct disa_encrypt_decrypt_resp_params *resp)
{
	struct osif_request *request;
	struct hdd_encrypt_decrypt_msg_context *context;

	hdd_enter();

	if (!resp) {
		hdd_err("rsp params is NULL");
		return;
	}

	request = osif_request_get(cookie);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	print_hex_dump(KERN_INFO, "Data in hdd_encrypt_decrypt_msg_cb: ",
		DUMP_PREFIX_NONE, 16, 1,
		resp->data,
		resp->data_len, 0);

	hdd_debug("vdev_id: %d status:%d data_length: %d",
		resp->vdev_id,
		resp->status,
		resp->data_len);

	context = osif_request_priv(request);
	context->response = *resp;
	context->status = 0;
	if (resp->data_len) {
		context->response.data =
			qdf_mem_malloc(sizeof(uint8_t) *
				resp->data_len);
		if (!context->response.data) {
			context->status = -ENOMEM;
		} else {
			qdf_mem_copy(context->response.data,
				     resp->data,
				     resp->data_len);
		}
	} else {
		/* make sure we don't have a rogue pointer */
		context->response.data = NULL;
	}

	osif_request_complete(request);
	osif_request_put(request);
	hdd_exit();
}

/**
 * hdd_post_encrypt_decrypt_msg_rsp () - send encrypt/decrypt data to user space
 * @encrypt_decrypt_rsp_params: encrypt/decrypt response parameters
 *
 * Return: none
 */
static int hdd_post_encrypt_decrypt_msg_rsp(struct hdd_context *hdd_ctx,
	struct disa_encrypt_decrypt_resp_params *resp)
{
	struct sk_buff *skb;
	uint32_t nl_buf_len;

	hdd_enter();

	nl_buf_len = resp->data_len + NLA_HDRLEN;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (resp->data_len) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA,
				resp->data_len, resp->data)) {
			hdd_err("put fail");
			goto nla_put_failure;
		}
	}

	cfg80211_vendor_cmd_reply(skb);
	hdd_exit();
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

const struct nla_policy
encrypt_decrypt_policy[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_NEEDS_DECRYPTION] = {
		.type = NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_CIPHER] = {
		.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_KEYID] = {
		.type = NLA_U8},
};

/**
 * hdd_fill_encrypt_decrypt_params () - parses data from user space
 * and fills encrypt/decrypt parameters
 * @encrypt_decrypt_params: encrypt/decrypt request parameters
 * @adapter : adapter context
 * @data: Pointer to data
 * @data_len: Data length
 *
 Return: 0 on success, negative errno on failure
 */
static int
hdd_fill_encrypt_decrypt_params(struct disa_encrypt_decrypt_req_params
				*encrypt_decrypt_params,
				struct hdd_adapter *adapter,
				const void *data,
				int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_MAX + 1];
	uint8_t len, mac_hdr_len;
	uint8_t *tmp;
	uint8_t fc[2];

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_MAX,
				    data, data_len, encrypt_decrypt_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	encrypt_decrypt_params->vdev_id = adapter->vdev_id;
	hdd_debug("vdev_id: %d", encrypt_decrypt_params->vdev_id);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_NEEDS_DECRYPTION]) {
		hdd_err("attr flag NEEDS_DECRYPTION not present");
		encrypt_decrypt_params->key_flag = WMI_ENCRYPT;
	} else {
		hdd_err("attr flag NEEDS_DECRYPTION present");
		encrypt_decrypt_params->key_flag = WMI_DECRYPT;
	}
	hdd_debug("Key flag: %d", encrypt_decrypt_params->key_flag);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_KEYID]) {
		hdd_err("attr key id failed");
		return -EINVAL;
	}
	encrypt_decrypt_params->key_idx = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_KEYID]);
	hdd_debug("Key Idx: %d", encrypt_decrypt_params->key_idx);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_CIPHER]) {
		hdd_err("attr Cipher failed");
		return -EINVAL;
	}
	encrypt_decrypt_params->key_cipher = nla_get_u32(tb
		    [QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_CIPHER]);
	hdd_debug("key_cipher: %d", encrypt_decrypt_params->key_cipher);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_TK]) {
		hdd_err("attr TK failed");
		return -EINVAL;
	}
	encrypt_decrypt_params->key_len =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_TK]);
	if (!encrypt_decrypt_params->key_len) {
		hdd_err("Invalid TK length");
		return -EINVAL;
	}
	hdd_debug("Key len: %d", encrypt_decrypt_params->key_len);

	if (encrypt_decrypt_params->key_len > SIR_MAC_MAX_KEY_LENGTH)
		encrypt_decrypt_params->key_len = SIR_MAC_MAX_KEY_LENGTH;

	tmp = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_TK]);

	qdf_mem_copy(encrypt_decrypt_params->key_data, tmp,
			encrypt_decrypt_params->key_len);

	print_hex_dump(KERN_INFO, "Key : ", DUMP_PREFIX_NONE, 16, 1,
			&encrypt_decrypt_params->key_data,
			encrypt_decrypt_params->key_len, 0);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_PN]) {
		hdd_err("attr PN failed");
		return -EINVAL;
	}
	len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_PN]);
	if (!len || len > sizeof(encrypt_decrypt_params->pn)) {
		hdd_err("Invalid PN length %u", len);
		return -EINVAL;
	}

	tmp = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_PN]);

	qdf_mem_copy(encrypt_decrypt_params->pn, tmp, len);

	print_hex_dump(KERN_INFO, "PN received : ", DUMP_PREFIX_NONE, 16, 1,
			&encrypt_decrypt_params->pn, len, 0);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA]) {
		hdd_err("attr header failed");
		return -EINVAL;
	}
	len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA]);
	if (len < MIN_MAC_HEADER_LEN) {
		hdd_err("Invalid header and payload length %u", len);
		return -EINVAL;
	}

	hdd_debug("Header and Payload length: %d", len);

	tmp = nla_data(tb[QCA_WLAN_VENDOR_ATTR_ENCRYPTION_TEST_DATA]);

	print_hex_dump(KERN_INFO, "Header and Payload received: ",
			DUMP_PREFIX_NONE, 16, 1,
			tmp, len, 0);

	mac_hdr_len = MIN_MAC_HEADER_LEN;

	/*
	 * Check to find out address 4. Address 4 is present if ToDS and FromDS
	 * are 1 and data representation is little endian.
	 */
	fc[1] = *tmp;
	fc[0] = *(tmp + 1);
	if ((fc[0] & 0x03) == 0x03) {
		hdd_err("Address 4 is present");
		mac_hdr_len += QDF_MAC_ADDR_SIZE;
	}

	/*
	 * Check to find out Qos control field. Qos control field is present
	 * if msb of subtype field is 1 and data representation is
	 * little endian.
	 */
	if (fc[1] & 0x80) {
		hdd_err("Qos control is present");
		mac_hdr_len += QOS_CONTROL_LEN;
	}

	hdd_debug("mac_hdr_len: %d", mac_hdr_len);

	if (len < mac_hdr_len) {
		hdd_err("Invalid header and payload length %u", len);
		return -EINVAL;
	}
	qdf_mem_copy(encrypt_decrypt_params->mac_header,
			tmp, mac_hdr_len);

	print_hex_dump(KERN_INFO, "Header received in request: ",
			DUMP_PREFIX_NONE, 16, 1,
			encrypt_decrypt_params->mac_header,
			mac_hdr_len, 0);

	encrypt_decrypt_params->data_len =
			len - mac_hdr_len;

	hdd_debug("Payload length: %d", encrypt_decrypt_params->data_len);

	if (encrypt_decrypt_params->data_len) {
		encrypt_decrypt_params->data =
			qdf_mem_malloc(sizeof(uint8_t) *
				encrypt_decrypt_params->data_len);

		if (!encrypt_decrypt_params->data)
			return -ENOMEM;

		qdf_mem_copy(encrypt_decrypt_params->data,
			tmp + mac_hdr_len,
			encrypt_decrypt_params->data_len);

		print_hex_dump(KERN_INFO, "Data received in request: ",
			DUMP_PREFIX_NONE, 16, 1,
			encrypt_decrypt_params->data,
			encrypt_decrypt_params->data_len, 0);
	}

	return 0;
}

static void hdd_encrypt_decrypt_context_dealloc(void *priv)
{
	struct hdd_encrypt_decrypt_msg_context *context = priv;

	qdf_mem_free(context->request.data);
	qdf_mem_free(context->response.data);
}

/**
 * hdd_encrypt_decrypt_msg () - process encrypt/decrypt message
 * @adapter : adapter context
 * @hdd_ctx: hdd context
 * @data: Pointer to data
 * @data_len: Data length
 *
 Return: 0 on success, negative errno on failure
 */
static int hdd_encrypt_decrypt_msg(struct hdd_adapter *adapter,
				   struct hdd_context *hdd_ctx,
				   const void *data,
				   int data_len)
{
	QDF_STATUS qdf_status;
	int ret;
	void *cookie;
	struct osif_request *request;
	struct hdd_encrypt_decrypt_msg_context *context;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*context),
		.timeout_ms = WLAN_WAIT_TIME_ENCRYPT_DECRYPT,
		.dealloc = hdd_encrypt_decrypt_context_dealloc,
	};

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}
	context = osif_request_priv(request);

	ret = hdd_fill_encrypt_decrypt_params(&context->request, adapter,
					      data, data_len);
	if (ret)
		goto cleanup;

	cookie = osif_request_cookie(request);

	qdf_status = ucfg_disa_encrypt_decrypt_req(hdd_ctx->psoc,
				&context->request,
				hdd_encrypt_decrypt_msg_cb,
				cookie);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Unable to post encrypt/decrypt message");
		ret = -EINVAL;
		goto cleanup;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("Target response timed out");
		goto cleanup;
	}

	ret = context->status;
	if (ret) {
		hdd_err("Target response processing failed");
		goto cleanup;
	}

	ret = hdd_post_encrypt_decrypt_msg_rsp(hdd_ctx, &context->response);
	if (ret)
		hdd_err("Failed to post encrypt/decrypt message response");

cleanup:
	osif_request_put(request);

	hdd_exit();
	return ret;
}

/**
 * __wlan_hdd_cfg80211_encrypt_decrypt_msg () - Encrypt/Decrypt msg
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_encrypt_decrypt_msg(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = NULL;
	int ret;
	bool is_bmps_enabled;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	ucfg_mlme_is_bmps_enabled(hdd_ctx->psoc, &is_bmps_enabled);
	if (is_bmps_enabled) {
		hdd_debug("DISA is not supported when PS is enabled");
		return -EINVAL;
	}

	ret = hdd_encrypt_decrypt_msg(adapter, hdd_ctx, data, data_len);

	return ret;
}

/**
 * wlan_hdd_cfg80211_encrypt_decrypt_msg () - Encrypt/Decrypt msg
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_encrypt_decrypt_msg(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_encrypt_decrypt_msg(wiphy, wdev,
							data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
