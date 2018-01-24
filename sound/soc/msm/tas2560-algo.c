/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_SND_SOC_TAS2560
/* #define DEBUG */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <sound/soc.h>
#include <sound/core.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#include <sound/q6core.h>
#include <sound/tas2560_algo.h>

static struct mutex routing_lock;
/*Port IDs to be read from dts*/
static uint32_t tx_port_id = TAS2560_ALGO_TX_PORT_ID;
static uint32_t rx_port_id = TAS2560_ALGO_RX_PORT_ID;
/*Not much significance and can be removed in future*/
static uint8_t calib_status;
static int32_t force_algo_disable;

/*Pointer to hold the data recieved from DSP
* TODO:Decide wether it needs memory or pointer is sufficient
*/
struct afe_tas2560_algo_params_t *tas2560_algo_payload = NULL;

/*Call back to recieve data from DSP*/
static int32_t tas2560_algo_afe_callback(struct apr_client_data *data)
{
	if (!data) {
		pr_err("TAS2560_ALGO:%s Invalid param data\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2) {
		uint32_t *payload32 = data->payload;

		if ((payload32[1] == AFE_TAS2560_ALGO_MODULE_RX) || (payload32[1] == AFE_TAS2560_ALGO_MODULE_TX)) {
			tas2560_algo_payload = (struct afe_tas2560_algo_params_t *) &payload32[4];
		}
	}
	return 0;
}

/*AFE set param to DSP*/
static int tas2560_algo_afe_set_param(uint32_t param_id, uint32_t module_id, struct afe_tas2560_algo_set_config_t *arg1)
{
	int result = 0;
	int index = 0;
	int size = 0;
	uint32_t port_id = rx_port_id;
	struct afe_tas2560_algo_set_config_t *config = NULL;
	struct afe_tas2560_algo_params_t *data_payload = NULL;

	if (module_id == AFE_TAS2560_ALGO_MODULE_TX)
		port_id = tx_port_id;

	if ((q6audio_validate_port(port_id) < 0)) {
		pr_err("TAS2560_ALGO:%s invalid port %d", __func__, port_id);
		return -EINVAL;
	}

	index = afe_get_port_index(port_id);

	/* Allocate memory for the message */
	size = sizeof(struct afe_tas2560_algo_set_config_t) +
		sizeof(struct afe_tas2560_algo_params_t);
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("TAS2560_ALGO:%s Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}
	data_payload = (struct afe_tas2560_algo_params_t *)
		((u8 *)config + sizeof(struct afe_tas2560_algo_set_config_t));

	/* Configure actual parameter settings */
	if (arg1)
		memcpy(data_payload->payload, arg1, sizeof(struct afe_tas2560_algo_params_t));
	else
		pr_err("TAS2560_ALGO:%s arg1 Null pointer recieved\n", __func__);

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	config->hdr.pkt_size = size;
	config->hdr.src_svc = APR_SVC_AFE;
	config->hdr.src_domain = APR_DOMAIN_APPS;
	config->hdr.src_port = 0;
	config->hdr.dest_svc = APR_SVC_AFE;
	config->hdr.dest_domain = APR_DOMAIN_ADSP;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;

	/* Set param section */
	config->param.port_id = port_id;
	config->param.payload_size =
		sizeof(struct afe_port_param_data_v2) +
		sizeof(struct afe_tas2560_algo_params_t);
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;

	/* Set data section */
	config->data.module_id = module_id;
	config->data.param_id = param_id;
	config->data.param_size = sizeof(struct afe_tas2560_algo_params_t);
	config->data.reserved = 0;

	result = tas2560_algo_afe_apr_send_pkt(config, index);
	if (result) {
		pr_debug("TAS2560_ALGO:%s set_param for port %d failed with code %d\n",
			 __func__, port_id, result);
	} else {
		pr_debug("TAS2560_ALGO:%s set_param packet param 0x%08X to module 0x%08X\n",
			__func__, param_id, module_id);
	}
	kfree(config);
	return result;
}

/*AFE get param to DSP*/
static int tas2560_algo_afe_get_param(uint32_t param_id, uint32_t module_id)
{
	int result = 0;
	int index = 0;
	int size = 0;
	uint32_t port_id = rx_port_id;
	struct afe_tas2560_algo_get_config_t *config = NULL;

	if (module_id == AFE_TAS2560_ALGO_MODULE_TX)
		port_id = tx_port_id;

	if ((q6audio_validate_port(port_id) < 0)) {
		pr_err("TAS2560_ALGO:%s invalid port %d", __func__, port_id);
		return -EINVAL;
	}

	index = afe_get_port_index(port_id);

	/* Allocate memory for the message */
	size = sizeof(struct afe_tas2560_algo_get_config_t);
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("TAS2560_ALGO:%s Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
						APR_PKT_VER);
	config->hdr.pkt_size = size;
	config->hdr.src_svc = APR_SVC_AFE;
	config->hdr.src_domain = APR_DOMAIN_APPS;
	config->hdr.src_port = 0;
	config->hdr.dest_svc = APR_SVC_AFE;
	config->hdr.dest_domain = APR_DOMAIN_ADSP;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_GET_PARAM_V2;

	/* Set param section */
	config->param.port_id = port_id;
	config->param.payload_size =
		sizeof(struct afe_port_param_data_v2);
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;
	config->param.module_id = module_id;
	config->param.param_id = param_id;

	/* Set data section */
	config->data.module_id = module_id;
	config->data.param_id = param_id;
	config->data.reserved = 0;

	/* payload and param size section */
	config->param.payload_size +=
		sizeof(struct afe_tas2560_algo_params_t);
	config->data.param_size =
		sizeof(struct afe_tas2560_algo_params_t);

	result = tas2560_algo_afe_apr_send_pkt(config, index);
	if (result) {
		pr_debug("TAS2560_ALGO:%s get_param for port %d failed with code %d\n",
			__func__, port_id, result);
	} else {
		pr_debug("TAS2560_ALGO:%s get_param packet param 0x%08X to module 0x%08X\n",
			__func__, param_id, module_id);
	}
	kfree(config);
	return result;
}

/*Wrapper to handle get set params*/
static int tas2560_algo_get_set(u8 *user_data, uint32_t param_id, uint32_t module_id,
			uint8_t get_set, int32_t length)
{
	int32_t  ret = 0;

	switch (get_set) {
	case TAS2560_ALGO_SET_PARAM:
		pr_debug("TAS2560_ALGO:%s set param\n", __func__);
		ret = tas2560_algo_afe_set_param(param_id, module_id,
			(struct afe_tas2560_algo_set_config_t *)user_data);
		break;
	case TAS2560_ALGO_GET_PARAM:
		pr_debug("TAS2560_ALGO:%s get param\n", __func__);
		tas2560_algo_payload = NULL;
		tas2560_algo_afe_set_callback(tas2560_algo_afe_callback);
		ret = tas2560_algo_afe_get_param(param_id, module_id);
		if ((user_data) && (tas2560_algo_payload))
			memcpy(user_data , tas2560_algo_payload, TAS2560_ALGO_MIN(length, sizeof(tas2560_algo_payload)));
		else
			pr_err("TAS2560_ALGO:%s GetParam Failed tas2560_algo_payload or user_data is NULL\n", __func__);
		break;
	default:
		pr_err("TAS2560_ALGO:%s Invalid case\n", __func__);
		ret = -EINVAL;
		break;
	}
	return ret;
}
/*Wrapper to control(with mutex) the get set params*/
int afe_tas2560_algo_ctrl(u8 *data, u32 param_id, u32 module_id, u8 dir, u8 size)
{
	int32_t ret = 0;

	mutex_lock(&routing_lock);
	ret = tas2560_algo_get_set(data, param_id, module_id, dir, size);
	mutex_unlock(&routing_lock);

	return ret;
}
/* FB Info */

/*Control:1 Start/Stop commands for Rdc-Calibration/F0-Q-test */
static const char *tas2560_algo_calib_text[] = {
	"NONE",
	"CALIB_START_RDC",
	"CALIB_STOP_RDC",
	"CALIB_START_F0_Q",
	"CALIB_STOP_F0_Q"
};

static const struct soc_enum tas2560_algo_calib_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_calib_text), tas2560_algo_calib_text)
};
/*Can be ignored*/
static int tas2560_algo_calib_get (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = calib_status;
	pr_debug("TAS2560_ALGO:%s data %d\n", __func__, calib_status);
	return 0;
}

static int tas2560_algo_calib_set (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;
	((int32_t *)buff)[0] = 1;
	calib_status = ucontrol->value.integer.value[0];
	switch (calib_status) {
	case 0:
		pr_err("TAS2560_ALGO:%s NONE\n", __func__);
		break;
	case 1:
		paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_CALIB_START_RDC, 1, SLAVE1);
		pr_err("TAS2560_ALGO:%s CALIB_START_RDC paramid 0x%x\n", __func__, paramid);
		break;
	case 2:
		paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_CALIB_STOP_RDC, 1, SLAVE1);
		pr_err("TAS2560_ALGO:%s CALIB_STOP_RDC paramid 0x%x\n", __func__, paramid);
		break;
	case 3:
		pr_err("TAS2560_ALGO:%s CALIB_START_F0_Q Not Implemented \n", __func__);
		break;
	case 4:
		pr_err("TAS2560_ALGO:%s CALIB_STOP_F0_Q Not Implemented \n", __func__);
		break;
	default:
		pr_err("TAS2560_ALGO:%s Invalid Case 0x%x\n", __func__, paramid);
		ret = -EINVAL;
		break;
	}

	if (paramid)
		ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));
	return ret;
}

/*Control:2 Command to get Rdc result after Rdc-Calibration*/
static int tas2560_algo_get_re (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	int32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_GET_RDC, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];

	pr_debug("TAS2560_ALGO:%s Rdc 0x%x\n", __func__, (unsigned int)ucontrol->value.integer.value[0]);
	return ret;
}

/*Control:3 Command to get F0 result after F0-Q-test*/
static int tas2560_algo_get_f0 (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_GET_F0, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];

	pr_debug("TAS2560_ALGO:%s F0 0x%x\n", __func__, (unsigned int)ucontrol->value.integer.value[0]);
	return ret;
}

/*Control:4 Command to get Q result after F0-Q-test*/
static int tas2560_algo_get_q (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_GET_Q, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];

	pr_debug("TAS2560_ALGO:%s Q 0x%x\n", __func__, (unsigned int)ucontrol->value.integer.value[0]);
	return ret;
}

/*Control:5 Command to bypass algorithm dynaically*/
static const char *tas2560_algo_text[] = {
	"DISABLE",
	"ENABLE"
};

static const struct soc_enum tas2560_algo_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_text), tas2560_algo_text)
};


static int tas2560_algo_get_status (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid =  AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;
	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_CFG_ENABLE, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];
	pr_debug("TAS2560_ALGO:%s status data %d", __func__, ((int32_t *)buff)[0]);
	return ret;
}

static int tas2560_algo_set_status (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid =  AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];
	pr_err("TAS2560_ALGO:%s status data %d", __func__, ((int32_t *)buff)[0]);
	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_CFG_ENABLE, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));
	return ret;
}

/*Control:6 Command to set calibrated Rdc*/
static int tas2560_algo_set_cal(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_TX;
	u8 buff[56] = {0};
	u8 *ptr = buff;
	u8 re_count = 0, i = 0;

	/*If algo is forced to be disable, we do nothing here*/
	if (force_algo_disable) {
		pr_err("TAS2560_ALGO,%s,the algo is force disabled,%d\n",
			__func__, force_algo_disable);
		return ret;
	}

	/*Count is ignored as it is mono*/
	pr_debug("TAS2560_ALGO:%s [0] = %ld [1] = %ld\n", __func__, ucontrol->value.integer.value[0], ucontrol->value.integer.value[1]);
	re_count = ucontrol->value.integer.value[0];

	/*Sending TX parameters*/
	/*TODO:Values of left-I,right-V or left-V,right-I can be made generic by reading from dts file*/
	pr_debug("TAS2560_ALGO:%s Sending Tx-FB info", __func__);
	ret = afe_spk_prot_feed_back_cfg(tx_port_id, rx_port_id,
		1, 0, 1);

	if (!ret) {
		pr_debug("TAS2560_ALGO:%s Sending Tx-Enable ", __func__);
		paramid = AFE_PARAM_ID_TAS2560_ALGO_TX_ENABLE;
		((int32_t *)buff)[0] = 1;
		ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));
	} else
		pr_err("TAS2560_ALGO:%s Sending Tx-Enable already enabled or port running", __func__);

	/*Sending RX parameters*/
	moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	pr_debug("TAS2560_ALGO:%s Sending Rx-Enable data", __func__);
	paramid = AFE_PARAM_ID_TAS2560_ALGO_RX_ENABLE;
	((int32_t *)buff)[0] = 1;
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));

	if (!ret) {
		pr_debug("TAS2560_ALGO:%s Sending Rx-Cfg", __func__);
		paramid = AFE_PARAM_ID_TAS2560_ALGO_RX_CFG;
		((int32_t *)buff)[0] = 1;
		afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));

		pr_debug("TAS2560_ALGO:%s Sending Rdc", __func__);
		/*TODO:Note multi channel send Rdc is not verified*/
		for (i = 0; i < re_count; i++) {
			paramid = TAS2560_ALGO_CALC_PIDX(0, TAS2560_ALGO_CFG_SET_RE, 1, GET_SLAVE(i+1));
			((int32_t *)buff)[0] = ucontrol->value.integer.value[i+1];
			ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));
		}
	} else
		pr_err("TAS2560_ALGO:%s Sending Rx-Enable failed", __func__);
	return ret;
}

/*Control:7 Command to Enable FF module*/
static const char *tas2560_algo_ff_module_text[] = {
	"DISABLE",
	"ENABLE"
};

static const struct soc_enum tas2560_algo_ff_module_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_ff_module_text), tas2560_algo_ff_module_text)
};


static int tas2560_algo_get_ff_module (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	int32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = AFE_PARAM_ID_TAS2560_ALGO_RX_ENABLE;
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];
	pr_debug("TAS2560_ALGO:%s Recieving Rx-Enable data %d", __func__, ((int32_t *)buff)[0]);
	return ret;
}

static int tas2560_algo_set_ff_module (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];
	pr_debug("TAS2560_ALGO:%s Sending Rx-Enable data %d", __func__, ((int32_t *)buff)[0]);

	if ((force_algo_disable) && (((int32_t *)buff)[0])) {
		pr_err("TAS2560_ALGO:%s,try to enable ff while force disabled",
			__func__);
		return ret;
	}

	paramid = AFE_PARAM_ID_TAS2560_ALGO_RX_ENABLE;
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));

	return ret;
}
/*Control:8 Command to Enable FB module*/
static const char *tas2560_algo_fb_module_text[] = {
	"DISABLE",
	"ENABLE"
};

static const struct soc_enum tas2560_algo_fb_module_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_fb_module_text), tas2560_algo_fb_module_text)
};


static int tas2560_algo_get_fb_module (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	int32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_TX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = AFE_PARAM_ID_TAS2560_ALGO_TX_ENABLE;
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];
	pr_debug("TAS2560_ALGO:%s Recieving Tx-Enable data %d", __func__, ((int32_t *)buff)[0]);
	return ret;
}

static int tas2560_algo_set_fb_module (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_TX;
	u8 buff[56] = {0};
	u8 *ptr = buff;
	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];

	pr_debug("TAS2560_ALGO:%s Sending Tx-Enable data %d", __func__, ((int32_t *)buff)[0]);

	if ((force_algo_disable) && (((int32_t *)buff)[0])) {
		pr_err("TAS2560_ALGO:%s,try to enable fb while force disabled",
			__func__);
		return ret;
	}

	paramid = AFE_PARAM_ID_TAS2560_ALGO_TX_ENABLE;
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));

	return ret;
}
/*Control:9 Command to Set Profile*/
/*TODO:Profile count can be extended, no implementation restrictions*/
static const char *tas2560_algo_profile_text[] = {
	"NORMAL",
	"VOICE",
	"RING",
	"SONIFICATION",
	"CALIBRATION"
};

static const struct soc_enum tas2560_algo_profile_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_profile_text), tas2560_algo_profile_text)
};


static int tas2560_algo_get_profile (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	int32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_PROFILE, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];
	pr_debug("TAS2560_ALGO:%s %ld \n", __func__, ucontrol->value.integer.value[0]);

	return ret;
}

static int tas2560_algo_set_profile (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0};
	u8 *ptr = buff;

	((int32_t *)buff)[0] = ucontrol->value.integer.value[0];
	pr_debug("TAS2560_ALGO:%s %ld \n", __func__, ucontrol->value.integer.value[0]);
	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_PROFILE, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_SET_PARAM, sizeof(u32));
	return ret;
}

/*Control:10 Command to disable TAS2560 ALGO to be enabled by audio hal,
used only for factory mode*/
static const char * const tas2560_algo_disable_text[] = {
	"FALSE",
	"TRUE"
};

static const struct soc_enum tas2560_algo_disable_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_algo_disable_text),
		tas2560_algo_disable_text)
};


static int tas2560_algo_get_disable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = force_algo_disable;
	pr_debug("TAS2560_ALGO:%s force_algo_disable=%d",
		__func__, force_algo_disable);
	return 0;
}

static int tas2560_algo_set_disable(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	force_algo_disable = ucontrol->value.integer.value[0];
	pr_err("TAS2560_ALGO:%s force_algo_disable=%d",
		__func__, force_algo_disable);
	return 0;
}

static int tas2560_algo_get_tv (struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t ret = 0;
	uint32_t paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u8 buff[56] = {0}, *ptr = buff;

	paramid = TAS2560_ALGO_CALC_PIDX(paramid, TAS2560_ALGO_GET_TV, 1, SLAVE1);
	ret = afe_tas2560_algo_ctrl(ptr, paramid, moduleid, TAS2560_ALGO_GET_PARAM, sizeof(u32));
	ucontrol->value.integer.value[0] = ((int32_t *)buff)[0];

	pr_debug("TAS2560_ALGO:%s TV 0x%x\n", __func__, (unsigned int)ucontrol->value.integer.value[0]);
	return ret;
}

const struct snd_kcontrol_new tas2560_algo_filter_mixer_controls[] = {
	SOC_ENUM_EXT("TAS2560_ALGO_CALIB", tas2560_algo_calib_enum[0], tas2560_algo_calib_get, tas2560_algo_calib_set),
	SOC_SINGLE_EXT("TAS2560_ALGO_GET_RE", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0, tas2560_algo_get_re, NULL),
	SOC_SINGLE_EXT("TAS2560_ALGO_GET_F0", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0, tas2560_algo_get_f0, NULL),
	SOC_SINGLE_EXT("TAS2560_ALGO_GET_Q", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0, tas2560_algo_get_q, NULL),
	SOC_SINGLE_EXT("TAS2560_ALGO_GET_TV", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0, tas2560_algo_get_tv, NULL),
	SOC_SINGLE_MULTI_EXT("TAS2560_ALGO_CMD_SEND_CAL", SND_SOC_NOPM, 0, 0x7FFFFFFF, 0, 5, NULL, tas2560_algo_set_cal),
	SOC_ENUM_EXT("TAS2560_ALGO_STATUS", tas2560_algo_enum[0], tas2560_algo_get_status, tas2560_algo_set_status),
	SOC_ENUM_EXT("TAS2560_ALGO_FF_MODULE", tas2560_algo_ff_module_enum[0], tas2560_algo_get_ff_module, tas2560_algo_set_ff_module),
	SOC_ENUM_EXT("TAS2560_ALGO_FB_MODULE", tas2560_algo_fb_module_enum[0], tas2560_algo_get_fb_module, tas2560_algo_set_fb_module),
	SOC_ENUM_EXT("TAS2560_ALGO_PROFILE", tas2560_algo_profile_enum[0],
		tas2560_algo_get_profile, tas2560_algo_set_profile),
	SOC_ENUM_EXT("TAS2560_ALGO_DISABLE", tas2560_algo_disable_enum[0],
		tas2560_algo_get_disable, tas2560_algo_set_disable)
};

/*
* tas2560_algo_routing_init
* - register mixer controls
*/

int tas2560_algo_routing_init(struct snd_soc_pcm_runtime *rtd)
{
     struct snd_soc_codec *codec = rtd->codec;

     /* add the codec controls here */
     mutex_init(&routing_lock);

     snd_soc_add_codec_controls(codec, tas2560_algo_filter_mixer_controls,
			ARRAY_SIZE(tas2560_algo_filter_mixer_controls));

     return 0;
}


static int tas2560_algo_probe(struct platform_device *pdev)
{
     int ret = 0;
     uint32_t num_entries = 0;
     uint32_t *val_array = NULL;

     pr_debug("TAS2560_ALGO:%s Enter.\n", __func__);

     mutex_init(&routing_lock);

     ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

     num_entries = of_property_count_u32_elems(pdev->dev.of_node,
			"ti,tas2560-port-id");
     if (num_entries > 0) {
	val_array = kcalloc(num_entries, sizeof(uint32_t), GFP_KERNEL);
	if (!val_array) {
		pr_err("TAS2560_ALGO:%s malloc fail \n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
		"ti,tas2560-port-id", val_array, num_entries);
	rx_port_id = (uint32_t)val_array[0];
	tx_port_id = (uint32_t)val_array[1];
	pr_err("TAS2560_ALGO:%s rx_port_id = 0x%x tx_port_id = 0x%x\n", __func__, rx_port_id, tx_port_id);
    }
    kfree(val_array);
err:
    return ret;
}

static int tas2560_algo_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id tas2560_algo_of_match[] = {
		{ .compatible = "ti,tas2560-algo" },
		{  },
};
MODULE_DEVICE_TABLE(of, tas2560_algo_of_match);

static struct platform_driver tas2560_algo_driver = {
		.probe             = tas2560_algo_probe,
		.remove            = tas2560_algo_remove,
		.driver         = {
			.name   = "tas2560-algo",
			.of_match_table      = tas2560_algo_of_match,
		},
};

static int __init tas2560_algo_init(void)
{
	return platform_driver_register(&tas2560_algo_driver);
}
module_init(tas2560_algo_init);

static void __exit tas2560_algo_exit(void)
{
	platform_driver_unregister(&tas2560_algo_driver);
}
module_exit(tas2560_algo_exit);


MODULE_DESCRIPTION("TAS2560 ALSA SOC Smart Amplifier algo driver");
MODULE_LICENSE("GPL v2");
#endif /* CONFIG_SND_SOC_TAS2560 */

