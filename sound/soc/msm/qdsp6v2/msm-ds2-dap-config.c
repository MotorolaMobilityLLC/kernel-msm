/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/err.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <sound/control.h>
#include <sound/q6adm-v2.h>

#include "msm-ds2-dap-config.h"
#include "msm-pcm-routing-v2.h"
#include "q6core.h"

#ifdef CONFIG_DOLBY_DS2

/* dolby param ids to/from dsp */
static uint32_t	ds2_dap_params_id[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_ID_VDHE, DOLBY_PARAM_ID_VSPE, DOLBY_PARAM_ID_DSSF,
	DOLBY_PARAM_ID_DVLI, DOLBY_PARAM_ID_DVLO, DOLBY_PARAM_ID_DVLE,
	DOLBY_PARAM_ID_DVMC, DOLBY_PARAM_ID_DVME, DOLBY_PARAM_ID_IENB,
	DOLBY_PARAM_ID_IEBF, DOLBY_PARAM_ID_IEON, DOLBY_PARAM_ID_DEON,
	DOLBY_PARAM_ID_NGON, DOLBY_PARAM_ID_GEON, DOLBY_PARAM_ID_GENB,
	DOLBY_PARAM_ID_GEBF, DOLBY_PARAM_ID_AONB, DOLBY_PARAM_ID_AOBF,
	DOLBY_PARAM_ID_AOBG, DOLBY_PARAM_ID_AOON, DOLBY_PARAM_ID_ARNB,
	DOLBY_PARAM_ID_ARBF, DOLBY_PARAM_ID_PLB,  DOLBY_PARAM_ID_PLMD,
	DOLBY_PARAM_ID_DHSB, DOLBY_PARAM_ID_DHRG, DOLBY_PARAM_ID_DSSB,
	DOLBY_PARAM_ID_DSSA, DOLBY_PARAM_ID_DVLA, DOLBY_PARAM_ID_IEBT,
	DOLBY_PARAM_ID_IEA,  DOLBY_PARAM_ID_DEA,  DOLBY_PARAM_ID_DED,
	DOLBY_PARAM_ID_GEBG, DOLBY_PARAM_ID_AOCC, DOLBY_PARAM_ID_ARBI,
	DOLBY_PARAM_ID_ARBL, DOLBY_PARAM_ID_ARBH, DOLBY_PARAM_ID_AROD,
	DOLBY_PARAM_ID_ARTP, DOLBY_PARAM_ID_VMON, DOLBY_PARAM_ID_VMB,
	DOLBY_PARAM_ID_VCNB, DOLBY_PARAM_ID_VCBF, DOLBY_PARAM_ID_PREG,
	DOLBY_PARAM_ID_VEN,  DOLBY_PARAM_ID_PSTG, DOLBY_PARAM_ID_INIT_ENDP,
};

/* modifed state:	0x00000000 - Not updated
*			> 0x00000000 && < 0x00010000
*				Updated and not commited to DSP
*			0x00010001 - Updated and commited to DSP
*			> 0x00010001 - Modified the commited value
*/
/* param offset */
static uint32_t	ds2_dap_params_offset[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_VDHE_OFFSET, DOLBY_PARAM_VSPE_OFFSET,
	DOLBY_PARAM_DSSF_OFFSET, DOLBY_PARAM_DVLI_OFFSET,
	DOLBY_PARAM_DVLO_OFFSET, DOLBY_PARAM_DVLE_OFFSET,
	DOLBY_PARAM_DVMC_OFFSET, DOLBY_PARAM_DVME_OFFSET,
	DOLBY_PARAM_IENB_OFFSET, DOLBY_PARAM_IEBF_OFFSET,
	DOLBY_PARAM_IEON_OFFSET, DOLBY_PARAM_DEON_OFFSET,
	DOLBY_PARAM_NGON_OFFSET, DOLBY_PARAM_GEON_OFFSET,
	DOLBY_PARAM_GENB_OFFSET, DOLBY_PARAM_GEBF_OFFSET,
	DOLBY_PARAM_AONB_OFFSET, DOLBY_PARAM_AOBF_OFFSET,
	DOLBY_PARAM_AOBG_OFFSET, DOLBY_PARAM_AOON_OFFSET,
	DOLBY_PARAM_ARNB_OFFSET, DOLBY_PARAM_ARBF_OFFSET,
	DOLBY_PARAM_PLB_OFFSET,  DOLBY_PARAM_PLMD_OFFSET,
	DOLBY_PARAM_DHSB_OFFSET, DOLBY_PARAM_DHRG_OFFSET,
	DOLBY_PARAM_DSSB_OFFSET, DOLBY_PARAM_DSSA_OFFSET,
	DOLBY_PARAM_DVLA_OFFSET, DOLBY_PARAM_IEBT_OFFSET,
	DOLBY_PARAM_IEA_OFFSET,  DOLBY_PARAM_DEA_OFFSET,
	DOLBY_PARAM_DED_OFFSET,  DOLBY_PARAM_GEBG_OFFSET,
	DOLBY_PARAM_AOCC_OFFSET, DOLBY_PARAM_ARBI_OFFSET,
	DOLBY_PARAM_ARBL_OFFSET, DOLBY_PARAM_ARBH_OFFSET,
	DOLBY_PARAM_AROD_OFFSET, DOLBY_PARAM_ARTP_OFFSET,
	DOLBY_PARAM_VMON_OFFSET, DOLBY_PARAM_VMB_OFFSET,
	DOLBY_PARAM_VCNB_OFFSET, DOLBY_PARAM_VCBF_OFFSET,
	DOLBY_PARAM_PREG_OFFSET, DOLBY_PARAM_VEN_OFFSET,
	DOLBY_PARAM_PSTG_OFFSET, DOLBY_PARAM_INT_ENDP_OFFSET,
};
/* param_length */
static uint32_t	ds2_dap_params_length[MAX_DS2_PARAMS] = {
	DOLBY_PARAM_VDHE_LENGTH, DOLBY_PARAM_VSPE_LENGTH,
	DOLBY_PARAM_DSSF_LENGTH, DOLBY_PARAM_DVLI_LENGTH,
	DOLBY_PARAM_DVLO_LENGTH, DOLBY_PARAM_DVLE_LENGTH,
	DOLBY_PARAM_DVMC_LENGTH, DOLBY_PARAM_DVME_LENGTH,
	DOLBY_PARAM_IENB_LENGTH, DOLBY_PARAM_IEBF_LENGTH,
	DOLBY_PARAM_IEON_LENGTH, DOLBY_PARAM_DEON_LENGTH,
	DOLBY_PARAM_NGON_LENGTH, DOLBY_PARAM_GEON_LENGTH,
	DOLBY_PARAM_GENB_LENGTH, DOLBY_PARAM_GEBF_LENGTH,
	DOLBY_PARAM_AONB_LENGTH, DOLBY_PARAM_AOBF_LENGTH,
	DOLBY_PARAM_AOBG_LENGTH, DOLBY_PARAM_AOON_LENGTH,
	DOLBY_PARAM_ARNB_LENGTH, DOLBY_PARAM_ARBF_LENGTH,
	DOLBY_PARAM_PLB_LENGTH,  DOLBY_PARAM_PLMD_LENGTH,
	DOLBY_PARAM_DHSB_LENGTH, DOLBY_PARAM_DHRG_LENGTH,
	DOLBY_PARAM_DSSB_LENGTH, DOLBY_PARAM_DSSA_LENGTH,
	DOLBY_PARAM_DVLA_LENGTH, DOLBY_PARAM_IEBT_LENGTH,
	DOLBY_PARAM_IEA_LENGTH,  DOLBY_PARAM_DEA_LENGTH,
	DOLBY_PARAM_DED_LENGTH,  DOLBY_PARAM_GEBG_LENGTH,
	DOLBY_PARAM_AOCC_LENGTH, DOLBY_PARAM_ARBI_LENGTH,
	DOLBY_PARAM_ARBL_LENGTH, DOLBY_PARAM_ARBH_LENGTH,
	DOLBY_PARAM_AROD_LENGTH, DOLBY_PARAM_ARTP_LENGTH,
	DOLBY_PARAM_VMON_LENGTH, DOLBY_PARAM_VMB_LENGTH,
	DOLBY_PARAM_VCNB_LENGTH, DOLBY_PARAM_VCBF_LENGTH,
	DOLBY_PARAM_PREG_LENGTH, DOLBY_PARAM_VEN_LENGTH,
	DOLBY_PARAM_PSTG_LENGTH, DOLBY_PARAM_INT_ENDP_LENGTH,
};

struct ds2_dap_params_s {
	int32_t params_val[TOTAL_LENGTH_DS2_PARAM];
	int32_t dap_params_modified[MAX_DS2_PARAMS];
};


static struct ds2_dap_params_s ds2_dap_params[DOLBY_MAX_CACHE];

struct ds2_device_mapping {
	int32_t device_id;
	int port_id;
	int cache_dev;
	uint32_t stream_ref_count;
	bool active;
};

static struct ds2_device_mapping dev_map[NUM_DS2_ENDP_DEVICE];

struct ds2_dap_params_states_s {
	bool use_cache;
	bool dap_bypass;
	bool node_opened;
	int32_t  device;
};

static struct ds2_dap_params_states_s ds2_dap_params_states = {true, false,
				false, DEVICE_NONE};

int all_supported_devices = EARPIECE|SPEAKER|WIRED_HEADSET|WIRED_HEADPHONE|
			BLUETOOTH_SCO|AUX_DIGITAL|ANLG_DOCK_HEADSET|
			DGTL_DOCK_HEADSET|REMOTE_SUBMIX|ANC_HEADSET|
			ANC_HEADPHONE|PROXY|FM|FM_TX|DEVICE_NONE|
			BLUETOOTH_SCO_HEADSET|BLUETOOTH_SCO_CARKIT;

static bool check_is_param_modified(int32_t *dap_params_modified,
				    int32_t idx, int32_t commit)
{
	if ((dap_params_modified[idx] == 0) ||
		(commit &&
		((dap_params_modified[idx] & 0x00010000) &&
		((dap_params_modified[idx] & 0x0000FFFF) <= 1)))) {
		pr_debug("%s: not modified at idx %d\n", __func__, idx);
		return false;
	}
	pr_debug("%s: modified at idx %d\n", __func__, idx);
	return true;
}

static int map_device_to_dolby_cache_devices(int32_t device_id)
{
	int32_t cache_dev = -1;
	switch (device_id) {
	case DEVICE_NONE:
		cache_dev = DOLBY_OFF_CACHE;
		break;
	case EARPIECE:
	case SPEAKER:
		cache_dev = DOLBY_SPEKAER_CACHE;
		break;
	case WIRED_HEADSET:
	case WIRED_HEADPHONE:
	case ANLG_DOCK_HEADSET:
	case DGTL_DOCK_HEADSET:
	case ANC_HEADSET:
	case ANC_HEADPHONE:
	case BLUETOOTH_SCO:
	case BLUETOOTH_SCO_HEADSET:
	case BLUETOOTH_SCO_CARKIT:
		cache_dev = DOLBY_HEADPHONE_CACHE;
		break;
	case FM:
	case FM_TX:
		cache_dev = DOLBY_FM_CACHE;
		break;
	case AUX_DIGITAL:
		cache_dev = DOLBY_HDMI_CACHE;
		break;
	case PROXY:
	case REMOTE_SUBMIX:
		cache_dev = DOLBY_WFD_CACHE;
		break;
	default:
		pr_err("%s: invalid cache device\n", __func__);
	}
	pr_debug("%s: cache device %d\n", __func__, cache_dev);
	return cache_dev;
}

static int msm_ds2_update_num_devices(struct dolby_param_data *dolby_data,
				      int32_t *num_device, int32_t *dev_arr,
				      int32_t array_size)
{
	int32_t idx = 0;
	int supported_devices = 0;

	if (!array_size) {
		pr_err("%s: array size zero\n", __func__);
		return -EINVAL;
	}

	if (dolby_data->device_id == DEVICE_OUT_ALL ||
		dolby_data->device_id == DEVICE_OUT_DEFAULT)
		supported_devices = all_supported_devices;
	else
		supported_devices = dolby_data->device_id;

	if ((idx < array_size) && (supported_devices & EARPIECE))
		dev_arr[idx++] = EARPIECE;
	if ((idx < array_size) && (supported_devices & SPEAKER))
		dev_arr[idx++] = SPEAKER;
	if ((idx < array_size) && (supported_devices & WIRED_HEADSET))
		dev_arr[idx++] = WIRED_HEADSET;
	if ((idx < array_size) && (supported_devices & WIRED_HEADPHONE))
		dev_arr[idx++] = WIRED_HEADPHONE;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO))
		dev_arr[idx++] = BLUETOOTH_SCO;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO_CARKIT))
		dev_arr[idx++] = BLUETOOTH_SCO_CARKIT;
	if ((idx < array_size) && (supported_devices & BLUETOOTH_SCO_HEADSET))
		dev_arr[idx++] = BLUETOOTH_SCO_HEADSET;
	if ((idx < array_size) && (supported_devices & AUX_DIGITAL))
		dev_arr[idx++] = AUX_DIGITAL;
	if ((idx < array_size) && (supported_devices & ANLG_DOCK_HEADSET))
		dev_arr[idx++] = ANLG_DOCK_HEADSET;
	if ((idx < array_size) && (supported_devices & DGTL_DOCK_HEADSET))
		dev_arr[idx++] = DGTL_DOCK_HEADSET;
	if ((idx < array_size) && (supported_devices & REMOTE_SUBMIX))
		dev_arr[idx++] = REMOTE_SUBMIX;
	if ((idx < array_size) && (supported_devices & ANC_HEADSET))
		dev_arr[idx++] = ANC_HEADSET;
	if ((idx < array_size) && (supported_devices & ANC_HEADPHONE))
		dev_arr[idx++] = ANC_HEADPHONE;
	if ((idx < array_size) && (supported_devices & PROXY))
		dev_arr[idx++] = PROXY;
	if ((idx < array_size) && (supported_devices & FM))
		dev_arr[idx++] = FM;
	if ((idx < array_size) && (supported_devices & FM_TX))
		dev_arr[idx++] = FM_TX;
	/* CHECK device none separately */
	if ((idx < array_size) && (supported_devices == DEVICE_NONE))
		dev_arr[idx++] = DEVICE_NONE;
	pr_debug("%s: dev id 0x%x, idx %d\n", __func__,
		 supported_devices, idx);
	*num_device = idx;
	return 0;
}

static int msm_ds2_dap_get_port_id(
		int32_t device_id, int32_t be_id)
{
	struct msm_pcm_routing_bdai_data bedais;
	int port_id = DOLBY_INVALID_PORT_ID;
	int port_type = 0;

	if (be_id < 0) {
		port_id = -1;
		goto end;
	}

	msm_pcm_routing_get_bedai_info(be_id, &bedais);
	pr_debug("%s: be port_id %d\n", __func__, bedais.port_id);
	port_id = bedais.port_id;
	port_type = afe_get_port_type(bedais.port_id);
	if (port_type != MSM_AFE_PORT_TYPE_RX)
		port_id = DOLBY_INVALID_PORT_ID;
end:
	pr_debug("%s: device_id 0x%x, be_id %d,  port_id %d\n",
		 __func__, device_id, be_id, port_id);
	return port_id;
}

static int msm_update_dev_map_port_id(int32_t device_id, int port_id)
{
	int i;
	for (i = 0; i < NUM_DS2_ENDP_DEVICE; i++) {
		if (dev_map[i].device_id == device_id)
			dev_map[i].port_id = port_id;
	}
	pr_debug("%s: port_id %d, device_id 0x%x\n",
		 __func__, port_id, device_id);
	return 0;
}

static int msm_ds2_get_device_index_from_port_id(int port_id)
{
	int i, idx = -1;
	for (i = 0; i < NUM_DS2_ENDP_DEVICE; i++) {
		if ((dev_map[i].port_id == port_id) &&
			/*TODO: handle multiple instance */
			(dev_map[i].device_id ==
			ds2_dap_params_states.device)) {
			idx = i;
			if (dev_map[i].device_id == SPEAKER)
				continue;
			else
				break;
		}
	}
	pr_debug("%s: port: %d, idx %d, dev 0x%x\n",  __func__, port_id, idx,
		 dev_map[idx].device_id);
	return idx;
}

static int msm_ds2_dap_send_end_point(int dev_map_idx, int endp_idx)
{
	int rc = 0;
	char *params_value;
	int32_t  *update_params_value;
	uint32_t params_length = (DOLBY_PARAM_INT_ENDP_LENGTH +
				DOLBY_PARAM_PAYLOAD_SIZE) * sizeof(uint32_t);
	int cache_device = dev_map[dev_map_idx].cache_dev;
	struct ds2_dap_params_s *ds2_ap_params_obj = NULL;
	int32_t *modified_param = NULL;

	if (dev_map_idx < 0 || dev_map_idx >= NUM_DS2_ENDP_DEVICE) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	ds2_ap_params_obj = &ds2_dap_params[cache_device];
	pr_debug("%s: cache dev %d, dev_map_idx %d\n", __func__,
		 cache_device, dev_map_idx);
	pr_debug("%s: endp - %p %p\n",  __func__,
		 &ds2_dap_params[cache_device], ds2_ap_params_obj);

	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: invalid port\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	update_params_value = (int32_t *)params_value;
	*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
	*update_params_value++ = DOLBY_PARAM_ID_INIT_ENDP;
	*update_params_value++ = DOLBY_PARAM_INT_ENDP_LENGTH * sizeof(uint32_t);
	*update_params_value++ = ds2_ap_params_obj->params_val[
					ds2_dap_params_offset[endp_idx]];
	pr_debug("%s: off %d, length %d\n", __func__,
		 ds2_dap_params_offset[endp_idx],
		 ds2_dap_params_length[endp_idx]);
	pr_debug("%s: param 0x%x, param val %d\n", __func__,
		 ds2_dap_params_id[endp_idx], ds2_ap_params_obj->
		 params_val[ds2_dap_params_offset[endp_idx]]);
	rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
				       params_value, params_length);
	if (rc) {
		pr_err("%s: send dolby params failed rc %d\n", __func__, rc);
		rc = -EINVAL;
	}
	modified_param = ds2_ap_params_obj->dap_params_modified;
	if (modified_param == NULL) {
		pr_err("%s: modified param structure invalid\n",
		       __func__);
		rc = -EINVAL;
		goto end;
	}

	if (check_is_param_modified(modified_param, endp_idx, 0))
		ds2_ap_params_obj->dap_params_modified[endp_idx] = 0x00010001;

end:
	kfree(params_value);
	return rc;
}

static int msm_ds2_dap_send_cached_params(int dev_map_idx,
					  int commit)
{
	char *params_value = NULL;
	int32_t *update_params_value;
	uint32_t idx, i, j, ret = 0;
	uint32_t params_length = (TOTAL_LENGTH_DOLBY_PARAM +
				(MAX_DS2_PARAMS - 1) *
				DOLBY_PARAM_PAYLOAD_SIZE) *
				sizeof(uint32_t);
	int cache_device = dev_map[dev_map_idx].cache_dev;
	struct ds2_dap_params_s *ds2_ap_params_obj = NULL;
	int32_t *modified_param = NULL;

	if (dev_map_idx < 0 || dev_map_idx >= NUM_DS2_ENDP_DEVICE) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		ret = -EINVAL;
		goto end;
	}

	if (ds2_dap_params_states.dap_bypass == true) {
		pr_debug("%s: use bypass cache 0\n", __func__);
		cache_device =  dev_map[0].cache_dev;
	}

	ds2_ap_params_obj = &ds2_dap_params[cache_device];
	pr_debug("%s: cached param - %p %p, cache_device %d\n", __func__,
		 &ds2_dap_params[cache_device], ds2_ap_params_obj,
		 cache_device);
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		ret =  -ENOMEM;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: invalid port id\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	update_params_value = (int32_t *)params_value;
	params_length = 0;
	for (i = 0; i < (MAX_DS2_PARAMS-1); i++) {
		/*get the pointer to the param modified array in the cache*/
		modified_param = ds2_ap_params_obj->dap_params_modified;
		if (modified_param == NULL) {
			pr_err("%s: modified param structure invalid\n",
			       __func__);
			ret = -EINVAL;
			goto end;
		}
		if (!check_is_param_modified(modified_param, i, commit))
			continue;
		*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
		*update_params_value++ = ds2_dap_params_id[i];
		*update_params_value++ = ds2_dap_params_length[i] *
						sizeof(uint32_t);
		idx = ds2_dap_params_offset[i];
		for (j = 0; j < ds2_dap_params_length[i]; j++) {
			*update_params_value++ =
					ds2_ap_params_obj->params_val[idx+j];
			pr_debug("%s: id 0x%x,val %d\n", __func__,
				 ds2_dap_params_id[i],
				 ds2_ap_params_obj->params_val[idx+j]);
		}
		params_length += (DOLBY_PARAM_PAYLOAD_SIZE +
				ds2_dap_params_length[i]) * sizeof(uint32_t);
	}

	pr_debug("%s: valid param length: %d\n", __func__, params_length);
	if (params_length) {
		ret = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
						params_value, params_length);
		if (ret) {
			pr_err("%s: send dolby params failed ret %d\n",
				__func__, ret);
			ret = -EINVAL;
			goto end;
		}
		for (i = 0; i < MAX_DS2_PARAMS-1; i++) {
			/*get pointer to the param modified array in the cache*/
			modified_param = ds2_ap_params_obj->dap_params_modified;
			if (modified_param == NULL) {
				pr_err("%s: modified param struct invalid\n",
					__func__);
				ret = -EINVAL;
				goto end;
			}
			if (!check_is_param_modified(modified_param, i, commit))
				continue;
			ds2_ap_params_obj->dap_params_modified[i] = 0x00010001;
		}
	}
end:
	kfree(params_value);
	return ret;
}

static int msm_ds2_commit_params(struct dolby_param_data *dolby_data,
				 int commit)
{
	int ret = 0, i, idx;
	struct ds2_dap_params_s *ds2_ap_params_obj =  NULL;
	int32_t *modified_param = NULL;

	for (idx = 0; idx < MAX_DS2_PARAMS; idx++) {
		if (DOLBY_PARAM_ID_INIT_ENDP == ds2_dap_params_id[idx])
			break;
	}
	if (idx >= MAX_DS2_PARAMS || idx < 0) {
		pr_err("%s: index of DS2 Param not found idx %d\n",
			__func__, idx);
		ret = -EINVAL;
		goto end;
	}
	pr_debug("%s: found endp - idx %d 0x%x\n", __func__, idx,
		ds2_dap_params_id[idx]);
	for (i = 0; i < NUM_DS2_ENDP_DEVICE; i++) {
		pr_debug("%s:Commit dev [0x%x,0x%x] idx  %d, active %d bypass %d\n",
			__func__, dolby_data->device_id, dev_map[i].device_id,
			i, dev_map[i].active, ds2_dap_params_states.dap_bypass);

		if (((dev_map[i].device_id == ds2_dap_params_states.device) ||
			(ds2_dap_params_states.dap_bypass == true)) &&
			(dev_map[i].active == true)) {

			/*get ptr to the cache storing the params for device*/
			if (ds2_dap_params_states.dap_bypass == true)
				ds2_ap_params_obj =
					&ds2_dap_params[dev_map[0].cache_dev];
			else
				ds2_ap_params_obj =
					&ds2_dap_params[dev_map[i].cache_dev];

			/*get the pointer to the param modified array in cache*/
			modified_param = ds2_ap_params_obj->dap_params_modified;
			if (modified_param == NULL) {
				pr_err("%s: modified_param NULL\n", __func__);
				ret = -EINVAL;
				goto end;
			}

			/*
			 * Send the endp param if use cache is set
			 * or if param is modified
			 */
			if (!commit || check_is_param_modified(
					modified_param, idx, commit)) {
				msm_ds2_dap_send_end_point(i, idx);
				commit = 0;
			}
			ret = msm_ds2_dap_send_cached_params(i, commit);
			if (ret < 0) {
				pr_err("%s: send cached param %d\n",
					__func__, ret);
				goto end;
			}
		}
	}
end:
	return ret;
}

static int msm_ds2_dap_handle_commands(u32 cmd, void *arg)
{
	int ret  = 0, port_id = 0;
	struct dolby_param_data dolby_data;

	if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
		pr_err("%s: Copy from user failed\n", __func__);
		return -EFAULT;
	}
	pr_debug("%s: param_id %d,be_id %d,device_id 0x%x,length %d,data %d\n",
		 __func__, dolby_data.param_id, dolby_data.be_id,
		dolby_data.device_id, dolby_data.length, dolby_data.data[0]);

	switch (dolby_data.param_id) {
	case DAP_CMD_COMMIT_ALL:
		msm_ds2_commit_params(&dolby_data, 0);
	break;

	case DAP_CMD_COMMIT_CHANGED:
		msm_ds2_commit_params(&dolby_data, 1);
	break;

	case DAP_CMD_USE_CACHE_FOR_INIT:
		ds2_dap_params_states.use_cache = dolby_data.data[0];
	break;

	case DAP_CMD_SET_BYPASS:
		ds2_dap_params_states.dap_bypass = dolby_data.data[0];
		msm_ds2_commit_params(&dolby_data, 0);
	break;

	case DAP_CMD_SET_ACTIVE_DEVICE:
		pr_debug("%s: DAP_CMD_SET_ACTIVE_DEVICE length %d\n",
			__func__, dolby_data.length);
		/* TODO: need to handle multiple instance*/
		ds2_dap_params_states.device = dolby_data.device_id;
		port_id = msm_ds2_dap_get_port_id(
						  dolby_data.device_id,
						  dolby_data.be_id);
		pr_debug("%s: device id 0x%x all_dev 0x%x port_id %d\n",
			__func__, dolby_data.device_id,
			ds2_dap_params_states.device, port_id);
		msm_update_dev_map_port_id(dolby_data.device_id,
					   port_id);
		if (port_id == DOLBY_INVALID_PORT_ID) {
			pr_err("%s: invalid port id %d\n", __func__, port_id);
			ret = -EINVAL;
			goto end;
		}
	break;
	}
end:
	return ret;

}

static int msm_ds2_dap_set_param(u32 cmd, void *arg)
{
	int rc = 0, idx, i, j, off, port_id = 0, cdev = 0;
	int32_t num_device = 0;
	int32_t dev_arr[NUM_DS2_ENDP_DEVICE] = {0};
	struct dolby_param_data dolby_data;

	if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
		pr_err("%s: Copy from user failed\n", __func__);
		rc =  -EFAULT;
		goto end;
	}

	rc = msm_ds2_update_num_devices(&dolby_data, &num_device, dev_arr,
				   NUM_DS2_ENDP_DEVICE);
	if (num_device == 0 || rc < 0) {
		pr_err("%s: num devices 0\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	for (i = 0; i < num_device; i++) {
		port_id = msm_ds2_dap_get_port_id(dev_arr[i],
						  dolby_data.be_id);
		if (port_id != DOLBY_INVALID_PORT_ID)
			msm_update_dev_map_port_id(dev_arr[i], port_id);

		cdev = map_device_to_dolby_cache_devices(dev_arr[i]);
		if (cdev < 0 || cdev >= DOLBY_MAX_CACHE) {
			pr_err("%s: Invalide cache device %d for device 0x%x\n",
				__func__, cdev, dev_arr[i]);
			rc = -EINVAL;
			goto end;
		}
		pr_debug("%s:port:%d,be:%d,dev:0x%x,cdev:%d,param:0x%x,len:%d\n"
			 , __func__, port_id, dolby_data.be_id, dev_arr[i],
			 cdev, dolby_data.param_id, dolby_data.length);
		for (idx = 0; idx < MAX_DS2_PARAMS; idx++) {
			/*paramid from user space*/
			if (dolby_data.param_id == ds2_dap_params_id[idx])
				break;
		}
		if (idx > MAX_DS2_PARAMS-1) {
			pr_err("%s: invalid param id 0x%x at idx %d\n",
				__func__, dolby_data.param_id, idx);
			rc = -EINVAL;
			goto end;
		}

		/* cache the parameters */
		ds2_dap_params[cdev].dap_params_modified[idx] += 1;
		for (j = 0; j <  dolby_data.length; j++) {
			off = ds2_dap_params_offset[idx];
			ds2_dap_params[cdev].params_val[off + j] =
							dolby_data.data[j];
				pr_debug("%s:off %d,val[i/p:o/p]-[%d / %d]\n",
					 __func__, off, dolby_data.data[j],
					 ds2_dap_params[cdev].
					 params_val[off + j]);
		}
	}
end:
	return rc;
}

static int msm_ds2_dap_get_param(u32 cmd, void *arg)
{

	int rc = 0, i, port_id = 0;
	struct dolby_param_data dolby_data;
	char *params_value = NULL;
	int32_t *update_params_value;
	uint32_t params_length = DOLBY_MAX_LENGTH_INDIVIDUAL_PARAM *
					sizeof(uint32_t);
	uint32_t param_payload_len =
			DOLBY_PARAM_PAYLOAD_SIZE * sizeof(uint32_t);

	if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
		pr_err("%s: Copy from user failed\n", __func__);
		return -EFAULT;
	}

	if (ds2_dap_params_states.dap_bypass) {
		pr_err("%s: called in bypass %d\n", __func__,
			ds2_dap_params_states.dap_bypass);
		rc = -EINVAL;
		goto end;
	}

	for (i = 0; i < NUM_DS2_ENDP_DEVICE; i++) {
		if ((dev_map[i].active) &&
			(dev_map[i].device_id & dolby_data.device_id)) {
			port_id = dev_map[i].port_id;
			break;
		}
	}

	if (port_id == DOLBY_INVALID_PORT_ID) {
		pr_err("%s: Invalid port\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	pr_debug("%s: port_id 0x%x,  dev_map[i].device_id %x\n",
		 __func__, port_id, dev_map[i].device_id);

	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	if (dolby_data.param_id == DOLBY_PARAM_ID_VER) {
		rc = adm_get_params(port_id,
				    DOLBY_BUNDLE_MODULE_ID,
				    DOLBY_PARAM_ID_VER,
				    params_length + param_payload_len,
				    params_value);
	} else {
		for (i = 0; i < MAX_DS2_PARAMS; i++)
			if (ds2_dap_params_id[i] ==
				dolby_data.param_id)
				break;
		if (i > MAX_DS2_PARAMS-1) {
			pr_err("%s: invalid param id 0x%x at id %d\n", __func__,
				dolby_data.param_id, i);
			rc = -EINVAL;
			goto end;
		} else {
			params_length = (ds2_dap_params_length[i] +
						DOLBY_PARAM_PAYLOAD_SIZE) *
						sizeof(uint32_t);
			rc = adm_get_params(port_id,
					    DOLBY_BUNDLE_MODULE_ID,
					    ds2_dap_params_id[i],
					    params_length +
					    param_payload_len,
					    params_value);
		}
	}
	if (rc) {
		pr_err("%s: get parameters failed rc %d\n", __func__, rc);
		rc = -EINVAL;
		goto end;
	}
	update_params_value = (int32_t *)params_value;

	if (copy_to_user((void *)dolby_data.data,
			&update_params_value[DOLBY_PARAM_PAYLOAD_SIZE],
			(dolby_data.length * sizeof(uint32_t)))) {
		pr_err("%s: error getting param\n", __func__);
		rc = -EFAULT;
		goto end;
	}
end:
	kfree(params_value);
	return rc;
}

static int msm_ds2_dap_set_vspe_vdhe(int dev_map_idx,
				     bool is_custom_stereo_enabled)
{
	char *params_value = NULL;
	int32_t *update_params_value;
	int idx, i, j, rc = 0, cdev;
	uint32_t params_length = (TOTAL_LENGTH_DOLBY_PARAM +
				2 * DOLBY_PARAM_PAYLOAD_SIZE) *
				sizeof(uint32_t);

	if (dev_map_idx < 0 || dev_map_idx >= NUM_DS2_ENDP_DEVICE) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		pr_debug("%s: Invalid port id\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	update_params_value = (int32_t *)params_value;
	params_length = 0;
	cdev = dev_map[dev_map_idx].cache_dev;
	/* for VDHE and VSPE DAP params at index 0 and 1 in table */
	for (i = 0; i < 2; i++) {
		*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
		*update_params_value++ = ds2_dap_params_id[i];
		*update_params_value++ = ds2_dap_params_length[i] *
					sizeof(uint32_t);
		idx = ds2_dap_params_offset[i];
		for (j = 0; j < ds2_dap_params_length[i]; j++) {
			if (is_custom_stereo_enabled)
				*update_params_value++ = 0;
			else
				*update_params_value++ =
					ds2_dap_params[cdev].params_val[idx+j];
		}
		params_length += (DOLBY_PARAM_PAYLOAD_SIZE +
				  ds2_dap_params_length[i]) *
				  sizeof(uint32_t);
	}

	pr_debug("%s: valid param length: %d\n", __func__, params_length);
	if (params_length) {
		rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
					       params_value, params_length);
		if (rc) {
			pr_err("%s: send vdhe/vspe params failed with rc=%d\n",
				__func__, rc);
			kfree(params_value);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	kfree(params_value);
	return rc;
}

static int msm_ds2_dap_param_visualizer_control_get(u32 cmd, void *arg)
{
	char *visualizer_data = NULL;
	int  i = 0, ret = 0, port_id = -1, cache_dev = -1;
	int32_t *update_visualizer_data;
	struct dolby_param_data dolby_data;
	uint32_t offset, length, params_length;
	uint32_t param_payload_len =
		DOLBY_PARAM_PAYLOAD_SIZE * sizeof(uint32_t);

	if (copy_from_user((void *)&dolby_data, (void *)arg,
				sizeof(struct dolby_param_data))) {
		pr_err("%s: Copy from user failed\n", __func__);
		return -EFAULT;
	}

	for (i = 0; i < NUM_DS2_ENDP_DEVICE; i++) {
		if ((dev_map[i].active))  {
			port_id = dev_map[i].port_id;
			cache_dev = dev_map[i].cache_dev;
			break;
		}
	}

	if (port_id == DOLBY_INVALID_PORT_ID) {
		ret = 0;
		dolby_data.length = 0;
		pr_err("%s: no device active\n", __func__);
		goto copy_data;
	}

	length = ds2_dap_params[cache_dev].params_val[DOLBY_PARAM_VCNB_OFFSET];
	params_length = (2*length + DOLBY_VIS_PARAM_HEADER_SIZE) *
							 sizeof(uint32_t);

	visualizer_data = kzalloc(params_length, GFP_KERNEL);
	if (!visualizer_data) {
		pr_err("%s: params memory alloc failed\n", __func__);
		ret = -ENOMEM;
		dolby_data.length = 0;
		goto copy_data;
	}

	if (ds2_dap_params_states.dap_bypass) {
		pr_debug("%s: visualizer called in bypass, return 0\n",
			 __func__);
		ret = 0;
		dolby_data.length = 0;
		goto copy_data;
	}

	offset = 0;
	params_length = length * sizeof(uint32_t);
	ret = adm_get_params(port_id,
			    DOLBY_BUNDLE_MODULE_ID,
			    DOLBY_PARAM_ID_VCBG,
			    params_length + param_payload_len,
			    visualizer_data + offset);
	if (ret) {
		pr_err("%s: get parameters failed ret %d\n", __func__, ret);
		kfree(visualizer_data);
		ret = -EINVAL;
		dolby_data.length = 0;
		goto copy_data;
	}
	offset = length * sizeof(uint32_t);
	ret = adm_get_params(port_id,
			    DOLBY_BUNDLE_MODULE_ID,
			    DOLBY_PARAM_ID_VCBE,
			    params_length + param_payload_len,
			    visualizer_data + offset);
	if (ret) {
		pr_err("%s: get parameters failed ret %d\n", __func__, ret);
		kfree(visualizer_data);
		ret = -EINVAL;
		dolby_data.length = 0;
		goto copy_data;
	}

	update_visualizer_data = (int *)visualizer_data;
	dolby_data.length = 2 * length;

	if (copy_to_user((void *)dolby_data.data,
			(void *)update_visualizer_data,
			(dolby_data.length * sizeof(uint32_t)))) {
		pr_err("%s: copy to user failed for data\n", __func__);
		dolby_data.length = 0;
		ret = -EFAULT;
		goto copy_data;
	}

copy_data:
	if (copy_to_user((void *)arg,
			(void *)&dolby_data,
			sizeof(struct dolby_param_data))) {
		pr_err("%s: copy to user failed for data\n", __func__);
		ret = -EFAULT;
	}

	kfree(visualizer_data);
	return ret;
}

int msm_ds2_dap_set_security_control(u32 cmd, void *arg)
{
	int32_t  manufacturer_id = 0;
	if (copy_from_user((void *)&manufacturer_id,
			arg, sizeof(int32_t))) {
		pr_err("%s: Copy from user failed\n", __func__);
		return -EFAULT;
	}
	pr_debug("%s: manufacturer_id %d\n", __func__, manufacturer_id);
	core_set_dolby_manufacturer_id(manufacturer_id);
	return 0;
}

int msm_ds2_dap_update_port_parameters(struct snd_hwdep *hw,  struct file *file,
				       bool open)
{
	int  i = 0, dev_id = 0;
	pr_debug("%s: open %d\n", __func__, open);
	ds2_dap_params_states.node_opened = open;
	ds2_dap_params_states.dap_bypass = true;
	ds2_dap_params_states.use_cache = 0;
	ds2_dap_params_states.device = 0;
	for (i = 0; i < ALL_DEVICES; i++) {
		if (i == 0)
			dev_map[i].device_id = 0;
		else {
			dev_id = (1 << (i-1));
			if (all_supported_devices & dev_id)
				dev_map[i].device_id = dev_id;
			else
				continue;
		}
		dev_map[i].cache_dev = map_device_to_dolby_cache_devices(
				    dev_map[i].device_id);
		if (dev_map[i].cache_dev < 0 ||
				dev_map[i].cache_dev >= DOLBY_MAX_CACHE)
			pr_err("%s: Invalide cache device %d for device 0x%x\n",
						__func__,
						dev_map[i].cache_dev,
						dev_map[i].device_id);
		dev_map[i].port_id = -1;
		dev_map[i].active = false;
		dev_map[i].stream_ref_count = 0;
		pr_debug("%s: device_id 0x%x, cache_dev %d act  %d\n", __func__,
			 dev_map[i].device_id, dev_map[i].cache_dev,
			 dev_map[i].active);
	}
	return 0;

}

int msm_ds2_dap_ioctl(struct snd_hwdep *hw, struct file *file,
		      u32 cmd, void *arg)
{
	int ret = 0;
	pr_debug("%s: cmd: 0x%x\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_DEVDEP_DAP_IOCTL_SET_PARAM:
		ret = msm_ds2_dap_set_param(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_PARAM:
		ret = msm_ds2_dap_get_param(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_COMMAND:
		ret = msm_ds2_dap_handle_commands(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_DAP_LICENSE:
		ret = msm_ds2_dap_set_security_control(cmd, arg);
	break;
	case SNDRV_DEVDEP_DAP_IOCTL_GET_VISUALIZER:
		ret = msm_ds2_dap_param_visualizer_control_get(cmd, arg);
	break;
	default:
		pr_err("%s: called with invalid control 0x%x\n", __func__, cmd);
		ret = -EINVAL;
	}
	return ret;

}

int msm_ds2_dap_init(int port_id, int channels,
		     bool is_custom_stereo_on)
{
	int ret = 0, idx = 0;
	struct dolby_param_data dolby_data;

	pr_debug("%s: port id  %d\n", __func__, port_id);
	if (port_id != DOLBY_INVALID_PORT_ID) {
		idx = msm_ds2_get_device_index_from_port_id(port_id);
		if (idx < 0) {
			pr_err("%s: invalid index for port %d\n",
				__func__, port_id);
			ret = -EINVAL;
			goto end;
		}
		dev_map[idx].stream_ref_count++;
		dev_map[idx].active = true;
		dolby_data.param_id = DOLBY_COMMIT_ALL_TO_DSP;
		dolby_data.length = 0;
		dolby_data.data = NULL;
		dolby_data.device_id = dev_map[idx].device_id;
		pr_debug("%s:  idx  %d, active %d, dev id 0x%x\n",
			 __func__, idx, dev_map[idx].active,
			 dev_map[idx].device_id);
		ret  = msm_ds2_commit_params(&dolby_data, 0);
		if (ret < 0) {
			pr_err("%s: commit params ret %d\n", __func__, ret);
			ret = -EINVAL;
			goto end;
		}
		if (is_custom_stereo_on)
			msm_ds2_dap_set_custom_stereo_onoff(idx,
						is_custom_stereo_on);
	}

end:
	return ret;
}

void msm_ds2_dap_deinit(int port_id)
{
	/*
	 * Get the active port corrresponding to the active device
	 * Check if this is same as incoming port
	 * Set it to invalid
	 */
	int idx = -1;
	pr_debug("%s: port_id %d\n", __func__, port_id);
	if (port_id != DOLBY_INVALID_PORT_ID) {
		idx = msm_ds2_get_device_index_from_port_id(port_id);
		if (idx < 0) {
			pr_err("%s: invalid index for port %d\n",
				__func__, port_id);
			return;
		}
		dev_map[idx].stream_ref_count--;
		if (!dev_map[idx].stream_ref_count)
			dev_map[idx].active = false;
		pr_debug("%s:idx  %d, active %d, dev id 0x%x\n", __func__,
			 idx, dev_map[idx].active, dev_map[idx].device_id);
	}
}

int msm_ds2_dap_set_custom_stereo_onoff(int dev_map_idx,
					bool is_custom_stereo_enabled)
{
	char *params_value = NULL;
	int32_t *update_params_value, rc = 0;
	uint32_t params_length = (TOTAL_LENGTH_DOLBY_PARAM +
				DOLBY_PARAM_PAYLOAD_SIZE) *
				sizeof(uint32_t);
	pr_debug("%s\n", __func__);

	if (dev_map_idx < 0 || dev_map_idx >= NUM_DS2_ENDP_DEVICE) {
		pr_err("%s: invalid dev map index %d\n", __func__, dev_map_idx);
		rc = -EINVAL;
		goto end;
	}

	if (dev_map[dev_map_idx].port_id == DOLBY_INVALID_PORT_ID) {
		rc = -EINVAL;
		goto end;
	}

	msm_ds2_dap_set_vspe_vdhe(dev_map_idx,
				  is_custom_stereo_enabled);
	params_value = kzalloc(params_length, GFP_KERNEL);
	if (!params_value) {
		pr_err("%s: params memory alloc failed\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	update_params_value = (int32_t *)params_value;
	params_length = 0;
	*update_params_value++ = DOLBY_BUNDLE_MODULE_ID;
	*update_params_value++ = DOLBY_ENABLE_CUSTOM_STEREO;
	*update_params_value++ = sizeof(uint32_t);
	if (is_custom_stereo_enabled)
		*update_params_value++ = 1;
	else
		*update_params_value++ = 0;
	params_length += (DOLBY_PARAM_PAYLOAD_SIZE + 1) * sizeof(uint32_t);
	pr_debug("%s: valid param length: %d\n", __func__, params_length);
	if (params_length) {
		rc = adm_dolby_dap_send_params(dev_map[dev_map_idx].port_id,
					       params_value, params_length);
		if (rc) {
			pr_err("%s: custom stereo param failed with rc=%d\n",
				__func__, rc);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	kfree(params_value);
	return rc;
}

#else

static bool check_is_param_modified(int32_t *dap_params_modified,
				    int32_t idx, int32_t commit)
{
	return false;
}


static int map_device_to_dolby_cache_devices(int32_t device_id)
{
	return 0;
}

static int msm_ds2_update_num_devices(struct dolby_param_data *dolby_data,
				      int32_t *num_device, int32_t *dev_arr,
				      int32_t array_size)
{
	return 0;
}

static int msm_ds2_commit_params(struct dolby_param_data *dolby_data,
				 int commit)
{
	return 0;
}

static int msm_ds2_dap_handle_commands(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_set_param(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_get_param(u32 cmd, void *arg)
{
	return 0;
}

static int msm_ds2_dap_send_end_point(int dev_map_idx, int endp_idx)
{
	return 0;
}

static int msm_ds2_dap_send_cached_params(int dev_map_idx,
					  int commit)
{
	return 0;
}

static int msm_ds2_dap_set_vspe_vdhe(int dev_map_idx,
				     bool is_custom_stereo_enabled)
{
	return 0;
}

static int msm_ds2_dap_param_visualizer_control_get(
			u32 cmd, void *arg,
			struct msm_pcm_routing_bdai_data *bedais)
{
	return 0;
}

static int msm_ds2_dap_set_security_control(u32 cmd, void *arg)
{
	return 0
}
static int msm_update_dev_map_port_id(int32_t device_id, int port_id)
{
	return 0;
}
static int32_t msm_ds2_dap_get_port_id(
		int32_t device_id, int32_t be_id)
{
	return 0;
}
#endif /*CONFIG_DOLBY_DS2*/
