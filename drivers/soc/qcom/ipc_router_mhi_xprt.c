/* Copyright (c) 2014-2016,2020 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/ipc_router_xprt.h>
#include <linux/skbuff.h>
#include <linux/mod_devicetable.h>
#include <linux/mhi.h>
#include <net/sock.h>
#include <linux/of.h>

static int ipc_router_mhi_xprt_debug_mask;
module_param_named(debug_mask, ipc_router_mhi_xprt_debug_mask,
		   int, 0664);

#define D(x...) do { \
if (ipc_router_mhi_xprt_debug_mask) \
	pr_info(x); \
} while (0)

#define XPRT_NAME_LEN 32
#define XPRT_VERSION 1
#define XPRT_LINK_ID 1
#define FRAG_PKT_WRITE_DISABLE 0

struct ipc_router_mhi_xprt {
	struct mhi_device *mhi_dev;
	struct device *dev;
	char xprt_name[XPRT_NAME_LEN];
	struct msm_ipc_router_xprt xprt;
	spinlock_t ul_lock;		/* lock to protect ul_pkts */
	struct list_head ul_pkts;
	atomic_t in_reset;
	struct completion sft_close_complete;
	unsigned int xprt_version;
	unsigned int xprt_option;
	struct rr_packet *in_pkt;
};

struct ipc_router_mhi_pkt {
	struct list_head node;
	struct sk_buff *skb;
	struct rr_packet *rr_pkt;
	struct completion done;
};

static void ipc_router_mhi_pkt_release(struct kref *ref)
{
	struct rr_packet *pkt = container_of(ref, struct rr_packet,
						ref);

	release_pkt(pkt);
}

/* from mhi to qrtr */
static void qcom_mhi_ipc_router_dl_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct ipc_router_mhi_xprt *mhi_xprtp = dev_get_drvdata(&mhi_dev->dev);
	struct sk_buff *skb;
	void *dest_buf;

	if (!mhi_xprtp || mhi_res->transaction_status) {
		D("%s: Transport error transaction_status %d\n",
		__func__, mhi_res->transaction_status);
		return;
	}
	/* Create a new rr_packet */
	mhi_xprtp->in_pkt = create_pkt(NULL);
	if (!mhi_xprtp->in_pkt) {
		IPC_RTR_ERR("%s: Couldn't alloc rr_packet\n",
					__func__);
		return;
	}
	D("%s: Allocated rr_packet\n", __func__);

	skb = alloc_skb(mhi_res->bytes_xferd, GFP_KERNEL);
	if (!skb) {
		IPC_RTR_ERR("%s: Couldn't alloc skb\n",
				__func__);
		release_pkt(mhi_xprtp->in_pkt);
		return;
	}
	dest_buf = skb_put(skb, mhi_res->bytes_xferd);
	memcpy(dest_buf, mhi_res->buf_addr, mhi_res->bytes_xferd);
	mhi_xprtp->in_pkt->length = mhi_res->bytes_xferd;
	skb_queue_tail(mhi_xprtp->in_pkt->pkt_fragment_q, skb);

	msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
						IPC_ROUTER_XPRT_EVENT_DATA,
						(void *)mhi_xprtp->in_pkt);
	release_pkt(mhi_xprtp->in_pkt);
	mhi_xprtp->in_pkt = NULL;
}

/* from mhi to qrtr */
static void qcom_mhi_ipc_router_ul_callback(struct mhi_device *mhi_dev,
				      struct mhi_result *mhi_res)
{
	struct ipc_router_mhi_xprt *mhi_xprtp = dev_get_drvdata(&mhi_dev->dev);
	struct ipc_router_mhi_pkt *pkt;
	unsigned long flags;

	spin_lock_irqsave(&mhi_xprtp->ul_lock, flags);
	pkt = list_first_entry(&mhi_xprtp->ul_pkts,
		struct ipc_router_mhi_pkt, node);
	list_del(&pkt->node);
	complete_all(&pkt->done);

	spin_unlock_irqrestore(&mhi_xprtp->ul_lock, flags);
}

/* fatal error */
static void qcom_mhi_ipc_router_status_callback(struct mhi_device *mhi_dev,
					  enum MHI_CB mhi_cb)
{
	struct ipc_router_mhi_xprt *mhi_xprtp = dev_get_drvdata(&mhi_dev->dev);
	struct ipc_router_mhi_pkt *pkt;
	unsigned long flags;

	if (mhi_cb != MHI_CB_FATAL_ERROR)
		return;

	atomic_inc(&mhi_xprtp->in_reset);
	spin_lock_irqsave(&mhi_xprtp->ul_lock, flags);
	list_for_each_entry(pkt, &mhi_xprtp->ul_pkts, node)
		complete_all(&pkt->done);
	spin_unlock_irqrestore(&mhi_xprtp->ul_lock, flags);
	init_completion(&mhi_xprtp->sft_close_complete);
	msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
			IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
		  __func__, mhi_xprtp->xprt.name);
	wait_for_completion(&mhi_xprtp->sft_close_complete);
}

/* from qrtr to mhi */
static int ipc_router_mhi_write_skb(struct ipc_router_mhi_xprt *mhi_xprtp,
				struct sk_buff *skb)
{
	struct ipc_router_mhi_pkt *mhi_pkt;
	int rc;

	mhi_pkt = kzalloc(sizeof(*mhi_pkt), GFP_KERNEL);
	if (!mhi_pkt) {
		IPC_RTR_ERR("%s: Couldn't alloc mhi router pkt\n",
				__func__);
		return -ENOMEM;
	}

	init_completion(&mhi_pkt->done);

	spin_lock_bh(&mhi_xprtp->ul_lock);
	list_add_tail(&mhi_pkt->node, &mhi_xprtp->ul_pkts);
	rc = mhi_queue_transfer(mhi_xprtp->mhi_dev, DMA_TO_DEVICE,
				skb, skb->len, MHI_EOT);
	if (rc) {
		spin_unlock_bh(&mhi_xprtp->ul_lock);
		goto release_mhi;
	}
	spin_unlock_bh(&mhi_xprtp->ul_lock);

	rc = wait_for_completion_interruptible_timeout(&mhi_pkt->done, HZ * 5);
	if (atomic_read(&mhi_xprtp->in_reset)) {
		rc = -ECONNRESET;
		IPC_RTR_ERR("%s: Connection Reset %d\n",
				__func__, rc);
		goto release_mhi;
	} else if (rc == 0) {
		rc = -ETIMEDOUT;
		goto release_mhi;
	} else if (rc > 0) {
		kfree(mhi_pkt);
		rc = 0;
	}

	return rc;
release_mhi:
	D("%s: Transfer failed rc %d\n", __func__, rc);
	spin_lock_bh(&mhi_xprtp->ul_lock);
	list_del(&mhi_pkt->node);
	kfree(mhi_pkt);
	spin_unlock_bh(&mhi_xprtp->ul_lock);
	return rc;
}

/**
 * ipc_router_mhi_get_xprt_version() - Get IPC Router header version
 *				       supported by the XPRT
 * @xprt: XPRT for which the version information is required.
 *
 * @return: IPC Router header version supported by the XPRT.
 */
static int ipc_router_mhi_get_xprt_version(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_xprt *mhi_xprtp;

	if (!xprt)
		return -EINVAL;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_xprt, xprt);

	return (int)mhi_xprtp->xprt_version;
}

/**
 * ipc_router_mhi_set_xprt_version() - Set the IPC Router version in transport
 * @xprt:      Reference to the transport structure.
 * @version:   The version to be set in transport.
 */
static void ipc_router_mhi_set_xprt_version(struct msm_ipc_router_xprt *xprt,
					   unsigned int version)
{
	struct ipc_router_mhi_xprt *mhi_xprtp;

	if (!xprt)
		return;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_xprt, xprt);
	mhi_xprtp->xprt_version = version;
}

/**
 * ipc_router_mhi_get_xprt_option() - Get XPRT options
 * @xprt: XPRT for which the option information is required.
 *
 * @return: Options supported by the XPRT.
 */
static int ipc_router_mhi_get_xprt_option(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_xprt *mhi_xprtp;

	if (!xprt)
		return -EINVAL;
	mhi_xprtp = container_of(xprt, struct ipc_router_mhi_xprt, xprt);

	return (int)mhi_xprtp->xprt_option;
}

/**
 * ipc_router_mhi_write() - Write to XPRT
 * @data: Data to be written to the XPRT.
 * @len: Length of the data to be written.
 * @xprt: XPRT to which the data has to be written.
 *
 * @return: Data Length on success, standard Linux error codes on failure.
 */
static int ipc_router_mhi_write(void *data,
		uint32_t len, struct msm_ipc_router_xprt *xprt)
{
	struct rr_packet *pkt = (struct rr_packet *)data;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_packet *cloned_pkt;
	int rc;
	struct ipc_router_mhi_xprt *mhi_xprtp =
		container_of(xprt, struct ipc_router_mhi_xprt, xprt);

	if (!pkt || !len || pkt->length != len) {
		rc = -EINVAL;
		goto write_fail;
	}

	cloned_pkt = clone_pkt(pkt);
	if (!cloned_pkt) {
		pr_err("%s: Error in cloning packet while tx\n", __func__);
		rc = -ENOMEM;
		goto write_fail;
	}
	D("%s: Ready to write %d bytes\n", __func__, len);
	skb_queue_walk(cloned_pkt->pkt_fragment_q, ipc_rtr_pkt) {
		rc = ipc_router_mhi_write_skb(mhi_xprtp, ipc_rtr_pkt);
		if (rc < 0) {
			IPC_RTR_ERR("%s: Error writing SKB %d\n",
				    __func__, rc);
			break;
		}
	}
	kref_put(&cloned_pkt->ref, ipc_router_mhi_pkt_release);
	if (rc < 0)
		goto write_fail;
	else
		return len;

write_fail:
	D("%s: MHI Write failed rc=%d\n", __func__, rc);
	return rc;
}

/**
 * mhi_xprt_sft_close_done() - Completion of XPRT reset
 * @xprt: XPRT on which the reset operation is complete.
 *
 * This function is used by IPC Router to signal this MHI XPRT Abstraction
 * Layer(XAL) that the reset of XPRT is completely handled by IPC Router.
 */
static void mhi_xprt_sft_close_done(struct msm_ipc_router_xprt *xprt)
{
	struct ipc_router_mhi_xprt *mhi_xprtp =
		container_of(xprt, struct ipc_router_mhi_xprt, xprt);

	complete_all(&mhi_xprtp->sft_close_complete);
}

static int qcom_mhi_ipc_router_probe(struct mhi_device *mhi_dev,
			       const struct mhi_device_id *id)
{
	struct ipc_router_mhi_xprt *mhi_xprtp;

	mhi_xprtp = devm_kzalloc(&mhi_dev->dev, sizeof(*mhi_xprtp), GFP_KERNEL);
	if (!mhi_xprtp)
		return -ENOMEM;

	mhi_xprtp->mhi_dev = mhi_dev;
	mhi_xprtp->dev = &mhi_dev->dev;

	scnprintf(mhi_xprtp->xprt_name, XPRT_NAME_LEN,
		  "IPCRTR_MHI%x:%x_%s",
		  mhi_dev->ul_chan_id, mhi_dev->dl_chan_id, "ext_ap");
	mhi_xprtp->xprt_version = XPRT_VERSION;
	mhi_xprtp->xprt_option = FRAG_PKT_WRITE_DISABLE;

	/* Initialize XPRT operations and parameters registered with IPC RTR */
	mhi_xprtp->xprt.link_id = XPRT_LINK_ID;
	mhi_xprtp->xprt.name = mhi_xprtp->xprt_name;
	mhi_xprtp->xprt.get_version = ipc_router_mhi_get_xprt_version;
	mhi_xprtp->xprt.set_version = ipc_router_mhi_set_xprt_version;
	mhi_xprtp->xprt.get_option = ipc_router_mhi_get_xprt_option;
	mhi_xprtp->xprt.read_avail = NULL;
	mhi_xprtp->xprt.read = NULL;
	mhi_xprtp->xprt.write_avail = NULL;
	mhi_xprtp->xprt.write = ipc_router_mhi_write;
	mhi_xprtp->xprt.close = NULL;
	mhi_xprtp->xprt.sft_close_done = mhi_xprt_sft_close_done;
	mhi_xprtp->xprt.priv = NULL;

	atomic_set(&mhi_xprtp->in_reset, 0);

	INIT_LIST_HEAD(&mhi_xprtp->ul_pkts);
	spin_lock_init(&mhi_xprtp->ul_lock);

	dev_set_drvdata(&mhi_dev->dev, mhi_xprtp);

	msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
			IPC_ROUTER_XPRT_EVENT_OPEN, NULL);
	D("%s: Notified IPC Router of %s OPEN\n",
		  __func__, mhi_xprtp->xprt.name);

	return 0;
}

static void qcom_mhi_ipc_router_remove(struct mhi_device *mhi_dev)
{
	struct ipc_router_mhi_xprt *mhi_xprtp = dev_get_drvdata(&mhi_dev->dev);

	msm_ipc_router_xprt_notify(&mhi_xprtp->xprt,
			IPC_ROUTER_XPRT_EVENT_CLOSE, NULL);
	D("%s: Notified IPC Router of %s CLOSE\n",
		  __func__, mhi_xprtp->xprt.name);
	dev_set_drvdata(&mhi_dev->dev, NULL);
}

static const struct mhi_device_id qcom_mhi_ipc_router_mhi_match[] = {
	{ .chan = "IPCR" },
	{}
};

static struct mhi_driver qcom_mhi_ipc_router_driver = {
	.probe = qcom_mhi_ipc_router_probe,
	.remove = qcom_mhi_ipc_router_remove,
	.dl_xfer_cb = qcom_mhi_ipc_router_dl_callback,
	.ul_xfer_cb = qcom_mhi_ipc_router_ul_callback,
	.status_cb = qcom_mhi_ipc_router_status_callback,
	.id_table = qcom_mhi_ipc_router_mhi_match,
	.driver = {
		.name = "qcom_mhi_ipc_router",
		.owner = THIS_MODULE,
	},
};

module_driver(qcom_mhi_ipc_router_driver, mhi_driver_register,
	      mhi_driver_unregister);

MODULE_DESCRIPTION("IPC Router MHI XPRT");
MODULE_LICENSE("GPL v2");
