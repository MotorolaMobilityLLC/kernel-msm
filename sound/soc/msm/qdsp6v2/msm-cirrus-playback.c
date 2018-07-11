/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/


#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/compat.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "msm-cirrus-playback.h"

#define CRUS_TX_CONFIG "crus_sp_tx%d.bin"
#define CRUS_RX_CONFIG "crus_sp_rx%d.bin"

static struct device *crus_sp_device;
static atomic_t crus_sp_misc_usage_count;

static struct crus_single_data_t crus_enable;

static struct crus_sp_ioctl_header crus_sp_hdr;
static struct cirrus_cal_result_t crus_sp_cal_rslt;
static int32_t *crus_sp_get_buffer;
static atomic_t crus_sp_get_param_flag;
struct mutex crus_sp_get_param_lock;
struct mutex crus_sp_lock;
static int cirrus_sp_en;
static int cirrus_sp_prot_en;
static int cirrus_sp_usecase;
static int cirrus_fb_port_ctl;
static int cirrus_fb_load_conf_sel;
static int cirrus_fb_delta_sel;
static int cirrus_ff_chan_swap_sel;
static int cirrus_ff_chan_swap_dur;
static bool cirrus_fail_detect_en;
static int cirrus_fb_port = AFE_PORT_ID_QUATERNARY_TDM_TX;
static int cirrus_ff_port = AFE_PORT_ID_QUATERNARY_TDM_RX;

static int crus_sp_usecase_dt_count;
static const char *crus_sp_usecase_dt_text[MAX_TUNING_CONFIGS];

static void *crus_gen_afe_get_header(int length, int port, int module,
				     int param)
{
	struct afe_custom_crus_get_config_t *config = NULL;
	int size = sizeof(struct afe_custom_crus_get_config_t);
	int index = afe_get_port_index(port);
	uint16_t payload_size = sizeof(struct afe_port_param_data_v2) +
				length;

	/* Allocate memory for the message */
	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return NULL;

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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
	config->param.port_id = (uint16_t) port;
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;
	config->param.module_id = (uint32_t) module;
	config->param.param_id = (uint32_t) param;
	/* max data size of the param_ID/module_ID combination */
	config->param.payload_size = payload_size;

	/* Set data section */
	config->data.module_id = (uint32_t) module;
	config->data.param_id = (uint32_t) param;
	config->data.reserved = 0; /* Must be set to 0 */
	/* actual size of the data for the module_ID/param_ID pair */
	config->data.param_size = length;

	return (void *)config;
}

static void *crus_gen_afe_set_header(int length, int port, int module,
				     int param)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	int size = sizeof(struct afe_custom_crus_set_config_t) + length;
	int index = afe_get_port_index(port);
	uint16_t payload_size = sizeof(struct afe_port_param_data_v2) +
				length;

	/* Allocate memory for the message */
	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return NULL;

	/* Set header section */
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
					APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
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
	config->param.port_id = (uint16_t) port;
	config->param.payload_address_lsw = 0;
	config->param.payload_address_msw = 0;
	config->param.mem_map_handle = 0;
	/* max data size of the param_ID/module_ID combination */
	config->param.payload_size = payload_size;

	/* Set data section */
	config->data.module_id = (uint32_t) module;
	config->data.param_id = (uint32_t) param;
	config->data.reserved = 0; /* Must be set to 0 */
	/* actual size of the data for the module_ID/param_ID pair */
	config->data.param_size = length;

	return (void *)config;
}

static int crus_afe_get_param(int port, int module, int param, int length,
			      void *data)
{
	struct afe_custom_crus_get_config_t *config = NULL;
	int index = afe_get_port_index(port);
	int ret = 0, count = 0;

	pr_info("%s: port = %d module = %d param = 0x%x length = %d\n",
		__func__, port, module, param, length);

	config = (struct afe_custom_crus_get_config_t *)
		 crus_gen_afe_get_header(length, port, module, param);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	pr_info("%s: Preparing to send apr packet\n", __func__);

	mutex_lock(&crus_sp_get_param_lock);
	atomic_set(&crus_sp_get_param_flag, 0);

	crus_sp_get_buffer = kzalloc(config->param.payload_size + 16,
				     GFP_KERNEL);

	ret = afe_apr_send_pkt_crus(config, index, 0);
	if (ret)
		pr_err("%s: crus get_param for port %d failed with code %d\n",
						__func__, port, ret);
	else
		pr_info("%s: crus get_param sent packet with param id 0x%08x to module 0x%08x.\n",
			__func__, param, module);

	/* Wait for afe callback to populate data */
	while (!atomic_read(&crus_sp_get_param_flag)) {
		usleep_range(800, 1200);
		if (count++ >= 1000) {
			pr_err("%s: AFE callback timeout\n", __func__);
			atomic_set(&crus_sp_get_param_flag, 1);
			return -EINVAL;
		}
	}

	/* Copy from dynamic buffer to return buffer */
	memcpy((u8 *)data, &crus_sp_get_buffer[4], length);

	kfree(crus_sp_get_buffer);

	mutex_unlock(&crus_sp_get_param_lock);

	kfree(config);
	return ret;
}

static int crus_afe_set_param(int port, int module, int param, int length,
			      void *data_ptr)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	int index = afe_get_port_index(port);
	int ret = 0;

	pr_info("%s: port = %d module = %d param = 0x%x length = %d\n",
		__func__, port, module, param, length);

	config = crus_gen_afe_set_header(length, port, module, param);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	memcpy((u8 *)config + sizeof(struct afe_custom_crus_set_config_t),
	       (u8 *) data_ptr, length);

	pr_debug("%s: Preparing to send apr packet.\n", __func__);

	ret = afe_apr_send_pkt_crus(config, index, 1);
	if (ret) {
		pr_err("%s: crus set_param for port %d failed with code %d\n",
						__func__, port, ret);
	} else {
		pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
			 __func__, param, module);
	}

	kfree(config);
	return ret;
}

static int crus_afe_send_config(const char *data, int32_t length,
				int32_t port, int32_t module)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	struct crus_external_config_t *payload = NULL;
	int size = sizeof(struct crus_external_config_t);
	int ret = 0;
	int index = afe_get_port_index(port);
	uint32_t param = 0;
	int mem_size = 0;
	int sent = 0;
	int chars_to_send = 0;

	pr_info("%s: called with module_id = %x, string length = %d\n",
						__func__, module, length);

	/* Destination settings for message */
	if (port == cirrus_ff_port)
		param = CRUS_PARAM_RX_SET_EXT_CONFIG;
	else if (port == cirrus_fb_port)
		param = CRUS_PARAM_TX_SET_EXT_CONFIG;
	else {
		pr_err("%s: Received invalid port parameter %d\n",
		       __func__, module);
		return -EINVAL;
	}

	if (length > APR_CHUNK_SIZE)
		mem_size = APR_CHUNK_SIZE;
	else
		mem_size = length;

	config = crus_gen_afe_set_header(size, port, module, param);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	payload = (struct crus_external_config_t *)((u8 *)config +
			sizeof(struct afe_custom_crus_set_config_t));
	payload->total_size = (uint32_t)length;
	payload->reserved = 0;
	payload->config = PAYLOAD_FOLLOWS_CONFIG;
	    /* ^ This tells the algorithm to expect array */
	    /*   immediately following the header */

	/* Send config string in chunks of APR_CHUNK_SIZE bytes */
	while (sent < length) {
		chars_to_send = length - sent;
		if (chars_to_send > APR_CHUNK_SIZE) {
			chars_to_send = APR_CHUNK_SIZE;
			payload->done = 0;
		} else {
			payload->done = 1;
		}

		/* Configure per message parameter settings */
		memcpy(payload->data, data + sent, chars_to_send);
		payload->chunk_size = chars_to_send;

		/* Send the actual message */
		pr_debug("%s: Preparing to send apr packet.\n", __func__);
		ret = afe_apr_send_pkt_crus(config, index, 1);

		if (ret)
			pr_err("%s: crus set_param for port %d failed with code %d\n",
			       __func__, port, ret);
		else
			pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
				 __func__, param, module);

		sent += chars_to_send;
	}

	kfree(config);
	return ret;
}

static int crus_afe_send_delta(const char *data, uint32_t length)
{
	struct afe_custom_crus_set_config_t *config = NULL;
	struct crus_delta_config_t *payload = NULL;
	int size = sizeof(struct crus_delta_config_t);
	int port = cirrus_ff_port;
	int param = CRUS_PARAM_RX_SET_DELTA_CONFIG;
	int module = CIRRUS_SP;
	int ret = 0;
	int index = afe_get_port_index(port);
	int mem_size = 0;
	int sent = 0;
	int chars_to_send = 0;

	pr_info("%s: called with module_id = %x, string length = %d\n",
						__func__, module, length);

	if (length > APR_CHUNK_SIZE)
		mem_size = APR_CHUNK_SIZE;
	else
		mem_size = length;

	config = crus_gen_afe_set_header(size, port, module, param);
	if (config == NULL) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	payload = (struct crus_delta_config_t *)((u8 *)config +
			sizeof(struct afe_custom_crus_set_config_t));
	payload->total_size = length;
	payload->index = 0;
	payload->reserved = 0;
	payload->config = PAYLOAD_FOLLOWS_CONFIG;
	    /* ^ This tells the algorithm to expect array */
	    /*   immediately following the header */

	/* Send config string in chunks of APR_CHUNK_SIZE bytes */
	while (sent < length) {
		chars_to_send = length - sent;
		if (chars_to_send > APR_CHUNK_SIZE) {
			chars_to_send = APR_CHUNK_SIZE;
			payload->done = 0;
		} else {
			payload->done = 1;
		}

		/* Configure per message parameter settings */
		memcpy(payload->data, data + sent, chars_to_send);
		payload->chunk_size = chars_to_send;

		/* Send the actual message */
		pr_debug("%s: Preparing to send apr packet.\n", __func__);
		ret = afe_apr_send_pkt_crus(config, index, 1);

		if (ret)
			pr_err("%s: crus set_param for port %d failed with code %d\n",
			       __func__, port, ret);
		else
			pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
				 __func__, param, module);

		sent += chars_to_send;
	}

	kfree(config);
	return ret;
}

extern int crus_afe_callback(void *payload, int size)
{
	uint32_t *payload32 = payload;

	pr_debug("Cirrus AFE CALLBACK: size = %d\n", size);

	switch (payload32[1]) {
	case CIRRUS_SP:
		memcpy(crus_sp_get_buffer, payload32, size);
		atomic_set(&crus_sp_get_param_flag, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int msm_routing_cirrus_fbport_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: cirrus_fb_port_ctl = %d", __func__, cirrus_fb_port_ctl);
	ucontrol->value.integer.value[0] = cirrus_fb_port_ctl;
	return 0;
}

int msm_routing_cirrus_fbport_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	cirrus_fb_port_ctl = ucontrol->value.integer.value[0];

	switch (cirrus_fb_port_ctl) {
	case 0:
		cirrus_fb_port = AFE_PORT_ID_PRIMARY_MI2S_TX;
		cirrus_ff_port = AFE_PORT_ID_PRIMARY_MI2S_RX;
		break;
	case 1:
		cirrus_fb_port = AFE_PORT_ID_SECONDARY_MI2S_TX;
		cirrus_ff_port = AFE_PORT_ID_SECONDARY_MI2S_RX;
		break;
	case 2:
		cirrus_fb_port = AFE_PORT_ID_TERTIARY_MI2S_TX;
		cirrus_ff_port = AFE_PORT_ID_TERTIARY_MI2S_RX;
		break;
	case 3:
		cirrus_fb_port = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		cirrus_ff_port = AFE_PORT_ID_QUATERNARY_MI2S_RX;
		break;
	case 4:
		cirrus_fb_port = AFE_PORT_ID_QUATERNARY_TDM_TX;
		cirrus_ff_port = AFE_PORT_ID_QUATERNARY_TDM_RX;
		break;
	default:
		/* Default port to QUATERNARY */
		cirrus_fb_port_ctl = 4;
		cirrus_fb_port = AFE_PORT_ID_QUATERNARY_TDM_TX;
		cirrus_ff_port = AFE_PORT_ID_QUATERNARY_TDM_RX;
		break;
	}
	return 0;
}

static int msm_routing_crus_sp_enable(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];

	if (crus_set > 255) {
		pr_err("Cirrus SP Enable: Invalid entry; Enter 0 to DISABLE, 1 to ENABLE\n");
		return -EINVAL;
	}

	switch (crus_set) {
	case 0: /* "Config SP Disable" */
		pr_info("Cirrus SP Enable: Config DISABLE\n");
		crus_enable.value = 0;
		cirrus_sp_en = 0;
		break;
	case 1: /* "Config SP Enable" */
		pr_info("Cirrus SP Enable: Config ENABLE\n");
		crus_enable.value = 1;
		cirrus_sp_en = 1;
		break;
	default:
	return -EINVAL;
	}

	mutex_lock(&crus_sp_lock);
	crus_afe_set_param(cirrus_ff_port, CIRRUS_SP,
			   CRUS_AFE_PARAM_ID_ENABLE,
			   sizeof(struct crus_single_data_t),
			   (void *)&crus_enable);
	mutex_unlock(&crus_sp_lock);

	mutex_lock(&crus_sp_lock);
	crus_afe_set_param(cirrus_fb_port, CIRRUS_SP,
			   CRUS_AFE_PARAM_ID_ENABLE,
			   sizeof(struct crus_single_data_t),
			   (void *)&crus_enable);
	mutex_unlock(&crus_sp_lock);

	return 0;
}

static int msm_routing_crus_sp_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("Starting Cirrus SP Enable Get function call\n");

	return cirrus_sp_en;
}

static int msm_routing_crus_sp_prot_enable(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];

	if (crus_set > 1) {
		pr_err("Cirrus SP Protection: Invalid entry\n");
		return -EINVAL;
	}

	cirrus_sp_prot_en = crus_set;

	return 0;
}

static int msm_routing_crus_sp_prot_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_info("Starting Cirrus SP Protection Get function call\n");

	ucontrol->value.integer.value[0] = cirrus_sp_prot_en;

	return 0;
}

static int msm_routing_crus_sp_usecase(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct crus_rx_run_case_ctrl_t case_ctrl;
	const int crus_set = ucontrol->value.integer.value[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	uint32_t max_index = e->items;

	pr_debug("Starting Cirrus SP Config function call %d\n", crus_set);

	if (crus_set >= max_index) {
		pr_err("Cirrus SP Config index out of bounds (%d)\n", crus_set);
		return -EINVAL;
	}

	cirrus_sp_usecase = crus_set;

	case_ctrl.status_l = 1;
	case_ctrl.status_r = 1;
	case_ctrl.z_l = crus_sp_cal_rslt.z_l;
	case_ctrl.z_r = crus_sp_cal_rslt.z_r;
	case_ctrl.checksum_l = crus_sp_cal_rslt.z_l + 1;
	case_ctrl.checksum_r = crus_sp_cal_rslt.z_r + 1;
	case_ctrl.atemp = 23;
	case_ctrl.value = cirrus_sp_usecase;

	crus_afe_set_param(cirrus_fb_port, CIRRUS_SP,
			   CRUS_PARAM_TX_SET_USECASE, sizeof(cirrus_sp_usecase),
			   (void *)&cirrus_sp_usecase);
	crus_afe_set_param(cirrus_ff_port, CIRRUS_SP,
			   CRUS_PARAM_RX_SET_USECASE, sizeof(case_ctrl),
			   (void *)&case_ctrl);

	return 0;
}

static int msm_routing_crus_sp_usecase_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Config Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_sp_usecase;

	return 0;
}

static int msm_routing_crus_load_config(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];
	char config[CONFIG_FILE_SIZE];
	const struct firmware *firmware;

	pr_debug("Starting Cirrus SP Load Config function call %d\n", crus_set);

	switch (crus_set) {
	case 0:
		break;
	case 1: /* Load RX Config */
		cirrus_fb_load_conf_sel = crus_set;
		snprintf(config, CONFIG_FILE_SIZE, CRUS_RX_CONFIG,
			 cirrus_sp_usecase);

		if (request_firmware(&firmware, config, crus_sp_device) != 0) {
			pr_err("%s: Request firmware failed\n", __func__);
			return -EINVAL;
		}

		pr_info("%s: Sending RX config\n", __func__);
		crus_afe_send_config(firmware->data, firmware->size,
				     cirrus_ff_port, CIRRUS_SP);
		release_firmware(firmware);
		break;
	case 2: /* Load TX Config */
		cirrus_fb_load_conf_sel = crus_set;
		snprintf(config, CONFIG_FILE_SIZE, CRUS_TX_CONFIG,
			 cirrus_sp_usecase);

		if (request_firmware(&firmware, config, crus_sp_device) != 0) {
			pr_err("%s: Request firmware failed\n", __func__);
			return -EINVAL;
		}

		pr_info("%s: Sending TX config\n", __func__);
		crus_afe_send_config(firmware->data, firmware->size,
				     cirrus_fb_port, CIRRUS_SP);
		release_firmware(firmware);
		break;
	default:
		return -EINVAL;
	}

	cirrus_fb_load_conf_sel = 0;

	return 0;
}

static int msm_routing_crus_load_config_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Load Config Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_fb_load_conf_sel;

	return 0;
}

static int msm_routing_crus_delta_config(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct crus_single_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];
	const struct firmware *firmware;

	pr_debug("Starting Cirrus SP Delta Config function call %d\n",
		 crus_set);

	switch (crus_set) {
	case 0:
		break;
	case 1: /* Load delta config over AFE */
		cirrus_fb_delta_sel = crus_set;

		if (request_firmware(&firmware, "crus_sp_delta_config.bin",
				     crus_sp_device) != 0) {
			pr_err("%s: Request firmware failed\n", __func__);
			cirrus_fb_delta_sel = 0;
			return -EINVAL;
		}

		pr_info("%s: Sending delta config\n", __func__);
		crus_afe_send_delta(firmware->data, firmware->size);
		release_firmware(firmware);
		break;
	case 2: /* Run delta transition */
		cirrus_fb_delta_sel = crus_set;
		data.value = 0;
		crus_afe_set_param(cirrus_ff_port, CIRRUS_SP,
				   CRUS_PARAM_RX_RUN_DELTA_CONFIG,
				   sizeof(struct crus_single_data_t), &data);
		break;
	default:
		return -EINVAL;
	}

	cirrus_fb_delta_sel = 0;
	return 0;
}

static int msm_routing_crus_delta_config_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Delta Config Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_fb_delta_sel;

	return 0;
}

static int msm_routing_crus_chan_swap(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct crus_dual_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];

	pr_debug("Starting Cirrus SP Channel Swap function call %d\n",
		 crus_set);

	switch (crus_set) {
	case 0: /* L/R */
		data.data1 = 1;
		break;
	case 1: /* R/L */
		data.data1 = 2;
		break;
	default:
		return -EINVAL;
	}

	data.data2 = cirrus_ff_chan_swap_dur;

	crus_afe_set_param(cirrus_ff_port, CIRRUS_SP,
			   CRUS_PARAM_RX_CHANNEL_SWAP,
			   sizeof(struct crus_dual_data_t), &data);

	cirrus_ff_chan_swap_sel = crus_set;

	return 0;
}

static int msm_routing_crus_chan_swap_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Channel Swap Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_chan_swap_sel;

	return 0;
}

static int msm_routing_crus_chan_swap_dur(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int crus_set = ucontrol->value.integer.value[0];

	pr_debug("Starting Cirrus SP Channel Swap Duration function call\n");

	if ((crus_set < 0) || (crus_set > MAX_CHAN_SWAP_SAMPLES)) {
		pr_err("%s: Value out of range (%d)\n", __func__, crus_set);
		return -EINVAL;
	}

	if (crus_set < MIN_CHAN_SWAP_SAMPLES) {
		pr_info("%s: Received %d, rounding up to min value %d\n",
			__func__, crus_set, MIN_CHAN_SWAP_SAMPLES);
		crus_set = MIN_CHAN_SWAP_SAMPLES;
	}

	cirrus_ff_chan_swap_dur = crus_set;

	return 0;
}

static int msm_routing_crus_chan_swap_dur_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SP Channel Swap Duration Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_chan_swap_dur;

	return 0;
}

static int msm_routing_crus_fail_det(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	cirrus_fail_detect_en = ucontrol->value.integer.value[0];
	return 0;
}

static int msm_routing_crus_fail_det_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = cirrus_fail_detect_en;
	return 0;
}

static const char * const cirrus_fb_port_text[] = {"PRI_MI2S_RX",
						   "SEC_MI2S_RX",
						   "TERT_MI2S_RX",
						   "QUAT_MI2S_RX",
						   "QUAT_TDM_RX_0"};

static const char * const crus_en_text[] = {"Config SP Disable",
					    "Config SP Enable"};

static const char * const crus_prot_en_text[] = {"Disable", "Enable"};

static const char * const crus_conf_load_text[] = {"Idle", "Load RX",
						   "Load TX"};

static const char * const crus_delta_text[] = {"Idle", "Load", "Run"};

static const char * const crus_chan_swap_text[] = {"LR", "RL"};

static const struct soc_enum cirrus_fb_controls_enum[] = {
	SOC_ENUM_SINGLE_EXT(5, cirrus_fb_port_text),
};

static const struct soc_enum crus_en_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_en_text),
};

static const struct soc_enum crus_prot_en_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_prot_en_text),
};

static struct soc_enum crus_sp_usecase_enum[] = {
	SOC_ENUM_SINGLE_EXT(MAX_TUNING_CONFIGS, crus_sp_usecase_dt_text),
};

static const struct soc_enum crus_conf_load_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_conf_load_text),
};

static const struct soc_enum crus_delta_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_delta_text),
};

static const struct soc_enum crus_chan_swap_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_chan_swap_text),
};

static const struct snd_kcontrol_new crus_mixer_controls[] = {
	SOC_ENUM_EXT("Cirrus SP FBPort", cirrus_fb_controls_enum[0],
	msm_routing_cirrus_fbport_get, msm_routing_cirrus_fbport_put),
	SOC_ENUM_EXT("Cirrus SP", crus_en_enum[0],
	msm_routing_crus_sp_enable_get, msm_routing_crus_sp_enable),
	SOC_ENUM_EXT("Cirrus SP Protection", crus_prot_en_enum[0],
	msm_routing_crus_sp_prot_enable_get, msm_routing_crus_sp_prot_enable),
	SOC_ENUM_EXT("Cirrus SP Usecase", crus_sp_usecase_enum[0],
	msm_routing_crus_sp_usecase_get, msm_routing_crus_sp_usecase),
	SOC_ENUM_EXT("Cirrus SP Load Config", crus_conf_load_enum[0],
	msm_routing_crus_load_config_get, msm_routing_crus_load_config),
	SOC_ENUM_EXT("Cirrus SP Delta Config", crus_delta_enum[0],
	msm_routing_crus_delta_config_get, msm_routing_crus_delta_config),
	SOC_ENUM_EXT("Cirrus SP Channel Swap", crus_chan_swap_enum[0],
	msm_routing_crus_chan_swap_get, msm_routing_crus_chan_swap),
	SOC_SINGLE_EXT("Cirrus SP Channel Swap Duration", SND_SOC_NOPM, 0,
	MAX_CHAN_SWAP_SAMPLES, 0, msm_routing_crus_chan_swap_dur_get,
	msm_routing_crus_chan_swap_dur),
	SOC_SINGLE_BOOL_EXT("Cirrus SP Failure Detection", 0,
	msm_routing_crus_fail_det_get, msm_routing_crus_fail_det),
};

void msm_crus_pb_add_controls(struct snd_soc_platform *platform)
{
	crus_sp_device = platform->dev;

	if (crus_sp_device == NULL)
		pr_err("%s: platform->dev is NULL!\n", __func__);
	else
		pr_debug("%s: platform->dev = %lx\n", __func__,
			 (unsigned long)crus_sp_device);

	if (crus_sp_usecase_dt_count == 0)
		pr_err("%s: Missing usecase config selections\n", __func__);

	crus_sp_usecase_enum[0].items = crus_sp_usecase_dt_count;
	crus_sp_usecase_enum[0].texts = crus_sp_usecase_dt_text;

	snd_soc_add_platform_controls(platform, crus_mixer_controls,
					ARRAY_SIZE(crus_mixer_controls));
}

static long crus_sp_shared_ioctl(struct file *f, unsigned int cmd,
				 void __user *arg)
{
	int result = 0, port;
	uint32_t bufsize = 0, size;
	uint32_t option = 0;
	void *io_data = NULL;

	pr_info("%s\n", __func__);

	if (copy_from_user(&size, arg, sizeof(size))) {
		pr_err("%s: copy_from_user (size) failed\n", __func__);
		result = -EFAULT;
		goto exit;
	}

	/* Copy IOCTL header from usermode */
	if (copy_from_user(&crus_sp_hdr, arg, size)) {
		pr_err("%s: copy_from_user (struct) failed\n", __func__);
		result = -EFAULT;
		goto exit;
	}

	bufsize = crus_sp_hdr.data_length;
	io_data = kzalloc(bufsize, GFP_KERNEL);

	switch (cmd) {
	case CRUS_SP_IOCTL_GET:
		switch (crus_sp_hdr.module_id) {
		case CRUS_MODULE_ID_TX:
			port = cirrus_fb_port;
		break;
		case CRUS_MODULE_ID_RX:
			port = cirrus_ff_port;
		break;
		default:
			pr_info("%s: Unrecognized port ID (%d)\n", __func__,
			       crus_sp_hdr.module_id);
			port = cirrus_ff_port;
		}

		crus_afe_get_param(port, CIRRUS_SP, crus_sp_hdr.param_id,
				   bufsize, io_data);

		result = copy_to_user(crus_sp_hdr.data, io_data, bufsize);
		if (result) {
			pr_err("%s: copy_to_user failed (%d)\n", __func__,
			       result);
			result = -EFAULT;
		} else {
			result = bufsize;
		}
	break;
	case CRUS_SP_IOCTL_SET:
		result = copy_from_user(io_data, (void *)crus_sp_hdr.data,
					bufsize);
		if (result) {
			pr_err("%s: copy_from_user failed (%d)\n", __func__,
			       result);
			result = -EFAULT;
			goto exit_io;
		}

		switch (crus_sp_hdr.module_id) {
		case CRUS_MODULE_ID_TX:
			port = cirrus_fb_port;
		break;
		case CRUS_MODULE_ID_RX:
			port = cirrus_ff_port;
		break;
		default:
			pr_info("%s: Unrecognized port ID (%d)\n", __func__,
			       crus_sp_hdr.module_id);
			port = cirrus_ff_port;
		}

		crus_afe_set_param(port, CIRRUS_SP, crus_sp_hdr.param_id,
				   bufsize, io_data);
	break;
	case CRUS_SP_IOCTL_GET_CALIB:
		if (copy_from_user(io_data, (void *)crus_sp_hdr.data,
				   bufsize)) {
			pr_err("%s: copy_from_user failed\n", __func__);
			result = -EFAULT;
			goto exit_io;
		}
		option = 1;
		crus_afe_set_param(cirrus_ff_port, CIRRUS_SP,
				   CRUS_PARAM_RX_SET_CALIB, sizeof(option),
				   (void *)&option);
		crus_afe_set_param(cirrus_fb_port, CIRRUS_SP,
				   CRUS_PARAM_TX_SET_CALIB, sizeof(option),
				   (void *)&option);
		msleep(2000);
		crus_afe_get_param(cirrus_fb_port, CIRRUS_SP,
				   CRUS_PARAM_TX_GET_TEMP_CAL, bufsize,
				   io_data);
		if (copy_to_user(crus_sp_hdr.data, io_data, bufsize)) {
			pr_err("%s: copy_to_user failed\n", __func__);
			result = -EFAULT;
		} else
			result = bufsize;
	break;
	case CRUS_SP_IOCTL_SET_CALIB:
		if (copy_from_user(io_data, (void *)crus_sp_hdr.data,
				   bufsize)) {
			pr_err("%s: copy_from_user failed\n", __func__);
			result = -EFAULT;
			goto exit_io;
		}
		memcpy(&crus_sp_cal_rslt, io_data, bufsize);
	break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n", __func__, cmd);
		result = -EINVAL;
	}

exit_io:
	kfree(io_data);
exit:
	return result;
}

static long crus_sp_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	pr_info("%s\n", __func__);

	return crus_sp_shared_ioctl(f, cmd, (void __user *)arg);
}

static long crus_sp_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;

	pr_info("%s\n", __func__);

	switch (cmd) {
	case CRUS_SP_IOCTL_GET32:
		cmd64 = CRUS_SP_IOCTL_GET;
		break;
	case CRUS_SP_IOCTL_SET32:
		cmd64 = CRUS_SP_IOCTL_SET;
		break;
	case CRUS_SP_IOCTL_GET_CALIB32:
		cmd64 = CRUS_SP_IOCTL_GET_CALIB;
		break;
	case CRUS_SP_IOCTL_SET_CALIB32:
		cmd64 = CRUS_SP_IOCTL_SET_CALIB;
		break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n", __func__, cmd);
		return -EINVAL;
	}

	return crus_sp_shared_ioctl(f, cmd64, compat_ptr(arg));
}

static int crus_sp_open(struct inode *inode, struct file *f)
{
	int result = 0;

	pr_debug("%s\n", __func__);

	atomic_inc(&crus_sp_misc_usage_count);
	return result;
}

static int crus_sp_release(struct inode *inode, struct file *f)
{
	int result = 0;

	pr_debug("%s\n", __func__);

	atomic_dec(&crus_sp_misc_usage_count);
	pr_debug("%s: ref count %d!\n", __func__,
		atomic_read(&crus_sp_misc_usage_count));

	return result;
}

static ssize_t temperature_left_show(struct device *dev,
				     struct device_attribute *a, char *buf)
{
	static const int material = 250;
	static const int scale_factor = 100000;
	int buffer[96];
	int out_cal0;
	int out_cal1;
	int z, r, t;
	int temp0;

	crus_afe_get_param(cirrus_ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
			   384, buffer);

	out_cal0 = buffer[12];
	out_cal1 = buffer[13];

	z = buffer[4];

	temp0 = buffer[10];

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = buffer[3];
	t = (material * scale_factor * (r-z) / z) + (temp0 * scale_factor);

	return snprintf(buf, PAGE_SIZE, "%d.%05dc\n", t / scale_factor,
			t % scale_factor);
}
static DEVICE_ATTR_RO(temperature_left);

static ssize_t temperature_right_show(struct device *dev,
				      struct device_attribute *a, char *buf)
{
	static const int material = 250;
	static const int scale_factor = 100000;
	int buffer[96];
	int out_cal0;
	int out_cal1;
	int z, r, t;
	int temp0;

	crus_afe_get_param(cirrus_ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
			   384, buffer);

	out_cal0 = buffer[14];
	out_cal1 = buffer[15];

	z = buffer[2];

	temp0 = buffer[10];

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = buffer[1];
	t = (material * scale_factor * (r-z) / z) + (temp0 * scale_factor);

	return snprintf(buf, PAGE_SIZE, "%d.%05dc\n", t / scale_factor,
			t % scale_factor);
}
static DEVICE_ATTR_RO(temperature_right);

static ssize_t resistance_left_show(struct device *dev,
				    struct device_attribute *a, char *buf)
{
	static const int scale_factor = 100000000;
	static const int amp_factor = 71498;
	int buffer[96];
	int out_cal0;
	int out_cal1;
	int r;

	crus_afe_get_param(cirrus_ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
			   384, buffer);

	out_cal0 = buffer[12];
	out_cal1 = buffer[13];

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = buffer[3] * amp_factor;

	return snprintf(buf, PAGE_SIZE, "%d.%08d ohms\n", r / scale_factor,
		       r % scale_factor);
}
static DEVICE_ATTR_RO(resistance_left);

static ssize_t resistance_right_show(struct device *dev,
				     struct device_attribute *a, char *buf)
{
	static const int scale_factor = 100000000;
	static const int amp_factor = 71498;
	int buffer[96];
	int out_cal0;
	int out_cal1;
	int r;

	crus_afe_get_param(cirrus_ff_port, CIRRUS_SP, CRUS_PARAM_RX_GET_TEMP,
			   384, buffer);

	out_cal0 = buffer[14];
	out_cal1 = buffer[15];

	if ((out_cal0 != 2) || (out_cal1 != 2))
		return snprintf(buf, PAGE_SIZE, "Calibration is not done\n");

	r = buffer[1] * amp_factor;

	return snprintf(buf, PAGE_SIZE, "%d.%08d ohms\n", r / scale_factor,
		       r % scale_factor);
}
static DEVICE_ATTR_RO(resistance_right);

static struct attribute *crus_sp_attrs[] = {
	&dev_attr_temperature_left.attr,
	&dev_attr_temperature_right.attr,
	&dev_attr_resistance_left.attr,
	&dev_attr_resistance_right.attr,
	NULL,
};

static const struct attribute_group crus_sp_group = {
	.attrs  = crus_sp_attrs,
};

static const struct attribute_group *crus_sp_groups[] = {
	&crus_sp_group,
	NULL,
};

static int msm_cirrus_playback_probe(struct platform_device *pdev)
{
	int i;

	pr_info("CRUS_SP: initializing platform device\n");

	crus_sp_usecase_dt_count = of_property_count_strings(pdev->dev.of_node,
							     "usecase-names");
	if (crus_sp_usecase_dt_count <= 0) {
		dev_dbg(&pdev->dev, "Usecase names not found\n");
		crus_sp_usecase_dt_count = 0;
		return 0;
	}

	if ((crus_sp_usecase_dt_count > 0) &&
	    (crus_sp_usecase_dt_count <= MAX_TUNING_CONFIGS))
		of_property_read_string_array(pdev->dev.of_node,
					      "usecase-names",
					      crus_sp_usecase_dt_text,
					      crus_sp_usecase_dt_count);
	else if (crus_sp_usecase_dt_count > MAX_TUNING_CONFIGS) {
		dev_err(&pdev->dev, "Max of %d usecase configs allowed\n",
			MAX_TUNING_CONFIGS);
		return -EINVAL;
	}

	for (i = 0; i < crus_sp_usecase_dt_count; i++)
		pr_info("CRUS_SP: usecase[%d] = %s\n", i,
			 crus_sp_usecase_dt_text[i]);

	return 0;
}

static const struct of_device_id msm_cirrus_playback_dt_match[] = {
	{.compatible = "cirrus,msm-cirrus-playback"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_cirrus_playback_dt_match);

static struct platform_driver msm_cirrus_playback_driver = {
	.driver = {
		.name = "msm-cirrus-playback",
		.owner = THIS_MODULE,
		.of_match_table = msm_cirrus_playback_dt_match,
	},
	.probe = msm_cirrus_playback_probe,
};

static const struct file_operations crus_sp_fops = {
	.owner = THIS_MODULE,
	.open = crus_sp_open,
	.release = crus_sp_release,
	.unlocked_ioctl = crus_sp_ioctl,
	.compat_ioctl = crus_sp_compat_ioctl,
};

struct miscdevice crus_sp_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_cirrus_playback",
	.fops = &crus_sp_fops,
};

static int __init crus_sp_init(void)
{
	pr_info("CRUS_SP_INIT: initializing misc device\n");
	atomic_set(&crus_sp_get_param_flag, 0);
	atomic_set(&crus_sp_misc_usage_count, 0);
	mutex_init(&crus_sp_get_param_lock);
	mutex_init(&crus_sp_lock);

	misc_register(&crus_sp_misc);

	if (sysfs_create_groups(&crus_sp_misc.this_device->kobj,
				crus_sp_groups))
		pr_err("%s: Could not create sysfs groups\n", __func__);

	return platform_driver_register(&msm_cirrus_playback_driver);
}
module_init(crus_sp_init);

static void __exit crus_sp_exit(void)
{
	mutex_destroy(&crus_sp_get_param_lock);
	mutex_destroy(&crus_sp_lock);

	platform_driver_unregister(&msm_cirrus_playback_driver);
}
module_exit(crus_sp_exit);
