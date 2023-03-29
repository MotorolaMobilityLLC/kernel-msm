/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_QCT_SYS_H__)
#define WLAN_QCT_SYS_H__

/**===========================================================================

   \file  wlan_qct_sys.h

   \brief System module API

   ==========================================================================*/

/* $HEADER$ */

/*---------------------------------------------------------------------------
   Include files
   -------------------------------------------------------------------------*/
#include <qdf_types.h>
#include <qdf_status.h>
#include <scheduler_api.h>

struct mac_context;

/*---------------------------------------------------------------------------
   Preprocessor definitions and constants
   -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
   Type declarations
   -------------------------------------------------------------------------*/

/**
 * sys_rsp_cb() - SYS async resonse callback
 * @user_data: context data for callback
 *
 * This is a protype for the callback function that SYS makes to various
 * modules in the system.
 *
 * Return: None
 */
typedef void (*sys_rsp_cb)(void *user_data);

/**
 * sys_build_message_header() - to build the sys message header
 * @msg_id: message id
 * @msg: pointer to message context
 *
 * This function will initialize the SYS message header with the
 * message type and any internal fields needed for a new SYS
 * message. This function sets all but the message body, which is up
 * to the caller to setup based on the specific message being built.
 *
 * NOTE: There are internal / reserved items in a SYS message that
 * must be set correctly for the message to be recognized as a SYS
 * message by the SYS message handlers.  It is important for every SYS
 * message to be setup / built / initialized through this function.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sys_build_message_header(SYS_MSG_ID msg_id,
				    struct scheduler_msg *msg);

/**
 * umac_stop() - send schedule message to mc thread to stop umac (sme and mac)
 *
 * Return: status of operation
 */
QDF_STATUS umac_stop(void);

QDF_STATUS sys_mc_process_handler(struct scheduler_msg *msg);

/**
 * sys_process_mmh_msg() - api to process an mmh message
 * @mac: pointer to mac context
 * @msg: pointer to message
 *
 * This API is used to process an mmh message.
 *
 * NOTE WELL: Ownership of the @msg bodyptr, if present, is always
 * transferred, and the caller must not attempt to dereference or free
 * the bodyptr after invoking this API.
 *
 * Return: none
 */
void sys_process_mmh_msg(struct mac_context *mac,
			 struct scheduler_msg *msg);

#endif /* WLAN_QCT_SYS_H__ */
