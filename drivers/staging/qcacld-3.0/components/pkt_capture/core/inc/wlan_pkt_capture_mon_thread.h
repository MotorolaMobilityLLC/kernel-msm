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
 * DOC: Declare APIs which shall be used for monitor thread access.
 */

#ifndef _WLAN_PKT_CAPTURE_MON_THREAD_H_
#define _WLAN_PKT_CAPTURE_MON_THREAD_H_

#include "wlan_pkt_capture_main.h"

#define PKT_CAPTURE_RX_POST_EVENT 0x01
#define PKT_CAPTURE_RX_SUSPEND_EVENT 0x02
#define PKT_CAPTURE_RX_SHUTDOWN_EVENT 0x04
#define PKT_CAPTURE_REGISTER_EVENT 0x08

/*
 * Maximum number of packets to be allocated for
 * Packet Capture Monitor thread.
 */
#define MAX_MON_PKT_SIZE 4000

/* timeout in msec to wait for mon thread to suspend */
#define PKT_CAPTURE_SUSPEND_TIMEOUT 200

typedef void (*pkt_capture_mon_thread_cb)(
			void *context, void *ppdev, void *monpkt,
			uint8_t vdev_id, uint8_t tid,
			uint16_t status, bool pkt_format,
			uint8_t *bssid,
			uint8_t tx_retry_cnt);

/*
 * struct pkt_capture_mon_pkt - mon packet wrapper for mon data from TXRX
 * @list: List for storing mon packets
 * @context: Callback context
 * @pdev: pointer to pdev handle
 * @monpkt: Mon skb
 * @vdev_id: Vdev id to which this packet is destined
 * @tid: Tid of mon packet
 * @status: Tx packet status
 * @pkt_format: Mon packet format, 0 = 802.3 format , 1 = 802.11 format
 * @bssid: bssid
 * @tx_retry_cnt: tx retry count
 * @callback: Mon callback
 */
struct pkt_capture_mon_pkt {
	struct list_head list;
	void *context;
	void *pdev;
	void *monpkt;
	uint8_t vdev_id;
	uint8_t tid;
	uint16_t status;
	bool pkt_format;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint8_t tx_retry_cnt;
	pkt_capture_mon_thread_cb callback;
};

/**
 * struct pkt_capture_mon_context - packet capture mon thread context
 * @mon_thread_lock: MON thread lock
 * @mon_thread: MON thread handle
 * @mon_start_event: Handle of Event for MON thread to signal startup
 * @suspend_mon_event: Completion to suspend packet capture MON thread
 * @resume_mon_event: Completion to resume packet capture MON thread
 * @mon_shutdown: Completion for packet capture MON thread shutdown
 * @mon_register_event: Completion for packet capture register
 * @mon_wait_queue: Waitq for packet capture MON thread
 * @mon_event_flag: Mon event flag
 * @mon_thread_queue: MON buffer queue
 * @mon_queue_lock: Spinlock to synchronize between tasklet and thread
 * @mon_pkt_freeq_lock: Lock to synchronize free buffer queue access
 * @mon_pkt_freeq: Free message queue for packet capture MON processing
 * @is_mon_thread_suspended: flag to check mon thread suspended or not
 */
struct pkt_capture_mon_context {
	/* MON thread lock */
	spinlock_t mon_thread_lock;
	struct task_struct *mon_thread;
	struct completion mon_start_event;
	struct completion suspend_mon_event;
	struct completion resume_mon_event;
	struct completion mon_shutdown;
	struct completion mon_register_event;
	wait_queue_head_t mon_wait_queue;
	unsigned long mon_event_flag;
	struct list_head mon_thread_queue;

	/* Spinlock to synchronize between tasklet and thread */
	spinlock_t mon_queue_lock;

	/* Lock to synchronize free buffer queue access */
	spinlock_t mon_pkt_freeq_lock;

	struct list_head mon_pkt_freeq;
	bool is_mon_thread_suspended;
};

/**
 * struct radiotap_header - base radiotap header
 * @it_version: radiotap version, always 0
 * @it_pad: padding (or alignment)
 * @it_len: overall radiotap header length
 * @it_present: (first) present word
 */
struct radiotap_header {
	uint8_t it_version;
	uint8_t it_pad;
	__le16 it_len;
	__le32 it_present;
} __packed;

/**
 * pkt_capture_suspend_mon_thread() - suspend packet capture mon thread
 * vdev: pointer to vdev object manager
 *
 * Return: 0 on success, -EINVAL on failure
 */
int pkt_capture_suspend_mon_thread(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_resume_mon_thread() - resume packet capture mon thread
 * vdev: pointer to vdev object manager
 *
 * Resume packet capture MON thread by completing RX thread resume event.
 *
 * Return: None
 */
void pkt_capture_resume_mon_thread(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_drop_monpkt() - API to drop pending mon packets
 * mon_ctx: pointer to packet capture mon context
 *
 * This api drops all the pending packets in the queue.
 *
 * Return: None
 */
void pkt_capture_drop_monpkt(struct pkt_capture_mon_context *mon_ctx);

/**
 * pkt_capture_indicate_monpkt() - API to Indicate rx data packet
 * @vdev: pointer to vdev object manager
 * @pkt: MON pkt pointer containing to mon data message buffer
 *
 * Return: None
 */
void pkt_capture_indicate_monpkt(struct wlan_objmgr_vdev *vdev,
				 struct pkt_capture_mon_pkt *pkt);

/**
 * pkt_capture_wakeup_mon_thread() - wakeup packet capture mon thread
 * @vdev: pointer to vdev object
 *
 * This api wake up pkt_capture_mon_thread() to process pkt.
 *
 * Return: None
 */
void pkt_capture_wakeup_mon_thread(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_close_mon_thread() - close packet capture MON thread
 * @mon_ctx: pointer to packet capture mon context
 *
 * This api closes packet capture MON thread.
 *
 * Return: None
 */
void pkt_capture_close_mon_thread(struct pkt_capture_mon_context *mon_ctx);

/**
 * pkt_capture_open_mon_thread() - open packet capture MON thread
 * @mon_ctx: pointer to packet capture mon context
 *
 * This api opens the packet capture MON thread.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_open_mon_thread(struct pkt_capture_mon_context *mon_ctx);

/**
 * pkt_capture_alloc_mon_thread() - alloc resources for
 * packet capture MON thread
 * @mon_ctx: pointer to packet capture mon context
 *
 * This api alloc resources for packet capture MON thread.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_alloc_mon_thread(struct pkt_capture_mon_context *mon_ctx);

/**
 * pkt_capture_alloc_mon_pkt() - API to return next available mon packet
 * @vdev: pointer to vdev object manager
 *
 * This api returns next available mon packet buffer used for mon data
 * processing.
 *
 * Return: Pointer to pkt_capture_mon_pkt
 */
struct pkt_capture_mon_pkt *
pkt_capture_alloc_mon_pkt(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_free_mon_pkt_freeq() - free mon packet free queue
 * @mon_ctx: pointer to packet capture mon context
 *
 * This API does mem free of the buffers available in free mon packet
 * queue which is used for mon Data processing.
 *
 * Return: None
 */
void pkt_capture_free_mon_pkt_freeq(struct pkt_capture_mon_context *mon_ctx);
#endif /* _WLAN_PKT_CAPTURE_MON_THREAD_H_ */
