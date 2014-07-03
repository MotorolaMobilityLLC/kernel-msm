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


#include <linux/time.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "otm_hdmi.h"
#include "ipil_hdmi.h"

#include "hdmi_internal.h"
#include "hdmi_timings.h"
#ifdef OTM_HDMI_HDCP_ENABLE
#include "hdcp_api.h"
#endif
#include "edid.h"
#include "ps_hdmi.h"
#include "ips_hdmi.h"
#include "infoframes_api.h"

#define OTM_HDMI_NAME "OTM HDMI"

/*
 * Table of attributes
 */
otm_hdmi_attribute_t otm_hdmi_attributes_table
		[OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES];

/* Placeholder for all TX supported modes */
static const otm_hdmi_timing_t *g_video_modes[MAX_TIMINGS];
/* Placeholder for all TX supported modes per CEA 861E spec used by EDID parser
 */
static const otm_hdmi_timing_t *g_video_modes_ref[MAX_TIMINGS];
static otm_hdmi_ret_t otm_hdmi_attr_get_validate(otm_hdmi_attribute_id_t id);

static otm_hdmi_ret_t __pd_attr_declare(otm_hdmi_attribute_t *table,
				otm_hdmi_attribute_id_t id,
				otm_hdmi_attribute_type_t type,
				otm_hdmi_attribute_flag_t flags,
				char *name,
				void *value,
				unsigned int min,
				unsigned int max);

static unsigned int g_gpio = GPIO_MIN;
static unsigned int g_dtv;
static unsigned int g_dc = 1;

#define EDID_SIGNATURE 0x00FFFFFFFFFFFF00ull

static hdmi_context_t *g_context;

/* This table preserves the special timings for DTV models */
static const otm_hdmi_timing_t *static_dtv_modes[] = {
	&MODE_1920x1080p60__PANEL,
	&MODE_1920x1080p50__PANEL,
	&MODE_1920x1080p48__PANEL,
	&MODE_1280x720p60__PANEL_FS,
	&MODE_1280x720p50__PANEL_FS,
	&MODE_1920x540p60__PANEL_FS,
	&MODE_1920x540p50__PANEL_FS,
	&MODE_920x1080p60__PANEL_FS,
	&MODE_920x1080p50__PANEL_FS,
	&MODE_1920x1080p30__PANEL_FS,
	&MODE_1920x1080p25__PANEL_FS,
	&MODE_1920x1080p24__PANEL_FS,
};

/* This table contains list of audio timings supported by Intel CE Media
 * Processors and used in the situations when EDID is not available
 *
 * Note: Do *NOT* add declaration WMA in here as we dont have approval for that
 */
static otm_hdmi_audio_cap_t static_audio_modes[] = {
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_PCM, 8, ALL_SF, ALL_SS),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_AC3, 8, ALL_SF, 640 / 8),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DTS, 8, ALL_SF, 1536 / 8),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DDP, 8, ALL_SF, 0),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_DTSHD, 8, ALL_SF, 0),
	DECLARE_AUDIO_CAP(OTM_HDMI_AUDIO_FORMAT_MLP, 8, ALL_SF, 0),
};

/**
 * This function called by edid_print tool internally
 * @ctx		: hdmi context handle
 * @edid		: edid information
 *
 * Returns nothing. Called by edid_print tool to print
 * edid information to dmesg for debugging purposes
 */
static void __hdmi_report_edid(hdmi_context_t *ctx, edid_info_t *edid)
{
	int i = 0;

	LOG_PRINT(LOG_LEVEL_HIGH, "----------------------\n");
	LOG_PRINT(LOG_LEVEL_HIGH, "Name     : %s\n", edid->product_name);
	LOG_PRINT(LOG_LEVEL_HIGH, "Year     : %d\n", edid->product_year);
	LOG_PRINT(LOG_LEVEL_HIGH, "SN       : %d\n", edid->product_sn);
	LOG_PRINT(LOG_LEVEL_HIGH, "Type     : %s\n",
			edid->hdmi ? "HDMI" : "DVI");
	LOG_PRINT(LOG_LEVEL_HIGH, "YCbCr444 : %s\n",
			edid->ycbcr444 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "YCbCr422 : %s\n",
			edid->ycbcr422 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "30 bpp   : %s\n",
			edid->dc_30 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "36 bpp   : %s\n",
			edid->dc_36 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "48 bpp   : %s\n",
			edid->dc_48 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "DC_YUV   : %s\n",
			edid->dc_y444 ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "Max CLK  : %d\n",
			edid->max_tmds_clock);
	LOG_PRINT(LOG_LEVEL_HIGH, "Lip sync : %s\n",
			edid->latency_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "ILip sync: %s\n",
			edid->latency_int_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "Vid lat  : %d\n",
			edid->latency_video);
	LOG_PRINT(LOG_LEVEL_HIGH, "Aud lat  : %d\n",
			edid->latency_audio);
	LOG_PRINT(LOG_LEVEL_HIGH, "IVid lat : %d\n",
			edid->latency_video_interlaced);
	LOG_PRINT(LOG_LEVEL_HIGH, "IAud lat : %d\n",
			edid->latency_audio_interlaced);
	LOG_PRINT(LOG_LEVEL_HIGH, "HDMI VID : %s\n",
			edid->hdmi_video_present ? "Y" : "N");
	LOG_PRINT(LOG_LEVEL_HIGH, "HDMI 3D  : %s\n",
			edid->enabled_3d ? "Y" : "N");

	LOG_PRINT(LOG_LEVEL_HIGH, "SPA      : %d.%d.%d.%d\n",
		  (edid->spa & 0xF000) >> 12,
		  (edid->spa & 0x0F00) >> 8,
		  (edid->spa & 0x00F0) >> 4, (edid->spa & 0x000F) >> 0);

	LOG_PRINT(LOG_LEVEL_HIGH, "Supported timings [%d]:\n",
		  edid->num_timings);

	for (i = 0; i < edid->num_timings; i++)
		print_pd_timing(&edid->timings[i], edid->order[i]);

	LOG_PRINT(LOG_LEVEL_HIGH, "Audio capabilities:\n");
	for (i = 0; i < edid->num_caps; i++)
		print_audio_capability(&edid->audio_caps[i]);

	print_speaker_layout(edid->speaker_map);
	LOG_PRINT(LOG_LEVEL_HIGH, "----------------------\n");
}

/**
 * This function overrides the edid information with static timings
 * @ctx		: hdmi context handle
 * @edid		: edid information
 * @safe		: boolean for edid option
 *
 * Returns OTM_HDMI_SUCCESS or OTM_HDMI_ERR_INTERNAL
 *
 * This function overrides the edid information with static timings
 */
static otm_hdmi_ret_t __hdmi_edid_override(hdmi_context_t *ctx,
				edid_info_t *edid,
				bool safe)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	unsigned int i, n = 0;
	bool hdmi;
	bool dc_30, dc_36;
	const otm_hdmi_timing_t **modes = NULL;
	bool hdcp = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]);
	unsigned int n_modes_dtv = NUM_ENTRIES_IN(static_dtv_modes);

	/* Verify pointers */
	if (!(edid)) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	/* Save device type and DC caps */
	hdmi = !ctx->dtv && (safe ? edid->hdmi : true);
	dc_30 = hdmi && edid->dc_30;
	dc_36 = hdmi && edid->dc_36;

	/* Clear EDID */
	memset(edid, 0, sizeof(edid_info_t));

	/* Set device type */
	edid->hdmi = hdmi;

	/* Pick caps table based on whether we are HDMI TX or DTV */
	modes = ctx->dtv ? static_dtv_modes : g_video_modes;
	n = ctx->dtv ? n_modes_dtv : ctx->n_modes_tx;

	/* Add all supported video modes */
	for (i = edid->num_timings = 0; i < n; i++) {
		edid->timings[edid->num_timings++] = *modes[i];

		/* Do NOT advertise 3D modes in DVI mode unless we are in DTV
		 * mode which means always use DTV static table
		 */
		if (!ctx->dtv && !hdmi &&
			modes[i]->stereo_type != OTM_HDMI_STEREO_NONE) {
			edid->num_timings--;
		}
	}

	/* Set HDCP based on DTV indicator */
	PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_HDCP]) =
				ctx->dtv ? false : hdcp;

	/* Dont bother with HDMI caps if we are in DVI mode */
	if (!(hdmi))
		goto exit;

	/* Add all supported audio modes */
	edid->num_caps = NUM_ENTRIES_IN(static_audio_modes);
	for (i = 0; i < edid->num_caps; i++)
		edid->audio_caps[i] = static_audio_modes[i];

	/* Enable all possible speaker allocation maps */
	edid->speaker_map |= 0x3ff;

	/* Indicate support of deep color and YCbCr output */
	edid->ycbcr444 = true;
	edid->ycbcr422 = true;
	edid->dc_30 = safe ? dc_30 : true;
	edid->dc_36 = safe ? dc_36 : true;

exit:
	return rc;
}

/**
 * otm_hdmi_edid_parse() - fill capability table
 * @context:      hdmi context
 * @use_edid: True or False
 *
 * Returns - check otm_hdmi_ret_t
 *
 * This routine files capability table.
 */
otm_hdmi_ret_t otm_hdmi_edid_parse(void *context, otm_hdmi_use_edid_t use_edid)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	i2c_read_t edid_foo = ps_hdmi_i2c_edid_read;
	bool cable = PD_ATTR_BOOL(ATTRS[OTM_HDMI_ATTR_ID_CABLE_STATUS]);
	edid_info_t *edid;
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	/* Verify pointers */
	if (!ctx) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	/* Init locals */
	edid = &ctx->edid_int;

	/* Begin EDID update protection */
	mutex_lock(&ctx->modes_sema);

	/* Clear EDID */
	memset(edid, 0, sizeof(edid_info_t));

	/* Setup reference table for parsing */
	edid->num_ref_timings = ctx->n_modes_ref;
	edid->ref_timings = g_video_modes_ref;

	/* DTV mode will use static DTV timings directly */
	if (ctx->dtv)
		goto edid_override;

	switch (use_edid) {
	case OTM_HDMI_USE_EDID_REAL:
		/* Try reading EDID. If reading failed pick overriding strategy
		 * based on cable status
		*/
		rc = edid_parse(edid, edid_foo, ctx);
		if (rc != OTM_HDMI_SUCCESS) {
			pr_debug("Failed to read EDID info\n");
			use_edid = cable ? OTM_HDMI_USE_EDID_SAFE :
				OTM_HDMI_USE_EDID_NONE;
		}
		break;
	case OTM_HDMI_USE_EDID_SAFE:
		/* In safe mode we still need real EDID */
		edid_parse(edid, edid_foo, ctx);
		break;
	case OTM_HDMI_USE_EDID_NONE:
		/* In full override mode we dont care of real EDID
		 * so do nothing
		 */
		break;
	default:
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	/* Dont bother with static timings if we are using real EDID */
	if (use_edid == OTM_HDMI_USE_EDID_REAL)
		goto twin_caps;

edid_override:
	/* Use static timings */
	__hdmi_edid_override(ctx, edid, use_edid == OTM_HDMI_USE_EDID_SAFE);

	/* Insertion of twin entries below is done right in the parsed table of
	 * modes without knowledge of its maximum size. Be extra careful about
	 * it and check that MAX_TIMINGS is big enough; This needs to be fixed
	 * in long run
	 */
twin_caps:
	/* Print warning message in case there are no timings */
	if (ctx->edid_int.num_timings == 0) {
		LOG_ERROR
		    ("----------------- WARNING -----------------------\n");
		LOG_ERROR
		    ("-- TV timings are not available		--\n");
		LOG_ERROR
		    ("-- To resolve this switch to static TV timings --\n");
	}
	/* Update EDID availability indicator */
	PD_ATTR_UINT(ATTRS[OTM_HDMI_ATTR_ID_USE_EDID]) = use_edid;

	/* End EDID update protection */
	mutex_unlock(&ctx->modes_sema);

exit:
	return rc;
}

/**
 * Parse EDID extension blocks
 * @context:	hdmi context
 * @raw_edid:	raw edid of first 2 blocks, read by DRM
 *
 * Returns:	check otm_hdmi_ret_t
 *
 * This routine fills capability table.
 */
otm_hdmi_ret_t otm_hdmi_edid_extension_parse(void *context,
			struct edid *raw_edid,
			struct i2c_adapter *adapter)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	unsigned char *edid = (u8 *) raw_edid;
	edid_info_t   *edid_info;
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	pr_debug("enter %s\n", __func__);

	/* Verify pointers */
	if (!ctx || !edid || !adapter) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}

	/* Init locals */
	edid_info = &ctx->edid_int;

	/* Begin EDID update protection */
	mutex_lock(&ctx->modes_sema);

	/* Clear EDID */
	memset(edid_info, 0, sizeof(edid_info_t));

	/* Setup reference table for parsing */
	edid_info->num_ref_timings = ctx->n_modes_ref;
	edid_info->ref_timings = g_video_modes_ref;

	rc = edid_extension_parse(adapter, edid_info, edid);
	if (rc != OTM_HDMI_SUCCESS)
		pr_debug("Failed to read EDID info\n");

	/* End EDID update protection */
	mutex_unlock(&ctx->modes_sema);

exit:
	pr_debug("exit %s (ret = %d)\n", __func__, rc);
	return rc;
}


/**
* prepare hdmi eld packet and copy it to the given buffer
* @ctx: hdmi context
* @eld: pointer to otm_hdmi_eld_t structure
*
* Returns: OTM_HDMI_SUCCESS, OTM_HDMI_ERR_INTERNAL or OTM_HDMI_ERR_NULL_ARG
*/
otm_hdmi_ret_t otm_hdmi_get_eld(void *ctx, otm_hdmi_eld_t *eld)
{
	hdmi_context_t *context = (hdmi_context_t *)ctx;
	edid_info_t *edid_int = NULL;

	if (!context)
		return OTM_HDMI_ERR_INTERNAL;

	if (eld == NULL)
		return OTM_HDMI_ERR_NULL_ARG;

	memset(eld->eld_data, 0, sizeof(OTM_HDMI_ELD_SIZE));

	edid_int = &(context->edid_int);

	/* ELD Version Number - version 2, supporting CEA 861D or below */
	eld->eld_ver = 2;

	/* Version number of the ELD, reserved for future */
	eld->veld_ver = 0;

	/* Length of the Baseline structure, in number of DWORD. Maximum 80 bytes as per EELD proposal */
	eld->baseline_eld_length = OTM_HDMI_ELD_SIZE/4;

	/* monitor name length */
	/* monitor name is not populated intentionally */
	eld->mnl = 0;
	/*011b - indicates CEA 861 B, C or D */
	eld->cea_edid_rev_id = 3;

	/* Capabilities */
	eld->hdcp = 1;
	eld->ai_support = edid_int->supports_ai;
	/* 00b - indicate HDMI connection type */
	eld->connection_type = 0;
	/* number of Short Audio Descriptors  (SAD) */
	eld->sadc = edid_int->short_audio_descriptor_count;

	/* delay of video compared to audio in terms of units of 2ms */
	eld->audio_synch_delay = 0;
	/* valid value of video/audio latency ranges from 1 to 251, see HDMI spec 3a */
	/* Section 8.3.2 HDMI Vendor Specific Data Block (VSDB) */
	if (edid_int->latency_video > 0 &&
		edid_int->latency_video < 252 &&
		edid_int->latency_audio > 0 &&
		edid_int->latency_audio < 252) {
		eld->audio_synch_delay = edid_int->latency_video - edid_int->latency_audio;
		/* the maximum delay is 500ms */
		if (eld->audio_synch_delay > 250)
			eld->audio_synch_delay = 0;

		pr_debug("video latency: %d, audio_latency: %d, lipsync: %d\n",
			edid_int->latency_video,
			edid_int->latency_audio,
			eld->audio_synch_delay);
	}

	/* bits 8 - 10 are ignored: FLH/FRH (Front Left/Right High), TC (Top Center), FCH (Front Center High) */
	eld->speaker_allocation_block = (uint8_t) (edid_int->speaker_map & 0xff);

	/* The following fields are intentionally ignored */
	/* eld->port_id_value */
	/* eld->manufacturer_id */
	/* eld->product_id */

	/* 64-byte of baseline data, including monitor names and and list of 3-byte SAD */
	/* monitor name is not populated here */
	if (edid_int->short_audio_descriptor_count) {
		WARN_ON(edid_int->short_audio_descriptor_count >
						sizeof(eld->mn_sand_sads));
		memcpy(eld->mn_sand_sads, edid_int->short_audio_descriptor_data,
		       min_t(int, MAX_DATA_BLOCK_SIZE,
			     3 * edid_int->short_audio_descriptor_count));
	}

	return OTM_HDMI_SUCCESS;
}

/**
 * otm_hdmi_timing_from_cea_modes() - get timings for cea modes
 * @buffer: the extension block buffer
 * @timings: the result CEA timings extacted from the buffer
 *
 * Returns - the number of modes in the timings
 */
int otm_hdmi_timing_from_cea_modes(unsigned char *buffer,
				   otm_hdmi_timing_t *timings)
{
	edid_info_t *edid  = NULL;

	if (buffer == NULL)
		return 0;

	if (timings == NULL)
		return 0;

	if (g_context == NULL)
		return 0;

	edid = &g_context->edid_int;
	if (edid == NULL)
		return 0;

	edid->num_ref_timings = g_context->n_modes_ref;
	edid->ref_timings = g_video_modes_ref;

	return edid_parse_pd_timing_from_cea_block(edid, buffer, timings);
}

/**
 * otm_hdmi_get_mode_timings() - get timings of a mode, given:
 * @context : HDMI context
 * @hdisplay: mode width
 * @vdisplay: mode height
 * @vrefresh: mode refresh rate
 *
 * Returns matching mode, NULL otherwise.
 */
const otm_hdmi_timing_t *otm_hdmi_get_mode_timings(void *context,
						int hdisplay,
						int vdisplay,
						int vrefresh)
{
	const otm_hdmi_timing_t *mode = NULL;
	int i, refresh_rate;

	if (hdisplay < 0 || vdisplay < 0 || vrefresh < 0)
		goto exit;

	for (i = 0; i < MAX_TIMINGS; i++) {
		mode = g_video_modes[i];
		refresh_rate = ((mode->dclk * 1000) /
					(mode->htotal * mode->vtotal));
		if (hdisplay == mode->width &&
			vdisplay == mode->height &&
			vrefresh == refresh_rate)
			return mode;
	}
exit:
	return NULL;
}

/**
 * otm_hdmi_hpd_init - Initialize and enable HPD driver service.
 *
 * No input arguments
 *
 * Returns - OTM_HDMI_SUCCESS on successful initialization
 * Returns - OTM_HDMI_ERR_FAILED on init failure
 */
otm_hdmi_ret_t otm_hdmi_hpd_init(void)
{
	int result = 0;
	result = ps_hdmi_hpd_register_driver();
	return (result) ? OTM_HDMI_SUCCESS : OTM_HDMI_ERR_FAILED;
}

/**
 * otm_hdmi_hpd_deinit - Deinit HPD driver service.
 *
 * No input arguments
 *
 * Returns - OTM_HDMI_SUCCESS
 */
otm_hdmi_ret_t otm_hdmi_hpd_deinit(void)
{
	ps_hdmi_hpd_unregister_driver();
	return OTM_HDMI_SUCCESS;
}

/**
 * otm_hdmi_get_hpd_pin - get hdmi hpd pin number.
 *
 * No input arguments
 *
 * Returns - pin number
 */
unsigned int otm_hdmi_get_hpd_pin(void)
{
	unsigned int result = 0;
	result = ps_hdmi_get_hpd_pin();
	return result;
}

/**
 * otm_hdmi_override_cable_status - override hdmi hpd cable status.
 *
 * Input: override state and auto test state
 */
void otm_hdmi_override_cable_status(bool state, bool auto_state)
{
	ps_hdmi_override_cable_status(state, auto_state);
	return;
}



/**
 * This function fills the given table with timings
 * @unit_id	: hdmi unit revision id
 * @table	: handle to table to be filled
 * @max_size	: max number of entries in the table
 * @reference	: is this table reference table?
 *
 * This routine fills given table with timings according to current unit version
 * and subsequent use of table
 */
static int __init_tx_modes(hdmi_unit_revision_id_t unit_id,
			   const otm_hdmi_timing_t **table,
			   unsigned int max_size, bool reference)
{
	int i = 0;

#define __ADD_MODE(mode)    \
	do { \
		if (i < max_size) \
			table[i++] = mode;  \
		else {		       \
		    i = -1;	     \
		    goto exit;	  \
		}	\
	} while (0);

	/* The following 2D modes are supported on all unit revisions */
	__ADD_MODE(&MODE_640x480p5994_60);
	__ADD_MODE(&MODE_720_1440x576i50);
	__ADD_MODE(&MODE_720_1440x480i5994_60);
	__ADD_MODE(&MODE_720x576p50);
	__ADD_MODE(&MODE_720x480p5994_60);
	__ADD_MODE(&MODE_1280x720p50);
	__ADD_MODE(&MODE_1280x720p5994_60);
	__ADD_MODE(&MODE_1920x1080i50);
	__ADD_MODE(&MODE_1920x1080i5994_60);
	__ADD_MODE(&MODE_1920x1080p24);
	__ADD_MODE(&MODE_1920x1080p25);
	__ADD_MODE(&MODE_1920x1080p30);
	__ADD_MODE(&MODE_1920x1080p50);
	__ADD_MODE(&MODE_1920x1080p5994_60);

	/* The following 3D modes are supported on all unit revisions */
	__ADD_MODE(&MODE_1280x720p50__SBSH2);
	__ADD_MODE(&MODE_1280x720p5994_60__SBSH2);
	__ADD_MODE(&MODE_1920x1080i50__SBSH2);
	__ADD_MODE(&MODE_1920x1080i5994_60__SBSH2);
	__ADD_MODE(&MODE_1920x1080p24__SBSH2);
	__ADD_MODE(&MODE_1920x1080p50__SBSH2);
	__ADD_MODE(&MODE_1920x1080p5994_60__SBSH2);
	__ADD_MODE(&MODE_1280x720p50__TBH2);
	__ADD_MODE(&MODE_1280x720p5994_60__TBH2);
	__ADD_MODE(&MODE_1920x1080p24__TBH2);
	__ADD_MODE(&MODE_1920x1080p30__TBH2);
	__ADD_MODE(&MODE_1920x1080p50__TBH2);
	__ADD_MODE(&MODE_1920x1080p5994_60__TBH2);
	__ADD_MODE(&MODE_1280x720p50__FP2);
	__ADD_MODE(&MODE_1920x1080p30__FP2);

	/* The following modes are only included if the table is used as a
	 * reference set for EDID parsing
	 */
	if (reference) {
		__ADD_MODE(&MODE_720_1440x576i50__16by9);
		__ADD_MODE(&MODE_720_1440x480i5994_60__16by9);
		__ADD_MODE(&MODE_720x576p50__16by9);
		__ADD_MODE(&MODE_720x480p5994_60__16by9);
	}
	/* The following mode are supported only on CE4200 B0 and further */
	if (unit_id >= HDMI_PCI_REV_CE4200_B0) {
		__ADD_MODE(&MODE_1280x720p50__FP);
		__ADD_MODE(&MODE_1280x720p5994_60__FP);
		__ADD_MODE(&MODE_1920x1080i50__FP);
		__ADD_MODE(&MODE_1920x1080i5994_60__FP);
		__ADD_MODE(&MODE_1920x1080p24__FP);
		__ADD_MODE(&MODE_1920x1080p30__FP);
	}
#undef __ADD_MODE

exit:
	return i;
}

static void log_entry(void *uhandle, char *foo)
{
#ifdef __HDMI_HAL_TRACE__
	PD_PRINT("%s: Entering %s\n", PD_NAME, foo);
#endif
}

static void log_exit(void *uhandle, char *foo, int rc)
{
#ifdef __HDMI_HAL_TRACE__
	PD_PRINT("%s: Exiting %s with %d\n", PD_NAME, foo, rc);
#endif
}

/* Microseconds domain timer initialization */
static void __poll_start(void *poll_timer)
{
	do_gettimeofday(poll_timer);
}

/* Microseconds domain timeout verification */
static bool __poll_timeout(void *poll_timer)
{
	struct timeval tv_stop;
	do_gettimeofday(&tv_stop);
	return TIME_DIFF(tv_stop, *((struct timeval *) poll_timer)) >
				I2C_SW_TIMEOUT;
}

/**
 * This function initializes hdmi_context
 * @context	: opaque hdmi_context
 * @pdev		: pci_device
 *
 * Returns check otm_hdmi_ret_t
 * Initializes hdmi_context
 */
static otm_hdmi_ret_t __hdmi_context_init(void *context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	/* Setup resources for HDMI VDC */
	rc = ps_hdmi_pci_dev_init(ctx, pdev);
	if (rc != OTM_HDMI_SUCCESS)
		goto exit;

	pr_debug("About to call initialize HAL members and io_address is %p\n",
		 ctx->io_address);

	/* Initialize HAL; It's important that ALL entries are initialized!!! */
	ctx->dev.log_entry = log_entry;
	ctx->dev.log_exit = log_exit;
	ctx->dev.poll_timer = &ctx->hal_timer;
	ctx->dev.poll_start = __poll_start;
	ctx->dev.poll_timeout = __poll_timeout;
	ctx->dev.io_address = ctx->io_address;

	ctx->dev.uhandle = ctx->io_address;

	/* Create modes table sharing protection semaphore */
	mutex_init(&ctx->modes_sema);

	/* Create execution protection semaphore */
	mutex_init(&ctx->exec_sema);

	/* Create HPD protection semaphore */
	mutex_init(&ctx->hpd_sema);

	/* Create server thread synchronization semaphore */
	mutex_init(&ctx->srv_sema);

	/* Create AV mute synchronization semaphore */
	mutex_init(&ctx->mute_sema);

exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * otm_hdmi_deinit - deinit called during shutdown
 * @context	:opaque hdmi_context
 *
 * Returns nothing. de-initializes and frees pointers
 * Called during power down
 */
void otm_hdmi_deinit(void *context)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = context;

	/* Destroy semaphores */
	mutex_destroy(&ctx->modes_sema);
	mutex_destroy(&ctx->exec_sema);
	mutex_destroy(&ctx->hpd_sema);
	mutex_destroy(&ctx->srv_sema);
	mutex_destroy(&ctx->mute_sema);

	/* Unmap IO region, Disable the PCI devices
	 */
	ps_hdmi_pci_dev_deinit(ctx);

	/* Free context */
	kfree(ctx);

	pr_debug("Exiting deinit with error code %d\n", rc);
exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return;
}

/* turn HDMI power rails on */
bool otm_hdmi_power_rails_on(void)
{
	return ps_hdmi_power_rails_on();
}

/* turn HDMI power rails off */
bool otm_hdmi_power_rails_off(void)
{
	return ps_hdmi_power_rails_off();
}

/* turn HDMI power islands on */
bool otm_hdmi_power_islands_on(void)
{
	hdmi_context_t *ctx = g_context;

	if (ctx && ctx->islands_powered_on == false) {
		ctx->islands_powered_on = true;
		return ps_hdmi_power_islands_on();
	}
	return true;
}

/* turn HDMI power islands off */
void otm_hdmi_power_islands_off(void)
{
	hdmi_context_t *ctx = g_context;

	if (ctx && ctx->islands_powered_on == true) {
		ctx->islands_powered_on = false;
		ps_hdmi_power_islands_off();
	}
}

/* enable/disable IRQ and CPD_HPD */
bool otm_hdmi_enable_hpd(bool enable)
{
	return ps_hdmi_enable_hpd(enable);
}

/* control HDMI vblank interrupt */
void otm_hdmi_vblank_control(struct drm_device *dev, bool on)
{
	ps_hdmi_vblank_control(dev, on);
}

/*
 * otm_hdmi_get_cable_status - Get HDMI cable connection status
 * @context: hdmi device context
 *
 * Returns - boolean state.
 * true - HDMI cable connected
 * false - HDMI cable disconnected
 */
bool otm_hdmi_get_cable_status(void *context)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	if (ctx == NULL)
		return false;

	return ps_hdmi_get_cable_status(ctx);
}

/**
 * get pixel clock range
 * @pc_min	: min pixel clock
 * @pc_max	: max pixel clock
 *
 * Returns check otm_hdmi_ret_t
 * This functions returns the minimum and maximum
 * pixel clock values
 */
otm_hdmi_ret_t otm_hdmi_get_pixel_clock_range(unsigned int *pc_min,
						unsigned int *pc_max)
{
	return ipil_get_pixel_clock_range(pc_min, pc_max);
}

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
					      void *data)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;

	pr_debug("Entering %s, context = %p\n", __func__, context);
	/* Verify pointers */
	if (context == NULL || phdmi_irq_cb == NULL ||
		data == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}
	ctx = (hdmi_context_t *)context;

	pr_debug("Registering HPD callback\n");
	ctx->hpd_callback = phdmi_irq_cb;
	ctx->hpd_data = data;
exit:
	return rc;

}

/**
 * otm_hdmi_device_init	-	init hdmi device driver
 * @context: hdmi device context
 * @pdev: pci device
 *
 * Perform HDMI device initialization which includes 3 steps:
 * 1) otm context create,
 * 2) os specific context init,
 * 3) device enable
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_device_init(void **context, struct pci_dev *pdev)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = NULL;
	int n;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/* Verify pointers */
	if (context == NULL) {
		rc = OTM_HDMI_ERR_INTERNAL;
		goto exit;
	}

	/* Create and clear context */
	g_context = ctx =
	    (hdmi_context_t *) kmalloc(sizeof(hdmi_context_t), GFP_KERNEL);
	if (ctx == NULL) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	memset(ctx, 0, sizeof(hdmi_context_t));

	pr_debug("HDMI Context created = %p\n", ctx);

	/* Init HDMI context */
	rc = __hdmi_context_init(ctx, pdev);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nHDMI Context init failed\n");
		goto exit;
	}

	rc = otm_hdmi_declare_attributes(__pd_attr_declare, __pd_attr_get_name);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nHDMI declare attributes table failed\n");
		goto exit;
	}

	ipil_hdmi_set_hdmi_dev(&ctx->dev);

	/* Save the output mode as DTV or HDMT tx */
	ctx->dtv = g_dtv;

	/* Save the deep color enable flag */
	ctx->dc = g_dc;

	/* Save active GPIO number */
	ctx->gpio = g_gpio;

	/* Save context */
	*context = ctx;

	/* Fill in static timing table */
	n = __init_tx_modes(ctx->dev.id, g_video_modes, MAX_TIMINGS,
				false);
	if (n < 0) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	ctx->n_modes_tx = n;

	/* Fill EDID parser reference timing table */
	n = __init_tx_modes(ctx->dev.id, g_video_modes_ref, MAX_TIMINGS,
			    true);
	if (n < 0) {
		rc = OTM_HDMI_ERR_NO_MEMORY;
		goto exit;
	}
	ctx->n_modes_ref = n;

	/* Fill in advertised timings table */
	otm_hdmi_edid_parse(ctx, OTM_HDMI_USE_EDID_NONE);

exit:
	/* Clean up if appropriate */
	if ((rc != OTM_HDMI_SUCCESS) && (ctx != NULL))
		otm_hdmi_deinit((void *)ctx);

	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * Returns if the given values is preferred mode or not
 * @hdisplay	: width
 * @vdisplay	: height
 * @refresh	: refresh rate
 *
 * Returns true if preferred mode else false
 */
bool otm_hdmi_is_preferred_mode(int hdisplay, int vdisplay, int refresh)
{
	return ipil_hdmi_is_preferred_mode(hdisplay, vdisplay, refresh);
}

/**
 * Set raw edid to the hdmi context
 * @context	: opaque hdmi_context
 * @raw_edid	: raw edid information
 *
 * Returns - check otm_hdmi_ret_t
 * Copy raw edid to the hdmi context
 */
otm_hdmi_ret_t otm_hdmi_set_raw_edid(void *context, char *raw_edid)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL)
		return OTM_HDMI_ERR_FAILED;

	/* TODO: need more flexiable way which should be edid size-aware copy */
	memcpy(ctx->edid_raw, raw_edid, MAX_EDID_BLOCKS * SEGMENT_SIZE);

	return OTM_HDMI_SUCCESS;
}

/**
 * Get raw edid to the hdmi context
 * @context	: opaque hdmi_context
 * @raw_edid	: raw edid information
 *
 * Returns - check otm_hdmi_ret_t
 * Retrieves raw edid in the hdmi context
 */
otm_hdmi_ret_t otm_hdmi_get_raw_edid(void *context, char **raw_edid)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL || raw_edid == NULL)
		return OTM_HDMI_ERR_FAILED;

	*raw_edid = (char *)ctx->edid_raw;

	return OTM_HDMI_SUCCESS;
}

/**
 * Set scaling type to the hdmi context
 * @context	: opaque hdmi_context
 * @scaling     : scaling type to be set
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_set_scaling_type(void *context, int scaling)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL)
		return OTM_HDMI_ERR_FAILED;

	ctx->scaling_type = scaling;

	return OTM_HDMI_SUCCESS;
}

/**
 * Check if monitor connected is hdmi
 * @context	: opaque hdmi_context
 *
 * Returns true if hdmi else false
 * Check if monitor connected is hdmi
 */
bool otm_hdmi_is_monitor_hdmi(void *context)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	if (ctx == NULL)
		return true; /* default to HDMI */

	return ctx->edid_int.hdmi;
}

/**
 * validates the attribute to be read or not.
 * @id	: attribute id to be validated
 *
 * Read's the attributes flag value.
 *
 * Returns -
 *	OTM_HDMI_SUCCESS - if the attribute is readable.
 *	OTM_HDMI_ERR_INTERNAL -	if the attribute is non-readable.
 *	OTM_HDMI_ERR_FAILED - if the attribute is not in range.
 */
static otm_hdmi_ret_t otm_hdmi_attr_get_validate(otm_hdmi_attribute_id_t id)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	if (id < 0 || id >= OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES) {
		LOG_ERROR("Invalid argument passed (id): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}
	/*
	 * Based on attribute type perform appropriate check
	*/
	if (OTM_HDMI_ATTR_FLAG_WRITE & ATTRS[id].flags) {
		return rc;
	} else if (OTM_HDMI_ATTR_FLAG_SUPPORTED & ATTRS[id].flags) {
		/*
		 * Needs a Fix.
		 */
		return rc;
	} else if (OTM_HDMI_ATTR_FLAG_INTERNAL & ATTRS[id].flags) {
		rc = OTM_HDMI_ERR_INTERNAL;
		return rc;
	} else {
		LOG_ERROR("Invalid attribute accessed: (%d)\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		return rc;
	}
exit:
	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}

/**
 * Getting given attribute
 * @context		:opaque hdmi context
 * @id			: attribute id
 * @attribute		:user provided buffer for attribute details
 * @log			: a hint wether port driver should log the call
 *
 * Returns otm_hdmi_ret_t check. Getting given attribute values
 */
otm_hdmi_ret_t otm_hdmi_get_attribute(void *context,
					otm_hdmi_attribute_id_t id,
					otm_hdmi_attribute_t *attribute,
					bool log)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	LOG_ENTRY((log) ? LOG_LEVEL_HIGH : LOG_LEVEL_VBLANK);

	rc = otm_hdmi_attr_get_validate(id);
	if (OTM_HDMI_SUCCESS != rc)
		goto exit;
	if (NULL == attribute || NULL == context) {
		LOG_ERROR("Invalid argument passed (attribute): %d\n", id);
		rc = OTM_HDMI_ERR_FAILED;
		goto exit;
	}

	*attribute = ATTRS[id];
exit:
	LOG_EXIT((log) ? LOG_LEVEL_HIGH : LOG_LEVEL_VBLANK, rc);
	return rc;
}
EXPORT_SYMBOL(otm_hdmi_get_attribute);

/**
 * Attribute name getting routine
 * @id		: attribute id
 *
 * Returns attribute name corresponding to id
 */
char *__pd_attr_get_name(otm_hdmi_attribute_id_t id)
{
	otm_hdmi_attribute_t *table = otm_hdmi_attributes_table;

	if ((0 <= id) && (id < OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES))
		return table[id].name;
	else
		return NULL;
}
EXPORT_SYMBOL(__pd_attr_get_name);

/**
 * Generic attribute declaration routine
 * @table	: attribute table to be updated
 * @id		: id to be updated to the table
 * @type		: attribute type
 * @flags	: attribute flags
 * @name		: attribute name
 * @value	: attribute default value
 * @min		: min value possible for the attribute
 * @max		: max value possible for the attribute
 *
 * Returns check otm_hdmi_ret_t
 */
static otm_hdmi_ret_t __pd_attr_declare(otm_hdmi_attribute_t *table,
				otm_hdmi_attribute_id_t id,
				otm_hdmi_attribute_type_t type,
				otm_hdmi_attribute_flag_t flags,
				char *name,
				void *value,
				unsigned int min,
				unsigned int max)
{
	size_t strsiz = 0;

	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	if ((id < 0) || (id > OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES))
		return  OTM_HDMI_ERR_FAILED;

	/* Buffer sizes in 'otm_hdmi_attribute_t table' struct are
	 * [OTM_HDMI_MAX_STRING_LENGTH + 1].
	 */
	if (name)
		strsiz = strlen(name);
	if ((name != NULL) && (strsiz <= OTM_HDMI_MAX_STRING_LENGTH))
		strncpy(table[id].name, name, OTM_HDMI_MAX_STRING_LENGTH);
	else
		LOG_ERROR("set default name\n");
		/* TODO: set default name */
	table[id].name[OTM_HDMI_MAX_STRING_LENGTH] = 0;
	table[id].flags = flags;

	switch (type) {
	case OTM_HDMI_ATTR_TYPE_UINT:
		table[id].content._uint.value         = (unsigned int) value;
		table[id].content._uint.value_default = (unsigned int) value;
		table[id].content._uint.value_min     = min;
		table[id].content._uint.value_max     = max;
		break;
	case OTM_HDMI_ATTR_TYPE_BOOLEAN:
		table[id].content._bool.value         = (bool) value;
		table[id].content._bool.value_default = (bool) value;
		break;
	case OTM_HDMI_ATTR_TYPE_STRING:
		if (value)
			strsiz = strlen(value);
		if ((value != NULL) && strsiz <= OTM_HDMI_MAX_STRING_LENGTH)
			strncpy(table[id].content.string.value,
				(char *) value, OTM_HDMI_MAX_STRING_LENGTH);
		else
			rc = OTM_HDMI_ERR_FAILED;
		table[id].content.string.value[OTM_HDMI_MAX_STRING_LENGTH] = 0;
		break;
	default:
		break;
	}
	return rc;
}

/**
 * otm_hdmi_declare_attributes - init hdmi global attributes table
 * @declare	: declare attribute
 * @get_name	: name of the attribute
 *
 * Returns - check otm_hdmi_ret_t
 */
otm_hdmi_ret_t otm_hdmi_declare_attributes(pd_attr_declare_t declare,
						pd_attr_get_name_t get_name)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;

	otm_hdmi_attribute_t *table = otm_hdmi_attributes_table;

	LOG_ENTRY(LOG_LEVEL_HIGH);

	/*
	 * declare(table, OTM_HDMI_ATTR_ID_NAME,
	 * OTM_HDMI_ATTR_TYPE_STRING, PD_ATTR_FLAGS_RS,
	 * get_name(OTM_HDMI_ATTR_ID_NAME),
	 * (void *) PD_NAME, 0, 0);
	 */

	PD_DECLARE_ATTRIBUTE_STRING(declare, table,
		OTM_HDMI_ATTR_ID_NAME,
		PD_ATTR_FLAGS_RS, get_name, PD_NAME);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_CABLE_STATUS,
		PD_ATTR_FLAGS_RS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_POWER,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_MUTE,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_STATUS,
		PD_ATTR_FLAGS_RS, get_name,
		OTM_HDMI_HDCP_STATUS_OFF,
		OTM_HDMI_HDCP_STATUS_OFF,
		OTM_HDMI_HDCP_STATUS_ON);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_ENCRYPT,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_COLOR_SPACE_INPUT,
		PD_ATTR_FLAGS_RWSI,
		get_name,
		OTM_HDMI_COLOR_SPACE_RGB,
		0, OTM_HDMI_COLOR_SPACE_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PIXEL_FORMAT_OUTPUT,
		PD_ATTR_FLAGS_RWS, get_name,
		OTM_HDMI_OPF_RGB444, 0, OTM_HDMI_OPF_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PIXEL_DEPTH,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_OPD_24BIT,
		0, OTM_HDMI_PIXEL_DEPTH_COUNT - 1);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_BG_COLOR,
		PD_ATTR_FLAGS_RWS,
		get_name, 0xFF0000, 0x000000, 0xFFFFFF);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_USE_EDID,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_USE_EDID_REAL,
		OTM_HDMI_USE_EDID_NONE,
		OTM_HDMI_USE_EDID_SAFE);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_DEBUG,
		PD_ATTR_FLAGS_RWS,
		get_name,
		LOG_LEVEL_ERROR,
		__LOG_LEVEL_MIN, __LOG_LEVEL_MAX);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_VERSION_MAJOR,
		PD_ATTR_FLAGS_RS, get_name, 0, 0, 100);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_VERSION_MINOR,
		PD_ATTR_FLAGS_RS, get_name, 4, 0, 9);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_DISPLAY_PIPE,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_DISPLAY_ID_0,
		OTM_HDMI_DISPLAY_ID_0,
		OTM_HDMI_DISPLAY_ID_UNDEFINED);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_PAR,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_PAR_NO_DATA,
		OTM_HDMI_PAR_NO_DATA,
		OTM_HDMI_PAR_16_9);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_FAR,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_FAR_SAME_AS_PAR,
		OTM_HDMI_FAR_16_9_TOP,
		OTM_HDMI_FAR_16_9_SP_4_3);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_SLOW_DDC,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_AUDIO_CLOCK,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_AUDIO_CLOCK_36,
		OTM_HDMI_AUDIO_CLOCK_24,
		OTM_HDMI_AUDIO_CLOCK_16);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_AUDIO_STATUS,
		PD_ATTR_FLAGS_RS, get_name, false);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_TMDS_DELAY,
		PD_ATTR_FLAGS_RWS, get_name, 400, 100, 500);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_COLOR_SPACE_EXT,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_OUTPUT_CLAMP,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_OUTPUT_DITHER,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_1P1,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_MUTE,
		PD_ATTR_FLAGS_RWS,
		get_name,
		OTM_HDMI_MUTE_OFF,
		OTM_HDMI_MUTE_OFF, OTM_HDMI_MUTE_BOTH);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_PURE_VIDEO,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_PHY,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_AUTO_RETRY,
		PD_ATTR_FLAGS_RWS, get_name, true);

	PD_DECLARE_ATTRIBUTE_BOOL(declare, table,
		OTM_HDMI_ATTR_ID_DVI,
		PD_ATTR_FLAGS_RWS, get_name, false);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_RI_RETRY,
		PD_ATTR_FLAGS_RWS, get_name, 40, 0, 50);

	PD_DECLARE_ATTRIBUTE_UINT(declare, table,
		OTM_HDMI_ATTR_ID_HDCP_DELAY,
		PD_ATTR_FLAGS_RWS, get_name, 500, 0, 1000);

	LOG_EXIT(LOG_LEVEL_HIGH, rc);
	return rc;
}
EXPORT_SYMBOL(otm_hdmi_declare_attributes);

 /**
 * Description: static function that programs display and pfit registers.
 *
 * @ctx:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width		:allocated frame buffer dimensions
 * @fb_height		:allocated frame buffer dimensions
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
static otm_hdmi_ret_t __hdmi_crtc_set_scaling(hdmi_context_t *ctx,
					otm_hdmi_timing_t *mode,
					otm_hdmi_timing_t *adjusted_mode,
					int fb_width, int fb_height)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int scalingtype = ctx->scaling_type;

	/* program display related registers: dpssize and pipesrc, pfit */
	rc = ipil_hdmi_crtc_mode_set_program_dspregs(&ctx->dev, scalingtype,
						mode, adjusted_mode,
						fb_width, fb_height);
	if (rc != OTM_HDMI_SUCCESS)
		pr_debug("\nfailed to set display registers\n");

	return rc;
}

/**
 * Description: Interface to program display and pfit registers.
 *
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 * @fb_width		:allocated frame buffer dimensions
 * @fb_height		:allocated frame buffer dimensions
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_crtc_set_scaling(void *context,
			otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode,
			int fb_width, int fb_height)
{
	hdmi_context_t *ctx = (hdmi_context_t *)context;

	pr_debug("Enter %s\n", __func__);

	/* NULL checks */
	if (context == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_INVAL;
	}

	return __hdmi_crtc_set_scaling(ctx, mode, adjusted_mode,
						fb_width, fb_height);
}

/**
 * Description: crtc mode set function for hdmi.
 *
 * @context		:hdmi_context
 * @mode		:mode requested
 * @adjusted_mode	:adjusted mode
 * @fb_width		:allocated frame buffer dimensions
 * @fb_height		:allocated frame buffer dimensions
 * @pclk_khz:		tmds clk value for the best pll and is needed for audio.
 *			This field has to be moved into OTM audio
 *			interfaces when implemented
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 */
otm_hdmi_ret_t otm_hdmi_crtc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode, int fb_width,
			int fb_height, uint32_t *pclk_khz)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	int scalingtype = ctx->scaling_type;

	pr_debug("Enter %s\n", __func__);

	/* NULL checks */
	if (context == NULL || mode == NULL || adjusted_mode == NULL
						|| pclk_khz == NULL) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_INVAL;
	}

	/* prepare for crtc mode set. This includes any hdmi unit reset etc. */
	rc = ipil_hdmi_crtc_mode_set_prepare(&ctx->dev);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed in preparing for mode set\n");
		return rc;
	}

	/* program display related registers: dpssize and pipesrc, pfit
	 * according to the preferred scaling mode.
	 */
	rc = __hdmi_crtc_set_scaling(ctx, mode, adjusted_mode,
					fb_width, fb_height);

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_timings(&ctx->dev,
						scalingtype, mode,
						adjusted_mode);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to set timing registers\n");
		return rc;
	}

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_dpll(&ctx->dev,
						adjusted_mode->dclk);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed to program dpll\n");
		return rc;
	}
	*pclk_khz = ctx->dev.clock_khz;

	/* program hdmi mode timing registers */
	rc = ipil_hdmi_crtc_mode_set_program_pipeconf(&ctx->dev);
	if (rc != OTM_HDMI_SUCCESS)
		pr_debug("\nfailed to program pipeconf\n");

	pr_debug("Exit%s\n", __func__);

	return rc;
}

/**
 * encoder mode set function for hdmi
 * @context:		hdmi_context
 * @mode:		mode requested
 * @adjusted_mode:	adjusted mode
 *
 * Returns:	OTM_HDMI_SUCCESS on success
 *		OTM_HDMI_ERR_INVAL on NULL input arguments
 * encoder mode set function for hdmi. enables phy.
 * set correct polarity for the current mode.
 */
otm_hdmi_ret_t otm_hdmi_enc_mode_set(void *context, otm_hdmi_timing_t *mode,
			otm_hdmi_timing_t *adjusted_mode)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	hdmi_context_t *ctx = (hdmi_context_t *)context;
	bool is_monitor_hdmi;

	/* NULL checks */
	if (context == NULL || mode == NULL || adjusted_mode == NULL) {
		pr_debug("\ninvalid arguments\n");
		return OTM_HDMI_ERR_INVAL;
	}

	is_monitor_hdmi = otm_hdmi_is_monitor_hdmi(ctx);
	pr_debug("\nMonitor Mode: %s\n", is_monitor_hdmi ? "HDMI" : "DVI");

	/* handle encoder mode set */
	rc = ipil_hdmi_enc_mode_set(&ctx->dev, mode, adjusted_mode,
					is_monitor_hdmi);
	if (rc != OTM_HDMI_SUCCESS) {
		pr_debug("\nfailed in programing enc mode set\n");
		return rc;
	}

	/* Enable AVI infoframes for HDMI mode */
	if (is_monitor_hdmi) {
		rc = otm_hdmi_infoframes_set_avi(context, mode);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to program avi infoframe\n");
	} else {/* Disable all inofoframes for DVI mode */
		rc = otm_hdmi_disable_all_infoframes(context);
		if (rc != OTM_HDMI_SUCCESS)
			pr_debug("\nfailed to disable all infoframes\n");
	}

	return rc;
}

/**
 * Restore HDMI registers and enable the display
 * @context	:hdmi_context
 * @connected	:hdmi connected or not
 *
 * Returns:	none
 * Restore HDMI registers and enable the display
 */
void otm_hdmi_restore_and_enable_display(void *context, bool connected)
{
	pr_debug("Entered %s\n", __func__);
	if (NULL != context) {
		if (connected) {
			ipil_hdmi_restore_and_enable_display(
					&((hdmi_context_t *)context)->dev);
			/* restore data island packets */
			if (otm_hdmi_is_monitor_hdmi(context)) {
				ipil_hdmi_restore_data_island(
					&((hdmi_context_t *)context)->dev);
			}
		} else {
			ipil_hdmi_destroy_saved_data(
					&((hdmi_context_t *)context)->dev);
		}
#ifdef OTM_HDMI_HDCP_ENABLE
		/* inform HDCP about resume */
		if (otm_hdmi_hdcp_set_power_save(context, false)
						== false)
			pr_debug("failed to resume hdcp\n");
#endif
	}
}

/**
 * save HDMI display registers
 * @context	:hdmi_context
 * @connected	:hdmi connected or not
 *
 * Returns:	none
 * save HDMI display registers
 */
void otm_hdmi_save_display_registers(void *context, bool connected)
{
	pr_debug("Entered %s\n", __func__);
	if (NULL != context) {
		if (connected) {
			ipil_hdmi_save_display_registers(
					&((hdmi_context_t *)context)->dev);
			/* save data island packets */
			if (otm_hdmi_is_monitor_hdmi(context)) {
				ipil_hdmi_save_data_island(
					&((hdmi_context_t *)context)->dev);
			}
		} else {
			ipil_hdmi_destroy_saved_data(
					&((hdmi_context_t *)context)->dev);
		}
	}
}

/**
 * notify security component of hdcp and hdmi cable status
 *
 * @hdcp	HDCP status: true if phase1 is enabled
 * @cable	HDMI connection status: true if connected
 *
 * Returns:	none
 */
void otm_hdmi_update_security_hdmi_hdcp_status(bool hdcp, bool cable)
{
	ps_hdmi_update_security_hdmi_hdcp_status(hdcp, cable);
}

/**
 * disable HDMI display
 * @context:	hdmi_context
 *
 * Returns:	none
 * disable HDMI display
 */
void otm_disable_hdmi(void *context)
{
	pr_debug("Entered %s\n", __func__);
	if (NULL != context) {
#ifdef OTM_HDMI_HDCP_ENABLE
		/* inform HDCP about suspend */
		if (otm_hdmi_hdcp_set_power_save(context, true)
						== false)
			pr_debug("failed to suspend hdcp\n");
#endif
		/* disable HDMI */
		ipil_disable_hdmi(&((hdmi_context_t *)context)->dev);
	}
}

/*
 *
 * Internal scripts wrapper functions.
 *
 */

/* Starting this off, but all scripts/unit test helpers should move
 * to another file.
 */

#ifdef OTM_HDMI_UNIT_TEST

/**
 * test_otm_hdmi_report_edid() - Report current EDID information
 *
 * This routine simply dumps the EDID information
 *
 * Returns - nothing
 */
void test_otm_hdmi_report_edid(void)
{
	edid_info_t *edid = NULL;
	if (NULL == g_context) {
		LOG_PRINT(LOG_LEVEL_HIGH,
			     "Cant print EDID, Initialize otm_hdmi first!\n");
		return;
	}
	edid = &g_context->edid_int;
	if (NULL == edid) {
		LOG_PRINT(LOG_LEVEL_HIGH,
			     "EDID not initialized in driver.\n");
		return;
	}
	__hdmi_report_edid(g_context, edid);
}
EXPORT_SYMBOL_GPL(test_otm_hdmi_report_edid);
#endif

#ifdef OTM_HDMI_UNIT_TEST

/**
 * get_hdmi_context() - Getting hdmi_context
 *
 * This routine gives a handle to hdmi_context
 * to be used with other function calls like
 * set_attribute which requires hdmi_context
 * as one of the params
 *
 * Returns - hdmi_context
 */
void *otm_hdmi_get_context(void)
{
	return (void *)g_context;
}
EXPORT_SYMBOL_GPL(otm_hdmi_get_context);
#endif
