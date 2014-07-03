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

#ifndef _OTM_HDMI_H
#define _OTM_HDMI_H


#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <drm/drm_edid.h>

#include "otm_hdmi_types.h"
#include "otm_hdmi_defs.h"
#include "otm_hdmi_eld.h"
#include <drm/drmP.h>

/**
 * Attribute name getting routine
 */
char *__pd_attr_get_name(otm_hdmi_attribute_id_t id);

/**
 * Type definition based on the function signature above
 */
typedef otm_hdmi_ret_t(*pd_attr_set_t)(otm_hdmi_attribute_t *table,
				otm_hdmi_attribute_id_t id,
				void *value,
				otm_hdmi_attribute_flag_t flags);

/**
 * Attribute name getting routine
 */
char *__pd_attr_get_name(otm_hdmi_attribute_id_t id);

/**
 * Getting given attribute
 * @context     : opaque hdmi context
 * @id          : attribute id
 * @attribute   : user provided buffer for attribute details
 * @log         : a hint wether port driver should log the call
 *
 * Returns otm_hdmi_ret_t check. Getting given attribute values
 */
otm_hdmi_ret_t otm_hdmi_get_attribute(void *context,
						otm_hdmi_attribute_id_t id,
						otm_hdmi_attribute_t *attribute,
						bool log);

/**
 * Type definition based on the function signature above
 */
typedef char *(*pd_attr_get_name_t)(otm_hdmi_attribute_id_t id);

/**
 * Attribute declaration / setting macros
 */
#define PD_DECLARE_ATTRIBUTE_BOOL(f_d, t, id, flags, \
	f_n, value) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_BOOLEAN, flags, \
	f_n(id), (void *) value, 0, 0)

#define PD_DECLARE_ATTRIBUTE_BOOL_CUSTOM(f_d, t, id, flags, \
	name, value) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_BOOLEAN, flags, \
	name, (void *) value, 0, 0)

#define PD_DECLARE_ATTRIBUTE_UINT(f_d, t, id, flags, \
	f_n, value, min, max) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_UINT, flags, \
	f_n(id), (void *) value, min, max)

#define PD_DECLARE_ATTRIBUTE_UINT_CUSTOM(f_d, t, id, flags, \
	name, value, min, max) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_UINT, flags, \
	name, (void *) value, min, max)

#define PD_DECLARE_ATTRIBUTE_STRING(f_d, t, id, \
	flags, f_n, value) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_STRING, flags, \
	f_n(id), (void *) value, 0, 0)

#define PD_DECLARE_ATTRIBUTE_STRING_CUSTOM(f_d, t, id, \
	flags, name, value) \
	f_d(t, id, OTM_HDMI_ATTR_TYPE_STRING, flags,	\
	name, (void *) value, 0, 0)

/**
 * Attribute values accessor macros
 */
#define PD_ATTR_BOOL(attr) ((attr).content._bool.value)
#define PD_ATTR_UINT(attr) ((attr).content._uint.value)

/**
 * Shortcuts for common flags combinations
 */
#define PD_ATTR_FLAGS_RWSI (OTM_HDMI_ATTR_FLAG_WRITE | \
			OTM_HDMI_ATTR_FLAG_SUPPORTED | \
			OTM_HDMI_ATTR_FLAG_INTERNAL)

#define PD_ATTR_FLAGS_RWS (OTM_HDMI_ATTR_FLAG_WRITE | \
			OTM_HDMI_ATTR_FLAG_SUPPORTED)

#define PD_ATTR_FLAGS_RS (OTM_HDMI_ATTR_FLAG_SUPPORTED)

/**
 * Display timing information
 */
typedef struct {
	unsigned short width;		/* width                            */
	unsigned short height;		/* height                           */
	otm_hdmi_refresh_t refresh;	/* refresh rate                     */
	unsigned long dclk;		/* refresh rate dot clock in kHz    */
	unsigned short htotal;		/* horizontal total                 */
	unsigned short hblank_start;	/* horizontal blank start           */
	unsigned short hblank_end;	/* horizontal blank end             */
	unsigned short hsync_start;	/* horizontal sync start            */
	unsigned short hsync_end;	/* horizontal sync end              */
	unsigned short vtotal;		/* vertical total                   */
	unsigned short vblank_start;	/* vertical blank start             */
	unsigned short vblank_end;	/* vertical blank end               */
	unsigned short vsync_start;	/* vertical sync start              */
	unsigned short vsync_end;	/* vertical sync end                */
	unsigned long mode_info_flags;	/* flags                            */
	otm_hdmi_stereo_t stereo_type;	/* stereo mode type                 */
	unsigned long metadata;		/* port driver specific information */
} otm_hdmi_timing_t;

/* set timing from cea modes */
int otm_hdmi_timing_from_cea_modes(unsigned char *buffer,
				   otm_hdmi_timing_t *pdts);

/* get timings of a mode */
const otm_hdmi_timing_t *otm_hdmi_get_mode_timings(void *context,
					int hdisplay,
					int vdisplay,
					int vrefresh);

/**
 * otm_hdmi_hpd_init - Initialize and enable HPD driver service.
 *
 * No input arguments
 *
 * Returns - OTM_HDMI_SUCCESS on successful initialization
 * Returns - OTM_HDMI_ERR_FAILED on init failure
 */
otm_hdmi_ret_t otm_hdmi_hpd_init(void);

/**
 * otm_hdmi_hpd_deinit - Deinit HPD driver service.
 *
 * No input arguments
 *
 * Returns - OTM_HDMI_SUCCESS
 */
otm_hdmi_ret_t otm_hdmi_hpd_deinit(void);


/**
 * otm_hdmi_get_hpd_pin - get hdmi hpd pin number.
 *
 * No input arguments
 *
 * Returns - pin number
 */
unsigned int otm_hdmi_get_hpd_pin(void);


/**
 * otm_hdmi_override_cable_status - override hdmi hpd cable status.
 *
 * Input: override state and auto test state
 */
void otm_hdmi_override_cable_status(bool state, bool auto_state);






/**
 * otm_hdmi_hpd_callback_register - Register a callback for HPD events
 * @context: hdmi device context
 * @phdmi_irq_cb: function pointer for hotplug/unplug IRQ callbacks.
 * @data: data for irq callback
 *
 * Perform HPD IRQ call back initialization
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_hpd_callback_register(void *context,
					      irqreturn_t (*phdmi_irq_cb) (int, void*),
					      void *data);

/* parse the raw edid and fill the capability table */
otm_hdmi_ret_t otm_hdmi_edid_parse(void *ctx, otm_hdmi_use_edid_t use_edid);

/* parse extension EDID blocks and fill the capability table */
otm_hdmi_ret_t otm_hdmi_edid_extension_parse(void *context,
			 struct edid *raw_edid,
			 struct i2c_adapter *adapter);

/* prepare hdmi eld packet and copy it to the input buffer */
otm_hdmi_ret_t otm_hdmi_get_eld(void *ctx, otm_hdmi_eld_t *eld);

/* init hdmi device driver */
otm_hdmi_ret_t otm_hdmi_device_init(void **context, struct pci_dev *pdev);

/*deinit hdmi device driver */
void otm_hdmi_deinit(void *context);


/* read edid information */
unsigned char *otm_hdmi_read_edid(void);

/* turn HDMI power rails on */
bool otm_hdmi_power_rails_on(void);

/* turn HDMI power rails off */
bool otm_hdmi_power_rails_off(void);

/* turn HDMI power islands on */
bool otm_hdmi_power_islands_on(void);

/* turn HDMI power islands off */
void otm_hdmi_power_islands_off(void);

/* enable/disable IRQ and CPD_HPD */
bool otm_hdmi_enable_hpd(bool enable);

/* control HDMI vblank interrupt */
void otm_hdmi_vblank_control(struct drm_device *dev, bool on);

/*
 * otm_hdmi_get_cable_status - Get HDMI cable connection status
 * @context: hdmi device context
 *
 * Returns - boolean state.
 * true - HDMI cable connected
 * false - HDMI cable disconnected
 */
bool otm_hdmi_get_cable_status(void *context);

/* get pixel clock range */
otm_hdmi_ret_t otm_hdmi_get_pixel_clock_range(unsigned int *pc_min,
					unsigned int *pc_max);

/*
 *  Getting given attribute
 *  @param [in ] id      : attribute id
 *  @param [out] data    : user provided buffer for attribute details
 *  @param [in ] log     : a hint wether port driver should log the call
 */
otm_hdmi_ret_t otm_hdmi_get_attribute(void *context,
					otm_hdmi_attribute_id_t id,
					otm_hdmi_attribute_t *attribute,
					bool log);

/* check the mode is preferred or not */
bool otm_hdmi_is_preferred_mode(int hdisplay,
					int vdisplay, int refresh);

/* set the raw edid into HDMI context */
otm_hdmi_ret_t otm_hdmi_set_raw_edid(void *context, char *raw_edid);

/* get the raw edid from HDMI context */
otm_hdmi_ret_t otm_hdmi_get_raw_edid(void *context, char **raw_edid);

/* set the scaling type for HDMI display */
otm_hdmi_ret_t otm_hdmi_set_scaling_type(void *context, int scaling);

/* check the connection is hdmi or dvi */
bool otm_hdmi_is_monitor_hdmi(void *context);

 /**
 * Description: program display and pfit registers.
 *
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width:		allocated frame buffer dimensions
 * @fb_height:		allocated frame buffer dimensions
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_crtc_set_scaling(void *context,
			otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode,
			int fb_width, int fb_height);

/*
 * Description: crtc mode set function for hdmi.
 *
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width, fb_height:allocated frame buffer dimensions
 * @pclk_khz:		tmds clk value for the best pll and is needed for audio.
 *			This field has to be moved into OTM audio
 *			interfaces when implemented
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_crtc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode, int fb_width,
			int fb_height, uint32_t *pclock_khz);

/*
 * Description: encoder mode set function for hdmi. enables phy.
 *		set correct polarity for the current mode.
 *
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_enc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode);

/*
 * Description: set avi infoframe based on mode
 *
 * @context:            hdmi_context
 * @mode:               mode requested
 *
 * Returns:     OTM_HDMI_SUCCESS on success
 *              OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_infoframes_set_avi(void *context,
					   otm_hdmi_timing_t *mode);

/*
 * Description:	disable all infoframes
 *
 * @context:	hdmi_context
 *
 * Returns:	OTM_HDMI_ERR_NULL_ARG on NULL parameters
 *		OTM_HDMI_SUCCESS on success
 */
otm_hdmi_ret_t otm_hdmi_disable_all_infoframes(void *context);

/*
 * Description: save hdmi display registers
 *
 * @context:	hdmi_context
 *
 * Returns:	none
 */
void otm_hdmi_save_display_registers(void *context, bool connected);

/*
 * Description: disable HDMI display
 *
 * @context:	hdmi_context
 *
 * Returns:	none
 */
void otm_disable_hdmi(void *context);

/*
 * Description: restore hdmi display registers and enable the display
 *
 * @context:	hdmi_context
 *
 * Returns:	none
 */
void otm_hdmi_restore_and_enable_display(void *context, bool connected);

/**
 * Description: notify security component of hdcp and hdmi cable status
 *
 * @hdcp	HDCP status: true if phase1 is enabled
 * @cable	HDMI connection status: true if connected
 *
 * Returns:	none
 */
void otm_hdmi_update_security_hdmi_hdcp_status(bool hdcp, bool cable);


/* TODO: Need refactor the logging and attr table */

/*
 * Logging macros
 */

/*
 * Table of attributes
 */
extern otm_hdmi_attribute_t otm_hdmi_attributes_table[];

#define ATTR otm_hdmi_attributes_table

typedef enum {
	LOG_LEVEL_ERROR = 0,	/* Error messages; Will always be printed */
	__LOG_LEVEL_MIN = LOG_LEVEL_ERROR,
	/* Add log levels below this line */
	LOG_LEVEL_HIGH = 1,	/* Printed if 'debug' is set to 1 or higher */
	LOG_LEVEL_LOW,	/* Printed if 'debug' is set to 2 or higher */
	LOG_LEVEL_VBLANK,	/* Printed if 'debug' at highest level */
	LOG_LEVEL_DETAIL, /* Print detailed info */
	/* Add log levels above this line */
	__LOG_LEVEL_TEMP_UPPER__,	/* DO NOT USE */
	__LOG_LEVEL_MAX = __LOG_LEVEL_TEMP_UPPER__ - 1,
} log_level_t;

/* Used to log entry in to a function */
#define LOG_ENTRY(log_level) \
	if ((log_level) <= (int) PD_ATTR_UINT(ATTR[OTM_HDMI_ATTR_ID_DEBUG])) \
		printk("OTM_HDMI: Entering %s\n", __func__)

/* Used to log exit from a function */
#define LOG_EXIT(log_level, rc) \
	if ((log_level) <= (int) PD_ATTR_UINT(ATTR[OTM_HDMI_ATTR_ID_DEBUG])) \
		printk("OTM_HDMI: Exiting %s with %d\n", __func__, rc)

#define LOG_PRINT(log_level, args...) \
	if ((log_level) <= (int) PD_ATTR_UINT(ATTR[OTM_HDMI_ATTR_ID_DEBUG])) \
		printk("OTM_HDMI:" args)

#define LOG_ERROR(msg, args...) LOG_PRINT(LOG_LEVEL_ERROR, msg, ##args)

/*
 * Bits in 'mode_info_flags' field
 */
#define PD_SCAN_INTERLACE     0x80000000 /* Interlaced pipe configuration     */
#define PD_PIXEL_REPLICATION  0x40000000 /* Mode uses pixel replication       */
#define PD_HSYNC_HIGH         0x20000000 /* HSYNC is active high              */
#define PD_VSYNC_HIGH         0x10000000 /* VSYNC is active high              */
#define PD_CLOCK_OVERSAMPLE   0x00020000 /* Indicates dot clock is a multiple */
					 /* of the MDVO clock; See VDC_SetMode*/
#define PD_EVO_DITHER         0x00008000 /* Enable EVO dithering              */
#define PD_EVO_FORMAT_8       0x00004000 /* Set EVO 8 bit mode                */
#define PD_EVO_CLK_INV        0x00002000 /* Invert EVO pixel clock            */
#define PD_WSS_WORKAROUND     0x00000800 /* PAL Wide Screen Signal workaround */
#define PD_DTV_MODE           0x00000400 /* Digital Panel mode                */
#define PD_SVBI               0x00000010 /* Software VBI supported timings    */
#define PD_AR_16_BY_9         OTM_HDMI_PAR_16_9 /* 16:9 aspect ratio          */
#define OTM_HDMI_COLOR_RANGE_MASK	0x0c

/*
 * Description: report edid tool helper function
 *
 * Returns:	none
 */
#ifdef OTM_HDMI_UNIT_TEST
void test_otm_hdmi_report_edid(void);
#endif

/*
 * Description: function to get hdmi_context
 *		handle
 *
 * Returns:	hdmi_context
 */
extern void *otm_hdmi_get_context(void);

typedef struct _sqword {
	union {
		unsigned long long quad_part;
		struct {
			unsigned long low_part;
			unsigned long high_part;
		} u;
		struct {
			uint8_t byte[8];
		};
	};
} sqword_t;

#endif /* _OTM_HDMI_H */
