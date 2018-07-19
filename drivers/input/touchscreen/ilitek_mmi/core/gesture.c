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

#include "../common.h"
#include "finger_report.h"
#include "firmware.h"
#include "gesture.h"
#include "protocol.h"
#include "config.h"

struct core_gesture_data *core_gesture;

#ifdef HOST_DOWNLOAD
int core_gesture_load_code(void)
{
	int i = 0, ret = 0;
	uint8_t temp[64] = {0};

	core_gesture->entry = true;

	/* Already read during parsing hex file */
	ipio_info("gesture_start_addr = 0x%x, length = 0x%x\n",
	 core_gesture->start_addr, core_gesture->length);
	ipio_info("area = %d, ap_start_addr = 0x%x, ap_length = 0x%x\n",
	 core_gesture->area_section, core_gesture->ap_start_addr,
	  core_gesture->ap_length);

	/* write load gesture flag */
	ipio_info("write load gesture flag\n");
	temp[0] = 0x01;
	temp[1] = 0x0A;
	temp[2] = 0x03;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write gesture flag error\n");

	ipio_info("enter gesture cmd lpwg start\n");
	/* enter gesture cmd lpwg start */
	temp[0] = 0x01;
	temp[1] = 0x0A;
	temp[2] = core_gesture->mode + 1;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write lpwg start error\n");

	for (i = 0 ; i < 20 ; i++) {
		temp[0] = 0xF6;
		temp[1] = 0x0A;
		temp[2] = 0x05;
		if ((core_write(core_config->slave_i2c_addr, temp, 2)) < 0)
			ipio_err("write 0xF6,0xA,0x05 command error\n");

		mdelay(i * 50);

		temp[0] = 0x01;
		temp[1] = 0x0A;
		temp[2] = 0x05;
		if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
			ipio_err("write command error\n");

		if ((core_read(core_config->slave_i2c_addr, temp, 1)) < 0)
			ipio_err("Read command error\n");

		if (temp[0] == 0x91) {
			ipio_info("check fw ready\n");
			break;
		}
	}

	if (temp[0] != 0x91)
		ipio_err("FW is busy, error\n");
	ipio_info("load gesture code\n");
	/* load gesture code */
	if (core_config_ice_mode_enable() < 0)
		ipio_err("Failed to enter ICE mode\n");

	tddi_host_download(true);

	temp[0] = 0x01;
	temp[1] = 0x0A;
	temp[2] = 0x06;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write command error\n");

	core_gesture->entry = false;
	return ret;
}
EXPORT_SYMBOL(core_gesture_load_code);

int core_gesture_load_ap_code(void)
{
	int i = 0, ret = 0;
	uint8_t temp[64] = {0};

	core_gesture->entry = true;

	/* Write Load AP Flag */
	temp[0] = 0x01;
	temp[1] = 0x01;
	temp[2] = 0x00;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write command AP Flag error\n");

	if ((core_read(core_config->slave_i2c_addr, temp, 20)) < 0)
		ipio_err("Read AP Flag error\n");

	/* Leave Gesture Cmd LPWG Stop */
	temp[0] = 0x01;
	temp[1] = 0x0A;
	temp[2] = 0x00;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write command	 LPWG Stop error\n");

	for (i = 0 ; i < 20 ; i++) {

		mdelay(i * 100 + 100);

		temp[0] = 0x01;
		temp[1] = 0x0A;
		temp[2] = 0x05;
		if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
			ipio_err("write command error\n");
		if ((core_read(core_config->slave_i2c_addr, temp, 1)) < 0)
			ipio_err("Read command error\n");
		if (temp[0] == 0x91) {
			ipio_info("check fw ready\n");
			break;
		}
	}

	if (i == 3 && temp[0] != 0x01)
		ipio_err("FW is busy, error\n");

	/* load AP code */
	if (core_config_ice_mode_enable() < 0)
		ipio_err("Failed to enter ICE mode\n");

	tddi_host_download(false);

	temp[0] = 0x01;
	temp[1] = 0x0A;
	temp[2] = 0x06;
	if ((core_write(core_config->slave_i2c_addr, temp, 3)) < 0)
		ipio_err("write command error\n");

	core_gesture->entry = false;
	return ret;
}
EXPORT_SYMBOL(core_gesture_load_ap_code);
#endif

int core_gesture_match_key(uint8_t gdata)
{
	int gcode;

	switch (gdata) {
	case GESTURE_LEFT:
		gcode = KEY_GESTURE_LEFT;
		break;
	case GESTURE_RIGHT:
		gcode = KEY_GESTURE_RIGHT;
		break;
	case GESTURE_UP:
		gcode = KEY_GESTURE_UP;
		break;
	case GESTURE_DOWN:
		gcode = KEY_GESTURE_DOWN;
		break;
	case GESTURE_DOUBLECLICK:
		gcode = KEY_GESTURE_D;
		break;
	case GESTURE_O:
		gcode = KEY_GESTURE_O;
		break;
	case GESTURE_W:
		gcode = KEY_GESTURE_W;
		break;
	case GESTURE_M:
		gcode = KEY_GESTURE_M;
		break;
	case GESTURE_E:
		gcode = KEY_GESTURE_E;
		break;
	case GESTURE_S:
		gcode = KEY_GESTURE_S;
		break;
	case GESTURE_V:
		gcode = KEY_GESTURE_V;
		break;
	case GESTURE_Z:
		gcode = KEY_GESTURE_Z;
		break;
	case GESTURE_C:
		gcode = KEY_GESTURE_C;
		break;
	default:
		gcode = ERROR;
		break;
	}

	ipio_debug(DEBUG_GESTURE, "gcode = %d\n", gcode);
	return gcode;
}
EXPORT_SYMBOL(core_gesture_match_key);

void core_gesture_set_key(struct core_fr_data *fr_data)
{
	struct input_dev *input_dev = fr_data->input_device;

	if (input_dev != NULL) {
		input_set_capability(input_dev, EV_KEY, KEY_POWER);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
		input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);

		__set_bit(KEY_POWER, input_dev->keybit);
		__set_bit(KEY_GESTURE_UP, input_dev->keybit);
		__set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
		__set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
		__set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
		__set_bit(KEY_GESTURE_O, input_dev->keybit);
		__set_bit(KEY_GESTURE_E, input_dev->keybit);
		__set_bit(KEY_GESTURE_M, input_dev->keybit);
		__set_bit(KEY_GESTURE_W, input_dev->keybit);
		__set_bit(KEY_GESTURE_S, input_dev->keybit);
		__set_bit(KEY_GESTURE_V, input_dev->keybit);
		__set_bit(KEY_GESTURE_Z, input_dev->keybit);
		__set_bit(KEY_GESTURE_C, input_dev->keybit);
		return;
	}

	ipio_err("GESTURE: input dev is NULL\n");
}
EXPORT_SYMBOL(core_gesture_set_key);

int core_gesture_init(void)
{
	if (core_gesture == NULL) {
		core_gesture = kzalloc(sizeof(*core_gesture), GFP_KERNEL);
		if (ERR_ALLOC_MEM(core_gesture)) {
			ipio_err("Failed to allocate core_gesture mem, %ld\n",
			 PTR_ERR(core_gesture));
			core_gesture_remove();
			return -ENOMEM;
		}

		core_gesture->entry = false;
	}

	return 0;
}
EXPORT_SYMBOL(core_gesture_init);

void core_gesture_remove(void)
{
	ipio_info("Remove core-gesture members\n");
	ipio_kfree((void **)&core_gesture);
}
EXPORT_SYMBOL(core_gesture_remove);


