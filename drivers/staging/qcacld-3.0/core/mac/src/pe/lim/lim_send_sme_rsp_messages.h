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

/*
 * This file lim_send_sme_rsp_messages.h contains the definitions for
 * sending SME response/notification messages to applications above
 * MAC software.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __LIM_SEND_SME_RSP_H
#define __LIM_SEND_SME_RSP_H

#include "sir_common.h"
#include "sir_api.h"
#include "sir_mac_prot_def.h"

/* Functions for sending responses up the stack */

/**
 * lim_send_sme_rsp() - Send Generic Response to upper layers
 * @mac_ctx:        Pointer to Global MAC structure
 * @msg_type:       Indicates message type
 * @result_code:    Indicates the result of previously issued
 *                  eWNI_SME_msg_type_REQ message
 * @vdev_id: vdev_id
 *
 * This function is called by lim_process_sme_req_messages() to send
 * eWNI_SME_START_RSP, eWNI_SME_STOP_BSS_RSP
 * or eWNI_SME_SWITCH_CHL_RSP messages to applications above MAC
 * Software.
 *
 * Return: None
 */
void lim_send_sme_rsp(struct mac_context *mac_ctx, uint16_t msg_type,
		      tSirResultCodes result_code, uint8_t vdev_id);

/**
 * lim_send_sme_start_bss_rsp() - Send Start BSS response
 * @mac: Pointer to Global MAC structure
 * @msgType: Indicates message type
 * @resultCode: Indicates the result of previously issued request
 * @pe_session: PE session associated with the BSS
 * @smesessionId: ID of the SME session associated with the BSS
 *
 * This function is called to send eWNI_SME_START_BSS_RSP
 * message to applications above MAC Software.
 */
void lim_send_sme_start_bss_rsp(struct mac_context *mac,
				uint16_t msgType,
				tSirResultCodes resultCode,
				struct pe_session *pe_session,
				uint8_t smesessionId);

/**
 * lim_send_sme_join_reassoc_rsp() - Send Response to Upper Layers
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_type: Indicates message type
 * @result_code: Indicates the result of previously issued request
 * @prot_status_code: Protocol Status Code
 * @session_entry: PE Session Info
 * @vdev_id: vdev_id
 *
 * This function is called by lim_process_sme_req_messages() to send
 * eWNI_SME_JOIN_RSP or eWNI_SME_REASSOC_RSP messages to applications
 * above MAC Software.
 *
 * Return: None
 */
void lim_send_sme_join_reassoc_rsp(struct mac_context *mac_ctx,
				   uint16_t msg_type,
				   tSirResultCodes result_code,
				   uint16_t prot_status_code,
				   struct pe_session *session_entry,
				   uint8_t vdev_id);

/**
 * lim_prepare_disconnect_done_ind() - Prepares the disconnect done ind message
 * @mac_ctx: Global mac_ctx
 * @session_id: PE session id
 * @reason_code: Disconnect indication reason code
 * @peer_mac_addr: MAC address of the peer
 *
 * Prepares the disconnect done indication message to be sent to the upper layer
 *
 * Return: QDF Status
 */
QDF_STATUS lim_prepare_disconnect_done_ind(struct mac_context *mac_ctx,
					   uint32_t **msg,
					   uint8_t session_id,
					   tSirResultCodes reason_code,
					   uint8_t *peer_mac_addr);

/**
 * lim_send_sme_disassoc_ntf() - Send disassoc notification to upper layer
 * @mac: Global MAC context
 * @peerMacAddr: The peer MAC addr to which disassociate was initiated
 * @reasonCode: The reason for Disassociation
 * @disassocTrigger: The trigger for Disassociation
 * @aid: The STAID. This parameter is present only on AP
 * @smesessionId: ID of the SME session associated with the event
 * @pe_session: The PE session associated with the event
 *
 * This function is used for sending eWNI_SME_DISASSOC_RSP,
 * or eWNI_SME_DISASSOC_IND to upper layer depending on
 * disassociation trigger.
 *
 * Return: None
 */
void lim_send_sme_disassoc_ntf(struct mac_context *mac,
			       tSirMacAddr peerMacAddr,
			       tSirResultCodes reasonCode,
			       uint16_t disassocTrigger,
			       uint16_t aid,
			       uint8_t smesessionId,
			       struct pe_session *pe_session);

/**
 * lim_send_sme_deauth_ntf() - send deauth notice to upper layer
 * @peerMacAddr: peer MAC addr to which deauthentication was initiated
 * @reasonCode: the reason for Deauthetication
 * @deauthTrigger: the trigger for Deauthetication
 * @aid: the STAID. This parameter is present only on AP.
 * @vdev_id: vdev id
 *
 * This function is used for sending eWNI_SME_DEAUTH_RSP or
 * eWNI_SME_DEAUTH_IND to upper layers depending on deauthentication
 * trigger.
 *
 * Return: None
 */
void lim_send_sme_deauth_ntf(struct mac_context *mac, tSirMacAddr peerMacAddr,
			     tSirResultCodes reasonCode, uint16_t deauthTrigger,
			     uint16_t aid, uint8_t vdev_id);

void lim_send_sme_disassoc_ind(struct mac_context *, tpDphHashNode, struct pe_session *);
void lim_send_sme_deauth_ind(struct mac_context *, tpDphHashNode,
			     struct pe_session *pe_session);
void lim_send_sme_wm_status_change_ntf(struct mac_context *, tSirSmeStatusChangeCode,
				       uint32_t *, uint16_t, uint8_t);

/**
 * lim_send_sme_set_context_rsp() - Send set context response to upper layer
 * @mac: Pointer to Global MAC structure
 * @peer_macaddr: the peer MAC addr to which setContext was performed
 * @aid: the aid corresponding to the peer MAC address
 * @resultCode: the result of previously issued Set Context Req message
 * @pe_session: The PE session associated with the peer
 * @smesessionId: ID of the SME session associated with the peer
 *
 * This function is called to send eWNI_SME_SETCONTEXT_RSP message to
 * upper layer
 *
 * Return: None
 */
void lim_send_sme_set_context_rsp(struct mac_context *mac,
				  struct qdf_mac_addr peer_macaddr,
				  uint16_t aid,
				  tSirResultCodes resultCode,
				  struct pe_session *pe_session,
				  uint8_t smesessionId);

/**
 * lim_handle_delete_bss_rsp () - API to handle delete bss response
 * @mac: global mac structure
 * @del_bss_rsp: pointer to del bss response
 *
 * Return: None
 */
void lim_handle_delete_bss_rsp(struct mac_context *mac,
				struct del_bss_resp *del_bss_rsp);

void lim_handle_csa_offload_msg(struct mac_context *mac_ctx,
				struct scheduler_msg *msg);

void
lim_send_sme_aggr_qos_rsp(struct mac_context *mac, tpSirAggrQosRsp aggrQosRsp,
			  uint8_t smesessionId);

/**
 * lim_send_sme_addts_rsp() - sends SME ADDTS RSP
 * @mac: global mac structure
 * @rspReqd: is SmeAddTsRsp required
 * @status: status code of SME_ADD_TS_RSP
 * @pe_session: The PE session associated with the connection
 * @tspec: The TSpec that was added
 * @smesessionId: ID of the SME session associated with the connection
 *
 * This function sends a eWNI_SME_ADDTS_RSP to upper layer
 *
 * Return: None
 */
void lim_send_sme_addts_rsp(struct mac_context *mac,
			    uint8_t rspReqd, uint32_t status,
			    struct pe_session *pe_session,
			    struct mac_tspec_ie tspec,
			    uint8_t smesessionId);

void lim_send_sme_delts_rsp(struct mac_context *mac, tpSirDeltsReq delts,
			    uint32_t status, struct pe_session *pe_session,
			    uint8_t smesessionId);
void lim_send_sme_delts_ind(struct mac_context *mac,
			    struct delts_req_info *delts,
			    uint16_t aid, struct pe_session *);

#ifdef FEATURE_WLAN_ESE
void lim_send_sme_pe_ese_tsm_rsp(struct mac_context *mac, tAniGetTsmStatsRsp *pStats);
#endif

void lim_send_sme_max_assoc_exceeded_ntf(struct mac_context *mac, tSirMacAddr peerMacAddr,
					 uint8_t smesessionId);

void lim_send_sme_ap_channel_switch_resp(struct mac_context *mac,
					 struct pe_session *pe_session,
					 struct vdev_start_response *rsp);
/*
 * lim_process_beacon_tx_success_ind() - handle successful beacon transmission
 * indication from the FW This is a generic event generated by the FW afer the
 * first beacon is sent out after the beacon template update by the host.
 *
 * @mac_ctx: Global mac_ctx
 * @msg_type: msg_type
 */
void
lim_process_beacon_tx_success_ind(struct mac_context *mac, uint16_t msgType,
				  void *event);

typedef enum {
	lim_csa_ie_present = 0x00000001,
	lim_xcsa_ie_present = 0x00000002,
	lim_wbw_ie_present = 0x00000004,
	lim_cswarp_ie_present = 0x00000008,
} lim_csa_event_ies_present_flag;

#endif /* __LIM_SEND_SME_RSP_H */
