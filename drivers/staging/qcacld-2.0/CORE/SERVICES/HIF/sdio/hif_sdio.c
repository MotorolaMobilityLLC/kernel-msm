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

#include <adf_os_types.h>
#include <adf_os_dma.h>
#include <adf_os_timer.h>
#include <adf_os_time.h>
#include <adf_os_lock.h>
#include <adf_os_io.h>
#include <adf_os_mem.h>
#include <adf_os_module.h>
#include <adf_os_util.h>
#include <adf_os_stdtypes.h>
#include <adf_os_defer.h>
#include <adf_os_atomic.h>
#include <adf_nbuf.h>
#include <athdefs.h>
#include "adf_net_types.h"
#include "a_types.h"
#include "athdefs.h"
#include "a_osapi.h"
#include <hif.h>
#include <htc_services.h>
#include <a_debug.h>
#include "hif_sdio_dev.h"
#include "if_ath_sdio.h"
#include "regtable.h"

#define ATH_MODULE_NAME hif_sdio

unsigned int htc_credit_flow = 1;

A_STATUS HIFStart(HIF_DEVICE *hif_device)
{
   HIF_SDIO_DEVICE *htc_sdio_device = HIFDevFromHIF(hif_device);
   ENTER();
   HIFDevEnableInterrupts(htc_sdio_device);
   EXIT();
   return A_OK;
}

void
HIFFlushSurpriseRemove(HIF_DEVICE *hif_device)
{

}

void HIFStop(HIF_DEVICE *hif_device)
{
    HIF_SDIO_DEVICE *htc_sdio_device = HIFDevFromHIF(hif_device);
    ENTER();
    if (htc_sdio_device != NULL) {
        HIFDevDisableInterrupts(htc_sdio_device);
        HIFDevDestroy(htc_sdio_device);
    }
    EXIT();
}

A_STATUS HIFSend_head(HIF_DEVICE *hif_device, a_uint8_t pipe,
        unsigned int transferID, unsigned int nbytes, adf_nbuf_t buf)
{
    HIF_SDIO_DEVICE *htc_sdio_device = HIFDevFromHIF(hif_device);
    return HIFDevSendBuffer(htc_sdio_device, transferID, pipe, nbytes, buf);

}

int HIFMapServiceToPipe(HIF_DEVICE *hif_device, a_uint16_t ServiceId,
        a_uint8_t *ULPipe, a_uint8_t *DLPipe, int *ul_is_polled,
        int *dl_is_polled)
{
    HIF_SDIO_DEVICE *htc_sdio_device = HIFDevFromHIF(hif_device);

    return HIFDevMapServiceToPipe(htc_sdio_device, ServiceId, ULPipe, DLPipe);
}

void HIFGetDefaultPipe(HIF_DEVICE *hif_device, a_uint8_t *ULPipe,
        a_uint8_t *DLPipe)
{
    HIFMapServiceToPipe(hif_device,
            HTC_CTRL_RSVD_SVC,
            ULPipe,
            DLPipe,
            NULL,
            NULL);
}

void HIFPostInit(HIF_DEVICE *hif_device, void *target,
        MSG_BASED_HIF_CALLBACKS *callbacks)
{
    HIF_SDIO_DEVICE *htc_sdio_device = HIFDevFromHIF(hif_device);
    if (htc_sdio_device == NULL) {
        htc_sdio_device = HIFDevCreate(hif_device, callbacks, target);
    }

    if (htc_sdio_device)
    HIFDevSetup(htc_sdio_device);

    return;
}


/*SDIO uses credit based flow control at the HTC layer so transmit resource checks are
bypassed. This API is relevant to USB where there may be a limited number of USB request
blocks (URBs) in the HIF*/
a_uint16_t HIFGetFreeQueueNumber(HIF_DEVICE *hif_device, a_uint8_t pipe)
{
    a_uint16_t rv;
    rv = 1;
    return rv;
}

void HIFSendCompleteCheck(HIF_DEVICE *hif_device, a_uint8_t pipe, int force)
{

}

void *hif_get_targetdef(HIF_DEVICE *hif_device)
{
   return (void*)sc->targetdef;
}

void HIFDump(HIF_DEVICE *hif_device, u_int8_t cmd_id, bool start) {
    ENTER("Dummy Function");
}
