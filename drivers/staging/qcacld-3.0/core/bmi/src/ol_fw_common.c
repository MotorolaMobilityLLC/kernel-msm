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

#include "ol_if_athvar.h"
#include "targaddrs.h"
#include "ol_cfg.h"
#include "i_ar6320v2_regtable.h"
#include "ol_fw.h"
#ifdef HIF_PCI
#include "ce_reg.h"
#endif
#if defined(HIF_SDIO)
#include "regtable_sdio.h"
#endif
#if defined(HIF_USB)
#include "regtable_usb.h"
#endif
#include "i_bmi.h"
#include "cds_api.h"

#ifdef CONFIG_DISABLE_SLEEP_BMI_OPTION
static inline void ol_sdio_disable_sleep(struct ol_context *ol_ctx)
{
	uint32_t value;

	BMI_ERR("prevent ROME from sleeping");
	bmi_read_soc_register(MBOX_BASE_ADDRESS + LOCAL_SCRATCH_OFFSET,
		/* this address should be 0x80C0 for ROME*/
		&value,
		ol_ctx);

	value |= SOC_OPTION_SLEEP_DISABLE;

	bmi_write_soc_register(MBOX_BASE_ADDRESS + LOCAL_SCRATCH_OFFSET,
				 value,
				 ol_ctx);
}

#else
static inline void ol_sdio_disable_sleep(struct ol_context *ol_ctx)
{
}

#endif

/**
 * ol_usb_extra_initialization() - USB extra initialization
 * @ol_ctx: pointer to ol_context
 *
 * USB specific initialization after firmware download
 *
 * Return: QDF_STATUS_SUCCESS on success and error QDF status on failure
 */
static QDF_STATUS
ol_usb_extra_initialization(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct hif_target_info *tgt_info =
				hif_get_target_info_handle(scn);
	QDF_STATUS status = !QDF_STATUS_SUCCESS;
	u_int32_t param = 0;

	param |= HI_ACS_FLAGS_ALT_DATA_CREDIT_SIZE;
	status = bmi_write_memory(
				hif_hia_item_address(tgt_info->target_type,
					offsetof(struct host_interest_s,
					hi_acs_flags)),
				(u_int8_t *)&param, 4, ol_ctx);

	return status;
}

/*Setting SDIO block size, mbox ISR yield limit for SDIO based HIF*/
static
QDF_STATUS ol_sdio_extra_initialization(struct ol_context *ol_ctx)
{

	QDF_STATUS status;
	uint32_t param;
	uint32_t blocksizes[HTC_MAILBOX_NUM_MAX];
	uint32_t MboxIsrYieldValue = 99;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_type = tgt_info->target_type;

	/* get the block sizes */
	status = hif_get_config_item(scn,
				HIF_DEVICE_GET_BLOCK_SIZE,
				blocksizes, sizeof(blocksizes));
	if (status) {
		BMI_ERR("Failed to get block size info from HIF layer");
		goto exit;
	}
	/* note: we actually get the block size for mailbox 1,
	 * for SDIO the block size on mailbox 0 is artificially
	 * set to 1 must be a power of 2
	 */
	qdf_assert((blocksizes[1] & (blocksizes[1] - 1)) == 0);

	/* set the host interest area for the block size */
	status = bmi_write_memory(hif_hia_item_address(target_type,
				 offsetof(struct host_interest_s,
				 hi_mbox_io_block_sz)),
				(uint8_t *)&blocksizes[1],
				4,
				ol_ctx);

	if (status) {
		BMI_ERR("BMIWriteMemory for IO block size failed");
		goto exit;
	}

	if (MboxIsrYieldValue != 0) {
		/* set the host for the mbox ISR yield limit */
		status =
		bmi_write_memory(hif_hia_item_address(target_type,
				offsetof(struct host_interest_s,
				hi_mbox_isr_yield_limit)),
				(uint8_t *)&MboxIsrYieldValue,
				4,
				ol_ctx);

		if (status) {
			BMI_ERR("BMI write for yield limit failed\n");
			goto exit;
		}
	}
	ol_sdio_disable_sleep(ol_ctx);
	status = bmi_read_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s,
			hi_acs_flags)),
			(uint8_t *)&param,
			4,
			ol_ctx);
	if (status) {
		BMI_ERR("BMIReadMemory for hi_acs_flags failed");
		goto exit;
	}

	param |= HI_ACS_FLAGS_ALT_DATA_CREDIT_SIZE;

	/* disable swap mailbox for FTM */
	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE)
		param |= HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_SET;

	if (!cds_is_ptp_tx_opt_enabled())
		param |= HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_SET;

	/* enable TX completion to collect tx_desc for pktlog */
	if (cds_is_packet_log_enabled())
		param &= ~HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_SET;

	bmi_write_memory(hif_hia_item_address(target_type,
			offsetof(struct host_interest_s,
			hi_acs_flags)),
			(uint8_t *)&param, 4, ol_ctx);
exit:
	return status;
}

/**
 * ol_extra_initialization() - OL extra initialization
 * @ol_ctx: pointer to ol_context
 *
 * Bus specific initialization after firmware download
 *
 * Return: QDF_STATUS_SUCCESS on success and error QDF status on failure
 */
QDF_STATUS ol_extra_initialization(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;

	if (hif_get_bus_type(scn) == QDF_BUS_TYPE_SDIO)
		return ol_sdio_extra_initialization(ol_ctx);
	else if (hif_get_bus_type(scn) == QDF_BUS_TYPE_USB)
		return ol_usb_extra_initialization(ol_ctx);

	return QDF_STATUS_SUCCESS;
}

void ol_target_ready(struct hif_opaque_softc *scn, void *cfg_ctx)
{
	uint32_t value = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);
	uint32_t target_type = tgt_info->target_type;

	if (hif_get_bus_type(scn) != QDF_BUS_TYPE_SDIO)
		return;
	status = hif_diag_read_mem(scn,
		hif_hia_item_address(target_type,
		offsetof(struct host_interest_s, hi_acs_flags)),
		(uint8_t *)&value, sizeof(u_int32_t));

	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("HIFDiagReadMem failed");
		return;
	}

	if (value & HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_FW_ACK) {
		BMI_ERR("MAILBOX SWAP Service is enabled!");
		hif_set_mailbox_swap(scn);
	}

	if (value & HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_FW_ACK) {
		BMI_ERR("Reduced Tx Complete service is enabled!");
		ol_cfg_set_tx_free_at_download(cfg_ctx);
	}
}
