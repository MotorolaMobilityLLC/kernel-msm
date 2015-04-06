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
#include <adf_net_types.h>
#include <a_types.h>
#include <athdefs.h>
#include <a_osapi.h>
#include <hif.h>
#include <htc_services.h>
#include <a_debug.h>
#include "hif_sdio_internal.h"

/*
 * Data structure to record required sending context data
 */
struct HIFSendContext
{
    A_BOOL bNewAlloc;
    HIF_SDIO_DEVICE *pDev;
    adf_nbuf_t netbuf;
    unsigned int transferID;
    unsigned int head_data_len;
};

/*
 * Completion routine for ALL HIF layer async I/O
 */
A_STATUS HIFDevRWCompletionHandler(void *context, A_STATUS status)
{
    struct HIFSendContext *pSendContext = (struct HIFSendContext *)context;
    unsigned int transferID = pSendContext->transferID;
    HIF_SDIO_DEVICE *pDev = pSendContext->pDev;
    adf_nbuf_t buf = pSendContext->netbuf;

    if (pSendContext->bNewAlloc){
        adf_os_mem_free((void*)pSendContext);
    } else {
        adf_nbuf_pull_head(buf, pSendContext->head_data_len);
    }
    if (pDev->hif_callbacks.txCompletionHandler) {
        pDev->hif_callbacks.txCompletionHandler(pDev->hif_callbacks.Context,
                buf,
                transferID);
    }
    return A_OK;
}

A_STATUS HIFDevSendBuffer(HIF_SDIO_DEVICE *pDev, unsigned int transferID, a_uint8_t pipe,
        unsigned int nbytes, adf_nbuf_t buf)
{
    A_STATUS status;
    A_UINT32 paddedLength;
    int frag_count = 0, i, head_data_len;
    struct HIFSendContext *pSendContext;
    unsigned char *pData;
    A_UINT32 request = HIF_WR_ASYNC_BLOCK_INC;
    A_UINT8 mboxIndex = HIFDevMapPipeToMailBox(pDev, pipe);

    paddedLength = DEV_CALC_SEND_PADDED_LEN(pDev, nbytes);
#ifdef ENABLE_MBOX_DUMMY_SPACE_FEATURE
    A_ASSERT(paddedLength - nbytes < HIF_DUMMY_SPACE_MASK + 1);
    /*
     * two most significant bytes to save dummy data count
     * data written into the dummy space will not put into the final mbox FIFO
     *
     */
    request |= ((paddedLength - nbytes) << 16);
#endif

    frag_count = adf_nbuf_get_num_frags(buf);

    if (frag_count > 1){
        /* header data length should be total sending length substract internal data length of netbuf */
        /*
         * | HIFSendContext | fragments except internal buffer | netbuf->data
         */
        head_data_len = sizeof(struct HIFSendContext) +
                (nbytes - adf_nbuf_get_frag_len(buf, frag_count - 1));
    } else {
        /*
         * | HIFSendContext | netbuf->data
         */
        head_data_len = sizeof(struct HIFSendContext);
    }

    /* Check whether head room is enough to save extra head data */
    if ((head_data_len <= adf_nbuf_headroom(buf)) &&
                (adf_nbuf_tailroom(buf) >= (paddedLength - nbytes))){
        pSendContext = (struct HIFSendContext*)adf_nbuf_push_head(buf, head_data_len);
        pSendContext->bNewAlloc = FALSE;
    } else {
        pSendContext = (struct HIFSendContext*)adf_os_mem_alloc(NULL,
                sizeof(struct HIFSendContext) + paddedLength);
        pSendContext->bNewAlloc = TRUE;
    }

    pSendContext->netbuf = buf;
    pSendContext->pDev = pDev;
    pSendContext->transferID = transferID;
    pSendContext->head_data_len = head_data_len;
    /*
     * Copy data to head part of netbuf or head of allocated buffer.
     * if buffer is new allocated, the last buffer should be copied also.
     * It assume last fragment is internal buffer of netbuf
     * sometime total length of fragments larger than nbytes
     */
    pData = (unsigned char *)pSendContext + sizeof(struct HIFSendContext);
    for (i = 0; i < (pSendContext->bNewAlloc ? frag_count : frag_count - 1); i ++){
        int frag_len = adf_nbuf_get_frag_len(buf, i);
        unsigned char *frag_addr = adf_nbuf_get_frag_vaddr(buf, i);
        if (frag_len > nbytes){
            frag_len = nbytes;
        }
        memcpy(pData, frag_addr, frag_len);
        pData += frag_len;
        nbytes -= frag_len;
        if (nbytes <= 0) {
            break;
        }
    }

    /* Reset pData pointer and send out */
    pData = (unsigned char *)pSendContext + sizeof(struct HIFSendContext);
    status = HIFReadWrite(pDev->HIFDevice,
            pDev->MailBoxInfo.MboxProp[mboxIndex].ExtendedAddress,
            (char*) pData,
            paddedLength,
            request,
            (void*)pSendContext);

    if (status == A_PENDING){
        /*
         * it will return A_PENDING in native HIF implementation,
         * which should be treated as successful result here.
         */
        status = A_OK;
    }
    /* release buffer or move back data pointer when failed */
    if (status != A_OK){
        if (pSendContext->bNewAlloc){
            adf_os_mem_free(pSendContext);
        } else {
            adf_nbuf_pull_head(buf, head_data_len);
        }
    }

    return status;
}
