/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_qct_wma_legacy.c
 *
 * This software unit holds the implementation of the WLAN Device Adaptation
 * Layer for the legacy functionalities that were part of the old HAL.
 *
 * The functions externalized by this module are to be called ONLY by other
 * WLAN modules that properly register with the Transport Layer initially.
 *
 */

/* Standard include files */
/* Application Specific include files */
#include "lim_api.h"
#include "wma.h"
#include "sme_power_save_api.h"
/* Locally used Defines */

#define HAL_MMH_MB_MSG_TYPE_MASK    0xFF00

/**
 * wma_post_ctrl_msg() - Posts WMA messages to MC thread
 * @mac: MAC parameters structure
 * @pMsg: pointer with message
 *
 * Return: Success or Failure
 */

QDF_STATUS wma_post_ctrl_msg(struct mac_context *mac, struct scheduler_msg *pMsg)
{
	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA, pMsg))
		return QDF_STATUS_E_FAILURE;
	else
		return QDF_STATUS_SUCCESS;
}

/**
 * u_mac_post_ctrl_msg() - post ctrl msg
 * @pMb: A pointer to the maibox message
 *
 * Forwards the completely received message to the respective
 * modules for further processing.
 *
 * NOTE:
 *  This function has been moved to the API file because for MAC running
 *  on Windows host, the host module will call this routine directly to
 *  send any mailbox messages. Making this function an API makes sure that
 *  outside world (any module outside MMH) only calls APIs to use MMH
 *  services and not an internal function.
 *
 * Return: success/error code
 */

QDF_STATUS u_mac_post_ctrl_msg(void *pSirGlobal, tSirMbMsg *pMb)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = (struct mac_context *) pSirGlobal;

	msg.type = pMb->type;
	msg.bodyval = 0;
	msg.bodyptr = pMb;

	switch (msg.type & HAL_MMH_MB_MSG_TYPE_MASK) {
	case WMA_MSG_TYPES_BEGIN:       /* Posts a message to the HAL MsgQ */
		status = wma_post_ctrl_msg(mac, &msg);
		break;

	case SIR_LIM_MSG_TYPES_BEGIN:   /* Posts a message to the LIM MsgQ */
		status = lim_post_msg_api(mac, &msg);
		break;

	case SIR_SME_MSG_TYPES_BEGIN:   /* Posts a message to the LIM MsgQ */
		status = sme_post_pe_message(mac, &msg);
		break;

	default:
		wma_debug("Unknown message type = 0x%X", msg.type);
		qdf_mem_free(msg.bodyptr);
		return QDF_STATUS_E_FAILURE;
	}

	if (status != QDF_STATUS_SUCCESS)
		qdf_mem_free(msg.bodyptr);

	return status;

} /* u_mac_post_ctrl_msg() */

QDF_STATUS umac_send_mb_message_to_mac(void *msg)
{
	void *mac_handle = cds_get_context(QDF_MODULE_ID_SME);

	if (!mac_handle) {
		QDF_TRACE_ERROR(QDF_MODULE_ID_SYS, "Invalid MAC handle");
		qdf_mem_free(msg);
		return QDF_STATUS_E_FAILURE;
	}

	return u_mac_post_ctrl_msg(mac_handle, msg);
}
