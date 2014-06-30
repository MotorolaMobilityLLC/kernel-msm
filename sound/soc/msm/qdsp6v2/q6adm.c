/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/wait.h>

#include <sound/apr_audio-v2.h>
#include <linux/qdsp6v2/apr.h>
#include <sound/q6adm-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6afe-v2.h>

#include "audio_acdb.h"

#define TIMEOUT_MS 1000

#define RESET_COPP_ID 99
#define INVALID_COPP_ID 0xFF
/* Used for inband payload copy, max size is 4k */
/* 2 is to account for module & param ID in payload */
#define ADM_GET_PARAMETER_LENGTH  (4096 - APR_HDR_SIZE - 2 * sizeof(uint32_t))

#define ULL_SUPPORTED_SAMPLE_RATE 48000


struct adm_ctl {
	void *apr;
	atomic_t copp_id[AFE_MAX_PORTS];
	atomic_t copp_cnt[AFE_MAX_PORTS];
	atomic_t copp_low_latency_id[AFE_MAX_PORTS];
	atomic_t copp_low_latency_cnt[AFE_MAX_PORTS];
	atomic_t copp_perf_mode[AFE_MAX_PORTS];
	atomic_t copp_stat[AFE_MAX_PORTS];
	wait_queue_head_t wait[AFE_MAX_PORTS];

	struct acdb_cal_block mem_addr_audproc[MAX_AUDPROC_TYPES];
	struct acdb_cal_block mem_addr_audvol[MAX_AUDPROC_TYPES];

	atomic_t mem_map_cal_handles[ADM_MAX_CAL_TYPES];
	atomic_t mem_map_cal_index;

	int set_custom_topology;
	int ec_ref_rx;

	wait_queue_head_t adm_delay_wait[AFE_MAX_PORTS];
	atomic_t adm_delay_stat[AFE_MAX_PORTS];
	uint32_t adm_delay[AFE_MAX_PORTS];
	int32_t topology[AFE_MAX_PORTS];
};

static struct adm_ctl			this_adm;

struct adm_multi_ch_map {
	bool set_channel_map;
	char channel_mapping[PCM_FORMAT_MAX_NUM_CHANNEL];
};

static struct adm_multi_ch_map multi_ch_map = { false,
						{0, 0, 0, 0, 0, 0, 0, 0}
					      };

static int adm_get_parameters[ADM_GET_PARAMETER_LENGTH];
static int adm_module_topo_list[ADM_GET_TOPO_MODULE_LIST_LENGTH];

int srs_trumedia_open(int port_id, int srs_tech_id, void *srs_params)
{
	struct adm_cmd_set_pp_params_inband_v5 *adm_params = NULL;
	int ret = 0, sz = 0;
	int index;

	pr_debug("SRS - %s", __func__);
	switch (srs_tech_id) {
	case SRS_ID_GLOBAL: {
		struct srs_trumedia_params_GLOBAL *glb_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_GLOBAL);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_GLOBAL) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_GLOBAL);
		glb_params = (struct srs_trumedia_params_GLOBAL *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(glb_params, srs_params,
			sizeof(struct srs_trumedia_params_GLOBAL));
		pr_debug("SRS - %s: Global params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x\n",
				__func__, (int)glb_params->v1,
				(int)glb_params->v2, (int)glb_params->v3,
				(int)glb_params->v4, (int)glb_params->v5,
				(int)glb_params->v6, (int)glb_params->v7,
				(int)glb_params->v8);
		break;
	}
	case SRS_ID_WOWHD: {
		struct srs_trumedia_params_WOWHD *whd_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_WOWHD);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_WOWHD) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_WOWHD;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_WOWHD);
		whd_params = (struct srs_trumedia_params_WOWHD *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(whd_params, srs_params,
				sizeof(struct srs_trumedia_params_WOWHD));
		pr_debug("SRS - %s: WOWHD params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x, 9 = %x, 10 = %x, 11 = %x\n",
			 __func__, (int)whd_params->v1,
			(int)whd_params->v2, (int)whd_params->v3,
			(int)whd_params->v4, (int)whd_params->v5,
			(int)whd_params->v6, (int)whd_params->v7,
			(int)whd_params->v8, (int)whd_params->v9,
			(int)whd_params->v10, (int)whd_params->v11);
		break;
	}
	case SRS_ID_CSHP: {
		struct srs_trumedia_params_CSHP *chp_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_CSHP);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_CSHP) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_CSHP;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_CSHP);
		chp_params = (struct srs_trumedia_params_CSHP *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(chp_params, srs_params,
				sizeof(struct srs_trumedia_params_CSHP));
		pr_debug("SRS - %s: CSHP params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x, 8 = %x, 9 = %x\n",
				__func__, (int)chp_params->v1,
				(int)chp_params->v2, (int)chp_params->v3,
				(int)chp_params->v4, (int)chp_params->v5,
				(int)chp_params->v6, (int)chp_params->v7,
				(int)chp_params->v8, (int)chp_params->v9);
		break;
	}
	case SRS_ID_HPF: {
		struct srs_trumedia_params_HPF *hpf_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_HPF);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_HPF) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_HPF;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_HPF);
		hpf_params = (struct srs_trumedia_params_HPF *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(hpf_params, srs_params,
			sizeof(struct srs_trumedia_params_HPF));
		pr_debug("SRS - %s: HPF params - 1 = %x\n", __func__,
				(int)hpf_params->v1);
		break;
	}
	case SRS_ID_PEQ: {
		struct srs_trumedia_params_PEQ *peq_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_PEQ);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
				sizeof(struct srs_trumedia_params_PEQ) +
				sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_PEQ;
		adm_params->params.param_size =
				sizeof(struct srs_trumedia_params_PEQ);
		peq_params = (struct srs_trumedia_params_PEQ *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(peq_params, srs_params,
				sizeof(struct srs_trumedia_params_PEQ));
		pr_debug("SRS - %s: PEQ params - 1 = %x 2 = %x, 3 = %x, 4 = %x\n",
			__func__, (int)peq_params->v1,
			(int)peq_params->v2, (int)peq_params->v3,
			(int)peq_params->v4);
		break;
	}
	case SRS_ID_HL: {
		struct srs_trumedia_params_HL *hl_params = NULL;
		sz = sizeof(struct adm_cmd_set_pp_params_inband_v5) +
			sizeof(struct srs_trumedia_params_HL);
		adm_params = kzalloc(sz, GFP_KERNEL);
		if (!adm_params) {
			pr_err("%s, adm params memory alloc failed\n",
				__func__);
			return -ENOMEM;
		}
		adm_params->payload_size =
			sizeof(struct srs_trumedia_params_HL) +
			sizeof(struct adm_param_data_v5);
		adm_params->params.param_id = SRS_TRUMEDIA_PARAMS_HL;
		adm_params->params.param_size =
			sizeof(struct srs_trumedia_params_HL);
		hl_params = (struct srs_trumedia_params_HL *)
			((u8 *)adm_params +
			sizeof(struct adm_cmd_set_pp_params_inband_v5));
		memcpy(hl_params, srs_params,
				sizeof(struct srs_trumedia_params_HL));
		pr_debug("SRS - %s: HL params - 1 = %x, 2 = %x, 3 = %x, 4 = %x, 5 = %x, 6 = %x, 7 = %x\n",
				__func__, (int)hl_params->v1,
				(int)hl_params->v2, (int)hl_params->v3,
				(int)hl_params->v4, (int)hl_params->v5,
				(int)hl_params->v6, (int)hl_params->v7);
		break;
	}
	default:
		goto fail_cmd;
	}

	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
				__func__, index, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;

	adm_params->params.module_id = SRS_TRUMEDIA_MODULE_ID;
	adm_params->params.reserved = 0;

	pr_debug("SRS - %s: Command was sent now check Q6 - port id = %d, size %d, module id %x, param id %x.\n",
			__func__, adm_params->hdr.dest_port,
			adm_params->payload_size, adm_params->params.module_id,
			adm_params->params.param_id);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (ret < 0) {
		pr_err("SRS - %s: ADM enable for port %d failed\n", __func__,
			port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait[index], 1,
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: SRS set params timed out port = %d\n",
			__func__, port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}

fail_cmd:
	kfree(adm_params);
	return ret;
}

int adm_set_stereo_to_custom_stereo(int port_id, unsigned int session_id,
				    char *params, uint32_t params_length)
{
	struct adm_cmd_set_pspd_mtmx_strtr_params_v5 *adm_params = NULL;
	int sz, rc = 0, index = afe_get_port_index(port_id);

	pr_debug("%s\n", __func__);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d port_id %#x\n", __func__, index,
			port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5) +
		params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed\n", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params +
		sizeof(struct adm_cmd_set_pspd_mtmx_strtr_params_v5)),
		params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;
	/* direction RX as 0 */
	adm_params->direction = 0;
	/* session id for this cmd to be applied on */
	adm_params->sessionid = session_id;
	/* valid COPP id for LPCM */
	adm_params->deviceid = atomic_read(&this_adm.copp_id[index]);
	adm_params->reserved = 0;
	pr_debug("%s: deviceid %d, session_id %d, src_port %d, dest_port %d\n",
		__func__, adm_params->deviceid, adm_params->sessionid,
		adm_params->hdr.src_port, adm_params->hdr.dest_port);
	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = %#x\n", __func__,
			port_id);
		rc = -EINVAL;
		goto set_stereo_to_custom_stereo_return;
	}
	rc = 0;
set_stereo_to_custom_stereo_return:
	kfree(adm_params);
	return rc;
}

int adm_dolby_dap_send_params(int port_id, char *params, uint32_t params_length)
{
	struct adm_cmd_set_pp_params_v5	*adm_params = NULL;
	int sz, rc = 0, index = afe_get_port_index(port_id);

	pr_debug("%s\n", __func__);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_set_pp_params_v5) + params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params + sizeof(struct adm_cmd_set_pp_params_v5)),
			params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params->payload_addr_lsw = 0;
	adm_params->payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->payload_size = params_length;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = %#x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto dolby_dap_send_param_return;
	}
	rc = 0;
dolby_dap_send_param_return:
	kfree(adm_params);
	return rc;
}

int adm_get_params(int port_id, uint32_t module_id, uint32_t param_id,
		uint32_t params_length, char *params)
{
	struct adm_cmd_get_pp_params_v5 *adm_params = NULL;
	int sz, rc = 0, i = 0, index = afe_get_port_index(port_id);
	int *params_data = (int *)params;

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d port id 0x%x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_get_pp_params_v5) + params_length;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s: adm params memory alloc failed", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_params + sizeof(struct adm_cmd_get_pp_params_v5)),
		params, params_length);
	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
	APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_GET_PP_PARAMS_V5;
	adm_params->data_payload_addr_lsw = 0;
	adm_params->data_payload_addr_msw = 0;
	adm_params->mem_map_handle = 0;
	adm_params->module_id = module_id;
	adm_params->param_id = param_id;
	adm_params->param_max_size = params_length;
	adm_params->reserved = 0;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Failed to Get Params on port 0x%x rc %d\n",
			__func__,
			port_id, rc);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	/* Wait for the callback with copp id */
	rc = wait_event_timeout(this_adm.wait[index],
	atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: get params timed out port = %d\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_get_param_return;
	}

	if (adm_get_parameters[0] < 0) {
		pr_err("%s: Size is invalid %d\n", __func__,
			adm_get_parameters[0]);
		rc = -EINVAL;
		goto adm_get_param_return;
	}
	if (params_data) {
		for (i = 0; i < adm_get_parameters[0]; i++)
			params_data[i] = adm_get_parameters[1+i];
	}
	rc = 0;
adm_get_param_return:
	kfree(adm_params);

	return rc;
}

int adm_get_pp_topo_module_list(int port_id, int32_t param_length, char *params)
{
	struct adm_cmd_get_pp_topo_module_list_t *adm_pp_module_list = NULL;
	int sz, rc = 0, i = 0;
	int index = afe_get_port_index(port_id);
	int32_t *params_data = (int32_t *)params;

	pr_debug("%s : port_id %x", __func__, port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	sz = sizeof(struct adm_cmd_get_pp_topo_module_list_t) + param_length;
	adm_pp_module_list = kzalloc(sz, GFP_KERNEL);
	if (!adm_pp_module_list) {
		pr_err("%s, adm params memory alloc failed", __func__);
		return -ENOMEM;
	}

	memcpy(((u8 *)adm_pp_module_list +
		sizeof(struct adm_cmd_get_pp_topo_module_list_t)),
		params, param_length);
	adm_pp_module_list->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
	APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_pp_module_list->hdr.pkt_size = sz;
	adm_pp_module_list->hdr.src_svc = APR_SVC_ADM;
	adm_pp_module_list->hdr.src_domain = APR_DOMAIN_APPS;
	adm_pp_module_list->hdr.src_port = port_id;
	adm_pp_module_list->hdr.dest_svc = APR_SVC_ADM;
	adm_pp_module_list->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_pp_module_list->hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	adm_pp_module_list->hdr.token = port_id;
	adm_pp_module_list->hdr.opcode = ADM_CMD_GET_PP_TOPO_MODULE_LIST;
	adm_pp_module_list->param_max_size = param_length;
	/* Payload address and mmap handle set to zero by kzalloc */

	atomic_set(&this_adm.copp_stat[index], 0);

	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_pp_module_list);
	if (rc < 0) {
		pr_err("%s: Failed to Get Params on port %d\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_pp_module_list_l;
	}
	/* Wait for the callback with copp id */
	rc = wait_event_timeout(this_adm.wait[index],
	atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: get params timed out port = %d\n", __func__,
			port_id);
		rc = -EINVAL;
		goto adm_pp_module_list_l;
	}
	if (params_data) {
		if (param_length <= ADM_GET_TOPO_MODULE_LIST_LENGTH)
			memcpy(params_data, adm_module_topo_list, param_length);
		else {
			pr_debug("%s: i/p size:%d > MAX param size:%d\n",
				 __func__, param_length,
				 ADM_GET_TOPO_MODULE_LIST_LENGTH);
		}
		for (i = 1; i <= params_data[0]; i++)
			pr_debug("module = 0x%x\n", params_data[i]);
	}
	rc = 0;
adm_pp_module_list_l:
	kfree(adm_pp_module_list);
	pr_debug("%s : rc = %d ", __func__, rc);
	return rc;
}
static void adm_callback_debug_print(struct apr_client_data *data)
{
	uint32_t *payload;
	payload = data->payload;

	if (data->payload_size >= 8)
		pr_debug("%s: code = 0x%x PL#0[0x%x], PL#1[0x%x], size = %d\n",
			__func__, data->opcode, payload[0], payload[1],
			data->payload_size);
	else if (data->payload_size >= 4)
		pr_debug("%s: code = 0x%x PL#0[0x%x], size = %d\n",
			__func__, data->opcode, payload[0],
			data->payload_size);
	else
		pr_debug("%s: code = 0x%x, size = %d\n",
			__func__, data->opcode, data->payload_size);
}

void adm_set_multi_ch_map(char *channel_map)
{
	memcpy(multi_ch_map.channel_mapping, channel_map,
		PCM_FORMAT_MAX_NUM_CHANNEL);
	multi_ch_map.set_channel_map = true;
}

void adm_get_multi_ch_map(char *channel_map)
{
	if (multi_ch_map.set_channel_map) {
		memcpy(channel_map, multi_ch_map.channel_mapping,
			PCM_FORMAT_MAX_NUM_CHANNEL);
	}
}

static int32_t adm_callback(struct apr_client_data *data, void *priv)
{
	uint32_t *payload;
	int i, index;

	if (data == NULL) {
		pr_err("%s: data paramter is null\n", __func__);
		return -EINVAL;
	}

	payload = data->payload;

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: Reset event is received: %d %d apr[%p]\n",
				__func__,
				data->reset_event, data->reset_proc,
				this_adm.apr);
		if (this_adm.apr) {
			apr_reset(this_adm.apr);
			for (i = 0; i < AFE_MAX_PORTS; i++) {
				atomic_set(&this_adm.copp_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_low_latency_id[i],
							RESET_COPP_ID);
				atomic_set(&this_adm.copp_cnt[i], 0);
				atomic_set(&this_adm.copp_low_latency_cnt[i],
						0);
				atomic_set(&this_adm.copp_perf_mode[i], 0);
				atomic_set(&this_adm.copp_stat[i], 0);
			}
			this_adm.apr = NULL;
			reset_custom_topology_flags();
			this_adm.set_custom_topology = 1;
			for (i = 0; i < ADM_MAX_CAL_TYPES; i++)
				atomic_set(&this_adm.mem_map_cal_handles[i],
					0);
			rtac_clear_mapping(ADM_RTAC_CAL);
		}
		pr_debug("%s: Resetting calibration blocks\n", __func__);
		for (i = 0; i < MAX_AUDPROC_TYPES; i++) {
			/* Device calibration */
			this_adm.mem_addr_audproc[i].cal_size = 0;
			this_adm.mem_addr_audproc[i].cal_kvaddr = 0;
			this_adm.mem_addr_audproc[i].cal_paddr = 0;

			/* Volume calibration */
			this_adm.mem_addr_audvol[i].cal_size = 0;
			this_adm.mem_addr_audvol[i].cal_kvaddr = 0;
			this_adm.mem_addr_audvol[i].cal_paddr = 0;
		}
		return 0;
	}

	adm_callback_debug_print(data);
	if (data->payload_size) {
		index = q6audio_get_port_index(data->token);
		if (index < 0 || index >= AFE_MAX_PORTS) {
			pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, data->token);
			return 0;
		}
		if (data->opcode == APR_BASIC_RSP_RESULT) {
			pr_debug("%s: APR_BASIC_RSP_RESULT id 0x%x\n",
				__func__, payload[0]);
			if (payload[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, payload[0], payload[1]);
			}
			switch (payload[0]) {
			case ADM_CMD_SET_PP_PARAMS_V5:
				pr_debug("%s: ADM_CMD_SET_PP_PARAMS_V5\n",
					__func__);
				if (rtac_make_adm_callback(
					payload, data->payload_size)) {
					break;
				}
				/*
				 * if soft volume is called and already
				 * interrupted break out of the sequence here
				 */
			case ADM_CMD_DEVICE_CLOSE_V5:
			case ADM_CMD_SHARED_MEM_UNMAP_REGIONS:
			case ADM_CMD_MATRIX_MAP_ROUTINGS_V5:
			case ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5:
			case ADM_CMD_ADD_TOPOLOGIES:
				pr_debug("%s: Basic callback received, wake up.\n",
					__func__);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			case ADM_CMD_SHARED_MEM_MAP_REGIONS:
				pr_debug("%s: ADM_CMD_SHARED_MEM_MAP_REGIONS\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				if (payload[1] != 0) {
					pr_err("%s: ADM map error, resuming\n",
						__func__);
					atomic_set(&this_adm.copp_stat[index],
							1);
					wake_up(&this_adm.wait[index]);
				}
				break;
			case ADM_CMD_GET_PP_PARAMS_V5:
				pr_debug("%s: ADM_CMD_GET_PP_PARAMS_V5\n",
					__func__);
				/* Should only come here if there is an APR */
				/* error or malformed APR packet. Otherwise */
				/* response will be returned as */
				/* ADM_CMDRSP_GET_PP_PARAMS_V5 */
				if (payload[1] != 0) {
					pr_err("%s: ADM get param error = %d, resuming\n",
						__func__, payload[1]);
					rtac_make_adm_callback(payload,
						data->payload_size);
				}
				break;
			case ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5:
				pr_debug("%s: ADM_CMD_SET_PSPD_MTMX_STRTR_PARAMS_V5\n",
					__func__);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			case ADM_CMD_GET_PP_TOPO_MODULE_LIST:
				pr_debug("%s:ADM_CMD_GET_PP_TOPO_MODULE_LIST\n",
					 __func__);
				if (payload[1] != 0)
					pr_err("%s: ADM get topo list error = %d,\n",
						__func__, payload[1]);
				break;
			default:
				pr_err("%s: Unknown Cmd: 0x%x\n", __func__,
								payload[0]);
				break;
			}
			return 0;
		}

		switch (data->opcode) {
		case ADM_CMDRSP_DEVICE_OPEN_V5: {
			struct adm_cmd_rsp_device_open_v5 *open =
			(struct adm_cmd_rsp_device_open_v5 *)data->payload;

			if (open->copp_id == INVALID_COPP_ID) {
				pr_err("%s: invalid coppid rxed %d\n",
					__func__, open->copp_id);
				atomic_set(&this_adm.copp_stat[index], 1);
				wake_up(&this_adm.wait[index]);
				break;
			}
			if (atomic_read(&this_adm.copp_perf_mode[index])) {
				atomic_set(&this_adm.copp_low_latency_id[index],
						open->copp_id);
			} else {
				atomic_set(&this_adm.copp_id[index],
					open->copp_id);
			}
			atomic_set(&this_adm.copp_stat[index], 1);
			pr_debug("%s: coppid rxed=%d\n", __func__,
							open->copp_id);
			wake_up(&this_adm.wait[index]);
			}
			break;
		case ADM_CMDRSP_GET_PP_PARAMS_V5:
			pr_debug("%s: ADM_CMDRSP_GET_PP_PARAMS_V5\n", __func__);
			if (payload[0] != 0)
				pr_err("%s: ADM_CMDRSP_GET_PP_PARAMS_V5 returned error = 0x%x\n",
					__func__, payload[0]);
			if (rtac_make_adm_callback(payload,
					data->payload_size))
				break;

			if (payload[0] == 0) {
				if (data->payload_size >
				    (4 * sizeof(uint32_t))) {
					adm_get_parameters[0] = payload[3];
					pr_debug("GET_PP PARAM:received parameter length: 0x%x\n",
						adm_get_parameters[0]);
					/* storing param size then params */
					for (i = 0; i < payload[3]; i++)
						adm_get_parameters[1+i] =
								payload[4+i];
				}
			} else {
				adm_get_parameters[0] = -1;
				pr_err("%s: GET_PP_PARAMS failed, setting size to %d\n",
					__func__, adm_get_parameters[0]);
			}
			atomic_set(&this_adm.copp_stat[index], 1);
			wake_up(&this_adm.wait[index]);
			break;
		case ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST:
			pr_debug("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST\n",
				 __func__);
			if (payload[0] != 0) {
				pr_err("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST",
					 __func__);
				pr_err(":err = 0x%x\n", payload[0]);
			} else if (payload[1] >
				   ((ADM_GET_TOPO_MODULE_LIST_LENGTH /
				   sizeof(uint32_t)) - 1)) {
				pr_err("%s: ADM_CMDRSP_GET_PP_TOPO_MODULE_LIST",
					 __func__);
				pr_err(":size = %d\n", payload[1]);
			} else {
				pr_debug("%s:Num modules payload[1] %d\n",
					 __func__, payload[1]);
				adm_module_topo_list[0] = payload[1];
				for (i = 1; i <= payload[1]; i++) {
					adm_module_topo_list[i] = payload[1+i];
					pr_debug("%s:payload[%d] = %x\n",
						 __func__, (i+1), payload[1+i]);
				}
			}
			atomic_set(&this_adm.copp_stat[index], 1);
			wake_up(&this_adm.wait[index]);
			break;
		case ADM_CMDRSP_SHARED_MEM_MAP_REGIONS:
			pr_debug("%s: ADM_CMDRSP_SHARED_MEM_MAP_REGIONS\n",
				__func__);
			atomic_set(&this_adm.mem_map_cal_handles[
				atomic_read(&this_adm.mem_map_cal_index)],
				*payload);
			atomic_set(&this_adm.copp_stat[index], 1);
			wake_up(&this_adm.wait[index]);
			break;
		default:
			pr_err("%s: Unknown cmd:0x%x\n", __func__,
							data->opcode);
			break;
		}
	}
	return 0;
}

void send_adm_custom_topology(int port_id)
{
	struct acdb_cal_block		cal_block;
	struct cmd_set_topologies	adm_top;
	int				index;
	int				result;
	int				size = 4096;

	get_adm_custom_topology(&cal_block);
	if (cal_block.cal_size == 0) {
		pr_err("%s: no cal to send\n", __func__);
		pr_debug("%s: no cal to send addr= 0x%pa\n",
				__func__, &cal_block.cal_paddr);
		goto done;
	}

	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid 0x%x\n",
				__func__, index, port_id);
		goto done;
	}

	if (this_adm.set_custom_topology) {
		/* specific index 4 for adm topology memory */
		atomic_set(&this_adm.mem_map_cal_index, ADM_CUSTOM_TOP_CAL);

		/* Only call this once */
		this_adm.set_custom_topology = 0;

		result = adm_memory_map_regions(port_id,
				&cal_block.cal_paddr, 0, &size, 1);
		if (result < 0) {
			pr_err("%s: mmap did not work! size = %zd result %d\n",
				__func__, cal_block.cal_size, result);
			pr_debug("%s: mmap did not work! addr = 0x%pa, size = %zd\n",
				__func__, &cal_block.cal_paddr,
			       cal_block.cal_size);
			goto done;
		}
	}


	adm_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_top.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_top));
	adm_top.hdr.src_svc = APR_SVC_ADM;
	adm_top.hdr.src_domain = APR_DOMAIN_APPS;
	adm_top.hdr.src_port = port_id;
	adm_top.hdr.dest_svc = APR_SVC_ADM;
	adm_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_top.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_top.hdr.token = port_id;
	adm_top.hdr.opcode = ADM_CMD_ADD_TOPOLOGIES;
	adm_top.payload_addr_lsw = lower_32_bits(cal_block.cal_paddr);
	adm_top.payload_addr_msw = upper_32_bits(cal_block.cal_paddr);
	adm_top.mem_map_handle =
		atomic_read(&this_adm.mem_map_cal_handles[ADM_CUSTOM_TOP_CAL]);
	adm_top.payload_size = cal_block.cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending ADM_CMD_ADD_TOPOLOGIES payload = 0x%x, size = %d\n",
		__func__, adm_top.payload_addr_lsw,
		adm_top.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_top);
	if (result < 0) {
		pr_err("%s: Set topologies failed port = 0x%x result %d\n",
			__func__, port_id, result);
		pr_debug("%s: Set topologies failed port = 0x%x payload = 0x%pa\n",
			__func__, port_id, &cal_block.cal_paddr);
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set topologies timed out port = 0x%x\n",
			__func__, port_id);
		pr_debug("%s: Set topologies timed out port = 0x%x, payload = 0x%pa\n",
			__func__, port_id, &cal_block.cal_paddr);
		goto done;
	}

done:
	return;
}

static int send_adm_cal_block(int port_id, struct acdb_cal_block *aud_cal,
			      int perf_mode)
{
	s32				result = 0;
	struct adm_cmd_set_pp_params_v5	adm_params;
	int index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid 0x%x\n",
				__func__, index, port_id);
		return 0;
	}

	pr_debug("%s: Port id 0x%x, index %d, perf_mode %d, topo 0x%x\n",
		 __func__, port_id, index, perf_mode, this_adm.topology[index]);

	if (!aud_cal || aud_cal->cal_size == 0) {
		pr_debug("%s: No ADM cal send for port_id = 0x%x!\n",
			__func__, port_id);
		result = -EINVAL;
		goto done;
	}

	if (perf_mode == LEGACY_PCM_MODE &&
		this_adm.topology[index] == DS2_ADM_COPP_TOPOLOGY_ID) {
		pr_info("%s: perf_mode %d, topology 0x%x\n", __func__,
			perf_mode, this_adm.topology[index]);
		goto done;
	}

	adm_params.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(20), APR_PKT_VER);
	adm_params.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(adm_params));
	adm_params.hdr.src_svc = APR_SVC_ADM;
	adm_params.hdr.src_domain = APR_DOMAIN_APPS;
	adm_params.hdr.src_port = port_id;
	adm_params.hdr.dest_svc = APR_SVC_ADM;
	adm_params.hdr.dest_domain = APR_DOMAIN_ADSP;

	if (perf_mode == LEGACY_PCM_MODE)
		adm_params.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	else
		adm_params.hdr.dest_port =
			atomic_read(&this_adm.copp_low_latency_id[index]);

	adm_params.hdr.token = port_id;
	adm_params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_params.payload_addr_lsw = lower_32_bits(aud_cal->cal_paddr);
	adm_params.payload_addr_msw = upper_32_bits(aud_cal->cal_paddr);
	adm_params.mem_map_handle = atomic_read(&this_adm.mem_map_cal_handles[
				atomic_read(&this_adm.mem_map_cal_index)]);
	adm_params.payload_size = aud_cal->cal_size;

	atomic_set(&this_adm.copp_stat[index], 0);
	pr_debug("%s: Sending SET_PARAMS payload = 0x%x, size = %d\n",
		__func__, adm_params.payload_addr_lsw,
		adm_params.payload_size);
	result = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_params);
	if (result < 0) {
		pr_err("%s: Set params failed port = 0x%x result %d\n",
			__func__, port_id, result);
		pr_debug("%s: Set params failed port = 0x%x payload = 0x%pa\n",
			__func__, port_id, &aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}
	/* Wait for the callback */
	result = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!result) {
		pr_err("%s: Set params timed out port = 0x%x\n",
			__func__, port_id);
		pr_debug("%s: Set params timed out port = 0x%x, payload = 0x%pa\n",
			__func__, port_id, &aud_cal->cal_paddr);
		result = -EINVAL;
		goto done;
	}

	result = 0;
done:
	return result;
}

static void send_adm_cal(int port_id, int path, int perf_mode)
{
	int			result = 0;
	s32			acdb_path;
	struct acdb_cal_block	aud_cal;
	int			size;
	pr_debug("%s:\n", __func__);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;
	if (acdb_path == TX_CAL)
		size = 4096 * 4;
	else
		size = 4096;

	pr_debug("%s: Sending audproc cal\n", __func__);
	get_audproc_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	atomic_set(&this_adm.mem_map_cal_index, acdb_path);
	if (((this_adm.mem_addr_audproc[acdb_path].cal_paddr !=
		aud_cal.cal_paddr)  && (aud_cal.cal_size > 0)) ||
		(aud_cal.cal_size >
		this_adm.mem_addr_audproc[acdb_path].cal_size)) {

		if (this_adm.mem_addr_audproc[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(port_id);

		result = adm_memory_map_regions(port_id, &aud_cal.cal_paddr,
						0, &size, 1);
		if (result < 0) {
			pr_err("ADM audproc mmap did not work! path = %d, size = %zd result %d\n",
				acdb_path, aud_cal.cal_size, result);
			pr_debug("ADM audproc mmap did not work! path = %d, addr = 0x%pa, size = %zd\n",
				acdb_path, &aud_cal.cal_paddr,
				aud_cal.cal_size);
		} else {
			this_adm.mem_addr_audproc[acdb_path].cal_paddr =
							aud_cal.cal_paddr;
			this_adm.mem_addr_audproc[acdb_path].cal_size = size;

			this_adm.mem_addr_audproc[acdb_path].cal_kvaddr =
				aud_cal.cal_kvaddr;
		}
	}

	if (!send_adm_cal_block(port_id, &aud_cal, perf_mode))
		pr_debug("%s: Audproc cal sent for port id: 0x%x, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audproc cal not sent for port id: 0x%x, path %d\n",
			__func__, port_id, acdb_path);

	pr_debug("%s: Sending audvol cal\n", __func__);
	get_audvol_cal(acdb_path, &aud_cal);

	/* map & cache buffers used */
	atomic_set(&this_adm.mem_map_cal_index,
		(acdb_path + MAX_AUDPROC_TYPES));
	if (((this_adm.mem_addr_audvol[acdb_path].cal_paddr !=
		aud_cal.cal_paddr)  && (aud_cal.cal_size > 0))  ||
		(aud_cal.cal_size >
		this_adm.mem_addr_audvol[acdb_path].cal_size)) {

		if (this_adm.mem_addr_audvol[acdb_path].cal_paddr != 0)
			adm_memory_unmap_regions(port_id);

		result = adm_memory_map_regions(port_id, &aud_cal.cal_paddr,
						0, &size, 1);
		if (result < 0) {
			pr_err("%s: ADM audvol mmap did not work! path = %d, size = %zd result %d\n",
				__func__,
				acdb_path, aud_cal.cal_size, result);
			pr_debug("%s: ADM audvol mmap did not work! path = %d, addr = 0x%pa, size = %zd\n",
				__func__,
				acdb_path, &aud_cal.cal_paddr,
				aud_cal.cal_size);
		} else {
			this_adm.mem_addr_audvol[acdb_path].cal_paddr =
							aud_cal.cal_paddr;
			this_adm.mem_addr_audvol[acdb_path].cal_size = size;

			this_adm.mem_addr_audvol[acdb_path].cal_kvaddr =
				aud_cal.cal_kvaddr;
		}
	}

	if (!send_adm_cal_block(port_id, &aud_cal, perf_mode))
		pr_debug("%s: Audvol cal sent for port id: 0x%x, path %d\n",
			__func__, port_id, acdb_path);
	else
		pr_debug("%s: Audvol cal not sent for port id: 0x%x, path %d\n",
			__func__, port_id, acdb_path);
}

int adm_map_rtac_block(struct rtac_cal_block_data *cal_block)
{
	int	result = 0;
	pr_debug("%s:\n", __func__);

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->cal_data.paddr == 0) {
		pr_debug("%s: No address to map!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	if (cal_block->map_data.map_size == 0) {
		pr_debug("%s: map size is 0!\n",
			__func__);
		result = -EINVAL;
		goto done;
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC);
	result = adm_memory_map_regions(PRIMARY_I2S_RX,
			&cal_block->cal_data.paddr, 0,
			&cal_block->map_data.map_size, 1);
	if (result < 0) {
		pr_err("%s: RTAC mmap did not work! size = %d result %d\n",
			__func__, cal_block->map_data.map_size, result);
		pr_debug("%s: RTAC mmap did not work! addr = 0x%pa, size = %d\n",
			__func__, &cal_block->cal_data.paddr,
			cal_block->map_data.map_size);
		goto done;
	}

	cal_block->map_data.map_handle = atomic_read(
		&this_adm.mem_map_cal_handles[ADM_RTAC]);
done:
	return result;
}

int adm_unmap_rtac_block(uint32_t *mem_map_handle)
{
	int	result = 0;
	pr_debug("%s:\n", __func__);

	if (mem_map_handle == NULL) {
		pr_debug("%s: Map handle is NULL, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle == 0) {
		pr_debug("%s: Map handle is 0, nothing to unmap\n",
			__func__);
		goto done;
	}

	if (*mem_map_handle != atomic_read(
			&this_adm.mem_map_cal_handles[ADM_RTAC])) {
		pr_err("%s: Map handles do not match! Unmapping RTAC, RTAC map 0x%x, ADM map 0x%x\n",
			__func__, *mem_map_handle, atomic_read(
			&this_adm.mem_map_cal_handles[ADM_RTAC]));

		/* if mismatch use handle passed in to unmap */
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC],
			   *mem_map_handle);
	}

	/* valid port ID needed for callback use primary I2S */
	atomic_set(&this_adm.mem_map_cal_index, ADM_RTAC);
	result = adm_memory_unmap_regions(PRIMARY_I2S_RX);
	if (result < 0) {
		pr_debug("%s: adm_memory_unmap_regions failed, error %d\n",
			__func__, result);
	} else {
		atomic_set(&this_adm.mem_map_cal_handles[ADM_RTAC], 0);
		*mem_map_handle = 0;
	}
done:
	return result;
}

int adm_unmap_cal_blocks(void)
{
	int	i;
	int	result = 0;
	int	result2 = 0;

	for (i = 0; i < ADM_MAX_CAL_TYPES; i++) {
		if (atomic_read(&this_adm.mem_map_cal_handles[i]) != 0) {

			if (i <= ADM_TX_AUDPROC_CAL) {
				this_adm.mem_addr_audproc[i].cal_paddr = 0;
				this_adm.mem_addr_audproc[i].cal_size = 0;
			} else if (i <= ADM_TX_AUDVOL_CAL) {
				this_adm.mem_addr_audvol
					[(i - ADM_RX_AUDVOL_CAL)].cal_paddr
					= 0;
				this_adm.mem_addr_audvol
					[(i - ADM_RX_AUDVOL_CAL)].cal_size
					= 0;
			} else if (i == ADM_CUSTOM_TOP_CAL) {
				this_adm.set_custom_topology = 1;
			} else {
				continue;
			}

			/* valid port ID needed for callback use primary I2S */
			atomic_set(&this_adm.mem_map_cal_index, i);
			result2 = adm_memory_unmap_regions(PRIMARY_I2S_RX);
			if (result2 < 0) {
				pr_err("%s: adm_memory_unmap_regions failed, err %d\n",
						__func__, result2);
				result = result2;
			} else {
				atomic_set(&this_adm.mem_map_cal_handles[i],
					0);
			}
		}
	}
	return result;
}

int adm_connect_afe_port(int mode, int session_id, int port_id)
{
	struct adm_cmd_connect_afe_port_v5	cmd;
	int ret = 0;
	int index;

	pr_debug("%s: port %d session id:%d mode:%d\n", __func__,
				port_id, session_id, mode);

	port_id = afe_convert_virtual_to_portid(port_id);

	ret = afe_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id[0x%x] is invalid ret %d\n",
			__func__, port_id, ret);
		return -ENODEV;
	}
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}
	index = afe_get_port_index(port_id);
	pr_debug("%s: Port ID 0x%x, index %d\n", __func__, port_id, index);

	cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.src_svc = APR_SVC_ADM;
	cmd.hdr.src_domain = APR_DOMAIN_APPS;
	cmd.hdr.src_port = port_id;
	cmd.hdr.dest_svc = APR_SVC_ADM;
	cmd.hdr.dest_domain = APR_DOMAIN_ADSP;
	cmd.hdr.dest_port = port_id;
	cmd.hdr.token = port_id;
	cmd.hdr.opcode = ADM_CMD_CONNECT_AFE_PORT_V5;

	cmd.mode = mode;
	cmd.session_id = session_id;
	cmd.afe_port_id = port_id;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&cmd);
	if (ret < 0) {
		pr_err("%s: ADM enable for port 0x%x failed ret %d\n",
					__func__, port_id, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback with copp id */
	ret = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM connect AFE failed for port 0x%x\n", __func__,
							port_id);
		ret = -EINVAL;
		goto fail_cmd;
	}
	atomic_inc(&this_adm.copp_cnt[index]);
	return 0;

fail_cmd:

	return ret;
}

int adm_open(int port_id, int path, int rate, int channel_mode, int topology,
			int perf_mode, uint16_t bits_per_sample)
{
	struct adm_cmd_device_open_v5	open;
	int ret = 0;
	int index;
	int tmp_port = q6audio_get_port_id(port_id);

	pr_debug("%s: port 0x%x path:%d rate:%d mode:%d perf_mode:%d topo_id 0x%x\n",
		 __func__,
		port_id, path, rate, channel_mode, perf_mode, topology);

	port_id = q6audio_convert_virtual_to_portid(port_id);

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port idi[0x%x] is invalid ret %d\n",
			__func__, port_id, ret);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);
	pr_debug("%s: Port ID 0x%x, index %d\n", __func__, port_id, index);

	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	if (perf_mode == LEGACY_PCM_MODE) {
		atomic_set(&this_adm.copp_perf_mode[index], 0);
		if (path != ADM_PATH_COMPRESSED_RX)
			send_adm_custom_topology(port_id);
	} else {
		atomic_set(&this_adm.copp_perf_mode[index], 1);
	}

	if (this_adm.adm_delay[index] && perf_mode == LEGACY_PCM_MODE) {
		atomic_set(&this_adm.adm_delay_stat[index], 1);
		this_adm.adm_delay[index] = 0;
		wake_up(&this_adm.adm_delay_wait[index]);
	}

	/* Create a COPP if port id are not enabled */
	if ((perf_mode == LEGACY_PCM_MODE &&
		(atomic_read(&this_adm.copp_cnt[index]) == 0)) ||
		(perf_mode != LEGACY_PCM_MODE &&
		(atomic_read(&this_adm.copp_low_latency_cnt[index]) == 0))) {
		pr_debug("%s: opening ADM: perf_mode: %d\n", __func__,
			 perf_mode);
		memset(open.dev_channel_mapping, 0, ADM_MAX_CHANNELS);
		if (path == ADM_PATH_COMPRESSED_RX) {
			pr_debug("%s: compressed passthrough default topo\n",
				 __func__);
			open.flags = 0;
			open.topology_id =
				COMPRESSED_PASSTHROUGH_DEFAULT_TOPOLOGY;
			open.bit_width = 16;
			open.dev_num_channel = 2;
			open.sample_rate  = rate;
		} else {
			if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE)
				open.flags =
					ADM_ULTRA_LOW_LATENCY_DEVICE_SESSION;
			else if (perf_mode == LOW_LATENCY_PCM_MODE)
				open.flags = ADM_LOW_LATENCY_DEVICE_SESSION;
			else
				open.flags = ADM_LEGACY_DEVICE_SESSION;

			open.topology_id = topology;
			if ((open.topology_id ==
				VPM_TX_SM_ECNS_COPP_TOPOLOGY) ||
				(open.topology_id ==
				VPM_TX_DM_FLUENCE_COPP_TOPOLOGY))
					rate = 16000;

			if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE) {
				open.topology_id = NULL_COPP_TOPOLOGY;
				rate = ULL_SUPPORTED_SAMPLE_RATE;
			} else if (perf_mode == LOW_LATENCY_PCM_MODE) {
				if ((open.topology_id ==
					DOLBY_ADM_COPP_TOPOLOGY_ID) ||
					(open.topology_id ==
					SRS_TRUMEDIA_TOPOLOGY_ID) ||
					(open.topology_id ==
					DS2_ADM_COPP_TOPOLOGY_ID))
					open.topology_id =
						DEFAULT_COPP_TOPOLOGY;
			} else
				this_adm.topology[index] = open.topology_id;

			open.dev_num_channel = channel_mode & 0xFF;
			open.bit_width = bits_per_sample;
			WARN_ON(perf_mode == ULTRA_LOW_LATENCY_PCM_MODE &&
							(rate != 48000));
			open.sample_rate  = rate;
			memset(open.dev_channel_mapping, 0, ADM_MAX_CHANNELS);

			if (channel_mode == 1) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FC;
			} else if (channel_mode == 2) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
			} else if (channel_mode == 3) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
				open.dev_channel_mapping[2] = PCM_CHANNEL_FC;
			} else if (channel_mode == 4) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
				open.dev_channel_mapping[2] = PCM_CHANNEL_RB;
				open.dev_channel_mapping[3] = PCM_CHANNEL_LB;
			} else if (channel_mode == 5) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
				open.dev_channel_mapping[2] = PCM_CHANNEL_FC;
				open.dev_channel_mapping[3] = PCM_CHANNEL_LB;
				open.dev_channel_mapping[4] = PCM_CHANNEL_RB;
			} else if (channel_mode == 6) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
				open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
				open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
				open.dev_channel_mapping[4] = PCM_CHANNEL_LS;
				open.dev_channel_mapping[5] = PCM_CHANNEL_RS;
			} else if (channel_mode == 8) {
				open.dev_channel_mapping[0] = PCM_CHANNEL_FL;
				open.dev_channel_mapping[1] = PCM_CHANNEL_FR;
				open.dev_channel_mapping[2] = PCM_CHANNEL_LFE;
				open.dev_channel_mapping[3] = PCM_CHANNEL_FC;
				open.dev_channel_mapping[4] = PCM_CHANNEL_LB;
				open.dev_channel_mapping[5] = PCM_CHANNEL_RB;
				open.dev_channel_mapping[6] = PCM_CHANNEL_FLC;
				open.dev_channel_mapping[7] = PCM_CHANNEL_FRC;
			} else {
				pr_err("%s: invalid num_chan %d\n", __func__,
						channel_mode);
				ret = -EINVAL;
				goto fail_cmd;
			}
			if ((open.dev_num_channel > 2) &&
				multi_ch_map.set_channel_map)
				memcpy(open.dev_channel_mapping,
					multi_ch_map.channel_mapping,
					PCM_FORMAT_MAX_NUM_CHANNEL);
		}

		open.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		open.hdr.pkt_size = sizeof(open);
		open.hdr.src_svc = APR_SVC_ADM;
		open.hdr.src_domain = APR_DOMAIN_APPS;
		open.hdr.src_port = tmp_port;
		open.hdr.dest_svc = APR_SVC_ADM;
		open.hdr.dest_domain = APR_DOMAIN_ADSP;
		open.hdr.dest_port = tmp_port;
		open.hdr.token = port_id;
		open.hdr.opcode = ADM_CMD_DEVICE_OPEN_V5;

		open.mode_of_operation = path;
		open.endpoint_id_1 = tmp_port;

		if (this_adm.ec_ref_rx == -1) {
			open.endpoint_id_2 = 0xFFFF;
		} else if (this_adm.ec_ref_rx && (path != 1)) {
			open.endpoint_id_2 = this_adm.ec_ref_rx;
			this_adm.ec_ref_rx = -1;
		}


		pr_debug("%s: port_id=0x%x rate=%d topology_id=0x%X\n",
			__func__, open.endpoint_id_1, open.sample_rate,
			open.topology_id);

		atomic_set(&this_adm.copp_stat[index], 0);

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&open);
		if (ret < 0) {
			pr_err("%s: ADM enable for port 0x%x for[%d] failed %d\n",
				__func__, tmp_port, port_id, ret);
			ret = -EINVAL;
			goto fail_cmd;
		}
		/* Wait for the callback with copp id */
		ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[index]),
			msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM open failed for port 0x%x for [%d]\n",
						__func__, tmp_port, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}
	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
			perf_mode == LOW_LATENCY_PCM_MODE) {
		atomic_inc(&this_adm.copp_low_latency_cnt[index]);
		pr_debug("%s: index: %d coppid: %d", __func__, index,
			atomic_read(&this_adm.copp_low_latency_id[index]));
	} else {
		atomic_inc(&this_adm.copp_cnt[index]);
		pr_debug("%s: index: %d coppid: %d\n", __func__, index,
			atomic_read(&this_adm.copp_id[index]));
	}
	return 0;

fail_cmd:

	this_adm.topology[index] = 0;
	return ret;
}

int adm_multi_ch_copp_open(int port_id, int path, int rate, int channel_mode,
			int topology, int perf_mode, uint16_t bits_per_sample)
{
	int ret = 0;

	ret = adm_open(port_id, path, rate, channel_mode,
				   topology, perf_mode, bits_per_sample);

	return ret;
}

int adm_matrix_map(int session_id, int path, int num_copps,
			unsigned int *port_id, int copp_id, int perf_mode)
{
	struct adm_cmd_matrix_map_routings_v5	*route;
	struct adm_session_map_node_v5 *node;
	uint16_t *copps_list;
	int cmd_size = 0;
	int ret = 0, i = 0;
	void *payload = NULL;
	void *matrix_map = NULL;

	/* Assumes port_ids have already been validated during adm_open */
	int index = q6audio_get_port_index(copp_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d token %d\n",
					__func__, index, copp_id);
		return 0;
	}
	cmd_size = (sizeof(struct adm_cmd_matrix_map_routings_v5) +
			sizeof(struct adm_session_map_node_v5) +
			(sizeof(uint32_t) * num_copps));
	matrix_map = kzalloc(cmd_size, GFP_KERNEL);
	if (matrix_map == NULL) {
		pr_err("%s: Mem alloc failed\n", __func__);
		ret = -EINVAL;
		return ret;
	}
	route = (struct adm_cmd_matrix_map_routings_v5 *)matrix_map;

	pr_debug("%s: session 0x%x path:%d num_copps:%d port_id[0]:0x%x coppid[%d]\n",
		 __func__, session_id, path, num_copps, port_id[0], copp_id);

	route->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	route->hdr.pkt_size = cmd_size;
	route->hdr.src_svc = 0;
	route->hdr.src_domain = APR_DOMAIN_APPS;
	route->hdr.src_port = copp_id;
	route->hdr.dest_svc = APR_SVC_ADM;
	route->hdr.dest_domain = APR_DOMAIN_ADSP;
	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
			perf_mode == LOW_LATENCY_PCM_MODE) {
		route->hdr.dest_port =
			atomic_read(&this_adm.copp_low_latency_id[index]);
	} else {
		route->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	}
	route->hdr.token = copp_id;
	if (path == ADM_PATH_COMPRESSED_RX) {
		pr_debug("ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5 0x%x\n",
			  ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5);
		route->hdr.opcode = ADM_CMD_STREAM_DEVICE_MAP_ROUTINGS_V5;
	} else {
		pr_debug("ADM_CMD_MATRIX_MAP_ROUTINGS_V5 0x%x\n",
			ADM_CMD_MATRIX_MAP_ROUTINGS_V5);
		route->hdr.opcode = ADM_CMD_MATRIX_MAP_ROUTINGS_V5;
	}
	route->num_sessions = 1;

	switch (path) {
	case ADM_PATH_PLAYBACK:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_RX;
		break;
	case ADM_PATH_LIVE_REC:
	case ADM_PATH_NONLIVE_REC:
		route->matrix_id = ADM_MATRIX_ID_AUDIO_TX;
		break;
	case ADM_PATH_COMPRESSED_RX:
		route->matrix_id = ADM_MATRIX_ID_COMPRESSED_AUDIO_RX;
		break;
	default:
		pr_err("%s: Wrong path set[%d]\n", __func__, path);
		break;
	}
	payload = ((u8 *)matrix_map +
			sizeof(struct adm_cmd_matrix_map_routings_v5));
	node = (struct adm_session_map_node_v5 *)payload;

	node->session_id = session_id;
	node->num_copps = num_copps;
	payload = (u8 *)node + sizeof(struct adm_session_map_node_v5);
	copps_list = (uint16_t *)payload;
	for (i = 0; i < num_copps; i++) {
		int tmp;
		port_id[i] = q6audio_convert_virtual_to_portid(port_id[i]);

		tmp = q6audio_get_port_index(port_id[i]);


		if (tmp >= 0 && tmp < AFE_MAX_PORTS) {
			if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
					perf_mode == LOW_LATENCY_PCM_MODE)
				copps_list[i] =
				atomic_read(&this_adm.copp_low_latency_id[tmp]);
			else
				copps_list[i] =
					atomic_read(&this_adm.copp_id[tmp]);
		}
		else
			continue;
		pr_debug("%s: port_id[0x%x]: %d, index: %d act coppid[0x%x]\n",
			__func__, i, port_id[i], tmp, copps_list[i]);
	}
	atomic_set(&this_adm.copp_stat[index], 0);

	ret = apr_send_pkt(this_adm.apr, (uint32_t *)matrix_map);
	if (ret < 0) {
		pr_err("%s: ADM routing for port 0x%x failed %d\n",
					__func__, port_id[0], ret);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: ADM cmd Route failed for port 0x%x\n",
					__func__, port_id[0]);
		ret = -EINVAL;
		goto fail_cmd;
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE &&
		 path != ADM_PATH_COMPRESSED_RX) {
		for (i = 0; i < num_copps; i++)
			send_adm_cal(port_id[i], path, perf_mode);

		for (i = 0; i < num_copps; i++) {
			int tmp, copp_id;
			tmp = afe_get_port_index(port_id[i]);
			if (tmp >= 0 && tmp < AFE_MAX_PORTS) {
				if (perf_mode == LEGACY_PCM_MODE)
					copp_id = atomic_read(
					&this_adm.copp_id[tmp]);
				else
					copp_id = atomic_read(
					&this_adm.copp_low_latency_id[tmp]);
				rtac_add_adm_device(port_id[i],
						copp_id, path, session_id);
				pr_debug("%s: copp_id: %d\n",
							__func__, copp_id);
			} else
				pr_debug("%s: Invalid port index %d",
							__func__, tmp);
		}
	}

fail_cmd:
	kfree(matrix_map);
	return ret;
}

int adm_memory_map_regions(int port_id,
		phys_addr_t *buf_add, uint32_t mempool_id,
		uint32_t *bufsz, uint32_t bufcnt)
{
	struct  avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct  avs_shared_map_region_payload *mregions = NULL;
	void    *mmap_region_cmd = NULL;
	void    *payload = NULL;
	int     ret = 0;
	int     i = 0;
	int     cmd_size = 0;
	int     index = 0;

	pr_debug("%s:\n", __func__);
	if (this_adm.apr == NULL) {
		this_adm.apr = apr_register("ADSP", "ADM", adm_callback,
						0xFFFFFFFF, &this_adm);
		if (this_adm.apr == NULL) {
			pr_err("%s: Unable to register ADM\n", __func__);
			ret = -ENODEV;
			return ret;
		}
		rtac_set_adm_handle(this_adm.apr);
	}

	port_id = q6audio_convert_virtual_to_portid(port_id);

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port id 0x%x is invalid %d\n",
			__func__, port_id, ret);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ sizeof(struct avs_shared_map_region_payload)
			* bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (!mmap_region_cmd) {
		pr_err("%s: allocate mmap_region_cmd failed\n", __func__);
		return -ENOMEM;
	}
	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	mmap_regions->hdr.token = port_id;
	mmap_regions->hdr.opcode = ADM_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL & 0x00ff;
	mmap_regions->num_regions = bufcnt & 0x00ff;
	mmap_regions->property_flag = 0x00;

	pr_debug("%s: map_regions->num_regions = %d\n", __func__,
				mmap_regions->num_regions);
	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->shm_addr_lsw = lower_32_bits(buf_add[i]);
		mregions->shm_addr_msw = upper_32_bits(buf_add[i]);
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) mmap_region_cmd);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
					mmap_regions->hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
			atomic_read(&this_adm.copp_stat[index]), 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_map\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
fail_cmd:
	kfree(mmap_region_cmd);
	return ret;
}

int adm_memory_unmap_regions(int32_t port_id)
{
	struct  avs_cmd_shared_mem_unmap_regions unmap_regions;
	int     ret = 0;
	int     index = 0;

	pr_debug("%s:\n", __func__);

	if (this_adm.apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port_id = q6audio_convert_virtual_to_portid(port_id);

	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: port idi[%d] is invalid %d\n",
			__func__, port_id, ret);
		return -ENODEV;
	}

	index = q6audio_get_port_index(port_id);

	unmap_regions.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
							APR_PKT_VER);
	unmap_regions.hdr.pkt_size = sizeof(unmap_regions);
	unmap_regions.hdr.src_port = 0;
	unmap_regions.hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	unmap_regions.hdr.token = port_id;
	unmap_regions.hdr.opcode = ADM_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap_regions.mem_map_handle = atomic_read(&this_adm.
		mem_map_cal_handles[atomic_read(&this_adm.mem_map_cal_index)]);
	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *) &unmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap_regions op[0x%x]rc[%d]\n", __func__,
				unmap_regions.hdr.opcode, ret);
		ret = -EINVAL;
		goto fail_cmd;
	}

	ret = wait_event_timeout(this_adm.wait[index],
				 atomic_read(&this_adm.copp_stat[index]),
				 5 * HZ);
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap index %d\n",
		       __func__, index);
		ret = -EINVAL;
		goto fail_cmd;
	} else {
		pr_debug("%s: Unmap handle 0x%x succeeded\n", __func__,
			 unmap_regions.mem_map_handle);
	}
fail_cmd:
	return ret;
}

#ifdef CONFIG_RTAC
int adm_get_copp_id(int port_index)
{
	int copp_id;
	pr_debug("%s:\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	copp_id = atomic_read(&this_adm.copp_id[port_index]);
	if (copp_id == RESET_COPP_ID)
		copp_id = atomic_read(
			&this_adm.copp_low_latency_id[port_index]);
	return copp_id;
}

int adm_get_lowlatency_copp_id(int port_index)
{
	pr_debug("%s:\n", __func__);

	if (port_index < 0) {
		pr_err("%s: invalid port_id = %d\n", __func__, port_index);
		return -EINVAL;
	}

	return atomic_read(&this_adm.copp_low_latency_id[port_index]);
}
#else
int adm_get_copp_id(int port_index)
{
	return -EINVAL;
}

int adm_get_lowlatency_copp_id(int port_index)
{
	return -EINVAL;
}
#endif /* #ifdef CONFIG_RTAC */

void adm_ec_ref_rx_id(int port_id)
{
	this_adm.ec_ref_rx = port_id;
	pr_debug("%s: ec_ref_rx:%d", __func__, this_adm.ec_ref_rx);
}

int adm_close(int port_id, int perf_mode)
{
	struct apr_hdr close;

	int ret = 0;
	int index = 0;
	int copp_id = RESET_COPP_ID;

	port_id = q6audio_convert_virtual_to_portid(port_id);

	index = q6audio_get_port_index(port_id);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s err ret %d\n", __func__, ret);
		return -EINVAL;
	}

	pr_debug("%s: port_id=0x%x index %d perf_mode: %d\n", __func__, port_id,
		index, perf_mode);

	if (this_adm.adm_delay[index] && perf_mode == LEGACY_PCM_MODE) {
		atomic_set(&this_adm.adm_delay_stat[index], 1);
		this_adm.adm_delay[index] = 0;
		wake_up(&this_adm.adm_delay_wait[index]);
	}

	if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE) {
		if (!(atomic_read(&this_adm.copp_low_latency_cnt[index]))) {
			pr_err("%s: copp count for port 0x%x is 0\n", __func__,
				port_id);
			goto fail_cmd;
		}
		atomic_dec(&this_adm.copp_low_latency_cnt[index]);
	} else {
		if (!(atomic_read(&this_adm.copp_cnt[index]))) {
			pr_err("%s: copp count for port 0x%x is 0\n", __func__,
				port_id);
			goto fail_cmd;
		}
		atomic_dec(&this_adm.copp_cnt[index]);
	}
	if ((perf_mode == LEGACY_PCM_MODE &&
		!(atomic_read(&this_adm.copp_cnt[index]))) ||
		((perf_mode != LEGACY_PCM_MODE) &&
		!(atomic_read(&this_adm.copp_low_latency_cnt[index])))) {

		pr_debug("%s: Closing ADM: perf_mode: %d\n", __func__,
				perf_mode);
		close.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		close.pkt_size = sizeof(close);
		close.src_svc = APR_SVC_ADM;
		close.src_domain = APR_DOMAIN_APPS;
		close.src_port = port_id;
		close.dest_svc = APR_SVC_ADM;
		close.dest_domain = APR_DOMAIN_ADSP;
		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE)
			close.dest_port =
			     atomic_read(&this_adm.copp_low_latency_id[index]);
		else
			close.dest_port = atomic_read(&this_adm.copp_id[index]);
		close.token = port_id;
		close.opcode = ADM_CMD_DEVICE_CLOSE_V5;

		atomic_set(&this_adm.copp_stat[index], 0);

		if (perf_mode == ULTRA_LOW_LATENCY_PCM_MODE ||
				perf_mode == LOW_LATENCY_PCM_MODE) {
			copp_id = atomic_read(
				&this_adm.copp_low_latency_id[index]);
			pr_debug("%s: coppid %d portid 0x%x index=%d coppcnt=%d\n",
				__func__,
				copp_id,
				port_id, index,
				atomic_read(
					&this_adm.copp_low_latency_cnt[index]));
			atomic_set(&this_adm.copp_low_latency_id[index],
				RESET_COPP_ID);
		} else {
			copp_id = atomic_read(&this_adm.copp_id[index]);
			pr_debug("%s: coppid %d portid 0x%x index=%d coppcnt=%d\n",
				__func__,
				copp_id,
				port_id, index,
				atomic_read(&this_adm.copp_cnt[index]));
			atomic_set(&this_adm.copp_id[index],
				RESET_COPP_ID);
			this_adm.topology[index] = 0;
		}

		ret = apr_send_pkt(this_adm.apr, (uint32_t *)&close);
		if (ret < 0) {
			pr_err("%s: ADM close failed %d\n", __func__, ret);
			ret = -EINVAL;
			goto fail_cmd;
		}

		ret = wait_event_timeout(this_adm.wait[index],
				atomic_read(&this_adm.copp_stat[index]),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: ADM cmd Route failed for port 0x%x\n",
						__func__, port_id);
			ret = -EINVAL;
			goto fail_cmd;
		}
	}

	if (perf_mode != ULTRA_LOW_LATENCY_PCM_MODE) {
		pr_debug("%s: remove adm device from rtac\n", __func__);
		rtac_remove_adm_device(port_id, copp_id);
	}

fail_cmd:
	return ret;
}

int adm_set_volume(int port_id, int volume)
{
	struct audproc_volume_ctrl_master_gain audproc_vol;
	int sz = 0;
	int rc  = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: port_id %d, volume %d\n", __func__, port_id, volume);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		return -EINVAL;
	}

	sz = sizeof(struct audproc_volume_ctrl_master_gain);
	audproc_vol.params.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	audproc_vol.params.hdr.pkt_size = sz;
	audproc_vol.params.hdr.src_svc = APR_SVC_ADM;
	audproc_vol.params.hdr.src_domain = APR_DOMAIN_APPS;
	audproc_vol.params.hdr.src_port = port_id;
	audproc_vol.params.hdr.dest_svc = APR_SVC_ADM;
	audproc_vol.params.hdr.dest_domain = APR_DOMAIN_ADSP;
	audproc_vol.params.hdr.dest_port =
				atomic_read(&this_adm.copp_id[index]);
	audproc_vol.params.hdr.token = port_id;
	audproc_vol.params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	audproc_vol.params.payload_addr_lsw = 0;
	audproc_vol.params.payload_addr_msw = 0;
	audproc_vol.params.mem_map_handle = 0;
	audproc_vol.params.payload_size = sizeof(audproc_vol) -
				sizeof(audproc_vol.params);
	audproc_vol.data.module_id = AUDPROC_MODULE_ID_VOL_CTRL;
	audproc_vol.data.param_id = AUDPROC_PARAM_ID_VOL_CTRL_MASTER_GAIN;
	audproc_vol.data.param_size = audproc_vol.params.payload_size -
						sizeof(audproc_vol.data);
	audproc_vol.data.reserved = 0;
	audproc_vol.master_gain = volume;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)&audproc_vol);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Vol cntrl Set params timed out port = %#x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int adm_set_softvolume(int port_id,
			struct audproc_softvolume_params *softvol_param)
{
	struct audproc_soft_step_volume_params audproc_softvol;
	int sz = 0;
	int rc  = 0;

	int index = afe_get_port_index(port_id);

	pr_debug("%s: period %d step %d curve %d\n", __func__,
		 softvol_param->period, softvol_param->step,
		 softvol_param->rampingcurve);

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d port_id %#x\n", __func__, index,
			port_id);
		return -EINVAL;
	}
	sz = sizeof(struct audproc_soft_step_volume_params);

	audproc_softvol.params.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	audproc_softvol.params.hdr.pkt_size = sz;
	audproc_softvol.params.hdr.src_svc = APR_SVC_ADM;
	audproc_softvol.params.hdr.src_domain = APR_DOMAIN_APPS;
	audproc_softvol.params.hdr.src_port = port_id;
	audproc_softvol.params.hdr.dest_svc = APR_SVC_ADM;
	audproc_softvol.params.hdr.dest_domain = APR_DOMAIN_ADSP;
	audproc_softvol.params.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	audproc_softvol.params.hdr.token = port_id;
	audproc_softvol.params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	audproc_softvol.params.payload_addr_lsw = 0;
	audproc_softvol.params.payload_addr_msw = 0;
	audproc_softvol.params.mem_map_handle = 0;
	audproc_softvol.params.payload_size = sizeof(audproc_softvol) -
				sizeof(audproc_softvol.params);
	audproc_softvol.data.module_id = AUDPROC_MODULE_ID_VOL_CTRL;
	audproc_softvol.data.param_id =
			AUDPROC_PARAM_ID_SOFT_VOL_STEPPING_PARAMETERS;
	audproc_softvol.data.param_size = audproc_softvol.params.payload_size -
				sizeof(audproc_softvol.data);
	audproc_softvol.data.reserved = 0;
	audproc_softvol.period = softvol_param->period;
	audproc_softvol.step = softvol_param->step;
	audproc_softvol.ramping_curve = softvol_param->rampingcurve;

	pr_debug("%s: period %d, step %d, curve %d\n", __func__,
		 audproc_softvol.period, audproc_softvol.step,
		 audproc_softvol.ramping_curve);

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)&audproc_softvol);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Soft volume Set params timed out port = %#x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;
}

int adm_param_enable(int port_id, int module_id,  int enable)
{
	struct audproc_enable_param_t adm_mod_enable;
	int sz = 0;
	int rc  = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s port_id %d, enable 0x%x, enable %d\n",
		 __func__, port_id,  module_id,  enable);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d port_id %#x\n", __func__, index,
			port_id);
		return -EINVAL;
	}
	sz = sizeof(struct audproc_enable_param_t);

	adm_mod_enable.pp_params.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_mod_enable.pp_params.hdr.pkt_size = sz;
	adm_mod_enable.pp_params.hdr.src_svc = APR_SVC_ADM;
	adm_mod_enable.pp_params.hdr.src_domain = APR_DOMAIN_APPS;
	adm_mod_enable.pp_params.hdr.src_port = port_id;
	adm_mod_enable.pp_params.hdr.dest_svc = APR_SVC_ADM;
	adm_mod_enable.pp_params.hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_mod_enable.pp_params.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	adm_mod_enable.pp_params.hdr.token = port_id;
	adm_mod_enable.pp_params.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	adm_mod_enable.pp_params.payload_addr_lsw = 0;
	adm_mod_enable.pp_params.payload_addr_msw = 0;
	adm_mod_enable.pp_params.mem_map_handle = 0;
	adm_mod_enable.pp_params.payload_size = sizeof(adm_mod_enable) -
				sizeof(adm_mod_enable.pp_params) +
				sizeof(adm_mod_enable.pp_params.params);
	adm_mod_enable.pp_params.params.module_id = module_id;
	adm_mod_enable.pp_params.params.param_id = AUDPROC_PARAM_ID_ENABLE;
	adm_mod_enable.pp_params.params.param_size =
		adm_mod_enable.pp_params.payload_size -
		sizeof(adm_mod_enable.pp_params.params);
	adm_mod_enable.pp_params.params.reserved = 0;
	adm_mod_enable.enable = enable;

	atomic_set(&this_adm.copp_stat[index], 0);

	rc = apr_send_pkt(this_adm.apr, (uint32_t *)&adm_mod_enable);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s:  module %x  enable %d timed out on port = %#x\n",
			 __func__, module_id, enable, port_id);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = 0;
fail_cmd:
	return rc;

}

int adm_send_calibration(int port_id, int path, int perf_mode, int cal_type,
			 char *params, int size)
{

	struct adm_cmd_set_pp_params_v5	*adm_params = NULL;
	s32			acdb_path;
	int sz, rc = 0, index = afe_get_port_index(port_id);

	pr_debug("%s:port_id %d, path %d, perf_mode %d, cal_type %d, size %d\n",
		 __func__, port_id, path, perf_mode, cal_type, size);

	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx %d portid %#x\n",
			__func__, index, port_id);
		rc = -EINVAL;
		goto end;
	}

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;
	if (acdb_path != RX_CAL) {
		pr_err("%s: acdb_path %d\n", __func__, acdb_path);
		rc = -EINVAL;
		goto end;
	}

	sz = sizeof(struct adm_cmd_set_pp_params_v5) + size;
	adm_params = kzalloc(sz, GFP_KERNEL);
	if (!adm_params) {
		pr_err("%s, adm params memory alloc failed", __func__);
		rc = -ENOMEM;
		goto end;
	}

	memcpy(((u8 *)adm_params + sizeof(struct adm_cmd_set_pp_params_v5)),
			params, size);

	adm_params->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	adm_params->hdr.pkt_size = sz;
	adm_params->hdr.src_svc = APR_SVC_ADM;
	adm_params->hdr.src_domain = APR_DOMAIN_APPS;
	adm_params->hdr.src_port = port_id;
	adm_params->hdr.dest_svc = APR_SVC_ADM;
	adm_params->hdr.dest_domain = APR_DOMAIN_ADSP;
	adm_params->hdr.dest_port = atomic_read(&this_adm.copp_id[index]);
	adm_params->hdr.token = port_id;
	adm_params->hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	/* payload address and mmap handle initialized to zero by kzalloc */
	adm_params->payload_size = size;

	atomic_set(&this_adm.copp_stat[index], 0);
	rc = apr_send_pkt(this_adm.apr, (uint32_t *)adm_params);
	if (rc < 0) {
		pr_err("%s: Set params failed port = %#x\n",
			__func__, port_id);
		rc = -EINVAL;
		goto end;
	}
	/* Wait for the callback */
	rc = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!rc) {
		pr_err("%s: Set params timed out port = %#x\n",
			 __func__, port_id);
		rc = -EINVAL;
		goto end;
	}
	rc = 0;

end:
	kfree(adm_params);
	return rc;
}

/*
 * adm_update_wait_parameters must be called with routing driver locks.
 * adm_reset_wait_parameters must be called with routing driver locks.
 * set and reset parmeters are seperated to make sure it is always called
 * under routing driver lock.
 * adm_wait_timeout is to block until timeout or interrupted. Timeout is
 * not a an error.
 */
int adm_set_wait_parameters(int port_id)
{

	int ret = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: port_id 0x%x\n", __func__, port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x port_id 0x%x\n", __func__,
			index, port_id);
		ret = -EINVAL;
		goto end;
	}

	this_adm.adm_delay[index] = 1;
	atomic_set(&this_adm.adm_delay_stat[index], 0);

end:
	return ret;

}

int adm_reset_wait_parameters(int port_id)
{
	int ret = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: port_id 0x%x\n", __func__, port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x port_id 0x%x\n", __func__,
			index, port_id);
		ret = -EINVAL;
		goto end;
	}

	atomic_set(&this_adm.adm_delay_stat[index], 1);
	this_adm.adm_delay[index] = 0;

end:
	return ret;
}

int adm_wait_timeout(int port_id, int wait_time)
{
	int ret = 0;
	int index = afe_get_port_index(port_id);

	pr_debug("%s: port_id 0x%x, wait_time %d\n", __func__, port_id,
			wait_time);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x port_id 0x%x\n", __func__,
			index, port_id);
		ret = -EINVAL;
		goto end;
	}

	ret = wait_event_timeout(this_adm.adm_delay_wait[index],
		atomic_read(&this_adm.adm_delay_stat[index]),
		msecs_to_jiffies(wait_time));
	pr_debug("%s: return %d\n", __func__, ret);
	if (ret != 0)
		ret = -EINTR;
end:
	pr_debug("%s: return %d--\n", __func__, ret);
	return ret;
}

int adm_store_cal_data(int port_id, int path, int perf_mode,
		       int cal_type, char *params, int *size)
{
	int rc = 0;
	s32			acdb_path;
	struct acdb_cal_block	aud_cal;
	pr_debug("%s:port_id %d, path %d, perf_mode %d, cal_type %d, size %d\n",
		 __func__, port_id, path, perf_mode, cal_type, *size);

	/* Maps audio_dev_ctrl path definition to ACDB definition */
	acdb_path = path - 1;
	if (acdb_path != RX_CAL) {
		pr_err("%s: Not RX calibration %d\n", __func__, acdb_path);
		rc = -EINVAL;
		goto end;
	}
	if (cal_type == ADM_RX_AUDPROC_CAL) {
		rc = get_audproc_cal(acdb_path, &aud_cal);
		if (rc < 0) {
			pr_err("%s: get_audproc_cal return error %d\n",
				__func__, rc);
			goto end;
		}
		if (aud_cal.cal_size > AUD_PROC_BLOCK_SIZE) {
			pr_err("%s:audproc:invalid size exp/actual[%d, %d]\n",
				__func__, aud_cal.cal_size, *size);
			rc = -ENOMEM;
			goto end;
		}
	} else if (cal_type == ADM_RX_AUDVOL_CAL) {
		rc = get_audvol_cal(acdb_path, &aud_cal);
		if (rc < 0) {
			pr_err("%s: get_audproc_cal return error %d\n",
				__func__, rc);
			goto end;
		}

		if (aud_cal.cal_size > AUD_VOL_BLOCK_SIZE) {
			pr_err("%s:aud_vol:invalid size exp/actual[%d, %d]\n",
				__func__, aud_cal.cal_size, *size);
			rc = -ENOMEM;
			goto end;
		}
	} else {
		pr_debug("%s: Not valid calibration for dolby topolgy\n",
			 __func__);
		rc = -EINVAL;
		goto end;
	}
	memcpy(params, aud_cal.cal_kvaddr, aud_cal.cal_size);
	*size = aud_cal.cal_size;

end:
	return rc;
}

int adm_get_topology_id(int port_id)
{

	int index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x portid 0x%x\n",
			__func__, index, port_id);
		return -EINVAL;
	}
	return this_adm.topology[index];

}

int adm_send_compressed_device_mute(int port_id, bool mute_on)
{
	struct adm_set_compressed_device_mute mute_params;
	int index, ret = 0;

	pr_debug("%s port_id: 0x%x, mute_on: %d\n", __func__, port_id, mute_on);
	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x portid 0x%x\n",
			__func__, index, port_id);
		ret = -EINVAL;
		goto end;
	}

	mute_params.command.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	mute_params.command.hdr.pkt_size =
			sizeof(struct adm_set_compressed_device_mute);
	mute_params.command.hdr.src_svc = APR_SVC_ADM;
	mute_params.command.hdr.src_domain = APR_DOMAIN_APPS;
	mute_params.command.hdr.src_port = port_id;
	mute_params.command.hdr.dest_svc = APR_SVC_ADM;
	mute_params.command.hdr.dest_domain = APR_DOMAIN_ADSP;
	mute_params.command.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	mute_params.command.hdr.token = port_id;
	mute_params.command.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	mute_params.command.payload_addr_lsw = 0;
	mute_params.command.payload_addr_msw = 0;
	mute_params.command.mem_map_handle = 0;
	mute_params.command.payload_size = sizeof(mute_params) -
						sizeof(mute_params.command);
	mute_params.params.module_id = AUDPROC_MODULE_ID_COMPRESSED_MUTE;
	mute_params.params.param_id = AUDPROC_PARAM_ID_COMPRESSED_MUTE;
	mute_params.params.param_size = mute_params.command.payload_size -
					sizeof(mute_params.params);
	mute_params.params.reserved = 0;
	mute_params.mute_on = mute_on;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&mute_params);
	if (ret < 0) {
		pr_err("%s: send device mute for port %d failed, ret %d\n",
			__func__, port_id, ret);
		ret = -EINVAL;
		goto end;
	}

	/* Wait for the callback */
	ret = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: send device mute for port %d failed\n", __func__,
			port_id);
		ret = -EINVAL;
		goto end;
	}
	ret = 0;
end:
	return ret;
}

int adm_send_compressed_device_latency(int port_id, int latency)
{
	struct adm_set_compressed_device_latency latency_params;
	int index, ret = 0;

	pr_debug("%s port_id: 0x%x, latency: %d\n", __func__, port_id, latency);
	index = afe_get_port_index(port_id);
	if (index < 0 || index >= AFE_MAX_PORTS) {
		pr_err("%s: invalid port idx 0x%x portid 0x%x\n",
			__func__, index, port_id);
		ret = -EINVAL;
		goto end;
	}

	latency_params.command.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	latency_params.command.hdr.pkt_size =
			sizeof(struct adm_set_compressed_device_latency);
	latency_params.command.hdr.src_svc = APR_SVC_ADM;
	latency_params.command.hdr.src_domain = APR_DOMAIN_APPS;
	latency_params.command.hdr.src_port = port_id;
	latency_params.command.hdr.dest_svc = APR_SVC_ADM;
	latency_params.command.hdr.dest_domain = APR_DOMAIN_ADSP;
	latency_params.command.hdr.dest_port =
			atomic_read(&this_adm.copp_id[index]);
	latency_params.command.hdr.token = port_id;
	latency_params.command.hdr.opcode = ADM_CMD_SET_PP_PARAMS_V5;
	latency_params.command.payload_addr_lsw = 0;
	latency_params.command.payload_addr_msw = 0;
	latency_params.command.mem_map_handle = 0;
	latency_params.command.payload_size = sizeof(latency_params) -
						sizeof(latency_params.command);
	latency_params.params.module_id = AUDPROC_MODULE_ID_COMPRESSED_LATENCY;
	latency_params.params.param_id = AUDPROC_PARAM_ID_COMPRESSED_LATENCY;
	latency_params.params.param_size = latency_params.command.payload_size -
					sizeof(latency_params.params);
	latency_params.params.reserved = 0;
	latency_params.latency = latency;

	atomic_set(&this_adm.copp_stat[index], 0);
	ret = apr_send_pkt(this_adm.apr, (uint32_t *)&latency_params);
	if (ret < 0) {
		pr_err("%s: send device latency for port %d failed, ret %d\n",
			__func__, port_id, ret);
		ret = -EINVAL;
		goto end;
	}

	/* Wait for the callback */
	ret = wait_event_timeout(this_adm.wait[index],
		atomic_read(&this_adm.copp_stat[index]),
		msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: send device latency for port %d failed\n", __func__,
			port_id);
		ret = -EINVAL;
		goto end;
	}
	ret = 0;
end:
	return ret;
}
static int __init adm_init(void)
{
	int i = 0;
	this_adm.apr = NULL;
	this_adm.set_custom_topology = 1;
	this_adm.ec_ref_rx = -1;

	for (i = 0; i < AFE_MAX_PORTS; i++) {
		atomic_set(&this_adm.copp_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_low_latency_id[i], RESET_COPP_ID);
		atomic_set(&this_adm.copp_cnt[i], 0);
		atomic_set(&this_adm.copp_low_latency_cnt[i], 0);
		atomic_set(&this_adm.copp_stat[i], 0);
		atomic_set(&this_adm.copp_perf_mode[i], 0);
		atomic_set(&this_adm.adm_delay_stat[i], 0);
		init_waitqueue_head(&this_adm.wait[i]);
		init_waitqueue_head(&this_adm.adm_delay_wait[i]);
		this_adm.adm_delay[i] = 0;
		this_adm.topology[i] =  -1;
	}
	return 0;
}

device_initcall(adm_init);
