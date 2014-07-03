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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include "hdcp_rx_defs.h"
#include "otm_hdmi.h"
#include "otm_hdmi_types.h"
#include "ipil_hdcp_api.h"
#include "psb_powermgmt.h"
#include "ipil_hdmi.h"

#define OTM_HDCP_DEBUG_MODULE

#ifdef OTM_HDCP_DEBUG_MODULE
bool module_disable_hdcp = false;
EXPORT_SYMBOL_GPL(module_disable_hdcp);
bool module_force_ri_mismatch = false;
EXPORT_SYMBOL_GPL(module_force_ri_mismatch);
#endif

enum {
	HDCP_ENABLE = 1,
	HDCP_RESET,
	HDCP_RI_CHECK,
	HDCP_REPEATER_CHECK,
	HDCP_REPEATER_WDT_EXPIRED,
	HDCP_SET_POWER_SAVE_STATUS,
	HDCP_SET_HPD_STATUS,
	HDCP_SET_DPMS_STATUS
} hdcp_task_msg_en;

struct hdcp_wq_struct_t {
	struct delayed_work dwork;
	int msg;
	void *msg_data;
};

/* = = = = = = = = = = = = = = = = == = = = = = = = = = = = = = = = = = = = = */
/*!  \brief Our local context.
 */
struct hdcp_context_t {
	int		can_authenticate;
			/*!< indicates HDCP Authentication currently allowed
			 */
	bool	is_required;
	bool	is_phase1_enabled;
	bool	is_phase2_enabled;
	bool	is_phase3_valid;
	bool	previous_phase1_status;
	bool	suspend;
	bool	hpd;
	bool	previous_hpd;
	bool	display_power_on;
	bool	auto_retry;
	bool	wdt_expired;
	bool	sink_query /* used to unconditionally read sink bksv */;
	bool	force_reset;
	unsigned int	auth_id;
			/*!< id that indicates current
			 * authentication attempt
			 */
	unsigned int	ri_check_interval;
			/*!< phase 3 ri-check interval based on mode */
	unsigned int	ri_check_interval_upper;
			/*!< upper bound of ri-check interval */
	unsigned int	ri_check_interval_lower;
			/*!< lower bound of ri-check interval */
	unsigned int	video_refresh_interval;
			/*!< time interval (msec) of video refresh. */
	unsigned int	prev_ri_frm_cnt_status;
			/*!< Ri frame count in HDCP status register when
			 * doing previous Ri check. */
	unsigned int	current_srm_ver;
			/*!< currently used SRM version (if vrl is not null)
			 */
	uint64_t	*vrl;
			/*!< pointer to our VRL, formatted as array of KSVs
			 * as hdcp__u64_t's
			 */
	unsigned int	vrl_count;
			/*!< total number of KSVs in our VRL */
	int (*ddc_read_write)(bool, uint8_t, uint8_t, uint8_t *, int);
			/*!< Pointer to callback function for DDC Read Write */
	struct workqueue_struct *hdcp_wq;
			/*!< single-thread workqueue handling HDCP events */
	unsigned int ri_retry;
			/*!< time delay (msec) to re-try Ri check */
	unsigned int hdcp_delay;
			/*!< time delay (msec) to wait for TMDS to be ready */
	bool hdmi; /* HDMI or DVI*/
};

/* Global instance of local context */
static struct hdcp_context_t *hdcp_context;

/* HDCP Main Event Handler */
static void hdcp_task_event_handler(struct work_struct *work);

/**
 * Description: this function sends a message to the hdcp workqueue to be
 *		processed with a delay
 *
 * @msg		message type
 * @msg_data	any additional data accompanying the message
 * @delay	amount of delay for before the message gets processed
 *
 * Returns:	true if message was successfully queued else false
 */
static bool wq_send_message_delayed(int msg,
					void *msg_data,
					unsigned long delay)
{
	struct hdcp_wq_struct_t *hwq = NULL;
	hwq = kmalloc(sizeof(struct hdcp_wq_struct_t), GFP_KERNEL);
	if (hwq == NULL) {
		if (msg_data)
			kfree(msg_data);
		return false;
	}

	hwq->msg = msg;
	hwq->msg_data = msg_data;

	INIT_DELAYED_WORK(&hwq->dwork, hdcp_task_event_handler);

	if (queue_delayed_work(hdcp_context->hdcp_wq, &hwq->dwork,
		(unsigned long)(msecs_to_jiffies(delay))) != 0)
		return true;
	else
		pr_debug("hdcp: failed to add message to delayed wq\n");

	if (msg_data)
		kfree(msg_data);

	return false;
}

/**
 * Description: this function sends a message to the hdcp workqueue to be
 *		processed without delay
 *
 * @msg		message type
 * @msg_data	any additional data accompanying the message
 *
 * Returns:	true if message was successfully queued else false
 */
static bool wq_send_message(int msg, void *msg_data)
{
	return wq_send_message_delayed(msg, msg_data, 0);
}

/**
 * Description: this function verifies all conditions to enable hdcp
 *
 * Returns:	true if hdcp can be enabled else false
 */
static bool hdcp_enable_condition_ready(void)
{
	if (hdcp_context != NULL &&
	    hdcp_context->can_authenticate == true &&
	    hdcp_context->is_required == true &&
	    hdcp_context->suspend == false &&
	    hdcp_context->hpd == true &&
	    hdcp_context->display_power_on == true)
		return true;

	if (hdcp_context == NULL) {
		pr_err("hdcp: hdcp_context is NULL\n");
	} else {
		pr_err("hdcp: condition not ready, required %d, hpd %d\n",
			hdcp_context->is_required, hdcp_context->hpd);
	}

	return false;
}

/**
 * Description: this function reads data from downstream i2c hdcp device
 *
 * @offset	offset address on hdcp device
 * @buffer	buffer to store data
 * @size	size of buffer to be read
 *
 * Returns:	true on success else false
 */
static int hdcp_ddc_read(uint8_t offset, uint8_t *buffer, int size)
{
	if (hdcp_enable_condition_ready() == true ||
		(hdcp_context->sink_query == true && 
		 offset == HDCP_RX_BKSV_ADDR))
		return hdcp_context->ddc_read_write(true,
			HDCP_PRIMARY_I2C_ADDR, offset, buffer, size);
	return false;
}

/**
 * Description: this function writes data to downstream i2c hdcp device
 *
 * @offset	offset address on hdcp device
 * @buffer	data to be written
 * @size	size of data to be written
 *
 * Returns:	true on success else false
 */
static int hdcp_ddc_write(uint8_t offset, uint8_t *buffer, int size)
{
	if (hdcp_enable_condition_ready() == true)
		return hdcp_context->ddc_read_write(false,
			HDCP_PRIMARY_I2C_ADDR, offset, buffer, size);
	return false;
}

/**
 * Description: this function converts ksv byte buffer into 64 bit number
 *
 * @ksv		ksv value
 * @size	size of the ksv
 *
 * Returns:	64bit value of the ksv
 */
static uint64_t hdcp_ksv_64val_conv(uint8_t *ksv, uint32_t size)
{
	int i = 0;
	uint64_t ksv64 = 0;
	if (ksv != NULL && size == HDCP_KSV_SIZE) {
		for (i = 0; i < 5; i++)
			ksv64 |= (ksv[i] << (i*8));
	}
	return ksv64;
}

/**
 * Description: this function validates a ksv value
 *		1. 20 1's & 20 o's
 *		2. SRM check: check for revoked keys
 *
 * @ksv		ksv value
 * @size	size of the ksv
 *
 * Returns:	true if valid else false
 */
static bool hdcp_validate_ksv(uint8_t *ksv, uint32_t size)
{
	int i = 0, count = 0;
	uint8_t temp = 0;
	uint64_t ksv64 = hdcp_ksv_64val_conv(ksv, size);
	bool ret = false;
	if (ksv != NULL  && size == HDCP_KSV_SIZE) {
		count = 0;
		for (i = 0; i < 5; i++) {
			temp = ksv[i];
			while (temp) {
				temp &= (temp-1);
				count++;
			}
		}
		if (count == HDCP_KSV_HAMMING_WT)
			ret = true;
	}
	if (ret) {
		/* SRM Check ? */
		if (hdcp_context->vrl != NULL) {
			const uint64_t *vrl = hdcp_context->vrl;
			for (i = 0; i < hdcp_context->vrl_count; i++, vrl++) {
				if (ksv64 == *vrl)
					return true;
			}
		}
	}
	return ret;
}

/**
 * Description: this function reads aksv from local hdcp tx device
 *
 * @aksv	buffer to store aksv
 * @size	size of the aksv buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_get_aksv(uint8_t *aksv, uint32_t size)
{
	bool ret = false;
	if (ipil_hdcp_get_aksv(aksv, HDCP_KSV_SIZE) == true) {
		if (hdcp_validate_ksv(aksv, size) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads bksv from downstream device
 *
 * @bksv	buffer to store bksv
 * @size	size of the bksv buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bksv(uint8_t *bksv, uint32_t size)
{
	bool ret = false;
	if (bksv != NULL  && size == HDCP_KSV_SIZE) {
		if (hdcp_ddc_read(HDCP_RX_BKSV_ADDR,
				bksv, HDCP_KSV_SIZE) == true) {
			if (hdcp_validate_ksv(bksv, size) == true)
				ret = true;
		}
	}
	return ret;
}

/**
 * Description: this function reads bcaps from downstream device
 *
 * @bcaps	buffer to store bcaps
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bcaps(uint8_t *bcaps)
{
	bool ret = false;
	if (bcaps != NULL) {
		if (hdcp_ddc_read(HDCP_RX_BCAPS_ADDR,
				bcaps, HDCP_RX_BCAPS_SIZE) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads bstatus from downstream device
 *
 * @bstatus	buffer to store bstatus
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_bstatus(uint16_t *bstatus)
{
	bool ret = false;
	if (bstatus != NULL) {
		if (hdcp_ddc_read(HDCP_RX_BSTATUS_ADDR,
			(uint8_t *)bstatus, HDCP_RX_BSTATUS_SIZE) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads ri from downstream device
 *
 * @rx_ri	buffer to store ri
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_ri(uint16_t *rx_ri)
{
	bool ret = false;
	if (rx_ri != NULL) {
		if (hdcp_ddc_read(HDCP_RX_RI_ADDR,
				(uint8_t *)rx_ri, HDCP_RI_SIZE) == true)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function reads r0 from downstream device
 *
 * @rx_r0	buffer to store r0
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_r0(uint16_t *rx_r0)
{
	return hdcp_read_rx_ri(rx_r0);
}

/**
 * Description: this function reads all ksv's from downstream repeater
 *
 * @ksv_list	buffer to store ksv list
 * @size	size of the ksv_list to read into the buffer
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_ksv_list(uint8_t *ksv_list, uint32_t size)
{
	bool ret = false;
	if (ksv_list != NULL && size) {
		if (hdcp_ddc_read(HDCP_RX_KSV_FIFO_ADDR,
		    ksv_list, size) == true) {
			ret = true;
		}
	}
	return ret;
}

/**
 * Description: this function reads sha1 value from downstream device
 *
 * @v		buffer to store the sha1 value
 *
 * Returns:	true on success else false
 */
static bool hdcp_read_rx_v(uint8_t *v)
{
	bool ret = false;
	uint8_t *buf = v;
	uint8_t offset = HDCP_RX_V_H0_ADDR;

	if (v != NULL) {
		for (; offset <= HDCP_RX_V_H4_ADDR; offset += 4) {
			if (hdcp_ddc_read(offset, buf, 4) == false) {
				pr_debug("hdcp: read rx v failure\n");
				break;
			}
			buf += 4;
		}
		if (offset > HDCP_RX_V_H4_ADDR)
			ret = true;
	}
	return ret;
}

/**
 * Description: this function performs ri match
 *
 * Returns:	true on match else false
 */
static bool hdcp_stage3_ri_check(void)
{
	uint16_t rx_ri = 0;

	if (hdcp_enable_condition_ready() == false ||
	    hdcp_context->is_phase1_enabled == false)
		return false;

#ifdef OTM_HDCP_DEBUG_MODULE
	if (module_force_ri_mismatch) {
		pr_debug("hdcp: force Ri mismatch\n");
		module_force_ri_mismatch = false;
		return false;
	}
#endif

	if (hdcp_read_rx_ri(&rx_ri) == true) {
		if (ipil_hdcp_does_ri_match(rx_ri) == true)
			/* pr_debug("hdcp: Ri Matches %04x\n", rx_ri);*/
			return true;

		/* If first Ri check fails,we re-check it after ri_retry (msec).
		 * This is because some receivers do not immediately have valid
		 * Ri' at frame 128.
		 * */
		pr_debug("re-check Ri after %d (msec)\n",
				hdcp_context->ri_retry);

		msleep(hdcp_context->ri_retry);
		if (hdcp_read_rx_ri(&rx_ri) == true)
			if (ipil_hdcp_does_ri_match(rx_ri) == true)
				return true;
	}

	/* ri check failed update phase3 status */
	hdcp_context->is_phase3_valid = false;

	pr_debug("hdcp: error!!!  Ri check failed %x\n", rx_ri);
	return false;
}

/**
 * Description: this function sends an aksv to downstream device
 *
 * @an		AN value to send
 * @an_size	size of an
 * @aksv	AKSV value to send
 * @aksv_size	size of aksv
 *
 * Returns:	true on success else false
 */
static bool hdcp_send_an_aksv(uint8_t *an, uint8_t an_size,
			uint8_t *aksv, uint8_t aksv_size)
{
	bool ret = false;
	if (an != NULL && an_size == HDCP_AN_SIZE &&
	   aksv != NULL  && aksv_size == HDCP_KSV_SIZE) {
		if (hdcp_ddc_write(HDCP_RX_AN_ADDR, an, HDCP_AN_SIZE) ==
			true) {
			/* wait 20ms for i2c write for An to complete */
			/* msleep(20); */
			if (hdcp_ddc_write(HDCP_RX_AKSV_ADDR, aksv,
					HDCP_KSV_SIZE) == true)
				ret = true;
		}
	}
	return ret;
}

/**
 * Description: this function resets hdcp state machine to initial state
 *
 * Returns:	none
 */
static void hdcp_reset(void)
{
	pr_debug("hdcp: reset\n");

	/* blank TV screen */
	ipil_enable_planes_on_pipe(1, false);

	/* Stop HDCP */
	if (hdcp_context->is_phase1_enabled == true ||
	    hdcp_context->force_reset == true) {
		pr_debug("hdcp: off state\n");
		ipil_hdcp_disable();
		hdcp_context->force_reset = false;
	}

#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	/* this flag will be re-enabled by upper layers */
	hdcp_context->is_required = false;
#endif
	hdcp_context->is_phase1_enabled = false;
	hdcp_context->is_phase2_enabled = false;
	hdcp_context->is_phase3_valid   = false;
	hdcp_context->prev_ri_frm_cnt_status = 0;
}

/**
 * Description: this function schedules repeater authentication
 *
 * @first	whether its first time schedule or not, delay for check is
 *		varied between first and subsequent times
 *
 * Returns:	true on success else false
 */
static bool hdcp_rep_check(bool first)
{
	int msg = HDCP_REPEATER_CHECK;
	int delay = (first) ? 50 : 100;
	unsigned int *auth_id = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (auth_id != NULL) {
		*auth_id = hdcp_context->auth_id;
		return wq_send_message_delayed(msg, (void *)auth_id, delay);
	} else
		pr_debug("hdcp: %s failed to alloc mem\n", __func__);
	return false;
}

/**
 * Description: this function creates a watch dog timer for repeater auth
 *
 * Returns:	true on success else false
 */
static bool hdcp_rep_watch_dog(void)
{
	int msg = HDCP_REPEATER_WDT_EXPIRED;
	unsigned int *auth_id = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (auth_id != NULL) {
		*auth_id = hdcp_context->auth_id;
		/* set a watch dog timer for 5.2 secs, added additional
		   0.2 seconds to be safe */
		return wq_send_message_delayed(msg, (void *)auth_id, 5200);
	} else
		pr_debug("hdcp: %s failed to alloc mem\n", __func__);
	return false;
}

/**
 * Description: this function initiates repeater authentication
 *
 * Returns:	true on success else false
 */
static bool hdcp_initiate_rep_auth(void)
{
	pr_debug("hdcp: initiating repeater check\n");
	hdcp_context->wdt_expired = false;
	if (hdcp_rep_check(true) == true) {
		if (hdcp_rep_watch_dog() == true)
			return true;
		else
			pr_debug("hdcp: failed to start repeater wdt\n");
	} else
		pr_debug("hdcp: failed to start repeater check\n");
	return false;
}

/**
 * Description:	this function schedules ri check
 *
 * @first_check	whether its the first time, interval is varied if first time
 *
 * Returns:	true on success else false
 */
static bool hdcp_stage3_schedule_ri_check(bool first_check)
{
	int msg = HDCP_RI_CHECK;
	unsigned int *msg_data = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	/* Do the first check immediately while adding some randomness  */
	int ri_check_interval = (first_check) ? (20 + (jiffies % 10)) :
				hdcp_context->ri_check_interval;
	if (msg_data != NULL) {
		*msg_data = hdcp_context->auth_id;
		return wq_send_message_delayed(msg, (void *)msg_data,
						ri_check_interval);
	}
	return false;
}

/**
 * Description: this function performs 1st stage HDCP authentication i.e.
 *		exchanging keys and r0 match
 *
 * @is_repeater	variable to return type of downstream device, i.e. repeater
 *		or not
 *
 * Returns:	true on successful authentication else false
 */
static bool hdcp_stage1_authentication(bool *is_repeater)
{
	uint8_t bksv[HDCP_KSV_SIZE], aksv[HDCP_KSV_SIZE], an[HDCP_AN_SIZE];
	struct hdcp_rx_bstatus_t bstatus;
	struct hdcp_rx_bcaps_t bcaps;
	uint8_t retry = 0;
	uint16_t rx_r0 = 0;

	/* Wait (up to 2s) for HDMI sink to be in HDMI mode */
	retry = 40;
	if (hdcp_context->hdmi) {
		while (retry--) {
			if (hdcp_read_bstatus(&bstatus.value) == false) {
				pr_err("hdcp: failed to read bstatus\n");
				return false;
			}
			if (bstatus.hdmi_mode)
				break;
			msleep(50);
			pr_debug("hdcp: waiting for sink to be in HDMI mode\n");
		}
	}

	if (retry == 0)
		pr_err("hdcp: sink is not in HDMI mode\n");

	pr_debug("hdcp: bstatus: %04x\n", bstatus.value);

	/* Read BKSV */
	if (hdcp_read_bksv(bksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to read bksv\n");
		return false;
	}
	pr_debug("hdcp: bksv: %02x%02x%02x%02x%02x\n",
		bksv[0], bksv[1], bksv[2], bksv[3], bksv[4]);

	/* Read An */
	if (ipil_hdcp_get_an(an, HDCP_AN_SIZE) == false) {
		pr_err("hdcp: failed to get an\n");
		return false;
	}
	pr_debug("hdcp: an: %02x%02x%02x%02x%02x%02x%02x%02x\n",
		an[0], an[1], an[2], an[3], an[4], an[5], an[6], an[7]);

	/* Read AKSV */
	if (hdcp_get_aksv(aksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to get aksv\n");
		return false;
	}
	pr_debug("hdcp: aksv: %02x%02x%02x%02x%02x\n",
			aksv[0], aksv[1], aksv[2], aksv[3], aksv[4]);

	/* Write An AKSV to Downstream Rx */
	if (hdcp_send_an_aksv(an, HDCP_AN_SIZE, aksv, HDCP_KSV_SIZE)
						== false) {
		pr_err("hdcp: failed to send an and aksv\n");
		return false;
	}
	pr_debug("hdcp: sent an aksv\n");

	/* Read BKSV */
	if (hdcp_read_bksv(bksv, HDCP_KSV_SIZE) == false) {
		pr_err("hdcp: failed to read bksv\n");
		return false;
	}
	pr_debug("hdcp: bksv: %02x%02x%02x%02x%02x\n",
			bksv[0], bksv[1], bksv[2], bksv[3], bksv[4]);

	/* Read BCAPS */
	if (hdcp_read_bcaps(&bcaps.value) == false) {
		pr_err("hdcp: failed to read bcaps\n");
		return false;
	}
	pr_debug("hdcp: bcaps: %x\n", bcaps.value);


	/* Update repeater present status */
	*is_repeater = bcaps.is_repeater;

	/* Set Repeater Bit */
	if (ipil_hdcp_set_repeater(bcaps.is_repeater) == false) {
		pr_err("hdcp: failed to set repeater bit\n");
		return false;
	}

	/* Write BKSV to Self (hdcp tx) */
	if (ipil_hdcp_set_bksv(bksv) == false) {
		pr_err("hdcp: failed to write bksv to self\n");
		return false;
	}

	pr_debug("hdcp: set repeater & bksv\n");

	/* Start Authentication i.e. computations using hdcp keys */
	if (ipil_hdcp_start_authentication() == false) {
		pr_err("hdcp: failed to start authentication\n");
		return false;
	}

	pr_debug("hdcp: auth started\n");

	/* Wait for 120ms before reading R0' */
	msleep(120);

	/* Check if R0 Ready in hdcp tx */
	retry = 20;
	do {
		if (ipil_hdcp_is_r0_ready() == true)
			break;
		msleep(5);
		retry--;
	} while (retry);

	if (retry == 0 && ipil_hdcp_is_r0_ready() == false) {
		pr_err("hdcp: R0 is not ready\n");
		return false;
	}

	pr_debug("hdcp: tx_r0 ready\n");

	/* Read Ro' from Receiver hdcp rx */
	if (hdcp_read_rx_r0(&rx_r0) == false) {
		pr_err("hdcp: failed to read R0 from receiver\n");
		return false;
	}

	pr_debug("hdcp: rx_r0 = %04x\n", rx_r0);

	/* Check if R0 Matches */
	if (ipil_hdcp_does_ri_match(rx_r0) == false) {
		pr_err("hdcp: R0 does not match\n");
		return false;
	}
	pr_debug("hdcp: R0 matched\n");

	/* Enable Encryption & Check status */
	if (ipil_hdcp_enable_encryption() == false) {
		pr_err("hdcp: failed to enable encryption\n");
		return false;
	}
	pr_debug("hdcp: encryption enabled\n");

	hdcp_context->is_phase1_enabled = true;

	return true;
}

/**
 * Description: this function performs repeater authentication i.e. 2nd
 *		stage HDCP authentication
 *
 * Returns:	true if repeater authentication is in progress or succesful
 *		else false. If in progress repeater authentication would be
 *		rescheduled
 */
static bool hdcp_stage2_repeater_authentication(void)
{
	uint8_t *rep_ksv_list = NULL;
	uint32_t rep_prime_v[HDCP_V_H_SIZE] = {0};
	struct hdcp_rx_bstatus_t bstatus;
	struct hdcp_rx_bcaps_t bcaps;
	bool ret = false;

	/* Repeater Authentication */
	if (hdcp_enable_condition_ready() == false ||
	    hdcp_context->is_phase1_enabled == false ||
	    hdcp_context->wdt_expired == true) {
		pr_debug("hdcp: stage2 auth condition not ready\n");
		return false;
	}

	/* Read BCAPS */
	if (hdcp_read_bcaps(&bcaps.value) == false)
		return false;

	if (!bcaps.is_repeater)
		return false;

	/* Check if fifo ready */
	if (!bcaps.ksv_fifo_ready) {
		/* not ready: reschedule but return true */
		pr_debug("hdcp: rescheduling repeater auth\n");
		hdcp_rep_check(false);
		return true;
	}

	/* Read BSTATUS */
	if (hdcp_read_bstatus(&bstatus.value) == false)
		return false;

	/* Check validity of repeater depth & device count */
	if (bstatus.max_devs_exceeded)
		return false;

	if (bstatus.max_cascade_exceeded)
		return false;

	if (0 == bstatus.device_count)
		return true;

	if (bstatus.device_count > HDCP_MAX_DEVICES)
		return false;

	/* allocate memory for ksv_list */
	rep_ksv_list = kzalloc(bstatus.device_count * HDCP_KSV_SIZE,
				GFP_KERNEL);
	if (!rep_ksv_list) {
		pr_debug("hdcp: rep ksv list alloc failure\n");
		return false;
	}

	/* Read ksv list from repeater */
	if (hdcp_read_rx_ksv_list(rep_ksv_list,
				  bstatus.device_count * HDCP_KSV_SIZE)
				  == false) {
		pr_debug("hdcp: rep ksv list read failure\n");
		goto exit;
	}

	/* TODO: SRM check */

	/* Compute tx sha1 (V) */
	if (ipil_hdcp_compute_tx_v(rep_ksv_list, bstatus.device_count,
				   bstatus.value) == false) {
		pr_debug("hdcp: rep compute tx v failure\n");
		goto exit;
	}

	/* Read rx sha1 (V') */
	if (hdcp_read_rx_v((uint8_t *)rep_prime_v) == false) {
		pr_debug("hdcp: rep read rx v failure\n");
		goto exit;
	}

	/* Verify SHA1 tx(V) = rx(V') */
	if (ipil_hdcp_compare_v(rep_prime_v) == false) {
		pr_debug("hdcp: rep compare v failure\n");
		goto exit;
	}

	pr_debug("hdcp: repeater auth success\n");
	hdcp_context->is_phase2_enabled = true;
	ret = true;

exit:
	kfree(rep_ksv_list);
	return ret;
}

/**
 * Description: Main function that initiates all stages of HDCP authentication
 *
 * Returns:	true on succesful authentication else false
 */
static bool hdcp_start(void)
{
	bool is_repeater = false;

	/* Make sure TMDS is available
	 * Remove this delay since HWC already has the delay
	 */
	/* msleep(hdcp_context->hdcp_delay); */

	pr_debug("hdcp: start\n");

	/* Increment Auth Check Counter */
	hdcp_context->auth_id++;

	/* blank TV screen */
	ipil_enable_planes_on_pipe(1, false);

	/* Check HDCP Status */
	if (ipil_hdcp_is_ready() == false) {
		pr_err("hdcp: hdcp is not ready\n");
		return false;
	}

	/* start 1st stage of hdcp authentication */
	if (hdcp_stage1_authentication(&is_repeater) == false) {
		pr_debug("hdcp: stage 1 authentication fails\n");
		return false;
	}

	/* un-blank TV screen */
	ipil_enable_planes_on_pipe(1, true);


	pr_debug("hdcp: initial authentication completed, repeater:%d\n",
		is_repeater);

	/* Branch Repeater Mode Authentication */
	if (is_repeater == true)
		if (hdcp_initiate_rep_auth() == false)
			return false;

	/* Initiate phase3_valid with true status */
	hdcp_context->is_phase3_valid = true;
	/* Branch Periodic Ri Check */
	pr_debug("hdcp: starting periodic Ri check\n");

	/* Schedule Ri check after 2 sec*/
	if (hdcp_stage3_schedule_ri_check(false) == false) {
		pr_err("hdcp: fail to schedule Ri check\n");
		return false;
	}

	return true;
}

/**
 * Description: verify conditions necessary for re-authentication and
 *		enable HDCP if favourable
 *
 * Returns:	none
 */
static void hdcp_retry_enable(void)
{
	int msg = HDCP_ENABLE;
	if (hdcp_enable_condition_ready() == true &&
		hdcp_context->is_phase1_enabled == false &&
		hdcp_context->auto_retry == true) {
		wq_send_message_delayed(msg, NULL, 30);
		pr_debug("hdcp: retry enable\n");
	}
}

/* Based on hardware Ri frame count, adjust ri_check_interval.
 * Also, make sure Ri check happens right after Ri frame count
 * becomes multiples of 128.
 *  */
static bool hdcp_ri_check_reschedule(void)
{
	#define RI_FRAME_WAIT_LIMIT 150

	struct hdcp_context_t *ctx = hdcp_context;
	uint32_t prev_ri_frm_cnt_status = ctx->prev_ri_frm_cnt_status;
	uint8_t  ri_frm_cnt_status;
	int32_t  ri_frm_cnt;
	int32_t  adj;  /* Adjustment of ri_check_interval in msec */
	uint32_t cnt_ri_wait = 0;
	bool     ret = false;


	/* Query hardware Ri frame counter.
	 * This value is used to adjust ri_check_interval
	 * */
	ipil_hdcp_get_ri_frame_count(&ri_frm_cnt_status);

	/* (frm_cnt_ri - prev_frm_cnt_ri) is expected to be 128. If not,
	 * we have to compensate the time difference, which is caused by async
	 * behavior of CPU clock, scheduler and HDMI clock. If hardware can
	 * provide interrupt signal for Ri check, then this compensation work
	 * can be avoided.
	 * Hardcode "256" is because hardware Ri frame counter is 8 bits.
	 * Hardcode "128" is based on HDCP spec.
	* */
	ri_frm_cnt = ri_frm_cnt_status >= prev_ri_frm_cnt_status      ?
		ri_frm_cnt_status - prev_ri_frm_cnt_status       :
		256 - prev_ri_frm_cnt_status + ri_frm_cnt_status;
	pr_debug("current ri_frm_cnt = %d, previous ri_frm_cnt = %d\n",
			  ri_frm_cnt_status, prev_ri_frm_cnt_status);

	/* Compute adjustment of ri_check_interval*/
	adj = (128 - ri_frm_cnt) * hdcp_context->video_refresh_interval;

	/* Adjust ri_check_interval */
	/* adj<0:  Ri check speed is slower than HDMI clock speed
	 * adj>0:  Ri check speed is faster than HDMI clock speed
	 * */
	pr_debug("adjustment of ri_check_interval  = %d (ms)\n", adj);
	ctx->ri_check_interval += adj;
	if (ctx->ri_check_interval > ctx->ri_check_interval_upper)
		ctx->ri_check_interval = ctx->ri_check_interval_upper;

	if (ctx->ri_check_interval < ctx->ri_check_interval_lower)
		ctx->ri_check_interval = ctx->ri_check_interval_lower;

	pr_debug("ri_check_interval=%d(ms)\n", ctx->ri_check_interval);

	/* Update prev_ri_frm_cnt_status*/
	hdcp_context->prev_ri_frm_cnt_status = ri_frm_cnt_status;

	/* Queue next Ri check task with new ri_check_interval*/
	ret = hdcp_stage3_schedule_ri_check(false);
	if (!ret)
		goto exit;

	/* Now, check if ri_frm_cnt_status is multiples of 128.
	 * If we are too fast, wait for frame 128 (or a few frames after
	 * frame 128) to happen to make sure Ri' is ready.
	 * Why using hardcode "64"? : if ri_frm_cnt_status is close to
	 * multiples of 128 (ie, ri_frm_cnt_status % 128 > 64), we keep waiting.
	 * Otherwise if ri_frm_cnt_status just passes 128
	 * (ie, ri_frm_cnt_status % 128 < 64) we continue.
	 * Note the assumption here is this thread is scheduled to run at least
	 * once in one 64-frame period.
	 *
	 * RI_FRAME_WAIT_LIMIT is in case HW stops updating ri frame
	 * count and causes infinite looping.
	*/
	while ((ri_frm_cnt_status % 128 >= 64) &&
			(cnt_ri_wait < RI_FRAME_WAIT_LIMIT)) {
		msleep(hdcp_context->video_refresh_interval);
		ipil_hdcp_get_ri_frame_count(&ri_frm_cnt_status);
		cnt_ri_wait++;
		pr_debug("current Ri frame count = %d\n", ri_frm_cnt_status);
	}

	if (RI_FRAME_WAIT_LIMIT == cnt_ri_wait) {
		ret = false;
		goto exit;
	}

	/* Match Ri with Ri'*/
	ret = hdcp_stage3_ri_check();

exit:
	return ret;
}

/**
 * Description: workqueue event handler to execute all hdcp tasks
 *
 * @work	work assigned from workqueue contains the task to be handled
 *
 * Returns:	none
 */
static void hdcp_task_event_handler(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct hdcp_wq_struct_t *hwq = container_of(delayed_work,
						struct hdcp_wq_struct_t,
						dwork);
	int msg = 0;
	void *msg_data = NULL;
	bool reset_hdcp = false;
	struct hdcp_context_t *ctx = hdcp_context;

	if (hwq != NULL) {
		msg = hwq->msg;
		msg_data = hwq->msg_data;
	}

	if (hdcp_context == NULL || hwq == NULL)
		goto EXIT_HDCP_HANDLER;

	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
				OSPM_UHB_FORCE_POWER_ON)) {
		pr_err("Unable to power on display island!");
		goto EXIT_HDCP_HANDLER;
	}

	switch (msg) {
	case HDCP_ENABLE:
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
		if (hdcp_enable_condition_ready() == false) {
			/* if enable condition is not ready have to return
			 * from hdcp_enable function immediately */
			reset_hdcp = true;
			break;
		}
#endif
		if (hdcp_enable_condition_ready() == true &&
			hdcp_context->is_phase1_enabled == false &&
			hdcp_start() == false) {
			reset_hdcp = true;
			hdcp_context->force_reset = true;
			pr_debug("hdcp: failed to start hdcp\n");
		}
		break;

	case HDCP_RI_CHECK:
		/*pr_debug("hdcp: RI CHECK\n");*/
		if (msg_data == NULL ||
			*(unsigned int *)msg_data != hdcp_context->auth_id)
			/*pr_debug("hdcp: auth count %d mismatch %d\n",
				*(unsigned int *)msg_data,
				hdcp_context->auth_id);*/
			break;

		/* Do phase 3 only if phase 1 was successful*/
		if (hdcp_context->is_phase1_enabled == false)
			break;

		if (hdcp_ri_check_reschedule() == false)
			reset_hdcp = true;
		break;

	case HDCP_REPEATER_CHECK:
		pr_debug("hdcp: repeater check\n");
		if (msg_data == NULL ||
			*(unsigned int *)msg_data != hdcp_context->auth_id)
			/*pr_debug("hdcp: auth count %d mismatch %d\n",
				*(unsigned int *)msg_data,
				hdcp_context->auth_id);*/
			break;

		if (hdcp_stage2_repeater_authentication() == false)
			reset_hdcp = true;
		break;

	case HDCP_REPEATER_WDT_EXPIRED:
		if (msg_data != NULL && *(unsigned int *)msg_data ==
				hdcp_context->auth_id) {
			/*pr_debug("hdcp: reapter wdt expired, "
				    "auth_id = %d, msg_data = %d\n",
				    hdcp_context->auth_id,
				    *(unsigned int *)msg_data);*/

			hdcp_context->wdt_expired = true;
			if(!hdcp_context->is_phase2_enabled &&
				hdcp_context->is_phase1_enabled)
				reset_hdcp = true;
		}
		break;

	case HDCP_RESET:
		hdcp_reset();
		break;

	case HDCP_SET_POWER_SAVE_STATUS:/* handle suspend resume */
		/* ignore suspend state if HPD is low */
		if (msg_data != NULL && hdcp_context->hpd == true) {
			hdcp_context->suspend = *((bool *)msg_data);
			pr_debug("hdcp: suspend = %d\n",
					hdcp_context->suspend);
			if (hdcp_context->suspend == true) {
				if (hdcp_context->is_phase1_enabled
				    == true)
					reset_hdcp = true;
			}
		}
		break;

	case HDCP_SET_HPD_STATUS:/* handle hpd status */
		if (msg_data != NULL) {
			hdcp_context->hpd = *((bool *)msg_data);
			pr_debug("hdcp: hpd = %d\n",
					hdcp_context->hpd);
			if (hdcp_context->hpd == false) {
				/* reset suspend state if HPD is Low */
				hdcp_context->suspend = false;
				reset_hdcp = true;
			}
		}
		break;

	case HDCP_SET_DPMS_STATUS:/* handle display_power_on status */
		if (msg_data != NULL) {
			hdcp_context->display_power_on =
					*((bool *)msg_data);
			pr_debug("hdcp: display_power_on = %d\n",
					hdcp_context->display_power_on);
			if (hdcp_context->display_power_on == false)
				reset_hdcp = true;
		}
		break;

	default:
		break;
	}

	if (reset_hdcp == true) {
		msg = HDCP_RESET;
		wq_send_message(msg, NULL);
	} else
		/* if disabled retry HDCP authentication */
		hdcp_retry_enable();

	/* Update security component of hdmi and hdcp status */
	if ((ctx->is_phase1_enabled != ctx->previous_phase1_status) ||
			(ctx->hpd != ctx->previous_hpd)) {
		ctx->previous_phase1_status = ctx->is_phase1_enabled;
		ctx->previous_hpd           = ctx->hpd;

		otm_hdmi_update_security_hdmi_hdcp_status(
				ctx->is_phase1_enabled, ctx->hpd);
	}
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);

EXIT_HDCP_HANDLER:
	if (msg_data != NULL)
		kfree(msg_data);
	if (hwq != NULL)
		kfree(hwq);

	return;
}

/**
 * Description: function to update HPD status
 *
 * @hdmi_context handle hdmi_context
 * @hpd		 HPD high/low status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_hpd_state(hdmi_context_t *hdmi_context,
					bool hpd)
{
	int msg = HDCP_SET_HPD_STATUS;
	bool *p_hpd = NULL;
	hpd = !!(hpd);

	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->hpd != hpd) {
		p_hpd = kmalloc(sizeof(bool), GFP_KERNEL);
		if (p_hpd != NULL) {
			*p_hpd = hpd;
			return wq_send_message(msg, (void *)p_hpd);
		} else
			pr_debug("hdcp: %s failed to alloc mem\n", __func__);
	}

	return false;
}

/**
 * Description: function to update power save (suspend/resume) status
 *
 * @hdmi_context handle hdmi_context
 * @suspend	 suspend/resume status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_power_save(hdmi_context_t *hdmi_context,
					bool suspend)
{
	int msg = HDCP_SET_POWER_SAVE_STATUS;
	bool *p_suspend = NULL;

	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->suspend != suspend) {
		p_suspend = kmalloc(sizeof(bool), GFP_KERNEL);
		if (p_suspend != NULL) {
			*p_suspend = suspend;
			return wq_send_message(msg, (void *)p_suspend);
		} else
			pr_debug("hdcp: %s failed to alloc mem\n", __func__);
		if (suspend == true)
			/* Cleanup WorkQueue */
			flush_workqueue(hdcp_context->hdcp_wq);
	}

	return false;
}

/**
 * Description: function to update display_power_on status
 *
 * @hdmi_context handle hdmi_context
 * @display_power_on	display power on/off status
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_set_dpms(hdmi_context_t *hdmi_context,
			bool display_power_on)
{
#ifdef OTM_HDMI_HDCP_ALWAYS_ENC
	int msg = HDCP_SET_DPMS_STATUS;
	bool *p_display_power_on = NULL;
	display_power_on = !!(display_power_on);
#endif

	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	return true;
#else
	if (hdcp_context->display_power_on != display_power_on) {
		p_display_power_on = kmalloc(sizeof(bool), GFP_KERNEL);
		if (p_display_power_on != NULL) {
			*p_display_power_on = display_power_on;
			return wq_send_message(msg, (void *)p_display_power_on);
		} else
			pr_debug("hdcp: %s failed to alloc mem\n", __func__);
		if (display_power_on == false)
			/* Cleanup WorkQueue */

			flush_workqueue(hdcp_context->hdcp_wq);
	}
	return false;
#endif
}

/**
 * Description: Function to check HDCP encryption status
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true if encrypting
 *		else false
 */
bool otm_hdmi_hdcp_enc_status(hdmi_context_t *hdmi_context)
{
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->is_required && hdcp_context->is_phase1_enabled)
		return true;
#endif
	return false;	
}

/**
 * Description: Function to check HDCP Phase3 Link status
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true if link is verified Ri Matches
 *		else false
 */
bool otm_hdmi_hdcp_link_status(hdmi_context_t *hdmi_context)
{
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->is_phase3_valid)
		return true;
#endif
	return false;	
}

/**
 * Description: Function to read BKSV and validate it
 *
 * @hdmi_context handle hdmi_context
 * @bksv	 buffer to store bksv
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_read_validate_bksv(hdmi_context_t *hdmi_context,
				uint8_t *bksv)
{
	bool ret = false;
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	if (hdmi_context == NULL || hdcp_context == NULL || bksv == NULL)
		return false;

	if(hdcp_context->hpd ==true &&
		hdcp_context->suspend == false &&
		hdcp_context->display_power_on == true &&
		hdcp_context->is_required == false &&
		hdcp_context->is_phase1_enabled == false) {
		hdcp_context->sink_query = true;
		ret = hdcp_read_bksv(bksv, HDCP_KSV_SIZE);
		hdcp_context->sink_query = false;
	}
#endif
	return ret;
}

/**
 * Description: function to enable HDCP
 *
 * @hdmi_context handle hdmi_context
 * @refresh_rate vertical refresh rate of the video mode
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_enable(hdmi_context_t *hdmi_context,
				int refresh_rate)
{
	int                  msg = HDCP_ENABLE;
	otm_hdmi_attribute_t hdmi_attr;
	otm_hdmi_ret_t       rc;

	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->is_required == true) {
		pr_debug("hdcp: already enabled\n");
		return true;
	}
#ifdef OTM_HDCP_DEBUG_MODULE
	if (module_disable_hdcp) {
		pr_debug("hdcp: disabled by module\n");
		return false;
	}
#endif

	hdcp_context->is_required = true;

	/* compute ri check interval based on refresh rate */
	if (refresh_rate) {
		/*compute msec time for 1 frame*/
		hdcp_context->video_refresh_interval = 1000 / refresh_rate;

		/* compute msec time for 128 frames per HDCP spec */
		hdcp_context->ri_check_interval = ((128 * 1000) / refresh_rate);
	} else {
		/*compute msec time for 1 frame, assuming refresh rate of 60*/
		hdcp_context->video_refresh_interval = 1000 / 60;

		/* default to 128 frames @ 60 Hz */
		hdcp_context->ri_check_interval      = ((128 * 1000) / 60);
	}

	/* Set upper and lower bounds for ri_check_interval to
	 *  avoid dynamic adjustment to go wild.
	 *  Set adjustment range to 100ms, which is safe if HZ <=100.
	*/
	hdcp_context->ri_check_interval_lower =
			hdcp_context->ri_check_interval - 100;
	hdcp_context->ri_check_interval_upper =
			hdcp_context->ri_check_interval + 100;

	/* Init prev_ri_frm_cnt_status*/
	hdcp_context->prev_ri_frm_cnt_status = 0;

	/* Set ri_retry
	* Default to interval of 3 frames if can not read
	* OTM_HDMI_ATTR_ID_HDCP_RI_RETRY */
	rc = otm_hdmi_get_attribute(hdmi_context,
				OTM_HDMI_ATTR_ID_HDCP_RI_RETRY,
				&hdmi_attr, false);

	hdcp_context->ri_retry = (OTM_HDMI_SUCCESS == rc) ?
				 hdmi_attr.content._uint.value :
				 3 * hdcp_context->video_refresh_interval;

	hdcp_context->hdmi = otm_hdmi_is_monitor_hdmi(hdmi_context);

	pr_debug("hdcp: schedule HDCP enable\n");

#ifdef OTM_HDMI_HDCP_ALWAYS_ENC
	return wq_send_message(msg, NULL);
#else
	/* send message and wait for 1st stage authentication to complete */
	if (wq_send_message(msg, NULL)) {
		/* on any failure is_required flag will be reset */
		while (hdcp_context->is_required) {
			/* wait for phase1 to be enabled before
			 * returning from this function */
			if(hdcp_context->is_phase1_enabled)
				return true;
			msleep(1);
		}
	}

	return false;
#endif
}

/**
 * Description: function to disable HDCP
 *
 * @hdmi_context handle hdmi_context
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_disable(hdmi_context_t *hdmi_context)
{
	int msg = HDCP_RESET;

	if (hdmi_context == NULL || hdcp_context == NULL)
		return false;

	if (hdcp_context->is_required == false) {
		pr_debug("hdcp: already disabled\n");
		return true;
	}

	/* send reset message */
	wq_send_message(msg, NULL);

	/* Cleanup WorkQueue */
	/*flush_workqueue(hdcp_context->hdcp_wq);*/

	/* Wait until hdcp is disabled.
	 * No need to wait for workqueue flushed since it may block for 2sec
	 * */
	while (hdcp_context->is_phase1_enabled)
		msleep(1);

	hdcp_context->is_required = false;

	pr_debug("hdcp: disable\n");

	return true;
}

/**
 * Description: hdcp init function
 *
 * @hdmi_context handle hdmi_context
 * @ddc_rd_wr:	pointer to ddc read write function
 *
 * Returns:	true on success
 *		false on failure
 */
bool otm_hdmi_hdcp_init(hdmi_context_t *hdmi_context,
	int (*ddc_rd_wr)(bool, uint8_t, uint8_t, uint8_t *, int))
{
	otm_hdmi_attribute_t hdmi_attr;
	otm_hdmi_ret_t       rc;

	if (hdmi_context == NULL ||
	    ddc_rd_wr == NULL ||
	    ipil_hdcp_device_can_authenticate() == false ||
	    hdcp_context != NULL) {
		pr_debug("hdcp: init error!!! parameters\n");
		return false;
	}

	/* allocate hdcp context */
	hdcp_context = kmalloc(sizeof(struct hdcp_context_t), GFP_KERNEL);

	/* Create hdcp workqueue to handle all hdcp tasks.
	 * To avoid multiple threads created for multi-core CPU (eg CTP)
	 * use create_singlethread_workqueue.
	 * */
	if (hdcp_context != NULL) {
		hdcp_context->hdcp_wq =
				create_singlethread_workqueue("HDCP_WQ");
	}

	if (hdcp_context == NULL || hdcp_context->hdcp_wq == NULL) {
		pr_debug("hdcp: init error!!! allocation\n");
		goto EXIT_INIT;
	}

	/* initialize hdcp context and hence hdcp state machine */
	hdcp_context->is_required	= false;
	hdcp_context->is_phase1_enabled	= false;
	hdcp_context->is_phase2_enabled	= false;
	hdcp_context->is_phase3_valid	= false;
	hdcp_context->suspend		= false;
	hdcp_context->hpd		= false;
#ifndef OTM_HDMI_HDCP_ALWAYS_ENC
	hdcp_context->display_power_on	= true;
	hdcp_context->auto_retry	= false;
#else
	hdcp_context->display_power_on	= false;
	hdcp_context->auto_retry	= true;
#endif
	hdcp_context->wdt_expired	= false;
	hdcp_context->sink_query	= false;
	hdcp_context->can_authenticate	= true;
	hdcp_context->current_srm_ver	= 0u;
	hdcp_context->vrl		= NULL;
	hdcp_context->vrl_count		= 0u;
	hdcp_context->ri_check_interval	= 0u;
	hdcp_context->force_reset	= false;
	hdcp_context->auth_id		= 0;

	hdcp_context->previous_hpd = false;
	hdcp_context->previous_phase1_status = false;

	/* store i2c function pointer used for ddc read/write */
	hdcp_context->ddc_read_write = ddc_rd_wr;

	/* Find hdcp delay
	 * If attribute not set, default to 200ms
	 */
	rc = otm_hdmi_get_attribute(hdmi_context,
				OTM_HDMI_ATTR_ID_HDCP_DELAY,
				&hdmi_attr, false);

	hdcp_context->hdcp_delay = (rc == OTM_HDMI_SUCCESS) ?
			hdmi_attr.content._uint.value :
			200;

	/* perform any hardware initializations */
	if (ipil_hdcp_init() == true) {
		pr_debug("hdcp: initialized\n");
		return true;
	}

EXIT_INIT:
	/* Cleanup and exit */
	if (hdcp_context != NULL) {
		if (hdcp_context->hdcp_wq != NULL)
			destroy_workqueue(hdcp_context->hdcp_wq);
		kfree(hdcp_context);
		hdcp_context = NULL;
	}

	return false;
}

