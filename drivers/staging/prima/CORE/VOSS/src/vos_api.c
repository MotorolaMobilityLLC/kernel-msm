/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/**=========================================================================
  
  \file  vos_api.c

  \brief Stub file for all virtual Operating System Services (vOSS) APIs
               
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
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
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "aniGlobal.h"
#include "halTypes.h"
#include "wlan_qct_sal.h"
#include "wlan_qct_bal.h"
#endif
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


#ifdef WLAN_SOFTAP_FEATURE
#include "sapApi.h"
#endif



#ifdef WLAN_BTAMP_FEATURE
#include "bapApi.h"
#include "bapInternal.h"
#include "bap_hdd_main.h"
#endif //WLAN_BTAMP_FEATURE


/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ------------------------------------------------------------------------*/
/* Amount of time to wait for WDA to perform an asynchronous activity.
   This value should be larger than the timeout used by WDI to wait for
   a response from WCNSS since in the event that WCNSS is not responding,
   WDI should handle that timeout */
#define VOS_WDA_TIMEOUT 15000

/* Approximate amount of time to wait for WDA to stop WDI */
#define VOS_WDA_STOP_TIMEOUT WDA_STOP_TIMEOUT 

/*---------------------------------------------------------------------------
 * Data definitions
 * ------------------------------------------------------------------------*/
static VosContextType  gVosContext;
static pVosContextType gpVosContext;

/*---------------------------------------------------------------------------
 * Forward declaration
 * ------------------------------------------------------------------------*/
v_VOID_t vos_sys_probe_thread_cback ( v_VOID_t *pUserData );

#ifndef FEATURE_WLAN_INTEGRATED_SOC
v_VOID_t vos_sys_start_complete_cback  ( v_VOID_t *pUserData );
#endif
v_VOID_t vos_core_return_msg(v_PVOID_t pVContext, pVosMsgWrapper pMsgWrapper);

v_VOID_t vos_fetch_tl_cfg_parms ( WLANTL_ConfigInfoType *pTLConfig, 
    hdd_config_t * pConfig );
#ifndef FEATURE_WLAN_INTEGRATED_SOC
VOS_STATUS vos_get_fwbinary( v_VOID_t **ppBinary, v_SIZE_t *pNumBytes );
#endif


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

   /* Initialize the probe event */
   if (vos_event_init(&gpVosContext->ProbeEvent) != VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                    "%s: Unable to init probeEvent", __func__);
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
   if (vos_event_init( &(gpVosContext->wdaCompleteEvent) ) != VOS_STATUS_SUCCESS )
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: Unable to init wdaCompleteEvent", __func__);
      VOS_ASSERT(0);
    
      goto err_probe_event;
   }

   /* Saving the HDD context */
   /* This is saved in hdd_wlan_start_up before calling vos_open
      gpVosContext->pHDDContext = pHddContext;*/
 
   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
               "%s: HDD context is saved successfully", __func__);
   
#endif

   /* Initialize the free message queue */
   vStatus = vos_mq_init(&gpVosContext->freeVosMq);
   if (! VOS_IS_STATUS_SUCCESS(vStatus))
   {

      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to initialize VOS free message queue", __func__);
      VOS_ASSERT(0);
#ifndef FEATURE_WLAN_INTEGRATED_SOC
      goto err_probe_event;
#else
      goto err_wda_complete_event;
#endif
   }

   for (iter = 0; iter < VOS_CORE_MAX_MESSAGES; iter++)
   {
      (gpVosContext->aMsgWrappers[iter]).pVosMsg = 
         &(gpVosContext->aMsgBuffers[iter]); 
      INIT_LIST_HEAD(&gpVosContext->aMsgWrappers[iter].msgNode);
      vos_mq_put(&gpVosContext->freeVosMq, &(gpVosContext->aMsgWrappers[iter]));
   }
#ifndef FEATURE_WLAN_INTEGRATED_SOC
   /* Initialize here the VOS Packet sub module */
   vStatus = vos_packet_open( gpVosContext, &gpVosContext->vosPacket,
                              sizeof( vos_pkt_context_t ) );

   if ( !VOS_IS_STATUS_SUCCESS( vStatus ) )
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open VOS Packet Module", __func__);
      VOS_ASSERT(0);
      goto err_msg_queue;
   }
#endif   

   /* Now Open the VOS Scheduler */
   vStatus= vos_sched_open(gpVosContext, &gpVosContext->vosSched,
                           sizeof(VosSchedContext));

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open VOS Scheduler", __func__);
      VOS_ASSERT(0);
#ifndef FEATURE_WLAN_INTEGRATED_SOC
      goto err_packet_close;
#else
      goto err_msg_queue;
#endif
   }

#ifdef FEATURE_WLAN_INTEGRATED_SOC
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
   vStatus = WDA_open( gpVosContext, gpVosContext->pHDDContext, &macOpenParms );

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open WDA module", __func__);
      VOS_ASSERT(0);
      goto err_sched_close;
   }

   /* Initialize here the VOS Packet sub module */
   vStatus = vos_packet_open( gpVosContext, &gpVosContext->vosPacket,
                              sizeof( vos_pkt_context_t ) );

   if ( !VOS_IS_STATUS_SUCCESS( vStatus ) )
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open VOS Packet Module", __func__);
      VOS_ASSERT(0);
      goto err_wda_close;
   }
#endif

   /* Open the SYS module */
   vStatus = sysOpen(gpVosContext);

   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
      /* Critical Error ...  Cannot proceed further */
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to open SYS module", __func__);
      VOS_ASSERT(0);
#ifndef FEATURE_WLAN_INTEGRATED_SOC
      goto err_sched_close;
#else
      goto err_packet_close;
#endif
   }


   /* initialize the NV module */
   vStatus = vos_nv_open();
   if (!VOS_IS_STATUS_SUCCESS(vStatus))
   {
     // NV module cannot be initialized
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to initialize the NV module", __func__);
     goto err_sys_close;
   }
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
   /* Probe the MC thread */
   sysMcThreadProbe(gpVosContext, 
                    &vos_sys_probe_thread_cback,
                    gpVosContext);

   if (vos_wait_single_event(&gpVosContext->ProbeEvent, 0)!= VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to probe MC Thread", __func__);
      VOS_ASSERT(0);
      goto err_nv_close;
   }

   /*Now probe the Tx thread */
   sysTxThreadProbe(gpVosContext, 
                    &vos_sys_probe_thread_cback,
                    gpVosContext);
   
   if (vos_wait_single_event(&gpVosContext->ProbeEvent, 0)!= VOS_STATUS_SUCCESS)
   {
      VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Failed to probe TX Thread", __func__);
      VOS_ASSERT(0);
      goto err_nv_close;
   }
#endif

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

   VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS successfully Opened", __func__);

   *pVosContext = gpVosContext;

   return VOS_STATUS_SUCCESS;


err_sme_close:
   sme_Close(gpVosContext->pMACContext);

err_mac_close:
   macClose(gpVosContext->pMACContext);

err_nv_close:
   vos_nv_close();
   
err_sys_close:   
   sysClose(gpVosContext);

#ifdef FEATURE_WLAN_INTEGRATED_SOC
err_packet_close:
   vos_packet_close( gpVosContext );

err_wda_close:
   WDA_close(gpVosContext);
#endif

err_sched_close:   
   vos_sched_close(gpVosContext);

#ifndef FEATURE_WLAN_INTEGRATED_SOC
err_packet_close:
   vos_packet_close( gpVosContext );
#endif

err_msg_queue:
   vos_mq_deinit(&gpVosContext->freeVosMq);

#ifdef FEATURE_WLAN_INTEGRATED_SOC
err_wda_complete_event:
   vos_event_destroy( &gpVosContext->wdaCompleteEvent );
#endif

err_probe_event:
   vos_event_destroy(&gpVosContext->ProbeEvent);

   return VOS_STATUS_E_FAILURE;

} /* vos_open() */

#ifdef FEATURE_WLAN_INTEGRATED_SOC
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
   
   VOS_TRACE(VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_INFO,
             "vos prestart");

   VOS_ASSERT(gpVosContext == pVosContext);

   VOS_ASSERT( NULL != pVosContext->pMACContext);

   VOS_ASSERT( NULL != pVosContext->pWDAContext);

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
      macStop(gpVosContext->pMACContext, HAL_STOP_TYPE_SYS_DEEP_SLEEP);
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
          "%s: Timeout occurred before WDA complete\n", __func__);
      }
      else
      {
         VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: WDA_preStart reporting other error", __func__);
      }
      macStop(gpVosContext->pMACContext, HAL_STOP_TYPE_SYS_DEEP_SLEEP);
      ccmStop(gpVosContext->pMACContext);
      VOS_ASSERT( 0 );
      return VOS_STATUS_E_FAILURE;
   }

   return VOS_STATUS_SUCCESS;
}
#endif

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
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  v_VOID_t *pFwBinary = NULL;
  v_SIZE_t  numFwBytes = 0;
#endif

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: Starting Libra SW", __func__);

  /* We support only one instance for now ...*/
  if (gpVosContext != pVosContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: mismatch in context", __FUNCTION__);
     return VOS_STATUS_E_FAILURE;
  }

#ifndef FEATURE_WLAN_INTEGRATED_SOC
  if (( pVosContext->pBALContext == NULL) || ( pVosContext->pMACContext == NULL)
     || ( pVosContext->pTLContext == NULL))
  {
     if (pVosContext->pBALContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: BAL NULL context", __FUNCTION__);
     else if (pVosContext->pMACContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: MAC NULL context", __FUNCTION__);
     else
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: TL NULL context", __FUNCTION__);
     
     return VOS_STATUS_E_FAILURE;
  }
#else
  if (( pVosContext->pWDAContext == NULL) || ( pVosContext->pMACContext == NULL)
     || ( pVosContext->pTLContext == NULL))
  {
     if (pVosContext->pWDAContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: WDA NULL context", __FUNCTION__);
     else if (pVosContext->pMACContext == NULL)
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: MAC NULL context", __FUNCTION__);
     else
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
            "%s: TL NULL context", __FUNCTION__);
     
     return VOS_STATUS_E_FAILURE;
  }

  /* WDA_Start will be called after NV image download because the 
    NV image data has to be updated at HAL before HAL_Start gets executed*/

  /* Start the NV Image Download */

  vos_event_reset( &(gpVosContext->wdaCompleteEvent) );

  vStatus = WDA_NVDownload_Start(pVosContext);

  if ( vStatus != VOS_STATUS_SUCCESS )
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                 "%s: Failed to start NV Download", __func__);
     return VOS_STATUS_E_FAILURE;
  }

  vStatus = vos_wait_single_event( &(gpVosContext->wdaCompleteEvent),
                                   VOS_WDA_TIMEOUT );

  if ( vStatus != VOS_STATUS_SUCCESS )
  {
     if ( vStatus == VOS_STATUS_E_TIMEOUT )
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Timeout occurred before WDA_NVDownload_start complete", __func__);
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: WDA_NVDownload_start reporting other error", __func__);
     }
     VOS_ASSERT(0);
     goto err_wda_stop;   
  }

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: WDA_NVDownload_start correctly started", __func__);

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

#endif
  /* Start the MAC */
  vos_mem_zero((v_PVOID_t)&halStartParams, sizeof(tHalMacStartParameters));

#ifndef FEATURE_WLAN_INTEGRATED_SOC
  /* Attempt to get the firmware binary through VOS.  We need to pass this
     to the MAC when starting. */
  vStatus = vos_get_fwbinary(&pFwBinary, &numFwBytes);
  
  if ( !VOS_IS_STATUS_SUCCESS( vStatus ) )
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s: Failed to get firmware binary", __func__);
    return VOS_STATUS_E_FAILURE;
  }

  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
             "%s: Firmware binary file found", __func__);
   

  //Begining kernel 2.6.31, memory buffer returned by request_firmware API
  //cannot be overwritten. So need to copy the firmware into a separate buffer
  //as HAL needs to modify the endianess of FW binary.

  //Kernel may not have ~40 pages of free buffers always, so
  //Store the pointer to the buffer provided by kernel for now,
  //When required copy it to local buffer for translation.
  //This is done in vos_api.c, during firmware download
  //This would provide common fix for ftm driver issue aswell, 
  //which is not creating the copy of firmware image for endian conversion.
  halStartParams.FW.cbImage = numFwBytes;
  halStartParams.FW.pImage = pFwBinary;

 //determine the driverType for the mode of operation
 halStartParams.driverType = eDRIVER_TYPE_PRODUCTION;
#endif
  /* Start the MAC */
  sirStatus = macStart(pVosContext->pMACContext,(v_PVOID_t)&halStartParams);

#ifndef FEATURE_WLAN_INTEGRATED_SOC
  hdd_release_firmware(WLAN_FW_FILE, pVosContext->pHDDContext);

  halStartParams.FW.pImage = NULL;
  halStartParams.FW.cbImage = 0;
#endif 
  if (eSIR_SUCCESS != sirStatus)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
              "%s: Failed to start MAC", __func__);
#ifndef FEATURE_WLAN_INTEGRATED_SOC
    return VOS_STATUS_E_FAILURE;
#else
    goto err_wda_stop;
#endif
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
#ifndef FEATURE_WLAN_INTEGRATED_SOC  
  /* START SYS. This will trigger the CFG download */
  sysMcStart(pVosContext, vos_sys_start_complete_cback, pVosContext);

  if (vos_wait_single_event(&gpVosContext->ProbeEvent, 0)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Failed to start SYS module", __func__);
     goto err_tl_stop;
  }


   /**
   EVM issue is observed with 1.6Mhz freq for 1.3V RF supply in wlan standalone case.
   During concurrent operation (e.g. WLAN and WCDMA) this issue is not observed.
   To workaround, wlan will vote for 3.2Mhz during startup and will vote for 1.6Mhz
   during exit.
   Since using 3.2Mhz has a side effect on power (extra 200ua), this is left configurable.
   If customers do their design right, they should not see the EVM issue and in that case they
   can decide to keep 1.6Mhz by setting an NV.
   If NV item is not present, use the default 3.2Mhz
   vos_stop is also invoked if wlan startup seq fails (after vos_start, where 3.2Mhz is voted.)
   */
  {
   sFreqFor1p3VSupply freq;
   vStatus = vos_nv_read( NV_TABLE_FREQUENCY_FOR_1_3V_SUPPLY, &freq, NULL,
         sizeof(freq) );
   if (VOS_STATUS_SUCCESS != vStatus)
    freq.freqFor1p3VSupply = VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_3P2MH;

    if (vos_chipVoteFreqFor1p3VSupply(NULL, NULL, NULL, freq.freqFor1p3VSupply) != VOS_STATUS_SUCCESS)
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to set the freq %d for 1.3V Supply",
                   __func__, freq.freqFor1p3VSupply );
  }

#endif
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
            "%s: VOSS Start is successful!!", __func__);

  return VOS_STATUS_SUCCESS;

#ifndef FEATURE_WLAN_INTEGRATED_SOC
err_tl_stop:
  WLANTL_Stop(pVosContext);
#endif

err_sme_stop:
  sme_Stop(pVosContext->pMACContext, TRUE);
    
err_mac_stop:
  macStop( pVosContext->pMACContext, HAL_STOP_TYPE_SYS_RESET );

#ifdef FEATURE_WLAN_INTEGRATED_SOC
err_wda_stop:   
  vos_event_reset( &(gpVosContext->wdaCompleteEvent) );
  WDA_stop( pVosContext, HAL_STOP_TYPE_RF_KILL);
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
  }
#endif

  return VOS_STATUS_E_FAILURE;
   
} /* vos_start() */


/* vos_stop function */
VOS_STATUS vos_stop( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;

#ifdef FEATURE_WLAN_INTEGRATED_SOC
  /* WDA_Stop is called before the SYS so that the processing of Riva 
  pending responces will not be handled during uninitialization of WLAN driver */
  vos_event_reset( &(gpVosContext->wdaCompleteEvent) );

  vosStatus = WDA_stop( vosContext, HAL_STOP_TYPE_RF_KILL );

  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to stop WDA", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = vos_wait_single_event( &(gpVosContext->wdaCompleteEvent),
                                     VOS_WDA_STOP_TIMEOUT );
   
  if ( vosStatus != VOS_STATUS_SUCCESS )
  {
     if ( vosStatus == VOS_STATUS_E_TIMEOUT )
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Timeout occurred before WDA complete", __func__);
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: WDA_stop reporting other error", __func__ );
     }
     /* if WDA stop failed, call WDA shutdown to cleanup WDA/WDI */
     vosStatus = WDA_shutdown( vosContext, VOS_TRUE );
     if (VOS_IS_STATUS_SUCCESS( vosStatus ) )
     {
        hdd_set_ssr_required( VOS_TRUE );
     }
     else
     {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                               "%s: Failed to shutdown WDA", __func__ );
        VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
     }
  }
#endif

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

#ifndef FEATURE_WLAN_INTEGRATED_SOC
   /**
   EVM issue is observed with 1.6Mhz freq for 1.3V supply in wlan standalone case.
   During concurrent operation (e.g. WLAN and WCDMA) this issue is not observed.
   To workaround, wlan will vote for 3.2Mhz during startup and will vote for 1.6Mhz
   during exit.
   vos_stop is also invoked if wlan startup seq fails (after vos_start, where 3.2Mhz is voted.)
   */
   if (vos_chipVoteFreqFor1p3VSupply(NULL, NULL, NULL, VOS_NV_FREQUENCY_FOR_1_3V_SUPPLY_1P6MH) != VOS_STATUS_SUCCESS)
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to set the freq to 1.6Mhz for 1.3V Supply", __func__ );
#endif

  return VOS_STATUS_SUCCESS;
}


/* vos_close function */
VOS_STATUS vos_close( v_CONTEXT_t vosContext )
{
  VOS_STATUS vosStatus;

#ifdef WLAN_BTAMP_FEATURE
  vosStatus = WLANBAP_Close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close BAP", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif // WLAN_BTAMP_FEATURE


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

  vosStatus = vos_nv_close();
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close NV", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }


  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SYS", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

#ifdef FEATURE_WLAN_INTEGRATED_SOC
  vosStatus = WDA_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close WDA", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
  
  /* Let DXE return packets in WDA_close and then free them here */
  vosStatus = vos_packet_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close VOSS Packet", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif

#ifndef FEATURE_WLAN_INTEGRATED_SOC
  vosStatus = vos_packet_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close VOSS Packet", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif

  vos_mq_deinit(&((pVosContextType)vosContext)->freeVosMq);

#ifdef FEATURE_WLAN_INTEGRATED_SOC
  vosStatus = vos_event_destroy(&gpVosContext->wdaCompleteEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy wdaCompleteEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif

  vosStatus = vos_event_destroy(&gpVosContext->ProbeEvent);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: failed to destroy ProbeEvent", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

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
        "%s: vos context pointer is null", __FUNCTION__);
    return NULL;
  }

  if (gpVosContext != pVosContext)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: pVosContext != gpVosContext", __FUNCTION__);
    return NULL;
  }

  switch(moduleId)
  {
    case VOS_MODULE_ID_TL:  
    {
      pModContext = gpVosContext->pTLContext;
      break;
    }

#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_BAL:
    {
      pModContext = gpVosContext->pBALContext;
      break;
    }   

    case VOS_MODULE_ID_SAL:
    {
      pModContext = gpVosContext->pSALContext;
      break;
    }   

    case VOS_MODULE_ID_SSC:   
    {
      pModContext = gpVosContext->pSSCContext;
      break;
    }
#endif
#ifdef WLAN_BTAMP_FEATURE
    case VOS_MODULE_ID_BAP:
    {
        pModContext = gpVosContext->pBAPContext;
        break;
    }    
#endif //WLAN_BTAMP_FEATURE

#ifdef WLAN_SOFTAP_FEATURE
    case VOS_MODULE_ID_SAP:
    {
      pModContext = gpVosContext->pSAPContext;
      break;
    }

    case VOS_MODULE_ID_HDD_SOFTAP:
    {
      pModContext = gpVosContext->pHDDSoftAPContext;
      break;
    }
#endif

    case VOS_MODULE_ID_HDD:
    {
      pModContext = gpVosContext->pHDDContext;
      break;
    }

    case VOS_MODULE_ID_SME:
#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_HAL:
#endif
    case VOS_MODULE_ID_PE:
    {
      /* 
      ** In all these cases, we just return the MAC Context
      */
      pModContext = gpVosContext->pMACContext;
      break;
    }

#ifdef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_WDA:
    {
      /* For WDA module */
      pModContext = gpVosContext->pWDAContext;
      break;
    }
#endif

    case VOS_MODULE_ID_VOSS:
    {
      /* For SYS this is VOS itself*/
      pModContext = gpVosContext;
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
        "%s: global voss context is NULL", __FUNCTION__);
  }

  return gpVosContext;

} /* vos_get_global_context() */


v_U8_t vos_is_logp_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: global voss context is NULL", __FUNCTION__);
    return 1;
  }

   return gpVosContext->isLogpInProgress;
}

void vos_set_logp_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: global voss context is NULL", __FUNCTION__);
    return;
  }

   gpVosContext->isLogpInProgress = value;
}

v_U8_t vos_is_load_unload_in_progress(VOS_MODULE_ID moduleId, v_VOID_t *moduleContext)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: global voss context is NULL", __FUNCTION__);
    return 0; 
  }

   return gpVosContext->isLoadUnloadInProgress;
}

void vos_set_load_unload_in_progress(VOS_MODULE_ID moduleId, v_U8_t value)
{
  if (gpVosContext == NULL)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: global voss context is NULL", __FUNCTION__);
    return;
  }

   gpVosContext->isLoadUnloadInProgress = value;
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
        "%s: vos context is null", __FUNCTION__);
    return VOS_STATUS_E_FAILURE;
  }

  if (( gpVosContext != pVosContext) || ( ppModuleContext == NULL)) {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
        "%s: context mismatch or null param passed", __FUNCTION__);
    return VOS_STATUS_E_FAILURE;
  }

  switch(moduleID)
  {
    case VOS_MODULE_ID_TL:  
    {
      pGpModContext = &(gpVosContext->pTLContext); 
      break;
    }

#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_BAL:
    {
      pGpModContext = &(gpVosContext->pBALContext);
      break;
    }

    case VOS_MODULE_ID_SAL:
    {
      pGpModContext = &(gpVosContext->pSALContext);
      break;
    }   

    case VOS_MODULE_ID_SSC:
    {
      pGpModContext = &(gpVosContext->pSSCContext); 
      break;
    }
#endif
#ifdef WLAN_BTAMP_FEATURE
    case VOS_MODULE_ID_BAP:
    {
        pGpModContext = &(gpVosContext->pBAPContext);
        break;
    }    
#endif //WLAN_BTAMP_FEATURE

#ifdef WLAN_SOFTAP_FEATURE
    case VOS_MODULE_ID_SAP:
    {
      pGpModContext = &(gpVosContext->pSAPContext);
      break;
    }
#endif

#ifdef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_WDA:
    {
      pGpModContext = &(gpVosContext->pWDAContext);
      break;
    }
#endif
    case VOS_MODULE_ID_SME:
#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_HAL:
#endif
    case VOS_MODULE_ID_PE:
    case VOS_MODULE_ID_HDD:
#ifdef WLAN_SOFTAP_FEATURE
    case VOS_MODULE_ID_HDD_SOFTAP:
#endif
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

#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_BAL:
    {
      pGpModContext = &(gpVosContext->pBALContext);
      break;
    }   

    case VOS_MODULE_ID_SAL:
    {
      pGpModContext = &(gpVosContext->pSALContext);
      break;
    }   

    case VOS_MODULE_ID_SSC:
    {
      pGpModContext = &(gpVosContext->pSSCContext); 
      break;
    }
#endif
#ifdef WLAN_BTAMP_FEATURE
    case VOS_MODULE_ID_BAP:
    {
        pGpModContext = &(gpVosContext->pBAPContext);
        break;
    }
#endif //WLAN_BTAMP_FEATURE
 
#ifdef WLAN_SOFTAP_FEATURE
    case VOS_MODULE_ID_SAP:
    {
      pGpModContext = &(gpVosContext->pSAPContext); 
      break;
    }
#endif

#ifdef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_WDA:
    {
      pGpModContext = &(gpVosContext->pWDAContext);
      break;
    }
#endif
    case VOS_MODULE_ID_HDD:
    case VOS_MODULE_ID_SME:
#ifndef FEATURE_WLAN_INTEGRATED_SOC
    case VOS_MODULE_ID_HAL:
#endif
    case VOS_MODULE_ID_PE:
#ifdef WLAN_SOFTAP_FEATURE
    case VOS_MODULE_ID_HDD_SOFTAP:
#endif
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
        "%s: pGpModContext != pModuleContext", __FUNCTION__);
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

#ifndef FEATURE_WLAN_INTEGRATED_SOC
    /// Message Queue ID for messages bound for HAL
    case VOS_MQ_ID_HAL: 
    {
       pTargetMq = &(gpVosContext->vosSched.halMcMq);
       break;
    }
#else
    /// Message Queue ID for messages bound for WDA
    case VOS_MQ_ID_WDA: 
    {
       pTargetMq = &(gpVosContext->vosSched.wdaMcMq);
       break;
    }

    /// Message Queue ID for messages bound for WDI
    case VOS_MQ_ID_WDI:
    {
       pTargetMq = &(gpVosContext->vosSched.wdiMcMq);
       break;
    }
#endif

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
         "%s: pTargetMq == NULL", __FUNCTION__);
     return VOS_STATUS_E_FAILURE;
  } 

  /*
  ** Try and get a free Msg wrapper
  */
  pMsgWrapper = vos_mq_get(&gpVosContext->freeVosMq);

  if (NULL == pMsgWrapper)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: VOS Core run out of message wrapper", __func__);

    return VOS_STATUS_E_RESOURCES;
  }
  
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


/**---------------------------------------------------------------------------
  
  \brief vos_tx_mq_serialize() - serialize a message to the Tx execution flow

  This API allows messages to be posted to a specific message queue in the 
  Tx excution flow.  Messages for the Tx execution flow can be posted only 
  to the following queue.
  
  <ul>
    <li> TL
    <li> SSC/WDI
  </ul>
  
  \param msgQueueId - identifies the message queue upon which the message
         will be posted.
         
  \param message - a pointer to a message buffer.  Body memory for this message 
         buffer is allocated by the caller and free'd by the vOSS after the
         message is dispacthed to the appropriate component.  If the consumer 
         of the message needs to keep anything in the body, it needs to copy 
         the contents before returning from the message handler.
  
  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.
          
          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not 
          refer to a valid Message Queue Id.
          
          VOS_STATUS_E_FAULT  - message is an invalid pointer.     
          
          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa
  
  --------------------------------------------------------------------------*/
VOS_STATUS vos_tx_mq_serialize( VOS_MQ_ID msgQueueId, vos_msg_t *pMsg )
{
  pVosMqType      pTargetMq   = NULL;
  pVosMsgWrapper  pMsgWrapper = NULL;

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
    case  VOS_MQ_ID_TL: 
    {
       pTargetMq = &(gpVosContext->vosSched.tlTxMq);
       break;
    }

#ifndef FEATURE_WLAN_INTEGRATED_SOC
    /// Message Queue ID for messages bound for SSC
    case VOS_MQ_ID_SSC:  
    {
       pTargetMq = &(gpVosContext->vosSched.sscTxMq);
       break;
    }
#else
    /// Message Queue ID for messages bound for SSC
    case VOS_MQ_ID_WDI:  
    {
       pTargetMq = &(gpVosContext->vosSched.wdiTxMq);
       break;
    }
#endif 
    
    /// Message Queue ID for messages bound for the SYS module
    case VOS_MQ_ID_SYS:
    {
       pTargetMq = &(gpVosContext->vosSched.sysTxMq);
       break;
    }

    default:

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "Trying to queue msg into unknown Tx Msg queue ID %d",
               __func__, msgQueueId);

    return VOS_STATUS_E_FAILURE;
  }

  if (pTargetMq == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: pTargetMq == NULL", __FUNCTION__);
     return VOS_STATUS_E_FAILURE;
  } 
    

  /*
  ** Try and get a free Msg wrapper
  */
  pMsgWrapper = vos_mq_get(&gpVosContext->freeVosMq);

  if (NULL == pMsgWrapper)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
              "%s: VOS Core run out of message wrapper", __func__);

    return VOS_STATUS_E_RESOURCES;
  }

  /*
  ** Copy the message now
  */
  vos_mem_copy( (v_VOID_t*)pMsgWrapper->pVosMsg, 
                (v_VOID_t*)pMsg, sizeof(vos_msg_t));

  vos_mq_put(pTargetMq, pMsgWrapper);

  set_bit(TX_POST_EVENT_MASK, &gpVosContext->vosSched.txEventFlag);
  wake_up_interruptible(&gpVosContext->vosSched.txWaitQueue);

  return VOS_STATUS_SUCCESS;

} /* vos_tx_mq_serialize()*/

#ifdef FEATURE_WLAN_INTEGRATED_SOC
/**---------------------------------------------------------------------------

  \brief vos_rx_mq_serialize() - serialize a message to the Rx execution flow

  This API allows messages to be posted to a specific message queue in the
  Tx excution flow.  Messages for the Rx execution flow can be posted only
  to the following queue.

  <ul>
    <li> TL
    <li> WDI
  </ul>

  \param msgQueueId - identifies the message queue upon which the message
         will be posted.

  \param message - a pointer to a message buffer.  Body memory for this message
         buffer is allocated by the caller and free'd by the vOSS after the
         message is dispacthed to the appropriate component.  If the consumer
         of the message needs to keep anything in the body, it needs to copy
         the contents before returning from the message handler.

  \return VOS_STATUS_SUCCESS - the message has been successfully posted
          to the message queue.

          VOS_STATUS_E_INVAL - The value specified by msgQueueId does not
          refer to a valid Message Queue Id.

          VOS_STATUS_E_FAULT  - message is an invalid pointer.

          VOS_STATUS_E_FAILURE - the message queue handler has reported
          an unknown failure.

  \sa

  --------------------------------------------------------------------------*/

VOS_STATUS vos_rx_mq_serialize( VOS_MQ_ID msgQueueId, vos_msg_t *pMsg )
{
  pVosMqType      pTargetMq   = NULL;
  pVosMsgWrapper  pMsgWrapper = NULL;
  if ((gpVosContext == NULL) || (pMsg == NULL))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Null params or global vos context is null", __func__);
    VOS_ASSERT(0);
    return VOS_STATUS_E_FAILURE;
  }

  switch (msgQueueId)
  {

    case VOS_MQ_ID_SYS:
    {
       pTargetMq = &(gpVosContext->vosSched.sysRxMq);
       break;
    }

    /// Message Queue ID for messages bound for WDI
    case VOS_MQ_ID_WDI:
    {
       pTargetMq = &(gpVosContext->vosSched.wdiRxMq);
       break;
    }

    default:

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "Trying to queue msg into unknown Rx Msg queue ID %d",
               __func__, msgQueueId);

    return VOS_STATUS_E_FAILURE;
  }

  if (pTargetMq == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pTargetMq == NULL", __FUNCTION__);
     return VOS_STATUS_E_FAILURE;
  }


  /*
  ** Try and get a free Msg wrapper
  */
  pMsgWrapper = vos_mq_get(&gpVosContext->freeVosMq);

  if (NULL == pMsgWrapper)
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: VOS Core run out of message wrapper", __func__);

    return VOS_STATUS_E_RESOURCES;
  }

  /*
  ** Copy the message now
  */
  vos_mem_copy( (v_VOID_t*)pMsgWrapper->pVosMsg,
                (v_VOID_t*)pMsg, sizeof(vos_msg_t));

  vos_mq_put(pTargetMq, pMsgWrapper);

  set_bit(RX_POST_EVENT_MASK, &gpVosContext->vosSched.rxEventFlag);
  wake_up_interruptible(&gpVosContext->vosSched.rxWaitQueue);

  return VOS_STATUS_SUCCESS;

} /* vos_rx_mq_serialize()*/

#endif
v_VOID_t 
vos_sys_probe_thread_cback 
( 
  v_VOID_t *pUserData 
)
{
  if (gpVosContext != pUserData)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: gpVosContext != pUserData", __FUNCTION__);
     return;
  } 

  if (vos_event_set(&gpVosContext->ProbeEvent)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: vos_event_set failed", __FUNCTION__);
     return;
  }

} /* vos_sys_probe_thread_cback() */

#ifndef FEATURE_WLAN_INTEGRATED_SOC
v_VOID_t vos_sys_start_complete_cback
( 
  v_VOID_t *pUserData
)
{

  if (gpVosContext != pUserData)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: gpVosContext != pUserData", __FUNCTION__);
     return;
  } 

  if (vos_event_set(&gpVosContext->ProbeEvent)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: vos_event_set failed", __FUNCTION__);
     return;
  }

} /* vos_sys_start_complete_cback() */
#else
v_VOID_t vos_WDAComplete_cback
(
  v_VOID_t *pUserData
)
{

  if (gpVosContext != pUserData)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosContext != pUserData", __FUNCTION__);
     return;
  }

  if (vos_event_set(&gpVosContext->wdaCompleteEvent)!= VOS_STATUS_SUCCESS)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: vos_event_set failed", __FUNCTION__);
     return;
  }

} /* vos_WDAComplete_cback() */
#endif

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
         "%s: gpVosContext != pVosContext", __FUNCTION__);
     return;
  } 

  VOS_ASSERT( NULL !=pMsgWrapper );

  if (pMsgWrapper == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, 
         "%s: pMsgWrapper == NULL in function", __FUNCTION__);
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
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR, "%s NULL ptr passed in!", __FUNCTION__);
   return;
  }

  pTLConfig->ucAcWeights[0] = pConfig->WfqBkWeight;
  pTLConfig->ucAcWeights[1] = pConfig->WfqBeWeight;
  pTLConfig->ucAcWeights[2] = pConfig->WfqViWeight;
  pTLConfig->ucAcWeights[3] = pConfig->WfqVoWeight;
  pTLConfig->uDelayedTriggerFrmInt = pConfig->DelayedTriggerFrmInt;
#ifdef WLAN_SOFTAP_FEATURE
  pTLConfig->uMinFramesProcThres = pConfig->MinFramesProcThres;
#endif

}

v_BOOL_t vos_is_apps_power_collapse_allowed(void* pHddCtx)
{
  return hdd_is_apps_power_collapse_allowed((hdd_context_t*) pHddCtx);
}

void vos_abort_mac_scan(void)
{
    hdd_context_t *pHddCtx = NULL;
    v_CONTEXT_t pVosContext        = NULL;

    /* Get the Global VOSS Context */
    pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
    if(!pVosContext) {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Global VOS context is Null", __func__);
       return;
    }
    
    /* Get the HDD context */
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
    if(!pHddCtx) {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: HDD context is Null", __func__);
       return;
    }

    hdd_abort_mac_scan(pHddCtx);
    return;
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

#ifdef WLAN_BTAMP_FEATURE
  vosStatus = WLANBAP_Close(vosContext);
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close BAP", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }
#endif // WLAN_BTAMP_FEATURE

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

  vosStatus = vos_nv_close();
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close NV", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

  vosStatus = sysClose( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close SYS", __func__);
     VOS_ASSERT( VOS_IS_STATUS_SUCCESS( vosStatus ) );
  }

 /* Let DXE return packets in WDA_close and then free them here */
  vosStatus = vos_packet_close( vosContext );
  if (!VOS_IS_STATUS_SUCCESS(vosStatus))
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: Failed to close VOSS Packet", __func__);
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
