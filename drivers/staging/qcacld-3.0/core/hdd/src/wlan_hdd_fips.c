/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_fips.c
 *
 * WLAN Host Device Driver FIPS Certification Feature
 */

#include "osif_sync.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_fips.h"
#include "wlan_osif_request_manager.h"
#include "qdf_mem.h"
#include "sme_api.h"

#define WLAN_WAIT_TIME_FIPS 5000

/**
 * hdd_fips_context - hdd fips context
 * @status: status of response. 0: no error, -ENOMEM: unable to allocate
 *   memory for the response payload
 * @request: fips request
 * @response: fips response
 */
struct hdd_fips_context {
	int status;
	struct fips_params request;
	struct wmi_host_fips_event_param response;
};

/**
 * hdd_fips_event_dup () - duplicate a fips event
 * @dest: destination event
 * @src: source event
 *
 * Make a "deep" duplicate of a FIPS event
 *
 * Return: 0 if the event was duplicated, otherwise an error
 */
static int hdd_fips_event_dup(struct wmi_host_fips_event_param *dest,
			      const struct wmi_host_fips_event_param *src)
{
	*dest = *src;
	if  (dest->data_len) {
		dest->data = qdf_mem_malloc(dest->data_len);
		if (!dest->data)
			return -ENOMEM;

		qdf_mem_copy(dest->data, src->data, src->data_len);
	} else {
		/* make sure we don't have a rogue pointer */
		dest->data = NULL;
	}

	return 0;
}

/**
 * hdd_fips_cb () - fips response message handler
 * @cookie: hdd request cookie
 * @response: fips response parameters
 *
 * Return: none
 */
static void hdd_fips_cb(void *cookie,
			struct wmi_host_fips_event_param *response)
{
	struct osif_request *request;
	struct hdd_fips_context *context;

	hdd_enter();

	if (!response) {
		hdd_err("response is NULL");
		return;
	}

	request = osif_request_get(cookie);
	if (!request) {
		hdd_debug("Obsolete request");
		return;
	}

	hdd_debug("pdev_id %u, status %u, data_len %u",
		  response->pdev_id,
		  response->error_status,
		  response->data_len);
	qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   response->data, response->data_len);

	context = osif_request_priv(request);
	if (response->error_status) {
		context->status = -ETIMEDOUT;
	} else {
		context->status = hdd_fips_event_dup(&context->response,
						     response);
	}

	osif_request_complete(request);
	osif_request_put(request);
	hdd_exit();
}

static void hdd_fips_context_dealloc(void *priv)
{
	struct hdd_fips_context *context = priv;

	qdf_mem_free(context->response.data);
}


static int hdd_fips_validate_request(struct iw_fips_test_request *user_request,
				     uint32_t request_len)
{
	uint32_t expected_data_len;

	if (request_len < sizeof(*user_request)) {
		hdd_debug("Request len %u is too small", request_len);
		return -EINVAL;
	}

	if ((user_request->key_len != FIPS_KEY_LENGTH_128) &&
	    (user_request->key_len != FIPS_KEY_LENGTH_256)) {
		hdd_debug("Invalid key len %u", user_request->key_len);
		return -EINVAL;
	}

	expected_data_len = request_len - sizeof(*user_request);
	if (expected_data_len != user_request->data_len) {
		hdd_debug("Unexpected data_len %u for request_len %u",
			  user_request->data_len, request_len);
		return -EINVAL;
	}

	if ((user_request->mode != FIPS_ENGINE_AES_CTR) &&
	    (user_request->mode != FIPS_ENGINE_AES_MIC)) {
		hdd_debug("Invalid mode %u", user_request->mode);
		return -EINVAL;
	}

	if ((user_request->operation != FIPS_ENCRYPT_CMD) &&
	    (user_request->operation != FIPS_DECRYPT_CMD)) {
		hdd_debug("Invalid operation %u", user_request->operation);
		return -EINVAL;
	}

	return 0;
}

static int __hdd_fips_test(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	struct iw_fips_test_request *user_request;
	struct iw_fips_test_response *user_response;
	uint32_t request_len;
	int ret;
	QDF_STATUS qdf_status;
	void *cookie;
	struct osif_request *request;
	struct hdd_fips_context *context;
	struct fips_params *fips_request;
	struct wmi_host_fips_event_param *fips_response;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*context),
		.timeout_ms = WLAN_WAIT_TIME_FIPS,
		.dealloc = hdd_fips_context_dealloc,
	};

	hdd_enter_dev(dev);

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	user_request = (struct iw_fips_test_request *)extra;
	request_len = wrqu->data.length;
	ret = hdd_fips_validate_request(user_request, request_len);
	if (ret)
		return ret;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}
	context = osif_request_priv(request);
	fips_request = &context->request;
	fips_request->key = &user_request->key[0];
	fips_request->key_len = user_request->key_len;
	fips_request->data = &user_request->data[0];
	fips_request->data_len = user_request->data_len;
	fips_request->mode = user_request->mode;
	fips_request->op = user_request->operation;
	fips_request->pdev_id = WMI_PDEV_ID_1ST;

	cookie = osif_request_cookie(request);
	qdf_status = sme_fips_request(hdd_ctx->mac_handle, &context->request,
				      hdd_fips_cb, cookie);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Unable to post fips message");
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

	fips_response = &context->response;
	if (fips_response->data_len != fips_request->data_len) {
		hdd_err("Data length mismatch, got %u, expected %u",
			fips_response->data_len, fips_request->data_len);
		ret = -EINVAL;
		goto cleanup;
	}
	user_response = (struct iw_fips_test_response *)extra;
	user_response->status = context->status;
	if (user_response->status) {
		user_response->data_len = 0;
	} else {
		user_response->data_len = fips_response->data_len;
		qdf_mem_copy(user_response->data, fips_response->data,
			     fips_response->data_len);
	}

	/*
	 * By default wireless extensions private ioctls have either
	 * SET semantics (even numbered ioctls) or GET semantics (odd
	 * numbered ioctls). This is an even numbered ioctl so the SET
	 * semantics apply. This means the core kernel ioctl code took
	 * care of copying the request parameters from userspace to
	 * kernel space. However this ioctl also needs to return the
	 * response. Since the core kernel ioctl code doesn't support
	 * SET ioctls returning anything other than status, we have to
	 * explicitly copy the result to userspace.
	 */
	wrqu->data.length = sizeof(*user_response) + user_response->data_len;
	if (copy_to_user(wrqu->data.pointer, user_response, wrqu->data.length))
		ret = -EFAULT;

cleanup:
	osif_request_put(request);

	hdd_exit();
	return ret;
}

int hdd_fips_test(struct net_device *dev,
		  struct iw_request_info *info,
		  union iwreq_data *wrqu, char *extra)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_fips_test(dev, info, wrqu, extra);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
