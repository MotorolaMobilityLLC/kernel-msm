/*
 * copyright (c) 2012,2014 The Linux Foundation. All rights reserved.
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

#include "hif.h"
#include "bmi.h"
#include "bmi_internal.h"
#include "ol_fw.h"

#ifdef HIF_MESSAGE_BASED

#ifdef HIF_USB
#include "a_usb_defs.h"
#endif

#if defined(HIF_BMI_MAX_TRANSFER_SIZE)

#if BMI_DATASZ_MAX > HIF_BMI_MAX_TRANSFER_SIZE
    /* override */
#undef BMI_DATASZ_MAX
#define BMI_DATASZ_MAX  HIF_BMI_MAX_TRANSFER_SIZE
#endif
#endif

#endif

#ifdef DEBUG
static ATH_DEBUG_MASK_DESCRIPTION bmi_debug_desc[] = {
    { ATH_DEBUG_BMI , "BMI Tracing"},
};

ATH_DEBUG_INSTANTIATE_MODULE_VAR(bmi,
                                 "bmi",
                                 "Boot Manager Interface",
                                 ATH_DEBUG_MASK_DEFAULTS,
                                 ATH_DEBUG_DESCRIPTION_COUNT(bmi_debug_desc),
                                 bmi_debug_desc);

#endif

/*
Although we had envisioned BMI to run on top of HTC, this is not how the
final implementation ended up. On the Target side, BMI is a part of the BSP
and does not use the HTC protocol nor even DMA -- it is intentionally kept
very simple.
*/

#define MAX_BMI_CMDBUF_SZ (BMI_DATASZ_MAX + \
                       sizeof(A_UINT32) /* cmd */ + \
                       sizeof(A_UINT32) /* addr */ + \
                       sizeof(A_UINT32))/* length */
#define BMI_COMMAND_FITS(sz) ((sz) <= MAX_BMI_CMDBUF_SZ)
#define BMI_EXCHANGE_TIMEOUT_MS  1000

/* APIs visible to the driver */
void
BMIInit(struct ol_softc *scn)
{
    if (!scn) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Invalid scn context\n"));
        ASSERT(0);
        return;
    }
    scn->bmiDone = FALSE;

    /*
     * On some platforms, it's not possible to DMA to a static variable
     * in a device driver (e.g. Linux loadable driver module).
     * So we need to A_MALLOC space for "command credits" and for commands.
     *
     * Note: implicitly relies on A_MALLOC to provide a buffer that is
     * suitable for DMA (or PIO).  This buffer will be passed down the
     * bus stack.
     */

    if (!scn->pBMICmdBuf) {
#ifndef HIF_PCI
        scn->pBMICmdBuf =
                (A_UCHAR *)A_MALLOC(MAX_BMI_CMDBUF_SZ);
#else
        scn->pBMICmdBuf =
                (A_UCHAR *)pci_alloc_consistent(scn->sc_osdev->bdev,
                                    MAX_BMI_CMDBUF_SZ,
                                    &scn->BMICmd_pa);
#endif
        ASSERT(scn->pBMICmdBuf);
    }

    if (!scn->pBMIRspBuf) {
#ifndef HIF_PCI
        scn->pBMIRspBuf =
                (A_UCHAR *)A_MALLOC(MAX_BMI_CMDBUF_SZ);
#else
        scn->pBMIRspBuf =
                (A_UCHAR *)pci_alloc_consistent(scn->sc_osdev->bdev,
                                MAX_BMI_CMDBUF_SZ,
                                &scn->BMIRsp_pa);
#endif
        ASSERT(scn->pBMIRspBuf);
    }

    A_REGISTER_MODULE_DEBUG_INFO(bmi);
}

void
BMICleanup(struct ol_softc *scn)
{
    if (scn->pBMICmdBuf) {
#ifndef HIF_PCI
        A_FREE(scn->pBMICmdBuf );
#else
        pci_free_consistent(scn->sc_osdev->bdev, MAX_BMI_CMDBUF_SZ,
                        scn->pBMICmdBuf, scn->BMICmd_pa);
#endif
        scn->pBMICmdBuf = NULL;
        scn->BMICmd_pa = 0;
    }

    if (scn->pBMIRspBuf) {
#ifndef HIF_PCI
        A_FREE(scn->pBMIRspBuf);
#else
        pci_free_consistent(scn->sc_osdev->bdev, MAX_BMI_CMDBUF_SZ,
                        scn->pBMIRspBuf, scn->BMIRsp_pa);
#endif
        scn->pBMIRspBuf = NULL;
        scn->BMIRsp_pa = 0;
    }
}

static A_STATUS
BMIDone(HIF_DEVICE *device, struct ol_softc *scn)
{
    A_STATUS status;
    A_UINT32 cid;

    if (!scn) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Invalid scn context\n"));
        ASSERT(0);
        return A_ERROR;
    }

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF (ATH_DEBUG_BMI, ("BMIDone skipped\n"));
        return A_OK;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Done: Enter (device: 0x%p)\n", device));

#if defined(A_SIMOS_DEVHOST)
    /* Let HIF layer know that BMI phase is done.
     * Note that this call enqueues a bunch of receive buffers,
     * so it is important that this complete before we tell the
     * target about BMI_DONE.
     */
    (void)HIFConfigureDevice(device, HIF_BMI_DONE, NULL, 0);
#endif

    scn->bmiDone = TRUE;
    cid = BMI_DONE;

    if (!scn->pBMICmdBuf) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Invalid scn BMICmdBuff\n"));
        ASSERT(0);
        return A_ERROR;
    }

    A_MEMCPY(scn->pBMICmdBuf,&cid,sizeof(cid));

    status = HIFExchangeBMIMsg(device, scn->pBMICmdBuf, sizeof(cid), NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    if (scn->pBMICmdBuf) {
#ifndef HIF_PCI
        A_FREE(scn->pBMICmdBuf);
#else
        pci_free_consistent(scn->sc_osdev->bdev, MAX_BMI_CMDBUF_SZ,
                        scn->pBMICmdBuf, scn->BMICmd_pa);
#endif
        scn->pBMICmdBuf = NULL;
        scn->BMICmd_pa = 0;
    }

    if (scn->pBMIRspBuf) {
#ifndef HIF_PCI
        A_FREE(scn->pBMIRspBuf);
#else
        pci_free_consistent(scn->sc_osdev->bdev, MAX_BMI_CMDBUF_SZ,
                        scn->pBMIRspBuf, scn->BMIRsp_pa);
#endif
        scn->pBMIRspBuf = NULL;
        scn->BMIRsp_pa = 0;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Done: Exit\n"));

    return A_OK;
}

A_STATUS bmi_done(struct ol_softc *scn)
{
    HIFClaimDevice(scn->hif_hdl, scn);

    if (BMIDone(scn->hif_hdl, scn) != A_OK)
	    return -1;

    return 0;
}

#ifndef HIF_MESSAGE_BASED
extern A_STATUS HIFRegBasedGetTargetInfo(HIF_DEVICE *device, struct bmi_target_info *targ_info);
#endif

static A_STATUS
BMIGetTargetInfo(HIF_DEVICE *device, struct bmi_target_info *targ_info, struct ol_softc *scn)
{
#ifndef HIF_MESSAGE_BASED
#else
    A_STATUS status;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = scn->pBMIRspBuf;
#endif

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("BMI Get Target Info Command disallowed\n"));
        return A_ERROR;
    }

#ifndef HIF_MESSAGE_BASED
        /* getting the target ID requires special handling because of the variable length
         * message */
    return HIFRegBasedGetTargetInfo(device,targ_info);
#else

    {
        A_UINT32 cid;
        A_UINT32 length;

        cid = BMI_GET_TARGET_INFO;

        A_MEMCPY(pBMICmdBuf,&cid,sizeof(cid));
        length = sizeof(struct bmi_target_info);

        status = HIFExchangeBMIMsg(device,
                                 pBMICmdBuf,
                                 sizeof(cid),
                                 (A_UINT8 *)pBMIRspBuf,
                                 &length,
                                 BMI_EXCHANGE_TIMEOUT_MS);

        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to get target information from the device\n"));
            return A_ERROR;
        }

        A_MEMCPY(targ_info, pBMIRspBuf, length);
        return status;
    }
#endif
}

A_STATUS bmi_download_firmware(struct ol_softc *scn)
{
	struct bmi_target_info targ_info;
	OS_MEMZERO(&targ_info, sizeof(targ_info));

	if (!scn){
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Invalid scn context\n"));
		ASSERT(0);
		return A_EINVAL;
	}

	/* Initialize BMI */
	BMIInit(scn);

	if (scn->pBMICmdBuf == NULL || scn->pBMIRspBuf == NULL) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("BMIInit failed!\n"));
		return -1;
	}

	/* Get target information */
	if (BMIGetTargetInfo(scn->hif_hdl, &targ_info, scn) != A_OK)
		return -1;

	scn->target_type = targ_info.target_type;
	scn->target_version = targ_info.target_ver;

	/* Configure target */
	if (ol_configure_target(scn) != A_OK)
		return -1;

	if (ol_download_firmware(scn) != EOK)
		return -EIO;

	return 0;
}

A_STATUS
BMIReadMemory(HIF_DEVICE *device,
              A_UINT32 address,
              A_UCHAR *buffer,
              A_UINT32 length,
              struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, rxlen;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = scn->pBMIRspBuf;
    A_UINT32 align;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length)));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length));
    memset (pBMIRspBuf, 0, BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
                ("BMI Read Memory: Enter (device: 0x%p, address: 0x%x, length: %d)\n",
                    device, address, length));

    cid = BMI_READ_MEMORY;
#if defined(SDIO_3_0)
    /* 4bytes align operation */
    align = 4 - (length & 3);
    remaining = length + align;
#else
    align = 0;
    remaining = length;
#endif
    while (remaining)
    {
        rxlen = (remaining < BMI_DATASZ_MAX) ? remaining : BMI_DATASZ_MAX;
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(address);
        A_MEMCPY(&(pBMICmdBuf[offset]), &rxlen, sizeof(rxlen));
        offset += sizeof(length);

        status = HIFExchangeBMIMsg(device,
                                   pBMICmdBuf,
                                   offset,
                                   pBMIRspBuf, /* note we reuse the same buffer to receive on */
                                   &rxlen,
                                   BMI_EXCHANGE_TIMEOUT_MS);

        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
            return A_ERROR;
        }
	if (remaining == rxlen) {
            A_MEMCPY(&buffer[length - remaining + align], pBMIRspBuf, rxlen - align); /* last align bytes are invalid */
	} else {
            A_MEMCPY(&buffer[length - remaining + align], pBMIRspBuf, rxlen);
	}
        remaining -= rxlen; address += rxlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read Memory: Exit\n"));
    return A_OK;
}

A_STATUS
BMIWriteMemory(HIF_DEVICE *device,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length,
               struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, txlen;
    const A_UINT32 header = sizeof(cid) + sizeof(address) + sizeof(length);
    A_UCHAR alignedBuffer[BMI_DATASZ_MAX];
    A_UCHAR *src;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX + header);

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI Write Memory: Enter (device: 0x%p, address: 0x%x, length: %d)\n",
         device, address, length));

    cid = BMI_WRITE_MEMORY;

    remaining = length;
    while (remaining)
    {
        src = &buffer[length - remaining];
        if (remaining < (BMI_DATASZ_MAX - header)) {
            if (remaining & 3) {
                /* align it with 4 bytes */
                remaining = remaining + (4 - (remaining & 3));
                memcpy(alignedBuffer, src, remaining);
                src = alignedBuffer;
            }
            txlen = remaining;
        } else {
            txlen = (BMI_DATASZ_MAX - header);
        }
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(address);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), src, txlen);
        offset += txlen;
        status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, BMI_EXCHANGE_TIMEOUT_MS);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen; address += txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Write Memory: Exit\n"));

    return A_OK;
}

A_STATUS
BMIExecute(HIF_DEVICE *device,
           A_UINT32 address,
           A_UINT32 *param,
           struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 paramLen;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = scn->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address) + sizeof(param)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));
    memset (pBMIRspBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Execute: Enter (device: 0x%p, address: 0x%x, param: %d)\n",
        device, address, *param));

    cid = BMI_EXECUTE;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    A_MEMCPY(&(pBMICmdBuf[offset]), param, sizeof(*param));
    offset += sizeof(*param);
    paramLen = sizeof(*param);
    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, pBMIRspBuf, &paramLen, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
        return A_ERROR;
    }

    A_MEMCPY(param, pBMIRspBuf, sizeof(*param));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Execute: Exit (param: %d)\n", *param));
    return A_OK;
}

A_STATUS
BMISetAppStart(HIF_DEVICE *device,
               A_UINT32 address,
               struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Set App Start: Enter (device: 0x%p, address: 0x%x)\n",
        device, address));

    cid = BMI_SET_APP_START;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);

    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Set App Start: Exit\n"));
    return A_OK;
}

A_STATUS
BMIReadSOCRegister(HIF_DEVICE *device,
                   A_UINT32 address,
                   A_UINT32 *param,
                   struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset,paramLen;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = scn->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));
    memset (pBMIRspBuf, 0, sizeof(cid) + sizeof(address));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Read SOC Register: Enter (device: 0x%p, address: 0x%x)\n",
       device, address));

    cid = BMI_READ_SOC_REGISTER;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    paramLen = sizeof(*param);
    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, pBMIRspBuf, &paramLen, BMI_EXCHANGE_TIMEOUT_MS);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
        return A_ERROR;
    }
    A_MEMCPY(param, pBMIRspBuf, sizeof(*param));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read SOC Register: Exit (value: %d)\n", *param));
    return A_OK;
}

A_STATUS
BMIWriteSOCRegister(HIF_DEVICE *device,
                    A_UINT32 address,
                    A_UINT32 param,
                    struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address) + sizeof(param)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
     ("BMI Write SOC Register: Enter (device: 0x%p, address: 0x%x, param: %d)\n",
     device, address, param));

    cid = BMI_WRITE_SOC_REGISTER;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    A_MEMCPY(&(pBMICmdBuf[offset]), &param, sizeof(param));
    offset += sizeof(param);
    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read SOC Register: Exit\n"));
    return A_OK;
}

A_STATUS
BMILZData(HIF_DEVICE *device,
          A_UCHAR *buffer,
          A_UINT32 length,
          struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, txlen;
    const A_UINT32 header = sizeof(cid) + sizeof(length);
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX+header));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX+header);

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI Send LZ Data: Enter (device: 0x%p, length: %d)\n",
         device, length));

    cid = BMI_LZ_DATA;

    remaining = length;
    while (remaining)
    {
        txlen = (remaining < (BMI_DATASZ_MAX - header)) ?
                                       remaining : (BMI_DATASZ_MAX - header);
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), &buffer[length - remaining], txlen);
        offset += txlen;
        status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, 0);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI LZ Data: Exit\n"));

    return A_OK;
}

A_STATUS
BMISignStreamStart(HIF_DEVICE *device,
                   A_UINT32 address,
                   A_UCHAR *buffer,
                   A_UINT32 length,
                   struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    const A_UINT32 header = sizeof(cid) + sizeof(address) + sizeof(length);
    A_UCHAR alignedBuffer[BMI_DATASZ_MAX + 4];
    A_UCHAR *src;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UINT32 remaining, txlen;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
    memset(pBMICmdBuf, 0, BMI_DATASZ_MAX + header);

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI SIGN Stream Start: Enter (device: 0x%p, address: 0x%x, length: %d)\n",
         device, address, length));

    cid = BMI_SIGN_STREAM_START;
    remaining = length;
    while (remaining)
    {
        src = &buffer[length - remaining];
        if (remaining < (BMI_DATASZ_MAX - header)) {
            /*  Actually it shall be aligned binary from header definition.
             *  Not necessary for align process. Kept for possible changes
             */
            if (remaining & 0x3) {
                remaining = remaining + (4 - (remaining & 0x3));
                memcpy(alignedBuffer, src, remaining);
                src = alignedBuffer;
            }
            txlen = remaining;
        } else {
            txlen = (BMI_DATASZ_MAX - header);
        }

        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(offset);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), src, txlen);
        offset += txlen;
        status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, BMI_EXCHANGE_TIMEOUT_MS);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI SIGN Stream Start: Exit\n"));

    return A_OK;
}

A_STATUS
BMILZStreamStart(HIF_DEVICE *device,
                 A_UINT32 address,
                 struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI LZ Stream Start: Enter (device: 0x%p, address: 0x%x)\n",
         device, address));

    cid = BMI_LZ_STREAM_START;
    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to Start LZ Stream to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI LZ Stream Start: Exit\n"));

    return A_OK;
}

A_STATUS
BMIFastDownload(HIF_DEVICE *device, A_UINT32 address, A_UCHAR *buffer, A_UINT32 length, struct ol_softc *scn)
{
    A_STATUS status = A_ERROR;
    A_UINT32  lastWord = 0;
    A_UINT32  lastWordOffset = length & ~0x3;
    A_UINT32  unalignedBytes = length & 0x3;

    status = BMILZStreamStart (device, address, scn);
    if (A_FAILED(status)) {
            return A_ERROR;
    }

    if (unalignedBytes) {
            /* copy the last word into a zero padded buffer */
        A_MEMCPY(&lastWord, &buffer[lastWordOffset], unalignedBytes);
    }

    status = BMILZData(device, buffer, lastWordOffset, scn);

    if (A_FAILED(status)) {
        return A_ERROR;
    }

    if (unalignedBytes) {
        status = BMILZData(device, (A_UINT8 *)&lastWord, 4, scn);
    }

    if (A_SUCCESS(status)) {
        //
        // Close compressed stream and open a new (fake) one.  This serves mainly to flush Target caches.
        //
        status = BMILZStreamStart (device, 0x00, scn);
        if (A_FAILED(status)) {
           return A_ERROR;
        }
    }
    return status;
}

A_STATUS
BMInvramProcess(HIF_DEVICE *device, A_UCHAR *seg_name, A_UINT32 *retval,
                 struct ol_softc *scn)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 retvalLen;
    A_UCHAR *pBMICmdBuf = scn->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = scn->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + BMI_NVRAM_SEG_NAME_SZ));

    if (scn->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI NVRAM Process: Enter (device: 0x%p, name: %s)\n",
           device, seg_name));

    cid = BMI_NVRAM_PROCESS;
    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), seg_name, BMI_NVRAM_SEG_NAME_SZ);
    offset += BMI_NVRAM_SEG_NAME_SZ;
    retvalLen = sizeof(*retval);
    status = HIFExchangeBMIMsg(device, pBMICmdBuf, offset, pBMIRspBuf, &retvalLen, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to access the device\n"));
        return A_ERROR;
    }

    A_MEMCPY(retval, pBMIRspBuf, sizeof(*retval));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI NVRAM Process: Exit\n"));

    return A_OK;
}

#ifdef HIF_MESSAGE_BASED

/* TODO.. stubs.. for message-based HIFs, the RAW access APIs need to be changed
 */

A_STATUS
BMIRawWrite(HIF_DEVICE *device, A_UCHAR *buffer, A_UINT32 length)
{
        /* TODO */
    return A_ERROR;
}

A_STATUS
BMIRawRead(HIF_DEVICE *device, A_UCHAR *buffer, A_UINT32 length, A_BOOL want_timeout)
{
        /* TODO */
    return A_ERROR;
}

#endif
