/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef HIF_SDIO_DEV_H_
#define HIF_SDIO_DEV_H_

#include "adf_net_types.h"
#include "a_types.h"
#include "athdefs.h"
#include "a_osapi.h"
#include "hif_msg_based.h"
#include "athstartpack.h"

typedef struct TAG_HIF_SDIO_DEVICE HIF_SDIO_DEVICE;

HIF_SDIO_DEVICE* HIFDevFromHIF(HIF_DEVICE *hif_device);

HIF_SDIO_DEVICE* HIFDevCreate(HIF_DEVICE *hif_device,
        MSG_BASED_HIF_CALLBACKS *callbacks,
        void *target);

void HIFDevDestroy(HIF_SDIO_DEVICE *htc_sdio_device);

A_STATUS HIFDevSetup(HIF_SDIO_DEVICE *htc_sdio_device);

A_STATUS HIFDevEnableInterrupts(HIF_SDIO_DEVICE *htc_sdio_device);

A_STATUS HIFDevDisableInterrupts(HIF_SDIO_DEVICE *htc_sdio_device);

A_STATUS HIFDevSendBuffer(HIF_SDIO_DEVICE *htc_sdio_device, unsigned int transferID, a_uint8_t pipe,
        unsigned int nbytes, adf_nbuf_t buf);

A_STATUS HIFDevMapServiceToPipe(HIF_SDIO_DEVICE *pDev, A_UINT16 ServiceId,
        A_UINT8 *ULPipe, A_UINT8 *DLPipe);

#endif /* HIF_SDIO_DEV_H_ */
