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

#ifndef HIF_SDIO_INTERNAL_H_
#define HIF_SDIO_INTERNAL_H_

#include "a_debug.h"
#include "hif_sdio_dev.h"
#include "htc_packet.h"
#include "htc_api.h"


#define HIF_SDIO_RX_DATA_OFFSET            64

/* TODO: print output level and mask control */
#define ATH_DEBUG_IRQ  ATH_DEBUG_MAKE_MODULE_MASK(4)
#define ATH_DEBUG_XMIT ATH_DEBUG_MAKE_MODULE_MASK(5)
#define ATH_DEBUG_RECV ATH_DEBUG_MAKE_MODULE_MASK(6)

#define ATH_DEBUG_MAX_MASK 32

#define OTHER_INTS_ENABLED (INT_STATUS_ENABLE_ERROR_MASK |   \
                            INT_STATUS_ENABLE_CPU_MASK   |   \
                            INT_STATUS_ENABLE_COUNTER_MASK)

/* HTC operational parameters */
#define HTC_TARGET_RESPONSE_TIMEOUT        2000 /* in ms */
#define HTC_TARGET_DEBUG_INTR_MASK         0x01
#define HTC_TARGET_CREDIT_INTR_MASK        0xF0

#define MAILBOX_COUNT 4
#define MAILBOX_FOR_BLOCK_SIZE 1
#define MAILBOX_USED_COUNT 2
#if defined(SDIO_3_0)
#define MAILBOX_LOOKAHEAD_SIZE_IN_WORD 2
#else
#define MAILBOX_LOOKAHEAD_SIZE_IN_WORD 1
#endif
#define AR6K_TARGET_DEBUG_INTR_MASK     0x01

typedef PREPACK struct _MBOX_IRQ_PROC_REGISTERS {
    A_UINT8 host_int_status;
    A_UINT8 cpu_int_status;
    A_UINT8 error_int_status;
    A_UINT8 counter_int_status;
    A_UINT8 mbox_frame;
    A_UINT8 rx_lookahead_valid;
    A_UINT8 host_int_status2;
    A_UINT8 gmbox_rx_avail;
    A_UINT32 rx_lookahead[MAILBOX_LOOKAHEAD_SIZE_IN_WORD * MAILBOX_COUNT];
    A_UINT32 int_status_enable;
}POSTPACK MBOX_IRQ_PROC_REGISTERS;

typedef PREPACK struct _MBOX_IRQ_ENABLE_REGISTERS {
    A_UINT8 int_status_enable;
    A_UINT8 cpu_int_status_enable;
    A_UINT8 error_status_enable;
    A_UINT8 counter_int_status_enable;
}POSTPACK MBOX_IRQ_ENABLE_REGISTERS;

#define TOTAL_CREDIT_COUNTER_CNT 4

typedef PREPACK struct _MBOX_COUNTER_REGISTERS {
    A_UINT32 counter[TOTAL_CREDIT_COUNTER_CNT];
}POSTPACK MBOX_COUNTER_REGISTERS;

#define SDIO_NUM_DATA_RX_BUFFERS  64
#define SDIO_DATA_RX_SIZE         1664

struct TAG_HIF_SDIO_DEVICE {
    HIF_DEVICE *HIFDevice;
    A_MUTEX_T Lock;
    A_MUTEX_T TxLock;
    A_MUTEX_T RxLock;
    MBOX_IRQ_PROC_REGISTERS IrqProcRegisters;
    MBOX_IRQ_ENABLE_REGISTERS IrqEnableRegisters;
    MBOX_COUNTER_REGISTERS MailBoxCounterRegisters;
    MSG_BASED_HIF_CALLBACKS hif_callbacks;
    HIF_DEVICE_MBOX_INFO MailBoxInfo;
    A_UINT32 BlockSize;
    A_UINT32 BlockMask;
    HIF_DEVICE_IRQ_PROCESSING_MODE HifIRQProcessingMode;
    HIF_DEVICE_IRQ_YIELD_PARAMS HifIRQYieldParams;
    A_BOOL DSRCanYield;
    HIF_MASK_UNMASK_RECV_EVENT HifMaskUmaskRecvEvent;
    int CurrentDSRRecvCount;
    int RecheckIRQStatusCnt;
    A_UINT32 RecvStateFlags;
	void *pTarget;
};

#define LOCK_HIF_DEV(device)    A_MUTEX_LOCK(&(device)->Lock);
#define UNLOCK_HIF_DEV(device)  A_MUTEX_UNLOCK(&(device)->Lock);
#define LOCK_HIF_DEV_RX(t)      A_MUTEX_LOCK(&(t)->RxLock);
#define UNLOCK_HIF_DEV_RX(t)    A_MUTEX_UNLOCK(&(t)->RxLock);
#define LOCK_HIF_DEV_TX(t)      A_MUTEX_LOCK(&(t)->TxLock);
#define UNLOCK_HIF_DEV_TX(t)    A_MUTEX_UNLOCK(&(t)->TxLock);

#define DEV_CALC_RECV_PADDED_LEN(pDev, length) (((length) + (pDev)->BlockMask) & (~((pDev)->BlockMask)))
#define DEV_CALC_SEND_PADDED_LEN(pDev, length) DEV_CALC_RECV_PADDED_LEN(pDev,length)
#define DEV_IS_LEN_BLOCK_ALIGNED(pDev, length) (((length) % (pDev)->BlockSize) == 0)

#define HTC_RECV_WAIT_BUFFERS        (1 << 0)
#define HTC_OP_STATE_STOPPING        (1 << 0)

#define HTC_RX_PKT_IGNORE_LOOKAHEAD                         (1 << 0)
#define HTC_RX_PKT_REFRESH_HDR                              (1 << 1)
#define HTC_RX_PKT_PART_OF_BUNDLE                           (1 << 2)
#define HTC_RX_PKT_NO_RECYCLE                               (1 << 3)
#define HTC_RX_PKT_LAST_BUNDLED_PKT_HAS_ADDTIONAL_BLOCK     (1 << 4)

#define IS_DEV_IRQ_PROCESSING_ASYNC_ALLOWED(pDev) ((pDev)->HifIRQProcessingMode != HIF_DEVICE_IRQ_SYNC_ONLY)

/* hif_sdio_dev.c */
HTC_PACKET *HIFDevAllocRxBuffer(HIF_SDIO_DEVICE *pDev, size_t length);

A_UINT8 HIFDevMapPipeToMailBox(HIF_SDIO_DEVICE *pDev, A_UINT8 pipeid);
A_UINT8 HIFDevMapMailBoxToPipe(HIF_SDIO_DEVICE *pDev, A_UINT8 mboxIndex,
        A_BOOL upload);

/* hif_sdio_recv.c */
A_STATUS HIFDevRWCompletionHandler(void *context, A_STATUS status);
A_STATUS HIFDevDsrHandler(void *context);

#endif /* HIF_SDIO_INTERNAL_H_ */
