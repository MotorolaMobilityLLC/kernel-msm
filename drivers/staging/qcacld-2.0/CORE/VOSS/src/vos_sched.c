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

/*===========================================================================
  @file vos_sched.c
  @brief VOS Scheduler Implementation
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
#include <wlan_qct_sys.h>
#include <wlan_qct_tl.h>
#include "vos_sched.h"
#include <wlan_hdd_power.h>
#include "wlan_qct_wda.h"
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/topology.h>
#if defined(QCA_CONFIG_SMP)
#include "vos_cnss.h"
#endif

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ------------------------------------------------------------------------*/
#define VOS_SCHED_THREAD_HEART_BEAT    INFINITE
/* Milli seconds to delay SSR thread when an Entry point is Active */
#define SSR_WAIT_SLEEP_TIME 200
/* MAX iteration count to wait for Entry point to exit before
 * we proceed with SSR in WD Thread
 */
#define MAX_SSR_WAIT_ITERATIONS 200
#define MAX_SSR_PROTECT_LOG (16)

static atomic_t ssr_protect_entry_count;

struct ssr_protect {
   const char* func;
   bool  free;
   uint32_t pid;
};

static spinlock_t ssr_protect_lock;
static struct ssr_protect ssr_protect_log[MAX_SSR_PROTECT_LOG];

/*---------------------------------------------------------------------------
 * Type Declarations
 * ------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
 * Data definitions
 * ------------------------------------------------------------------------*/
static pVosSchedContext gpVosSchedContext;
static pVosWatchdogContext gpVosWatchdogContext;

/*---------------------------------------------------------------------------
 * Forward declaration
 * ------------------------------------------------------------------------*/
static int VosMCThread(void *Arg);
static int VosWDThread(void *Arg);
#ifdef QCA_CONFIG_SMP
static int VosTlshimRxThread(void *arg);
static unsigned long affine_cpu = 0;
static VOS_STATUS vos_alloc_tlshim_pkt_freeq(pVosSchedContext pSchedContext);
#endif
extern v_VOID_t vos_core_return_msg(v_PVOID_t pVContext, pVosMsgWrapper pMsgWrapper);


#ifdef QCA_CONFIG_SMP

/*Maximum 2 clusters supported*/
#define VOS_MAX_CPU_CLUSTERS 2

#define VOS_CPU_CLUSTER_TYPE_LITTLE 0
#define VOS_CPU_CLUSTER_TYPE_PERF 1

/**
 * vos_sched_find_attach_cpu - find available cores and attach to required core
 * @pSchedContext:	wlan scheduler context
 * @high_throughput:	high throughput is required or not
 *
 * Find current online cores.
 * high troughput required and PERF core online, then attach to last PERF core
 * low throughput required or only little cores online, the attach any little
 * core
 *
 * Return: 0 success
 *         1 fail
 */
static int vos_sched_find_attach_cpu(pVosSchedContext pSchedContext,
	bool high_throughput)
{
	unsigned long *online_perf_cpu = NULL;
	unsigned long *online_litl_cpu = NULL;
	unsigned char perf_core_count = 0;
	unsigned char litl_core_count = 0;
	int vosMaxClusterId = 0;
#if defined(WLAN_OPEN_SOURCE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	struct cpumask litl_mask;
	unsigned long cpus;
	int i;
#endif

	VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_LOW,
		"%s: num possible cpu %d",
		__func__, num_possible_cpus());


	online_perf_cpu = vos_mem_malloc(
		num_possible_cpus() * sizeof(unsigned long));
	if (!online_perf_cpu) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: perf cpu cache alloc fail", __func__);
		return 1;
	}
	vos_mem_zero(online_perf_cpu,
		num_possible_cpus() * sizeof(unsigned long));

	online_litl_cpu = vos_mem_malloc(
		num_possible_cpus() * sizeof(unsigned long));
	if (!online_litl_cpu) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: lttl cpu cache alloc fail", __func__);
		vos_mem_free(online_perf_cpu);
		return 1;
	}
	vos_mem_zero(online_litl_cpu,
		num_possible_cpus() * sizeof(unsigned long));

	/* Get Online perf CPU count */
#if defined(WLAN_OPEN_SOURCE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	for_each_online_cpu(cpus) {
		if ( topology_physical_package_id(cpus) > VOS_MAX_CPU_CLUSTERS) {
			VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
				"%s: can handle max %d clusters, returning...",
				__func__, VOS_MAX_CPU_CLUSTERS);
				goto err;
			}

		if (topology_physical_package_id(cpus) == VOS_CPU_CLUSTER_TYPE_PERF) {
			online_perf_cpu[perf_core_count] = cpus;
			perf_core_count++;
		} else {
			online_litl_cpu[litl_core_count] = cpus;
			litl_core_count++;
		}
		vosMaxClusterId =  topology_physical_package_id(cpus);
	}
#else
	vosMaxClusterId = 0;
#endif

	/* Single cluster system, not need to handle this */
	if (0 == vosMaxClusterId) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_LOW,
		"%s: single cluster system. returning", __func__);
		goto success;
	}

	if ((!litl_core_count) && (!perf_core_count)) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: Both Cluster off, do nothing", __func__);
		goto success;
	}

	if ((high_throughput && perf_core_count) || (!litl_core_count)) {
		/* Attach RX thread to PERF CPU */
		if (pSchedContext->rx_thread_cpu !=
			online_perf_cpu[perf_core_count - 1]) {
			if (vos_set_cpus_allowed_ptr(
				pSchedContext->TlshimRxThread,
				online_perf_cpu[perf_core_count - 1])) {
				VOS_TRACE(VOS_MODULE_ID_VOSS,
					VOS_TRACE_LEVEL_ERROR,
					"%s: rx thread perf core set fail",
					__func__);
				goto err;
			}
			pSchedContext->rx_thread_cpu =
				online_perf_cpu[perf_core_count - 1];
		}
	} else {

#if defined(WLAN_OPEN_SOURCE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
		/* Attach to any little core
		 * Final decision should made by scheduler */

		cpumask_clear(&litl_mask);
		for (i = 0;
			i < litl_core_count;
			i++) {
			cpumask_set_cpu(online_litl_cpu[i], &litl_mask);
		}

		set_cpus_allowed_ptr(pSchedContext->TlshimRxThread, &litl_mask);
		pSchedContext->rx_thread_cpu = 0;
#else
		/* Attach RX thread to last little core CPU */
		if (pSchedContext->rx_thread_cpu !=
			online_litl_cpu[litl_core_count - 1]) {
			if (vos_set_cpus_allowed_ptr(
				pSchedContext->TlshimRxThread,
				online_litl_cpu[litl_core_count - 1])) {
				VOS_TRACE(VOS_MODULE_ID_VOSS,
					VOS_TRACE_LEVEL_ERROR,
					"%s: rx thread litl core set fail",
					__func__);
					goto err;
			}
			pSchedContext->rx_thread_cpu =
				online_litl_cpu[litl_core_count - 1];
		}
#endif
	}

	VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
		"%s: NUM PERF CORE %d, HIGH TPUTR REQ %d, RX THRE CPU %lu",
		__func__, perf_core_count,
		(int)pSchedContext->high_throughput_required,
		pSchedContext->rx_thread_cpu);
success:
	vos_mem_free(online_perf_cpu);
	vos_mem_free(online_litl_cpu);
	return 0;

err:
	vos_mem_free(online_perf_cpu);
	vos_mem_free(online_litl_cpu);
	return 1;

}

/**
 * vos_sched_handle_cpu_hot_plug - cpu hotplug event handler
 *
 * cpu hotplug indication handler
 * will find online cores and will assign proper core based on perf requirement
 *
 * Return: 0 success
 *         1 fail
 */
int vos_sched_handle_cpu_hot_plug(void)
{
	pVosSchedContext pSchedContext = get_vos_sched_ctxt();

	if (!pSchedContext) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: invalid context", __func__);
		return 1;
	}

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_VOSS, NULL) ||
		(vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)))
		return 0;

	vos_lock_acquire(&pSchedContext->affinity_lock);
	if (vos_sched_find_attach_cpu(pSchedContext,
		pSchedContext->high_throughput_required)) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: handle hot plug fail", __func__);
		vos_lock_release(&pSchedContext->affinity_lock);
		return 1;
	}
	vos_lock_release(&pSchedContext->affinity_lock);
	return 0;
}

/**
 * vos_sched_handle_throughput_req - cpu throughput requirement handler
 * @high_tput_required:	high throughput is required or not
 *
 * high or low throughput indication ahndler
 * will find online cores and will assign proper core based on perf requirement
 *
 * Return: 0 success
 *         1 fail
 */
int vos_sched_handle_throughput_req(bool high_tput_required)
{
	pVosSchedContext pSchedContext = get_vos_sched_ctxt();

	if (!pSchedContext) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: invalid context", __func__);
		return 1;
	}

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_VOSS, NULL) ||
		(vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)))
		return 0;

	vos_lock_acquire(&pSchedContext->affinity_lock);
	pSchedContext->high_throughput_required = high_tput_required;
	if (vos_sched_find_attach_cpu(pSchedContext, high_tput_required)) {
		VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
			"%s: handle throughput req fail", __func__);
		vos_lock_release(&pSchedContext->affinity_lock);
		return 1;
	}
	vos_lock_release(&pSchedContext->affinity_lock);
	return 0;
}

/**
 * __vos_cpu_hotplug_notify - cpu core on-off notification handler
 * @block:	notifier block
 * @state:	state of core
 * @hcpu:	target cpu core
 *
 * pre-registered core status change notify callback function
 * will handle only ONLINE, OFFLINE notification
 * based on cpu architecture, rx thread affinity will be different
 *
 * Return: 0 success
 *         1 fail
 */
static int __vos_cpu_hotplug_notify(struct notifier_block *block,
                                  unsigned long state, void *hcpu)
{
   unsigned long cpu = (unsigned long) hcpu;
   unsigned long pref_cpu = 0;
   pVosSchedContext pSchedContext = get_vos_sched_ctxt();
   int i;
   unsigned int multi_cluster;
   unsigned int num_cpus;
#if defined(WLAN_OPEN_SOURCE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
   int cpus;
#endif

   if ((NULL == pSchedContext) || (NULL == pSchedContext->TlshimRxThread))
       return NOTIFY_OK;

   if (vos_is_load_unload_in_progress(VOS_MODULE_ID_VOSS, NULL) ||
      (vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)))
       return NOTIFY_OK;

   num_cpus = num_possible_cpus();
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_LOW,
             "%s: RX CORE %d, STATE %d, NUM CPUS %d",
              __func__, (int)affine_cpu, (int)state, num_cpus);
   multi_cluster = 0;

#if defined(WLAN_OPEN_SOURCE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))

   for_each_online_cpu(cpus) {
        multi_cluster =  topology_physical_package_id(cpus);
    }
#endif

   if ((multi_cluster) &&
       ((CPU_ONLINE == state) || (CPU_DEAD == state))) {
      vos_sched_handle_cpu_hot_plug();
      return NOTIFY_OK;
   }

   switch (state) {
   case CPU_ONLINE:
       if (affine_cpu != 0)
           return NOTIFY_OK;

       for_each_online_cpu(i) {
           if (i == 0)
               continue;
           pref_cpu = i;
               break;
       }
       break;
   case CPU_DEAD:
       if (cpu != affine_cpu)
           return NOTIFY_OK;

       affine_cpu = 0;
       for_each_online_cpu(i) {
           if (i == 0)
               continue;
           pref_cpu = i;
               break;
       }
   }

   if (pref_cpu == 0)
       return NOTIFY_OK;

   if (!vos_set_cpus_allowed_ptr(pSchedContext->TlshimRxThread, pref_cpu))
       affine_cpu = pref_cpu;

   return NOTIFY_OK;
}

/**
 * vos_cpu_hotplug_notify - cpu core on-off notification handler wrapper
 * @block:	notifier block
 * @state:	state of core
 * @hcpu:	target cpu core
 *
 * pre-registered core status change notify callback function
 * will handle only ONLINE, OFFLINE notification
 * based on cpu architecture, rx thread affinity will be different
 * wrapper function
 *
 * Return: 0 success
 *         1 fail
 */
static int vos_cpu_hotplug_notify(struct notifier_block *block,
                                  unsigned long state, void *hcpu)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __vos_cpu_hotplug_notify(block, state, hcpu);
	vos_ssr_unprotect(__func__);

	return ret;
}

static struct notifier_block vos_cpu_hotplug_notifier = {
   .notifier_call = vos_cpu_hotplug_notify,
};
#endif

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
  init_completion(&pSchedContext->McShutdown);
  init_completion(&pSchedContext->ResumeMcEvent);

  spin_lock_init(&pSchedContext->McThreadLock);
#ifdef QCA_CONFIG_SMP
  spin_lock_init(&pSchedContext->TlshimRxThreadLock);
#endif

  init_waitqueue_head(&pSchedContext->mcWaitQueue);
  pSchedContext->mcEventFlag = 0;

#ifdef QCA_CONFIG_SMP
  init_waitqueue_head(&pSchedContext->tlshimRxWaitQueue);
  init_completion(&pSchedContext->TlshimRxStartEvent);
  init_completion(&pSchedContext->SuspndTlshimRxEvent);
  init_completion(&pSchedContext->ResumeTlshimRxEvent);
  init_completion(&pSchedContext->TlshimRxShutdown);
  pSchedContext->tlshimRxEvtFlg = 0;
  spin_lock_init(&pSchedContext->TlshimRxQLock);
  spin_lock_init(&pSchedContext->VosTlshimPktFreeQLock);
  INIT_LIST_HEAD(&pSchedContext->tlshimRxQueue);
  spin_lock_bh(&pSchedContext->VosTlshimPktFreeQLock);
  INIT_LIST_HEAD(&pSchedContext->VosTlshimPktFreeQ);
  if (vos_alloc_tlshim_pkt_freeq(pSchedContext) !=  VOS_STATUS_SUCCESS)
  {
       spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
       return VOS_STATUS_E_FAILURE;
  }
  spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
  register_hotcpu_notifier(&vos_cpu_hotplug_notifier);
  pSchedContext->cpuHotPlugNotifier = &vos_cpu_hotplug_notifier;
  vos_lock_init(&pSchedContext->affinity_lock);
  pSchedContext->high_throughput_required = false;
#endif


  /*
  ** This initialization is critical as the threads will later access the
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

#ifdef QCA_CONFIG_SMP
  pSchedContext->TlshimRxThread = kthread_create(VosTlshimRxThread,
                                                 pSchedContext,
                                                 "VosTlshimRxThread");
  if (IS_ERR(pSchedContext->TlshimRxThread))
  {

     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
               "%s: Could not Create VOSS Tlshim RX Thread", __func__);
     goto TLSHIM_RX_THREAD_START_FAILURE;

  }
  wake_up_process(pSchedContext->TlshimRxThread);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
             ("VOSS Tlshim RX thread Created"));
#endif
  /*
  ** Now make sure all threads have started before we exit.
  ** Each thread should normally ACK back when it starts.
  */
  wait_for_completion_interruptible(&pSchedContext->McStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS MC Thread has started",__func__);
#ifdef QCA_CONFIG_SMP
  wait_for_completion_interruptible(&pSchedContext->TlshimRxStartEvent);
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: VOSS Tlshim Rx Thread has started", __func__);
#endif
  /*
  ** We're good now: Let's get the ball rolling!!!
  */
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: VOSS Scheduler successfully Opened",__func__);
  return VOS_STATUS_SUCCESS;


#ifdef QCA_CONFIG_SMP
TLSHIM_RX_THREAD_START_FAILURE:
#endif
MC_THREAD_START_FAILURE:
    //Try and force the Main thread controller to exit
    set_bit(MC_SHUTDOWN_EVENT_MASK, &pSchedContext->mcEventFlag);
    set_bit(MC_POST_EVENT_MASK, &pSchedContext->mcEventFlag);
    wake_up_interruptible(&pSchedContext->mcWaitQueue);
    //Wait for MC to exit
    wait_for_completion_interruptible(&pSchedContext->McShutdown);

  //De-initialize all the message queues
  vos_sched_deinit_mqs(pSchedContext);


#ifdef QCA_CONFIG_SMP
  unregister_hotcpu_notifier(&vos_cpu_hotplug_notifier);
  vos_free_tlshim_pkt_freeq(gpVosSchedContext);
#endif

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
        "%s: Bad Args passed", __func__);
     return 0;
  }
  set_user_nice(current, -2);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
  daemonize("MC_Thread");
#endif

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
         "%s: wait_event_interruptible returned -ERESTARTSYS", __func__);
      VOS_BUG(0);
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
              "%s: pMsgWrapper is NULL", __func__);
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
              "%s: pMsgWrapper is NULL", __func__);
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
              "%s: pMsgWrapper is NULL", __func__);
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
               "%s: pMsgWrapper is NULL", __func__);
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
              "%s: pMsgWrapper is NULL", __func__);
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
      break; //All queues are empty now
    } // while message loop processing
  } // while TRUE
  // If we get here the MC thread must exit
  VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
      "%s: MC Thread exiting!!!!", __func__);
  complete_and_exit(&pSchedContext->McShutdown, 0);
} /* VosMCThread() */

v_BOOL_t isWDresetInProgress(void)
{
   if(gpVosWatchdogContext!=NULL)
   {
      return gpVosWatchdogContext->resetInProgress;
   }
   else
   {
      return FALSE;
   }
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
  int count                      = 0;
  VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
  set_user_nice(current, -3);

  if (Arg == NULL)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
        "%s: Bad Args passed", __func__);
     return 0;
  }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
  daemonize("WD_Thread");
#endif

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
         "%s: wait_event_interruptible returned -ERESTARTSYS", __func__);
      break;
    }
    clear_bit(WD_POST_EVENT_MASK, &pWdContext->wdEventFlag);
    while(1)
    {
      /* Check for any Active Entry Points
       * If active, delay SSR until no entry point is active or
       * delay until count is decremented to ZERO
       */
      count = MAX_SSR_WAIT_ITERATIONS;
      while (count)
      {
         if (!atomic_read(&ssr_protect_entry_count))
         {
             /* no external threads are executing */
             break;
         }
         /* at least one external thread is executing */
         if (--count)
         {
             VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "%s: Waiting for active entry points to exit", __func__);
             msleep(SSR_WAIT_SLEEP_TIME);
         }
      }
      /* at least one external thread is executing */
      if (!count)
      {
          VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Continuing SSR when %d Entry points are still active",
                     __func__, atomic_read(&ssr_protect_entry_count));
      }
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
        vosStatus = hdd_wlan_re_init(NULL);

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
      "%s: Watchdog Thread exiting !!!!", __func__);
  complete_and_exit(&pWdContext->WdShutdown, 0);

err_reset:
    VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
      "%s: Watchdog Thread Failed to Reset, Exiting!!!!", __func__);
    return 0;

} /* VosMCThread() */

#ifdef QCA_CONFIG_SMP
/*---------------------------------------------------------------------------
  \brief vos_free_tlshim_pkt_freeq() - Free voss buffer free queue
  The \a vos_free_tlshim_pkt_freeq() does mem free of the buffers
  available in free vos buffer queue which is used for Data rx processing
  from Tlshim.
  \param pSchedContext - pointer to the global vOSS Sched Context

  \return Nothing
  \sa vos_free_tlshim_pkt_freeq()
  -------------------------------------------------------------------------*/
void vos_free_tlshim_pkt_freeq(pVosSchedContext pSchedContext)
{
   struct VosTlshimPkt *pkt;

   spin_lock_bh(&pSchedContext->VosTlshimPktFreeQLock);
   while (!list_empty(&pSchedContext->VosTlshimPktFreeQ)) {
       pkt = list_entry((&pSchedContext->VosTlshimPktFreeQ)->next,
                     typeof(*pkt), list);
       list_del(&pkt->list);
       spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
       vos_mem_free(pkt);
       spin_lock_bh(&pSchedContext->VosTlshimPktFreeQLock);
   }
   spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);

}

/*---------------------------------------------------------------------------
  \brief vos_alloc_tlshim_pkt_freeq() - Function to allocate free buffer queue
  The \a vos_alloc_tlshim_pkt_freeq() allocates VOSS_MAX_TLSHIM_PKT
  number of vos message buffers which are used for Rx data processing
  from Tlshim.
  \param  pSchedContext - pointer to the global vOSS Sched Context

  \return status of memory allocation
  \sa vos_alloc_tlshim_pkt_freeq()
  -------------------------------------------------------------------------*/
static VOS_STATUS vos_alloc_tlshim_pkt_freeq(pVosSchedContext pSchedContext)
{
   struct VosTlshimPkt *pkt, *tmp;
   int i;

   for (i = 0; i < VOSS_MAX_TLSHIM_PKT; i++) {
       pkt = vos_mem_malloc(sizeof(*pkt));
       if (!pkt) {
           VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                      "%s Vos packet allocation for tlshim thread failed",
                      __func__);
           goto free;
       }
       memset(pkt, 0, sizeof(*pkt));
       list_add_tail(&pkt->list, &pSchedContext->VosTlshimPktFreeQ);
   }

   return VOS_STATUS_SUCCESS;

free:
   list_for_each_entry_safe(pkt, tmp, &pSchedContext->VosTlshimPktFreeQ, list) {
       list_del(&pkt->list);
       vos_mem_free(pkt);
   }
   return VOS_STATUS_E_NOMEM;
}

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
                         struct VosTlshimPkt *pkt)
{
   memset(pkt, 0, sizeof(*pkt));
   spin_lock_bh(&pSchedContext->VosTlshimPktFreeQLock);
   list_add_tail(&pkt->list, &pSchedContext->VosTlshimPktFreeQ);
   spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
}

/*---------------------------------------------------------------------------
  \brief vos_alloc_tlshim_pkt() - API to return next available vos message
  The \a vos_alloc_tlshim_pkt() returns next available vos message buffer
  used for Rx Data processing.
  \param pSchedContext - pointer to the global vOSS Sched Context

  \return pointer to vos message buffer
  \sa vos_alloc_tlshim_pkt()
  -------------------------------------------------------------------------*/
struct VosTlshimPkt *vos_alloc_tlshim_pkt(pVosSchedContext pSchedContext)
{
   struct VosTlshimPkt *pkt;

   spin_lock_bh(&pSchedContext->VosTlshimPktFreeQLock);
   if (list_empty(&pSchedContext->VosTlshimPktFreeQ)) {
       spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
       return NULL;
   }
   pkt = list_first_entry(&pSchedContext->VosTlshimPktFreeQ,
                          struct VosTlshimPkt, list);
   list_del(&pkt->list);
   spin_unlock_bh(&pSchedContext->VosTlshimPktFreeQLock);
   return pkt;
}

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
                        struct VosTlshimPkt *pkt)
{
   spin_lock_bh(&pSchedContext->TlshimRxQLock);
   list_add_tail(&pkt->list, &pSchedContext->tlshimRxQueue);
   spin_unlock_bh(&pSchedContext->TlshimRxQLock);
   set_bit(RX_POST_EVENT_MASK, &pSchedContext->tlshimRxEvtFlg);
   wake_up_interruptible(&pSchedContext->tlshimRxWaitQueue);
}

/*---------------------------------------------------------------------------
  \brief vos_drop_rxpkt_by_staid() - API to drop pending Rx packets for a sta
  The \a vos_drop_rxpkt_by_staid() drops queued packets for a station, to drop
  all the pending packets the caller has to send WLAN_MAX_STA_COUNT as staId.
  \param  pSchedContext - pointer to the global vOSS Sched Context
  \param staId - Station Id

  \return Nothing
  \sa vos_drop_rxpkt_by_staid()
  -------------------------------------------------------------------------*/
void vos_drop_rxpkt_by_staid(pVosSchedContext pSchedContext, u_int16_t staId)
{
   struct list_head local_list;
   struct VosTlshimPkt *pkt, *tmp;
   adf_nbuf_t buf, next_buf;

   INIT_LIST_HEAD(&local_list);
   spin_lock_bh(&pSchedContext->TlshimRxQLock);
   if (list_empty(&pSchedContext->tlshimRxQueue)) {
       spin_unlock_bh(&pSchedContext->TlshimRxQLock);
       return;
   }
   list_for_each_entry_safe(pkt, tmp, &pSchedContext->tlshimRxQueue, list) {
       if (pkt->staId == staId || staId == WLAN_MAX_STA_COUNT)
           list_move_tail(&pkt->list, &local_list);
   }
   spin_unlock_bh(&pSchedContext->TlshimRxQLock);

   list_for_each_entry_safe(pkt, tmp, &local_list, list) {
       list_del(&pkt->list);
       buf = pkt->Rxpkt;
       while (buf) {
           next_buf = adf_nbuf_queue_next(buf);
           adf_nbuf_free(buf);
           buf = next_buf;
       }
       vos_free_tlshim_pkt(pSchedContext, pkt);
   }
}

/*---------------------------------------------------------------------------
  \brief vos_rx_from_queue() - Function to process pending Rx packets
  The \a vos_rx_from_queue() traverses the pending buffer list and calling
  the callback. This callback would essentially send the packet to HDD.
  \param  pSchedContext - pointer to the global vOSS Sched Context

  \return Nothing
  \sa vos_rx_from_queue()
  -------------------------------------------------------------------------*/
static void vos_rx_from_queue(pVosSchedContext pSchedContext)
{
   struct VosTlshimPkt *pkt;
   u_int16_t sta_id;

   spin_lock_bh(&pSchedContext->TlshimRxQLock);
   while (!list_empty(&pSchedContext->tlshimRxQueue)) {
           pkt = list_first_entry(&pSchedContext->tlshimRxQueue,
                                  struct VosTlshimPkt, list);
           list_del(&pkt->list);
           spin_unlock_bh(&pSchedContext->TlshimRxQLock);
           sta_id = pkt->staId;
           pkt->callback(pkt->context, pkt->Rxpkt, sta_id);
           vos_free_tlshim_pkt(pSchedContext, pkt);
           spin_lock_bh(&pSchedContext->TlshimRxQLock);
   }
   spin_unlock_bh(&pSchedContext->TlshimRxQLock);
}

/*---------------------------------------------------------------------------
  \brief VosTlshimRxThread() - The VOSS Main Tlshim Rx thread
  The \a VosTlshimRxThread() is the thread for Tlshim Data packet processing.
  \param  Arg - pointer to the global vOSS Sched Context

  \return Thread exit code
  \sa VosTlshimRxThread()
  -------------------------------------------------------------------------*/
static int VosTlshimRxThread(void *arg)
{
   pVosSchedContext pSchedContext = (pVosSchedContext)arg;
   unsigned long pref_cpu = 0;
   bool shutdown = false;
   int status, i;

   set_user_nice(current, -1);
#ifdef MSM_PLATFORM
   set_wake_up_idle(true);
#endif

   /* Find the available cpu core other than cpu 0 and
    * bind the thread */
   for_each_online_cpu(i) {
       if (i == 0)
           continue;
       pref_cpu = i;
           break;
   }
   if (pref_cpu != 0 && (!vos_set_cpus_allowed_ptr(current, pref_cpu)))
       affine_cpu = pref_cpu;

   if (!arg) {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
       "%s: Bad Args passed", __func__);
       return 0;
   }

   complete(&pSchedContext->TlshimRxStartEvent);

   while (!shutdown) {
       status = wait_event_interruptible(pSchedContext->tlshimRxWaitQueue,
                         test_bit(RX_POST_EVENT_MASK,
                                  &pSchedContext->tlshimRxEvtFlg) ||
                         test_bit(RX_SUSPEND_EVENT_MASK,
                                  &pSchedContext->tlshimRxEvtFlg));
       if (status == -ERESTARTSYS)
           break;

       clear_bit(RX_POST_EVENT_MASK, &pSchedContext->tlshimRxEvtFlg);
       while (true) {
           if (test_bit(RX_SHUTDOWN_EVENT_MASK,
                      &pSchedContext->tlshimRxEvtFlg)) {
               clear_bit(RX_SHUTDOWN_EVENT_MASK,
                         &pSchedContext->tlshimRxEvtFlg);
               if (test_bit(RX_SUSPEND_EVENT_MASK,
                            &pSchedContext->tlshimRxEvtFlg)) {
                   clear_bit(RX_SUSPEND_EVENT_MASK,
                             &pSchedContext->tlshimRxEvtFlg);
                   complete(&pSchedContext->SuspndTlshimRxEvent);
               }
               VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                         "%s: Shutting down tl shim Tlshim rx thread", __func__);
               shutdown = true;
               break;
           }
           vos_rx_from_queue(pSchedContext);

           if (test_bit(RX_SUSPEND_EVENT_MASK,
                        &pSchedContext->tlshimRxEvtFlg)) {
               clear_bit(RX_SUSPEND_EVENT_MASK,
                         &pSchedContext->tlshimRxEvtFlg);
               spin_lock(&pSchedContext->TlshimRxThreadLock);
               complete(&pSchedContext->SuspndTlshimRxEvent);
               INIT_COMPLETION(pSchedContext->ResumeTlshimRxEvent);
               spin_unlock(&pSchedContext->TlshimRxThreadLock);
               wait_for_completion_interruptible(
                              &pSchedContext->ResumeTlshimRxEvent);
           }
           break;
       }
   }

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s: Exiting VOSS Tlshim rx thread", __func__);
   complete_and_exit(&pSchedContext->TlshimRxShutdown, 0);
}
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
        "%s: invoked", __func__);
    if (gpVosSchedContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: gpVosSchedContext == NULL",__func__);
       return VOS_STATUS_E_FAILURE;
    }

    // shut down MC Thread
    set_bit(MC_SHUTDOWN_EVENT_MASK, &gpVosSchedContext->mcEventFlag);
    set_bit(MC_POST_EVENT_MASK, &gpVosSchedContext->mcEventFlag);
    wake_up_interruptible(&gpVosSchedContext->mcWaitQueue);
    //Wait for MC to exit
    wait_for_completion(&gpVosSchedContext->McShutdown);
    gpVosSchedContext->McThread = 0;

    //Clean up message queues of MC thread
    vos_sched_flush_mc_mqs(gpVosSchedContext);

    //Deinit all the queues
    vos_sched_deinit_mqs(gpVosSchedContext);

#ifdef QCA_CONFIG_SMP
    vos_lock_destroy(&gpVosSchedContext->affinity_lock);
    // Shut down Tlshim Rx thread
    set_bit(RX_SHUTDOWN_EVENT_MASK, &gpVosSchedContext->tlshimRxEvtFlg);
    set_bit(RX_POST_EVENT_MASK, &gpVosSchedContext->tlshimRxEvtFlg);
    wake_up_interruptible(&gpVosSchedContext->tlshimRxWaitQueue);
    wait_for_completion(&gpVosSchedContext->TlshimRxShutdown);
    gpVosSchedContext->TlshimRxThread = NULL;
    vos_drop_rxpkt_by_staid(gpVosSchedContext, WLAN_MAX_STA_COUNT);
    vos_free_tlshim_pkt_freeq(gpVosSchedContext);
    unregister_hotcpu_notifier(&vos_cpu_hotplug_notifier);
#endif
    return VOS_STATUS_SUCCESS;
} /* vox_sched_close() */

VOS_STATUS vos_watchdog_close ( v_PVOID_t pVosContext )
{
    VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
        "%s: vos_watchdog closing now", __func__);
    if (gpVosWatchdogContext == NULL)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
           "%s: gpVosWatchdogContext is NULL",__func__);
       return VOS_STATUS_E_FAILURE;
    }
    set_bit(WD_SHUTDOWN_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    set_bit(WD_POST_EVENT_MASK, &gpVosWatchdogContext->wdEventFlag);
    wake_up_interruptible(&gpVosWatchdogContext->wdWaitQueue);
    //Wait for Watchdog thread to exit
    wait_for_completion(&gpVosWatchdogContext->WdShutdown);
    return VOS_STATUS_SUCCESS;
} /* vos_watchdog_close() */

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
 // MC WDA
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the WDA MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->wdaMcMq);
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
  //MC SYS
  VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s De-Initializing the SYS MC Message queue",__func__);
  vos_mq_deinit(&pSchedContext->sysMcMq);

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
             ("Flushing the MC Thread message queue") );

  if (NULL == pSchedContext)
  {
     VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: pSchedContext is NULL", __func__);
     return;
  }

  vosCtx = (pVosContextType)(pSchedContext->pVContext);
  if (NULL == vosCtx)
  {
     VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                "%s: vosCtx is NULL", __func__);
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
  /* Flush the WDA Mq */
  while( NULL != (pMsgWrapper = vos_mq_get(&pSchedContext->wdaMcMq) ))
  {
    if(pMsgWrapper->pVosMsg != NULL)
    {
        VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                   "%s: Freeing MC WDA MSG message type %d",
                   __func__, pMsgWrapper->pVosMsg->type );
        if (pMsgWrapper->pVosMsg->bodyptr) {
            vos_mem_free((v_VOID_t*)pMsgWrapper->pVosMsg->bodyptr);
        }

        pMsgWrapper->pVosMsg->bodyptr = NULL;
        pMsgWrapper->pVosMsg->bodyval = 0;
        pMsgWrapper->pVosMsg->type = 0;
    }
    vos_core_return_msg(pSchedContext->pVContext, pMsgWrapper);
  }

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
} /* vos_sched_flush_mc_mqs() */

/*-------------------------------------------------------------------------
 Helper function to get the scheduler context
 ------------------------------------------------------------------------*/
pVosSchedContext get_vos_sched_ctxt(void)
{
   //Make sure that Vos Scheduler context has been initialized
   if (gpVosSchedContext == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
         "%s: gpVosSchedContext == NULL",__func__);
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
         "%s: gpVosWatchdogContext == NULL",__func__);
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
        "%s: WLAN driver is shutting down ", __func__);
    if (NULL == gpVosWatchdogContext)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Watchdog not enabled. LOGP ignored.", __func__);
       return VOS_STATUS_E_FAILURE;
    }

    pVosContext = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);
    pHddCtx = (hdd_context_t *)vos_get_context(VOS_MODULE_ID_HDD, pVosContext );
    if (NULL == pHddCtx)
    {
       VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
           "%s: Invalid HDD Context", __func__);
       return VOS_STATUS_E_FAILURE;
    }

    /* Take the lock here */
    spin_lock(&gpVosWatchdogContext->wdLock);

    /* reuse the existing 'reset in progress' */
    if (gpVosWatchdogContext->resetInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
            "%s: Shutdown already in Progress. Ignoring signaling Watchdog",
                                                           __func__);
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
                                                           __func__);
        /* Release the lock here */
        spin_unlock(&gpVosWatchdogContext->wdLock);
        return VOS_STATUS_E_FAILURE;
    }

    /* Set the flags so that all future CMD53 and Wext commands get blocked right away */
    vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);
    vos_set_reinit_in_progress(VOS_MODULE_ID_VOSS, FALSE);
    pHddCtx->isLogpInProgress = TRUE;

    /* Release the lock here */
    spin_unlock(&gpVosWatchdogContext->wdLock);

    if ((pHddCtx->isLoadInProgress) ||
        (pHddCtx->isUnloadInProgress))
    {
        VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "%s: Load/unload in Progress. Ignoring signaling Watchdog",
                __func__);
        /* wcnss has crashed, and SSR has alredy been started by Kernel driver.
         * So disable SSR from WLAN driver */
        hdd_set_ssr_required( HDD_SSR_DISABLED );
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

/**
 * vos_ssr_protect_init() - initialize ssr protection debug functionality
 *
 * Return:
 *        void
 */
void vos_ssr_protect_init(void)
{
    int i = 0;

    spin_lock_init(&ssr_protect_lock);

    while (i < MAX_SSR_PROTECT_LOG) {
       ssr_protect_log[i].func = NULL;
       ssr_protect_log[i].free = true;
       ssr_protect_log[i].pid =  0;
       i++;
    }
}

/**
 * vos_print_external_threads() - print external threads stuck in driver
 *
 * Return:
 *        void
 */

static void vos_print_external_threads(void)
{
    int i = 0;
    unsigned long irq_flags;

    spin_lock_irqsave(&ssr_protect_lock, irq_flags);

    while (i < MAX_SSR_PROTECT_LOG) {
        if (!ssr_protect_log[i].free) {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "PID %d is stuck at %s", ssr_protect_log[i].pid,
               ssr_protect_log[i].func);
        }
        i++;
    }

    spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);
}


/**
  @brief vos_ssr_protect()

  This function is called to keep track of active driver entry points

  @param
         caller_func - Name of calling function.
  @return
         void
*/
void vos_ssr_protect(const char *caller_func)
{
     int count;
     int i = 0;
     bool status = false;
     unsigned long irq_flags;

     count = atomic_inc_return(&ssr_protect_entry_count);

     spin_lock_irqsave(&ssr_protect_lock, irq_flags);

     while (i < MAX_SSR_PROTECT_LOG) {
         if (ssr_protect_log[i].free) {
              ssr_protect_log[i].func = caller_func;
              ssr_protect_log[i].free = false;
              ssr_protect_log[i].pid = current->pid;
              status = true;
              break;
         }
         i++;
     }

     spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);

     if (!status)
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "Could not track PID %d call %s: log is full",
             current->pid, caller_func);

}

/**
  @brief vos_ssr_unprotect()

  @param
         caller_func - Name of calling function.
  @return
         void
*/
void vos_ssr_unprotect(const char *caller_func)
{
   int count;
   int i = 0;
   bool status = false;
   unsigned long irq_flags;

   count = atomic_dec_return(&ssr_protect_entry_count);

   spin_lock_irqsave(&ssr_protect_lock, irq_flags);

   while (i < MAX_SSR_PROTECT_LOG) {
      if (!ssr_protect_log[i].free) {
          if ((ssr_protect_log[i].pid == current->pid) &&
              !strcmp(ssr_protect_log[i].func, caller_func)) {
              ssr_protect_log[i].func = NULL;
              ssr_protect_log[i].free = true;
              ssr_protect_log[i].pid =  0;
              status = true;
              break;
          }
      }
      i++;
   }

   spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);

   if (!status)
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
           "Untracked call %s", caller_func);
}

/**
  @brief vos_is_ssr_ready()

  This function will check if the calling execution can
  proceed with SSR.

  @param
         caller_func - Name of calling function.
  @return
         true or false
*/
bool vos_is_ssr_ready(const char *caller_func)
{
    int count = MAX_SSR_WAIT_ITERATIONS;

    while (count) {

        if (!atomic_read(&ssr_protect_entry_count))
            break;

        if (--count) {
            VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                    "%s: Waiting for active entry points to exit", __func__);
            msleep(SSR_WAIT_SLEEP_TIME);
        }
    }
    /* at least one external thread is executing */
    if (!count) {
        vos_print_external_threads();
        return false;
    }

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "Allowing SSR for %s", caller_func);

    return true;
}

/**
 * vos_get_gfp_flags(): get GFP flags
 *
 * Based on the scheduled context, return GFP flags
 * Return: gfp flags
 */
int vos_get_gfp_flags(void)
{
	int flags = GFP_KERNEL;

	if (in_interrupt() || in_atomic() || irqs_disabled())
		flags = GFP_ATOMIC;

	return flags;
}
