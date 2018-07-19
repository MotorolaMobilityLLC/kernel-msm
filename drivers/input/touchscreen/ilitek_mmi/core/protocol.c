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
#include "config.h"
#include "i2c.h"
#include "spi.h"
#include "protocol.h"

#define FUNC_NUM    20

struct protocol_sup_list {
	uint8_t major;
	uint8_t mid;
	uint8_t minor;
};

struct DataItem {
	int key;
	char *name;
	int len;
	uint8_t *cmd;
};

struct DataItem *hashArray[FUNC_NUM];
struct protocol_cmd_list *protocol;

int core_write(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize)
{
#if (INTERFACE == I2C_INTERFACE)
	return core_i2c_write(nSlaveId, pBuf, nSize);
#else
	return core_spi_write(pBuf, nSize);
#endif
}
EXPORT_SYMBOL(core_write);

int core_read(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize)
{
#if (INTERFACE == I2C_INTERFACE)
	return core_i2c_read(nSlaveId, pBuf, nSize);
#else
	return core_spi_read(pBuf, nSize);
#endif
}
EXPORT_SYMBOL(core_read);

static int hashCode(int key)
{
	return key % FUNC_NUM;
}

static struct DataItem *search_func(int key)
{
	int hashIndex = hashCode(key);

	while (hashArray[hashIndex] != NULL) {
		if (hashArray[hashIndex]->key == key)
			return hashArray[hashIndex];

		hashIndex++;
		hashIndex %= FUNC_NUM;
	}

	return NULL;
}

static void insert_func(int key, int len, uint8_t *cmd, char *name)
{
	int i, hashIndex;
	struct DataItem *tmp = NULL;

	tmp = kmalloc(sizeof(struct DataItem), GFP_KERNEL);
	if (ERR_ALLOC_MEM(tmp)) {
		ipio_err("Failed to allocate memory\n");
		return;
	}

	tmp->key = key;
	tmp->name = name;
	tmp->len = len;

	tmp->cmd = kcalloc(len, sizeof(uint8_t), GFP_KERNEL);
	for (i = 0 ; i < len ; i++)
		tmp->cmd[i] = cmd[i];

	hashIndex = hashCode(key);

	while (hashArray[hashIndex] != NULL) {
		hashIndex++;
		hashIndex %= FUNC_NUM;
	}

	hashArray[hashIndex] = tmp;
}

static void free_func_hash(void)
{
	int i;

	for (i = 0 ; i < FUNC_NUM; i++)
		ipio_kfree((void **)&hashArray[i]);
}

static void create_func_hash(void)
{
	/* if protocol is updated, we free its allocated mem at all before create new ones. */
	free_func_hash();

	insert_func(0, protocol->func_ctrl_len, protocol->sense_ctrl, "sense_ctrl");
	insert_func(1, protocol->func_ctrl_len, protocol->sleep_ctrl, "sleep_ctrl");
	insert_func(2, protocol->func_ctrl_len, protocol->glove_ctrl, "glove_ctrl");
	insert_func(3, protocol->func_ctrl_len, protocol->stylus_ctrl, "stylus_ctrl");
	insert_func(4, protocol->func_ctrl_len, protocol->tp_scan_mode, "tp_scan_mode");
	insert_func(5, protocol->func_ctrl_len, protocol->lpwg_ctrl, "lpwg_ctrl");
	insert_func(6, protocol->func_ctrl_len, protocol->gesture_ctrl, "gesture_ctrl");
	insert_func(7, protocol->func_ctrl_len, protocol->phone_cover_ctrl, "phone_cover_ctrl");
	insert_func(8, protocol->func_ctrl_len, protocol->finger_sense_ctrl, "finger_sense_ctrl");
	insert_func(9, protocol->window_len, protocol->phone_cover_window, "phone_cover_window");
	insert_func(10, protocol->func_ctrl_len, protocol->finger_sense_ctrl, "finger_sense_ctrl");
	insert_func(11, protocol->func_ctrl_len, protocol->proximity_ctrl, "proximity_ctrl");
	insert_func(12, protocol->func_ctrl_len, protocol->plug_ctrl, "plug_ctrl");
}

static void config_protocol_v5_cmd(void)
{
	if (protocol->mid == 0x0) {
		protocol->func_ctrl_len = 2;

		protocol->sense_ctrl[0] = 0x1;
		protocol->sense_ctrl[1] = 0x0;

		protocol->sleep_ctrl[0] = 0x2;
		protocol->sleep_ctrl[1] = 0x0;

		protocol->glove_ctrl[0] = 0x6;
		protocol->glove_ctrl[1] = 0x0;

		protocol->stylus_ctrl[0] = 0x7;
		protocol->stylus_ctrl[1] = 0x0;

		protocol->tp_scan_mode[0] = 0x8;
		protocol->tp_scan_mode[1] = 0x0;

		protocol->lpwg_ctrl[0] = 0xA;
		protocol->lpwg_ctrl[1] = 0x0;

		protocol->gesture_ctrl[0] = 0xB;
		protocol->gesture_ctrl[1] = 0x3F;

		protocol->phone_cover_ctrl[0] = 0xC;
		protocol->phone_cover_ctrl[1] = 0x0;

		protocol->finger_sense_ctrl[0] = 0xF;
		protocol->finger_sense_ctrl[1] = 0x0;

		protocol->phone_cover_window[0] = 0xD;

		/* Non support on v5.0 */
		/* protocol->proximity_ctrl[0] = 0x10; */
		/* protocol->proximity_ctrl[1] = 0x0; */

		/* protocol->plug_ctrl[0] = 0x11; */
		/* protocol->plug_ctrl[1] = 0x0; */
	} else {
		protocol->func_ctrl_len = 3;

		protocol->sense_ctrl[0] = 0x1;
		protocol->sense_ctrl[1] = 0x1;
		protocol->sense_ctrl[2] = 0x0;

		protocol->sleep_ctrl[0] = 0x1;
		protocol->sleep_ctrl[1] = 0x2;
		protocol->sleep_ctrl[2] = 0x0;

		protocol->glove_ctrl[0] = 0x1;
		protocol->glove_ctrl[1] = 0x6;
		protocol->glove_ctrl[2] = 0x0;

		protocol->stylus_ctrl[0] = 0x1;
		protocol->stylus_ctrl[1] = 0x7;
		protocol->stylus_ctrl[2] = 0x0;

		protocol->tp_scan_mode[0] = 0x1;
		protocol->tp_scan_mode[1] = 0x8;
		protocol->tp_scan_mode[2] = 0x0;

		protocol->lpwg_ctrl[0] = 0x1;
		protocol->lpwg_ctrl[1] = 0xA;
		protocol->lpwg_ctrl[2] = 0x0;

		protocol->gesture_ctrl[0] = 0x1;
		protocol->gesture_ctrl[1] = 0xB;
		protocol->gesture_ctrl[2] = 0x3F;

		protocol->phone_cover_ctrl[0] = 0x1;
		protocol->phone_cover_ctrl[1] = 0xC;
		protocol->phone_cover_ctrl[2] = 0x0;

		protocol->finger_sense_ctrl[0] = 0x1;
		protocol->finger_sense_ctrl[1] = 0xF;
		protocol->finger_sense_ctrl[2] = 0x0;

		protocol->proximity_ctrl[0] = 0x1;
		protocol->proximity_ctrl[1] = 0x10;
		protocol->proximity_ctrl[2] = 0x0;

		protocol->plug_ctrl[0] = 0x1;
		protocol->plug_ctrl[1] = 0x11;
		protocol->plug_ctrl[2] = 0x0;

		protocol->phone_cover_window[0] = 0xE;
	}
	if (protocol->mid >= 0x3)
		protocol->fw_ver_len = 9;
	else
		protocol->fw_ver_len = 4;

	if (protocol->mid == 0x1) {
		protocol->pro_ver_len = 3;
	} else {
		protocol->pro_ver_len = 4;
	}

	protocol->tp_info_len = 14;
	protocol->key_info_len = 30;
	protocol->core_ver_len = 5;
	protocol->window_len = 8;

	/* The commadns about panel information */
	protocol->cmd_read_ctrl = P5_0_READ_DATA_CTRL;
	protocol->cmd_get_tp_info = P5_0_GET_TP_INFORMATION;
	protocol->cmd_get_key_info = P5_0_GET_KEY_INFORMATION;
	protocol->cmd_get_fw_ver = P5_0_GET_FIRMWARE_VERSION;
	protocol->cmd_get_pro_ver = P5_0_GET_PROTOCOL_VERSION;
	protocol->cmd_get_core_ver = P5_0_GET_CORE_VERSION;
	protocol->cmd_mode_ctrl = P5_0_MODE_CONTROL;
	protocol->cmd_cdc_busy = P5_0_CDC_BUSY_STATE;
	protocol->cmd_i2cuart = P5_0_I2C_UART;

	/* The commands about the packets of finger report from FW */
	protocol->unknown_mode = P5_0_FIRMWARE_UNKNOWN_MODE;
	protocol->demo_mode = P5_0_FIRMWARE_DEMO_MODE;
	protocol->debug_mode = P5_0_FIRMWARE_DEBUG_MODE;
	protocol->test_mode = P5_0_FIRMWARE_TEST_MODE;
	protocol->i2cuart_mode = P5_0_FIRMWARE_I2CUART_MODE;
	protocol->gesture_mode = P5_0_FIRMWARE_GESTURE_MODE;

	protocol->demo_pid = P5_0_DEMO_PACKET_ID;
	protocol->debug_pid = P5_0_DEBUG_PACKET_ID;
	protocol->test_pid = P5_0_TEST_PACKET_ID;
	protocol->i2cuart_pid = P5_0_I2CUART_PACKET_ID;
	protocol->ges_pid = P5_0_GESTURE_PACKET_ID;

	protocol->demo_len = P5_0_DEMO_MODE_PACKET_LENGTH;
	protocol->debug_len = P5_0_DEBUG_MODE_PACKET_LENGTH;
	protocol->test_len = P5_0_TEST_MODE_PACKET_LENGTH;

	/* The commands about MP test */
	protocol->cmd_cdc = P5_0_SET_CDC_INIT;
	protocol->cmd_get_cdc = P5_0_GET_CDC_DATA;

	if (protocol->mid < 4) {
		protocol->cdc_len = 3;
	} else {
		protocol->cdc_len = 15;
	}

	protocol->mutual_dac = 0x1;
	protocol->mutual_bg = 0x2;
	protocol->mutual_signal = 0x3;
	protocol->mutual_no_bk = 0x5;
	protocol->mutual_has_bk = 0x8;
	protocol->mutual_bk_dac = 0x10;

	protocol->self_dac = 0xC;
	protocol->self_bg = 0xF;
	protocol->self_signal = 0xD;
	protocol->self_no_bk = 0xE;
	protocol->self_has_bk = 0xB;
	protocol->self_bk_dac = 0x11;

	protocol->key_dac = 0x14;
	protocol->key_bg = 0x16;
	protocol->key_no_bk = 0x7;
	protocol->key_has_bk = 0x15;
	protocol->key_open = 0x12;
	protocol->key_short = 0x13;

	protocol->st_dac = 0x1A;
	protocol->st_bg = 0x1C;
	protocol->st_no_bk = 0x17;
	protocol->st_has_bk = 0x1B;
	protocol->st_open = 0x18;

	protocol->tx_short = 0x19;

	protocol->rx_short = 0x4;
	protocol->rx_open = 0x6;

	protocol->tx_rx_delta = 0x1E;

	protocol->cm_data = 0x9;
	protocol->cs_data = 0xA;

	protocol->trcrq_pin = 0x20;
	protocol->resx2_pin = 0x21;
	protocol->mutual_integra_time = 0x22;
	protocol->self_integra_time = 0x23;
	protocol->key_integra_time = 0x24;
	protocol->st_integra_time = 0x25;
	protocol->peak_to_peak = 0x1D;

	protocol->get_timing = 0x30;
	protocol->doze_p2p = 0x32;
	protocol->doze_raw = 0x33;
}

void core_protocol_func_control(int key, int ctrl)
{
	struct DataItem *tmp = search_func(key);

	if (tmp != NULL) {
		ipio_info("Found func's name: %s, key = %d\n", tmp->name, key);

		/* last element is used to control this func */
		if (tmp->key != 9)
			tmp->cmd[tmp->len - 1] = ctrl;

		core_write(core_config->slave_i2c_addr, tmp->cmd, tmp->len);
		return;
	}

	ipio_info("Can't find any main functions\n");
}
EXPORT_SYMBOL(core_protocol_func_control);

int core_protocol_update_ver(uint8_t major, uint8_t mid, uint8_t minor)
{
	int i = 0;
	struct protocol_sup_list pver[] = {
		{0x5, 0x0, 0x0},
		{0x5, 0x1, 0x0},
		{0x5, 0x2, 0x0},
		{0x5, 0x3, 0x0},
		{0x5, 0x4, 0x0},
	};

	for (i = 0 ; i < ARRAY_SIZE(pver); i++) {
		if (pver[i].major == major && pver[i].mid == mid && pver[i].minor == minor) {
			protocol->major = major;
			protocol->mid = mid;
			protocol->minor = minor;
			ipio_info("protocol: major = %d, mid = %d, minor = %d\n",
				 protocol->major, protocol->mid, protocol->minor);

			if (protocol->major == 0x5)
				config_protocol_v5_cmd();

			/* We need to recreate function controls because of the commands updated. */
			create_func_hash();
			return 0;
		}
	}

	ipio_err("Doesn't support this version of protocol\n");
	return ERROR;
}
EXPORT_SYMBOL(core_protocol_update_ver);

int core_protocol_init(void)
{
	if (protocol == NULL) {
		protocol = kzalloc(sizeof(*protocol), GFP_KERNEL);
		if (ERR_ALLOC_MEM(protocol)) {
			ipio_err("Failed to allocate protocol mem, %ld\n", PTR_ERR(protocol));
			core_protocol_remove();
			return -ENOMEM;
		}
	}

	/* The default version must only once be set up at this initial time. */
	core_protocol_update_ver(PROTOCOL_MAJOR, PROTOCOL_MID, PROTOCOL_MINOR);
	return 0;
}
EXPORT_SYMBOL(core_protocol_init);

void core_protocol_remove(void)
{
	ipio_info("Remove core-protocol memebers\n");
	ipio_kfree((void **)&protocol);
	free_func_hash();
}
EXPORT_SYMBOL(core_protocol_remove);
