/*
 * Copyright (C) 2010-2011 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif
#ifdef CONFIG_MFD_CPCAP
#include <linux/spi/cpcap.h>
#endif

#include "hid-ids.h"

#define MOT_SEMU	0x0001
#define MOT_IR_REMOTE	0x0002
#define MOT_AUDIO_JACK	0x0004

/* Driver data indicator for a Motorola Multitouch device */
#define MOT_MULTITOUCH  0x0008

#define AUDIO_JACK_STATUS_REPORT	0x3E

#ifdef CONFIG_SWITCH
static struct switch_dev sdev;
#endif

static bool mot_pass_through_mt_input;
module_param(mot_pass_through_mt_input, bool, 0644);
MODULE_PARM_DESC(mot_pass_through_mt_input, "Pass MT to FW without processing");

/* Motorola Multitouch specific Macros */
#define MT_REPORT_ID 0x33
#define MT_DOUBLE_REPORT_ID 0x34
#define MT_MODE_FEATURE_REPORT_ID 0x34
#define MT_KB_REPORT_ID 0x01
#define MT_MOUSE_REPORT_ID 0x02

#define MT_TOUCH_MASK 0x10
#define MT_DRAG_MASK 0x20
#define MT_CONTACT_MASK 0x0f

#define MT_MID_MASK 0x08
#define MT_LEFT_MASK 0x02
#define MT_RIGHT_MASK 0x04
#define MT_ANY_BUTTON_MASK 0x0e
#define MT_NO_BUTTON 0x00
#define MT_TAP_MASK 0x01

#define MT_INVALID_TOUCH 0xff
#define MT_MAX_TOUCHES 0x10

#define MT_TOUCH_SIZE 0x04
#define MT_HDR_SIZE 0x02

#define MT_TOUCH_MAJOR_YES 0x05
#define MT_TOUCH_MAJOR_NO 0x00

/* HID LED usages. Reference Document: USB HID Usage Tables (www.usb.org) */
#define MT_HID_LED_NUML 0x01
#define MT_HID_LED_CAPSL 0x02
#define MT_HID_LED_SCROLL 0x03
#define MT_HID_LED_COMPOSE 0x04
#define MT_HID_LED_KANA 0x05
#define MT_HID_LED_MUTE 0x09
#define MT_HID_LED_MSG_WAITING 0x19
#define MT_HID_LED_STANDBY 0x27
#define MT_HID_LED_GENERIC 0x4b
#define MT_HID_LED_SUSPEND 0x4c
#define MT_HID_LED_EXT_POWER_CONNECTED 0x4d

#define DEBUG_MT 0

static void dbg_mt_log_buffer(u8 *raw_data, int size)
{
	int i = 0;
	char buffer[1024];
	char *p = buffer;
	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < size; i++) {
		sprintf(p, "%.2x,", raw_data[i]);
		p += 3;
	}
	dbg_hid("Motorola Driver Received buffer=%s\n", buffer);
}



/**
 * Enumeration mot_mt_state - to represent the various states that
 * the driver can be on.
 */
enum mot_mt_state {
	MT_TP_INITIAL_STATE = 0x00,
	MT_TP_MOUSE_STATE = 0x01,
	MT_TP_GESTURE_STATE = 0x02,
	MT_TP_ERROR_STATE = 0x03
};

/**
 * struct motorola_sc - Tracks Motorola Mobility Multitouch specific data.
 * @input: Input device through which we report multitouch events for
 * multitouch reports.
 * @rel_inputs: Input device through which we report mouse events for
 * multitouch reports.
 * @kb_input: Input device through which we report keyboard events
 * @quirks: Used to indicate that the driver supports multiple inputs.
 * @ntouches: Number of touches in most recent touch report.
 * @active_touches: Number of touches that are active in the most recent
 * touch report.
 * @touches: Most recent data for all fingers. Indexed by the contact id sent
 * in the report.
 * @prev_btn_state: Button press/release state of the previous report.
 * @prev_mouse_touch_id: Single finger contact id from the previous report
 * which was treated as a mouse input.
 * @driver_state: The current mode of the driver indicating how to handle the
 * next incoming report.
 */
struct motorola_sc {
	bool isMultitouchDev;
	struct input_dev *input;
	struct input_dev *rel_input;
	struct input_dev *kb_input;
	unsigned long quirks;
	int ntouches;
	int active_touches;

	struct {
		char active;
		char drag;
		char width;
		char pad1;
		short x;
		short y;
	} touches[MT_MAX_TOUCHES];
	int prev_btn_state;
	int prev_mouse_touch_id;
	bool mouse_dev_allocated;
	enum mot_mt_state driver_state;
	unsigned long audio_cable_inserted;
	struct work_struct work;
};

static void audio_jack_status_work(struct work_struct *work)
{
#ifdef CONFIG_MFD_CPCAP
	struct motorola_sc *sc = container_of(work, struct motorola_sc, work);

	if (sc->audio_cable_inserted)
		cpcap_accy_whisper_audio_switch_spdif_state(1);
	else
		cpcap_accy_whisper_audio_switch_spdif_state(0);
#endif
}


/* mot_mt_cleanup: Function to ensure proper touch and button
 * release data are sent in case of a disconnection to
 * ensure that the protocol with the UI framework
 * is completed in case of Honeycomb and Later releases
 */
static void mot_mt_cleanup(struct motorola_sc *sc)
{
	int i;
	bool data_sent = false;
	for (i = 0; i < MT_MAX_TOUCHES; i++) {
		if (sc->touches[i].active == true) {
			sc->touches[i].active = false;
			input_report_abs(sc->input, ABS_MT_TOUCH_MAJOR,
						MT_TOUCH_MAJOR_NO);
			input_report_abs(sc->input, ABS_MT_POSITION_X,
							 sc->touches[i].x);
			input_report_abs(sc->input, ABS_MT_POSITION_Y,
							 sc->touches[i].y);
			data_sent = true;
			input_mt_sync(sc->input);
		}
	}
	if (data_sent == true) {
		input_report_key(sc->input, BTN_TOUCH, 0);
		input_sync(sc->input);
	}

	if (sc->prev_btn_state) {
		if (sc->prev_btn_state & MT_LEFT_MASK)
			input_report_key(sc->input, BTN_LEFT, 0);
		if (sc->prev_btn_state & MT_RIGHT_MASK)
			input_report_key(sc->input, BTN_RIGHT, 0);
		if (sc->prev_btn_state & MT_MID_MASK)
			input_report_key(sc->input, BTN_MIDDLE, 0);
		input_sync(sc->input);
	}

	sc->prev_btn_state = MT_NO_BUTTON;
}

/* mot_mt_process_touch_input : Function to process the touch input
 * from the trackpad and convey it across to the Framework layer in
 * Honeycomb or later releases.
 */
static void mot_mt_process_touch_input(struct motorola_sc *sc, u8 *data)
{
	int contact_id, i, offset;
	bool data_sent = false;

	sc->active_touches = 0;
	/* Fill data for all the touches and send the abs touch
	 * data up to the UI framework
	 */
	for (i = 0; i < sc->ntouches; i++) {
		offset = MT_HDR_SIZE + (i * MT_TOUCH_SIZE);
		contact_id = data[offset] & MT_CONTACT_MASK;
		sc->touches[contact_id].x = data[1+offset] |
				((data[2+offset] & 0x0f) << 8);
		sc->touches[contact_id].y = (data[2+offset] >> 4) |
					(data[3+offset] << 4);
		if ((data[offset] & MT_TOUCH_MASK) == MT_TOUCH_MASK) {
			sc->touches[contact_id].active = true;
			sc->active_touches++;
			input_report_abs(sc->input,
				ABS_MT_TOUCH_MAJOR, MT_TOUCH_MAJOR_YES);
		} else {
			sc->touches[contact_id].active = false;
			input_report_abs(sc->input,
				ABS_MT_TOUCH_MAJOR, MT_TOUCH_MAJOR_NO);
		}
		input_report_abs(sc->input, ABS_MT_POSITION_X,
				 sc->touches[contact_id].x);
		input_report_abs(sc->input, ABS_MT_POSITION_Y,
				 sc->touches[contact_id].y);
		input_report_abs(sc->input, ABS_MT_TRACKING_ID,
				 contact_id);
		input_mt_sync(sc->input);
		data_sent = true;
	}
	if (data_sent == true) {
		input_report_abs(sc->input, BTN_TOUCH,
					(sc->active_touches > 0));
		input_sync(sc->input);
	}
}

/* mot_mt_process_btn_input : Function to process the button input
 * from the trackpad and convey it across to the Framework layer in
 * Honeycomb or later releases.
 */
static void mot_mt_process_btn_input(struct motorola_sc *sc, u8 *data)
{
	int sent_btn_input = false;

	if ((sc->prev_btn_state & MT_LEFT_MASK) ^ (data[1] & MT_LEFT_MASK)) {
		input_report_key(sc->input, BTN_LEFT,
					(data[1] & MT_LEFT_MASK));
		sent_btn_input = true;
	}
	if ((sc->prev_btn_state & MT_RIGHT_MASK) ^ (data[1] & MT_RIGHT_MASK)) {
		input_report_key(sc->input, BTN_RIGHT,
					(data[1] & MT_RIGHT_MASK));
		sent_btn_input = true;
	}
	if ((sc->prev_btn_state & MT_MID_MASK) ^ (data[1] & MT_MID_MASK)) {
		input_report_key(sc->input, BTN_MIDDLE,
					(data[1] & MT_MID_MASK));
		sent_btn_input = true;
	}
	sc->prev_btn_state = data[1] & MT_ANY_BUTTON_MASK;

	if (sent_btn_input == true)
		input_sync(sc->input);
}

/* mot_mt_mouse_state_cleanup : Function to ensure proper btn
 * release events are sent in case of a disconnect or a mode change due to
 * a different input to ensure that the protocol with the UI framework is
 * completed (in Pre Honeycomb releases)
 */
static void mot_mt_mouse_state_cleanup(struct motorola_sc *sc)
{
	if (sc->prev_btn_state) {
		if (sc->prev_btn_state & MT_LEFT_MASK)
			input_report_key(sc->rel_input, BTN_LEFT, 0);
		if (sc->prev_btn_state & MT_RIGHT_MASK)
			input_report_key(sc->rel_input, BTN_RIGHT, 0);
		if (sc->prev_btn_state & MT_MID_MASK)
			input_report_key(sc->rel_input, BTN_MIDDLE, 0);
		input_sync(sc->rel_input);
	}
	sc->prev_mouse_touch_id = MT_INVALID_TOUCH;
	sc->prev_btn_state = MT_NO_BUTTON;
}

/* mot_mt_gesture_state_cleanup: Function to ensure proper touch
 * release data is sent in case of a disconnect or a mode change due to
 * a different input to ensure that the protocol with the UI framework
 * is completed (in Pre Honeycomb releases)
 */
static void mot_mt_gesture_state_cleanup(struct motorola_sc *sc)
{
	int i;
	bool data_sent = false;
	for (i = 0; i < MT_MAX_TOUCHES; i++) {
		if (sc->touches[i].active == true) {
			sc->touches[i].active = false;
			input_report_abs(sc->input, ABS_MT_TOUCH_MAJOR,
						MT_TOUCH_MAJOR_NO);
			input_report_abs(sc->input, ABS_MT_POSITION_X, 0);
			input_report_abs(sc->input, ABS_MT_POSITION_Y, 0);
			data_sent = true;
			input_mt_sync(sc->input);
		}
	}
	if (data_sent == true)
		input_sync(sc->input);
}

/* mot_mt_gesture_state_raw_data : Function to handle incoming data
 * when the driver is in gesture state (in Pre Honeycomb releases)
 */
static void mot_mt_gesture_state_raw_data(struct motorola_sc *sc,
				struct hid_device *hdev,
				struct hid_report *report, u8 *data, int size)
{

	int contact_id, i, offset, num_active_touches = 0;
	int active_finger_id = MT_INVALID_TOUCH;
	int x_val = 0;
	int y_val = 0;
	int drag_val = 0;
	bool data_sent = false;

	if (sc->driver_state == MT_TP_MOUSE_STATE)
		mot_mt_mouse_state_cleanup(sc);

	/* Fill the data for the one active finger if there is only 1 finger
	 * active and all the other fingers released
	 * so that we can handle that finger as a mouse input
	 * CASE 6
	 */
	for (i = 0; i < sc->ntouches; i++) {
		offset = MT_HDR_SIZE + (i * MT_TOUCH_SIZE);
		if ((data[offset] & MT_TOUCH_MASK) == MT_TOUCH_MASK) {
			num_active_touches++;
			active_finger_id = data[offset] & MT_CONTACT_MASK;
			x_val = data[1+offset] | ((data[2+offset] & 0x0f) << 8);
			y_val = (data[2+offset] >> 4) | (data[3+offset] << 4);
			drag_val = data[offset] & MT_DRAG_MASK;
		}
	}
	/* Fill data for that active touch id */
	if (num_active_touches == 1) {
		if (sc->driver_state == MT_TP_GESTURE_STATE)
			mot_mt_gesture_state_cleanup(sc);
		sc->driver_state = MT_TP_MOUSE_STATE;
		sc->prev_mouse_touch_id = active_finger_id;
		sc->touches[active_finger_id].active = true;
		sc->touches[active_finger_id].x = x_val;
		sc->touches[active_finger_id].y = y_val;
		sc->touches[active_finger_id].drag = drag_val;
	} else {
		sc->driver_state = MT_TP_GESTURE_STATE;
		/* Fill data for all the touches and send the abs touch
		 * data up for any touch with drag bit 1 */
		for (i = 0; i < sc->ntouches; i++) {
			offset = MT_HDR_SIZE + (i * MT_TOUCH_SIZE);
			contact_id = data[offset] & MT_CONTACT_MASK;
			sc->touches[contact_id].x = data[1+offset] |
					((data[2+offset] & 0x0f) << 8);
			sc->touches[contact_id].y = (data[2+offset] >> 4) |
						(data[3+offset] << 4);
			if ((data[offset] & MT_TOUCH_MASK) == MT_TOUCH_MASK)
				sc->touches[contact_id].active = true;
			else
				sc->touches[contact_id].active = false;
			sc->touches[contact_id].drag = data[offset] &
								MT_DRAG_MASK;
			if (sc->touches[contact_id].drag == MT_DRAG_MASK) {
				if (sc->touches[contact_id].active == true) {
					input_report_abs(sc->input,
					ABS_MT_TOUCH_MAJOR, MT_TOUCH_MAJOR_YES);
				} else {
					input_report_abs(sc->input,
					ABS_MT_TOUCH_MAJOR, MT_TOUCH_MAJOR_NO);
				}
				input_report_abs(sc->input, ABS_MT_POSITION_X,
						 sc->touches[contact_id].x);
				input_report_abs(sc->input, ABS_MT_POSITION_Y,
						 sc->touches[contact_id].y);
				input_mt_sync(sc->input);
				data_sent = true;
			}
		}
		if (data_sent == true)
			input_sync(sc->input);
	}
}


/* mot_mt_mouse_state_raw_data: Function to handle incoming data that
 * requires relative data to be sent out or if there needs to be a transition
 * from the mouse state to a different state (in Pre Honeycomb releases)
 */
static void mot_mt_mouse_state_raw_data(struct motorola_sc *sc,
			struct hid_device *hdev, struct hid_report *report,
			u8 *data, int size)
{
	int X, Y, i, x_disp, y_disp, disp_mag, prev_magnitude;
	int sent_btn_input = false;
	int send_relative_input = 0;
	int rel_x = 0; int rel_y = 0;
	int contact_id, offset, touch_state, current_btn_state;
	prev_magnitude = 0;

	current_btn_state = data[1] & MT_ANY_BUTTON_MASK;

	/* If there is a single touch and that is active */
	if ((sc->ntouches == 1) && (data[2] & MT_TOUCH_MASK)) {
		offset = MT_HDR_SIZE ;
		contact_id = data[offset] & MT_CONTACT_MASK;
		X = data[1+offset] | ((data[2+offset] & 0x0f) << 8);
		Y = (data[2+offset] >> 4) | (data[3+offset] << 4);

		/* Case 2 : Set the Mouse indicator to that touch data and
		wait for the next report */
		if ((current_btn_state == MT_NO_BUTTON) &&
				(sc->prev_btn_state == MT_NO_BUTTON) &&
				(sc->driver_state == MT_TP_INITIAL_STATE)) {
			sc->prev_mouse_touch_id = contact_id;
		} else if (sc->prev_mouse_touch_id == contact_id) {
			/* Case 3 : Save the relative data to send out relative
			inputs for the single touch*/
			rel_x = X - (sc->touches[sc->prev_mouse_touch_id].x);
			rel_y = Y - (sc->touches[sc->prev_mouse_touch_id].y);
		} else if (sc->prev_mouse_touch_id != contact_id) {
			sc->prev_mouse_touch_id = contact_id;
		}

		/* Case 8 will be taken care down the logic cause all btn
		changes are sent out before the relative inputs and any
		relative change is anyway tracked by case 3 */
	} else if ((sc->ntouches == 2) &&
			(current_btn_state == MT_NO_BUTTON)) {
		/* CASE 4 - If 1 finger is released and replaced by another new
		active finger */
		if (((data[MT_HDR_SIZE] & MT_TOUCH_MASK) != MT_TOUCH_MASK) ||
			((data[MT_HDR_SIZE + MT_TOUCH_SIZE] & MT_TOUCH_MASK)
							!= MT_TOUCH_MASK)) {
			if ((data[MT_HDR_SIZE] & MT_TOUCH_MASK) ==
								MT_TOUCH_MASK)
				sc->prev_mouse_touch_id = data[MT_HDR_SIZE] &
								MT_CONTACT_MASK;
			else
				sc->prev_mouse_touch_id = data[MT_HDR_SIZE +
					MT_TOUCH_SIZE] & MT_CONTACT_MASK;
		}
	} else {
	/* CASE 7 : Extract the relative data for the mouse input */
		for (i = 0; i < sc->ntouches; i++) {
			offset = MT_HDR_SIZE + (MT_TOUCH_SIZE*i);
			contact_id = data[offset] & MT_CONTACT_MASK;
			touch_state = data[offset] & MT_TOUCH_MASK;
			X = data[1+offset] | ((data[2+offset] & 0x0f) << 8);
			Y = (data[2+offset] >> 4) | (data[3+offset] << 4);

			/* If a finger was previously active and it is also
			 active now */
			if ((sc->touches[contact_id].active == true) &&
					(touch_state == MT_TOUCH_MASK)) {
				/* Calculate the displacement */
				x_disp = X - sc->touches[contact_id].x ;
				y_disp = Y - sc->touches[contact_id].y ;
				disp_mag = (x_disp * x_disp) +
						(y_disp * y_disp);
				if (disp_mag > prev_magnitude) {
					prev_magnitude = disp_mag;
					rel_x = x_disp;
					rel_y = y_disp;
					sc->prev_mouse_touch_id = contact_id;
				}
			}
		}
	}

	if (rel_x != 0 || rel_y != 0)
		send_relative_input = true;

	if (sc->driver_state == MT_TP_GESTURE_STATE)
		mot_mt_gesture_state_cleanup(sc);

	sc->driver_state = MT_TP_MOUSE_STATE;

	/* Fill finger data */
	for (i = 0; i < sc->ntouches; i++) {
		offset = MT_HDR_SIZE + (MT_TOUCH_SIZE*i);
		contact_id = data[offset] & MT_CONTACT_MASK;
		if ((data[offset] & MT_TOUCH_MASK) == MT_TOUCH_MASK)
			sc->touches[contact_id].active = true;
		else
			 sc->touches[contact_id].active = false;
		sc->touches[contact_id].drag = data[offset] & MT_DRAG_MASK;
		sc->touches[contact_id].x = data[1+offset] |
						((data[2+offset] & 0x0f) << 8);
		sc->touches[contact_id].y = (data[2+offset] >> 4) |
						(data[3+offset] << 4);
	}

	/* Send the mouse btn events , send in priority order
	 * left/ right/middle if more than one pressed
	 */
	if ((sc->prev_btn_state & MT_LEFT_MASK) ^ (data[1] & MT_LEFT_MASK)) {
		input_report_key(sc->rel_input, BTN_LEFT,
					(data[1] & MT_LEFT_MASK));
		sent_btn_input = true;
		sc->prev_btn_state = MT_LEFT_MASK;
	} else if ((sc->prev_btn_state & MT_RIGHT_MASK) ^
				(data[1] & MT_RIGHT_MASK)) {
		input_report_key(sc->rel_input, BTN_RIGHT,
					(data[1] & MT_RIGHT_MASK));
		sent_btn_input = true;
		sc->prev_btn_state = MT_RIGHT_MASK;
	} else if ((sc->prev_btn_state & MT_MID_MASK) ^
				(data[1] & MT_MID_MASK)) {
		input_report_key(sc->rel_input, BTN_MIDDLE,
					(data[1] & MT_MID_MASK));
		sent_btn_input = true;
		sc->prev_btn_state = MT_MID_MASK;
	}

	if (sent_btn_input == true)
		input_sync(sc->rel_input);

	/* Send relative data */
	if (send_relative_input == true) {
		input_report_rel(sc->rel_input, REL_X, rel_x);
		input_report_rel(sc->rel_input, REL_Y, rel_y);
		input_sync(sc->rel_input);
	}
}

static int mot_mt_verify_raw_data(struct motorola_sc *sc, u8 *data, int size)
{
	int i, offset, contact_id, touch_state, j;
	bool finger_tracked;
	bool data_error = false;

	sc->active_touches = 0;
	/* Expect 2 bytes of prefix, and N*4 bytes of touch data. */
	if (size < MT_HDR_SIZE ||
			(((size - MT_HDR_SIZE) % MT_TOUCH_SIZE) != 0))
		/* ignore since the data appears to be incomplete */
		return 1;

	sc->ntouches = (size - MT_HDR_SIZE) / MT_TOUCH_SIZE;

	/* Move to Error state if the data is incorrect */
	if (sc->ntouches > 0) {
		for (i = 0; i < sc->ntouches; i++) {
			offset = MT_HDR_SIZE + (i * MT_TOUCH_SIZE);
			contact_id = data[offset] & MT_CONTACT_MASK;
			touch_state = data[offset] & MT_TOUCH_MASK;
			if (touch_state == MT_TOUCH_MASK)
				sc->active_touches++;
			if ((touch_state != MT_TOUCH_MASK) &&
				(sc->touches[contact_id].active != true)) {
				/* Move to error state because a finger that was
				 * never tracked was asked to be removed*/
				data_error = true;
			}
		}

		/* Verify that all tracked fingers with touch 1 have a data in
		the report data. */
		for (i = 0; i < MT_MAX_TOUCHES; i++) {
			finger_tracked = true;
			if (sc->touches[i].active == true) {
				finger_tracked = false;
				for (j = 0; j < sc->ntouches; j++) {
					offset = MT_HDR_SIZE +
							(MT_TOUCH_SIZE * j);
					contact_id = data[offset] &
								MT_CONTACT_MASK;
					if (contact_id == i) {
						finger_tracked = true;
						break;
					}
				}
			}
			/* Move to error state because a finger that was
			 * tracked is now not present in the report */
			if (finger_tracked == false)
				data_error = true;
		}
	}
	if (data_error == true) {
		dbg_hid("HID_MT: Entering error state\n");
		sc->driver_state = MT_TP_ERROR_STATE;
		mot_mt_mouse_state_cleanup(sc);
		mot_mt_gesture_state_cleanup(sc);
	}
	return 0;
}

/* mot_rawevent : Function to handle the input from the HID device
 * For a Multitouch Device:
 * Based on the various inputs, the handling is either here or the data is
 * passed on to mot_mt_mouse_raw_data or mot_mt_gesture_raw_data
 * The various cases handled and referenced in the code are:
 * Case 1: No fingers are active and no btns are pressed in initial state
 * Case 2: 1 Active finger and no btns pressed
 * Case 3: 1 Active finger when there is already 1 active previously
 * Case 4: 1 Active finger and 1 finger release when 1 previously tracked
 * Case 5: 2 or more finger active
 * Case 6: 1 Active finger and more than one finger released
 * Case 7: Any btn active
 * Case 8: 1 Active finger when a btn was previously tracked.
 * Case 9: Wrong data size received or wrong finger data recieved.
 * Case 10: No fingers active and no btns pressed when driver is in error state.
 * Case 11: Finger data received when driver is in error state
*/
static int mot_rawevent(struct hid_device *hdev, struct hid_report *report,
		     u8 *data, int size)
{
	struct motorola_sc *sc = hid_get_drvdata(hdev);
	int current_btn_state;

	dbg_hid("%s\n", __func__);
	if (DEBUG_MT)
		dbg_mt_log_buffer(data, size);

	if (sc->quirks & MOT_AUDIO_JACK) {
		if (data[0] == AUDIO_JACK_STATUS_REPORT) {
			printk(KERN_INFO "HID %s: Audio cable %s\n", __func__,
				 data[1] ? "inserted" : "removed");
			sc->audio_cable_inserted = data[1];
			schedule_work(&sc->work);
			return 1;
		}
	}

	if (sc->isMultitouchDev == false) {
		/* Pass all data to the  hid core for processing */
		if (DEBUG_MT)
			dbg_hid("For a non-MT device");
		return 0x00;
	} else { /* Process for Multitouch devices Only */
		switch (data[0]) {
		case MT_MOUSE_REPORT_ID:
			/* Send back to the hid core to do the processing */
			return 0x00;

		case MT_KB_REPORT_ID:
			hid_report_raw_event(hdev, HID_INPUT_REPORT,
						data, size, 1);
			return 0x01;

		case MT_REPORT_ID:
			if (mot_pass_through_mt_input == true) {
				if (size < MT_HDR_SIZE ||
				(((size - MT_HDR_SIZE) % MT_TOUCH_SIZE) != 0)) {
					dbg_hid("HC:Incoming data size Error");
					return 0x01;
				} else {
					sc->ntouches =
					   (size - MT_HDR_SIZE) / MT_TOUCH_SIZE;
					mot_mt_process_touch_input(sc, data);
					mot_mt_process_btn_input(sc, data);
					break;
				}
			}
			if (!sc->rel_input)
				return 0x01;
			if ((mot_mt_verify_raw_data(sc, data, size)) == 1)
				return 0x01;

			current_btn_state = data[1] & MT_ANY_BUTTON_MASK ;

			/* CASE 1 : No active fingers and no active btns */
			if ((sc->active_touches == 0) &&
				(current_btn_state == MT_NO_BUTTON)) {

				/* CASE 10 : Cleanup if it was in the mouse or
				 * Gesture state when no active fingers and no
				 *  active btns */
				if (sc->driver_state == MT_TP_MOUSE_STATE)
					mot_mt_mouse_state_cleanup(sc);
				else if (sc->driver_state ==
							MT_TP_GESTURE_STATE)
					mot_mt_gesture_state_cleanup(sc);
				sc->driver_state = MT_TP_INITIAL_STATE;
			} else if (sc->driver_state == MT_TP_ERROR_STATE) {
				/* CASE 11 :In Error state ignore all finger
				*  data */
				if (DEBUG_MT)
					dbg_hid("Ignoring Data in Error State");
			} else if ((current_btn_state > 0) ||
						 (sc->ntouches == 1)) {
				/* Case 7: Handle all btn data under
				 *  mouse protocol */
				/* Case 2,3,8:Handle 1 finger data in
				 *  mouse protocol */
				mot_mt_mouse_state_raw_data(sc, hdev, report,
								data, size);
			} else if (sc->ntouches == 2) {
				/* Case 4 : Handle 2 finger data under mousei
				 * protocol only if one active finger replaced
				 *  by another */
				if (((data[MT_HDR_SIZE] & MT_TOUCH_MASK) !=
							MT_TOUCH_MASK) ||
				    ((data[MT_HDR_SIZE + MT_TOUCH_SIZE] &
					MT_TOUCH_MASK) != MT_TOUCH_MASK)) {
					mot_mt_mouse_state_raw_data(sc,
						      hdev, report, data, size);
				} else {
					mot_mt_gesture_state_raw_data(sc, hdev,
							report, data, size);
				}
			} else if (sc->ntouches > 2) {
				/* Case 5,6:Handle all finger data > 2 as ani
				 *  MT input */
				mot_mt_gesture_state_raw_data(sc, hdev,
							report, data, size);
			}
			break;

		case MT_DOUBLE_REPORT_ID:
			/* Handle double report ids with kb and mt inputs : */
			mot_rawevent(hdev, report, data + 2, data[1]);
			mot_rawevent(hdev, report, data + 2 + data[1],
							size - 2 - data[1]);
			break;

		default:
			return 0x00;
		}
		return 0x01;
	}
	return 0x00;
}

/* Routine to take care of Multitouch specifc Mapping */
static int mot_input_mapping(struct hid_device *hdev,
		struct hid_input *hi, struct hid_field *field,
		struct hid_usage *usage, unsigned long **bit, int *max)
{
	struct hid_report *rep = field->report;
	struct motorola_sc *sc = hid_get_drvdata(hdev);

	/* Perform mapping only for a Multitouch device as necessary */
	if ((sc->quirks) & MOT_MULTITOUCH) {
		switch (rep->id) {
		case MT_MOUSE_REPORT_ID:
			/* Store the mouse input virtual device pointer to sen
			 *  relative data from mt input */
			sc->rel_input = hi->input;
			/* mapping needs to be done by core*/
			return 0;

		case MT_KB_REPORT_ID:
			/* Store this since we may need to send Kb events
			 * ourselves for a double report id and also to
			 * configure the LED inputs */
			if (sc->kb_input == NULL)
				sc->kb_input = hi->input;

			switch (usage->hid & HID_USAGE_PAGE) {
			case HID_UP_LED:
				/* Set the MISC button which is required
				 * for LED functionality in Keyboards */
				set_bit(EV_KEY, sc->kb_input->evbit);
				set_bit(BTN_MISC, sc->kb_input->keybit);

				/* Set the corresponding LED event bits */
				switch (usage->hid & 0xffff) {
				case MT_HID_LED_NUML:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_NUML);
					break;
				case MT_HID_LED_CAPSL:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_CAPSL);
					break;
				case MT_HID_LED_SCROLL:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_SCROLLL);
					break;
				case MT_HID_LED_COMPOSE:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_COMPOSE);
					break;
				case MT_HID_LED_KANA:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_KANA);
					break;
				case MT_HID_LED_MUTE:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_MUTE);
					break;
				case MT_HID_LED_MSG_WAITING:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_MAIL);
					break;
				case MT_HID_LED_STANDBY:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_SLEEP);
					break;
				case MT_HID_LED_GENERIC:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_MISC);
					break;
				case MT_HID_LED_SUSPEND:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_SUSPEND);
					break;
				case MT_HID_LED_EXT_POWER_CONNECTED:
					hid_map_usage(hi, usage, bit,
						 max, EV_LED, LED_CHARGING);
					break;
				}
				set_bit(usage->type, sc->kb_input->evbit);
				set_bit(usage->code, sc->kb_input->ledbit);

				return 0;

			default:
				break;
			}
			return 0;

		case MT_REPORT_ID:
			sc->input = hi->input;
			switch (usage->hid & HID_USAGE_PAGE) {
			case HID_UP_DIGITIZER:
				sc->isMultitouchDev = true;
				switch (usage->hid) {
					/* Used Switch case for future use*/
				case HID_DG_TOUCH:
					hid_map_usage_clear(hi, usage,
						 bit, max, EV_ABS,
						 ABS_MT_TOUCH_MAJOR);
					set_bit(usage->type,
						 hi->input->evbit);
					input_set_abs_params(hi->input,
						 ABS_MT_TOUCH_MAJOR,
						 MT_TOUCH_MAJOR_NO,
						 MT_TOUCH_MAJOR_YES,
						 0, 0);
					return -1;
				default:
					return 0;
				}
			case HID_UP_GENDESK:
				switch (usage->hid) {
				case HID_GD_X:
					hid_map_usage_clear(hi, usage, bit, max,
						EV_ABS, ABS_MT_POSITION_X);
					set_bit(usage->type, hi->input->absbit);
					input_set_abs_params(hi->input,
							ABS_MT_POSITION_X,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					/* Add additional features for webtop */
					set_bit(BTN_TOOL_FINGER,
							hi->input->keybit);
					input_set_abs_params(hi->input, ABS_X,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					return -1;
				case HID_GD_Y:
					hid_map_usage_clear(hi, usage, bit, max,
						EV_ABS, ABS_MT_POSITION_Y);
					set_bit(usage->type, hi->input->absbit);
					input_set_abs_params(hi->input,
							ABS_MT_POSITION_Y,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					/* Add additional features for webtop */
					set_bit(BTN_TOOL_FINGER,
							hi->input->keybit);
					input_set_abs_params(hi->input, ABS_Y,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					return -1;
				default:
					return 0;
				}

			case HID_UP_BUTTON:
				if (mot_pass_through_mt_input == true) {
					dbg_hid("configuration of btn_touch");
					set_bit(EV_KEY, hi->input->evbit);
					set_bit(BTN_LEFT, hi->input->keybit);
					set_bit(BTN_RIGHT, hi->input->keybit);
					set_bit(BTN_MIDDLE, hi->input->keybit);
					set_bit(BTN_TOUCH, hi->input->keybit);
					return -1;
				}
				return 0;

			default:
				return 0;
			}
		default:
			return 0;
		}
	}
	return 0;
}

/* Routine to allocate, register and configure an input device for Mouse
 * inputs when the HID device report descriptio does not declare a mouse
 * report iddeclare a mouse. This will be used to send relative data.
 */
static void mot_mt_setup_mouse_dev(struct input_dev *rel_input,
		struct input_dev *input, struct hid_device *hdev)
{
	input_set_drvdata(rel_input, hdev);
	rel_input->event = input->event;
	rel_input->open = input->open;
	rel_input->close = input->close;
	rel_input->setkeycode = input->setkeycode;
	rel_input->getkeycode = input->getkeycode;
	rel_input->name = "Motorola Multitouch Relative";
	rel_input->phys = input->phys;
	rel_input->uniq = input->uniq;
	rel_input->id.bustype = input->id.bustype;
	rel_input->id.vendor = input->id.vendor;
	rel_input->id.product = input->id.product;
	rel_input->id.version = input->id.version;
	rel_input->dev.parent = input->dev.parent;

	__set_bit(EV_KEY, rel_input->evbit);
	__set_bit(BTN_LEFT, rel_input->keybit);
	__set_bit(BTN_RIGHT, rel_input->keybit);
	__set_bit(BTN_MIDDLE, rel_input->keybit);

	__set_bit(EV_REL, rel_input->evbit);
	__set_bit(REL_X, rel_input->relbit);
	__set_bit(REL_Y, rel_input->relbit);

}

static int mot_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret, result;
	unsigned long quirks = id->driver_data;
	struct motorola_sc *sc;
	unsigned int connect_mask = 0;
	/* Multitouch specific feature report */
	__u8 feature[] = { MT_MODE_FEATURE_REPORT_ID, 0x01 };
	struct hid_report *report;

	dbg_hid("%s %d\n", __func__, __LINE__);

	sc = kzalloc(sizeof(*sc), GFP_KERNEL);
	if (sc == NULL) {
		dev_err(&hdev->dev, "can't alloc motorola descriptor\n");
		return -ENOMEM;
	}

	sc->quirks = quirks;
	hid_set_drvdata(hdev, sc);
	hdev->quirks |= HID_QUIRK_MULTI_INPUT;
	sc->driver_state = MT_TP_INITIAL_STATE;
	sc->prev_mouse_touch_id = MT_INVALID_TOUCH;
	sc->mouse_dev_allocated = false;
	sc->isMultitouchDev = false;

	ret = hid_parse(hdev);
	if (ret) {
		dev_err(&hdev->dev, "parse failed\n");
		goto err_free;
	}
	if (quirks & MOT_SEMU)
		connect_mask |= HID_CONNECT_HIDRAW;
	if (quirks & MOT_IR_REMOTE)
		connect_mask |=
			(HID_CONNECT_HIDINPUT | HID_CONNECT_HIDINPUT_FORCE);
	if (quirks & MOT_AUDIO_JACK)
		INIT_WORK(&sc->work, audio_jack_status_work);

	if (quirks & MOT_MULTITOUCH)
		connect_mask |= (HID_CONNECT_HIDINPUT | HID_CONNECT_HIDRAW);

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		dev_err(&hdev->dev, "hw start failed\n");
		goto err_free_cancel;
	}

	/* After parsing the HID report descriptor, if we receive a Multitouch
	 * and no mouse, then allocate an input device to send relative input */
	if ((sc->rel_input == NULL) && (sc->input != NULL)) {
		dbg_hid("Allocating Mouse input device");
		sc->rel_input = input_allocate_device();
		if (!sc->rel_input) {
			dev_err(&hdev->dev, "Unable to allocate Mouse dev\n");
			goto err_skip_mt;
		} else {
			mot_mt_setup_mouse_dev(sc->rel_input, sc->input, hdev);
			result = input_register_device(sc->rel_input);
			if (result) {
				dev_err(&hdev->dev,
					 "input device registration failed\n");
				input_free_device(sc->rel_input);
				sc->rel_input = NULL;
				goto err_skip_mt;
			} else {
				sc->mouse_dev_allocated = true;
			}
		}
	}

	if (sc->isMultitouchDev == true) {
		report = hid_register_report(hdev, HID_INPUT_REPORT,
						MT_KB_REPORT_ID);
		if (!report) {
			dev_err(&hdev->dev, "unable to register KB report\n");
			goto err_skip_mt;
		}

		report = hid_register_report(hdev, HID_INPUT_REPORT,
						MT_REPORT_ID);
		if (!report) {
			dev_err(&hdev->dev, "unable to register MT report\n");
			goto err_skip_mt;
		}

		report = hid_register_report(hdev, HID_INPUT_REPORT,
						MT_DOUBLE_REPORT_ID);

		if (!report) {
			dev_err(&hdev->dev, "Can't register Double report\n");
			goto err_skip_mt;
		}
		dbg_hid("HID_MT: Sending back feature report");
		ret = hdev->hid_output_raw_report(hdev, feature,
					sizeof(feature), HID_FEATURE_REPORT);
		if (ret != sizeof(feature))
			dev_err(&hdev->dev, "Output report fail:(1:%d)\n", ret);
	}

err_skip_mt:
#ifdef CONFIG_SWITCH
	if (quirks & MOT_SEMU)
		switch_set_state(&sdev, 1);
#endif

	return 0;

err_free_cancel:
	cancel_work_sync(&sc->work);
err_free:
	kfree(sc);
	return ret;
}

static void mot_remove(struct hid_device *hdev)
{
	struct motorola_sc *sc = hid_get_drvdata(hdev);

	dbg_hid("%s\n", __func__);

	cancel_work_sync(&sc->work);
#ifdef CONFIG_MFD_CPCAP
	if (sc->audio_cable_inserted)
		cpcap_accy_whisper_audio_switch_spdif_state(0);
#endif

#ifdef CONFIG_SWITCH
	if (sc->quirks & MOT_SEMU)
		switch_set_state(&sdev, 0);
#endif

	/* Verify presence of Mouse input dev for Multitouch before cleanup */
	if (mot_pass_through_mt_input == false) {
		if (sc->rel_input != NULL)
			mot_mt_mouse_state_cleanup(sc);
		mot_mt_gesture_state_cleanup(sc);
	} else {
		mot_mt_cleanup(sc);
	}

	if (sc->mouse_dev_allocated == true) {
		dbg_hid("Mouse input device is not null");
		if (sc->rel_input != NULL)
			input_unregister_device(sc->rel_input);
	}
	hid_hw_stop(hdev);
	kfree(hid_get_drvdata(hdev));
}

static const struct hid_device_id mot_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_MOTOROLA, USB_DEVICE_ID_HD_DOCK),
		.driver_data = MOT_SEMU | MOT_IR_REMOTE |
			       MOT_AUDIO_JACK | MOT_MULTITOUCH},
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MOT_KB_1G), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_1), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_2), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_3), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_4), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_5), .driver_data = MOT_MULTITOUCH },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MOTOROLA,
		USB_DEVICE_ID_MM_MULTITOUCH_6), .driver_data = MOT_MULTITOUCH },
	{}
};
MODULE_DEVICE_TABLE(hid, mot_devices);

static const struct hid_report_id mot_reports[] = {
	{ HID_INPUT_REPORT },
	{ HID_TERMINATOR }
};

static struct hid_driver motorola_driver = {
	.name = "motorola",
	.id_table = mot_devices,
	.probe = mot_probe,
	.remove = mot_remove,
	.raw_event = mot_rawevent,
	.report_table = mot_reports,
	.input_mapping = mot_input_mapping,
};

static int motorola_init(void)
{
	int ret;

	dbg_hid("Registering MOT HID driver\n");

	ret = hid_register_driver(&motorola_driver);
	if (ret)
		printk(KERN_ERR "Can't register Motorola driver\n");

#ifdef CONFIG_SWITCH
	sdev.name = "whisper_hid";
	switch_dev_register(&sdev);
#endif

	return ret;
}

static void motorola_exit(void)
{
#ifdef CONFIG_SWITCH
	switch_dev_unregister(&sdev);
#endif
	hid_unregister_driver(&motorola_driver);
}

module_init(motorola_init);
module_exit(motorola_exit);
MODULE_LICENSE("GPL");
