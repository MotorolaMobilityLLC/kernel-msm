/*
 * INTEL MID Remote Processor - SCU driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_ids.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

#include <asm/intel_scu_ipc.h>
#include <asm/scu_ipc_rpmsg.h>
#include <asm/intel-mid.h>

#include "intel_mid_rproc_core.h"
#include "remoteproc_internal.h"

static struct rpmsg_ns_list *nslist;


static int scu_ipc_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	ret = intel_scu_ipc_command(tx_msg->cmd, tx_msg->sub,
				tx_msg->in, tx_msg->inlen,
				tx_msg->out, tx_msg->outlen);
	return ret;
}

static int scu_ipc_raw_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	intel_scu_ipc_lock();
	ret = intel_scu_ipc_raw_cmd(tx_msg->cmd, tx_msg->sub,
				tx_msg->in, tx_msg->inlen,
				tx_msg->out, tx_msg->outlen,
				tx_msg->dptr, tx_msg->sptr);
	intel_scu_ipc_unlock();

	return ret;
}

static int scu_ipc_simple_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	ret = intel_scu_ipc_simple_command(tx_msg->cmd, tx_msg->sub);

	return ret;
}

static void scu_ipc_send_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;

	tx_msg = (struct tx_ipc_msg *)tx_buf;
	intel_scu_ipc_send_command(tx_msg->sub << 12 | tx_msg->cmd);
}

static int scu_ipc_fw_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	switch (tx_msg->cmd) {
	case RP_GET_FW_REVISION:
		ret = scu_ipc_command(tx_buf);
		break;
	case RP_FW_UPDATE:
		/* Only scu_ipc_send_command works for fw update */
		scu_ipc_send_command(tx_buf);
		break;
	default:
		pr_info("Command %x not supported\n", tx_msg->cmd);
		break;
	};

	return ret;
}

static int scu_ipc_util_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	switch (tx_msg->cmd) {
	case RP_GET_FW_REVISION:
	case RP_GET_HOBADDR:
	case RP_OSC_CLK_CTRL:
		ret = scu_ipc_command(tx_buf);
		break;
	case RP_S0IX_COUNTER:
		ret = scu_ipc_simple_command(tx_buf);
		break;
	case RP_WRITE_OSNIB:
		ret = scu_ipc_raw_command(tx_buf);
		break;
	default:
		pr_info("Command %x not supported\n", tx_msg->cmd);
		break;
	};

	return ret;
}

static int scu_ipc_vrtc_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	switch (tx_msg->cmd) {
	case RP_GET_HOBADDR:
		ret = scu_ipc_command(tx_buf);
		break;
	case RP_VRTC:
		ret = scu_ipc_simple_command(tx_buf);
		break;
	default:
		pr_info("Command %x not supported\n", tx_msg->cmd);
		break;
	};

	return ret;
}

static int scu_ipc_fw_logging_command(void *tx_buf)
{
	struct tx_ipc_msg *tx_msg;
	int ret = 0;

	tx_msg = (struct tx_ipc_msg *)tx_buf;

	switch (tx_msg->cmd) {
	case RP_GET_HOBADDR:
	case RP_SCULOG_TRACE:
		ret = scu_ipc_command(tx_buf);
		break;
	case RP_CLEAR_FABERROR:
	case RP_SCULOG_CTRL:
		ret = scu_ipc_simple_command(tx_buf);
		break;
	default:
		pr_info("Command %x not supported\n", tx_msg->cmd);
		break;
	};

	return ret;
}

/**
 * scu_ipc_rpmsg_handle() - scu rproc specified ipc rpmsg handle
 * @rx_buf: rx buffer to be add
 * @tx_buf: tx buffer to be get
 * @r_len: rx buffer length
 * @s_len: tx buffer length
 */
int scu_ipc_rpmsg_handle(void *rx_buf, void *tx_buf, u32 *r_len, u32 *s_len)
{
	struct rpmsg_hdr *tx_hdr, *tmp_hdr;
	struct tx_ipc_msg *tx_msg;
	struct rx_ipc_msg *tmp_msg;
	int ret = 0;

	*r_len = sizeof(struct rpmsg_hdr) + sizeof(struct rx_ipc_msg);
	*s_len = sizeof(struct rpmsg_hdr) + sizeof(struct tx_ipc_msg);

	/* get tx_msg and send scu ipc command */
	tx_hdr = (struct rpmsg_hdr *)tx_buf;
	tx_msg = (struct tx_ipc_msg *)(tx_buf + sizeof(*tx_hdr));

	tmp_hdr = (struct rpmsg_hdr *)rx_buf;
	tmp_msg = (struct rx_ipc_msg *)tmp_hdr->data;

	switch (tx_hdr->dst) {
	case RP_PMIC_ACCESS:
	case RP_FLIS_ACCESS:
	case RP_IPC_COMMAND:
		tmp_msg->status = scu_ipc_command(tx_msg);
		break;
	case RP_SET_WATCHDOG:
		if ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) ||
			(intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE))
			tmp_msg->status = scu_ipc_raw_command(tx_msg);
		else
			tmp_msg->status = scu_ipc_command(tx_msg);
		break;
	case RP_MIP_ACCESS:
	case RP_IPC_RAW_COMMAND:
		tmp_msg->status = scu_ipc_raw_command(tx_msg);
		break;
	case RP_IPC_SIMPLE_COMMAND:
		tmp_msg->status = scu_ipc_simple_command(tx_msg);
		break;
	case RP_IPC_UTIL:
		tmp_msg->status = scu_ipc_util_command(tx_msg);
		break;
	case RP_FW_ACCESS:
		tmp_msg->status = scu_ipc_fw_command(tx_msg);
		break;
	case RP_VRTC:
		tmp_msg->status = scu_ipc_vrtc_command(tx_msg);
		break;
	case RP_FW_LOGGING:
		tmp_msg->status = scu_ipc_fw_logging_command(tx_msg);
		break;
	default:
		tmp_msg->status = 0;
		pr_info("Command %x not supported yet\n", tx_hdr->dst);
		break;
	};

	/* prepare rx buffer, switch src and dst */
	tmp_hdr->src = tx_hdr->dst;
	tmp_hdr->dst = tx_hdr->src;

	tmp_hdr->flags = tx_hdr->flags;
	tmp_hdr->len = sizeof(struct rx_ipc_msg);

	return ret;
}

/* kick a virtqueue */
static void intel_rproc_scu_kick(struct rproc *rproc, int vqid)
{
	int idx;
	int ret;
	struct intel_mid_rproc *iproc;
	struct rproc_vdev *rvdev;
	struct device *dev = rproc->dev.parent;
	static unsigned long ns_info_all_received;

	iproc = (struct intel_mid_rproc *)rproc->priv;

	/*
	 * Remote processor virtqueue being kicked.
	 * This part simulates remote processor handling messages.
	 */
	idx = find_vring_index(rproc, vqid, VIRTIO_ID_RPMSG);

	switch (idx) {
	case RX_VRING:
		if (iproc->ns_enabled && !ns_info_all_received) {
			/* push messages with ns_info for ALL available
			name services in the list (nslist) into
			rx buffers. */
			list_for_each_entry_continue(iproc->ns_info,
				&nslist->list, node) {
				ret = intel_mid_rproc_ns_handle(iproc,
					iproc->ns_info);
				if (ret) {
					dev_err(dev, "ns handle error\n");
					return;
				}
			}

			ns_info_all_received = 1;
			intel_mid_rproc_vq_interrupt(rproc, vqid);
		}
		break;

	case TX_VRING:

		dev_dbg(dev, "remote processor got the message ...\n");
		intel_mid_rproc_msg_handle(iproc);
		intel_mid_rproc_vq_interrupt(rproc, vqid);

		/*
		 * After remoteproc handles the message, it calls
		 * the receive callback.
		 * TODO: replace this part with real remote processor
		 * operation.
		 */
		rvdev = find_rvdev(rproc, VIRTIO_ID_RPMSG);
		if (rvdev)
			intel_mid_rproc_vq_interrupt(rproc,
				rvdev->vring[RX_VRING].notifyid);
		else
			WARN(1, "%s: can't find given rproc state\n", __func__);
		break;

	default:
		dev_err(dev, "invalid vring index\n");
		break;
	}
}

/* power up the remote processor */
static int intel_rproc_scu_start(struct rproc *rproc)
{
	struct intel_mid_rproc *iproc;

	pr_info("Started intel scu remote processor\n");
	iproc = (struct intel_mid_rproc *)rproc->priv;
	intel_mid_rproc_vring_init(rproc, &iproc->rx_vring, RX_VRING);
	intel_mid_rproc_vring_init(rproc, &iproc->tx_vring, TX_VRING);

	return 0;
}

/* power off the remote processor */
static int intel_rproc_scu_stop(struct rproc *rproc)
{
	pr_info("Stopped intel scu remote processor\n");
	return 0;
}

static struct rproc_ops intel_rproc_scu_ops = {
	.start		= intel_rproc_scu_start,
	.stop		= intel_rproc_scu_stop,
	.kick		= intel_rproc_scu_kick,
};

static int intel_rproc_scu_probe(struct platform_device *pdev)
{
	struct intel_mid_rproc_pdata *pdata = pdev->dev.platform_data;
	struct intel_mid_rproc *iproc;
	struct rproc *rproc;
	int ret;

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(pdev->dev.parent, "dma_set_coherent_mask: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(&pdev->dev, pdata->name, &intel_rproc_scu_ops,
				pdata->firmware, sizeof(*iproc));
	if (!rproc)
		return -ENOMEM;

	iproc = rproc->priv;
	iproc->rproc = rproc;
	nslist = pdata->nslist;

	platform_set_drvdata(pdev, rproc);

	ret = rproc_add(rproc);
	if (ret)
		goto free_rproc;

	/*
	 * Temporarily follow the rproc framework to load firmware
	 * TODO: modify remoteproc code according to X86 architecture
	 */
	if (0 == wait_for_completion_timeout(&rproc->firmware_loading_complete,
		RPROC_FW_LOADING_TIMEOUT)) {
		dev_err(pdev->dev.parent, "fw loading not complete\n");
		goto free_rproc;
	}

	/* Initialize intel_rproc_scu private data */
	strncpy(iproc->name, pdev->id_entry->name, sizeof(iproc->name) - 1);
	iproc->type = pdev->id_entry->driver_data;
	iproc->r_vring_last_used = 0;
	iproc->s_vring_last_used = 0;
	iproc->ns_enabled = true;
	iproc->rproc_rpmsg_handle = scu_ipc_rpmsg_handle;
	iproc->ns_info = list_entry(&nslist->list,
			struct rpmsg_ns_info, node);

	return 0;

free_rproc:
	rproc_put(rproc);
	return ret;
}

static int intel_rproc_scu_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);

	if (nslist)
		rpmsg_ns_del_list(nslist);

	rproc_del(rproc);
	rproc_put(rproc);

	return 0;
}

static const struct platform_device_id intel_rproc_scu_id_table[] = {
	{ "intel_rproc_scu", RPROC_SCU },
	{ },
};

static struct platform_driver intel_rproc_scu_driver = {
	.probe = intel_rproc_scu_probe,
	.remove = intel_rproc_scu_remove,
	.driver = {
		.name = "intel_rproc_scu",
		.owner = THIS_MODULE,
	},
	.id_table = intel_rproc_scu_id_table,
};

static int __init intel_rproc_scu_init(void)
{
	return platform_driver_register(&intel_rproc_scu_driver);
}

static void __exit intel_rproc_scu_exit(void)
{
	platform_driver_unregister(&intel_rproc_scu_driver);
}

subsys_initcall(intel_rproc_scu_init);
module_exit(intel_rproc_scu_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ning Li<ning.li@intel.com>");
MODULE_DESCRIPTION("INTEL MID Remoteproc Core driver");
