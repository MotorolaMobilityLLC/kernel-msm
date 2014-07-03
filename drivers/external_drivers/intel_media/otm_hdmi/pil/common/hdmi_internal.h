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

#ifndef _OTM_HDMI_INTERNAL_H
#define _OTM_HDMI_INTERNAL_H


#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "otm_hdmi_types.h"
#include "otm_hdmi_defs.h"

#include "edid.h"
#include "hdmi_hal.h"

/**
 * Defines, shortcuts and globals
 */

extern otm_hdmi_attribute_t
	otm_hdmi_attributes_table[OTM_HDMI_MAX_SUPPORTED_ATTRIBUTES];

#define ATTRS otm_hdmi_attributes_table

/* Note: this macro should **only** be used when time difference is less than
 * 4s */
#define TIME_DIFF(tv2, tv1) ((((tv2).tv_sec - (tv1).tv_sec) * 1000000) + \
			     ((tv2).tv_usec - (tv1).tv_usec))

#define GPIO_MIN 0
#define GPIO_MAX 28

#define I2C_HW_TIMEOUT TIME_MS(200)
#define I2C_SW_TIMEOUT 500

#define CURRENT_DMA_READ_DESCRIPTOR(ai) \
(((hdmi_dma_descriptor_t *) (ai)->dscr_buf_addr_v)[(ai)->dscr_current_r])

#define NUM_ENTRIES_IN(set) (sizeof(set) / sizeof(*set))

#define ALL_SF (OTM_HDMI_AUDIO_FS_192_KHZ   | \
		OTM_HDMI_AUDIO_FS_176_4_KHZ | \
		OTM_HDMI_AUDIO_FS_96_KHZ    | \
		OTM_HDMI_AUDIO_FS_88_2_KHZ  | \
		OTM_HDMI_AUDIO_FS_48_KHZ    | \
		OTM_HDMI_AUDIO_FS_44_1_KHZ  | \
		OTM_HDMI_AUDIO_FS_32_KHZ)

#define ALL_SS (OTM_HDMI_AUDIO_SS_16 | \
		OTM_HDMI_AUDIO_SS_20 | \
		OTM_HDMI_AUDIO_SS_24)

#define DECLARE_AUDIO_CAP(_fmt, _nch, _fs, _ss) \
	{ .format = _fmt, .max_channels = _nch, .fs = _fs, .ss_bitrate = _ss }

/**
 * Interrupts grouped by use
 */
#define HDMI_INTERRUPTS (HDMI_INTERRUPT_I2C_BUS_ERROR	 | \
			 HDMI_INTERRUPT_I2C_BUFFER_FULL       | \
			 HDMI_INTERRUPT_I2C_TRANSACTION_DONE  | \
			 HDMI_INTERRUPT_HDCP_KEYS_READY       | \
			 HDMI_INTERRUPT_HDCP_RI	       | \
			 HDMI_INTERRUPT_HDCP_PI	       | \
			 HDMI_INTERRUPT_HDCP_FRAME	    | \
			 HDMI_INTERRUPT_HDCP_M0	       | \
			 HDMI_INTERRUPT_HDCP_R0	       | \
			 HDMI_INTERRUPT_AUDIO_FIFO_UNDERFLOW  | \
			 HDMI_INTERRUPT_DMA_SRC_COMPLETE      | \
			 HDMI_INTERRUPT_HOTPLUG_DETECT)	/* Enable HPD */

/**
 * Infoframe slot aliases
 */
enum {
	PACKET_SLOT_AVI = 0,
	PACKET_SLOT_AUDIO = 1,
	PACKET_SLOT_GENERIC_0 = 2,
	PACKET_SLOT_GENERIC_1 = 3,
};

/**
 * Infoframe Transmission Frequency
 */
enum {
	HDMI_DIP_SEND_ONCE = 0,
	HDMI_DIP_SEND_EVERY_VSYNC = 1,
	HDMI_DIP_SEND_ATLEAST_EVERY_OTHER_VSYNC = 2,
};

/**
 * Supported packets
 */
typedef enum {
	HDMI_PACKET_NULL = 0x00,
	HDMI_PACKET_ACP = 0x04,
	HDMI_PACKET_ISRC1 = 0x05,
	HDMI_PACKET_ISRC2 = 0x06,
	HDMI_PACKET_GAMUT = 0x0A,
	HDMI_PACKET_VS = 0x81,
	HDMI_PACKET_AVI = 0x82,
	HDMI_PACKET_SPD = 0x83,
} hdmi_packet_type_t;

/**
 * Packet management info
 */
typedef struct {
	otm_hdmi_packet_t packet;
	bool int_use;
} packet_info_t;

/**
 * HDMI AV mute source
 */
typedef enum {
	MUTE_SOURCE_HDCP = 0x01,
	MUTE_SOURCE_APP = 0x02,
} mute_source_t;

/**
 * This structure represents typical "enumerator - value" pair
 */
typedef struct {
	int e;
	int v;
} ev_t;

/**
 * Audio setup information
 */
typedef struct {
	unsigned int dscr_buf_addr;	/* DMA descriptors buffer physical
					   address */
	void *dscr_buf_addr_v;		/* DMA descriptors buffer virtual
					   address */
	unsigned int dscr_buf_size;	/* DMA descriptors buffer size */
	unsigned int dscr_current_w;	/* index of current write descriptor */
	unsigned int dscr_current_r;	/* index of current read descriptor */
	bool playback;		/* playback status */
	bool prebuffer;		/* prebuffering status */

	otm_hdmi_audio_fmt_t fmt;	/* format */
	otm_hdmi_audio_fs_t fs;		/* sampling frequency */
	unsigned int nch;		/* number of channels */
	otm_hdmi_audio_ss_t ss;		/* sample size */
	unsigned int map;		/* speaker allocation map */
	unsigned int chst[2];		/* channel status info */
	bool hbr;		/* HBR vs non-HBR transmission mode */
	otm_hdmi_audio_fs_t fs_adj;	/* audio frame rate */
} audio_info_t;

/**
 * HDMI context definition
 */
typedef struct {
	hdmi_device_t dev;	/* hdmi hal handle */
	void *io_address;	/* address of mapped io region */
	unsigned int io_length;	/* size of io region */

	edid_info_t edid_ext;	/* external EDID structure */
	edid_info_t edid_int;	/* internal EDID structure */
	char edid_raw[MAX_EDID_BLOCKS*SEGMENT_SIZE];	 /* raw EDID data */

	otm_hdmi_timing_t mode;	/* current mode */
	bool mode_set;	/* mode switch completion indicator */
	bool hdmi;	/* active transmission type */
	bool is_connected; /* indicate whether cable is connected */

	unsigned int irq_number;	/* IRQ number */
	void *hpd_callback;	/* hot plug call back */
	void *hpd_data;         /* Hotplug user data to be passed back in callback */
	struct workqueue_struct *post_resume_wq;
	struct work_struct post_resume_work;     /* Used to perform any
						  * notification after
						  * resuming from deep sleep
						  * without HDMI interrupts.
						  */

	struct mutex modes_sema;	/* protecting modes table sharing */
	struct mutex exec_sema;	/* to sync pd entries execution */
	struct mutex hpd_sema;	/* semaphore to sync hot plug sensetive data */
	struct mutex srv_sema;	/* semaphore to sync service and main threads */
	struct mutex mute_sema;	/* to sync av mute operations */

	bool phy_status;/* current HW PHY status */
	bool dtv;	/* TX vs DTV indicator */
	bool dc;	/* Deep Color enable indicator */

	struct timeval hal_timer;	/* HAL polling timer */

	packet_info_t pi_0;	/* data to be sent via 1st available slot */
	packet_info_t pi_1;	/* data to be sent via 2nd available slot */

	unsigned int gpio;	/* GPIO ID for I2C workaround */
	int n_modes_tx;		/* number of static modes */
	int n_modes_ref;	/* number of reference modes */

	audio_info_t audio_info;	/* hdmi audio unit information */
	mute_source_t mute_source;	/* current mute sources list */

	otm_hdmi_phy_info_t phy_info;	/* Current PHY electrical properties */
	int scaling_type; /* scaling type for HDMI display */
	int gpio_hpd_pin; /* GPIO pin number of HDMI hotplug detection (in) */
	int gpio_ls_en_pin; /* GPIO pin number for EDID level shifter (out) */
	bool is_connected_overridden; /* indicate overridden cable state, set by external mean*/
	bool override_cable_state; /*if true, cable state is indicated by is_connected_overriden;
							if false, cable state is indicated by is_connected*/
	bool islands_powered_on; /* HDMI power island status */

} hdmi_context_t;

/* mapping structures between pil and ipil */
typedef	otm_hdmi_timing_t ipil_timings_t;

/* HDMI attributes setup routine */
typedef otm_hdmi_ret_t(*pd_attr_declare_t)(otm_hdmi_attribute_t *table,
					otm_hdmi_attribute_id_t id,
					otm_hdmi_attribute_type_t type,
					otm_hdmi_attribute_flag_t flags,
					char *name,
					void *value,
					unsigned int min,
					unsigned int max);

otm_hdmi_ret_t otm_hdmi_declare_attributes(pd_attr_declare_t declare,
					pd_attr_get_name_t get_name);

otm_hdmi_ret_t hdmi_timing_add_twin_entries(edid_info_t *edid,
					    otm_hdmi_refresh_t src,
					    otm_hdmi_refresh_t dst);

void hdmi_timing_edid_to_vdc(otm_hdmi_timing_t *t);

#endif /* _OTM_HDMI_INTERNAL_H */
