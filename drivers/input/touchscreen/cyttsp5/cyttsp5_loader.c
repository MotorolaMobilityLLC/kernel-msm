/*
 * cyttsp5_loader.c
 * Cypress TrueTouch(TM) Standard Product V5 FW Loader Module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/firmware.h>

/* cyttsp */
#include "cyttsp5_regs.h"


/*******************************************
 *
 * CYTTSP5 LOADER - Firmware Update
 *
 *******************************************/


#define CYTTSP5_LOADER_NAME "cyttsp5_loader"
#define CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW 1
#define CYTTSP5_LOADER_FW_UPGRADE_RETRY_COUNT 3

#define CYTTSP5_FW_UPGRADE \
	(defined(CYTTSP5_PLATFORM_FW_UPGRADE) \
	|| defined(CYTTSP5_BINARY_FW_UPGRADE))

static const u8 cyttsp5_security_key[] = {
	0xA5, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0x5A
};

#define CY_HW_VERSION 0x01
#define CY_CSP_FW_VERSION 0x0600
#define CY_QFN_FW_VERSION 0x1400
#define CY_FW_VERSION 0x2100

#ifdef CYTTSP5_PLATFORM_FW_UPGRADE
#include "cyttsp5_firmware.h"

static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = cyttsp5_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp5_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
	.hw_version = CY_HW_VERSION,
	.fw_version = CY_FW_VERSION,
};
#else
static struct cyttsp5_touch_firmware cyttsp5_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
	.hw_version = CY_HW_VERSION,
	.fw_version = CY_FW_VERSION,
};
#endif

static struct cyttsp5_touch_config cyttsp5_ttconfig = {
	.param_regs = NULL,
	.param_size = NULL,
	.fw_ver = NULL,
	.fw_vsize = 0,
};

struct cyttsp5_loader_platform_data _cyttsp5_loader_platform_data = {
	.fw = &cyttsp5_firmware,
	.ttconfig = &cyttsp5_ttconfig,
	.flags = CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE,
};

/* Timeout values in ms. */
#define CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT		500
#define CY_LDR_SWITCH_TO_APP_MODE_TIMEOUT		300

#define CY_MAX_STATUS_SIZE				32

#define CY_DATA_MAX_ROW_SIZE				256
#define CY_DATA_ROW_SIZE				128

#define CY_ARRAY_ID_OFFSET				0
#define CY_ROW_NUM_OFFSET				1
#define CY_ROW_SIZE_OFFSET				3
#define CY_ROW_DATA_OFFSET				5

#define CY_POST_TT_CFG_CRC_MASK				0x2

struct cyttsp5_loader_data {
	struct device *dev;
	struct cyttsp5_sysinfo *si;
	u8 status_buf[CY_MAX_STATUS_SIZE];
	struct completion int_running;
#ifdef CYTTSP5_BINARY_FW_UPGRADE
	struct completion binary_bin_fw_complete;
	int binary_bin_fw_status;
	bool is_force_upgrade;
#endif
	struct work_struct calibration_work;
	struct cyttsp5_loader_platform_data *loader_pdata;
	u32 fw_ver_bin;
};

struct cyttsp5_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

struct cyttsp5_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[CY_DATA_ROW_SIZE];
} __packed;

static struct cyttsp5_core_commands *cmd;

static inline struct cyttsp5_loader_data *cyttsp5_get_loader_data(
		struct device *dev)
{
	return cyttsp5_get_dynamic_data(dev, CY_MODULE_LOADER);
}

#if CYTTSP5_FW_UPGRADE
#ifndef SAMSUNG_TSP_INFO
/*
 * return code:
 * -1: Do not upgrade firmware
 *  0: Version info same, let caller decide
 *  1: Do a firmware upgrade
 */
static int cyttsp5_check_firmware_version(struct device *dev,
		u32 fw_ver_new, u32 fw_revctrl_new)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	u32 fw_ver_img;
	u32 fw_revctrl_img;

	fw_ver_img = ld->si->cydata.fw_ver_major << 8;
	fw_ver_img += ld->si->cydata.fw_ver_minor;

	dev_info(dev, "%s: img vers:0x%04X new vers:0x%04X\n", __func__,
			fw_ver_img, fw_ver_new);

	if (fw_ver_new > fw_ver_img) {
		dev_info(dev, "%s: Image is newer, will upgrade\n",
				__func__);
		return 1;
	}

	if (fw_ver_new < fw_ver_img) {
		dev_info(dev, "%s: Image is older, will NOT upgrade\n",
				__func__);
		return -EPERM;
	}

	fw_revctrl_img = ld->si->cydata.revctrl;

	dev_info(dev, "%s: img revctrl:0x%04X new revctrl:0x%04X\n",
			__func__, fw_revctrl_img, fw_revctrl_new);

	if (fw_revctrl_new > fw_revctrl_img) {
		dev_info(dev, "%s: Image is newer, will upgrade\n",
				__func__);
		return 1;
	}

	if (fw_revctrl_new < fw_revctrl_img) {
		dev_info(dev, "%s: Image is older, will NOT upgrade\n",
				__func__);
		return -EPERM;
	}

	return 0;
}
#endif
#endif /* CYTTSP5_FW_UPGRADE */


#if CYTTSP5_FW_UPGRADE
static u8 *cyttsp5_get_row_(struct device *dev, u8 *row_buf,
		u8 *image_buf, int size)
{
	memcpy(row_buf, image_buf, size);
	return image_buf + size;
}

static int cyttsp5_ldr_enter_(struct device *dev, struct cyttsp5_dev_id *dev_id)
{
	int rc;
	u8 return_data[8];
	u8 mode;

	dev_id->silicon_id = 0;
	dev_id->rev_id = 0;
	dev_id->bl_ver = 0;

	cmd->request_reset(dev);
	msleep(CY_LDR_SWITCH_TO_APP_MODE_TIMEOUT);

	rc = cmd->request_get_mode(dev, 0, &mode);
	if (rc < 0)
		return rc;

	if (mode == CY_MODE_UNKNOWN)
		return -EINVAL;

	if (mode == CY_MODE_OPERATIONAL) {
		rc = cmd->cmd->start_bl(dev, 0);
		if (rc < 0)
			return rc;
	}

	rc = cmd->cmd->get_bl_info(dev, 0, return_data);
	if (rc < 0)
		return rc;

	dev_id->silicon_id = get_unaligned_le32(&return_data[0]);
	dev_id->rev_id = return_data[4];
	dev_id->bl_ver = return_data[5] + (return_data[6] << 8)
		+ (return_data[7] << 16);

	return 0;
}

static int cyttsp5_ldr_init_(struct device *dev,
		struct cyttsp5_hex_image *row_image)
{
	return cmd->cmd->initiate_bl(dev, 0, 8, (u8 *)cyttsp5_security_key,
			row_image->row_size, row_image->row_data);
}

static int cyttsp5_ldr_parse_row_(struct device *dev, u8 *row_buf,
	struct cyttsp5_hex_image *row_image)
{
	int rc = 0;

	row_image->array_id = row_buf[CY_ARRAY_ID_OFFSET];
	row_image->row_num = get_unaligned_be16(&row_buf[CY_ROW_NUM_OFFSET]);
	row_image->row_size = get_unaligned_be16(&row_buf[CY_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		dev_err(dev, "%s: row data buffer overflow\n", __func__);
		rc = -EOVERFLOW;
		goto cyttsp5_ldr_parse_row_exit;
	}

	memcpy(row_image->row_data, &row_buf[CY_ROW_DATA_OFFSET],
	       row_image->row_size);
cyttsp5_ldr_parse_row_exit:
	return rc;
}

static int cyttsp5_ldr_prog_row_(struct device *dev,
				 struct cyttsp5_hex_image *row_image)
{
	u16 length = row_image->row_size + 3;
	u8 data[3 + row_image->row_size];
	u8 offset = 0;

	data[offset++] = row_image->array_id;
	data[offset++] = LOW_BYTE(row_image->row_num);
	data[offset++] = HI_BYTE(row_image->row_num);
	memcpy(data + 3, row_image->row_data, row_image->row_size);
	return cmd->cmd->prog_and_verify(dev, 0, length, data);
}

static int cyttsp5_ldr_verify_chksum_(struct device *dev)
{
	u8 result;
	int rc;

	rc = cmd->cmd->verify_app_integrity(dev, 0, &result);
	if (rc)
		return rc;

	/* fail */
	if (result == 0)
		return -EINVAL;

	return 0;
}

static int cyttsp5_ldr_exit_(struct device *dev)
{
	return cmd->cmd->launch_app(dev, 0);
}

static int cyttsp5_load_app_(struct device *dev, const u8 *fw, int fw_size)
{
	struct cyttsp5_dev_id *dev_id;
	struct cyttsp5_hex_image *row_image;
	u8 *row_buf;
	size_t image_rec_size;
	size_t row_buf_size = CY_DATA_MAX_ROW_SIZE;
	int row_count = 0;
	u8 *p;
	u8 *last_row;
	int rc;
	int rc_tmp;

	image_rec_size = sizeof(struct cyttsp5_hex_image);
	if (fw_size % image_rec_size != 0) {
		dev_err(dev, "%s: Firmware image is misaligned\n", __func__);
		rc = -EINVAL;
		goto _cyttsp5_load_app_error;
	}

	dev_info(dev, "%s: start load app\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "start load app");
#endif

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct cyttsp5_hex_image), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct cyttsp5_dev_id), GFP_KERNEL);
	if (!row_buf || !row_image || !dev_id) {
		dev_err(dev, "%s: Unable to alloc row buffers(%p %p %p)\n",
			__func__, row_buf, row_image, dev_id);
		rc = -ENOMEM;
		goto _cyttsp5_load_app_exit;
	}

	cmd->request_stop_wd(dev);

	dev_info(dev, "%s: Send BL Loader Enter\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Enter");
#endif
	rc = cyttsp5_ldr_enter_(dev, dev_id);
	if (rc) {
		dev_err(dev, "%s: Error cannot start Loader (ret=%d)\n",
			__func__, rc);
		goto _cyttsp5_load_app_exit;
	}
	dev_vdbg(dev, "%s: dev: silicon id=%08X rev=%02X bl=%08X\n",
		__func__, dev_id->silicon_id,
		dev_id->rev_id, dev_id->bl_ver);

	/* get last row */
	last_row = (u8 *)fw + fw_size - image_rec_size;
	cyttsp5_get_row_(dev, row_buf, last_row, image_rec_size);
	cyttsp5_ldr_parse_row_(dev, row_buf, row_image);

	/* initialise bootloader */
	rc = cyttsp5_ldr_init_(dev, row_image);
	if (rc) {
		dev_err(dev, "%s: Error cannot init Loader (ret=%d)\n",
			__func__, rc);
		goto _cyttsp5_load_app_exit;
	}

	dev_info(dev, "%s: Send BL Loader Blocks\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Blocks");
#endif
	p = (u8 *)fw;
	while (p < last_row) {
		/* Get row */
		dev_dbg(dev, "%s: read row=%d\n", __func__, ++row_count);
		memset(row_buf, 0, row_buf_size);
		p = cyttsp5_get_row_(dev, row_buf, p, image_rec_size);

		/* Parse row */
		dev_vdbg(dev, "%s: p=%p buf=%p buf[0]=%02X\n",
			__func__, p, row_buf, row_buf[0]);
		rc = cyttsp5_ldr_parse_row_(dev, row_buf, row_image);
		dev_vdbg(dev, "%s: array_id=%02X row_num=%04X(%d) row_size=%04X(%d)\n",
			__func__, row_image->array_id,
			row_image->row_num, row_image->row_num,
			row_image->row_size, row_image->row_size);
		if (rc) {
			dev_err(dev, "%s: Parse Row Error (a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
			goto _cyttsp5_load_app_exit;
		} else {
			dev_vdbg(dev, "%s: Parse Row (a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
		}

		/* program row */
		rc = cyttsp5_ldr_prog_row_(dev, row_image);
		if (rc) {
			dev_err(dev, "%s: Program Row Error (array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, rc);
			goto _cyttsp5_load_app_exit;
		}

		dev_vdbg(dev, "%s: array=%d row_cnt=%d row_num=%04X\n",
			__func__, row_image->array_id, row_count,
			row_image->row_num);
	}

	/* exit loader */
	dev_info(dev, "%s: Send BL Loader Terminate\n", __func__);
#ifdef TTHE_TUNER_SUPPORT
	cmd->request_tthe_print(dev, NULL, 0, "Send BL Loader Terminate");
#endif
	rc = cyttsp5_ldr_exit_(dev);
	if (rc) {
		dev_err(dev, "%s: Error on exit Loader (ret=%d)\n",
			__func__, rc);

		/* verify app checksum */
		rc_tmp = cyttsp5_ldr_verify_chksum_(dev);
		if (rc_tmp)
			dev_err(dev, "%s: ldr_verify_chksum fail r=%d\n",
				__func__, rc_tmp);
		else
			dev_info(dev, "%s: APP Checksum Verified\n", __func__);
	}

_cyttsp5_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(dev_id);
_cyttsp5_load_app_error:
	return rc;
}

static void cyttsp5_fw_calibrate(struct work_struct *calibration_work)
{
	struct cyttsp5_loader_data *ld = container_of(calibration_work,
			struct cyttsp5_loader_data, calibration_work);
	struct device *dev = ld->dev;
	u8 mode;
	int rc;

	rc = cmd->request_exclusive(dev, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		return;

	rc = cmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0)
		goto release;

	for (mode = 0; mode < 3; mode++) {
		rc = cmd->cmd->calibrate_idacs(dev, 0, mode);
		if (rc < 0)
			goto release;
	}

	rc = cmd->cmd->resume_scanning(dev, 0);
	if (rc < 0)
		goto release;

	dev_dbg(dev, "%s: Calibration Done\n", __func__);

release:
	cmd->release_exclusive(dev);
}

static int cyttsp5_fw_calibration_attention(struct device *dev)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	int rc = 0;

	dev_dbg(dev, "%s\n", __func__);

	schedule_work(&ld->calibration_work);

	cmd->unsubscribe_attention(dev, CY_ATTEN_STARTUP, CY_MODULE_LOADER,
		cyttsp5_fw_calibration_attention, 0);

	return rc;
}

static int cyttsp5_upgrade_firmware(struct device *dev, const u8 *fw_img,
		int fw_size)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	/*struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);*/
	int retry = CYTTSP5_LOADER_FW_UPGRADE_RETRY_COUNT;
	int rc;

	cd->stay_awake = true;
	/*pm_runtime_get_sync(dev);*/
	dev_dbg(dev, "%s: cmd->request_exclusive = %p\n",
					__func__, cmd->request_exclusive);

	rc = cmd->request_exclusive(dev, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		/*pm_runtime_put(dev);*/
		goto exit;
	}

	while (retry--) {
		rc = cyttsp5_load_app_(dev, fw_img, fw_size);
		if (rc < 0)
			dev_err(dev, "%s: Firmware update failed rc=%d, retry:%d\n",
				__func__, rc, retry);
		else
			break;
		msleep(20);
	}
	if (rc < 0) {
		dev_err(dev, "%s: Firmware update failed with error code %d\n",
			__func__, rc);
	} else/* if (ld->loader_pdata &&
			(ld->loader_pdata->flags
			 & CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE)) */{
		/* set up call back for startup */
		dev_vdbg(dev, "%s: Adding callback for calibration\n",
			__func__);
		rc = cmd->subscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_LOADER, cyttsp5_fw_calibration_attention, 0);
		if (rc) {
			dev_err(dev, "%s: Failed adding callback for calibration\n",
				__func__);
			dev_err(dev, "%s: No calibration will be performed\n",
				__func__);
			rc = 0;
		}
	}

	cmd->release_exclusive(dev);
	/*pm_runtime_put_sync(dev);*/

	cmd->request_restart(dev);
exit:
	cd->stay_awake = false;
	return rc;
}

static int cyttsp5_loader_attention(struct device *dev)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	complete(&ld->int_running);
	return 0;
}
#endif /* CYTTSP5_FW_UPGRADE */

#ifdef CYTTSP5_PLATFORM_FW_UPGRADE
static int cyttsp5_check_firmware_version_platform(struct device *dev,
		struct cyttsp5_touch_firmware *fw)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
#ifdef SAMSUNG_TSP_INFO
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(dev);

	if (!ld->si) {
		dev_info(dev, "%s: No firmware infomation found, device FW may be corrupted\n",
			__func__);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	if ((ld->si->cydata.post_code & CY_POST_TT_CFG_CRC_MASK) == 0) {
		dev_dbg(dev, "%s: POST, TT_CFG failed (0x%04X), will upgrade\n",
			__func__, ld->si->cydata.post_code);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}
	dev_dbg(dev, "%s: POST = 0x%04x\n", __func__, ld->si->cydata.post_code);

	if (!sti) {
		dev_info(dev, "%s: Samsung TSP Info Invalid\n",
			__func__);
		return 0;
	}

	dev_info(dev, "%s: phone hw ver=0x%02x, tsp hw ver=0x%02x\n",
		__func__, fw->hw_version, sti->hw_version);
	dev_info(dev, "%s: phone fw ver=0x%04x, tsp fw ver=0x%04x\n",
		__func__, fw->fw_version,
		get_unaligned_be16(&sti->fw_versionh));

	if (fw->hw_version != sti->hw_version)
		return 0;
	if (fw->fw_version > get_unaligned_be16(&sti->fw_versionh))
		return 1;

	dev_info(dev, "%s: upgrade not required\n", __func__);
#else
	u32 fw_ver_new;
	u32 fw_revctrl_new;
	int upgrade;

	if (!ld->si) {
		dev_info(dev, "%s: No firmware infomation found, device FW may be corrupted\n",
			__func__);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->ver + 2);
	/* 4 middle bytes are not used */
	fw_revctrl_new = get_unaligned_be32(fw->ver + 8);

	upgrade = cyttsp5_check_firmware_version(dev, fw_ver_new,
		fw_revctrl_new);

	if (upgrade > 0)
		return 1;
#endif
	return 0;
}

static int upgrade_firmware_from_platform(struct device *dev,
							bool forcedUpgrade)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	struct cyttsp5_touch_firmware *fw;
	int rc = -ENOSYS;
	int upgrade;

	dev_dbg(dev, "%s\n", __func__);

	if (!ld->loader_pdata) {
		dev_err(dev, "%s: No loader platform data\n", __func__);
		return rc;
	}

	fw = ld->loader_pdata->fw;
	if (!fw || !fw->img || !fw->size) {
		dev_err(dev, "%s: No platform firmware\n", __func__);
		return rc;
	}

	if (!fw->ver || !fw->vsize) {
		dev_err(dev, "%s: No platform firmware version\n",
			__func__);
		return rc;
	}

	upgrade = cyttsp5_check_firmware_version_platform(dev, fw);
	if (forcedUpgrade)
		dev_info(dev, "%s: forced upgrade\n", __func__);
	if (upgrade || forcedUpgrade)
		return cyttsp5_upgrade_firmware(dev, fw->img, fw->size);

	return 0;
}
#endif /* CYTTSP5_PLATFORM_FW_UPGRADE */

#ifdef CYTTSP5_BINARY_FW_UPGRADE
static void _cyttsp5_firmware_cont(const struct firmware *fw, void *context)
{
	struct device *dev = context;
	u8 header_size = 0;

	if (!fw)
		goto cyttsp5_firmware_cont_exit;

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: No firmware received\n", __func__);
		goto cyttsp5_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		dev_err(dev, "%s: Firmware format is invalid\n", __func__);
		goto cyttsp5_firmware_cont_release_exit;
	}

	cyttsp5_upgrade_firmware(dev, &(fw->data[header_size + 1]),
		fw->size - (header_size + 1));

cyttsp5_firmware_cont_release_exit:
	release_firmware(fw);

cyttsp5_firmware_cont_exit:
	return;
}

#ifdef SAMSUNG_TSP_INFO
static int cyttsp5_check_firmware_version_binary(struct device *dev,
		const struct firmware *fw)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(dev);
	u32 hw_ver_new = CY_HW_VERSION;
	u32 fw_ver_new = CY_FW_VERSION;

	if (cd->silicon_id == CSP_SILICON_ID) {
		fw_ver_new = CY_CSP_FW_VERSION;
		cyttsp5_firmware.fw_version = CY_CSP_FW_VERSION;
	} else {
		if (system_rev < 4) {
			fw_ver_new = CY_QFN_FW_VERSION;
			cyttsp5_firmware.fw_version = CY_QFN_FW_VERSION;
		}
	}

	if (!ld->si) {
		dev_info(dev, "%s: No firmware infomation found, device FW may be corrupted\n",
			__func__);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	if ((ld->si->cydata.post_code & CY_POST_TT_CFG_CRC_MASK) == 0) {
		dev_dbg(dev, "%s: POST, TT_CFG failed (0x%04X), will upgrade\n",
			__func__, ld->si->cydata.post_code);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}
	dev_dbg(dev, "%s: POST = 0x%04x\n", __func__, ld->si->cydata.post_code);

	if (!sti) {
		dev_info(dev, "%s: Samsung TSP Info Invalid\n",
			__func__);
		return 0;
	}

	dev_info(dev, "%s: phone hw ver=0x%02x, tsp hw ver=0x%02x\n",
		__func__, hw_ver_new, sti->hw_version);
	dev_info(dev, "%s: phone fw ver=0x%04x, tsp fw ver=0x%04x\n",
		__func__, fw_ver_new, get_unaligned_be16(&sti->fw_versionh));

	if (hw_ver_new != sti->hw_version)
		return 1;

	if (fw_ver_new > get_unaligned_be16(&sti->fw_versionh))
		return 1;

	dev_info(dev, "%s: upgrade not required\n", __func__);
	return 0;
}
#else
static int cyttsp5_check_firmware_version_binary(struct device *dev,
		const struct firmware *fw)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new;
	int upgrade;

	if (!ld->si) {
		dev_info(dev, "%s: No firmware infomation found, device FW may be corrupted\n",
			__func__);
		return CYTTSP5_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->data + 3);
	/* 4 middle bytes are not used */
	fw_revctrl_new = get_unaligned_be32(fw->data + 9);

	ld->fw_ver_bin = fw_ver_new;
	upgrade = cyttsp5_check_firmware_version(dev, fw_ver_new,
			fw_revctrl_new);

	if (upgrade > 0)
		return 1;

	return 0;
}
#endif

static void _cyttsp5_firmware_cont_binary(const struct firmware *fw,
		void *context)
{
	struct device *dev = context;
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	int upgrade;

	if (!fw) {
		dev_info(dev, "%s: No binary firmware\n", __func__);
		goto _cyttsp5_firmware_cont_binary_exit;
	}

	if (!fw->data || !fw->size) {
		dev_err(dev, "%s: Invalid binary firmware\n", __func__);
		goto _cyttsp5_firmware_cont_binary_exit;
	}

	upgrade = cyttsp5_check_firmware_version_binary(dev, fw);
	if (ld->is_force_upgrade)
		dev_info(dev, "%s: forced upgrade\n", __func__);
	if (upgrade || ld->is_force_upgrade) {
		_cyttsp5_firmware_cont(fw, dev);
		ld->binary_bin_fw_status = 0;
		ld->is_force_upgrade = false;
		complete(&ld->binary_bin_fw_complete);
		return;
	}

_cyttsp5_firmware_cont_binary_exit:
	release_firmware(fw);

	ld->binary_bin_fw_status = -EINVAL;
	ld->is_force_upgrade = false;
	complete(&ld->binary_bin_fw_complete);
}

static int upgrade_firmware_from_binary(struct device *dev,
							bool forcedUpgrade)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	int retval;

	dev_vdbg(dev, "%s: Enabling firmware class loader built-in\n",
		__func__);

	ld->is_force_upgrade = forcedUpgrade;

	dev_info(dev, "Request firmware %s\n", cd->fw_path);
	if (cd->fw_path != NULL)
		retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				cd->fw_path, dev, GFP_KERNEL, dev,
				_cyttsp5_firmware_cont_binary);
	else
		retval = -1;

	if (retval < 0) {
		dev_err(dev, "%s: Fail request firmware class file load: %s\n",
			__func__, cd->fw_path);
		return retval;
	}

	/* wait until FW binary upgrade finishes */
	wait_for_completion(&ld->binary_bin_fw_complete);

	return ld->binary_bin_fw_status;
}
#endif /* CYTTSP5_BINARY_FW_UPGRADE */

int upgrade_firmware_from_sdcard(struct device *dev,
	const u8 *fw_data, int fw_size)
{
	u8 header_size = 0;
	int rc = 0;

	if (!fw_data || !fw_size) {
		dev_err(dev, "%s: No firmware received\n", __func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: bin data[0~3]=0x%02x 0x%02x 0x%02x 0x%02x\n",
		__func__, fw_data[0], fw_data[1], fw_data[2], fw_data[3]);

	header_size = fw_data[0];
	dev_dbg(dev, "%s: header_size=0x%02x\n", __func__, header_size);
	dev_dbg(dev, "%s: fw_size=0x%08x\n", __func__, fw_size);
	if (header_size >= (fw_size + 1)) {
		dev_err(dev, "%s: Firmware format is invalid\n", __func__);
		return -EINVAL;
	}

	rc = cyttsp5_upgrade_firmware(dev, &(fw_data[header_size + 1]),
		fw_size - (header_size + 1));

	return rc;
}
EXPORT_SYMBOL(upgrade_firmware_from_sdcard);

static void cyttsp5_fw_and_config_upgrade(
		struct cyttsp5_loader_data *ld)
{
	struct device *dev = ld->dev;
	dev_dbg(dev, "%s:\n", __func__);

	ld->si = cmd->request_sysinfo(dev);
	if (!ld->si)
		dev_err(dev, "%s: Fail get sysinfo pointer from core\n",
			__func__);
#if !CYTTSP5_FW_UPGRADE
	dev_err(dev, "%s: No FW upgrade method selected!\n", __func__);
#endif

#ifdef CYTTSP5_PLATFORM_FW_UPGRADE
	if (!upgrade_firmware_from_platform(dev, false))
		return;
#endif
#ifdef CYTTSP5_BINARY_FW_UPGRADE
	if (!upgrade_firmware_from_binary(dev, false))
		return;
#endif
}

int cyttsp5_loader_probe(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_loader_data *ld;
	struct cyttsp5_platform_data *pdata = dev_get_platdata(dev);
	int rc;

	dev_dbg(dev, "%s:\n", __func__);

	cmd = cyttsp5_get_commands();
	if (!cmd) {
		dev_err(dev, "%s: cmd invalid\n", __func__);
		return -EINVAL;
	}
	if (!pdata || !pdata->loader_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	ld = kzalloc(sizeof(*ld), GFP_KERNEL);
	if (!ld) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	ld->loader_pdata = pdata->loader_pdata;
	ld->dev = dev;
	cd->cyttsp5_dynamic_data[CY_MODULE_LOADER] = ld;

	cyttsp5_set_upgrade_firmware_from_builtin(dev,
#if defined(CYTTSP5_PLATFORM_FW_UPGRADE)
			upgrade_firmware_from_platform);
#elif defined(CYTTSP5_BINARY_FW_UPGRADE)
			upgrade_firmware_from_binary);
#else
			NULL);
#endif

#if CYTTSP5_FW_UPGRADE
	init_completion(&ld->int_running);
#ifdef CYTTSP5_BINARY_FW_UPGRADE
	init_completion(&ld->binary_bin_fw_complete);
#endif
	cmd->subscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_LOADER,
		cyttsp5_loader_attention, CY_MODE_BOOTLOADER);

	INIT_WORK(&ld->calibration_work, cyttsp5_fw_calibrate);
#endif
	cyttsp5_fw_and_config_upgrade(ld);

	dev_dbg(dev, "%s: Successful probe %s\n", __func__, dev_name(dev));
	return 0;

	kfree(ld);
error_alloc_data_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}
EXPORT_SYMBOL(cyttsp5_loader_probe);

int cyttsp5_loader_release(struct device *dev)
{
	struct cyttsp5_loader_data *ld = cyttsp5_get_loader_data(dev);
	int rc = 0;

#if CYTTSP5_FW_UPGRADE
	rc = cmd->unsubscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_LOADER,
		cyttsp5_loader_attention, CY_MODE_BOOTLOADER);
	if (rc < 0)
		dev_err(dev, "%s: Failed to restart IC with error code %d\n",
			__func__, rc);
#endif
	kfree(ld);
	return rc;
}
EXPORT_SYMBOL(cyttsp5_loader_release);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product Loader Driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");

