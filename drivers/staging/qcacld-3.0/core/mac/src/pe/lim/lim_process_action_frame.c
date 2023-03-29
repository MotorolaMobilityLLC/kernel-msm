/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * This file lim_process_action_frame.cc contains the code
 * for processing Action Frame.
 * Author:      Michael Lui
 * Date:        05/23/03
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_api.h"
#include "sir_api.h"
#include "ani_global.h"
#include "wni_cfg.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_send_sme_rsp_messages.h"
#include "parser_api.h"
#include "lim_admit_control.h"
#include "wmm_apsd.h"
#include "lim_send_messages.h"
#include "rrm_api.h"
#include "lim_session_utils.h"
#include "wlan_policy_mgr_api.h"
#include "wma_types.h"
#include "wma.h"
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include "dot11f.h"
#include "wlan_p2p_cfg_api.h"

#define SA_QUERY_REQ_MIN_LEN \
(DOT11F_FF_CATEGORY_LEN + DOT11F_FF_ACTION_LEN + DOT11F_FF_TRANSACTIONID_LEN)
#define SA_QUERY_RESP_MIN_LEN \
(DOT11F_FF_CATEGORY_LEN + DOT11F_FF_ACTION_LEN + DOT11F_FF_TRANSACTIONID_LEN)

static last_processed_msg rrm_link_action_frm;

/**-----------------------------------------------------------------
   \fn     lim_stop_tx_and_switch_channel
   \brief  Stops the transmission if channel switch mode is silent and
   starts the channel switch timer.

   \param  mac
   \return NONE
   -----------------------------------------------------------------*/
void lim_stop_tx_and_switch_channel(struct mac_context *mac, uint8_t sessionId)
{
	struct pe_session *pe_session;
	QDF_STATUS status;

	pe_session = pe_find_session_by_session_id(mac, sessionId);

	if (!pe_session) {
		pe_err("Session: %d not active", sessionId);
		return;
	}

	if (pe_session->ftPEContext.pFTPreAuthReq) {
		pe_debug("Avoid Switch Channel req during pre auth");
		return;
	}

	status = policy_mgr_check_and_set_hw_mode_for_channel_switch(mac->psoc,
				pe_session->smeSessionId,
				pe_session->gLimChannelSwitch.sw_target_freq,
				POLICY_MGR_UPDATE_REASON_CHANNEL_SWITCH_STA);

	/*
	 * If status is QDF_STATUS_E_FAILURE, mean HW mode change was required
	 * but driver failed to set HW mode so ignore CSA for the channel.
	 * If status is QDF_STATUS_SUCCESS mean HW mode change was required
	 * and was sucessfully changed so the channel switch will continue after
	 * HW mode change completion.
	 * If status is QDF_STATUS_E_NOSUPPORT or QDF_STATUS_E_ALREADY, mean
	 * DBS is not supported or required HW mode is already set, so
	 * So contunue with CSA from here.
	 */
	if (status == QDF_STATUS_E_FAILURE) {
		pe_err("Failed to set required HW mode for channel %d freq %d, ignore CSA",
		       pe_session->gLimChannelSwitch.primaryChannel,
		       pe_session->gLimChannelSwitch.sw_target_freq);
		return;
	}

	if (QDF_IS_STATUS_SUCCESS(status)) {
		pe_info("Channel change will continue after HW mode change");
		return;
	}

	lim_process_channel_switch(mac, pe_session->smeSessionId);

	return;
}

/**
 * lim_process_ext_channel_switch_action_frame()- Process ECSA Action
 * Frames.
 * @mac_ctx: pointer to global mac structure
 * @rx_packet_info: rx packet meta information
 * @session_entry: Session entry.
 *
 * This function is called when ECSA action frame is received.
 *
 * Return: void
 */
static void
lim_process_ext_channel_switch_action_frame(struct mac_context *mac_ctx,
		uint8_t *rx_packet_info, struct pe_session *session_entry)
{

	tpSirMacMgmtHdr         hdr;
	uint8_t                 *body;
	tDot11fext_channel_switch_action_frame *ext_channel_switch_frame;
	uint32_t                frame_len;
	uint32_t                status;
	uint32_t                target_freq;

	hdr = WMA_GET_RX_MAC_HEADER(rx_packet_info);
	body = WMA_GET_RX_MPDU_DATA(rx_packet_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_packet_info);

	pe_debug("Received EXT Channel switch action frame");

	ext_channel_switch_frame =
		 qdf_mem_malloc(sizeof(*ext_channel_switch_frame));
	if (!ext_channel_switch_frame)
		return;

	/* Unpack channel switch frame */
	status = dot11f_unpack_ext_channel_switch_action_frame(mac_ctx,
			body, frame_len, ext_channel_switch_frame, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse CHANSW action frame (0x%08x, len %d):",
			status, frame_len);
		qdf_mem_free(ext_channel_switch_frame);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking CHANSW Request (0x%08x, %d bytes):",
		  status, frame_len);
	}

	if (!wlan_reg_is_6ghz_supported(mac_ctx->psoc) &&
	    (wlan_reg_is_6ghz_op_class(mac_ctx->pdev,
				       ext_channel_switch_frame->
				       ext_chan_switch_ann_action.op_class))) {
		pe_err("channel belongs to 6 ghz spectrum, abort");
		qdf_mem_free(ext_channel_switch_frame);
		return;
	}

	target_freq =
		wlan_reg_chan_opclass_to_freq(ext_channel_switch_frame->ext_chan_switch_ann_action.new_channel,
					      ext_channel_switch_frame->ext_chan_switch_ann_action.op_class,
					      false);

	/* Free ext_channel_switch_frame here as its no longer needed */
	qdf_mem_free(ext_channel_switch_frame);
	/*
	 * Now, validate if channel change is required for the passed
	 * channel and if is valid in the current regulatory domain,
	 * and no concurrent session is running.
	 */
	if (!(session_entry->curr_op_freq != target_freq &&
	      ((wlan_reg_get_channel_state_for_freq(mac_ctx->pdev, target_freq) ==
		  CHANNEL_STATE_ENABLE) ||
	       (wlan_reg_get_channel_state_for_freq(mac_ctx->pdev, target_freq) ==
		  CHANNEL_STATE_DFS &&
		!policy_mgr_concurrent_open_sessions_running(
			mac_ctx->psoc))))) {
		pe_err("Channel freq: %d is not valid", target_freq);
		return;
	}

	if (session_entry->opmode == QDF_P2P_GO_MODE) {

		struct sir_sme_ext_cng_chan_ind *ext_cng_chan_ind;
		struct scheduler_msg mmh_msg = {0};

		ext_cng_chan_ind = qdf_mem_malloc(sizeof(*ext_cng_chan_ind));
		if (!ext_cng_chan_ind)
			return;

		ext_cng_chan_ind->session_id =
					session_entry->smeSessionId;

		/* No need to extract op mode as BW will be decided in
		 *  in SAP FSM depending on previous BW.
		 */
		ext_cng_chan_ind->new_chan_freq = target_freq;

		mmh_msg.type = eWNI_SME_EXT_CHANGE_CHANNEL_IND;
		mmh_msg.bodyptr = ext_cng_chan_ind;
		mmh_msg.bodyval = 0;
		lim_sys_process_mmh_msg_api(mac_ctx, &mmh_msg);
	}
	return;
} /*** end lim_process_ext_channel_switch_action_frame() ***/

/**
 * __lim_process_operating_mode_action_frame() - To process op mode frames
 * @mac_ctx: pointer to mac context
 * @rx_pkt_info: pointer to received packet info
 * @session: pointer to session
 *
 * This routine is called to process operating mode action frames
 *
 * Return: None
 */
static void __lim_process_operating_mode_action_frame(struct mac_context *mac_ctx,
			uint8_t *rx_pkt_info, struct pe_session *session)
{

	tpSirMacMgmtHdr mac_hdr;
	uint8_t *body_ptr;
	tDot11fOperatingMode *operating_mode_frm;
	uint32_t frame_len;
	uint32_t status;
	tpDphHashNode sta_ptr;
	uint16_t aid;
	uint8_t oper_mode;
	uint8_t cb_mode;
	uint8_t ch_bw = 0;
	uint8_t skip_opmode_update = false;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_debug("Received Operating Mode action frame");

	/*
	 * Ignore opmode change during channel change The opmode will be updated
	 * with the beacons on new channel once the AP move to new channel.
	 */
	if (session->ch_switch_in_progress) {
		pe_debug("Ignore opmode change as channel switch is in progress");
		return;
	}
	operating_mode_frm = qdf_mem_malloc(sizeof(*operating_mode_frm));
	if (!operating_mode_frm)
		return;

	/* Unpack channel switch frame */
	status = dot11f_unpack_operating_mode(mac_ctx, body_ptr, frame_len,
			operating_mode_frm, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to unpack and parse (0x%08x, %d bytes)",
			status, frame_len);
		qdf_mem_free(operating_mode_frm);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("warnings while unpacking (0x%08x, %d bytes):",
			status, frame_len);
	}
	sta_ptr = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa, &aid,
			&session->dph.dphHashTable);

	if (!sta_ptr) {
		pe_err("Station context not found");
		goto end;
	}

	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
		cb_mode = mac_ctx->roam.configParam.channelBondingMode24GHz;
	else
		cb_mode = mac_ctx->roam.configParam.channelBondingMode5GHz;
	/*
	 * Do not update the channel bonding mode if channel bonding
	 * mode is disabled in INI.
	 */
	if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == cb_mode) {
		pe_debug("channel bonding disabled");
		goto update_nss;
	}

	if (sta_ptr->htSupportedChannelWidthSet) {
		if (WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ <
				sta_ptr->vhtSupportedChannelWidthSet)
			oper_mode = eHT_CHANNEL_WIDTH_160MHZ;
		else
			oper_mode = sta_ptr->vhtSupportedChannelWidthSet + 1;
	} else {
		oper_mode = eHT_CHANNEL_WIDTH_20MHZ;
	}

	if ((oper_mode == eHT_CHANNEL_WIDTH_80MHZ) &&
			(operating_mode_frm->OperatingMode.chanWidth >
				eHT_CHANNEL_WIDTH_80MHZ))
		skip_opmode_update = true;

	if (!skip_opmode_update && (oper_mode !=
		operating_mode_frm->OperatingMode.chanWidth)) {
		uint32_t fw_vht_ch_wd = wma_get_vht_ch_width();

		pe_debug("received Chanwidth: %d",
			 operating_mode_frm->OperatingMode.chanWidth);

		pe_debug(" MAC: %0x:%0x:%0x:%0x:%0x:%0x",
			mac_hdr->sa[0], mac_hdr->sa[1], mac_hdr->sa[2],
			mac_hdr->sa[3], mac_hdr->sa[4], mac_hdr->sa[5]);

		if (operating_mode_frm->OperatingMode.chanWidth >=
				eHT_CHANNEL_WIDTH_160MHZ
				&& (fw_vht_ch_wd >= eHT_CHANNEL_WIDTH_160MHZ)) {
			sta_ptr->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
			sta_ptr->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
			ch_bw = eHT_CHANNEL_WIDTH_160MHZ;
		} else if (operating_mode_frm->OperatingMode.chanWidth >=
				eHT_CHANNEL_WIDTH_80MHZ) {
			sta_ptr->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
			sta_ptr->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
			ch_bw = eHT_CHANNEL_WIDTH_80MHZ;
		} else if (operating_mode_frm->OperatingMode.chanWidth ==
				eHT_CHANNEL_WIDTH_40MHZ) {
			sta_ptr->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
			sta_ptr->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
			ch_bw = eHT_CHANNEL_WIDTH_40MHZ;
		} else if (operating_mode_frm->OperatingMode.chanWidth ==
				eHT_CHANNEL_WIDTH_20MHZ) {
			sta_ptr->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
			sta_ptr->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_20MHZ;
			ch_bw = eHT_CHANNEL_WIDTH_20MHZ;
		}
		lim_check_vht_op_mode_change(mac_ctx, session, ch_bw,
					     mac_hdr->sa);
	}

update_nss:
	if (sta_ptr->vhtSupportedRxNss !=
			(operating_mode_frm->OperatingMode.rxNSS + 1)) {
		sta_ptr->vhtSupportedRxNss =
			operating_mode_frm->OperatingMode.rxNSS + 1;
		lim_set_nss_change(mac_ctx, session, sta_ptr->vhtSupportedRxNss,
			mac_hdr->sa);
	}

end:
	qdf_mem_free(operating_mode_frm);
	return;
}

/**
 * __lim_process_gid_management_action_frame() - To process group-id mgmt frames
 * @mac_ctx: Pointer to mac context
 * @rx_pkt_info: Rx packet info
 * @session: pointer to session
 *
 * This routine will be called to process group id management frames
 *
 * Return: none
 */
static void
__lim_process_gid_management_action_frame(struct mac_context *mac_ctx,
					  uint8_t *rx_pkt_info,
					  struct pe_session *session)
{
	uint8_t *body_ptr;
	uint16_t aid;
	uint32_t frame_len, status, membership = 0, usr_position = 0;
	uint32_t *mem_lower, *mem_upper, *mem_cur;
	tpSirMacMgmtHdr mac_hdr;
	tDot11fVHTGidManagementActionFrame *gid_mgmt_frame;
	tpDphHashNode sta_ptr;
	struct sDot11fFfVhtMembershipStatusArray *vht_member_status = NULL;
	struct sDot11fFfVhtUserPositionArray *vht_user_position = NULL;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_debug("Received GID Management action frame");
	gid_mgmt_frame = qdf_mem_malloc(sizeof(*gid_mgmt_frame));
	if (!gid_mgmt_frame)
		return;

	/* Unpack Gid Management Action frame */
	status = dot11f_unpack_vht_gid_management_action_frame(mac_ctx,
			body_ptr, frame_len, gid_mgmt_frame, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Fail to parse an Grp id frame (0x%08x, %d bytes):",
			status, frame_len);
		qdf_mem_free(gid_mgmt_frame);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("warnings while unpacking Grp id frm (0x%08x, %d bytes):",
		 status, frame_len);
	}
	sta_ptr = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa, &aid,
			&session->dph.dphHashTable);
	if (!sta_ptr) {
		pe_err("Failed to get STA entry from hash table");
		goto out;
	}

	pe_debug(" MAC: %0x:%0x:%0x:%0x:%0x:%0x",
		mac_hdr->sa[0], mac_hdr->sa[1], mac_hdr->sa[2],
		mac_hdr->sa[3], mac_hdr->sa[4], mac_hdr->sa[5]);
	vht_member_status = &gid_mgmt_frame->VhtMembershipStatusArray;
	mem_lower =  (uint32_t *) vht_member_status->membershipStatusArray;
	mem_upper = (uint32_t *) &vht_member_status->membershipStatusArray[4];

	if (*mem_lower && *mem_upper) {
		pe_err("rcved frame with mult group ID set");
		goto out;
	}
	if (*mem_lower) {
		mem_cur = mem_lower;
	} else if (*mem_upper) {
		mem_cur = mem_upper;
		membership += sizeof(uint32_t);
	} else {
		pe_err("rcved Gid frame with no group ID set");
		goto out;
	}
	while (!(*mem_cur & 1)) {
		*mem_cur >>= 1;
		++membership;
	}
	if (*mem_cur) {
		pe_err("rcved frame with mult group ID set");
		goto out;
	}

	/*Just read the last two bits */
	vht_user_position = &gid_mgmt_frame->VhtUserPositionArray;
	usr_position = vht_user_position->userPositionArray[membership] & 0x3;
	lim_check_membership_user_position(mac_ctx, session, membership,
			usr_position);
out:
	qdf_mem_free(gid_mgmt_frame);
	return;
}

static void
__lim_process_add_ts_req(struct mac_context *mac, uint8_t *pRxPacketInfo,
			 struct pe_session *pe_session)
{
}

/**
 * __lim_process_add_ts_rsp() - To process add ts response frame
 * @mac_ctx: pointer to mac context
 * @rx_pkt_info: Received packet info
 * @session: pointer to session
 *
 * This routine is to handle add ts response frame
 *
 * Return: none
 */
static void __lim_process_add_ts_rsp(struct mac_context *mac_ctx,
		uint8_t *rx_pkt_info, struct pe_session *session)
{
	tSirAddtsRspInfo addts;
	QDF_STATUS retval;
	tpSirMacMgmtHdr mac_hdr;
	tpDphHashNode sta_ptr;
	uint16_t aid;
	uint32_t frameLen;
	uint8_t *body_ptr;
	tpLimTspecInfo tspec_info;
	uint8_t ac;
	tpDphHashNode sta_ds_ptr = NULL;
	uint8_t rsp_reqd = 1;
	uint32_t cfg_len;
	tSirMacAddr peer_macaddr;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_warn("Recv AddTs Response");
	if (LIM_IS_AP_ROLE(session)) {
		pe_warn("AddTsRsp recvd at AP: ignoring");
		return;
	}

	sta_ptr = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa, &aid,
				&session->dph.dphHashTable);
	if (!sta_ptr) {
		pe_err("Station context not found - ignoring AddTsRsp");
		return;
	}

	retval = sir_convert_addts_rsp2_struct(mac_ctx, body_ptr,
			frameLen, &addts);
	if (retval != QDF_STATUS_SUCCESS) {
		pe_err("AddTsRsp parsing failed %d", retval);
		return;
	}
	/*
	 * don't have to check for qos/wme capabilities since we wouldn't have
	 * this flag set otherwise
	 */
	if (!mac_ctx->lim.gLimAddtsSent) {
		/* we never sent an addts request! */
		pe_warn("rx AddTsRsp but no req was ever sent-ignoring");
		return;
	}

	if (mac_ctx->lim.gLimAddtsReq.req.dialogToken != addts.dialogToken) {
		pe_warn("token mismatch got: %d exp: %d - ignoring",
			addts.dialogToken,
			mac_ctx->lim.gLimAddtsReq.req.dialogToken);
		return;
	}

	/*
	 * for successful addts response, try to add the classifier.
	 * if this fails for any reason, we should send a delts request to the
	 * ap for now, its ok not to send a delts since we are going to add
	 * support for multiple tclas soon and until then we won't send any
	 * addts requests with multiple tclas elements anyway.
	 * In case of addClassifier failure, we just let the addts timer run out
	 */
	if (((addts.tspec.tsinfo.traffic.accessPolicy ==
		SIR_MAC_ACCESSPOLICY_HCCA) ||
		(addts.tspec.tsinfo.traffic.accessPolicy ==
			SIR_MAC_ACCESSPOLICY_BOTH)) &&
		(addts.status == STATUS_SUCCESS)) {
		/* add the classifier - this should always succeed */
		if (addts.numTclas > 1) {
			/* currently no support for multiple tclas elements */
			pe_err("Sta: %d Too many Tclas: %d 1 supported",
				aid, addts.numTclas);
			return;
		} else if (addts.numTclas == 1) {
			pe_debug("Response from STA: %d tsid: %d UP: %d OK!",
				aid, addts.tspec.tsinfo.traffic.tsid,
				addts.tspec.tsinfo.traffic.userPrio);
		}
	}
	pe_debug("Recv AddTsRsp: tsid: %d UP: %d status: %d",
		addts.tspec.tsinfo.traffic.tsid,
		addts.tspec.tsinfo.traffic.userPrio, addts.status);

	/* deactivate the response timer */
	lim_deactivate_and_change_timer(mac_ctx, eLIM_ADDTS_RSP_TIMER);

	if (addts.status != STATUS_SUCCESS) {
		pe_debug("Recv AddTsRsp: tsid: %d UP: %d status: %d",
			addts.tspec.tsinfo.traffic.tsid,
			addts.tspec.tsinfo.traffic.userPrio, addts.status);
		lim_send_sme_addts_rsp(mac_ctx, true, addts.status, session,
				       addts.tspec, session->smeSessionId);

		/* clear the addts flag */
		mac_ctx->lim.gLimAddtsSent = false;

		return;
	}
#ifdef FEATURE_WLAN_ESE
	if (addts.tsmPresent) {
		pe_debug("TSM IE Present");
		session->eseContext.tsm.tid =
			addts.tspec.tsinfo.traffic.userPrio;
		qdf_mem_copy(&session->eseContext.tsm.tsmInfo,
			     &addts.tsmIE, sizeof(struct ese_tsm_ie));
		lim_send_sme_tsm_ie_ind(mac_ctx, session, addts.tsmIE.tsid,
					addts.tsmIE.state,
					addts.tsmIE.msmt_interval);
	}
#endif
	/*
	 * Since AddTS response was successful, check for the PSB flag
	 * and directional flag inside the TS Info field.
	 * An AC is trigger enabled AC if the PSB subfield is set to 1
	 * in the uplink direction.
	 * An AC is delivery enabled AC if the PSB subfield is set to 1
	 * in the downlink direction.
	 * An AC is trigger and delivery enabled AC if the PSB subfield
	 * is set to 1 in the bi-direction field.
	 */
	if (addts.tspec.tsinfo.traffic.psb == 1)
		lim_set_tspec_uapsd_mask_per_session(mac_ctx, session,
						     &addts.tspec.tsinfo,
						     SET_UAPSD_MASK);
	else
		lim_set_tspec_uapsd_mask_per_session(mac_ctx, session,
						     &addts.tspec.tsinfo,
						     CLEAR_UAPSD_MASK);

	/*
	 * ADDTS success, so AC is now admitted. We shall now use the default
	 * EDCA parameters as advertised by AP and send the updated EDCA params
	 * to HAL.
	 */
	ac = upToAc(addts.tspec.tsinfo.traffic.userPrio);
	if (addts.tspec.tsinfo.traffic.direction ==
	    SIR_MAC_DIRECTION_UPLINK) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
	} else if (addts.tspec.tsinfo.traffic.direction ==
		   SIR_MAC_DIRECTION_DNLINK) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	} else if (addts.tspec.tsinfo.traffic.direction ==
		   SIR_MAC_DIRECTION_BIDIR) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
		session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	}
	lim_set_active_edca_params(mac_ctx, session->gLimEdcaParams,
				   session);
	sta_ds_ptr = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				   &session->dph.dphHashTable);
	if (sta_ds_ptr)
		lim_send_edca_params(mac_ctx, session->gLimEdcaParamsActive,
				     session->vdev_id, false);
	else
		pe_err("Self entry missing in Hash Table");
	sir_copy_mac_addr(peer_macaddr, session->bssId);
	/* if schedule is not present then add TSPEC with svcInterval as 0. */
	if (!addts.schedulePresent)
		addts.schedule.svcInterval = 0;
	if (QDF_STATUS_SUCCESS !=
	    lim_tspec_add(mac_ctx, sta_ptr->staAddr, sta_ptr->assocId,
		&addts.tspec, addts.schedule.svcInterval, &tspec_info)) {
		pe_err("Adding entry in lim Tspec Table failed");
		lim_send_delts_req_action_frame(mac_ctx, peer_macaddr, rsp_reqd,
						&addts.tspec.tsinfo,
						&addts.tspec, session);
		mac_ctx->lim.gLimAddtsSent = false;
		return;
		/*
		 * Error handling. send the response with error status.
		 * need to send DelTS to tear down the TSPEC status.
		 */
	}
	if ((addts.tspec.tsinfo.traffic.accessPolicy !=
			SIR_MAC_ACCESSPOLICY_EDCA) ||
		((upToAc(addts.tspec.tsinfo.traffic.userPrio) < QCA_WLAN_AC_ALL))) {
#ifdef FEATURE_WLAN_ESE
		retval = lim_send_hal_msg_add_ts(mac_ctx,
				tspec_info->idx,
				addts.tspec, session->peSessionId,
				addts.tsmIE.msmt_interval);
#else
		retval = lim_send_hal_msg_add_ts(mac_ctx,
				tspec_info->idx,
				addts.tspec, session->peSessionId);
#endif
		if (QDF_STATUS_SUCCESS != retval) {
			lim_admit_control_delete_ts(mac_ctx, sta_ptr->assocId,
				&addts.tspec.tsinfo, NULL, &tspec_info->idx);

			/* Send DELTS action frame to AP */
			cfg_len = sizeof(tSirMacAddr);
			lim_send_delts_req_action_frame(mac_ctx, peer_macaddr,
					rsp_reqd, &addts.tspec.tsinfo,
					&addts.tspec, session);
			lim_send_sme_addts_rsp(mac_ctx, true, retval,
					session, addts.tspec,
					session->smeSessionId);
			mac_ctx->lim.gLimAddtsSent = false;
			return;
		}
		pe_debug("AddTsRsp received successfully UP: %d TSID: %d",
			addts.tspec.tsinfo.traffic.userPrio,
			addts.tspec.tsinfo.traffic.tsid);
	} else {
		pe_debug("AddTsRsp received successfully UP: %d TSID: %d",
			addts.tspec.tsinfo.traffic.userPrio,
			addts.tspec.tsinfo.traffic.tsid);
		pe_debug("no ACM: Bypass sending WMA_ADD_TS_REQ to HAL");
		lim_send_sme_addts_rsp(mac_ctx, true, eSIR_SME_SUCCESS,
				       session, addts.tspec,
				       session->smeSessionId);
	}
	/* clear the addts flag */
	mac_ctx->lim.gLimAddtsSent = false;
	return;
}

/**
 * __lim_process_del_ts_req() - To process del ts response frame
 * @mac_ctx: pointer to mac context
 * @rx_pkt_info: Received packet info
 * @session: pointer to session
 *
 * This routine is to handle del ts request frame
 *
 * Return: none
 */
static void __lim_process_del_ts_req(struct mac_context *mac_ctx,
		uint8_t *rx_pkt_info, struct pe_session *session)
{
	QDF_STATUS retval;
	struct delts_req_info delts;
	tpSirMacMgmtHdr mac_hdr;
	tpDphHashNode sta_ptr;
	uint32_t frame_len;
	uint16_t aid;
	uint8_t *body_ptr;
	uint8_t ts_status;
	struct mac_ts_info *tsinfo;
	uint8_t tspec_idx;
	uint8_t ac;
	tpDphHashNode sta_ds_ptr = NULL;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	sta_ptr = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa, &aid,
				      &session->dph.dphHashTable);
	if (!sta_ptr) {
		pe_err("Station context not found - ignoring DelTs");
		return;
	}
	/* parse the delts request */
	retval = sir_convert_delts_req2_struct(mac_ctx, body_ptr,
			frame_len, &delts);
	if (retval != QDF_STATUS_SUCCESS) {
		pe_err("DelTs parsing failed %d", retval);
		return;
	}

	if (delts.wmeTspecPresent) {
		if ((!session->limWmeEnabled) || (!sta_ptr->wmeEnabled)) {
			pe_warn("Ignore delts req: wme not enabled");
			return;
		}
		pe_debug("WME Delts received");
	} else if ((session->limQosEnabled) && sta_ptr->lleEnabled) {
		pe_debug("11e QoS Delts received");
	} else if ((session->limWsmEnabled) && sta_ptr->wsmEnabled) {
		pe_debug("WSM Delts received");
	} else {
		pe_warn("Ignoring delts request: qos not enabled/capable");
		return;
	}

	tsinfo = delts.wmeTspecPresent ? &delts.tspec.tsinfo : &delts.tsinfo;

	/* if no Admit Control, ignore the request */
	if (tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA) {
		if (upToAc(tsinfo->traffic.userPrio) >= QCA_WLAN_AC_ALL) {
			pe_warn("DelTs with UP: %d has no AC - ignoring req",
				tsinfo->traffic.userPrio);
			return;
		}
	}

	if (!LIM_IS_AP_ROLE(session))
		lim_send_sme_delts_ind(mac_ctx, &delts, aid, session);

	/* try to delete the TS */
	if (QDF_STATUS_SUCCESS !=
	    lim_admit_control_delete_ts(mac_ctx, sta_ptr->assocId, tsinfo,
				&ts_status, &tspec_idx)) {
		pe_warn("Unable to Delete TS");
		return;
	} else if (!((tsinfo->traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_HCCA)
			|| (tsinfo->traffic.accessPolicy ==
					SIR_MAC_ACCESSPOLICY_BOTH))){
		/* send message to HAL to delete TS */
		if (QDF_STATUS_SUCCESS != lim_send_hal_msg_del_ts(mac_ctx,
						tspec_idx,
						delts, session->peSessionId,
						session->bssId)) {
			pe_warn("DelTs with UP: %d failed ignoring request",
				tsinfo->traffic.userPrio);
			return;
		}
	}
	/*
	 * We successfully deleted the TSPEC. Update the dynamic UAPSD Mask.
	 * The AC for this TSPEC is no longer trigger enabled if this Tspec
	 * was set-up in uplink direction only.
	 * The AC for this TSPEC is no longer delivery enabled if this Tspec
	 * was set-up in downlink direction only.
	 * The AC for this TSPEC is no longer triiger enabled and delivery
	 * enabled if this Tspec was a bidirectional TSPEC.
	 */
	lim_set_tspec_uapsd_mask_per_session(mac_ctx, session,
					     tsinfo, CLEAR_UAPSD_MASK);
	/*
	 * We're deleting the TSPEC.
	 * The AC for this TSPEC is no longer admitted in uplink/downlink
	 * direction if this TSPEC was set-up in uplink/downlink direction only.
	 * The AC for this TSPEC is no longer admitted in both uplink and
	 * downlink directions if this TSPEC was a bi-directional TSPEC.
	 * If ACM is set for this AC and this AC is admitted only in downlink
	 * direction, PE needs to downgrade the EDCA parameter
	 * (for the AC for which TS is being deleted) to the
	 * next best AC for which ACM is not enabled, and send the
	 * updated values to HAL.
	 */
	ac = upToAc(tsinfo->traffic.userPrio);
	if (tsinfo->traffic.direction == SIR_MAC_DIRECTION_UPLINK) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
	} else if (tsinfo->traffic.direction ==
		   SIR_MAC_DIRECTION_DNLINK) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	} else if (tsinfo->traffic.direction == SIR_MAC_DIRECTION_BIDIR) {
		session->gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &=
			~(1 << ac);
		session->gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &=
			~(1 << ac);
	}
	lim_set_active_edca_params(mac_ctx, session->gLimEdcaParams,
				   session);
	sta_ds_ptr = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				   &session->dph.dphHashTable);
	if (sta_ds_ptr)
		lim_send_edca_params(mac_ctx, session->gLimEdcaParamsActive,
				     session->vdev_id, false);
	else
		pe_err("Self entry missing in Hash Table");

	pe_debug("DeleteTS succeeded");
#ifdef FEATURE_WLAN_ESE
	lim_send_sme_tsm_ie_ind(mac_ctx, session, 0, 0, 0);
#endif
}

/**
 * __lim_process_qos_map_configure_frame() - to process QoS map configure frame
 * @mac_ctx: pointer to mac context
 * @rx_pkt_info: pointer to received packet info
 * @session: pointer to session
 *
 * This routine will called to process qos map configure frame
 *
 * Return: none
 */
static void __lim_process_qos_map_configure_frame(struct mac_context *mac_ctx,
			uint8_t *rx_pkt_info, struct pe_session *session)
{
	tpSirMacMgmtHdr mac_hdr;
	uint32_t frame_len;
	uint8_t *body_ptr;
	QDF_STATUS retval;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	retval = sir_convert_qos_map_configure_frame2_struct(mac_ctx,
				body_ptr, frame_len, &session->QosMapSet);
	if (retval != QDF_STATUS_SUCCESS) {
		pe_err("QosMapConfigure frame parsing fail %d", retval);
		return;
	}
	lim_send_sme_mgmt_frame_ind(mac_ctx, mac_hdr->fc.subType,
				    (uint8_t *)mac_hdr,
				    frame_len + sizeof(tSirMacMgmtHdr), 0,
				    WMA_GET_RX_FREQ(rx_pkt_info), session,
				    WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info),
				    RXMGMT_FLAG_NONE);
}

#ifdef ANI_SUPPORT_11H
static void
__lim_process_basic_meas_req(struct mac_context *mac,
			     tpSirMacMeasReqActionFrame pMeasReqFrame,
			     tSirMacAddr peerMacAddr, struct pe_session *pe_session)
{
	if (lim_send_meas_report_frame(mac, pMeasReqFrame,
				       peerMacAddr, pe_session) !=
					 QDF_STATUS_SUCCESS) {
		pe_err("fail to send Basic Meas report");
		return;
	}
}
static void
__lim_process_cca_meas_req(struct mac_context *mac,
			   tpSirMacMeasReqActionFrame pMeasReqFrame,
			   tSirMacAddr peerMacAddr, struct pe_session *pe_session)
{
	if (lim_send_meas_report_frame(mac, pMeasReqFrame,
				       peerMacAddr, pe_session) !=
					 QDF_STATUS_SUCCESS) {
		pe_err("fail to send CCA Meas report");
		return;
	}
}
static void
__lim_process_rpi_meas_req(struct mac_context *mac,
			   tpSirMacMeasReqActionFrame pMeasReqFrame,
			   tSirMacAddr peerMacAddr, struct pe_session *pe_session)
{
	if (lim_send_meas_report_frame(mac, pMeasReqFrame,
				       peerMacAddr, pe_session) !=
					 QDF_STATUS_SUCCESS) {
		pe_err("fail to send RPI Meas report");
		return;
	}
}
static void
__lim_process_measurement_request_frame(struct mac_context *mac,
					uint8_t *pRxPacketInfo,
					struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	uint8_t *pBody;
	tpSirMacMeasReqActionFrame pMeasReqFrame;
	uint32_t frameLen;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	pMeasReqFrame = qdf_mem_malloc(sizeof(tSirMacMeasReqActionFrame));
	if (!pMeasReqFrame)
		return;

	if (sir_convert_meas_req_frame2_struct(mac, pBody, pMeasReqFrame, frameLen)
	    != QDF_STATUS_SUCCESS) {
		pe_warn("Rcv invalid Measurement Request Action Frame");
		return;
	}
	switch (pMeasReqFrame->measReqIE.measType) {
	case SIR_MAC_BASIC_MEASUREMENT_TYPE:
		__lim_process_basic_meas_req(mac, pMeasReqFrame, pHdr->sa,
					     pe_session);
		break;
	case SIR_MAC_CCA_MEASUREMENT_TYPE:
		__lim_process_cca_meas_req(mac, pMeasReqFrame, pHdr->sa,
					   pe_session);
		break;
	case SIR_MAC_RPI_MEASUREMENT_TYPE:
		__lim_process_rpi_meas_req(mac, pMeasReqFrame, pHdr->sa,
					   pe_session);
		break;
	default:
		pe_warn("Unknown Measurement Type: %d",
			       pMeasReqFrame->measReqIE.measType);
		break;
	}
} /*** end limProcessMeasurementRequestFrame ***/
static void
__lim_process_tpc_request_frame(struct mac_context *mac, uint8_t *pRxPacketInfo,
				struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	uint8_t *pBody;
	tpSirMacTpcReqActionFrame pTpcReqFrame;
	uint32_t frameLen;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
	pe_debug("****LIM: Processing TPC Request from peer ****");

	pTpcReqFrame = qdf_mem_malloc(sizeof(tSirMacTpcReqActionFrame));
	if (!pTpcReqFrame)
		return;

	if (sir_convert_tpc_req_frame2_struct(mac, pBody, pTpcReqFrame, frameLen) !=
	    QDF_STATUS_SUCCESS) {
		pe_warn("Rcv invalid TPC Req Action Frame");
		return;
	}
	if (lim_send_tpc_report_frame(mac,
				      pTpcReqFrame,
				      pHdr->sa, pe_session) != QDF_STATUS_SUCCESS) {
		pe_err("fail to send TPC Report Frame");
		return;
	}
}
#endif

static void
__lim_process_sm_power_save_update(struct mac_context *mac, uint8_t *pRxPacketInfo,
				   struct pe_session *pe_session)
{

	tpSirMacMgmtHdr pHdr;
	tDot11fSMPowerSave frmSMPower;
	tSirMacHTMIMOPowerSaveState state;
	tpDphHashNode pSta;
	uint16_t aid;
	uint32_t frameLen, nStatus;
	uint8_t *pBody;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	pSta =
		dph_lookup_hash_entry(mac, pHdr->sa, &aid,
				      &pe_session->dph.dphHashTable);
	if (!pSta) {
		pe_err("STA context not found - ignoring UpdateSM PSave Mode from");
		lim_print_mac_addr(mac, pHdr->sa, LOGE);
		return;
	}

	/**Unpack the received frame */
	nStatus = dot11f_unpack_sm_power_save(mac, pBody, frameLen,
					      &frmSMPower, false);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to unpack and parse a Update SM Power (0x%08x, %d bytes):",
			nStatus, frameLen);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pBody, frameLen);
		return;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_debug("There were warnings while unpacking a SMPower Save update (0x%08x, %d bytes):",
			nStatus, frameLen);
	}

	pe_debug("Received SM Power save Mode update Frame with PS_Enable: %d"
		   "PS Mode: %d", frmSMPower.SMPowerModeSet.PowerSave_En,
		frmSMPower.SMPowerModeSet.Mode);

	/** Update in the DPH Table about the Update in the SM Power Save mode*/
	if (frmSMPower.SMPowerModeSet.PowerSave_En
	    && frmSMPower.SMPowerModeSet.Mode)
		state = eSIR_HT_MIMO_PS_DYNAMIC;
	else if ((frmSMPower.SMPowerModeSet.PowerSave_En)
		 && (frmSMPower.SMPowerModeSet.Mode == 0))
		state = eSIR_HT_MIMO_PS_STATIC;
	else if ((frmSMPower.SMPowerModeSet.PowerSave_En == 0)
		 && (frmSMPower.SMPowerModeSet.Mode == 0))
		state = eSIR_HT_MIMO_PS_NO_LIMIT;
	else {
		pe_warn("Received SM Power save Mode update Frame with invalid mode");
		return;
	}

	if (state == pSta->htMIMOPSState) {
		pe_err("The PEER is already set in the same mode");
		return;
	}

	/** Update in the HAL Station Table for the Update of the Protection Mode */
	pSta->htMIMOPSState = state;
	lim_post_sm_state_update(mac, pSta->htMIMOPSState,
				 pSta->staAddr, pe_session->smeSessionId);
}


static void
__lim_process_radio_measure_request(struct mac_context *mac, uint8_t *pRxPacketInfo,
				    struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	tDot11fRadioMeasurementRequest *frm;
	uint32_t frameLen, nStatus;
	uint8_t *pBody;
	uint16_t curr_seq_num;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	if (!pe_session) {
		return;
	}

	curr_seq_num = ((pHdr->seqControl.seqNumHi <<
			 HIGH_SEQ_NUM_OFFSET) |
			pHdr->seqControl.seqNumLo);
	if (curr_seq_num == mac->rrm.rrmPEContext.prev_rrm_report_seq_num &&
	    (mac->rrm.rrmPEContext.pCurrentReq[DEFAULT_RRM_IDX] ||
	     mac->rrm.rrmPEContext.pCurrentReq[DEFAULT_RRM_IDX + 1])) {
		pe_err("rrm report req frame, seq num: %d is already in progress, drop it",
			curr_seq_num);
		return;
	}
	/* Save seq no of currently processing rrm report req frame */
	mac->rrm.rrmPEContext.prev_rrm_report_seq_num = curr_seq_num;
	lim_send_sme_mgmt_frame_ind(mac, pHdr->fc.subType, (uint8_t *)pHdr,
				    frameLen + sizeof(tSirMacMgmtHdr), 0,
				    WMA_GET_RX_FREQ(pRxPacketInfo), pe_session,
				    WMA_GET_RX_RSSI_NORMALIZED(pRxPacketInfo),
				    RXMGMT_FLAG_NONE);

	frm = qdf_mem_malloc(sizeof(*frm));
	if (!frm)
		return;

	/**Unpack the received frame */
	nStatus = dot11f_unpack_radio_measurement_request(mac, pBody,
							  frameLen, frm, false);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to unpack and parse a Radio Measure request (0x%08x, %d bytes):",
			nStatus, frameLen);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pBody, frameLen);
		goto err;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_debug("Warnings while unpacking a Radio Measure request (0x%08x, %d bytes):",
			 nStatus, frameLen);
	}
	/* Call rrm function to handle the request. */

	rrm_process_radio_measurement_request(mac, pHdr->sa, frm,
					      pe_session);
err:
	qdf_mem_free(frm);
}

static QDF_STATUS
__lim_process_link_measurement_req(struct mac_context *mac, uint8_t *pRxPacketInfo,
				   struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	tDot11fLinkMeasurementRequest frm;
	uint32_t frameLen, nStatus;
	uint8_t *pBody;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	if (!pe_session) {
		return QDF_STATUS_E_FAILURE;
	}

	/**Unpack the received frame */
	nStatus =
		dot11f_unpack_link_measurement_request(mac, pBody, frameLen,
						       &frm, false);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to unpack and parse a Link Measure request (0x%08x, %d bytes):",
			nStatus, frameLen);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pBody, frameLen);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_debug("There were warnings while unpacking a Link Measure request (0x%08x, %d bytes):",
			nStatus, frameLen);
	}
	/* Call rrm function to handle the request. */

	return rrm_process_link_measurement_request(mac, pRxPacketInfo, &frm,
					     pe_session);

}

static void
__lim_process_neighbor_report(struct mac_context *mac, uint8_t *pRxPacketInfo,
			      struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	tDot11fNeighborReportResponse *pFrm;
	uint32_t frameLen, nStatus;
	uint8_t *pBody;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	pFrm = qdf_mem_malloc(sizeof(tDot11fNeighborReportResponse));
	if (!pFrm)
		return;

	if (!pe_session) {
		qdf_mem_free(pFrm);
		return;
	}

	/**Unpack the received frame */
	nStatus =
		dot11f_unpack_neighbor_report_response(mac, pBody,
						       frameLen, pFrm, false);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to unpack and parse a Neighbor report response (0x%08x, %d bytes):",
			nStatus, frameLen);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pBody, frameLen);
		qdf_mem_free(pFrm);
		return;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_debug("There were warnings while unpacking a Neighbor report response (0x%08x, %d bytes):",
			nStatus, frameLen);
	}

	/* Call rrm function to handle the request. */
	rrm_process_neighbor_report_response(mac, pFrm, pe_session);

	qdf_mem_free(pFrm);
}


#ifdef WLAN_FEATURE_11W
/**
 * limProcessSAQueryRequestActionFrame
 *
 ***FUNCTION:
 * This function is called by lim_process_action_frame() upon
 * SA query request Action frame reception.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - Handle to the Rx packet info
 * @param  pe_session - PE session entry
 *
 * @return None
 */
static void __lim_process_sa_query_request_action_frame(struct mac_context *mac,
							uint8_t *pRxPacketInfo,
							struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	uint8_t *pBody;
	uint32_t frame_len;
	uint8_t transId[2];

	/* Prima  --- Below Macro not available in prima
	   pHdr = SIR_MAC_BD_TO_MPDUHEADER(pBd);
	   pBody = SIR_MAC_BD_TO_MPDUDATA(pBd); */

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	if (frame_len < SA_QUERY_REQ_MIN_LEN) {
		pe_err("Invalid frame length %d", frame_len);
		return;
	}
	/* If this is an unprotected SA Query Request, then ignore it. */
	if (pHdr->fc.wep == 0)
		return;

	/* 11w offload is enabled then firmware should not fwd this frame */
	if (LIM_IS_STA_ROLE(pe_session) && mac->pmf_offload) {
		pe_err("11w offload enabled, SA Query req isn't expected");
		return;
	}

	/*Extract 11w trsansId from SA query request action frame
	   In SA query response action frame we will send same transId
	   In SA query request action frame:
	   Category       : 1 byte
	   Action         : 1 byte
	   Transaction ID : 2 bytes */
	qdf_mem_copy(&transId[0], &pBody[2], 2);

	/* Send 11w SA query response action frame */
	if (lim_send_sa_query_response_frame(mac,
					     transId,
					     pHdr->sa,
					     pe_session) != QDF_STATUS_SUCCESS) {
		pe_err("fail to send SA query response action frame");
		return;
	}
}

/**
 * __lim_process_sa_query_response_action_frame
 *
 ***FUNCTION:
 * This function is called by lim_process_action_frame() upon
 * SA query response Action frame reception.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - Handle to the Rx packet info
 * @param  pe_session - PE session entry
 * @return None
 */
static void __lim_process_sa_query_response_action_frame(struct mac_context *mac,
							 uint8_t *pRxPacketInfo,
							 struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	uint32_t frame_len;
	uint8_t *pBody;
	tpDphHashNode pSta;
	uint16_t aid;
	uint16_t transId;
	uint8_t retryNum;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	pe_debug("SA Query Response received");

	if (frame_len < SA_QUERY_RESP_MIN_LEN) {
		pe_err("Invalid frame length %d", frame_len);
		return;
	}
	/* When a station, supplicant handles SA Query Response.
	 * Forward to SME to HDD to wpa_supplicant.
	 */
	if (LIM_IS_STA_ROLE(pe_session)) {
		lim_send_sme_mgmt_frame_ind(mac, pHdr->fc.subType,
					    (uint8_t *)pHdr,
					    frame_len + sizeof(tSirMacMgmtHdr),
					    0,
					    WMA_GET_RX_FREQ(pRxPacketInfo),
					    pe_session,
					    WMA_GET_RX_RSSI_NORMALIZED(
					    pRxPacketInfo), RXMGMT_FLAG_NONE);
		return;
	}

	/* If this is an unprotected SA Query Response, then ignore it. */
	if (pHdr->fc.wep == 0)
		return;

	pSta =
		dph_lookup_hash_entry(mac, pHdr->sa, &aid,
				      &pe_session->dph.dphHashTable);
	if (!pSta)
		return;

	pe_debug("SA Query Response source addr:  %0x:%0x:%0x:%0x:%0x:%0x",
		pHdr->sa[0], pHdr->sa[1], pHdr->sa[2], pHdr->sa[3],
		pHdr->sa[4], pHdr->sa[5]);
	pe_debug("SA Query state for station: %d", pSta->pmfSaQueryState);

	if (DPH_SA_QUERY_IN_PROGRESS != pSta->pmfSaQueryState)
		return;

	/* Extract 11w trsansId from SA query response action frame
	   In SA query response action frame:
	   Category       : 1 byte
	   Action         : 1 byte
	   Transaction ID : 2 bytes */
	qdf_mem_copy(&transId, &pBody[2], 2);

	/* If SA Query is in progress with the station and the station
	   responds then the association request that triggered the SA
	   query is from a rogue station, just go back to initial state. */
	for (retryNum = 0; retryNum <= pSta->pmfSaQueryRetryCount; retryNum++)
		if (transId == pSta->pmfSaQueryStartTransId + retryNum) {
			pe_debug("Found matching SA Query Request - transaction ID: %d",
				transId);
			tx_timer_deactivate(&pSta->pmfSaQueryTimer);
			pSta->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
			break;
		}
}
#endif

#ifdef WLAN_FEATURE_11W
/**
 * lim_drop_unprotected_action_frame
 *
 ***FUNCTION:
 * This function checks if an Action frame should be dropped since it is
 * a Robust Management Frame, it is unprotected, and it is received on a
 * connection where PMF is enabled.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac - Global MAC structure
 * @param  pe_session - PE session entry
 * @param  pHdr - Frame header
 * @param  category - Action frame category
 * @return true if frame should be dropped
 */

static bool
lim_drop_unprotected_action_frame(struct mac_context *mac, struct pe_session *pe_session,
				  tpSirMacMgmtHdr pHdr, uint8_t category)
{
	uint16_t aid;
	tpDphHashNode sta;
	bool rmfConnection = false;

	sta = dph_lookup_hash_entry(mac, pHdr->sa, &aid,
				    &pe_session->dph.dphHashTable);
	if (sta && sta->rmfEnabled)
		rmfConnection = true;

	if (rmfConnection && (pHdr->fc.wep == 0)) {
		pe_err("Dropping unprotected Action category: %d frame since RMF is enabled",
			category);
		return true;
	}

	return false;
}
#endif

/**
 * lim_process_addba_req() - process ADDBA Request
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to packet info structure
 * @session: PE session pointer
 *
 * This routine will be called to process ADDBA request action frame
 *
 * Return: None
 */
static void lim_process_addba_req(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
				  struct pe_session *session)
{
	tpSirMacMgmtHdr mac_hdr;
	uint8_t *body_ptr;
	tDot11faddba_req *addba_req;
	uint32_t frame_len, status;
	QDF_STATUS qdf_status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	tpDphHashNode sta_ds;
	uint16_t aid, buff_size;
	bool he_cap = false;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	QDF_TRACE_HEX_DUMP_DEBUG_RL(QDF_MODULE_ID_PE, body_ptr, frame_len);

	addba_req = qdf_mem_malloc(sizeof(*addba_req));
	if (!addba_req)
		return;

	/* Unpack ADDBA request frame */
	status = dot11f_unpack_addba_req(mac_ctx, body_ptr, frame_len,
					 addba_req, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to unpack and parse (0x%08x, %d bytes)",
			status, frame_len);
		goto error;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("warning: unpack addba Req(0x%08x, %d bytes)",
			status, frame_len);
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa, &aid,
				       &session->dph.dphHashTable);
	if (sta_ds && lim_is_session_he_capable(session))
		he_cap = lim_is_sta_he_capable(sta_ds);
	if (sta_ds && sta_ds->staType == STA_ENTRY_NDI_PEER)
		he_cap = lim_is_session_he_capable(session);

	if (he_cap)
		buff_size = MAX_BA_BUFF_SIZE;
	else
		buff_size = SIR_MAC_BA_DEFAULT_BUFF_SIZE;

	if (mac_ctx->usr_cfg_ba_buff_size)
		buff_size = mac_ctx->usr_cfg_ba_buff_size;

	if (addba_req->addba_param_set.buff_size)
		buff_size = QDF_MIN(buff_size,
				    addba_req->addba_param_set.buff_size);

	pe_debug("token %d tid %d timeout %d buff_size in frame %d buf_size calculated %d ssn %d",
		 addba_req->DialogToken.token, addba_req->addba_param_set.tid,
		 addba_req->ba_timeout.timeout,
		 addba_req->addba_param_set.buff_size, buff_size,
		 addba_req->ba_start_seq_ctrl.ssn);

	qdf_status = cdp_addba_requestprocess(
					soc, mac_hdr->sa,
					session->vdev_id,
					addba_req->DialogToken.token,
					addba_req->addba_param_set.tid,
					addba_req->ba_timeout.timeout,
					buff_size,
					addba_req->ba_start_seq_ctrl.ssn);

	if (QDF_STATUS_SUCCESS == qdf_status) {
		qdf_status = lim_send_addba_response_frame(mac_ctx,
			mac_hdr->sa,
			addba_req->addba_param_set.tid,
			session,
			addba_req->addba_extn_element.present,
			addba_req->addba_param_set.amsdu_supp,
			mac_hdr->fc.wep, buff_size);
		if (qdf_status != QDF_STATUS_SUCCESS) {
			pe_err("Failed to send addba response frame");
			cdp_addba_resp_tx_completion(
					soc, mac_hdr->sa, session->vdev_id,
					addba_req->addba_param_set.tid,
					WMI_MGMT_TX_COMP_TYPE_DISCARD);
		}
	} else {
		pe_err_rl("Failed to process addba request");
	}

error:
	qdf_mem_free(addba_req);
	return;
}

/**
 * lim_process_delba_req() - process DELBA Request
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to packet info structure
 * @session: PE session pointer
 *
 * This routine will be called to process ADDBA request action frame
 *
 * Return: None
 */
static void lim_process_delba_req(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
				  struct pe_session *session)
{
	tpSirMacMgmtHdr mac_hdr;
	uint8_t *body_ptr;
	tDot11fdelba_req *delba_req;
	uint32_t frame_len, status;
	QDF_STATUS qdf_status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
			   body_ptr, frame_len);

	delba_req = qdf_mem_malloc(sizeof(*delba_req));
	if (!delba_req)
		return;

	/* Unpack DELBA request frame */
	status = dot11f_unpack_delba_req(mac_ctx, body_ptr, frame_len,
					 delba_req, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to unpack and parse (0x%08x, %d bytes)",
			status, frame_len);
		goto error;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("warning: unpack addba Req(0x%08x, %d bytes)",
			status, frame_len);
	}

	qdf_status = cdp_delba_process(soc, mac_hdr->sa, session->vdev_id,
			delba_req->delba_param_set.tid, delba_req->Reason.code);

	if (QDF_STATUS_SUCCESS != qdf_status)
		pe_err("Failed to process delba request");

error:
	qdf_mem_free(delba_req);
	return;
}

/**
 * lim_process_action_frame() - to process action frames
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to packet info structure
 *
 * This function is called by limProcessMessageQueue() upon
 * Action frame reception.
 *
 * Return: none
 */

void lim_process_action_frame(struct mac_context *mac_ctx,
		uint8_t *rx_pkt_info, struct pe_session *session)
{
	uint8_t *body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	tpSirMacActionFrameHdr action_hdr = (tpSirMacActionFrameHdr) body_ptr;
#ifdef WLAN_FEATURE_11W
	tpSirMacMgmtHdr mac_hdr_11w = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
#endif
	tpSirMacMgmtHdr mac_hdr = NULL;
	int8_t rssi;
	uint32_t frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	tpSirMacVendorSpecificFrameHdr vendor_specific;
	uint8_t oui[] = { 0x00, 0x00, 0xf0 };
	uint8_t dpp_oui[] = { 0x50, 0x6F, 0x9A, 0x1A };
	tpSirMacVendorSpecificPublicActionFrameHdr pub_action;

	if (frame_len < sizeof(*action_hdr)) {
		pe_debug("frame_len %d less than Action Frame Hdr size",
			 frame_len);
		return;
	}

#ifdef WLAN_FEATURE_11W
	if (lim_is_robust_mgmt_action_frame(action_hdr->category) &&
	   lim_drop_unprotected_action_frame(mac_ctx, session,
			mac_hdr_11w, action_hdr->category))
		return;
#endif

	switch (action_hdr->category) {
	case ACTION_CATEGORY_QOS:
		if ((session->limQosEnabled) ||
		    (action_hdr->actionID == QOS_MAP_CONFIGURE)) {
			switch (action_hdr->actionID) {
			case QOS_ADD_TS_REQ:
				__lim_process_add_ts_req(mac_ctx,
						(uint8_t *) rx_pkt_info,
						session);
				break;

			case QOS_ADD_TS_RSP:
				__lim_process_add_ts_rsp(mac_ctx,
						 (uint8_t *) rx_pkt_info,
						 session);
				break;

			case QOS_DEL_TS_REQ:
				__lim_process_del_ts_req(mac_ctx,
						(uint8_t *) rx_pkt_info,
						session);
				break;

			case QOS_MAP_CONFIGURE:
				__lim_process_qos_map_configure_frame(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
				break;
			default:
				pe_warn("Qos action: %d not handled",
					action_hdr->actionID);
				break;
			}
			break;
		}
		break;

	case ACTION_CATEGORY_SPECTRUM_MGMT:
		switch (action_hdr->actionID) {
#ifdef ANI_SUPPORT_11H
		case ACTION_SPCT_MSR_REQ:
			if (session->lim11hEnable)
				__lim_process_measurement_request_frame(mac_ctx,
							rx_pkt_info,
							session);
			break;
		case ACTION_SPCT_TPC_REQ:
			if ((LIM_IS_STA_ROLE(session) ||
				LIM_IS_AP_ROLE(session)) &&
				session->lim11hEnable)
					__lim_process_tpc_request_frame(mac_ctx,
						rx_pkt_info, session);
			break;
#endif
		default:
			pe_warn("Spectrum mgmt action id: %d not handled",
				action_hdr->actionID);
			break;
		}
		break;

	case ACTION_CATEGORY_WMM:
		if (!session->limWmeEnabled) {
			pe_warn("WME mode disabled - dropping frame: %d",
				action_hdr->actionID);
			break;
		}
		switch (action_hdr->actionID) {
		case QOS_ADD_TS_REQ:
			__lim_process_add_ts_req(mac_ctx,
				(uint8_t *) rx_pkt_info, session);
			break;

		case QOS_ADD_TS_RSP:
			__lim_process_add_ts_rsp(mac_ctx,
				(uint8_t *) rx_pkt_info, session);
			break;

		case QOS_DEL_TS_REQ:
			__lim_process_del_ts_req(mac_ctx,
				(uint8_t *) rx_pkt_info, session);
			break;

		case QOS_MAP_CONFIGURE:
			__lim_process_qos_map_configure_frame(mac_ctx,
				(uint8_t *)rx_pkt_info, session);
			break;

		default:
			pe_warn("WME action: %d not handled",
				action_hdr->actionID);
			break;
		}
		break;

	case ACTION_CATEGORY_HT:
		/** Type of HT Action to be performed*/
		switch (action_hdr->actionID) {
		case SIR_MAC_SM_POWER_SAVE:
			if (LIM_IS_AP_ROLE(session))
				__lim_process_sm_power_save_update(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
			break;
		default:
			pe_warn("Action ID: %d not handled in HT category",
				action_hdr->actionID);
			break;
		}
		break;

	case ACTION_CATEGORY_WNM:
		pe_debug("WNM Action category: %d action: %d",
			action_hdr->category, action_hdr->actionID);
		switch (action_hdr->actionID) {
		case WNM_BSS_TM_QUERY:
		case WNM_BSS_TM_REQUEST:
		case WNM_BSS_TM_RESPONSE:
			if (cfg_p2p_is_roam_config_disabled(mac_ctx->psoc) &&
			    session && LIM_IS_STA_ROLE(session) &&
			    (policy_mgr_mode_specific_connection_count(
				mac_ctx->psoc, PM_P2P_CLIENT_MODE, NULL) ||
			     policy_mgr_mode_specific_connection_count(
				mac_ctx->psoc, PM_P2P_GO_MODE, NULL))) {
				pe_debug("p2p session active drop BTM frame");
				break;
			}
			/* fallthrough */
		case WNM_NOTIF_REQUEST:
		case WNM_NOTIF_RESPONSE:
			rssi = WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
			mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
			/* Forward to the SME to HDD to wpa_supplicant */
			lim_send_sme_mgmt_frame_ind(mac_ctx,
					mac_hdr->fc.subType,
					(uint8_t *) mac_hdr,
					frame_len + sizeof(tSirMacMgmtHdr),
					session->smeSessionId,
					WMA_GET_RX_FREQ(rx_pkt_info),
					session, rssi, RXMGMT_FLAG_NONE);
			break;
		default:
			pe_debug("Action ID: %d not handled in WNM category",
				action_hdr->actionID);
			break;
		}
		break;

	case ACTION_CATEGORY_RRM:
		/* Ignore RRM measurement request until DHCP is set */
		if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
		    mac_ctx->roam.roamSession
		    [session->smeSessionId].dhcp_done) {
			switch (action_hdr->actionID) {
			case RRM_RADIO_MEASURE_REQ:
				__lim_process_radio_measure_request(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
				break;
			case RRM_LINK_MEASUREMENT_REQ:
				if (!lim_is_valid_frame(
					&rrm_link_action_frm,
					rx_pkt_info))
					break;

				if (__lim_process_link_measurement_req(
						mac_ctx,
						(uint8_t *)rx_pkt_info,
						session) == QDF_STATUS_SUCCESS)
					lim_update_last_processed_frame(
							&rrm_link_action_frm,
							rx_pkt_info);

				break;
			case RRM_NEIGHBOR_RPT:
				__lim_process_neighbor_report(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
				break;
			default:
				pe_warn("Action ID: %d not handled in RRM",
					action_hdr->actionID);
				break;

			}
		} else {
			/* Else we will just ignore the RRM messages. */
			pe_debug("RRM frm ignored, it is disabled in cfg: %d or DHCP not completed: %d",
			  mac_ctx->rrm.rrmPEContext.rrmEnable,
			  mac_ctx->roam.roamSession
			  [session->smeSessionId].dhcp_done);
		}
		break;

	case SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY:
		vendor_specific = (tpSirMacVendorSpecificFrameHdr) action_hdr;
		mac_hdr = NULL;

		mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);

		if (frame_len < sizeof(*vendor_specific)) {
			pe_debug("frame len %d less than Vendor Specific Hdr len",
				 frame_len);
			return;
		}

		/* Check if it is a vendor specific action frame. */
		if (LIM_IS_STA_ROLE(session) &&
		    (!qdf_mem_cmp(session->self_mac_addr,
					&mac_hdr->da[0], sizeof(tSirMacAddr)))
		    && IS_WES_MODE_ENABLED(mac_ctx)
		    && !qdf_mem_cmp(vendor_specific->Oui, oui, 3)) {
			pe_debug("Rcvd Vendor specific frame OUI: %x %x %x",
				vendor_specific->Oui[0],
				vendor_specific->Oui[1],
				vendor_specific->Oui[2]);
			/*
			 * Forward to the SME to HDD to wpa_supplicant
			 * type is ACTION
			 */
			lim_send_sme_mgmt_frame_ind(mac_ctx,
					mac_hdr->fc.subType,
					(uint8_t *) mac_hdr,
					frame_len +
					sizeof(tSirMacMgmtHdr),
					session->smeSessionId,
					WMA_GET_RX_FREQ(rx_pkt_info),
					session,
					WMA_GET_RX_RSSI_NORMALIZED(
					rx_pkt_info), RXMGMT_FLAG_NONE);
		} else {
			pe_debug("Dropping the vendor specific action frame"
					"beacause of (WES Mode not enabled "
					"(WESMODE: %d) or OUI mismatch "
					"(%02x %02x %02x) or not received with"
					"SelfSta address) system role: %d",
				IS_WES_MODE_ENABLED(mac_ctx),
				vendor_specific->Oui[0],
				vendor_specific->Oui[1],
				vendor_specific->Oui[2],
				GET_LIM_SYSTEM_ROLE(session));
		}
	break;
	case ACTION_CATEGORY_PUBLIC:
		mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);

		switch (action_hdr->actionID) {
		case SIR_MAC_ACTION_EXT_CHANNEL_SWITCH_ID:
			lim_process_ext_channel_switch_action_frame(mac_ctx,
							rx_pkt_info, session);
			break;
		case SIR_MAC_ACTION_VENDOR_SPECIFIC:
			pub_action =
				(tpSirMacVendorSpecificPublicActionFrameHdr)
				action_hdr;
			if (frame_len < sizeof(*pub_action)) {
				pe_debug("Received vendor specific public action frame of invalid len %d",
					 frame_len);
				return;
			}
			/*
			 * Check if it is a DPP public action frame and fall
			 * thru, else drop the frame.
			 */
			if (qdf_mem_cmp(pub_action->Oui, dpp_oui, 4)) {
				pe_debug("Unhandled public action frame (Vendor specific) OUI: %x %x %x %x",
					pub_action->Oui[0], pub_action->Oui[1],
					pub_action->Oui[2], pub_action->Oui[3]);
				break;
			}
			/* send the frame to supplicant */
			/* fallthrough */
		case SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY:
		case SIR_MAC_ACTION_2040_BSS_COEXISTENCE:
		case SIR_MAC_ACTION_GAS_INITIAL_REQUEST:
		case SIR_MAC_ACTION_GAS_INITIAL_RESPONSE:
		case SIR_MAC_ACTION_GAS_COMEBACK_REQUEST:
		case SIR_MAC_ACTION_GAS_COMEBACK_RESPONSE:
			/*
			 * Forward to the SME to HDD to wpa_supplicant
			 * type is ACTION
			 */
			lim_send_sme_mgmt_frame_ind(mac_ctx,
					mac_hdr->fc.subType,
					(uint8_t *) mac_hdr,
					frame_len + sizeof(tSirMacMgmtHdr),
					session->smeSessionId,
					WMA_GET_RX_FREQ(rx_pkt_info), session,
					WMA_GET_RX_RSSI_NORMALIZED(
					rx_pkt_info), RXMGMT_FLAG_NONE);
			break;
		default:
			pe_debug("Unhandled public action frame: %d",
				 action_hdr->actionID);
			break;
		}
		break;

#ifdef WLAN_FEATURE_11W
	case ACTION_CATEGORY_SA_QUERY:
		pe_debug("SA Query Action category: %d action: %d",
			action_hdr->category, action_hdr->actionID);
		switch (action_hdr->actionID) {
		case SA_QUERY_REQUEST:
			/**11w SA query request action frame received**/
			/* Respond directly to the incoming request in LIM */
			__lim_process_sa_query_request_action_frame(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
			break;
		case SA_QUERY_RESPONSE:
			/**11w SA query response action frame received**/
			/* Handle based on the current SA Query state */
			__lim_process_sa_query_response_action_frame(mac_ctx,
						(uint8_t *)rx_pkt_info,
						session);
			break;
		default:
			break;
		}
		break;
#endif
	case ACTION_CATEGORY_VHT:
		if (!session->vhtCapability)
			break;
		switch (action_hdr->actionID) {
		case SIR_MAC_VHT_OPMODE_NOTIFICATION:
			__lim_process_operating_mode_action_frame(mac_ctx,
					rx_pkt_info, session);
			break;
		case SIR_MAC_VHT_GID_NOTIFICATION:
			/* Only if ini supports it */
			if (session->enableVhtGid)
				__lim_process_gid_management_action_frame(
					mac_ctx, rx_pkt_info, session);
			break;
		default:
			break;
		}
		break;
	case ACTION_CATEGORY_RVS:
	case ACTION_CATEGORY_FST: {
		tpSirMacMgmtHdr     hdr;

		hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);

		pe_debug("Received %s MGMT action frame",
			 (action_hdr->category == ACTION_CATEGORY_FST) ?
			"FST" : "RVS");

		/* Forward to the SME to HDD */
		lim_send_sme_mgmt_frame_ind(mac_ctx, hdr->fc.subType,
					    (uint8_t *)hdr,
					    frame_len + sizeof(tSirMacMgmtHdr),
					    session->smeSessionId,
					    WMA_GET_RX_FREQ(rx_pkt_info),
					    session,
					    WMA_GET_RX_RSSI_NORMALIZED(
					    rx_pkt_info), RXMGMT_FLAG_NONE);
		break;
	}
	case ACTION_CATEGORY_PROTECTED_DUAL_OF_PUBLIC_ACTION:
		pe_debug("Rcvd Protected Dual of Public Action: %d",
			action_hdr->actionID);
		switch (action_hdr->actionID) {
		case SIR_MAC_PDPA_GAS_INIT_REQ:
		case SIR_MAC_PDPA_GAS_INIT_RSP:
		case SIR_MAC_PDPA_GAS_COMEBACK_REQ:
		case SIR_MAC_PDPA_GAS_COMEBACK_RSP:
			mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
			rssi = WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info);
			lim_send_sme_mgmt_frame_ind(mac_ctx,
				mac_hdr->fc.subType, (uint8_t *) mac_hdr,
				frame_len + sizeof(tSirMacMgmtHdr),
				session->smeSessionId,
				WMA_GET_RX_FREQ(rx_pkt_info), session, rssi,
				RXMGMT_FLAG_NONE);
			break;
		default:
			pe_debug("Unhandled - Protected Dual Public Action");
			break;
		}
		break;
	case ACTION_CATEGORY_BACK:
		pe_debug("Rcvd Block Ack for "QDF_MAC_ADDR_FMT"; action: %d",
			  QDF_MAC_ADDR_REF(session->self_mac_addr),
			  action_hdr->actionID);
		switch (action_hdr->actionID) {
		case ADDBA_REQUEST:
			lim_process_addba_req(mac_ctx, rx_pkt_info, session);
			break;
		case DELBA:
			lim_process_delba_req(mac_ctx, rx_pkt_info, session);
			break;
		default:
			pe_err("Unhandle BA action frame");
			break;
		}
		break;
	default:
		pe_warn("Action category: %d not handled",
			action_hdr->category);
		break;
	}
}

/**
 * lim_process_action_frame_no_session
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Action frame reception and no session.
 * Currently only public action frames can be received from
 * a non-associated station.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  *pBd - A pointer to Buffer descriptor + associated PDUs
 * @return None
 */
void lim_process_action_frame_no_session(struct mac_context *mac, uint8_t *pBd)
{
	tpSirMacMgmtHdr mac_hdr = WMA_GET_RX_MAC_HEADER(pBd);
	uint32_t frame_len = WMA_GET_RX_PAYLOAD_LEN(pBd);
	uint8_t *pBody = WMA_GET_RX_MPDU_DATA(pBd);
	uint8_t dpp_oui[] = { 0x50, 0x6F, 0x9A, 0x1A };
	tpSirMacActionFrameHdr action_hdr = (tpSirMacActionFrameHdr) pBody;
	tpSirMacVendorSpecificPublicActionFrameHdr vendor_specific;

	pe_debug("Received an Action frame -- no session");

	if (frame_len < sizeof(*action_hdr)) {
		pe_debug("frame_len %d less than action frame header len",
			 frame_len);
		return;
	}

	switch (action_hdr->category) {
	case ACTION_CATEGORY_PUBLIC:
		switch (action_hdr->actionID) {
		case SIR_MAC_ACTION_VENDOR_SPECIFIC:
			vendor_specific =
				(tpSirMacVendorSpecificPublicActionFrameHdr)
				action_hdr;

			if (frame_len < sizeof(*vendor_specific)) {
				pe_debug("Received vendor specific public action frame of invalid len %d",
					 frame_len);
				return;
			}
			/*
			 * Check if it is a DPP public action frame and fall
			 * thru, else drop the frame.
			 */
			if (qdf_mem_cmp(vendor_specific->Oui, dpp_oui, 4)) {
				pe_debug("Unhandled public action frame (Vendor specific) OUI: %x %x %x %x",
					vendor_specific->Oui[0],
					vendor_specific->Oui[1],
					vendor_specific->Oui[2],
					vendor_specific->Oui[3]);
				break;
			}
			/* fallthrough */
		case SIR_MAC_ACTION_GAS_INITIAL_REQUEST:
		case SIR_MAC_ACTION_GAS_INITIAL_RESPONSE:
		case SIR_MAC_ACTION_GAS_COMEBACK_REQUEST:
		case SIR_MAC_ACTION_GAS_COMEBACK_RESPONSE:
			/*
			 * Forward the GAS frames to  wpa_supplicant
			 * type is ACTION
			 */
			lim_send_sme_mgmt_frame_ind(mac,
					mac_hdr->fc.subType,
					(uint8_t *) mac_hdr,
					frame_len + sizeof(tSirMacMgmtHdr), 0,
					WMA_GET_RX_FREQ(pBd), NULL,
					WMA_GET_RX_RSSI_NORMALIZED(pBd),
					RXMGMT_FLAG_NONE);
			break;
		default:
			pe_warn("Unhandled public action frame: %x",
				       action_hdr->actionID);
			break;
		}
		break;
	default:
		pe_warn("Unhandled action frame without session: %x",
			       action_hdr->category);
		break;

	}
}
