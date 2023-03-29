/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
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

/* ================================================================ */
/* BMI declarations and prototypes */
/* */
/* ================================================================= */

#ifndef _BMI_H_
#define _BMI_H_
#include "bmi_msg.h"
#include "qdf_trace.h"
#include "ol_if_athvar.h"
#include "hif.h"

struct ol_context;

/**
 * struct hif_config_info - Place Holder for hif confiruation
 * @enable_uart_print: UART Print
 * @enable_self_recovery: Self Recovery
 * @enable_fw_log:      To Enable FW LOG
 * @enable_lpass_support: LPASS support
 * @enable_ramdump_collection: Ramdump Collection
 *
 * Structure for holding ini parameters.
 */

struct ol_config_info {
	bool enable_uart_print;
	bool enable_self_recovery;
	uint8_t enable_fw_log;
	bool enable_lpass_support;
	bool enable_ramdump_collection;
};

#ifdef WLAN_FEATURE_BMI
QDF_STATUS ol_cds_init(qdf_device_t qdf_dev, void *hif_ctx);
void ol_cds_free(void);
void ol_init_ini_config(struct ol_context *ol_ctx,
			struct ol_config_info *cfg);
/**
 * ol_set_fw_crashed_cb() - set firmware crashed callback
 * @ol_ctx: ol context
 * @callback_fn: fw crashed callback function
 *
 * Return: None
 */
void ol_set_fw_crashed_cb(struct ol_context *ol_ctx,
			  void (*callback_fn)(void));
void bmi_cleanup(struct ol_context *scn);
QDF_STATUS bmi_done(struct ol_context *ol_ctx);
void bmi_target_ready(struct hif_opaque_softc *scn, void *cfg_ctx);
QDF_STATUS bmi_download_firmware(struct ol_context *ol_ctx);

#else /* WLAN_FEATURE_BMI */

static inline QDF_STATUS
ol_cds_init(qdf_device_t qdf_dev, void *hif_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ol_cds_free(void)
{
}

static inline void
ol_init_ini_config(struct ol_context *ol_ctx, struct ol_config_info *cfg)
{
}

static inline void
ol_set_fw_crashed_cb(struct ol_context *ol_ctx, void (*callback_fn)(void))
{
}

static inline void bmi_cleanup(struct ol_context *scn)
{
}

static inline QDF_STATUS bmi_done(struct ol_context *ol_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
bmi_target_ready(struct hif_opaque_softc *scn, void *cfg_ctx)
{
}

static inline QDF_STATUS
bmi_download_firmware(struct ol_context *ol_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_BMI */

#endif /* _BMI_H_ */
