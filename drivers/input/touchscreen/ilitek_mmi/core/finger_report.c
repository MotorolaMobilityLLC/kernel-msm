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
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/list.h>

#include "../common.h"
#include "../platform.h"
#include "config.h"
#include "i2c.h"
#include "finger_report.h"
#include "gesture.h"
#include "mp_test.h"
#include "protocol.h"

/* An id with position in each fingers */
struct mutual_touch_point {
	uint16_t id;
	uint16_t x;
	uint16_t y;
	uint16_t pressure;
};

/* Keys and code with each fingers */
struct mutual_touch_info {
	uint8_t touch_num;
	uint8_t key_code;
	struct mutual_touch_point mtp[10];
};

/* Store the packet of finger report */
struct fr_data_node {
	uint8_t *data;
	uint16_t len;
};

/* record the status of touch being pressed or released currently and previosuly */
uint8_t g_current_touch[10];
uint8_t g_previous_touch[10];

/* the total length of finger report packet */
uint16_t g_total_len;

struct mutual_touch_info g_mutual_data;
struct fr_data_node *g_fr_node = NULL, *g_fr_uart = NULL;
struct core_fr_data *core_fr;

/**
 * Calculate the check sum of each packet reported by firmware
 *
 * @pMsg: packet come from firmware
 * @nLength : the length of its packet
 */
uint8_t core_fr_calc_checksum(uint8_t *pMsg, uint32_t nLength)
{
	int i;
	int32_t nCheckSum = 0;

	for (i = 0 ; i < nLength; i++) {
		nCheckSum += pMsg[i];
	}

	return (uint8_t) ((-nCheckSum) & 0xFF);
}
EXPORT_SYMBOL(core_fr_calc_checksum);

/**
 *  Receive data when fw mode stays at i2cuart mode.
 *
 *  the first is to receive N bytes depending on the mode that firmware stays
 *  before going in this function, and it would check with i2c buffer if it
 *  remains the rest of data.
 */
static void i2cuart_recv_packet(void)
{
	int res = 0, need_read_len = 0, one_data_bytes = 0;
	int type = g_fr_node->data[3] & 0x0F;
	int actual_len = g_fr_node->len - 5;

	ipio_debug(DEBUG_FINGER_REPORT, "pid = %x, data[3] = %x, type = %x, actual_len = %d\n",
		g_fr_node->data[0], g_fr_node->data[3], type, actual_len);

	need_read_len = g_fr_node->data[1] * g_fr_node->data[2];

	if (type == 0 || type == 1 || type == 6) {
		one_data_bytes = 1;
	} else if (type == 2 || type == 3) {
		one_data_bytes = 2;
	} else if (type == 4 || type == 5) {
		one_data_bytes = 4;
	}

	ipio_debug(DEBUG_FINGER_REPORT, "need_read_len = %d	 one_data_bytes = %d\n", need_read_len, one_data_bytes);

	need_read_len = need_read_len * one_data_bytes + 1;

	if (need_read_len > actual_len) {
		g_fr_uart = kmalloc(sizeof(*g_fr_uart), GFP_ATOMIC);
		if (ERR_ALLOC_MEM(g_fr_uart)) {
			ipio_err("Failed to allocate g_fr_uart memory %ld\n", PTR_ERR(g_fr_uart));
			return;
		}

		g_fr_uart->len = need_read_len - actual_len;
		g_fr_uart->data = kzalloc(g_fr_uart->len, GFP_ATOMIC);
		if (ERR_ALLOC_MEM(g_fr_uart->data)) {
			ipio_err("Failed to allocate g_fr_uart memory %ld\n", PTR_ERR(g_fr_uart->data));
			return;
		}

		g_total_len += g_fr_uart->len;
		res = core_read(core_config->slave_i2c_addr, g_fr_uart->data, g_fr_uart->len);
		if (res < 0)
			ipio_err("Failed to read finger report packet\n");
	}
}

/*
 * It'd be called when a finger's touching down a screen. It'll notify the event
 * to the uplayer from input device.
 *
 * @x: the axis of X
 * @y: the axis of Y
 * @pressure: the value of pressue on a screen
 * @id: an id represents a finger pressing on a screen
 */
void core_fr_touch_press(int32_t x, int32_t y, uint32_t pressure, int32_t id)
{
	ipio_debug(DEBUG_FINGER_REPORT, "DOWN: id = %d, x = %d, y = %d\n", id, x, y);

	if (ipd->x_flip)
			x = ipd->x_max - x;
	if (ipd->y_flip)
			y = ipd->y_max - y;
	ipio_debug(DEBUG_FINGER_REPORT, "Flip:: DOWN: id = %d, x = %d, y = %d\n", id, x, y);

    if (ipd->MT_B_TYPE) {
		input_mt_slot(core_fr->input_device, id);
		input_mt_report_slot_state(core_fr->input_device, MT_TOOL_FINGER, true);
		input_report_abs(core_fr->input_device, ABS_MT_POSITION_X, x);
		input_report_abs(core_fr->input_device, ABS_MT_POSITION_Y, y);
		if (core_fr->isEnablePressure)
		input_report_abs(core_fr->input_device, ABS_MT_PRESSURE, pressure);
    } else{
		input_report_key(core_fr->input_device, BTN_TOUCH, 1);
		input_report_abs(core_fr->input_device, ABS_MT_TRACKING_ID, id);
		input_report_abs(core_fr->input_device, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(core_fr->input_device, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(core_fr->input_device, ABS_MT_POSITION_X, x);
		input_report_abs(core_fr->input_device, ABS_MT_POSITION_Y, y);
		if (core_fr->isEnablePressure)
			input_report_abs(core_fr->input_device, ABS_MT_PRESSURE, pressure);

		input_mt_sync(core_fr->input_device);
    }
}
EXPORT_SYMBOL(core_fr_touch_press);

/*
 * It'd be called when a finger's touched up from a screen. It'll notify
 * the event to the uplayer from input device.
 *
 * @x: the axis of X
 * @y: the axis of Y
 * @id: an id represents a finger leaving from a screen.
 */
void core_fr_touch_release(int32_t x, int32_t y, int32_t id)
{
	ipio_debug(DEBUG_FINGER_REPORT, "UP: id = %d, x = %d, y = %d\n", id, x, y);

    if (ipd->MT_B_TYPE) {
		input_mt_slot(core_fr->input_device, id);
		input_mt_report_slot_state(core_fr->input_device, MT_TOOL_FINGER, false);
} else{
		input_report_key(core_fr->input_device, BTN_TOUCH, 0);
		input_mt_sync(core_fr->input_device);
	}
}
EXPORT_SYMBOL(core_fr_touch_release);

static int parse_touch_package_v3_2(void)
{
	ipio_info("Not implemented yet\n");
	return 0;
}

static int finger_report_ver_3_2(void)
{
	ipio_info("Not implemented yet\n");
	parse_touch_package_v3_2();
	return 0;
}

/*
 * It mainly parses the packet assembled by protocol v5.0
 */
static int parse_touch_package_v5_0(uint8_t pid)
{
	int i, res = 0;
	uint8_t check_sum = 0;
	uint32_t nX = 0, nY = 0;

	for (i = 0 ; i < 9; i++)
		ipio_debug(DEBUG_FINGER_REPORT, "data[%d] = %x\n", i, g_fr_node->data[i]);

	check_sum = core_fr_calc_checksum(&g_fr_node->data[0], (g_fr_node->len - 1));
	ipio_debug(DEBUG_FINGER_REPORT, "data = %x	;  check_sum : %x\n", g_fr_node->data[g_fr_node->len - 1], check_sum);

	if (g_fr_node->data[g_fr_node->len - 1] != check_sum) {
		ipio_err("Wrong checksum\n");
		res = ERROR;
		goto out;
	}

	/* start to parsing the packet of finger report */
	if (pid == protocol->demo_pid) {
		ipio_debug(DEBUG_FINGER_REPORT, " **** Parsing DEMO packets : 0x%x ****\n", pid);

		for (i = 0 ; i < ipd->MAX_TOUCH_NUM; i++) {
			if ((g_fr_node->data[(4 * i) + 1] == 0xFF) && (g_fr_node->data[(4 * i) + 2] && 0xFF)
				&& (g_fr_node->data[(4 * i) + 3] == 0xFF)) {
				if (ipd->MT_B_TYPE)
					g_current_touch[i] = 0;
				continue;
			}

			nX = (((g_fr_node->data[(4 * i) + 1] & 0xF0) << 4) | (g_fr_node->data[(4 * i) + 2]));
			nY = (((g_fr_node->data[(4 * i) + 1] & 0x0F) << 8) | (g_fr_node->data[(4 * i) + 3]));

			if (!core_fr->isSetResolution) {
				g_mutual_data.mtp[g_mutual_data.touch_num].x = nX * (ipd->x_max) / (ipd->TPD_WIDTH);
				g_mutual_data.mtp[g_mutual_data.touch_num].y = nY * (ipd->y_max) / (ipd->TPD_HEIGHT);
				g_mutual_data.mtp[g_mutual_data.touch_num].id = i;
			} else {
				g_mutual_data.mtp[g_mutual_data.touch_num].x = nX;
				g_mutual_data.mtp[g_mutual_data.touch_num].y = nY;
				g_mutual_data.mtp[g_mutual_data.touch_num].id = i;
			}

			if (core_fr->isEnablePressure)
				g_mutual_data.mtp[g_mutual_data.touch_num].pressure = g_fr_node->data[(4 * i) + 4];
			else
				g_mutual_data.mtp[g_mutual_data.touch_num].pressure = 1;

			ipio_debug(DEBUG_FINGER_REPORT, "[x,y]=[%d,%d]\n", nX, nY);
			ipio_debug(DEBUG_FINGER_REPORT, "point[%d] : (%d,%d) = %d\n",
				g_mutual_data.mtp[g_mutual_data.touch_num].id,
				g_mutual_data.mtp[g_mutual_data.touch_num].x,
				g_mutual_data.mtp[g_mutual_data.touch_num].y, g_mutual_data.mtp[g_mutual_data.touch_num].pressure);

			g_mutual_data.touch_num++;

			if (ipd->MT_B_TYPE)
				g_current_touch[i] = 1;
		}
	} else if (pid == protocol->debug_pid) {
		ipio_debug(DEBUG_FINGER_REPORT, " **** Parsing DEBUG packets : 0x%x ****\n", pid);
		ipio_debug(DEBUG_FINGER_REPORT, "Length = %d\n", (g_fr_node->data[1] << 8 | g_fr_node->data[2]));

		for (i = 0 ; i < ipd->MAX_TOUCH_NUM; i++) {
			if ((g_fr_node->data[(3 * i) + 5] == 0xFF) && (g_fr_node->data[(3 * i) + 6] && 0xFF)
				&& (g_fr_node->data[(3 * i) + 7] == 0xFF)) {
				if (ipd->MT_B_TYPE)
					g_current_touch[i] = 0;
				continue;
			}

			nX = (((g_fr_node->data[(3 * i) + 5] & 0xF0) << 4) | (g_fr_node->data[(3 * i) + 6]));
			nY = (((g_fr_node->data[(3 * i) + 5] & 0x0F) << 8) | (g_fr_node->data[(3 * i) + 7]));

			if (!core_fr->isSetResolution) {
				g_mutual_data.mtp[g_mutual_data.touch_num].x = nX * (ipd->x_max) / (ipd->TPD_WIDTH);
				g_mutual_data.mtp[g_mutual_data.touch_num].y = nY * (ipd->y_max) / (ipd->TPD_HEIGHT);
				g_mutual_data.mtp[g_mutual_data.touch_num].id = i;
			} else {
				g_mutual_data.mtp[g_mutual_data.touch_num].x = nX;
				g_mutual_data.mtp[g_mutual_data.touch_num].y = nY;
				g_mutual_data.mtp[g_mutual_data.touch_num].id = i;
			}

			if (core_fr->isEnablePressure)
				g_mutual_data.mtp[g_mutual_data.touch_num].pressure = g_fr_node->data[(4 * i) + 4];
			else
				g_mutual_data.mtp[g_mutual_data.touch_num].pressure = 1;

			ipio_debug(DEBUG_FINGER_REPORT, "[x,y]=[%d,%d]\n", nX, nY);
			ipio_debug(DEBUG_FINGER_REPORT, "point[%d] : (%d,%d) = %d\n",
				g_mutual_data.mtp[g_mutual_data.touch_num].id,
				g_mutual_data.mtp[g_mutual_data.touch_num].x,
				g_mutual_data.mtp[g_mutual_data.touch_num].y, g_mutual_data.mtp[g_mutual_data.touch_num].pressure);

			g_mutual_data.touch_num++;

			if (ipd->MT_B_TYPE)
				g_current_touch[i] = 1;
		}
	} else {
		if (pid != 0) {
			/* ignore the pid with 0x0 after enable irq at once */
			ipio_err(" **** Unknown PID : 0x%x ****\n", pid);
			res = ERROR;
		}
	}

out:
	return res;
}

/*
 * The function is called by an interrupt and used to handle packet of finger
 * touch from firmware. A differnece in the process of the data is acorrding to the protocol
 */
static int finger_report_ver_5_0(void)
{
	int i, gesture, res = 0;
	static int last_touch;
	uint8_t pid = 0x0;

	memset(&g_mutual_data, 0x0, sizeof(struct mutual_touch_info));

#ifdef I2C_SEGMENT
	res = core_i2c_segmental_read(core_config->slave_i2c_addr, g_fr_node->data, g_fr_node->len);
#else
	res = core_read(core_config->slave_i2c_addr, g_fr_node->data, g_fr_node->len);
#endif

	if (res < 0) {
		ipio_err("Failed to read finger report packet\n");
		if (res == CHECK_RECOVER) {
			ipio_err("==================Recover=================\n");
			/*ilitek_platform_tp_hw_reset(true);*/
			/* Soft reset */
			core_config_ice_mode_enable();
			core_config_set_watch_dog(false);
			mdelay(10);
			core_config_ic_reset();
		}
		goto out;
	}

	pid = g_fr_node->data[0];
	ipio_debug(DEBUG_FINGER_REPORT, "PID = 0x%x\n", pid);

	if (pid == 0)
		goto out;


	if (pid == protocol->i2cuart_pid) {
		ipio_debug(DEBUG_FINGER_REPORT, "I2CUART(0x%x): prepare to receive rest of data\n", pid);
		i2cuart_recv_packet();
		goto out;
	}

	if (pid == protocol->ges_pid && core_config->isEnableGesture) {
		ipio_debug(DEBUG_FINGER_REPORT, "pid = 0x%x, code = %x\n", pid, g_fr_node->data[1]);
		gesture = core_gesture_match_key(g_fr_node->data[1]);
		if (gesture != -1) {
			input_report_key(core_fr->input_device, gesture, 1);
			input_sync(core_fr->input_device);
			input_report_key(core_fr->input_device, gesture, 0);
			input_sync(core_fr->input_device);
		}
		goto out;
	}

	res = parse_touch_package_v5_0(pid);
	if (res < 0) {
		ipio_err("Failed to parse packet of finger touch\n");
		goto out;
	}

	ipio_debug(DEBUG_FINGER_REPORT, "Touch Num = %d, LastTouch = %d\n", g_mutual_data.touch_num, last_touch);

	/* interpret parsed packat and send input events to system */
	if (g_mutual_data.touch_num > 0) {
	if (ipd->MT_B_TYPE) {
		for (i = 0 ; i < g_mutual_data.touch_num; i++) {
			input_report_key(core_fr->input_device, BTN_TOUCH, 1);
			core_fr_touch_press(g_mutual_data.mtp[i].x, g_mutual_data.mtp[i].y, g_mutual_data.mtp[i].pressure, g_mutual_data.mtp[i].id);

			input_report_key(core_fr->input_device, BTN_TOOL_FINGER, 1);
		}

		for (i = 0 ; i < ipd->MAX_TOUCH_NUM; i++) {
			ipio_debug(DEBUG_FINGER_REPORT, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i,
				g_previous_touch[i], i, g_current_touch[i]);

			if (g_current_touch[i] == 0 && g_previous_touch[i] == 1) {
				core_fr_touch_release(0, 0, i);
			}

			g_previous_touch[i] = g_current_touch[i];
		}
	} else{
		for (i = 0 ; i < g_mutual_data.touch_num; i++) {
			core_fr_touch_press(g_mutual_data.mtp[i].x, g_mutual_data.mtp[i].y, g_mutual_data.mtp[i].pressure, g_mutual_data.mtp[i].id);
		}
	}
		input_sync(core_fr->input_device);

		last_touch = g_mutual_data.touch_num;
	} else {
		if (last_touch > 0) {
		if (ipd->MT_B_TYPE) {
			for (i = 0 ; i < ipd->MAX_TOUCH_NUM; i++) {
				ipio_debug(DEBUG_FINGER_REPORT, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i,
					g_previous_touch[i], i, g_current_touch[i]);

				if (g_current_touch[i] == 0 && g_previous_touch[i] == 1) {
					core_fr_touch_release(0, 0, i);
				}
				g_previous_touch[i] = g_current_touch[i];
			}
			input_report_key(core_fr->input_device, BTN_TOUCH, 0);
			input_report_key(core_fr->input_device, BTN_TOOL_FINGER, 0);
		} else{
			core_fr_touch_release(0, 0, 0);
		}

			input_sync(core_fr->input_device);

			last_touch = 0;
		}
	}

out:
	return res;
}

void core_fr_mode_control(uint8_t *from_user)
{
	int i, mode;
	int checksum = 0, codeLength = 8;
	uint8_t mp_code[8] = { 0 };
	uint8_t cmd[4] = { 0 };

	ilitek_platform_disable_irq();

	if (from_user == NULL) {
		ipio_err("Arguments from user space are invaild\n");
		goto out;
	}

	ipio_debug(DEBUG_FINGER_REPORT, "mode = %x, b1 = %x, b2 = %x, b3 = %x\n",
		from_user[0], from_user[1], from_user[2], from_user[3]);

	mode = from_user[0];

	if (protocol->major == 0x5) {
		if (mode == protocol->i2cuart_mode) {
			cmd[0] = protocol->cmd_i2cuart;
			cmd[1] = *(from_user + 1);
			cmd[2] = *(from_user + 2);

			ipio_info("Switch to I2CUART mode, cmd = %x, b1 = %x, b2 = %x\n", cmd[0], cmd[1], cmd[2]);

			if ((core_write(core_config->slave_i2c_addr, cmd, 3)) < 0) {
				ipio_err("Failed to switch I2CUART mode\n");
				goto out;
			}

		} else if (mode == protocol->demo_mode || mode == protocol->debug_mode) {
			cmd[0] = protocol->cmd_mode_ctrl;
			cmd[1] = mode;

			ipio_info("Switch to Demo/Debug mode, cmd = 0x%x, b1 = 0x%x\n", cmd[0], cmd[1]);

			if ((core_write(core_config->slave_i2c_addr, cmd, 2)) < 0) {
				ipio_err("Failed to switch Demo/Debug mode\n");
				goto out;
			}

			core_fr->actual_fw_mode = mode;

		} else if (mode == protocol->test_mode) {
			cmd[0] = protocol->cmd_mode_ctrl;
			cmd[1] = mode;

			ipio_info("Switch to Test mode, cmd = 0x%x, b1 = 0x%x\n", cmd[0], cmd[1]);

			if ((core_write(core_config->slave_i2c_addr, cmd, 2)) < 0) {
				ipio_err("Failed to switch Test mode\n");
				goto out;
			}

			cmd[0] = 0xFE;

			/* Read MP Test information to ensure if fw supports test mode. */
			core_write(core_config->slave_i2c_addr, cmd, 1);
			mdelay(10);
			core_read(core_config->slave_i2c_addr, mp_code, codeLength);

			for (i = 0 ; i < codeLength - 1; i++)
				checksum += mp_code[i];

			if ((-checksum & 0xFF) != mp_code[codeLength - 1]) {
				ipio_info("checksume error (0x%x), FW doesn't support test mode.\n",
						(-checksum & 0XFF));
				goto out;
			}

			/* FW enter to Test Mode */
			#if (INTERFACE == SPI_INTERFACE)
			core_fr->actual_fw_mode = mode;
			#endif
			if (core_mp_move_code() == 0)
				core_fr->actual_fw_mode = mode;

		} else {
			ipio_err("Unknown firmware mode: %x\n", mode);
		}
	} else {
		ipio_err("Wrong the major version of protocol, 0x%x\n", protocol->major);
	}

out:
	ilitek_platform_enable_irq();
}
EXPORT_SYMBOL(core_fr_mode_control);

/**
 * Calculate the length with different modes according to the format of protocol 5.0
 *
 * We compute the length before receiving its packet. If the length is differnet between
 * firmware and the number we calculated, in this case I just print an error to inform users
 * and still send up to users.
 */
static uint16_t calc_packet_length(void)
{
	uint16_t xch = 0, ych = 0, stx = 0, srx = 0;
	/* FIXME: self_key not defined by firmware yet */
	uint16_t self_key = 2;
	uint16_t rlen = 0;

	if (protocol->major == 0x5) {
		if (!ERR_ALLOC_MEM(core_config->tp_info)) {
			xch = core_config->tp_info->nXChannelNum;
			ych = core_config->tp_info->nYChannelNum;
			stx = core_config->tp_info->self_tx_channel_num;
			srx = core_config->tp_info->self_rx_channel_num;
		}

		ipio_debug(DEBUG_FINGER_REPORT, "firmware mode : 0x%x\n", core_fr->actual_fw_mode);

		if (protocol->demo_mode == core_fr->actual_fw_mode) {
			rlen = protocol->demo_len;
		} else if (protocol->test_mode == core_fr->actual_fw_mode) {
			if (ERR_ALLOC_MEM(core_config->tp_info)) {
				rlen = protocol->test_len;
			} else {
				rlen = (2 * xch * ych) + (stx * 2) + (srx * 2) + 2 * self_key + 1;
				rlen += 1;
			}
		} else if (protocol->debug_mode == core_fr->actual_fw_mode) {
			if (ERR_ALLOC_MEM(core_config->tp_info)) {
				rlen = protocol->debug_len;
			} else {
				rlen = (2 * xch * ych) + (stx * 2) + (srx * 2) + 2 * self_key + (8 * 2) + 1;
				rlen += 35;
			}
		} else if (protocol->gesture_mode == core_fr->actual_fw_mode) {
			if (core_gesture->mode == GESTURE_NORMAL_MODE)
				rlen = GESTURE_MORMAL_LENGTH;
			else
				rlen = GESTURE_INFO_LENGTH;
			ipio_debug(DEBUG_FINGER_REPORT, "rlen = %d\n", rlen);
		} else {
			ipio_err("Unknown firmware mode : %d\n", core_fr->actual_fw_mode);
			rlen = 0;
		}
	} else {
		ipio_err("Wrong the major version of protocol, 0x%x\n", protocol->major);
		return ERROR;
	}

	ipio_debug(DEBUG_FINGER_REPORT, "rlen = %d\n", rlen);
	return rlen;
}

/**
 * The table is used to handle calling functions that deal with packets of finger report.
 * The callback function might be different of what a protocol is used on a chip.
 *
 * It's possible to have the different protocol according to customer's requirement on the same
 * touch ic with customised firmware, so I don't have to identify which of the ic has been used; instead,
 * the version of protocol should match its parsing pattern.
 */
typedef struct {
	uint8_t protocol_marjor_ver;
	uint8_t protocol_minor_ver;
	int (*finger_report)(void);
} fr_hashtable;

fr_hashtable fr_t[] = {
	{0x3, 0x2, finger_report_ver_3_2},
	{0x5, 0x0, finger_report_ver_5_0},
	{0x5, 0x1, finger_report_ver_5_0},
};

/**
 * The function is an entry for the work queue registered by ISR activates.
 *
 * Here will allocate the size of packet depending on what the current protocol
 * is used on its firmware.
 */
void core_fr_handler(void)
{
	int i = 0;
	uint8_t *tdata = NULL;

	if (core_fr->isEnableFR) {
		g_total_len = calc_packet_length();
		if (g_total_len) {
			g_fr_node = kmalloc(sizeof(*g_fr_node), GFP_ATOMIC);
			if (ERR_ALLOC_MEM(g_fr_node)) {
				ipio_err("Failed to allocate g_fr_node memory %ld\n", PTR_ERR(g_fr_node));
				goto out;
			}

			g_fr_node->data = kcalloc(g_total_len, sizeof(uint8_t), GFP_ATOMIC);
			if (ERR_ALLOC_MEM(g_fr_node->data)) {
				ipio_err("Failed to allocate g_fr_node memory %ld\n", PTR_ERR(g_fr_node->data));
				goto out;
			}

			g_fr_node->len = g_total_len;
			memset(g_fr_node->data, 0xFF, (uint8_t) sizeof(uint8_t) * g_total_len);

			while (i < ARRAY_SIZE(fr_t)) {
				if (protocol->major == fr_t[i].protocol_marjor_ver) {
					mutex_lock(&ipd->plat_mutex);
					fr_t[i].finger_report();
					mutex_unlock(&ipd->plat_mutex);

					/* 2048 is referred to the defination by user */
					if (g_total_len < 2048) {
						tdata = kmalloc(g_total_len, GFP_ATOMIC);
						if (ERR_ALLOC_MEM(tdata)) {
							ipio_err("Failed to allocate g_fr_node memory %ld\n",
								PTR_ERR(tdata));
							goto out;
						}

						memcpy(tdata, g_fr_node->data, g_fr_node->len);
						/* merge uart data if it's at i2cuart mode */
						if (g_fr_uart != NULL)
							memcpy(tdata + g_fr_node->len, g_fr_uart->data, g_fr_uart->len);
					} else {
						ipio_err("total length (%d) is too long than user can handle\n",
							g_total_len);
						goto out;
					}

					if (core_fr->isEnableNetlink)
						netlink_reply_msg(tdata, g_total_len);

					if (ipd->debug_node_open) {
						mutex_lock(&ipd->ilitek_debug_mutex);
						memset(ipd->debug_buf[ipd->debug_data_frame], 0x00,
							   (uint8_t) sizeof(uint8_t) * 2048);
						memcpy(ipd->debug_buf[ipd->debug_data_frame], tdata, g_total_len);
						ipd->debug_data_frame++;
						if (ipd->debug_data_frame > 1) {
							ipio_info("ipd->debug_data_frame = %d\n", ipd->debug_data_frame);
						}
						if (ipd->debug_data_frame > 1023) {
							ipio_err("ipd->debug_data_frame = %d > 1024\n",
								ipd->debug_data_frame);
							ipd->debug_data_frame = 1023;
						}
						mutex_unlock(&ipd->ilitek_debug_mutex);
						wake_up(&(ipd->inq));
					}
					break;
				}
				i++;
			}

			if (i >= ARRAY_SIZE(fr_t))
				ipio_err("Can't find any callback functions to handle INT event\n");
		} else {
			ipio_err("Wrong the length of packet\n");
		}
	} else {
		ipio_err("The figner report was disabled\n");
		return;
	}
	ilitek_platform_enable_irq();

out:
	ipio_kfree((void **)&tdata);

	if (g_fr_node != NULL) {
		ipio_kfree((void **)&g_fr_node->data);
		ipio_kfree((void **)&g_fr_node);
	}

	if (g_fr_uart != NULL) {
		ipio_kfree((void **)&g_fr_uart->data);
		ipio_kfree((void **)&g_fr_uart);
	}

	g_total_len = 0;
	ipio_debug(DEBUG_IRQ, "handle INT done\n\n");
}
EXPORT_SYMBOL(core_fr_handler);

void core_fr_input_set_param(struct input_dev *input_device)
{
	int max_x = 0, max_y = 0, min_x = 0, min_y = 0;
	int max_tp = 0;

	core_fr->input_device = input_device;

	/* set the supported event type for input device */
	set_bit(EV_ABS, core_fr->input_device->evbit);
	set_bit(EV_SYN, core_fr->input_device->evbit);
	set_bit(EV_KEY, core_fr->input_device->evbit);
	set_bit(BTN_TOUCH, core_fr->input_device->keybit);
	set_bit(BTN_TOOL_FINGER, core_fr->input_device->keybit);
	set_bit(INPUT_PROP_DIRECT, core_fr->input_device->propbit);

	if (core_fr->isSetResolution) {
		max_x = core_config->tp_info->nMaxX;
		max_y = core_config->tp_info->nMaxY;
		min_x = core_config->tp_info->nMinX;
		min_y = core_config->tp_info->nMinY;
		max_tp = core_config->tp_info->nMaxTouchNum;
	} else {
		max_x = ipd->x_max;
		max_y = ipd->y_max;
		min_x = ipd->x_min;
		min_y = ipd->y_min;
		max_tp = ipd->MAX_TOUCH_NUM;
	}

	ipio_info("input resolution : max_x = %d, max_y = %d, min_x = %d, min_y = %d\n", max_x, max_y, min_x, min_y);
	ipio_info("input touch number: max_tp = %d\n", max_tp);

	input_set_abs_params(core_fr->input_device, ABS_MT_POSITION_X, min_x, max_x - 1, 0, 0);
	input_set_abs_params(core_fr->input_device, ABS_MT_POSITION_Y, min_y, max_y - 1, 0, 0);

	input_set_abs_params(core_fr->input_device, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(core_fr->input_device, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	if (core_fr->isEnablePressure)
		input_set_abs_params(core_fr->input_device, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (ipd->MT_B_TYPE) {
		#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(core_fr->input_device, max_tp, INPUT_MT_DIRECT);
		#else
		input_mt_init_slots(core_fr->input_device, max_tp);
		#endif /* LINUX_VERSION_CODE */
	} else{
		input_set_abs_params(core_fr->input_device, ABS_MT_TRACKING_ID, 0, max_tp, 0, 0);
	} /* MT_B_TYPE */

	/* Set up virtual key with gesture code */
	core_gesture_set_key(core_fr);
}
EXPORT_SYMBOL(core_fr_input_set_param);

int core_fr_init(void)
{
	int i = 0;

	core_fr = kzalloc(sizeof(*core_fr), GFP_KERNEL);
	if (ERR_ALLOC_MEM(core_fr)) {
		ipio_err("Failed to allocate core_fr mem, %ld\n", PTR_ERR(core_fr));
		core_fr_remove();
		return -ENOMEM;
	}

	for (i = 0 ; i < ARRAY_SIZE(ipio_chip_list); i++) {
		if (ipio_chip_list[i] == ipd->chip_id) {
			core_fr->isEnableFR = true;
			core_fr->isEnableNetlink = false;
			core_fr->isEnablePressure = false;
			core_fr->isSetResolution = false;
			core_fr->actual_fw_mode = protocol->demo_mode;
			return 0;
		}
	}

	ipio_err("Can't find this chip in support list\n");
	return 0;
}
EXPORT_SYMBOL(core_fr_init);

void core_fr_remove(void)
{
	ipio_info("Remove core-FingerReport members\n");
	ipio_kfree((void **)&core_fr);
}
EXPORT_SYMBOL(core_fr_remove);
