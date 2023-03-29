/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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

/**
 *  File: cds_sched.c
 *
 *  DOC: CDS Scheduler Implementation
 */

#include <cds_api.h>
#include <ani_global.h>
#include <sir_types.h>
#include <qdf_types.h>
#include <lim_api.h>
#include <sme_api.h>
#include <wlan_qct_sys.h>
#include "cds_sched.h"
#include <wlan_hdd_power.h>
#include "wma_types.h"
#include <dp_txrx.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#ifdef RX_PERFORMANCE
#include <linux/sched/types.h>
#endif

static spinlock_t ssr_protect_lock;

struct shutdown_notifier {
	struct list_head list;
	void (*cb)(void *priv);
	void *priv;
};

struct list_head shutdown_notifier_head;

enum notifier_state {
	NOTIFIER_STATE_NONE,
	NOTIFIER_STATE_NOTIFYING,
} notifier_state;

static p_cds_sched_context gp_cds_sched_context;

#ifdef QCA_CONFIG_SMP
static int cds_ol_rx_thread(void *arg);
static uint32_t affine_cpu;
static QDF_STATUS cds_alloc_ol_rx_pkt_freeq(p_cds_sched_context pSchedContext);

#define CDS_CORE_PER_CLUSTER (4)
/*Maximum 2 clusters supported*/
#define CDS_MAX_CPU_CLUSTERS 2

#define CDS_CPU_CLUSTER_TYPE_LITTLE 0
#define CDS_CPU_CLUSTER_TYPE_PERF 1

static inline
int cds_set_cpus_allowed_ptr_with_cpu(struct task_struct *task,
				      unsigned long cpu)
{
	return set_cpus_allowed_ptr(task, cpumask_of(cpu));
}

static inline
int cds_set_cpus_allowed_ptr_with_mask(struct task_struct *task,
				       qdf_cpu_mask *new_mask)
{
	return set_cpus_allowed_ptr(task, new_mask);
}

void cds_set_rx_thread_cpu_mask(uint8_t cpu_affinity_mask)
{
	p_cds_sched_context sched_context = get_cds_sched_ctxt();

	if (!sched_context) {
		qdf_err("invalid context");
		return;
	}
	sched_context->conf_rx_thread_cpu_mask = cpu_affinity_mask;
}

void cds_set_rx_thread_ul_cpu_mask(uint8_t cpu_affinity_mask)
{
	p_cds_sched_context sched_context = get_cds_sched_ctxt();

	if (!sched_context) {
		qdf_err("invalid context");
		return;
	}
	sched_context->conf_rx_thread_ul_affinity = cpu_affinity_mask;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
/**
 * cds_rx_thread_log_cpu_affinity_change - Log Rx thread affinity change
 * @core_affine_cnt: Available cores
 * @tput_req: Throughput request
 * @old_mask: Old affinity mask
 * @new_mask: New affinity mask
 *
 * Return: NONE
 */
static void cds_rx_thread_log_cpu_affinity_change(unsigned char core_affine_cnt,
						  int tput_req,
						  struct cpumask *old_mask,
						  struct cpumask *new_mask)
{
	char new_mask_str[10];
	char old_mask_str[10];

	qdf_mem_zero(new_mask_str, sizeof(new_mask_str));
	qdf_mem_zero(new_mask_str, sizeof(old_mask_str));

	cpumap_print_to_pagebuf(false, old_mask_str, old_mask);
	cpumap_print_to_pagebuf(false, new_mask_str, new_mask);

	cds_debug("num online cores %d, high tput req %d, Rx_thread old mask %s new mask %s",
		  core_affine_cnt, tput_req, old_mask_str, new_mask_str);
}
#else
static void cds_rx_thread_log_cpu_affinity_change(unsigned char core_affine_cnt,
						  int tput_req,
						  struct cpumask *old_mask,
						  struct cpumask *new_mask)
{
}
#endif

/**
 * cds_sched_find_attach_cpu - find available cores and attach to required core
 * @pSchedContext:	wlan scheduler context
 * @high_throughput:	high throughput is required or not
 *
 * Find current online cores.
 * During high TPUT,
 * 1) If user INI configured cores, affine to those cores
 * 2) Otherwise perf cores.
 * 3) Otherwise to all cores.
 *
 * During low TPUT, set affinity to any core, let system decide.
 *
 * Return: 0 success
 *         1 fail
 */
static int cds_sched_find_attach_cpu(p_cds_sched_context pSchedContext,
	bool high_throughput)
{
	unsigned char core_affine_count = 0;
	qdf_cpu_mask new_mask;
	unsigned long cpus;
	struct cds_config_info *cds_cfg;

	cds_debug("num possible cpu %d", num_possible_cpus());

	qdf_cpumask_clear(&new_mask);

	if (high_throughput) {
		/* Get Online perf/pwr CPU count */
		for_each_online_cpu(cpus) {
			if (topology_physical_package_id(cpus) >
							CDS_MAX_CPU_CLUSTERS) {
				cds_err("can handle max %d clusters, returning...",
					CDS_MAX_CPU_CLUSTERS);
				goto err;
			}

			if (pSchedContext->conf_rx_thread_cpu_mask) {
				if (pSchedContext->conf_rx_thread_cpu_mask &
								(1 << cpus))
					qdf_cpumask_set_cpu(cpus, &new_mask);
			} else if (topology_physical_package_id(cpus) ==
						 CDS_CPU_CLUSTER_TYPE_PERF) {
				qdf_cpumask_set_cpu(cpus, &new_mask);
			}

			core_affine_count++;
		}
	} else {
		/* Attach to all cores, let scheduler decide */
		qdf_cpumask_setall(&new_mask);
	}

	cds_rx_thread_log_cpu_affinity_change(core_affine_count,
				(int)pSchedContext->high_throughput_required,
				&pSchedContext->rx_thread_cpu_mask,
				&new_mask);

	if (!cpumask_equal(&pSchedContext->rx_thread_cpu_mask, &new_mask)) {
		cds_cfg = cds_get_ini_config();
		cpumask_copy(&pSchedContext->rx_thread_cpu_mask, &new_mask);
		if (cds_cfg && cds_cfg->enable_dp_rx_threads)
			dp_txrx_set_cpu_mask(cds_get_context(QDF_MODULE_ID_SOC),
					     &new_mask);
		else
			cds_set_cpus_allowed_ptr_with_mask(pSchedContext->ol_rx_thread,
							   &new_mask);
	}

	return 0;
err:
	return 1;
}

/**
 * cds_sched_handle_cpu_hot_plug - cpu hotplug event handler
 *
 * cpu hotplug indication handler
 * will find online cores and will assign proper core based on perf requirement
 *
 * Return: 0 success
 *         1 fail
 */
int cds_sched_handle_cpu_hot_plug(void)
{
	p_cds_sched_context pSchedContext = get_cds_sched_ctxt();

	if (!pSchedContext) {
		cds_err("invalid context");
		return 1;
	}

	if (cds_is_load_or_unload_in_progress())
		return 0;

	mutex_lock(&pSchedContext->affinity_lock);
	if (cds_sched_find_attach_cpu(pSchedContext,
		pSchedContext->high_throughput_required)) {
		cds_err("handle hot plug fail");
		mutex_unlock(&pSchedContext->affinity_lock);
		return 1;
	}
	mutex_unlock(&pSchedContext->affinity_lock);
	return 0;
}

void cds_sched_handle_rx_thread_affinity_req(bool high_throughput)
{
	p_cds_sched_context pschedcontext = get_cds_sched_ctxt();
	unsigned long cpus;
	qdf_cpu_mask new_mask;
	unsigned char core_affine_count = 0;

	if (!pschedcontext || !pschedcontext->ol_rx_thread)
		return;

	if (cds_is_load_or_unload_in_progress()) {
		cds_err("load or unload in progress");
		return;
	}

	if (pschedcontext->rx_affinity_required == high_throughput)
		return;

	pschedcontext->rx_affinity_required = high_throughput;
	qdf_cpumask_clear(&new_mask);
	if (!high_throughput) {
		/* Attach to all cores, let scheduler decide */
		qdf_cpumask_setall(&new_mask);
		goto affine_thread;
	}
	for_each_online_cpu(cpus) {
		if (topology_physical_package_id(cpus) >
		    CDS_MAX_CPU_CLUSTERS) {
			cds_err("can handle max %d clusters ",
				CDS_MAX_CPU_CLUSTERS);
			return;
		}
		if (pschedcontext->conf_rx_thread_ul_affinity &&
		    (pschedcontext->conf_rx_thread_ul_affinity &
				 (1 << cpus)))
			qdf_cpumask_set_cpu(cpus, &new_mask);

		core_affine_count++;
	}

affine_thread:
	cds_rx_thread_log_cpu_affinity_change(
		core_affine_count,
		(int)pschedcontext->rx_affinity_required,
		&pschedcontext->rx_thread_cpu_mask,
		&new_mask);

	mutex_lock(&pschedcontext->affinity_lock);
	if (!cpumask_equal(&pschedcontext->rx_thread_cpu_mask, &new_mask)) {
		cpumask_copy(&pschedcontext->rx_thread_cpu_mask, &new_mask);
		cds_set_cpus_allowed_ptr_with_mask(pschedcontext->ol_rx_thread,
						   &new_mask);
	}
	mutex_unlock(&pschedcontext->affinity_lock);
}

/**
 * cds_sched_handle_throughput_req - cpu throughput requirement handler
 * @high_tput_required:	high throughput is required or not
 *
 * high or low throughput indication ahndler
 * will find online cores and will assign proper core based on perf requirement
 *
 * Return: 0 success
 *         1 fail
 */
int cds_sched_handle_throughput_req(bool high_tput_required)
{
	p_cds_sched_context pSchedContext = get_cds_sched_ctxt();

	if (!pSchedContext) {
		cds_err("invalid context");
		return 1;
	}

	if (cds_is_load_or_unload_in_progress()) {
		cds_err("load or unload in progress");
		return 0;
	}

	mutex_lock(&pSchedContext->affinity_lock);
	if (pSchedContext->high_throughput_required != high_tput_required) {
		pSchedContext->high_throughput_required = high_tput_required;
		if (cds_sched_find_attach_cpu(pSchedContext,
					      high_tput_required)) {
			mutex_unlock(&pSchedContext->affinity_lock);
			return 1;
		}
	}
	mutex_unlock(&pSchedContext->affinity_lock);
	return 0;
}

/**
 * cds_cpu_hotplug_multi_cluster() - calls the multi-cluster hotplug handler,
 *	when on a multi-cluster platform
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS cds_cpu_hotplug_multi_cluster(void)
{
	int cpus;
	unsigned int multi_cluster = 0;

	for_each_online_cpu(cpus) {
		multi_cluster = topology_physical_package_id(cpus);
	}

	if (!multi_cluster)
		return QDF_STATUS_E_NOSUPPORT;

	if (cds_sched_handle_cpu_hot_plug())
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * __cds_cpu_hotplug_notify() - CPU hotplug event handler
 * @cpu: CPU Id of the CPU generating the event
 * @cpu_up: true if the CPU is online
 *
 * Return: None
 */
static void __cds_cpu_hotplug_notify(uint32_t cpu, bool cpu_up)
{
	unsigned long pref_cpu = 0;
	p_cds_sched_context pSchedContext = get_cds_sched_ctxt();
	int i;

	if (!pSchedContext || !pSchedContext->ol_rx_thread)
		return;

	if (cds_is_load_or_unload_in_progress() || cds_is_driver_recovering())
		return;

	cds_debug("'%s' event on CPU %u (of %d); Currently affine to CPU %u",
		  cpu_up ? "Up" : "Down", cpu, num_possible_cpus(), affine_cpu);

	/* try multi-cluster scheduling first */
	if (QDF_IS_STATUS_SUCCESS(cds_cpu_hotplug_multi_cluster()))
		return;

	if (cpu_up) {
		if (affine_cpu != 0)
			return;

		for_each_online_cpu(i) {
			if (i == 0)
				continue;
			pref_cpu = i;
			break;
		}
	} else {
		if (cpu != affine_cpu)
			return;

		affine_cpu = 0;
		for_each_online_cpu(i) {
			if (i == 0)
				continue;
			pref_cpu = i;
			break;
		}
	}

	if (pref_cpu == 0)
		return;

	if (pSchedContext->ol_rx_thread &&
	    !cds_set_cpus_allowed_ptr_with_cpu(pSchedContext->ol_rx_thread,
					       pref_cpu))
		affine_cpu = pref_cpu;
}

/**
 * cds_cpu_hotplug_notify - cpu core up/down notification handler wrapper
 * @cpu: CPU Id of the CPU generating the event
 * @cpu_up: true if the CPU is online
 *
 * Return: None
 */
static void cds_cpu_hotplug_notify(uint32_t cpu, bool cpu_up)
{
	struct qdf_op_sync *op_sync;

	if (qdf_op_protect(&op_sync))
		return;

	__cds_cpu_hotplug_notify(cpu, cpu_up);

	qdf_op_unprotect(op_sync);
}

static void cds_cpu_online_cb(void *context, uint32_t cpu)
{
	cds_cpu_hotplug_notify(cpu, true);
}

static void cds_cpu_before_offline_cb(void *context, uint32_t cpu)
{
	cds_cpu_hotplug_notify(cpu, false);
}
#endif /* QCA_CONFIG_SMP */

/**
 * cds_sched_open() - initialize the CDS Scheduler
 * @p_cds_context: Pointer to the global CDS Context
 * @pSchedContext: Pointer to a previously allocated buffer big
 *	enough to hold a scheduler context.
 * @SchedCtxSize: CDS scheduler context size
 *
 * This function initializes the CDS Scheduler
 * Upon successful initialization:
 *	- All the message queues are initialized
 *	- The Main Controller thread is created and ready to receive and
 *	dispatch messages.
 *
 *
 * Return: QDF status
 */
QDF_STATUS cds_sched_open(void *p_cds_context,
		p_cds_sched_context pSchedContext,
		uint32_t SchedCtxSize)
{
	cds_debug("Opening the CDS Scheduler");
	/* Sanity checks */
	if ((!p_cds_context) || (!pSchedContext)) {
		cds_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}
	if (sizeof(cds_sched_context) != SchedCtxSize) {
		cds_debug("Incorrect CDS Sched Context size passed");
		return QDF_STATUS_E_INVAL;
	}
	qdf_mem_zero(pSchedContext, sizeof(cds_sched_context));
#ifdef QCA_CONFIG_SMP
	spin_lock_init(&pSchedContext->ol_rx_thread_lock);
	init_waitqueue_head(&pSchedContext->ol_rx_wait_queue);
	init_completion(&pSchedContext->ol_rx_start_event);
	init_completion(&pSchedContext->ol_suspend_rx_event);
	init_completion(&pSchedContext->ol_resume_rx_event);
	init_completion(&pSchedContext->ol_rx_shutdown);
	pSchedContext->ol_rx_event_flag = 0;
	spin_lock_init(&pSchedContext->ol_rx_queue_lock);
	spin_lock_init(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	INIT_LIST_HEAD(&pSchedContext->ol_rx_thread_queue);
	spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	INIT_LIST_HEAD(&pSchedContext->cds_ol_rx_pkt_freeq);
	spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	if (cds_alloc_ol_rx_pkt_freeq(pSchedContext) != QDF_STATUS_SUCCESS)
		goto pkt_freeqalloc_failure;
	qdf_cpuhp_register(&pSchedContext->cpuhp_event_handle,
			   NULL,
			   cds_cpu_online_cb,
			   cds_cpu_before_offline_cb);
	mutex_init(&pSchedContext->affinity_lock);
	pSchedContext->high_throughput_required = false;
	pSchedContext->rx_affinity_required = false;
#endif
	gp_cds_sched_context = pSchedContext;

#ifdef QCA_CONFIG_SMP
	pSchedContext->ol_rx_thread = kthread_create(cds_ol_rx_thread,
						       pSchedContext,
						       "cds_ol_rx_thread");
	if (IS_ERR(pSchedContext->ol_rx_thread)) {

		cds_alert("Could not Create CDS OL RX Thread");
		goto OL_RX_THREAD_START_FAILURE;

	}
	wake_up_process(pSchedContext->ol_rx_thread);
	cds_debug("CDS OL RX thread Created");
	wait_for_completion_interruptible(&pSchedContext->ol_rx_start_event);
	cds_debug("CDS OL Rx Thread has started");
#endif
	/* We're good now: Let's get the ball rolling!!! */
	cds_debug("CDS Scheduler successfully Opened");
	return QDF_STATUS_SUCCESS;
#ifdef QCA_CONFIG_SMP
OL_RX_THREAD_START_FAILURE:
#endif
#ifdef QCA_CONFIG_SMP
	qdf_cpuhp_unregister(&pSchedContext->cpuhp_event_handle);
	cds_free_ol_rx_pkt_freeq(gp_cds_sched_context);
pkt_freeqalloc_failure:
#endif
	gp_cds_sched_context = NULL;

	return QDF_STATUS_E_RESOURCES;

} /* cds_sched_open() */

#ifdef QCA_CONFIG_SMP
/**
 * cds_free_ol_rx_pkt_freeq() - free cds buffer free queue
 * @pSchedContext - pointer to the global CDS Sched Context
 *
 * This API does mem free of the buffers available in free cds buffer
 * queue which is used for Data rx processing.
 *
 * Return: none
 */
void cds_free_ol_rx_pkt_freeq(p_cds_sched_context pSchedContext)
{
	struct cds_ol_rx_pkt *pkt;

	spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	while (!list_empty(&pSchedContext->cds_ol_rx_pkt_freeq)) {
		pkt = list_entry((&pSchedContext->cds_ol_rx_pkt_freeq)->next,
			typeof(*pkt), list);
		list_del(&pkt->list);
		spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
		qdf_mem_free(pkt);
		spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	}
	spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
}

/**
 * cds_alloc_ol_rx_pkt_freeq() - Function to allocate free buffer queue
 * @pSchedContext - pointer to the global CDS Sched Context
 *
 * This API allocates CDS_MAX_OL_RX_PKT number of cds message buffers
 * which are used for Rx data processing.
 *
 * Return: status of memory allocation
 */
static QDF_STATUS cds_alloc_ol_rx_pkt_freeq(p_cds_sched_context pSchedContext)
{
	struct cds_ol_rx_pkt *pkt, *tmp;
	int i;

	for (i = 0; i < CDS_MAX_OL_RX_PKT; i++) {
		pkt = qdf_mem_malloc(sizeof(*pkt));
		if (!pkt) {
			cds_err("Vos packet allocation for ol rx thread failed");
			goto free;
		}
		spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
		list_add_tail(&pkt->list, &pSchedContext->cds_ol_rx_pkt_freeq);
		spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	}

	return QDF_STATUS_SUCCESS;

free:
	spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	list_for_each_entry_safe(pkt, tmp, &pSchedContext->cds_ol_rx_pkt_freeq,
				 list) {
		list_del(&pkt->list);
		spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
		qdf_mem_free(pkt);
		spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	}
	spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	return QDF_STATUS_E_NOMEM;
}

/**
 * cds_free_ol_rx_pkt() - api to release cds message to the freeq
 * This api returns the cds message used for Rx data to the free queue
 * @pSchedContext: Pointer to the global CDS Sched Context
 * @pkt: CDS message buffer to be returned to free queue.
 *
 * Return: none
 */
void
cds_free_ol_rx_pkt(p_cds_sched_context pSchedContext,
		    struct cds_ol_rx_pkt *pkt)
{
	memset(pkt, 0, sizeof(*pkt));
	spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	list_add_tail(&pkt->list, &pSchedContext->cds_ol_rx_pkt_freeq);
	spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
}

/**
 * cds_alloc_ol_rx_pkt() - API to return next available cds message
 * @pSchedContext: Pointer to the global CDS Sched Context
 *
 * This api returns next available cds message buffer used for rx data
 * processing
 *
 * Return: Pointer to cds message buffer
 */
struct cds_ol_rx_pkt *cds_alloc_ol_rx_pkt(p_cds_sched_context pSchedContext)
{
	struct cds_ol_rx_pkt *pkt;

	spin_lock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	if (list_empty(&pSchedContext->cds_ol_rx_pkt_freeq)) {
		spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
		return NULL;
	}
	pkt = list_first_entry(&pSchedContext->cds_ol_rx_pkt_freeq,
			       struct cds_ol_rx_pkt, list);
	list_del(&pkt->list);
	spin_unlock_bh(&pSchedContext->cds_ol_rx_pkt_freeq_lock);
	return pkt;
}

/**
 * cds_indicate_rxpkt() - indicate rx data packet
 * @Arg: Pointer to the global CDS Sched Context
 * @pkt: CDS data message buffer
 *
 * This api enqueues the rx packet into ol_rx_thread_queue and notifies
 * cds_ol_rx_thread()
 *
 * Return: none
 */
void
cds_indicate_rxpkt(p_cds_sched_context pSchedContext,
		   struct cds_ol_rx_pkt *pkt)
{
	spin_lock_bh(&pSchedContext->ol_rx_queue_lock);
	list_add_tail(&pkt->list, &pSchedContext->ol_rx_thread_queue);
	spin_unlock_bh(&pSchedContext->ol_rx_queue_lock);
	set_bit(RX_POST_EVENT, &pSchedContext->ol_rx_event_flag);
	wake_up_interruptible(&pSchedContext->ol_rx_wait_queue);
}

/**
 * cds_close_rx_thread() - close the Rx thread
 *
 * This api closes the Rx thread:
 *
 * Return: qdf status
 */
QDF_STATUS cds_close_rx_thread(void)
{
	cds_debug("invoked");

	if (!gp_cds_sched_context) {
		cds_err("!gp_cds_sched_context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!gp_cds_sched_context->ol_rx_thread)
		return QDF_STATUS_SUCCESS;

	/* Shut down Tlshim Rx thread */
	set_bit(RX_SHUTDOWN_EVENT, &gp_cds_sched_context->ol_rx_event_flag);
	set_bit(RX_POST_EVENT, &gp_cds_sched_context->ol_rx_event_flag);
	wake_up_interruptible(&gp_cds_sched_context->ol_rx_wait_queue);
	wait_for_completion(&gp_cds_sched_context->ol_rx_shutdown);
	gp_cds_sched_context->ol_rx_thread = NULL;
	cds_drop_rxpkt_by_staid(gp_cds_sched_context, WLAN_MAX_STA_COUNT);
	cds_free_ol_rx_pkt_freeq(gp_cds_sched_context);
	qdf_cpuhp_unregister(&gp_cds_sched_context->cpuhp_event_handle);

	return QDF_STATUS_SUCCESS;
} /* cds_close_rx_thread */

/**
 * cds_drop_rxpkt_by_staid() - api to drop pending rx packets for a sta
 * @pSchedContext: Pointer to the global CDS Sched Context
 * @staId: Station Id
 *
 * This api drops queued packets for a station, to drop all the pending
 * packets the caller has to send WLAN_MAX_STA_COUNT as staId.
 *
 * Return: none
 */
void cds_drop_rxpkt_by_staid(p_cds_sched_context pSchedContext, uint16_t staId)
{
	struct list_head local_list;
	struct cds_ol_rx_pkt *pkt, *tmp;
	qdf_nbuf_t buf, next_buf;

	INIT_LIST_HEAD(&local_list);
	spin_lock_bh(&pSchedContext->ol_rx_queue_lock);
	if (list_empty(&pSchedContext->ol_rx_thread_queue)) {
		spin_unlock_bh(&pSchedContext->ol_rx_queue_lock);
		return;
	}
	list_for_each_entry_safe(pkt, tmp, &pSchedContext->ol_rx_thread_queue,
								list) {
		if (pkt->staId == staId || staId == WLAN_MAX_STA_COUNT)
			list_move_tail(&pkt->list, &local_list);
	}
	spin_unlock_bh(&pSchedContext->ol_rx_queue_lock);

	list_for_each_entry_safe(pkt, tmp, &local_list, list) {
		list_del(&pkt->list);
		buf = pkt->Rxpkt;
		while (buf) {
			next_buf = qdf_nbuf_queue_next(buf);
			qdf_nbuf_free(buf);
			buf = next_buf;
		}
		cds_free_ol_rx_pkt(pSchedContext, pkt);
	}
}

/**
 * cds_rx_from_queue() - function to process pending Rx packets
 * @pSchedContext: Pointer to the global CDS Sched Context
 *
 * This api traverses the pending buffer list and calling the callback.
 * This callback would essentially send the packet to HDD.
 *
 * Return: none
 */
static void cds_rx_from_queue(p_cds_sched_context pSchedContext)
{
	struct cds_ol_rx_pkt *pkt;
	uint16_t sta_id;

	spin_lock_bh(&pSchedContext->ol_rx_queue_lock);
	while (!list_empty(&pSchedContext->ol_rx_thread_queue)) {
		pkt = list_first_entry(&pSchedContext->ol_rx_thread_queue,
				       struct cds_ol_rx_pkt, list);
		list_del(&pkt->list);
		spin_unlock_bh(&pSchedContext->ol_rx_queue_lock);
		sta_id = pkt->staId;
		pkt->callback(pkt->context, pkt->Rxpkt, sta_id);
		cds_free_ol_rx_pkt(pSchedContext, pkt);
		spin_lock_bh(&pSchedContext->ol_rx_queue_lock);
	}
	spin_unlock_bh(&pSchedContext->ol_rx_queue_lock);
}

/**
 * cds_ol_rx_thread() - cds main tlshim rx thread
 * @Arg: pointer to the global CDS Sched Context
 *
 * This api is the thread handler for Tlshim Data packet processing.
 *
 * Return: thread exit code
 */
static int cds_ol_rx_thread(void *arg)
{
	p_cds_sched_context pSchedContext = (p_cds_sched_context) arg;
	bool shutdown = false;
	int status;

#ifdef RX_THREAD_PRIORITY
	struct sched_param scheduler_params = {0};

	scheduler_params.sched_priority = 1;
	sched_setscheduler(current, SCHED_FIFO, &scheduler_params);
#else
	set_user_nice(current, -1);
#endif

	qdf_set_wake_up_idle(true);

	complete(&pSchedContext->ol_rx_start_event);

	while (!shutdown) {
		status =
			wait_event_interruptible(pSchedContext->ol_rx_wait_queue,
						 test_bit(RX_POST_EVENT,
							  &pSchedContext->ol_rx_event_flag)
						 || test_bit(RX_SUSPEND_EVENT,
							     &pSchedContext->ol_rx_event_flag));
		if (status == -ERESTARTSYS)
			break;

		clear_bit(RX_POST_EVENT, &pSchedContext->ol_rx_event_flag);
		while (true) {
			if (test_bit(RX_SHUTDOWN_EVENT,
				     &pSchedContext->ol_rx_event_flag)) {
				clear_bit(RX_SHUTDOWN_EVENT,
					  &pSchedContext->ol_rx_event_flag);
				if (test_bit(RX_SUSPEND_EVENT,
					     &pSchedContext->ol_rx_event_flag)) {
					clear_bit(RX_SUSPEND_EVENT,
						  &pSchedContext->ol_rx_event_flag);
					complete
						(&pSchedContext->ol_suspend_rx_event);
				}
				cds_debug("Shutting down OL RX Thread");
				shutdown = true;
				break;
			}
			cds_rx_from_queue(pSchedContext);

			if (test_bit(RX_SUSPEND_EVENT,
				     &pSchedContext->ol_rx_event_flag)) {
				clear_bit(RX_SUSPEND_EVENT,
					  &pSchedContext->ol_rx_event_flag);
				spin_lock(&pSchedContext->ol_rx_thread_lock);
				INIT_COMPLETION
					(pSchedContext->ol_resume_rx_event);
				complete(&pSchedContext->ol_suspend_rx_event);
				spin_unlock(&pSchedContext->ol_rx_thread_lock);
				wait_for_completion_interruptible
					(&pSchedContext->ol_resume_rx_event);
			}
			break;
		}
	}

	cds_debug("Exiting CDS OL rx thread");
	complete_and_exit(&pSchedContext->ol_rx_shutdown, 0);

	return 0;
}

void cds_resume_rx_thread(void)
{
	p_cds_sched_context cds_sched_context;

	cds_sched_context = get_cds_sched_ctxt();
	if (!cds_sched_context) {
		cds_err("cds_sched_context is NULL");
		return;
	}

	complete(&cds_sched_context->ol_resume_rx_event);
}
#endif

/**
 * cds_sched_close() - close the cds scheduler
 *
 * This api closes the CDS Scheduler upon successful closing:
 *	- All the message queues are flushed
 *	- The Main Controller thread is closed
 *	- The Tx thread is closed
 *
 *
 * Return: qdf status
 */
QDF_STATUS cds_sched_close(void)
{
	cds_debug("invoked");

	if (!gp_cds_sched_context) {
		cds_err("!gp_cds_sched_context");
		return QDF_STATUS_E_FAILURE;
	}

	cds_close_rx_thread();

	gp_cds_sched_context = NULL;
	return QDF_STATUS_SUCCESS;
} /* cds_sched_close() */

/**
 * get_cds_sched_ctxt() - get cds scheduler context
 *
 * Return: none
 */
p_cds_sched_context get_cds_sched_ctxt(void)
{
	/* Make sure that Vos Scheduler context has been initialized */
	if (!gp_cds_sched_context)
		cds_err("!gp_cds_sched_context");

	return gp_cds_sched_context;
}

/**
 * cds_ssr_protect_init() - initialize ssr protection debug functionality
 *
 * Return:
 *        void
 */
void cds_ssr_protect_init(void)
{
	spin_lock_init(&ssr_protect_lock);
	INIT_LIST_HEAD(&shutdown_notifier_head);
}

/**
 * cds_shutdown_notifier_register() - Register for shutdown notification
 * @cb          : Call back to be called
 * @priv        : Private pointer to be passed back to call back
 *
 * During driver remove or shutdown (recovery), external threads might be stuck
 * waiting on some event from firmware at lower layers. Remove or shutdown can't
 * proceed till the thread completes to avoid any race condition. Call backs can
 * be registered here to get early notification of remove or shutdown so that
 * waiting thread can be unblocked and hence remove or shutdown can proceed
 * further as waiting there may not make sense when FW may already have been
 * down.
 *
 * This is intended for early notification of remove() or shutdown() only so
 * that lower layers can take care of stuffs like external waiting thread.
 *
 * Return: CDS status
 */
QDF_STATUS cds_shutdown_notifier_register(void (*cb)(void *priv), void *priv)
{
	struct shutdown_notifier *notifier;
	unsigned long irq_flags;

	notifier = qdf_mem_malloc(sizeof(*notifier));

	if (!notifier)
		return QDF_STATUS_E_NOMEM;

	/*
	 * This logic can be simpilfied if there is separate state maintained
	 * for shutdown and reinit. Right now there is only recovery in progress
	 * state and it doesn't help to check against it as during reinit some
	 * of the modules may need to register the call backs.
	 * For now this logic added to avoid notifier registration happen while
	 * this function is trying to call the call back with the notification.
	 */
	spin_lock_irqsave(&ssr_protect_lock, irq_flags);
	if (notifier_state == NOTIFIER_STATE_NOTIFYING) {
		spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);
		qdf_mem_free(notifier);
		return -EINVAL;
	}

	notifier->cb = cb;
	notifier->priv = priv;

	list_add_tail(&notifier->list, &shutdown_notifier_head);
	spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);

	return 0;
}

/**
 * cds_shutdown_notifier_purge() - Purge all the notifiers
 *
 * Shutdown notifiers are added to provide the early notification of remove or
 * shutdown being initiated. Adding this API to purge all the registered call
 * backs as they are not useful any more while all the lower layers are being
 * shutdown.
 *
 * Return: None
 */
void cds_shutdown_notifier_purge(void)
{
	struct shutdown_notifier *notifier, *temp;
	unsigned long irq_flags;

	spin_lock_irqsave(&ssr_protect_lock, irq_flags);
	list_for_each_entry_safe(notifier, temp,
				 &shutdown_notifier_head, list) {
		list_del(&notifier->list);
		spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);

		qdf_mem_free(notifier);

		spin_lock_irqsave(&ssr_protect_lock, irq_flags);
	}

	spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);
}

/**
 * cds_shutdown_notifier_call() - Call shutdown notifier call back
 *
 * Call registered shutdown notifier call back to indicate about remove or
 * shutdown.
 */
void cds_shutdown_notifier_call(void)
{
	struct shutdown_notifier *notifier;
	unsigned long irq_flags;

	spin_lock_irqsave(&ssr_protect_lock, irq_flags);
	notifier_state = NOTIFIER_STATE_NOTIFYING;

	list_for_each_entry(notifier, &shutdown_notifier_head, list) {
		spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);

		notifier->cb(notifier->priv);

		spin_lock_irqsave(&ssr_protect_lock, irq_flags);
	}

	notifier_state = NOTIFIER_STATE_NONE;
	spin_unlock_irqrestore(&ssr_protect_lock, irq_flags);
}

/**
 * cds_get_gfp_flags(): get GFP flags
 *
 * Based on the scheduled context, return GFP flags
 * Return: gfp flags
 */
int cds_get_gfp_flags(void)
{
	int flags = GFP_KERNEL;

	if (in_interrupt() || in_atomic() || irqs_disabled())
		flags = GFP_ATOMIC;

	return flags;
}

