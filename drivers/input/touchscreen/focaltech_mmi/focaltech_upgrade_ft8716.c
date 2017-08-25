/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2016, Focaltech Ltd. All rights reserved.
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


#include <linux/firmware.h>
#include "focaltech_flash.h"
#include "focaltech_upgrade_common.h"

#define APP_FILE_MAX_SIZE		(64 * 1024)
#define APP_FILE_MIN_SIZE		(8)
#define APP_FILE_VER_MAPPING		(0x10E)
#define APP_FILE_VENDORID_MAPPING	(0x10C)
#define APP_FILE_CHIPID_MAPPING		(0x11E)
#define CONFIG_START_ADDR		(0x0000)
#define CONFIG_VENDOR_ID_OFFSET		(0x04)
#define CONFIG_PROJECT_ID_OFFSET	(0x20)
#define CONFIG_VENDOR_ID_ADDR		(CONFIG_START_ADDR+ \
						CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR		(CONFIG_START_ADDR+ \
						CONFIG_PROJECT_ID_OFFSET)

#define FW_CFG_TOTAL_SIZE		0x00
#define APP1_START			0x00
#define APP1_LEN			0x100
#define APP_VERIF_ADDR			(APP1_START + APP1_LEN)
#define APP_VERIF_LEN			0x20
#define APP1_ECC_ADDR			(APP_VERIF_ADDR + APP_P1_ECC)
#define APP2_START			(APP_VERIF_ADDR + APP_VERIF_LEN + \
						FW_CFG_TOTAL_SIZE)
#define APP2_ECC_ADDR			(APP_VERIF_ADDR + APP_P2_ECC)

#define FT8716_PRAMBOOT_FW_NAME		"FT8716_Pramboot.bin"

static int fts_ctpm_get_app_bin_file_ver(const char *firmware_name);
static int fts_ctpm_fw_upgrade_with_app_bin_file(
			struct i2c_client *client, const char *firmware_name);

static struct fts_upgrade_fun fts_updatefun_8716 = {
	.get_app_bin_file_ver = fts_ctpm_get_app_bin_file_ver,
	.upgrade_with_app_bin_file = fts_ctpm_fw_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_bin_file = NULL,
};


static struct ft_chip_t fts_chip_type_8716 = {
	.type = 0x05,
	.chip_idh = 0x87,
	.chip_idl = 0x16,
	.rom_idh = 0x87,
	.rom_idl = 0x16,
	.pramboot_idh = 0x87,
	.pramboot_idl = 0xA6,
	.bootloader_idh = 0x00,
	.bootloader_idl = 0x00,
	.chip_type = 0x87160805,
};

inline void ft8716_set_upgrade_function(struct fts_upgrade_fun **curr)
{
	*curr = &fts_updatefun_8716;
}
inline void ft8716_set_chip_id(struct ft_chip_t **curr)
{
	*curr = &fts_chip_type_8716;
}

static int fts_ctpm_get_app_bin_file_ver(const char *firmware_name)
{
	int fw_ver = 0;
	int rc;
	const struct firmware *fw = NULL;

	FTS_FUNC_ENTER();

	rc = request_firmware(&fw, firmware_name, NULL);
	if (rc < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
							firmware_name, rc);
		return rc;
	}
	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] FW length(%zd) error\n", fw->size);
		return -EIO;
		goto file_ver_rel_fw;
	}

	fw_ver = fw->data[APP_FILE_VER_MAPPING];

file_ver_rel_fw:
	release_firmware(fw);
	FTS_FUNC_EXIT();

	return fw_ver;
}

static int fts_ctpm_get_vendor_id_flash(struct i2c_client *client)
{
	int i_ret = 0;
	u8 vendorid[4] = {0};
	u8 auc_i2c_write_buf[10];
	u8 i = 0;

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		auc_i2c_write_buf[0] = 0x03;
		auc_i2c_write_buf[1] = 0x00;

		auc_i2c_write_buf[2] = (u8)((CONFIG_VENDOR_ID_ADDR - 1) >> 8);
		auc_i2c_write_buf[3] = (u8)(CONFIG_VENDOR_ID_ADDR - 1);
		i_ret = fts_i2c_read(client, auc_i2c_write_buf, 4, vendorid, 2);
		if (i_ret < 0) {
			FTS_ERROR("[UPGRADE] read flash i_ret = %d\n", i_ret);
			continue;
		}
		break;
	}

	FTS_INFO("[UPGRADE] vendor id from flash rom: 0x%x\n", vendorid[1]);
	if (i >= FTS_UPGRADE_LOOP) {
		FTS_ERROR("[UPGRADE] read vendor id failed\n");
		return -EIO;
	}

	return 0;
}

static int fts_ctpm_write_pram(struct i2c_client *client,
					const char *pramfw_name)
{
	int i_ret;
	bool inrom = false;
	const struct firmware *pramfw = NULL;

	FTS_FUNC_ENTER();

	i_ret = request_firmware(&pramfw, pramfw_name, &client->dev);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
							pramfw_name, i_ret);
		return i_ret;
	}

	/*check the length of the pramboot*/
	if ((pramfw->size > APP_FILE_MAX_SIZE) ||
		(pramfw->size < APP_FILE_MIN_SIZE)) {
		FTS_ERROR("[UPGRADE] pramboot length(%zd) fail\n",
							pramfw->size);
		i_ret = -EINVAL;
		goto write_pram_rel_fw;
	}

	/*send command to FW, reset and start write pramboot*/
	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] send upgrade cmd to FW error\n");
		goto write_pram_rel_fw;
	}

	/*check run in rom or not! if run in rom, will write pramboot*/
	inrom = fts_ctpm_check_run_state(client, FTS_RUN_IN_ROM);
	if (!inrom) {
		FTS_ERROR("[UPGRADE] not run in rom, write pramboot fail\n");
		i_ret = -EIO;
		goto write_pram_rel_fw;
	}

	/*write pramboot to pram*/
	i_ret = fts_ctpm_write_pramboot_for_idc(client,
						pramfw->size, pramfw->data);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] write pramboot fail\n");
		goto write_pram_rel_fw;
	}

	/*read out checksum*/
	i_ret = fts_ctpm_pramboot_ecc(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] write pramboot ecc error\n");
		goto write_pram_rel_fw;
	}

	/*start pram*/
	fts_ctpm_start_pramboot(client);

	FTS_FUNC_EXIT();

write_pram_rel_fw:
	release_firmware(pramfw);
	return i_ret;
}

static int fts_ctpm_write_app(struct i2c_client *client,
					const u8 *pbt_buf, u32 dw_length)
{
	u32 temp;
	int i_ret;
	bool inpram = false;

	FTS_FUNC_ENTER();

	/*check run in pramboot or not if not rum in pramboot, can not upgrade*/
	inpram = fts_ctpm_check_run_state(client, FTS_RUN_IN_PRAM);
	if (!inpram) {
		FTS_ERROR("[UPGRADE] not run in pram, upgrade fail\n");
		return -EIO;
	}

	/*upgrade init*/
	i_ret = fts_ctpm_upgrade_idc_init(client, dw_length);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] upgrade init error, upgrade fail\n");
		return i_ret;
	}

	/*read vendor id from flash, if vendor id error, can not upgrade*/
	i_ret = fts_ctpm_get_vendor_id_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] read vendor id in flash fail\n");
		return i_ret;
	}

	/*erase the app erea in flash*/
	i_ret = fts_ctpm_erase_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] erase flash error\n");
		return i_ret;
	}

	/*start to write app*/
	i_ret = fts_ctpm_write_app_for_idc(client, dw_length, pbt_buf);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] write app error\n");
		return i_ret;
	}

	/*read check sum*/
	temp = 0x1000;
	i_ret = fts_ctpm_upgrade_ecc(client, temp, dw_length);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] ecc error\n");
		return i_ret;
	}

	FTS_FUNC_EXIT();

	return 0;
}

static int fts_ctpm_fw_upgrade_use_buf(struct i2c_client *client,
						const u8 *pbt_buf, u32 fwsize)
{
	int i_ret = 0;

	FTS_FUNC_ENTER();
	FTS_DEBUG("[UPGRADE] pramboot\n");
	i_ret = fts_ctpm_write_pram(client, FT8716_PRAMBOOT_FW_NAME);
	if (i_ret != 0) {
		FTS_ERROR("[UPGRADE] write pram failed\n");
		return -EIO;
	}

	i_ret =  fts_ctpm_write_app(client, pbt_buf, fwsize);

	FTS_FUNC_EXIT();

	return i_ret;
}

static int fts_ctpm_check_vendor_id(struct i2c_client *client,
						const u8 *data)
{
	u8 vendor_id;

	fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
	if ((vendor_id != 0x8D) &&
		(data[APP_FILE_VENDORID_MAPPING] != vendor_id)) {
		FTS_ERROR("[UPGRADE] vendor id(%02x)[%02x] is error\n",
			data[APP_FILE_VENDORID_MAPPING], vendor_id);
		return -EINVAL;
	}
	return 0;
}

static int fts_ctpm_check_chip_id(const u8 *data)
{
	if ((data[APP_FILE_CHIPID_MAPPING] !=
		fts_chip_type_8716.pramboot_idh) ||
		(data[APP_FILE_CHIPID_MAPPING + 1] !=
		fts_chip_type_8716.pramboot_idl)) {
		FTS_ERROR("[UPGRADE] chip id(%02X%02X)[%02X%02X] is error\n",
			data[APP_FILE_CHIPID_MAPPING],
			data[APP_FILE_CHIPID_MAPPING + 1],
			fts_chip_type_8716.pramboot_idh,
			fts_chip_type_8716.pramboot_idl);
		return -EINVAL;
	}
	return 0;
}

static bool fts_ctpm_check_app_bin_valid(const u8 *pbt_buf)
{
	u32 len;

	if (pbt_buf[0] != 0x02) {
		FTS_ERROR("[UPGRADE] APP.BIN Verify the first byte(%x) error\n",
			pbt_buf[0]);
		return -EINVAL;
	}

	if (!fts_ecc_check(pbt_buf, APP1_START, APP1_LEN, APP1_ECC_ADDR)) {
		FTS_ERROR("[UPGRADE] APP.BIN Verify- ecc1 error\n");
		return -EINVAL;
	}

	if ((fts_data_word(pbt_buf, APP_VERIF_ADDR + APP_LEN) +
		fts_data_word(pbt_buf, APP_VERIF_ADDR + APP_LEN_NE)) !=
		0xFFFF) {
		FTS_ERROR("[UPGRADE] APP.BIN Verify- Length XOR error\n");
		return -EINVAL;
	}

	len = fts_data_word(pbt_buf, APP_VERIF_ADDR + APP_LEN);
	FTS_DEBUG("%x %x %x %x", APP2_START, len,
		((u32)fts_data_word(pbt_buf, APP_VERIF_ADDR + APP_LEN_H) << 16),
		fts_data_word(pbt_buf, APP_VERIF_ADDR + APP_LEN));
	len -= APP2_START;
	if (!fts_ecc_check(pbt_buf, APP2_START, len, APP2_ECC_ADDR)) {
		FTS_ERROR("[UPGRADE] APP.BIN Verify- ecc2 error\n");
		return -EINVAL;
	}

	return 0;
}

static int fts_ctpm_fw_upgrade_with_app_bin_file
			(struct i2c_client *client, const char *firmware_name)
{
	int i_ret = 0;
	const struct firmware *fw = NULL;

	FTS_INFO("[UPGRADE] start upgrade with app.bin\n");

	i_ret = request_firmware(&fw, firmware_name, &client->dev);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
							firmware_name, i_ret);
		return i_ret;
	}

	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] FW length(%zd) error\n", fw->size);
		i_ret = -EINVAL;
		goto file_upgrade_rel_fw;
	}

	i_ret = fts_ctpm_check_vendor_id(client, fw->data);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] check vendor id failed\n");
		goto file_upgrade_rel_fw;
	}

	i_ret = fts_ctpm_check_chip_id(fw->data);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] check chip id failed\n");
		goto file_upgrade_rel_fw;
	}

	i_ret = fts_ctpm_check_app_bin_valid(fw->data);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] app.bin ecc failed\n");
		goto file_upgrade_rel_fw;
	}

	i_ret = fts_ctpm_fw_upgrade_use_buf(client, fw->data, fw->size);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] upgrade app.bin failed\n");
		goto file_upgrade_rel_fw;
	}

file_upgrade_rel_fw:
	release_firmware(fw);
	return i_ret;
}
