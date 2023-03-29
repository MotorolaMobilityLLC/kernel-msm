/*
 * Copyright (c) 2011-2015, 2017-2019, 2021 The Linux Foundation. All rights
 * reserved.
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
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __SCH_API_H__
#define __SCH_API_H__

#include "sir_common.h"
#include "sir_mac_prot_def.h"

#include "ani_global.h"

/* update only the broadcast qos params */
void sch_qos_update_broadcast(struct mac_context *mac,
			      struct pe_session *pe_session);

/* fill in the default local edca parameter into gLimEdcaParams[] */
void sch_set_default_edca_params(struct mac_context *mac, struct pe_session *pe_session);

/* update only local qos params */
void sch_qos_update_local(struct mac_context *mac, struct pe_session *pe_session);

/* update the edca profile parameters based on STA-SAP concurrency */
void sch_qos_concurrency_update(void);

/* update the edca profile parameters */
void sch_edca_profile_update(struct mac_context *mac,
			     struct pe_session *pe_session);

/* / Set the fixed fields in a beacon frame */
QDF_STATUS sch_set_fixed_beacon_fields(struct mac_context *mac,
				       struct pe_session *pe_session);

/**
 * sch_process_pre_beacon_ind() - Process the PreBeacon Indication from the Lim
 * @mac: pointer to mac structure
 * @msg: schedular msg
 * @reason: beaon update reason
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
QDF_STATUS sch_process_pre_beacon_ind(struct mac_context *mac,
				      struct scheduler_msg *msg,
				      enum sir_bcn_update_reason reason);

void sch_beacon_process(struct mac_context *mac, uint8_t *pRxPacketInfo,
			struct pe_session *pe_session);

QDF_STATUS sch_beacon_edca_process(struct mac_context *mac,
				   tSirMacEdcaParamSetIE *edca,
				   struct pe_session *pe_session);

void sch_generate_tim(struct mac_context *, uint8_t **, uint16_t *, uint8_t);

void sch_set_beacon_interval(struct mac_context *mac,
			     struct pe_session *pe_session);

/**
 * sch_send_beacon_req() - send beacon update req to wma
 * @mac_ctx: pointer to mac structure
 * @bcn_payload: beacon payload
 * @size: beacon size
 * @session:pe session
 * @reason: beaon update reason
 *
 * return: success: QDF_STATUS_SUCCESS failure: QDF_STATUS_E_FAILURE
 */
QDF_STATUS sch_send_beacon_req(struct mac_context *mac_ctx, uint8_t *bcn_payload,
			       uint16_t size, struct pe_session *session,
			       enum sir_bcn_update_reason reason);


QDF_STATUS lim_update_probe_rsp_template_ie_bitmap_beacon1(struct mac_context *,
							   tDot11fBeacon1 *,
							   struct pe_session *
							   pe_session);
void lim_update_probe_rsp_template_ie_bitmap_beacon2(struct mac_context *,
						     tDot11fBeacon2 *,
						     uint32_t *,
						     tDot11fProbeResponse *);
void set_probe_rsp_ie_bitmap(uint32_t *, uint32_t);
uint32_t lim_send_probe_rsp_template_to_hal(struct mac_context *,
					    struct pe_session *,
					    uint32_t *);

int sch_gen_timing_advert_frame(struct mac_context *mac, tSirMacAddr self_addr,
				uint8_t **buf, uint32_t *timestamp_offset,
				uint32_t *time_value_offset);

/*
 * sch_beacon_process_for_ap() - process the beacon frame for AP sessions
 * @mac_ctx: pointer to the global mac_ctx
 * @rx_pkt_info: pointer to the frame Rx Meta
 * @bcn: pointer to the beacon struct
 *
 * Process the beacon in the context of any existing AP or BTAP
 * session. This takes cares of following two scenarios:
 *  - session = NULL:
 * e.g. beacon received from a neighboring BSS, you want to apply the
 * protection settings to BTAP/InfraAP beacons
 *  - session is non NULL:
 * e.g. beacon received is from the INFRA AP to which you are connected
 * on another concurrent link. In this case also, we want to apply the
 * protection settings(as advertised by Infra AP) to BTAP beacons
 *
 * Return: None
 */
void sch_beacon_process_for_ap(struct mac_context *mac_ctx,
			       uint8_t session_id,
			       uint8_t *rx_pkt_info,
			       tSchBeaconStruct *bcn);

/**
 * sch_edca_profile_update_all() - update edca profile for all sessions
 * @pmac: pointer to mac structure
 *
 * return: None
 */
void sch_edca_profile_update_all(struct mac_context *pmac);

#endif
