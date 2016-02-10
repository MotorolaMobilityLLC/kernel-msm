/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * DOC: wma_ocb.c
 *
 * WLAN Host Device Driver 802.11p OCB implementation
 */

#include "wma_ocb.h"
#include "wmi_unified_api.h"
#include "vos_utils.h"

/**
 * wma_ocb_resp() - send the OCB set config response via callback
 * @wma_handle: pointer to the WMA handle
 * @status: status of the set config command
 */
int wma_ocb_set_config_resp(tp_wma_handle wma_handle, uint8_t status)
{
	VOS_STATUS vos_status;
	struct sir_ocb_set_config_response *resp;
	vos_msg_t msg = {0};
	struct sir_ocb_config *req = wma_handle->ocb_config_req;
	ol_txrx_vdev_handle vdev = (req ?
		wma_handle->interfaces[req->session_id].handle : 0);

	/*
	 * If the command was successful, save the channel information in the
	 * vdev.
	 */
	if (status == VOS_STATUS_SUCCESS) {
		if (vdev && req) {
			if (vdev->ocb_channel_info)
				vos_mem_free(vdev->ocb_channel_info);
			vdev->ocb_channel_count =
				req->channel_count;
			if (req->channel_count) {
				int i;
				int buf_size = sizeof(*vdev->ocb_channel_info) *
				    req->channel_count;
				vdev->ocb_channel_info =
					vos_mem_malloc(buf_size);
				if (!vdev->ocb_channel_info)
					return -ENOMEM;
				vos_mem_zero(vdev->ocb_channel_info, buf_size);
				for (i = 0; i < req->channel_count; i++) {
					vdev->ocb_channel_info[i].chan_freq =
						req->channels[i].chan_freq;
					if (req->channels[i].flags &
					  OCB_CHANNEL_FLAG_DISABLE_RX_STATS_HDR)
						vdev->ocb_channel_info[i].
						disable_rx_stats_hdr = 1;
				}
			} else {
				vdev->ocb_channel_info = 0;
			}
		}
	}

	/* Free the configuration that was saved in wma_ocb_set_config. */
	vos_mem_free(wma_handle->ocb_config_req);
	wma_handle->ocb_config_req = 0;

	resp = vos_mem_malloc(sizeof(*resp));
	if (!resp)
		return -ENOMEM;

	resp->status = status;

	msg.type = eWNI_SME_OCB_SET_CONFIG_RSP;
	msg.bodyptr = resp;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGE(FL("Fail to post msg to SME"));
		vos_mem_free(resp);
		return -EINVAL;
	}

	return 0;
}

/**
 * copy_sir_ocb_config() - deep copy of an OCB config struct
 * @src: pointer to the source struct
 *
 * Return: pointer to the copied struct
 */
static struct sir_ocb_config *copy_sir_ocb_config(struct sir_ocb_config *src)
{
	struct sir_ocb_config *dst;
	uint32_t length;
	void *cursor;

	length = sizeof(*src) +
		src->channel_count * sizeof(*src->channels) +
		src->schedule_size * sizeof(*src->schedule) +
		src->dcc_ndl_chan_list_len +
		src->dcc_ndl_active_state_list_len;

	dst = vos_mem_malloc(length);
	if (!dst)
		return NULL;

	*dst = *src;

	cursor = dst;
	cursor += sizeof(*dst);
	dst->channels = cursor;
	cursor += src->channel_count * sizeof(*dst->channels);
	vos_mem_copy(dst->channels, src->channels,
		     src->channel_count * sizeof(*dst->channels));
	dst->schedule = cursor;
	cursor += src->schedule_size * sizeof(*dst->schedule);
	vos_mem_copy(dst->schedule, src->schedule,
		     src->schedule_size * sizeof(*dst->schedule));
	dst->dcc_ndl_chan_list = cursor;
	cursor += src->dcc_ndl_chan_list_len;
	vos_mem_copy(dst->dcc_ndl_chan_list, src->dcc_ndl_chan_list,
		     src->dcc_ndl_chan_list_len);
	dst->dcc_ndl_active_state_list = cursor;
	cursor += src->dcc_ndl_active_state_list_len;
	vos_mem_copy(dst->dcc_ndl_active_state_list,
		     src->dcc_ndl_active_state_list,
		     src->dcc_ndl_active_state_list_len);
	return dst;
}

/**
 * wma_ocb_set_config_req() - send the OCB config request
 * @wma_handle: pointer to the WMA handle
 * @config_req: the configuration to be set.
 */
int wma_ocb_set_config_req(tp_wma_handle wma_handle,
			   struct sir_ocb_config *config_req)
{
	struct wma_target_req *msg;
	struct wma_vdev_start_req req;
	VOS_STATUS status = VOS_STATUS_SUCCESS;

	/* if vdev is not yet up, send vdev start request and wait for response.
	 * OCB set_config request should be sent on receiving
	 * vdev start response message
	 */
	if (!wma_handle->interfaces[config_req->session_id].vdev_up) {
		vos_mem_zero(&req, sizeof(req));
		/* Enqueue OCB Set Schedule request message */
		msg = wma_fill_vdev_req(wma_handle, config_req->session_id,
					WDA_OCB_SET_CONFIG_CMD,
					WMA_TARGET_REQ_TYPE_VDEV_START,
					(void *)config_req, 1000);
		if (!msg) {
			WMA_LOGE(FL("Failed to fill vdev req %d"), req.vdev_id);
			status = VOS_STATUS_E_NOMEM;
			return status;
		}
		req.chan = vos_freq_to_chan(config_req->channels[0].chan_freq);
		req.vdev_id = msg->vdev_id;
		if (vos_chan_to_band(req.chan) == VOS_BAND_2GHZ)
		    req.dot11_mode = WNI_CFG_DOT11_MODE_11G;
		else
		    req.dot11_mode = WNI_CFG_DOT11_MODE_11A;

		if (wma_handle->ocb_config_req)
			vos_mem_free(wma_handle->ocb_config_req);
		wma_handle->ocb_config_req = copy_sir_ocb_config(config_req);

		status = wma_vdev_start(wma_handle, &req, VOS_FALSE);
		if (status != VOS_STATUS_SUCCESS) {
			wma_remove_vdev_req(wma_handle, req.vdev_id,
					    WMA_TARGET_REQ_TYPE_VDEV_START);
			WMA_LOGE(FL("vdev_start failed, status = %d"), status);
		}
		return 0;
	} else {
		return wma_ocb_set_config(wma_handle, config_req);
	}
}

int wma_ocb_start_resp_ind_cont(tp_wma_handle wma_handle)
{
	VOS_STATUS vos_status = 0;

	if (!wma_handle->ocb_config_req) {
		WMA_LOGE(FL("The request could not be found"));
		return VOS_STATUS_E_EMPTY;
	}

	vos_status = wma_ocb_set_config(wma_handle, wma_handle->ocb_config_req);
	return vos_status;
}

static WLAN_PHY_MODE wma_ocb_freq_to_mode(uint32_t freq)
{
	if (vos_chan_to_band(vos_freq_to_chan(freq)) == VOS_BAND_2GHZ)
		return MODE_11G;
	else
		return MODE_11A;
}

/**
 * wma_send_ocb_set_config() - send the OCB config to the FW
 * @wma_handle: pointer to the WMA handle
 * @config: the OCB configuration
 *
 * Return: 0 on success
 */
int wma_ocb_set_config(tp_wma_handle wma_handle, struct sir_ocb_config *config)
{
	int32_t ret;
	wmi_ocb_set_config_cmd_fixed_param *cmd;
	wmi_channel *chan;
	wmi_ocb_channel *ocb_chan;
	wmi_qos_parameter *qos_param;
	wmi_dcc_ndl_chan *ndl_chan;
	wmi_dcc_ndl_active_state_config *ndl_active_config;
	wmi_ocb_schedule_element *sched_elem;
	uint8_t *buf_ptr;
	wmi_buf_t buf;
	int32_t len;
	int32_t i, j, active_state_count;

	/*
	 * Validate the dcc_ndl_chan_list_len and count the number of active
	 * states. Validate dcc_ndl_active_state_list_len.
	 */
	active_state_count = 0;
	if (config->dcc_ndl_chan_list_len) {
		if (!config->dcc_ndl_chan_list ||
			config->dcc_ndl_chan_list_len !=
			config->channel_count * sizeof(wmi_dcc_ndl_chan)) {
			WMA_LOGE(FL("NDL channel is invalid. List len: %d"),
				 config->dcc_ndl_chan_list_len);
			return -EINVAL;
		}

		for (i = 0, ndl_chan = config->dcc_ndl_chan_list;
				i < config->channel_count; ++i, ++ndl_chan)
			active_state_count +=
				WMI_NDL_NUM_ACTIVE_STATE_GET(ndl_chan);

		if (active_state_count) {
			if (!config->dcc_ndl_active_state_list ||
				config->dcc_ndl_active_state_list_len !=
				active_state_count *
				sizeof(wmi_dcc_ndl_active_state_config)) {
				WMA_LOGE(FL("NDL active state is invalid."));
				return -EINVAL;
			}
		}
	}

	len = sizeof(*cmd) +
		WMI_TLV_HDR_SIZE + config->channel_count *
			sizeof(wmi_channel) +
		WMI_TLV_HDR_SIZE + config->channel_count *
			sizeof(wmi_ocb_channel) +
		WMI_TLV_HDR_SIZE + config->channel_count *
			sizeof(wmi_qos_parameter) * WLAN_MAX_AC +
		WMI_TLV_HDR_SIZE + config->dcc_ndl_chan_list_len +
		WMI_TLV_HDR_SIZE + active_state_count *
			sizeof(wmi_dcc_ndl_active_state_config) +
		WMI_TLV_HDR_SIZE + config->schedule_size *
			sizeof(wmi_ocb_schedule_element);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_ocb_set_config_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_ocb_set_config_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_ocb_set_config_cmd_fixed_param));
	cmd->vdev_id = config->session_id;
	cmd->channel_count = config->channel_count;
	cmd->schedule_size = config->schedule_size;
	cmd->flags = config->flags;
	buf_ptr += sizeof(*cmd);

	/* Add the wmi_channel info */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       config->channel_count*sizeof(wmi_channel));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for (i = 0; i < config->channel_count; i++) {
		chan = (wmi_channel *)buf_ptr;
		WMITLV_SET_HDR(&chan->tlv_header,
				WMITLV_TAG_STRUC_wmi_channel,
				WMITLV_GET_STRUCT_TLVLEN(wmi_channel));

		chan->mhz = config->channels[i].chan_freq;
		chan->band_center_freq1 = config->channels[i].chan_freq;
		chan->band_center_freq2 = 0;
		chan->info = 0;

		WMI_SET_CHANNEL_MODE(chan, wma_ocb_freq_to_mode(chan->mhz));
		WMI_SET_CHANNEL_MAX_POWER(chan, config->channels[i].max_pwr);
		WMI_SET_CHANNEL_MIN_POWER(chan, config->channels[i].min_pwr);
		WMI_SET_CHANNEL_MAX_TX_POWER(chan, config->channels[i].max_pwr);
		WMI_SET_CHANNEL_REG_POWER(chan, config->channels[i].reg_pwr);
		WMI_SET_CHANNEL_ANTENNA_MAX(chan,
					    config->channels[i].antenna_max);

		if (config->channels[i].bandwidth < 10)
			WMI_SET_CHANNEL_FLAG(chan, WMI_CHAN_FLAG_QUARTER_RATE);
		else if (config->channels[i].bandwidth < 20)
			WMI_SET_CHANNEL_FLAG(chan, WMI_CHAN_FLAG_HALF_RATE);
		buf_ptr += sizeof(*chan);
	}

	/* Add the wmi_ocb_channel info */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       config->channel_count*sizeof(wmi_ocb_channel));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for (i = 0; i < config->channel_count; i++) {
		ocb_chan = (wmi_ocb_channel *)buf_ptr;
		WMITLV_SET_HDR(&ocb_chan->tlv_header,
			       WMITLV_TAG_STRUC_wmi_ocb_channel,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_ocb_channel));
		ocb_chan->bandwidth = config->channels[i].bandwidth;
		WMI_CHAR_ARRAY_TO_MAC_ADDR(config->channels[i].mac_address,
					   &ocb_chan->mac_address);
		buf_ptr += sizeof(*ocb_chan);
	}

	/* Add the wmi_qos_parameter info */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		config->channel_count * sizeof(wmi_qos_parameter)*WLAN_MAX_AC);
	buf_ptr += WMI_TLV_HDR_SIZE;
	/* WLAN_MAX_AC parameters for each channel */
	for (i = 0; i < config->channel_count; i++) {
		for (j = 0; j < WLAN_MAX_AC; j++) {
			qos_param = (wmi_qos_parameter *)buf_ptr;
			WMITLV_SET_HDR(&qos_param->tlv_header,
				WMITLV_TAG_STRUC_wmi_qos_parameter,
				WMITLV_GET_STRUCT_TLVLEN(wmi_qos_parameter));
			qos_param->aifsn =
				config->channels[i].qos_params[j].aifsn;
			qos_param->cwmin =
				config->channels[i].qos_params[j].cwmin;
			qos_param->cwmax =
				config->channels[i].qos_params[j].cwmax;
			buf_ptr += sizeof(*qos_param);
		}
	}

	/* Add the wmi_dcc_ndl_chan (per channel) */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       config->dcc_ndl_chan_list_len);
	buf_ptr += WMI_TLV_HDR_SIZE;
	if (config->dcc_ndl_chan_list_len) {
		ndl_chan = (wmi_dcc_ndl_chan *)buf_ptr;
		vos_mem_copy(ndl_chan, config->dcc_ndl_chan_list,
			     config->dcc_ndl_chan_list_len);
		for (i = 0; i < config->channel_count; i++)
			WMITLV_SET_HDR(&(ndl_chan[i].tlv_header),
				WMITLV_TAG_STRUC_wmi_dcc_ndl_chan,
				WMITLV_GET_STRUCT_TLVLEN(wmi_dcc_ndl_chan));
		buf_ptr += config->dcc_ndl_chan_list_len;
	}

	/* Add the wmi_dcc_ndl_active_state_config */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, active_state_count *
		       sizeof(wmi_dcc_ndl_active_state_config));
	buf_ptr += WMI_TLV_HDR_SIZE;
	if (active_state_count) {
		ndl_active_config = (wmi_dcc_ndl_active_state_config *)buf_ptr;
		vos_mem_copy(ndl_active_config,
			config->dcc_ndl_active_state_list,
			active_state_count * sizeof(*ndl_active_config));
		for (i = 0; i < active_state_count; ++i)
			WMITLV_SET_HDR(&(ndl_active_config[i].tlv_header),
			  WMITLV_TAG_STRUC_wmi_dcc_ndl_active_state_config,
			  WMITLV_GET_STRUCT_TLVLEN(
				wmi_dcc_ndl_active_state_config));
		buf_ptr += active_state_count *
			sizeof(*ndl_active_config);
	}

	/* Add the wmi_ocb_schedule_element info */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		config->schedule_size * sizeof(wmi_ocb_schedule_element));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for (i = 0; i < config->schedule_size; i++) {
		sched_elem = (wmi_ocb_schedule_element *)buf_ptr;
		WMITLV_SET_HDR(&sched_elem->tlv_header,
			WMITLV_TAG_STRUC_wmi_ocb_schedule_element,
			WMITLV_GET_STRUCT_TLVLEN(wmi_ocb_schedule_element));
		sched_elem->channel_freq = config->schedule[i].chan_freq;
		sched_elem->total_duration = config->schedule[i].total_duration;
		sched_elem->guard_interval = config->schedule[i].guard_interval;
		buf_ptr += sizeof(*sched_elem);
	}

	/*
	 * Save the configuration so that it can be used in
	 * wma_ocb_set_config_event_handler.
	 */
	if (wma_handle->ocb_config_req != config) {
		if (wma_handle->ocb_config_req)
			vos_mem_free(wma_handle->ocb_config_req);
		wma_handle->ocb_config_req = copy_sir_ocb_config(config);
	}

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_OCB_SET_CONFIG_CMDID);
	if (ret != EOK) {
		if (wma_handle->ocb_config_req) {
			vos_mem_free(wma_handle->ocb_config_req);
			wma_handle->ocb_config_req = 0;
		}

		WMA_LOGE("Failed to set OCB config");
		wmi_buf_free(buf);
		return -EIO;
	}
	return 0;
}

/**
 * wma_ocb_set_config_event_handler() - Response event for the set config cmd
 * @handle: the WMA handle
 * @event_buf: buffer with the event parameters
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
int wma_ocb_set_config_event_handler(void *handle, uint8_t *event_buf,
				     uint32_t len)
{
	WMI_OCB_SET_CONFIG_RESP_EVENTID_param_tlvs *param_tlvs;
	wmi_ocb_set_config_resp_event_fixed_param *fix_param;
	param_tlvs = (WMI_OCB_SET_CONFIG_RESP_EVENTID_param_tlvs *)event_buf;
	fix_param = param_tlvs->fixed_param;
	return wma_ocb_set_config_resp(handle, fix_param->status);
};

/**
 * wma_ocb_set_utc_time() - send the UTC time to the firmware
 * @wma_handle: pointer to the WMA handle
 * @utc: pointer to the UTC time struct
 *
 * Return: 0 on succes
 */
int wma_ocb_set_utc_time(tp_wma_handle wma_handle, struct sir_ocb_utc *utc)
{
	int32_t ret;
	wmi_ocb_set_utc_time_cmd_fixed_param *cmd;
	uint8_t *buf_ptr;
	uint32_t len, i;
	wmi_buf_t buf;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_ocb_set_utc_time_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_ocb_set_utc_time_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_ocb_set_utc_time_cmd_fixed_param));
	cmd->vdev_id = utc->vdev_id;

	for (i = 0; i < SIZE_UTC_TIME; i++)
		WMI_UTC_TIME_SET(cmd, i, utc->utc_time[i]);

	for (i = 0; i < SIZE_UTC_TIME_ERROR; i++)
		WMI_TIME_ERROR_SET(cmd, i, utc->time_error[i]);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_OCB_SET_UTC_TIME_CMDID);
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to set OCB UTC time"));
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_ocb_start_timing_advert() - start sending the timing advertisement
 *				   frames on a channel
 * @wma_handle: pointer to the WMA handle
 * @timing_advert: pointer to the timing advertisement struct
 *
 * Return: 0 on succes
 */
int wma_ocb_start_timing_advert(tp_wma_handle wma_handle,
	struct sir_ocb_timing_advert *timing_advert)
{
	int32_t ret;
	wmi_ocb_start_timing_advert_cmd_fixed_param *cmd;
	uint8_t *buf_ptr;
	uint32_t len, len_template;
	wmi_buf_t buf;

	len = sizeof(*cmd) +
		     WMI_TLV_HDR_SIZE;

	len_template = timing_advert->template_length;
	/* Add padding to the template if needed */
	if (len_template % 4 != 0)
		len_template += 4 - (len_template % 4);
	len += len_template;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_ocb_start_timing_advert_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_ocb_start_timing_advert_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_ocb_start_timing_advert_cmd_fixed_param));
	cmd->vdev_id = timing_advert->vdev_id;
	cmd->repeat_rate = timing_advert->repeat_rate;
	cmd->channel_freq = timing_advert->chan_freq;
	cmd->timestamp_offset = timing_advert->timestamp_offset;
	cmd->time_value_offset = timing_advert->time_value_offset;
	cmd->timing_advert_template_length = timing_advert->template_length;
	buf_ptr += sizeof(*cmd);

	/* Add the timing advert template */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       len_template);
	vos_mem_copy(buf_ptr + WMI_TLV_HDR_SIZE,
		     (uint8_t *)timing_advert->template_value,
		     timing_advert->template_length);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_OCB_START_TIMING_ADVERT_CMDID);
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to start OCB timing advert"));
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_ocb_stop_timing_advert() - stop sending the timing advertisement frames
 *				  on a channel
 * @wma_handle: pointer to the WMA handle
 * @timing_advert: pointer to the timing advertisement struct
 *
 * Return: 0 on succes
 */
int wma_ocb_stop_timing_advert(tp_wma_handle wma_handle,
	struct sir_ocb_timing_advert *timing_advert)
{
	int32_t ret;
	wmi_ocb_stop_timing_advert_cmd_fixed_param *cmd;
	uint8_t *buf_ptr;
	uint32_t len;
	wmi_buf_t buf;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_ocb_stop_timing_advert_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_ocb_stop_timing_advert_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_ocb_stop_timing_advert_cmd_fixed_param));
	cmd->vdev_id = timing_advert->vdev_id;
	cmd->channel_freq = timing_advert->chan_freq;

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_OCB_STOP_TIMING_ADVERT_CMDID);
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to stop OCB timing advert"));
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_ocb_get_tsf_timer() - stop sending the timing advertisement frames on a
 *			     channel
 * @wma_handle: pointer to the WMA handle
 * @request: pointer to the request
 *
 * Return: 0 on succes
 */
int wma_ocb_get_tsf_timer(tp_wma_handle wma_handle,
			  struct sir_ocb_get_tsf_timer *request)
{
	VOS_STATUS ret;
	wmi_ocb_get_tsf_timer_cmd_fixed_param *cmd;
	uint8_t *buf_ptr;
	wmi_buf_t buf;
	int32_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}
	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	cmd = (wmi_ocb_get_tsf_timer_cmd_fixed_param *)buf_ptr;
	vos_mem_zero(cmd, len);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_ocb_get_tsf_timer_cmd_fixed_param));
	cmd->vdev_id = request->vdev_id;

	/* Send the WMI command */
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_OCB_GET_TSF_TIMER_CMDID);
	/* If there is an error, set the completion event */
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to send WMI message: %d"), ret);
		wmi_buf_free(buf);
		return -EIO;
	}
	return 0;
}

/**
 * wma_ocb_get_tsf_timer_resp_event_handler() - Event for the get TSF timer cmd
 * @handle: the WMA handle
 * @event_buf: buffer with the event parameters
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
int wma_ocb_get_tsf_timer_resp_event_handler(void *handle, uint8_t *event_buf,
					     uint32_t len)
{
	VOS_STATUS vos_status;
	struct sir_ocb_get_tsf_timer_response *response;
	WMI_OCB_GET_TSF_TIMER_RESP_EVENTID_param_tlvs *param_tlvs;
	wmi_ocb_get_tsf_timer_resp_event_fixed_param *fix_param;
	vos_msg_t msg = {0};

	param_tlvs = (WMI_OCB_GET_TSF_TIMER_RESP_EVENTID_param_tlvs *)event_buf;
	fix_param = param_tlvs->fixed_param;

	/* Allocate and populate the response */
	response = vos_mem_malloc(sizeof(*response));
	if (response == NULL)
		return -ENOMEM;
	response->vdev_id = fix_param->vdev_id;
	response->timer_high = fix_param->tsf_timer_high;
	response->timer_low = fix_param->tsf_timer_low;

	msg.type = eWNI_SME_OCB_GET_TSF_TIMER_RSP;
	msg.bodyptr = response;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGE(FL("Failed to post msg to SME"));
		vos_mem_free(response);
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_dcc_get_stats() - get the DCC channel stats
 * @wma_handle: pointer to the WMA handle
 * @get_stats_param: pointer to the dcc stats
 *
 * Return: 0 on succes
 */
int wma_dcc_get_stats(tp_wma_handle wma_handle,
		      struct sir_dcc_get_stats *get_stats_param)
{
	int32_t ret;
	wmi_dcc_get_stats_cmd_fixed_param *cmd;
	wmi_dcc_channel_stats_request *channel_stats_array;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len;
	uint32_t i;

	/* Validate the input */
	if (get_stats_param->request_array_len !=
	    get_stats_param->channel_count * sizeof(*channel_stats_array)) {
		WMA_LOGE(FL("Invalid parameter"));
		return -EINVAL;
	}

	/* Allocate memory for the WMI command */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE +
		get_stats_param->request_array_len;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);

	/* Populate the WMI command */
	cmd = (wmi_dcc_get_stats_cmd_fixed_param *)buf_ptr;
	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_dcc_get_stats_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			   wmi_dcc_get_stats_cmd_fixed_param));
	cmd->vdev_id = get_stats_param->vdev_id;
	cmd->num_channels = get_stats_param->channel_count;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       get_stats_param->request_array_len);
	buf_ptr += WMI_TLV_HDR_SIZE;

	channel_stats_array = (wmi_dcc_channel_stats_request *)buf_ptr;
	vos_mem_copy(channel_stats_array, get_stats_param->request_array,
		     get_stats_param->request_array_len);
	for (i = 0; i < cmd->num_channels; i++)
		WMITLV_SET_HDR(&channel_stats_array[i].tlv_header,
			WMITLV_TAG_STRUC_wmi_dcc_channel_stats_request,
			WMITLV_GET_STRUCT_TLVLEN(
			    wmi_dcc_channel_stats_request));

	/* Send the WMI command */
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_DCC_GET_STATS_CMDID);

	if (ret != EOK) {
		WMA_LOGE(FL("Failed to send WMI message: %d"), ret);
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_dcc_get_stats_resp_event_handler() - Response event for the get stats cmd
 * @handle: the WMA handle
 * @event_buf: buffer with the event parameters
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
int wma_dcc_get_stats_resp_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	VOS_STATUS vos_status;
	struct sir_dcc_get_stats_response *response;
	WMI_DCC_GET_STATS_RESP_EVENTID_param_tlvs *param_tlvs;
	wmi_dcc_get_stats_resp_event_fixed_param *fix_param;
	vos_msg_t msg = {0};

	param_tlvs = (WMI_DCC_GET_STATS_RESP_EVENTID_param_tlvs *)event_buf;
	fix_param = param_tlvs->fixed_param;

	/* Allocate and populate the response */
	response = vos_mem_malloc(sizeof(*response) + fix_param->num_channels *
		sizeof(wmi_dcc_ndl_stats_per_channel));
	if (response == NULL)
		return -ENOMEM;

	response->vdev_id = fix_param->vdev_id;
	response->num_channels = fix_param->num_channels;
	response->channel_stats_array_len =
		fix_param->num_channels * sizeof(wmi_dcc_ndl_stats_per_channel);
	response->channel_stats_array = ((void *)response) + sizeof(*response);
	vos_mem_copy(response->channel_stats_array,
		     param_tlvs->stats_per_channel_list,
		     response->channel_stats_array_len);

	msg.type = eWNI_SME_DCC_GET_STATS_RSP;
	msg.bodyptr = response;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGE(FL("Failed to post msg to SME"));
		vos_mem_free(response);
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_dcc_clear_stats() - command to clear the DCC stats
 * @wma_handle: pointer to the WMA handle
 * @clear_stats_param: parameters to the command
 *
 * Return: 0 on succes
 */
int wma_dcc_clear_stats(tp_wma_handle wma_handle,
			struct sir_dcc_clear_stats *clear_stats_param)
{
	int32_t ret;
	wmi_dcc_clear_stats_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len;

	/* Allocate memory for the WMI command */
	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);

	/* Populate the WMI command */
	cmd = (wmi_dcc_clear_stats_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_dcc_clear_stats_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			   wmi_dcc_clear_stats_cmd_fixed_param));
	cmd->vdev_id = clear_stats_param->vdev_id;
	cmd->dcc_stats_bitmap = clear_stats_param->dcc_stats_bitmap;

	/* Send the WMI command */
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_DCC_CLEAR_STATS_CMDID);
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to send the WMI command"));
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_dcc_update_ndl() - command to update the NDL data
 * @wma_handle: pointer to the WMA handle
 * @update_ndl_param: pointer to the request parameters
 *
 * Return: 0 on success
 */
int wma_dcc_update_ndl(tp_wma_handle wma_handle,
		       struct sir_dcc_update_ndl *update_ndl_param)
{
	VOS_STATUS vos_status;
	wmi_dcc_update_ndl_cmd_fixed_param *cmd;
	wmi_dcc_ndl_chan *ndl_chan_array;
	wmi_dcc_ndl_active_state_config *ndl_active_state_array;
	uint32_t active_state_count;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len;
	uint32_t i;

	/* validate the input */
	if (update_ndl_param->dcc_ndl_chan_list_len !=
	    update_ndl_param->channel_count * sizeof(*ndl_chan_array)) {
		WMA_LOGE(FL("Invalid parameter"));
		return VOS_STATUS_E_INVAL;
	}
	active_state_count = 0;
	ndl_chan_array = update_ndl_param->dcc_ndl_chan_list;
	for (i = 0; i < update_ndl_param->channel_count; i++)
		active_state_count +=
			WMI_NDL_NUM_ACTIVE_STATE_GET(&ndl_chan_array[i]);
	if (update_ndl_param->dcc_ndl_active_state_list_len !=
	    active_state_count * sizeof(*ndl_active_state_array)) {
		WMA_LOGE(FL("Invalid parameter"));
		return VOS_STATUS_E_INVAL;
	}

	/* Allocate memory for the WMI command */
	len = sizeof(*cmd) +
		WMI_TLV_HDR_SIZE + update_ndl_param->dcc_ndl_chan_list_len +
		WMI_TLV_HDR_SIZE +
		update_ndl_param->dcc_ndl_active_state_list_len;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);

	/* Populate the WMI command */
	cmd = (wmi_dcc_update_ndl_cmd_fixed_param *)buf_ptr;
	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_dcc_update_ndl_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			   wmi_dcc_update_ndl_cmd_fixed_param));
	cmd->vdev_id = update_ndl_param->vdev_id;
	cmd->num_channel = update_ndl_param->channel_count;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       update_ndl_param->dcc_ndl_chan_list_len);
	buf_ptr += WMI_TLV_HDR_SIZE;

	ndl_chan_array = (wmi_dcc_ndl_chan *)buf_ptr;
	vos_mem_copy(ndl_chan_array, update_ndl_param->dcc_ndl_chan_list,
		     update_ndl_param->dcc_ndl_chan_list_len);
	for (i = 0; i < cmd->num_channel; i++)
		WMITLV_SET_HDR(&ndl_chan_array[i].tlv_header,
			WMITLV_TAG_STRUC_wmi_dcc_ndl_chan,
			WMITLV_GET_STRUCT_TLVLEN(
			    wmi_dcc_ndl_chan));
	buf_ptr += update_ndl_param->dcc_ndl_chan_list_len;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       update_ndl_param->dcc_ndl_active_state_list_len);
	buf_ptr += WMI_TLV_HDR_SIZE;

	ndl_active_state_array = (wmi_dcc_ndl_active_state_config *) buf_ptr;
	vos_mem_copy(ndl_active_state_array,
		     update_ndl_param->dcc_ndl_active_state_list,
		     update_ndl_param->dcc_ndl_active_state_list_len);
	for (i = 0; i < active_state_count; i++) {
		WMITLV_SET_HDR(&ndl_active_state_array[i].tlv_header,
			WMITLV_TAG_STRUC_wmi_dcc_ndl_active_state_config,
			WMITLV_GET_STRUCT_TLVLEN(
			    wmi_dcc_ndl_active_state_config));
	}
	buf_ptr += update_ndl_param->dcc_ndl_active_state_list_len;

	/* Send the WMI command */
	vos_status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_DCC_UPDATE_NDL_CMDID);
	/* If there is an error, set the completion event */
	if (vos_status) {
		WMA_LOGE(FL("Failed to send WMI message: %d"), vos_status);
		wmi_buf_free(buf);
		return -EIO;
	}

	return 0;
}

/**
 * wma_dcc_update_ndl_resp_event_handler() - Response event for the update NDL
 * command
 * @handle: the WMA handle
 * @event_buf: buffer with the event parameters
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
int wma_dcc_update_ndl_resp_event_handler(void *handle, uint8_t *event_buf,
					  uint32_t len)
{
	VOS_STATUS vos_status;
	struct sir_dcc_update_ndl_response *resp;
	WMI_DCC_UPDATE_NDL_RESP_EVENTID_param_tlvs *param_tlvs;
	wmi_dcc_update_ndl_resp_event_fixed_param *fix_param;
	vos_msg_t msg = {0};

	param_tlvs = (WMI_DCC_UPDATE_NDL_RESP_EVENTID_param_tlvs *)event_buf;
	fix_param = param_tlvs->fixed_param;
	/* Allocate and populate the response */
	resp = vos_mem_malloc(sizeof(*resp));
	if (!resp) {
		WMA_LOGE(FL("Error allocating memory for the response."));
		return -ENOMEM;
	}
	resp->vdev_id = fix_param->vdev_id;
	resp->status = fix_param->status;

	msg.type = eWNI_SME_DCC_UPDATE_NDL_RSP;
	msg.bodyptr = resp;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status))	{
		WMA_LOGE(FL("Failed to post msg to SME"));
		vos_mem_free(resp);
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_dcc_stats_event_handler() - Response event for the get stats cmd
 * @handle: the WMA handle
 * @event_buf: buffer with the event parameters
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
int wma_dcc_stats_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	VOS_STATUS vos_status;
	struct sir_dcc_get_stats_response *response;
	WMI_DCC_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_dcc_stats_event_fixed_param *fix_param;
	vos_msg_t msg = {0};

	param_tlvs = (WMI_DCC_STATS_EVENTID_param_tlvs *)event_buf;
	fix_param = param_tlvs->fixed_param;
	/* Allocate and populate the response */
	response = vos_mem_malloc(sizeof(*response) +
	    fix_param->num_channels * sizeof(wmi_dcc_ndl_stats_per_channel));
	if (response == NULL)
		return -ENOMEM;
	response->vdev_id = fix_param->vdev_id;
	response->num_channels = fix_param->num_channels;
	response->channel_stats_array_len =
		fix_param->num_channels * sizeof(wmi_dcc_ndl_stats_per_channel);
	response->channel_stats_array = ((void *)response) + sizeof(*response);
	vos_mem_copy(response->channel_stats_array,
		     param_tlvs->stats_per_channel_list,
		     response->channel_stats_array_len);

	msg.type = eWNI_SME_DCC_STATS_EVENT;
	msg.bodyptr = response;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status))	{
		WMA_LOGE(FL("Failed to post msg to SME"));
		vos_mem_free(response);
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_ocb_register_event_handlers() - register handlers for the OCB WMI
 * events
 * @wma_handle: pointer to the WMA handle
 *
 * Return: 0 on success, non-zero on failure
 */
int wma_ocb_register_event_handlers(tp_wma_handle wma_handle)
{
	int status;

	if (!wma_handle) {
		WMA_LOGE(FL("wma_handle is NULL"));
		return -EINVAL;
	}

	/* Initialize the members in WMA used by wma_ocb */
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_OCB_SET_CONFIG_RESP_EVENTID,
			wma_ocb_set_config_event_handler);
	if (status)
		return status;

	status = wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			WMI_OCB_GET_TSF_TIMER_RESP_EVENTID,
			wma_ocb_get_tsf_timer_resp_event_handler);
	if (status)
		return status;

	status = wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			WMI_DCC_GET_STATS_RESP_EVENTID,
			wma_dcc_get_stats_resp_event_handler);
	if (status)
		return status;

	status = wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			WMI_DCC_UPDATE_NDL_RESP_EVENTID,
			wma_dcc_update_ndl_resp_event_handler);
	if (status)
		return status;

	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_DCC_STATS_EVENTID,
			wma_dcc_stats_event_handler);
	if (status)
		return status;

	return VOS_STATUS_SUCCESS;
}
