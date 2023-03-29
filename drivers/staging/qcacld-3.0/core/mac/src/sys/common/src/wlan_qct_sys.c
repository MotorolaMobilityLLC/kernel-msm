/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#include <wlan_qct_sys.h>
#include <cds_api.h>
#include <sir_types.h>
#include <sir_params.h>          /* needed for tSirMbMsg */
#include <sir_api.h>             /* needed for SIR_... message types */
#include <wni_api.h>             /* needed for WNI_... message types */
#include "ani_global.h"
#include "wma_types.h"
#include "sme_api.h"
#include "mac_init_api.h"
#include "qdf_trace.h"

/*
 * Cookie for SYS messages.  Note that anyone posting a SYS Message
 * has to write the COOKIE in the reserved field of the message.  The
 * SYS Module relies on this COOKIE
 */
#define SYS_MSG_COOKIE      0xFACE

/* SYS stop timeout 30 seconds */
#define SYS_STOP_TIMEOUT (30000)
static qdf_event_t g_stop_evt;

QDF_STATUS sys_build_message_header(SYS_MSG_ID msg_id,
				    struct scheduler_msg *msg)
{
	msg->type = msg_id;
	msg->reserved = SYS_MSG_COOKIE;

	return QDF_STATUS_SUCCESS;
}

/**
 * umac_stop_complete_cb() - a callback when system stop completes
 * @msg: pointer to actual message being handled
 *
 * this callback is used once system stop is completed.
 *
 * Return: QDF_STATUS
 */
#ifdef QDF_ENABLE_TRACING
static QDF_STATUS umac_stop_complete_cb(struct scheduler_msg *msg)
{
	qdf_event_t *stop_evt = msg->bodyptr;
	QDF_STATUS qdf_status = qdf_event_set(stop_evt);

	QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));

	return qdf_status;
}
#else
static QDF_STATUS umac_stop_complete_cb(struct scheduler_msg *msg)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static inline QDF_STATUS umac_stop_flush_cb(struct scheduler_msg *msg)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * umac_stop() - To post stop message to system module
 *
 * This API is used post a stop message to system module
 *
 * Return: QDF_STATUS
 */
QDF_STATUS umac_stop(void)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct scheduler_msg umac_stop_msg;

	/* Initialize the stop event */
	qdf_status = qdf_event_create(&g_stop_evt);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		return qdf_status;

	/* post a message to SYS module in MC to stop SME and MAC */
	sys_build_message_header(SYS_MSG_ID_UMAC_STOP, &umac_stop_msg);

	/* Save the user callback and user data */
	umac_stop_msg.callback = umac_stop_complete_cb;
	umac_stop_msg.bodyptr = (void *)&g_stop_evt;
	umac_stop_msg.flush_callback = umac_stop_flush_cb;

	/* post the message.. */
	qdf_status = scheduler_post_message(QDF_MODULE_ID_SYS,
					    QDF_MODULE_ID_SYS,
					    QDF_MODULE_ID_SYS, &umac_stop_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		qdf_status = QDF_STATUS_E_BADMSG;

	qdf_status = qdf_wait_single_event(&g_stop_evt, SYS_STOP_TIMEOUT);
	QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));

	qdf_status = qdf_event_destroy(&g_stop_evt);
	QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));

	return qdf_status;
}

/**
 * sys_mc_process_msg() - to process system mc thread messages
 * @pMsg: message pointer
 *
 * This API is used to process the message
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sys_mc_process_msg(struct scheduler_msg *pMsg)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	mac_handle_t mac_handle;

	if (!pMsg) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "%s: NULL pointer to struct scheduler_msg", __func__);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * All 'new' SYS messages are identified by a cookie in the reserved
	 * field of the message as well as the message type.  This prevents
	 * the possibility of overlap in the message types defined for new
	 * SYS messages with the 'legacy' message types.  The legacy messages
	 * will not have this cookie in the reserved field
	 */
	if (SYS_MSG_COOKIE == pMsg->reserved) {
		/* Process all the new SYS messages.. */
		switch (pMsg->type) {
		case SYS_MSG_ID_UMAC_STOP:
			QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
				"Processing SYS MC STOP");
			mac_handle = cds_get_context(QDF_MODULE_ID_PE);
			if (!mac_handle) {
				QDF_TRACE(QDF_MODULE_ID_SYS,
					QDF_TRACE_LEVEL_ERROR,
					"%s: Invalid mac_handle", __func__);
				break;
			}
			qdf_status = sme_stop(mac_handle);
			QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
			qdf_status = mac_stop(mac_handle);
			QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
			qdf_status = pMsg->callback(pMsg);
			break;
		case SYS_MSG_ID_DATA_STALL_MSG:
			if (pMsg->callback)
				qdf_status = pMsg->callback(pMsg);

			qdf_mem_free(pMsg->bodyptr);
			break;
		default:
			QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
				"Unknown message type msgType= %d [0x%08x]",
				pMsg->type, pMsg->type);
			break;

		}
	} else {
		QDF_TRACE(QDF_MODULE_ID_SYS,
				QDF_TRACE_LEVEL_ERROR,
				"Rx SYS unknown MC msgtype= %d [0x%08X]",
				pMsg->type, pMsg->type);
		QDF_ASSERT(0);
		qdf_status = QDF_STATUS_E_BADMSG;

		if (pMsg->bodyptr)
			qdf_mem_free(pMsg->bodyptr);
	}
	return qdf_status;
}

QDF_STATUS sys_mc_process_handler(struct scheduler_msg *msg)
{
	return sys_mc_process_msg(msg);
}

void sys_process_mmh_msg(struct mac_context *mac, struct scheduler_msg *msg)
{
	QDF_MODULE_ID dest_module = QDF_MODULE_ID_SYS;

	if (!msg) {
		QDF_ASSERT(0);
		return;
	}

	switch (msg->type) {
	case eWNI_SME_SYS_READY_IND:
		/* Forward this message to the PE module */
		dest_module = QDF_MODULE_ID_PE;
		break;
	default:
		if ((msg->type >= eWNI_SME_MSG_TYPES_BEGIN) &&
		    (msg->type <= eWNI_SME_MSG_TYPES_END)) {
			dest_module = QDF_MODULE_ID_SME;
			break;
		}

		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "Message of ID %d is not yet handled by SYS",
			  msg->type);
		QDF_ASSERT(0);
	}

	/*
	 * Post now the message to the appropriate module for handling
	 */
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SYS,
							 QDF_MODULE_ID_SYS,
							 dest_module,
							 msg))
		qdf_mem_free(msg->bodyptr);
}
