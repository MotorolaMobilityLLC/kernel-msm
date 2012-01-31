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
	bool mt_dev_allocated;
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
		input_report_key(sc->input, BTN_TOUCH,
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

/* Routine to allocate, register and configure an input device for
 * Multitouch inputs when the HID device report descriptor indicates
 * the presence of a Multitouch report id.
 */
static void mot_mt_setup_mt_dev(struct input_dev *mt_input,
		struct input_dev *input, struct hid_device *hdev)
{
	input_set_drvdata(mt_input, hdev);
	mt_input->event = input->event;
	mt_input->open = input->open;
	mt_input->close = input->close;
	mt_input->setkeycode = input->setkeycode;
	mt_input->getkeycode = input->getkeycode;
	mt_input->name = "Motorola Multitouch Absolute";
	mt_input->phys = input->phys;
	mt_input->uniq = input->uniq;
	mt_input->id.bustype = input->id.bustype;
	mt_input->id.vendor = input->id.vendor;
	mt_input->id.product = input->id.product;
	mt_input->id.version = input->id.version;
	mt_input->dev.parent = input->dev.parent;
}


/* Routine to take care of Multitouch specifc Mapping */
static int mot_input_mapping(struct hid_device *hdev,
		struct hid_input *hi, struct hid_field *field,
		struct hid_usage *usage, unsigned long **bit, int *max)
{
	struct hid_report *rep = field->report;
	struct motorola_sc *sc = hid_get_drvdata(hdev);
	int result;

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
			 * ourselves for a double report id */
			sc->kb_input = hi->input;
			return 0;

		case MT_REPORT_ID:
			if (sc->input == NULL) {
				dbg_hid("Allocating MT virtual Device");
				sc->input = input_allocate_device();
				if (!sc->input) {
					dev_err(&hdev->dev, "Unable to allocate MT dev\n");
					sc->input = hi->input;
				} else {
					mot_mt_setup_mt_dev(sc->input,
							 hi->input, hdev);
					result = input_register_device(
								sc->input);
					if (result) {
						dev_err(&hdev->dev,
						 "input device reg failed\n");
						input_free_device(sc->input);
						sc->input = hi->input;
					} else {
						sc->mt_dev_allocated = true;
					}
				}
			}
			switch (usage->hid & HID_USAGE_PAGE) {
			case HID_UP_DIGITIZER:
				sc->isMultitouchDev = true;
				switch (usage->hid) {
					/* Used Switch case for future use*/
				case HID_DG_TOUCH:
					set_bit(EV_ABS,
						 sc->input->evbit);
					input_set_abs_params(sc->input,
						 ABS_MT_TOUCH_MAJOR,
						 MT_TOUCH_MAJOR_NO,
						 MT_TOUCH_MAJOR_YES,
						 0, 0);
					input_set_abs_params(sc->input,
						 ABS_MT_TRACKING_ID,
						 0, MT_MAX_TOUCHES, 0, 0);
					input_set_abs_params(sc->input,
						 ABS_MISC, 0, 0, 0, 0);

					return -1;
				default:
					return 0;
				}
			case HID_UP_GENDESK:
				switch (usage->hid) {
				case HID_GD_X:
					set_bit(EV_ABS, sc->input->absbit);
					input_set_abs_params(sc->input,
							ABS_MT_POSITION_X,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					/* Add additional features for webtop */
					set_bit(BTN_TOOL_FINGER,
							sc->input->keybit);
					input_set_abs_params(sc->input, ABS_X,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					return -1;
				case HID_GD_Y:
					set_bit(EV_ABS, sc->input->absbit);
					input_set_abs_params(sc->input,
							ABS_MT_POSITION_Y,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					/* Add additional features for webtop */
					set_bit(BTN_TOOL_FINGER,
							sc->input->keybit);
					input_set_abs_params(sc->input, ABS_Y,
							field->logical_minimum,
							field->logical_maximum,
							0, 0);
					return -1;

				default:
					return 0;
				}

			case HID_UP_BUTTON:
				dbg_hid("configuration of btn_touch");
				set_bit(EV_KEY, sc->input->evbit);
				set_bit(BTN_LEFT, sc->input->keybit);
				set_bit(BTN_RIGHT, sc->input->keybit);
				set_bit(BTN_MIDDLE, sc->input->keybit);
				set_bit(BTN_TOUCH, sc->input->keybit);
				return -1;

			default:
				return 0;
			}
		default:
			return 0;
		}
	}
	return 0;
}

static int mot_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
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
	sc->mt_dev_allocated = false;
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

	/* Multitouch cleanup */
	mot_mt_cleanup(sc);
	if (sc->mt_dev_allocated == true) {
		dbg_hid("MT input device is not null so de-registering");
		input_unregister_device(sc->input);
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
