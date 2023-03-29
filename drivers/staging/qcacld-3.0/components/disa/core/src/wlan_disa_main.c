/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: Implement various api / helper function which shall be used for
 * DISA user and target interface.
 */

#include "wlan_disa_main.h"
#include "wlan_disa_obj_mgmt_public_struct.h"
#include "wlan_disa_tgt_api.h"

static struct wlan_disa_ctx *gp_disa_ctx;

QDF_STATUS disa_allocate_ctx(void)
{
	/* If it is already created, ignore */
	if (gp_disa_ctx) {
		disa_debug("already allocated disa_ctx");
		return QDF_STATUS_SUCCESS;
	}

	/* allocate DISA ctx */
	gp_disa_ctx = qdf_mem_malloc(sizeof(*gp_disa_ctx));
	if (!gp_disa_ctx)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&gp_disa_ctx->lock);

	return QDF_STATUS_SUCCESS;
}

void disa_free_ctx(void)
{
	if (!gp_disa_ctx) {
		disa_err("disa ctx is already freed");
		QDF_ASSERT(0);
		return;
	}
	qdf_spinlock_destroy(&gp_disa_ctx->lock);
	qdf_mem_free(gp_disa_ctx);
	gp_disa_ctx = NULL;
}

struct wlan_disa_ctx *disa_get_context(void)
{
	return gp_disa_ctx;
}

QDF_STATUS disa_core_encrypt_decrypt_req(struct wlan_objmgr_psoc *psoc,
		struct disa_encrypt_decrypt_req_params *req,
		encrypt_decrypt_resp_callback cb,
		void *cookie)
{
	struct wlan_disa_ctx *disa_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	DISA_ENTER();
	disa_ctx = disa_get_context();
	if (!disa_ctx) {
		disa_err("DISA context is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&disa_ctx->lock);
	if (!disa_ctx->request_active) {
		disa_ctx->callback = cb;
		disa_ctx->callback_context = cookie;
		disa_ctx->request_active = true;
	} else {
		status = QDF_STATUS_E_INVAL;
	}
	qdf_spin_unlock_bh(&disa_ctx->lock);

	if (status != QDF_STATUS_SUCCESS) {
		disa_err("A request is already active!");
		return status;
	}

	status = disa_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		disa_err("DISA cannot get the reference out of psoc");
		return status;
	}

	status = tgt_disa_encrypt_decrypt_req(psoc, req);
	disa_psoc_put_ref(psoc);

	DISA_EXIT();
	return status;
}
