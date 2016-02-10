/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/**=========================================================================

  \file  vos_api.c

  \brief Stub file for all virtual Operating System Services (vOSS) APIs

  ========================================================================*/
 /*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  03/29/09    kanand     Created module.
===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_mq.h>
#include "vos_sched.h"
#include <vos_api.h>
#include "sirTypes.h"
#include "sirApi.h"
#include "sirMacProtDef.h"
#include "sme_Api.h"
#include "macInitApi.h"
#include "wlan_qct_sys.h"
#include "wlan_qct_tl.h"
#include "wlan_hdd_misc.h"
#include "i_vos_packet.h"
#include "vos_nvitem.h"
#include "wlan_qct_wda.h"
#include "wlan_hdd_main.h"
#include <linux/vmalloc.h>
#include "wlan_hdd_cfg80211.h"
#include "vos_cnss.h"

#include "sapApi.h"
#include "vos_trace.h"



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
#include "vos_utils.h"
#include "wlan_logging_sock_svc.h"
#include "wma.h"

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ------------------------------------------------------------------------*/
/* Approximate amount of time to wait for WDA to stop WDI */
#define VOS_WDA_STOP_TIMEOUT WDA_STOP_TIMEOUT

/* Approximate amount of time to wait for WDA to issue a DUMP req */
#define VOS_WDA_RESP_TIMEOUT WDA_STOP_TIMEOUT

/* Maximum number of vos message queue get wrapper failures to cause panic */
#define VOS_WRAPPER_MAX_FAIL_COUNT (VOS_CORE_MAX_MESSAGES * 3)

/*---------------------------------------------------------------------------
 * Data definitions
 * ------------------------------------------------------------------------*/
static VosContextType  gVosContext;
static pVosContextType gpVosContext;

/* Debug variable to detect MC thread stuck */
static atomic_t vos_wrapper_empty_count;

static uint8_t vos_multicast_logging;
/*---------------------------------------------------------------------------
 * Forward declaration
 * ------------------------------------------------------------------------*/
v_VOID_t vos_sys_probe_thread_cback ( v_VOID_t *pUserData );

v_VOID_t vos_core_return_msg(v_PVOID_t pVContext, pVosMsgWrapper pMsgWrapper);

v_VOID_t vos_fetch_tl_cfg_parms ( WLANTL_ConfigInfoType *pTLConfig,
    hdd_config_t * pConfig );


/*---------------------------------------------------------------------------

  \brief vos_preOpen() - PreOpen the vOSS Module

  The \a vos_preOpen() function allocates the Vos Context, but do not
  initialize all the members. This overal initialization will happen
  at vos_Open().
  The reason why we need vos_preOpen() is to get a minimum context
  where to store BAL and SAL relative data, which happens before
  vos_Open() is called.

  \param  pVosContext: A pointer to where to store the VOS Context


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_Open()

---------------------------------------------------------------------------*/
VOS_STATUS vos_preOpen ( v_CONTEXT_t *pVosContext )
{
   if ( pVosContext == NULL)
      return VOS_STATUS_E_FAILURE;

   /* Allocate the VOS Context */
   *pVosContext = NULL;
   gpVosContext = &gVosContext;

   if (NULL == gpVosContext)
   {
     /* Critical Error ...Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                 "%s: Failed to allocate VOS Context", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_RESOURCES;
   }

   vos_mem_zero(gpVosContext, sizeof(VosContextType));

   *pVosContext = gpVosContext;

   /* Initialize the spinlock */
   vos_trace_spin_lock_init();
   /* it is the right time to initialize MTRACE structures */
   #if defined(TRACE_RECORD)
       vosTraceInit();
   #endif

   return VOS_STATUS_SUCCESS;

} /* vos_preOpen()*/


/*---------------------------------------------------------------------------

  \brief vos_preClose() - PreClose the vOSS Module

  The \a vos_preClose() function frees the Vos Context.

  \param  pVosContext: A pointer to where the VOS Context was stored


  \return VOS_STATUS_SUCCESS - Always successful


  \sa vos_preClose()
  \sa vos_close()
---------------------------------------------------------------------------*/
VOS_STATUS vos_preClose( v_CONTEXT_t *pVosContext )
{

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: De-allocating the VOS Context", __func__);

   if (( pVosContext == NULL) || (*pVosContext == NULL))
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: vOS Context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   if (gpVosContext != *pVosContext)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Context mismatch", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   *pVosContext = gpVosContext = NULL;

   return VOS_STATUS_SUCCESS;

} /* vos_preClose()*/

#ifdef FEATURE_RUNTIME_PM
static inline void vos_runtime_pm_config(struct ol_softc *scn,
		hdd_context_t *pHddCtx)
{
	scn->enable_runtime_pm = pHddCtx->cfg_ini->runtime_pm;
	scn->runtime_pm_delay = pHddCtx->cfg_ini->runtime_pm_delay;
}
#else
static inline void vos_runtime_pm_config(struct ol_softc *scn,
		hdd_context_t *pHddCtx) { }
#endif

#if defined (FEATURE_SECURE_FIRMWARE) && defined (FEATURE_FW_HASH_CHECK)
static inline void vos_fw_hash_check_config(struct ol_softc *scn,
					hdd_context_t *pHddCtx)
{
	scn->enable_fw_hash_check = pHddCtx->cfg_ini->enable_fw_hash_check;
}
#elif defined (FEATURE_SECURE_FIRMWARE)
static inline void vos_fw_hash_check_config(struct ol_softc *scn,
					hdd_context_t *pHddCtx)
{
	scn->enable_fw_hash_check = true;
}
#else
static inline void vos_fw_hash_check_config(struct ol_softc *scn,
					hdd_context_t *pHddCtx) { }
#endif

#ifdef WLAN_FEATURE_NAN
/**
 * vos_set_nan_enable() - set nan enable flag in mac open param
 * @wma_handle: Pointer to mac open param
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static void vos_set_nan_enable(tMacOpenParameters *param,
					hdd_context_t *hdd_ctx)
{
	param->is_nan_enabled = hdd_ctx->cfg_ini->enable_nan_support;
}
#else
static void vos_set_nan_enable(tMacOpenParameters *param,
					hdd_context_t *pHddCtx)
{
}
#endif

/*---------------------------------------------------------------------------

  \brief vos_open() - Open the vOSS Module

  The \a vos_open() function opens the vOSS Scheduler
  Upon successful initialization:

     - All VOS submodules should have been initialized

     - The VOS scheduler should have opened

     - All the WLAN SW components should have been opened. This includes
       SYS, MAC, SME, WDA and TL.


  \param  hddContextSize: Size of the HDD context to allocate.


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_preOpen()

---------------------------------------------------------------------------*/
VOS_STATUS vos_open( v_CONTEXT_t *pVosContext, v_SIZE_t hddContextSize )

{
   VOS_STATUS vStatus      = VOS_STATUS_SUCCESS;
   int iter                = 0;
   tSirRetStatus sirStatus = eSIR_SUCCESS;
   tMacOpenParameters macOpenParms;
   WLANTL_ConfigInfoType TLConfig;
   adf_os_device_t adf_ctx;
   HTC_INIT_INFO  htcInfo;
   struct ol_softc *scn;
   v_VOID_t *HTCHandle;
   hdd_context_t *pHddCtx;

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Opening VOSS", __func__);

   if (NULL == gpVosContext)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "%s: Trying to open VOSS without a PreOpen", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Initialize the timer module */
   vos_timer_module_init();

   /* Initialize bug reporting structure */
   vos_init_log_completion();

   /* Initialize the probe event */
   if (vos_event_init(&gpVosContext->ProbeEvent) != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "%s: Unable to init probeEvent", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }
   if (vos_event_init( &(gpVosContext->wdaCompleteEvent) ) != VOS_STATUS_SUCCESS )
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: Unable to init wdaCompleteEvent", __func__);
      VOS_ASSERT(0);

      goto err_probe_event;
   }

   /* Initialize the free message queue */
   vStatus = vos_mq_init(&gpVosContext->freeVosMq);
   if (! VOS_IS_STATUS_SUCCESS(vStatus))
   {

      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to initialize VOS free message queue", __func__);
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
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open VOS Scheduler", __func__);
      VOS_ASSERT(0);
      goto err_msg_queue;
   }

   pHddCtx = (hdd_context_t*)(gpVosContext->pHDDContext);
   if((NULL == pHddCtx) ||
      (NULL == pHddCtx->cfg_ini))
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Hdd Context is Null", __func__);
     VOS_ASSERT(0);
     goto err_sched_close;
   }

   scn = vos_get_context(VOS_MODULE_ID_HIF, gpVosContext);
   if (!scn) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: scn is null!", __func__);
      goto err_sched_close;
   }
   scn->enableuartprint = pHddCtx->cfg_ini->enablefwprint;
   scn->enablefwlog     = pHddCtx->cfg_ini->enablefwlog;
   scn->enableFwSelfRecovery = pHddCtx->cfg_ini->enableFwSelfRecovery;
   scn->max_no_of_peers = pHddCtx->cfg_ini->maxNumberOfPeers;
#ifdef WLAN_FEATURE_LPSS
   scn->enablelpasssupport = pHddCtx->cfg_ini->enablelpasssupport;
#endif
   scn->enableRamdumpCollection = pHddCtx->cfg_ini->is_ramdump_enabled;
   scn->enable_self_recovery = pHddCtx->cfg_ini->enableSelfRecovery;

   vos_fw_hash_check_config(scn, pHddCtx);
   vos_runtime_pm_config(scn, pHddCtx);

   /* Initialize BMI and Download firmware */
   if (bmi_download_firmware(scn)) {
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

   if (bmi_done(scn)) {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: Failed to complete BMI phase", __func__);
        goto err_htc_close;
   }

   /*
   ** Need to open WDA first because it calls WDI_Init, which calls wpalOpen
   ** The reason that is needed becasue vos_packet_open need to use PAL APIs
   */

   /*Open the WDA module */
   vos_mem_set(&macOpenParms, sizeof(macOpenParms), 0);
   /* UMA is supported in hardware for performing the
   ** frame translation 802.11 <-> 802.3
   */
   macOpenParms.frameTransRequired = 1;
   macOpenParms.driverType         = eDRIVER_TYPE_PRODUCTION;
   macOpenParms.powersaveOffloadEnabled =
      pHddCtx->cfg_ini->enablePowersaveOffload;
   macOpenParms.staDynamicDtim = pHddCtx->cfg_ini->enableDynamicDTIM;
   macOpenParms.staModDtim = pHddCtx->cfg_ini->enableModulatedDTIM;
   macOpenParms.staMaxLIModDtim = pHddCtx->cfg_ini->fMaxLIModulatedDTIM;
   macOpenParms.wowEnable          = pHddCtx->cfg_ini->wowEnable;
   macOpenParms.maxWoWFilters      = pHddCtx->cfg_ini->maxWoWFilters;
  /* Here olIniInfo is used to store ini status of arp offload
   * ns offload and others. Currently 1st bit is used for arp
   * off load and 2nd bit for ns offload currently, rest bits are unused
   */
  if ( pHddCtx->cfg_ini->fhostArpOffload)
       macOpenParms.olIniInfo      = macOpenParms.olIniInfo | 0x1;
  if ( pHddCtx->cfg_ini->fhostNSOffload)
       macOpenParms.olIniInfo      = macOpenParms.olIniInfo | 0x2;
  /*
   * Copy the DFS Phyerr Filtering Offload status.
   * This parameter reflects the value of the
   * dfsPhyerrFilterOffload flag  as set in the ini.
   */
  macOpenParms.dfsPhyerrFilterOffload =
                        pHddCtx->cfg_ini->fDfsPhyerrFilterOffload;

  macOpenParms.ssdp = pHddCtx->cfg_ini->ssdp;
  macOpenParms.enable_bcst_ptrn = pHddCtx->cfg_ini->bcastptrn;

#ifdef FEATURE_WLAN_RA_FILTERING
   macOpenParms.RArateLimitInterval = pHddCtx->cfg_ini->RArateLimitInterval;
   macOpenParms.IsRArateLimitEnabled = pHddCtx->cfg_ini->IsRArateLimitEnabled;
#endif

   macOpenParms.force_target_assert_enabled =
               pHddCtx->cfg_ini->crash_inject_enabled;
   macOpenParms.apMaxOffloadPeers = pHddCtx->cfg_ini->apMaxOffloadPeers;

   macOpenParms.apMaxOffloadReorderBuffs =
                        pHddCtx->cfg_ini->apMaxOffloadReorderBuffs;

   macOpenParms.apDisableIntraBssFwd = pHddCtx->cfg_ini->apDisableIntraBssFwd;

   macOpenParms.dfsRadarPriMultiplier = pHddCtx->cfg_ini->dfsRadarPriMultiplier;
   macOpenParms.reorderOffload = pHddCtx->cfg_ini->reorderOffloadSupport;

#ifdef IPA_UC_OFFLOAD
    /* IPA micro controller data path offload resource config item */
    macOpenParms.ucOffloadEnabled = pHddCtx->cfg_ini->IpaUcOffloadEnabled;
    macOpenParms.ucTxBufCount = pHddCtx->cfg_ini->IpaUcTxBufCount;
    macOpenParms.ucTxBufSize = pHddCtx->cfg_ini->IpaUcTxBufSize;
    macOpenParms.ucRxIndRingCount = pHddCtx->cfg_ini->IpaUcRxIndRingCount;
    macOpenParms.ucTxPartitionBase = pHddCtx->cfg_ini->IpaUcTxPartitionBase;
#endif /* IPA_UC_OFFLOAD */

    macOpenParms.tx_chain_mask_cck = pHddCtx->cfg_ini->tx_chain_mask_cck;
    macOpenParms.self_gen_frm_pwr = pHddCtx->cfg_ini->self_gen_frm_pwr;
    macOpenParms.max_mgmt_tx_fail_count =
                     pHddCtx->cfg_ini->max_mgmt_tx_fail_count;

#ifdef WLAN_FEATURE_LPSS
    macOpenParms.is_lpass_enabled = pHddCtx->cfg_ini->enablelpasssupport;
#endif

   vos_set_nan_enable(&macOpenParms, pHddCtx);

   vStatus = WDA_open( gpVosContext, gpVosContext->pHDDContext,
                       hdd_update_tgt_cfg,
                       hdd_dfs_indicate_radar,
                       &macOpenParms );

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open WDA module", __func__);
      VOS_ASSERT(0);
      goto err_htc_close;
   }

   /* Number of peers limit differs in each chip version. If peer max
    * limit configured in ini exceeds more than supported, WMA adjusts
    * and keeps correct limit in macOpenParms.maxStation. So, make sure
    * ini entry pHddCtx->cfg_ini->maxNumberOfPeers has adjusted value
   */
   pHddCtx->cfg_ini->maxNumberOfPeers = macOpenParms.maxStation;
   HTCHandle = vos_get_context(VOS_MODULE_ID_HTC, gpVosContext);
   if (!HTCHandle) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: HTCHandle is null!", __func__);
      goto err_wda_close;
   }
   if (HTCWaitTarget(HTCHandle)) {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to complete BMI phase", __func__);
           goto err_wda_close;
   }

   bmi_target_ready(scn, gpVosContext->cfg_ctx);
   /* Open the SYS module */
   vStatus = sysOpen(gpVosContext);

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open SYS module", __func__);
      VOS_ASSERT(0);
      goto err_packet_close;
   }


   /* If we arrive here, both threads dispacthing messages correctly */

   /* Now proceed to open the MAC */

   /* UMA is supported in hardware for performing the
      frame translation 802.11 <-> 802.3 */
   macOpenParms.frameTransRequired = 1;

   sirStatus = macOpen(&(gpVosContext->pMACContext), gpVosContext->pHDDContext,
                         &macOpenParms);

   if (eSIR_SUCCESS != sirStatus)
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to open MAC", __func__);
     VOS_ASSERT(0);
     goto err_nv_close;
   }

   /* Now proceed to open the SME */
   vStatus = sme_Open(gpVosContext->pMACContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to open SME", __func__);
     VOS_ASSERT(0);
     goto err_mac_close;
   }

   /* Now proceed to open TL. Read TL config first */
   vos_fetch_tl_cfg_parms ( &TLConfig,
       ((hdd_context_t*)(gpVosContext->pHDDContext))->cfg_ini);

   vStatus = WLANTL_Open(gpVosContext, &TLConfig);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     /* Critical Error ...  Cannot proceed further */
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to open TL", __func__);
     VOS_ASSERT(0);
     goto err_sme_close;
   }

#ifdef IPA_UC_OFFLOAD
   WLANTL_GetIpaUcResource(gpVosContext,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->ce_sr_base_paddr,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->ce_sr_ring_size,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->ce_reg_paddr,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->tx_comp_ring_base_paddr,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->tx_comp_ring_size,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->tx_num_alloc_buffer,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->rx_rdy_ring_base_paddr,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->rx_rdy_ring_size,
       &((hdd_context_t*)(gpVosContext->pHDDContext))->rx_proc_done_idx_paddr);
#endif /* IPA_UC_OFFLOAD */

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS successfully Opened", __func__);

   *pVosContext = gpVosContext;

   return VOS_STATUS_SUCCESS;


err_sme_close:
   sme_Close(gpVosContext->pMACContext);

err_mac_close:
   macClose(gpVosContext->pMACContext);

err_nv_close:

   sysClose(gpVosContext);

err_packet_close:
err_wda_close:
   WDA_close(gpVosContext);

   wma_wmi_service_close(gpVosContext);

err_htc_close:
   if (gpVosContext->htc_ctx) {
      HTCDestroy(gpVosContext->htc_ctx);
      gpVosContext->htc_ctx = NULL;
   }

err_bmi_close:
      BMICleanup(scn);

err_sched_close:
   vos_sched_close(gpVosContext);


err_msg_queue:
   vos_mq_deinit(&gpVosContext->freeVosMq);

err_wda_complete_event:
   vos_event_destroy( &gpVosContext->wdaCompleteEvent );

err_probe_event:
   vos_event_destroy(&gpVosContext->ProbeEvent);

   return VOS_STATUS_E_FAILURE;

} /* vos_open() */

/*---------------------------------------------------------------------------

  \brief vos_preStart() -

  The \a vos_preStart() function to download CFG.
  including:
      - ccmStart

      - WDA: triggers the CFG download


  \param  pVosContext: The VOS context


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_start

---------------------------------------------------------------------------*/
VOS_STATUS vos_preStart( v_CONTEXT_t vosContext )
{
   VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
   pVosContextType pVosContext = (pVosContextType)vosContext;
   v_VOID_t *scn;
   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO,
             "vos prestart");

   if (gpVosContext != pVosContext)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Context mismatch", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }

   if (pVosContext->pMACContext == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: MAC NULL context", __func__);
       VOS_ASSERT(0);
       return VOS_STATUS_E_INVAL;
   }

   if (pVosContext->pWDAContext == NULL)
   {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: WDA NULL context", __func__);
       VOS_ASSERT(0);
       return VOS_STATUS_E_INVAL;
   }

   scn = vos_get_context(VOS_MODULE_ID_HIF, gpVosContext);
   if (!scn) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: scn is null!", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   /* call macPreStart */
   vStatus = macPreStart(gpVosContext->pMACContext);
   if ( !VOS_IS_STATUS_SUCCESS(vStatus) )
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_FATAL,
             "Failed at macPreStart ");
      return VOS_STATUS_E_FAILURE;
   }

   /* call ccmStart */
   ccmStart(gpVosContext->pMACContext);

   /* Reset wda wait event */
   vos_event_reset(&gpVosContext->wdaCompleteEvent);


   /*call WDA pre start*/
   vStatus = WDA_preStart(gpVosContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_FATAL,
             "Failed to WDA prestart");
      ccmStop(gpVosContext->pMACContext);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   /* Need to update time out of complete */
   vStatus = vos_wait_single_event( &gpVosContext->wdaCompleteEvent,
                                    VOS_WDA_TIMEOUT );
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
           "%s: WDA_preStart reporting other error", __func__);
      }
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: Test MC thread by posting a probe message to SYS", __func__);
      wlan_sys_probe();

      ccmStop(gpVosContext->pMACContext);
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }

   vStatus = HTCStart(gpVosContext->htc_ctx);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_FATAL,
               "Failed to Start HTC");
      ccmStop(gpVosContext->pMACContext);
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }
   vStatus = wma_wait_for_ready_event(gpVosContext->pWDAContext);
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_FATAL,
               "Failed to get ready event from target firmware");
      HTCSetTargetToSleep(scn);
      ccmStop(gpVosContext->pMACContext);
      HTCStop(gpVosContext->htc_ctx);
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }

   HTCSetTargetToSleep(scn);

   return VOS_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------

  \brief vos_start() - Start the Libra SW Modules

  The \a vos_start() function starts all the components of the Libra SW
  including:
      - SAL/BAL, which in turn starts SSC

      - the MAC (HAL and PE)

      - SME

      - TL

      - SYS: triggers the CFG download


  \param  pVosContext: The VOS context


  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the scheduler


          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/

  \sa vos_preStart()
  \sa vos_open()

---------------------------------------------------------------------------*/
VOS_STATUS vos_start( v_CONTEXT_t vosContext )
{
  VOS_STATUS vStatus          = VOS_STATUS_SUCCESS;
  tSirRetStatus sirStatus     = eSIR_SUCCESS;
  pVosContextType pVosContext = (pVosContextType)vosContext;
  tHalMacStartParameters halStartParams;

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: Starting Libra SW", __func__);

  /* We support only one instance for now ...*/
  if (gpVosContext != pVosContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: mismatch in context", __func__);
     return VOS_STATUS_E_FAILURE;
  }

  if (( pVosContext->pWDAContext == NULL) || ( pVosContext->pMACContext == NULL)
     || ( pVosContext->pTLContext == NULL))
  {
     if (pVosContext->pWDAContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: WDA NULL context", __func__);
     else if (pVosContext->pMACContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: MAC NULL context", __func__);
     else
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: TL NULL context", __func__);

     return VOS_STATUS_E_FAILURE;
  }


  /* Start the WDA */
  vStatus = WDA_start(pVosContext);
  if ( vStatus != VOS_STATUS_SUCCESS )
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to start WDA", __func__);
     return VOS_STATUS_E_FAILURE;
  }
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: WDA correctly started", __func__);

  /* Start the MAC */
  vos_mem_zero((v_PVOID_t)&halStartParams, sizeof(tHalMacStartParameters));

  /* Start the MAC */
  sirStatus = macStart(pVosContext->pMACContext,(v_PVOID_t)&halStartParams);

  if (eSIR_SUCCESS != sirStatus)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
              "%s: Failed to start MAC", __func__);
    goto err_wda_stop;
  }

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: MAC correctly started", __func__);

  /* START SME */
  vStatus = sme_Start(pVosContext->pMACContext);

  if (!VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to start SME", __func__);
    goto err_mac_stop;
  }

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: SME correctly started", __func__);

  /** START TL */
  vStatus = WLANTL_Start(pVosContext);
  if (!VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to start TL", __func__);
    goto err_sme_stop;
  }

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "TL correctly started");
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: VOSS Start is successful!!", __func__);

  return VOS_STATUS_SUCCESS;


err_sme_stop:
  sme_Stop(pVosContext->pMACContext, HAL_STOP_TYPE_SYS_RESET);

err_mac_stop:
  macStop( pVosContext->pMACContext, HAL_STOP_TYPE_SYS_RESET );

err_wda_stop:
  vos_event_reset( &(gpVosContext->wdaCompleteEvent) );
  vStatus = WDA_stop( pVosContext, HAL_STOP_TYPE_RF_KILL);
  if (!VOS_IS_STATUS_SUCCESS(vStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to stop WDA", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vStatus ) );
     WDA_setNeedShutdown(vosContext);
  }
  else
  {
    vStatus = vos_wait_single_event( &(gpVosContext->wdaCompleteEvent),
                                     VOS_WDA_TIMEOUT );
    if( vStatus != VOS_STATUS_SUCCESS )
    {
       if( vStatus == VOS_STATUS_E_TIMEOUT )
       {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Timeout occurred before WDA_stop complete", __func__);

       }
       else
       {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: WDA_stop reporting other error", __func__);
       }
       VOS_ASSERT( 0 );
       WDA_setNeedShutdown(vosContext);
    }
  }

  return VOS_STATUS_E_FAILURE;

} /* vos_start() */


/* vos_stop function */
VOS_STATUS vos_stop( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;

  /* WDA_Stop is called before the SYS so that the processing of Riva
  pending responces will not be handled during uninitialization of WLAN driver */
  vos_event_reset( &(gpVosContext->wdaCompleteEvent) );

  vosStatus = WDA_stop( vosContext, HAL_STOP_TYPE_RF_KILL );

  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to stop WDA", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
     WDA_setNeedShutdown(vosContext);
  }

  hif_disable_isr(((VosContextType*)vosContext)->pHIFContext);
  hif_reset_soc(((VosContextType*)vosContext)->pHIFContext);

  /* SYS STOP will stop SME and MAC */
  vosStatus = sysStop( vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to stop SYS", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = WLANTL_Stop( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to stop TL", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  return VOS_STATUS_SUCCESS;
}


/* vos_close function */
VOS_STATUS vos_close( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;

  vosStatus = wma_wmi_work_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close wma_wmi_work", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  if (gpVosContext->htc_ctx)
  {
      HTCStop(gpVosContext->htc_ctx);
      HTCDestroy(gpVosContext->htc_ctx);
      gpVosContext->htc_ctx = NULL;
  }

  vosStatus = WLANTL_Close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close TL", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = sme_Close( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SME", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = macClose( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close MAC", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  ((pVosContextType)vosContext)->pMACContext = NULL;

  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SYS", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  if ( TRUE == WDA_needShutdown(vosContext ))
  {
     /* if WDA stop failed, call WDA shutdown to cleanup WDA/WDI */
     vosStatus = WDA_shutdown( vosContext, VOS_TRUE );
     if (VOS_IS_STATUS_SUCCESS( vosStatus ) )
     {
        hdd_set_ssr_required( HDD_SSR_REQUIRED );
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                               "%s: Failed to shutdown WDA", __func__ );
        VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
     }
  }
  else
  {
     vosStatus = WDA_close( vosContext );
     if (!VOS_IS_STATUS_SUCCESS(vosStatus))
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to close WDA", __func__);
        VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
     }
  }

  vosStatus = wma_wmi_service_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close wma_wmi_service", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }


  vos_mq_deinit(&((pVosContextType)vosContext)->freeVosMq);

  vosStatus = vos_event_destroy(&gpVosContext->wdaCompleteEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy wdaCompleteEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_event_destroy(&gpVosContext->ProbeEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy ProbeEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vos_deinit_log_completion();

  return VOS_STATUS_SUCCESS;
}


/**---------------------------------------------------------------------------

  \brief vos_get_context() - get context data area

  Each module in the system has a context / data area that is allocated
  and maanged by voss.  This API allows any user to get a pointer to its
  allocated context data area from the VOSS global context.

  \param vosContext - the VOSS Global Context.

  \param moduleId - the module ID, who's context data are is being retrived.

  \return - pointer to the context data area.

          - NULL if the context data is not allocated for the module ID
            specified

  --------------------------------------------------------------------------*/
v_VOID_t* vos_get_context( VOS_MODULE_ID moduleId,
                           v_CONTEXT_t pVosContext )
{
  v_PVOID_t pModContext = NULL;

  if (pVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: vos context pointer is null", __func__);
    return NULL;
  }

  if (gpVosContext != pVosContext)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: pVosContext != gpVosContext", __func__);
    return NULL;
  }

  switch(moduleId)
  {
    case VOS_MODULE_ID_TL:
    {
      pModContext = gpVosContext->pTLContext;
      break;
    }


#ifndef WLAN_FEATURE_MBSSID
    case VOS_MODULE_ID_SAP:
    {
      pModContext = gpVosContext->pSAPContext;
      break;
    }
#endif

    case VOS_MODULE_ID_HDD:
    {
      pModContext = gpVosContext->pHDDContext;
      break;
    }

    case VOS_MODULE_ID_SME:
    case VOS_MODULE_ID_PE:
    case VOS_MODULE_ID_PMC:
    {
      /*
      ** In all these cases, we just return the MAC Context
      */
      pModContext = gpVosContext->pMACContext;
      break;
    }

    case VOS_MODULE_ID_WDA:
    {
      /* For WDA module */
      pModContext = gpVosContext->pWDAContext;
      break;
    }

    case VOS_MODULE_ID_VOSS:
    {
      /* For SYS this is VOS itself*/
      pModContext = gpVosContext;
      break;
    }


    case VOS_MODULE_ID_HIF:
    {
        pModContext = gpVosContext->pHIFContext;
        break;
    }

    case VOS_MODULE_ID_HTC:
    {
        pModContext = gpVosContext->htc_ctx;
        break;
    }

    case VOS_MODULE_ID_ADF:
    {
        pModContext = gpVosContext->adf_ctx;
        break;
    }

    case VOS_MODULE_ID_TXRX:
    {
        pModContext = gpVosContext->pdev_txrx_ctx;
        break;
    }

    case VOS_MODULE_ID_CFG:
    {
       pModContext = gpVosContext->cfg_ctx;
        break;
    }

    default:
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,"%s: Module ID %i "
          "does not have its context maintained by VOSS", __func__, moduleId);
      VOS_ASSERT(0);
      return NULL;
    }
  }

  if (pModContext == NULL )
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,"%s: Module ID %i "
          "context is Null", __func__, moduleId);
  }

  return pModContext;

} /* vos_get_context()*/


/**---------------------------------------------------------------------------

  \brief vos_get_global_context() - get VOSS global Context

  This API allows any user to get the VOS Global Context pointer from a
  module context data area.

  \param moduleContext - the input module context pointer

  \param moduleId - the module ID who's context pointer is input in
         moduleContext.

  \return - pointer to the VOSS global context

          - NULL if the function is unable to retreive the VOSS context.

  --------------------------------------------------------------------------*/
v_CONTEXT_t vos_get_global_context( VOS_MODULE_ID moduleId,
                                    v_VOID_t *moduleContext )
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
  }

  return gpVosContext;

} /* vos_get_global_context() */


v_U8_t vos_is_logp_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
    return 1;
  }

   return gpVosContext->isLogpInProgress;
}

void vos_set_logp_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
  hdd_context_t *pHddCtx = NULL;

  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
    return;
  }
  gpVosContext->isLogpInProgress = value;

  /* HDD uses it's own context variable to check if SSR in progress,
   * instead of modifying all HDD APIs set the HDD context variable
   * here */
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, gpVosContext);
   if (!pHddCtx) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: HDD context is Null", __func__);
      return;
   }
   pHddCtx->isLogpInProgress = value;
}

v_U8_t vos_is_load_unload_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
    return 0;
  }

   return gpVosContext->isLoadUnloadInProgress;
}

void vos_set_load_unload_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
    if (gpVosContext == NULL)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: global voss context is NULL", __func__);
        return;
    }
    gpVosContext->isLoadUnloadInProgress = value;

    vos_set_driver_status(value);
}

/**
 * vos_is_unload_in_progress - check if driver unload is in progress
 *
 * Return: true - unload in progress
 *         false - unload not in progress
 */
v_U8_t vos_is_unload_in_progress(void)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: global voss context is NULL", __func__);
		return 0;
	}

	return gpVosContext->is_unload_in_progress;
}

/**
 * vos_is_load_in_progress - check if driver load is in progress
 *
 * @moduleId: the module ID who's context pointer is input in moduleContext
 * @moduleContext: the input module context pointer
 *
 * Return: true - load in progress
 *         false - load not in progress
 */
v_U8_t vos_is_load_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: global voss context is NULL", __func__);
		return 0;
	}

	return gpVosContext->is_load_in_progress;
}

/**
 * vos_set_unload_in_progress - set driver unload in progress status
 * @value: true - driver unload starts
 *         false - driver unload completes
 *
 * Return: none
 */
void vos_set_unload_in_progress(v_U8_t value)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: global voss context is NULL", __func__);
		return;
	}

	gpVosContext->is_unload_in_progress = value;
}

/**
 * vos_set_load_in_progress - set driver load in progress status
 *
 * @moduleId: the module ID of the caller
 * @value: true - driver load starts
 *         false - driver load completes
 * Return: none
 */
void vos_set_load_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: global voss context is NULL", __func__);
		return;
	}

	gpVosContext->is_load_in_progress = value;
}

v_U8_t vos_is_reinit_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
    return 1;
  }

   return gpVosContext->isReInitInProgress;
}

void vos_set_reinit_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: global voss context is NULL", __func__);
    return;
  }

   gpVosContext->isReInitInProgress = value;
}


/**---------------------------------------------------------------------------

  \brief vos_alloc_context() - allocate a context within the VOSS global Context

  This API allows any user to allocate a user context area within the
  VOS Global Context.

  \param pVosContext - pointer to the global Vos context

  \param moduleId - the module ID who's context area is being allocated.

  \param ppModuleContext - pointer to location where the pointer to the
                           allocated context is returned.  Note this
                           output pointer is valid only if the API
                           returns VOS_STATUS_SUCCESS

  \param size - the size of the context area to be allocated.

  \return - VOS_STATUS_SUCCESS - the context for the module ID has been
            allocated successfully.  The pointer to the context area
            can be found in *ppModuleContext.
            \note This function returns VOS_STATUS_SUCCESS if the
            module context was already allocated and the size
            allocated matches the size on this call.

            VOS_STATUS_E_INVAL - the moduleId is not a valid or does
            not identify a module that can have a context allocated.

            VOS_STATUS_E_EXISTS - vos could allocate the requested context
            because a context for this module ID already exists and it is
            a *different* size that specified on this call.

            VOS_STATUS_E_NOMEM - vos could not allocate memory for the
            requested context area.

  \sa vos_get_context(), vos_free_context()

  --------------------------------------------------------------------------*/
VOS_STATUS vos_alloc_context( v_VOID_t *pVosContext, VOS_MODULE_ID moduleID,
                              v_VOID_t **ppModuleContext, v_SIZE_t size )
{
  v_VOID_t ** pGpModContext = NULL;

  if ( pVosContext == NULL) {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: vos context is null", __func__);
    return VOS_STATUS_E_FAILURE;
  }

  if (( gpVosContext != pVosContext) || ( ppModuleContext == NULL)) {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: context mismatch or null param passed", __func__);
    return VOS_STATUS_E_FAILURE;
  }

  switch(moduleID)
  {
    case VOS_MODULE_ID_TL:
    {
      pGpModContext = &(gpVosContext->pTLContext);
      break;
    }


#ifndef WLAN_FEATURE_MBSSID
    case VOS_MODULE_ID_SAP:
    {
      pGpModContext = &(gpVosContext->pSAPContext);
      break;
    }
#endif

    case VOS_MODULE_ID_WDA:
    {
      pGpModContext = &(gpVosContext->pWDAContext);
      break;
    }
    case VOS_MODULE_ID_SME:
    case VOS_MODULE_ID_PE:
    case VOS_MODULE_ID_PMC:
    case VOS_MODULE_ID_HDD:
    case VOS_MODULE_ID_HDD_SOFTAP:
    default:
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Module ID %i "
          "does not have its context allocated by VOSS", __func__, moduleID);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
    }
  }

  if ( NULL != *pGpModContext)
  {
    /*
    ** Context has already been allocated!
    ** Prevent double allocation
    */
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Module ID %i context has already been allocated",
                __func__, moduleID);
    return VOS_STATUS_E_EXISTS;
  }

  /*
  ** Dynamically allocate the context for module
  */

  *ppModuleContext = kmalloc(size, GFP_KERNEL);


  if ( *ppModuleContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,"%s: Failed to "
        "allocate Context for module ID %i", __func__, moduleID);
    VOS_ASSERT(0);
    return VOS_STATUS_E_NOMEM;
  }

  if (moduleID==VOS_MODULE_ID_TL)
  {
     vos_mem_zero(*ppModuleContext, size);
  }

  *pGpModContext = *ppModuleContext;

  return VOS_STATUS_SUCCESS;

} /* vos_alloc_context() */


/**---------------------------------------------------------------------------

  \brief vos_free_context() - free an allocated a context within the
                               VOSS global Context

  This API allows a user to free the user context area within the
  VOS Global Context.

  \param pVosContext - pointer to the global Vos context

  \param moduleId - the module ID who's context area is being free

  \param pModuleContext - pointer to module context area to be free'd.

  \return - VOS_STATUS_SUCCESS - the context for the module ID has been
            free'd.  The pointer to the context area is not longer
            available.

            VOS_STATUS_E_FAULT - pVosContext or pModuleContext are not
            valid pointers.

            VOS_STATUS_E_INVAL - the moduleId is not a valid or does
            not identify a module that can have a context free'd.

            VOS_STATUS_E_EXISTS - vos could not free the requested
            context area because a context for this module ID does not
            exist in the global vos context.

  \sa vos_get_context()

  --------------------------------------------------------------------------*/
VOS_STATUS vos_free_context( v_VOID_t *pVosContext, VOS_MODULE_ID moduleID,
                             v_VOID_t *pModuleContext )
{
  v_VOID_t ** pGpModContext = NULL;

  if (( pVosContext == NULL) || ( gpVosContext != pVosContext) ||
      ( pModuleContext == NULL))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Null params or context mismatch", __func__);
    return VOS_STATUS_E_FAILURE;
  }


  switch(moduleID)
  {
    case VOS_MODULE_ID_TL:
    {
      pGpModContext = &(gpVosContext->pTLContext);
      break;
    }


#ifndef WLAN_FEATURE_MBSSID
    case VOS_MODULE_ID_SAP:
    {
      pGpModContext = &(gpVosContext->pSAPContext);
      break;
    }
#endif

    case VOS_MODULE_ID_WDA:
    {
      pGpModContext = &(gpVosContext->pWDAContext);
      break;
    }
    case VOS_MODULE_ID_HDD:
    case VOS_MODULE_ID_SME:
    case VOS_MODULE_ID_PE:
    case VOS_MODULE_ID_PMC:
    case VOS_MODULE_ID_HDD_SOFTAP:
    default:
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s: Module ID %i "
          "does not have its context allocated by VOSS", __func__, moduleID);
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
    }
  }

  if ( NULL == *pGpModContext)
  {
    /*
    ** Context has not been allocated or freed already!
    */
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,"%s: Module ID %i "
        "context has not been allocated or freed already", __func__,moduleID);
    return VOS_STATUS_E_FAILURE;
  }

  if (*pGpModContext != pModuleContext)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: pGpModContext != pModuleContext", __func__);
    return VOS_STATUS_E_FAILURE;
  }

  if(pModuleContext != NULL)
      kfree(pModuleContext);

  *pGpModContext = NULL;

  return VOS_STATUS_SUCCESS;

} /* vos_free_context() */


/**---------------------------------------------------------------------------

  \brief vos_mq_post_message() - post a message to a message queue

  This API allows messages to be posted to a specific message queue.  Messages
  can be posted to the following message queues:

  <ul>
    <li> SME
    <li> PE
    <li> HAL
    <li> TL
  </ul>

  \param msgQueueId - identifies the message queue upon which the message
         will be posted.

  \param message - a pointer to a message buffer.  Memory for this message
         buffer is allocated by the caller and free'd by the vOSS after the
         message is posted to the message queue.  If the consumer of the
         message needs anything in this message, it needs to copy the contents
         before returning from the message queue handler.

  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.

          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not
          refer to a valid Message Queue Id.

          VOS_STATUS_E_FAULT  - message is an invalid pointer.

          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa

  --------------------------------------------------------------------------*/
VOS_STATUS vos_mq_post_message( VOS_MQ_ID msgQueueId, vos_msg_t *pMsg )
{
  pVosMqType      pTargetMq   = NULL;
  pVosMsgWrapper  pMsgWrapper = NULL;
  uint32_t debug_count;

  if ((gpVosContext == NULL) || (pMsg == NULL))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Null params or global vos context is null", __func__);
    VOS_ASSERT(0);
    return VOS_STATUS_E_FAILURE;
  }

  switch (msgQueueId)
  {
    /// Message Queue ID for messages bound for SME
    case  VOS_MQ_ID_SME:
    {
       pTargetMq = &(gpVosContext->vosSched.smeMcMq);
       break;
    }

    /// Message Queue ID for messages bound for PE
    case VOS_MQ_ID_PE:
    {
       pTargetMq = &(gpVosContext->vosSched.peMcMq);
       break;
    }

    /// Message Queue ID for messages bound for WDA
    case VOS_MQ_ID_WDA:
    {
       pTargetMq = &(gpVosContext->vosSched.wdaMcMq);
       break;
    }

    /// Message Queue ID for messages bound for TL
    case VOS_MQ_ID_TL:
    {
       pTargetMq = &(gpVosContext->vosSched.tlMcMq);
       break;
    }

    /// Message Queue ID for messages bound for the SYS module
    case VOS_MQ_ID_SYS:
    {
       pTargetMq = &(gpVosContext->vosSched.sysMcMq);
       break;
    }

    default:

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              ("%s: Trying to queue msg into unknown MC Msg queue ID %d"),
              __func__, msgQueueId);

    return VOS_STATUS_E_FAILURE;
  }

  VOS_ASSERT(NULL !=pTargetMq);
  if (pTargetMq == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pTargetMq == NULL", __func__);
     return VOS_STATUS_E_FAILURE;
  }

  /*
  ** Try and get a free Msg wrapper
  */
  pMsgWrapper = vos_mq_get(&gpVosContext->freeVosMq);

  if (NULL == pMsgWrapper) {
      debug_count = atomic_inc_return(&vos_wrapper_empty_count);
      if (1 == debug_count)
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: VOS Core run out of message wrapper %d",
              __func__, debug_count);

      if (VOS_WRAPPER_MAX_FAIL_COUNT == debug_count) {
          VOS_BUG(0);
      }

    return VOS_STATUS_E_RESOURCES;
  }

  atomic_set(&vos_wrapper_empty_count, 0);

  /*
  ** Copy the message now
  */
  vos_mem_copy( (v_VOID_t*)pMsgWrapper->pVosMsg,
                (v_VOID_t*)pMsg, sizeof(vos_msg_t));

  vos_mq_put(pTargetMq, pMsgWrapper);

  set_bit(MC_POST_EVENT_MASK, &gpVosContext->vosSched.mcEventFlag);
  wake_up_interruptible(&gpVosContext->vosSched.mcWaitQueue);

  return VOS_STATUS_SUCCESS;

} /* vos_mq_post_message()*/

v_VOID_t
vos_sys_probe_thread_cback
(
  v_VOID_t *pUserData
)
{
  if (gpVosContext != pUserData)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosContext != pUserData", __func__);
     return;
  }

  if (vos_event_set(&gpVosContext->ProbeEvent)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: vos_event_set failed", __func__);
     return;
  }

} /* vos_sys_probe_thread_cback() */

v_VOID_t vos_WDAComplete_cback
(
  v_VOID_t *pUserData
)
{

  if (gpVosContext != pUserData)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosContext != pUserData", __func__);
     return;
  }

  if (vos_event_set(&gpVosContext->wdaCompleteEvent)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: vos_event_set failed", __func__);
     return;
  }

} /* vos_WDAComplete_cback() */

v_VOID_t vos_core_return_msg
(
  v_PVOID_t      pVContext,
  pVosMsgWrapper pMsgWrapper
)
{
  pVosContextType pVosContext = (pVosContextType) pVContext;

  VOS_ASSERT( gpVosContext == pVosContext);

  if (gpVosContext != pVosContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosContext != pVosContext", __func__);
     return;
  }

  VOS_ASSERT( NULL !=pMsgWrapper );

  if (pMsgWrapper == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pMsgWrapper == NULL in function", __func__);
     return;
  }

  /*
  ** Return the message on the free message queue
  */
  INIT_LIST_HEAD(&pMsgWrapper->msgNode);
  vos_mq_put(&pVosContext->freeVosMq, pMsgWrapper);

} /* vos_core_return_msg() */


/**
  @brief vos_fetch_tl_cfg_parms() - this function will attempt to read the
  TL config params from the registry

  @param pAdapter : [inout] pointer to TL config block

  @return
  None

*/
v_VOID_t
vos_fetch_tl_cfg_parms
(
  WLANTL_ConfigInfoType *pTLConfig,
  hdd_config_t * pConfig
)
{
  if (pTLConfig == NULL)
  {
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s NULL ptr passed in!", __func__);
   return;
  }

  pTLConfig->uDelayedTriggerFrmInt = pConfig->DelayedTriggerFrmInt;
  pTLConfig->uMinFramesProcThres = pConfig->MinFramesProcThres;
  pTLConfig->ip_checksum_offload = pConfig->enableIPChecksumOffload;
  pTLConfig->enable_rxthread =
    (WLAN_HDD_RX_HANDLE_RX_THREAD == pConfig->rxhandle) ? 1 : 0;
}

/*---------------------------------------------------------------------------

  \brief vos_shutdown() - shutdown VOS

     - All VOS submodules are closed.

     - All the WLAN SW components should have been opened. This includes
       SYS, MAC, SME and TL.


  \param  vosContext: Global vos context


  \return VOS_STATUS_SUCCESS - Operation successfull & vos is shutdown

          VOS_STATUS_E_FAILURE - Failure to close

---------------------------------------------------------------------------*/
VOS_STATUS vos_shutdown(v_CONTEXT_t vosContext)
{
  VOS_STATUS vosStatus;
  tpAniSirGlobal pMac = (((pVosContextType)vosContext)->pMACContext);

  vosStatus = WLANTL_Close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close TL", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = sme_Close( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SME", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  /* CAC timer will be initiated and started only when SAP starts on
  * DFS channel and it will be stopped and destroyed immediately once the
  * radar detected or timedout. So as per design CAC timer should be
  * destroyed after stop.*/
  if (pMac->sap.SapDfsInfo.is_dfs_cac_timer_running) {
     vos_timer_stop(&pMac->sap.SapDfsInfo.sap_dfs_cac_timer);
     pMac->sap.SapDfsInfo.is_dfs_cac_timer_running = 0;
     vos_timer_destroy(&pMac->sap.SapDfsInfo.sap_dfs_cac_timer);
  }

  vosStatus = macClose( ((pVosContextType)vosContext)->pMACContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close MAC", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  ((pVosContextType)vosContext)->pMACContext = NULL;

  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SYS", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  if (TRUE == WDA_needShutdown(vosContext))
  {
    /* If WDA stop failed, call WDA shutdown to cleanup WDA/WDI. */
    vosStatus = WDA_shutdown(vosContext, VOS_TRUE);
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to shutdown WDA!", __func__);
      VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
    }
  }
  else
  {
    vosStatus = WDA_close(vosContext);
    if (!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to close WDA!", __func__);
      VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
    }
  }

  vosStatus = wma_wmi_work_close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to close wma_wmi_work!", __func__);
     VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
  }

  if (gpVosContext->htc_ctx)
  {
    HTCStop(gpVosContext->htc_ctx);
    HTCDestroy(gpVosContext->htc_ctx);
    gpVosContext->htc_ctx = NULL;
  }

  vosStatus = wma_wmi_service_close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to close wma_wmi_service!", __func__);
               VOS_ASSERT(VOS_IS_STATUS_SUCCESS(vosStatus));
  }


  vos_mq_deinit(&((pVosContextType)vosContext)->freeVosMq);

  vosStatus = vos_event_destroy(&gpVosContext->wdaCompleteEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy wdaCompleteEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_event_destroy(&gpVosContext->ProbeEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy ProbeEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  return VOS_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------

  \brief vos_wda_shutdown() - VOS interface to wda shutdown

     - WDA/WDI shutdown

  \param  vosContext: Global vos context


  \return VOS_STATUS_SUCCESS - Operation successfull

          VOS_STATUS_E_FAILURE - Failure to close

---------------------------------------------------------------------------*/
VOS_STATUS vos_wda_shutdown(v_CONTEXT_t vosContext)
{
  VOS_STATUS vosStatus;
  vosStatus = WDA_shutdown(vosContext, VOS_FALSE);

  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to shutdown WDA", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
  return vosStatus;
}
/**
  @brief vos_wlanShutdown() - This API will shutdown WLAN driver

  This function is called when Riva subsystem crashes.  There are two
  methods (or operations) in WLAN driver to handle Riva crash,
    1. shutdown: Called when Riva goes down, this will shutdown WLAN
                 driver without handshaking with Riva.
    2. re-init:  Next API
  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_wlanShutdown(void)
{
   VOS_STATUS vstatus;
   vstatus = vos_watchdog_wlan_shutdown();
   return vstatus;
}
/**
  @brief vos_wlanReInit() - This API will re-init WLAN driver

  This function is called when Riva subsystem reboots.  There are two
  methods (or operations) in WLAN driver to handle Riva crash,
    1. shutdown: Previous API
    2. re-init:  Called when Riva comes back after the crash. This will
                 re-initialize WLAN driver. In some cases re-open may be
                 referred instead of re-init.
  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_wlanReInit(void)
{
   VOS_STATUS vstatus;
   vstatus = vos_watchdog_wlan_re_init();
   return vstatus;
}
/**
  @brief vos_wlanRestart() - This API will reload WLAN driver.

  This function is called if driver detects any fatal state which
  can be recovered by a WLAN module reload ( Android framwork initiated ).
  Note that this API will not initiate any RIVA subsystem restart.

  The function wlan_hdd_restart_driver protects against re-entrant calls.

  @param
       NONE
  @return
       VOS_STATUS_SUCCESS   - Operation completed successfully.
       VOS_STATUS_E_FAILURE - Operation failed.
       VOS_STATUS_E_EMPTY   - No configured interface
       VOS_STATUS_E_ALREADY - Request already in progress


*/
VOS_STATUS vos_wlanRestart(void)
{
   VOS_STATUS vstatus;
   hdd_context_t *pHddCtx = NULL;
   v_CONTEXT_t pVosContext        = NULL;

   /* Check whether driver load unload is in progress */
   if(vos_is_load_unload_in_progress( VOS_MODULE_ID_VOSS, NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Driver load/unload is in progress, retry later.", __func__);
      return VOS_STATUS_E_AGAIN;
   }

   /* Get the Global VOSS Context */
   pVosContext = vos_get_global_context(VOS_MODULE_ID_VOSS, NULL);
   if(!pVosContext) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Global VOS context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   /* Get the HDD context */
   pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
   if(!pHddCtx) {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: HDD context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   /* Reload the driver */
   vstatus = wlan_hdd_restart_driver(pHddCtx);
   return vstatus;
}


/**
  @brief vos_fwDumpReq()

  This function is called to issue dump commands to Firmware

  @param
       cmd - Command No. to execute
       arg1 - argument 1 to cmd
       arg2 - argument 2 to cmd
       arg3 - argument 3 to cmd
       arg4 - argument 4 to cmd
  @return
       NONE
*/
v_VOID_t vos_fwDumpReq(tANI_U32 cmd, tANI_U32 arg1, tANI_U32 arg2,
                        tANI_U32 arg3, tANI_U32 arg4)
{
   WDA_HALDumpCmdReq(NULL, cmd, arg1, arg2, arg3, arg4, NULL);
}

VOS_STATUS vos_get_vdev_types(tVOS_CON_MODE mode, tANI_U32 *type,
        tANI_U32 *sub_type)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    *type = 0;
    *sub_type = 0;
    switch (mode)
    {
        case VOS_STA_MODE:
            *type = WMI_VDEV_TYPE_STA;
            break;
        case VOS_STA_SAP_MODE:
            *type = WMI_VDEV_TYPE_AP;
            break;
        case VOS_P2P_DEVICE_MODE:
            *type = WMI_VDEV_TYPE_AP;
            *sub_type = WMI_UNIFIED_VDEV_SUBTYPE_P2P_DEVICE;
            break;
        case VOS_P2P_CLIENT_MODE:
            *type = WMI_VDEV_TYPE_STA;
            *sub_type = WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT;
            break;
        case VOS_P2P_GO_MODE:
            *type = WMI_VDEV_TYPE_AP;
            *sub_type = WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO;
            break;
        case VOS_OCB_MODE:
            *type = WMI_VDEV_TYPE_OCB;
            break;
        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "Invalid device mode %d", mode);
            status = VOS_STATUS_E_INVAL;
            break;
    }
    return status;
}

v_BOOL_t vos_is_packet_log_enabled(void)
{
   hdd_context_t *pHddCtx;

   pHddCtx = (hdd_context_t*)(gpVosContext->pHDDContext);
   if((NULL == pHddCtx) ||
      (NULL == pHddCtx->cfg_ini))
   {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Hdd Context is Null", __func__);
     return FALSE;
   }

   return pHddCtx->cfg_ini->enablePacketLog;
}

void vos_trigger_recovery(void)
{
	pVosContextType vos_context;
	tp_wma_handle wma_handle;
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	void *runtime_context = NULL;

	vos_context = vos_get_global_context(VOS_MODULE_ID_VOSS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"VOS context is invald!");
		return;
	}

	wma_handle = (tp_wma_handle)vos_get_context(VOS_MODULE_ID_WDA,
						vos_context);
	if (!wma_handle) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"WMA context is invald!");
		return;
	}

	runtime_context = vos_runtime_pm_prevent_suspend_init("vos_recovery");
	vos_runtime_pm_prevent_suspend(runtime_context);

	wma_crash_inject(wma_handle, RECOVERY_SIM_SELF_RECOVERY, 0);

	status = vos_wait_single_event(&wma_handle->recovery_event,
		WMA_CRASH_INJECT_TIMEOUT);

	if (VOS_STATUS_SUCCESS != status) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"CRASH_INJECT command is timed out!");
		if (vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
			VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				"LOGP is in progress, ignore!");
			goto out;
		}
		vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);
		vos_schedule_recovery_work();
	}

out:
	vos_runtime_pm_allow_suspend(runtime_context);
	vos_runtime_pm_prevent_suspend_deinit(runtime_context);
}

/**
 * @brief vos_get_monotonic_boottime()
 * Get kernel boot time.
 * @return Time in microseconds
 */

v_U64_t vos_get_monotonic_boottime(void)
{
#ifdef CONFIG_CNSS
   struct timespec ts;

   vos_get_monotonic_boottime_ts(&ts);
   return (((v_U64_t)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
#else
   return ((v_U64_t)adf_os_ticks_to_msecs(adf_os_ticks()) * 1000);
#endif
}

#ifdef FEATURE_WLAN_D0WOW
v_VOID_t vos_pm_control(v_BOOL_t vote)
{
    vos_wlan_pm_control(vote);
}
#endif

/**
 * vos_set_wakelock_logging() - Logging of wakelock enabled/disabled
 * @value: Boolean value
 *
 * This function is used to set the flag which will indicate whether
 * logging of wakelock is enabled or not
 *
 * Return: None
 */
void vos_set_wakelock_logging(bool value)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"vos context is Invald");
		return;
	}
	vos_context->is_wakelock_log_enabled = value;
}

/**
 * vos_is_wakelock_enabled() - Check if logging of wakelock is enabled/disabled
 * @value: Boolean value
 *
 * This function is used to check whether logging of wakelock is enabled or not
 *
 * Return: true if logging of wakelock is enabled
 */
bool vos_is_wakelock_enabled(void)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"vos context is Invald");
		return false;
	}
	return vos_context->is_wakelock_log_enabled;
}

/**
 * vos_set_ring_log_level() - Convert HLOS values to driver log levels
 * @ring_id: ring_id
 * @log_levelvalue: Log level specificed
 *
 * This function sets the log level of a particular ring
 *
 * Return: None
 */
void vos_set_ring_log_level(uint32_t ring_id, uint32_t log_level)
{
	VosContextType *vos_context;
	uint32_t log_val;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invald", __func__);
		return;
	}

	switch (log_level) {
	case LOG_LEVEL_NO_COLLECTION:
		log_val = WLAN_LOG_LEVEL_OFF;
		break;
	case LOG_LEVEL_NORMAL_COLLECT:
		log_val = WLAN_LOG_LEVEL_NORMAL;
		break;
	case LOG_LEVEL_ISSUE_REPRO:
		log_val = WLAN_LOG_LEVEL_REPRO;
		break;
	case LOG_LEVEL_ACTIVE:
	default:
		log_val = WLAN_LOG_LEVEL_ACTIVE;
		break;
	}

	if (ring_id == RING_ID_WAKELOCK) {
		vos_context->wakelock_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_CONNECTIVITY) {
		vos_context->connectivity_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_PER_PACKET_STATS) {
		vos_context->packet_stats_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_DRIVER_DEBUG) {
		vos_context->driver_debug_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_FIRMWARE_DEBUG) {
		vos_context->fw_debug_log_level = log_val;
		return;
	}
}

/**
 * vos_get_ring_log_level() - Get the a ring id's log level
 * @ring_id: Ring id
 *
 * Fetch and return the log level corresponding to a ring id
 *
 * Return: Log level corresponding to the ring ID
 */
enum wifi_driver_log_level vos_get_ring_log_level(uint32_t ring_id)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invald", __func__);
		return WLAN_LOG_LEVEL_OFF;
	}

	if (ring_id == RING_ID_WAKELOCK)
		return vos_context->wakelock_log_level;
	else if (ring_id == RING_ID_CONNECTIVITY)
		return vos_context->connectivity_log_level;
	else if (ring_id == RING_ID_PER_PACKET_STATS)
		return vos_context->packet_stats_log_level;
	else if (ring_id == RING_ID_DRIVER_DEBUG)
		return vos_context->driver_debug_log_level;
	else if (ring_id == RING_ID_FIRMWARE_DEBUG)
		return vos_context->fw_debug_log_level;

	return WLAN_LOG_LEVEL_OFF;
}

/**
 * vos_set_multicast_logging() - Set mutlicast logging value
 * @value: Value of multicast logging
 *
 * Set the multicast logging value which will indicate
 * whether to multicast host and fw messages even
 * without any registration by userspace entity
 *
 * Return: None
 */
void vos_set_multicast_logging(uint8_t value)
{
	vos_multicast_logging = value;
}

/**
 * vos_is_multicast_logging() - Get multicast logging value
 *
 * Get the multicast logging value which will indicate
 * whether to multicast host and fw messages even
 * without any registration by userspace entity
 *
 * Return: 0 - Multicast logging disabled, 1 - Multicast logging enabled
 */
uint8_t vos_is_multicast_logging(void)
{
	return vos_multicast_logging;
}

/*
 * vos_init_log_completion() - Initialize log param structure
 *
 * This function is used to initialize the logging related
 * parameters
 *
 * Return: None
 */
void vos_init_log_completion(void)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return;
	}

	vos_context->log_complete.is_fatal = WLAN_LOG_TYPE_NON_FATAL;
	vos_context->log_complete.indicator = WLAN_LOG_INDICATOR_UNUSED;
	vos_context->log_complete.reason_code = WLAN_LOG_REASON_CODE_UNUSED;
	vos_context->log_complete.is_report_in_progress = false;
	/* Attempting to initialize an already initialized lock
	 * results in a failure. This must be ok here.
	 */
	vos_spin_lock_init(&vos_context->bug_report_lock);
}

/**
 * vos_deinit_log_completion() - Deinitialize log param structure
 *
 * This function is used to deinitialize the logging related
 * parameters
 *
 * Return: None
 */
void vos_deinit_log_completion(void)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return;
	}

	vos_spin_lock_destroy(&vos_context->bug_report_lock);
}

/**
 * vos_set_log_completion() - Store the logging params
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 *
 * This function is used to set the logging parameters based on the
 * caller
 *
 * Return: 0 if setting of params is successful
 */
VOS_STATUS vos_set_log_completion(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	vos_spin_lock_acquire(&vos_context->bug_report_lock);
	vos_context->log_complete.is_fatal = is_fatal;
	vos_context->log_complete.indicator = indicator;
	vos_context->log_complete.reason_code = reason_code;
	vos_context->log_complete.is_report_in_progress = true;
	vos_spin_lock_release(&vos_context->bug_report_lock);
	return VOS_STATUS_SUCCESS;
}

/**
 * vos_get_log_completion() - Get the logging related params
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 *
 * This function is used to get the logging related parameters
 *
 * Return: None
 */
void vos_get_log_completion(uint32_t *is_fatal,
		uint32_t *indicator,
		uint32_t *reason_code)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return;
	}

	vos_spin_lock_acquire(&vos_context->bug_report_lock);
	*is_fatal =  vos_context->log_complete.is_fatal;
	*indicator = vos_context->log_complete.indicator;
	*reason_code = vos_context->log_complete.reason_code;
	vos_context->log_complete.is_report_in_progress = false;
	vos_spin_lock_release(&vos_context->bug_report_lock);
}

/**
 * vos_is_log_report_in_progress() - Check if bug reporting is in progress
 *
 * This function is used to check if the bug reporting is already in progress
 *
 * Return: true if the bug reporting is in progress
 */
bool vos_is_log_report_in_progress(void)
{
	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return true;
	}
	return vos_context->log_complete.is_report_in_progress;
}

/**
 * vos_flush_logs() - Report fatal event to userspace
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 *
 * This function sets the log related params and send the WMI command to the
 * FW to flush its logs. On receiving the flush completion event from the FW
 * the same will be conveyed to userspace
 *
 * Return: 0 on success
 */
VOS_STATUS vos_flush_logs(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code)
{
	uint32_t ret;
	VOS_STATUS status;

	VosContextType *vos_context;

	vos_context = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_context) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: vos context is Invalid", __func__);
		return eHAL_STATUS_FAILURE;
	}

	if (vos_is_log_report_in_progress() == true) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
				"%s: Bug report already in progress - dropping! type:%d, indicator=%d reason_code=%d",
				__func__, is_fatal, indicator, reason_code);
		return VOS_STATUS_E_FAILURE;
	}

	status = vos_set_log_completion(is_fatal, indicator, reason_code);
	if (VOS_STATUS_SUCCESS != status) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
			"%s: Failed to set log trigger params", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
			"%s: Triggering bug report: type:%d, indicator=%d reason_code=%d",
			__func__, is_fatal, indicator, reason_code);

	ret = vos_send_flush_logs_cmd_to_fw(vos_context->pMACContext);
	if (0 != ret) {
		VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
			"%s: Failed to send flush FW log", __func__);
		vos_init_log_completion();
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

/**
 * vos_logging_set_fw_flush_complete() - Wrapper for FW log flush completion
 *
 * This function is used to send signal to the logger thread to indicate
 * that the flushing of FW logs is complete by the FW
 *
 * Return: None
 *
 */
void vos_logging_set_fw_flush_complete(void)
{
	wlan_logging_set_fw_flush_complete();
}

/**
 * vos_is_crash_indication_pending() - get crash indication status
 *
 * After wlan start up, we check the pending flag to know whether
 * it was caused by SSR. If it 's true,we need to indicate a netlink
 * message to wlan service to restart application process (hostapd).
 *
 * Return: true if carsh indication is pending.
 */
bool vos_is_crash_indication_pending(void)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
		"%s: global voss context is NULL", __func__);
	  return false;
	}

	return gpVosContext->crash_indication_pending;
}

/**
 * vos_set_crash_indication_pending() - set crash indication status
 * @value: pending statue to set
 *
 * Upon crash happends, we set the pending flag to true. To indicate
 * the crash indication event to wlan service is needed after recovery.
 *
 * Return: None
 */
void vos_set_crash_indication_pending(bool value)
{
	if (gpVosContext == NULL) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
		"%s: global voss context is NULL", __func__);
		return ;
	}

	gpVosContext->crash_indication_pending = value;
}
