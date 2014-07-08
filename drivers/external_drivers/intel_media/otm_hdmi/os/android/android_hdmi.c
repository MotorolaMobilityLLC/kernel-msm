/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include "psb_intel_display.h"
#include "psb_intel_reg.h"
#include "psb_drv.h"
#include "psb_powermgmt.h"
#include "mdfld_output.h"
#include "mdfld_hdmi_audio_if.h"
#include "otm_hdmi_types.h"
#include "otm_hdmi.h"
#include "android_hdmi.h"
#ifdef OTM_HDMI_HDCP_ENABLE
#include "hdcp_api.h"
#endif
#ifdef CONFIG_SUPPORT_MIPI
#include "mdfld_dsi_output.h"
#endif

/* Include file for sending uevents */
#include "psb_umevents.h"

/* External state dependency */
extern int hdmi_state;

/* External default scaling mode dependency */
extern int default_hdmi_scaling_mode;

static const struct {
	int width, height, htotal, vtotal, dclk, vrefr, vic, par;
} vic_formats[12] = {
	/* 640x480p60 4:3 */
	{  640,  480,  800,  525,  25200, 60,  1 , OTM_HDMI_PAR_4_3  },
	/* 720x480p60 4:3 */
	{  720,  480,  858,  525,  27027, 60,  2 , OTM_HDMI_PAR_4_3  },
	/* 720x480p60 16:9 */
	{  720,  480,  858,  525,  27027, 60,  3 , OTM_HDMI_PAR_16_9 },
	/* 1280x720p60 16:9 */
	{ 1280,  720, 1650,  750,  74250, 60,  4 , OTM_HDMI_PAR_16_9 },
	/* 1920x1080p60 16:9 */
	{ 1920, 1080, 2200, 1125, 148500, 60, 16 , OTM_HDMI_PAR_16_9 },
	/* 720x576p50 4:3 */
	{  720,  576,  864,  625,  27000, 50, 17 , OTM_HDMI_PAR_4_3  },
	/* 720x576p50 16:9 */
	{  720,  576,  864,  625,  27000, 50, 18 , OTM_HDMI_PAR_16_9 },
	/* 1280x720p50 16:9 */
	{  1280,  720, 1980,  750,  74250, 50, 19 , OTM_HDMI_PAR_16_9 },
	/* 1920x1080p50 16:9 */
	{  1920, 1080, 2640, 1125, 148500, 50, 31 , OTM_HDMI_PAR_16_9 },
	/* 1920x1080p24 16:9 */
	{  1920, 1080, 2750, 1125,  74250, 24, 32 , OTM_HDMI_PAR_16_9 },
	/* 1920x1080p25 16:9 */
	{  1920, 1080, 2640, 1125,  74250, 25, 33 , OTM_HDMI_PAR_16_9 },
	/* 1920x1080p30 16:9 */
	{  1920, 1080, 2200, 1125,  74250, 30, 34 , OTM_HDMI_PAR_16_9 },
};

/* Function declarations for interrupt routines */
static irqreturn_t android_hdmi_irq_callback(int irq, void *data);
static irqreturn_t __hdmi_irq_handler_bottomhalf(void *data);

static int calculate_refresh_rate(struct drm_display_mode *mode);
void android_hdmi_encoder_restore_wq(struct work_struct *work);

/*
 * mode array struct to support EDID testing
 */
#define DEBUG_MODES 100
static struct debug_modes__t {
	int clk;
	int frq;
	char name[DRM_DISPLAY_MODE_LEN + 1];
} arr_modes[DEBUG_MODES];

static u32 debug_modes_count;

#define OTM_HDMI_I2C_ADAPTER_NUM 10
#define OTM_HDMI_PIPE_NUM 1
#define OTM_HDMI_MAX_DDC_WRITE_SIZE 20
#define OTM_HDMI_MAX_HDISPLAY 1920
#define OTM_HDMI_MAX_VDISPLAY 1080

/* Default HDMI Edid - 640x480p 720x480p 1280x720p */
static unsigned char default_edid[] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x25, 0xD4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x14, 0x01, 0x03, 0x80, 0x00, 0x00, 0xFF,
	0x2A, 0xBA, 0x45, 0xA1, 0x59, 0x55, 0x9D, 0x28,
	0x0D, 0x50, 0x54, 0x20, 0x00, 0x00, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D,
	0x00, 0x72, 0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28,
	0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E,
	0x8F, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10,
	0x10, 0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x49,
	0x4E, 0x54, 0x45, 0x4C, 0x2D, 0x54, 0x56, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x0C, 0x37, 0x3D, 0x1F, 0x31, 0x0F, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xCE,
	0x02, 0x03, 0x13, 0x41, 0x42, 0x04, 0x02, 0x23,
	0x09, 0x07, 0x07, 0x67, 0x03, 0x0C, 0x00, 0x10,
	0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B
};

static struct edid *drm_get_edid_retry(struct drm_connector *connector,
				struct i2c_adapter *adapter, void *context)
{
#define N_EDID_READ_RETRIES 5
	int retries = N_EDID_READ_RETRIES;
	struct edid *edid = NULL;

	do {
		if (retries != N_EDID_READ_RETRIES) {
			/* 50ms delay helps successfully read edid
			 * on agilent HDMI analyzer*/
			msleep(50);
			/* check for hdmi status before retrying */
			if (otm_hdmi_get_cable_status(context) == false)
				break;
			pr_debug("retrying edid read after delay\n");
		}
		edid = drm_get_edid(connector, adapter);
	} while(edid == NULL && --retries &&
			otm_hdmi_get_cable_status(context));

	return edid;
}

/*store the state whether the edid is ready
 *in HPD (1) or not (0)*/
static int edid_ready_in_hpd = 0;

/**
 * This function handles bottom half of HDMI hotplug interrupts
 * @data	: android hdmi private structure
 *
 * Returns the behavior of the interrupt handler
 *	IRQ_HANDLED - if interrupt handled
 * This function handles bottom half of HDMI hotplug interrupts.
 * IRQ interrupt bottomhalf handler callback. This callback
 * will be called for hdmi plug/unplug interrupts
 */
static irqreturn_t __hdmi_irq_handler_bottomhalf(void *data)
{
	struct android_hdmi_priv *hdmi_priv = data;
	static int processed_hdmi_status = -1;

	if (hdmi_priv == NULL || !hdmi_priv->dev)
		return IRQ_HANDLED;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_FORCE_POWER_ON)) {
		pr_err("Unable to power on display island!");
		return IRQ_HANDLED;
	} else {
		struct drm_mode_config *mode_config = NULL;
		struct edid *edid = NULL;
		struct drm_connector *connector = NULL;
		struct i2c_adapter *adapter = NULL;
		bool hdmi_status = 0;
		char *uevent_string = NULL;

		otm_hdmi_power_rails_on();
		/* Check HDMI status, read EDID only if connected */
		hdmi_status = otm_hdmi_get_cable_status(hdmi_priv->context);

		/* if the cable status has not changed return */
		if (hdmi_status == processed_hdmi_status) {
			if (!hdmi_status)
				otm_hdmi_power_rails_off();
			ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
			return IRQ_HANDLED;
		}

		if (hdmi_status) {
			/* Debounce for atleast 60ms in order for the
			* cable status to have stabilized
			*/
			msleep(60);
			hdmi_status =
				otm_hdmi_get_cable_status(hdmi_priv->context);
		}
		processed_hdmi_status = hdmi_status;

#ifdef OTM_HDMI_HDCP_ENABLE
		otm_hdmi_hdcp_set_hpd_state(hdmi_priv->context, hdmi_status);
#endif
		if (!hdmi_status)
			goto exit;

		adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);
		if (!adapter) {
			pr_err("Unable to get i2c adapter for HDMI");
			goto exit;
		}

		mode_config = &hdmi_priv->dev->mode_config;
		list_for_each_entry(connector,
					&mode_config->connector_list,
					head) {
			if (DRM_MODE_CONNECTOR_DVID ==
				connector->connector_type) {
				edid = (struct edid *)
					drm_get_edid(connector, adapter);
				if (edid) {
					if (drm_detect_hdmi_monitor(edid))
						/* MONITOR_TYPE_HDMI */
						hdmi_priv->monitor_type =
							MONITOR_TYPE_HDMI;
					else
						/* MONITOR_TYPE_DVI */
						hdmi_priv->monitor_type =
							MONITOR_TYPE_DVI;
					/* Store raw edid in HDMI context */
					otm_hdmi_set_raw_edid(
						hdmi_priv->context,
						(char *)edid);
					/* Raw edid is ready in HDMI context */
					edid_ready_in_hpd = 1;
					kfree(edid);
					pr_debug("Edid Read Success");
				} else {
					pr_err("Edid Read failed");
					/* Retry in next get modes */
					edid_ready_in_hpd = 0;
				}
				break;
			}
		}
exit:
		/* Notify user space */
		pr_debug("%s: HDMI hot plug state  = %d\n", __func__, hdmi_status);

		if (hdmi_status) {
			/* hdmi_state indicates that hotplug event happens */
			hdmi_state = 1;
			uevent_string = "HOTPLUG_IN=1";
			psb_sysfs_uevent(hdmi_priv->dev, uevent_string);
		} else {
			otm_hdmi_power_rails_off();
			hdmi_state = 0;
			edid_ready_in_hpd = 0;
			uevent_string = "HOTPLUG_OUT=1";
			psb_sysfs_uevent(hdmi_priv->dev, uevent_string);

			switch_set_state(&hdmi_priv->sdev, 0);
			pr_info("%s: hdmi state switched to %d\n", __func__,
			hdmi_priv->sdev.state);
		}

		drm_helper_hpd_irq_event(hdmi_priv->dev);
	}
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	return IRQ_HANDLED;
}

#ifdef OTM_HDMI_HDCP_ENABLE
static int hdmi_ddc_read_write(bool read,
			uint8_t i2c_addr,
			uint8_t offset,
			uint8_t *buffer,
			int size)
{
	struct i2c_adapter *adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);
	struct i2c_msg msgs[2];
	int num_of_msgs = 0;
	uint8_t wr_buffer[OTM_HDMI_MAX_DDC_WRITE_SIZE];

	/* Use one i2c message to write and two to read as some
	 * monitors don't handle two write messages properly
	*/
	if (read) {
		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = 1,
		msgs[0].buf    = &offset,

		msgs[1].addr   = i2c_addr,
		msgs[1].flags  = ((read) ? I2C_M_RD : 0),
		msgs[1].len    = size,
		msgs[1].buf    = buffer,

		num_of_msgs = 2;
	} else {
		BUG_ON(size + 1 > OTM_HDMI_MAX_DDC_WRITE_SIZE);

		wr_buffer[0] = offset;
		memcpy(&wr_buffer[1], buffer, size);

		msgs[0].addr   = i2c_addr,
		msgs[0].flags  = 0,
		msgs[0].len    = size + 1,
		msgs[0].buf    = wr_buffer,

		num_of_msgs = 1;
	}

	if (adapter != NULL && i2c_transfer(adapter, msgs, num_of_msgs) ==
								num_of_msgs)
		return 1;

	return 0;
}
#endif

/**
 * IRQ interrupt bottomhalf handler callback.
 * @irq		: IRQ number
 * @data		: hdmi_priv data
 *
 * Returns IRQ_HANDLED when the interrupt has been handled.
 * IRQ interrupt bottomhalf handler callback.
 * This callback will be called for hdmi plug/unplug interrupts.
 */
static irqreturn_t android_hdmi_irq_callback(int irq, void *data)
{
	pr_debug("%s: IRQ Interrupt callback", __func__);

	return __hdmi_irq_handler_bottomhalf(data);
}

int android_hdmi_irq_test(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;

	return __hdmi_irq_handler_bottomhalf(hdmi_priv);
}


/**
 * This function sets the hdmi driver during bootup
 * @dev		: handle to drm_device
 *
 * Returns nothing
 *
 * This function is called from psb driver to setup the
 * hdmi driver. Called only once during boot-up of the system
 */
void android_hdmi_driver_setup(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv;
	int ret;

	pr_info("Enter %s", __func__);

	/* HDMI private data */
	hdmi_priv = kzalloc(sizeof(struct android_hdmi_priv), GFP_KERNEL);
	if (!hdmi_priv) {
		pr_err("failed to allocate memory");
		goto out;
	}

	pr_debug("%s: Initialize the HDMI device\n", __func__);
	/* Initialize the HDMI context */
	if (otm_hdmi_device_init(&(hdmi_priv->context), dev->pdev)) {
		pr_err("failed to initialize hdmi device\n");
		goto free;
	}

	hdmi_priv->dev = dev;

	hdmi_priv->hdmib_reg = HDMIB_CONTROL;
	hdmi_priv->monitor_type = MONITOR_TYPE_HDMI;
	hdmi_priv->is_hdcp_supported = true;

	dev_priv->hdmi_priv = (void *)hdmi_priv;

	/* Register hdmi switch_dev */
	hdmi_priv->sdev.name = "hdmi";
	if (switch_dev_register(&hdmi_priv->sdev) < 0) {
		pr_err("%s: Hdmi switch registration failed\n", __func__);
		goto free;
	}

	/* Register callback to be used with Hotplug interrupts */
	ret = otm_hdmi_hpd_callback_register(hdmi_priv->context,
					     &android_hdmi_irq_callback,
					     (void *)hdmi_priv);
	if (ret) {
		pr_err("Failed to register callback for hotplug!\n");
		goto free;
	}

	pr_info("%s: Done with driver setup\n", __func__);
	pr_info("Exit %s\n", __func__);
	return;
free:
	kfree(hdmi_priv);
out:
	pr_info("Exit %s\n", __func__);
	return;
}

/* structure for hdmi cmdline module
 * don't upstream the code
 */
typedef struct {
	int hdisplay, vdisplay;
	int refresh;
	int refresh_specified;
	int vic;
	int vic_specified;
	int specified; /* 1: cmdline_mode is set */
} otm_cmdline_mode;

static otm_cmdline_mode cmdline_mode = { 0, 0, 0, 0, 0, 0, 0 };

/**
 * This function is used by external tools to force modes
 * @cmdoption		: cmd line option parameter
 *
 * Returns -1 on error 0 on success -2 on invalid output
 * This function gets the input parameters mentioned and sets the
 * driver to the mentioned mode. These utility functions are
 * for validating the various modes and useful for compliance
 * testing as well as easy debug
 */
int otm_cmdline_parse_option(char *cmdoption)
{
	int ret = 0;
	int namelen = 0;
	int i;
	int v_spec = 0;
	char *name;
	if (NULL == cmdoption)
		return -1;

	cmdline_mode.specified = 0;
	cmdline_mode.refresh_specified = 0;
	cmdline_mode.vic_specified = 0;

	name = cmdoption;
	namelen = strlen(name);
	for (i = namelen-1; i >= 0; i--) {
		switch (name[i]) {
		case '@':
			namelen = i;
			cmdline_mode.refresh =
				simple_strtol(&name[i+1], NULL, 10);
			cmdline_mode.refresh_specified = 1;
			break;
		case 'x':
		case 'X':
			cmdline_mode.vdisplay =
				simple_strtol(&name[i+1], NULL, 10);
			v_spec = 1;
			break;
		case '0' ... '9':
			break;
		default:
			/* invalid input */
			return -2;
		}
	}

	if ((i < 0) && (1 == v_spec))
		cmdline_mode.hdisplay = simple_strtol(name, NULL, 10);

	cmdline_mode.specified = 1;
	return ret;
}
EXPORT_SYMBOL_GPL(otm_cmdline_parse_option);

/**
 * This function is used by external tools to force the vic#
 * @vic		: vic number
 *
 * Returns 0 on success and -1 on invalid input vic
 * This function gets the input parameters mentioned and sets the
 * driver to the mentioned vic number. These utility functions are
 * for validating the various modes and useful for compliance
 * testing as well as easy debug
 */
int otm_cmdline_set_vic_option(int vic)
{
	int i = 0;

	pr_debug("Entered %s\n", __func__);
	cmdline_mode.specified = 0;
	cmdline_mode.refresh_specified = 0;
	cmdline_mode.vic_specified = 0;

	for (i = 0; i < ARRAY_SIZE(vic_formats); i++) {
		if (vic == vic_formats[i].vic) {
			cmdline_mode.refresh = vic_formats[i].vrefr;
			cmdline_mode.hdisplay = vic_formats[i].width;
			cmdline_mode.vdisplay = vic_formats[i].height;
			cmdline_mode.vic = vic;
			cmdline_mode.specified = 1;
			cmdline_mode.refresh_specified = 1;
			cmdline_mode.vic_specified = 1;
			pr_debug("%s:Command line mode option set to VIC: %d\n",
				 __func__, vic);
			return 0;
		}
	}

	pr_debug("HDMI cmdline: Unsupported VIC(%d) specified\n", vic);
	return -1;
}
EXPORT_SYMBOL_GPL(otm_cmdline_set_vic_option);

/**
 * This function is used by tools to print the cmdline options
 *
 * Returns nothing
 * This function is used by external tools to print
 * the cmdline options passed through tools
 */
void otm_print_cmdline_option(void)
{
	if (1 == cmdline_mode.specified) {
		if (1 == cmdline_mode.vic_specified)
			printk(KERN_INFO "HDMI cmdline option: %dx%d@%d (%d)\n",
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh,
				cmdline_mode.vic);
		else if (1 == cmdline_mode.refresh_specified)
			printk(KERN_INFO "HDMI cmdline option: %dx%d@%d\n",
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh);
		else
			printk(KERN_INFO "HDMI cmdline option: %dx%d\n",
				cmdline_mode.hdisplay, cmdline_mode.vdisplay);
	} else
		printk(KERN_INFO "HDMI cmdline option is not set\n");
}
EXPORT_SYMBOL_GPL(otm_print_cmdline_option);

/**
 * DRM connector helper routine.
 * @connector	: drm_connector handle
 * @mode		: drm_display_mode handle
 *
 * Returns integer values which tell whether the hdmi mode
 * is valid or not
 * MODE_CLOCK_LOW - mode clock less than min pixel clock value
 * MODE_CLOCK_HIGH - mode clock greater than min pixel clock value
 * MODE_BAD - mode values are incorrect
 * MODE_OK - mode values are correct
 * MODE_NO_DBLESCAN - double scan mode not supported
 * MODE_NO_INTERLACE - interlace mode not supported
 * This is the DRM connector helper routine
 */
int android_hdmi_mode_valid(struct drm_connector *connector,
				struct drm_display_mode *mode)
{
	unsigned int pc_min, pc_max;
	struct drm_display_mode *mode_entry, *t;
	struct drm_display_mode *saved_mode = NULL;
	int active_region = 0, vrefresh = 0;
	int saved_active_region = 0, saved_vrefresh = 0;
	int ret = MODE_OK;

	pr_debug("display info. hdisplay = %d, vdisplay = %d, clock = %d.\n",
			mode->hdisplay, mode->vdisplay, mode->clock);

	if (mode->hdisplay > OTM_HDMI_MAX_HDISPLAY) {
		ret = MODE_BAD_HVALUE;
		goto err;
	}

	if (mode->vdisplay > OTM_HDMI_MAX_VDISPLAY) {
		ret = MODE_BAD_VVALUE;
		goto err;
	}

	/* Restricting modes within the supported pixel clock */
	if (OTM_HDMI_SUCCESS == otm_hdmi_get_pixel_clock_range(
					&pc_min, &pc_max)) {
		if (mode->clock < pc_min) {
			pr_debug("pruned mode %dx%d@%d.\n",
				mode->hdisplay,
				mode->vdisplay,
				mode->clock);
			ret = MODE_CLOCK_LOW;
			goto err;
		}
		if (mode->clock > pc_max) {
			pr_debug("pruned mode %dx%d@%d.\n",
				mode->hdisplay,
				mode->vdisplay,
				mode->clock);
			ret = MODE_CLOCK_HIGH;
			goto err;
		}
	}

#if defined(OTM_HDMI_UNIT_TEST) && defined (OTM_HDMI_CMDLINE)
	/* if cmdline_mode is set, prune all other modes. */
	if (1 == cmdline_mode.specified) {
		if ((cmdline_mode.hdisplay != mode->hdisplay) ||
			(cmdline_mode.vdisplay != mode->vdisplay) ||
			((1 == cmdline_mode.refresh_specified) &&
			(cmdline_mode.refresh !=
			calculate_refresh_rate(mode)))) {
			return MODE_BAD;
		}
	}
#endif

	if (mode->type == DRM_MODE_TYPE_USERDEF)
		return MODE_OK;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		pr_debug("pruned mode %dx%d@%dHz. Double scan not supported.\n",
			mode->hdisplay,
			mode->vdisplay,
			calculate_refresh_rate(mode));
		ret = MODE_NO_DBLESCAN;
		goto err;
	}
	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		pr_debug("pruned mode %dx%d@%dHz. Interlace not supported.\n",
			mode->hdisplay,
			mode->vdisplay,
			calculate_refresh_rate(mode));
		ret = MODE_NO_INTERLACE;
		goto err;
	}
	return MODE_OK;

err:
	mode->status = ret;

	if (mode->type & DRM_MODE_TYPE_PREFERRED) {
		mode->type &= ~DRM_MODE_TYPE_PREFERRED;

		/*
		 * Find the mode with maximum active region and refresh rate in
		 * the supported mode list, and set it as preferred one.
		 */
		list_for_each_entry_safe(mode_entry, t,
				&connector->modes, head) {
			if (mode_entry->status != MODE_OK)
				continue;

			active_region =
				mode_entry->hdisplay * mode_entry->vdisplay;

			if (active_region >= saved_active_region) {
				saved_active_region = active_region;
				vrefresh = drm_mode_vrefresh(mode_entry);

				if ((vrefresh >= saved_vrefresh)) {
					saved_vrefresh = vrefresh;
					saved_mode = mode_entry;
				}
			}
		}

		if (saved_mode)
			saved_mode->type |= DRM_MODE_TYPE_PREFERRED;
	}

	return ret;
}


/**
 * This function maps the timings to drm_display_mode
 * @timings	: This holds the timings information
 * @dev		: drm_device handle
 *
 * Returns the mapped drm_display_mode
 * This function maps the timings in EDID information
 * to drm_display_mode and returns the same
 */
static struct drm_display_mode
*android_hdmi_get_drm_mode_from_pdt(const otm_hdmi_timing_t *timings,
				    struct drm_device *dev)
{
	struct drm_display_mode *mode;
	int i;
	static const struct {
	int w, h;
	} cea_interlaced[7] = {
		{ 1920, 1080 },
		{  720,  480 },
		{ 1440,  480 },
		{ 2880,  480 },
		{  720,  576 },
		{ 1440,  576 },
		{ 2880,  576 },
	};

	if (timings == NULL || dev == NULL)
		return NULL;

	mode = drm_mode_create(dev);
	if (mode == NULL)
		return NULL;

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->clock = timings->dclk;

	mode->hdisplay = timings->width;
	mode->hsync_start = timings->hsync_start;
	mode->hsync_end = timings->hsync_end;
	mode->htotal = timings->htotal;

	mode->vdisplay = timings->height;
	mode->vsync_start = timings->vsync_start;
	mode->vsync_end = timings->vsync_end;
	mode->vtotal = timings->vtotal;

	if (timings->mode_info_flags & PD_SCAN_INTERLACE) {

		mode->flags |= DRM_MODE_FLAG_INTERLACE;

		for (i = 0; i < ARRAY_SIZE(cea_interlaced); i++) {
			if ((mode->hdisplay == cea_interlaced[i].w) &&
			    (mode->vdisplay == cea_interlaced[i].h / 2)) {
				mode->vdisplay *= 2;
				mode->vsync_start *= 2;
				mode->vsync_end *= 2;
				mode->vtotal *= 2;
				mode->vtotal |= 1;
			}
		}
	}

	mode->flags |= (timings->mode_info_flags & PD_HSYNC_HIGH) ?
		DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;
	mode->flags |= (timings->mode_info_flags & PD_VSYNC_HIGH) ?
		DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

	/* Store aspect ratio information */
	mode->flags |= (timings->mode_info_flags & OTM_HDMI_PAR_16_9) ?
		DRM_MODE_FLAG_PAR16_9 : DRM_MODE_FLAG_PAR4_3;

	return mode;
}

/**
 * helper function to check whether two clocks can fall into the same VIC.
 *
 * Returns: true if possible, false otherwise.
 */
static bool __android_check_clock_match(int target, int reference)
{
	/* check target clock is in range of 60Hz or 59.94 (reference * 1000/1001) with
	 * (-0.5%, +0.5%) tolerance. Based on CEA spec, when determining whether two video timings
	 * are identical, clock frequencey within (-0.5%, +0.5%) tolerance should be considered
	 * as the same.
	 */

	if (target >= DIV_ROUND_UP(reference * 995, 1001) &&
		target <= DIV_ROUND_UP(reference * 1005, 1000))
		return true;
	return false;
}

/**
 * This function adds the cea modes in extension blocks of EDID
 * @context	: hdmi context
 * @connector	: handle to drm_connector
 *
 * Returns the number of modes added
 */
static int android_hdmi_add_cea_edid_modes(void *context,
					   struct drm_connector *connector)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	edid_info_t *edid_info;
	struct drm_display_mode *newmode = NULL, *mode_entry, *t;
	unsigned int saved_flags;
	int i = 0, ret_count = 0;

	if (connector == NULL || ctx == NULL)
		return 0;

	/* Init locals */
	edid_info = &ctx->edid_int;

	/* Do Mapping from PDT to drm_display_mode */
	for (i = 0; i < edid_info->num_timings; i++) {
		newmode = android_hdmi_get_drm_mode_from_pdt(
				&edid_info->timings[i], connector->dev);
		if (!newmode)
			continue;

		/* add new mode to list */
		drm_mode_set_name(newmode);
		drm_mode_probed_add(connector, newmode);
		ret_count++;
	}

	list_for_each_entry_safe(mode_entry, t, &connector->probed_modes, head) {
		/* If DRM already correctly handled PAR, skip this mode_entry */
		if ((mode_entry->flags & DRM_MODE_FLAG_PAR4_3) || (mode_entry->flags & DRM_MODE_FLAG_PAR16_9))
			continue;

		for (i = 0; i < edid_info->num_timings; i++) {
			newmode = android_hdmi_get_drm_mode_from_pdt(
					&edid_info->timings[i], connector->dev);
			if (!newmode)
				continue;

			/* Clear PAR flag for comparison */
			saved_flags = newmode->flags;
			newmode->flags &= (~DRM_MODE_FLAG_PAR4_3) & (~DRM_MODE_FLAG_PAR16_9);

			/* If same mode, then update PAR flag */
			if (drm_mode_equal_no_clocks(newmode, mode_entry) &&
					__android_check_clock_match(newmode->clock, mode_entry->clock)) {
					if (saved_flags & DRM_MODE_FLAG_PAR4_3)
						mode_entry->flags |= DRM_MODE_FLAG_PAR4_3;
					else
						mode_entry->flags |= DRM_MODE_FLAG_PAR16_9;
					break;
			}
			drm_mode_destroy(connector->dev, newmode);
		}
	}

	return ret_count;
}

#ifdef OTM_HDMI_UNIT_TEST
static bool android_hdmi_probed_mode_exists(
				struct drm_connector *connector,
				int hdisplay, int vdisplay, int vrefresh)
{
	struct drm_display_mode *mode, *t;
	if (!connector || hdisplay < 0 || vdisplay < 0 || vrefresh < 0)
		goto exit;

	/* loop through all probed modes to match */
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		if (mode->hdisplay == hdisplay &&
			mode->vdisplay == vdisplay &&
			vrefresh == drm_mode_vrefresh(mode)) {
			return true;
		}
	}
exit:
	return false;
}

/**
 * This function adds the edid information from cmdline
 * @context	: handle hdmi_context
 * @connector	: handle drm_connector
 * @hdisplay	: width
 * @vdisplay	: height
 * @vrefresh	: refresh rate
 *
 * Returns true if mode added, false if not added
 * This function is used to set the user requested mode
 * into the mode list
 */
static bool android_hdmi_add_noedid_mode(
				void *context,
				struct drm_connector *connector,
				int hdisplay, int vdisplay, int vrefresh)
{
	struct drm_display_mode *newmode = NULL;
	const otm_hdmi_timing_t *pdt = NULL;

	if (!context || !connector || hdisplay < 0 ||
			vdisplay < 0 || vrefresh < 0)
		goto exit;

	/* get mode timings */
	pdt = otm_hdmi_get_mode_timings(context, hdisplay, vdisplay, vrefresh);
	if (!pdt)
		goto exit;

	/* add mode */
	newmode = android_hdmi_get_drm_mode_from_pdt(pdt, connector->dev);
	if (newmode) {
		drm_mode_probed_add(connector, newmode);
		return true;
	}
exit:
	return false;
}
#endif

/**
 * Calculate refresh rate from mode
 * @mode	: handle to drm_display_mode
 *
 * Returns the calculated refresh rate
 */
static int calculate_refresh_rate(struct drm_display_mode *mode)
{
	int refresh_rate = 0;

	if (!mode)
		return refresh_rate;

	refresh_rate = (((mode->flags & DRM_MODE_FLAG_INTERLACE) ? 2 : 1) *
			mode->clock * 1000) /
			(mode->htotal * mode->vtotal);

	return refresh_rate;
}

/**
 * DRM get modes helper routine
 * @connector	: handle to drm_connector
 *
 * Returns the number of modes added
 * This is a helper routines for DRM get modes.
 * This function gets the edid information from the external sink
 * device using i2c when connected and parses the edid information
 * obtained and adds the modes to connector list
 * If sink device is not connected, then static edid timings are
 * used and those modes are added to the connector list
 */
int android_hdmi_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;
	struct edid *edid = NULL;
	/* Edid address in HDMI context */
	struct edid *ctx_edid = NULL;
	struct drm_display_mode *mode, *t;
	int i = 0, j = 0, ret = 0;
	int refresh_rate = 0;
	bool pref_mode_found = false;
	struct i2c_adapter *adapter = NULL;
	struct drm_display_mode *pref_mode_assigned;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	struct drm_display_mode *dup_mode, *user_mode;
	int mode_present = 0
#endif

	debug_modes_count = 0;
	pr_debug("Enter %s\n", __func__);

	adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);

	/* Lazy edid read feature, which can save I2C transactions largely.
	 * Basically, HPD will do edid read and store to HDMI context.
	 * Therefore, get modes should read edid with the condition
	 * whether the edid is ready in HDP or not in a lazy way. */
	if (edid_ready_in_hpd)
		goto edid_is_ready;

	/* Read edid blocks from i2c device if cable is connected */
	if (NULL != adapter && otm_hdmi_get_cable_status(hdmi_priv->context))
		edid = (struct edid *)drm_get_edid_retry(connector, adapter,
							hdmi_priv->context);

	if (edid == NULL) {
		pr_err("%s Edid Read failed -use default edid", __func__);
		edid = (struct edid *)default_edid;
	} else {
		edid_ready_in_hpd = 1;
		if (drm_detect_hdmi_monitor(edid))
			/* MONITOR_TYPE_HDMI */
			hdmi_priv->monitor_type =
					MONITOR_TYPE_HDMI;
		else
			/* MONITOR_TYPE_DVI */
			hdmi_priv->monitor_type =
					MONITOR_TYPE_DVI;
		pr_debug("Edid Read Done in get modes\n");
	}

	/* Store raw edid into HDMI context */
	otm_hdmi_set_raw_edid(hdmi_priv->context, (char *)edid);

	/* Release edid */
	if (edid && ((unsigned char *)edid != (unsigned char *)default_edid))
		kfree(edid);

edid_is_ready:
	/* Get the edid stored in HDMI context */
	otm_hdmi_get_raw_edid(hdmi_priv->context, (char **)&ctx_edid);

	/* Add modes into DRM mode list */
	drm_mode_connector_update_edid_property(connector, ctx_edid);
	ret = drm_add_edid_modes(connector, ctx_edid);

	/* Parse the edid extensions.
	 * If successful, add modes in extension blocks to DRM
	 */
	if (otm_hdmi_edid_extension_parse(hdmi_priv->context, ctx_edid,
			adapter) == OTM_HDMI_SUCCESS) {
		ret += android_hdmi_add_cea_edid_modes(hdmi_priv->context,
						connector);
	}

#ifdef OTM_HDMI_UNIT_TEST
	if (1 == cmdline_mode.specified) {
		/* Add cmdline mode if it does not exist in EDID */
		if (!android_hdmi_probed_mode_exists(connector,
			cmdline_mode.hdisplay,
			cmdline_mode.vdisplay,
			cmdline_mode.refresh))
			if (android_hdmi_add_noedid_mode(
				hdmi_priv->context,
				connector,
				cmdline_mode.hdisplay,
				cmdline_mode.vdisplay,
				cmdline_mode.refresh))
				ret++;
	}
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	connector->display_info.raw_edid = NULL;
#endif
	/* monitor_type is being used to switch state between HDMI & DVI */
	if (otm_hdmi_is_monitor_hdmi(hdmi_priv->context))
		hdmi_priv->monitor_type = MONITOR_TYPE_HDMI;
	else
		hdmi_priv->monitor_type = MONITOR_TYPE_DVI;

	j = 0;
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		refresh_rate = calculate_refresh_rate(mode);
		pr_debug("Mode %02d: %s %dHz\t Clk: %dKHz H/V: %c,%c"
			"flags: 0x%08x",
			j, mode->name, refresh_rate, mode->clock,
			(mode->flags & DRM_MODE_FLAG_PHSYNC) ? '+' : '-',
			(mode->flags & DRM_MODE_FLAG_PVSYNC) ? '+' : '-',
			mode->flags);

		if (debug_modes_count < DEBUG_MODES) {
			strncpy(arr_modes[debug_modes_count].name, mode->name,
				strlen(mode->name));
			arr_modes[debug_modes_count].name[strlen(mode->name)]
				= '\0';
			arr_modes[debug_modes_count].frq = refresh_rate;
			arr_modes[debug_modes_count].clk = mode->clock;
			debug_modes_count++;
		} else {
			pr_err("Increase size of DEBUG_MODES, some modes not"
				 " listed in report_edid.sh\n");
		}

		j++;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	/* add user mode to connector->mode list to support
	 * DRM IOCTL attachmode
	 */
        /*we don't support attach mode in DRM_IOCTL in kernel3.10 now,
         * so saftly remove below code when linux kernel version is 3.10
         */
	list_for_each_entry_safe(user_mode, t, &connector->user_modes, head) {
		mode_present = 0;
		/* check for whether user_mode is already in the mode_list */
		list_for_each_entry(mode, &connector->modes, head) {
			if (drm_mode_equal(mode, user_mode)) {
				mode_present = 1;
				mode->status = MODE_OK;
			}
		}

		pr_debug("user_mode ret: 0x%x, mode_present: 0x%x\n", ret,
			 mode_present);
		if (!mode_present) {
			dup_mode = drm_mode_duplicate(dev, user_mode);
			if (!dup_mode) {
				pr_err("Invalid dup_mode\n");
				break;
			}
			dup_mode->status = MODE_OK;
			list_add_tail(&dup_mode->head, &connector->modes);
			ret += 1;
		}
	}
#endif

	pref_mode_assigned = NULL;

	/* choose a preferred mode and set the mode type accordingly */
	list_for_each_entry_safe(mode, t, &connector->probed_modes, head) {
		/* check whether the display has support for 1080p.
		 * 720P is the minimum requirement expected from
		 * external display.
		 * (extend this if condition to set other modes as preferred).
		 */
		refresh_rate = calculate_refresh_rate(mode);
		if (mode->type & DRM_MODE_TYPE_PREFERRED) {
			pr_debug("Preferred timing mode of extenal display is %dx%d@%dHz.\n",
				mode->hdisplay, mode->vdisplay, refresh_rate);
			pref_mode_found = true;
			break;
		}
		if (pref_mode_assigned == NULL) {
			if (otm_hdmi_is_preferred_mode(mode->hdisplay, mode->vdisplay,
						refresh_rate)) {
				pr_debug("External display has %dx%d@%dHz support.\n",
					mode->hdisplay, mode->vdisplay, refresh_rate);
				pr_debug("This mode will be assigned as preferred if none is indicated.\n");
				pref_mode_assigned = mode;
			}
		}
	}

	/* if the external display does not indicate a preferred timing mode,
	 * assign 1080p mode (if found) as the preferred mode.
	 */
	if (pref_mode_found == false && pref_mode_assigned != NULL)
		pref_mode_assigned->type |= DRM_MODE_TYPE_PREFERRED;

	if (pref_mode_found == false && pref_mode_assigned == NULL)
		pr_err("Preferred mode is not indicated or assigned.\n");

	pr_debug("Exit %s (%d)\n", __func__, (ret - i));

	return ret - i;
}

/**
 * helper function to print the display mode details.
 * @mode:		drm display mode to print
 *
 * Returns:	none.
 */
static void __android_hdmi_dump_crtc_mode(struct drm_display_mode *mode)
{
	if (mode == NULL)
		return;

	pr_debug("hdisplay = %d\n", mode->hdisplay);
	pr_debug("vdisplay = %d\n", mode->vdisplay);
	pr_debug("hsync_start = %d\n", mode->hsync_start);
	pr_debug("hsync_end = %d\n", mode->hsync_end);
	pr_debug("htotal = %d\n", mode->htotal);
	pr_debug("vsync_start = %d\n", mode->vsync_start);
	pr_debug("vsync_end = %d\n", mode->vsync_end);
	pr_debug("vtotal = %d\n", mode->vtotal);
	pr_debug("clock = %d\n", mode->clock);
	pr_debug("flags = 0x%x\n", mode->flags);
}

/**
 * helper function to convert drm_display_mode to otm_hdmi_timing.
 * @otm_mode:		otm hdmi mode to be populated
 * @drm_mode:		drm_display_mode
 *
 * Returns:	none.
 */
static void __android_hdmi_drm_mode_to_otm_timing(otm_hdmi_timing_t *otm_mode,
				struct drm_display_mode *drm_mode)
{
	uint8_t i = 0;
	uint32_t par = OTM_HDMI_PAR_NO_DATA;

	if (otm_mode == NULL || drm_mode == NULL)
		return;

	otm_mode->width			= (unsigned short)
						drm_mode->crtc_hdisplay;
	otm_mode->height		= (unsigned short)
						drm_mode->crtc_vdisplay;
	otm_mode->dclk			= (unsigned long)
						drm_mode->clock;
	otm_mode->htotal		= (unsigned short)
						drm_mode->crtc_htotal;
	otm_mode->hblank_start		= (unsigned short)
						drm_mode->crtc_hblank_start;
	otm_mode->hblank_end		= (unsigned short)
						drm_mode->crtc_hblank_end;
	otm_mode->hsync_start		= (unsigned short)
						drm_mode->crtc_hsync_start;
	otm_mode->hsync_end		= (unsigned short)
						drm_mode->crtc_hsync_end;
	otm_mode->vtotal		= (unsigned short)
						drm_mode->crtc_vtotal;
	otm_mode->vblank_start		= (unsigned short)
						drm_mode->crtc_vblank_start;
	otm_mode->vblank_end		= (unsigned short)
						drm_mode->crtc_vblank_end;
	otm_mode->vsync_start		= (unsigned short)
						drm_mode->crtc_vsync_start;
	otm_mode->vsync_end		= (unsigned short)
						drm_mode->crtc_vsync_end;

	otm_mode->mode_info_flags = 0;
	if (drm_mode->flags & DRM_MODE_FLAG_INTERLACE)
		otm_mode->mode_info_flags |= PD_SCAN_INTERLACE;

	if (drm_mode->flags & DRM_MODE_FLAG_PAR4_3) {
		otm_mode->mode_info_flags |= OTM_HDMI_PAR_4_3;
		par = OTM_HDMI_PAR_4_3;
	} else if (drm_mode->flags & DRM_MODE_FLAG_PAR16_9) {
		otm_mode->mode_info_flags |= OTM_HDMI_PAR_16_9;
		par = OTM_HDMI_PAR_16_9;
	}

	if (drm_mode->flags & DRM_MODE_FLAG_PHSYNC)
		otm_mode->mode_info_flags |= PD_HSYNC_HIGH;

	if (drm_mode->flags & DRM_MODE_FLAG_PVSYNC)
		otm_mode->mode_info_flags |= PD_VSYNC_HIGH;

	otm_mode->metadata = 0;
	for (i = 0; i < ARRAY_SIZE(vic_formats); i++) {
		if (otm_mode->width == vic_formats[i].width &&
			otm_mode->height == vic_formats[i].height &&
			otm_mode->htotal == vic_formats[i].htotal &&
			otm_mode->vtotal == vic_formats[i].vtotal &&
			__android_check_clock_match(otm_mode->dclk, vic_formats[i].dclk) &&
			par == vic_formats[i].par) {
			if (1 == cmdline_mode.specified &&
				1 == cmdline_mode.vic_specified) {
				if (cmdline_mode.vic == vic_formats[i].vic) {
					otm_mode->metadata = cmdline_mode.vic;
					break;
				}
				/* else continue */
			} else {
				otm_mode->metadata = vic_formats[i].vic;
				pr_debug("Assigning vic %d to mode %dx%d@%d, flags=%#x.\n",
					(int)otm_mode->metadata, otm_mode->width,
					otm_mode->height, (int)otm_mode->dclk,
					(int)otm_mode->mode_info_flags);
				break;
			}
		}
	}
}

/**
 * helper function to set scaling mode in OTM HDMI
 * @context:		hdmi context
 * @scalingType:	scaling type
 *
 * Returns:	none.
 */
static void __android_hdmi_set_scaling_type(void *context, int scalingType)
{
	switch (scalingType) {
	case DRM_MODE_SCALE_NONE:
		otm_hdmi_set_scaling_type(context, OTM_HDMI_SCALE_NONE);
		break;
	case DRM_MODE_SCALE_CENTER:
		otm_hdmi_set_scaling_type(context, OTM_HDMI_SCALE_CENTER);
		break;
	case DRM_MODE_SCALE_FULLSCREEN:
		otm_hdmi_set_scaling_type(context, OTM_HDMI_SCALE_FULLSCREEN);
		break;
	case DRM_MODE_SCALE_ASPECT:
		otm_hdmi_set_scaling_type(context, OTM_HDMI_SCALE_ASPECT);
		break;
	default:
		break;
	}
	return;
}

/**
 * set display and pfit registers as per the preferred scaling property.
 * @crtc		: crtc
 *
 * Returns:	0 on success
 *		-1 on failure
 */
int android_hdmi_set_scaling_property(struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	struct psb_intel_crtc *psb_intel_crtc;
	struct drm_display_mode *adjusted_mode, *mode;
	otm_hdmi_timing_t otm_mode, otm_adjusted_mode;
	int pipe;

	pr_debug("Enter %s\n", __func__);

	BUG_ON(crtc == NULL);

	/* get handles for required data */
	dev = crtc->dev;
	BUG_ON(dev == NULL);
	dev_priv = dev->dev_private;
	BUG_ON(dev_priv == NULL);
	hdmi_priv = dev_priv->hdmi_priv;
	BUG_ON(hdmi_priv == NULL);
	psb_intel_crtc = to_psb_intel_crtc(crtc);
	pipe = psb_intel_crtc->pipe;
	adjusted_mode = &psb_intel_crtc->saved_adjusted_mode;
	mode = &psb_intel_crtc->saved_mode;

	BUG_ON(pipe != OTM_HDMI_PIPE_NUM);

	__android_hdmi_drm_mode_to_otm_timing(&otm_mode, mode);
	__android_hdmi_drm_mode_to_otm_timing(&otm_adjusted_mode,
						adjusted_mode);

	if (otm_hdmi_crtc_set_scaling(hdmi_priv->context, &otm_mode,
				&otm_adjusted_mode, crtc->fb->width,
				crtc->fb->height)) {
		pr_err("%s: failed to program scaling", __func__);
		return -1;
	}

	pr_debug("Exit %s\n", __func__);

	return 0;
}

/**
 * crtc mode set for hdmi pipe.
 * @crtc		: crtc
 * @mode		:mode requested
 * @adjusted_mode:adjusted mode
 * @x		:x value
 * @y		:y value
 * @old_fb	: old frame buffer values for flushing old planes
 *
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */
int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb)
{
	struct drm_device *dev;
	struct psb_intel_crtc *psb_intel_crtc;
	struct drm_psb_private *dev_priv;
	struct drm_framebuffer *fb;
	struct android_hdmi_priv *hdmi_priv;
	struct drm_mode_config *mode_config;
	struct psb_intel_output *psb_intel_output = NULL;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	uint64_t scalingType = DRM_MODE_SCALE_CENTER;

	int pipe;
	otm_hdmi_timing_t otm_mode, otm_adjusted_mode;
	uint32_t clock_khz;
	int fb_width, fb_height;

	pr_debug("Enter %s\n", __func__);

	if (crtc == NULL || mode == NULL || adjusted_mode == NULL)
		return -EINVAL;

	/* get handles for required data */
	dev = crtc->dev;
	psb_intel_crtc = to_psb_intel_crtc(crtc);
	pipe = psb_intel_crtc->pipe;
	dev_priv = dev->dev_private;
	fb = crtc->fb;
	fb_width = fb->width;
	fb_height = fb->height;
	hdmi_priv = dev_priv->hdmi_priv;
	mode_config = &dev->mode_config;

	if (pipe != 1) {
		pr_err("%s: Invalid pipe %d", __func__, pipe);
		return 0;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return 0;

	pr_debug("%s mode info:\n", __func__);
	__android_hdmi_dump_crtc_mode(mode);
	pr_debug("%s adjusted mode info:\n", __func__);
	__android_hdmi_dump_crtc_mode(adjusted_mode);

	memcpy(&psb_intel_crtc->saved_mode, mode,
				sizeof(struct drm_display_mode));
	memcpy(&psb_intel_crtc->saved_adjusted_mode, adjusted_mode,
				sizeof(struct drm_display_mode));

	__android_hdmi_drm_mode_to_otm_timing(&otm_mode, mode);
	__android_hdmi_drm_mode_to_otm_timing(&otm_adjusted_mode,
						adjusted_mode);

	list_for_each_entry(connector, &mode_config->connector_list, head) {
		encoder = connector->encoder;
		if (!encoder)
			continue;
		if (encoder->crtc != crtc)
			continue;
		psb_intel_output = to_psb_intel_output(connector);
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	if (psb_intel_output)
		drm_connector_property_get_value(&psb_intel_output->base,
			dev->mode_config.scaling_mode_property, &scalingType);
#else
        if (psb_intel_output)
            drm_object_property_get_value(&(&psb_intel_output->base)->base,
                    dev->mode_config.scaling_mode_property, &scalingType);
#endif

	psb_intel_crtc->scaling_type = scalingType;

	__android_hdmi_set_scaling_type(hdmi_priv->context, scalingType);

	/* Disable the VGA plane that we never use */
	REG_WRITE(VGACNTRL, VGA_DISP_DISABLE);

	/* Disable the panel fitter if it was on our pipe */
	if (psb_intel_panel_fitter_pipe(dev) == pipe)
		REG_WRITE(PFIT_CONTROL, 0);

	/* Flush the plane changes */
	{
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc_funcs->mode_set_base(crtc, x, y, old_fb);
	}

	if (otm_hdmi_crtc_mode_set(hdmi_priv->context, &otm_mode,
				&otm_adjusted_mode, fb_width,
				fb_height, &clock_khz)) {
		pr_err("%s: failed to perform hdmi crtc mode set",
					__func__);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		return 0;
	}

	hdmi_priv->clock_khz = clock_khz;

	/*
	 * SW workaround for Compliance 7-29 ACR test on 576p@50
	 * use the nominal pixel clock, instead of the actual clock
	 */
	if (otm_adjusted_mode.metadata == 17 ||
			otm_adjusted_mode.metadata == 18)
		hdmi_priv->clock_khz = otm_adjusted_mode.dclk;

	psb_intel_wait_for_vblank(dev);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	pr_debug("Exit %s\n", __func__);
	return 0;
}

/**
 * encoder mode set for hdmi pipe.
 * @encoder:		hdmi encoder
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	none.
 */
void android_hdmi_enc_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev;
	struct android_hdmi_priv *hdmi_priv;
	struct drm_psb_private *dev_priv;
	otm_hdmi_timing_t otm_mode, otm_adjusted_mode;

	pr_debug("Enter %s\n", __func__);

	if (encoder == NULL || mode == NULL || adjusted_mode == NULL)
		return;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return;

	/* get handles for required data */
	dev = encoder->dev;
	dev_priv = dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;

	__android_hdmi_drm_mode_to_otm_timing(&otm_mode, mode);
	__android_hdmi_drm_mode_to_otm_timing(&otm_adjusted_mode,
						adjusted_mode);

	if (otm_hdmi_enc_mode_set(hdmi_priv->context, &otm_mode,
				&otm_adjusted_mode)) {
		pr_err("%s: failed to perform hdmi enc mode set",
					__func__);
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		return;
	}

	/*save current set mode*/
	if (hdmi_priv->current_mode)
		drm_mode_destroy(dev,
				 hdmi_priv->current_mode);
	hdmi_priv->current_mode =
		drm_mode_duplicate(dev, adjusted_mode);

	/* Send MODE_CHANGE event to Audio driver */
	mid_hdmi_audio_signal_event(dev, HAD_EVENT_MODE_CHANGING);

#ifdef OTM_HDMI_HDCP_ENABLE
#ifdef OTM_HDMI_HDCP_ALWAYS_ENC
	/* enable hdcp */
	if (otm_hdmi_hdcp_enable(hdmi_priv->context, adjusted_mode->vrefresh))
		pr_debug("hdcp enabled");
	else
		pr_err("hdcp could not be enabled");
#endif
#endif
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	return;
}

/**
 * save the register for HDMI display and disable HDMI
 * @dev:		drm device
 *
 * Returns:	none.
 */
void android_hdmi_suspend_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	bool is_connected;

	if (NULL == dev)
		return;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return;

	if (hdmi_priv->hdmi_audio_enabled) {
		pr_err("OSPM: %s: hdmi audio is busy\n", __func__);
		return;
	}

	/* Check if monitor is attached to HDMI connector. */
	is_connected = otm_hdmi_get_cable_status(hdmi_priv->context);

	otm_hdmi_save_display_registers(hdmi_priv->context,
					is_connected);

	otm_disable_hdmi(hdmi_priv->context);

	/* power island is turnned off by IRQ handler if device is disconnected */
	if (is_connected && !hdmi_priv->hdmi_suspended) {
		/* Keep DSPB & HDMIO islands off after suspending. */
		otm_hdmi_power_islands_off();
	}
	hdmi_priv->hdmi_suspended = true;

	otm_hdmi_power_rails_off();

	/* disable hotplug detection */
	otm_hdmi_enable_hpd(false);

	return;
}

/**
 * Prepare HDMI EDID-like data and copy it to the given buffer
 * Input parameters:
 * @dev: drm Device
 * @eld: pointer to otm_hdmi_eld_t data structure
*
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */

int android_hdmi_get_eld(struct drm_device *dev, void *eld)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	otm_hdmi_eld_t *hdmi_eld;
	otm_hdmi_ret_t ret;

	if (NULL == dev || NULL == eld)
		return -EINVAL;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return -EINVAL;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return -EINVAL;

	hdmi_eld = (otm_hdmi_eld_t *)eld;
	ret = otm_hdmi_get_eld(hdmi_priv->context, hdmi_eld);
	if (ret == OTM_HDMI_SUCCESS)
		return 0;

	/* TODO: return proper error code. */
	return -EINVAL;
}


/**
 * get DPLL clock in khz
 * Input parameters:
 * @dev: drm Device
*
 * Returns:  DPLL clock in khz
 */

uint32_t android_hdmi_get_dpll_clock(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;

	if (NULL == dev || NULL == dev->dev_private)
		return 0;
	dev_priv = dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return 0;
	return hdmi_priv->clock_khz;
}

/**
 * Restore the register and enable the HDMI display
 * @dev:		drm device
 *
 * Returns:	none.
 */
void android_hdmi_resume_display(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	bool is_connected;  /* connection status during resume */
	bool was_connected; /* connection status before suspend */
	hdmi_context_t *ctx;
	if (NULL == dev)
		return;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return;

	ctx = (hdmi_context_t *)(hdmi_priv->context);
	if (NULL == ctx)
		return;

	/* use the connection status before suspend to determine if
	  * to power on islands. HDMI may have been plugged out
	  * during suspend
	*/
	was_connected = ctx->is_connected;
	if (was_connected && hdmi_priv->hdmi_suspended) {
		/* Keep DSPB & HDMIO islands on after resuming. */
		if (!otm_hdmi_power_islands_on()) {
			pr_err("Unable to power on display island!");
			return;
		}
	}
	hdmi_priv->hdmi_suspended = false;

	otm_hdmi_power_rails_on();
	/* Check if monitor is attached to HDMI connector. */
	is_connected = otm_hdmi_get_cable_status(hdmi_priv->context);

	/* only restore display if there is no connection status change */
	otm_hdmi_restore_and_enable_display(hdmi_priv->context,
				was_connected & is_connected);

	if (!is_connected) {
		/* power off rails, HPD will continue to work */
		otm_hdmi_power_rails_off();
	}

	/* enable hotplug detection */
	otm_hdmi_enable_hpd(true);
}

/**
 * DRM encoder save helper routine
 * @encoder      : handle to drm_encoder
 *
 * Returns nothing
 * This helper routine is used by DRM during early suspend
 * operation to simply disable active plane.
 */
void android_hdmi_encoder_save(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct psb_intel_output *output = enc_to_psb_intel_output(encoder);
	struct android_hdmi_priv *hdmi_priv = output->dev_priv;
	int dspcntr_reg = DSPBCNTR;
	int dspbsurf_reg = DSPBSURF;
	u32 dspbcntr_val;

	pr_debug("Entered %s\n", __func__);

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return ;

	hdmi_priv->need_encoder_restore = true;

	/*Use Disable pipeB plane to turn off HDMI screen
	 in early_suspend  */
	dspbcntr_val = REG_READ(dspcntr_reg);
	if ((dspbcntr_val & DISPLAY_PLANE_ENABLE) != 0) {
		REG_WRITE(dspcntr_reg,
				dspbcntr_val & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		REG_WRITE(dspbsurf_reg, REG_READ(dspbsurf_reg));
	}

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	pr_debug("Exiting %s\n", __func__);
}

void android_hdmi_encoder_restore_wq(struct work_struct *work)
{
	struct android_hdmi_priv *hdmi_priv =
		container_of(work, struct android_hdmi_priv, enc_work.work);
	struct drm_encoder *encoder;
	struct drm_device *dev = NULL;
	struct drm_psb_private *dev_priv = NULL;
	struct psb_intel_output *output = NULL;
	int dspcntr_reg = DSPBCNTR;
	int dspbsurf_reg = DSPBSURF;
	u32 dspcntr_val = 0;
	otm_hdmi_timing_t otm_mode;
	bool is_monitor_hdmi;
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	pr_debug("Entered %s\n", __func__);

	if (unlikely(!hdmi_priv))
		return;

	encoder = (struct drm_encoder *)hdmi_priv->data;
	if (unlikely(!encoder))
		return;

	dev = (struct drm_device *)encoder->dev;
	if (unlikely(!dev))
		return;

	dev_priv = dev->dev_private;
	if (unlikely(!dev_priv))
		return;

	output = enc_to_psb_intel_output(encoder);
	if (unlikely(!output))
		return;

	if (!drm_helper_encoder_in_use(encoder))
		return;

	if (unlikely(!hdmi_priv->need_encoder_restore)) {
		pr_debug("No need to restore encoder\n");
		return;
	}

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return;

	if (!dev_priv->bhdmiconnected ||
			!(dev_priv->panel_desc & DISPLAY_B)) {
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
		return;
	}

	/* Restore pipe B plane to turn on HDMI screen */
	dspcntr_val = REG_READ(dspcntr_reg);
	if ((dspcntr_val & DISPLAY_PLANE_ENABLE) == 0 &&
			(DISP_PLANEB_STATUS != DISPLAY_PLANE_DISABLE)) {
		REG_WRITE(dspcntr_reg,
				dspcntr_val | DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		REG_WRITE(dspbsurf_reg, REG_READ(dspbsurf_reg));
	}

	if (hdmi_priv->current_mode)
		__android_hdmi_drm_mode_to_otm_timing(&otm_mode,
			hdmi_priv->current_mode);
	else
		pr_err("%s: No saved current mode found, unable to restore\n",
			__func__);

	is_monitor_hdmi = otm_hdmi_is_monitor_hdmi(hdmi_priv->context);

	/* Enable AVI infoframes for HDMI mode */
	if (is_monitor_hdmi) {
		rc = otm_hdmi_infoframes_set_avi(hdmi_priv->context, &otm_mode);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to program avi infoframe\n");
	} else {/* Disable all inofoframes for DVI mode */
		rc = otm_hdmi_disable_all_infoframes(hdmi_priv->context);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to disable all infoframes\n");
	}

	hdmi_priv->need_encoder_restore = false;

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

	pr_debug("Exiting %s\n", __func__);
}

/**
 * DRM encoder restore helper routine
 * @encoder      : handle to drm_encoder
 *
 * Returns nothing
 * This helper routine is used by DRM during late resume
 * operation for restoring the pipe and enabling it. The
 * operation itself is completed in a delayed workqueue
 * item which ensures restore can be done once the system
 * is resumed.
 */
void android_hdmi_encoder_restore(struct drm_encoder *encoder)
{
	unsigned long delay = 0;
	struct psb_intel_output *output = NULL;
	struct android_hdmi_priv *hdmi_priv = NULL;

	pr_debug("Entered %s\n", __func__);

	if (unlikely(!encoder))
		return;

	output = enc_to_psb_intel_output(encoder);
	if (unlikely(!output))
		return;

	hdmi_priv = output->dev_priv;
	if (unlikely(!hdmi_priv))
		return;

	hdmi_priv->data = (void *)encoder;

	delay = HZ/5;
	schedule_delayed_work(&hdmi_priv->enc_work, delay);
	pr_debug("Exiting %s\n", __func__);
}

/**
 * DRM encoder mode fixup helper routine
 * @encoder      : handle to drm_encoder
 * @mode         : proposed display mode
 * @adjusted_mode: actual mode to be displayed by HW
 *
 * Returns boolean to indicate success/failure
 * This routine can be used to make adjustments to actual
 * mode parameters as required by underlying HW.
 * This is currently not required.
 */
bool android_hdmi_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	pr_debug("%s: Nothing be done here\n", __func__);
	return true;
}

void android_hdmi_encoder_prepare(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs =
		encoder->helper_private;
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
}

void android_hdmi_encoder_commit(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs =
		encoder->helper_private;
	encoder_funcs->dpms(encoder, DRM_MODE_DPMS_ON);
}

void android_hdmi_enc_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

/**
 * Currently the driver doesn't take advantage of encoders.
 */
struct drm_encoder *android_hdmi_best_encoder(struct drm_connector *connector)
{
	struct psb_intel_output *psb_intel_output =
		to_psb_intel_output(connector);
	return &psb_intel_output->enc;
}


/**
 * DRM connector save helper routine
 * @connector       : handle to drm_connector
 *
 * Returns nothing.
 * This routine is used to save connector state.
 */
void android_hdmi_connector_save(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct psb_intel_output *output = to_psb_intel_output(connector);
	struct android_hdmi_priv *hdmi_priv = output->dev_priv;

	pr_debug("Entered %s\n", __func__);

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return ;

	hdmi_priv->save_HDMIB = REG_READ(hdmi_priv->hdmib_reg);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	pr_debug("Exiting %s\n", __func__);
}

/**
 * DRM connector restore helper routine
 * @connector       : handle to drm_connector
 *
 * Returns nothing.
 * This routine is used to restore connector state.
 */
void android_hdmi_connector_restore(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct psb_intel_output *output = to_psb_intel_output(connector);
	struct android_hdmi_priv *hdmi_priv = output->dev_priv;

	pr_debug("Entering %s\n", __func__);
	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return ;

	REG_WRITE(hdmi_priv->hdmib_reg, hdmi_priv->save_HDMIB);
	REG_READ(hdmi_priv->hdmib_reg);

	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	pr_debug("Exiting %s\n", __func__);
}

/**
 * Enable HDCP on HDMI display
 * @dev:	drm device
 *
 * Returns:	true on success else false
 */
bool android_enable_hdmi_hdcp(struct drm_device *dev)
{
#ifdef OTM_HDMI_HDCP_ENABLE
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev)
		return false;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return false;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return false;
	if (NULL == hdmi_priv->current_mode)
		return false;

	/* enable hdcp */
	if (otm_hdmi_hdcp_enable(hdmi_priv->context,
		hdmi_priv->current_mode->vrefresh)) {
		pr_debug("hdcp enabled");
		return true;
	} else {
		pr_debug("hdcp could not be enabled");
		return false;
	}
#endif
#endif
	return false;
}

/**
 * disable HDCP on HDMI display
 * @dev:	drm device
 *
 * Returns:	true on success else false
 */
bool android_disable_hdmi_hdcp(struct drm_device *dev)
{
#ifdef OTM_HDMI_HDCP_ENABLE
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev)
		return false;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return false;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return false;

	/* disable hdcp */
	if (otm_hdmi_hdcp_disable(hdmi_priv->context)) {
		pr_debug("hdcp disabled\n");
		return true;
	} else {
		pr_err("hdcp could not be disabled");
		return false;
	}
#endif
#endif
	return false;
}

/**
 * Query whether HDCP is enabled & encrypting on HDMI display
 * @dev:	drm device
 *
 * Returns:	true if encrypting else false
 */
bool android_check_hdmi_hdcp_enc_status(struct drm_device *dev)
{
#ifdef OTM_HDMI_HDCP_ENABLE
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev)
		return false;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return false;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return false;

	/* disable hdcp */
	if (otm_hdmi_hdcp_enc_status(hdmi_priv->context))
		return true;
	else
		return false;
#endif
#endif
	return false;
}

/**
 * Query presence of a HDCP Sink Device
 * @dev:	drm device
 * @bksv:	ksv value of the sink device will be returned on success
 *
 * Returns:	true on successful detection else false
 */
bool android_query_hdmi_hdcp_sink(struct drm_device *dev, uint8_t *bksv)
{
#ifdef OTM_HDMI_HDCP_ENABLE
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev || bksv == NULL)
		return false;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return false;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return false;

	/* query bksv availability */
	if (otm_hdmi_hdcp_read_validate_bksv(hdmi_priv->context, bksv)) {
		return true;
	} else {
		return false;
	}
#endif
#endif
	return false;
}

/**
 * Query HDCP Phase 3 Link Status
 * @dev:	drm device
 *
 * Returns:	true if link is not compromised else false
 */
bool android_check_hdmi_hdcp_link_status(struct drm_device *dev)
{
#ifdef OTM_HDMI_HDCP_ENABLE
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	if (NULL == dev)
		return false;
	dev_priv = dev->dev_private;
	if (NULL == dev_priv)
		return false;
	hdmi_priv = dev_priv->hdmi_priv;
	if (NULL == hdmi_priv)
		return false;

	/* disable hdcp */
	if (otm_hdmi_hdcp_link_status(hdmi_priv->context)) {
		return true;
	} else {
		pr_debug("hdcp link failed\n");
		return false;
	}
#endif
#endif
	return false;
}

/**
 * hdmi helper function to detect whether hdmi/dvi is connected or not.
 * @connector:	hdmi connector
 *
 * Returns:	connector_status_connected if hdmi/dvi is connected.
 *		connector_status_disconnected if hdmi/dvi is not connected.
 */
enum drm_connector_status
android_hdmi_detect(struct drm_connector *connector,
		    bool force)
{
	struct drm_device *dev = NULL;
	struct drm_psb_private *dev_priv = NULL;
	struct android_hdmi_priv *hdmi_priv = NULL;
	bool data = 0;
	static bool first_boot = true;
	static int prev_connection_status = connector_status_disconnected;
	struct i2c_adapter *adapter = i2c_get_adapter(OTM_HDMI_I2C_ADAPTER_NUM);

	if (NULL == connector || NULL == adapter)
		return connector_status_disconnected;

	dev = connector->dev;
	dev_priv = (struct drm_psb_private *)dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;

	/* Check if monitor is attached to HDMI connector. */
	data = otm_hdmi_get_cable_status(hdmi_priv->context);
	pr_debug("%s: HPD connected data = 0x%x.\n", __func__, data);

#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_set_hpd_state(hdmi_priv->context, data);
#else
	otm_hdmi_update_security_hdmi_hdcp_status(false, data);
#endif

	if (data) {
		if (first_boot) {
			hdmi_state = 1;
			first_boot = false;
		}

		if (prev_connection_status == connector_status_connected)
			return connector_status_connected;

		prev_connection_status = connector_status_connected;

		/* Turn on power islands and hold ref count */
		if (!otm_hdmi_power_islands_on())
			pr_err("otm_hdmi_power_islands_on failed!");

		dev_priv->panel_desc |= DISPLAY_B;
		dev_priv->bhdmiconnected = true;

		return connector_status_connected;
	} else {
		if (prev_connection_status != connector_status_disconnected) {
#ifdef OTM_HDMI_HDCP_ENABLE
#ifdef OTM_HDMI_HDCP_ALWAYS_ENC
			if (otm_hdmi_hdcp_disable(hdmi_priv->context))
				pr_debug("hdcp disabled\n");
			else
				pr_debug("failed to disable hdcp\n");
#endif
#endif
			dev_priv->panel_desc &= ~DISPLAY_B;
			dev_priv->bhdmiconnected = false;

			/* Turn off power islands and decrement ref count */
			otm_hdmi_power_islands_off();
		}

		/* Always turn off power rails when hdmi is disconnected */
		otm_hdmi_power_rails_off();

		prev_connection_status = connector_status_disconnected;

		return connector_status_disconnected;
	}
}

int android_hdmi_probe_single_connector_modes(struct drm_connector *connector,
		uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev;
	int count = 0;

	if (!connector)
		return count;

	dev = connector->dev;

	count = drm_helper_probe_single_connector_modes(connector,
			dev->mode_config.max_width,
			dev->mode_config.max_height);
	return count;
}

/**
 * Description: check whether hdmi/dvi is connected or not.
 *
 * @dev:	drm device
 *
 * Returns:	true if hdmi/dvi is connected.
 *		false if hdmi/dvi is not connected.
 */
bool android_hdmi_is_connected(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = NULL;
	struct android_hdmi_priv *hdmi_priv = NULL;

	if (NULL == dev)
		return false;

	dev_priv = (struct drm_psb_private *)dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;

	/* Check if monitor is attached to HDMI connector. */
	return otm_hdmi_get_cable_status(hdmi_priv->context);
}

static int android_hdmi_set_property(struct drm_connector *connector,
				     struct drm_property *property,
				     uint64_t value)
{
	struct drm_encoder *pEncoder = connector->encoder;
	struct psb_intel_crtc *pPsbCrtc = NULL;
	bool bTransitionFromToCentered = false;
	bool bTransitionFromToAspect = false;
	uint64_t curValue;

	pr_debug("Entered %s\n", __func__);
	if (!pPsbCrtc) {
		pr_err("Invalid pointer\n");
		goto set_prop_error;
	}
	if (!strcmp(property->name, "scaling mode") && pEncoder) {
		pr_debug("Property: scaling mode\n");
	} else if (!strcmp(property->name, "DPMS") && pEncoder) {
		pr_debug("Property: DPMS\n");
	}
	else {
		pr_err("%s: Unable to handle property %s\n", __func__,
			property->name);
		goto set_prop_error;
	}

	if (!strcmp(property->name, "scaling mode") && pEncoder) {
		pPsbCrtc = to_psb_intel_crtc(pEncoder->crtc);

		if (!pPsbCrtc)
			goto set_prop_error;

		switch (value) {
		case DRM_MODE_SCALE_FULLSCREEN:
			break;
		case DRM_MODE_SCALE_CENTER:
			break;
		case DRM_MODE_SCALE_NO_SCALE:
			break;
		case DRM_MODE_SCALE_ASPECT:
			break;
		default:
			goto set_prop_error;
		}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		if (drm_connector_property_get_value(connector, property, &curValue))
#else
                if (drm_object_property_get_value(&connector->base, property, &curValue))
#endif
			goto set_prop_error;

		if (curValue == value)
			goto set_prop_done;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		if (drm_connector_property_set_value(connector, property, value))
#else 
                if (drm_object_property_set_value(&connector->base, property, value))
#endif
			goto set_prop_error;

		bTransitionFromToCentered =
			(curValue == DRM_MODE_SCALE_NO_SCALE) ||
			(value == DRM_MODE_SCALE_NO_SCALE) ||
			(curValue == DRM_MODE_SCALE_CENTER) ||
			(value == DRM_MODE_SCALE_CENTER);

		bTransitionFromToAspect = (curValue == DRM_MODE_SCALE_ASPECT) ||
			(value == DRM_MODE_SCALE_ASPECT);

		if (pPsbCrtc->saved_mode.hdisplay != 0 &&
			pPsbCrtc->saved_mode.vdisplay != 0) {
			if (bTransitionFromToCentered ||
					bTransitionFromToAspect) {
				if (!drm_crtc_helper_set_mode(pEncoder->crtc,
						&pPsbCrtc->saved_mode,
						pEncoder->crtc->x,
						pEncoder->crtc->y,
						pEncoder->crtc->fb))
					goto set_prop_error;
			} else {
				struct drm_encoder_helper_funcs *pEncHFuncs =
					pEncoder->helper_private;
				pEncHFuncs->mode_set(pEncoder,
					&pPsbCrtc->saved_mode,
					&pPsbCrtc->saved_adjusted_mode);
			}
		}
	}
set_prop_done:
	return 0;
set_prop_error:
	return -1;
}


void android_hdmi_connector_destroy(struct drm_connector *connector)
{
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

void android_hdmi_connector_dpms(struct drm_connector *connector, int mode)
{
	struct drm_device *dev = connector->dev;
	bool hdmi_audio_busy = false;
	u32 dspcntr_val;
#if (defined CONFIG_PM_RUNTIME) && (!defined MERRIFIELD) \
	&& (defined CONFIG_SUPPORT_MIPI)
	struct drm_psb_private *dev_priv = dev->dev_private;
	bool panel_on = false, panel_on2 = false;
	struct mdfld_dsi_config **dsi_configs;
#endif

	pr_debug("Entered %s\n", __func__);
	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON))
		return ;

	/* Check HDMI Audio Status */
	hdmi_audio_busy = mid_hdmi_audio_is_busy(dev);

	pr_debug("[DPMS] audio busy: 0x%x\n", hdmi_audio_busy);

	/* if hdmi audio is busy, just turn off hdmi display plane */
	if (hdmi_audio_busy) {
		dspcntr_val = PSB_RVDC32(DSPBCNTR);
		connector->dpms = mode;

		if (mode != DRM_MODE_DPMS_ON) {
			REG_WRITE(DSPBCNTR, dspcntr_val &
							~DISPLAY_PLANE_ENABLE);
			DISP_PLANEB_STATUS = DISPLAY_PLANE_DISABLE;
		} else {
			REG_WRITE(DSPBCNTR, dspcntr_val |
							DISPLAY_PLANE_ENABLE);
			DISP_PLANEB_STATUS = DISPLAY_PLANE_ENABLE;
		}
	} else {
		drm_helper_connector_dpms(connector, mode);

		if (mode != DRM_MODE_DPMS_ON)
			DISP_PLANEB_STATUS = DISPLAY_PLANE_DISABLE;
		else
			DISP_PLANEB_STATUS = DISPLAY_PLANE_ENABLE;
	}

#if (defined CONFIG_PM_RUNTIME) && (!defined MERRIFIELD) \
	&& (defined CONFIG_SUPPORT_MIPI)
	dsi_configs = dev_priv->dsi_configs;

	if (dsi_configs[0])
		panel_on = dsi_configs[0]->dsi_hw_context.panel_on;
	if (dsi_configs[1])
		panel_on2 = dsi_configs[1]->dsi_hw_context.panel_on;

	/*then check all display panels + monitors status*/
	if (!panel_on &&
		!panel_on2 &&
		!(REG_READ(HDMIB_CONTROL) & HDMIB_PORT_EN)) {
		/*request rpm idle*/
		if(dev_priv->rpm_enabled) {
			pm_request_idle(&dev->pdev->dev);
		}
	}
#endif
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	pr_debug("Exiting %s\n", __func__);
}

/**
 * hdmi helper function to manage power to the display (dpms)
 * @encoder:	hdmi encoder
 * @mode:	dpms on or off
 *
 * Returns:	none
 */
void android_hdmi_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	struct android_hdmi_priv *hdmi_priv;
	otm_hdmi_timing_t otm_mode;
	bool is_monitor_hdmi;
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	struct psb_intel_output *output;
	int dspcntr_reg = DSPBCNTR;
	int dspbase_reg = MRST_DSPBBASE;
	u32 hdmip_enabled = 0;
	u32 hdmib, hdmi_phy_misc;
	u32 temp;

	pr_debug("Entered %s\n", __func__);
	if (encoder == NULL)
		return;

	pr_debug("%s\n", mode == DRM_MODE_DPMS_ON ? "on" : "off");
	dev = encoder->dev;
	dev_priv = (struct drm_psb_private *)dev->dev_private;
	hdmi_priv = dev_priv->hdmi_priv;
	output = enc_to_psb_intel_output(encoder);

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_FORCE_POWER_ON))
		return;
	hdmib = REG_READ(hdmi_priv->hdmib_reg) | HDMIB_PIPE_B_SELECT;

	is_monitor_hdmi = otm_hdmi_is_monitor_hdmi(hdmi_priv->context);

	if (is_monitor_hdmi)
		hdmib |= (HDMIB_NULL_PACKET | HDMIB_AUDIO_ENABLE);
	else
		hdmib &= ~(HDMIB_NULL_PACKET | HDMIB_AUDIO_ENABLE);

	hdmi_phy_misc = REG_READ(HDMIPHYMISCCTL);
	hdmip_enabled = REG_READ(hdmi_priv->hdmib_reg) & HDMIB_PORT_EN;
	pr_debug("hdmip_enabled is %x\n", hdmip_enabled);

	if (dev_priv->early_suspended) {
		/* Use Disable pipeB plane to turn off HDMI screen
		  * in early_suspend
		  */
		temp = REG_READ(dspcntr_reg);
		if ((temp & DISPLAY_PLANE_ENABLE) != 0) {
			REG_WRITE(dspcntr_reg,
				temp & ~DISPLAY_PLANE_ENABLE);
			/* Flush the plane changes */
			REG_WRITE(dspbase_reg, REG_READ(dspbase_reg));
		}
	}

	if (mode != DRM_MODE_DPMS_ON) {
		if (is_monitor_hdmi && (hdmip_enabled != 0))
			mid_hdmi_audio_signal_event(dev, HAD_EVENT_HOT_UNPLUG);

		REG_WRITE(hdmi_priv->hdmib_reg,
			hdmib & ~HDMIB_PORT_EN & ~HDMIB_AUDIO_ENABLE);
		otm_hdmi_vblank_control(dev, false);
		REG_WRITE(HDMIPHYMISCCTL, hdmi_phy_misc | HDMI_PHY_POWER_DOWN);
		rc = otm_hdmi_disable_all_infoframes(hdmi_priv->context);
		if (rc != OTM_HDMI_SUCCESS)
			pr_err("%s: failed to disable all infoframes\n",
				__func__);

	} else {
		REG_WRITE(HDMIPHYMISCCTL, hdmi_phy_misc & ~HDMI_PHY_POWER_DOWN);
		otm_hdmi_vblank_control(dev, true);
		REG_WRITE(hdmi_priv->hdmib_reg, hdmib | HDMIB_PORT_EN);

		if (is_monitor_hdmi && (hdmip_enabled == 0)) {
			mid_hdmi_audio_signal_event(dev, HAD_EVENT_HOT_PLUG);
			switch_set_state(&hdmi_priv->sdev, 1);
			pr_info("%s: hdmi state switched to %d\n", __func__,
			hdmi_priv->sdev.state);
		}

		if (hdmi_priv->current_mode)
			__android_hdmi_drm_mode_to_otm_timing(&otm_mode,
				hdmi_priv->current_mode);
		else
			pr_info("%s: No saved current mode found, unable to restore\n",
				__func__);

		if (is_monitor_hdmi) {
			/* Enable AVI infoframes for HDMI mode */
			rc = otm_hdmi_infoframes_set_avi(hdmi_priv->context,
								&otm_mode);
			if (rc != OTM_HDMI_SUCCESS)
				pr_err("%s: failed to program avi infoframe\n",
					__func__);
		} else {
			/* Disable all infoframes for DVI mode */
			rc = otm_hdmi_disable_all_infoframes
							(hdmi_priv->context);
			if (rc != OTM_HDMI_SUCCESS)
				pr_err("%s: failed to disable all infoframes\n",
					__func__);
		}
	}

#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_set_dpms(hdmi_priv->context,
			(mode == DRM_MODE_DPMS_ON));
#endif
	/* flush hdmi port register */
	REG_WRITE(hdmi_priv->hdmib_reg, REG_READ(hdmi_priv->hdmib_reg));
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	pr_debug("Exiting %s\n", __func__);
}

/* OS Adaptation Layer Function Pointers
 * Functions required to be implemented by Linux DRM framework
 */
const struct drm_encoder_helper_funcs android_hdmi_enc_helper_funcs = {
	.dpms = android_hdmi_encoder_dpms,
	.save = android_hdmi_encoder_save,
	.restore = android_hdmi_encoder_restore,
	.mode_fixup = android_hdmi_mode_fixup,
	.prepare = android_hdmi_encoder_prepare,
	.mode_set = android_hdmi_enc_mode_set,
	.commit = android_hdmi_encoder_commit,
};

const struct drm_connector_helper_funcs
	android_hdmi_connector_helper_funcs = {
	.get_modes = android_hdmi_get_modes,
	.mode_valid = android_hdmi_mode_valid,
	.best_encoder = android_hdmi_best_encoder,
};

const struct drm_connector_funcs android_hdmi_connector_funcs = {
	.dpms = android_hdmi_connector_dpms,
	.save = android_hdmi_connector_save,
	.restore = android_hdmi_connector_restore,
	.detect = android_hdmi_detect,
	.fill_modes = android_hdmi_probe_single_connector_modes,
	.set_property = android_hdmi_set_property,
	.destroy = android_hdmi_connector_destroy,
};

const struct drm_encoder_funcs android_hdmi_enc_funcs = {
	.destroy = android_hdmi_enc_destroy,
};

/* END - OS Adaptation Layer Function Pointers
 * Functions required to be implemented by Linux DRM framework
 */

/**
 * This function initializes the hdmi driver called during bootup
 * @dev		: handle to drm_device
 * @mode_dev	: device mode
 *
 * Returns nothing
 *
 * This function initializes the hdmi driver called during bootup
 * which includes initializing drm_connector, drm_encoder, hdmi audio
 * and msic and collects all information reqd in hdmi private.
 */
void android_hdmi_driver_init(struct drm_device *dev,
				    void *mode_dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct android_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;
	struct psb_intel_output *psb_intel_output;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	bool power_on = false;

	pr_info("Enter %s", __func__);

	psb_intel_output = kzalloc(sizeof(struct psb_intel_output), GFP_KERNEL);
	if (!psb_intel_output)
		return;

	psb_intel_output->mode_dev = mode_dev;
	psb_intel_output->type = INTEL_OUTPUT_HDMI;
	psb_intel_output->dev_priv = (struct drm_psb_private *)hdmi_priv;

	connector = &psb_intel_output->base;
	encoder = &psb_intel_output->enc;

	drm_connector_init(dev, &psb_intel_output->base,
				&android_hdmi_connector_funcs,
				DRM_MODE_CONNECTOR_DVID);
	drm_connector_helper_add(connector,
				 &android_hdmi_connector_helper_funcs);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	drm_connector_attach_property(connector,
					dev->mode_config.scaling_mode_property,
					default_hdmi_scaling_mode);
#else
        drm_object_attach_property(&connector->base,
                                        dev->mode_config.scaling_mode_property,
					default_hdmi_scaling_mode);
#endif
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	/* Disable polling */
	connector->polled = 0;

	drm_encoder_init(dev, &psb_intel_output->enc, &android_hdmi_enc_funcs,
			 DRM_MODE_ENCODER_TMDS);
	drm_encoder_helper_add(encoder, &android_hdmi_enc_helper_funcs);

	drm_mode_connector_attach_encoder(&psb_intel_output->base,
					  &psb_intel_output->enc);

#ifdef OTM_HDMI_HDCP_ENABLE
	otm_hdmi_hdcp_init(hdmi_priv->context, &hdmi_ddc_read_write);
#endif
	/* Initialize the audio driver interface */
	mid_hdmi_audio_init(hdmi_priv);
	/* initialize hdmi encoder restore delayed work */
	INIT_DELAYED_WORK(&hdmi_priv->enc_work, android_hdmi_encoder_restore_wq);

	drm_sysfs_connector_add(connector);

	/* Turn on power rails for HDMI */
	power_on = otm_hdmi_power_rails_on();
	if (!power_on)
		pr_err("%s: Unable to power on HDMI rails\n", __func__);

	/* Enable hotplug detection */
	otm_hdmi_enable_hpd(true);

	pr_info("%s: Done with driver init\n", __func__);
	pr_info("Exit %s\n", __func__);
}
/*
 *
 * Internal scripts wrapper functions.
 *
 */

#ifdef OTM_HDMI_UNIT_TEST
/**
 * test_otm_hdmi_report_edid_full() - Report current EDID information
 *
 * This routine simply dumps the EDID information
 * Returns - nothing
 */
void test_otm_hdmi_report_edid_full(void)
{
	int i = 0;
	printk(KERN_ALERT "\n*** Supported Modes ***\n");

	for (i = 0; i < debug_modes_count; i++)
		printk(KERN_ALERT "Mode %02d: %s @%dHz Clk: %dKHz\n", i,
		arr_modes[i].name, arr_modes[i].frq, arr_modes[i].clk);

	printk(KERN_ALERT "\n");
}
EXPORT_SYMBOL_GPL(test_otm_hdmi_report_edid_full);
#endif
