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

/*
 * NB: Inappropriate references to "HTC" are used in this (and other)
 * HIF implementations.  HTC is typically the calling layer, but it
 * theoretically could be some alternative.
 */

/*
 * This holds all state needed to process a pending send/recv interrupt.
 * The information is saved here as soon as the interrupt occurs (thus
 * allowing the underlying CE to re-use the ring descriptor). The
 * information here is eventually processed by a completion processing
 * thread.
 */

#include <adf_os_atomic.h> /* adf_os_atomic_read */
#include "vos_lock.h"
struct HIF_CE_completion_state {
    struct HIF_CE_completion_state *next;
    int send_or_recv;
    struct CE_handle *copyeng;
    void *ce_context;
    void *transfer_context;
    CE_addr_t data;
    unsigned int nbytes;
    unsigned int transfer_id;
    unsigned int flags;
};

/* compl_state.send_or_recv */
#define HIF_CE_COMPLETE_FREE 0
#define HIF_CE_COMPLETE_SEND 1
#define HIF_CE_COMPLETE_RECV 2

enum ol_ath_hif_pkt_ecodes {
    HIF_PIPE_NO_RESOURCE=0
};

struct HIF_CE_state ;

#define HIF_CE_COMPLETE_STATE_NUM 18 /* 56 * 18 + 4/8 = 1012/1016 bytes */
struct HIF_CE_completion_state_list {
    struct HIF_CE_completion_state_list *next;
};
/* Per-pipe state. */
struct HIF_CE_pipe_info {
    /* Handle of underlying Copy Engine */
    struct CE_handle *ce_hdl;

    /* Our pipe number; facilitiates use of pipe_info ptrs. */
    A_UINT8 pipe_num;

    /* Convenience back pointer to HIF_CE_state. */
    struct HIF_CE_state *HIF_CE_state;

    /* Instantaneous number of receive buffers that should be posted */
    atomic_t recv_bufs_needed;
    adf_os_size_t buf_sz;
    adf_os_spinlock_t  recv_bufs_needed_lock;

    adf_os_spinlock_t completion_freeq_lock;
    /* Limit the number of outstanding send requests. */
    int num_sends_allowed;
    struct HIF_CE_completion_state_list *completion_space_list;
    struct HIF_CE_completion_state *completion_freeq_head;
    struct HIF_CE_completion_state *completion_freeq_tail;
    /* adding three counts for debugging ring buffer errors */
    uint32_t nbuf_alloc_err_count;
    uint32_t nbuf_dma_err_count;
    uint32_t nbuf_ce_enqueue_err_count;
} ;

struct HIF_CE_state {
    struct hif_pci_softc *sc;
    A_BOOL started;

    adf_os_spinlock_t keep_awake_lock;
    adf_os_spinlock_t suspend_lock;
    unsigned int keep_awake_count;
    A_BOOL verified_awake;
    A_BOOL fake_sleep;
    adf_os_timer_t sleep_timer;
    unsigned long sleep_ticks;

    //struct task_struct *pci_dev_inserted_thread;
    //struct completion pci_dev_inserted_thread_done;

    //wait_queue_head_t replenish_recv_buf_waitq;
    //atomic_t replenish_recv_buf_flag;
    //struct task_struct *replenish_thread;
    //struct completion post_recv_bufs_thread_done;

    adf_os_spinlock_t completion_pendingq_lock;
    /* Queue of send/recv completions that need to be processed */
    struct HIF_CE_completion_state *completion_pendingq_head;
    struct HIF_CE_completion_state *completion_pendingq_tail;
    atomic_t fw_event_pending;
    adf_os_atomic_t hif_thread_idle;

    //wait_queue_head_t service_waitq;
    //struct task_struct *compl_thread;
    //struct completion compl_thread_done;

    /* Per-pipe state. */
    struct HIF_CE_pipe_info  pipe_info[CE_COUNT_MAX];

    MSG_BASED_HIF_CALLBACKS msg_callbacks_pending; /* to be activated after BMI_DONE */
    MSG_BASED_HIF_CALLBACKS msg_callbacks_current; /* current msg callbacks in use */

    void *claimedContext;

    /* Target address used to signal a pending firmware event */
    u_int32_t fw_indicator_address;

    /* Copy Engine used for Diagnostic Accesses */
    struct CE_handle *ce_diag;

    /* For use with A_TARGET_ API */
    A_target_id_t targid;
};

void priv_start_agc(struct hif_pci_softc *sc);
void priv_dump_agc(struct hif_pci_softc *sc);
void priv_start_cap_chaninfo(struct hif_pci_softc *sc);
void priv_dump_chaninfo(struct hif_pci_softc *sc);
void priv_dump_bbwatchdog(struct hif_pci_softc *sc);
void hif_dump_pipe_debug_count(HIF_DEVICE *hif_device);

#ifdef FEATURE_RUNTIME_PM
#include <linux/pm_runtime.h>
void hif_pci_runtime_pm_timeout_fn(unsigned long data);
#ifdef WLAN_OPEN_SOURCE
static inline int hif_pm_request_resume(struct device *dev)
{
	return pm_request_resume(dev);
}
static inline int __hif_pm_runtime_get(struct device *dev)
{
	return pm_runtime_get(dev);
}

static inline int hif_pm_runtime_put_auto(struct device *dev)
{
	return pm_runtime_put_autosuspend(dev);
}

static inline void hif_pm_runtime_mark_last_busy(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);
}

static inline int hif_pm_runtime_resume(struct device *dev)
{
	return pm_runtime_resume(dev);
}
#else
static inline int hif_pm_request_resume(struct device *dev)
{
	return cnss_pm_runtime_request(dev, CNSS_PM_REQUEST_RESUME);
}

static inline int __hif_pm_runtime_get(struct device *dev)
{
	return cnss_pm_runtime_request(dev, CNSS_PM_RUNTIME_GET);
}

static inline int hif_pm_runtime_put_auto(struct device *dev)
{
	return cnss_pm_runtime_request(dev, CNSS_PM_RUNTIME_PUT_AUTO);
}

static inline void hif_pm_runtime_mark_last_busy(struct device *dev)
{
	cnss_pm_runtime_request(dev, CNSS_PM_RUNTIME_MARK_LAST_BUSY);
}
static inline int hif_pm_runtime_resume(struct device *dev)
{
	return cnss_pm_runtime_request(dev, CNSS_PM_RUNTIME_RESUME);
}
#endif /* WLAN_OPEN_SOURCE */
#else /* FEATURE_RUNTIME_PM */
static inline void hif_pm_runtime_mark_last_busy(struct device *dev) { }
#endif

#define CE_HTT_T2H_MSG 1
#define CE_HTT_H2T_MSG 4
