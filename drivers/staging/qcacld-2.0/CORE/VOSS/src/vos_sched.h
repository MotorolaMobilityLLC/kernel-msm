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

#if !defined( __VOS_SCHED_H )
#define __VOS_SCHED_H

/**=========================================================================

  \file  vos_sched.h

  \brief virtual Operating System Servies (vOSS)

   Definitions for some of the internal data type that is internally used
   by the vOSS scheduler on Windows Mobile.

   This file defines a vOSS message queue on Win Mobile and give some
   insights about how the scheduler implements the execution model supported
   by vOSS.

  ========================================================================*/

/*===========================================================================

                       EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


  when        who    what, where, why
  --------    ---    --------------------------------------------------------
  09/15/08    lac    Removed hardcoded #define for VOS_TRACE.
  06/12/08    hba    Created module.

===========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_event.h>
#include "i_vos_types.h"
#include <linux/wait.h>
#if defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#include <vos_mq.h>
#include <adf_os_types.h>
#include <vos_lock.h>

#define TX_POST_EVENT_MASK               0x001
#define TX_SUSPEND_EVENT_MASK            0x002
#define MC_POST_EVENT_MASK               0x001
#define MC_SUSPEND_EVENT_MASK            0x002
#define RX_POST_EVENT_MASK               0x001
#define RX_SUSPEND_EVENT_MASK            0x002
#define TX_SHUTDOWN_EVENT_MASK           0x010
#define MC_SHUTDOWN_EVENT_MASK           0x010
#define RX_SHUTDOWN_EVENT_MASK           0x010
#define WD_POST_EVENT_MASK               0x001
#define WD_SHUTDOWN_EVENT_MASK           0x002
#define WD_CHIP_RESET_EVENT_MASK         0x004
#define WD_WLAN_SHUTDOWN_EVENT_MASK      0x008
#define WD_WLAN_REINIT_EVENT_MASK        0x010



/*
** Maximum number of messages in the system
** These are buffers to account for all current messages
** with some accounting of what we think is a
** worst-case scenario.  Must be able to handle all
** incoming frames, as well as overhead for internal
** messaging
**
** Increased to 8000 to handle more RX frames
*/
#define VOS_CORE_MAX_MESSAGES 8000

#ifdef QCA_CONFIG_SMP
/*
** Maximum number of vos messages to be allocated for
** Tlshim Rx thread.
*/
#define VOSS_MAX_TLSHIM_PKT 4000

typedef void (*vos_tlshim_cb) (void *context, void *rxpkt, u_int16_t staid);
#endif

/*
** vOSS Message queue definition.
*/
typedef struct _VosMqType
{
  /* Lock use to synchronize access to this message queue */
  spinlock_t       mqLock;

  /* List of vOS Messages waiting on this queue */
  struct list_head  mqList;

} VosMqType, *pVosMqType;

#ifdef QCA_CONFIG_SMP
/*
** VOSS message wrapper for data rx from Tlshim.
*/
typedef struct VosTlshimPkt
{
   struct list_head list;

   /* Tlshim context */
   void *context;

   /* Rx skb */
   void *Rxpkt;

   /* Station id to which this packet is destined */
   u_int16_t staId;

   /* Call back to further send this packet to tlshim layer */
   vos_tlshim_cb callback;

} *pVosTlshimPkt;
#endif

/*
** vOSS Scheduler context
** The scheduler context contains the following:
**   ** the messages queues
**   ** the handle to the tread
**   ** pointer to the events that gracefully shutdown the MC and Tx threads
**
*/
typedef struct _VosSchedContext
{
  /* Place holder to the VOSS Context */
   v_PVOID_t           pVContext;
  /* WDA Message queue on the Main thread*/
   VosMqType           wdaMcMq;



   /* PE Message queue on the Main thread*/
   VosMqType           peMcMq;

   /* SME Message queue on the Main thread*/
   VosMqType           smeMcMq;

   /* TL Message queue on the Main thread */
   VosMqType           tlMcMq;

   /* SYS Message queue on the Main thread */
   VosMqType           sysMcMq;

   /* Handle of Event for MC thread to signal startup */
   struct completion   McStartEvent;

   struct task_struct* McThread;


   /* completion object for MC thread shutdown */
   struct completion   McShutdown;

   /* Wait queue for MC thread */
   wait_queue_head_t mcWaitQueue;

   unsigned long     mcEventFlag;

   /* Completion object to resume Mc thread */
   struct completion ResumeMcEvent;

   /* lock to make sure that McThread and TxThread Suspend/resume mechanism is in sync*/
   spinlock_t McThreadLock;
#ifdef QCA_CONFIG_SMP
   spinlock_t TlshimRxThreadLock;

   /* Tlshim Rx thread handle */
   struct task_struct *TlshimRxThread;

   /* Handle of Event for Rx thread to signal startup */
   struct completion TlshimRxStartEvent;

   /* Completion object to suspend tlshim rx thread */
   struct completion SuspndTlshimRxEvent;

   /* Completion objext to resume tlshim rx thread */
   struct completion ResumeTlshimRxEvent;

   /* Completion object for Tlshim Rxthread shutdown */
   struct completion TlshimRxShutdown;

   /* Waitq for tlshim Rx thread */
   wait_queue_head_t tlshimRxWaitQueue;

   unsigned long tlshimRxEvtFlg;

   /* Rx buffer queue */
   struct list_head tlshimRxQueue;

   /* Spinlock to synchronize between tasklet and thread */
   spinlock_t TlshimRxQLock;

   /* Rx queue length */
   unsigned int TlshimRxQlen;

   /* Lock to synchronize free buffer queue access */
   spinlock_t VosTlshimPktFreeQLock;

   /* Free message queue for Tlshim Rx processing */
   struct list_head VosTlshimPktFreeQ;

   /* cpu hotplug notifier */
   struct notifier_block *cpuHotPlugNotifier;

   /* affinity lock */
   vos_lock_t affinity_lock;

   /* rx thread affinity cpu */
   unsigned long rx_thread_cpu;

   /* high throughput required */
   bool high_throughput_required;
#endif
} VosSchedContext, *pVosSchedContext;

/*
** VOSS watchdog context
** The watchdog context contains the following:
** The messages queues and events
** The handle to the thread
**
*/
typedef struct _VosWatchdogContext
{

   /* Place holder to the VOSS Context */
   v_PVOID_t pVContext;

   /* Handle of Event for Watchdog thread to signal startup */
   struct completion WdStartEvent;

   /* Watchdog Thread handle */

   struct task_struct* WdThread;

   /* completion object for Watchdog thread shutdown */
   struct completion WdShutdown;

   /* Wait queue for Watchdog thread */
   wait_queue_head_t wdWaitQueue;

   /* Event flag for events handled by Watchdog */
   unsigned long wdEventFlag;

   v_BOOL_t resetInProgress;

   /* Lock for preventing multiple reset being triggered simultaneously */
   spinlock_t wdLock;

} VosWatchdogContext, *pVosWatchdogContext;

/*
** vOSS Sched Msg Wrapper
** Wrapper messages so that they can be chained to their respective queue
** in the scheduler.
*/
typedef struct _VosMsgWrapper
{
   /* Message node */
   struct list_head  msgNode;

   /* the Vos message it is associated to */
   vos_msg_t    *pVosMsg;

} VosMsgWrapper, *pVosMsgWrapper;

/**
 * struct vos_log_complete - Log completion internal structure
 * @is_fatal: Type is fatal or not
 * @indicator: Source of bug report
 * @reason_code: Reason code for bug report
 * @is_report_in_progress: If bug report is in progress
 *
 * This structure internally stores the log related params
 */
struct vos_log_complete {
	uint32_t is_fatal;
	uint32_t indicator;
	uint32_t reason_code;
	bool is_report_in_progress;
};

typedef struct _VosContextType
{
   /* Messages buffers */
   vos_msg_t           aMsgBuffers[VOS_CORE_MAX_MESSAGES];

   VosMsgWrapper       aMsgWrappers[VOS_CORE_MAX_MESSAGES];

   /* Free Message queue*/
   VosMqType           freeVosMq;

   /* Scheduler Context */
   VosSchedContext     vosSched;

   /* Watchdog Context */
   VosWatchdogContext  vosWatchdog;

   /* HDD Module Context  */
   v_VOID_t           *pHDDContext;

   /* TL Module Context  */
   v_VOID_t           *pTLContext;

   /* MAC Module Context  */
   v_VOID_t           *pMACContext;

#ifndef WLAN_FEATURE_MBSSID
   /* SAP Context */
   v_VOID_t           *pSAPContext;
#endif

   vos_event_t         ProbeEvent;

   volatile v_U8_t     isLogpInProgress;

   vos_event_t         wdaCompleteEvent;

   /* WDA Context */
   v_VOID_t            *pWDAContext;

   v_VOID_t        *pHIFContext;

   v_VOID_t        *htc_ctx;

   /*
    * adf_ctx will be used by adf
    * while allocating dma memory
    * to access dev information.
    */
   adf_os_device_t adf_ctx;

   v_VOID_t        *pdev_txrx_ctx;

   /* Configuration handle used to get system configuration */
   v_VOID_t    *cfg_ctx;

   volatile v_U8_t    isLoadUnloadInProgress;
   volatile v_U8_t    is_load_in_progress;
   volatile v_U8_t    is_unload_in_progress;

   /* SSR re-init in progress */
   volatile v_U8_t     isReInitInProgress;

   bool is_wakelock_log_enabled;
   uint32_t wakelock_log_level;
   uint32_t connectivity_log_level;
   uint32_t packet_stats_log_level;
   uint32_t driver_debug_log_level;
   uint32_t fw_debug_log_level;

   struct vos_log_complete log_complete;
   vos_spin_lock_t bug_report_lock;

   bool crash_indication_pending;
} VosContextType, *pVosContextType;



/*---------------------------------------------------------------------------
  Function declarations and documenation
---------------------------------------------------------------------------*/

#ifdef QCA_CONFIG_SMP
int vos_sched_handle_cpu_hot_plug(void);
int vos_sched_handle_throughput_req(bool high_tput_required);

/*---------------------------------------------------------------------------
  \brief vos_drop_rxpkt_by_staid() - API to drop pending Rx packets for a sta
  The \a vos_drop_rxpkt_by_staid() drops queued packets for a station, to drop
  all the pending packets the caller has to send WLAN_MAX_STA_COUNT as staId.
  \param  pSchedContext - pointer to the global vOSS Sched Context
  \param staId - Station Id

  \return Nothing
  \sa vos_drop_rxpkt_by_staid()
  -------------------------------------------------------------------------*/
void vos_drop_rxpkt_by_staid(pVosSchedContext pSchedContext, u_int16_t staId);

/*---------------------------------------------------------------------------
  \brief vos_indicate_rxpkt() - API to Indicate rx data packet
  The \a vos_indicate_rxpkt() enqueues the rx packet onto tlshimRxQueue
  and notifies VosTlshimRxThread().
  \param  Arg - pointer to the global vOSS Sched Context
  \param pkt - Vos data message buffer

  \return Nothing
  \sa vos_indicate_rxpkt()
  -------------------------------------------------------------------------*/
void vos_indicate_rxpkt(pVosSchedContext pSchedContext,
                        struct VosTlshimPkt *pkt);

/*---------------------------------------------------------------------------
  \brief vos_alloc_tlshim_pkt() - API to return next available vos message
  The \a vos_alloc_tlshim_pkt() returns next available vos message buffer
  used for Rx Data processing.
  \param pSchedContext - pointer to the global vOSS Sched Context

  \return pointer to vos message buffer
  \sa vos_alloc_tlshim_pkt()
  -------------------------------------------------------------------------*/
struct VosTlshimPkt *vos_alloc_tlshim_pkt(pVosSchedContext pSchedContext);

/*---------------------------------------------------------------------------
  \brief vos_free_tlshim_pkt() - API to release vos message to the freeq
  The \a vos_free_tlshim_pkt() returns the vos message used for Rx data
  to the free queue.
  \param  pSchedContext - pointer to the global vOSS Sched Context
  \param  pkt - Vos message buffer to be returned to free queue.

  \return Nothing
  \sa vos_free_tlshim_pkt()
  -------------------------------------------------------------------------*/
void vos_free_tlshim_pkt(pVosSchedContext pSchedContext,
                         struct VosTlshimPkt *pkt);
/*---------------------------------------------------------------------------
  \brief vos_free_tlshim_pkt_freeq() - Free voss buffer free queue
  The \a vos_free_tlshim_pkt_freeq() does mem free of the buffers
  available in free vos buffer queue which is used for Data rx processing
  from Tlshim.
  \param pSchedContext - pointer to the global vOSS Sched Context

  \return Nothing
  \sa vos_free_tlshim_pkt_freeq()
  -------------------------------------------------------------------------*/
void vos_free_tlshim_pkt_freeq(pVosSchedContext pSchedContext);
#else
static inline int vos_sched_handle_throughput_req(
	bool high_tput_required)
{
	return 0;
}
#endif

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
  \

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
VOS_STATUS vos_sched_open( v_PVOID_t pVosContext,
                           pVosSchedContext pSchedCxt,
                           v_SIZE_t SchedCtxSize);

/*---------------------------------------------------------------------------

  \brief vos_watchdog_open() - initialize the vOSS watchdog

  The \a vos_watchdog_open() function initializes the vOSS watchdog. Upon successful
        initialization, the watchdog thread is created and ready to receive and  process messages.


  \param  pVosContext - pointer to the global vOSS Context

  \param  pWdContext - pointer to a previously allocated buffer big
          enough to hold a watchdog context.

  \return VOS_STATUS_SUCCESS - Watchdog was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_RESOURCES - System resources (other than memory)
          are unavailable to initilize the Watchdog

          VOS_STATUS_E_NOMEM - insufficient memory exists to initialize
          the Watchdog

          VOS_STATUS_E_INVAL - Invalid parameter passed to the Watchdog Open
          function

          VOS_STATUS_E_FAILURE - Failure to initialize the Watchdog/

  \sa vos_watchdog_open()

  -------------------------------------------------------------------------*/

VOS_STATUS vos_watchdog_open

(
  v_PVOID_t           pVosContext,
  pVosWatchdogContext pWdContext,
  v_SIZE_t            wdCtxSize
);

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
VOS_STATUS vos_sched_close( v_PVOID_t pVosContext);

/*---------------------------------------------------------------------------

  \brief vos_watchdog_close() - Close the vOSS Watchdog

  The \a vos_watchdog_close() function closes the vOSS Watchdog
  Upon successful closing:

     - The Watchdog thread is closed


  \param  pVosContext - pointer to the global vOSS Context

  \return VOS_STATUS_SUCCESS - Watchdog was successfully initialized and
          is ready to be used.

          VOS_STATUS_E_INVAL - Invalid parameter passed

          VOS_STATUS_E_FAILURE - Failure to initialize the Watchdog/

  \sa vos_watchdog_close()

---------------------------------------------------------------------------*/
VOS_STATUS vos_watchdog_close ( v_PVOID_t pVosContext );

/* Helper routines provided to other VOS API's */
VOS_STATUS vos_mq_init(pVosMqType pMq);
void vos_mq_deinit(pVosMqType pMq);
void vos_mq_put(pVosMqType pMq, pVosMsgWrapper pMsgWrapper);
pVosMsgWrapper vos_mq_get(pVosMqType pMq);
v_BOOL_t vos_is_mq_empty(pVosMqType pMq);
pVosSchedContext get_vos_sched_ctxt(void);
pVosWatchdogContext get_vos_watchdog_ctxt(void);
VOS_STATUS vos_sched_init_mqs   (pVosSchedContext pSchedContext);
void vos_sched_deinit_mqs (pVosSchedContext pSchedContext);
void vos_sched_flush_mc_mqs  (pVosSchedContext pSchedContext);
void clearWlanResetReason(void);

void vos_timer_module_init( void );
VOS_STATUS vos_watchdog_wlan_shutdown(void);
VOS_STATUS vos_watchdog_wlan_re_init(void);
v_BOOL_t isWDresetInProgress(void);
void vos_ssr_protect_init(void);
void vos_ssr_protect(const char *caller_func);
void vos_ssr_unprotect(const char *caller_func);
bool vos_is_ssr_ready(const char *caller_func);
int vos_get_gfp_flags(void);

#define vos_wait_for_work_thread_completion(func) vos_is_ssr_ready(func)

#endif // #if !defined __VOSS_SCHED_H
