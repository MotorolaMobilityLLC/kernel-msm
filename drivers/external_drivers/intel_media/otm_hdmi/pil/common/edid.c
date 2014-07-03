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

#include <linux/string.h>
#include <linux/types.h>

#include "otm_hdmi_types.h"
#include "edid_internal.h"
#include "hdmi_timings.h"

/*
 * Macro for error checking
 */
#define VERIFY(exp, rc, error, label) \
	if (!(exp)) {                 \
		rc = error;           \
		goto label;           \
	}

#define VERIFY_QUICK(exp, label) \
	if (!(exp)) {            \
		goto label;      \
	}

/*
 * Various constants
 */
#define EDID_SIGNATURE 0x00FFFFFFFFFFFF00ull

/*
 * Structure to keep state of read operation
 */
typedef struct {
	unsigned char *buffer;
	int position;
} read_context_t;

static void fetch_timing_descriptor(timing_descriptor_t *td,
				read_context_t *rctx)
				__attribute__((unused));

static void declare_mandatory_3d(edid_info_t *edid)
				__attribute__((unused));

static void declare_short_3d(unsigned int struc_3d,
				unsigned int mask,
				edid_info_t *edid)
				__attribute__((unused));

static void declare_explicit_3d(unsigned char *e,
				unsigned int n,
				edid_info_t *edid)
				__attribute__((unused));

static void decode_misc_modes(unsigned char *e,
				int n, edid_info_t *edid)
				__attribute__((unused));

/*
 * fetch_next_field()
 */
static void fetch_next_field(void *dst,
			read_context_t *read_context,
			unsigned int size)
{
	unsigned char *b = read_context->buffer + read_context->position;

	switch (size) {
#ifdef __ORDER_MSB__
	case 1:
		*(unsigned char *)dst = *b;
		break;

	case 2:
		*(unsigned short *)dst |= (unsigned short)*(b + 1) << 0;
		*(unsigned short *)dst |= (unsigned short)*(b + 0) << 8;
		break;

	case 4:
		*(unsigned int *)dst |= (unsigned int)*(b + 3) << 0;
		*(unsigned int *)dst |= (unsigned int)*(b + 2) << 8;
		*(unsigned int *)dst |= (unsigned int)*(b + 1) << 16;
		*(unsigned int *)dst |= (unsigned int)*(b + 0) << 24;
		break;

	case 8:
		*(unsigned long long *)dst |= (unsigned long long)*(b + 7) << 0;
		*(unsigned long long *)dst |= (unsigned long long)*(b + 6) << 8;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 5) << 16;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 4) << 24;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 3) << 32;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 2) << 40;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 1) << 48;
		*(unsigned long long *)dst |=
		    (unsigned long long)*(b + 0) << 56;
		break;
#endif

	default:
		/*
		 * This is only for byte sequences with LSB order, or where
		 * where order does not matter
		 */
		memcpy(dst, b, size);
		break;
	}

	read_context->position += size;
}

/*
 * fetch_generic_descriptor()
 */
static void fetch_generic_descriptor(
				generic_descriptor_t *gd,
				read_context_t *rctx)
{
	fetch_next_field(&gd->flag_required, rctx, 2);
	fetch_next_field(&gd->flag_reserved, rctx, 1);
	fetch_next_field(&gd->data_type_tag, rctx, 1);
	fetch_next_field(&gd->flag, rctx, 1);
	fetch_next_field(&gd->payload, rctx, 13);
}

/*
 * fetch_timing_descriptor()
 */
static void fetch_timing_descriptor(
				timing_descriptor_t *td,
				read_context_t *rctx)
{
	fetch_next_field(&td->pixel_clock, rctx, 2);
	fetch_next_field(&td->h_active, rctx, 1);
	fetch_next_field(&td->h_blanking, rctx, 1);
	fetch_next_field(&td->h_active_blanking_hb, rctx, 1);
	fetch_next_field(&td->v_active, rctx, 1);
	fetch_next_field(&td->v_blanking, rctx, 1);
	fetch_next_field(&td->v_active_blanking_hb, rctx, 1);
	fetch_next_field(&td->h_sync_offset, rctx, 1);
	fetch_next_field(&td->h_sync_pulse_width, rctx, 1);
	fetch_next_field(&td->vs_offset_pulse_width, rctx, 1);
	fetch_next_field(&td->offset_pulse_width_hb, rctx, 1);
	fetch_next_field(&td->h_image_size, rctx, 1);
	fetch_next_field(&td->v_image_size, rctx, 1);
	fetch_next_field(&td->hv_image_size, rctx, 1);
	fetch_next_field(&td->h_border, rctx, 1);
	fetch_next_field(&td->v_border, rctx, 1);
	fetch_next_field(&td->flags, rctx, 1);
}

/*
 * fetch_block_zero()
 * - ebz : structure representing edid block zero to be filled in
 * - data: buffer of 128 bytes containing raw edid data supplied by TV
 */
static otm_hdmi_ret_t fetch_block_zero(edid_block_zero_t *ebz,
				unsigned char *data)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	read_context_t read_context = {data, 0};
	read_context_t *rctx = &read_context;
	int i;

	VERIFY(ebz != NULL, rc, OTM_HDMI_ERR_NULL_ARG, exit);
	VERIFY(data != NULL, rc, OTM_HDMI_ERR_NULL_ARG, exit);

	/* EDID signature */
	fetch_next_field(&ebz->signature, rctx, 8);

	/* Manufacturer name id */
	fetch_next_field(&ebz->manufacturer_id, rctx, 2);

	/* Product code */
	fetch_next_field(&ebz->product_code, rctx, 2);

	/* Serial number */
	fetch_next_field(&ebz->serial_number, rctx, 4);

	/* Manufacture week */
	fetch_next_field(&ebz->manufacture_week, rctx, 1);

	/* Manufacture year */
	fetch_next_field(&ebz->manufacture_year, rctx, 1);

	/* EDID version */
	fetch_next_field(&ebz->version, rctx, 1);

	/* EDID revision */
	fetch_next_field(&ebz->revision, rctx, 1);

	/* Video input definition */
	fetch_next_field(&ebz->video_input_definition, rctx, 1);

	/* Max horizontal image size */
	fetch_next_field(&ebz->max_horz_image_size, rctx, 1);

	/* Max vertical image size*/
	fetch_next_field(&ebz->max_vert_image_size, rctx, 1);

	/* Gamma */
	fetch_next_field(&ebz->gamma, rctx, 1);

	/* Feature support */
	fetch_next_field(&ebz->feature_support, rctx, 1);

	/* Color characteristics */
	fetch_next_field(&ebz->rg_lowbits, rctx, 1);
	fetch_next_field(&ebz->bw_lowbits, rctx, 1);
	fetch_next_field(&ebz->red_x, rctx, 1);
	fetch_next_field(&ebz->red_y, rctx, 1);
	fetch_next_field(&ebz->green_x, rctx, 1);
	fetch_next_field(&ebz->green_y, rctx, 1);
	fetch_next_field(&ebz->blue_x, rctx, 1);
	fetch_next_field(&ebz->blue_y, rctx, 1);
	fetch_next_field(&ebz->white_x, rctx, 1);
	fetch_next_field(&ebz->white_x, rctx, 1);

	/* Established timings */
	fetch_next_field(&ebz->est_timing_1, rctx, 1);
	fetch_next_field(&ebz->est_timing_2, rctx, 1);
	fetch_next_field(&ebz->est_timing_3, rctx, 1);

	/* Standard timings */
	for (i = 0; i < EDID_STD_TIMINGS_NUM; i++)
		fetch_next_field(&ebz->std_timings[i], rctx, 2);

	/* Detailed timing descriptors 1 and 2 */
	fetch_generic_descriptor((generic_descriptor_t *) &ebz->td_1, rctx);
	fetch_generic_descriptor((generic_descriptor_t *) &ebz->td_2, rctx);

	/* Monitor Descriptors 1 and 2 */
	fetch_generic_descriptor((generic_descriptor_t *) &ebz->md_1, rctx);
	fetch_generic_descriptor((generic_descriptor_t *) &ebz->md_2, rctx);

	/* Number of 128 byte blocks to follow */
	fetch_next_field(&ebz->num_extentions, rctx, 1);

exit:
	return rc;
}

/*
 * fetch_extension_block_cea()
 */
static otm_hdmi_ret_t fetch_extension_block_cea(extention_block_cea_t *eb,
				    unsigned char *data)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	read_context_t read_context = {data, 0};
	read_context_t *rctx = &read_context;

	VERIFY(eb != NULL, rc, OTM_HDMI_ERR_NULL_ARG, exit);
	VERIFY(data != NULL, rc, OTM_HDMI_ERR_NULL_ARG, exit);

	fetch_next_field(&eb->tag, rctx, 1);
	fetch_next_field(&eb->revision, rctx, 1);
	fetch_next_field(&eb->content_offset, rctx, 1);
	fetch_next_field(&eb->flags, rctx, 1);
	fetch_next_field(&eb->data, rctx, 124);

exit:
	return rc;
}

/*
 * encode_refresh()
 * Convert an integer refresh rate to the equivalent enumeration
 */
static otm_hdmi_refresh_t encode_refresh(unsigned refresh)
{
	/* Both 1/1001 and 1/1000 refresh rates are included in case
	 * EDID contains 1/1001 pixel clock values in the entry
	 */
	switch (refresh) {
	case 23:
		return OTM_HDMI_REFRESH_24;
	case 24:
		return OTM_HDMI_REFRESH_24;
	case 25:
		return OTM_HDMI_REFRESH_25;
	case 29:
		return OTM_HDMI_REFRESH_30;
	case 30:
		return OTM_HDMI_REFRESH_30;
	case 50:
		return OTM_HDMI_REFRESH_50;
	case 59:
		return OTM_HDMI_REFRESH_60;
	case 60:
		return OTM_HDMI_REFRESH_60;
	}
	return OTM_HDMI_REFRESH_USER_DEFINED;
}

/*
 * Returns index of timing with given VIC in given table; -1 otherwise
 */
static int find_timing_by_vic_tp(const otm_hdmi_timing_t **set, int size,
						unsigned int vic)
{
	int i, rc = -1;

	VERIFY(set, rc, -1, exit);

	for (i = 0; i < size; i++) {
		if (set[i]->metadata == vic) {
			rc = i;
			break;
		}
	}

exit:
	return rc;
}

/*
 * Timings comparison
 */
static bool __timing_equal(const otm_hdmi_timing_t *t1,
				    const otm_hdmi_timing_t *t2)
{
	unsigned int t1_flags = t1->mode_info_flags &
			(PD_SCAN_INTERLACE | PD_AR_16_BY_9);
	unsigned int t2_flags = t2->mode_info_flags &
			(PD_SCAN_INTERLACE | PD_AR_16_BY_9);

	return ((t1->width == t2->width) &&
		(t1->height == t2->height) &&
		(t1->refresh == t2->refresh) &&
		(t1_flags == t2_flags) && (t1->stereo_type == t2->stereo_type));
}

/*
 * Returns index of given timings in given table of timings; -1 otherwise
 */
int find_timing(const otm_hdmi_timing_t *set, int size,
					const otm_hdmi_timing_t *e)
{
	int i, rc = -1;
	VERIFY(set && e, rc, -1, exit);
	for (i = 0; i < size && !__timing_equal(&set[i], e); i++);

	rc = (i < size) ? i : -1;
exit:
	return rc;
}

/*
 * Returns index of given timings in given table of timing pointers;
 * -1 otherwise
 */
static int find_timing_tp(const otm_hdmi_timing_t **set, int size,
					const otm_hdmi_timing_t *e)
{
	int i, rc = -1;
	VERIFY(set && e, rc, -1, exit);
	for (i = 0; i < size && !__timing_equal(set[i], e); i++);

	rc = (i < size) ? i : -1;
exit:
	return rc;
}

/*
 * add_timings()
 */
static otm_hdmi_ret_t add_timings(edid_info_t *edid,
				const otm_hdmi_timing_t *pdt,
				unsigned int order)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int i, j;

	/* Safety checks */
	VERIFY((edid != NULL) && (pdt != NULL), rc,
				OTM_HDMI_ERR_NULL_ARG, exit);

	/* Is there room for more timings at all? */
	VERIFY(edid->num_timings < MAX_TIMINGS, rc,
				OTM_HDMI_ERR_INVAL, exit);

	/* Print info about it */
	print_pd_timing(pdt, order);

	/* Do not add modes that we dont support */
	i = find_timing_tp(edid->ref_timings, edid->num_ref_timings, pdt);
	VERIFY(i > -1, rc, OTM_HDMI_ERR_INVAL, exit);

	/* Do not add duplicates; Update discovery order though for cases when
	 * a mode was decoded from DTD first
	 */
	if ((j = find_timing(edid->timings, edid->num_timings, pdt)) != -1) {
		edid->order[j] = !edid->order[j] ? order : edid->order[j];
		goto exit;
	}
	/* Save discovery order	*/
	edid->order[edid->num_timings] = order;

	/* Add timing */
	edid->timings[edid->num_timings++] = *edid->ref_timings[i];

	/* Update supported family of refresh rates */
	edid->supports_60Hz = edid->supports_60Hz
	    || pdt->refresh == OTM_HDMI_REFRESH_60;
	edid->supports_50Hz = edid->supports_50Hz
	    || pdt->refresh == OTM_HDMI_REFRESH_50;

exit:
	return rc;
}

/*
 * checksum_valid()
 */
static bool checksum_valid(unsigned char *buffer, int size)
{
	unsigned char sum_computed = 0;

	while (size-- > 0)
		sum_computed += *(buffer++);

	return (sum_computed == 0) ? true : false;
}

/*
 * decode_speaker_allocation_data_block()
 */
static void decode_speaker_allocation_data_block(unsigned char *e, int n,
					edid_info_t *edid)
{
	int ne = n / 3;

	LOG_PRINT(LOG_LEVEL_DETAIL, "[speaker block]\n");

	while (ne-- > 0) {
		edid->speaker_map =
		    (unsigned)*e | (unsigned)(*(e + 1) & 0x7) << 8;
		print_speaker_layout(edid->speaker_map);
		e++;
		e++;		 /* skip the rest of the block */
	}
}

/*
 * decode_video_data_block()
 */
static void decode_video_data_block(unsigned char *e, int n, edid_info_t *edid)
{
	int vic, j, i = 0;

	LOG_PRINT(LOG_LEVEL_DETAIL, "[video data block]\n");

	while (n-- > 0) {
		vic = *e & 0x7F;
		LOG_PRINT(LOG_LEVEL_DETAIL,
			 "- mode #%d %s\n", vic, (*e & 0x80) ? "native" : "");

		if ((j =
		     find_timing_by_vic_tp(edid->ref_timings,
					   edid->num_ref_timings, vic)) != -1) {
			add_timings(edid, edid->ref_timings[j], ++i);

			 /* Handle native mode */
			if (*e & 0x80) {
				edid->native_idx =
				    find_timing(edid->timings,
						edid->num_timings,
						edid->ref_timings[j]);
			}
		}
		e++;
	}
}

/*
 * decode_audio_data_block()
 */
static void decode_audio_data_block(unsigned char *e, int n, edid_info_t *edid)
{
	int ne = n / 3;
	otm_hdmi_audio_cap_t *adb = (otm_hdmi_audio_cap_t *) &edid->audio_caps;

	LOG_PRINT(LOG_LEVEL_DETAIL, "[audio data block... %d entries]\n", ne);

	edid->short_audio_descriptor_count = ne;
	if (ne > 0)
		memcpy(edid->short_audio_descriptor_data, e, n);

	while (ne-- > 0) {
		/* Do we have room for another capability? */
		if (edid->num_caps < MAX_CAPS) {
			adb[edid->num_caps].format = (*e & 0x78) >> 3;
			adb[edid->num_caps].max_channels = (*e & 0x07) + 1;
			adb[edid->num_caps].fs = *(e + 1) & 0x7F;
			adb[edid->num_caps].ss_bitrate = *(e + 2);
			print_audio_capability(&adb[edid->num_caps]);
			edid->num_caps++;
		}
		/* Go to the next entry of the block */
		e += 3;
	}
}

/*
 *
 */
static void declare_mandatory_3d(edid_info_t *edid)
{
	if (true) {
		add_timings(edid, &MODE_1920x1080p24__FP2, 0);
		add_timings(edid, &MODE_1920x1080p24__FP, 0);
		add_timings(edid, &MODE_1920x1080p24__TBH2, 0);
	}

	if (edid->supports_60Hz) {
		add_timings(edid, &MODE_1280x720p5994_60__FP2, 0);
		add_timings(edid, &MODE_1280x720p5994_60__FP, 0);
		add_timings(edid, &MODE_1920x1080i5994_60__SBSH2, 0);
		add_timings(edid, &MODE_1280x720p5994_60__TBH2, 0);
	}

	if (edid->supports_50Hz) {
		add_timings(edid, &MODE_1280x720p50__FP2, 0);
		add_timings(edid, &MODE_1280x720p50__FP, 0);
		add_timings(edid, &MODE_1920x1080i50__SBSH2, 0);
		add_timings(edid, &MODE_1280x720p50__TBH2, 0);
	}
};

/*
 * Addition of 3D timing via 2D mode
 */
static void add_3d_mode_via_2d(unsigned int vic, unsigned int struc_3d,
						edid_info_t *edid)
{
	unsigned int j, k, num_timings = edid->num_timings;
	otm_hdmi_timing_t t;

	struct {
		unsigned int struc;
		unsigned int type;
	} details_3d[] = {
		{
		0x00, OTM_HDMI_STEREO_FRAME_PACKING_2}, {
		0x00, OTM_HDMI_STEREO_FRAME_PACKING}, {
		0x06, OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2}, {
	0x08, OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2},};

	/* Look for an entry with given order among all decoded 2D timings */
	for (j = 0; j < num_timings; j++) {
		if (edid->order[j] == vic) {
			/*
			 * Create all required 3D variations for given
			 * 2D mode
			 */
			for (k = 0;
			     k < sizeof(details_3d) / sizeof(*details_3d);
			     k++) {
				if (details_3d[k].struc == struc_3d) {
					t = edid->timings[j];
					t.stereo_type = details_3d[k].type;
					add_timings(edid, &t, 0);
				}
			}

			/* Entry of given order was found so start looking for
			 * new one
			 */
			break;
		}
	}
}

/*
 * Processing of 3D modes declared via 3D_Struture_ALL and 3D_MASK
 */
static void declare_short_3d(
		unsigned int struc_3d, unsigned int mask,
		edid_info_t *edid)
{
	unsigned int i, j, modes;

	/* Go through each 2D_VIC specified via 3D_MASK */
	for (i = 0; i < 16; i++, mask = mask >> 1) {
		/*
		 * Go through stereo variations specified via
		 * 3D_Structure_ALL
		 */
		for (j = 0, modes = struc_3d; j < 16; j++, modes = modes >> 1) {
			if (modes & 0x01 && mask & 0x01)
				add_3d_mode_via_2d(i + 1, j, edid);
		}
	}
}

/*
 * Processing of 3D modes declared via explicit list of 2D vics
 */
static void declare_explicit_3d(
			unsigned char *e,
			unsigned int n,
			edid_info_t *edid)
{
	unsigned int i;

	/* Go through the list of 2D_VIC_ORDER_X entries
	 * 0x08 typed entries are 2 bytes, others are 1 byte
	 */
	for (i = 0; i < n; i += (e[i] & 0x0F) == 0x08 ? 2 : 1)
		add_3d_mode_via_2d(((e[i] & 0xF0) >> 4) + 1, e[i] & 0x0F, edid);
}

/*
 * decode_misc_modes()
 */
static void decode_misc_modes(unsigned char *e,
			int n, edid_info_t *edid)
{

}

#ifdef OTM_HDMI_3D_ENABLE
/* TODO: Should be Revisted
 * 3D parsing hangs system with edid09 of analyzer Bug206379 */
/*
 * decode_3D_modes()
 */
void decode_3D_modes(unsigned char *e, int n, int layout, edid_info_t *edid)
{
	unsigned int offset;

	LOG_PRINT(LOG_LEVEL_DETAIL, "- 3D modes supported:\n");

	/* Declare mandatory modes */
	declare_mandatory_3d(edid);

	/* There are several ways of declaring 3D modes support */
	switch (layout) {
	case 0:		 /* Mandatory modes only */
		offset = 0;
		break;

	case 1:		/* Mandatory modes + variations described in
			 * 3D_Structure_ALL */
		/* supported by each of first 16 VICs */
		offset = 2;
		declare_short_3d(e[1] | (e[0] << 8), 0xFFFF, edid);
		break;

	case 2:		 /* Mandatory modes + variations described in
			  * 3D_Structure_ALL */
		/* supported only by some of 16 first VICs
		 * [as told by 3D_MASK] */
		offset = 4;
		declare_short_3d(e[1] | (e[0] << 8), e[3] | (e[2] << 8), edid);
		break;

	default:		 /* Reserved for future use */
		offset = 0;
		break;
	}

	/* Declare 3D modes based on present 2D VIC entries */
	declare_explicit_3d(e + offset,
			(n >= offset) ? n - offset : 0, edid);
}
#endif

/*
 * decode_vendor_data_block()
 */
static void decode_vendor_data_block(unsigned char *e,
				int n, edid_info_t *edid)
{
	unsigned int pos;
#ifdef OTM_HDMI_3D_ENABLE
	unsigned int len_3d, len_hdmi;
#endif

	LOG_PRINT(LOG_LEVEL_DETAIL,
		"[vendor specific data block.. length %d]\n", n);

	/* Look for HDMI signature [0x030C00] */
	if (n >= 3) {
		if ((e[0] == 0x03) && (e[1] == 0x0C) && (e[2] == 0x00)) {
			edid->hdmi = true;
			LOG_PRINT(LOG_LEVEL_DETAIL, "- HDMI signature found\n");
		}
	}
	/* Parse Source Physical Address */
	if (n >= 5)
		edid->spa = (e[3] << 8) | e[4];
	/* Look for more optional stuff */
	if (n >= 6) {
		/* Deep Color support */
		edid->dc_48 = (e[5] & 0x40) != 0;
		edid->dc_36 = (e[5] & 0x20) != 0;
		edid->dc_30 = (e[5] & 0x10) != 0;
		edid->dc_y444 = (e[5] & 0x08) != 0;

		/* AI support */
		edid->supports_ai = (e[5] & 0x80) != 0;
	}
	/* MAX TMDS clock */
	if (n >= 7)
		edid->max_tmds_clock = e[6] * 5;
	/* Check for optional latency and 3D fields */
	if ((n >= 8)) {
		edid->latency_present = (e[7] & 0x80) != 0;
		edid->latency_int_present = (e[7] & 0x40) != 0;
		edid->hdmi_video_present = (e[7] & 0x20) != 0;
	}
	/* From now on keep explicit track of position we are reading */
	pos = 8;

	/* Get video latency [in ms] */
	if (edid->latency_present) {
		edid->latency_video = e[pos++];
		edid->latency_audio = e[pos++];

		/* Get interlaced video latency [in ms] */
		if (edid->latency_int_present) {
			edid->latency_video_interlaced = e[pos++];
			edid->latency_audio_interlaced = e[pos++];
		}
	}

#ifdef OTM_HDMI_3D_ENABLE
	/* 3D and misc modes information from HDMI 1.4 specification */
	if (edid->hdmi_video_present) {
		edid->enabled_3d = (e[pos++] & 0x80) != 0;

		len_3d = (e[pos] & 0x1F);
		len_hdmi = (e[pos++] & 0xE0) >> 5;

		/* Assumption is that both misc and 3D modes can be
		 * present so deal with
		 * misc modes first
		 */
		decode_misc_modes(e + pos, len_hdmi, edid);

		/* Now deal with 3D stuff */
		if (len_3d || edid->enabled_3d) {
			decode_3D_modes(e + pos + len_hdmi, len_3d,
					(e[pos - 2] & 0x60) >> 5, edid);
		}

	}
#endif

}

/*
 * decode_extended_data_block()
 */
static void decode_extended_data_block(unsigned char *e,
				int n, edid_info_t *edid)
{
	LOG_PRINT(LOG_LEVEL_DETAIL, "[extended data block.. length %d]\n", n);

	switch (*(e + 0)) {
	case 0x00:		 /* Video Capability Block */
		LOG_PRINT(LOG_LEVEL_DETAIL, "Video Capability Block\n");
		edid->rgb_quant_selectable = *(e + 1) & 0x40;
		edid->ycc_quant_selectable = *(e + 1) & 0x80;
		break;
	case 0x01:		 /* Vendor Specific Video Data Block */
		LOG_PRINT(LOG_LEVEL_DETAIL,
			"Vendor Specific Video Data Block\n");
		break;
	case 0x05:		 /* Colorimetry Block */
		LOG_PRINT(LOG_LEVEL_DETAIL, "Colorimetry Block\n");
		if (n == 3) {
			edid->xvycc601 = (*(e + 1) & 0x01) != 0;
			edid->xvycc709 = (*(e + 1) & 0x02) != 0;
		}
		break;
	case 0x11:		 /* CEA Misc Audio Block */
		LOG_PRINT(LOG_LEVEL_DETAIL, "CEA Misc Audio Block\n");
		break;
	case 0x12:		 /* Vendor specific audio data block */
		LOG_PRINT(LOG_LEVEL_DETAIL,
			"Vendor specific audio data Block\n");
		break;
	default:		 /* reserved blocks */
		LOG_PRINT(LOG_LEVEL_DETAIL, "Reserved Block\n");
		break;
	}

}

/*
 * This is what short descriptor handler signature should look like
 * NOTE: e is where payload starts, i.e. header byte is not included!!!
 */
typedef void (*short_block_decoder_t)(unsigned char *e, int n,
					edid_info_t *edid);

static short_block_decoder_t short_block_decoders[] = {
	/* Reserved */
	NULL,
	/* Audio data block decoder */
	decode_audio_data_block,
	/* Video data block decoder */
	decode_video_data_block,
	/* Vendor specific block decoder */
	decode_vendor_data_block,
	/* Speaker allocation block decoder */
	decode_speaker_allocation_data_block,
	/* VESA DTC data block decoder */
	NULL,
	/* Reserved */
	NULL,
	/* Extended tag handler */
	decode_extended_data_block
};

/*
 * decode_block_collection()
 * See section 7.5 of CEA-861-C for details
 */
static void decode_block_collection(extention_block_cea_t *eb,
				edid_info_t *edid)
{
	unsigned char *c = eb->data;
	int block_type, payload_size;

	/* All area before detailed descriptors should be filled
	 */
	while (c < ((unsigned char *)eb + eb->content_offset)) {
		block_type = (*c & 0xE0) >> 5;
		payload_size = *c & 0x1F;

		/* Simple block types */
		if ((block_type < 8) && (block_type >= 0)) {
			if (short_block_decoders[block_type]) {
				short_block_decoders[block_type] ((unsigned char
								   *)c + 1,
								  payload_size,
								  edid);
			} else {
				LOG_PRINT(LOG_LEVEL_DETAIL,
					 "[block 0x%x.. TBA]\n", block_type);
			}
		}
		/* Unknown */
		else
			LOG_PRINT(LOG_LEVEL_DETAIL,
					"[unknown block 0x%x]\n", (int)*c);

		LOG_PRINT(LOG_LEVEL_DETAIL, "\n");
		c += (*c & 0x1F) + 1;
	}
}

/*
 * decode_standard_timings()
 * Section 3.9.1 of EDID STD
 */
static void decode_standard_timings(unsigned short st, edid_info_t *edid)
{
	struct {
		int h;
		int v;
	} ar[] = { {
	16, 10}, {
	4, 3}, {
	5, 4}, {
	16, 9} };
	otm_hdmi_timing_t pdt;
	int r;

	if (st != 0x0101) {
		pdt.width = ((st & 0x00FF) + 31) * 8;
		pdt.refresh = encode_refresh(((st & 0x3F00) >> 8) + 60);

		r = ((st & 0xC000) >> 14);

		/* Init flags with respect to aspect ratio */
		pdt.mode_info_flags = (r == 3) ? PD_AR_16_BY_9 : 0;

		/* TODO: Add proper logic for EDID earlier than 1.3
		 * This weird line below makes sure division by zero is avoided
		 */
		pdt.height =
		    pdt.width * ar[r].v / (ar[r].h ? ar[r].h : ar[r].v);

		/* Indicate no stereo support */
		pdt.stereo_type = OTM_HDMI_STEREO_NONE;

		LOG_PRINT(LOG_LEVEL_DETAIL, "[Standart timing]\n");
		add_timings(edid, &pdt, 0);
	}
}

/*
 * decode_detailed_timings()
 * Table 3.16 of EDID STD
 */
static bool decode_detailed_timings(timing_descriptor_t *td,
					otm_hdmi_timing_t *pdt)
{
	bool rc = true;

	int pixel_clock = td->pixel_clock * 10;
	int h_active = ((td->h_active_blanking_hb & 0xF0) << 4) | td->h_active;
	int h_blanking =
	    ((td->h_active_blanking_hb & 0x0F) << 8) | td->h_blanking;
	int v_active = ((td->v_active_blanking_hb & 0xF0) << 4) | td->v_active;
	int v_blanking =
	    ((td->v_active_blanking_hb & 0x0F) << 8) | td->v_blanking;
	int h_sync_off =
	    ((td->offset_pulse_width_hb & 0xC0) << 2) | td->h_sync_offset;
	int h_sync_pw =
	    ((td->offset_pulse_width_hb & 0x30) << 4) | td->h_sync_pulse_width;

	int v_sync_off = ((td->vs_offset_pulse_width & 0xF0) >> 4)
	    | (td->offset_pulse_width_hb & 0x0C);

	int v_sync_pw = ((td->offset_pulse_width_hb & 0x03) << 4)
	    | (td->vs_offset_pulse_width & 0x0F);

	int h_img_size = ((td->hv_image_size & 0xF0) << 4) | td->h_image_size;
	int v_img_size = ((td->hv_image_size & 0x0F) << 8) | td->v_image_size;

	LOG_PRINT(LOG_LEVEL_DETAIL, "[detailed timing descriptor]\n");
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - pixel_clock     : %d KHz\n", pixel_clock);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - horz_active     : %d pixels\n", h_active);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - horz_blanking   : %d pixels\n", h_blanking);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - vert_active     : %d lines\n", v_active);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - vert_blanking   : %d lines\n", v_blanking);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - horz_sync_off   : %d pixels\n", h_sync_off);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - horz_sync_pw    : %d pixels\n", h_sync_pw);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - vert_sync_off   : %d lines\n", v_sync_off);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - vert_sync_pw    : %d lines\n", v_sync_pw);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		" - image ratio     : %d : %d\n", h_img_size, v_img_size);

	pdt->width = h_active;
	pdt->htotal = h_active + h_blanking;
	pdt->hblank_start = h_active;
	pdt->hblank_end = h_active + h_blanking;
	pdt->hsync_start = h_active + h_sync_off;
	pdt->hsync_end = h_active + h_sync_off + h_sync_pw;

	pdt->height = v_active;
	pdt->vtotal = v_active + v_blanking;
	pdt->vblank_start = v_active;
	pdt->vblank_end = v_active + v_blanking;
	pdt->vsync_start = v_active + v_sync_off;
	pdt->vsync_end = v_active + v_sync_off + v_sync_pw;

	pdt->dclk = pixel_clock;

	/* Make sure we are seeing valid mode */
	VERIFY(pdt->htotal && pdt->vtotal && pdt->dclk, rc, false, exit);

	pdt->refresh = (pdt->dclk * 1000) / (pdt->htotal * pdt->vtotal);

	/* Convert refresh velue to enumeration entry */
	pdt->refresh = encode_refresh(pdt->refresh);

	/* Check if mode is interlaced */
	pdt->mode_info_flags |= (td->flags & 0x80) ? PD_SCAN_INTERLACE : 0;

	/* Determine picture aspect ratio */
	pdt->mode_info_flags |=
	    (h_img_size / 4 == v_img_size / 3) ? 0 : PD_AR_16_BY_9;

	/* Check for sync signal polarity */
	if (td->flags & 0x18) {
		pdt->mode_info_flags |= (td->flags & 0x02) ? PD_HSYNC_HIGH : 0;
		pdt->mode_info_flags |= (td->flags & 0x04) ? PD_VSYNC_HIGH : 0;
	}
	/* Indicate no stereo support */
	pdt->stereo_type = OTM_HDMI_STEREO_NONE;

exit:
	return rc;
}

/*
 * decode_generic_descriptor()
 * Table 3.19. Table 3.20 of EDID STD
 */
static void decode_generic_descriptor(generic_descriptor_t *gd,
				edid_info_t *edid)
{
	int i;

	/* Not a timing descriptor */
	if ((gd->flag_required == 0) && (gd->flag_reserved == 0)
	    && (gd->flag == 0)) {
		switch (gd->data_type_tag) {
		case 0xFF:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Monitor Serial Number ]\n");
			LOG_PRINT(LOG_LEVEL_DETAIL, " - %s\n", gd->payload);
			break;
		case 0xFE:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[ASCII String          ]\n");
			LOG_PRINT(LOG_LEVEL_DETAIL, " - %s\n", gd->payload);
			break;
		case 0xFD:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Monitor Range Limits  ]\n");
			LOG_PRINT(LOG_LEVEL_DETAIL, " - ...\n");
			break;
		case 0xFC:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Monitor Name          ]\n");
			LOG_PRINT(LOG_LEVEL_DETAIL, " - %s\n", gd->payload);
			for (i = 0; i < 13; i++) {
				if (gd->payload[i] == '\n')
					break;
				else
					edid->product_name[i] = gd->payload[i];
			}
			break;
		case 0xFB:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Color Data            ]\n");
			break;
		case 0xFA:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[More Standard Timings ]\n");
			for (i = 0; i < 12; i += 2) {
				decode_standard_timings
				    ((gd->payload[i] << 8) | gd->payload[i + 1],
				     edid);
			}
			break;
		case 0x10:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Dummy                 ]\n");
			break;
		default:
			LOG_PRINT(LOG_LEVEL_DETAIL,
				"[Manufacturer/Undefined]\n");
			break;
		}
	}
	/* Timing descriptor */
	else {
		otm_hdmi_timing_t pdt;
		memset(&pdt, 0, sizeof(otm_hdmi_timing_t));
		if (decode_detailed_timings((timing_descriptor_t *) gd, &pdt))
			add_timings(edid, &pdt, 0);
	}
}

#define __BASIC_AUDIO_FS (OTM_HDMI_AUDIO_FS_32_KHZ | \
			OTM_HDMI_AUDIO_FS_44_1_KHZ | \
			OTM_HDMI_AUDIO_FS_48_KHZ)

/*
 * __find_and_declare_basic_audio_support()
 */
static otm_hdmi_ret_t __find_and_declare_basic_audio_support(edid_info_t *edid)
{
	otm_hdmi_audio_cap_t *caps = edid->audio_caps;
	unsigned int i = 0;

	otm_hdmi_audio_cap_t cap = {
		.format = OTM_HDMI_AUDIO_FORMAT_PCM,
		.max_channels = 2,
		.fs = __BASIC_AUDIO_FS,
		.ss_bitrate = OTM_HDMI_AUDIO_SS_16,
	};

	/* Try to find basic audio functionality among declared capabilities */
	for (i = 0; i < edid->num_caps; i++) {
		if ((caps[i].format == cap.format)
		    && (caps[i].max_channels >= cap.max_channels)
		    && (caps[i].ss_bitrate & cap.ss_bitrate)
		    && ((caps[i].fs & cap.fs) == cap.fs)) {
			goto exit;
		}
	}

	/* Add RLRF pair to the speaker allocation map */
	edid->speaker_map |= 0x01;

	/* Add basic audio capability to the list of capabilities */
	memcpy(&caps[edid->num_caps++], &cap, sizeof(cap));

exit:
	return OTM_HDMI_SUCCESS;
}

/*
 * decode_extention_block_cea()
 * Refer to section A.2.13 of CEA-861-C document for additional details
 */
static void decode_extention_block_cea(extention_block_cea_t *eb,
				edid_info_t *edid)
{
	int i;
	generic_descriptor_t gd;

	/* Check YCbCr444 and YCbCr422 support */
	if (eb->revision >= 2) {
		edid->ycbcr444 = (eb->flags & 0x20) ? true : false;
		edid->ycbcr422 = (eb->flags & 0x10) ? true : false;
	}
	/*
	 * Short descriptors section exists when:
	 * - offset is not 4
	 * - CEA extension version is 3
	 */
	if ((eb->content_offset != 4) && (eb->revision >= 3))
		decode_block_collection(eb, edid);
	/*
	 * Detailed timing descriptors:
	 * - do not exist when offset is zero
	 * - may still not exist even when offset is non-zero; in this case
	 *   location where they would exist is padded so make sure
	 *   appropriate decoding routine can handle that padding correctly
	 */
#define __DSCR_SIZE 18
	if (eb->content_offset != 0) {
		for (i = 0; i < (128 - eb->content_offset) / __DSCR_SIZE; i++) {
			/*
			 * Instead of using a casted pointer to descriptor,
			 * we explicitely copy memory content temporary
			 * placeholder. This way we are avoiding possible
			 * integer addressing problems
			 */
			memcpy(&gd,
			       (char *)eb + eb->content_offset +
			       __DSCR_SIZE * i, __DSCR_SIZE);
			decode_generic_descriptor(&gd, edid);
		}
	}
#undef __DSCR_SIZE

	/* Check for basic audio support and add it to caps list if necessary */
	if ((eb->revision >= 2) && (eb->flags & 0x40))
		__find_and_declare_basic_audio_support(edid);
}

/*
 * fetch_block_map()
 */
static otm_hdmi_ret_t fetch_block_map(edid_block_map_t *ebm,
				unsigned char *data)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	read_context_t read_context = { data, 0 };
	read_context_t *rctx = &read_context;

	VERIFY(ebm != NULL, rc, OTM_HDMI_ERR_INTERNAL, exit);
	VERIFY(data != NULL, rc, OTM_HDMI_ERR_INTERNAL, exit);

	/* Fill tag, map and checksum fields */
	fetch_next_field(&ebm->tag, rctx, 1);
	fetch_next_field(ebm->map, rctx, BLOCK_MAP_SIZE);
	fetch_next_field(&ebm->checksum, rctx, 1);

exit:
	return rc;
}

/*
 * block_decode()
 */
static otm_hdmi_ret_t block_decode(edid_info_t *edid_info, unsigned int type,
					unsigned char *buffer)
{
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	extention_block_cea_t eb;

	LOG_PRINT(LOG_LEVEL_DETAIL, "Decoding extension 0x%x\n", type);

	switch (type) {
	case 0x02:
		VERIFY(buffer[0] == 0x02, rc, OTM_HDMI_ERR_FAILED, exit);
		memset(&eb, 0, sizeof(extention_block_cea_t));
		fetch_extension_block_cea(&eb, buffer);
		decode_extention_block_cea(&eb, edid_info);
		break;

	default:
		LOG_PRINT(LOG_LEVEL_DETAIL,
			"Extension 0x%x is not supported; Bypassing\n",
			   type);
		break;
	}

exit:
	return rc;
}

int edid_parse_pd_timing_from_cea_block(edid_info_t *edid_info,
					unsigned char *buffer,
					otm_hdmi_timing_t *pdts)
{
	extention_block_cea_t eb;
	int i = 0;

	if (buffer[0x0] == 0x2) {
		memset(&eb, 0, sizeof(extention_block_cea_t));
		edid_info->num_timings = 0;
		fetch_extension_block_cea(&eb, buffer);
		decode_extention_block_cea(&eb, edid_info);

		for (i = 0; i < edid_info->num_timings; i++) {
			memcpy((unsigned char *)&pdts[i],
				     (unsigned char *)&edid_info->timings[i],
				     sizeof(otm_hdmi_timing_t));
		}
		return edid_info->num_timings;
	}
	return 0;
}


/*
 * edid_parse()
 */
otm_hdmi_ret_t edid_parse(edid_info_t *edid_info,
			i2c_read_t data_read, void *cd)
{
	unsigned char buffer[SEGMENT_SIZE];
	edid_block_zero_t ebz;
	edid_block_map_t ebm;
	extention_block_cea_t eb;
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int i, sp, offset;

	/* Read block zero */
	rc = data_read(cd, 0, 0, buffer, SEGMENT_SIZE);
	VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
	print_raw_block(buffer, SEGMENT_SIZE);
	VERIFY(checksum_valid(buffer, SEGMENT_SIZE), rc,
				OTM_HDMI_ERR_FAILED, exit);

	/* Process block zero */
	memset(&ebz, 0, sizeof(edid_block_zero_t));
	fetch_block_zero(&ebz, buffer);

	VERIFY(ebz.signature == EDID_SIGNATURE, rc, OTM_HDMI_ERR_FAILED, exit);

	/* Decode general stuff */
	edid_info->manufacturer_id = ebz.manufacturer_id;
	edid_info->product_code = ebz.product_code;
	edid_info->product_sn = ebz.serial_number;
	edid_info->product_year = ebz.manufacture_year + 1990;
	edid_info->product_week = ebz.manufacture_week;
	edid_info->max_horz_image_size = ebz.max_horz_image_size;
	edid_info->max_vert_image_size = ebz.max_vert_image_size;
	edid_info->native_idx = -1;

	/* Decode timings */
	decode_generic_descriptor((generic_descriptor_t *) &ebz.td_1,
				  edid_info);
	decode_generic_descriptor((generic_descriptor_t *) &ebz.td_2,
				  edid_info);
	decode_generic_descriptor((generic_descriptor_t *) &ebz.md_1,
				  edid_info);
	decode_generic_descriptor((generic_descriptor_t *) &ebz.md_2,
				  edid_info);

	for (i = 0; i < EDID_STD_TIMINGS_NUM; i++)
		decode_standard_timings(ebz.std_timings[i], edid_info);

	/* If there are no extentions - we are done */
	VERIFY(ebz.num_extentions != 0, rc, OTM_HDMI_SUCCESS, exit);

	/* Read next block */
	rc = data_read(cd, 0, SEGMENT_SIZE, buffer, SEGMENT_SIZE);
	VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
	print_raw_block(buffer, SEGMENT_SIZE);
	VERIFY(checksum_valid(buffer, SEGMENT_SIZE), rc,
				OTM_HDMI_ERR_FAILED, exit);

	/* There is only 1 extention; Assume its CEA Extension [tag = 0x02] */
	if (ebz.num_extentions == 1) {
		/* Process extention block */
		memset(&eb, 0, sizeof(extention_block_cea_t));
		fetch_extension_block_cea(&eb, buffer);
		VERIFY(eb.tag == 0x02, rc, OTM_HDMI_SUCCESS, exit);
		decode_extention_block_cea(&eb, edid_info);
	}
	/* There is a block map and more extentions */
	else {
		/* Process block map */
		memset(&ebm, 0, sizeof(edid_block_map_t));
		fetch_block_map(&ebm, buffer);

		/* Verify we are indeed dealing with map block */
		VERIFY(ebm.tag == 0xF0, rc, OTM_HDMI_ERR_FAILED, exit);

		/* Deal with each block in the map */
		for (i = 0; (i < BLOCK_MAP_SIZE) && ebm.map[i]; i++) {
			/* Compute extension block location */
			sp = (i + 2) / 2;
			offset = (i % 2) ? SEGMENT_SIZE : 0;

			/* Read extension block */
			rc = data_read(cd, sp, offset, buffer, SEGMENT_SIZE);
			VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
			print_raw_block(buffer, SEGMENT_SIZE);
			VERIFY(checksum_valid(buffer, SEGMENT_SIZE), rc,
			       OTM_HDMI_ERR_FAILED, exit);

			/* Decode extension block */
			rc = block_decode(edid_info, ebm.map[i], buffer);
			VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
		}

		goto exit;
	}

exit:
	return rc;
}

/*
 * Read enhanced EDID data blocks at 0xA0 using segment pointer address 0x60
 */
static otm_hdmi_ret_t __enhanced_ddc_read_one_block(struct i2c_adapter *adapter,
		unsigned char sp, unsigned char offset, unsigned char *buffer)
{
	#define SEGMENT_ADDR 0x30
	#define EDID_ADDR    0x50
	struct i2c_msg msgs[] = {
			{
				.addr = SEGMENT_ADDR,
				.flags = 0,
				.len = 1,
				.buf = &sp,

			}, {
				.addr = EDID_ADDR,
				.flags = 0,
				.len = 1,
				.buf = &offset,

			}, {
				.addr = EDID_ADDR,
				.flags = I2C_M_RD,
				.len = SEGMENT_SIZE,
				.buf = buffer,
			}
	};

	int ret_i2c, retries = 5, siz = sizeof(msgs) / sizeof(msgs[0]);

	pr_debug("enter %s\n", __func__);

	/* Safty check */
	if (!adapter || !buffer)
		return OTM_HDMI_ERR_FAILED;

	do {
		ret_i2c = i2c_transfer(adapter, msgs, siz);
		pr_debug("ret_i2c: %d\n", ret_i2c);
	} while (ret_i2c != siz && --retries);

	if (ret_i2c != siz) {
		pr_err("i2c failed1\n");
		return OTM_HDMI_ERR_FAILED;
	}

	pr_debug("exit %s\n", __func__);

	return OTM_HDMI_SUCCESS;
}

/*
 * edid_extension_parse()
 */
otm_hdmi_ret_t edid_extension_parse(struct i2c_adapter *adapter,
			edid_info_t *edid_info, unsigned char *edid)
{
	unsigned char *buffer;
	edid_block_map_t ebm;
	extention_block_cea_t eb;
	otm_hdmi_ret_t rc = OTM_HDMI_SUCCESS;
	int i, sp, offset;

	if (!edid_info || !edid)
		return OTM_HDMI_ERR_FAILED;

	pr_debug("enter %s\n", __func__);

	/* If there are no extentions - we are done */
	VERIFY(edid[0x7e] != 0, rc, OTM_HDMI_SUCCESS, exit);

	/* Point to the first extension block */
	buffer = edid + SEGMENT_SIZE;
	VERIFY(checksum_valid(buffer, SEGMENT_SIZE), rc,
			OTM_HDMI_ERR_FAILED, exit);

	/* There is only 1 extention; Assume its CEA Extension [tag = 0x02] */
	if (edid[0x7e] == 1) {
		/* Process first extention block */
		memset(&eb, 0, sizeof(extention_block_cea_t));
		fetch_extension_block_cea(&eb, buffer);
		VERIFY(eb.tag == 0x02, rc, OTM_HDMI_SUCCESS, exit);
		decode_extention_block_cea(&eb, edid_info);
	}
	/* There is a block map and more extentions */
	else {
		/* Process block map */
		memset(&ebm, 0, sizeof(edid_block_map_t));
		fetch_block_map(&ebm, buffer);

		/* Verify we are indeed dealing with map block */
		VERIFY(ebm.tag == 0xF0, rc, OTM_HDMI_ERR_FAILED, exit);

		/* Deal with each block in the map */
		for (i = 0; (i < BLOCK_MAP_SIZE) && ebm.map[i]; i++) {
			/* Compute extension block location */
			sp = (i + 2) / 2;
			offset = (i % 2) ? SEGMENT_SIZE : 0;

			/* Read extension block */
			rc = __enhanced_ddc_read_one_block(adapter, sp, offset,
					buffer);
			VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
			VERIFY(checksum_valid(buffer, SEGMENT_SIZE), rc,
			       OTM_HDMI_ERR_FAILED, exit);

			/* Decode extension block */
			rc = block_decode(edid_info, ebm.map[i], buffer);
			VERIFY_QUICK(rc == OTM_HDMI_SUCCESS, exit);
		}
	}
exit:
	pr_debug("exit %s (ret = %d)\n", __func__, rc);
	return rc;
}

