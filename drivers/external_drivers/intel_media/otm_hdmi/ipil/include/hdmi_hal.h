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

/*
 * GENERAL ASSUMPTIONS / REQUIREMENTS
 *
 * 1. HAL entries mentioned in this document are intended to be simple wrappers
 *    on top of register reads and writes. They should not keep any state
 *    and should not implement any policies. It is completely up to higher
 *    levels when and how to use these entries. The only safety checks HAL
 *    entries should do is for NULL pointers and (in very few cases) for
 *    correctness of values supplied for register writes (range and alignment
 *    checks).
 * 2. HAL should be implemented in such a way that they can be used in both
 *    both user and kernel space code.
 * 3. HAL should hide actual register layout where appropriate (infoframes, csc
 *    coefficients, etc). This objective is not met at the moment since in some
 *    cases (DMA configuration) we assume that end user knows the register
 *    layout (at least for now).
 *
 * ABBREVIATIONS
 *
 * GCP  - General Control Packet
 * AVI  - Auxiliaty Video Information
 * ACP  - Audio Content Protection
 * ISRC - International Standard Recording Code
 * SPD  - Source Product Description
 */

#ifndef __HDMI_HAL_H__
#define __HDMI_HAL_H__


#include <linux/types.h>

#include "otm_hdmi_types.h"
#include "otm_hdmi_defs.h"

/*
 * This enumeration represents HDMI unit revision
 */
typedef enum {
	HDMI_PCI_REV_CE3100 = 0,
	HDMI_PCI_REV_CE4100_A0 = 1,
	HDMI_PCI_REV_CE4100_B012 = 2,
	HDMI_PCI_REV_CE4200_A0 = 3,
	HDMI_PCI_REV_CE4200_B0 = 4,
} hdmi_unit_revision_id_t;

/*
 * HDMI register state
 */
typedef struct {
	bool valid;
	uint32_t saveDPLL;
	uint32_t saveFPA0;
	uint32_t savePIPEBCONF;
	uint32_t saveHTOTAL_B;
	uint32_t saveHBLANK_B;
	uint32_t saveHSYNC_B;
	uint32_t saveVTOTAL_B;
	uint32_t saveVBLANK_B;
	uint32_t saveVSYNC_B;
	uint32_t savePIPEBSRC;
	uint32_t saveDSPBSTRIDE;
	uint32_t saveDSPBLINOFF;
	uint32_t saveDSPBTILEOFF;
	uint32_t saveDSPBSIZE;
	uint32_t saveDSPBPOS;
	uint32_t saveDSPBSURF;
	uint32_t saveDSPBCNTR;
	uint32_t saveDSPBSTATUS;
	uint32_t save_palette_b[256];
	uint32_t savePFIT_CONTROL;
	uint32_t savePFIT_PGM_RATIOS;
	uint32_t saveHDMIPHYMISCCTL;
	uint32_t saveHDMIB_CONTROL;
	uint32_t saveHDMIB_DATALANES;
} hdmi_register_state_t;

/*
 * HDMI infoframe information
 */
struct hdmi_infoframe_info_t {
	bool valid;
	uint32_t freq;
	otm_hdmi_packet_t pkt;
};

/*
 * This structure is used by HAL user to configure and use HAL
 */
typedef struct {
	/* Base address of mapped registers */
	void __iomem *io_address;

	/* Base address of mapped interrupt registers */
	void __iomem *irq_io_address;

	/* Pointer to register read routine */
	unsigned int (*io_read) (void *uhandle,	/* User provided data */
				unsigned int base, /* Base address */
				unsigned int offset);	/* Register offset */

	/* Pointer to register write routine */
	void (*io_write) (void *uhandle, /* User provided data */
			unsigned int base, /* Base address */
			unsigned int offset, /* Register offset */
			unsigned int value); /* Value */


	/* Pointer to the data that will be
	 * passed to both io_read and io_write */
	void *uhandle;

	/* Pointer to the routine invoked at the beginning of every
	 * HAL call */
	void (*log_entry) (void *uhandle, /* User provided data */
			   char *foo);	/* Name of the routine */

	/* Pointer to the routine invoked at the end of every
	 * HAL call */
	void (*log_exit) (void *uhandle, /* User provided data */
			  char *foo, /* Name of the routine */
			  int rc); /* Return code */

	/* HDMI unit identifier */
	hdmi_unit_revision_id_t id;

	/* Pointer to opaque polling timer */
	void *poll_timer;

	/* Pointer to the polling timer initialization routine */
	void (*poll_start) (void *poll_timer);

	/* Pointer to the timeout verification routine */
	 bool(*poll_timeout) (void *poll_timer);

	/* Interrupt status to interrupt handling function */
	unsigned int isr_status;

	/*
	 * TODO: tmds clk value for the best pll found and is needed for audio.
	 * This field has to be moved into OTM audio interfaces
	 * when implemented.
	 */
	uint32_t clock_khz;

	/* HDMI register value */
	hdmi_register_state_t reg_state;
	/* AVI Infoframe - used for suspend resume */
	struct hdmi_infoframe_info_t avi;
} hdmi_device_t;

#endif /* __HDMI_HAL_H__*/
