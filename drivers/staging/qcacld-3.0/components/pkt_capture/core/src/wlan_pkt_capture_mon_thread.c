/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * DOC: Define internal APIs related to the packet capture component
 */

#include "wlan_pkt_capture_mon_thread.h"
#include "cds_ieee80211_common.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "cdp_txrx_ctrl.h"
#include "cfg_ucfg_api.h"

void pkt_capture_mon(struct pkt_capture_cb_context *cb_ctx, qdf_nbuf_t msdu,
		     struct wlan_objmgr_vdev *vdev, uint16_t ch_freq)
{
	struct radiotap_header *rthdr;
	uint8_t rtlen, type, sub_type;
	struct ieee80211_frame *wh;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	cdp_config_param_type val;

	rthdr = (struct radiotap_header *)qdf_nbuf_data(msdu);
	rtlen = rthdr->it_len;
	wh = (struct ieee80211_frame *)(qdf_nbuf_data(msdu) + rtlen);
	type = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	sub_type = (wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if ((type == IEEE80211_FC0_TYPE_MGT) &&
	    (sub_type == MGMT_SUBTYPE_AUTH)) {
		uint8_t chan = wlan_freq_to_chan(ch_freq);

		val.cdp_pdev_param_monitor_chan = chan;
		cdp_txrx_set_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(pdev),
					CDP_MONITOR_CHANNEL, val);

		val.cdp_pdev_param_mon_freq = ch_freq;
		cdp_txrx_set_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(pdev),
					CDP_MONITOR_FREQUENCY, val);
	}

	if (cb_ctx->mon_cb(cb_ctx->mon_ctx, msdu) != QDF_STATUS_SUCCESS) {
		pkt_capture_err("Frame Rx to HDD failed");
		qdf_nbuf_free(msdu);
	}
}

void pkt_capture_free_mon_pkt_freeq(struct pkt_capture_mon_context *mon_ctx)
{
	struct pkt_capture_mon_pkt *pkt;

	spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	while (!list_empty(&mon_ctx->mon_pkt_freeq)) {
		pkt = list_entry((&mon_ctx->mon_pkt_freeq)->next,
				 typeof(*pkt), list);
		list_del(&pkt->list);
		spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
		qdf_mem_free(pkt);
		spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	}
	spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
}

/**
 * pkt_capture_alloc_mon_pkt_freeq() - Function to allocate free buffer queue
 * @mon_ctx: pointer to packet capture mon context
 *
 * This API allocates MAX_MON_PKT_SIZE number of mon packets
 * which are used for mon data processing.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
pkt_capture_alloc_mon_pkt_freeq(struct pkt_capture_mon_context *mon_ctx)
{
	struct pkt_capture_mon_pkt *pkt, *tmp;
	int i;

	for (i = 0; i < MAX_MON_PKT_SIZE; i++) {
		pkt = qdf_mem_malloc(sizeof(*pkt));
		if (!pkt)
			goto free;

		spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
		list_add_tail(&pkt->list,
			      &mon_ctx->mon_pkt_freeq);
		spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
	}

	return QDF_STATUS_SUCCESS;
free:
	spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	list_for_each_entry_safe(pkt, tmp,
				 &mon_ctx->mon_pkt_freeq,
				 list) {
		list_del(&pkt->list);
		spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
		qdf_mem_free(pkt);
		spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	}
	spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);

	return QDF_STATUS_E_NOMEM;
}

/**
 * pkt_capture_free_mon_pkt() - api to release mon packet to the freeq
 * @mon_ctx: Pointer to packet capture mon context
 * @pkt: MON packet buffer to be returned to free queue.
 *
 * This api returns the mon packet used for mon data to the free queue
 *
 * Return: None
 */
static void
pkt_capture_free_mon_pkt(struct pkt_capture_mon_context *mon_ctx,
			 struct pkt_capture_mon_pkt *pkt)
{
	memset(pkt, 0, sizeof(*pkt));
	spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	list_add_tail(&pkt->list, &mon_ctx->mon_pkt_freeq);
	spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
}

struct pkt_capture_mon_pkt *
pkt_capture_alloc_mon_pkt(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_mon_context *mon_ctx;
	struct pkt_capture_mon_pkt *pkt;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return NULL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return NULL;
	}

	mon_ctx = vdev_priv->mon_ctx;
	if (!mon_ctx) {
		pkt_capture_err("packet capture mon context is NULL");
		return NULL;
	}

	spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	if (list_empty(&mon_ctx->mon_pkt_freeq)) {
		spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);
		return NULL;
	}

	pkt = list_first_entry(&mon_ctx->mon_pkt_freeq,
			       struct pkt_capture_mon_pkt, list);
	list_del(&pkt->list);
	spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);

	return pkt;
}

void pkt_capture_indicate_monpkt(struct wlan_objmgr_vdev *vdev,
				 struct pkt_capture_mon_pkt *pkt)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_mon_context *mon_ctx;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return;
	}
	mon_ctx = vdev_priv->mon_ctx;

	spin_lock_bh(&mon_ctx->mon_queue_lock);
	list_add_tail(&pkt->list, &mon_ctx->mon_thread_queue);
	spin_unlock_bh(&mon_ctx->mon_queue_lock);
	set_bit(PKT_CAPTURE_RX_POST_EVENT, &mon_ctx->mon_event_flag);
	wake_up_interruptible(&mon_ctx->mon_wait_queue);
}

void pkt_capture_wakeup_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_mon_context *mon_ctx;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return;
	}
	mon_ctx = vdev_priv->mon_ctx;

	set_bit(PKT_CAPTURE_RX_POST_EVENT, &mon_ctx->mon_event_flag);
	wake_up_interruptible(&mon_ctx->mon_wait_queue);
}

/**
 * pkt_capture_process_from_queue() - function to process pending mon packets
 * @mon_ctx: Pointer to packet capture mon context
 *
 * This api traverses the pending buffer list and calling the callback.
 * This callback would essentially send the packet to HDD.
 *
 * Return: None
 */
static void
pkt_capture_process_from_queue(struct pkt_capture_mon_context *mon_ctx)
{
	struct pkt_capture_mon_pkt *pkt;
	uint8_t vdev_id;
	uint8_t tid;

	spin_lock_bh(&mon_ctx->mon_queue_lock);
	while (!list_empty(&mon_ctx->mon_thread_queue)) {
		pkt = list_first_entry(&mon_ctx->mon_thread_queue,
				       struct pkt_capture_mon_pkt, list);
		list_del(&pkt->list);
		spin_unlock_bh(&mon_ctx->mon_queue_lock);
		vdev_id = pkt->vdev_id;
		tid = pkt->tid;
		pkt->callback(pkt->context, pkt->pdev, pkt->monpkt, vdev_id,
			      tid, pkt->status, pkt->pkt_format, pkt->bssid,
			      pkt->tx_retry_cnt);
		pkt_capture_free_mon_pkt(mon_ctx, pkt);
		spin_lock_bh(&mon_ctx->mon_queue_lock);
	}
	spin_unlock_bh(&mon_ctx->mon_queue_lock);
}

/**
 * pkt_capture_mon_thread() - packet capture mon thread
 * @arg: Pointer to vdev object manager
 *
 * This api is the thread handler for mon Data packet processing.
 *
 * Return: thread exit code
 */
static int pkt_capture_mon_thread(void *arg)
{
	struct pkt_capture_mon_context *mon_ctx;
	unsigned long pref_cpu = 0;
	bool shutdown = false;
	int status, i;

	if (!arg) {
		pkt_capture_err("Bad Args passed to mon thread");
		return 0;
	}
	mon_ctx = (struct pkt_capture_mon_context *)arg;
	set_user_nice(current, -1);
#ifdef MSM_PLATFORM
	set_wake_up_idle(true);
#endif

	/**
	 * Find the available cpu core other than cpu 0 and
	 * bind the thread
	 */
	for_each_online_cpu(i) {
		if (i == 0)
			continue;
		pref_cpu = i;
			break;
	}

	set_cpus_allowed_ptr(current, cpumask_of(pref_cpu));

	complete(&mon_ctx->mon_start_event);

	while (!shutdown) {
		status =
		wait_event_interruptible(mon_ctx->mon_wait_queue,
					 test_bit(PKT_CAPTURE_RX_POST_EVENT,
						  &mon_ctx->mon_event_flag) ||
					 test_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
						  &mon_ctx->mon_event_flag));
		if (status == -ERESTARTSYS)
			break;

		clear_bit(PKT_CAPTURE_RX_POST_EVENT,
			  &mon_ctx->mon_event_flag);
		while (true) {
			if (test_bit(PKT_CAPTURE_RX_SHUTDOWN_EVENT,
				     &mon_ctx->mon_event_flag)) {
				clear_bit(PKT_CAPTURE_RX_SHUTDOWN_EVENT,
					  &mon_ctx->mon_event_flag);
				if (test_bit(
					PKT_CAPTURE_RX_SUSPEND_EVENT,
					&mon_ctx->mon_event_flag)) {
					clear_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
						  &mon_ctx->mon_event_flag);
					complete(&mon_ctx->suspend_mon_event);
				}
				pkt_capture_info("Shutting down pktcap thread");
				shutdown = true;
				break;
			}

			/*
			 * if packet capture deregistratin happens stop
			 * processing packets in queue because mon cb will
			 * be set to NULL.
			 */
			if (test_bit(PKT_CAPTURE_REGISTER_EVENT,
				     &mon_ctx->mon_event_flag))
				pkt_capture_process_from_queue(mon_ctx);
			else
				complete(&mon_ctx->mon_register_event);

			if (test_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
				     &mon_ctx->mon_event_flag)) {
				clear_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
					  &mon_ctx->mon_event_flag);
				spin_lock(&mon_ctx->mon_thread_lock);
				INIT_COMPLETION(mon_ctx->resume_mon_event);
				complete(&mon_ctx->suspend_mon_event);
				spin_unlock(&mon_ctx->mon_thread_lock);
				wait_for_completion_interruptible
					(&mon_ctx->resume_mon_event);
			}
			break;
		}
	}
	pkt_capture_debug("Exiting packet capture mon thread");
	complete_and_exit(&mon_ctx->mon_shutdown, 0);

	return 0;
}

void pkt_capture_close_mon_thread(struct pkt_capture_mon_context *mon_ctx)
{
	if (!mon_ctx->mon_thread)
		return;

	/* Shut down mon thread */
	set_bit(PKT_CAPTURE_RX_SHUTDOWN_EVENT,
		&mon_ctx->mon_event_flag);
	set_bit(PKT_CAPTURE_RX_POST_EVENT,
		&mon_ctx->mon_event_flag);
	wake_up_interruptible(&mon_ctx->mon_wait_queue);
	wait_for_completion(&mon_ctx->mon_shutdown);
	mon_ctx->mon_thread = NULL;
	pkt_capture_drop_monpkt(mon_ctx);
	pkt_capture_free_mon_pkt_freeq(mon_ctx);
}

QDF_STATUS
pkt_capture_open_mon_thread(struct pkt_capture_mon_context *mon_ctx)
{
	mon_ctx->mon_thread = kthread_create(pkt_capture_mon_thread,
					     mon_ctx,
					     "pkt_capture_mon_thread");

	if (IS_ERR(mon_ctx->mon_thread)) {
		pkt_capture_fatal("Could not Create packet capture mon thread");
		return QDF_STATUS_E_FAILURE;
	}
	wake_up_process(mon_ctx->mon_thread);
	pkt_capture_debug("packet capture MON thread Created");

	wait_for_completion_interruptible(&mon_ctx->mon_start_event);
	pkt_capture_debug("packet capture MON Thread has started");

	return QDF_STATUS_SUCCESS;
}

void pkt_capture_drop_monpkt(struct pkt_capture_mon_context *mon_ctx)
{
	struct pkt_capture_mon_pkt *pkt, *tmp;
	struct list_head local_list;
	qdf_nbuf_t buf, next_buf;

	INIT_LIST_HEAD(&local_list);
	spin_lock_bh(&mon_ctx->mon_queue_lock);
	if (list_empty(&mon_ctx->mon_thread_queue)) {
		spin_unlock_bh(&mon_ctx->mon_queue_lock);
		return;
	}
	list_for_each_entry_safe(pkt, tmp,
				 &mon_ctx->mon_thread_queue,
				 list)
		list_move_tail(&pkt->list, &local_list);

	spin_unlock_bh(&mon_ctx->mon_queue_lock);

	list_for_each_entry_safe(pkt, tmp, &local_list, list) {
		list_del(&pkt->list);
		buf = pkt->monpkt;
		while (buf) {
			next_buf = qdf_nbuf_queue_next(buf);
			qdf_nbuf_free(buf);
			buf = next_buf;
		}
		pkt_capture_free_mon_pkt(mon_ctx, pkt);
	}
}

int pkt_capture_suspend_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_mon_context *mon_ctx;
	int rc;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return -EINVAL;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return -EINVAL;
	}
	mon_ctx = vdev_priv->mon_ctx;
	if (!mon_ctx) {
		pkt_capture_err("packet capture mon context is NULL");
		return -EINVAL;
	}

	set_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
		&mon_ctx->mon_event_flag);
	wake_up_interruptible(&mon_ctx->mon_wait_queue);
	rc = wait_for_completion_timeout(
			&mon_ctx->suspend_mon_event,
			msecs_to_jiffies(PKT_CAPTURE_SUSPEND_TIMEOUT));
	if (!rc) {
		clear_bit(PKT_CAPTURE_RX_SUSPEND_EVENT,
			  &mon_ctx->mon_event_flag);
		pkt_capture_err("Failed to suspend packet capture mon thread");
		return -EINVAL;
	}
	mon_ctx->is_mon_thread_suspended = true;

	return 0;
}

void pkt_capture_resume_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;
	struct pkt_capture_mon_context *mon_ctx;

	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return;
	}

	vdev_priv = pkt_capture_vdev_get_priv(vdev);
	if (!vdev_priv) {
		pkt_capture_err("packet capture vdev priv is NULL");
		return;
	}
	mon_ctx = vdev_priv->mon_ctx;
	if (!mon_ctx) {
		pkt_capture_err("packet capture mon context is NULL");
		return;
	}

	if (mon_ctx->is_mon_thread_suspended)
		complete(&mon_ctx->resume_mon_event);
}

QDF_STATUS
pkt_capture_alloc_mon_thread(struct pkt_capture_mon_context *mon_ctx)
{
	spin_lock_init(&mon_ctx->mon_thread_lock);
	init_waitqueue_head(&mon_ctx->mon_wait_queue);
	init_completion(&mon_ctx->mon_start_event);
	init_completion(&mon_ctx->suspend_mon_event);
	init_completion(&mon_ctx->resume_mon_event);
	init_completion(&mon_ctx->mon_shutdown);
	init_completion(&mon_ctx->mon_register_event);
	mon_ctx->mon_event_flag = 0;
	spin_lock_init(&mon_ctx->mon_queue_lock);
	spin_lock_init(&mon_ctx->mon_pkt_freeq_lock);
	INIT_LIST_HEAD(&mon_ctx->mon_thread_queue);
	spin_lock_bh(&mon_ctx->mon_pkt_freeq_lock);
	INIT_LIST_HEAD(&mon_ctx->mon_pkt_freeq);
	spin_unlock_bh(&mon_ctx->mon_pkt_freeq_lock);

	return pkt_capture_alloc_mon_pkt_freeq(mon_ctx);
}
