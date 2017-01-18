/* Copyright (c) 2015-2017, Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <sound/soc.h>
#include <sound/core.h>
#include <sound/q6afe-v2.h>
#include <sound/ospl2xx.h>

/*
 * QDSP internal tune index and port config reading
 */
#define MAX_TUNE_COUNT 4
static uint32_t ospl_tune_index[MAX_TUNE_COUNT] = {0, 1, 2, 3};
static uint32_t AFE_RX_PORT_ID = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_RX;
static uint32_t AFE_TX_PORT_ID = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_1_TX;
static bool feedback_on_left;
static bool feedback_on_right;
static bool ospl_volume_control;

/*
 * external configuration string reading
 */
static char const *ospl2xx_ext_config_tables[] = {
	"opalum.rx.ext.config.0",
	"opalum.rx.ext.config.1",
	"opalum.rx.ext.config.2",
	"opalum.rx.ext.config.3",
	"opalum.tx.ext.config.0",
	"opalum.tx.ext.config.1",
	"opalum.tx.ext.config.2",
	"opalum.tx.ext.config.3",
};

static int ext_config_loaded;
static const struct firmware
		*ospl2xx_config[ARRAY_SIZE(ospl2xx_ext_config_tables)];

static struct workqueue_struct *ospl2xx_wq;
static struct work_struct ospl2xx_init_work;
static struct platform_device *spdev;

static DEFINE_MUTEX(lr_lock);
static void ospl2xx_load_config(struct work_struct *work)
{
	int i, ret;

	mutex_lock(&lr_lock);
	for (i = 0; i < ARRAY_SIZE(ospl2xx_ext_config_tables); i++) {
		ret = request_firmware(&ospl2xx_config[i],
			ospl2xx_ext_config_tables[i], &spdev->dev);
		if (ret ||
		    ospl2xx_config[i]->data == NULL ||
		    ospl2xx_config[i]->size <= 0) {
			pr_debug("failed to load config[%d], ret=%d\n", i, ret);
		} else {
			pr_debug("loading external configuration %s\n",
				ospl2xx_ext_config_tables[i]);
			ext_config_loaded = 1;
		}
	}
	mutex_unlock(&lr_lock);
}

/*
 * OSPL Hexagon AFE communication
 */
int ospl2xx_afe_set_single_param(uint32_t param_id, int32_t arg1)
{
	int result = 0;
	int index = 0;
	int size = 0;
	uint32_t module_id = 0, port_id = 0;
	struct afe_custom_opalum_set_config_t *config = NULL;
	struct opalum_single_data_ctrl_t *settings = NULL;

	/* Destination settings for message */
	if ((param_id >> 8) << 8 == AFE_CUSTOM_OPALUM_RX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_RX_MODULE;
		port_id = AFE_RX_PORT_ID;
	} else if ((param_id >> 8) << 8 == AFE_CUSTOM_OPALUM_TX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_TX_MODULE;
		port_id = AFE_TX_PORT_ID;
	} else {
		pr_err("%s: unsupported paramID[0x%08X]\n", __func__, param_id);
		return -EINVAL;
	}

	index = afe_get_port_index(port_id);

	/* Allocate memory for the message */
	size = sizeof(struct afe_custom_opalum_set_config_t) +
		sizeof(struct opalum_single_data_ctrl_t);
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}
	settings = (struct opalum_single_data_ctrl_t *)
		((u8 *)config + sizeof(struct afe_custom_opalum_set_config_t));

	/* Configure actual parameter settings */
	settings->value = arg1;

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
		sizeof(struct opalum_single_data_ctrl_t);
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;

	/* Set data section */
	config->data.module_id = module_id;
	config->data.param_id = param_id;
	config->data.param_size = sizeof(struct opalum_single_data_ctrl_t);
	config->data.reserved = 0;

	result = ospl2xx_afe_apr_send_pkt(config, index);
	if (result) {
		pr_debug("%s: set_param for port %d failed with code %d\n",
			 __func__, port_id, result);
	} else {
		pr_debug("%s: set_param packet param 0x%08X to module 0x%08X\n",
			__func__, param_id, module_id);
	}
	kfree(config);
	return result;
}

int ospl2xx_afe_set_tri_param(uint32_t param_id,
			int32_t arg1, int32_t arg2, int32_t arg3) {
	int result = 0;
	int index = 0;
	int size = 0;
	uint32_t port_id = 0, module_id = 0;
	struct afe_custom_opalum_set_config_t *config = NULL;
	struct opalum_tri_data_ctrl_t *settings = NULL;

	/* Destination settings for message */
	if ((param_id >> 8) << 8 ==
		AFE_CUSTOM_OPALUM_RX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_RX_MODULE;
		port_id = AFE_RX_PORT_ID;
	} else if ((param_id >> 8) << 8 ==
		AFE_CUSTOM_OPALUM_TX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_TX_MODULE;
		port_id = AFE_TX_PORT_ID;
	} else {
		pr_err("%s: unsupported paramID[0x%08X]\n",
			__func__, param_id);
		return -EINVAL;
	}
	index = afe_get_port_index(port_id);

	/* Allocate memory for the message */
	size = sizeof(struct afe_custom_opalum_set_config_t) +
		sizeof(struct opalum_tri_data_ctrl_t);
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}
	settings = (struct opalum_tri_data_ctrl_t *)
		((u8 *)config + sizeof(struct afe_custom_opalum_set_config_t));

	/* Configure actual parameter settings */
	settings->data1 = arg1;
	settings->data2 = arg2;
	settings->data3 = arg3;

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
		sizeof(struct opalum_tri_data_ctrl_t);
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;

	/* Set data section */
	config->data.module_id = module_id;
	config->data.param_id = param_id;
	config->data.param_size = sizeof(struct opalum_tri_data_ctrl_t);
	config->data.reserved = 0;
	result = ospl2xx_afe_apr_send_pkt(config, index);
	if (result) {
		pr_debug("%s: set_param for port %d failed with code %d\n",
			__func__, port_id, result);
	} else {
		pr_debug("%s: set_param packet param 0x%08X to module 0x%08X\n",
			__func__, param_id, module_id);
	}
	kfree(config);
	return result;
}

int ospl2xx_afe_set_ext_config_param(const char *string, uint32_t param_id)
{
	int result = 0;
	int index = 0;
	int size = 0;
	int string_size = 0;
	int mem_size = 0;
	int sent = 0;
	int chars_to_send = 0;
	uint32_t port_id = 0, module_id = 0;
	struct afe_custom_opalum_set_config_t *config = NULL;
	struct opalum_external_config_t *payload = NULL;

	/* Destination settings for message */
	if ((param_id >> 8) << 8 ==
		AFE_CUSTOM_OPALUM_RX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_RX_MODULE;
		port_id = AFE_RX_PORT_ID;
	} else if ((param_id >> 8) << 8 ==
		AFE_CUSTOM_OPALUM_TX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_TX_MODULE;
		port_id = AFE_TX_PORT_ID;
	} else {
		pr_err("%s: unsupported paramID[0x%08X]\n",
			__func__, param_id);
		return -EINVAL;
	}
	index = afe_get_port_index(port_id);

	string_size = strlen(string);
	if (string_size > 4000)
		mem_size = 4000;
	else
		mem_size = string_size;

	/* Allocate memory for the message */
	size = sizeof(struct afe_custom_opalum_set_config_t) +
		sizeof(struct opalum_external_config_t) + mem_size;
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}
	payload = (struct opalum_external_config_t *)
		((u8 *)config + sizeof(struct afe_custom_opalum_set_config_t));
	payload->total_size = (uint32_t)string_size;

	/* Initial configuration of message */
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
		sizeof(struct opalum_external_config_t) +
		mem_size;
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;

	/* Set data section */
	config->data.module_id = module_id;
	config->data.param_id = param_id;
	config->data.param_size =
		sizeof(struct opalum_external_config_t) +
		mem_size;
	config->data.reserved = 0;

	/* Send config string in chunks of maximum 4000 bytes */
	while (sent < string_size) {
		chars_to_send = string_size - sent;
		if (chars_to_send > 4000) {
			chars_to_send = 4000;
			payload->done = 0;
		} else {
			payload->done = 1;
		}

		/* Configure per message parameter settings */
		memcpy(&payload->config, string + sent, chars_to_send);
		payload->chunk_size = chars_to_send;

		/* Send the actual message */
		result = ospl2xx_afe_apr_send_pkt(config, index);
		if (result) {
			pr_debug("%s: set_param for port %d failed, code %d\n",
				__func__, port_id, result);
		} else {
			pr_debug("%s: set_param param 0x%08X to module 0x%08X\n",
				 __func__, param_id, module_id);
		}
		sent += chars_to_send;
	}
	kfree(config);
	return result;
}

int ospl2xx_afe_get_param(uint32_t param_id)
{
	int result = 0;
	int index = 0;
	int size = 0;
	uint32_t module_id = 0, port_id = 0;
	struct afe_custom_opalum_get_config_t *config = NULL;

	if ((param_id >> 8) << 8 == AFE_CUSTOM_OPALUM_RX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_RX_MODULE;
		port_id = AFE_RX_PORT_ID;
	} else if ((param_id >> 8) << 8 == AFE_CUSTOM_OPALUM_TX_MODULE) {
		module_id = AFE_CUSTOM_OPALUM_TX_MODULE;
		port_id = AFE_TX_PORT_ID;
	} else {
		pr_err("%s: unsupported paramID[0x%08X]\n", __func__, param_id);
		return -EINVAL;
	}

	index = afe_get_port_index(port_id);

	/* Allocate memory for the message */
	size = sizeof(struct afe_custom_opalum_get_config_t);
	config = kzalloc(size, GFP_KERNEL);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
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
	switch (param_id) {
	case PARAM_ID_OPALUM_RX_ENABLE:
	case PARAM_ID_OPALUM_RX_EXC_MODEL:
	case PARAM_ID_OPLAUM_RX_TEMPERATURE:
	case PARAM_ID_OPALUM_RX_CURRENT_PARAM_SET:
	case PARAM_ID_OPALUM_RX_VOLUME_CONTROL:
	case PARAM_ID_OPALUM_TX_ENABLE:
	case PARAM_ID_OPALUM_TX_CURRENT_PARAM_SET:
		config->param.payload_size +=
			sizeof(struct opalum_single_data_ctrl_t);
		config->data.param_size =
			sizeof(struct opalum_single_data_ctrl_t);
		break;
	case PARAM_ID_OPALUM_TX_F0_CALIBRATION_VALUE:
	case PARAM_ID_OPALUM_TX_TEMP_MEASUREMENT_VALUE:
		config->param.payload_size +=
			sizeof(struct opalum_dual_data_ctrl_t);
		config->data.param_size =
			sizeof(struct opalum_dual_data_ctrl_t);
		break;
	case PARAM_ID_OPALUM_TX_F0_CURVE:
		config->param.payload_size +=
			sizeof(struct opalum_f0_curve_t);
		config->data.param_size =
			sizeof(struct opalum_f0_curve_t);
		break;
	}

	result = ospl2xx_afe_apr_send_pkt(config, index);
	if (result) {
		pr_debug("%s: get_param for port %d failed with code %d\n",
			__func__, port_id, result);
	} else {
		pr_debug("%s: get_param packet param 0x%08X to module 0x%08X\n",
			__func__, param_id, module_id);
	}

	kfree(config);
	return result;
}

#define NUM_RX_CONFIGS 4
static int ext_rxconfig;
static int ext_txconfig = NUM_RX_CONFIGS;
static int int_rxconfig;
static int int_txconfig;

static int ospl2xx_update_rx_ext_config(void)
{
	int i = ext_rxconfig, size = 0;
	char *config = NULL;

	if (ospl2xx_config[i] == NULL)
		goto error_return;

	size = ospl2xx_config[i]->size;
	if (size <= 0)
		goto error_return;

	config = kzalloc(sizeof(char) * (size+1), GFP_KERNEL);
	if (config == NULL || ospl2xx_config[i]->data == NULL)
		goto error_return;

	memcpy(config, ospl2xx_config[i]->data, size);
	config[size] = '\0';

	pr_debug("%s: ospl2xx_config[%d] size[%d]\n",
		__func__, i, size);

	ospl2xx_afe_set_ext_config_param(config,
		PARAM_ID_OPALUM_RX_SET_EXTERNAL_CONFIG);

	kfree(config);
	return 0;

error_return:
	kfree(config);
	return 0;

}

static int ospl2xx_update_tx_ext_config(void)
{
	int i = ext_txconfig, size = 0;
	char *config = NULL;

	if (ospl2xx_config[i] == NULL)
		goto error_return;

	size = ospl2xx_config[i]->size;
	if (size <= 0)
		goto error_return;

	config = kzalloc(sizeof(char) * (size+1), GFP_KERNEL);
	if (config == NULL || ospl2xx_config[i]->data == NULL)
		goto error_return;

	memcpy(config, ospl2xx_config[i]->data, size);
	config[size] = '\0';

	pr_debug("%s: ospl2xx_config[%d] size[%d]\n",
		__func__, i, size);

	ospl2xx_afe_set_ext_config_param(config,
		PARAM_ID_OPALUM_TX_SET_EXTERNAL_CONFIG);

	kfree(config);
	return 0;

error_return:
	kfree(config);
	return 0;
}

static int ospl2xx_update_rx_int_config(void)
{
	pr_debug("%s: set configuration index = %d, qdsp index = %d\n",
		__func__, int_rxconfig, ospl_tune_index[int_rxconfig]);

	ospl2xx_afe_set_single_param(
		PARAM_ID_OPALUM_RX_SET_USE_CASE,
		ospl_tune_index[int_rxconfig]);

	return 0;
}

static int ospl2xx_update_tx_int_config(void)
{
	pr_debug("%s: set configuration index = %d, qdsp index = %d\n",
		__func__, int_txconfig, ospl_tune_index[int_txconfig]);

	ospl2xx_afe_set_single_param(
		PARAM_ID_OPALUM_TX_SET_USE_CASE,
		ospl_tune_index[int_txconfig]);

	return 0;
}

/*
 * AFE callback
 */
int32_t afe_cb_payload32_data[2] = {0,};
int32_t *f0_curve;
int32_t f0_curve_size;
static int32_t ospl2xx_afe_callback(struct apr_client_data *data)
{
	if (!data) {
		pr_err("%s: Invalid param data\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == AFE_PORT_CMDRSP_GET_PARAM_V2) {
		uint32_t *payload32 = data->payload;

		if (payload32[1] == AFE_CUSTOM_OPALUM_RX_MODULE ||
		    payload32[1] == AFE_CUSTOM_OPALUM_TX_MODULE) {
			switch (payload32[2]) {
			case PARAM_ID_OPALUM_RX_ENABLE:
			case PARAM_ID_OPALUM_RX_EXC_MODEL:
			case PARAM_ID_OPLAUM_RX_TEMPERATURE:
			case PARAM_ID_OPALUM_RX_CURRENT_PARAM_SET:
			case PARAM_ID_OPALUM_RX_VOLUME_CONTROL:
			case PARAM_ID_OPALUM_TX_ENABLE:
			case PARAM_ID_OPALUM_TX_F0_CALIBRATION_VALUE:
			case PARAM_ID_OPALUM_TX_TEMP_MEASUREMENT_VALUE:
			case PARAM_ID_OPALUM_TX_CURRENT_PARAM_SET:
				afe_cb_payload32_data[0] = payload32[4];
				afe_cb_payload32_data[1] = payload32[5];
				pr_debug("%s, payload-module_id = 0x%08X, ",
					__func__, (int) payload32[1]);
				pr_debug("payload-param_id = 0x%08X, ",
					(int) payload32[2]);
				pr_debug("afe_cb_payload32_data[0] = %d, ",
					(int) afe_cb_payload32_data[0]);
				pr_debug("afe_cb_payload32_data[1] = %d\n",
					(int) afe_cb_payload32_data[1]);
				break;
			case PARAM_ID_OPALUM_TX_F0_CURVE:
				f0_curve_size = payload32[3];
				f0_curve = (int32_t *) &payload32[4];
				pr_debug("%s, payload-module_id = 0x%08X, ",
					__func__, (int) payload32[1]);
				pr_debug("payload-param_id = 0x%08X, ",
					(int) payload32[2]);
				pr_debug("f0_curve size = %d",
					(int) f0_curve_size);
				break;
			default:
				break;
			}
		}
	}

	return 0;
}

/*
 * mixer controls
 */
static DEFINE_MUTEX(mr_lock);
/* RX */
/* PARAM_ID_OPALUM_RX_ENABLE */
static int ospl2xx_rx_enable_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	if (enable != 0 && enable != 1)
		return 0;

	ospl2xx_afe_set_single_param(
		PARAM_ID_OPALUM_RX_ENABLE,
		enable);

	return 0;
}
static int ospl2xx_rx_enable_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_RX_ENABLE);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static const char *const ospl2xx_rx_enable_text[] = {"Disable", "Enable"};
static const struct soc_enum ospl2xx_rx_enable_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ospl2xx_rx_enable_text),
};

/* PARAM_ID_OPALUM_RX_SET_USE_CASE */
static int ospl2xx_rx_int_config_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_RX_CURRENT_PARAM_SET);

	ucontrol->value.integer.value[0] =
			afe_cb_payload32_data[0] - ospl_tune_index[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static int ospl2xx_rx_int_config_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, ret = 0;

	i = ucontrol->value.integer.value[0];
	if (i < 0 || i >= NUM_RX_CONFIGS)
		return -ERANGE;

	mutex_lock(&lr_lock);
	if (int_rxconfig != i) {
		int_rxconfig = i;
		ret = ospl2xx_update_rx_int_config();
	}
	mutex_unlock(&lr_lock);
	return ret;
}
static char const *ospl2xx_rx_int_config_index[] = {
	"opalum.rx.int.config.0",
	"opalum.rx.int.config.1",
	"opalum.rx.int.config.2",
	"opalum.rx.int.config.3",
};
static const struct soc_enum ospl2xx_rx_int_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(NUM_RX_CONFIGS, ospl2xx_rx_int_config_index),
};

/* PARAM_ID_OPALUM_RX_RUN_CALIBRATION */
static int ospl2xx_rx_run_diagnostic(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int command = ucontrol->value.integer.value[0];

	if (command != 1)
		return 0;

	ospl2xx_afe_set_single_param(PARAM_ID_OPALUM_RX_RUN_CALIBRATION, 1);

	return 0;
}
static int ospl2xx_rx_run_diagnostic_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	/* not supported */
	return 0;
}
static const char *const ospl2xx_rx_run_diagnostic_text[] = {"Off", "Run"};
static const struct soc_enum ospl2xx_rx_run_diagnostic_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ospl2xx_rx_run_diagnostic_text),
};

/* PARAM_ID_OPALUM_RX_SET_EXTERNAL_CONFIG */
static int ospl2xx_rx_ext_config_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ext_rxconfig;
	return 0;
}

static int ospl2xx_rx_ext_config_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, ret = 0;

	i = ucontrol->value.integer.value[0];
	if (i < 0 || i >= NUM_RX_CONFIGS)
		return -ERANGE;

	mutex_lock(&lr_lock);
	if (ext_rxconfig != i) {
		ext_rxconfig = i;
		ret = ospl2xx_update_rx_ext_config();
	}
	mutex_unlock(&lr_lock);
	return ret;
}

static char const *ospl2xx_rx_ext_config_text[] = {
	"opalum.rx.ext.config.0",
	"opalum.rx.ext.config.1",
	"opalum.rx.ext.config.2",
	"opalum.rx.ext.config.3",
};
static const struct soc_enum ospl2xx_rx_ext_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(NUM_RX_CONFIGS,
		ospl2xx_rx_ext_config_text),
};

static int ospl2xx_audio_mode_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	if (ext_config_loaded)
		ucontrol->value.integer.value[0] = ext_rxconfig;
	else
		ucontrol->value.integer.value[0] = int_rxconfig;
	return 0;
}

static int ospl2xx_audio_mode_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, ret = 0;

	i = ucontrol->value.integer.value[0];
	if (i < 0 || i >= NUM_RX_CONFIGS)
		return -ERANGE;

	mutex_lock(&lr_lock);
	if (ext_config_loaded) {
		if (ext_rxconfig != i) {
			ext_rxconfig = i;
			ext_txconfig = i + NUM_RX_CONFIGS;
			ret = ospl2xx_update_rx_ext_config();
			if (ret == 0)
				ret = ospl2xx_update_tx_ext_config();
		}
	} else {
		if (int_rxconfig != i) {
			int_rxconfig = i;
			int_txconfig = i;
			ospl2xx_update_rx_int_config();
			ospl2xx_update_tx_int_config();
		}
	}
	mutex_unlock(&lr_lock);
	return ret;
}
/*NOTE that audio mode name must align with RX configs above*/
static char const *ospl2xx_audio_mode_text[] = {
	"NORMAL",
	"VOICE",
	"RING",
	"SONIFICATION",
};
static const struct soc_enum ospl2xx_audio_mode_enum[] = {
	SOC_ENUM_SINGLE_EXT(NUM_RX_CONFIGS,
		ospl2xx_audio_mode_text),
};

/* PARAM_ID_OPALUM_RX_EXC_MODEL */
static int ospl2xx_rx_get_exc_model(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_RX_EXC_MODEL);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static int ospl2xx_rx_put_exc_model(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	/* not implemented yet */
	return 0;
}

/* PARAM_ID_OPLAUM_RX_TEMPERATURE */
static int ospl2xx_rx_get_temperature(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPLAUM_RX_TEMPERATURE);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static int ospl2xx_rx_put_temperature(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	/* not implemented yet */
	return 0;
}

/* PARAM_ID_OPALUM_RX_VOLUME_CONTROL */
static int ospl2xx_rx_get_volume(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_RX_VOLUME_CONTROL);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static int ospl2xx_rx_put_volume(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t volume = ucontrol->value.integer.value[0];

	if (volume > 0x7FFFFFFF || volume < 0)
		volume = 0x7FFFFFFF;

	pr_debug("%s, volume[%d]\n", __func__, volume);

	ospl2xx_afe_set_single_param(
		PARAM_ID_OPALUM_RX_VOLUME_CONTROL, volume);

	return 0;
}

/* PARAM_ID_OPALUM_RX_TEMP_CAL_DATA */
static int ospl2xx_rx_put_temp_cal(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int32_t accumulated = ucontrol->value.integer.value[0];
	int32_t count       = ucontrol->value.integer.value[1];
	int32_t temperature = ucontrol->value.integer.value[2];

	pr_debug("%s, acc[%d], cnt[%d], tmpr[%d]\n", __func__,
			accumulated, count, temperature);
	pr_debug("%s, left_feedback[%d], right_feedback[%d], tx[%d], rx[%d]\n",
			__func__, feedback_on_left, feedback_on_right,
			AFE_TX_PORT_ID, AFE_RX_PORT_ID);

	mutex_lock(&mr_lock);
	if (feedback_on_left || feedback_on_right)
		afe_spk_prot_feed_back_cfg(AFE_TX_PORT_ID, AFE_RX_PORT_ID,
					feedback_on_left,
					feedback_on_right, 1);
	ospl2xx_afe_set_tri_param(
			PARAM_ID_OPALUM_RX_TEMP_CAL_DATA,
			accumulated, count, temperature);
	mutex_unlock(&mr_lock);

	mutex_lock(&lr_lock);
	if (ext_config_loaded) {
		ospl2xx_update_rx_ext_config();
		ospl2xx_update_tx_ext_config();
	} else {
		ospl2xx_update_rx_int_config();
		ospl2xx_update_tx_int_config();
	}
	mutex_unlock(&lr_lock);

	return 0;
}

/* TX */
/* PARAM_ID_OPALUM_TX_ENABLE*/
static int ospl2xx_tx_enable_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	if (enable != 0 && enable != 1)
		return 0;

	ospl2xx_afe_set_single_param(
		PARAM_ID_OPALUM_TX_ENABLE,
		enable);

	return 0;
}
static int ospl2xx_tx_enable_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_TX_ENABLE);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static const char *const ospl2xx_tx_enable_text[] = {"Disable", "Enable"};
static const struct soc_enum ospl2xx_tx_enable_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ospl2xx_tx_enable_text),
};

/* PARAM_ID_OPALUM_TX_RUN_CALIBRATION */
static int ospl2xx_tx_run_diagnostic(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int command = ucontrol->value.integer.value[0];

	if (command != 1)
		return 0;
	ospl2xx_afe_set_single_param(PARAM_ID_OPALUM_TX_RUN_CALIBRATION, 1);

	return 0;
}
static int ospl2xx_tx_run_diagnostic_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	/* not supported */
	return 0;
}
static const char *const ospl2xx_tx_run_diagnostic_text[] = {"Off", "Run"};
static const struct soc_enum ospl2xx_tx_run_diagnostic_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ospl2xx_tx_run_diagnostic_text),
};

/* PARAM_ID_OPALUM_TX_F0_CALIBRATION_VALUE */
static int ospl2xx_tx_get_f0(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_TX_F0_CALIBRATION_VALUE);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	ucontrol->value.integer.value[1] = afe_cb_payload32_data[1];
	mutex_unlock(&mr_lock);

	return 0;
}

/* PARAM_ID_OPALUM_TX_F0_CURVE */
static int ospl2xx_tx_get_f0_curve(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0;

	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_TX_F0_CURVE);

	if (f0_curve != NULL &&
	    f0_curve_size >= F0_CURVE_POINTS * sizeof(int32_t))
		for (i = 0; i < F0_CURVE_POINTS; i++)
			ucontrol->value.integer.value[i] = f0_curve[i];
	else
		pr_debug("%s: returned f0 points too small, %d\n",
			__func__, f0_curve_size);

	mutex_unlock(&mr_lock);

	return 0;
}

/* PARAM_ID_OPALUM_TX_TEMP_MEASUREMENT_VALUE */
static int ospl2xx_tx_get_temp_cal(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_TX_TEMP_MEASUREMENT_VALUE);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	ucontrol->value.integer.value[1] = afe_cb_payload32_data[1];
	mutex_unlock(&mr_lock);

	return 0;
}

/* PARAM_ID_OPALUM_TX_SET_USE_CASE */
#define NUM_TX_CONFIGS 4
static int ospl2xx_tx_int_config_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&mr_lock);
	ospl2xx_afe_set_callback(ospl2xx_afe_callback);
	ospl2xx_afe_get_param(PARAM_ID_OPALUM_TX_CURRENT_PARAM_SET);

	ucontrol->value.integer.value[0] = afe_cb_payload32_data[0];
	mutex_unlock(&mr_lock);

	return 0;
}
static int ospl2xx_tx_int_config_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, ret = 0;

	i = ucontrol->value.integer.value[0];
	if (i < 0 || i >= NUM_TX_CONFIGS)
		return -ERANGE;

	mutex_lock(&lr_lock);
	if (int_txconfig != i) {
		int_txconfig = i;
		ret = ospl2xx_update_tx_int_config();
	}
	mutex_unlock(&lr_lock);
	return ret;
}
static char const *ospl2xx_tx_int_config_index[] = {
	"opalum.tx.int.config.0",
	"opalum.tx.int.config.1",
	"opalum.tx.int.config.2",
	"opalum.tx.int.config.3",
};
static const struct soc_enum ospl2xx_tx_int_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(NUM_TX_CONFIGS, ospl2xx_tx_int_config_index),
};

/* PARAM_ID_OPALUM_TX_SET_EXTERNAL_CONFIG */
static int ospl2xx_tx_ext_config_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ext_txconfig - NUM_RX_CONFIGS;
	return 0;
}

static int ospl2xx_tx_ext_config_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0, ret = 0;

	i = ucontrol->value.integer.value[0] + NUM_RX_CONFIGS;
	if (i < NUM_RX_CONFIGS || i >= ARRAY_SIZE(ospl2xx_ext_config_tables))
		return -ERANGE;

	mutex_lock(&lr_lock);
	if (ext_txconfig != i) {
		ext_txconfig = i;
		ret = ospl2xx_update_tx_ext_config();
	}
	mutex_unlock(&lr_lock);
	return ret;
}

static char const *ospl2xx_tx_ext_config_text[] = {
	"opalum.tx.ext.config.0",
	"opalum.tx.ext.config.1",
	"opalum.tx.ext.config.2",
	"opalum.tx.ext.config.3",
};
static const struct soc_enum ospl2xx_tx_ext_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(NUM_TX_CONFIGS,
		ospl2xx_tx_ext_config_text),
};

/* Internal logic to re-load external configuration to driver */
static int ospl2xx_reload_ext_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int i = 0;
	int reload = ucontrol->value.integer.value[0];

	if (reload && ext_config_loaded) {
		pr_debug("re-loading configuration files\n");
		for (i = 0; i < ARRAY_SIZE(ospl2xx_ext_config_tables); i++)
			release_firmware(ospl2xx_config[i]);

		INIT_WORK(&ospl2xx_init_work, ospl2xx_load_config);
		queue_work(ospl2xx_wq, &ospl2xx_init_work);
	}

	return 0;
}
static int ospl2xx_reload_ext_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}
static const char *const ospl2xx_reload_ext_text[] = {"Stay", "Reload"};
static const struct soc_enum ospl2xx_reload_ext_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, ospl2xx_reload_ext_text),
};


static const struct snd_kcontrol_new ospl2xx_params_controls[] = {
	/* GET,PUT */ SOC_ENUM_EXT("OSPL Rx",
		ospl2xx_rx_enable_enum[0],
		ospl2xx_rx_enable_get, ospl2xx_rx_enable_put),
	/* GET,PUT */ SOC_ENUM_EXT("OSPL Int RxConfig",
		ospl2xx_rx_int_config_enum[0],
		ospl2xx_rx_int_config_get, ospl2xx_rx_int_config_put),
	/* PUT */ SOC_ENUM_EXT("OSPL Ext RxConfig",
		ospl2xx_rx_ext_config_enum[0],
		ospl2xx_rx_ext_config_get, ospl2xx_rx_ext_config_put),
	/* PUT */ SOC_ENUM_EXT("OSPL Rx diagnostic",
		ospl2xx_rx_run_diagnostic_enum[0],
		ospl2xx_rx_run_diagnostic_get, ospl2xx_rx_run_diagnostic),
	/* GET */ SOC_SINGLE_EXT("OSPL Rx exc_model",
		SND_SOC_NOPM, 0, 0xFFFF, 0,
		ospl2xx_rx_get_exc_model, ospl2xx_rx_put_exc_model),
	/* GET */ SOC_SINGLE_EXT("OSPL Rx temperature",
		SND_SOC_NOPM, 0, 0xFFFF, 0,
		ospl2xx_rx_get_temperature, ospl2xx_rx_put_temperature),
	/* PUT */ SOC_SINGLE_MULTI_EXT("OSPL Rx temp_cal",
		SND_SOC_NOPM, 0, 0xFFFF, 0, 3,
		NULL, ospl2xx_rx_put_temp_cal),

	/* GET,PUT */ SOC_ENUM_EXT("OSPL Tx",
		ospl2xx_tx_enable_enum[0],
		ospl2xx_tx_enable_get, ospl2xx_tx_enable_put),
	/* PUT */ SOC_ENUM_EXT("OSPL Tx diagnostic",
		ospl2xx_tx_run_diagnostic_enum[0],
		ospl2xx_tx_run_diagnostic_get, ospl2xx_tx_run_diagnostic),
	/* GET */ SOC_SINGLE_MULTI_EXT("OSPL Tx F0",
		SND_SOC_NOPM, 0, 0xFFFF, 0, 2,
		ospl2xx_tx_get_f0, NULL),
	/* GET */ SOC_SINGLE_MULTI_EXT("OSPL Tx F0_curve",
		SND_SOC_NOPM, 0, 0xFFFF, 0, F0_CURVE_POINTS,
		ospl2xx_tx_get_f0_curve, NULL),
	/* GET */ SOC_SINGLE_MULTI_EXT("OSPL Tx temp_cal",
		SND_SOC_NOPM, 0, 0xFFFF, 0, 2,
		ospl2xx_tx_get_temp_cal, NULL),
	/* GET,PUT */ SOC_ENUM_EXT("OSPL Int TxConfig",
		ospl2xx_tx_int_config_enum[0],
		ospl2xx_tx_int_config_get, ospl2xx_tx_int_config_put),
	/* PUT */ SOC_ENUM_EXT("OSPL Ext TxConfig",
		ospl2xx_tx_ext_config_enum[0],
		ospl2xx_tx_ext_config_get, ospl2xx_tx_ext_config_put),

	/* PUT */ SOC_ENUM_EXT("OSPL Ext Reload",
		ospl2xx_reload_ext_enum[0],
		ospl2xx_reload_ext_get, ospl2xx_reload_ext_put),

	/* PUT */ SOC_ENUM_EXT("OSPL Audio Mode",
		ospl2xx_audio_mode_enum[0],
		ospl2xx_audio_mode_get, ospl2xx_audio_mode_put),
};

static const struct snd_kcontrol_new ospl2xx_volume_controls[] = {
	/* GET,PUT */ SOC_SINGLE_EXT("OSPL Rx Volume",
		SND_SOC_NOPM, 0, 0x7FFFFFFF, 0,
		ospl2xx_rx_get_volume, ospl2xx_rx_put_volume),
};

/*
 * ospl2xx initialization
 * - register mixer controls
 * - load external configuration strings
 */
int ospl2xx_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;

	snd_soc_add_codec_controls(codec, ospl2xx_params_controls,
			ARRAY_SIZE(ospl2xx_params_controls));

	if (ospl_volume_control) {
		snd_soc_add_codec_controls(codec, ospl2xx_volume_controls,
			ARRAY_SIZE(ospl2xx_volume_controls));
		pr_info("%s: ospl volume control is enabled\n", __func__);
	}

	ospl2xx_wq = create_singlethread_workqueue("ospl2xx");
	if (ospl2xx_wq == NULL)
		return -ENOMEM;

	INIT_WORK(&ospl2xx_init_work, ospl2xx_load_config);
	queue_work(ospl2xx_wq, &ospl2xx_init_work);

	return 0;
}

static int ospl2xx_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	uint32_t num_entries = 0;
	uint32_t *val_array = NULL;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret)
		goto err;

	num_entries = of_property_count_u32_elems(pdev->dev.of_node,
					"mmi,ospl-tune-index");
	if (num_entries > 0) {
		val_array = kcalloc(num_entries, sizeof(uint32_t), GFP_KERNEL);
		if (!val_array) {
			pr_err("%s malloc fail at ospl-tune-index\n", __func__);
			ret = -ENOMEM;
			goto err;
		}
		ret = of_property_read_u32_array(pdev->dev.of_node,
					"mmi,ospl-tune-index",
					val_array, num_entries);
		if (ret < 0) {
			pr_err("%s failed to load ospl-tune-index\n", __func__);
			kfree(val_array);
			goto err;
		}
		for (i = 0 ; i < num_entries; i++)
			ospl_tune_index[i] = val_array[i];
		kfree(val_array);
	}

	ret = of_property_read_u32_index(pdev->dev.of_node,
					"mmi,ospl-afe-port-id", 0,
					&AFE_RX_PORT_ID);
	if (ret) {
		pr_err("%s couldn't read ospl afe rx port id\n", __func__);
		goto err;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node,
					"mmi,ospl-afe-port-id", 1,
					&AFE_TX_PORT_ID);
	if (ret) {
		pr_err("%s couldn't read ospl afe tx port id\n", __func__);
		goto err;
	}

	feedback_on_left = of_property_read_bool(pdev->dev.of_node,
					"mmi,ospl-left-feedback");
	feedback_on_right = of_property_read_bool(pdev->dev.of_node,
					"mmi,ospl-right-feedback");

	ospl_volume_control = of_property_read_bool(pdev->dev.of_node,
					"mmi,ospl-volume-control");

err:
	spdev = pdev;
	return ret;
}

static int ospl2xx_remove(struct platform_device *pdev)
{
	if (ospl2xx_wq)
		destroy_workqueue(ospl2xx_wq);

	return 0;
}

static const struct of_device_id ospl2xx_of_match[] = {
	{ .compatible = "mmi,ospl2xx" },
	{  },
};
MODULE_DEVICE_TABLE(of, ospl2xx_of_match);

static struct platform_driver ospl2xx_driver = {
	.probe		= ospl2xx_probe,
	.remove		= ospl2xx_remove,
	.driver         = {
		.name   = "ospl2xx",
		.of_match_table	= ospl2xx_of_match,
	},
};

static int __init ospl2xx_drv_init(void)
{
	return platform_driver_register(&ospl2xx_driver);
}
module_init(ospl2xx_drv_init);

static void __exit ospl2xx_drv_exit(void)
{
	platform_driver_unregister(&ospl2xx_driver);
}
module_exit(ospl2xx_drv_exit);

MODULE_DESCRIPTION("ASoC OSPL interface driver");
MODULE_AUTHOR("Yoon (Seungyoon) Lee, Motorola Mobility Inc, <yoon@motorola.com>");
