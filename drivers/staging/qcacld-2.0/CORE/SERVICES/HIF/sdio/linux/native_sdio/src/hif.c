/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/kthread.h>
#ifdef CONFIG_CNSS_SDIO
#include <net/cnss.h>
#endif
#include "if_ath_sdio.h"
#include "regtable.h"
#include "vos_api.h"
#include "wma_api.h"
#include "hif_internal.h"
#include "adf_os_time.h"
/* by default setup a bounce buffer for the data packets, if the underlying host controller driver
   does not use DMA you may be able to skip this step and save the memory allocation and transfer time */
#define HIF_USE_DMA_BOUNCE_BUFFER 1
#define ATH_MODULE_NAME hif
#include "a_debug.h"

#if HIF_USE_DMA_BOUNCE_BUFFER
/* macro to check if DMA buffer is WORD-aligned and DMA-able.  Most host controllers assume the
 * buffer is DMA'able and will bug-check otherwise (i.e. buffers on the stack).
 * virt_addr_valid check fails on stack memory.
 */
#define BUFFER_NEEDS_BOUNCE(buffer)  (((unsigned long)(buffer) & 0x3) || !virt_addr_valid((buffer)))
#else
#define BUFFER_NEEDS_BOUNCE(buffer)   (FALSE)
#endif

#define MAX_HIF_DEVICES 2

#ifdef HIF_MBOX_SLEEP_WAR
#define HIF_MIN_SLEEP_INACTIVITY_TIME_MS     50
#define HIF_SLEEP_DISABLE_UPDATE_DELAY 1
#define HIF_IS_WRITE_REQUEST_MBOX1_TO_3(request) \
                ((request->request & HIF_WRITE)&& \
                (request->address >= 0x1000 && request->address < 0x1FFFF))
#endif
unsigned int mmcbuswidth = 0;
module_param(mmcbuswidth, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mmcbuswidth, "Set MMC driver Bus Width: 1-1Bit, 4-4Bit, 8-8Bit");
EXPORT_SYMBOL(mmcbuswidth);

unsigned int mmcclock = 0;
module_param(mmcclock, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mmcclock, "Set MMC driver Clock value");
EXPORT_SYMBOL(mmcclock);

unsigned int brokenirq = 0;
module_param(brokenirq, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(brokenirq, "Set as 1 to use polling method instead of interrupt mode");

unsigned int forcesleepmode = 0;
module_param(forcesleepmode, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(forcesleepmode, "Set sleep mode: 0-host capbility, 1-force WOW, 2-force DeepSleep, 3-force CutPower");

/* Some laptop with JMicron SDIO host has compitable
 * issue with asyncintdelay value,
 * change default value to 2 */
unsigned int asyncintdelay = 2;
module_param(asyncintdelay, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(asyncintdelay, "Delay clock count for aysnc interrupt, 2 is default, vaild values are 1 and 2");

unsigned int forcecard = 0;
module_param(forcecard, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(forcecard, "Ignore card capabilities information to switch bus mode");

unsigned int debugcccr = 1;
module_param(debugcccr, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(debugcccr, "Output this cccr values");

unsigned int writecccr1 = 0;
module_param(writecccr1, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
unsigned int writecccr1value = 0;
module_param(writecccr1value, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

unsigned int writecccr2 = 0;
module_param(writecccr2, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
unsigned int writecccr2value = 0;
module_param(writecccr2value, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

unsigned int writecccr3 = 0;
module_param(writecccr3, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
unsigned int writecccr3value = 0;
module_param(writecccr3value, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

unsigned int writecccr4 = 0;
module_param(writecccr4, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

unsigned int writecccr4value = 0;
module_param(writecccr4value, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

unsigned int modstrength = 0;
module_param(modstrength, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(modstrength, "Adjust internal driver strength");

/* ATHENV */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
#define dev_to_sdio_func(d) container_of(d, struct sdio_func, dev)
#define to_sdio_driver(d)      container_of(d, struct sdio_driver, drv)
static int hifDeviceSuspend(struct device *dev);
static int hifDeviceResume(struct device *dev);
#endif /* CONFIG_PM */
static int hifDeviceInserted(struct sdio_func *func, const struct sdio_device_id *id);
static void hifDeviceRemoved(struct sdio_func *func);
static HIF_DEVICE *addHifDevice(struct sdio_func *func);
static HIF_DEVICE *getHifDevice(struct sdio_func *func);
static void delHifDevice(HIF_DEVICE * device);
static int Func0_CMD52WriteByte(struct mmc_card *card, unsigned int address, unsigned char byte);
static int Func0_CMD52ReadByte(struct mmc_card *card, unsigned int address, unsigned char *byte);


int reset_sdio_on_unload = 0;
module_param(reset_sdio_on_unload, int, 0644);

A_UINT32 nohifscattersupport = 1;

A_UINT32 forcedriverstrength = 1; /* force driver strength to type D */

/* ------ Static Variables ------ */
static const struct sdio_device_id ar6k_id_table[] = {
#ifdef AR6002_HEADERS_DEF
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6002_BASE | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6002_BASE | 0x1))  },
#endif
#ifdef AR6003_HEADERS_DEF
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6003_BASE | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6003_BASE | 0x1))  },
#endif
#ifdef AR6004_HEADERS_DEF
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6004_BASE | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6004_BASE | 0x1))  },
#endif
#ifdef AR6320_HEADERS_DEF
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x1))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x2))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x3))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x4))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x5))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x6))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x7))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x8))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0x9))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xA))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xB))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xC))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xD))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xE))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_AR6320_BASE | 0xF))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x1))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x2))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x3))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x4))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x5))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x6))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x7))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x8))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0x9))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xA))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xB))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xC))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xD))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xE))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (MANUFACTURER_ID_QCA9377_BASE | 0xF))  },
    /* TODO: just for compatible with old image which ManufacturerID is 0, should delete later */
    {  SDIO_DEVICE(MANUFACTURER_CODE, (0 | 0x0))  },
    {  SDIO_DEVICE(MANUFACTURER_CODE, (0 | 0x1))  },
#endif
    { /* null */                                         },
};
MODULE_DEVICE_TABLE(sdio, ar6k_id_table);

#ifdef CONFIG_CNSS_SDIO
static int hif_sdio_device_inserted(struct sdio_func *func, const struct sdio_device_id * id);
static void hif_sdio_device_removed(struct sdio_func *func);
static int hif_sdio_device_reinit(struct sdio_func *func, const struct sdio_device_id * id);
static void hif_sdio_device_shutdown(struct sdio_func *func);
static void hif_sdio_crash_shutdown(struct sdio_func *func);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
static int hif_sdio_device_suspend(struct device *dev);
static int hif_sdio_device_resume(struct device *dev);
#endif

static struct cnss_sdio_wlan_driver ar6k_driver = {
	.name = "ar6k_wlan",
	.id_table = ar6k_id_table,
	.probe = hif_sdio_device_inserted,
	.remove = hif_sdio_device_removed,
	.reinit = hif_sdio_device_reinit,
	.shutdown = hif_sdio_device_shutdown,
	.crash_shutdown = hif_sdio_crash_shutdown,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
	.suspend = hif_sdio_device_suspend,
	.resume = hif_sdio_device_resume,
#endif
};
#else
static struct sdio_driver ar6k_driver = {
    .name = "ar6k_wlan",
    .id_table = ar6k_id_table,
    .probe = hifDeviceInserted,
    .remove = hifDeviceRemoved,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
/* New suspend/resume based on linux-2.6.32
 * Need to patch linux-2.6.32 with mmc2.6.32_suspend.patch
 * Need to patch with msmsdcc2.6.29_suspend.patch for msm_sdcc host
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static struct dev_pm_ops ar6k_device_pm_ops = {
#else
static struct pm_ops ar6k_device_pm_ops = {
#endif
    .suspend = hifDeviceSuspend,
    .resume = hifDeviceResume,
};
#endif /* CONFIG_PM */
#endif

/* make sure we only unregister when registered. */
static int registered = 0;

OSDRV_CALLBACKS osdrvCallbacks;
extern A_UINT32 onebitmode;
extern A_UINT32 busspeedlow;
extern A_UINT32 debughif;

static HIF_DEVICE *hif_devices[MAX_HIF_DEVICES];

static void ResetAllCards(void);
static A_STATUS hifDisableFunc(HIF_DEVICE *device, struct sdio_func *func);
static A_STATUS hifEnableFunc(HIF_DEVICE *device, struct sdio_func *func);

#ifdef DEBUG

ATH_DEBUG_INSTANTIATE_MODULE_VAR(hif,
                                 "hif",
                                 "(Linux MMC) Host Interconnect Framework",
                                 ATH_DEBUG_MASK_DEFAULTS,
                                 0,
                                 NULL);

#endif

#ifdef CONFIG_CNSS_SDIO
static int hif_sdio_register_driver(OSDRV_CALLBACKS *callbacks)
{
	int status;
	/* store the callback handlers */
	osdrvCallbacks = *callbacks;

	/* Register with bus driver core */
	AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: HIFInit registering\n"));
	registered = 1;
	status = cnss_sdio_wlan_register_driver(&ar6k_driver);
	return status;
}
static void hif_sdio_unregister_driver(void)
{
	cnss_sdio_wlan_unregister_driver(&ar6k_driver);
}
#else
static int hif_sdio_register_driver(OSDRV_CALLBACKS *callbacks)
{
	int status;
	/* store the callback handlers */
	osdrvCallbacks = *callbacks;

	/* Register with bus driver core */
	AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: HIFInit registering\n"));
	registered = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
	if (callbacks->deviceSuspendHandler && callbacks->deviceResumeHandler) {
		ar6k_driver.drv.pm = &ar6k_device_pm_ops;
	}
#endif /* CONFIG_PM */

	status = sdio_register_driver(&ar6k_driver);
	return status;
}
static void hif_sdio_unregister_driver(void)
{
	sdio_unregister_driver(&ar6k_driver);
}
#endif

/* ------ Functions ------ */
A_STATUS HIFInit(OSDRV_CALLBACKS *callbacks)
{
    int status;

    if (callbacks == NULL)
        return A_ERROR;

    A_REGISTER_MODULE_DEBUG_INFO(hif);

    ENTER();

    status = hif_sdio_register_driver(callbacks);
    AR_DEBUG_ASSERT(status==0);

    if (status != 0) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,("%s sdio_register_driver failed!",__func__));
        return A_ERROR;
    }
    else
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,("%s sdio_register_driver successful",__func__));

    return A_OK;
}

static A_STATUS
__HIFReadWrite(HIF_DEVICE *device,
             A_UINT32 address,
             A_UCHAR *buffer,
             A_UINT32 length,
             A_UINT32 request,
             void *context)
{
    A_UINT8 opcode;
    A_STATUS    status = A_OK;
    int     ret;
    A_UINT8 *tbuffer;
    A_BOOL   bounced = FALSE;

    if (device == NULL || device->func == NULL)
        return A_ERROR;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("__HIFReadWrite, addr:0X%06X, len:%08d, %s, %s\n",
                    address,
                    length,
                    request & HIF_READ ? "Read " : "Write",
                    request & HIF_ASYNCHRONOUS ? "Async" : "Sync "));

    do {
        if (request & HIF_EXTENDED_IO) {
            //AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Command type: CMD53\n"));
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid command type: 0x%08x\n", request));
            status = A_EINVAL;
            break;
        }

        if (request & HIF_BLOCK_BASIS) {
            /* round to whole block length size */
            length = (length / HIF_MBOX_BLOCK_SIZE) * HIF_MBOX_BLOCK_SIZE;
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
                            ("AR6000: Block mode (BlockLen: %d)\n",
                            length));
        } else if (request & HIF_BYTE_BASIS) {
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
                            ("AR6000: Byte mode (BlockLen: %d)\n",
                            length));
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid data mode: 0x%08x\n", request));
            status = A_EINVAL;
            break;
        }

#if 0
        /* useful for checking register accesses */
        if (length & 0x3) {
            A_PRINTF(KERN_ALERT"AR6000: HIF (%s) is not a multiple of 4 bytes, addr:0x%X, len:%d\n",
                                request & HIF_WRITE ? "write":"read", address, length);
        }
#endif

        if (request & HIF_WRITE) {
            HIF_DEVICE_MBOX_INFO MailBoxInfo;
            unsigned int mboxLength = 0;
            HIFConfigureDevice(device,
                    HIF_DEVICE_GET_MBOX_ADDR,
                    &MailBoxInfo,
                    sizeof(MailBoxInfo));
            if (address >= 0x800 && address < 0xC00) {
                /* Host control register and CIS Window */
                mboxLength = 0;
            } else if (address == MailBoxInfo.MboxAddresses[0]
                    || address == MailBoxInfo.MboxAddresses[1]
                    || address == MailBoxInfo.MboxAddresses[2]
                    || address == MailBoxInfo.MboxAddresses[3]) {
                mboxLength = HIF_MBOX_WIDTH;
            } else if (address == MailBoxInfo.MboxProp[0].ExtendedAddress) {
                mboxLength = MailBoxInfo.MboxProp[0].ExtendedSize;
            } else if (address == MailBoxInfo.MboxProp[1].ExtendedAddress) {
                mboxLength = MailBoxInfo.MboxProp[1].ExtendedSize;
            } else {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                        ("Invalid written address: 0x%08x\n", address));
                break;
            }
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
                    ("address:%08X, Length:0x%08X, Dummy:0x%04X, Final:0x%08X\n", address, length, (request & HIF_DUMMY_SPACE_MASK) >> 16, mboxLength == 0 ? address : address + (mboxLength - length)));
            if (mboxLength != 0) {
                if (length > mboxLength) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("HIFReadWrite: written length(0x%08X) "
                            "larger than mbox length(0x%08x)\n", length, mboxLength));
                    break;
                }
                address += (mboxLength - length);
#ifdef ENABLE_MBOX_DUMMY_SPACE_FEATURE
                /*
                 * plus dummy byte count
                 */
                address += ((request & HIF_DUMMY_SPACE_MASK) >> 16);
#endif
            }
        }

        if (request & HIF_FIXED_ADDRESS) {
            opcode = CMD53_FIXED_ADDRESS;
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Address mode: Fixed 0x%X\n", address));
        } else if (request & HIF_INCREMENTAL_ADDRESS) {
            opcode = CMD53_INCR_ADDRESS;
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Address mode: Incremental 0x%X\n", address));
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid address mode: 0x%08x\n", request));
            status = A_EINVAL;
            break;
        }

        if (request & HIF_WRITE) {
#if HIF_USE_DMA_BOUNCE_BUFFER
            if (BUFFER_NEEDS_BOUNCE(buffer) && device->dma_buffer != NULL) {
                tbuffer = device->dma_buffer;
                    /* copy the write data to the dma buffer */
                AR_DEBUG_ASSERT(length <= HIF_DMA_BUFFER_SIZE);
                if (length > HIF_DMA_BUFFER_SIZE) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid write length: %d\n", length));
                    status = A_EINVAL;
                    break;
                }
                memcpy(tbuffer, buffer, length);
                bounced = TRUE;
            } else {
                tbuffer = buffer;
            }
#else
            tbuffer = buffer;
#endif
            if (opcode == CMD53_FIXED_ADDRESS && tbuffer != NULL) {
                ret = sdio_writesb(device->func, address, tbuffer, length);
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: writesb ret=%d address: 0x%X, len: %d, 0x%X\n",
                          ret, address, length, *(int *)tbuffer));
            } else {
                ret = sdio_memcpy_toio(device->func, address, tbuffer, length);
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: writeio ret=%d address: 0x%X, len: %d, 0x%X\n",
                          ret, address, length, *(int *)tbuffer));
            }
        } else if (request & HIF_READ) {
#if HIF_USE_DMA_BOUNCE_BUFFER
            if (BUFFER_NEEDS_BOUNCE(buffer) && device->dma_buffer != NULL) {
                AR_DEBUG_ASSERT(length <= HIF_DMA_BUFFER_SIZE);
                if (length > HIF_DMA_BUFFER_SIZE) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid read length: %d\n", length));
                    status = A_EINVAL;
                    break;
                }
                tbuffer = device->dma_buffer;
                bounced = TRUE;
            } else {
                tbuffer = buffer;
            }
#else
            tbuffer = buffer;
#endif
            if (opcode == CMD53_FIXED_ADDRESS && tbuffer != NULL) {
                ret = sdio_readsb(device->func, tbuffer, address, length);
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: readsb ret=%d address: 0x%X, len: %d, 0x%X\n",
                          ret, address, length, *(int *)tbuffer));
            } else {
                ret = sdio_memcpy_fromio(device->func, tbuffer, address, length);
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: readio ret=%d address: 0x%X, len: %d, 0x%X\n",
                          ret, address, length, *(int *)tbuffer));
            }
#if HIF_USE_DMA_BOUNCE_BUFFER
            if (bounced) {
                   /* copy the read data from the dma buffer */
                memcpy(buffer, tbuffer, length);
            }
#endif
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid direction: 0x%08x\n", request));
            status = A_EINVAL;
            break;
        }

        if (ret) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: SDIO bus operation failed! MMC stack returned : %d \n", ret));
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("__HIFReadWrite, addr:0X%06X, len:%08d, %s, %s\n",
                            address,
                            length,
                            request & HIF_READ ? "Read " : "Write",
                            request & HIF_ASYNCHRONOUS ? "Async" : "Sync "));
            status = A_ERROR;
        }
    } while (FALSE);

    return status;
}

void AddToAsyncList(HIF_DEVICE *device, BUS_REQUEST *busrequest)
{
    unsigned long flags;
    BUS_REQUEST *async;
    BUS_REQUEST *active;

    spin_lock_irqsave(&device->asynclock, flags);
    active = device->asyncreq;
    if (active == NULL) {
        device->asyncreq = busrequest;
        device->asyncreq->inusenext = NULL;
    } else {
        for (async = device->asyncreq;
             async != NULL;
             async = async->inusenext) {
             active =  async;
        }
        active->inusenext = busrequest;
        busrequest->inusenext = NULL;
    }
    spin_unlock_irqrestore(&device->asynclock, flags);
}

A_STATUS
HIFSyncRead(HIF_DEVICE *device,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length,
               A_UINT32 request,
               void *context)
{
       A_STATUS status;

       if (device == NULL || device->func == NULL)
           return A_ERROR;

       sdio_claim_host(device->func);
       status = __HIFReadWrite(device, address, buffer, length, request & ~HIF_SYNCHRONOUS, NULL);
       sdio_release_host(device->func);
       return status;
}

/* queue a read/write request */
A_STATUS
HIFReadWrite(HIF_DEVICE *device,
             A_UINT32 address,
             A_UCHAR *buffer,
             A_UINT32 length,
             A_UINT32 request,
             void *context)
{
    A_STATUS    status = A_OK;
    BUS_REQUEST *busrequest;


    if (device == NULL || device->func == NULL)
        return A_ERROR;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
        ("AR6000: device 0x%p addr 0x%X buffer 0x%p len %d req 0x%X context 0x%p",
        device, address, buffer, length, request, context));

    /*sdio r/w action is not needed when suspend with cut power,so just return*/
    if((device->is_suspend == TRUE)&&(device->powerConfig == HIF_DEVICE_POWER_CUT)){
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("skip io when suspending\n"));
        return A_OK;
    }
    do {
        if ((request & HIF_ASYNCHRONOUS) || (request & HIF_SYNCHRONOUS)){
            /* serialize all requests through the async thread */
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Execution mode: %s\n",
                        (request & HIF_ASYNCHRONOUS)?"Async":"Synch"));
            busrequest = hifAllocateBusRequest(device);
            if (busrequest == NULL) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                    ("AR6000: no async bus requests available (%s, addr:0x%X, len:%d) \n",
                        request & HIF_READ ? "READ":"WRITE", address, length));
                return A_ERROR;
            }
            busrequest->address = address;
            busrequest->buffer = buffer;
            busrequest->length = length;
            busrequest->request = request;
            busrequest->context = context;

            AddToAsyncList(device, busrequest);

            if (request & HIF_SYNCHRONOUS) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: queued sync req: 0x%lX\n", (unsigned long)busrequest));

                /* wait for completion */
                up(&device->sem_async);
                if (down_interruptible(&busrequest->sem_req) != 0) {
                    /* interrupted, exit */
                    return A_ERROR;
                } else {
                    A_STATUS status = busrequest->status;
                    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: sync return freeing 0x%lX: 0x%X\n",
                              (unsigned long)busrequest, busrequest->status));
                    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: freeing req: 0x%X\n", (unsigned int)request));
                    hifFreeBusRequest(device, busrequest);
                    return status;
                }
            } else {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: queued async req: 0x%lX\n", (unsigned long)busrequest));
                up(&device->sem_async);
                return A_PENDING;
            }
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Invalid execution mode: 0x%08x\n", (unsigned int)request));
            status = A_EINVAL;
            break;
        }
    } while(0);

    return status;
}
/* thread to serialize all requests, both sync and async */
static int async_task(void *param)
 {
    HIF_DEVICE *device;
    BUS_REQUEST *request;
    A_STATUS status;
    unsigned long flags;

    device = (HIF_DEVICE *)param;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async task\n"));
    set_current_state(TASK_INTERRUPTIBLE);
    while(!device->async_shutdown) {
        /* wait for work */
        if (down_interruptible(&device->sem_async) != 0) {
            /* interrupted, exit */
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async task interrupted\n"));
            break;
        }
        if (device->async_shutdown) {
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async task stopping\n"));
            break;
        }
#ifdef HIF_MBOX_SLEEP_WAR
        /* No write request, and device state is sleep enter into sleep mode */
        if ((device->asyncreq == NULL) &&
            (adf_os_atomic_read(&device->mbox_state) == HIF_MBOX_REQUEST_TO_SLEEP_STATE)) {
            HIFSetMboxSleep(device, true, true, false);
            continue;
        }
#endif
        /* we want to hold the host over multiple cmds if possible, but holding the host blocks card interrupts */
        sdio_claim_host(device->func);
        spin_lock_irqsave(&device->asynclock, flags);
        /* pull the request to work on */
        while (device->asyncreq != NULL) {
            request = device->asyncreq;
            if (request->inusenext != NULL) {
                device->asyncreq = request->inusenext;
            } else {
                device->asyncreq = NULL;
            }
            spin_unlock_irqrestore(&device->asynclock, flags);
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async_task processing req: 0x%lX\n", (unsigned long)request));
#ifdef HIF_MBOX_SLEEP_WAR
            /* write request pending for mailbox(1-3),
	     * and the mbox state is sleep then awake the device */
            if (HIF_IS_WRITE_REQUEST_MBOX1_TO_3(request)) {
                if (adf_os_atomic_read(&device->mbox_state) == HIF_MBOX_SLEEP_STATE) {
                    HIFSetMboxSleep(device, false, true, false);
                    adf_os_timer_cancel(&device->sleep_timer);
                    adf_os_timer_start(&device->sleep_timer, HIF_MIN_SLEEP_INACTIVITY_TIME_MS);
                }
                /* Update the write time stamp */
                device->sleep_ticks = adf_os_ticks();
            }
#endif
            if (request->pScatterReq != NULL) {
                A_ASSERT(device->scatter_enabled);
                    /* this is a queued scatter request, pass the request to scatter routine which
                     * executes it synchronously, note, no need to free the request since scatter requests
                     * are maintained on a separate list */
                status = DoHifReadWriteScatter(device,request);
            } else {
                    /* call HIFReadWrite in sync mode to do the work */
                status = __HIFReadWrite(device, request->address, request->buffer,
                                      request->length, request->request & ~HIF_SYNCHRONOUS, NULL);
                if (request->request & HIF_ASYNCHRONOUS) {
                    void *context = request->context;
                    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async_task freeing req: 0x%lX\n", (unsigned long)request));
                    hifFreeBusRequest(device, request);
                    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async_task completion routine req: 0x%lX\n", (unsigned long)request));
                    device->htcCallbacks.rwCompletionHandler(context, status);
                } else {
                    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: async_task upping req: 0x%lX\n", (unsigned long)request));
                    request->status = status;
                    up(&request->sem_req);
                }
            }
            spin_lock_irqsave(&device->asynclock, flags);
        }
        spin_unlock_irqrestore(&device->asynclock, flags);
        sdio_release_host(device->func);
    }

    complete_and_exit(&device->async_completion, 0);
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32))
static A_INT32 IssueSDCommand(HIF_DEVICE *device, A_UINT32 opcode, A_UINT32 arg, A_UINT32 flags, A_UINT32 *resp)
{
    struct mmc_command cmd;
    A_INT32 err;
    struct mmc_host *host;
    struct sdio_func *func;

    func = device->func;
    host = func->card->host;

    memset(&cmd, 0, sizeof(struct mmc_command));
    cmd.opcode = opcode;
    cmd.arg = arg;
    cmd.flags = flags;
    err = mmc_wait_for_cmd(host, &cmd, 3);

    if ((!err) && (resp)) {
        *resp = cmd.resp[0];
    }

    return err;
}
#endif
A_STATUS ReinitSDIO(HIF_DEVICE *device)
{
    A_INT32 err = 0;
    struct mmc_host *host;
    struct mmc_card *card;
    struct sdio_func *func;
    A_UINT8  cmd52_resp;
    A_UINT32 clock;

    func = device->func;
    card = func->card;
    host = card->host;

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +ReinitSDIO \n"));
    sdio_claim_host(func);

    do {
        /* Enable high speed */
        if (card->host->caps & MMC_CAP_SD_HIGHSPEED) {
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("ReinitSDIO: Set high speed mode\n"));
            err = Func0_CMD52ReadByte(card, SDIO_CCCR_SPEED, &cmd52_resp);
            if (err) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("ReinitSDIO: CMD52 read to CCCR speed register failed  : %d \n",err));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
                card->state &= ~MMC_STATE_HIGHSPEED;
#endif
                /* no need to break */
            } else {
                err = Func0_CMD52WriteByte(card, SDIO_CCCR_SPEED, (cmd52_resp | SDIO_SPEED_EHS));
                if (err) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("ReinitSDIO: CMD52 write to CCCR speed register failed  : %d \n",err));
                    break;
                }
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
                mmc_card_set_highspeed(card);
#endif
                host->ios.timing = MMC_TIMING_SD_HS;
                host->ops->set_ios(host, &host->ios);
            }
        }

        /* Set clock */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
        if (mmc_card_highspeed(card)) {
#else
        if (mmc_card_hs(card)) {
#endif
            clock = 50000000;
        } else {
            clock = card->cis.max_dtr;
        }

        if (clock > host->f_max) {
            clock = host->f_max;
        }
        /*
         * In fpga mode the clk should be set to 12500000, or will result in scan channel setting timeout error.
         * So in fpga mode, please set module parameter mmcclock to 12500000.
         */
        if(mmcclock > 0)
            clock = mmcclock;
        host->ios.clock = clock;
        host->ops->set_ios(host, &host->ios);


        if (card->host->caps & MMC_CAP_4_BIT_DATA) {
            /* CMD52: Set bus width & disable card detect resistor */
            err = Func0_CMD52WriteByte(card, SDIO_CCCR_IF, SDIO_BUS_CD_DISABLE | SDIO_BUS_WIDTH_4BIT);
            if (err) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("ReinitSDIO: CMD52 to set bus mode failed : %d \n",err));
                break;
            }
            host->ios.bus_width = MMC_BUS_WIDTH_4;
            host->ops->set_ios(host, &host->ios);
        }
    } while (0);

    sdio_release_host(func);
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -ReinitSDIO \n"));

    return (err) ? A_ERROR : A_OK;
}

#ifdef CONFIG_PM
/*
 * Setup IRQ mode for deep sleep and WoW
 * Switch back to 1 bits mode when we suspend for WoW in order to
 * detect SDIO irq without clock.
 * Re-enable async 4-bit irq mode for some host controllers after resume
 */
static int SdioEnable4bits(HIF_DEVICE *device,  int enable)
{
    int ret = 0;
    struct sdio_func *func = device->func;
    struct mmc_card *card = func->card;
    struct mmc_host *host = card->host;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
    unsigned char ctrl = 0;
    unsigned int width;
#ifdef SDIO_BUS_WIDTH_8BIT
    unsigned char wide_mask = (SDIO_BUS_WIDTH_4BIT|SDIO_BUS_WIDTH_8BIT);
#else
    unsigned char wide_mask = (SDIO_BUS_WIDTH_4BIT);
#endif
#endif

    if (!(host->caps & (MMC_CAP_4_BIT_DATA)))
        return 0;

    if (card->cccr.low_speed && !card->cccr.wide_bus)
        return 0;

    sdio_claim_host(func);
    do {
        int setAsyncIRQ = 0;
        __u16 manufacturer_id = device->id->device & MANUFACTURER_ID_AR6K_BASE_MASK;
        /* 2.6.34 will setup 1bits automatically. No need to setup */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
        ret = Func0_CMD52ReadByte(card, SDIO_CCCR_IF, &ctrl);
        if (ret) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("%s: Fail to read CCCR_IF : %d \n", __func__, ret));
            break;
        }
        if (enable) {
            width = MMC_BUS_WIDTH_4;
            ctrl &= ~(SDIO_BUS_WIDTH_1BIT|wide_mask);
            ctrl |= SDIO_BUS_WIDTH_4BIT;
        } else {
            width = MMC_BUS_WIDTH_1;
            ctrl &= ~(wide_mask);
        }

        ret = Func0_CMD52WriteByte(card, SDIO_CCCR_IF, ctrl);
        if (ret) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("%s: Fail to write CCCR_IF : %d \n", __func__, ret));
             break;
        }
        host->ios.bus_width = width;
        host->ops->set_ios(host, &host->ios);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34) */

        /* Re-enable 4-bit ASYNC interrupt on AR6003x after system resume for some host controller */
        if (manufacturer_id == MANUFACTURER_ID_AR6003_BASE) {
            setAsyncIRQ = 1;
            ret = Func0_CMD52WriteByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6003,
                    enable ? SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_AR6003 : 0);
        } else if (manufacturer_id == MANUFACTURER_ID_AR6320_BASE || manufacturer_id == MANUFACTURER_ID_QCA9377_BASE) {
            unsigned char data = 0;
            setAsyncIRQ = 1;
            ret = Func0_CMD52ReadByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6320, &data);
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to read interrupt extension register %d \n",ret));
                sdio_release_host(func);
                return ret;
            }
            if (enable)
                data |= SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_AR6320;
            else
                data &= ~SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_AR6320;
            ret = Func0_CMD52WriteByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6320, data);
        }
        if (setAsyncIRQ){
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to setup 4-bit ASYNC IRQ mode into %d err %d \n", enable, ret));
            } else {
                AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("AR6000: Setup 4-bit ASYNC IRQ mode into %d successfully\n", enable));
            }
        }
    } while (0);
    sdio_release_host(func);
    return ret;
}

#endif /* CONFIG_PM */
A_STATUS
PowerStateChangeNotify(HIF_DEVICE *device, HIF_DEVICE_POWER_CHANGE_TYPE config)
{
    A_STATUS status = A_OK;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
    struct sdio_func *func = device->func;
    int old_reset_val;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +PowerStateChangeNotify %d\n", config));
    switch (config) {
       case HIF_DEVICE_POWER_DOWN:
#ifdef HIF_MBOX_SLEEP_WAR
            adf_os_timer_cancel(&device->sleep_timer);
            HIFSetMboxSleep(device, true, true, false);
#endif
            /* Disable 4bits in order to let SDIO bus detect DAT1 as interrupt source */
            SdioEnable4bits(device, 0);
            break;
       case HIF_DEVICE_POWER_CUT:
            old_reset_val = reset_sdio_on_unload;
            reset_sdio_on_unload = 1;
            status = hifDisableFunc(device, func);
            reset_sdio_on_unload = old_reset_val;
            if (!device->is_suspend) {
                device->powerConfig = config;
                mmc_detect_change(device->host, HZ/3);
            }
            break;
       case HIF_DEVICE_POWER_UP:
            if (device->powerConfig == HIF_DEVICE_POWER_CUT) {
                if (device->is_suspend) {
                    status = ReinitSDIO(device);
                    /* set powerConfig before EnableFunc to passthrough sdio r/w action when resuming from cut power */
                    device->powerConfig = config;
                    if (status == A_OK) {
                        status = hifEnableFunc(device, func);
                    }
                } else {
                    /* device->func is bad pointer at this time */
                    mmc_detect_change(device->host, 0);
                    return A_PENDING; /* Don't change powerConfig status */
                }
            } else if (device->powerConfig == HIF_DEVICE_POWER_DOWN) {
                int ret = SdioEnable4bits(device, 1);
                status = (ret==0) ? A_OK : A_ERROR;
            }
            break;
    }
    device->powerConfig = config;

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -PowerStateChangeNotify\n"));
#endif
    return status;
}

A_STATUS
HIFConfigureDevice(HIF_DEVICE *device, HIF_DEVICE_CONFIG_OPCODE opcode,
                   void *config, A_UINT32 configLen)
{
    A_UINT32 count;
    A_STATUS status = A_OK;

    switch(opcode) {
        case HIF_DEVICE_GET_MBOX_BLOCK_SIZE:
            ((A_UINT32 *)config)[0] = HIF_MBOX0_BLOCK_SIZE;
            ((A_UINT32 *)config)[1] = HIF_MBOX1_BLOCK_SIZE;
            ((A_UINT32 *)config)[2] = HIF_MBOX2_BLOCK_SIZE;
            ((A_UINT32 *)config)[3] = HIF_MBOX3_BLOCK_SIZE;
            break;

        case HIF_DEVICE_GET_MBOX_ADDR:
            for (count = 0; count < 4; count ++) {
                ((A_UINT32 *)config)[count] = HIF_MBOX_START_ADDR(count);
            }

            if (configLen >= sizeof(HIF_DEVICE_MBOX_INFO)) {
                SetExtendedMboxWindowInfo((A_UINT16)device->func->device,
                                          (HIF_DEVICE_MBOX_INFO *)config);
            }

            break;
        case HIF_DEVICE_GET_PENDING_EVENTS_FUNC:
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                            ("AR6000: configuration opcode %d is not used for Linux SDIO stack \n", opcode));
            status = A_ERROR;
            break;
        case HIF_DEVICE_GET_IRQ_PROC_MODE:
            *((HIF_DEVICE_IRQ_PROCESSING_MODE *)config) = HIF_DEVICE_IRQ_SYNC_ONLY;
            break;
        case HIF_DEVICE_GET_RECV_EVENT_MASK_UNMASK_FUNC:
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                            ("AR6000: configuration opcode %d is not used for Linux SDIO stack \n", opcode));
            status = A_ERROR;
            break;
        case HIF_CONFIGURE_QUERY_SCATTER_REQUEST_SUPPORT:
            if (!device->scatter_enabled) {
                return A_ENOTSUP;
            }
            status = SetupHIFScatterSupport(device, (HIF_DEVICE_SCATTER_SUPPORT_INFO *)config);
            if (A_FAILED(status)) {
                device->scatter_enabled = FALSE;
            }
            break;
        case HIF_DEVICE_GET_OS_DEVICE:
                /* pass back a pointer to the SDIO function's "dev" struct */
            ((HIF_DEVICE_OS_DEVICE_INFO *)config)->pOSDevice = &device->func->dev;
            break;
        case HIF_DEVICE_POWER_STATE_CHANGE:
            status = PowerStateChangeNotify(device, *(HIF_DEVICE_POWER_CHANGE_TYPE *)config);
            break;
        case HIF_DEVICE_GET_IRQ_YIELD_PARAMS:
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                            ("AR6000: configuration opcode %d is only used for RTOS systems, not Linux systems\n", opcode));
            status = A_ERROR;
            break;
        case HIF_DEVICE_SET_HTC_CONTEXT:
            device->htcContext = config;
            break;
        case HIF_DEVICE_GET_HTC_CONTEXT:
            if (config == NULL){
                AR_DEBUG_PRINTF(ATH_DEBUG_WARN, ("Argument of HIF_DEVICE_GET_HTC_CONTEXT is NULL\n"));
                return A_ERROR;
            }
            *(void**)config = device->htcContext;
            break;
      case HIF_BMI_DONE:
        {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: BMI_DONE\n", __FUNCTION__)); /* TBDXXX */
            break;
        }
        default:
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                            ("AR6000: Unsupported configuration opcode: %d\n", opcode));
            status = A_ERROR;
    }

    return status;
}

void
HIFShutDownDevice(HIF_DEVICE *device)
{
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +HIFShutDownDevice\n"));
    if (device != NULL) {
        AR_DEBUG_ASSERT(device->powerConfig==HIF_DEVICE_POWER_CUT || device->func != NULL);
    } else {
        int i;
            /* since we are unloading the driver anyways, reset all cards in case the SDIO card
             * is externally powered and we are unloading the SDIO stack.  This avoids the problem when
             * the SDIO stack is reloaded and attempts are made to re-enumerate a card that is already
             * enumerated */
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: HIFShutDownDevice, resetting\n"));
        ResetAllCards();

        /* Unregister with bus driver core */
        if (registered) {
            registered = 0;
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Unregistering with the bus driver\n"));
            hif_sdio_unregister_driver();
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: Unregistered!"));
        }

        for (i=0; i<MAX_HIF_DEVICES; ++i) {
            if (hif_devices[i] && hif_devices[i]->func == NULL) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
                                ("AR6000: Remove pending hif_device %p\n", hif_devices[i]));
                delHifDevice(hif_devices[i]);
                hif_devices[i] = NULL;
            }
        }
    }
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -HIFShutDownDevice\n"));
}

static void
hifIRQHandler(struct sdio_func *func)
{
    A_STATUS status;
    HIF_DEVICE *device;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +hifIRQHandler\n"));

    device = getHifDevice(func);
    atomic_set(&device->irqHandling, 1);
    /* release the host during ints so we can pick it back up when we process cmds */
    sdio_release_host(device->func);
    status = device->htcCallbacks.dsrHandler(device->htcCallbacks.context);
    sdio_claim_host(device->func);
    atomic_set(&device->irqHandling, 0);
    AR_DEBUG_ASSERT(status == A_OK || status == A_ECANCELED);
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -hifIRQHandler\n"));
}

#ifdef HIF_MBOX_SLEEP_WAR
static void
HIF_sleep_entry(void *arg)
{
    HIF_DEVICE *device = (HIF_DEVICE *)arg;
    A_UINT32 idle_ms;

    idle_ms = adf_os_ticks_to_msecs(adf_os_ticks()
                                    - device->sleep_ticks);
    if (idle_ms >= HIF_MIN_SLEEP_INACTIVITY_TIME_MS) {
       adf_os_atomic_set(&device->mbox_state, HIF_MBOX_REQUEST_TO_SLEEP_STATE);
       up(&device->sem_async);
    } else {
       adf_os_timer_cancel(&device->sleep_timer);
       adf_os_timer_start(&device->sleep_timer,
                          HIF_MIN_SLEEP_INACTIVITY_TIME_MS);
    }
}

void
HIFSetMboxSleep(HIF_DEVICE *device, bool sleep, bool wait, bool cache)
{
    if (!device || !device->func|| !device->func->card) {
        printk("HIFSetMboxSleep incorrect input arguments\n");
        return;
    }
    sdio_claim_host(device->func);
    if (cache) {
       __HIFReadWrite(device, FIFO_TIMEOUT_AND_CHIP_CONTROL,
                      (A_UCHAR*)(&device->init_sleep), 4,
                      HIF_RD_SYNC_BYTE_INC, NULL);
    }
    if (sleep) {
        device->init_sleep &= FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_OFF;
        adf_os_atomic_set(&device->mbox_state, HIF_MBOX_SLEEP_STATE);
    } else {
        device->init_sleep |=  FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_ON;
        adf_os_atomic_set(&device->mbox_state, HIF_MBOX_AWAKE_STATE);
    }

    __HIFReadWrite(device, FIFO_TIMEOUT_AND_CHIP_CONTROL,
                   (A_UCHAR*)&device->init_sleep, 4,
                   HIF_WR_SYNC_BYTE_INC, NULL);
    sdio_release_host(device->func);
     /*Wait for 1ms for the written value to take effect */
    if (wait) {
       adf_os_mdelay(HIF_SLEEP_DISABLE_UPDATE_DELAY);
    }
    return;
}
#endif

/* handle HTC startup via thread*/
static int startup_task(void *param)
{
    HIF_DEVICE *device;

    device = (HIF_DEVICE *)param;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: call HTC from startup_task\n"));
        /* start  up inform DRV layer */
    if ((osdrvCallbacks.deviceInsertedHandler(osdrvCallbacks.context,device)) != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Device rejected\n"));
    }
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
static int enable_task(void *param)
{
    HIF_DEVICE *device;
    device = (HIF_DEVICE *)param;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: call  from resume_task\n"));

        /* start  up inform DRV layer */
    if (device &&
        device->claimedContext &&
        osdrvCallbacks.devicePowerChangeHandler &&
        osdrvCallbacks.devicePowerChangeHandler(device->claimedContext, HIF_DEVICE_POWER_UP) != A_OK)
    {
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: Device rejected\n"));
    }

    return 0;
}
#endif
static int hifDeviceInserted(struct sdio_func *func, const struct sdio_device_id *id)
{
    int i;
    int ret;
    HIF_DEVICE * device = NULL;
    int count;

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,
            ("AR6000: hifDeviceInserted, Function: 0x%X, Vendor ID: 0x%X, Device ID: 0x%X, block size: 0x%X/0x%X\n",
             func->num, func->vendor, id->device, func->max_blksize, func->cur_blksize));
    /*
    dma_mask should not be NULL, otherwise dma_map_single will crash.
    TODO: check why dma_mask is NULL here
    */
    if (func->dev.dma_mask == NULL){
        static u64 dma_mask = 0xFFFFFFFF;
        func->dev.dma_mask = &dma_mask;
    }
    for (i=0; i<MAX_HIF_DEVICES; ++i) {
        HIF_DEVICE *hifdevice = hif_devices[i];
        if (hifdevice && hifdevice->powerConfig == HIF_DEVICE_POWER_CUT &&
                hifdevice->host == func->card->host) {
            hifdevice->func = func;
            hifdevice->powerConfig = HIF_DEVICE_POWER_UP;
            sdio_set_drvdata(func, hifdevice);
            device = getHifDevice(func);

            if (device->is_suspend) {
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE,("AR6000: hifDeviceInserted, Resume from suspend, need to ReinitSDIO.\n"));
                ret = ReinitSDIO(device);
            }
            break;
        }
    }

    if (device==NULL) {
        if (addHifDevice(func) == NULL) {
            return -1;
        }
        device = getHifDevice(func);

        for (i=0; i<MAX_HIF_DEVICES; ++i) {
            if (hif_devices[i] == NULL) {
                hif_devices[i] = device;
                break;
            }
        }
        if (i==MAX_HIF_DEVICES) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                ("AR6000: hifDeviceInserted, No more hif_devices[] slot for %p", device));
        }

        device->id = id;
        device->host = func->card->host;
        device->is_disabled = TRUE;
/*
TODO: MMC SDIO3.0 Setting should also be modified in ReInit() function when Power Manage work.
*/
        {
            A_UINT32 clock, clock_set = 12500000;

            sdio_claim_host(func);

            /* force driver strength to type D */
            if (forcedriverstrength == 1) {
                unsigned int  addr = SDIO_CCCR_DRIVE_STRENGTH;
                unsigned char value = 0;
                A_UINT32 err = Func0_CMD52ReadByte(func->card, addr, &value);
                if (err) {
                    printk("Read CCCR 0x%02X failed: %d\n",
                            (unsigned int) addr,
                            (unsigned int) err);
                } else {
                    value = (value &
                            (~(SDIO_DRIVE_DTSx_MASK << SDIO_DRIVE_DTSx_SHIFT))) |
                            SDIO_DTSx_SET_TYPE_D;
                    err = Func0_CMD52WriteByte(func->card, addr, value);
                    if (err) {
                        printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                                (unsigned int) addr,
                                (unsigned int) value,
                                (unsigned int) err);
                    } else {
                        addr = CCCR_SDIO_DRIVER_STRENGTH_ENABLE_ADDR;
                        value = 0;
                        err = Func0_CMD52ReadByte(func->card, addr, &value);
                        if (err) {
                             printk("Read CCCR 0x%02X failed: %d\n",
                                    (unsigned int) addr,
                                    (unsigned int) err);
                        } else {
                            value = (value &
                                    (~CCCR_SDIO_DRIVER_STRENGTH_ENABLE_MASK)) |
                                    CCCR_SDIO_DRIVER_STRENGTH_ENABLE_A |
                                    CCCR_SDIO_DRIVER_STRENGTH_ENABLE_C |
                                    CCCR_SDIO_DRIVER_STRENGTH_ENABLE_D;
                            err = Func0_CMD52WriteByte(func->card, addr, value);
                            if (err) {
                                printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                                        (unsigned int) addr,
                                        (unsigned int) value,
                                        (unsigned int) err);
                            }
                        }
                    }
                }
            }

            if (writecccr1) {
                A_UINT32 err = Func0_CMD52WriteByte(func->card,
                        writecccr1,
                        writecccr1value);
                if (err) {
                    printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                            (unsigned int) writecccr1,
                            (unsigned int) writecccr1value,
                            (unsigned int) err);
                } else {
                    printk("Write CCCR 0x%02X to 0x%02X OK\n",
                            (unsigned int) writecccr1,
                            (unsigned int) writecccr1value);
                }
            }
            if (writecccr2) {
                A_UINT32 err = Func0_CMD52WriteByte(func->card,
                        writecccr2,
                        writecccr2value);
                if (err) {
                    printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                            (unsigned int) writecccr2,
                            (unsigned int) writecccr2value,
                            (unsigned int) err);
                } else {
                    printk("Write CCCR 0x%02X to 0x%02X OK\n",
                            (unsigned int) writecccr2,
                            (unsigned int) writecccr2value);
                }
            }
            if (writecccr3) {
                A_UINT32 err = Func0_CMD52WriteByte(func->card,
                        writecccr3,
                        writecccr3value);
                if (err) {
                    printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                            (unsigned int) writecccr3,
                            (unsigned int) writecccr3value,
                            (unsigned int) err);
                } else {
                    printk("Write CCCR 0x%02X to 0x%02X OK\n",
                            (unsigned int) writecccr3,
                            (unsigned int) writecccr3value);
                }
            }
            if (writecccr4) {
                A_UINT32 err = Func0_CMD52WriteByte(func->card,
                        writecccr4,
                        writecccr4value);
                if (err) {
                    printk("Write CCCR 0x%02X to 0x%02X failed: %d\n",
                            (unsigned int) writecccr4,
                            (unsigned int) writecccr4value,
                            (unsigned int) err);
                } else {
                    printk("Write CCCR 0x%02X to 0x%02X OK\n",
                            (unsigned int) writecccr4,
                            (unsigned int) writecccr4value);
                }
            }
            // Set MMC Clock
            if (mmcclock > 0){
                clock_set = mmcclock;
            }
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,16,0))
            if (mmc_card_highspeed(func->card)){
#else
            if (mmc_card_hs(func->card)) {
#endif
                clock = 50000000;
            } else {
                clock = func->card->cis.max_dtr;
            }
            if (clock > device->host->f_max){
                clock = device->host->f_max;
            }

            printk(KERN_ERR "%s: Dumping clocks (%d,%d)\n", __func__, func->card->cis.max_dtr, device->host->f_max);

/*
//          We don't need to set the clock explicitly on 8064/ADP platforms.
//          The clock chosen by the MMC stack will take affect.

            printk(KERN_ERR "Decrease host clock from %d to %d(%d,%d)\n",
                    clock, clock_set, func->card->cis.max_dtr, device->host->f_max);
            device->host->ios.clock = clock_set;
            device->host->ops->set_ios(device->host, &device->host->ios);
*/
            /* only when mmcclock module parameter is specified,
             * set the clock explicitly
             */
            if (mmcclock > 0) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Decrease host clock from %d to %d(%d,%d)\n",
                        clock, clock_set, func->card->cis.max_dtr, device->host->f_max));
                device->host->ios.clock = clock_set;
                device->host->ops->set_ios(device->host, &device->host->ios);
            }

// Set SDIO3.0
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0)
            // Set MMC Bus Width: 1-1Bit, 4-4Bit, 8-8Bit
            if (mmcbuswidth > 0){
                 if (mmcbuswidth == 1){
                    ret = Func0_CMD52WriteByte(func->card, SDIO_CCCR_IF, SDIO_BUS_CD_DISABLE|SDIO_BUS_WIDTH_1BIT);
                    if (ret){
                        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s: CMD52 to set bus width failed: %d \n", __func__, ret));
                        return ret;
                    }
                    device->host->ios.bus_width = MMC_BUS_WIDTH_1;
                    device->host->ops->set_ios(device->host, &device->host->ios);
                } else if (mmcbuswidth == 4 && (device->host->caps & MMC_CAP_4_BIT_DATA)){
                    ret = Func0_CMD52WriteByte(func->card, SDIO_CCCR_IF, SDIO_BUS_CD_DISABLE|SDIO_BUS_WIDTH_4BIT);
                    if (ret){
                        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s: CMD52 to set bus width failed: %d \n", __func__, ret));
                        return ret;
                    }
                    device->host->ios.bus_width = MMC_BUS_WIDTH_4;
                    device->host->ops->set_ios(device->host, &device->host->ios);
                }
#ifdef SDIO_BUS_WIDTH_8BIT
                else if (mmcbuswidth == 8 && (device->host->caps & MMC_CAP_8_BIT_DATA)){
                    ret = Func0_CMD52WriteByte(func->card, SDIO_CCCR_IF, SDIO_BUS_CD_DISABLE|SDIO_BUS_WIDTH_8BIT);
                    if (ret){
                        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s: CMD52 to set bus width failed: %d \n", __func__, ret));
                        return ret;
                    }
                    device->host->ios.bus_width = MMC_BUS_WIDTH_8;
                    device->host->ops->set_ios(device->host, &device->host->ios);
                }
#endif /* SDIO_BUS_WIDTH_8BIT */
                else {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s: MMC bus width %d is not supported. \n", __func__, mmcbuswidth));
                    return ret = -1;
                }
                AR_DEBUG_PRINTF(ATH_DEBUG_ANY,("%s: Set MMC bus width to %dBit. \n", __func__, mmcbuswidth));
            }
            if (debugcccr) {
               HIFDumpCCCR(device);
            }

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0) */
            sdio_release_host(func);
        }

        spin_lock_init(&device->lock);

        spin_lock_init(&device->asynclock);

        DL_LIST_INIT(&device->ScatterReqHead);

        if (!nohifscattersupport) {
            /* try to allow scatter operation on all instances,
                    * unless globally overridden */
                    device->scatter_enabled = TRUE;
            }
        else
            device->scatter_enabled = FALSE;

        /* Initialize the bus requests to be used later */
        A_MEMZERO(device->busRequest, sizeof(device->busRequest));
        for (count = 0; count < BUS_REQUEST_MAX_NUM; count ++) {
            sema_init(&device->busRequest[count].sem_req, 0);
            hifFreeBusRequest(device, &device->busRequest[count]);
        }
        sema_init(&device->sem_async, 0);
    }
#ifdef HIF_MBOX_SLEEP_WAR
    adf_os_timer_init(NULL, &device->sleep_timer,
                      HIF_sleep_entry, (void *)device,
                      ADF_NON_DEFERRABLE_TIMER);
    adf_os_atomic_set(&device->mbox_state, HIF_MBOX_UNKNOWN_STATE);
#endif

    ret = hifEnableFunc(device, func);
    return (ret == A_OK || ret == A_PENDING) ? 0 : -1;
}


void
HIFAckInterrupt(HIF_DEVICE *device)
{
    AR_DEBUG_ASSERT(device != NULL);

    /* Acknowledge our function IRQ */
}

void
HIFUnMaskInterrupt(HIF_DEVICE *device)
{
    int ret;;

    if (device == NULL || device->func == NULL)
        return;

    ENTER();
    /*
     * On HP Elitebook 8460P, interrupt mode is not stable in high throughput,
     * so polling method should be used instead of interrupt mode
     */
    if (brokenirq){
        printk(KERN_ERR"AR6000:Using broken IRQ mode\n");
        /* disable IRQ support even the capability exists */
        device->func->card->host->caps &= ~MMC_CAP_SDIO_IRQ;
    }
    /* Register the IRQ Handler */
    sdio_claim_host(device->func);
    ret = sdio_claim_irq(device->func, hifIRQHandler);
    sdio_release_host(device->func);
    AR_DEBUG_ASSERT(ret == 0);
    EXIT();
}

void HIFMaskInterrupt(HIF_DEVICE *device)
{
    int ret;

    if (device == NULL || device->func == NULL)
        return;
    ENTER();

    /* Mask our function IRQ */
    sdio_claim_host(device->func);
    while (atomic_read(&device->irqHandling)) {
        sdio_release_host(device->func);
        schedule_timeout_interruptible(HZ/10);
        sdio_claim_host(device->func);
    }
    ret = sdio_release_irq(device->func);
    sdio_release_host(device->func);
    if (ret) {
        if (ret == -ETIMEDOUT) {
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
                       ("AR6000: Timeout to mask interrupt. Card removed?\n"));
        } else {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                       ("AR6000: Unable to mask interrupt %d\n", ret));
            AR_DEBUG_ASSERT(ret == 0);
        }
    }
    EXIT();
}

BUS_REQUEST *hifAllocateBusRequest(HIF_DEVICE *device)
{
    BUS_REQUEST *busrequest;
    unsigned long flag;

    /* Acquire lock */
    spin_lock_irqsave(&device->lock, flag);

    /* Remove first in list */
    if((busrequest = device->s_busRequestFreeQueue) != NULL)
    {
        device->s_busRequestFreeQueue = busrequest->next;
    }
    /* Release lock */
    spin_unlock_irqrestore(&device->lock, flag);
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: hifAllocateBusRequest: 0x%p\n", busrequest));
    return busrequest;
}

void
hifFreeBusRequest(HIF_DEVICE *device, BUS_REQUEST *busrequest)
{
    unsigned long flag;

    if (busrequest == NULL)
       return;
    //AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: hifFreeBusRequest: 0x%p\n", busrequest));
    /* Acquire lock */
    spin_lock_irqsave(&device->lock, flag);


    /* Insert first in list */
    busrequest->next = device->s_busRequestFreeQueue;
    busrequest->inusenext = NULL;
    device->s_busRequestFreeQueue = busrequest;

    /* Release lock */
    spin_unlock_irqrestore(&device->lock, flag);
}

static A_STATUS hifDisableFunc(HIF_DEVICE *device, struct sdio_func *func)
{
    int ret;
    A_STATUS status = A_OK;

    ENTER();
    device = getHifDevice(func);
    if (device->async_task) {
        init_completion(&device->async_completion);
        device->async_shutdown = 1;
        up(&device->sem_async);
        wait_for_completion(&device->async_completion);
        device->async_task = NULL;
        sema_init(&device->sem_async, 0);
    }
#ifdef HIF_MBOX_SLEEP_WAR
    adf_os_timer_cancel(&device->sleep_timer);
    HIFSetMboxSleep(device, true, true, false);
#endif
    /* Disable the card */
    sdio_claim_host(device->func);
    ret = sdio_disable_func(device->func);
    if (ret) {
        status = A_ERROR;
    }

    if (reset_sdio_on_unload && status == A_OK) {
        /* reset the SDIO interface.  This is useful in automated testing where the card
         * does not need to be removed at the end of the test.  It is expected that the user will
         * also unload/reload the host controller driver to force the bus driver to re-enumerate the slot */
        AR_DEBUG_PRINTF(ATH_DEBUG_WARN, ("AR6000: reseting SDIO card back to uninitialized state \n"));

        /* NOTE : sdio_f0_writeb() cannot be used here, that API only allows access
         *        to undefined registers in the range of: 0xF0-0xFF */

        ret = Func0_CMD52WriteByte(device->func->card, SDIO_CCCR_ABORT, (1 << 3));
        if (ret) {
            status = A_ERROR;
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: reset failed : %d \n",ret));
        }
    }

    sdio_release_host(device->func);

    if (status == A_OK) {
        device->is_disabled = TRUE;
    }
    CleanupHIFScatterResources(device);

    EXIT();
    return status;
}

static A_STATUS hifEnableFunc(HIF_DEVICE *device, struct sdio_func *func)
{
    struct task_struct* pTask;
    const char *taskName = NULL;
    int (*taskFunc)(void *) = NULL;
    int ret = A_OK;

    ENTER("sdio_func 0x%p", func);

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +hifEnableFunc\n"));
    device = getHifDevice(func);

    if (device->is_disabled) {
        int setAsyncIRQ = 0;
        __u16 manufacturer_id = device->id->device & MANUFACTURER_ID_AR6K_BASE_MASK;
       /* enable the SDIO function */
        sdio_claim_host(func);
        /* enable 4-bit ASYNC interrupt on AR6003x or later devices */
        if (manufacturer_id == MANUFACTURER_ID_AR6003_BASE) {
            setAsyncIRQ = 1;
            ret = Func0_CMD52WriteByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6003,
                    SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_AR6003);
        } else if (manufacturer_id == MANUFACTURER_ID_AR6320_BASE ||
                   manufacturer_id == MANUFACTURER_ID_QCA9377_BASE) {
            unsigned char data = 0;
            setAsyncIRQ = 1;
            ret = Func0_CMD52ReadByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6320, &data);
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to read interrupt extension register %d \n",ret));
                sdio_release_host(func);
                return A_ERROR;
            }
            data |= SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_AR6320;
            ret = Func0_CMD52WriteByte(func->card, CCCR_SDIO_IRQ_MODE_REG_AR6320, data);
        }
        if (setAsyncIRQ){
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to enable 4-bit ASYNC IRQ mode %d \n",ret));
                sdio_release_host(func);
                return A_ERROR;
            }
            AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: 4-bit ASYNC IRQ mode enabled\n"));
        }

        /* set CCCR 0xF0[7:6] to increase async interrupt delay clock to fix interrupt missing issue on dell 8460p */
        if (asyncintdelay != 0){
            unsigned char data = 0;
            ret = Func0_CMD52ReadByte(func->card, CCCR_SDIO_ASYNC_INT_DELAY_ADDRESS, &data);
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to read CCCR %d, result is %d \n",
                        CCCR_SDIO_ASYNC_INT_DELAY_ADDRESS, ret));
                sdio_release_host(func);
                return A_ERROR;
            }
            data  = (data & ~CCCR_SDIO_ASYNC_INT_DELAY_MASK) |
                    ((asyncintdelay << CCCR_SDIO_ASYNC_INT_DELAY_LSB) & CCCR_SDIO_ASYNC_INT_DELAY_MASK);
            ret = Func0_CMD52WriteByte(func->card, CCCR_SDIO_ASYNC_INT_DELAY_ADDRESS, data);
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AR6000: failed to write CCCR %d, result is %d \n",
                        CCCR_SDIO_ASYNC_INT_DELAY_ADDRESS, ret));
                sdio_release_host(func);
                return A_ERROR;
            }
            printk(KERN_ERR"AR6000: Set async interrupt delay clock as %d.\n", asyncintdelay);
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
        /* give us some time to enable, in ms */
        func->enable_timeout = 100;
#endif
        ret = sdio_enable_func(func);
        if (ret) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s(), Unable to enable AR6K: 0x%X\n",
                      __FUNCTION__, ret));
            sdio_release_host(func);
            return A_ERROR;
        }
        ret = sdio_set_block_size(func, HIF_MBOX_BLOCK_SIZE);

        if (modstrength){
            unsigned int address = WINDOW_DATA_ADDRESS;
            unsigned int value = 0x0FFF;
            ret = sdio_memcpy_toio(device->func, address, &value, 4);
            if (ret) {
                printk("memcpy_toio 0x%x 0x%x error:%d\n", address, value, ret);
            } else {
                printk("memcpy_toio, 0x%x 0x%x OK\n", address, value);
                address = WINDOW_WRITE_ADDR_ADDRESS;
                value = 0x50F8;
                ret = sdio_memcpy_toio(device->func, address, &value, 4);
                if (ret) {
                    printk("memcpy_toio 0x%x 0x%x error:%d\n", address, value, ret);
                } else {
                    printk("memcpy_toio, 0x%x 0x%x OK\n", address, value);
                }
            }
        };
        sdio_release_host(func);
        if (ret) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s(), Unable to set block size 0x%x  AR6K: 0x%X\n",
                      __FUNCTION__, HIF_MBOX_BLOCK_SIZE, ret));
            return A_ERROR;
        }
        device->is_disabled = FALSE;
        /* create async I/O thread */
        if (!device->async_task) {
            device->async_shutdown = 0;
            device->async_task = kthread_create(async_task,
                                           (void *)device,
                                           "AR6K Async");
           if (IS_ERR(device->async_task)) {
               AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s(), to create async task\n", __FUNCTION__));
               device->async_task = NULL;
               return A_ERROR;
           }
           AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: start async task\n"));
           wake_up_process(device->async_task );
        }
    }

    if (!device->claimedContext) {
        taskFunc = startup_task;
        taskName = "AR6K startup";
        ret = A_OK;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
    } else {
        taskFunc = enable_task;
        taskName = "AR6K enable";
        ret = A_PENDING;
#endif /* CONFIG_PM */
    }
    /* create resume thread */
    pTask = kthread_create(taskFunc, (void *)device, taskName);
    if (IS_ERR(pTask)) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s(), to create enabel task\n", __FUNCTION__));
        return A_ERROR;
    }
    wake_up_process(pTask);
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -hifEnableFunc\n"));

    /* task will call the enable func, indicate pending */
    EXIT();
    return ret;
}
#if 0
//ToDO - This is a part of suspend/resume functionality
static int hifStopAllVap(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap;
    struct ieee80211vap *vapnext;
    osif_dev  *osifp;
    struct net_device *netdev;
    A_STATUS status = A_OK;

    ic = &scn->sc_ic;
    vap = TAILQ_FIRST(&ic->ic_vaps);
    while (vap != NULL) {
        vapnext = TAILQ_NEXT(vap, iv_next);
        osifp = (osif_dev *)vap->iv_ifp;
        if(osifp->is_up == 0){
            vap = vapnext;
            continue;
        }

        /* force target to sleep */
        wlan_pwrsave_force_sleep(vap,TRUE);

        netdev = osifp->netdev;
        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: %s()stopping interface %s\n", __FUNCTION__, netdev->name));
        status = osif_vap_stop(netdev);
        if(status){
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s()stop interface %s failed\n", __FUNCTION__, netdev->name));
        }
        vap = vapnext;
    }

    return A_OK;
}

static int hifRestartAllVap(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap;
    struct ieee80211vap *vapnext;
    osif_dev  *osifp;
    struct net_device *netdev;

    ic = &scn->sc_ic;;
    vap = TAILQ_FIRST(&ic->ic_vaps);
    while (vap != NULL) {
        vapnext = TAILQ_NEXT(vap, iv_next);
        osifp = (osif_dev *)vap->iv_ifp;
        netdev = osifp->netdev;

        AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: %s()restarting interface %s\n", __FUNCTION__, netdev->name));
        netdev->flags |= IFF_RUNNING;      /* we are ready to go */
        osifp->is_vap_pending = 1;
        osif_vap_init(netdev, 0);

        /* restore power save state */
        wlan_pwrsave_force_sleep(vap,FALSE);

        vap = vapnext;
    }

    return A_OK;
}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
static int hifDeviceSuspend(struct device *dev)
{
    struct sdio_func *func=dev_to_sdio_func(dev);
    A_STATUS status = A_OK;
    int ret = A_OK;
#if defined(MMC_PM_KEEP_POWER) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
    mmc_pm_flag_t pm_flag = 0;
    HIF_DEVICE_POWER_CHANGE_TYPE  config;
    struct mmc_host *host = NULL;
#endif

    HIF_DEVICE *device = getHifDevice(func);
    void *vos = vos_get_global_context(VOS_MODULE_ID_HIF, NULL);
    v_VOID_t *temp_module;

    if (vos_is_logp_in_progress(VOS_MODULE_ID_HIF, NULL)) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: LOPG in progress\n", __func__));
        return (-1);
    }

    temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
    if (!temp_module) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: WDA module is NULL\n", __func__));
        return (-1);
    }

    if (wma_check_scan_in_progress(temp_module)) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: Scan in progress. Aborting suspend\n", __func__));
        return (-1);
    }

#if defined(MMC_PM_KEEP_POWER) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
    if (device && device->func) {
        host = device->func->card->host;
    }
#endif

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +hifDeviceSuspend\n"));
    if (device && device->claimedContext && osdrvCallbacks.deviceSuspendHandler) {
        device->is_suspend = TRUE; /* set true first for PowerStateChangeNotify(..) */
        status = osdrvCallbacks.deviceSuspendHandler(device->claimedContext);

#if defined(MMC_PM_KEEP_POWER) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
        switch(forcesleepmode){
            case 0://depend on sdio host pm capbility
                pm_flag = sdio_get_host_pm_caps(func);
                break;
            case 1://force WOW
                pm_flag |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
                break;
            case 2://force DeepSleep
                pm_flag &= ~MMC_PM_WAKE_SDIO_IRQ;
                pm_flag |= MMC_PM_KEEP_POWER;
                break;
            case 3://force CutPower
                pm_flag &= ~(MMC_PM_WAKE_SDIO_IRQ | MMC_PM_WAKE_SDIO_IRQ);
                break;
        }
        if(!(pm_flag & MMC_PM_KEEP_POWER)){
            /* cut power support */
            /*setting powerConfig before HIFConfigureDevice to skip sdio r/w action when suspending with cut power*/
            AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: cut power enter\n"));
            config = HIF_DEVICE_POWER_CUT;
            device->powerConfig = config;
            if ((device->claimedContext != NULL)&&osdrvCallbacks.deviceRemovedHandler) {
                status = osdrvCallbacks.deviceRemovedHandler(device->claimedContext, device);
            }
            ret = HIFConfigureDevice(device,
                    HIF_DEVICE_POWER_STATE_CHANGE,
                    &config,
                    sizeof(HIF_DEVICE_POWER_CHANGE_TYPE));
            if(ret){
                AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: HIFConfigureDevice failed: %d\n",ret));
                return ret;
            }

            HIFMaskInterrupt(device);
            device->DeviceState = HIF_DEVICE_STATE_CUTPOWER;
            AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: cut power success\n"));
            return ret;
        } else {
            ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
            if (ret) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: set sdio pm flags MMC_PM_KEEP_POWER failed: %d\n",ret));
                return ret;
            }

            if (pm_flag & MMC_PM_WAKE_SDIO_IRQ){
                AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: wow enter\n"));
                config = HIF_DEVICE_POWER_DOWN;
                ret = HIFConfigureDevice(device,
                        HIF_DEVICE_POWER_STATE_CHANGE,
                        &config,
                        sizeof(HIF_DEVICE_POWER_CHANGE_TYPE));

                if(ret){
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: HIFConfigureDevice failed: %d\n",ret));
                    return ret;
                }
                ret = sdio_set_host_pm_flags(func, MMC_PM_WAKE_SDIO_IRQ);
                if (ret){
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: set sdio pm flags MMC_PM_WAKE_SDIO_IRQ failed: %d\n",ret));
                    return ret;
                }
                HIFMaskInterrupt(device);
                device->DeviceState = HIF_DEVICE_STATE_WOW;
                AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: wow success\n"));
                return ret;
            }
            else{
                /* deep sleep support */
                AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: deep sleep enter\n"));
//                hifStopAllVap((struct ol_ath_softc_net80211 *)device->claimedContext);

                /*
                 * Wait for some async clean handler finished.
                 * These clean handlers are part of vdev disconnect flow.
                 * As these handlers are async,sleep wait is not a good method, some block kernel method may be a good choice.
                 * But before adding finishe callback function to these handler, sleep wait is a simple method.
                 */
                msleep(100);
                HIFMaskInterrupt(device);
                device->DeviceState = HIF_DEVICE_STATE_DEEPSLEEP;
                AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("hifDeviceSuspend: deep sleep success\n"));
                return ret;
            }
        }
#endif
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -hifDeviceSuspend\n"));

    switch (status) {
    case A_OK:
#if defined(MMC_PM_KEEP_POWER) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
        if (host)  {
            host->pm_flags &= ~(MMC_PM_KEEP_POWER|MMC_PM_WAKE_SDIO_IRQ);
        }
#endif
        return 0;
    case A_EBUSY:
#if defined(MMC_PM_KEEP_POWER) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
        if (host) {
            /* Some controller need to WAKE_SDIO_IRQ in order to wake up by DAT1 */
            host->pm_flags |=  (MMC_PM_KEEP_POWER|MMC_PM_WAKE_SDIO_IRQ);
            host->pm_flags &= host->pm_caps;
        }
        return 0;
#else
        return -EBUSY; /* Hack for kernel to support deep sleep and wow */
#endif
    default:
        device->is_suspend = FALSE;
        return -1;
    }
}

static int hifDeviceResume(struct device *dev)
{
    struct sdio_func *func=dev_to_sdio_func(dev);
    A_STATUS status = A_OK;
    HIF_DEVICE_POWER_CHANGE_TYPE  config;
    HIF_DEVICE *device;
    void *vos = vos_get_global_context(VOS_MODULE_ID_HIF, NULL);
    v_VOID_t * temp_module;

    if (vos == NULL)
        return 0;

    temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
    if (!temp_module) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: WDA module is NULL\n", __func__));
        return (-1);
    }

    device = getHifDevice(func);
    if (device == NULL) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("%s: device is NULL\n", __func__));
        return (-1);
    }

    if(device->DeviceState == HIF_DEVICE_STATE_CUTPOWER){
        config = HIF_DEVICE_POWER_UP;
        status = HIFConfigureDevice(device,
                HIF_DEVICE_POWER_STATE_CHANGE,
                &config,
                sizeof(HIF_DEVICE_POWER_CHANGE_TYPE));
        if(status){
            AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: HIFConfigureDevice failed\n"));
            return status;
        }
    }
    else if(device->DeviceState == HIF_DEVICE_STATE_DEEPSLEEP){
        HIFUnMaskInterrupt(device);
//        hifRestartAllVap((struct ol_ath_softc_net80211 *)device->claimedContext);
    }
    else if(device->DeviceState == HIF_DEVICE_STATE_WOW){
        config = HIF_DEVICE_POWER_UP;
        status = HIFConfigureDevice(device,
                          HIF_DEVICE_POWER_STATE_CHANGE,
                          &config,
                          sizeof(HIF_DEVICE_POWER_CHANGE_TYPE));
        if(status){
          AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
              ("AR6000: HIFConfigureDevice failed status:%d\n", status));
          return status;
        }
        /*TODO:WOW support*/
        HIFUnMaskInterrupt(device);
    }

    /*
     * deviceResumeHandler do nothing now.
     * If some operation should be added to this handler in power cut resume flow,
     * do make sure those operation is not
     * depent on what startup_task has done,or the resume flow will block.
     */
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: +hifDeviceResume\n"));
    if (device && device->claimedContext && osdrvCallbacks.deviceSuspendHandler) {
        status = osdrvCallbacks.deviceResumeHandler(device->claimedContext);
        device->is_suspend = FALSE;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: -hifDeviceResume\n"));
    device->DeviceState = HIF_DEVICE_STATE_ON;

    return A_SUCCESS(status) ? 0 : status;
}
#endif /* CONFIG_PM */

static void hifDeviceRemoved(struct sdio_func *func)
{
    A_STATUS status = A_OK;
    HIF_DEVICE *device;
    AR_DEBUG_ASSERT(func != NULL);

    ENTER();

    device = getHifDevice(func);

    if (device->powerConfig == HIF_DEVICE_POWER_CUT) {
        device->func = NULL; /* func will be free by mmc stack */
        return; /* Just return for cut-off mode */
    } else {
        int i;
        for (i=0; i<MAX_HIF_DEVICES; ++i) {
            if (hif_devices[i] == device) {
                hif_devices[i] = NULL;
            }
        }
    }

    if (device->claimedContext != NULL) {
        status = osdrvCallbacks.deviceRemovedHandler(device->claimedContext, device);
    }

    HIFMaskInterrupt(device);

    if (device->is_disabled) {
        device->is_disabled = FALSE;
    } else {
        status = hifDisableFunc(device, func);
    }

    delHifDevice(device);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_WARN,
            ("AR6000: Unable to disable sdio func. Card removed?\n"));
    }

    EXIT();
}

/*
 * This should be moved to AR6K HTC layer.
 */
A_STATUS hifWaitForPendingRecv(HIF_DEVICE *device)
{
    A_INT32 cnt = 10;
    A_UINT8 host_int_status;
    A_STATUS status = A_OK;

    do {
        while (atomic_read(&device->irqHandling)) {
            /* wait until irq handler finished all the jobs */
            schedule_timeout_interruptible(HZ / 10);
        }
        /* check if there is any pending irq due to force done */
        host_int_status = 0;
        status = HIFReadWrite(device, HOST_INT_STATUS_ADDRESS,
                    (A_UINT8 *)&host_int_status, sizeof(host_int_status),
                     HIF_RD_SYNC_BYTE_INC, NULL);
        host_int_status = A_SUCCESS(status) ? (host_int_status & (1 << 0)) : 0;
        if (host_int_status) {
            /* wait until irq handler finishs its job */
            schedule_timeout_interruptible(1);
        }
    } while (host_int_status && --cnt > 0);

    if (host_int_status && cnt == 0) {
         AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
                            ("AR6000: %s(), Unable clear up pending IRQ before the system suspended\n", __FUNCTION__));
     }

    return A_OK;
}


static HIF_DEVICE *
addHifDevice(struct sdio_func *func)
{
    HIF_DEVICE *hifdevice = NULL;
#if(LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)) && !defined(WITH_BACKPORTS)
    int ret = 0;
#endif
    ENTER();
    if (func == NULL)
        return NULL;

    hifdevice = (HIF_DEVICE *)A_MALLOC(sizeof(HIF_DEVICE));
    if (hifdevice == NULL)
        return NULL;
    A_MEMZERO(hifdevice, sizeof(*hifdevice));
#if HIF_USE_DMA_BOUNCE_BUFFER
    hifdevice->dma_buffer = A_MALLOC(HIF_DMA_BUFFER_SIZE);
    AR_DEBUG_ASSERT(hifdevice->dma_buffer != NULL);
    if (hifdevice->dma_buffer == NULL) {
       A_FREE(hifdevice);
       return NULL;
    }
#endif
    hifdevice->func = func;
    hifdevice->powerConfig = HIF_DEVICE_POWER_UP;
    hifdevice->DeviceState = HIF_DEVICE_STATE_ON;
#if(LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)) && !defined(WITH_BACKPORTS)
    ret = sdio_set_drvdata(func, hifdevice);
    EXIT("status %d", ret);
#else
    sdio_set_drvdata(func, hifdevice);
    EXIT();
#endif
    return hifdevice;
}

static HIF_DEVICE *
getHifDevice(struct sdio_func *func)
{
    AR_DEBUG_ASSERT(func != NULL);
    return (HIF_DEVICE *)sdio_get_drvdata(func);
}

static void
delHifDevice(HIF_DEVICE * device)
{
    if (device == NULL)
        return;
    AR_DEBUG_PRINTF(ATH_DEBUG_TRACE, ("AR6000: delHifDevice; 0x%p\n", device));
    if (device->dma_buffer != NULL) {
        A_FREE(device->dma_buffer);
    }
    A_FREE(device);
}

static void ResetAllCards(void)
{
}

void HIFClaimDevice(HIF_DEVICE  *device, void *context)
{
    device->claimedContext = context;
}

void HIFSetMailboxSwap(HIF_DEVICE  *device)
{
    device->swap_mailbox = TRUE;
}

void HIFReleaseDevice(HIF_DEVICE  *device)
{
    device->claimedContext = NULL;
}

A_STATUS HIFAttachHTC(HIF_DEVICE *device, HTC_CALLBACKS *callbacks)
{
    if (device->htcCallbacks.context != NULL) {
            /* already in use! */
        return A_ERROR;
    }
    device->htcCallbacks = *callbacks;
    return A_OK;
}

static void hif_flush_async_task(HIF_DEVICE *device)
{
    if (device->async_task) {
        init_completion(&device->async_completion);
        device->async_shutdown = 1;
        up(&device->sem_async);
        wait_for_completion(&device->async_completion);
        device->async_task = NULL;
        sema_init(&device->sem_async, 0);
    }
}

void HIFDetachHTC(HIF_DEVICE *device)
{
    hif_flush_async_task(device);
    A_MEMZERO(&device->htcCallbacks,sizeof(device->htcCallbacks));
}

#define SDIO_SET_CMD52_ARG(arg,rw,func,raw,address,writedata) \
    (arg) = (((rw) & 1) << 31)           | \
            (((func) & 0x7) << 28)       | \
            (((raw) & 1) << 27)          | \
            (1 << 26)                    | \
            (((address) & 0x1FFFF) << 9) | \
            (1 << 8)                     | \
            ((writedata) & 0xFF)

#define SDIO_SET_CMD52_READ_ARG(arg,func,address) \
    SDIO_SET_CMD52_ARG(arg,0,(func),0,address,0x00)
#define SDIO_SET_CMD52_WRITE_ARG(arg,func,address,value) \
    SDIO_SET_CMD52_ARG(arg,1,(func),0,address,value)

static int Func0_CMD52WriteByte(struct mmc_card *card, unsigned int address, unsigned char byte)
{
    struct mmc_command ioCmd;
    unsigned long      arg;
    int status = 0;

    memset(&ioCmd,0,sizeof(ioCmd));
    SDIO_SET_CMD52_WRITE_ARG(arg,0,address,byte);
    ioCmd.opcode = SD_IO_RW_DIRECT;
    ioCmd.arg = arg;
    ioCmd.flags = MMC_RSP_R5 | MMC_CMD_AC;
    status = mmc_wait_for_cmd(card->host, &ioCmd, 0);

    if (status) AR_DEBUG_PRINTF(ATH_DEBUG_ERROR,
        ("AR6000: %s mmc_wait_for_cmd returned %d\n",__func__, status));

    return status;
}

static int Func0_CMD52ReadByte(struct mmc_card *card, unsigned int address, unsigned char *byte)
{
    struct mmc_command ioCmd;
    unsigned long      arg;
    A_INT32 err;

    memset(&ioCmd,0,sizeof(ioCmd));
    SDIO_SET_CMD52_READ_ARG(arg,0,address);
    ioCmd.opcode = SD_IO_RW_DIRECT;
    ioCmd.arg = arg;
    ioCmd.flags = MMC_RSP_R5 | MMC_CMD_AC;

    err = mmc_wait_for_cmd(card->host, &ioCmd, 0);

    if ((!err) && (byte)) {
        *byte =  ioCmd.resp[0] & 0xFF;
    }

    if (err) AR_DEBUG_PRINTF(ATH_DEBUG_ERROR, ("AR6000: %s mmc_wait_for_cmd returned %d\n",__func__, err));

    return err;
}

void HIFDumpCCCR(HIF_DEVICE *hif_device)
{
   int i;
   A_UINT8 cccr_val;
   A_UINT32 err;

   if (!hif_device || !hif_device->func|| !hif_device->func->card) {
      printk("HIFDumpCCCR incorrect input arguments\n");
      return;
   }

   printk("HIFDumpCCCR ");
   for (i = 0; i <= 0x16; i ++) {
     err = Func0_CMD52ReadByte(hif_device->func->card, i, &cccr_val);
     if (err) {
         printk("Reading CCCR 0x%02X failed: %d\n", (unsigned int)i, (unsigned int)err);
     } else {
         printk("%X(%X) ", (unsigned int)i, (unsigned int)cccr_val);
     }
   }
/*
   printk("\nHIFDumpCCCR ");
   for (i = 0xF0; i <= 0xFF; i ++) {
     err = Func0_CMD52ReadByte(hif_device->func->card, i, &cccr_val);
     if (err) {
         printk("Reading CCCR 0x%02X failed: %d\n", (unsigned int)i, (unsigned int)err);
     } else {
         printk("0x%02X(%02X)", (unsigned int)i, (unsigned int)cccr_val);
     }
   }
*/
   printk("\n");
}

void HIFsuspendwow(HIF_DEVICE *hif_device)
{
    printk(KERN_INFO "HIFsuspendwow TODO\n");
}

A_BOOL HIFIsMailBoxSwapped(HIF_DEVICE *hd)
{
    return ((struct hif_device *)hd)->swap_mailbox;
}

/**
 * hif_is_80211_fw_wow_required() - API to check if target suspend is needed
 *
 * API determines if fw can be suspended and returns true/false to the caller.
 * Caller will call WMA WoW API's to suspend.
 * The API returns true only for SDIO bus types, for others it's a false.
 *
 * Return: bool
 */
bool hif_is_80211_fw_wow_required(void)
{
	return true;
}

#ifdef CONFIG_CNSS_SDIO
static int hif_sdio_device_inserted(struct sdio_func *func, const struct sdio_device_id * id)
{
	return hifDeviceInserted(func, id);
}

static void hif_sdio_device_removed(struct sdio_func *func)
{
	hifDeviceRemoved(func);
}

static int hif_sdio_device_reinit(struct sdio_func *func, const struct sdio_device_id * id)
{
	vos_set_crash_indication_pending(true);
	return hifDeviceInserted(func, id);
}

static void hif_sdio_device_shutdown(struct sdio_func *func)
{
	vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);
	hifDeviceRemoved(func);
}

static void hif_sdio_crash_shutdown(struct sdio_func *func)
{
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27) && defined(CONFIG_PM)
static int hif_sdio_device_suspend(struct device *dev)
{
	return hifDeviceSuspend(dev);
}

static int hif_sdio_device_resume(struct device *dev)
{
	return hifDeviceResume(dev);
}
#endif
#endif
