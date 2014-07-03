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

#ifndef __IPIL_INTERNAL_H
#define __IPIL_INTERNAL_H

#include "ips_hdmi.h"

/* hdmi display related registers */
#define IPIL_DSPBASE		IPS_DSPBASE
#define IPIL_DSPBSIZE		IPS_DSPBSIZE
#define IPIL_PIPEBSRC		IPS_PIPEBSRC
#define IPIL_DSPBPOS		IPS_DSPBPOS
#define IPIL_PFIT_CONTROL	IPS_PFIT_CONTROL
#define IPIL_HTOTAL_B		IPS_HTOTAL_B
#define IPIL_VTOTAL_B		IPS_VTOTAL_B
#define IPIL_HBLANK_B		IPS_HBLANK_B
#define IPIL_VBLANK_B		IPS_VBLANK_B
#define IPIL_HSYNC_B		IPS_HSYNC_B
#define IPIL_VSYNC_B		IPS_VSYNC_B
#define IPIL_DPLL_B		    IPS_DPLL_B
#define IPIL_DPLL_DIV0		IPS_DPLL_DIV0
#define IPIL_PIPEBCONF		IPS_PIPEBCONF
#define IPIL_DSPBCNTR		IPS_DSPBCNTR
#define IPIL_HDMIB_CONTROL  IPS_HDMIB_CONTROL
#define IPIL_DSPBSTRIDE     IPS_DSPBSTRIDE
#define IPIL_DSPBLINOFF     IPS_DSPBLINOFF
#define IPIL_DSPBTILEOFF    IPS_DSPBTILEOFF
#define IPIL_DSPBSURF       IPS_DSPBSURF
#define IPIL_DSPBSTAT       IPS_DSPBSTAT
#define IPIL_PALETTE_B      IPS_PALETTE_B
#define IPIL_PFIT_PGM_RATIOS     IPS_PFIT_PGM_RATIOS
#define IPIL_HDMIPHYMISCCTL      IPS_HDMIPHYMISCCTL

#define IPIL_VGACNTRL 0x71400
#define IPIL_DPLL_PWR_GATE_EN (1<<30)
#define IPIL_DPLL_VCO_ENABLE (1<<31)
#define IPIL_VGA_DISP_DISABLE (1<<31)

/* TODO: revisit this. make then IP specific */
#define IPIL_PFIT_ENABLE		(1 << 31)
#define IPIL_PFIT_PIPE_SHIFT		29
#define IPIL_PFIT_PIPE_SELECT_B		(1 << IPIL_PFIT_PIPE_SHIFT)
#define IPIL_PFIT_SCALING_AUTO		(0 << 26)
#define IPIL_PFIT_SCALING_PROGRAM	(1 << 26)
#define IPIL_PFIT_SCALING_PILLARBOX	(1 << 27)
#define IPIL_PFIT_SCALING_LETTERBOX	(3 << 26)
#define IPIL_PFIT_FRACTIONAL_VALUE	(1 << 12)
#define IPIL_PFIT_VERT_SCALE_SHIFT	16
#define IPIL_PFIT_HORIZ_SCALE_SHIFT	0
#define IPIL_PFIT_VERT_MSB_SHIFT	28
#define IPIL_PFIT_HORIZ_MSB_SHIFT	12

#define IPIL_PWR_GATE_EN	(1 << 30)
#define IPIL_PIPECONF_PLL_LOCK	(1<<29)
#define IPIL_DSP_PLANE_PIPE_POS	24
#define IPIL_DSP_PLANE_ENABLE	(1<<31)
#define IPIL_PIPEACONF_ENABLE	(1<<31)
#define IPIL_PIPEACONF_PIPE_STATE    (1<<30)
#define IPIL_PIPECONF_PLANE_OFF  (1<<19)
#define IPIL_PIPECONF_CURSOR_OFF     (1<<18)
#define IPIL_P1_MASK		(0x1FF << 17)
#define IPIL_HDMIB_PORT_EN	(1 << 31)
#define IPIL_HDMIB_PIPE_B_SELECT (1 << 30)
#define IPIL_HDMIB_NULL_PACKET	(1 << 9)
#define IPIL_HDMIB_AUDIO_ENABLE	(1 << 6)
#define IPIL_HDMIB_COLOR_RANGE_SELECT (1 << 8)
#define IPIL_HDMI_PHY_POWER_DOWN 0x7f

#define IPIL_TIMING_FLAG_PHSYNC	(1<<0)
#define IPIL_TIMING_FLAG_NHSYNC	(1<<1)
#define IPIL_TIMING_FLAG_PVSYNC	(1<<2)
#define IPIL_TIMING_FLAG_NVSYNC	(1<<3)

#define IPIL_HSYNC_POLARITY_MASK IPS_HSYNC_POLARITY_MASK
#define IPIL_VSYNC_POLARITY_MASK IPS_VSYNC_POLARITY_MASK
#define CLEAR_BITS(val, mask) ((val) & (~mask))
#define SET_BITS(val, mask)  ((val) | (mask))

#endif /* __IPIL_INTERNAL_H */
