/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/firmware.h>
#include "ol_if_athvar.h"
#include "qdf_time.h"
#include "targaddrs.h"
#include "ol_cfg.h"
#include "cds_api.h"
#include "wma_api.h"
#include "wma.h"
#include "bin_sig.h"
#include "i_ar6320v2_regtable.h"
#include "epping_main.h"
#ifdef HIF_PCI
#include "ce_reg.h"
#endif
#if defined(HIF_SDIO)
#include "if_sdio.h"
#include "regtable_sdio.h"
#endif
#if defined(HIF_USB)
#include "if_usb.h"
#include "regtable_usb.h"
#endif
#include "pld_common.h"
#include "hif_main.h"

#include "i_bmi.h"
#include "qwlan_version.h"
#include "wlan_policy_mgr_api.h"
#include "dbglog_host.h"

#ifdef FEATURE_SECURE_FIRMWARE
static struct hash_fw fw_hash;
#endif

static uint32_t refclk_speed_to_hz[] = {
	48000000,               /* SOC_REFCLK_48_MHZ */
	19200000,               /* SOC_REFCLK_19_2_MHZ */
	24000000,               /* SOC_REFCLK_24_MHZ */
	26000000,               /* SOC_REFCLK_26_MHZ */
	37400000,               /* SOC_REFCLK_37_4_MHZ */
	38400000,               /* SOC_REFCLK_38_4_MHZ */
	40000000,               /* SOC_REFCLK_40_MHZ */
	52000000,               /* SOC_REFCLK_52_MHZ */
};

static int ol_target_coredump(void *inst, void *memory_block,
					uint32_t block_len);

#ifdef FEATURE_SECURE_FIRMWARE
static int ol_check_fw_hash(struct device *dev, const u8 *data,
			    u32 fw_size, enum ATH_BIN_FILE file)
{
	u8 *hash = NULL;
	u8 *fw_mem = NULL;
	u8 digest[SHA256_DIGEST_SIZE];
	u8 temp[SHA256_DIGEST_SIZE] = { };
	int ret = 0;

	switch (file) {
	case ATH_BOARD_DATA_FILE:
		hash = fw_hash.bdwlan;
		break;
	case ATH_OTP_FILE:
		hash = fw_hash.otp;
		break;
	case ATH_FIRMWARE_FILE:
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
			hash = fw_hash.utf;
			break;
		}
#endif
		hash = fw_hash.qwlan;
	default:
		break;
	}

	if (!hash) {
		BMI_INFO("No entry for file:%d Download FW in non-secure mode",
									file);
		goto end;
	}

	if (qdf_mem_cmp(hash, temp, SHA256_DIGEST_SIZE)) {
		BMI_INFO("Download FW in non-secure mode:%d", file);
		goto end;
	}

	fw_mem = pld_get_fw_ptr(dev);
	if (!fw_mem || (fw_size > MAX_FIRMWARE_SIZE)) {
		BMI_ERR("No Memory to copy FW data");
		ret = -1;
		goto end;
	}
	qdf_mem_copy(fw_mem, data, fw_size);

	ret = pld_get_sha_hash(dev, fw_mem, fw_size, "sha256", digest);

	if (ret) {
		BMI_ERR("Sha256 Hash computation failed err:%d", ret);
		goto end;
	}

	if (qdf_mem_cmp(hash, digest, SHA256_DIGEST_SIZE)) {
		BMI_ERR("Hash Mismatch");
		qdf_trace_hex_dump(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_FATAL,
				   digest, SHA256_DIGEST_SIZE);
		qdf_trace_hex_dump(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_FATAL,
				   hash, SHA256_DIGEST_SIZE);
		ret = QDF_STATUS_E_FAILURE;
	}
end:
	return ret;
}
#endif

/**
 * ol_board_id_to_filename() - Auto BDF board_id to filename conversion
 * @old_name: name of the default board data file
 * @board_id: board ID
 *
 * The API return board filename based on the board_id and chip_id.
 * eg: input = "bdwlan30.bin", board_id = 0x01, board_file = "bdwlan30.b01"
 * Return: The buffer with the formated board filename.
 */
static char *ol_board_id_to_filename(const char *old_name,
				     uint16_t board_id)
{
	int name_len;
	char *new_name;

	name_len = strlen(old_name);
	new_name = qdf_mem_malloc(name_len + 1);

	if (!new_name)
		goto out;

	if (board_id > 0xFF)
		board_id = 0x0;

	qdf_mem_copy(new_name, old_name, name_len);
	snprintf(&new_name[name_len - 2], 3, "%.2x", board_id);
out:
	return new_name;
}

#ifdef QCA_SIGNED_SPLIT_BINARY_SUPPORT
#define SIGNED_SPLIT_BINARY_VALUE true
#else
#define SIGNED_SPLIT_BINARY_VALUE false
#endif

static int
__ol_transfer_bin_file(struct ol_context *ol_ctx, enum ATH_BIN_FILE file,
		       uint32_t address, bool compressed)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	int status = 0;
	const char *filename;
	const struct firmware *fw_entry;
	uint32_t fw_entry_size;
	uint8_t *temp_eeprom;
	uint32_t board_data_size;
	bool bin_sign = false;
	int bin_off, bin_len;
	SIGN_HEADER_T *sign_header;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_type = tgt_info->target_type;
	struct bmi_info *bmi_ctx = GET_BMI_CONTEXT(ol_ctx);
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;
	int i;

	/*
	 * If there is no board data file bases on board id, the default
	 * board data file should be used.
	 * For factory mode, the sequence for file selection should be
	 * utfbd.board_id -> utfbd.bin -> bd.board_id -> bd.bin. So we
	 * need to cache 4 file names.
	 */
	uint32_t bd_files = 1;
	char *bd_id_filename[2] = {NULL, NULL};
	const char *bd_filename[2] = {NULL, NULL};

	switch (file) {
	default:
		BMI_ERR("%s: Unknown file type", __func__);
		return -EINVAL;
	case ATH_OTP_FILE:
		filename = bmi_ctx->fw_files.otp_data;
		if (SIGNED_SPLIT_BINARY_VALUE)
			bin_sign = true;

		break;
	case ATH_FIRMWARE_FILE:
		if (QDF_IS_EPPING_ENABLED(cds_get_conparam())) {
			filename = bmi_ctx->fw_files.epping_file;
			BMI_INFO("%s: Loading epping firmware file %s",
						__func__, filename);
			break;
		}
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
			filename = bmi_ctx->fw_files.utf_file;
			if (SIGNED_SPLIT_BINARY_VALUE)
				bin_sign = true;
			BMI_INFO("%s: Loading firmware file %s",
						__func__, filename);
			break;
		}
#endif
		if (cds_get_conparam() == QDF_GLOBAL_IBSS_MODE &&
		    (bmi_ctx->fw_files.ibss_image_file[0] != '\0')) {
			filename = bmi_ctx->fw_files.ibss_image_file;
		} else {
			filename = bmi_ctx->fw_files.image_file;
		}

		if (SIGNED_SPLIT_BINARY_VALUE)
			bin_sign = true;
		break;
	case ATH_PATCH_FILE:
		BMI_INFO("%s: no Patch file defined", __func__);
		return 0;
	case ATH_BOARD_DATA_FILE:
		filename = bmi_ctx->fw_files.board_data;
#ifdef QCA_WIFI_FTM
		if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
			filename = bmi_ctx->fw_files.utf_board_data;
			if (SIGNED_SPLIT_BINARY_VALUE)
				bin_sign = true;

			BMI_INFO("%s: Loading board data file %s",
						__func__, filename);

			/*
			 * In FTM mode, if utf files do not exit.
			 * bdwlan should be used.
			 */
			bd_files = 2;
		}
#endif /* QCA_WIFI_FTM */
		if (SIGNED_SPLIT_BINARY_VALUE)
			bin_sign = false;

		bd_filename[0] = filename;

		/*
		 * For factory mode, we should cache 2 group of file names.
		 * For mission mode, bd_files==1, only one group of file names.
		 */
		bd_filename[bd_files - 1] =
					bmi_ctx->fw_files.board_data;
		for (i = 0; i < bd_files; i++) {
			bd_id_filename[i] =
				ol_board_id_to_filename(bd_filename[i],
							bmi_ctx->board_id);
			if (bd_id_filename[i]) {
				BMI_INFO("%s: board data file is %s",
					 __func__, bd_id_filename[i]);
			} else {
				BMI_ERR("%s: Fail to allocate board filename",
					__func__);
			}
		}
		break;
	case ATH_SETUP_FILE:
		if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE &&
		    !QDF_IS_EPPING_ENABLED(cds_get_conparam())) {
			filename = bmi_ctx->fw_files.setup_file;
			if (filename[0] == 0) {
				BMI_INFO("%s: no Setup file defined", __func__);
				return -EPERM;
			}

			if (SIGNED_SPLIT_BINARY_VALUE)
				bin_sign = true;

			BMI_INFO("%s: Loading setup file %s",
			       __func__, filename);
		} else {
			BMI_INFO("%s: no Setup file needed", __func__);
			return -EPERM;
		}
		break;
	}

	/* For FTM mode. bd.bin is used if there is no utf.bin */
	if (file == ATH_BOARD_DATA_FILE) {
		for (i = 0; i < bd_files; i++) {
			if (bd_id_filename[i]) {
				BMI_DBG("%s: Trying to load %s",
					 __func__, bd_id_filename[i]);
				status = request_firmware(&fw_entry,
							  bd_id_filename[i],
							  qdf_dev->dev);
				if (!status)
					break;
				BMI_ERR("%s: Failed to get %s:%d",
					__func__, bd_id_filename[i],
					status);
			}

			/* bd.board_id not exits, using bd.bin */
			BMI_DBG("%s: Trying to load default %s",
				 __func__, bd_filename[i]);
			status = request_firmware(&fw_entry, bd_filename[i],
						  qdf_dev->dev);
			if (!status)
				break;
			BMI_ERR("%s: Failed to get default %s:%d",
				__func__, bd_filename[i], status);
		}
	} else {
		status = request_firmware(&fw_entry, filename, qdf_dev->dev);
	}

	if (status) {
		BMI_ERR("%s: Failed to get %s", __func__, filename);
		status = -ENOENT;
		goto release_fw;
	}

	if (!fw_entry || !fw_entry->data) {
		BMI_ERR("Invalid fw_entries");
		status = -ENOENT;
		goto release_fw;
	}

	fw_entry_size = fw_entry->size;
	temp_eeprom = NULL;

#ifdef FEATURE_SECURE_FIRMWARE
	if (ol_check_fw_hash(qdf_dev->dev, fw_entry->data,
			     fw_entry_size, file)) {
		BMI_ERR("Hash Check failed for file:%s", filename);
		status = -EINVAL;
		goto end;
	}
#endif

	if (file == ATH_BOARD_DATA_FILE) {
		uint32_t board_ext_address = 0;
		int32_t board_ext_data_size;

		temp_eeprom = qdf_mem_malloc(fw_entry_size);
		if (!temp_eeprom) {
			status = -ENOMEM;
			goto release_fw;
		}

		qdf_mem_copy(temp_eeprom, (uint8_t *) fw_entry->data,
			  fw_entry_size);

		switch (target_type) {
		case TARGET_TYPE_AR6004:
			board_data_size = AR6004_BOARD_DATA_SZ;
			board_ext_data_size = AR6004_BOARD_EXT_DATA_SZ;
			break;
		case TARGET_TYPE_AR9888:
			board_data_size = AR9888_BOARD_DATA_SZ;
			board_ext_data_size = AR9888_BOARD_EXT_DATA_SZ;
			break;
		default:
			board_data_size = 0;
			board_ext_data_size = 0;
			break;
		}

		/* Determine where in Target RAM to write Board Data */
		bmi_read_memory(HOST_INTEREST_ITEM_ADDRESS(target_type,
							   hi_board_ext_data),
				(uint8_t *) &board_ext_address, 4, ol_ctx);
		BMI_INFO("Board extended Data download address: 0x%x",
		       board_ext_address);

		/* Check whether the target has allocated memory for extended
		 * board data and file contains extended board data
		 */

		if ((board_ext_address)
		    && (fw_entry_size ==
			(board_data_size + board_ext_data_size))) {
			uint32_t param;

			status = bmi_write_memory(board_ext_address,
					(uint8_t *)(temp_eeprom +
					board_data_size),
					board_ext_data_size, ol_ctx);

			if (status)
				goto end;

			/* Record extended board Data initialized */
			param = (board_ext_data_size << 16) | 1;
			bmi_write_memory(
				HOST_INTEREST_ITEM_ADDRESS(target_type,
					hi_board_ext_data_config),
					(uint8_t *)&param, 4, ol_ctx);

			fw_entry_size = board_data_size;
		}
	}

	if (bin_sign && SIGNED_SPLIT_BINARY_VALUE) {
		uint32_t chip_id;

		if (fw_entry_size < sizeof(SIGN_HEADER_T)) {
			BMI_ERR("Invalid binary size %d", fw_entry_size);
			status = -EINVAL;
			goto end;
		}

		sign_header = (SIGN_HEADER_T *) fw_entry->data;
		chip_id = cpu_to_le32(sign_header->product_id);
		if (sign_header->magic_num == SIGN_HEADER_MAGIC
		    && (chip_id == AR6320_REV1_1_VERSION
			|| chip_id == AR6320_REV1_3_VERSION
			|| chip_id == AR6320_REV2_1_VERSION)) {

			bin_off = sizeof(SIGN_HEADER_T);
			status = bmi_sign_stream_start(address,
						(uint8_t *)fw_entry->data,
						bin_off, ol_ctx);
			if (status) {
				BMI_ERR("unable to start sign stream");
				status = -EINVAL;
				goto end;
			}

			bin_len = sign_header->rampatch_len - bin_off;
			if (bin_len <= 0 || bin_len > fw_entry_size - bin_off) {
				BMI_ERR("Invalid sign header");
				status = -EINVAL;
				goto end;
			}
		} else {
			bin_sign = false;
			bin_off = 0;
			bin_len = fw_entry_size;
		}
	} else {
		bin_len = fw_entry_size;
		bin_off = 0;
	}

	if (compressed) {
		status = bmi_fast_download(address,
					   (uint8_t *) fw_entry->data + bin_off,
					   bin_len, ol_ctx);
	} else {
		if (file == ATH_BOARD_DATA_FILE && fw_entry->data) {
			status = bmi_write_memory(address,
						  (uint8_t *) temp_eeprom,
						  fw_entry_size, ol_ctx);
		} else {
			status = bmi_write_memory(address,
						  (uint8_t *) fw_entry->data
						  + bin_off, bin_len, ol_ctx);
		}
	}

	if (bin_sign && SIGNED_SPLIT_BINARY_VALUE) {
		bin_off += bin_len;
		bin_len = sign_header->total_len - sign_header->rampatch_len;

		if (bin_len > 0 && bin_len <= fw_entry_size - bin_off) {
			status = bmi_sign_stream_start(0,
					(uint8_t *)fw_entry->data +
					bin_off, bin_len, ol_ctx);
			if (status)
				BMI_ERR("sign stream error");
		}
	}

end:
	if (temp_eeprom)
		qdf_mem_free(temp_eeprom);

release_fw:
	if (fw_entry)
		release_firmware(fw_entry);

	for (i = 0; i < bd_files; i++) {
		if (bd_id_filename[i]) {
			qdf_mem_free(bd_id_filename[i]);
			bd_id_filename[i] = NULL;
		}
	}

	if (status)
		BMI_ERR("%s, BMI operation failed: %d", __func__, __LINE__);
	else
		BMI_INFO("transferring file: %s size %d bytes done!",
			 (filename) ? filename : " ", fw_entry_size);
	return status;
}

static int
ol_transfer_bin_file(struct ol_context *ol_ctx, enum ATH_BIN_FILE file,
		     uint32_t address, bool compressed)
{
	int ret;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	/* Wait until suspend and resume are completed before loading FW */
	pld_lock_pm_sem(qdf_dev->dev);

	ret = __ol_transfer_bin_file(ol_ctx, file, address, compressed);

	pld_release_pm_sem(qdf_dev->dev);

	return ret;
}

/**
 * struct ramdump_info: Structure to hold ramdump information
 * @base: Base address for Ramdump collection
 * @size: Size of the dump
 *
 * Ramdump information.
 */
struct ramdump_info {
	void *base;
	unsigned long size;
};

/**
 * if have platform driver support, reinit will be called by CNSS.
 * recovery flag will be cleaned and CRASHED indication will be sent
 * to user space by reinit function. If not support, clean recovery
 * flag and send CRASHED indication in CLD driver.
 */
static inline void ol_check_clean_recovery_flag(struct ol_context *ol_ctx)
{
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	if (!pld_have_platform_driver_support(qdf_dev->dev)) {
		cds_set_recovery_in_progress(false);
		if (ol_ctx->fw_crashed_cb)
			ol_ctx->fw_crashed_cb();
	}
}

#if !defined(QCA_WIFI_3_0)
static inline void ol_get_ramdump_mem(struct device *dev,
				      struct ramdump_info *info)
{
	info->base = pld_get_virt_ramdump_mem(dev, &info->size);
}

static inline void ol_release_ramdump_mem(struct device *dev,
					  struct ramdump_info *info)
{
	pld_release_virt_ramdump_mem(dev, info->base);
}
#else
static inline void ol_get_ramdump_mem(struct device *dev,
				      struct ramdump_info *info) { }
static inline void ol_release_ramdump_mem(struct device *dev,
					  struct ramdump_info *info) { }
#endif

int ol_copy_ramdump(struct hif_opaque_softc *scn)
{
	int ret = -1;
	struct ramdump_info *info;
	qdf_device_t qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_dev) {
		BMI_ERR("%s qdf_dev is NULL", __func__);
		return -EINVAL;
	}
	if (pld_is_fw_dump_skipped(qdf_dev->dev)) {
		BMI_INFO("%s ssr enabled, skip ramdump", __func__);
		return 0;
	}
	info = qdf_mem_malloc(sizeof(struct ramdump_info));
	if (!info)
		return -ENOMEM;

	ol_get_ramdump_mem(qdf_dev->dev, info);

	if (!info->base || !info->size) {
		BMI_ERR("%s:ramdump collection fail", __func__);
		qdf_mem_free(info);
		return -EACCES;
	}

	ret = ol_target_coredump(scn, info->base, info->size);

	ol_release_ramdump_mem(qdf_dev->dev, info);
	qdf_mem_free(info);
	return ret;
}

static void __ramdump_work_handler(void *data)
{
	int ret;
	uint32_t host_interest_address;
	uint32_t dram_dump_values[4];
	uint32_t target_type;
	struct hif_target_info *tgt_info;
	struct ol_context *ol_ctx = data;
	struct hif_opaque_softc *ramdump_scn = ol_ctx->scn;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;
	struct ol_config_info *ini_cfg = ol_get_ini_handle(ol_ctx);

	if (!ramdump_scn) {
		BMI_ERR("%s:Ramdump_scn is null:", __func__);
		goto out_fail;
	}
	tgt_info = hif_get_target_info_handle(ramdump_scn);
	target_type = tgt_info->target_type;
#ifdef WLAN_DEBUG
	ret = hif_check_soc_status(ramdump_scn);
	if (ret)
		goto out_fail;

	ret = hif_dump_registers(ramdump_scn);
	if (ret)
		goto out_fail;

#endif

	if (hif_diag_read_mem(ramdump_scn,
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_failure_state)),
			(uint8_t *)&host_interest_address,
			sizeof(uint32_t)) != QDF_STATUS_SUCCESS) {
		BMI_ERR("HifDiagReadiMem FW Dump Area Pointer failed!");
		ol_copy_ramdump(ramdump_scn);
		pld_device_crashed(qdf_dev->dev);
		ol_check_clean_recovery_flag(ol_ctx);

		return;
	}

	BMI_ERR("Host interest item address: 0x%08x", host_interest_address);

	if (hif_diag_read_mem(ramdump_scn, host_interest_address,
			      (uint8_t *) &dram_dump_values[0],
			      4 * sizeof(uint32_t)) != QDF_STATUS_SUCCESS) {
		BMI_ERR("HifDiagReadiMem FW Dump Area failed!");
		goto out_fail;
	}
	BMI_ERR("FW Assertion at PC: 0x%08x BadVA: 0x%08x TargetID: 0x%08x",
	       dram_dump_values[2], dram_dump_values[3], dram_dump_values[0]);

	if (ol_copy_ramdump(ramdump_scn))
		goto out_fail;

	BMI_ERR("%s: RAM dump collecting completed!", __func__);

	/*
	 * if unloading is in progress, then skip SSR,
	 * otherwise notify SSR framework the target has crashed.
	 */
	if (cds_is_load_or_unload_in_progress())
		cds_set_recovery_in_progress(false);
	else {
		pld_device_crashed(qdf_dev->dev);
		ol_check_clean_recovery_flag(ol_ctx);
	}
	return;

out_fail:
	/* Silent SSR on dump failure */
	if (ini_cfg->enable_self_recovery)
		pld_device_self_recovery(qdf_dev->dev,
					 PLD_REASON_DEFAULT);
	else
		pld_device_crashed(qdf_dev->dev);

	ol_check_clean_recovery_flag(ol_ctx);
}

void ramdump_work_handler(void *data)
{
	struct qdf_op_sync *op_sync;

	if (qdf_op_protect(&op_sync))
		return;

	__ramdump_work_handler(data);

	qdf_op_unprotect(op_sync);
}

void fw_indication_work_handler(void *data)
{
	struct ol_context *ol_ctx = data;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	pld_device_self_recovery(qdf_dev->dev,
				 PLD_REASON_DEFAULT);

	ol_check_clean_recovery_flag(ol_ctx);
}

void ol_target_failure(void *instance, QDF_STATUS status)
{
	struct ol_context *ol_ctx = instance;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct ol_config_info *ini_cfg = ol_get_ini_handle(ol_ctx);
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;
	int ret;
	bool skip_recovering_check = false;
	enum hif_target_status target_status = hif_get_target_status(scn);

	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_SNOC) {
		BMI_ERR("SNOC doesn't suppor this code path!");
		return;
	}

	/* If Host driver trigger target failure, skip recovering check */
	if (cds_is_target_asserting())
		skip_recovering_check = true;

	qdf_event_set(&wma->recovery_event);

	if (TARGET_STATUS_RESET == target_status) {
		BMI_ERR("Target is already asserted, ignore!");
		return;
	}

	hif_set_target_status(scn, TARGET_STATUS_RESET);

	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_USB) {
		if (status == QDF_STATUS_E_USB_ERROR)
			hif_ramdump_handler(scn);
		return;
	}

	if (!skip_recovering_check && cds_is_driver_recovering()) {
		BMI_ERR("%s: Recovery in progress, ignore!\n", __func__);
		return;
	}

	if (cds_is_driver_in_bad_state()) {
		BMI_ERR("%s: Driver in bad state, ignore!\n", __func__);
		return;
	}

	if (cds_is_load_or_unload_in_progress()) {
		BMI_ERR("%s: Loading/Unloading is in progress, ignore!",
		       __func__);
		return;
	}
	cds_set_target_ready(false);
	cds_set_recovery_in_progress(true);

	ret = hif_check_fw_reg(scn);
	if (0 == ret) {
		if (ini_cfg->enable_self_recovery) {
			qdf_sched_work(0, &ol_ctx->fw_indication_work);
			return;
		}
	} else if (-1 == ret) {
		return;
	}

	BMI_ERR("XXX TARGET ASSERTED XXX");

	cds_svc_fw_shutdown_ind(qdf_dev->dev);
	/* Collect the RAM dump through a workqueue */
	if (ini_cfg->enable_ramdump_collection)
		qdf_sched_work(0, &ol_ctx->ramdump_work);
	else
		pr_debug("%s: athdiag read for target reg\n", __func__);
}

#ifdef CONFIG_DISABLE_CDC_MAX_PERF_WAR
static QDF_STATUS ol_disable_cdc_max_perf(struct ol_context *ol_ctx)
{
	uint32_t param;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_type = tgt_info->target_type;

	/* set the firmware to disable CDC max perf WAR */
		if (bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *) &param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("BMI READ for setting cdc max perf failed");
			return QDF_STATUS_E_FAILURE;
		}

		param |= HI_OPTION_DISABLE_CDC_MAX_PERF_WAR;
		if (bmi_write_memory(
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("setting cdc max perf failed");
			return QDF_STATUS_E_FAILURE;
		}

	return QDF_STATUS_SUCCESS;
}

#else
static QDF_STATUS ol_disable_cdc_max_perf(struct ol_context *ol_ctx)
{
	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_LPSS
static QDF_STATUS ol_set_lpass_support(struct ol_context *ol_ctx)
{
	uint32_t param;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	struct ol_config_info *ini_cfg = ol_get_ini_handle(ol_ctx);
	uint32_t target_type = tgt_info->target_type;

	if (ini_cfg->enable_lpass_support) {
		if (bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *) &param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("BMI READ:Setting LPASS Support failed");
			return QDF_STATUS_E_FAILURE;
		}

		param |= HI_OPTION_DBUART_SUPPORT;
		if (bmi_write_memory(
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag2)),
			(uint8_t *)&param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("BMI_READ for setting LPASS Support fail");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

#else
static QDF_STATUS ol_set_lpass_support(struct ol_context *ol_ctx)
{
	return QDF_STATUS_SUCCESS;
}

#endif


QDF_STATUS ol_configure_target(struct ol_context *ol_ctx)
{
	uint32_t param;
	struct pld_platform_cap cap = {0};
	int ret;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_type = tgt_info->target_type;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	/* Tell target which HTC version it is used */
	param = HTC_PROTOCOL_VERSION;
	if (bmi_write_memory(
		hif_hia_item_address(target_type,
		offsetof(struct host_interest_s, hi_app_host_interest)),
		(uint8_t *) &param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
		BMI_ERR("bmi_write_memory for htc version failed");
		return QDF_STATUS_E_FAILURE;
	}

	/* set the firmware mode to STA/IBSS/AP */
	{
		if (bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("bmi_read_memory for setting fwmode failed");
			return QDF_STATUS_E_FAILURE;
		}

		/* TODO following parameters need to be re-visited. */
		param |= (1 << HI_OPTION_NUM_DEV_SHIFT); /* num_device */
		/* Firmware mode ?? */
		param |= (HI_OPTION_FW_MODE_AP << HI_OPTION_FW_MODE_SHIFT);
		/* mac_addr_method */
		param |= (1 << HI_OPTION_MAC_ADDR_METHOD_SHIFT);
		/* firmware_bridge */
		param |= (0 << HI_OPTION_FW_BRIDGE_SHIFT);
		/* fwsubmode */
		param |= (0 << HI_OPTION_FW_SUBMODE_SHIFT);

		BMI_INFO("NUM_DEV=%d FWMODE=0x%x FWSUBMODE=0x%x FWBR_BUF %d",
		       1, HI_OPTION_FW_MODE_AP, 0, 0);

		if (bmi_write_memory(
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("BMI WRITE for setting fwmode failed");
			return QDF_STATUS_E_FAILURE;
		}
	}
	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_PCI) {
		if (ol_disable_cdc_max_perf(ol_ctx))
			return QDF_STATUS_E_FAILURE;

		qdf_mem_zero(&cap, sizeof(cap));

		ret = pld_get_platform_cap(qdf_dev->dev, &cap);
		if (ret)
			BMI_ERR("platform capability info not available");

		if (!ret && cap.cap_flag & PLD_HAS_EXTERNAL_SWREG) {
			if (bmi_read_memory(hif_hia_item_address(target_type,
				offsetof(struct host_interest_s,
					 hi_option_flag2)),
				(uint8_t *)&param, 4, ol_ctx) !=
							QDF_STATUS_SUCCESS) {
				BMI_ERR("BMI READ failed for external SWREG");
				return QDF_STATUS_E_FAILURE;
			}

			param |= HI_OPTION_USE_EXT_LDO;
			if (bmi_write_memory(
				hif_hia_item_address(target_type,
					offsetof(struct host_interest_s,
						 hi_option_flag2)),
					(uint8_t *)&param, 4, ol_ctx) !=
							QDF_STATUS_SUCCESS) {
				BMI_ERR("BMI WRITE failed for external SWREG");
				return QDF_STATUS_E_FAILURE;
			}
		}

		if (ol_set_lpass_support(ol_ctx))
			return QDF_STATUS_E_FAILURE;
	}

	/* If host is running on a BE CPU, set the host interest area */
	{
#ifdef BIG_ENDIAN_HOST
		param = 1;
#else
		param = 0;
#endif
		if (bmi_write_memory(
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_be)),
			(uint8_t *) &param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
			BMI_ERR("setting host CPU BE mode failed");
			return QDF_STATUS_E_FAILURE;
		}
	}

	/* FW descriptor/Data swap flags */
	param = 0;
	if (bmi_write_memory(
		hif_hia_item_address(target_type,
		offsetof(struct host_interest_s, hi_fw_swap)),
		(uint8_t *) &param, 4, ol_ctx) != QDF_STATUS_SUCCESS) {
		BMI_ERR("BMI WRITE failed setting FW data/desc swap flags");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static int
ol_check_dataset_patch(struct hif_opaque_softc *scn, uint32_t *address)
{
	/* Check if patch file needed for this target type/version. */
	return 0;
}

static QDF_STATUS ol_fw_populate_clk_settings(enum a_refclk_speed_t refclk,
					      struct cmnos_clock_s *clock_s)
{
	if (!clock_s)
		return QDF_STATUS_E_FAILURE;

	switch (refclk) {
	case SOC_REFCLK_48_MHZ:
		clock_s->wlan_pll.div = 0xE;
		clock_s->wlan_pll.rnfrac = 0x2AAA8;
		clock_s->pll_settling_time = 2400;
		break;
	case SOC_REFCLK_19_2_MHZ:
		clock_s->wlan_pll.div = 0x24;
		clock_s->wlan_pll.rnfrac = 0x2AAA8;
		clock_s->pll_settling_time = 960;
		break;
	case SOC_REFCLK_24_MHZ:
		clock_s->wlan_pll.div = 0x1D;
		clock_s->wlan_pll.rnfrac = 0x15551;
		clock_s->pll_settling_time = 1200;
		break;
	case SOC_REFCLK_26_MHZ:
		clock_s->wlan_pll.div = 0x1B;
		clock_s->wlan_pll.rnfrac = 0x4EC4;
		clock_s->pll_settling_time = 1300;
		break;
	case SOC_REFCLK_37_4_MHZ:
		clock_s->wlan_pll.div = 0x12;
		clock_s->wlan_pll.rnfrac = 0x34B49;
		clock_s->pll_settling_time = 1870;
		break;
	case SOC_REFCLK_38_4_MHZ:
		clock_s->wlan_pll.div = 0x12;
		clock_s->wlan_pll.rnfrac = 0x15551;
		clock_s->pll_settling_time = 1920;
		break;
	case SOC_REFCLK_40_MHZ:
		clock_s->wlan_pll.div = 0x11;
		clock_s->wlan_pll.rnfrac = 0x26665;
		clock_s->pll_settling_time = 2000;
		break;
	case SOC_REFCLK_52_MHZ:
		clock_s->wlan_pll.div = 0x1B;
		clock_s->wlan_pll.rnfrac = 0x4EC4;
		clock_s->pll_settling_time = 2600;
		break;
	case SOC_REFCLK_UNKNOWN:
		clock_s->wlan_pll.refdiv = 0;
		clock_s->wlan_pll.div = 0;
		clock_s->wlan_pll.rnfrac = 0;
		clock_s->wlan_pll.outdiv = 0;
		clock_s->pll_settling_time = 1024;
		clock_s->refclk_hz = 0;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	clock_s->refclk_hz = refclk_speed_to_hz[refclk];
	clock_s->wlan_pll.refdiv = 0;
	clock_s->wlan_pll.outdiv = 1;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ol_patch_pll_switch(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *hif = ol_ctx->scn;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t addr = 0;
	uint32_t reg_val = 0;
	uint32_t mem_val = 0;
	struct cmnos_clock_s clock_s;
	uint32_t cmnos_core_clk_div_addr = 0;
	uint32_t cmnos_cpu_pll_init_done_addr = 0;
	uint32_t cmnos_cpu_speed_addr = 0;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(hif);
	uint32_t target_version = tgt_info->target_version;
	struct targetdef_t *scn = &ol_ctx->tgt_def;

	switch (target_version) {
	case AR6320_REV1_1_VERSION:
		cmnos_core_clk_div_addr = AR6320_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320_CPU_SPEED_ADDR;
		break;
	case AR6320_REV1_3_VERSION:
	case AR6320_REV2_1_VERSION:
		cmnos_core_clk_div_addr = AR6320V2_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320V2_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320V2_CPU_SPEED_ADDR;
		break;
	case AR6320_REV3_VERSION:
	case AR6320_REV3_2_VERSION:
	case QCA9379_REV1_VERSION:
	case QCA9377_REV1_1_VERSION:
		cmnos_core_clk_div_addr = AR6320V3_CORE_CLK_DIV_ADDR;
		cmnos_cpu_pll_init_done_addr = AR6320V3_CPU_PLL_INIT_DONE_ADDR;
		cmnos_cpu_speed_addr = AR6320V3_CPU_SPEED_ADDR;
		break;
	default:
		BMI_ERR("%s: Unsupported target version %x", __func__,
							target_version);
		goto end;
	}

	addr = (RTC_SOC_BASE_ADDRESS | EFUSE_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read EFUSE Addr");
		goto end;
	}

	status = ol_fw_populate_clk_settings(EFUSE_XTAL_SEL_GET(reg_val),
					     &clock_s);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to set clock settings");
		goto end;
	}
	BMI_DBG("crystal_freq: %dHz", clock_s.refclk_hz);

	/* ------Step 1---- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | BB_PLL_CONFIG_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CONFIG Addr");
		goto end;
	}
	BMI_DBG("Step 1a: %8X", reg_val);

	reg_val &= ~(BB_PLL_CONFIG_FRAC_MASK | BB_PLL_CONFIG_OUTDIV_MASK);
	reg_val |= (BB_PLL_CONFIG_FRAC_SET(clock_s.wlan_pll.rnfrac) |
		    BB_PLL_CONFIG_OUTDIV_SET(clock_s.wlan_pll.outdiv));
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CONFIG Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CONFIG Addr");
		goto end;
	}
	BMI_DBG("Step 1b: %8X", reg_val);

	/* ------Step 2---- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_SETTLE_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_SETTLE Addr");
		goto end;
	}
	BMI_DBG("Step 2a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_SETTLE_TIME_MASK;
	reg_val |= WLAN_PLL_SETTLE_TIME_SET(clock_s.pll_settling_time);
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_SETTLE Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_SETTLE Addr");
		goto end;
	}
	BMI_DBG("Step 2b: %8X", reg_val);

	/* ------Step 3---- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CORE_CLK_CTRL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read CLK_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 3a: %8X", reg_val);

	reg_val &= ~SOC_CORE_CLK_CTRL_DIV_MASK;
	reg_val |= SOC_CORE_CLK_CTRL_DIV_SET(1);
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CLK_CTRL Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back CLK_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 3b: %8X", reg_val);

	/* ------Step 4----- */
	mem_val = 1;
	status = bmi_write_memory(cmnos_core_clk_div_addr,
				  (uint8_t *) &mem_val, 4, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CLK_DIV Addr");
		goto end;
	}

	/* ------Step 5----- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr");
		goto end;
	}
	BMI_DBG("Step 5a: %8X", reg_val);

	reg_val &= ~(WLAN_PLL_CONTROL_REFDIV_MASK | WLAN_PLL_CONTROL_DIV_MASK |
		     WLAN_PLL_CONTROL_NOPWD_MASK);
	reg_val |= (WLAN_PLL_CONTROL_REFDIV_SET(clock_s.wlan_pll.refdiv) |
		    WLAN_PLL_CONTROL_DIV_SET(clock_s.wlan_pll.div) |
		    WLAN_PLL_CONTROL_NOPWD_SET(1));
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr");
		goto end;
	}
	qdf_udelay(100);
	BMI_DBG("Step 5b: %8X", reg_val);

	/* ------Step 6------- */
	do {
		reg_val = 0;
		status = bmi_read_soc_register((RTC_WMAC_BASE_ADDRESS |
				RTC_SYNC_STATUS_OFFSET), &reg_val, ol_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			BMI_ERR("Failed to read RTC_SYNC_STATUS Addr");
			goto end;
		}
	} while (RTC_SYNC_STATUS_PLL_CHANGING_GET(reg_val));

	/* ------Step 7------- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}
	BMI_DBG("Step 7a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_CONTROL_BYPASS_MASK;
	reg_val |= WLAN_PLL_CONTROL_BYPASS_SET(0);
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr for CTRL_BYPASS");
		goto end;
	}
	BMI_DBG("Step 7b: %8X", reg_val);

	/* ------Step 8-------- */
	do {
		reg_val = 0;
		status = bmi_read_soc_register((RTC_WMAC_BASE_ADDRESS |
				RTC_SYNC_STATUS_OFFSET), &reg_val, ol_ctx);
		if (status != QDF_STATUS_SUCCESS) {
			BMI_ERR("Failed to read SYNC_STATUS Addr");
			goto end;
		}
	} while (RTC_SYNC_STATUS_PLL_CHANGING_GET(reg_val));

	/* ------Step 9-------- */
	reg_val = 0;
	addr = (RTC_SOC_BASE_ADDRESS | SOC_CPU_CLOCK_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read CPU_CLK Addr");
		goto end;
	}
	BMI_DBG("Step 9a: %8X", reg_val);

	reg_val &= ~SOC_CPU_CLOCK_STANDARD_MASK;
	reg_val |= SOC_CPU_CLOCK_STANDARD_SET(1);
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CPU_CLK Addr");
		goto end;
	}

	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back CPU_CLK Addr");
		goto end;
	}
	BMI_DBG("Step 9b: %8X", reg_val);

	/* ------Step 10------- */
	reg_val = 0;
	addr = (RTC_WMAC_BASE_ADDRESS | WLAN_PLL_CONTROL_OFFSET);
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read PLL_CTRL Addr for NOPWD");
		goto end;
	}
	BMI_DBG("Step 10a: %8X", reg_val);

	reg_val &= ~WLAN_PLL_CONTROL_NOPWD_MASK;
	status = bmi_write_soc_register(addr, reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_CTRL Addr for NOPWD");
		goto end;
	}
	reg_val = 0;
	status = bmi_read_soc_register(addr, &reg_val, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to read back PLL_CTRL Addr for NOPWD");
		goto end;
	}
	BMI_DBG("Step 10b: %8X", reg_val);

	/* ------Step 11------- */
	mem_val = 1;
	status = bmi_write_memory(cmnos_cpu_pll_init_done_addr,
				  (uint8_t *) &mem_val, 4, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write PLL_INIT Addr");
		goto end;
	}

	mem_val = TARGET_CPU_FREQ;
	status = bmi_write_memory(cmnos_cpu_speed_addr,
				  (uint8_t *) &mem_val, 4, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("Failed to write CPU_SPEED Addr");
		goto end;
	}

end:
	return status;
}

QDF_STATUS ol_download_firmware(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t param, address = 0;
	QDF_STATUS status = !QDF_STATUS_SUCCESS;
	QDF_STATUS ret;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	struct ol_config_info *ini_cfg = ol_get_ini_handle(ol_ctx);
	uint32_t target_type = tgt_info->target_type;
	uint32_t target_version = tgt_info->target_version;
	struct bmi_info *bmi_ctx = GET_BMI_CONTEXT(ol_ctx);
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	if (0 != pld_get_fw_files_for_target(qdf_dev->dev,
					     &bmi_ctx->fw_files,
					      target_type,
					      target_version)) {
		BMI_ERR("%s: No FW files from platform driver", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	/* Transfer Board Data from Target EEPROM to Target RAM */
	/* Determine where in Target RAM to write Board Data */
	bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_board_data)),
			(uint8_t *)&address, 4, ol_ctx);

	if (!address) {
		address = AR6004_REV5_BOARD_DATA_ADDRESS;
		BMI_DBG("%s: Target address not known! Using 0x%x",
						__func__, address);
	}

	if (hif_get_bus_type(scn) != QDF_BUS_TYPE_USB) {
		ret = ol_patch_pll_switch(ol_ctx);
		if (ret != QDF_STATUS_SUCCESS) {
			BMI_ERR("pll switch failed. status %d", ret);
			return ret;
		}
	}

	if (ol_ctx->cal_in_flash) {
		/* Write EEPROM or Flash data to Target RAM */
		status = ol_transfer_bin_file(ol_ctx, ATH_FLASH_FILE,
						address, false);
	}

	if (!status) {
		/* Record the fact that Board Data is initialized */
		param = 1;
		bmi_write_memory(
			hif_hia_item_address(target_type,
			offsetof(struct host_interest_s,
				hi_board_data_initialized)),
				(uint8_t *) &param, 4, ol_ctx);
	} else {
		/* Transfer One Time Programmable data */
		address = BMI_SEGMENTED_WRITE_ADDR;
		BMI_INFO("%s: Using 0x%x for the remainder of init",
				__func__, address);

		status = ol_transfer_bin_file(ol_ctx, ATH_OTP_FILE,
					      address, true);
		/* Execute the OTP code only if entry found and downloaded */
		if (!status) {
			uint16_t board_id = 0xffff;
			/* get board id */
			param = 0x10;
			bmi_execute(address, &param, ol_ctx);
			if (!(param & 0xff))
				board_id = (param >> 8) & 0xffff;
			BMI_INFO("%s: board ID is 0x%0x", __func__, board_id);
			bmi_ctx->board_id = board_id;
		} else if (status < 0) {
			return status;
		}

		bmi_read_memory(hif_hia_item_address(target_type,
				offsetof(struct host_interest_s,
					hi_board_data)),
				(uint8_t *)&address, 4, ol_ctx);

		if (!address) {
			address = AR6004_REV5_BOARD_DATA_ADDRESS;
			pr_err("%s: Target address not known! Using 0x%x\n",
			       __func__, address);
		}

		/* Flash is either not available or invalid */
		if (ol_transfer_bin_file(ol_ctx, ATH_BOARD_DATA_FILE,
					 address, false)) {
			return QDF_STATUS_E_FAILURE;
		}

		/* Record the fact that Board Data is initialized */
		param = 1;
		bmi_write_memory(hif_hia_item_address(target_type,
				 offsetof(struct host_interest_s,
					  hi_board_data_initialized)),
				 (uint8_t *) &param, 4, ol_ctx);
		address = BMI_SEGMENTED_WRITE_ADDR;
		param = 0;
		bmi_execute(address, &param, ol_ctx);
	}

	if (!ol_transfer_bin_file(ol_ctx, ATH_SETUP_FILE,
				  BMI_SEGMENTED_WRITE_ADDR, true)) {
		param = 0;
		bmi_execute(address, &param, ol_ctx);
	}

	/* Download Target firmware
	 * TODO point to target specific files in runtime
	 */
	address = BMI_SEGMENTED_WRITE_ADDR;
	if (ol_transfer_bin_file(ol_ctx, ATH_FIRMWARE_FILE,
				 address, true)) {
		return QDF_STATUS_E_FAILURE;
	}

	/* Apply the patches */
	if (ol_check_dataset_patch(scn, &address)) {
		if (ol_transfer_bin_file(ol_ctx, ATH_PATCH_FILE, address,
					 false)) {
			return QDF_STATUS_E_FAILURE;
		}
		bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_dset_list_head)),
			(uint8_t *) &address, 4, ol_ctx);
	}

	switch (target_version) {
	case AR6004_VERSION_REV1_3:
		param = 11;
		break;
	case AR6320_REV1_VERSION:
	case AR6320_REV2_VERSION:
	case AR6320_REV3_VERSION:
	case AR6320_REV3_2_VERSION:
	case QCA9377_REV1_1_VERSION:
	case QCA9379_REV1_VERSION:
	case AR6320_REV4_VERSION:
	case AR6320_DEV_VERSION:
		/*
		 * In sdio interface chip, both sdio_data2 and uart_tx pin
		 * will use GPIO6. It is set by fw rom code, which will cause
		 * sdio CRC error when there is sdio transaction.
		 * Override uart tx pin to avoid side effect to sdio pin.
		 */
		if (hif_get_bus_type(scn) == QDF_BUS_TYPE_SDIO)
			param = 19;
		else
			param = 6;
		break;
	default:
	/* Configure GPIO AR9888 UART */
		param = 7;
	}

	bmi_write_memory(hif_hia_item_address(target_type,
		offsetof(struct host_interest_s, hi_dbg_uart_txpin)),
		(uint8_t *)&param, 4, ol_ctx);

	if (ini_cfg->enable_uart_print) {
		param = 1;
		bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_serial_enable)),
			(uint8_t *)&param, 4, ol_ctx);
	} else {
		/*
		 * Explicitly setting UART prints to zero as target turns it on
		 * based on scratch registers.
		 */
		param = 0;
		bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_serial_enable)),
			(uint8_t *)&param, 4, ol_ctx);
	}

	if (ini_cfg->enable_fw_log) {
		bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, ol_ctx);

		param &= ~(HI_OPTION_DISABLE_DBGLOG);
		bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, ol_ctx);
	} else {
		/*
		 * Explicitly setting fwlog prints to zero as target turns it on
		 * based on scratch registers.
		 */
		bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *)&param, 4, ol_ctx);

		param |= HI_OPTION_DISABLE_DBGLOG;
		bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s, hi_option_flag)),
			(uint8_t *) &param, 4, ol_ctx);
	}
	status = ol_extra_initialization(ol_ctx);

	return status;
}

static int ol_diag_read(struct hif_opaque_softc *scn, uint8_t *buffer,
			uint32_t pos, size_t count)
{
	int result = 0;

	if ((4 == count) && ((pos & 3) == 0)) {
		result = hif_diag_read_access(scn, pos,
					      (uint32_t *) buffer);
	} else {
		size_t amount_read = 0;
		size_t readSize = PCIE_READ_LIMIT;
		size_t remainder = 0;

		if (count > PCIE_READ_LIMIT) {
			while ((amount_read < count) && (0 == result)) {
				result = hif_diag_read_mem(scn, pos,
							   buffer, readSize);
				if (0 == result) {
					buffer += readSize;
					pos += readSize;
					amount_read += readSize;
					remainder = count - amount_read;
					if (remainder < PCIE_READ_LIMIT)
						readSize = remainder;
				}
			}
		} else {
		result = hif_diag_read_mem(scn, pos,
					   buffer, count);
		}
	}

	if (!result)
		return count;
	else
		return -EIO;
}

static int ol_ath_get_reg_table(struct hif_opaque_softc *scn,
				uint32_t target_version,
				struct tgt_reg_table *reg_table)
{
	int section_len = 0;

	if (!reg_table) {
		qdf_assert(0);
		return section_len;
	}

	if (hif_get_bus_type(scn) != QDF_BUS_TYPE_PCI &&
	    hif_get_bus_type(scn) != QDF_BUS_TYPE_SDIO)
		return section_len;

	switch (target_version) {
	case AR6320_REV2_1_VERSION:
		reg_table->section = ar6320v2_reg_table;
		reg_table->section_size = ARRAY_SIZE(ar6320v2_reg_table);
		section_len = AR6320_REV2_1_REG_SIZE;
		break;
	case AR6320_REV3_VERSION:
	case AR6320_REV3_2_VERSION:
	case QCA9379_REV1_VERSION:
	case QCA9377_REV1_1_VERSION:
		reg_table->section = ar6320v3_reg_table;
		reg_table->section_size = ARRAY_SIZE(ar6320v3_reg_table);
		section_len = AR6320_REV3_REG_SIZE;
		break;
	default:
		reg_table->section = NULL;
		reg_table->section_size = 0;
		section_len = 0;
	}

	return section_len;
}

static int ol_diag_read_reg_loc(struct hif_opaque_softc *scn, uint8_t *buffer,
				uint32_t buffer_len)
{
	int i, len, section_len, fill_len;
	int dump_len, result = 0;
	struct tgt_reg_table reg_table;
	const struct tgt_reg_section *curr_sec, *next_sec;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_version =  tgt_info->target_version;

	reg_table.section = NULL;
	reg_table.section_size = 0;

	section_len = ol_ath_get_reg_table(scn, target_version, &reg_table);

	if (!reg_table.section || !reg_table.section_size || !section_len) {
		BMI_ERR("%s: failed to get reg table", __func__);
		result = -EIO;
		goto out;
	}

	curr_sec = reg_table.section;
	for (i = 0; i < reg_table.section_size; i++) {

		dump_len = curr_sec->end_addr - curr_sec->start_addr;

		if ((buffer_len - result) < dump_len) {
			BMI_ERR("No buffer to dump regs:%d: 0x%08x-0x%08x",
				i, curr_sec->start_addr, curr_sec->end_addr);
			goto out;
		}

		len = ol_diag_read(scn, buffer, curr_sec->start_addr, dump_len);

		if (len != -EIO) {
			buffer += len;
			result += len;
		} else {
			BMI_ERR("%s: can't read reg 0x%08x len = %d",
				__func__, curr_sec->start_addr, dump_len);
			result = -EIO;
			goto out;
		}

		if (result < section_len) {
			next_sec = (struct tgt_reg_section *) ((uint8_t *)
						 curr_sec + sizeof(*curr_sec));
			fill_len = next_sec->start_addr - curr_sec->end_addr;
			if ((buffer_len - result) < fill_len) {
				BMI_ERR("No buf to fill regs:%d: 0x%08x-0x%08x",
					i, curr_sec->end_addr,
					next_sec->start_addr);
				goto out;
			}

			if (fill_len) {
				buffer += fill_len;
				result += fill_len;
			}
		}
		curr_sec++;
	}

out:
	return result;
}

static
void ol_dump_target_memory(struct hif_opaque_softc *scn, void *memory_block)
{
	char *buffer_loc = memory_block;
	u_int32_t section_count = 0;
	u_int32_t address = 0;
	u_int32_t size = 0;

	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_SDIO ||
	    hif_get_bus_type(scn) == QDF_BUS_TYPE_USB)
		return;

	for (; section_count < 2; section_count++) {
		switch (section_count) {
		case 0:
			address = DRAM_LOCAL_BASE_ADDR;
			size = DRAM_SIZE;
			break;
		case 1:
			address = AXI_LOCATION;
			size = AXI_SIZE;
		default:
			break;
		}
		hif_dump_target_memory(scn, buffer_loc, address, size);
		buffer_loc += size;
	}
}

static int
ol_dump_ce_register(struct hif_opaque_softc *scn, void *memory_block)
{
	int ret;

	BMI_ERR("Could not read dump section!");

	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_SDIO ||
	    hif_get_bus_type(scn) == QDF_BUS_TYPE_USB)
		return 0;

	if (hif_dump_registers(scn))
		BMI_ERR("Failed to dump bus registers");

	ol_dump_target_memory(scn, memory_block);
	ret = -EACCES;

	return ret;
}

static inline uint32_t
ol_get_max_section_count(struct hif_opaque_softc *scn)
{
	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_PCI)
		return 5;
	else
		return 4;
}

/**
 * ol_set_ram_config_reg() - set target RAM configuration register
 * @sc: pointer of hif_softc context
 * @config: value to be written to the register
 *
 * This function will write the given value to target RAM configuration
 * register which is bit[23-20] of target CPU inbound address in order to
 * provide correct address mapping.
 *
 * Return: 0 for success or reasons for failure
 */
static int ol_set_ram_config_reg(struct hif_opaque_softc *scn, uint32_t config)
{
	QDF_STATUS status;
	uint32_t val;
	struct targetdef_s *targetdef =
		(struct targetdef_s *)hif_get_targetdef(scn);
	uint32_t ram_config_addr =
		targetdef->d_SOC_CORE_BASE_ADDRESS + FW_RAM_CONFIG_ADDRESS;

	if (hif_get_bus_type(scn) != QDF_BUS_TYPE_PCI)
		return -EACCES;

	status = hif_diag_write_access(scn, ram_config_addr, config);
	if (QDF_IS_STATUS_ERROR(status)) {
		return -EACCES;
	}
	status = hif_diag_read_access(scn, ram_config_addr, &val);
	if (QDF_IS_STATUS_ERROR(status)) {
		return -EACCES;
	}
	if (val != config) {
		BMI_ERR("%s: Failed to set RAM config reg from 0x%x to 0x%x",
			__func__, val, config);
		return -EACCES;
	}
	return 0;
}

static int
ol_get_iram_len_and_pos(struct hif_opaque_softc *scn, uint32_t *pos,
			uint32_t *len, uint32_t section)
{
	enum hif_target_status status;
	uint32_t iram_addr, iram_size;
	int ret;

	if (hif_get_bus_type(scn) != QDF_BUS_TYPE_PCI) {
		*pos = IRAM_LOCATION;
		*len = IRAM_SIZE;
		BMI_ERR("%s: Dumping IRAM Section", __func__);
		return 0;
	}

	status = hif_get_target_status(scn);
	if (status != TARGET_STATUS_RESET) {
		BMI_ERR("%s: Target status invalid: %d", __func__, status);
		return -EBUSY;
	}

	switch (section) {
	case 3:
		BMI_ERR("%s: Dumping IRAM1 section", __func__);
		iram_addr = IRAM1_LOCATION;
		iram_size = IRAM1_SIZE;
		break;
	case 4:
		BMI_ERR("%s: Dumping IRAM2 section", __func__);
		iram_addr = IRAM2_LOCATION;
		iram_size = IRAM2_SIZE;
		break;
	default:
		BMI_ERR("%s: Invalid input iram section %d",
			__func__, section);
		return A_EINVAL;
	}

	ret = ol_set_ram_config_reg(scn, iram_addr >> 20);
	if (ret) {
		BMI_ERR("%s: Skip IRAM1 ret:%d", __func__, ret);
		return -EBUSY;
	}

	*pos = iram_addr;
	*len = iram_size;
	return 0;
}

/**
 * ol_target_coredump() - API to collect target ramdump
 * @inst - private context
 * @memory_block - non-NULL reserved memory location
 * @block_len - size of the dump to collect
 *
 * Function to perform core dump for the target.
 *
 * Return: int
 */
static int ol_target_coredump(void *inst, void *memory_block,
					uint32_t block_len)
{
	struct hif_opaque_softc *scn = (struct hif_opaque_softc *)inst;
	int8_t *buffer_loc = memory_block;
	int result = 0;
	int ret = 0;
	uint32_t amount_read = 0;
	uint32_t section_count = 0;
	uint32_t pos = 0;
	uint32_t read_len = 0;
	uint32_t max_count = ol_get_max_section_count(scn);

	while ((section_count < max_count) && (amount_read < block_len)) {
		switch (section_count) {
		case 0:
			pos = DRAM_LOCATION;
			read_len = DRAM_SIZE;
			BMI_ERR("%s: Dumping DRAM section...", __func__);
			break;
		case 1:
			pos = AXI_LOCATION;
			read_len = AXI_SIZE;
			BMI_ERR("%s: Dumping AXI section...", __func__);
			break;
		case 2:
			pos = REGISTER_LOCATION;
			/* ol_diag_read_reg_loc checks for buffer overrun */
			read_len = 0;
			BMI_ERR("%s: Dumping Register section...", __func__);
			break;
		case 3:
		case 4:
			ret = ol_get_iram_len_and_pos(scn, &pos, &read_len,
						      section_count);
			if (ret) {
				BMI_ERR("%s: Fail to Dump IRAM Section "
					"ret:%d", __func__, ret);
				return ret;
			}
			break;
		default:
			BMI_ERR("%s: INVALID SECTION_:%d", __func__,
				section_count);
			return 0;
		}

		if (block_len - amount_read < read_len) {
			BMI_ERR("%s: No memory to dump section:%d buffer!",
				__func__, section_count);
			return -ENOMEM;
		}

		if (((hif_get_bus_type(scn) == QDF_BUS_TYPE_PCI) ||
		     (hif_get_bus_type(scn) == QDF_BUS_TYPE_SDIO)) &&
		    pos == REGISTER_LOCATION)
			result = ol_diag_read_reg_loc(scn, buffer_loc,
						      block_len - amount_read);
		else
			result = ol_diag_read(scn, buffer_loc, pos, read_len);

		if (result == -EIO)
			return ol_dump_ce_register(scn, memory_block);

		BMI_INFO("%s: Section:%d Bytes Read:%0x", __func__,
			 section_count, result);

		amount_read += result;
		buffer_loc += result;
		section_count++;
	}
	return ret;
}

/**
 * ol_get_ini_handle() - API to get Ol INI configuration
 * @ol_ctx: OL Context
 *
 * Return: pointer to OL configuration
 */
struct ol_config_info *ol_get_ini_handle(struct ol_context *ol_ctx)
{
	return &ol_ctx->cfg_info;
}

/**
 * ol_init_ini_config() - API to initialize INI configuration
 * @ol_ctx: OL Context
 * @cfg: OL ini configuration
 *
 * Return: void
 */
void ol_init_ini_config(struct ol_context *ol_ctx,
			struct ol_config_info *cfg)
{
	qdf_mem_copy(&ol_ctx->cfg_info, cfg, sizeof(struct ol_config_info));
}

void ol_set_fw_crashed_cb(struct ol_context *ol_ctx,
			  void (*callback_fn)(void))
{
	ol_ctx->fw_crashed_cb = callback_fn;
}
