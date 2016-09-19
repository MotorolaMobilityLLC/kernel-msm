/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef _SP_TX_CEC_H
#define _SP_TX_CEC_H

#define CEC_DEVICE_TYPE     CEC_DEVICE_TYPE_PLAYBACK_DEVICE

#define DP 1
#define HDMI 0

#define CEC_FRAME_SIZE    16
#define CEC_BUF_SIZE    50
/*//change as main loop time*/
#define CEC_TIMEOUT_COUNT     100
/*// CEC device logical address*/
/*//not a valid logical address*/
#define CEC_DEVICE_UNKNOWN              15
#define CEC_DEVICE_TV                   0
#define CEC_DEVICE_RECORDING_DEVICE1    1
#define CEC_DEVICE_RECORDING_DEVICE2    2
#define CEC_DEVICE_TUNER1               3
#define CEC_DEVICE_PLAYBACK_DEVICE1     4
#define CEC_DEVICE_AUDIO_SYSTEM         5
#define CEC_DEVICE_TUNER2               6
#define CEC_DEVICE_TUNER3               7
#define CEC_DEVICE_PLAYBACK_DEVICE2     8
#define CEC_DEVICE_RECORDING_DEVICE3    9
#define CEC_DEVICE_TUNER4               10
#define CEC_DEVICE_PLAYBACK_DEVICE3     11
#define CEC_DEVICE_RESERVED1            12
#define CEC_DEVICE_RESERVED2            13
#define CEC_DEVICE_FREEUSE              14
#define CEC_DEVICE_UNREGISTERED         15
#define CEC_DEVICE_BROADCAST            15


/*// CEC device type*/
#define CEC_DEVICE_TYPE_TV                  0
#define CEC_DEVICE_TYPE_RECORDING_DEVICE    1
#define CEC_DEVICE_TYPE_RESERVED            2
#define CEC_DEVICE_TYPE_TUNER               3
#define CEC_DEVICE_TYPE_PLAYBACK_DEVICE     4
#define CEC_DEVICE_TYPE_AUDIO_SYSTEM        5


/*// CEC opcode*/
/*// Active Source*/
#define CEC_OPCODE_ACTIVE_SOURCE                    0x82
/*// Image View On*/
#define CEC_OPCODE_IMAGE_VIEW_ON                    0x04
/*// Text View On*/
#define CEC_OPCODE_TEXT_VIEW_ON                     0x0D
/*// Inactive Source*/
#define CEC_OPCODE_INACTIVE_SOURCE                  0x9D
/*// Request Active Source*/
#define CEC_OPCODE_REQUEST_ACTIVE_SOURCE            0x85
/*// Routing Change*/
#define CEC_OPCODE_ROUTING_CHANGE                   0x80
/*// Routing Information*/
#define CEC_OPCODE_ROUTING_INFORMATION              0x81
/*// Set Stream Path*/
#define CEC_OPCODE_SET_STREAM_PATH                  0x86
/*// Standby*/
#define CEC_OPCODE_STANDBY                          0x36
/*// Record Off*/
#define CEC_OPCODE_RECORD_OFF                       0x0B
/*// Record On*/
#define CEC_OPCODE_RECORD_ON                        0x09
/*// Record Status*/
#define CEC_OPCODE_RECORD_STATUS                    0x0A
/*// Record TV Screen*/
#define CEC_OPCODE_RECORD_TV_SCREEN                 0x0F
/*// Clear Analogue Timer*/
#define CEC_OPCODE_CLEAR_ANALOGUE_TIMER             0x33
/*// Clear Digital Timer*/
#define CEC_OPCODE_CLEAR_DIGITAL_TIMER              0x99
/*// Clear External Timer*/
#define CEC_OPCODE_CLEAR_EXTERNAL_TIMER             0xA1
/*// Set Analogue Timer*/
#define CEC_OPCODE_SET_ANALOGUE_TIMER               0x34
/*// Set Digital Timer*/
#define CEC_OPCODE_SET_DIGITAL_TIMER                0x97
/*// Set External Timer*/
#define CEC_OPCODE_SET_EXTERNAL_TIMER               0xA2
/*// Set Timer Program Title*/
#define CEC_OPCODE_SET_TIMER_PROGRAM_TITLE          0x67
/*// Timer Cleared Status*/
#define CEC_OPCODE_TIMER_CLEARED_STATUS             0x43
/*// Timer Status*/
#define CEC_OPCODE_TIMER_STATUS                     0x35
/*// CEC Version*/
#define CEC_OPCODE_CEC_VERSION                      0x9E
/*// Get CEC Version*/
#define CEC_OPCODE_GET_CEC_VERSION                  0x9F
/*// Give Physical Address*/
#define CEC_OPCODE_GIVE_PHYSICAL_ADDRESS            0x83
/*// Get Menu Language*/
#define CEC_OPCODE_GET_MENU_LANGUAGE                0x91
/*// Report Physical Address*/
#define CEC_OPCODE_REPORT_PHYSICAL_ADDRESS          0x84
/*// Set Menu Language*/
#define CEC_OPCODE_SET_MENU_LANGUAGE                0x32
/*// Deck Control*/
#define CEC_OPCODE_DECK_CONTROL                     0x42
/*// Deck Status*/
#define CEC_OPCODE_DECK_STATUS                      0x1B
/*// Give Deck Status*/
#define CEC_OPCODE_GIVE_DECK_STATUS                 0x1A
/*// Play*/
#define CEC_OPCODE_PLAY                             0x41
/*// Give Tuner Device Status*/
#define CEC_OPCODE_GIVE_TUNER_DEVICE_STATUS         0x08
/*// Select Analogue Service*/
#define CEC_OPCODE_SELECT_ANALOGUE_SERVICE          0x92
/*// Select Digital Service*/
#define CEC_OPCODE_SELECT_DIGITAL_SERVICE           0x93
/*// Tuner Device Status*/
#define CEC_OPCODE_TUNER_DEVICE_STATUS              0x07
/*// Tuner Step Decrement*/
#define CEC_OPCODE_TUNER_STEP_DECREMENT             0x06
/*// Tuner Step Increment*/
#define CEC_OPCODE_TUNER_STEP_INCREMENT             0x05
/*// Device Vendor ID*/
#define CEC_OPCODE_DEVICE_VENDOR_ID                 0x87
/*// Give Device Vendor ID*/
#define CEC_OPCODE_GIVE_DEVICE_VENDOR_ID            0x8C
/*// Vendor Command*/
#define CEC_OPCODE_VENDOR_COMMAND                   0x89
/*// Vendor Command With ID*/
#define CEC_OPCODE_VENDOR_COMMAND_WITH_ID           0xA0
/*// Vendor Remote Button Down*/
#define CEC_OPCODE_VENDOR_REMOTE_BUTTON_DOWN        0x8A
/*// Vendor Remote Button Up*/
#define CEC_OPCODE_VENDOR_REMOTE_BUTTON_UP          0x8B
/*// Set OSD String*/
#define CEC_OPCODE_SET_OSD_STRING                   0x64
/*// Give OSD Name*/
#define CEC_OPCODE_GIVE_OSD_NAME                    0x46
/*// Set OSD Name*/
#define CEC_OPCODE_SET_OSD_NAME                     0x47
/*// Menu Request*/
#define CEC_OPCODE_MENU_REQUEST                     0x8D
/*// Menu Status*/
#define CEC_OPCODE_MENU_STATUS                      0x8E
/*// User Control Pressed*/
#define CEC_OPCODE_USER_CONTROL_PRESSED             0x44
/*// User Control Released*/
#define CEC_OPCODE_USER_CONTROL_RELEASE             0x45
/*// Give Device Power Status*/
#define CEC_OPCODE_GIVE_DEVICE_POWER_STATUS         0x8F
/*// Report Power Status*/
#define CEC_OPCODE_REPORT_POWER_STATUS              0x90
/*// Feature Abort*/
#define CEC_OPCODE_FEATURE_ABORT                    0x00
/*// Abort Message*/
#define CEC_OPCODE_ABORT                            0xFF
/*// Give Audio Status*/
#define CEC_OPCODE_GIVE_AUDIO_STATUS                0x71
/*// Give System Audio Mode Status*/
#define CEC_OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS    0x7D
/*// Report Audio Status*/
#define CEC_OPCODE_REPORT_AUDIO_STATUS              0x7A
/*// Report Short Audio Descriptor*/
#define CEC_OPCODE_REPORT_SHORT_AUDIO_DESCRIPTOR    0xA3
/*// Request Short Audio Descriptor*/
#define CEC_OPCODE_REQUEST_SHORT_AUDIO_DESCRIPTOR   0xA4
/*// Set System Audio Mode*/
#define CEC_OPCODE_SET_SYSTEM_AUDIO_MODE            0x72
/*// System Audio Mode Request*/
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_REQUEST        0x70
/*// System Audio Mode Status*/
#define CEC_OPCODE_SYSTEM_AUDIO_MODE_STATUS         0x7E
/*// Set Audio Rate*/
#define CEC_OPCODE_SET_AUDIO_RATE                   0x9A
/*// Initiate ARC*/
#define CEC_OPCODE_INITIATE_ARC                     0xC0
/*// Report ARC Initiated*/
#define CEC_OPCODE_REPORT_ARC_INITIATED             0xC1
/*// Report ARC Terminated*/
#define CEC_OPCODE_REPORT_ARC_TERMINATED            0xC2
/*// Request ARC Initiation*/
#define CEC_OPCODE_REQUEST_ARC_INITIATION           0xC3
/*// Request ARC Termination*/
#define CEC_OPCODE_REQUEST_ARC_TERMINATION          0xC4
#define CEC_OPCODE_TERMINATE_ARC                    0xC5    /*// Terminate ARC*/
#define CEC_OPCODE_CDC_MESSAGE                      0xF8    /*// CDC Message*/


/*// CEC user control code*/
#define CEC_UI_SELECT           0x00    /*// Select*/
#define CEC_UI_UP               0x01    /*// Up*/
#define CEC_UI_DOWN             0x02    /*// Down*/
#define CEC_UI_LEFT             0x03    /*// Left*/
#define CEC_UI_RIGHT            0x04    /*// Right*/

#define CEC_UI_PLAY             0x44    /*// Play*/
#define CEC_UI_STOP             0x45    /*// Stop*/
#define CEC_UI_PAUSE            0x46    /*// Pause*/
#define CEC_UI_FORWARD          0x4B    /*// Forward*/
#define CEC_UI_BACKWARD         0x4C    /*// Backward*/

extern struct
{
	uint hdmi_cec_receive_done:1;
	uint hdmi_cec_transmit_done:1;
	uint dp_cec_receive_done:1;
	uint dp_cec_transmit_done:1;
	uint dp_resend_flag:1;
	uint hdmi_resend_flag:1;
	uint hdmi_frame_ready:1;
	uint dp_frame_ready:1;
	uint dp_frametx_error:1;
	uint dp_resend_count:3;
	uint hdmi_resend_count:3;
} cec;

void cec_variable_init(void);
void cec_function_enable(void);
void CEC_handler(void);
unchar dp_cec_recv_process(void);
unchar hdmi_cec_send_process(void);
unchar hdmi_cec_recv_process(void);
unchar dp_cec_send_process(void);
unchar dp_buf_length_get(void);
unchar hdmi_buf_length_get(void);
unchar  cec_frame_parse(unchar direction);
unchar  cec_frame_check(unchar direction);
#endif


