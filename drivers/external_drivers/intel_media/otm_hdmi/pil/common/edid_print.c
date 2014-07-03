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

#include "edid_internal.h"

/* Convert the refresh rate enum to a string for printing */
static char *_refresh_string(otm_hdmi_refresh_t r)
{
	char *ret;

	switch (r) {
	case OTM_HDMI_REFRESH_23_98:
		ret = "23.98";
		break;
	case OTM_HDMI_REFRESH_24:
		ret = "24";
		break;
	case OTM_HDMI_REFRESH_25:
		ret = "25";
		break;
	case OTM_HDMI_REFRESH_29_97:
		ret = "29.97";
		break;
	case OTM_HDMI_REFRESH_30:
		ret = "30";
		break;
	case OTM_HDMI_REFRESH_50:
		ret = "50";
		break;
	case OTM_HDMI_REFRESH_47_96:
		ret = "47.96";
		break;
	case OTM_HDMI_REFRESH_48:
		ret = "48";
		break;
	case OTM_HDMI_REFRESH_59_94:
		ret = "59.94";
		break;
	case OTM_HDMI_REFRESH_60:
		ret = "60";
		break;
	case OTM_HDMI_REFRESH_NONE:
		ret = "0";
		break;
	default:
		ret = "<Unknown refresh rate>";
		break;
	};

	return ret;
}

/* Convert stereo enum to a string for printing */
static char *_stereo_string(otm_hdmi_stereo_t stereo_type)
{
	char *ret;

	switch (stereo_type) {
	case OTM_HDMI_STEREO_NONE:
		ret = "Stereo None";
		break;
	case OTM_HDMI_STEREO_FRAME_PACKING_2:
		ret = "Stereo Frame Packing 2";
		break;
	case OTM_HDMI_STEREO_FRAME_PACKING:
		ret = "Stereo Frame Packing";
		break;
	case OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2:
		ret = "Stereo Side by Side Half 2";
		break;
	case OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2:
		ret = "Stereo Top Bottom Half 2";
		break;
	case OTM_HDMI_STEREO_FRAME_SEQUENTIAL:
		ret = "Stereo Frame Sequential";
		break;
	default:
		ret = "<Unknown Stereo type>";
		break;
	};

	return ret;
}

/* print_pd_timing */
void print_pd_timing(const otm_hdmi_timing_t *t,
		     unsigned int order)
{
	LOG_PRINT(LOG_LEVEL_DETAIL,
		"%dx%d @ %s %s [%s] [%d] [%s]\n",
		t->width, t->height, _refresh_string(t->refresh),
		((t->mode_info_flags & PD_SCAN_INTERLACE) ? "i" : "p"),
		((t->mode_info_flags & PD_AR_16_BY_9) ? "16:9" : "4:3"),
		order, _stereo_string(t->stereo_type));

	LOG_PRINT(LOG_LEVEL_DETAIL, "htotal      : %d\n",
				t->htotal);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		"hblank_start: %d\n", t->hblank_start);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		"hblank_end  : %d\n", t->hblank_end);
	LOG_PRINT(LOG_LEVEL_DETAIL, "hsync_start : %d\n",
			t->hsync_start);
	LOG_PRINT(LOG_LEVEL_DETAIL, "hsync_end   : %d\n",
			t->hsync_end);
	LOG_PRINT(LOG_LEVEL_DETAIL, "vtotal      : %d\n",
			t->vtotal);
	LOG_PRINT(LOG_LEVEL_DETAIL, "vblank_start: %d\n",
		t->vblank_start);
	LOG_PRINT(LOG_LEVEL_DETAIL, "vblank_end  : %d\n",
		t->vblank_end);
	LOG_PRINT(LOG_LEVEL_DETAIL, "vsync_start : %d\n",
		t->vsync_start);
	LOG_PRINT(LOG_LEVEL_DETAIL, "vsync_end   : %d\n",
		t->vsync_end);
	LOG_PRINT(LOG_LEVEL_DETAIL,
		"clock       : %dMhz\n", (int)(t->dclk / 1000));
}

/* Convert audio format enum to a string for printing */
static char *_audio_format(otm_hdmi_audio_fmt_t fmt)
{
	char *s;

	switch (fmt) {
	case OTM_HDMI_AUDIO_FORMAT_PCM:
		s = "PCM";
		break;
	case OTM_HDMI_AUDIO_FORMAT_AC3:
		s = "AC3";
		break;
	case OTM_HDMI_AUDIO_FORMAT_MPEG1:
		s = "MPEG1";
		break;
	case OTM_HDMI_AUDIO_FORMAT_MP3:
		s = "MP3";
		break;
	case OTM_HDMI_AUDIO_FORMAT_MPEG2:
		s = "MPEG2";
		break;
	case OTM_HDMI_AUDIO_FORMAT_AAC:
		s = "AAC";
		break;
	case OTM_HDMI_AUDIO_FORMAT_DTS:
		s = "DTS";
		break;
	case OTM_HDMI_AUDIO_FORMAT_ATRAC:
		s = "ATRAC";
		break;
	case OTM_HDMI_AUDIO_FORMAT_OBA:
		s = "One Bit Audio";
		break;
	case OTM_HDMI_AUDIO_FORMAT_DDP:
		s = "Dolby Digital +";
		break;
	case OTM_HDMI_AUDIO_FORMAT_DTSHD:
		s = "DTSHD";
		break;
	case OTM_HDMI_AUDIO_FORMAT_MLP:
		s = "MLP";
		break;
	case OTM_HDMI_AUDIO_FORMAT_DST:
		s = "DST";
		break;
	case OTM_HDMI_AUDIO_FORMAT_WMA_PRO:
		s = "WMA Pro";
		break;
	default:
		s = "< unknown format >";
		break;
	};

	return s;
}

/* Convert sampling rate enum to a string for printing */
static char *_sampling_rate(otm_hdmi_audio_fs_t fs)
{
	char *s;

	switch (fs) {
	case OTM_HDMI_AUDIO_FS_32_KHZ:
		s = "32";
		break;
	case OTM_HDMI_AUDIO_FS_44_1_KHZ:
		s = "44.1";
		break;
	case OTM_HDMI_AUDIO_FS_48_KHZ:
		s = "48";
		break;
	case OTM_HDMI_AUDIO_FS_88_2_KHZ:
		s = "88.2";
		break;
	case OTM_HDMI_AUDIO_FS_96_KHZ:
		s = "96";
		break;
	case OTM_HDMI_AUDIO_FS_176_4_KHZ:
		s = "176.4";
		break;
	case OTM_HDMI_AUDIO_FS_192_KHZ:
		s = "192";
		break;

	default:
		s = "< unknown sampling rate >";
		break;
	}

	return s;
}

/* print_audio_capability() */
void print_audio_capability(otm_hdmi_audio_cap_t *cap)
{
	int i;

	LOG_PRINT(LOG_LEVEL_DETAIL,
		"Format: %s; Max channels: %d; Sampling rates, KHz:",
		  _audio_format(cap->format), cap->max_channels);

	for (i = 0; i < 7; i++)
		LOG_PRINT(LOG_LEVEL_DETAIL,
		" %s", (cap->fs & (1 << i)) ? _sampling_rate(1 << i) : "");
	LOG_PRINT(LOG_LEVEL_DETAIL, "\n");
}

/* Convert a speaker map enum to a string for printing */
static char *_speaker_map(otm_hdmi_audio_speaker_map_t map)
{
	char *s;

	switch (map) {
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FLFR:
		s = "FL/FR";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_LFE:
		s = "LFE";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FC:
		s = "FC";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_RLRR:
		s = "RL/RR";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_RC:
		s = "RC";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FLCFRC:
		s = "FLC/FRC";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_RLCRRC:
		s = "RLC/RRC";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FLWFRW:
		s = "FLW/FRW";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FLHFRH:
		s = "FLH/FRH";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_TC:
		s = "TC";
		break;
	case OTM_HDMI_AUDIO_SPEAKER_MAP_FCH:
		s = "FCH";
		break;

	default:
		s = "< unknown allocation >\n";
		break;
	}

	return s;
}

/* print_speaker_layout() */
void print_speaker_layout(unsigned int layout)
{
	int i;

	LOG_PRINT(LOG_LEVEL_DETAIL, "Speaker layout map:");
	for (i = 0; i < 11; i++)
		LOG_PRINT(LOG_LEVEL_DETAIL,
		" %s", (layout & (1 << i)) ? _speaker_map(1 << i) : "");
	LOG_PRINT(LOG_LEVEL_DETAIL, "\n");
}

/* print_raw_block() */
void print_raw_block(unsigned char *buffer, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (i != 0 && ((i % 0x10) == 0))
			LOG_PRINT(LOG_LEVEL_DETAIL, "\n");
		LOG_PRINT(LOG_LEVEL_DETAIL, "%02X\n", buffer[i]);
	}
	LOG_PRINT(LOG_LEVEL_DETAIL, "\n");
}
