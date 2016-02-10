/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

/*========================================================================

  \file  epping_main.c

  \brief WLAN End Point Ping test tool implementation

  ========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <wcnss_api.h>
#include <wlan_hdd_tx_rx.h>
#include <wniApi.h>
#include <wlan_nlink_srv.h>
#include <wlan_btc_svc.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <linux/rtnetlink.h>
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "bmi.h"
#include "ol_fw.h"
#include "ol_if_athvar.h"
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#elif defined(HIF_SDIO)
#include "if_ath_sdio.h"
#endif
#include "epping_main.h"
#include "epping_internal.h"

#ifdef TIMER_MANAGER
#define TIMER_MANAGER_STR " +TIMER_MANAGER"
#else
#define TIMER_MANAGER_STR ""
#endif

#ifdef MEMORY_DEBUG
#define MEMORY_DEBUG_STR " +MEMORY_DEBUG"
#else
#define MEMORY_DEBUG_STR ""
#endif

#if defined(HIF_PCI) || defined(HIF_USB)
extern int hif_register_driver(void);
extern void hif_unregister_driver(void);
#endif

/**---------------------------------------------------------------------------

  \brief epping_driver_init() - End point ping driver Init Function

   This is the driver entry point - called in different timeline depending
   on whether the driver is statically or dynamically linked

  \param  - con_mode connection mode

  \return - 0 for success, negative for failure

----------------------------------------------------------------------------*/
int epping_driver_init(int con_mode, vos_wake_lock_t *g_wake_lock,
                       char *pwlan_module_name)
{
   int ret = 0;
   unsigned long rc;
   epping_context_t *pEpping_ctx = NULL;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Enter", __func__);

#ifdef TIMER_MANAGER
   vos_timer_manager_init();
#endif
#ifdef MEMORY_DEBUG
   vos_mem_init();
#endif

   pEpping_ctx = vos_mem_malloc(sizeof(epping_context_t));
   if (pEpping_ctx == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL, "%s: No memory", __func__);
      ret = -ENOMEM;
      goto error1;
   }
   vos_mem_zero(pEpping_ctx, sizeof(epping_context_t));
   pEpping_ctx->g_wake_lock = g_wake_lock;
   pEpping_ctx->con_mode = con_mode;
   pEpping_ctx->pwlan_module_name = pwlan_module_name;

   status = vos_preOpen(&pEpping_ctx->pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to preOpen VOSS", __func__);
      ret = -1;
      goto error1;
   }

   /* save epping_context in VOSS */
   ((VosContextType *)(pEpping_ctx->pVosContext))->pHDDContext =
      (v_VOID_t*)pEpping_ctx;

#ifdef HIF_SDIO
#define WLAN_WAIT_TIME_WLANSTART 10000
#else
#define WLAN_WAIT_TIME_WLANSTART 2000
#endif
   init_completion(&pEpping_ctx->wlan_start_comp);
   ret = hif_register_driver();
   if (!ret) {
      rc = wait_for_completion_timeout(
               &pEpping_ctx->wlan_start_comp,
               msecs_to_jiffies(WLAN_WAIT_TIME_WLANSTART));
      if (!rc) {
         EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
            "%s: timed-out waiting for hif_register_driver", __func__);
         ret = -1;
      } else
         ret = 0;
   }
   if (ret)
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: %s driver Initialization failed",
         __func__, pEpping_ctx->pwlan_module_name);
      hif_unregister_driver();
      vos_preClose(&pEpping_ctx->pVosContext);
      ret = -ENODEV;
      vos_mem_free(pEpping_ctx);

#ifdef MEMORY_DEBUG
      vos_mem_exit();
#endif
#ifdef TIMER_MANAGER
      vos_timer_exit();
#endif
      return ret;
   } else {
      pr_info("%s: %s driver loaded\n",
         __func__, pEpping_ctx->pwlan_module_name);
      return 0;
   }
error1:
   if (pEpping_ctx) {
      vos_mem_free(pEpping_ctx);
      pEpping_ctx = NULL;
   }
#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif
#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif
   return ret;
}

void epping_exit(v_CONTEXT_t pVosContext)
{
   epping_context_t *pEpping_ctx;
   VosContextType *gpVosContext;

   pEpping_ctx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if (pEpping_ctx == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: error: pEpping_ctx  = NULL",
         __func__);
      return;
   }
   gpVosContext = pEpping_ctx->pVosContext;
   if (pVosContext == NULL) {
         EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
            "%s: error: pVosContext  = NULL",
            __func__);
         return;
      }
   if (pEpping_ctx->epping_adapter) {
      epping_destroy_adapter(pEpping_ctx->epping_adapter);
      pEpping_ctx->epping_adapter = NULL;
   }
   hif_disable_isr(gpVosContext->pHIFContext);
   hif_reset_soc(gpVosContext->pHIFContext);
   HTCStop(gpVosContext->htc_ctx);
   HTCDestroy(gpVosContext->htc_ctx);
   gpVosContext->htc_ctx = NULL;
#ifdef HIF_PCI
   {
      int i;
      for (i = 0; i < EPPING_MAX_NUM_EPIDS; i++) {
         epping_unregister_tx_copier(i, pEpping_ctx);
      }
   }
#endif /* HIF_PCI */
   epping_cookie_cleanup(pEpping_ctx);
   vos_mem_free(pEpping_ctx);
}

void epping_driver_exit(v_CONTEXT_t pVosContext)
{
   epping_context_t *pEpping_ctx;

   pr_info("%s: unloading driver\n", __func__);

   pEpping_ctx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);

   if(!pEpping_ctx)
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: module exit called before probe",__func__);
   }
   else
   {
#ifdef QCA_PKT_PROTO_TRACE
      vos_pkt_proto_trace_close();
#endif /* QCA_PKT_PROTO_TRACE */
      //pHddCtx->isUnloadInProgress = TRUE;
      vos_set_unload_in_progress(TRUE);
      vos_set_load_unload_in_progress(VOS_MODULE_ID_VOSS, TRUE);
   }
   hif_unregister_driver();
   vos_preClose( &pVosContext );
#ifdef MEMORY_DEBUG
   vos_mem_exit();
#endif
#ifdef TIMER_MANAGER
   vos_timer_exit();
#endif
   pr_info("%s: driver unloaded\n", __func__);
}

static void epping_target_suspend_acknowledge(void *context)
{
   void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
   epping_context_t *pEpping_ctx = vos_get_context(VOS_MODULE_ID_HDD,
                                                   vos_context);
   int wow_nack = *((int *)context);

   if (NULL == pEpping_ctx) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: epping_ctx is NULL", __func__);
      return;
   }
    /* EPPING_TODO: do we need wow_nack? */
   pEpping_ctx->wow_nack = wow_nack;
}

int epping_wlan_startup(struct device *parent_dev, v_VOID_t *hif_sc)
{
   int ret = 0;
   epping_context_t *pEpping_ctx = NULL;
   VosContextType *pVosContext = NULL;
   HTC_INIT_INFO  htcInfo;
   struct ol_softc *scn;
   tSirMacAddr adapter_macAddr;
   adf_os_device_t adf_ctx;

   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Enter", __func__);

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

   if(pVosContext == NULL)
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Failed vos_get_global_context", __func__);
      ret = -1;
      return ret;
   }

   pEpping_ctx = vos_get_context(VOS_MODULE_ID_HDD, pVosContext);
   if(pEpping_ctx == NULL)
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to get pEpping_ctx", __func__);
      ret = -1;
      return ret;
   }
   pEpping_ctx->parent_dev = (void *)parent_dev;
   epping_get_dummy_mac_addr(adapter_macAddr);

   ((VosContextType*)pVosContext)->pHIFContext = hif_sc;

   /* store target type and target version info in hdd ctx */
   pEpping_ctx->target_type = ((struct ol_softc *)hif_sc)->target_type;

   /* Initialize the timer module */
   vos_timer_module_init();

   scn = vos_get_context(VOS_MODULE_ID_HIF, pVosContext);
   if (!scn) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: scn is null!", __func__);
      return -1;
   }
   scn->enableuartprint = 0;
   scn->enablefwlog     = 0;

   /* Initialize BMI and Download firmware */
   if (bmi_download_firmware(scn)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: BMI failed to download target", __func__);
      BMICleanup(scn);
      return -1;
   }

   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH,
      "%s: bmi_download_firmware done", __func__);

   htcInfo.pContext = pVosContext->pHIFContext;
   htcInfo.TargetFailure = ol_target_failure;
   htcInfo.TargetSendSuspendComplete = epping_target_suspend_acknowledge;
   adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, pVosContext);

   /* Create HTC */
   pVosContext->htc_ctx = HTCCreate(htcInfo.pContext, &htcInfo, adf_ctx);
   if (!pVosContext->htc_ctx) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to Create HTC", __func__);
      BMICleanup(scn);
      return -1;
   }
   pEpping_ctx->HTCHandle = vos_get_context(VOS_MODULE_ID_HTC, pVosContext);
   if (pEpping_ctx->HTCHandle == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: HTCHandle is NULL", __func__);
      return -1;
   }
   scn->htc_handle = pEpping_ctx->HTCHandle;

   HIFClaimDevice(scn->hif_hdl, scn);

   if (bmi_done(scn)) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Failed to complete BMI phase", __func__);
      goto error_end;
   }
   /* start HIF */
   if (HTCWaitTarget(scn->htc_handle) != A_OK) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: HTCWaitTarget error", __func__);
      goto error_end;
   }
   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH,
      "%s: HTC ready", __func__);

   ret = epping_connect_service(pEpping_ctx);
   if (ret != 0) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: HTCWaitTargetdone", __func__);
      goto error_end;
   }
   if (HTCStart(pEpping_ctx->HTCHandle) != A_OK) {
      goto error_end;
   }
   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH,
      "%s: HTC started", __func__);

   /* init the tx cookie resource */
   ret = epping_cookie_init(pEpping_ctx);
   if (ret == 0) {
      pEpping_ctx->epping_adapter = epping_add_adapter(pEpping_ctx,
                                       adapter_macAddr,
                                       WLAN_HDD_INFRA_STATION);
   }
   if (ret < 0 || pEpping_ctx->epping_adapter == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: epping_add_adaptererror error", __func__);
      HTCStop(pEpping_ctx->HTCHandle);
      epping_cookie_cleanup(pEpping_ctx);
      goto error_end;
   }
#ifdef HIF_PCI
   {
      int i;
      for (i = 0; i < EPPING_MAX_NUM_EPIDS; i++) {
         epping_register_tx_copier(i, pEpping_ctx);
      }
   }
#endif /* HIF_PCI */
   EPPING_LOG(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Exit", __func__);
   complete(&pEpping_ctx->wlan_start_comp);
   return ret;

error_end:
   HTCDestroy(pVosContext->htc_ctx);
   pVosContext->htc_ctx = NULL;
   BMICleanup(scn);
   return -1;
}
