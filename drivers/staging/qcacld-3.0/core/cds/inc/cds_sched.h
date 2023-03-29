/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#ifndef __CDS_SCHED_H
#define __CDS_SCHED_H

/**=========================================================================

   \file  cds_sched.h

   \brief Connectivity driver services scheduler

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include <qdf_event.h>
#include <i_qdf_types.h>
#include <linux/wait.h>
#if defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#include <qdf_types.h>
#include "qdf_lock.h"
#include "qdf_mc_timer.h"
#include "cds_config.h"
#include "qdf_cpuhp.h"

#define MC_SUSPEND_EVENT            0x002
#define RX_POST_EVENT               0x001
#define RX_SUSPEND_EVENT            0x002
#define RX_VDEV_DEL_EVENT           0x004
#define RX_SHUTDOWN_EVENT           0x010

#define RX_REFILL_POST_EVENT           0x001
#define RX_REFILL_SUSPEND_EVENT        0x002
#define RX_REFILL_SHUTDOWN_EVENT       0x004

#ifdef QCA_CONFIG_SMP
/*
** Maximum number of cds messages to be allocated for
** OL Rx thread.
*/
#define CDS_MAX_OL_RX_PKT 4000
#endif

typedef void (*cds_ol_rx_thread_cb)(void *context,
				    qdf_nbuf_t rxpkt,
				    uint16_t staid);

/*
** CDS message wrapper for data rx from TXRX
*/
struct cds_ol_rx_pkt {
	struct list_head list;
	void *context;

	/* Rx skb */
	qdf_nbuf_t Rxpkt;

	/* Station id to which this packet is destined */
	uint16_t staId;

	/* Call back to further send this packet to txrx layer */
	cds_ol_rx_thread_cb callback;

};

/*
** CDS Scheduler context
** The scheduler context contains the following:
**   ** the messages queues
**   ** the handle to the tread
**   ** pointer to the events that gracefully shutdown the MC and Tx threads
**
*/
typedef struct _cds_sched_context {
#ifdef QCA_CONFIG_SMP
	spinlock_t ol_rx_thread_lock;

	/* OL Rx thread handle */
	struct task_struct *ol_rx_thread;

	/* Handle of Event for Rx thread to signal startup */
	struct completion ol_rx_start_event;

	/* Completion object to suspend OL rx thread */
	struct completion ol_suspend_rx_event;

	/* Completion object to resume OL rx thread */
	struct completion ol_resume_rx_event;

	/* Completion object for OL Rxthread shutdown */
	struct completion ol_rx_shutdown;

	/* Waitq for OL Rx thread */
	wait_queue_head_t ol_rx_wait_queue;

	unsigned long ol_rx_event_flag;

	/* Rx buffer queue */
	struct list_head ol_rx_thread_queue;

	/* Spinlock to synchronize between tasklet and thread */
	spinlock_t ol_rx_queue_lock;

	/* Lock to synchronize free buffer queue access */
	spinlock_t cds_ol_rx_pkt_freeq_lock;

	/* Free message queue for OL Rx processing */
	struct list_head cds_ol_rx_pkt_freeq;

	/* The CPU hotplug event registration handle, used to unregister */
	struct qdf_cpuhp_handler *cpuhp_event_handle;

	/* affinity lock */
	struct mutex affinity_lock;

	/* Saved rx thread CPU affinity */
	struct cpumask rx_thread_cpu_mask;

	/* CPU affinity bitmask */
	uint8_t conf_rx_thread_cpu_mask;

	/* high throughput required */
	bool high_throughput_required;

	/* affinity requied during uplink traffic*/
	bool rx_affinity_required;
	uint8_t conf_rx_thread_ul_affinity;
#endif
} cds_sched_context, *p_cds_sched_context;

/**
 * struct cds_log_complete - Log completion internal structure
 * @is_fatal: Type is fatal or not
 * @indicator: Source of bug report
 * @reason_code: Reason code for bug report
 * @is_report_in_progress: If bug report is in progress
 * @recovery_needed: if recovery is needed after report completion
 *
 * This structure internally stores the log related params
 */
struct cds_log_complete {
	uint32_t is_fatal;
	uint32_t indicator;
	uint32_t reason_code;
	bool is_report_in_progress;
	bool recovery_needed;
};

struct cds_context {
	/* Scheduler Context */
	cds_sched_context qdf_sched;

	/* HDD Module Context  */
	void *hdd_context;

	/* MAC Module Context  */
	void *mac_context;

	uint32_t driver_state;

	/* WMA Context */
	void *wma_context;

	void *hif_context;

	void *htc_ctx;

	void *g_ol_context;
	/*
	 * qdf_ctx will be used by qdf
	 * while allocating dma memory
	 * to access dev information.
	 */
	qdf_device_t qdf_ctx;

	void *dp_soc;

	void *dp_mem_pre_alloc_ctx;

	/* Configuration handle used to get system configuration */
	struct cdp_cfg *cfg_ctx;

	/* radio index per driver */
	int radio_index;

	bool is_wakelock_log_enabled;
	uint32_t wakelock_log_level;
	uint32_t connectivity_log_level;
	uint32_t packet_stats_log_level;
	uint32_t driver_debug_log_level;
	uint32_t fw_debug_log_level;
	struct cds_log_complete log_complete;
	qdf_spinlock_t bug_report_lock;

	bool enable_fatal_event;
	struct cds_config_info *cds_cfg;

	struct ol_tx_sched_wrr_ac_specs_t ac_specs[QCA_WLAN_AC_ALL];
	qdf_work_t cds_recovery_work;
	qdf_workqueue_t *cds_recovery_wq;
	enum qdf_hang_reason recovery_reason;
};

/*---------------------------------------------------------------------------
   Function declarations and documenation
   ---------------------------------------------------------------------------*/
#ifdef QCA_CONFIG_SMP
int cds_sched_handle_cpu_hot_plug(void);
int cds_sched_handle_throughput_req(bool high_tput_required);

/**
 * cds_sched_handle_rx_thread_affinity_req - rx thread affinity req handler
 * @high_tput_required: high throughput is required or not
 *
 * rx thread affinity handler will find online cores and
 * will assign proper core based on perf requirement
 *
 * Return: None
 */
void cds_sched_handle_rx_thread_affinity_req(bool high_throughput);

/**
 * cds_set_rx_thread_ul_cpu_mask() - Rx_thread affinity for UL from INI
 * @cpu_affinity_mask: CPU affinity bitmap
 *
 * Return:None
 */
void cds_set_rx_thread_ul_cpu_mask(uint8_t cpu_affinity_mask);

/**
 * cds_set_rx_thread_cpu_mask() - Rx_thread affinity from INI
 * @cpu_affinity_mask: CPU affinity bitmap
 *
 * Return:None
 */
void cds_set_rx_thread_cpu_mask(uint8_t cpu_affinity_mask);

/*---------------------------------------------------------------------------
   \brief cds_drop_rxpkt_by_staid() - API to drop pending Rx packets for a sta
   The \a cds_drop_rxpkt_by_staid() drops queued packets for a station, to drop
   all the pending packets the caller has to send WLAN_MAX_STA_COUNT as staId.
   \param  pSchedContext - pointer to the global CDS Sched Context
   \param staId - Station Id

   \return Nothing
   \sa cds_drop_rxpkt_by_staid()
   -------------------------------------------------------------------------*/
void cds_drop_rxpkt_by_staid(p_cds_sched_context pSchedContext, uint16_t staId);

/*---------------------------------------------------------------------------
   \brief cds_indicate_rxpkt() - API to Indicate rx data packet
   The \a cds_indicate_rxpkt() enqueues the rx packet onto ol_rx_thread_queue
   and notifies cds_ol_rx_thread().
   \param  Arg - pointer to the global CDS Sched Context
   \param pkt - Vos data message buffer

   \return Nothing
   \sa cds_indicate_rxpkt()
   -------------------------------------------------------------------------*/
void cds_indicate_rxpkt(p_cds_sched_context pSchedContext,
			struct cds_ol_rx_pkt *pkt);

/**
 * cds_close_rx_thread() - close the Rx thread
 *
 * This api closes the Rx thread:
 *
 * Return: qdf status
 */
QDF_STATUS cds_close_rx_thread(void);

/*---------------------------------------------------------------------------
   \brief cds_alloc_ol_rx_pkt() - API to return next available cds message
   The \a cds_alloc_ol_rx_pkt() returns next available cds message buffer
   used for Rx Data processing.
   \param pSchedContext - pointer to the global CDS Sched Context

   \return pointer to cds message buffer
   \sa cds_alloc_ol_rx_pkt()
   -------------------------------------------------------------------------*/
struct cds_ol_rx_pkt *cds_alloc_ol_rx_pkt(p_cds_sched_context pSchedContext);

/*---------------------------------------------------------------------------
   \brief cds_free_ol_rx_pkt() - API to release cds message to the freeq
   The \a cds_free_ol_rx_pkt() returns the cds message used for Rx data
   to the free queue.
   \param  pSchedContext - pointer to the global CDS Sched Context
   \param  pkt - Vos message buffer to be returned to free queue.

   \return Nothing
   \sa cds_free_ol_rx_pkt()
   -------------------------------------------------------------------------*/
void cds_free_ol_rx_pkt(p_cds_sched_context pSchedContext,
			 struct cds_ol_rx_pkt *pkt);
/*---------------------------------------------------------------------------
   \brief cds_free_ol_rx_pkt_freeq() - Free cdss buffer free queue
   The \a cds_free_ol_rx_pkt_freeq() does mem free of the buffers
   available in free cds buffer queue which is used for Data rx processing
   from Tlshim.
   \param pSchedContext - pointer to the global CDS Sched Context

   \return Nothing
   \sa cds_free_ol_rx_pkt_freeq()
   -------------------------------------------------------------------------*/
void cds_free_ol_rx_pkt_freeq(p_cds_sched_context pSchedContext);
#else
/**
 * cds_sched_handle_rx_thread_affinity_req - rx thread affinity req handler
 * @high_tput_required: high throughput is required or not
 *
 * rx thread affinity handler will find online cores and
 * will assign proper core based on perf requirement
 *
 * Return: None
 */
static inline void cds_sched_handle_rx_thread_affinity_req(
	bool high_throughput) {}

/**
 * cds_set_rx_thread_ul_cpu_mask() - Rx_thread affinity for UL from INI
 * @cpu_affinity_mask: CPU affinity bitmap
 *
 * Return:None
 */
static inline void cds_set_rx_thread_ul_cpu_mask(uint8_t cpu_affinity_mask) {}

/**
 * cds_set_rx_thread_cpu_mask() - Rx_thread affinity from INI
 * @cpu_affinity_mask: CPU affinity bitmap
 *
 * Return:None
 */
static inline void cds_set_rx_thread_cpu_mask(uint8_t cpu_affinity_mask) {}

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
static inline
void cds_drop_rxpkt_by_staid(p_cds_sched_context pSchedContext, uint16_t staId)
{
}

/**
 * cds_indicate_rxpkt() - API to Indicate rx data packet
 * @pSchedContext: pointer to  CDS Sched Context
 * @pkt: CDS OL RX pkt pointer containing to RX data message buffer
 *
 * Return: none
 */
static inline
void cds_indicate_rxpkt(p_cds_sched_context pSchedContext,
			struct cds_ol_rx_pkt *pkt)
{
}

/**
 * cds_close_rx_thread() - close the Rx thread
 *
 * This api closes the Rx thread:
 *
 * Return: qdf status
 */
static inline
QDF_STATUS cds_close_rx_thread(void)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * cds_alloc_ol_rx_pkt() - API to return next available cds message
 * @pSchedContext: pointer to  CDS Sched Context
 *
 * Return: none
 */
static inline
struct cds_ol_rx_pkt *cds_alloc_ol_rx_pkt(p_cds_sched_context pSchedContext)
{
	return NULL;
}

/**
 * cds_free_ol_rx_pkt() - API to release cds message to the freeq
 * @pSchedContext: pointer to  CDS Sched Context
 * @pkt: CDS message buffer to be returned to free queue
 *
 * Return: none
 */
static inline
void cds_free_ol_rx_pkt(p_cds_sched_context pSchedContext,
			 struct cds_ol_rx_pkt *pkt)
{
}

/**
 * cds_free_ol_rx_pkt_freeq() - Free cds buffer free queue
 * @pSchedContext: pointer to  CDS Sched Context
 * @pkt: CDS message buffer to be returned to free queue
 *
 * Return: none
 */
static inline
void cds_free_ol_rx_pkt_freeq(p_cds_sched_context pSchedContext)
{
}

static inline int cds_sched_handle_throughput_req(
	bool high_tput_required)
{
	return 0;
}

#endif

/*---------------------------------------------------------------------------

   \brief cds_sched_open() - initialize the CDS Scheduler

   The \a cds_sched_open() function initializes the CDS Scheduler
   Upon successful initialization:

     - All the message queues are initialized

     - The Main Controller thread is created and ready to receive and
       dispatch messages.

     - The Tx thread is created and ready to receive and dispatch messages

   \param  p_cds_context - pointer to the global QDF Context

   \param  p_cds_sched_context - pointer to a previously allocated buffer big
   enough to hold a scheduler context.

   \return QDF_STATUS_SUCCESS - Scheduler was successfully initialized and
   is ready to be used.

   QDF_STATUS_E_RESOURCES - System resources (other than memory)
   are unavailable to initialize the scheduler

   QDF_STATUS_E_NOMEM - insufficient memory exists to initialize
   the scheduler

   QDF_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
   function

   QDF_STATUS_E_FAILURE - Failure to initialize the scheduler/

   \sa cds_sched_open()

   -------------------------------------------------------------------------*/
QDF_STATUS cds_sched_open(void *p_cds_context,
			  p_cds_sched_context pSchedCxt, uint32_t SchedCtxSize);

/*---------------------------------------------------------------------------

   \brief cds_sched_close() - Close the CDS Scheduler

   The \a cds_sched_closes() function closes the CDS Scheduler
   Upon successful closing:

     - All the message queues are flushed

     - The Main Controller thread is closed

     - The Tx thread is closed

   \return QDF_STATUS_SUCCESS - Scheduler was successfully initialized and
   is ready to be used.

   QDF_STATUS_E_INVAL - Invalid parameter passed to the scheduler Open
   function

   QDF_STATUS_E_FAILURE - Failure to initialize the scheduler/

   \sa cds_sched_close()

   ---------------------------------------------------------------------------*/
QDF_STATUS cds_sched_close(void);

p_cds_sched_context get_cds_sched_ctxt(void);

void qdf_timer_module_init(void);
void qdf_timer_module_deinit(void);
void cds_ssr_protect_init(void);
int cds_get_gfp_flags(void);

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
 * Return: CDS status
 */
QDF_STATUS cds_shutdown_notifier_register(void (*cb)(void *priv), void *priv);

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
void cds_shutdown_notifier_purge(void);

/**
 * cds_shutdown_notifier_call() - Call shutdown notifier call back
 *
 * Call registered shutdown notifier call back to indicate about remove or
 * shutdown.
 */
void cds_shutdown_notifier_call(void);

/**
 * cds_resume_rx_thread() - resume rx thread by completing its resume event
 *
 * Resume RX thread by completing RX thread resume event
 *
 * Return: None
 */
void cds_resume_rx_thread(void);

#endif /* #ifndef __CDS_SCHED_H */
