/* include/linux/lge_touch_core.h
 *
 * Copyright (C) 2011 LGE.
 *
 * Writer: yehan.ahn@lge.com, 	hyesung.shin@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef LGE_TOUCH_SYNAPTICS_H
#define LGE_TOUCH_SYNAPTICS_H

#define ARRAYED_TOUCH_FW_BIN
#define NUM_OF_EACH_FINGER_DATA_REG             5
#define MAX_NUM_OF_FINGERS                      10

#define DESCRIPTION_TABLE_START                 0xe9
#define ANALOG_TABLE_START                      0xe9
#define BUTTON_TABLE_START                      0xe3

struct ts_function_descriptor {
	u8 	query_base;
	u8 	command_base;
	u8 	control_base;
	u8 	data_base;
	u8 	int_source_count;
	u8 	id;
};

struct finger_data {
	u8	finger_status_reg[3];
	u8	finger_reg[MAX_NUM_OF_FINGERS][NUM_OF_EACH_FINGER_DATA_REG];
};

struct button_data {
	u16	key_code;
};

struct cur_touch_data {
	u8	device_status_reg;		/* DEVICE_STATUS_REG */
	u8	interrupt_status_reg;
	u8	button_data_reg;
	struct finger_data	finger;
	struct button_data	button;
};

struct interrupt_bit_mask {
	u8 flash;
	u8 status;
	u8 abs;
	u8 button;
};

struct synaptics_ts_data {
	u8	is_probed;
	struct regulator                *regulator_vdd;
	struct regulator                *regulator_vio;
	struct i2c_client               *client;
	struct touch_platform_data      *pdata;
	struct ts_function_descriptor   common_dsc;
	struct ts_function_descriptor   finger_dsc;
	struct ts_function_descriptor   button_dsc;
	struct ts_function_descriptor   flash_dsc;
	struct cur_touch_data           ts_data;
	struct touch_fw_info            *fw_info;
	struct interrupt_bit_mask       interrupt_mask;
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
	struct ts_function_descriptor   analog_dsc;
#endif

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
	u8                              ic_panel_type;
#endif
};

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
enum {IC7020_GFF, IC7020_G2, IC3203_G2, IC7020_GFF_H_PTN, IC7020_G2_H_PTN};
#endif

/* extern function */
extern int FirmwareUpgrade(struct synaptics_ts_data *ts, const char* fw_path);
#endif
