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

QDF_STATUS
bmi_read_memory(uint32_t address,
		uint8_t *buffer, uint32_t length, struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t cid;
	int status;
	uint32_t offset;
	uint32_t remaining, rxlen;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint8_t *bmi_rsp_buff = info->bmi_rsp_buff;
	uint32_t align;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	if (info->bmi_done) {
		BMI_DBG("command disallowed");
		return QDF_STATUS_E_PERM;
	}

	if (!info->bmi_cmd_buff || !info->bmi_rsp_buff) {
		BMI_ERR("BMI Initialization hasn't done");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	bmi_assert(BMI_COMMAND_FITS(BMI_DATASZ_MAX + sizeof(cid) +
			sizeof(address) + sizeof(length)));
	qdf_mem_zero(bmi_cmd_buff, BMI_DATASZ_MAX + sizeof(cid) +
			sizeof(address) + sizeof(length));
	qdf_mem_zero(bmi_rsp_buff, BMI_DATASZ_MAX + sizeof(cid) +
			sizeof(address) + sizeof(length));

	cid = BMI_READ_MEMORY;
	align = 0;
	remaining = length;

	while (remaining) {
		rxlen = (remaining < BMI_DATASZ_MAX) ?
				remaining : BMI_DATASZ_MAX;
		offset = 0;
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
		offset += sizeof(cid);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &address,
						sizeof(address));
		offset += sizeof(address);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &rxlen, sizeof(rxlen));
		offset += sizeof(length);

		/* note we reuse the same buffer to receive on */
		status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff,
						offset, bmi_rsp_buff, &rxlen,
						BMI_EXCHANGE_TIMEOUT_MS);
		if (status) {
			BMI_ERR("Unable to read from the device");
			return QDF_STATUS_E_FAILURE;
		}
		if (remaining == rxlen) {
			qdf_mem_copy(&buffer[length - remaining + align],
					bmi_rsp_buff, rxlen - align);
			/* last align bytes are invalid */
		} else {
			qdf_mem_copy(&buffer[length - remaining + align],
				 bmi_rsp_buff, rxlen);
		}
		remaining -= rxlen;
		address += rxlen;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS bmi_write_memory(uint32_t address, uint8_t *buffer, uint32_t length,
						struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t cid;
	int status;
	uint32_t offset;
	uint32_t remaining, txlen;
	const uint32_t header = sizeof(cid) + sizeof(address) + sizeof(length);
	uint8_t aligned_buffer[BMI_DATASZ_MAX];
	uint8_t *src;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	if (info->bmi_done) {
		BMI_ERR("Command disallowed");
		return QDF_STATUS_E_PERM;
	}

	if (!bmi_cmd_buff) {
		BMI_ERR("BMI initialization hasn't done");
		return QDF_STATUS_E_PERM;
	}

	bmi_assert(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
	qdf_mem_zero(bmi_cmd_buff, BMI_DATASZ_MAX + header);

	cid = BMI_WRITE_MEMORY;

	remaining = length;
	while (remaining) {
		src = &buffer[length - remaining];
		if (remaining < (BMI_DATASZ_MAX - header)) {
			if (remaining & 3) {
				/* align it with 4 bytes */
				remaining = remaining + (4 - (remaining & 3));
				memcpy(aligned_buffer, src, remaining);
				src = aligned_buffer;
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
		offset += sizeof(address);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), &txlen, sizeof(txlen));
		offset += sizeof(txlen);
		qdf_mem_copy(&(bmi_cmd_buff[offset]), src, txlen);
		offset += txlen;
		status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff,
						offset, NULL, NULL,
						BMI_EXCHANGE_TIMEOUT_MS);
		if (status) {
			BMI_ERR("Unable to write to the device; status:%d",
								status);
			return QDF_STATUS_E_FAILURE;
		}
		remaining -= txlen;
		address += txlen;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
bmi_execute(uint32_t address, A_UINT32 *param, struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	uint32_t cid;
	int status;
	uint32_t offset;
	uint32_t param_len;
	struct bmi_info *info = GET_BMI_CONTEXT(ol_ctx);
	uint8_t *bmi_cmd_buff = info->bmi_cmd_buff;
	uint8_t *bmi_rsp_buff = info->bmi_rsp_buff;
	uint32_t size = sizeof(cid) + sizeof(address) + sizeof(param);
	qdf_dma_addr_t cmd = info->bmi_cmd_da;
	qdf_dma_addr_t rsp = info->bmi_rsp_da;

	if (info->bmi_done) {
		BMI_ERR("Command disallowed");
		return QDF_STATUS_E_PERM;
	}

	if (!bmi_cmd_buff || !bmi_rsp_buff) {
		BMI_ERR("%s:BMI CMD/RSP Buffer is NULL", __func__);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	bmi_assert(BMI_COMMAND_FITS(size));
	qdf_mem_zero(bmi_cmd_buff, size);
	qdf_mem_zero(bmi_rsp_buff, size);


	BMI_DBG("BMI Execute: device: 0x%pK, address: 0x%x, param: %d",
						scn, address, *param);

	cid = BMI_EXECUTE;

	offset = 0;
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &cid, sizeof(cid));
	offset += sizeof(cid);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), &address, sizeof(address));
	offset += sizeof(address);
	qdf_mem_copy(&(bmi_cmd_buff[offset]), param, sizeof(*param));
	offset += sizeof(*param);
	param_len = sizeof(*param);
	status = hif_exchange_bmi_msg(scn, cmd, rsp, bmi_cmd_buff, offset,
					bmi_rsp_buff, &param_len, 0);
	if (status) {
		BMI_ERR("Unable to read from the device status:%d", status);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(param, bmi_rsp_buff, sizeof(*param));

	BMI_DBG("BMI Execute: Exit (param: %d)", *param);
	return QDF_STATUS_SUCCESS;
}

inline QDF_STATUS
bmi_no_command(struct ol_context *ol_ctx)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
bmi_firmware_download(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	QDF_STATUS status;
	struct bmi_target_info targ_info;
	struct hif_target_info *tgt_info = hif_get_target_info_handle(scn);

	qdf_mem_zero(&targ_info, sizeof(targ_info));
	/* Initialize BMI */
	status = bmi_init(ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("BMI Initialization Failed err:%d", status);
		return status;
	}

	/* Get target information */
	status = bmi_get_target_info(&targ_info, ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("BMI Target Info get failed: status:%d", status);
		return status;
	}

	tgt_info->target_type = targ_info.target_type;
	tgt_info->target_version = targ_info.target_ver;
	/* Configure target */
	status = ol_configure_target(ol_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		BMI_ERR("BMI Configure Target Failed status:%d", status);
		return status;
	}
	status = ol_download_firmware(ol_ctx);
	if (status != QDF_STATUS_SUCCESS)
		BMI_ERR("BMI Download Firmware Failed Status:%d", status);

	return status;
}

QDF_STATUS bmi_done_local(struct ol_context *ol_ctx)
{
	struct hif_opaque_softc *scn = ol_ctx->scn;
	int status;
	uint32_t cid;
	struct bmi_info *info;
	qdf_device_t qdf_dev = ol_ctx->qdf_dev;
	qdf_dma_addr_t cmd, rsp;

	if (!scn) {
		BMI_ERR("Invalid scn context");
		bmi_assert(0);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	if (!qdf_dev->dev) {
		BMI_ERR("%s: Invalid device pointer", __func__);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	info = GET_BMI_CONTEXT(ol_ctx);
	if (info->bmi_done) {
		BMI_DBG(FL("skipped"));
		return QDF_STATUS_E_PERM;
	}

	cmd = info->bmi_cmd_da;
	rsp = info->bmi_rsp_da;

	BMI_DBG("BMI Done: Enter (device: 0x%pK)", scn);

	info->bmi_done = true;
	cid = BMI_DONE;

	if (!info->bmi_cmd_buff) {
		BMI_ERR("Invalid scn BMICmdBuff");
		bmi_assert(0);
		return QDF_STATUS_NOT_INITIALIZED;
	}

	qdf_mem_copy(info->bmi_cmd_buff, &cid, sizeof(cid));

	status = hif_exchange_bmi_msg(scn, cmd, rsp, info->bmi_cmd_buff,
				sizeof(cid), NULL, NULL, 0);
	if (status) {
		BMI_ERR("Failed to write to the device; status:%d", status);
		return QDF_STATUS_E_FAILURE;
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

	return QDF_STATUS_SUCCESS;
}
