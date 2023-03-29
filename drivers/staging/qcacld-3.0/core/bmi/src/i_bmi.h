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

/* ===================================================================
 * Internal BMI Header File
 */

#ifndef _I_BMI_H_
#define _I_BMI_H_

#include "qdf_types.h"
#include "qdf_defer.h"
#include "hif.h"
#include "bmi_msg.h"
#include "bmi.h"
#include "ol_fw.h"
#include "pld_common.h"

/*
 * Note that not all the register locations are accessible.
 * A list of accessible target registers are specified with
 * their start and end addresses in a table for given target
 * version. We should NOT access other locations as either
 * they are invalid locations or host does not have read
 * access to it or the value of the particular register
 * read might change
 */
#define REGISTER_LOCATION       0x00000800

#define DRAM_LOCATION           0x00400000
#ifdef HIF_PCI
#define DRAM_SIZE               0x000a8000
#else
#define DRAM_SIZE               0x00098000
#endif
/* The local base addr is used to read the target dump using pcie I/O reads */
#define DRAM_LOCAL_BASE_ADDR    (0x100000)

/* Target IRAM config */
#define FW_RAM_CONFIG_ADDRESS   0x0018
#define IRAM1_LOCATION          0x00980000
#define IRAM1_SIZE              0x00080000
#define IRAM2_LOCATION          0x00a00000
#define IRAM2_SIZE              0x00040000
#ifdef HIF_SDIO
#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x000C0000
#else
#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x00038000
#endif

#define AXI_LOCATION            0x000a0000
#ifdef HIF_PCI
#define AXI_SIZE                0x00018000
#else
#define AXI_SIZE                0x00020000
#endif

#define PCIE_READ_LIMIT         0x00040000

#define SHA256_DIGEST_SIZE      32

/* BMI LOGGING WRAPPERS */

#define BMI_LOG(level, args...) QDF_TRACE(QDF_MODULE_ID_BMI, \
					level, ##args)
#define BMI_ERR(args ...)	BMI_LOG(QDF_TRACE_LEVEL_ERROR, args)
#define BMI_DBG(args ...)	BMI_LOG(QDF_TRACE_LEVEL_DEBUG, args)
#define BMI_WARN(args ...)	BMI_LOG(QDF_TRACE_LEVEL_WARN, args)
#define BMI_INFO(args ...)	BMI_LOG(QDF_TRACE_LEVEL_INFO, args)
/* End of BMI Logging Wrappers */

/* BMI Assert Wrappers */
#define bmi_assert QDF_BUG
/*
 * Although we had envisioned BMI to run on top of HTC, this is not how the
 * final implementation ended up. On the Target side, BMI is a part of the BSP
 * and does not use the HTC protocol nor even DMA -- it is intentionally kept
 * very simple.
 */

#define MAX_BMI_CMDBUF_SZ (BMI_DATASZ_MAX + \
			sizeof(uint32_t) /* cmd */ + \
			sizeof(uint32_t) /* addr */ + \
			sizeof(uint32_t))    /* length */
#define BMI_COMMAND_FITS(sz) ((sz) <= MAX_BMI_CMDBUF_SZ)
#define BMI_EXCHANGE_TIMEOUT_MS  1000

struct hash_fw {
	u8 qwlan[SHA256_DIGEST_SIZE];
	u8 otp[SHA256_DIGEST_SIZE];
	u8 bdwlan[SHA256_DIGEST_SIZE];
	u8 utf[SHA256_DIGEST_SIZE];
};

enum ATH_BIN_FILE {
	ATH_OTP_FILE,
	ATH_FIRMWARE_FILE,
	ATH_PATCH_FILE,
	ATH_BOARD_DATA_FILE,
	ATH_FLASH_FILE,
	ATH_SETUP_FILE,
};

#if defined(QCA_WIFI_3_0_ADRASTEA)
#define NO_BMI 1
#else
#define NO_BMI 0
#endif

/**
 * struct bmi_info - Structure to hold BMI Specific information
 * @bmi_cmd_buff - BMI Command Buffer
 * @bmi_rsp_buff - BMI Response Buffer
 * @bmi_cmd_da - BMI Command Physical address
 * @bmi_rsp_da - BMI Response Physical address
 * @bmi_done - Flag to check if BMI Phase is complete
 * @board_id - board ID
 * @fw_files - FW files
 *
 */
struct bmi_info {
	uint8_t *bmi_cmd_buff;
	uint8_t *bmi_rsp_buff;
	dma_addr_t bmi_cmd_da;
	dma_addr_t bmi_rsp_da;
	bool bmi_done;
	uint16_t board_id;
	struct pld_fw_files fw_files;
};

/**
 * struct ol_context - Structure to hold OL context
 * @bmi: BMI info
 * @cal_in_flash: For Firmware Flash Download
 * @qdf_dev: QDF Device
 * @scn: HIF Context
 * @ramdump_work: Work for Ramdump collection
 * @fw_indication_work: Work for Fw inciation
 * @tgt_def: Target Defnition pointer
 * @fw_crashed_cb: Callback for firmware crashed ind
 *
 * Structure to hold all ol BMI/Ramdump info
 */
struct ol_context {
	struct bmi_info bmi;
	struct ol_config_info cfg_info;
	uint8_t *cal_in_flash;
	qdf_device_t qdf_dev;
	qdf_work_t ramdump_work;
	qdf_work_t fw_indication_work;
	struct hif_opaque_softc *scn;
	struct targetdef_t {
		struct targetdef_s *targetdef;
	} tgt_def;
	void (*fw_crashed_cb)(void);
};

#define GET_BMI_CONTEXT(ol_ctx) ((struct bmi_info *)ol_ctx)

QDF_STATUS bmi_execute(uint32_t address, uint32_t *param,
				struct ol_context *ol_ctx);
QDF_STATUS bmi_init(struct ol_context *ol_ctx);
QDF_STATUS bmi_no_command(struct ol_context *ol_ctx);
QDF_STATUS bmi_read_memory(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
QDF_STATUS bmi_write_memory(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
QDF_STATUS bmi_fast_download(uint32_t address, uint8_t *buffer, uint32_t length,
					struct ol_context *ol_ctx);
QDF_STATUS bmi_read_soc_register(uint32_t address,
				uint32_t *param, struct ol_context *ol_ctx);
QDF_STATUS bmi_write_soc_register(uint32_t address, uint32_t param,
					struct ol_context *ol_ctx);
QDF_STATUS bmi_get_target_info(struct bmi_target_info *targ_info,
			       struct ol_context *ol_ctx);
QDF_STATUS bmi_firmware_download(struct ol_context *ol_ctx);
QDF_STATUS bmi_done_local(struct ol_context *ol_ctx);
QDF_STATUS ol_download_firmware(struct ol_context *ol_ctx);
QDF_STATUS ol_configure_target(struct ol_context *ol_ctx);
QDF_STATUS bmi_sign_stream_start(uint32_t address, uint8_t *buffer,
				 uint32_t length, struct ol_context *ol_ctx);
void ramdump_work_handler(void *arg);
void fw_indication_work_handler(void *arg);
struct ol_config_info *ol_get_ini_handle(struct ol_context *ol_ctx);

#ifdef HIF_SDIO
QDF_STATUS hif_reg_based_get_target_info(struct hif_opaque_softc *hif_ctx,
		  struct bmi_target_info *targ_info);
#endif
#if defined(HIF_PCI) || defined(HIF_SNOC) || defined(HIF_AHB) || defined(HIF_USB)
static inline QDF_STATUS
hif_reg_based_get_target_info(struct hif_opaque_softc *hif_ctx,
		  struct bmi_target_info *targ_info)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
