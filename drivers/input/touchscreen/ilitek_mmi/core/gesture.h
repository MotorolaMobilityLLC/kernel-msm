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

#ifndef __GESTURE_H
#define __GESTURE_H
#define GESTURE_NORMAL_MODE			 0
#define GESTURE_INFO_MPDE			 1
#define GESTURE_INFO_LENGTH			 170
#define GESTURE_MORMAL_LENGTH		 8

/* The example for the gesture virtual keys */
#define GESTURE_DOUBLECLICK				0x58
#define GESTURE_UP						0x60
#define GESTURE_DOWN					0x61
#define GESTURE_LEFT					0x62
#define GESTURE_RIGHT					0x63
#define GESTURE_M						0x64
#define GESTURE_W						0x65
#define GESTURE_C						0x66
#define GESTURE_E						0x67
#define GESTURE_V						0x68
#define GESTURE_O						0x69
#define GESTURE_S						0x6A
#define GESTURE_Z						0x6B

/* oppo gesture define */
#define GESTURE_CODE_V_DOWN						0x6C
#define GESTURE_CODE_V_LEFT						0x6D
#define GESTURE_CODE_V_RIGHT					0x6E
#define GESTURE_CODE_TWO_LINE_2_BOTTOM			0x6F

#define KEY_GESTURE_D					KEY_D
#define KEY_GESTURE_UP					KEY_UP
#define KEY_GESTURE_DOWN				KEY_DOWN
#define KEY_GESTURE_LEFT				KEY_LEFT
#define KEY_GESTURE_RIGHT				KEY_RIGHT
#define KEY_GESTURE_O					KEY_O
#define KEY_GESTURE_E					KEY_E
#define KEY_GESTURE_M					KEY_M
#define KEY_GESTURE_W					KEY_W
#define KEY_GESTURE_S					KEY_S
#define KEY_GESTURE_V					KEY_V
#define KEY_GESTURE_C					KEY_C
#define KEY_GESTURE_Z					KEY_Z

struct core_gesture_data {
	uint32_t start_addr;
	uint32_t length;
	bool entry;
	uint8_t mode; /* normal:0 info:1 */
	uint32_t ap_start_addr;
	uint32_t ap_length;
	uint32_t area_section;
	bool suspend;
};

extern struct core_gesture_data *core_gesture;
extern uint8_t ap_fw[MAX_AP_FIRMWARE_SIZE];
#ifdef HOST_DOWNLOAD
extern int core_gesture_load_code(void);
extern int core_gesture_load_ap_code(void);
#endif
extern int core_gesture_match_key(uint8_t gid);
extern void core_gesture_set_key(struct core_fr_data *fr_data);
extern int core_gesture_init(void);
extern void core_gesture_remove(void);
#endif
