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

#ifndef __ANDROID_HDMI_H
#define __ANDROID_HDMI_H

#include <linux/types.h>
#include <linux/switch.h>
#include <drm/drmP.h>

#define CEA_EXT     0x02
#define VTB_EXT     0x10
#define DI_EXT      0x40
#define LS_EXT      0x50
#define MI_EXT      0x60

/* Define the monitor type HDMI or DVI */
#define MONITOR_TYPE_HDMI 1
#define MONITOR_TYPE_DVI  2

struct android_hdmi_priv {
	/* common */
	struct drm_device *dev;

	struct switch_dev sdev;

	/*medfield specific */
	u32 hdmib_reg;
	u32 save_HDMIB;
	u32 clock_khz;

	/* Delayed Encoder Restore */
	struct drm_display_mode *current_mode;
	bool need_encoder_restore;
	struct delayed_work enc_work;
	struct work_struct suspend_wq;
	void *data;

	bool hdmi_suspended;
	bool hdmi_audio_enabled;
	bool is_hdcp_supported;
	int monitor_type;
	void *context;
};

extern int psb_intel_panel_fitter_pipe(struct drm_device *dev);
extern void mid_hdmi_audio_init(struct android_hdmi_priv *p_hdmi_priv);

#ifdef CONFIG_SUPPORT_HDMI

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
			      void *mode_dev);




int android_hdmi_irq_test(struct drm_device *dev);


/**
 * This function sets the hdmi driver during bootup
 * @dev		: handle to drm_device
 *
 * Returns nothing
 *
 * This function is called from psb driver to setup the
 * hdmi driver. Called only once during boot-up of the system
 */
void android_hdmi_driver_setup(struct drm_device *dev);

 /**
 * Description: programming display registers as per the scaling property.
 *
 * @crtc:		crtc
 *
 * Returns:	0 on success
 *		-1 on failure
 */
int android_hdmi_set_scaling_property(struct drm_crtc *crtc);

/**
 * Description: crtc mode set for hdmi pipe.
 *
 * @crtc:		crtc
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @x, y, old_fb:	old frame buffer values used for flushing old plane.
 *
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */
int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb);


/**
 * Store the HDMI registers and enable the display
 * Input parameters:
 *	psDrmDev: Drm Device.
 * Returns: none
 */
void android_hdmi_resume_display(struct drm_device *dev);

/**
 * Save the HDMI display registers and disable the display
 * Input parameters:
 *	psDrmDev: Drm Device.
 * Returns: none
 */
void android_hdmi_suspend_display(struct drm_device *dev);

/**
 * Prepare HDMI EDID-like data and copy it to the given buffer
 * Input parameters:
 * @dev: drm Device
 * @eld: pointer to otm_hdmi_eld_t data structure
 *
 * Returns:	0 on success
 *		-EINVAL on NULL input arguments
 */
int android_hdmi_get_eld(struct drm_device *dev, void *eld);

/**
 * Enable HDCP on HDMI display
 * @dev:	drm device
 *
 * Returns:	true on success else false
 */

/**
 * get DPLL clock in khz
 * Input parameters:
 * @dev: drm Device
 *
 * Returns:  clock in khz
 */
uint32_t android_hdmi_get_dpll_clock(struct drm_device *dev);

/**
 * enable HDMI HDCP
 * Input parameters:
 * @dev: Drm Device
 *
 * Returns:	true on success else false
 */
bool android_enable_hdmi_hdcp(struct drm_device *dev);

/**
 * disable HDCP on HDMI display
 * @dev:	drm device
 *
 * Returns:	true on success else false
 */
bool android_disable_hdmi_hdcp(struct drm_device *dev);

/**
 * Query whether HDCP is enabled & encrypting on HDMI display
 * @dev:	drm device
 *
 * Returns:	true if encrypting else false
 */
bool android_check_hdmi_hdcp_enc_status(struct drm_device *dev);

/**
 * Query HDCP Phase 3 Link Status
 * @dev:	drm device
 *
 * Returns:	true if link is not compromised else false
 */
bool android_check_hdmi_hdcp_link_status(struct drm_device *dev);

/**
 * Query presence of a HDCP Sink Device
 * @dev:	drm device
 * @bksv:	ksv value of the sink device will be returned on success
 *
 * Returns:	true on successful detection else false
 */
bool android_query_hdmi_hdcp_sink(struct drm_device *dev, uint8_t *bksv);


/**
 * Description: check whether hdmi/dvi is connected or not.
 *
 * @dev:	drm device
 *
 * Returns:	true if hdmi/dvi is connected.
 *		false if hdmi/dvi is not connected.
 */
bool android_hdmi_is_connected(struct drm_device *dev);


#else /* CONFIG_SUPPORT_HDMI */

static inline void android_hdmi_driver_init(struct drm_device *dev,
						void *mode_dev) {}

static inline void android_hdmi_driver_setup(struct drm_device *dev) {}

static inline int android_hdmi_set_scaling_property(struct drm_crtc *crtc)
{ return 0; }

static inline int android_hdmi_crtc_mode_set(struct drm_crtc *crtc,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode,
				int x, int y,
				struct drm_framebuffer *old_fb) { return 0; }

static inline void android_hdmi_resume_display(
				struct drm_device *dev) {}

static inline void android_hdmi_suspend_display(
				struct drm_device *dev) {}

static inline int android_hdmi_get_eld(struct drm_device *dev, void *eld)
{ return 0; }

static uint32_t android_hdmi_get_dpll_clock(struct drm_device *dev)
{ return 0; }

static inline bool android_enable_hdmi_hdcp(struct drm_device *dev)
{ return false; }
static inline bool android_disable_hdmi_hdcp(struct drm_device *dev)
{ return false; }
static inline bool android_check_hdmi_hdcp_enc_status(struct drm_device *dev)
{ return false; }
static inline bool android_check_hdmi_hdcp_link_status(struct drm_device *dev)
{ return false; }
static inline bool android_query_hdmi_hdcp_sink(struct drm_device *dev,
						uint8_t *bksv)
{ return false; }

static inline bool android_hdmi_is_connected(struct drm_device *dev)
{ return false; }

#endif /* CONFIG_SUPPORT_HDMI */

/*
 * Description: hdmi helper function to parse cmdline option
 *		from hdmicmd tool
 *
 * @cmdoption:	cmdline option
 *
 * Returns:	error codes 0(success),-1(cmd option),-2(invalid input)
 */
int otm_cmdline_parse_option(char *cmdoption);

/*
 * Description: hdmi helper function to parse vic option
 *		from hdmicmd tool
 *
 * @cmdoption:	cmdline option
 *
 * Returns:	error codes 0(success),-1(error)
 */
int otm_cmdline_set_vic_option(int vic);

/*
 * Description: hdmi helper function to print cmdline options
 *		from hdmicmd tool
 *
 * Returns:	none
 */
void otm_print_cmdline_option(void);

/*
 * Description: hdmi helper function to print edid information
 *		from report_edid tool
 *
 * Returns:	none
 */
void test_otm_hdmi_report_edid_full(void);

#endif /* __ANDROID_HDMI_H */
