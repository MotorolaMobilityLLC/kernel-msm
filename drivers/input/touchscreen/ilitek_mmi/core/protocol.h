/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

/* V3.2 */
#define P3_2_GET_TP_INFORMATION		0x20
#define P3_2_GET_KEY_INFORMATION	0x22
#define P3_2_GET_FIRMWARE_VERSION	0x40
#define P3_2_GET_PROTOCOL_VERSION	0x42

/* V5.X */
#define P5_0_READ_DATA_CTRL			    0xF6
#define P5_0_GET_TP_INFORMATION		    0x20
#define P5_0_GET_KEY_INFORMATION	    0x27
#define P5_0_GET_FIRMWARE_VERSION	    0x21
#define P5_0_GET_PROTOCOL_VERSION	    0x22
#define P5_0_GET_CORE_VERSION		    0x23
#define P5_0_MODE_CONTROL			    0xF0
#define P5_0_SET_CDC_INIT               0xF1
#define P5_0_GET_CDC_DATA               0xF2
#define P5_0_CDC_BUSY_STATE			    0xF3
#define P5_0_I2C_UART				    0x40

#define P5_0_FIRMWARE_UNKNOWN_MODE		0xFF
#define P5_0_FIRMWARE_DEMO_MODE			0x00
#define P5_0_FIRMWARE_TEST_MODE			0x01
#define P5_0_FIRMWARE_DEBUG_MODE		0x02
#define P5_0_FIRMWARE_I2CUART_MODE		0x03
#define P5_0_FIRMWARE_GESTURE_MODE		0x04

#define P5_0_DEMO_PACKET_ID		        0x5A
#define P5_0_DEBUG_PACKET_ID	        0xA7
#define P5_0_TEST_PACKET_ID		        0xF2
#define P5_0_GESTURE_PACKET_ID	        0xAA
#define P5_0_I2CUART_PACKET_ID	        0x7A

#define P5_0_DEMO_MODE_PACKET_LENGTH	43
#define P5_0_DEBUG_MODE_PACKET_LENGTH	1280
#define P5_0_TEST_MODE_PACKET_LENGTH	1180

struct protocol_cmd_list {
	/* version of protocol */
	uint8_t major;
	uint8_t mid;
	uint8_t minor;

	/* Length of command */
	int fw_ver_len;
	int pro_ver_len;
	int tp_info_len;
	int key_info_len;
	int core_ver_len;
	int func_ctrl_len;
	int window_len;
	int cdc_len;
	int cdc_raw_len;

	/* TP information */
	uint8_t cmd_read_ctrl;
	uint8_t cmd_get_tp_info;
	uint8_t cmd_get_key_info;
	uint8_t cmd_get_fw_ver;
	uint8_t cmd_get_pro_ver;
	uint8_t cmd_get_core_ver;
	uint8_t cmd_mode_ctrl;
	uint8_t cmd_i2cuart;
	uint8_t cmd_cdc_busy;

	/* Function control */
	uint8_t sense_ctrl[3];
	uint8_t sleep_ctrl[3];
	uint8_t glove_ctrl[3];
	uint8_t stylus_ctrl[3];
	uint8_t tp_scan_mode[3];
	uint8_t lpwg_ctrl[3];
	uint8_t gesture_ctrl[3];
	uint8_t phone_cover_ctrl[3];
	uint8_t finger_sense_ctrl[3];
	uint8_t proximity_ctrl[3];
	uint8_t plug_ctrl[3];
	uint8_t phone_cover_window[9];

	/* firmware mode */
	uint8_t unknown_mode;
	uint8_t demo_mode;
	uint8_t debug_mode;
	uint8_t test_mode;
	uint8_t i2cuart_mode;
	uint8_t gesture_mode;

	/* Pakcet ID reported by FW */
	uint8_t demo_pid;
	uint8_t debug_pid;
	uint8_t i2cuart_pid;
	uint8_t test_pid;
	uint8_t ges_pid;

	/* Length of finger report */
	int demo_len;
	int debug_len;
	int test_len;
	int gesture_len;

	/* MP Test with cdc commands */
	uint8_t cmd_cdc;
	uint8_t cmd_get_cdc;

	uint8_t cm_data;
	uint8_t cs_data;

	uint8_t rx_short;
	uint8_t rx_open;
	uint8_t tx_short;

	uint8_t mutual_dac;
	uint8_t mutual_bg;
	uint8_t mutual_signal;
	uint8_t mutual_no_bk;
	uint8_t mutual_bk_dac;
	uint8_t mutual_has_bk;
	uint16_t mutual_has_bk_16;

	uint8_t self_dac;
	uint8_t self_bk_dac;
	uint8_t self_has_bk;
	uint8_t self_no_bk;
	uint8_t self_signal;
	uint8_t self_bg;

	uint8_t key_dac;
	uint8_t key_has_bk;
	uint8_t key_bg;
	uint8_t key_no_bk;
	uint8_t key_open;
	uint8_t key_short;

	uint8_t st_no_bk;
	uint8_t st_open;
	uint8_t st_dac;
	uint8_t st_has_bk;
	uint8_t st_bg;

	uint8_t tx_rx_delta;

	uint8_t trcrq_pin;
	uint8_t resx2_pin;
	uint8_t mutual_integra_time;
	uint8_t self_integra_time;
	uint8_t key_integra_time;
	uint8_t st_integra_time;
	uint8_t peak_to_peak;

	uint8_t get_timing;
	uint8_t doze_p2p;
	uint8_t doze_raw;
};

extern struct protocol_cmd_list *protocol;

extern void core_protocol_func_control(int key, int ctrl);
extern int core_protocol_update_ver(uint8_t major, uint8_t mid, uint8_t minor);
extern int core_protocol_init(void);
extern void core_protocol_remove(void);
extern int core_write(uint8_t, uint8_t *, uint16_t);
extern int core_read(uint8_t, uint8_t *, uint16_t);

#endif
