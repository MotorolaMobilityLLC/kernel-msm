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

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <osdep.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include "bmi_msg.h" /* TARGET_TYPE_ */
#include "if_ath_sdio.h"
#include "vos_api.h"

#ifndef REMOVE_PKT_LOG
#include "ol_txrx_types.h"
#include "pktlog_ac_api.h"
#include "pktlog_ac.h"
#endif
#include "epping_main.h"

#ifndef ATH_BUS_PM
#ifdef CONFIG_PM
#define ATH_BUS_PM
#endif /* CONFIG_PM */
#endif /* ATH_BUS_PM */

#ifndef REMOVE_PKT_LOG
struct ol_pl_os_dep_funcs *g_ol_pl_os_dep_funcs = NULL;
#endif

typedef void * hif_handle_t;
typedef void * hif_softc_t;

extern int hdd_wlan_startup(struct device *dev, void *hif_sc);
extern void __hdd_wlan_exit(void);

struct ath_hif_sdio_softc *sc = NULL;

static A_STATUS
ath_hif_sdio_probe(void *context, void *hif_handle)
{
    int ret = 0;
    struct ol_softc *ol_sc;
    HIF_DEVICE_OS_DEVICE_INFO os_dev_info;
    struct sdio_func *func = NULL;
    const struct sdio_device_id *id;
    u_int32_t target_type;
    ENTER();

    sc = (struct ath_hif_sdio_softc *) A_MALLOC(sizeof(*sc));
    if (!sc) {
        ret = -ENOMEM;
        goto err_alloc;
    }
    A_MEMZERO(sc,sizeof(*sc));


    sc->hif_handle = hif_handle;
    HIFConfigureDevice(hif_handle, HIF_DEVICE_GET_OS_DEVICE, &os_dev_info, sizeof(os_dev_info));

    sc->aps_osdev.device = os_dev_info.pOSDevice;
    sc->aps_osdev.bc.bc_bustype = HAL_BUS_TYPE_SDIO;
    spin_lock_init(&sc->target_lock);

    {
        /*
         * Attach Target register table.  This is needed early on --
         * even before BMI -- since PCI and HIF initialization (and BMI init)
         * directly access Target registers (e.g. CE registers).
         *
         * TBDXXX: targetdef should not be global -- should be stored
         * in per-device struct so that we can support multiple
         * different Target types with a single Host driver.
         * The whole notion of an "hif type" -- (not as in the hif
         * module, but generic "Host Interface Type") is bizarre.
         * At first, one one expect it to be things like SDIO, USB, PCI.
         * But instead, it's an actual platform type. Inexplicably, the
         * values used for HIF platform types are *different* from the
         * values used for Target Types.
         */

#if defined(CONFIG_AR9888_SUPPORT)
        hif_register_tbl_attach(HIF_TYPE_AR9888);
        target_register_tbl_attach(TARGET_TYPE_AR9888);
        target_type = TARGET_TYPE_AR9888;
#elif defined(CONFIG_AR6320_SUPPORT)
        id = ((HIF_DEVICE*)hif_handle)->id;
        if (id->device == MANUFACTURER_ID_QCA9377_BASE) {
            hif_register_tbl_attach(HIF_TYPE_AR6320V2);
            target_register_tbl_attach(TARGET_TYPE_AR6320V2);
        } else {
            hif_register_tbl_attach(HIF_TYPE_AR6320);
            target_register_tbl_attach(TARGET_TYPE_AR6320);
        }
        target_type = TARGET_TYPE_AR6320;

#endif
    }
    func = ((HIF_DEVICE*)hif_handle)->func;

    ol_sc = A_MALLOC(sizeof(*ol_sc));
    if (!ol_sc){
           ret = -ENOMEM;
           goto err_attach;
    }
    OS_MEMZERO(ol_sc, sizeof(*ol_sc));
    ol_sc->sc_osdev = &sc->aps_osdev;
    ol_sc->hif_sc = (void *)sc;
    sc->ol_sc = ol_sc;
    ol_sc->target_type = target_type;
    ol_sc->enableuartprint = 1;
    ol_sc->enablefwlog = 0;
    ol_sc->enablesinglebinary = FALSE;
    ol_sc->max_no_of_peers = 1;

    ol_sc->hif_hdl = hif_handle;

    ol_sc->ramdump_base = ioremap(RAMDUMP_ADDR, RAMDUMP_SIZE);
    ol_sc->ramdump_size = RAMDUMP_SIZE;
    if (ol_sc->ramdump_base == NULL) {
        ol_sc->ramdump_base = 0;
        ol_sc->ramdump_size = 0;
    }
    init_waitqueue_head(&ol_sc->sc_osdev->event_queue);

    if (athdiag_procfs_init(sc) != 0) {
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_ERROR,
                "%s athdiag_procfs_init failed",__func__);
        ret =  A_ERROR;
        goto err_attach1;
    }

    ret = hdd_wlan_startup(&(func->dev), ol_sc);
    if ( ret ) {
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_FATAL," hdd_wlan_startup failed");
        goto err_attach2;
    }else{
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO," hdd_wlan_startup success!");
    }

	/* epping is minimum ethernet driver and the
	 * epping fw does not support pktlog, etc.
	 * After hdd_wladriver is epping directly return. */
	if (WLAN_IS_EPPING_ENABLED(vos_get_conparam()))
		goto end;

#ifndef REMOVE_PKT_LOG
    if (vos_get_conparam() != VOS_FTM_MODE) {
        /*
         * pktlog initialization
         */
        ol_pl_sethandle(&ol_sc->pdev_txrx_handle->pl_dev, ol_sc);

        if (pktlogmod_init(ol_sc))
            printk(KERN_ERR "%s: pktlogmod_init failed\n", __func__);
    }
#endif
end:
    return 0;

err_attach2:
    athdiag_procfs_remove();
err_attach1:
    A_FREE(ol_sc);
err_attach:
    A_FREE(sc);
    sc = NULL;
err_alloc:
    return ret;
}

int
ol_ath_sdio_configure(hif_softc_t hif_sc, struct net_device *dev, hif_handle_t *hif_hdl)
{
    struct ath_hif_sdio_softc *sc = (struct ath_hif_sdio_softc *) hif_sc;
    int ret = 0;

    sc->aps_osdev.netdev = dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
    SET_MODULE_OWNER(dev);
#endif

    *hif_hdl = sc->hif_handle;

    return ret;
}

static A_STATUS
ath_hif_sdio_remove(void *context, void *hif_handle)
{
    ENTER();

    if (!sc) {
        VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_ERROR,
                  "Global SDIO context is NULL");
        return A_ERROR;
    }

    athdiag_procfs_remove();

    iounmap(sc->ol_sc->ramdump_base);

#ifndef REMOVE_PKT_LOG
    if (vos_get_conparam() != VOS_FTM_MODE &&
		!WLAN_IS_EPPING_ENABLED(vos_get_conparam())){
        if (sc && sc->ol_sc)
            pktlogmod_exit(sc->ol_sc);
    }
#endif

    //cleaning up the upper layers
    __hdd_wlan_exit();

    if (sc && sc->ol_sc){
       A_FREE(sc->ol_sc);
       sc->ol_sc = NULL;
    }
    if (sc) {
        A_FREE(sc);
        sc = NULL;
    }

    EXIT();
    return 0;
}

static A_STATUS
ath_hif_sdio_suspend(void *context)
{
    printk(KERN_INFO "ol_ath_sdio_suspend TODO\n");
    return 0;
}

static A_STATUS
ath_hif_sdio_resume(void *context)
{
    printk(KERN_INFO "ol_ath_sdio_resume ODO\n");
    return 0;
}

static A_STATUS
ath_hif_sdio_power_change(void *context, A_UINT32 config)
{
    printk(KERN_INFO "ol_ath_sdio_power change TODO\n");
    return 0;
}

/*
 * Module glue.
 */
#include <linux/version.h>
static char *version = "HIF (Atheros/multi-bss)";
static char *dev_info = "ath_hif_sdio";

static int init_ath_hif_sdio(void)
{
    static int probed = 0;
    A_STATUS status;
    OSDRV_CALLBACKS osdrvCallbacks;
    ENTER();

    A_MEMZERO(&osdrvCallbacks,sizeof(osdrvCallbacks));
    osdrvCallbacks.deviceInsertedHandler = ath_hif_sdio_probe;
    osdrvCallbacks.deviceRemovedHandler = ath_hif_sdio_remove;
#ifdef CONFIG_PM
    osdrvCallbacks.deviceSuspendHandler = ath_hif_sdio_suspend;
    osdrvCallbacks.deviceResumeHandler = ath_hif_sdio_resume;
    osdrvCallbacks.devicePowerChangeHandler = ath_hif_sdio_power_change;
#endif

    if (probed) {
        return -ENODEV;
    }
    probed++;

    VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO,"%s %d",__func__,__LINE__);
    status = HIFInit(&osdrvCallbacks);
    if(status != A_OK){
       VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_FATAL, "%s HIFInit failed!",__func__);
        return -ENODEV;
    }

    printk(KERN_INFO "%s: %s\n", dev_info, version);

    return 0;
}

int hif_register_driver(void)
{
   int status = 0;
   ENTER();
   status = init_ath_hif_sdio();
   EXIT("status = %d", status);
   return status;

}

void hif_unregister_driver(void)
{
   ENTER();
   HIFShutDownDevice(NULL);
   EXIT();
   return ;
}

void hif_init_adf_ctx(adf_os_device_t adf_dev, void *ol_sc)
{
   struct ol_softc *ol_sc_local = (struct ol_softc *)ol_sc;
   struct ath_hif_sdio_softc *hif_sc = ol_sc_local->hif_sc;
   ENTER();
   adf_dev->drv = &hif_sc->aps_osdev;
   adf_dev->dev = hif_sc->aps_osdev.device;
   ol_sc_local->adf_dev = adf_dev;
   EXIT();
}

void hif_deinit_adf_ctx(void *ol_sc)
{
   struct ol_softc *sc = (struct ol_softc *)ol_sc;
   sc->adf_dev = NULL;
}

/* Function to set the TXRX handle in the ol_sc context */
void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle)
{
   struct ol_softc *sc = (struct ol_softc *)ol_sc;
   ENTER();
   sc->pdev_txrx_handle = txrx_handle;
}

void hif_disable_isr(void *ol_sc)
{
   ENTER("- dummy function!");
}

void
HIFSetTargetSleep(HIF_DEVICE *hif_device, A_BOOL sleep_ok, A_BOOL wait_for_it)
{
   ENTER("- dummy function!");
}

void
HIFCancelDeferredTargetSleep(HIF_DEVICE *hif_device)
{
    ENTER("- dummy function!");

}

/* Function to reset SoC */
void hif_reset_soc(void *ol_sc)
{
    ENTER("- dummy function!");
}

void hif_get_hw_info(void *ol_sc, u32 *version, u32 *revision)
{
    *version = ((struct ol_softc *)ol_sc)->target_version;
    /* Chip revision should be supported, set to 0 for now */
    *revision = 0;
}

void hif_set_fw_info(void *ol_sc, u32 target_fw_version)
{
    ((struct ol_softc *)ol_sc)->target_fw_version = target_fw_version;
}

int hif_pm_runtime_prevent_suspend(void *ol_sc)
{
    return 0;
}

int hif_pm_runtime_allow_suspend(void *ol_sc)
{
    return 0;
}

int hif_pm_runtime_prevent_suspend_timeout(void *ol_sc, unsigned int delay)
{
        return 0;
}
