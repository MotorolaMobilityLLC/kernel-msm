/*
 * Copyright (c) 2014-2019 The Linux Foundation. All rights reserved.
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

#include "i_bmi.h"
#include "cds_api.h"

/* APIs visible to the driver */

QDF_STATUS bmi_init(struct ol_context *ol_ctx)
{
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	struct hif_opaque_softc *scn = ol_ctx->scn;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;

	if (!scn) {
		BMI_ERR("Invalid scn Context");
		bmi_assert(0);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	if (!qdf_dev->dev) {
		BMI_ERR("%s: Invalid Device Pointer", __func__);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	info->bmi_done = false;

	if (!info->bmi_cmd_buff) {
		info->bmi_cmd_buff =
			qdf_mem_alloc_consistent(qdf_dev, qdf_dev->dev,
						 MAX_BMI_CMDBUF_SZ,
						 &info->bmi_cmd_da);
		if (!info->bmi_cmd_buff) {
			BMI_ERR("No Memory for BMI Command");
			return QDF_STATUS_E_NOMEM;
		}
	}

	if (!info->bmi_rsp_buff) {
		info->bmi_rsp_buff =
			qdf_mem_alloc_consistent(qdf_dev, qdf_dev->dev,
						 MAX_BMI_CMDBUF_SZ,
						 &info->bmi_rsp_da);
		if (!info->bmi_rsp_buff) {
			BMI_ERR("No Memory for BMI Response");
			goto end;
		}
	}
	return QDF_STATUS_SUCCESS;
end:
	qdf_mem_free_consistent(qdf_dev, qdf_dev->dev, MAX_BMI_CMDBUF_SZ,
				 info->bmi_cmd_buff, info->bmi_cmd_da, 0);
	info->bmi_cmd_buff = NULL;
	return QDF_STATUS_E_NOMEM;
}

void bmi_cleanup(struct ol_context *ol_ctx)
{
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	qdf_device_t qdf_dev;

	if (!info || !ol_ctx) {
		BMI_WARN("%s: no bmi to cleanup", __func__);
		return;
	}

	qdf_dev = ol_ctx->qdf_dev;
	if (!qdf_dev || !qdf_dev->dev) {
		BMI_ERR("%s: Invalid Device Pointer", __func__);
		return;
	}

	if (info->bmi_cmd_buff) {
		qdf_mem_free_consistent(qdf_dev, qdf_dev->dev,
					MAX_BMI_CMDBUF_SZ,
				    info->bmi_cmd_buff, info->bmi_cmd_da, 0);
		info->bmi_cmd_buff = NULL;
		info->bmi_cmd_da = 0;
	}

	if (info->bmi_rsp_buff) {
		qdf_mem_free_consistent(qdf_dev, qdf_dev->dev,
					MAX_BMI_CMDBUF_SZ,
				    info->bmi_rsp_buff, info->bmi_rsp_da, 0);
		info->bmi_rsp_buff = NULL;
		info->bmi_rsp_da = 0;
	}
}

/**
 * bmi_done() - finish the bmi opperation
 * @ol_ctx: the bmi context
 *
 * does some sanity checking.
 * exchanges one last message with firmware.
 * frees some buffers.
 *
 * Return: QDF_STATUS_SUCCESS if bmi isn't needed.
 *	QDF_STATUS_SUCCESS if bmi finishes.
 *	otherwise returns failure.
 */
QDF_STATUS bmi_done(struct ol_context *ol_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (NO_BMI)
		return QDF_STATUS_SUCCESS;

	if (!ol_ctx) {
		BMI_ERR("%s: null context", __func__);
		return QDF_STATUS_E_NOMEM;
	}
	hif_claim_device(ol_ctx->scn);

	if (!hif_needs_bmi(ol_ctx->scn))
		return QDF_STATUS_SUCCESS;

	status = bmi_done_local(ol_ctx);
	if (status != QDF_STATUS_SUCCESS)
		BMI_ERR("BMI_DONE Failed status:%d", status);

	return status;
}

void bmi_target_ready(struct hif_opaque_softc *scn, void *cfg_ctx)
{
	ol_target_ready(scn, cfg_ctx);
}

static QDF_STATUS
bmi_get_target_info_message_based(struct bmi_target_info *targ_info,
						struct ol_context *ol_ctx)
{
	int status = 0;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint8_t *bmi_rsp_buff = info->bmi_rsp_buff;
	uint32_t cid, length;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	if (!bmi_cmd_buff || !bmi_rsp_buff) {
		BMI_ERR("%s:BMI CMD/RSP Buffer is NULL", __func__);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	cid = BMI_GET_TARGET_INFO;

	qdf_mem_copy(bmi_cmd_buff, &cid, sizeof(cid));
	length = sizeof(struct bmi_target_info);

	status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff, sizeof(cid),
					(uint8_t *)bmi_rsp_buff, &length,
					BMI_EXCHANGE_TIMEOUT_MS);
	if (status) {
		BMI_ERR("Failed to target info: status:%d", status);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(targ_info, bmi_rsp_buff, length);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
bmi_get_target_info(struct bmi_target_info *targ_info,
						struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	QDF_STATUS status;

	if (info->bmi_done) {
		BMI_ERR("BMI Phase is Already Done");
		return QDF_STATUS_E_PERM;
	}

	switch (hif_get_bus_type(scn)) {
	case QDF_BUS_TYPE_PCI:
	case QDF_BUS_TYPE_SNOC:
	case QDF_BUS_TYPE_USB:
		status = bmi_get_target_info_message_based(targ_info, ol_ctx);
		break;
#ifdef HIF_SDIO
	case QDF_BUS_TYPE_SDIO:
		status = hif_reg_based_get_target_info(scn, targ_info);
		break;
#endif
	default:
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	return status;
}

QDF_STATUS bmi_download_firmware(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn;

	if (!ol_ctx) {
		if (NO_BMI) {
			/* ol_ctx is not allocated in NO_BMI case */
			return QDF_STATUS_SUCCESS;
		}

		BMI_ERR("ol_ctx is NULL");
		bmi_assert(0);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	scn = ol_ctx->scn;

	if (!scn) {
		BMI_ERR("Invalid scn context");
		bmi_assert(0);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	if (!hif_needs_bmi(scn))
		return QDF_STATUS_SUCCESS;
	else
		hif_register_bmi_callbacks(scn);

	return bmi_firmware_download(ol_ctx);
}

QDF_STATUS bmi_read_soc_register(uint32_t address, uint32_t *param,
						struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t cid;
	int status;
	uint32_t offset, param_len;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint8_t *bmi_rsp_buff = info->bmi_rsp_buff;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	bmi_assert(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
	qdf_mem_zero(bmi_cmd_buff, sizeof(cid) + sizeof(address));
	qdf_mem_zero(bmi_rsp_buff, sizeof(cid) + sizeof(address));

	if (info->bmi_done) {
		BMI_DBG("Command disallowed");
		return QDF_STATUS_E_PERM;
	}

	BMI_DBG("BMI Read SOC Register:device: 0x%pK, address: 0x%x",
			 scn, address);

	cid = BMI_READ_SOC_REGISTER;

	offset = 0;
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &address, sizeof(address));
	offset += sizeof(address);
	param_len = sizeof(*param);
	status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff, offset,
			bmi_rsp_buff, &param_len, BMI_EXCHANGE_TIMEOUT_MS);
	if (status) {
		BMI_DBG("Unable to read from the device; status:%d", status);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(param, bmi_rsp_buff, sizeof(*param));

	BMI_DBG("BMI Read SOC Register: Exit value: %d", *param);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS bmi_write_soc_register(uint32_t address, uint32_t param,
					struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t cid;
	int status;
	uint32_t offset;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint32_t size = sizeof(cid) + sizeof(address) + sizeof(param);
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	bmi_assert(BMI_COMMAND_FITS(size));
	qdf_mem_zero(bmi_cmd_buff, size);

	if (info->bmi_done) {
		BMI_DBG("Command disallowed");
		return QDF_STATUS_E_FAILURE;
	}

	BMI_DBG("SOC Register Write:device:0x%pK, addr:0x%x, param:%d",
						scn, address, param);

	cid = BMI_WRITE_SOC_REGISTER;

	offset = 0;
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &address, sizeof(address));
	offset += sizeof(address);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &param, sizeof(param));
	offset += sizeof(param);
	status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff, offset,
						NULL, NULL, 0);
	if (status) {
		BMI_ERR("Unable to write to the device: status:%d", status);
		return QDF_STATUS_E_FAILURE;
	}

	BMI_DBG("BMI Read SOC Register: Exit");
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
bmilz_data(uint8_t *buffer, uint32_t length, struct ol_context *ol_ctx)
{
	uint32_t cid;
	int status;
	uint32_t offset;
	uint32_t remaining, txlen;
	const uint32_t header = sizeof(cid) + sizeof(length);
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	bmi_assert(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
	qdf_mem_zero(bmi_cmd_buff, BMI_DATASZ_MAX + header);

	if (info->bmi_done) {
		BMI_ERR("Command disallowed");
		return QDF_STATUS_E_PERM;
	}

	BMI_DBG("BMI Send LZ Data: device: 0x%pK, length: %d",
						scn, length);

	cid = BMI_LZ_DATA;

	remaining = length;
	while (remaining) {
		txlen = (remaining < (BMI_DATASZ_MAX - header)) ?
			remaining : (BMI_DATASZ_MAX - header);
		offset = 0;
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &txlen, sizeof(txlen));
		offset += sizeof(txlen);
		qdf_mem_copy(&(bmi_cmd_buff[offset]),
			&buffer[length - remaining], txlen);
		offset += txlen;
		status = hif_exchange_bmi_msg(scn, cmd, rsp,
						bmi_cmd_buff, offset,
						NULL, NULL, 0);
		if (status) {
			BMI_ERR("Failed to write to the device: status:%d",
								status);
			return QDF_STATUS_E_FAILURE;
		}
		remaining -= txlen;
	}

	BMI_DBG("BMI LZ Data: Exit");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS bmi_sign_stream_start(uint32_t address, uint8_t *buffer,
				 uint32_t length, struct ol_context *ol_ctx)
{
	uint32_t cid;
	int status;
	uint32_t offset;
	const uint32_t header = sizeof(cid) + sizeof(address) + sizeof(length);
	uint8_t aligned_buf[BMI_DATASZ_MAX + 4];
	uint8_t *src;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint32_t remaining, txlen;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	bmi_assert(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
	qdf_mem_zero(bmi_cmd_buff, BMI_DATASZ_MAX + header);

	if (info->bmi_done) {
		BMI_ERR("Command disallowed");
		return QDF_STATUS_E_PERM;
	}

	BMI_ERR("Sign Stream start:device:0x%pK, addr:0x%x, length:%d",
						scn, address, length);

	cid = BMI_SIGN_STREAM_START;
	remaining = length;
	while (remaining) {
		src = &buffer[length - remaining];
		if (remaining < (BMI_DATASZ_MAX - header)) {
			if (remaining & 0x3) {
				memcpy(aligned_buf, src, remaining);
				remaining = remaining + (4 - (remaining & 0x3));
				src = aligned_buf;
			}
			txlen = remaining;
		} else {
			txlen = (BMI_DATASZ_MAX - header);
		}

		offset = 0;
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &address,
						sizeof(address));
		offset += sizeof(offset);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &txlen, sizeof(txlen));
		offset += sizeof(txlen);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), src, txlen);
		offset += txlen;
		status = hif_exchange_bmi_msg(scn, cmd, rsp,
						bmi_cmd_buff, offset, NULL,
						NULL, BMI_EXCHANGE_TIMEOUT_MS);
		if (status) {
			BMI_ERR("Unable to write to the device: status:%d",
								status);
			return QDF_STATUS_E_FAILURE;
		}
		remaining -= txlen;
	}
	BMI_DBG("BMI SIGN Stream Start: Exit");

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
bmilz_stream_start(uint32_t address, struct ol_context *ol_ctx)
{
	uint32_t cid;
	int status;
	uint32_t offset;
	struct hif_opaque_softc *scn = ol_ctx->scn;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	bmi_assert(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
	qdf_mem_zero(bmi_cmd_buff, sizeof(cid) + sizeof(address));

	if (info->bmi_done) {
		BMI_DBG("Command disallowed");
		return QDF_STATUS_E_PERM;
	}
	BMI_DBG("BMI LZ Stream Start: (device: 0x%pK, address: 0x%x)",
						scn, address);

	cid = BMI_LZ_STREAM_START;
	offset = 0;
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &address, sizeof(address));
	offset += sizeof(address);
	status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff, offset,
						NULL, NULL, 0);
	if (status) {
		BMI_ERR("Unable to Start LZ Stream to the device status:%d",
								status);
		return QDF_STATUS_E_FAILURE;
	}
	BMI_DBG("BMI LZ Stream: Exit");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
bmi_fast_download(uint32_t address, uint8_t *buffer,
		  uint32_t length, struct ol_context *ol_ctx)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t last_word = 0;
	uint32_t last_word_offset = length & ~0x3;
	uint32_t unaligned_bytes = length & 0x3;

	status = bmilz_stream_start(address, ol_ctx);
	if (status != QDF_STATUS_SUCCESS)
		goto end;

	/* copy the last word into a zero padded buffer */
	if (unaligned_bytes)
		qdf_mem_copy(&last_word, &buffer[last_word_offset],
						unaligned_bytes);

	status = bmilz_data(buffer, last_word_offset, ol_ctx);

	if (status != QDF_STATUS_SUCCESS)
		goto end;

	if (unaligned_bytes)
		status = bmilz_data((uint8_t *) &last_word, 4, ol_ctx);

	if (status != QDF_STATUS_SUCCESS)
		/*
		 * Close compressed stream and open a new (fake) one.
		 * This serves mainly to flush Target caches.
		 */
		status = bmilz_stream_start(0x00, ol_ctx);
end:
	return status;
}

/**
 * ol_cds_init() - API to initialize global CDS OL Context
 * @qdf_dev: QDF Device
 * @hif_ctx: HIF Context
 *
 * Return: Success/Failure
 */
QDF_STATUS ol_cds_init(qdf_device_t qdf_dev, void *hif_ctx)
{
	struct ol_context *ol_info;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (NO_BMI)
		return QDF_STATUS_SUCCESS; /* no BMI for Q6 bring up */

	status = cds_alloc_context(QDF_MODULE_ID_BMI,
				   (void **)&ol_info, sizeof(*ol_info));

	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("%s: CDS Allocation failed for ol_bmi context",
								__func__);
		return status;
	}

	ol_info->qdf_dev = qdf_dev;
	ol_info->scn = hif_ctx;
	ol_info->tgt_def.targetdef = hif_get_targetdef(hif_ctx);

	qdf_create_work(qdf_dev, &ol_info->ramdump_work,
			ramdump_work_handler, ol_info);
	qdf_create_work(qdf_dev, &ol_info->fw_indication_work,
			fw_indication_work_handler, ol_info);

	return status;
}

/**
 * ol_cds_free() - API to free the global CDS OL Context
 *
 * Return: void
 */
void ol_cds_free(void)
{
	struct ol_context *ol_info = cds_get_context(QDF_MODULE_ID_BMI);

	if (NO_BMI)
		return;

	cds_free_context(QDF_MODULE_ID_BMI, ol_info);
}
