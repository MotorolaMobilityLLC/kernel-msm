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
 *  DOC: wlan_hdd_packet_filter.c
 *
 *  WLAN Host Device Driver implementation
 *
 */

/* Include Files */
#include "wlan_hdd_packet_filter_api.h"
#include "wlan_hdd_packet_filter_rules.h"

int hdd_enable_default_pkt_filters(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	uint8_t filters = 0, i = 0, filter_id = 1;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null!!!");
		return -EINVAL;
	}
	if (hdd_ctx->user_configured_pkt_filter_rules) {
		hdd_info("user has defined pkt filter run hence skipping default packet filter rule");
		return 0;
	}

	filters = ucfg_pmo_get_pkt_filter_bitmap(hdd_ctx->psoc);

	while (filters != 0) {
		if (filters & 0x1) {
			hdd_err("setting filter[%d], of id = %d",
				i+1, filter_id);
			packet_filter_default_rules[i].filter_id = filter_id;
			wlan_hdd_set_filter(hdd_ctx,
					    &packet_filter_default_rules[i],
					    adapter->vdev_id);
			filter_id++;
		}
		filters = filters >> 1;
		i++;
	}

	return 0;
}

int hdd_disable_default_pkt_filters(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	uint8_t filters = 0, i = 0, filter_id = 1;

	struct pkt_filter_cfg packet_filter_default_rules = {0};

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("HDD context is Null!!!");
		return -EINVAL;
	}

	if (hdd_ctx->user_configured_pkt_filter_rules) {
		hdd_info("user has defined pkt filter run hence skipping default packet filter rule");
		return 0;
	}

	filters = ucfg_pmo_get_pkt_filter_bitmap(hdd_ctx->psoc);

	while (filters != 0) {
		if (filters & 0x1) {
			hdd_err("Clearing filter[%d], of id = %d",
				i+1, filter_id);
			packet_filter_default_rules.filter_action =
						HDD_RCV_FILTER_CLEAR;
			packet_filter_default_rules.filter_id = filter_id;
			wlan_hdd_set_filter(hdd_ctx,
					    &packet_filter_default_rules,
					    adapter->vdev_id);
			filter_id++;
		}
		filters = filters >> 1;
		i++;
	}

	return 0;
}

int wlan_hdd_set_filter(struct hdd_context *hdd_ctx,
				struct pkt_filter_cfg *request,
				uint8_t vdev_id)
{
	struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req = NULL;
	struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param = NULL;
	int i = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!ucfg_pmo_is_pkt_filter_enabled(hdd_ctx->psoc)) {
		hdd_warn("Packet filtering disabled in ini");
		return 0;
	}

	/* Debug display of request components. */
	hdd_debug("Packet Filter Request : FA %d params %d",
		request->filter_action, request->num_params);

	switch (request->filter_action) {
	case HDD_RCV_FILTER_SET:
		hdd_debug("Set Packet Filter Request for Id: %d",
			request->filter_id);

		pmo_set_pkt_fltr_req =
			qdf_mem_malloc(sizeof(*pmo_set_pkt_fltr_req));
		if (!pmo_set_pkt_fltr_req)
			return QDF_STATUS_E_NOMEM;

		pmo_set_pkt_fltr_req->filter_id = request->filter_id;
		if (request->num_params >= HDD_MAX_CMP_PER_PACKET_FILTER) {
			hdd_err("Number of Params exceed Max limit %d",
				request->num_params);
			status = QDF_STATUS_E_INVAL;
			goto out;
		}
		pmo_set_pkt_fltr_req->num_params = request->num_params;
		pmo_set_pkt_fltr_req->coalesce_time = 0;
		pmo_set_pkt_fltr_req->filter_type = PMO_RCV_FILTER_TYPE_FILTER_PKT;
		for (i = 0; i < request->num_params; i++) {
			pmo_set_pkt_fltr_req->params_data[i].protocol_layer =
				request->params_data[i].protocol_layer;
			pmo_set_pkt_fltr_req->params_data[i].compare_flag =
				request->params_data[i].compare_flag;
			pmo_set_pkt_fltr_req->params_data[i].data_offset =
				request->params_data[i].data_offset;
			pmo_set_pkt_fltr_req->params_data[i].data_length =
				request->params_data[i].data_length;
			pmo_set_pkt_fltr_req->params_data[i].reserved = 0;

			if (request->params_data[i].data_offset >
			    SIR_MAX_FILTER_TEST_DATA_OFFSET) {
				hdd_err("Invalid data offset %u for param %d (max = %d)",
					request->params_data[i].data_offset,
					i,
					SIR_MAX_FILTER_TEST_DATA_OFFSET);
				status = QDF_STATUS_E_INVAL;
				goto out;
			}

			if (request->params_data[i].data_length >
				SIR_MAX_FILTER_TEST_DATA_LEN) {
				hdd_err("Error invalid data length %d",
					request->params_data[i].data_length);
				status = QDF_STATUS_E_INVAL;
				goto out;
			}

			hdd_debug("Proto %d Comp Flag %d Filter Type %d",
				request->params_data[i].protocol_layer,
				request->params_data[i].compare_flag,
				pmo_set_pkt_fltr_req->filter_type);

			hdd_debug("Data Offset %d Data Len %d",
				request->params_data[i].data_offset,
				request->params_data[i].data_length);

			if (sizeof(
			    pmo_set_pkt_fltr_req->params_data[i].compare_data)
				< (request->params_data[i].data_length)) {
				hdd_err("Error invalid data length %d",
					request->params_data[i].data_length);
				status = QDF_STATUS_E_INVAL;
				goto out;
			}

			memcpy(
			    &pmo_set_pkt_fltr_req->params_data[i].compare_data,
			       request->params_data[i].compare_data,
			       request->params_data[i].data_length);
			memcpy(&pmo_set_pkt_fltr_req->params_data[i].data_mask,
			       request->params_data[i].data_mask,
			       request->params_data[i].data_length);

			hdd_debug("CData %d CData %d CData %d CData %d CData %d CData %d",
				request->params_data[i].compare_data[0],
				request->params_data[i].compare_data[1],
				request->params_data[i].compare_data[2],
				request->params_data[i].compare_data[3],
				request->params_data[i].compare_data[4],
				request->params_data[i].compare_data[5]);

			hdd_debug("MData %d MData %d MData %d MData %d MData %d MData %d",
				request->params_data[i].data_mask[0],
				request->params_data[i].data_mask[1],
				request->params_data[i].data_mask[2],
				request->params_data[i].data_mask[3],
				request->params_data[i].data_mask[4],
				request->params_data[i].data_mask[5]);
		}


		status= ucfg_pmo_set_pkt_filter(hdd_ctx->psoc,
						pmo_set_pkt_fltr_req,
						vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failure to execute Set Filter");
			status = QDF_STATUS_E_INVAL;
			goto out;
		}

		break;

	case HDD_RCV_FILTER_CLEAR:
		hdd_debug("Clear Packet Filter Request for Id: %d",
			request->filter_id);

		pmo_clr_pkt_fltr_param = qdf_mem_malloc(
					sizeof(*pmo_clr_pkt_fltr_param));
		if (!pmo_clr_pkt_fltr_param)
			return QDF_STATUS_E_NOMEM;

		pmo_clr_pkt_fltr_param->filter_id = request->filter_id;
		status = ucfg_pmo_clear_pkt_filter(hdd_ctx->psoc,
						   pmo_clr_pkt_fltr_param,
						   vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failure to execute Clear Filter");
			status = QDF_STATUS_E_INVAL;
			goto out;
		}
		break;

	default:
		hdd_err("Packet Filter Request: Invalid %d",
		       request->filter_action);
		return -EINVAL;
	}

out:
	if (pmo_set_pkt_fltr_req)
		qdf_mem_free(pmo_set_pkt_fltr_req);
	if (pmo_clr_pkt_fltr_param)
		qdf_mem_free(pmo_clr_pkt_fltr_param);

	return status;
}
