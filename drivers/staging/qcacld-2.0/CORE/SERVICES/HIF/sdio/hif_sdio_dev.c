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

#define ATH_MODULE_NAME hif
#include "a_debug.h"

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
#include <adf_net_types.h>
#include <a_types.h>
#include <athdefs.h>
#include <a_osapi.h>
#include <hif.h>
#include <htc_services.h>
#include "hif_sdio_internal.h"
#include "if_ath_sdio.h"
#include "regtable.h"

/* under HL SDIO, with Interface Memory support, we have the following
 * reasons to support 2 mboxs: a) we need place different buffers in different
 * mempool, for example, data using Interface Memory, desc and other using
 * DRAM, they need different SDIO mbox channels; b) currently, tx mempool in
 * LL case is seperated from main mempool, the structure (descs at the beginning
 * of every pool buffer) is different, because they only need store tx desc from host.
 * To align with LL case, we also need 2 mbox support just as PCIe LL cases.
 */

#define INVALID_MAILBOX_NUMBER 0xFF
A_UINT8 HIFDevMapPipeToMailBox(HIF_SDIO_DEVICE *pDev, A_UINT8 pipeid)
{
    // TODO: temp version, should not hardcoded here, will be updated after HIF design
    if (2 == pipeid || 3 == pipeid)
        return 1;
    else if (0 == pipeid || 1 == pipeid)
        return 0;
    else {
        printk("%s:--------------------pipeid=%d,should not happen\n",__func__,pipeid);
        adf_os_assert(0);
        return INVALID_MAILBOX_NUMBER;
    }
}

A_UINT8 HIFDevMapMailBoxToPipe(HIF_SDIO_DEVICE *pDev, A_UINT8 mboxIndex,
        A_BOOL upload)
{
    // TODO: temp version, should not hardcoded here, will be updated after HIF design
    if (mboxIndex == 0) {
        return upload ? 1 : 0;
    } else if (mboxIndex == 1) {
        return upload ? 3 : 2;
    } else {
        printk("%s:--------------------mboxIndex=%d,upload=%d,should not happen\n",__func__,mboxIndex,upload);
        adf_os_assert(0);
        return 0xff;
    }
}

A_STATUS HIFDevMapServiceToPipe(HIF_SDIO_DEVICE *pDev, A_UINT16 ServiceId,
        A_UINT8 *ULPipe, A_UINT8 *DLPipe)
{
    A_STATUS status = EOK;
    switch (ServiceId) {
    case HTT_DATA_MSG_SVC:
        *ULPipe = 3;
        *DLPipe = 2;
        break;

    case HTC_CTRL_RSVD_SVC:
    case HTC_RAW_STREAMS_SVC:
        *ULPipe = 1;
        *DLPipe = 0;
        break;

    case WMI_DATA_BE_SVC:
    case WMI_DATA_BK_SVC:
    case WMI_DATA_VI_SVC:
    case WMI_DATA_VO_SVC:
        *ULPipe = 1;
        *DLPipe = 0;
        break;

    case WMI_CONTROL_SVC:
        *ULPipe = 1;
        *DLPipe = 0;
        break;

    default:
        status = !EOK;
        break;
    }
    return status;
}

HTC_PACKET *HIFDevAllocRxBuffer(HIF_SDIO_DEVICE *pDev)
{
    HTC_PACKET *pPacket;
    adf_nbuf_t netbuf;
    A_UINT32 bufsize = 0, headsize = 0;

    bufsize = HIF_SDIO_RX_BUFFER_SIZE + HIF_SDIO_RX_DATA_OFFSET;
    headsize = sizeof(HTC_PACKET);
    netbuf = adf_nbuf_alloc(NULL, bufsize + headsize, 0, 4, FALSE);
    if (netbuf == NULL) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("(%s)Allocate netbuf failed\n", __FUNCTION__));
        return NULL;
    }
    pPacket = (HTC_PACKET *) adf_nbuf_data(netbuf);
    adf_nbuf_reserve(netbuf, headsize);

    SET_HTC_PACKET_INFO_RX_REFILL(pPacket,
            pDev,
            adf_nbuf_data(netbuf),
            bufsize,
            ENDPOINT_0);
    SET_HTC_PACKET_NET_BUF_CONTEXT(pPacket, netbuf);
    return pPacket;
}

HIF_SDIO_DEVICE* HIFDevCreate(HIF_DEVICE *hif_device,
        MSG_BASED_HIF_CALLBACKS *callbacks,
        void *target)
{

    A_STATUS status;
    HIF_SDIO_DEVICE *pDev;

    pDev = A_MALLOC(sizeof(HIF_SDIO_DEVICE));
    if (!pDev) {
        A_ASSERT(FALSE);
        return NULL;
    }

    A_MEMZERO(pDev, sizeof(HIF_SDIO_DEVICE));
    A_MUTEX_INIT(&pDev->Lock);
    A_MUTEX_INIT(&pDev->TxLock);
    A_MUTEX_INIT(&pDev->RxLock);

    pDev->HIFDevice = hif_device;
    pDev->pTarget = target;
    status = HIFConfigureDevice(hif_device,
            HIF_DEVICE_SET_HTC_CONTEXT,
            (void*) pDev,
            sizeof(pDev));
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("(%s)HIF_DEVICE_SET_HTC_CONTEXT failed!!!\n", __FUNCTION__));
    }

    A_MEMCPY(&pDev->hif_callbacks, callbacks, sizeof(*callbacks));

    return pDev;
}

void HIFDevDestroy(HIF_SDIO_DEVICE *pDev)
{
    A_STATUS status;

    status = HIFConfigureDevice(pDev->HIFDevice,
            HIF_DEVICE_SET_HTC_CONTEXT,
            (void*) NULL,
            0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("(%s)HIF_DEVICE_SET_HTC_CONTEXT failed!!!\n", __FUNCTION__));
    }
    A_FREE(pDev);
}

HIF_SDIO_DEVICE *HIFDevFromHIF(HIF_DEVICE *hif_device)
{
    HIF_SDIO_DEVICE *pDev = NULL;
    A_STATUS status;
    status = HIFConfigureDevice(hif_device,
            HIF_DEVICE_GET_HTC_CONTEXT,
            (void**) &pDev,
            sizeof(HIF_SDIO_DEVICE));
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("(%s)HTC_SDIO_CONTEXT is NULL!!!\n", __FUNCTION__));
    }
    return pDev;
}

A_STATUS HIFDevDisableInterrupts(HIF_SDIO_DEVICE *pDev)
{
    MBOX_IRQ_ENABLE_REGISTERS regs;
    A_STATUS    status = A_OK;
    ENTER();

    LOCK_HIF_DEV(pDev);
    /* Disable all interrupts */
    pDev->IrqEnableRegisters.int_status_enable = 0;
    pDev->IrqEnableRegisters.cpu_int_status_enable = 0;
    pDev->IrqEnableRegisters.error_status_enable = 0;
    pDev->IrqEnableRegisters.counter_int_status_enable = 0;
    /* copy into our temp area */
    A_MEMCPY(&regs,
            &pDev->IrqEnableRegisters,
            sizeof(pDev->IrqEnableRegisters));

    UNLOCK_HIF_DEV(pDev);

    /* always synchronous */
    status = HIFReadWrite(pDev->HIFDevice,
            INT_STATUS_ENABLE_ADDRESS,
            (A_UCHAR *)&regs,
            sizeof(MBOX_IRQ_ENABLE_REGISTERS),
            HIF_WR_SYNC_BYTE_INC,
            NULL);

    if (status != A_OK) {
        /* Can't write it for some reason */
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("Failed to update interrupt control registers err: %d", status));
    }

    EXIT("status :%d",status);
    return status;
}

A_STATUS HIFDevEnableInterrupts(HIF_SDIO_DEVICE *pDev)
{
    A_STATUS status;
    MBOX_IRQ_ENABLE_REGISTERS regs;
    ENTER();


    /* for good measure, make sure interrupt are disabled before unmasking at the HIF
     * layer.
     * The rationale here is that between device insertion (where we clear the interrupts the first time)
     * and when HTC is finally ready to handle interrupts, other software can perform target "soft" resets.
     * The AR6K interrupt enables reset back to an "enabled" state when this happens.
     *  */
    HIFDevDisableInterrupts(pDev);

    /* Unmask the host controller interrupts */
    HIFUnMaskInterrupt(pDev->HIFDevice);

    LOCK_HIF_DEV(pDev);

    /* Enable all the interrupts except for the internal AR6000 CPU interrupt */
    pDev->IrqEnableRegisters.int_status_enable =
            INT_STATUS_ENABLE_ERROR_SET(0x01) | INT_STATUS_ENABLE_CPU_SET(0x01)
                    | INT_STATUS_ENABLE_COUNTER_SET(0x01);

    pDev->IrqEnableRegisters.int_status_enable |=
            INT_STATUS_ENABLE_MBOX_DATA_SET(0x01) | INT_STATUS_ENABLE_MBOX_DATA_SET(0x02); // enable 2 mboxs INT

    /* Set up the CPU Interrupt Status Register, enable CPU sourced interrupt #0, #1
     * #0 is used for report assertion from target
     * #1 is used for inform host that credit arrived
     * */
    pDev->IrqEnableRegisters.cpu_int_status_enable = 0x03;

    /* Set up the Error Interrupt Status Register */
    pDev->IrqEnableRegisters.error_status_enable =
            (ERROR_STATUS_ENABLE_RX_UNDERFLOW_SET(0x01)
                    | ERROR_STATUS_ENABLE_TX_OVERFLOW_SET(0x01)) >> 16;

    /* Set up the Counter Interrupt Status Register (only for debug interrupt to catch fatal errors) */
    pDev->IrqEnableRegisters.counter_int_status_enable =
            (COUNTER_INT_STATUS_ENABLE_BIT_SET(AR6K_TARGET_DEBUG_INTR_MASK)) >> 24;

    /* copy into our temp area */
    A_MEMCPY(&regs,
            &pDev->IrqEnableRegisters,
            sizeof(MBOX_IRQ_ENABLE_REGISTERS));

    UNLOCK_HIF_DEV(pDev);


    /* always synchronous */
    status = HIFReadWrite(pDev->HIFDevice,
            INT_STATUS_ENABLE_ADDRESS,
            (A_UCHAR *)&regs,
            sizeof(MBOX_IRQ_ENABLE_REGISTERS),
            HIF_WR_SYNC_BYTE_INC,
            NULL);

    if (status != A_OK) {
        /* Can't write it for some reason */
        AR_DEBUG_PRINTF( ATH_DEBUG_ERR,
                ("Failed to update interrupt control registers err: %d\n", status));

    }
    EXIT();
    return status;
}

A_STATUS HIFDevSetup(HIF_SDIO_DEVICE *pDev)
{
    A_STATUS status;
    A_UINT32 blocksizes[MAILBOX_COUNT];
    HTC_CALLBACKS htcCallbacks;
    HIF_DEVICE *hif_device = pDev->HIFDevice;

    ENTER();

    status = HIFConfigureDevice(hif_device,
            HIF_DEVICE_GET_MBOX_ADDR,
            &pDev->MailBoxInfo,
            sizeof(pDev->MailBoxInfo));

    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                ("(%s)HIF_DEVICE_GET_MBOX_ADDR failed!!!\n", __FUNCTION__));
        A_ASSERT(FALSE);
    }

    status = HIFConfigureDevice(hif_device,
            HIF_DEVICE_GET_MBOX_BLOCK_SIZE,
            blocksizes,
            sizeof(blocksizes));
    if (status != A_OK) {
        AR_DEBUG_PRINTF( ATH_DEBUG_ERR,
                ("(%s)HIF_DEVICE_GET_MBOX_BLOCK_SIZE failed!!!\n", __FUNCTION__));
        A_ASSERT(FALSE);
    }

    pDev->BlockSize = blocksizes[MAILBOX_FOR_BLOCK_SIZE];
    pDev->BlockMask = pDev->BlockSize - 1;
    A_ASSERT((pDev->BlockSize & pDev->BlockMask) == 0);

    /* assume we can process HIF interrupt events asynchronously */
    pDev->HifIRQProcessingMode = HIF_DEVICE_IRQ_ASYNC_SYNC;

    /* see if the HIF layer overrides this assumption */
    HIFConfigureDevice(hif_device,
            HIF_DEVICE_GET_IRQ_PROC_MODE,
            &pDev->HifIRQProcessingMode,
            sizeof(pDev->HifIRQProcessingMode));

    switch (pDev->HifIRQProcessingMode) {
    case HIF_DEVICE_IRQ_SYNC_ONLY:
        AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                ("HIF Interrupt processing is SYNC ONLY\n"));
        /* see if HIF layer wants HTC to yield */
        HIFConfigureDevice(hif_device,
                HIF_DEVICE_GET_IRQ_YIELD_PARAMS,
                &pDev->HifIRQYieldParams,
                sizeof(pDev->HifIRQYieldParams));

        if (pDev->HifIRQYieldParams.RecvPacketYieldCount > 0) {
            AR_DEBUG_PRINTF( ATH_DEBUG_WARN,
                    ("HIF requests that DSR yield per %d RECV packets \n", pDev->HifIRQYieldParams.RecvPacketYieldCount));
            pDev->DSRCanYield = TRUE;
        }
        break;
    case HIF_DEVICE_IRQ_ASYNC_SYNC:
        AR_DEBUG_PRINTF(ATH_DEBUG_TRC,
                ("HIF Interrupt processing is ASYNC and SYNC\n"));
        break;
    default:
        A_ASSERT(FALSE);
        break;
    }

    pDev->HifMaskUmaskRecvEvent = NULL;

    /* see if the HIF layer implements the mask/unmask recv events function  */
    HIFConfigureDevice(hif_device,
            HIF_DEVICE_GET_RECV_EVENT_MASK_UNMASK_FUNC,
            &pDev->HifMaskUmaskRecvEvent,
            sizeof(pDev->HifMaskUmaskRecvEvent));

    status = HIFDevDisableInterrupts(pDev);

    A_MEMZERO(&htcCallbacks, sizeof(HTC_CALLBACKS));
    /* the device layer handles these */
    htcCallbacks.rwCompletionHandler = HIFDevRWCompletionHandler;
    htcCallbacks.dsrHandler = HIFDevDsrHandler;
    htcCallbacks.context = pDev;
    status = HIFAttachHTC(pDev->HIFDevice, &htcCallbacks);

    EXIT();
    return status;
}

