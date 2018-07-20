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

#define CRUS_RX_CONFIG "crus_sp_rx%d.bin"

static struct device *crus_se_device;
static atomic_t crus_se_misc_usage_count;

static struct crus_single_data_t crus_enable;

static struct crus_se_ioctl_header crus_se_hdr;
static uint32_t *crus_se_get_buffer;
static atomic_t crus_se_get_param_flag;
struct mutex crus_se_get_param_lock;
struct mutex crus_se_lock;
static int cirrus_se_en;
static int cirrus_se_usecase;
static int cirrus_ff_load_conf_sel;
static int cirrus_ff_delta_sel;
static int cirrus_ff_chan_swap_sel;
static int cirrus_ff_chan_swap_dur;
static int cirrus_ff_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_RX;

static int crus_se_usecase_dt_count;
static const char *crus_se_usecase_dt_text[MAX_TUNING_CONFIGS];

static int crus_get_param(int port, int module, int param, int length,
			      void *data)
{
	int ret = 0, count = 0;
	struct param_hdr_v3 param_hdr = {0};

	pr_debug("%s: port = %d module = %d param = 0x%x length = %d\n",
		__func__, port, module, param, length);

	param_hdr.module_id = module;
	param_hdr.param_id = param;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_size = length;

	pr_debug("%s: Preparing to send apr packet\n", __func__);

	mutex_lock(&crus_se_get_param_lock);
	atomic_set(&crus_se_get_param_flag, 0);

	crus_se_get_buffer = kzalloc(length + 16, GFP_KERNEL);
	if (!crus_se_get_buffer) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		mutex_unlock(&crus_se_get_param_lock);
		return -ENOMEM;
	}

	ret = afe_get_crus_params(port, NULL, &param_hdr);

	if (ret)
		pr_err("%s: crus get_param for port %d failed with code %d\n",
						__func__, port, ret);
	else
		pr_debug("%s: crus get_param sent packet with param id 0x%08x to module 0x%08x.\n",
			__func__, param, module);

	/* Wait for afe callback to populate data */
	while (!atomic_read(&crus_se_get_param_flag)) {
		usleep_range(800, 1200);
		if (count++ >= 1000) {
			pr_err("%s: AFE callback timeout\n", __func__);
			atomic_set(&crus_se_get_param_flag, 1);
			return -EINVAL;
		}
	}

	/* Copy from dynamic buffer to return buffer */
	memcpy((u8 *)data, &crus_se_get_buffer[4], length);

	kfree(crus_se_get_buffer);

	mutex_unlock(&crus_se_get_param_lock);

	return ret;
}

static int crus_set_param(int port, int module, int param, int length,
			  void *data_ptr)
{
	int ret = 0;
	struct param_hdr_v3 param_hdr = {0};

	pr_debug("%s: port = %d module = %d param = 0x%x length = %d\n",
		__func__, port, module, param, length);

	param_hdr.module_id = module;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param;
	param_hdr.param_size = length;

	pr_debug("%s: Preparing to send apr packet.\n", __func__);

	ret = afe_set_crus_params(port, param_hdr, (u8 *) data_ptr);
	if (ret) {
		pr_err("%s: crus set_param for port %d failed with code %d\n",
						__func__, port, ret);
	} else {
		pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
			 __func__, param, module);
	}

	return ret;
}

static int crus_send_config(const char *data, int32_t length,
			    int32_t port, int32_t module)
{
	struct param_hdr_v3 param_hdr = {0};
	struct crus_external_config_t *payload = NULL;
	int size = sizeof(struct crus_external_config_t);
	int ret = 0;
	uint32_t param = 0;
	int sent = 0;
	int chars_to_send = 0;

	pr_debug("%s: called with module_id = %x, string length = %d\n",
						__func__, module, length);

	/* Destination settings for message */
	if (port == cirrus_ff_port)
		param = CRUS_PARAM_RX_SET_EXT_CONFIG;
	else {
		pr_err("%s: Received invalid port parameter %d\n",
		       __func__, module);
		return -EINVAL;
	}

	/* Allocate memory for the message */
	size = sizeof(struct crus_external_config_t);

	payload = kzalloc(size, GFP_KERNEL);
	if (!payload) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	param_hdr.module_id = module;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param;
	param_hdr.param_size = size;

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
		ret = afe_set_crus_params(port, param_hdr, (u8 *) payload);
		if (ret)
			pr_err("%s: crus set_param for port %d failed with code %d\n",
			       __func__, port, ret);
		else
			pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
				 __func__, param, module);

		sent += chars_to_send;
	}

	kfree(payload);
	return ret;
}

static int crus_send_delta(const char *data, uint32_t length)
{
	struct param_hdr_v3 param_hdr = {0};
	struct crus_delta_config_t *payload = NULL;
	int size = sizeof(struct crus_delta_config_t);
	int port = cirrus_ff_port;
	int param = CRUS_PARAM_RX_SET_DELTA_CONFIG;
	int module = CIRRUS_SE;
	int ret = 0;
	int sent = 0;
	int chars_to_send = 0;

	pr_debug("%s: called with module_id = %x, string length = %d\n",
						__func__, module, length);

	/* Allocate memory for the message */
	payload = kzalloc(size, GFP_KERNEL);
	if (!payload) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	param_hdr.module_id = module;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param;
	param_hdr.param_size = size;

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
		ret = afe_set_crus_params(port, param_hdr, (u8 *) payload);

		if (ret)
			pr_err("%s: crus set_param for port %d failed with code %d\n",
			       __func__, port, ret);
		else
			pr_debug("%s: crus set_param sent packet with param id 0x%08x to module 0x%08x.\n",
				 __func__, param, module);

		sent += chars_to_send;
	}

	kfree(payload);
	return ret;
}

extern int crus_afe_callback(void *payload, int size)
{
	uint32_t *payload32 = payload;

	pr_debug("Cirrus AFE CALLBACK: size = %d\n", size);

	switch (payload32[1]) {
	case CIRRUS_SE:
		memcpy(crus_se_get_buffer, payload32, size);
		atomic_set(&crus_se_get_param_flag, 1);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int msm_crus_se_enable(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	static struct crus_single_data_t crus_id_ctrl;
	const int crus_set = ucontrol->value.integer.value[0];

	if (crus_set > 255) {
		pr_err("Cirrus SE Enable: Invalid entry; Enter 0 to DISABLE, 1 to ENABLE\n");
		return -EINVAL;
	}

	switch (crus_set) {
	case 0: /* "Config SE Disable" */
		pr_debug("Cirrus SE Enable: Config DISABLE\n");
		crus_enable.value = 0;
		cirrus_se_en = 0;
		break;
	case 1: /* "Config SE Enable" */
		pr_debug("Cirrus SE Enable: Config ENABLE\n");
		crus_enable.value = 1;
		cirrus_se_en = 1;
		break;
	default:
	return -EINVAL;
	}

	mutex_lock(&crus_se_lock);
	crus_set_param(cirrus_ff_port, CIRRUS_SE,
		       CIRRUS_SE_ENABLE,
		       sizeof(struct crus_single_data_t),
		       (void *)&crus_enable);

	crus_id_ctrl.value = 0; /*tx=1, rx=0*/
	crus_set_param(cirrus_ff_port, CIRRUS_SE,
		       CIRRUS_SE_PARAM_ID_CTRL,
		       sizeof(struct crus_single_data_t),
		       (void *)&crus_id_ctrl);
	mutex_unlock(&crus_se_lock);

	return 0;
}

static int msm_crus_se_enable_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SE Enable Get function call\n");

	crus_get_param(cirrus_ff_port, CIRRUS_SE, CIRRUS_SE_ENABLE,
			   sizeof(struct crus_single_data_t), (void *)&crus_enable);

	ucontrol->value.integer.value[0] =  crus_enable.value;

	return 0;
}

static int msm_crus_se_usecase(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct crus_rx_run_case_ctrl_t case_ctrl;
	const int crus_set = ucontrol->value.integer.value[0];
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	uint32_t max_index = e->items;

	pr_debug("Starting Cirrus SE Config function call %d\n", crus_set);

	if (crus_set >= max_index) {
		pr_err("Cirrus SE Config index out of bounds (%d)\n", crus_set);
		return -EINVAL;
	}

	cirrus_se_usecase = crus_set;

	case_ctrl.status_l = 0;
	case_ctrl.status_r = 0;
	case_ctrl.atemp = 0;
	case_ctrl.value = cirrus_se_usecase;

	crus_set_param(cirrus_ff_port, CIRRUS_SE,
		       CRUS_PARAM_RX_SET_USECASE, sizeof(case_ctrl),
		       (void *)&case_ctrl);

	return 0;
}

static int msm_crus_se_usecase_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	static struct crus_single_data_t crus_usecase;

	pr_debug("Starting Cirrus SE Usecase Get function call\n");

	crus_get_param(cirrus_ff_port, CIRRUS_SE, CRUS_PARAM_RX_GET_USECASE,
			   sizeof(struct crus_single_data_t), (void *)&crus_usecase);

	ucontrol->value.integer.value[0] = crus_usecase.value;

	return 0;
}

static int msm_crus_load_config(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	const int crus_set = ucontrol->value.integer.value[0];
	char config[CONFIG_FILE_SIZE];
	const struct firmware *firmware;

	pr_debug("Starting Cirrus SE Load Config function call %d\n", crus_set);

	switch (crus_set) {
	case 0:
		break;
	case 1: /* Load RX Config */
		cirrus_ff_load_conf_sel = crus_set;
		snprintf(config, CONFIG_FILE_SIZE, CRUS_RX_CONFIG,
			 cirrus_se_usecase);

		if (request_firmware(&firmware, config, crus_se_device) != 0) {
			pr_err("%s: Request firmware failed\n", __func__);
			return -EINVAL;
		}

		if (firmware->data != NULL && firmware->size > 0) {
			pr_debug("%s: Sending RX config(%d)\n",
				 __func__, cirrus_se_usecase);
			crus_send_config(firmware->data, firmware->size,
					 cirrus_ff_port, CIRRUS_SE);
		} else {
			pr_err("%s: can't read firmware data\n", __func__);
		}

		release_firmware(firmware);
		break;
	default:
		return -EINVAL;
	}

	cirrus_ff_load_conf_sel = 0;

	return 0;
}

static int msm_crus_load_config_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SE Load Config Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_load_conf_sel;

	return 0;
}

static int msm_crus_delta_config(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct crus_single_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];
	const struct firmware *firmware;

	pr_debug("Starting Cirrus SE Delta Config function call %d\n",
		 crus_set);

	switch (crus_set) {
	case 0:
		break;
	case 1: /* Load delta config over AFE */
		cirrus_ff_delta_sel = crus_set;

		if (request_firmware(&firmware, "crus_se_delta_config.bin",
				     crus_se_device) != 0) {
			pr_err("%s: Request firmware failed\n", __func__);
			cirrus_ff_delta_sel = 0;
			return -EINVAL;
		}

		pr_debug("%s: Sending delta config\n", __func__);
		crus_send_delta(firmware->data, firmware->size);
		release_firmware(firmware);
		break;
	case 2: /* Run delta transition */
		cirrus_ff_delta_sel = crus_set;
		data.value = 0;
		crus_set_param(cirrus_ff_port, CIRRUS_SE,
			       CRUS_PARAM_RX_RUN_DELTA_CONFIG,
			       sizeof(struct crus_single_data_t), &data);
		break;
	default:
		return -EINVAL;
	}

	cirrus_ff_delta_sel = 0;
	return 0;
}

static int msm_crus_delta_config_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SE Delta Config Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_delta_sel;

	return 0;
}

static int msm_crus_chan_swap(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct crus_dual_data_t data;
	const int crus_set = ucontrol->value.integer.value[0];

	pr_debug("Starting Cirrus SE Channel Swap function call %d\n",
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

	crus_set_param(cirrus_ff_port, CIRRUS_SE,
		       CRUS_PARAM_RX_CHANNEL_SWAP,
		       sizeof(struct crus_dual_data_t), &data);

	cirrus_ff_chan_swap_sel = crus_set;

	return 0;
}

static int msm_crus_chan_swap_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SE Channel Swap Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_chan_swap_sel;

	return 0;
}

static int msm_crus_chan_swap_dur(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int crus_set = ucontrol->value.integer.value[0];

	pr_debug("Starting Cirrus SE Channel Swap Duration function call\n");

	if ((crus_set < 0) || (crus_set > MAX_CHAN_SWAP_SAMPLES)) {
		pr_err("%s: Value out of range (%d)\n", __func__, crus_set);
		return -EINVAL;
	}

	if (crus_set < MIN_CHAN_SWAP_SAMPLES) {
		pr_debug("%s: Received %d, rounding up to min value %d\n",
			__func__, crus_set, MIN_CHAN_SWAP_SAMPLES);
		crus_set = MIN_CHAN_SWAP_SAMPLES;
	}

	cirrus_ff_chan_swap_dur = crus_set;

	return 0;
}

static int msm_crus_chan_swap_dur_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Starting Cirrus SE Channel Swap Duration Get function call\n");
	ucontrol->value.integer.value[0] = cirrus_ff_chan_swap_dur;

	return 0;
}

static const char * const crus_en_text[] = {"Disable", "Enable"};

static const char * const crus_conf_load_text[] = {"Idle", "Load RX"};

static const char * const crus_delta_text[] = {"Idle", "Load", "Run"};

static const char * const crus_chan_swap_text[] = {"LR", "RL"};

static const struct soc_enum crus_en_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_en_text),
};

static struct soc_enum crus_se_usecase_enum[] = {
	SOC_ENUM_SINGLE_EXT(MAX_TUNING_CONFIGS, crus_se_usecase_dt_text),
};

static const struct soc_enum crus_conf_load_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_conf_load_text),
};

static const struct soc_enum crus_delta_enum[] = {
	SOC_ENUM_SINGLE_EXT(3, crus_delta_text),
};

static const struct soc_enum crus_chan_swap_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, crus_chan_swap_text),
};

static const struct snd_kcontrol_new crus_mixer_controls[] = {
	SOC_ENUM_EXT("Cirrus SE", crus_en_enum[0],
	msm_crus_se_enable_get, msm_crus_se_enable),
	SOC_ENUM_EXT("Cirrus SE Audio Mode", crus_se_usecase_enum[0],
	msm_crus_se_usecase_get, msm_crus_se_usecase),
	SOC_ENUM_EXT("Cirrus SE Load Config", crus_conf_load_enum[0],
	msm_crus_load_config_get, msm_crus_load_config),
	SOC_ENUM_EXT("Cirrus SE Delta Config", crus_delta_enum[0],
	msm_crus_delta_config_get, msm_crus_delta_config),
	SOC_ENUM_EXT("Cirrus SE Channel Swap", crus_chan_swap_enum[0],
	msm_crus_chan_swap_get, msm_crus_chan_swap),
	SOC_SINGLE_EXT("Cirrus SE Channel Swap Duration", SND_SOC_NOPM, 0,
	MAX_CHAN_SWAP_SAMPLES, 0, msm_crus_chan_swap_dur_get,
	msm_crus_chan_swap_dur),
};

void msm_crus_pb_add_controls(struct snd_soc_platform *platform)
{
	crus_se_device = platform->dev;

	if (crus_se_device == NULL)
		pr_err("%s: platform->dev is NULL!\n", __func__);
	else
		pr_debug("%s: platform->dev = %lx\n", __func__,
			 (unsigned long)crus_se_device);

	if (crus_se_usecase_dt_count == 0)
		pr_err("%s: Missing usecase config selections\n", __func__);

	crus_se_usecase_enum[0].items = crus_se_usecase_dt_count;
	crus_se_usecase_enum[0].texts = crus_se_usecase_dt_text;

	snd_soc_add_platform_controls(platform, crus_mixer_controls,
					ARRAY_SIZE(crus_mixer_controls));
}

static long crus_se_shared_ioctl(struct file *f, unsigned int cmd,
				 void __user *arg)
{
	int result = 0, port;
	uint32_t bufsize = 0, size;
	void *io_data = NULL;

	pr_debug("%s\n", __func__);

	if (copy_from_user(&size, arg, sizeof(size))) {
		pr_err("%s: copy_from_user (size) failed\n", __func__);
		result = -EFAULT;
		goto exit;
	}

	/* Copy IOCTL header from usermode */
	if (copy_from_user(&crus_se_hdr, arg, size)) {
		pr_err("%s: copy_from_user (struct) failed\n", __func__);
		result = -EFAULT;
		goto exit;
	}

	bufsize = crus_se_hdr.data_length;
	io_data = kzalloc(bufsize, GFP_KERNEL);
	if (!io_data) {
		pr_err("%s: Memory allocation failed!\n", __func__);
		return -ENOMEM;
	}

	switch (cmd) {
	case CRUS_SE_IOCTL_GET:
		switch (crus_se_hdr.module_id) {
		case CRUS_MODULE_ID_RX:
			port = cirrus_ff_port;
		break;
		default:
			pr_debug("%s: Unrecognized port ID (%d)\n", __func__,
				 crus_se_hdr.module_id);
			port = cirrus_ff_port;
		}

		crus_get_param(port, CIRRUS_SE, crus_se_hdr.param_id,
			       bufsize, io_data);

		result = copy_to_user(crus_se_hdr.data, io_data, bufsize);
		if (result) {
			pr_err("%s: copy_to_user failed (%d)\n", __func__,
			       result);
			result = -EFAULT;
		} else {
			result = bufsize;
		}
	break;
	case CRUS_SE_IOCTL_SET:
		result = copy_from_user(io_data, (void *)crus_se_hdr.data,
					bufsize);
		if (result) {
			pr_err("%s: copy_from_user failed (%d)\n", __func__,
			       result);
			result = -EFAULT;
			goto exit_io;
		}

		switch (crus_se_hdr.module_id) {
		case CRUS_MODULE_ID_RX:
			port = cirrus_ff_port;
		break;
		default:
			pr_debug("%s: Unrecognized port ID (%d)\n", __func__,
				 crus_se_hdr.module_id);
			port = cirrus_ff_port;
		}

		crus_set_param(port, CIRRUS_SE, crus_se_hdr.param_id,
			       bufsize, io_data);
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

static long crus_se_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	pr_debug("%s\n", __func__);

	return crus_se_shared_ioctl(f, cmd, (void __user *)arg);
}

static long crus_se_compat_ioctl(struct file *f,
		unsigned int cmd, unsigned long arg)
{
	unsigned int cmd64;

	pr_debug("%s\n", __func__);

	switch (cmd) {
	case CRUS_SE_IOCTL_GET32:
		cmd64 = CRUS_SE_IOCTL_GET;
		break;
	case CRUS_SE_IOCTL_SET32:
		cmd64 = CRUS_SE_IOCTL_SET;
		break;
	default:
		pr_err("%s: Invalid IOCTL, command = %d!\n", __func__, cmd);
		return -EINVAL;
	}

	return crus_se_shared_ioctl(f, cmd64, compat_ptr(arg));
}

static int crus_se_open(struct inode *inode, struct file *f)
{
	int result = 0;

	pr_debug("%s\n", __func__);

	atomic_inc(&crus_se_misc_usage_count);
	return result;
}

static int crus_se_release(struct inode *inode, struct file *f)
{
	int result = 0;

	pr_debug("%s\n", __func__);

	atomic_dec(&crus_se_misc_usage_count);
	pr_debug("%s: ref count %d!\n", __func__,
		atomic_read(&crus_se_misc_usage_count));

	return result;
}

static int msm_cirrus_playback_probe(struct platform_device *pdev)
{
	int i, rc = 0;
	u32 ff_port_id;

	pr_info("CRUS_SE: initializing platform device\n");

	crus_se_usecase_dt_count = of_property_count_strings(pdev->dev.of_node,
							     "usecase-names");
	if (crus_se_usecase_dt_count <= 0) {
		dev_dbg(&pdev->dev, "Usecase names not found\n");
		crus_se_usecase_dt_count = 0;
		return 0;
	}

	if ((crus_se_usecase_dt_count > 0) &&
	    (crus_se_usecase_dt_count <= MAX_TUNING_CONFIGS))
		of_property_read_string_array(pdev->dev.of_node,
					      "usecase-names",
					      crus_se_usecase_dt_text,
					      crus_se_usecase_dt_count);
	else if (crus_se_usecase_dt_count > MAX_TUNING_CONFIGS) {
		dev_err(&pdev->dev, "Max of %d usecase configs allowed\n",
			MAX_TUNING_CONFIGS);
		return -EINVAL;
	}

	for (i = 0; i < crus_se_usecase_dt_count; i++)
		pr_info("CRUS_SE: usecase[%d] = %s\n", i,
			 crus_se_usecase_dt_text[i]);

	rc = of_property_read_u32_index(pdev->dev.of_node,
					"afe-port-id", 0, &ff_port_id);
	if (rc >= 0) {
		cirrus_ff_port = ff_port_id;
		pr_info("CRUS_SE: cirrus_ff_port = 0x%x", cirrus_ff_port);
	}

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

static const struct file_operations crus_se_fops = {
	.owner = THIS_MODULE,
	.open = crus_se_open,
	.release = crus_se_release,
	.unlocked_ioctl = crus_se_ioctl,
	.compat_ioctl = crus_se_compat_ioctl,
};

struct miscdevice crus_se_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msm_cirrus_playback",
	.fops = &crus_se_fops,
};

static int __init crus_se_init(void)
{
	pr_info("CRUS_SE_INIT: initializing misc device\n");
	atomic_set(&crus_se_get_param_flag, 0);
	atomic_set(&crus_se_misc_usage_count, 0);
	mutex_init(&crus_se_get_param_lock);
	mutex_init(&crus_se_lock);

	misc_register(&crus_se_misc);

	return platform_driver_register(&msm_cirrus_playback_driver);
}
module_init(crus_se_init);

static void __exit crus_se_exit(void)
{
	mutex_destroy(&crus_se_get_param_lock);
	mutex_destroy(&crus_se_lock);

	platform_driver_unregister(&msm_cirrus_playback_driver);
}
module_exit(crus_se_exit);
