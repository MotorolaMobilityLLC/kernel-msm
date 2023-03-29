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
 * DOC: declare various api which shall be used by
 * DISA user configuration and target interface
 */

#ifndef _WLAN_DISA_MAIN_H_
#define _WLAN_DISA_MAIN_H_

#include "wlan_disa_public_struct.h"
#include "wlan_disa_obj_mgmt_public_struct.h"
#include "wlan_disa_priv.h"
#include "wlan_disa_objmgr.h"

#define disa_fatal(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_DISA, params)
#define disa_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_DISA, params)
#define disa_warn(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_DISA, params)
#define disa_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_DISA, params)
#define disa_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_DISA, params)

#define disa_nofl_fatal(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_DISA, params)
#define disa_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_DISA, params)
#define disa_nofl_warn(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_DISA, params)
#define disa_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_DISA, params)
#define disa_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_DISA, params)

#define DISA_ENTER() \
	QDF_TRACE_ENTER(QDF_MODULE_ID_DISA, "enter")
#define DISA_EXIT() \
	QDF_TRACE_EXIT(QDF_MODULE_ID_DISA, "exit")

/**
 * disa_allocate_ctx() - Api to allocate disa ctx
 *
 * Helper function to allocate disa ctx
 *
 * Return: Success or failure.
 */
QDF_STATUS disa_allocate_ctx(void);

/**
 * disa_free_ctx() - to free disa context
 *
 * Helper function to free disa context
 *
 * Return: None.
 */
void disa_free_ctx(void);

/**
 * disa_get_context() - to get disa context
 *
 * Helper function to get disa context
 *
 * Return: disa context.
 */
struct wlan_disa_ctx *disa_get_context(void);

/**
 * disa_core_encrypt_decrypt_req() - Form encrypt/decrypt request
 * @psoc: objmgr psoc object
 * @req: DISA encrypt/decrypt request parameters
 *
 * Return: QDF status success or failure
 */
QDF_STATUS disa_core_encrypt_decrypt_req(struct wlan_objmgr_psoc *psoc,
		struct disa_encrypt_decrypt_req_params *req,
		encrypt_decrypt_resp_callback cb,
		void *cookie);

#endif /* end  of _WLAN_DISA_MAIN_H_ */
