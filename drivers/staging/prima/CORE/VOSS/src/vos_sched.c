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

/*===========================================================================
  @file vos_sched.c
  @brief VOS Scheduler Implementation

  Copyright (c) 2011 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/
/*===========================================================================
                       EDIT HISTORY FOR FILE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$ $DateTime: $ $Author: $

  when        who    what, where, why
  --------    ---    --------------------------------------------------------
===========================================================================*/
/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/
#include <vos_mq.h>
#include <vos_api.h>
#include <aniGlobal.h>
#include <sirTypes.h>
#include <halTypes.h>
#include <limApi.h>
#include <sme_Api.h>
#ifndef FEATURE_WLAN_INTEGRATED_SOC
#include <wlan_qct_ssc.h>
#endif
#include <wlan_qct_sys.h>
#include <wlan_qct_tl.h>
#include "vos_sched.h"
#include <wlan_hdd_power.h>
#ifdef FEATURE_WLAN_INTEGRATED_SOC
#include "wlan_qct_wda.h"
#include "wlan_qct_pal_msg.h"
#endif
#include <linux/spinlock.h>
#include <linux/kthread.h>
#ifndef FEATURE_WLAN_INTEGRATED_SOC
#include <libra_sdioif.h>
#include <wlan_sal_misc.h>
#endif
/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ------------------------------------------------------------------------*/
#define VOS_SCHED_THREAD_HEART_BEAT    INFINITE
/*---------------------------------------------------------------------------
 * Type Declarations
 * ------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * Data definitions
 * ------------------------------------------------------------------------*/
static pVosSchedContext gpVosSchedContext;
static pVosWatchdogContext gpVosWatchdogContext=NULL;

/*---------------------------------------------------------------------------
 * Forward declaration
 * ------------------------------------------------------------------------*/
static int VosMCThread(void * Arg);
static int VosWDThread(void * Arg);
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
/* Integrated SOC will have single BIN for FTM driver and Production Driver
 * So, anycase this must be compiled
 * None integrated SOC will compile this part only for FTM Driver */
static int VosTXThread(void * Arg);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
static int VosRXThread(void* Arg);
void vos_sched_flush_rx_mqs(pVosSchedContext SchedContext);
#endif
#endif
extern v_VOID_t vos_core_return_msg(v_PVOID_t pVContext, pVosMsgWrapper pMsgWrapper);
/*---------------------------------------------------------------------------
 * External Function implementation
 * ------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  \brief vos_sched_open() - initialize the vOSS Scheduler
  The \a vos_sched_open() function initializes the vOSS Scheduler
  Upon successful initialization:
     - All the message queues are initialized
     - The Main Controller thread is created and ready to receive and
       dispatch messages.
     - The Tx thread is created and ready to receive and dispatch messages

  \param  pVosContext - pointer to the global vOSS Context
  \param  pVosSchedContext - pointer to a previously allocated buffer big
          enough to hold a scheduler context.
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.
          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the scheduler
          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize
          the scheduler
          VOS_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
          function
          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/
  \sa vos_sched_open()
  -------------------------------------------------------------------------*/
VOS_STATUS
vos_sched_open
(
  v_PVOID_t        pVosContext,
  pVosSchedContext pSchedContext,
  v_SIZE_t         SchedCtxSize
)
{
  VOS_STATUS  vStatus = VOS_STATUS_SUCCESS;
/*-------------------------------------------------------------------------*/
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: Opening the VOSS Scheduler",__func__);
  // Sanity checks
  if ((pVosContext == NULL) || (pSchedContext == NULL)) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s: Null params being passed",__func__);
     return VOS_STATUS_E_FAILURE;
  }
  if (sizeof(VosSchedContext) != SchedCtxSize)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Incorrect VOS Sched Context size passed",__func__);
     return VOS_STATUS_E_INVAL;
  }
  vos_mem_zero(pSchedContext, sizeof(VosSchedContext));
  pSchedContext->pVContext = pVosContext;
  vStatus = vos_sched_init_mqs(pSchedContext);
  if (!VOS_IS_STATUS_SUCCESS(vStatus))
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: Failed to initialize VOS Scheduler MQs",__func__);
     return vStatus;
  }
  // Initialize the helper events and event queues
  init_completion(&pSchedContext->McStartEvent);
  init_completion(&pSchedContext->TxStartEvent);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  init_completion(&pSchedContext->RxStartEvent);
#endif
  init_completion(&pSchedContext->McShutdown);
  init_completion(&pSchedContext->TxShutdown);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  init_completion(&pSchedContext->RxShutdown);
#endif
  init_completion(&pSchedContext->ResumeMcEvent);
  init_completion(&pSchedContext->ResumeTxEvent);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  init_completion(&pSchedContext->ResumeRxEvent);
#endif

  spin_lock_init(&pSchedContext->McThreadLock);
  spin_lock_init(&pSchedContext->TxThreadLock);
  spin_lock_init(&pSchedContext->RxThreadLock);

  init_waitqueue_head(&pSchedContext->mcWaitQueue);
  pSchedContext->mcEventFlag = 0;
  init_waitqueue_head(&pSchedContext->txWaitQueue);
  pSchedContext->txEventFlag= 0;
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  init_waitqueue_head(&pSchedContext->rxWaitQueue);
  pSchedContext->rxEventFlag= 0;
#endif
  /*
  ** This initialization is critical as the threads will latter access the
  ** global contexts normally,
  **
  ** I shall put some memory barrier here after the next piece of code but
  ** I am keeping it simple for now.
  */
  gpVosSchedContext = pSchedContext;

  //Create the VOSS Main Controller thread
  pSchedContext->McThread = kthread_create(VosMCThread, pSchedContext,
                                           "VosMCThread");
  if (IS_ERR(pSchedContext->McThread)) 
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Could not Create VOSS Main Thread Controller",__func__);
     goto MC_THREAD_START_FAILURE;
  }
  wake_up_process(pSchedContext->McThread);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: VOSS Main Controller thread Created",__func__);

#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  pSchedContext->TxThread = kthread_create(VosTXThread, pSchedContext,
                                           "VosTXThread");
  if (IS_ERR(pSchedContext->TxThread)) 
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Could not Create VOSS TX Thread",__func__);
     goto TX_THREAD_START_FAILURE;
  }
  wake_up_process(pSchedContext->TxThread);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
             ("VOSS TX thread Created\n"));

#ifdef FEATURE_WLAN_INTEGRATED_SOC
  pSchedContext->RxThread = kthread_create(VosRXThread, pSchedContext,
                                           "VosRXThread");
  if (IS_ERR(pSchedContext->RxThread)) 
  {

     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Could not Create VOSS RX Thread",__func__);
     goto RX_THREAD_START_FAILURE;

  }
  wake_up_process(pSchedContext->RxThread);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
             ("VOSS RX thread Created\n"));
#endif
#endif

  /*
  ** Now make sure all threads have started before we exit.
  ** Each thread should normally ACK back when it starts.
  */
  wait_for_completion_interruptible(&pSchedContext->McStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS MC Thread has started",__func__);
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  wait_for_completion_interruptible(&pSchedContext->TxStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS Tx Thread has started",__func__);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  wait_for_completion_interruptible(&pSchedContext->RxStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS Rx Thread has started",__func__);
#endif

  /*
  ** We're good now: Let's get the ball rolling!!!
  */
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: VOSS Scheduler successfully Opened",__func__);
 #endif
  return VOS_STATUS_SUCCESS;

#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)

#ifdef FEATURE_WLAN_INTEGRATED_SOC
RX_THREAD_START_FAILURE:
    //Try and force the Tx thread controller to exit
    set_bit(MC_SHUTDOWN_EVENT_MASK, &pSchedContext->txEventFlag);
    set_bit(MC_POST_EVENT_MASK, &pSchedContext->txEventFlag);
    wake_up_interruptible(&pSchedContext->txWaitQueue);
     //Wait for TX to exit
    wait_for_completion_interruptible(&pSchedContext->TxShutdown);
#endif

TX_THREAD_START_FAILURE:
    //Try and force the Main thread controller to exit
    set_bit(MC_SHUTDOWN_EVENT_MASK, &pSchedContext->mcEventFlag);
    set_bit(MC_POST_EVENT_MASK, &pSchedContext->mcEventFlag);
    wake_up_interruptible(&pSchedContext->mcWaitQueue);
    //Wait for MC to exit
    wait_for_completion_interruptible(&pSchedContext->McShutdown);

#endif
MC_THREAD_START_FAILURE:
  //De-initialize all the message queues
  vos_sched_deinit_mqs(pSchedContext);
  return VOS_STATUS_E_RESOURCES;

} /* vos_sched_open() */

VOS_STATUS vos_watchdog_open
(
  v_PVOID_t           pVosContext,
  pVosWatchdogContext pWdContext,
  v_SIZE_t            wdCtxSize
)
{
/*-------------------------------------------------------------------------*/
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s: Opening the VOSS Watchdog module",__func__);
  //Sanity checks
  if ((pVosContext == NULL) || (pWdContext == NULL)) {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s: Null params being passed",__func__);
     return VOS_STATUS_E_FAILURE;
  }
  if (sizeof(VosWatchdogContext) != wdCtxSize)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Incorrect VOS Watchdog Context size passed",__func__);
     return VOS_STATUS_E_INVAL;
  }
  vos_mem_zero(pWdContext, sizeof(VosWatchdogContext));
  pWdContext->pVContext = pVosContext;
  gpVosWatchdogContext = pWdContext;

  //Initialize the helper events and event queues
  init_completion(&pWdContext->WdStartEvent);
  init_completion(&pWdContext->WdShutdown);
  init_waitqueue_head(&pWdContext->wdWaitQueue);
  pWdContext->wdEventFlag = 0;

  // Initialize the lock
  spin_lock_init(&pWdContext->wdLock);

  //Create the Watchdog thread
  pWdContext->WdThread = kthread_create(VosWDThread, pWdContext,"VosWDThread");
  
  if (IS_ERR(pWdContext->WdThread)) 
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Could not Create Watchdog thread",__func__);
     return VOS_STATUS_E_RESOURCES;
  }  
  else
  {
     wake_up_process(pWdContext->WdThread);
  }
 /*
  ** Now make sure thread has started before we exit.
  ** Each thread should normally ACK back when it starts.
  */
  wait_for_completion_interruptible(&pWdContext->WdStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS Watchdog Thread has started",__func__);
  return VOS_STATUS_SUCCESS;
} /* vos_watchdog_open() */
/*---------------------------------------------------------------------------
  \brief VosMcThread() - The VOSS Main Controller thread
  The \a VosMcThread() is the VOSS main controller thread:
  \param  Arg - pointer to the global vOSS Sched Context
  \return Thread exit code
  \sa VosMcThread()
  -------------------------------------------------------------------------*/
static int
VosMCThread
(
  void * Arg
)
{
  pVosSchedContext pSchedContext = (pVosSchedContext)Arg;
  pVosMsgWrapper pMsgWrapper     = NULL;
  tpAniSirGlobal pMacContext     = NULL;
  tSirRetStatus macStatus        = eSIR_SUCCESS;
  VOS_STATUS vStatus             = VOS_STATUS_SUCCESS;
  int retWaitStatus              = 0;
  v_BOOL_t shutdown              = VOS_FALSE;
  hdd_context_t *pHddCtx         = NULL;
  v_CONTEXT_t pVosContext        = NULL;

  if (Arg == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Bad Args passed", __FUNCTION__);
     return 0;
  }
  set_user_nice(current, -2);

  daemonize("MC_Thread");
  /*
  ** Ack back to the context from which the main controller thread has been
  ** created.
  */
  complete(&pSchedContext->McStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: MC Thread %d (%s) starting up",__func__, current->pid, current->comm);

  /* Get the Global VOSS Context */
  pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
  if(!pVosContext) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
     return 0;
  }

  /* Get the HDD context */
  pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
  if(!pHddCtx) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
     return 0;
  }

  while(!shutdown)
  {
    // This implements the execution model algorithm
    retWaitStatus = wait_event_interruptible(pSchedContext->mcWaitQueue,
       test_bit(MC_POST_EVENT_MASK, &pSchedContext->mcEventFlag) || 
       test_bit(MC_SUSPEND_EVENT_MASK, &pSchedContext->mcEventFlag));

    if(retWaitStatus == -ERESTARTSYS)
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: wait_event_interruptible returned -ERESTARTSYS", __FUNCTION__);
      break;
    }
    clear_bit(MC_POST_EVENT_MASK, &pSchedContext->mcEventFlag);

    while(1)
    {
      // Check if MC needs to shutdown
      if(test_bit(MC_SHUTDOWN_EVENT_MASK, &pSchedContext->mcEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: MC thread signaled to shutdown", __func__);
        shutdown = VOS_TRUE;
        /* Check for any Suspend Indication */
        if(test_bit(MC_SUSPEND_EVENT_MASK, &pSchedContext->mcEventFlag))
        {
           clear_bit(MC_SUSPEND_EVENT_MASK, &pSchedContext->mcEventFlag);
        
           /* Unblock anyone waiting on suspend */
           complete(&pHddCtx->mc_sus_event_var);
        }
        break;
      }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
      /*
      ** Check the WDI queue
      ** Service it till the entire queue is empty
      */
      if (!vos_is_mq_empty(&pSchedContext->wdiMcMq))
      {
        wpt_msg *pWdiMsg;
        /*
        ** Service the WDI message queue
        */
        VOS_TRACE(VOS_MODULE_ID_WDI, VOS_TRACE_LEVEL_INFO,
                  ("Servicing the VOS MC WDI Message queue"));

        pMsgWrapper = vos_mq_get(&pSchedContext->wdiMcMq);

        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        pWdiMsg = (wpt_msg *)pMsgWrapper->pVosMsg->bodyptr;

        if(pWdiMsg == NULL || pWdiMsg->callback == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: WDI Msg or Callback is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        pWdiMsg->callback(pWdiMsg);

        /* 
        ** return message to the Core
        */
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);

        continue;
      }

#endif
      // Check the SYS queue first
      if (!vos_is_mq_empty(&pSchedContext->sysMcMq))
      {
        // Service the SYS message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS SYS MC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->sysMcMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = sysMcProcessMsg(pSchedContext->pVContext,
           pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing SYS message",__func__);
        }
        //return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
      // Check the WDA queue
      if (!vos_is_mq_empty(&pSchedContext->wdaMcMq))
      {
        // Service the WDA message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 "%s: Servicing the VOS WDA MC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->wdaMcMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = WDA_McProcessMsg( pSchedContext->pVContext, pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing WDA message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
#else

      // Check the HAL queue
      if (!vos_is_mq_empty(&pSchedContext->halMcMq))
      {
        /* Need some optimization*/
        pMacContext = vos_get_context(VOS_MODULE_ID_HAL, pSchedContext->pVContext);

        if(pMacContext == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
              "%s: pMacContext is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        // Service the HAL message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 "%s: Servicing the VOS HAL MC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->halMcMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        macStatus = halProcessMsg( pMacContext, (tSirMsgQ*)pMsgWrapper->pVosMsg);
        if (eSIR_SUCCESS != macStatus)
        {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing HAL message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
#endif
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
      // Check the PE queue
      if (!vos_is_mq_empty(&pSchedContext->peMcMq))
      {
        // Service the PE message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS PE MC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->peMcMq);
        if (NULL == pMsgWrapper)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        /* Need some optimization*/
        pMacContext = vos_get_context(VOS_MODULE_ID_PE, pSchedContext->pVContext);
        if (NULL == pMacContext)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                     "MAC Context not ready yet");
           vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
           continue;
        }

        macStatus = peProcessMessages( pMacContext, (tSirMsgQ*)pMsgWrapper->pVosMsg);
        if (eSIR_SUCCESS != macStatus)
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing PE message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
      /** Check the SME queue **/
      if (!vos_is_mq_empty(&pSchedContext->smeMcMq))
      {
        /* Service the SME message queue */
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS SME MC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->smeMcMq);
        if (NULL == pMsgWrapper)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        /* Need some optimization*/
        pMacContext = vos_get_context(VOS_MODULE_ID_SME, pSchedContext->pVContext);
        if (NULL == pMacContext)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                     "MAC Context not ready yet");
           vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
           continue;
        }

        vStatus = sme_ProcessMsg( (tHalHandle)pMacContext, pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing SME message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
      /** Check the TL queue **/
      if (!vos_is_mq_empty(&pSchedContext->tlMcMq))
      {
        // Service the TL message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  ("Servicing the VOS TL MC Message queue"));
        pMsgWrapper = vos_mq_get(&pSchedContext->tlMcMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
              "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = WLANTL_McProcessMsg( pSchedContext->pVContext,
            pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing TL message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
      /* Check for any Suspend Indication */
      if(test_bit(MC_SUSPEND_EVENT_MASK, &pSchedContext->mcEventFlag))
      {
        clear_bit(MC_SUSPEND_EVENT_MASK, &pSchedContext->mcEventFlag);
        spin_lock(&pSchedContext->McThreadLock);

        /* Mc Thread Suspended */
        complete(&pHddCtx->mc_sus_event_var);

        INIT_COMPLETION(pSchedContext->ResumeMcEvent);
        spin_unlock(&pSchedContext->McThreadLock);

        /* Wait foe Resume Indication */
        wait_for_completion_interruptible(&pSchedContext->ResumeMcEvent);
      }
#endif /* ANI_MANF_DIAG */
      break; //All queues are empty now
    } // while message loop processing
  } // while TRUE
  // If we get here the MC thread must exit
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: MC Thread exiting!!!!", __FUNCTION__);
  complete_and_exit(&pSchedContext->McShutdown, 0);
} /* VosMCThread() */
int isWDresetInProgress(void)
{
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: Reset is in Progress...",__func__);
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
   if(gpVosWatchdogContext!=NULL)
   {
      return gpVosWatchdogContext->resetInProgress;
   }
   else
   {
      return 0;
   }
#else
   return 0;
#endif
}
/*---------------------------------------------------------------------------
  \brief VosWdThread() - The VOSS Watchdog thread
  The \a VosWdThread() is the Watchdog thread:
  \param  Arg - pointer to the global vOSS Sched Context
  \return Thread exit code
  \sa VosMcThread()
  -------------------------------------------------------------------------*/
static int
VosWDThread
(
  void * Arg
)
{
  pVosWatchdogContext pWdContext = (pVosWatchdogContext)Arg;
  int retWaitStatus              = 0;
  v_BOOL_t shutdown              = VOS_FALSE;
  VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
  set_user_nice(current, -3);

  if (Arg == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Bad Args passed", __FUNCTION__);
     return 0;
  }
  daemonize("WD_Thread");
  /*
  ** Ack back to the context from which the Watchdog thread has been
  ** created.
  */
  complete(&pWdContext->WdStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: Watchdog Thread %d (%s) starting up",__func__, current->pid, current->comm);

  while(!shutdown)
  {
    // This implements the Watchdog execution model algorithm
    retWaitStatus = wait_event_interruptible(pWdContext->wdWaitQueue,
       test_bit(WD_POST_EVENT_MASK, &pWdContext->wdEventFlag));
    if(retWaitStatus == -ERESTARTSYS)
    {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: wait_event_interruptible returned -ERESTARTSYS", __FUNCTION__);
      break;
    }
    clear_bit(WD_POST_EVENT_MASK, &pWdContext->wdEventFlag);
    while(1)
    {
      // Check if Watchdog needs to shutdown
      if(test_bit(WD_SHUTDOWN_EVENT_MASK, &pWdContext->wdEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: Watchdog thread signaled to shutdown", __func__);

        clear_bit(WD_SHUTDOWN_EVENT_MASK, &pWdContext->wdEventFlag);
        shutdown = VOS_TRUE;
        break;
      }
      /* subsystem restart: shutdown event handler */
      else if(test_bit(WD_WLAN_SHUTDOWN_EVENT_MASK, &pWdContext->wdEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Watchdog thread signaled to perform WLAN shutdown",__func__);
        clear_bit(WD_WLAN_SHUTDOWN_EVENT_MASK, &pWdContext->wdEventFlag);

        //Perform WLAN shutdown
        if(!pWdContext->resetInProgress)
        {
          pWdContext->resetInProgress = true;
          vosStatus = hdd_wlan_shutdown();

          if (! VOS_IS_STATUS_SUCCESS(vosStatus))
          {
             VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                     "%s: Failed to shutdown WLAN",__func__);
             VOS_ASSERT(0);
             goto err_reset;
          }
        }
      }
      /* subsystem restart: re-init event handler */
      else if(test_bit(WD_WLAN_REINIT_EVENT_MASK, &pWdContext->wdEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Watchdog thread signaled to perform WLAN re-init",__func__);
        clear_bit(WD_WLAN_REINIT_EVENT_MASK, &pWdContext->wdEventFlag);

        //Perform WLAN re-init
        if(!pWdContext->resetInProgress)
        {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
          "%s: Trying to do WLAN re-init when it is not shutdown !!",__func__);
        }
        vosStatus = hdd_wlan_re_init();

        if (! VOS_IS_STATUS_SUCCESS(vosStatus))
        {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                  "%s: Failed to re-init WLAN",__func__);
          VOS_ASSERT(0);
          goto err_reset;
        }
        pWdContext->resetInProgress = false;
      }
      else
      {
        //Unnecessary wakeup - Should never happen!!
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
        "%s: Watchdog thread woke up unnecessarily",__func__);
      }
      break;
    } // while message loop processing
  } // while shutdown

  // If we get here the Watchdog thread must exit
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: Watchdog Thread exiting !!!!", __FUNCTION__);
  complete_and_exit(&pWdContext->WdShutdown, 0);

err_reset:
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
      "%s: Watchdog Thread Failed to Reset, Exiting!!!!", __FUNCTION__);
    return 0;

} /* VosMCThread() */

/*---------------------------------------------------------------------------
  \brief VosTXThread() - The VOSS Main Tx thread
  The \a VosTxThread() is the VOSS main controller thread:
  \param  Arg - pointer to the global vOSS Sched Context

  \return Thread exit code
  \sa VosTxThread()
  -------------------------------------------------------------------------*/
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
static int VosTXThread ( void * Arg )
{
  pVosSchedContext pSchedContext = (pVosSchedContext)Arg;
  pVosMsgWrapper   pMsgWrapper   = NULL;
  VOS_STATUS       vStatus       = VOS_STATUS_SUCCESS;
  int              retWaitStatus = 0;
  v_BOOL_t shutdown = VOS_FALSE;
  hdd_context_t *pHddCtx         = NULL;
  v_CONTEXT_t pVosContext        = NULL;

  set_user_nice(current, -1);

  if (Arg == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s Bad Args passed", __FUNCTION__);
     return 0;
  }
  daemonize("TX_Thread");
  /*
  ** Ack back to the context from which the main controller thread has been
  ** created.
  */
  complete(&pSchedContext->TxStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: TX Thread %d (%s) starting up!",__func__, current->pid, current->comm);

  /* Get the Global VOSS Context */
  pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
  if(!pVosContext) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
     return 0;
  }

  /* Get the HDD context */
  pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
  if(!pHddCtx) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
     return 0;
  }


  while(!shutdown)
  {
    // This implements the execution model algorithm
    retWaitStatus = wait_event_interruptible(pSchedContext->txWaitQueue,
        test_bit(TX_POST_EVENT_MASK, &pSchedContext->txEventFlag) || 
        test_bit(TX_SUSPEND_EVENT_MASK, &pSchedContext->txEventFlag));


    if(retWaitStatus == -ERESTARTSYS)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: wait_event_interruptible returned -ERESTARTSYS", __FUNCTION__);
        break;
    }
    clear_bit(TX_POST_EVENT_MASK, &pSchedContext->txEventFlag);

    while(1)
    {
      if(test_bit(TX_SHUTDOWN_EVENT_MASK, &pSchedContext->txEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 "%s: TX thread signaled to shutdown", __func__);
        shutdown = VOS_TRUE;
        /* Check for any Suspend Indication */
        if(test_bit(TX_SUSPEND_EVENT_MASK, &pSchedContext->txEventFlag))
        {
           clear_bit(TX_SUSPEND_EVENT_MASK, &pSchedContext->txEventFlag);
        
           /* Unblock anyone waiting on suspend */
           complete(&pHddCtx->tx_sus_event_var);
        }
        break;
      }
      // Check the SYS queue first
      if (!vos_is_mq_empty(&pSchedContext->sysTxMq))
      {
        // Service the SYS message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: Servicing the VOS SYS TX Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->sysTxMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = sysTxProcessMsg( pSchedContext->pVContext,
                                   pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing TX SYS message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
      // Check now the TL queue
      if (!vos_is_mq_empty(&pSchedContext->tlTxMq))
      {
        // Service the TL message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: Servicing the VOS TL TX Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->tlTxMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = WLANTL_TxProcessMsg( pSchedContext->pVContext,
                                       pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing TX TL message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
      // Check the WDI queue
      if (!vos_is_mq_empty(&pSchedContext->wdiTxMq))
      {
        wpt_msg *pWdiMsg;
        VOS_TRACE(VOS_MODULE_ID_WDI, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS TX WDI Message queue",__func__);

        pMsgWrapper = vos_mq_get(&pSchedContext->wdiTxMq);

        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }

        pWdiMsg = (wpt_msg *)pMsgWrapper->pVosMsg->bodyptr;

        if(pWdiMsg == NULL || pWdiMsg->callback == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: WDI Msg or Callback is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        
        pWdiMsg->callback(pWdiMsg);

        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);

        continue;
      }
#else
      // Check the SSC queue
      if (!vos_is_mq_empty(&pSchedContext->sscTxMq))
      {
        // Service the PE message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS TX SSC Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->sscTxMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = WLANSSC_ProcessMsg( pSchedContext->pVContext,
                                      pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing TX TL message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }

#endif
      /* Check for any Suspend Indication */
      if(test_bit(TX_SUSPEND_EVENT_MASK, &pSchedContext->txEventFlag))
      {
        clear_bit(TX_SUSPEND_EVENT_MASK, &pSchedContext->txEventFlag);
        spin_lock(&pSchedContext->TxThreadLock);

        /* Tx Thread Suspended */
        complete(&pHddCtx->tx_sus_event_var);

        INIT_COMPLETION(pSchedContext->ResumeTxEvent);
        spin_unlock(&pSchedContext->TxThreadLock);

        /* Wait foe Resume Indication */
        wait_for_completion_interruptible(&pSchedContext->ResumeTxEvent);
      }

      break; //All queues are empty now
    } // while message loop processing
  } // while TRUE
  // If we get here the TX thread must exit
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: TX Thread exiting!!!!", __FUNCTION__);
  complete_and_exit(&pSchedContext->TxShutdown, 0);
} /* VosTxThread() */

#ifdef FEATURE_WLAN_INTEGRATED_SOC
/*---------------------------------------------------------------------------
  \brief VosRXThread() - The VOSS Main Rx thread
  The \a VosRxThread() is the VOSS Rx controller thread:
  \param  Arg - pointer to the global vOSS Sched Context

  \return Thread exit code
  \sa VosRxThread()
  -------------------------------------------------------------------------*/

static int VosRXThread ( void * Arg )
{
  pVosSchedContext pSchedContext = (pVosSchedContext)Arg;
  pVosMsgWrapper   pMsgWrapper   = NULL;
  int              retWaitStatus = 0;
  v_BOOL_t shutdown = VOS_FALSE;
  hdd_context_t *pHddCtx         = NULL;
  v_CONTEXT_t pVosContext        = NULL;
  VOS_STATUS       vStatus       = VOS_STATUS_SUCCESS;

  set_user_nice(current, -1);

  if (Arg == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s Bad Args passed", __FUNCTION__);
     return 0;
  }
  daemonize("RX_Thread");
  /*
  ** Ack back to the context from which the main controller thread has been
  ** created.
  */
  complete(&pSchedContext->RxStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: RX Thread %d (%s) starting up!",__func__, current->pid, current->comm);

  /* Get the Global VOSS Context */
  pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
  if(!pVosContext) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: Global VOS context is Null", __func__);
     return 0;
  }

  /* Get the HDD context */
  pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
  if(!pHddCtx) {
     hddLog(VOS_TRACE_LEVEL_FATAL,"%s: HDD context is Null",__func__);
     return 0;
  }

  while(!shutdown)
  {
    // This implements the execution model algorithm
    retWaitStatus = wait_event_interruptible(pSchedContext->rxWaitQueue,
        test_bit(RX_POST_EVENT_MASK, &pSchedContext->rxEventFlag) || 
        test_bit(RX_SUSPEND_EVENT_MASK, &pSchedContext->rxEventFlag));


    if(retWaitStatus == -ERESTARTSYS)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: wait_event_interruptible returned -ERESTARTSYS", __FUNCTION__);
        break;
    }
    clear_bit(RX_POST_EVENT_MASK, &pSchedContext->rxEventFlag);

    while(1)
    {
      if(test_bit(RX_SHUTDOWN_EVENT_MASK, &pSchedContext->rxEventFlag))
      {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                 "%s: RX thread signaled to shutdown", __func__);
        shutdown = VOS_TRUE;
        /* Check for any Suspend Indication */
        if(test_bit(RX_SUSPEND_EVENT_MASK, &pSchedContext->rxEventFlag))
        {
           clear_bit(RX_SUSPEND_EVENT_MASK, &pSchedContext->rxEventFlag);
        
           /* Unblock anyone waiting on suspend */
           complete(&pHddCtx->rx_sus_event_var);
        }
        break;
      }


      // Check the SYS queue first
      if (!vos_is_mq_empty(&pSchedContext->sysRxMq))
      {
        // Service the SYS message queue
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                "%s: Servicing the VOS SYS RX Message queue",__func__);
        pMsgWrapper = vos_mq_get(&pSchedContext->sysRxMq);
        if (pMsgWrapper == NULL)
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
               "%s: pMsgWrapper is NULL", __FUNCTION__);
           VOS_ASSERT(0);
           break;
        }
        vStatus = sysRxProcessMsg( pSchedContext->pVContext,
                                   pMsgWrapper->pVosMsg);
        if (!VOS_IS_STATUS_SUCCESS(vStatus))
        {
          VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                     "%s: Issue Processing TX SYS message",__func__);
        }
        // return message to the Core
        vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        continue;
      }

      // Check the WDI queue
      if (!vos_is_mq_empty(&pSchedContext->wdiRxMq))
      {
        wpt_msg *pWdiMsg;
        VOS_TRACE(VOS_MODULE_ID_WDI, VOS_TRACE_LEVEL_INFO,
                  "%s: Servicing the VOS RX WDI Message queue",__func__);

        pMsgWrapper = vos_mq_get(&pSchedContext->wdiRxMq);
        if ((NULL == pMsgWrapper) || (NULL == pMsgWrapper->pVosMsg))
        {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: wdiRxMq message is NULL", __FUNCTION__);
          VOS_ASSERT(0);
          // we won't return this wrapper since it is corrupt
        }
        else
        {
          pWdiMsg = (wpt_msg *)pMsgWrapper->pVosMsg->bodyptr;
          if ((NULL == pWdiMsg) || (NULL == pWdiMsg->callback))
          {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s: WDI Msg or callback is NULL", __FUNCTION__);
            VOS_ASSERT(0);
          }
          else
          {
            // invoke the message handler
            pWdiMsg->callback(pWdiMsg);
          }

          // return message to the Core
          vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
        }
        continue;
      }

      /* Check for any Suspend Indication */
      if(test_bit(RX_SUSPEND_EVENT_MASK, &pSchedContext->rxEventFlag))
      {
        clear_bit(RX_SUSPEND_EVENT_MASK, &pSchedContext->rxEventFlag);
        spin_lock(&pSchedContext->RxThreadLock);

        /* Rx Thread Suspended */
        complete(&pHddCtx->rx_sus_event_var);

        init_completion(&pSchedContext->ResumeRxEvent);
        spin_unlock(&pSchedContext->RxThreadLock);

        /* Wait for Resume Indication */
        wait_for_completion_interruptible(&pSchedContext->ResumeRxEvent);
      }

      break; //All queues are empty now
    } // while message loop processing
  } // while TRUE
  // If we get here the RX thread must exit
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
      "%s: RX Thread exiting!!!!", __FUNCTION__);
  complete_and_exit(&pSchedContext->RxShutdown, 0);
} /* VosRxThread() */
#endif

#endif
/*---------------------------------------------------------------------------
  \brief vos_sched_close() - Close the vOSS Scheduler
  The \a vos_sched_closes() function closes the vOSS Scheduler
  Upon successful closing:
     - All the message queues are flushed
     - The Main Controller thread is closed
     - The Tx thread is closed

  \param  pVosContext - pointer to the global vOSS Context
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.
          VOS_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
          function
          VOS_STATUS_E_FAILURE - Failure to initialize the scheduler/
  \sa vos_sched_close()
---------------------------------------------------------------------------*/
VOS_STATUS vos_sched_close ( v_PVOID_t pVosContext )
{
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
        "%s: invoked", __FUNCTION__);
    if (gpVosSchedContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: gpVosSchedContext == NULL\n",__FUNCTION__);
       return VOS_STATUS_E_FAILURE;
    }

    // shut down MC Thread
    set_bit(MC_SHUTDOWN_EVENT_MASK, &gpVosSchedContext->mcEventFlag);
    set_bit(MC_POST_EVENT_MASK, &gpVosSchedContext->mcEventFlag);
    wake_up_interruptible(&gpVosSchedContext->mcWaitQueue);
    //Wait for MC to exit
    wait_for_completion_interruptible(&gpVosSchedContext->McShutdown);
    gpVosSchedContext->McThread = 0;

#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
    // shut down TX Thread
    set_bit(TX_SHUTDOWN_EVENT_MASK, &gpVosSchedContext->txEventFlag);
    set_bit(TX_POST_EVENT_MASK, &gpVosSchedContext->txEventFlag);
    wake_up_interruptible(&gpVosSchedContext->txWaitQueue);
    //Wait for TX to exit
    wait_for_completion_interruptible(&gpVosSchedContext->TxShutdown);
    gpVosSchedContext->TxThread = 0;

#ifdef FEATURE_WLAN_INTEGRATED_SOC
    // shut down RX Thread
    set_bit(RX_SHUTDOWN_EVENT_MASK, &gpVosSchedContext->rxEventFlag);
    set_bit(RX_POST_EVENT_MASK, &gpVosSchedContext->rxEventFlag);
    wake_up_interruptible(&gpVosSchedContext->rxWaitQueue);
    //Wait for RX to exit
    wait_for_completion_interruptible(&gpVosSchedContext->RxShutdown);
    gpVosSchedContext->RxThread = 0;
#endif // FEATURE_WLAN_INTEGRATED_SOC
#endif // !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)

    //Clean up message queues of TX and MC thread
    vos_sched_flush_mc_mqs(gpVosSchedContext);
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
    vos_sched_flush_tx_mqs(gpVosSchedContext);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
    vos_sched_flush_rx_mqs(gpVosSchedContext);
#endif // FEATURE_WLAN_INTEGRATED_SOC
#endif // !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)

    //Deinit all the queues
    vos_sched_deinit_mqs(gpVosSchedContext);

    return VOS_STATUS_SUCCESS;
} /* vox_sched_close() */

VOS_STATUS vos_watchdog_close ( v_PVOID_t pVosContext )
{
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
        "%s: vos_watchdog closing now", __FUNCTION__);
    if (gpVosWatchdogContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: gpVosWatchdogContext is NULL\n",__FUNCTION__);
       return VOS_STATUS_E_FAILURE;
    }
    set_bit(WD_SHUTDOWN_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    set_bit(WD_POST_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    wake_up_interruptible(&gpVosWatchdogContext->wdWaitQueue);
    //Wait for Watchdog thread to exit
    wait_for_completion_interruptible(&gpVosWatchdogContext->WdShutdown);
    return VOS_STATUS_SUCCESS;
} /* vos_watchdog_close() */

VOS_STATUS vos_watchdog_chip_reset ( vos_chip_reset_reason_type  reason )
{
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;
    hdd_chip_reset_stats_t *pResetStats;
    struct sdio_func *sdio_func_dev = NULL;
    
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
        "%s: vos_watchdog resetting WLAN", __FUNCTION__);
    if (gpVosWatchdogContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Watchdog not enabled. LOGP ignored.",__FUNCTION__);
       return VOS_STATUS_E_FAILURE;
    }

    pVosContext = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );

    hdd_reset_all_adapters(pHddCtx);

    sdio_func_dev = libra_getsdio_funcdev();

    if(sdio_func_dev == NULL)
    {
         /* Our card got removed before LOGP. Continue with reset anyways */
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: sdio_func_dev is NULL!",__func__);
         return VOS_STATUS_SUCCESS;
    }

    sd_claim_host(sdio_func_dev);
    
    /* Disable SDIO IRQ since we are in LOGP state */
    libra_disable_sdio_irq_capability(sdio_func_dev, 1);
    libra_enable_sdio_irq(sdio_func_dev, 0);

    sd_release_host(sdio_func_dev);

    /* Take the lock here */
    spin_lock(&gpVosWatchdogContext->wdLock);

    if (gpVosWatchdogContext->resetInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: Reset already in Progress. Ignoring signaling Watchdog",
                                                           __FUNCTION__);
        /* Release the lock here */
        spin_unlock(&gpVosWatchdogContext->wdLock);
        return VOS_STATUS_E_FAILURE;
    } 
    else if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: LOGP already in Progress. Ignoring signaling Watchdog",
                                                           __FUNCTION__);
        /* Release the lock here */
        spin_unlock(&gpVosWatchdogContext->wdLock);
        return VOS_STATUS_E_FAILURE;
    }

    VOS_ASSERT(0);
    
    pVosContext = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);
    pAdapter = (hdd_adapter_t *)vos_get_context(VOS_MODULE_ID_HDD,pVosContext);

    /* Set the flags so that all future CMD53 and Wext commands get blocked right away */
    vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);
    pHddCtx->isLogpInProgress = TRUE;

    /* Release the lock here */
    spin_unlock(&gpVosWatchdogContext->wdLock);

    if (pHddCtx->isLoadUnloadInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: Load/unload in Progress. Ignoring signaling Watchdog",
                                                          __FUNCTION__);
        return VOS_STATUS_E_FAILURE;    
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    if(VOS_STATUS_SUCCESS != hdd_wlan_reset_initialization())
    {
       /* This can fail if card got removed by SDCC during resume */
       VOS_ASSERT(0);
    }
#endif

    /* Update Reset Statistics */
    pResetStats = &pHddCtx->hddChipResetStats;
    pResetStats->totalLogpResets++;

    switch (reason)
    {
     case VOS_CHIP_RESET_CMD53_FAILURE:
        pResetStats->totalCMD53Failures++;
        break;
     case VOS_CHIP_RESET_FW_EXCEPTION:
        pResetStats->totalFWHearbeatFailures++;
        break;
     case VOS_CHIP_RESET_MUTEX_READ_FAILURE:
        pResetStats->totalMutexReadFailures++;
        break;
     case VOS_CHIP_RESET_MIF_EXCEPTION:
        pResetStats->totalMIFErrorFailures++;
        break;
     default:
        pResetStats->totalUnknownExceptions++;
        break;
    }

    set_bit(WD_CHIP_RESET_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    set_bit(WD_POST_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    wake_up_interruptible(&gpVosWatchdogContext->wdWaitQueue);
#endif
    return VOS_STATUS_SUCCESS;
} /* vos_watchdog_chip_reset() */

/*---------------------------------------------------------------------------
  \brief vos_sched_init_mqs: Initialize the vOSS Scheduler message queues
  The \a vos_sched_init_mqs() function initializes the vOSS Scheduler
  message queues.
  \param  pVosSchedContext - pointer to the Scheduler Context.
  \return VOS_STATUS_SUCCESS - Scheduler was successfully initialized and
          is ready to be used.
          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the scheduler

  \sa vos_sched_init_mqs()
  -------------------------------------------------------------------------*/
VOS_STATUS vos_sched_init_mqs ( pVosSchedContext pSchedContext )
{
  VOS_STATUS vStatus = VOS_STATUS_SUCCESS;
  // Now intialize all the message queues
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the WDA MC Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->wdaMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init WDA MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#else
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the HAL MC Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->halMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init HAL MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#endif
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the PE MC Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->peMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init PE MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the SME MC Message queue", __func__);
  vStatus = vos_mq_init(&pSchedContext->smeMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init SME MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the TL MC Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->tlMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init TL MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#endif
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the SYS MC Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->sysMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init SYS MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the WDI MC Message queue",__func__);

  vStatus = vos_mq_init(&pSchedContext->wdiMcMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init WDI MC Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#endif

#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the TL Tx Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->tlTxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init TL TX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the SSC Tx Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->sscTxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init SSC TX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#else
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the WDI Tx Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->wdiTxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init WDI TX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }

  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the WDI Rx Message queue",__func__);

  vStatus = vos_mq_init(&pSchedContext->wdiRxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init WDI RX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }

#endif
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: Initializing the SYS Tx Message queue",__func__);
  vStatus = vos_mq_init(&pSchedContext->sysTxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init SYS TX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }

  vStatus = vos_mq_init(&pSchedContext->sysRxMq);
  if (! VOS_IS_STATUS_SUCCESS(vStatus))
  {
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
            "%s: Failed to init SYS RX Message queue",__func__);
    VOS_ASSERT(0);
    return vStatus;
  }
#endif
  return VOS_STATUS_SUCCESS;
} /* vos_sched_init_mqs() */

/*---------------------------------------------------------------------------
  \brief vos_sched_deinit_mqs: Deinitialize the vOSS Scheduler message queues
  The \a vos_sched_init_mqs() function deinitializes the vOSS Scheduler
  message queues.
  \param  pVosSchedContext - pointer to the Scheduler Context.
  \return None
  \sa vos_sched_deinit_mqs()
  -------------------------------------------------------------------------*/
void vos_sched_deinit_mqs ( pVosSchedContext pSchedContext )
{
  // Now de-intialize all message queues
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  // MC HAL
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the HAL MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->halMcMq);
#else
 // MC WDA
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the WDA MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->wdaMcMq);
#endif
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  //MC PE
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the PE MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->peMcMq);
  //MC SME
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the SME MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->smeMcMq);
  //MC TL
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the TL MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->tlMcMq);
#endif
  //MC SYS
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the SYS MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->sysMcMq);
#ifdef FEATURE_WLAN_INTEGRATED_SOC
  // MC WDI
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the WDI MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->wdiMcMq);
#endif

#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  //Tx TL
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the TL Tx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->tlTxMq);
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  //Tx SSC
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: DeInitializing the SSC Tx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->sscTxMq);

#else
  //Tx WDI
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: DeInitializing the WDI Tx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->wdiTxMq);


  //Rx WDI
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: DeInitializing the WDI Rx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->wdiRxMq);
#endif

  //Tx SYS
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: DeInitializing the SYS Tx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->sysTxMq);

  //Rx SYS
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: DeInitializing the SYS Rx Message queue",__func__);
  vos_mq_deinit(&pSchedContext->sysRxMq);

#endif
} /* vos_sched_deinit_mqs() */

/*-------------------------------------------------------------------------
 this helper function flushes all the MC message queues
 -------------------------------------------------------------------------*/
void vos_sched_flush_mc_mqs ( pVosSchedContext pSchedContext )
{
  pVosMsgWrapper pMsgWrapper = NULL;
  pVosContextType vosCtx;

  /*
  ** Here each of the MC thread MQ shall be drained and returned to the
  ** Core. Before returning a wrapper to the Core, the VOS message shall be
  ** freed  first
  */
  VOS_TRACE( VOS_MODULE_ID_VOSS,
             VOS_TRACE_LEVEL_INFO,
             ("Flushing the MC Thread message queue\n") );

  if (NULL == pSchedContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pSchedContext is NULL", __FUNCTION__);
     return;
  }

  vosCtx = (pVosContextType)(pSchedContext->pVContext);
  if (NULL == vosCtx)
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: vosCtx is NULL", __FUNCTION__);
     return;
  }

  /* Flush the SYS Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->sysMcMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing MC SYS message type %d ",__func__,
               pMsgWrapper->pVosMsg->type );
    sysMcFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  /* Flush the HAL Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->halMcMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing MC HAL MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    halFreeMsg(vosCtx->pMACContext, (tSirMsgQ*)pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
#else
  /* Flush the WDA Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->wdaMcMq) ))
  {
    if(pMsgWrapper->pVosMsg != NULL) 
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   "%s: Freeing MC WDA MSG message type %d",
                   __FUNCTION__, pMsgWrapper->pVosMsg->type );
        if (pMsgWrapper->pVosMsg->bodyptr) {
            vos_mem_free((v_VOID_t*)pMsgWrapper->pVosMsg->bodyptr);
        }

        pMsgWrapper->pVosMsg->bodyptr = NULL;
        pMsgWrapper->pVosMsg->bodyval = 0;
        pMsgWrapper->pVosMsg->type = 0;
    }
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }

  /* Flush the WDI Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->wdiMcMq) ))
  {
    if(pMsgWrapper->pVosMsg != NULL)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   "%s: Freeing MC WDI MSG message type %d",
                   __FUNCTION__, pMsgWrapper->pVosMsg->type );
        if (pMsgWrapper->pVosMsg->bodyptr) {
            vos_mem_free((v_VOID_t*)pMsgWrapper->pVosMsg->bodyptr);
        }

        pMsgWrapper->pVosMsg->bodyptr = NULL;
        pMsgWrapper->pVosMsg->bodyval = 0;
        pMsgWrapper->pVosMsg->type = 0;
    }
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }

#endif
#if !defined(ANI_MANF_DIAG) || defined(FEATURE_WLAN_INTEGRATED_SOC)
  /* Flush the PE Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->peMcMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing MC PE MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    peFreeMsg(vosCtx->pMACContext, (tSirMsgQ*)pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
  /* Flush the SME Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->smeMcMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing MC SME MSG message type %d", __func__,
               pMsgWrapper->pVosMsg->type );
    sme_FreeMsg(vosCtx->pMACContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
    /* Flush the TL Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->tlMcMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing MC TL message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    WLANTL_McFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
#endif
} /* vos_sched_flush_mc_mqs() */

/*-------------------------------------------------------------------------
 This helper function flushes all the TX message queues
 ------------------------------------------------------------------------*/
void vos_sched_flush_tx_mqs ( pVosSchedContext pSchedContext )
{
  pVosMsgWrapper pMsgWrapper = NULL;
  /*
  ** Here each of the TX thread MQ shall be drained and returned to the
  ** Core. Before returning a wrapper to the Core, the VOS message shall
  ** be freed first
  */
  VOS_TRACE( VOS_MODULE_ID_VOSS,
             VOS_TRACE_LEVEL_INFO,
             "%s: Flushing the TX Thread message queue",__func__);

  if (NULL == pSchedContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pSchedContext is NULL", __FUNCTION__);
     return;
  }

  /* Flush the SYS Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->sysTxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing TX SYS message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    sysTxFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
  /* Flush the TL Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->tlTxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing TX TL MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    WLANTL_TxFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
#ifndef FEATURE_WLAN_INTEGRATED_SOC
  /* Flush the SSC Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->sscTxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing TX SSC MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    WLANSSC_FreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
#else
  /* Flush the WDI Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->wdiTxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing TX WDI MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    sysTxFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }
 #endif
} /* vos_sched_flush_tx_mqs() */
#ifdef FEATURE_WLAN_INTEGRATED_SOC
/*-------------------------------------------------------------------------
 This helper function flushes all the RX message queues
 ------------------------------------------------------------------------*/
void vos_sched_flush_rx_mqs ( pVosSchedContext pSchedContext )
{
  pVosMsgWrapper pMsgWrapper = NULL;
  /*
  ** Here each of the RX thread MQ shall be drained and returned to the
  ** Core. Before returning a wrapper to the Core, the VOS message shall
  ** be freed first
  */
  VOS_TRACE( VOS_MODULE_ID_VOSS,
             VOS_TRACE_LEVEL_INFO,
             "%s: Flushing the RX Thread message queue",__func__);

  if (NULL == pSchedContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pSchedContext is NULL", __FUNCTION__);
     return;
  }

  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->wdiRxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing RX WDI MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    sysTxFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
  }

  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->sysRxMq) ))
  {
    VOS_TRACE( VOS_MODULE_ID_VOSS,
               VOS_TRACE_LEVEL_INFO,
               "%s: Freeing RX SYS MSG message type %d",__func__,
               pMsgWrapper->pVosMsg->type );
    sysTxFreeMsg(pSchedContext->pVContext, pMsgWrapper->pVosMsg);
  }

}/* vos_sched_flush_rx_mqs() */
#endif

/*-------------------------------------------------------------------------
 This helper function helps determine if thread id is of TX thread
 ------------------------------------------------------------------------*/
int vos_sched_is_tx_thread(int threadID)
{
   // Make sure that Vos Scheduler context has been initialized
   VOS_ASSERT( NULL != gpVosSchedContext);
   if (gpVosSchedContext == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: gpVosSchedContext == NULL",__FUNCTION__);
      return 0;
   }
   return ((gpVosSchedContext->TxThread) && (threadID == gpVosSchedContext->TxThread->pid));
}
#ifdef FEATURE_WLAN_INTEGRATED_SOC
/*-------------------------------------------------------------------------
 This helper function helps determine if thread id is of RX thread
 ------------------------------------------------------------------------*/
int vos_sched_is_rx_thread(int threadID)
{
   // Make sure that Vos Scheduler context has been initialized
   VOS_ASSERT( NULL != gpVosSchedContext);
   if (gpVosSchedContext == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
          "%s: gpVosSchedContext == NULL",__FUNCTION__);
      return 0;
   }
   return ((gpVosSchedContext->RxThread) && (threadID == gpVosSchedContext->RxThread->pid));
}
#endif
/*-------------------------------------------------------------------------
 Helper function to get the scheduler context
 ------------------------------------------------------------------------*/
pVosSchedContext get_vos_sched_ctxt(void)
{
   //Make sure that Vos Scheduler context has been initialized
   if (gpVosSchedContext == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosSchedContext == NULL",__FUNCTION__);
   }
   return (gpVosSchedContext);
}
/*-------------------------------------------------------------------------
 Helper function to get the watchdog context
 ------------------------------------------------------------------------*/
pVosWatchdogContext get_vos_watchdog_ctxt(void)
{
   //Make sure that Vos Scheduler context has been initialized
   if (gpVosWatchdogContext == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosWatchdogContext == NULL",__FUNCTION__);
   }
   return (gpVosWatchdogContext);
}
/**
  @brief vos_watchdog_wlan_shutdown()

  This function is called to shutdown WLAN driver during SSR.
  Adapters are disabled, and the watchdog task will be signalled
  to shutdown WLAN driver.

  @param
         NONE
  @return
         VOS_STATUS_SUCCESS   - Operation completed successfully.
         VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_watchdog_wlan_shutdown(void)
{
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx = NULL;

    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
        "%s: WLAN driver is shutting down ", __FUNCTION__);
    if (NULL == gpVosWatchdogContext)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Watchdog not enabled. LOGP ignored.", __FUNCTION__);
       return VOS_STATUS_E_FAILURE;
    }

    pVosContext = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
    if (NULL == pHddCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Invalid HDD Context", __FUNCTION__);
       return VOS_STATUS_E_FAILURE;
    }

    /* Take the lock here */
    spin_lock(&gpVosWatchdogContext->wdLock);

    /* reuse the existing 'reset in progress' */
    if (gpVosWatchdogContext->resetInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: Shutdown already in Progress. Ignoring signaling Watchdog",
                                                           __FUNCTION__);
        /* Release the lock here */
        spin_unlock(&gpVosWatchdogContext->wdLock);
        return VOS_STATUS_E_FAILURE;
    }
    /* reuse the existing 'logp in progress', eventhough it is not
     * exactly the same */
    else if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: shutdown/re-init already in Progress. Ignoring signaling Watchdog",
                                                           __FUNCTION__);
        /* Release the lock here */
        spin_unlock(&gpVosWatchdogContext->wdLock);
        return VOS_STATUS_E_FAILURE;
    } 

    /* Set the flags so that all future CMD53 and Wext commands get blocked right away */
    vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);
    pHddCtx->isLogpInProgress = TRUE;

    /* Release the lock here */
    spin_unlock(&gpVosWatchdogContext->wdLock);

    if (pHddCtx->isLoadUnloadInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Load/unload in Progress. Ignoring signaling Watchdog",
                __FUNCTION__);
        return VOS_STATUS_E_FAILURE;
    }
    /* Update Riva Reset Statistics */
    pHddCtx->hddRivaResetStats++;
#ifdef CONFIG_HAS_EARLYSUSPEND
    if(VOS_STATUS_SUCCESS != hdd_wlan_reset_initialization())
    {
       VOS_ASSERT(0);
    }
#endif

    set_bit(WD_WLAN_SHUTDOWN_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    set_bit(WD_POST_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    wake_up_interruptible(&gpVosWatchdogContext->wdWaitQueue);

    return VOS_STATUS_SUCCESS;
}

/**
  @brief vos_watchdog_wlan_re_init()

  This function is called to re-initialize WLAN driver, and this is
  called when Riva SS reboots.

  @param
         NONE
  @return
         VOS_STATUS_SUCCESS   - Operation completed successfully.
         VOS_STATUS_E_FAILURE - Operation failed.

*/
VOS_STATUS vos_watchdog_wlan_re_init(void)
{
    /* watchdog task is still running, it is not closed in shutdown */
    set_bit(WD_WLAN_REINIT_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    set_bit(WD_POST_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    wake_up_interruptible(&gpVosWatchdogContext->wdWaitQueue);

    return VOS_STATUS_SUCCESS;
}
