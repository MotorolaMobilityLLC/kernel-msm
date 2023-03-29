/*
 * Copyright (c) 2014-2016, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _OL_FW_H_
#define _OL_FW_H_

#include "qdf_types.h"
#include "hif.h"
#include "hif_hw_version.h"
#include "bmi.h"

#define AR6320_REV2_VERSION          AR6320_REV1_1_VERSION
#define AR6320_REV4_VERSION          AR6320_REV2_1_VERSION
#define SIGN_HEADER_MAGIC            0x454D4F52
#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_SET          (1 << 0)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_SET       (1 << 1)
#define HI_ACS_FLAGS_ALT_DATA_CREDIT_SIZE           (1 << 2)

#define HI_ACS_FLAGS_SDIO_SWAP_MAILBOX_FW_ACK       (1 << 16)
#define HI_ACS_FLAGS_SDIO_REDUCE_TX_COMPL_FW_ACK    (1 << 17)

#ifdef WLAN_FEATURE_BMI
void ol_target_failure(void *instance, QDF_STATUS status);

void ol_target_ready(struct hif_opaque_softc *scn, void *cfg_ctx);
QDF_STATUS ol_extra_initialization(struct ol_context *ol_ctx);
#else /* WLAN_FEATURE_BMI */
static inline void ol_target_failure(void *instance, QDF_STATUS status) {}
#endif /* WLAN_FEATURE_BMI */
#endif /* _OL_FW_H_ */
