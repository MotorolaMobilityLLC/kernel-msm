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
#include <linux/wakelock.h>
#include "video/msm_hdmi_modes.h"
#include "msm_dba_internal.h"
#include "../mdss_dsi.h"
#include "../mdss_panel.h"
#include "mot_dba.h"

#define DRVNAME "dsi_mod_display"

#define DSI_MOD_DISPLAY_EDID_SIZE 128

struct dsi_mod_display {
	bool cec_enabled;
	u32 edid_size;
	void *edid_buf;
	struct mod_display_panel_config *display_config;
	struct mod_display_dsi_config *dsi_config;
	struct msm_dba_device_info dev_info;
	bool connecting;
	struct completion dsi_on_wait;
	struct completion apba_on_wait;
	struct notifier_block panel_nb;
	struct mutex ops_mutex;
	struct wake_lock mod_disp_lock;

	int dsi_connect;
};

/* Helper Functions */
static struct dsi_mod_display *dsi_mod_display_get_platform_data(void *client)
{
	struct dsi_mod_display *pdata = NULL;
	struct msm_dba_device_info *dev = (struct msm_dba_device_info *)client;

	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct dsi_mod_display, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}


struct __attribute__((__packed__)) DTD {
	u16 pixel_clock;
	u8 h_active_lsb;
	u8 h_blanking_lsb;
	u8 h_blanking_msb : 4;
	u8 h_active_msb : 4;
	u8 v_active_lsb;
	u8 v_blanking_lsb;
	u8 v_blanking_msb : 4;
	u8 v_active_msb : 4;
	u8 h_sync_offset_lsb;
	u8 h_sync_lsb;
	u8 v_sync_lsb : 4;
	u8 v_sync_offset_lsb : 4;
	u8 v_sync_msb : 2;
	u8 v_sync_offset_msb : 2;
	u8 h_sync_msb : 2;
	u8 h_sync_offset_msb : 2;
	u8 h_size_lsb;
	u8 v_size_lsb;
	u8 v_size_msb : 4;
	u8 h_size_msb : 4;
	u8 h_border;
	u8 v_border;
	u8 bitmask;
};

struct __attribute__((__packed__)) EDID {
	u8 header[8];
	u16 vendor_id;
	u16 product_id;
	u32 serial_number;
	u8 manufacture_week;
	u8 manufacture_year;
	u8 edid_version;
	u8 edid_revision;
	u8 video_input_definition;
	u8 h_screen_size;
	u8 v_screen_size;
	u8 gamma;
	u8 feature_support;
	u8 color_characteristics[10];
	u8 established_timings[3];
	u8 standard_timings[16];
	struct DTD timing_descriptors[4];
	u8 ext_block_count;
	u8 checksum;
};

static inline u8 checksum(u8 *buf, int len)
{
	int i;
	u8 checksum = 0;

	for (i = 0; i < len; i++)
		checksum += buf[i];

	return checksum;
}

static int dsi_mod_display_conf_to_edid(struct dsi_mod_display *pdata)
{
	struct mod_display_dsi_config *dsi_config = pdata->dsi_config;
	u16 h_blanking = dsi_config->horizontal_front_porch +
		dsi_config->horizontal_back_porch +
		dsi_config->horizontal_pulse_width;
	u16 v_blanking = dsi_config->vertical_front_porch +
		dsi_config->vertical_back_porch +
		dsi_config->vertical_pulse_width;
	u32 total_pixels = (dsi_config->height + v_blanking) *
		(dsi_config->width + h_blanking);
	struct EDID edid = {
		.header = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 },
		.vendor_id = dsi_config->manufacturer_id,
		.product_id = 0xffff,
		.serial_number = 0xffffffff,
		.manufacture_week = 0xff,
		.manufacture_year = 0xff,
		.edid_version = 0x01,
		.edid_revision = 0x03,
		.video_input_definition = 0x80,
		.h_screen_size = 0xff,
		.v_screen_size = 0xff,
		.gamma = 0xff,
		.feature_support = 0xee, /* TODO */
		.color_characteristics = {
			0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff
		},
		.established_timings = { },
		.standard_timings = { },
		.timing_descriptors = {
			{
				.pixel_clock = (u16)(
					total_pixels * dsi_config->framerate /
								10 / 1000),
				.h_active_lsb = (u8)dsi_config->width & 0xff,
				.h_blanking_lsb = (u8)h_blanking,
				.h_blanking_msb = (h_blanking >> 8) & 0x0f,
				.h_active_msb = (dsi_config->width >> 8) & 0x0f,
				.v_active_lsb = dsi_config->height & 0xff,
				.v_blanking_lsb = v_blanking & 0xff,
				.v_blanking_msb = (v_blanking >> 8) & 0x0f,
				.v_active_msb = (dsi_config->height >> 8) &
									0x0f,
				.h_sync_offset_lsb =
					(u8)dsi_config->horizontal_front_porch,
				.h_sync_lsb =
					(u8)dsi_config->horizontal_pulse_width,
				.v_sync_lsb =
					dsi_config->vertical_pulse_width & 0x0f,
				.v_sync_offset_lsb =
					dsi_config->vertical_front_porch & 0x0f,
				.v_sync_msb =
					(dsi_config->vertical_pulse_width >> 4)
								& 0x03,
				.v_sync_offset_msb =
					(dsi_config->vertical_front_porch >> 4)
								& 0x03,
				.h_sync_msb =
					(dsi_config->horizontal_pulse_width >>
								8) & 0x03,
				.h_sync_offset_msb =
					(dsi_config->horizontal_front_porch >>
								8) & 0x03,
				.h_size_lsb =
					(u8)dsi_config->physical_width_dim,
				.v_size_lsb =
					(u8)dsi_config->physical_length_dim,
				.v_size_msb =
					(dsi_config->physical_length_dim >> 8) &
								0x0f,
				.h_size_msb =
					(dsi_config->physical_width_dim >> 8) &
								0x0f,
				.h_border =
					(u8)((dsi_config->horizontal_left_border
					+ dsi_config->horizontal_right_border) /
								2),
				.v_border =
					(u8)((dsi_config->vertical_top_border +
					dsi_config->vertical_bottom_border) /
								2),
				.bitmask = 0x1a, /* TODO */
			}
		},
		.ext_block_count = 0,
		.checksum = 0,
	};

	edid.checksum = -checksum((u8 *)&edid, sizeof(struct EDID) - 1);

	if (pdata->edid_buf) {
		pr_warn("%s: EDID buf is not NULL... Freeing...\n", __func__);
		kfree(pdata->edid_buf);
	}

	pdata->edid_size = sizeof(struct EDID);
	pdata->edid_buf = kzalloc(pdata->edid_size, GFP_KERNEL);
	if (!pdata->edid_buf)
		return -ENOMEM;

	pr_debug("%s: total_pixels =%d framerate=%d\n",
			__func__, total_pixels, dsi_config->framerate);
	memcpy(pdata->edid_buf, &edid, pdata->edid_size);

	return 0;
}

static void dsi_mod_display_do_hotplug(struct dsi_mod_display *pdata,
	u8 attached)
{
	pr_debug("%s: %d\n", __func__, attached);

	if (attached)
		mot_dba_notify_clients(MSM_DBA_CB_HPD_CONNECT);
	else
		mot_dba_notify_clients(MSM_DBA_CB_HPD_DISCONNECT);
}

static int dsi_mod_display_panel_event_handler(struct notifier_block *nb,
	unsigned long event, void *data)
{
	struct dsi_mod_display *pdata = container_of(nb, struct dsi_mod_display,
		panel_nb);
	struct mdss_panel_info *pinfo = (struct mdss_panel_info *)data;
	struct mdss_panel_data *panel_data = container_of(pinfo,
		struct mdss_panel_data, panel_info);
	struct mdss_dsi_ctrl_pdata *ctrl = container_of(panel_data,
		struct mdss_dsi_ctrl_pdata, panel_data);

	pr_debug("%s+: Received panel event: %lu dsi_ndx=%d dsi_connect=%d\n",
				__func__, event, ctrl->ndx, pdata->dsi_connect);

	if (ctrl->ndx != pdata->dsi_connect) {
		pr_debug("%s: Event not for this DBA\n", __func__);
		goto exit;
	}

	if (!pdata->connecting) {
		pr_debug("%s: Dont care about events while not connecting\n",
			__func__);
		goto exit;
	}

	switch (event) {
	case PANEL_EVENT_DISPLAY_ON:
		/* DSI should be LP11 Now... */
		complete(&pdata->dsi_on_wait);

		/* Block here until APBA is happy */
		pr_debug("%s: Block panel on returning...\n", __func__);
		wait_for_completion(&pdata->apba_on_wait);
		break;
	default:
		/* Dont care */
		break;
	}

exit:
	pr_debug("%s-\n", __func__);

	return 0;
}

/* DBA Ops */

static int dsi_mod_display_hdcp_enable(void *client, bool hdcp_on,
	bool enc_on, u32 flags)
{
	return 0;
}

static int dsi_mod_display_get_edid_size(void *client, u32 *size, u32 flags)
{
	struct dsi_mod_display *pdata =
			dsi_mod_display_get_platform_data(client);
	int ret = 0;

	if (!pdata) {
		pr_err("%s: invalid platform data\n", __func__);
		return ret;
	}

	pr_debug("%s\n", __func__);
	mutex_lock(&pdata->ops_mutex);

	if (pdata->edid_buf)
		*size = pdata->edid_size;
	else {
		pr_err("%s: EDID not available...\n", __func__);
		ret = -ENODEV;
	}

	mutex_unlock(&pdata->ops_mutex);

	return ret;
}

static int dsi_mod_display_get_raw_edid(void *client, u32 size, char *buf,
	u32 flags)
{
	struct dsi_mod_display *pdata =
			dsi_mod_display_get_platform_data(client);
	int ret = 0;

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s+\n", __func__);
	mutex_lock(&pdata->ops_mutex);

	if (pdata->edid_buf) {
		size = min_t(u32, size, pdata->edid_size);
		memcpy(buf, pdata->edid_buf, size);
	} else {
		pr_err("%s: EDID not available...\n", __func__);
		ret = -ENODEV;
	}

	mutex_unlock(&pdata->ops_mutex);

	return ret;
}

static int dsi_mod_display_get_dsi_config(void *client,
	struct msm_dba_dsi_cfg *dsi_config)
{
	struct dsi_mod_display *pdata =
				dsi_mod_display_get_platform_data(client);
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (dsi_config)
		memcpy(dsi_config, pdata->dsi_config,
					sizeof(struct msm_dba_dsi_cfg));
	else {
		WARN_ON(1);
		ret = -EFAULT;
	}

	return ret;
}


static int dsi_mod_display_parse_dt(struct platform_device *pdev,
					struct dsi_mod_display *pdata)
{
	int ret = 0;
	u32 temp_val = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "mod_display,dsi_connect",
								&temp_val);
	if (ret) {
		pr_warn("%s: can not find mod_display,dsi_connect. ret =%d\n",
							__func__, ret);
		pdata->dsi_connect = -1;
		/* return 0 because it is ok if dsi_connect is not defined */
		ret = 0;
	} else
		pdata->dsi_connect = temp_val;

	return ret;
}


static u32 dsi_mod_display_get_default_resolution(void *client)
{
	return HDMI_VFRMT_UNKNOWN;
}

/* Mod Display Implementation */

static int dsi_mod_display_handle_available(void *data)
{
	struct dsi_mod_display *pdata;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	pdata = (struct dsi_mod_display *)data;
	ret = mot_dba_device_enable(MOD_DISPLAY_TYPE_DSI);
	if (ret)
		pr_err("%s: fail to enable DBA device MOD_DISPLAY_TYPE_DSI\n",
						__func__);

	pr_debug("%s-\n", __func__);

	return ret;
}

static int dsi_mod_display_handle_unavailable(void *data)
{
	struct dsi_mod_display *pdata;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	pdata = (struct dsi_mod_display *)data;
	ret = mot_dba_device_disable(MOD_DISPLAY_TYPE_DSI);
	if (ret)
		pr_err("%s: fail to disable DBA device MOD_DISPLAY_TYPE_DSI\n",
							__func__);

	pr_debug("%s-\n", __func__);

	return ret;
}

static int dsi_mod_display_handle_connect(void *data)
{
	struct dsi_mod_display *pdata;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	pdata = (struct dsi_mod_display *)data;

	ret = mod_display_get_display_config(&pdata->display_config);
	if (ret) {
		pr_err("%s: Unable to get display config (ret: %d)\n",
			__func__, ret);
		goto exit;
	}
	pdata->dsi_config = (struct mod_display_dsi_config *)
		pdata->display_config->config_buf;
	dsi_mod_display_conf_to_edid(pdata);

	reinit_completion(&pdata->dsi_on_wait);
	reinit_completion(&pdata->apba_on_wait);
	wake_lock(&pdata->mod_disp_lock);

	pdata->connecting = true;
	dsi_mod_display_do_hotplug(pdata, 1);
	if (wait_for_completion_timeout(&pdata->dsi_on_wait,
		msecs_to_jiffies(5000))) {
		ret = mod_display_set_display_state(MOD_DISPLAY_ON);
		if (ret)
			pr_err("%s: Failed to set state to ON (%d)\n", __func__,
				ret);
	} else {
		pr_err("%s: DSI never came up!\n", __func__);
		ret = -ETIMEDOUT;
	}
	pdata->connecting = false;
	complete_all(&pdata->apba_on_wait);

	if (ret) {
		pr_err("%s: Connect failed, cleanup DSI\n", __func__);
		dsi_mod_display_do_hotplug(pdata, 0);

		kfree(pdata->edid_buf);
		pdata->edid_buf = NULL;
		kfree(pdata->display_config);
		pdata->display_config = NULL;
		wake_unlock(&pdata->mod_disp_lock);
	}

exit:
	pr_debug("%s-\n", __func__);

	return ret;
}

static int dsi_mod_display_handle_disconnect(void *data)
{
	struct dsi_mod_display *pdata;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	pdata = (struct dsi_mod_display *)data;

	dsi_mod_display_do_hotplug(pdata, 0);
	wake_unlock(&pdata->mod_disp_lock);
	wake_lock_timeout(&pdata->mod_disp_lock, 2 * HZ);

	mod_display_set_display_state(MOD_DISPLAY_OFF);

	kfree(pdata->edid_buf);
	pdata->edid_buf = NULL;
	kfree(pdata->display_config);
	pdata->display_config = NULL;

	pr_debug("%s-\n", __func__);

	return ret;
}

static struct mod_display_ops dsi_mod_display_ops = {
	.handle_available = dsi_mod_display_handle_available,
	.handle_unavailable = dsi_mod_display_handle_unavailable,
	.handle_connect = dsi_mod_display_handle_connect,
	.handle_disconnect = dsi_mod_display_handle_disconnect,
	.data = NULL,
};

static struct mod_display_impl_data dsi_mod_display_impl = {
	.mod_display_type = MOD_DISPLAY_TYPE_DSI,
	.ops = &dsi_mod_display_ops,
};

static int dsi_mod_display_probe(struct platform_device *pdev)
{
	struct dsi_mod_display *pdata;
	struct msm_dba_ops *client_ops;
	int ret = 0;

	if (!pdev->dev.of_node) {
		pr_err("%s: pdev not found for dsi_mod_display\n", __func__);
		return -ENODEV;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct dsi_mod_display),
		GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	client_ops = &pdata->dev_info.client_ops;

	client_ops->hdcp_enable     = dsi_mod_display_hdcp_enable;
	client_ops->get_edid_size   = dsi_mod_display_get_edid_size;
	client_ops->get_raw_edid    = dsi_mod_display_get_raw_edid;
	client_ops->get_dsi_config  = dsi_mod_display_get_dsi_config;
	client_ops->get_default_resolution =
		dsi_mod_display_get_default_resolution;

	strlcpy(pdata->dev_info.chip_name, DRVNAME,
		sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.instance_id = 0;

	ret = dsi_mod_display_parse_dt(pdev, pdata);
	if (ret) {
		pr_err("%s: failed to parse dt. ret = %d\n", __func__, ret);
		goto exit;
	}

	mutex_init(&pdata->ops_mutex);
	mutex_init(&pdata->dev_info.dev_mutex);
	wake_lock_init(&pdata->mod_disp_lock, WAKE_LOCK_SUSPEND,
			"mod_disp_wake_lock");

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	ret = mot_dba_add_device(&pdata->dev_info, MOD_DISPLAY_TYPE_DSI);

	dev_set_drvdata(&pdev->dev, &pdata->dev_info);
	ret = msm_dba_helper_sysfs_init(&pdev->dev);
	if (ret) {
		pr_err("%s: sysfs init failed\n", __func__);
		msm_dba_remove_probed_device(&pdata->dev_info);
		goto exit;
	}

	pdata->connecting = false;
	init_completion(&pdata->dsi_on_wait);
	init_completion(&pdata->apba_on_wait);

	pdata->panel_nb.notifier_call = dsi_mod_display_panel_event_handler;
	if (!panel_register_notifier(&pdata->panel_nb))
		pr_debug("%s: registered panel notifier\n", __func__);
	else {
		pr_err("%s: unable to register panel notifier\n", __func__);
		msm_dba_helper_sysfs_remove(&pdev->dev);
		msm_dba_remove_probed_device(&pdata->dev_info);
		goto exit;
	}

	dsi_mod_display_ops.data = (void *)pdata;
	mod_display_register_impl(&dsi_mod_display_impl);

exit:
	pr_debug("%s: ret = %d\n", __func__, ret);

	return 0;
}

static int dsi_mod_display_remove(struct platform_device *pdev)
{
	struct dsi_mod_display *pdata = platform_get_drvdata(pdev);

	wake_lock_destroy(&pdata->mod_disp_lock);

	return 0;
}

static const struct of_device_id dsi_mod_display_dt_match[] = {
	{.compatible = "mmi,dsi_mod_display"},
	{}
};
MODULE_DEVICE_TABLE(of, dsi_mod_display_dt_match);

static struct platform_driver dsi_mod_display_driver = {
	.probe = dsi_mod_display_probe,
	.remove = dsi_mod_display_remove,
	.driver = {
		.name = "dsi_mod_display",
		.owner = THIS_MODULE,
		.of_match_table = dsi_mod_display_dt_match,
	},
};

static int __init dsi_mod_display_init(void)
{
	return platform_driver_register(&dsi_mod_display_driver);
}

static void __exit dsi_mod_display_exit(void)
{
	platform_driver_unregister(&dsi_mod_display_driver);
}

late_initcall(dsi_mod_display_init);
module_exit(dsi_mod_display_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI Mod Display driver");
