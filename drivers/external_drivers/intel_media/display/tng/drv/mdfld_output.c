/*
 * Copyright (c)  2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicensen
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Thomas Eaton <thomas.g.eaton@intel.com>
 * Scott Rowe <scott.m.rowe@intel.com>
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include "displays/hdmi.h"

#include "psb_drv.h"
#include "android_hdmi.h"

#ifdef CONFIG_SUPPORT_MIPI
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_output.h"
#include "mdfld_dsi_output.h"
#include "displays/jdi_vid.h"
#include "displays/jdi_cmd.h"
#include "displays/cmi_vid.h"
#include "displays/cmi_cmd.h"
#include "displays/sharp10x19_cmd.h"
#include "displays/sharp25x16_vid.h"
#include "displays/sharp25x16_cmd.h"
#include "displays/sdc16x25_8_cmd.h"
#include "displays/sdc25x16_cmd.h"
#include "displays/jdi25x16_vid.h"
#include "displays/jdi25x16_cmd.h"

static struct intel_mid_panel_list panel_list[] = {
	{JDI_7x12_VID, MDFLD_DSI_ENCODER_DPI, jdi_vid_init},
	{JDI_7x12_CMD, MDFLD_DSI_ENCODER_DBI, jdi_cmd_init},
	{CMI_7x12_VID, MDFLD_DSI_ENCODER_DPI, cmi_vid_init},
	{CMI_7x12_CMD, MDFLD_DSI_ENCODER_DBI, cmi_cmd_init},
	{SHARP_10x19_CMD, MDFLD_DSI_ENCODER_DBI, sharp10x19_cmd_init},
	{SHARP_10x19_DUAL_CMD, MDFLD_DSI_ENCODER_DBI, sharp10x19_cmd_init},
	{SHARP_25x16_VID, MDFLD_DSI_ENCODER_DPI, sharp25x16_vid_init},
	{SHARP_25x16_CMD, MDFLD_DSI_ENCODER_DBI, sharp25x16_cmd_init},
	{JDI_25x16_VID, MDFLD_DSI_ENCODER_DPI, jdi25x16_vid_init},
	{JDI_25x16_CMD, MDFLD_DSI_ENCODER_DBI, jdi25x16_cmd_init},
	{SDC_16x25_CMD, MDFLD_DSI_ENCODER_DBI, sdc16x25_8_cmd_init},
	{SDC_25x16_CMD, MDFLD_DSI_ENCODER_DBI, sdc25x16_cmd_init},
};

enum panel_type get_panel_type(struct drm_device *dev, int pipe)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;

	return dev_priv->panel_id;
}

bool is_dual_dsi(struct drm_device *dev)
{
	if ((get_panel_type(dev, 0) == SHARP_25x16_VID) ||
		(get_panel_type(dev, 0) == SHARP_25x16_CMD) ||
		(get_panel_type(dev, 0) == SHARP_10x19_DUAL_CMD) ||
		(get_panel_type(dev, 0) == SDC_16x25_CMD) ||
		(get_panel_type(dev, 0) == SDC_25x16_CMD) ||
		(get_panel_type(dev, 0) == JDI_25x16_CMD) ||
		(get_panel_type(dev, 0) == JDI_25x16_VID))
		return true;
	else return false;
}

bool is_dual_panel(struct drm_device *dev)
{
	if (get_panel_type(dev, 0) == SHARP_10x19_DUAL_CMD)
		return true;
	else return false;
}

/**
 * is_panel_vid_or_cmd(struct drm_device *dev)
 * Function return value: panel encoder type
 */
mdfld_dsi_encoder_t is_panel_vid_or_cmd(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
	int i;

	for (i = 0; i < ARRAY_SIZE(panel_list); i++) {
		if (panel_list[i].p_type == dev_priv->panel_id)
			return panel_list[i].encoder_type;
	}
	DRM_INFO("%s : Could not find panel: pabel_id = %d, ARRAY_SIZE = %zd",
			__func__, dev_priv->panel_id, ARRAY_SIZE(panel_list));
	DRM_INFO("%s : Crashing...", __func__);
	BUG();

	/* This should be unreachable */
	return 0;
}
#endif

void init_panel(struct drm_device *dev, int mipi_pipe, enum panel_type p_type)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *) dev->dev_private;
#ifdef CONFIG_SUPPORT_MIPI
	struct panel_funcs *p_funcs = NULL;
	int i = 0, ret = 0;
#endif
	struct drm_connector *connector;

#ifdef CONFIG_SUPPORT_HDMI
	if (p_type == HDMI) {
		PSB_DEBUG_ENTRY("GFX: Initializing HDMI");
		android_hdmi_driver_init(dev, &dev_priv->mode_dev);
		if (!IS_MRFLD(dev))
			return;

		mutex_lock(&dev->mode_config.mutex);
		list_for_each_entry(connector,
				&dev->mode_config.connector_list, head) {
			if ((connector->connector_type !=
						DRM_MODE_CONNECTOR_DSI) &&
					(connector->connector_type !=
					 DRM_MODE_CONNECTOR_LVDS))
				connector->polled = DRM_CONNECTOR_POLL_HPD;
		}
		mutex_unlock(&dev->mode_config.mutex);

		return;
	}
#endif

#ifdef CONFIG_SUPPORT_MIPI
	dev_priv->cur_pipe = mipi_pipe;
	p_funcs = kzalloc(sizeof(struct panel_funcs), GFP_KERNEL);

	for (i = 0; i < ARRAY_SIZE(panel_list); i++) {
		if (panel_list[i].p_type == dev_priv->panel_id) {
			panel_list[i].panel_init(
					dev,
					p_funcs);
			ret = mdfld_dsi_output_init(dev,
					mipi_pipe,
					NULL,
					p_funcs);
			if (ret)
				kfree(p_funcs);
			break;
		}
	}
#endif
}

void mdfld_output_init(struct drm_device *dev)
{
#ifdef CONFIG_SUPPORT_MIPI
	enum panel_type p_type1;

	/* MIPI panel 1 */
	p_type1 = get_panel_type(dev, 0);
	init_panel(dev, 0, p_type1);

#ifdef CONFIG_MDFD_DUAL_MIPI
	{
		/* MIPI panel 2 */
		enum panel_type p_type2;
		p_type2 = get_panel_type(dev, 2);
		init_panel(dev, 2, p_type2);
	}
#endif
#endif

#ifdef CONFIG_SUPPORT_HDMI
	/* HDMI panel */
	init_panel(dev, 0, HDMI);
#endif
}

