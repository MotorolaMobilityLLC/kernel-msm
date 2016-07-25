/* Copyright (c) 2016, Motorola Mobility, LLC.
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

#include <linux/completion.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mod_display.h>
#include <linux/mod_display_ops.h>
#include <linux/module.h>
#include <linux/panel_notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "msm_dba_internal.h"
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "../mdss/mdss_dsi.h"
#include "../mdss/mdss_panel.h"
#include "mot_dba.h"

#define DRVNAME "mot_dba"

struct mot_dba {
	struct msm_dba_device_info dev_info;
	struct mutex ops_mutex;

	int gpio_sel_dsi_val;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(list_lock);
static struct mot_dba_device *mot_dba_dev;
static struct mot_dba *g_pdata;

/* Helper Functions */

static struct mot_dba *get_platform_data(void *client)
{
	struct mot_dba *pdata = NULL;
	struct msm_dba_device_info *dev;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *)client;

	if (!cinfo) {
		pr_err("%s: invalid client data\n", __func__);
		goto end;
	}

	dev = cinfo->dev;
	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct mot_dba, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}

/* DBA Ops */

static int mot_dba_check_hpd(void *client, u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int connected = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return connected;
}

static int mot_dba_power_on(void *client, bool on, u32 flags)
{
	/* This call doesn't work for MOT_DBA framework, because this will
	 * be called during probe or before mot_dba_device_enable is called
	 * therefore mot_dba will not have any registered DBA devices to call.
	 */

	return 0;
}

static int mot_dba_video_on(void *client, bool on,
			struct msm_dba_video_cfg *cfg, u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->video_on)
		ret = client_ops->video_on(mot_dba_dev->dev, on, cfg, flags);

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static int mot_dba_hdcp_enable(void *client, bool hdcp_on,
			bool enc_on, u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->hdcp_enable)
		ret = client_ops->hdcp_enable(mot_dba_dev->dev, hdcp_on,
							enc_on, flags);
	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static int mot_dba_configure_audio(void *client,
			struct msm_dba_audio_cfg *cfg, u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->configure_audio)
		ret = client_ops->configure_audio(mot_dba_dev->dev, cfg, flags);

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static int mot_dba_get_edid_size(void *client, u32 *size, u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->get_edid_size)
		ret = client_ops->get_edid_size(mot_dba_dev->dev, size, flags);

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static int mot_dba_get_raw_edid(void *client, u32 size, char *buf,
	u32 flags)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !buf || !mot_dba_dev) {
		pr_err("%s: invalid pdata/mot_dba_dev\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->get_raw_edid)
		ret = client_ops->get_raw_edid(mot_dba_dev->dev, size,
								buf, flags);

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static int mot_dba_get_dsi_config(void *client,
				struct msm_dba_dsi_cfg *dsi_config)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !dsi_config || !mot_dba_dev) {
		pr_err("%s: invalid data\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->get_dsi_config)
		ret = client_ops->get_dsi_config(mot_dba_dev->dev, dsi_config);
	else
		ret = -ENODEV;

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static u32 mot_dba_get_default_resolution(void *client)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	int ret = 0;

	pr_debug("%s+\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid data\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->get_default_resolution)
		ret = client_ops->get_default_resolution(mot_dba_dev->dev);
	else
		ret = -ENODEV;

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	return ret;
}

static bool mot_dba_get_dsi_hs_clk_always_on(void *client)
{
	struct mot_dba *pdata = get_platform_data(client);
	struct msm_dba_ops *client_ops;
	bool dsi_hs_clk_always_on = false;

	pr_debug("%s\n", __func__);
	mutex_lock(&list_lock);
	if (!pdata || !mot_dba_dev) {
		pr_err("%s: invalid data\n", __func__);
		goto exit;
	}

	mutex_lock(&pdata->ops_mutex);
	client_ops = &mot_dba_dev->dev->client_ops;
	if (client_ops && client_ops->get_dsi_hs_clk_always_on)
		dsi_hs_clk_always_on =
			client_ops->get_dsi_hs_clk_always_on(mot_dba_dev->dev);

	mutex_unlock(&pdata->ops_mutex);
exit:
	mutex_unlock(&list_lock);

	pr_debug("%s: dsi_hs_clk_always_on = %d\n",
					__func__, dsi_hs_clk_always_on);

	return dsi_hs_clk_always_on;
}

static int mot_dba_panel_parse_dt(struct platform_device *pdev,
					struct mot_dba *pdata)
{
	int ret = 0;
	u32 temp_val = 0;

	pr_debug("%s+\n", __func__);

	if (of_property_read_u32(pdev->dev.of_node, "mot_dba,sel-dsi-val",
				 &temp_val))
		pr_info("%s: failed to find mot_dba,sel-dsi-val\n", __func__);
	else
		pdata->gpio_sel_dsi_val = temp_val;

	return ret;
}

/* MOT DBA Implementation */

#include "qseecom_kernel.h"
enum JslrResult {
	Jslr_success = 0,
	Jslr_failure,
};

enum tz_jslr_cmd_type {
	TZ_JSLR_CMD_SET = 1,
	TZ_JSLR_CMD_UNSET = 2,
	TZ_JSLR_CMD_LAST = 0x7FFFFFFF
};

struct tz_jslr_req_s {
	u32 cmd_id;
	uint32_t which;
} __packed;

struct tz_jslr_rsp_s {
	u32 cmd_id;
	int32_t ret;
} __packed;

int mot_dba_device_enable(int mod_display_type)
{
	int ret = 0;
	struct mot_dba_device *cur;
	int gpio_val = 0;
	size_t res, cmd_len, rsp_len;
	struct qseecom_handle *qseecom_handle = NULL;
	struct tz_jslr_req_s *cmd;
	struct tz_jslr_rsp_s *rsp;

	pr_debug("%s+\n", __func__);
	if (!g_pdata) {
		pr_err("%s: invalid g_pdata.\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&list_lock);
	if (mot_dba_dev) {
		pr_err("%s: DBA driver already registered (%s, %d)\n",
				__func__, mot_dba_dev->dev->chip_name,
				mot_dba_dev->dev->instance_id);
		ret = -EBUSY;
		goto exit;
	}

	list_for_each_entry(cur, &device_list, list) {
		if (cur->mod_display_type == mod_display_type) {
			mot_dba_dev = cur;
			mot_dba_dev->enable = true;
			pr_info("%s: DBA driver(%s, %d) is enable\n", __func__,
				mot_dba_dev->dev->chip_name,
				mot_dba_dev->dev->instance_id);
		}
	}

	if (!mot_dba_dev) {
		pr_err("%s: Unable to find DBA driver type = %d\n", __func__,
				mod_display_type);
		ret = -EINVAL;
	} else if (g_pdata->gpio_sel_dsi_val >= 0) {
		res = qseecom_start_app(&qseecom_handle, "jslr", SZ_16K);
		if (res < 0) {
			pr_err("error loading jslr\n");
		} else {
			pr_info("jslr loaded\n");
			cmd_len = sizeof(struct tz_jslr_req_s);
			rsp_len = sizeof(struct tz_jslr_rsp_s);

			cmd = (struct tz_jslr_req_s *)qseecom_handle->sbuf;

			if (cmd_len & QSEECOM_ALIGN_MASK)
				cmd_len = QSEECOM_ALIGN(cmd_len);

			rsp = (struct tz_jslr_rsp_s *)qseecom_handle->sbuf +
				cmd_len;

			if (rsp_len & QSEECOM_ALIGN_MASK)
				rsp_len = QSEECOM_ALIGN(rsp_len);

			/* Populate command struct */
			cmd->which = 0;
			if (mod_display_type == MOD_DISPLAY_TYPE_DP)
				gpio_val = (g_pdata->gpio_sel_dsi_val ? 0 : 1);
			else
				gpio_val = g_pdata->gpio_sel_dsi_val;
			if (gpio_val)
				cmd->cmd_id = TZ_JSLR_CMD_SET;
			else
				cmd->cmd_id = TZ_JSLR_CMD_UNSET;

			/* Issue QSEECom command */
			res = qseecom_send_command(qseecom_handle, (void *)cmd,
				cmd_len, (void *)rsp, rsp_len);
			if (res < 0) {
				pr_err("error running jslr\n");
				qseecom_shutdown_app(&qseecom_handle);
			} else {
				pr_info("response from jslr %d\n", rsp->ret);
				qseecom_shutdown_app(&qseecom_handle);
			}
		}
	} else {
		pr_info("%s: gpio_sel_dsi_val not set\n", __func__);
	}

exit:
	mutex_unlock(&list_lock);
	return ret;
}

int mot_dba_device_disable(int mod_display_type)
{
	int ret = 0;
	struct mot_dba_device *cur;
	int found = 0;

	pr_debug("%s+\n", __func__);

	mutex_lock(&list_lock);
	list_for_each_entry(cur, &device_list, list) {
		if (cur->mod_display_type == mod_display_type) {
			if (cur->enable)
				cur->enable = false;
			if (cur == mot_dba_dev) {
				pr_info("%s: DBA driver(%s, %d) is disable\n",
								__func__,
				mot_dba_dev->dev->chip_name,
				mot_dba_dev->dev->instance_id);
				mot_dba_dev = NULL;
				found = 1;
			}
		}
	}

	mutex_unlock(&list_lock);
	if (!found) {
		pr_err("%s: Unable to find DBA driver type = %d\n", __func__,
							mod_display_type);
		ret = -EINVAL;
	}

	return ret;
}

int mot_dba_add_device(struct msm_dba_device_info *dev, int mod_display_type)
{
	int ret = 0;
	struct mot_dba_device *node;

	mutex_lock(&list_lock);

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		mutex_unlock(&list_lock);
		return -ENOMEM;
	}

	memset(node, 0x0, sizeof(*node));
	node->dev = dev;
	node->mod_display_type = mod_display_type;
	list_add(&node->list, &device_list);

	pr_debug("%s: Added new device (%s, %d, %d\n",
			__func__, dev->chip_name,
			dev->instance_id, node->mod_display_type);

	mutex_unlock(&list_lock);

	return ret;
}

void mot_dba_notify_clients(enum msm_dba_callback_event event)
{
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;
	struct msm_dba_device_info *dev;

	pr_debug("%s+\n", __func__);

	if (!g_pdata) {
		pr_err("%s: invalid g_pdata.\n", __func__);
		return;
	}
	dev = &g_pdata->dev_info;

	if (!dev) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);

		pr_info("%s: notifying event %d to client %s\n", __func__,
						event, c->client_name);

		if (c && c->cb)
			c->cb(c->cb_data, event);
	}

	pr_debug("%s-\n", __func__);
}

static int mot_dba_probe(struct platform_device *pdev)
{
	struct mot_dba *pdata;
	struct msm_dba_ops *client_ops;
	int ret = 0;

	if (!pdev->dev.of_node) {
		pr_err("%s: pdev not found for mot_dba\n", __func__);
		return -ENODEV;
	}


	pdata = devm_kzalloc(&pdev->dev, sizeof(struct mot_dba), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	client_ops = &pdata->dev_info.client_ops;

	client_ops->power_on        = mot_dba_power_on;
	client_ops->video_on        = mot_dba_video_on;
	client_ops->configure_audio = mot_dba_configure_audio;
	client_ops->hdcp_enable     = mot_dba_hdcp_enable;
	client_ops->get_edid_size   = mot_dba_get_edid_size;
	client_ops->get_raw_edid    = mot_dba_get_raw_edid;
	client_ops->check_hpd       = mot_dba_check_hpd;
	client_ops->get_dsi_config  = mot_dba_get_dsi_config;
	client_ops->get_default_resolution = mot_dba_get_default_resolution;
	client_ops->get_dsi_hs_clk_always_on = mot_dba_get_dsi_hs_clk_always_on;

	strlcpy(pdata->dev_info.chip_name, DRVNAME,
				sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.instance_id = 0;

	pdata->gpio_sel_dsi_val = -1;
	ret = mot_dba_panel_parse_dt(pdev, pdata);
	if (ret) {
		pr_err("%s: failed to parse dt. ret = %d\n", __func__, ret);
		goto exit;
	}

	mutex_init(&pdata->ops_mutex);
	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	ret = msm_dba_add_probed_device(&pdata->dev_info);

	dev_set_drvdata(&pdev->dev, &pdata->dev_info);
	ret = msm_dba_helper_sysfs_init(&pdev->dev);
	if (ret) {
		pr_err("%s: sysfs init failed\n", __func__);
		msm_dba_remove_probed_device(&pdata->dev_info);
		goto exit;
	}

	g_pdata = pdata;
exit:
	pr_debug("%s: ret = %d\n", __func__, ret);

	return ret;
}

static int mot_dba_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id mot_dba_dt_match[] = {
	{.compatible = "mmi,mot_dba"},
	{}
};
MODULE_DEVICE_TABLE(of, mot_dba_dt_match);

static struct platform_driver mot_dba_driver = {
	.probe = mot_dba_probe,
	.remove = mot_dba_remove,
	.driver = {
		.name = "mot_dba",
		.owner = THIS_MODULE,
		.of_match_table = mot_dba_dt_match,
	},
};

static int __init mot_dba_init(void)
{
	return platform_driver_register(&mot_dba_driver);
}

static void __exit mot_dba_exit(void)
{
	platform_driver_unregister(&mot_dba_driver);
}


late_initcall(mot_dba_init);
module_exit(mot_dba_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MOT DBA driver");
