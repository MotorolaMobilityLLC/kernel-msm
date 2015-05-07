/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

/**========================================================================

  \file  wlan_hdd_ftm.c

  \brief This file contains the WLAN factory test mode implementation

  ========================================================================*/

/**=========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  07/18/14    kanand        Cleanup. Remove legacy Prima code and retain  support for Rome only
  04/20/11    Leo/Henri     Convergence for Prima and Volans. Single image for FTM and mission mode
  04/05/09    Shailender    Created module.

  ==========================================================================*/
#include <vos_mq.h>
#include "vos_sched.h"
#include <vos_api.h>
#include "sirTypes.h"
#include "halTypes.h"
#include "sirApi.h"
#include "sirMacProtDef.h"
#include "sme_Api.h"
#include "macInitApi.h"
#include "wlan_qct_sys.h"
#include "wlan_qct_tl.h"
#include "wlan_hdd_misc.h"
#include "i_vos_packet.h"
#include "vos_nvitem.h"
#include "wlan_hdd_main.h"
#include "qwlan_version.h"
#include "wlan_nv.h"
#include "wlan_qct_wda.h"
#include "cfgApi.h"
#include "wlan_qct_pal_device.h"

#if  defined(QCA_WIFI_FTM)
#include "bmi.h"
#include "ol_fw.h"
#include "testmode.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_main.h"
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#include <linux/compat.h>
#elif defined(HIF_SDIO)
#include "if_ath_sdio.h"
#endif
#endif

static int wlan_ftm_stop(hdd_context_t *pHddCtx);
static int hdd_ftm_service_registration(hdd_context_t *pHddCtx);

#if  defined(QCA_WIFI_FTM)
#if defined(LINUX_QCMBR)
#define ATH_XIOCTL_UNIFIED_UTF_CMD  0x1000
#define ATH_XIOCTL_UNIFIED_UTF_RSP  0x1001
#define MAX_UTF_LENGTH              1024
typedef struct qcmbr_data_s {
    unsigned int cmd;
    unsigned int length;
    unsigned char buf[MAX_UTF_LENGTH + 4];
    unsigned int copy_to_user;
} qcmbr_data_t;
typedef struct qcmbr_queue_s {
    unsigned char utf_buf[MAX_UTF_LENGTH + 4];
    struct list_head list;
} qcmbr_queue_t;
LIST_HEAD(qcmbr_queue_head);
DEFINE_SPINLOCK(qcmbr_queue_lock);
#endif
#endif


/**---------------------------------------------------------------------------

  \brief wlan_ftm_postmsg() -

   The function used for sending the command to the halphy.

  \param  - cmd_ptr - Pointer command buffer.

  \param  - cmd_len - Command length.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static v_U32_t wlan_ftm_postmsg(v_U8_t *cmd_ptr, v_U16_t cmd_len)
{
    vos_msg_t   *ftmReqMsg;
    vos_msg_t    ftmMsg;
    ENTER();

    ftmReqMsg = (vos_msg_t *) cmd_ptr;

    ftmMsg.type = WDA_FTM_CMD_REQ;
    ftmMsg.reserved = 0;
    ftmMsg.bodyptr = (v_U8_t*)cmd_ptr;
    ftmMsg.bodyval = 0;

    /* Use Vos messaging mechanism to send the command to halPhy */
    if (VOS_STATUS_SUCCESS != vos_mq_post_message(
        VOS_MODULE_ID_WDA,
                                    (vos_msg_t *)&ftmMsg)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: : Failed to post Msg to HAL", __func__);

        return VOS_STATUS_E_FAILURE;
    }

    EXIT();
    return VOS_STATUS_SUCCESS;
}

void wlan_hdd_ftm_update_tgt_cfg(void *context, void *param)
{
    hdd_context_t *hdd_ctx = (hdd_context_t *)context;
    struct hdd_tgt_cfg *cfg = (struct hdd_tgt_cfg *)param;

    if (!vos_is_macaddr_zero(&cfg->hw_macaddr)) {
        hdd_update_macaddr(hdd_ctx->cfg_ini, cfg->hw_macaddr);
    } else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid MAC passed from target, using MAC from ini file"
               MAC_ADDRESS_STR, __func__,
               MAC_ADDR_ARRAY(hdd_ctx->cfg_ini->intfMacAddr[0].bytes));
    }
}

/*---------------------------------------------------------------------------

  \brief wlan_ftm_vos_open() - Open the vOSS Module

  The \a wlan_ftm_vos_open() function opens the vOSS Scheduler
  Upon successful initialization:

     - All VOS submodules should have been initialized

     - The VOS scheduler should have opened

     - All the WLAN SW components should have been opened. This include
       MAC.


  \param  hddContextSize: Size of the HDD context to allocate.


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initialize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa wlan_ftm_vos_open()

---------------------------------------------------------------------------*/
static VOS_STATUS wlan_ftm_vos_open( v_CONTEXT_t pVosContext, v_SIZE_t hddContextSize )
{
   VOS_STATUS vStatus      = VOS_STATUS_SUCCESS;
   int iter                = 0;
   tSirRetStatus sirStatus = eSIR_SUCCESS;
   tMacOpenParameters macOpenParms;
   pVosContextType gpVosContext = (pVosContextType)pVosContext;
#if  defined(QCA_WIFI_FTM)
   adf_os_device_t adf_ctx;
   HTC_INIT_INFO  htcInfo;
   v_PVOID_t pHifContext = NULL;
   v_PVOID_t pHtcContext = NULL;
#endif
   hdd_context_t *pHddCtx;

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Opening VOSS", __func__);

   if (NULL == gpVosContext)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Trying to open VOSS without a PreOpen",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Initialize the probe event */
   if (vos_event_init(&gpVosContext->ProbeEvent) != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Unable to init probeEvent",__func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   if(vos_event_init(&(gpVosContext->wdaCompleteEvent)) != VOS_STATUS_SUCCESS )
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Unable to init wdaCompleteEvent",__func__);
      VOS_ASSERT(0);

      goto err_probe_event;
   }

   /* Initialize the free message queue */
   vStatus = vos_mq_init(&gpVosContext->freeVosMq);
   if (! VOS_IS_STATUS_SUCCESS(vStatus))
   {

      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to initialize VOS free message queue %d",
                 __func__, vStatus);
      VOS_ASSERT(0);
      goto err_wda_complete_event;
   }

   for (iter = 0; iter < VOS_CORE_MAX_MESSAGES; iter++)
   {
      (gpVosContext->aMsgWrappers[iter]).pVosMsg =
         &(gpVosContext->aMsgBuffers[iter]);
      INIT_LIST_HEAD(&gpVosContext->aMsgWrappers[iter].msgNode);
      vos_mq_put(&gpVosContext->freeVosMq, &(gpVosContext->aMsgWrappers[iter]));
   }

   /* Now Open the VOS Scheduler */
   vStatus= vos_sched_open(gpVosContext, &gpVosContext->vosSched,
                           sizeof(VosSchedContext));

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to open VOS Scheduler %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_msg_queue;
   }

#if  defined(QCA_WIFI_FTM)
   /* Initialize BMI and Download firmware */
   pHifContext = vos_get_context(VOS_MODULE_ID_HIF, gpVosContext);
   if (!pHifContext)
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: failed to get HIF context", __func__);
       goto err_sched_close;
   }

   if (bmi_download_firmware(pHifContext)) {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: BMI failed to download target", __func__);
       goto err_bmi_close;
   }
   htcInfo.pContext = gpVosContext->pHIFContext;
   htcInfo.TargetFailure = ol_target_failure;
   htcInfo.TargetSendSuspendComplete = wma_target_suspend_acknowledge;
   adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, gpVosContext);

   /* Create HTC */
   gpVosContext->htc_ctx = HTCCreate(htcInfo.pContext, &htcInfo, adf_ctx);
   if (!gpVosContext->htc_ctx) {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: Failed to Create HTC", __func__);
           goto err_bmi_close;
           goto err_sched_close;
   }

   if (bmi_done(pHifContext)) {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: Failed to complete BMI phase", __func__);
       goto err_htc_close;
   }
#endif /* QCA_WIFI_FTM */

   /* Open the SYS module */
   vStatus = sysOpen(gpVosContext);

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to open SYS module %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_sched_close;
   }

   /*Open the WDA module */
   vos_mem_set(&macOpenParms, sizeof(macOpenParms), 0);
   macOpenParms.driverType = eDRIVER_TYPE_MFG;

   pHddCtx = (hdd_context_t*)(gpVosContext->pHDDContext);
   if((NULL == pHddCtx) ||
      (NULL == pHddCtx->cfg_ini))
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Hdd Context is Null", __func__);
     VOS_ASSERT(0);
     goto err_sys_close;
   }

   macOpenParms.powersaveOffloadEnabled =
      pHddCtx->cfg_ini->enablePowersaveOffload;
   vStatus = WDA_open(gpVosContext, gpVosContext->pHDDContext,
                      wlan_hdd_ftm_update_tgt_cfg, NULL,
                      &macOpenParms);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to open WDA module %d", __func__, vStatus);
      VOS_ASSERT(0);
      goto err_sys_close;
   }

#if  defined(QCA_WIFI_FTM)
   pHtcContext = vos_get_context(VOS_MODULE_ID_HTC, gpVosContext);
   if (!pHtcContext)
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: failed to get HTC context", __func__);
       goto err_wda_close;
   }
   if (HTCWaitTarget(pHtcContext)) {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to complete BMI phase", __func__);
           goto err_wda_close;
   }
#endif

   /* initialize the NV module */
   vStatus = vos_nv_open();
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     // NV module cannot be initialized, however the driver is allowed
     // to proceed
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to initialize the NV module %d", __func__, vStatus);
     goto err_wda_close;
   }

   /* If we arrive here, both threads dispatching messages correctly */

   /* Now proceed to open the MAC */

   /* UMA is supported in hardware for performing the
      frame translation 802.11 <-> 802.3 */
   macOpenParms.frameTransRequired = 1;

   sirStatus = macOpen(&(gpVosContext->pMACContext), gpVosContext->pHDDContext,
                         &macOpenParms);

   if (eSIR_SUCCESS != sirStatus)
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open MAC %d", __func__, sirStatus);
     VOS_ASSERT(0);
     goto err_nv_close;
   }

#ifndef QCA_WIFI_FTM
   /* Now proceed to open the SME */
   vStatus = sme_Open(gpVosContext->pMACContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to open SME %d", __func__, vStatus);
      goto err_mac_close;
   }

   vStatus = sme_init_chan_list(gpVosContext->pMACContext,
                                pHddCtx->reg.alpha2, pHddCtx->reg.cc_src);
   if (!VOS_IS_STATUS_SUCCESS(vStatus)) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to init sme channel list", __func__);
   } else {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: VOSS successfully Opened", __func__);
      return VOS_STATUS_SUCCESS;
   }
#else
   return VOS_STATUS_SUCCESS;
#endif

#ifndef QCA_WIFI_FTM
err_mac_close:
#endif
   macClose(gpVosContext->pMACContext);

err_nv_close:
   vos_nv_close();

err_wda_close:
   WDA_close(gpVosContext);

err_sys_close:
   sysClose(gpVosContext);

#if  defined(QCA_WIFI_FTM)
err_htc_close:
   if (gpVosContext->htc_ctx) {
      HTCDestroy(gpVosContext->htc_ctx);
      gpVosContext->htc_ctx = NULL;
   }

err_bmi_close:
   BMICleanup(pHifContext);
#endif /* QCA_WIFI_FTM */

err_sched_close:
   vos_sched_close(gpVosContext);
err_msg_queue:
   vos_mq_deinit(&gpVosContext->freeVosMq);

err_wda_complete_event:
   vos_event_destroy(&gpVosContext->wdaCompleteEvent);

err_probe_event:
   vos_event_destroy(&gpVosContext->ProbeEvent);

   return VOS_STATUS_E_FAILURE;

} /* wlan_ftm_vos_open() */

/*---------------------------------------------------------------------------

  \brief wlan_ftm_vos_close() - Close the vOSS Module

  The \a wlan_ftm_vos_close() function closes the vOSS Module

  \param vosContext  context of vos

  \return VOS_STATUS_SUCCESS - successfully closed

  \sa wlan_ftm_vos_close()

---------------------------------------------------------------------------*/

static VOS_STATUS wlan_ftm_vos_close( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;
  pVosContextType gpVosContext = (pVosContextType)vosContext;

#ifndef QCA_WIFI_FTM
  vosStatus = sme_Close(((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close SME %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif

  vosStatus = macClose( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close MAC %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  ((pVosContextType)vosContext)->pMACContext = NULL;

  vosStatus = vos_nv_close();
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close NV %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }


  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close SYS %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = WDA_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close WDA %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

#if  defined(QCA_WIFI_FTM)
  if (gpVosContext->htc_ctx)
  {
      HTCStop(gpVosContext->htc_ctx);
      HTCDestroy(gpVosContext->htc_ctx);
      gpVosContext->htc_ctx = NULL;
  }
  vosStatus = wma_wmi_service_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close wma_wmi_service", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  hif_disable_isr(gpVosContext->pHIFContext);
#endif

  vos_mq_deinit(&((pVosContextType)vosContext)->freeVosMq);

  vosStatus = vos_event_destroy(&gpVosContext->ProbeEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to destroy ProbeEvent %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_event_destroy(&gpVosContext->wdaCompleteEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to destroy wdaCompleteEvent %d", __func__, vosStatus);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  return VOS_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------

  \brief vos_ftm_preStart() -

  The \a vos_ftm_preStart() function to download CFG.
  including:
      - ccmStart

      - WDA: triggers the CFG download


  \param  pVosContext: The VOS context


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initialize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_start

---------------------------------------------------------------------------*/
VOS_STATUS vos_ftm_preStart( v_CONTEXT_t vosContext )
{
   VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
   pVosContextType pVosContext = (pVosContextType)vosContext;
#if  defined(QCA_WIFI_FTM)
   pVosContextType gpVosContext = vos_get_global_context(VOS_MODULE_ID_VOSS,
                                                         NULL);
#endif

   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO,
             "vos prestart");
   if (NULL == pVosContext->pWDAContext)
   {
      VOS_ASSERT(0);
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
            "%s: WDA NULL context", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   /* call macPreStart */
   vStatus = macPreStart(pVosContext->pMACContext);
   if ( !VOS_IS_STATUS_SUCCESS(vStatus) )
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Failed at macPreStart ");
      return VOS_STATUS_E_FAILURE;
   }

   /* call ccmStart */
   ccmStart(pVosContext->pMACContext);

   /* Reset wda wait event */
   vos_event_reset(&pVosContext->wdaCompleteEvent);


   /*call WDA pre start*/
   vStatus = WDA_preStart(pVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR,
             "Failed to WDA prestart ");
      ccmStop(pVosContext->pMACContext);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Need to update time out of complete */
   vStatus = vos_wait_single_event( &pVosContext->wdaCompleteEvent, 1000);
   if ( vStatus != VOS_STATUS_SUCCESS )
   {
      if ( vStatus == VOS_STATUS_E_TIMEOUT )
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Timeout occurred before WDA complete", __func__);
      }
      else
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: WDA_preStart reporting  other error", __func__);
      }
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }

#if  defined(QCA_WIFI_FTM)
   vStatus = HTCStart(gpVosContext->htc_ctx);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_FATAL,
               "Failed to Start HTC");
      ccmStop(gpVosContext->pMACContext);
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }
   wma_wait_for_ready_event(gpVosContext->pWDAContext);
#endif /* QCA_WIFI_FTM */

   return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_open() -

   The function hdd_wlan_startup calls this function to initialize the FTM specific modules.

  \param  - pAdapter - Pointer HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

int wlan_hdd_ftm_open(hdd_context_t *pHddCtx)
{
    VOS_STATUS vStatus       = VOS_STATUS_SUCCESS;
    pVosContextType pVosContext= NULL;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Opening VOSS", __func__);

    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);

    if (NULL == pVosContext)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Trying to open VOSS without a PreOpen", __func__);
        VOS_ASSERT(0);
        goto err_vos_status_failure;
    }

   vStatus = wlan_ftm_vos_open( pVosContext, 0);

   if ( !VOS_IS_STATUS_SUCCESS( vStatus ))
   {
      hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_open failed", __func__);
      goto err_vos_status_failure;
   }

   /*
    * For Integrated SOC, only needed to start WDA,
    * which happens in wlan_hdd_ftm_start()
    */
    /* Save the hal context in Adapter */
    pHddCtx->hHal = (tHalHandle)vos_get_context(VOS_MODULE_ID_SME, pVosContext );

    if ( NULL == pHddCtx->hHal )
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: HAL context is null", __func__);
       goto err_ftm_close;
    }

    return VOS_STATUS_SUCCESS;

err_ftm_close:
    wlan_ftm_vos_close(pVosContext);

err_vos_status_failure:
    return VOS_STATUS_E_FAILURE;
}

static int hdd_ftm_service_registration(hdd_context_t *pHddCtx)
{
    hdd_adapter_t *pAdapter;
    pAdapter = hdd_open_adapter( pHddCtx, WLAN_HDD_FTM, "wlan%d",
                wlan_hdd_get_intf_addr(pHddCtx), FALSE);
    if( NULL == pAdapter )
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: hdd_open_adapter failed", __func__);
               goto err_adapter_open_failure;
    }

    /* Initialize the ftm vos event */
    if (vos_event_init(&pHddCtx->ftm.ftm_vos_event) != VOS_STATUS_SUCCESS)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: Unable to init probeEvent", __func__);
        VOS_ASSERT(0);
        goto err_adapter_close;
    }

    pHddCtx->ftm.ftm_state = WLAN_FTM_INITIALIZED;

    return VOS_STATUS_SUCCESS;

err_adapter_close:
hdd_close_all_adapters( pHddCtx );

err_adapter_open_failure:

    return VOS_STATUS_E_FAILURE;
}



int wlan_hdd_ftm_close(hdd_context_t *pHddCtx)
{
    VOS_STATUS vosStatus;
    v_CONTEXT_t vosContext = pHddCtx->pvosContext;

    hdd_adapter_t *pAdapter = hdd_get_adapter(pHddCtx,WLAN_HDD_FTM);
    ENTER();
    if(pAdapter == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:pAdapter is NULL",__func__);
        return VOS_STATUS_E_NOMEM;
    }

    if (pHddCtx->ftm.IsCmdPending == TRUE)
    {
        if (vos_event_set(&pHddCtx->ftm.ftm_vos_event)!= VOS_STATUS_SUCCESS)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                      "%s: vos_event_set failed", __func__);
        }
    }

    if(WLAN_FTM_STARTED == pHddCtx->ftm.ftm_state)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: Ftm has been started. stopping ftm", __func__);
        wlan_ftm_stop(pHddCtx);
    }

    hdd_close_all_adapters( pHddCtx );

    vosStatus = vos_sched_close( vosContext );
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))       {
       VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: Failed to close VOSS Scheduler",__func__);
       VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
    }

    //Close VOSS
    wlan_ftm_vos_close(vosContext);


    vosStatus = vos_event_destroy(&pHddCtx->ftm.ftm_vos_event);
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to destroy ftm_vos Event",__func__);
        VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
    }

#if defined(QCA_WIFI_FTM) && defined(LINUX_QCMBR)
    spin_lock_bh(&qcmbr_queue_lock);
    if (!list_empty(&qcmbr_queue_head)) {
        qcmbr_queue_t *msg_buf, *tmp_buf;
        list_for_each_entry_safe(msg_buf, tmp_buf, &qcmbr_queue_head, list) {
            list_del(&msg_buf->list);
            kfree(msg_buf);
        }
    }
    spin_unlock_bh(&qcmbr_queue_lock);
#endif

    return 0;

}

/**---------------------------------------------------------------------------

  \brief wlan_hdd_ftm_start() -

   This function  starts the following modules.
   1) WDA Start.
   2) HTC Start.
   3) MAC Start to download the firmware.


  \param  - pAdapter - Pointer HDD Context.

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

static int wlan_hdd_ftm_start(hdd_context_t *pHddCtx)
{
    VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
    pVosContextType pVosContext = (pVosContextType)(pHddCtx->pvosContext);

    if (WLAN_FTM_STARTED == pHddCtx->ftm.ftm_state)
    {
       return VOS_STATUS_SUCCESS;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: Starting CLD SW", __func__);

    /* We support only one instance for now ...*/
    if (pVosContext == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
           "%s: mismatch in context",__func__);
        goto err_status_failure;
    }


    if (pVosContext->pMACContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: MAC NULL context",__func__);
        goto err_status_failure;
    }

    /* Vos preStart is calling */
    if ( !VOS_IS_STATUS_SUCCESS(vos_ftm_preStart(pHddCtx->pvosContext) ) )
    {
       hddLog(VOS_TRACE_LEVEL_FATAL,"%s: vos_preStart failed",__func__);
       goto err_status_failure;
    }



    vStatus = WDA_start(pVosContext);
    if (vStatus != VOS_STATUS_SUCCESS)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to start WDA",__func__);
       goto err_status_failure;
    }


    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: MAC correctly started",__func__);

    if (hdd_ftm_service_registration(pHddCtx)) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: failed", __func__);
       goto err_ftm_service_reg;
    }

    pHddCtx->ftm.ftm_state = WLAN_FTM_STARTED;

    return VOS_STATUS_SUCCESS;

err_ftm_service_reg:
    wlan_hdd_ftm_close(pHddCtx);


err_status_failure:

    return VOS_STATUS_E_FAILURE;

}

#if  defined(QCA_WIFI_FTM)
int hdd_ftm_start(hdd_context_t *pHddCtx)
{
    return wlan_hdd_ftm_start(pHddCtx);
}
#endif

static int wlan_ftm_stop(hdd_context_t *pHddCtx)
{
   VOS_STATUS vosStatus;

   if(pHddCtx->ftm.ftm_state != WLAN_FTM_STARTED)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:Ftm has not started. Please start the ftm. ",__func__);
       return VOS_STATUS_E_FAILURE;
   }

   {
       /*  STOP MAC only */
       v_VOID_t *hHal;
       hHal = vos_get_context( VOS_MODULE_ID_SME, pHddCtx->pvosContext );
       if (NULL == hHal) {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s: NULL hHal", __func__);
       } else {
           vosStatus = macStop(hHal, HAL_STOP_TYPE_SYS_DEEP_SLEEP );
           if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
               VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                          "%s: Failed to stop SYS", __func__);
               VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
           }
       }
       WDA_stop(pHddCtx->pvosContext, HAL_STOP_TYPE_RF_KILL);
    }
    return WLAN_FTM_SUCCESS;
}

#if  defined(QCA_WIFI_FTM)
int hdd_ftm_stop(hdd_context_t *pHddCtx)
{
    return wlan_ftm_stop(pHddCtx);
}
#endif

#if  defined(QCA_WIFI_FTM)
#if defined(LINUX_QCMBR)
static int wlan_hdd_qcmbr_command(hdd_adapter_t *pAdapter, qcmbr_data_t *pqcmbr_data)
{
    int ret = 0;
    qcmbr_queue_t *qcmbr_buf = NULL;

    switch (pqcmbr_data->cmd) {
        case ATH_XIOCTL_UNIFIED_UTF_CMD: {
            pqcmbr_data->copy_to_user = 0;
            if (pqcmbr_data->length) {
                if (wlan_hdd_ftm_testmode_cmd(pqcmbr_data->buf,
                                              pqcmbr_data->length)
                        != VOS_STATUS_SUCCESS) {
                    ret = -EBUSY;
                } else {
                    ret = 0;
                }
            }
        }
        break;

        case ATH_XIOCTL_UNIFIED_UTF_RSP: {
            pqcmbr_data->copy_to_user = 1;
            if (!list_empty(&qcmbr_queue_head)) {
                spin_lock_bh(&qcmbr_queue_lock);
                qcmbr_buf = list_first_entry(&qcmbr_queue_head,
                                             qcmbr_queue_t, list);
                list_del(&qcmbr_buf->list);
                spin_unlock_bh(&qcmbr_queue_lock);
                ret = 0;
            } else {
                ret = -1;
            }

            if (!ret) {
                memcpy(pqcmbr_data->buf, qcmbr_buf->utf_buf,
                       (MAX_UTF_LENGTH + 4));
                kfree(qcmbr_buf);
            } else {
                ret = -EAGAIN;
            }
        }
        break;
    }

    return ret;
}

#ifdef CONFIG_COMPAT
static int wlan_hdd_qcmbr_compat_ioctl(hdd_adapter_t *pAdapter,
                                       struct ifreq *ifr)
{
    qcmbr_data_t *qcmbr_data;
    int ret = 0;

    qcmbr_data = kzalloc(sizeof(qcmbr_data_t), GFP_KERNEL);
    if (qcmbr_data == NULL)
        return -ENOMEM;

    if (copy_from_user(qcmbr_data, ifr->ifr_data, sizeof(*qcmbr_data))) {
        ret = -EFAULT;
        goto exit;
    }

    ret = wlan_hdd_qcmbr_command(pAdapter, qcmbr_data);
    if (qcmbr_data->copy_to_user) {
        ret = copy_to_user(ifr->ifr_data, qcmbr_data->buf,
                           (MAX_UTF_LENGTH + 4));
    }

exit:
    kfree(qcmbr_data);
    return ret;
}
#else /* CONFIG_COMPAT */
static int wlan_hdd_qcmbr_compat_ioctl(hdd_adapter_t *pAdapter,
                                       struct ifreq *ifr)
{
   return 0;
}
#endif /* CONFIG_COMPAT */

static int wlan_hdd_qcmbr_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
    qcmbr_data_t *qcmbr_data;
    int ret = 0;

    qcmbr_data = kzalloc(sizeof(qcmbr_data_t), GFP_KERNEL);
    if (qcmbr_data == NULL)
        return -ENOMEM;

    if (copy_from_user(qcmbr_data, ifr->ifr_data, sizeof(*qcmbr_data))) {
        ret = -EFAULT;
        goto exit;
    }

    ret = wlan_hdd_qcmbr_command(pAdapter, qcmbr_data);
    if (qcmbr_data->copy_to_user) {
        ret = copy_to_user(ifr->ifr_data, qcmbr_data->buf,
                           (MAX_UTF_LENGTH + 4));
    }

exit:
    kfree(qcmbr_data);
    return ret;
}

int wlan_hdd_qcmbr_unified_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr)
{
    int ret = 0;

    if (is_compat_task()) {
        ret = wlan_hdd_qcmbr_compat_ioctl(pAdapter, ifr);
    } else {
        ret = wlan_hdd_qcmbr_ioctl(pAdapter, ifr);
    }

    return ret;
}

static void WLANQCMBR_McProcessMsg(v_VOID_t *message)
{
    qcmbr_queue_t *qcmbr_buf = NULL;
    u_int32_t data_len;

    data_len = *((u_int32_t *)message) + sizeof(u_int32_t);
    qcmbr_buf = kzalloc(sizeof(qcmbr_queue_t), GFP_KERNEL);
    if (qcmbr_buf != NULL) {
        memcpy(qcmbr_buf->utf_buf, message, data_len);
        spin_lock_bh(&qcmbr_queue_lock);
        list_add_tail(&(qcmbr_buf->list), &qcmbr_queue_head);
        spin_unlock_bh(&qcmbr_queue_lock);
    }
}
#endif /*LINUX_QCMBR*/

VOS_STATUS WLANFTM_McProcessMsg (v_VOID_t *message)
{
    void *data;
    u_int32_t data_len;

    if (!message)
        return VOS_STATUS_E_INVAL;

    data_len = *((u_int32_t *)message);
    data = (u_int32_t *)message + 1;

#if defined(LINUX_QCMBR)
    WLANQCMBR_McProcessMsg(message);
#else
#ifdef CONFIG_NL80211_TESTMODE
    wlan_hdd_testmode_rx_event(data, (size_t)data_len);
#endif
#endif

    vos_mem_free(message);

    return VOS_STATUS_SUCCESS;
}

VOS_STATUS wlan_hdd_ftm_testmode_cmd(void *data, int len)
{
    struct ar6k_testmode_cmd_data *cmd_data;

    cmd_data = (struct ar6k_testmode_cmd_data *)
               vos_mem_malloc(sizeof(*cmd_data));

    if (!cmd_data) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  ("Failed to allocate FTM command data"));
        return VOS_STATUS_E_NOMEM;
    }

    cmd_data->data = vos_mem_malloc(len);

    if (!cmd_data->data) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  ("Failed to allocate FTM command data buffer"));
        vos_mem_free(cmd_data);
        return VOS_STATUS_E_NOMEM;
    }

   cmd_data->len = len;
   vos_mem_copy(cmd_data->data, data, len);

   if (wlan_ftm_postmsg((v_U8_t *)cmd_data, sizeof(*cmd_data))) {
       vos_mem_free(cmd_data->data);
       vos_mem_free(cmd_data);
       return VOS_STATUS_E_FAILURE;
   }

   return VOS_STATUS_SUCCESS;
}
#endif /*QCA_WIFI_FTM*/
