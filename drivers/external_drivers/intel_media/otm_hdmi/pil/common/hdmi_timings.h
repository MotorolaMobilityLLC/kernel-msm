/*
 This file is provided under a dual BSD/GPLv2 license.  When using or
 redistributing this file, you may do so under either license.

 GPL LICENSE SUMMARY

 Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

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
      Santa Clara, CA  97052

 BSD LICENSE

 Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   - Neither the name of Intel Corporation nor the names of its
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


#ifndef __HDMI_TIMINGS_H__
#define __HDMI_TIMINGS_H__

#include "otm_hdmi.h"

extern const otm_hdmi_timing_t MODE_640x480p5994_60;
extern const otm_hdmi_timing_t MODE_720x480p5994_60;
extern const otm_hdmi_timing_t MODE_720x480p5994_60__16by9;
extern const otm_hdmi_timing_t MODE_1280x720p5994_60;
extern const otm_hdmi_timing_t MODE_1920x1080i5994_60;
extern const otm_hdmi_timing_t MODE_1920x1080i5994_60__FP;
extern const otm_hdmi_timing_t MODE_720_1440x480i5994_60;
extern const otm_hdmi_timing_t MODE_720_1440x480i5994_60__16by9;
extern const otm_hdmi_timing_t MODE_1920x1080p5994_60;
extern const otm_hdmi_timing_t MODE_720x576p50;
extern const otm_hdmi_timing_t MODE_720x576p50__16by9;
extern const otm_hdmi_timing_t MODE_1280x720p50;
extern const otm_hdmi_timing_t MODE_1920x1080i50;
extern const otm_hdmi_timing_t MODE_1920x1080i50__FP;
extern const otm_hdmi_timing_t MODE_720_1440x576i50;
extern const otm_hdmi_timing_t MODE_720_1440x576i50__16by9;
extern const otm_hdmi_timing_t MODE_1920x1080p50;
extern const otm_hdmi_timing_t MODE_1920x1080p24;
extern const otm_hdmi_timing_t MODE_1920x1080p25;
extern const otm_hdmi_timing_t MODE_1920x1080p30;
extern const otm_hdmi_timing_t MODE_1920x1080p30__FP2;
extern const otm_hdmi_timing_t MODE_1920x1080p30__FP;
extern const otm_hdmi_timing_t MODE_1920x1080p30__TBH2;
extern const otm_hdmi_timing_t MODE_1920x1080p48;
extern const otm_hdmi_timing_t MODE_1920x1080p24__FP2;
extern const otm_hdmi_timing_t MODE_1920x1080p24__FP;
extern const otm_hdmi_timing_t MODE_1280x720p5994_60__FP2;
extern const otm_hdmi_timing_t MODE_1280x720p5994_60__FP;
extern const otm_hdmi_timing_t MODE_1280x720p50__FP2;
extern const otm_hdmi_timing_t MODE_1280x720p50__FP;
extern const otm_hdmi_timing_t MODE_1280x720p5994_60__SBSH2;
extern const otm_hdmi_timing_t MODE_1280x720p50__SBSH2;
extern const otm_hdmi_timing_t MODE_1920x1080i5994_60__SBSH2;
extern const otm_hdmi_timing_t MODE_1920x1080i50__SBSH2;
extern const otm_hdmi_timing_t MODE_1920x1080p5994_60__SBSH2;
extern const otm_hdmi_timing_t MODE_1920x1080p50__SBSH2;
extern const otm_hdmi_timing_t MODE_1920x1080p24__SBSH2;
extern const otm_hdmi_timing_t MODE_1280x720p5994_60__TBH2;
extern const otm_hdmi_timing_t MODE_1280x720p50__TBH2;
extern const otm_hdmi_timing_t MODE_1920x1080p5994_60__TBH2;
extern const otm_hdmi_timing_t MODE_1920x1080p50__TBH2;
extern const otm_hdmi_timing_t MODE_1920x1080p24__TBH2;
extern const otm_hdmi_timing_t MODE_1280x720p60__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1280x720p50__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x540p60__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x540p50__PANEL_FS;
extern const otm_hdmi_timing_t MODE_920x1080p60__PANEL_FS;
extern const otm_hdmi_timing_t MODE_920x1080p50__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x1080p30__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x1080p25__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x1080p24__PANEL_FS;
extern const otm_hdmi_timing_t MODE_1920x1080p60__PANEL;
extern const otm_hdmi_timing_t MODE_1920x1080p50__PANEL;
extern const otm_hdmi_timing_t MODE_1920x1080p48__PANEL;
#endif /* __HDMI_TIMINGS_H_ */
