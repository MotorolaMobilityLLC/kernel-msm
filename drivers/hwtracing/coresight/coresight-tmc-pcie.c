// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Description: CoreSight TMC PCIe driver
 */

#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/qcom-iommu-util.h>
#include <linux/time.h>
#include <linux/slab.h>
#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-tmc.h"

static bool tmc_pcie_support_ipa(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "qcom,qdss-ipa-support");
}

static bool tmc_etr_support_pcie(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "qcom,qdss-pcie-support");
}

static void etr_pcie_close_channel(struct tmc_pcie_data *pcie_data)
{
	if (!pcie_data || !pcie_data->pcie_chan_opened)
		return;

	mutex_lock(&pcie_data->pcie_lock);
	mhi_dev_close_channel(pcie_data->out_handle);
	pcie_data->pcie_chan_opened = false;
	mutex_unlock(&pcie_data->pcie_lock);
}

static void tmc_pcie_read_bytes(struct tmc_pcie_data *pcie_data, loff_t *ppos,
			 size_t bytes, size_t *len, char **bufp)
{
	struct etr_buf *etr_buf = pcie_data->tmcdrvdata->sysfs_buf;
	size_t actual;

	if (*len >= bytes)
		*len = bytes;
	else if (((uint32_t)*ppos % bytes) + *len > bytes)
		*len = bytes - ((uint32_t)*ppos % bytes);

	actual = tmc_etr_buf_get_data(etr_buf, *ppos, *len, bufp);
	*len = actual;
	if (actual == bytes || (actual + (uint32_t)*ppos) % bytes == 0)
		atomic_dec(&pcie_data->irq_cnt);
}

static int tmc_pcie_sw_start(struct tmc_pcie_data *pcie_data)
{
	if (!pcie_data)
		return -ENOMEM;

	mutex_lock(&pcie_data->pcie_lock);
	coresight_csr_set_byte_cntr(pcie_data->csr, pcie_data->irqctrl_offset,
					PCIE_BLK_SIZE / 8);
	atomic_set(&pcie_data->irq_cnt, 0);
	pcie_data->total_size = 0;
	pcie_data->offset = 0;
	pcie_data->total_irq = 0;
	mutex_unlock(&pcie_data->pcie_lock);

	if (!pcie_data->pcie_chan_opened)
		queue_work(pcie_data->pcie_wq,
				&pcie_data->pcie_open_work);

	queue_work(pcie_data->pcie_wq, &pcie_data->pcie_write_work);
	dev_info(pcie_data->dev, "tmc pcie sw path enabled\n");

	return 0;
}

static void tmc_pcie_sw_stop(struct tmc_pcie_data *pcie_data)
{
	if (!pcie_data)
		return;

	etr_pcie_close_channel(pcie_data);
	wake_up(&pcie_data->pcie_wait_wq);

	mutex_lock(&pcie_data->pcie_lock);
	coresight_csr_set_byte_cntr(pcie_data->csr, pcie_data->irqctrl_offset,
					0);
	mutex_unlock(&pcie_data->pcie_lock);
}

static void tmc_pcie_client_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct tmc_pcie_data *pcie_data = NULL;

	if (!cb_data)
		return;

	pcie_data = cb_data->user_data;
	if (!pcie_data)
		return;
	if (pcie_data->pcie_path != TMC_PCIE_SW_PATH)
		return;
	switch (cb_data->ctrl_info) {
	case  MHI_STATE_CONNECTED:
		if (cb_data->channel == pcie_data->pcie_out_chan) {
			dev_dbg(pcie_data->dev,
				"PCIE out channel connected.\n");
			queue_work(pcie_data->pcie_wq,
					&pcie_data->pcie_open_work);
		}

		break;
	case MHI_STATE_DISCONNECTED:
		if (cb_data->channel == pcie_data->pcie_out_chan) {
			dev_dbg(pcie_data->dev,
				"PCIE out channel disconnected.\n");
			etr_pcie_close_channel(pcie_data);
		}
		break;
	default:
		break;
	}
}

static void tmc_pcie_write_complete_cb(void *req)
{
	struct mhi_req *mreq = req;

	if (!mreq)
		return;
	kfree(req);
}

static void tmc_pcie_open_work_fn(struct work_struct *work)
{
	int ret = 0;
	struct tmc_pcie_data *pcie_data = container_of(work,
					      struct tmc_pcie_data,
					      pcie_open_work);
	if (!pcie_data)
		return;

	/* Open write channel*/
	ret = mhi_dev_open_channel(pcie_data->pcie_out_chan,
			&pcie_data->out_handle,
			NULL);
	if (ret < 0) {
		dev_dbg(pcie_data->dev, "%s: open pcie out channel fail %d\n",
						__func__, ret);
	} else {
		dev_dbg(pcie_data->dev,
				"Open pcie out channel successfully\n");
		mutex_lock(&pcie_data->pcie_lock);
		pcie_data->pcie_chan_opened = true;
		mutex_unlock(&pcie_data->pcie_lock);
	}

}

static void tmc_pcie_write_work_fn(struct work_struct *work)
{
	int ret = 0;
	struct mhi_req *req;
	size_t actual;
	int bytes_to_write;
	char *buf;

	struct tmc_pcie_data *pcie_data = container_of(work,
						struct tmc_pcie_data,
						pcie_write_work);
	struct tmc_drvdata *tmcdrvdata = pcie_data->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;

	while (tmcdrvdata->mode == CS_MODE_SYSFS
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_PCIE) {
		if (!atomic_read(&pcie_data->irq_cnt)) {
			ret =  wait_event_interruptible(
				pcie_data->pcie_wait_wq,
				atomic_read(&pcie_data->irq_cnt) > 0
				|| tmcdrvdata->mode != CS_MODE_SYSFS
				|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_PCIE
				|| !pcie_data->pcie_chan_opened);
			if (ret == -ERESTARTSYS
			|| tmcdrvdata->mode != CS_MODE_SYSFS
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_PCIE
			|| !pcie_data->pcie_chan_opened)
				break;
		}

		actual = PCIE_BLK_SIZE;
		req = kzalloc(sizeof(*req), GFP_KERNEL);
		if (!req)
			break;

		tmc_pcie_read_bytes(pcie_data, &pcie_data->offset,
					PCIE_BLK_SIZE, &actual, &buf);

		if (actual <= 0) {
			kfree(req);
			req = NULL;
			break;
		}

		if (pcie_data->offset + actual >= etr_buf->size)
			pcie_data->offset = 0;
		else
			pcie_data->offset += actual;

		req->buf = buf;
		req->client = pcie_data->out_handle;
		req->context = pcie_data;
		req->len = actual;
		req->chan = pcie_data->pcie_out_chan;
		req->mode = DMA_ASYNC;
		req->client_cb = tmc_pcie_write_complete_cb;
		req->snd_cmpl = 1;

		bytes_to_write = mhi_dev_write_channel(req);

		if (bytes_to_write != actual) {
			dev_err_ratelimited(pcie_data->dev,
				"Write error %d\n", bytes_to_write);
			kfree(req);
			req = NULL;
			break;
		}

		pcie_data->total_size += actual;
	}
}

int tmc_register_pcie_channel(struct tmc_pcie_data *pcie_data)
{
	return mhi_register_state_cb(tmc_pcie_client_cb, pcie_data,
					pcie_data->pcie_out_chan);
}

static int tmc_pcie_sw_init(struct tmc_pcie_data *pcie_data)
{
	int ret;
	struct device *dev = pcie_data->dev;

	pcie_data->pcie_out_chan = MHI_CLIENT_QDSS_IN;
	pcie_data->pcie_chan_opened = false;
	INIT_WORK(&pcie_data->pcie_open_work, tmc_pcie_open_work_fn);
	INIT_WORK(&pcie_data->pcie_write_work, tmc_pcie_write_work_fn);
	init_waitqueue_head(&pcie_data->pcie_wait_wq);
	pcie_data->pcie_wq = create_singlethread_workqueue("tmc_pcie");
	if (!pcie_data->pcie_wq)
		return -ENOMEM;
	ret = of_property_read_u32(dev->of_node, "sw_pcie_buf_size", &pcie_data->buf_size);
	if (ret)
		pcie_data->buf_size = TMC_PCIE_MEM_SIZE;

	dev_info(dev, "setting tmc etr sw pcie buf size 0x%x\n", pcie_data->buf_size);
	mutex_init(&pcie_data->pcie_lock);

	return tmc_register_pcie_channel(pcie_data);
}

static int tmc_pcie_bam_enable(struct tmc_pcie_data *pcie_data)
{
	struct tmc_usb_bam_data *bamdata = pcie_data->bamdata;
	struct tmc_ipa_data *ipa_data = pcie_data->ipa_data;
	int ret;

	if (bamdata->enable)
		return 0;

	/* Reset bam to start with */
	ret = sps_device_reset(bamdata->handle);
	if (ret)
		goto err0;

	/* Now configure and enable bam */

	bamdata->pipe = sps_alloc_endpoint();
	if (!bamdata->pipe)
		return -ENOMEM;

	ret = sps_get_config(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->connect.mode = SPS_MODE_SRC;
	bamdata->connect.source = bamdata->handle;
	bamdata->connect.event_thresh = 0x4;
	bamdata->connect.src_pipe_index = TMC_ETR_BAM_PIPE_INDEX;

	if (bamdata->props.options & SPS_BAM_SMMU_EN) {
		dev_err(pcie_data->dev,
			"PCIE mode doesn't support smmu.\n");
		ret = -EINVAL;
		goto err1;
	}

	bamdata->connect.options = SPS_O_AUTO_ENABLE | SPS_O_DUMMY_PEER;

	bamdata->connect.destination =
		ipa_data->ipa_qdss_out.ipa_rx_db_pa;
	bamdata->connect.dest_pipe_index = 0;
	bamdata->connect.desc.phys_base =
		ipa_data->ipa_qdss_in.desc_fifo_base_addr;
	bamdata->connect.desc.size =
		ipa_data->ipa_qdss_in.desc_fifo_size;
	bamdata->connect.desc.base =
		ioremap(bamdata->connect.desc.phys_base,
		bamdata->connect.desc.size);
	if (!bamdata->connect.desc.base) {
		ret = -ENOMEM;
		goto err1;
	}

	bamdata->connect.data.phys_base =
		ipa_data->ipa_qdss_in.data_fifo_base_addr;
	bamdata->connect.data.size =
		ipa_data->ipa_qdss_in.data_fifo_size;
	bamdata->connect.data.base =
		ioremap(bamdata->connect.data.phys_base,
		bamdata->connect.data.size);
	if (!bamdata->connect.data.base) {
		ret = -ENOMEM;
		goto err1;
	}

	ret = sps_connect(bamdata->pipe, &bamdata->connect);
	if (ret)
		goto err1;

	bamdata->enable = true;
	return 0;
err1:
	sps_free_endpoint(bamdata->pipe);
err0:
	return ret;
}

static void tmc_pcie_bam_disable(struct tmc_pcie_data *pcie_data)
{
	struct tmc_usb_bam_data *bamdata = pcie_data->bamdata;

	if (!bamdata->enable)
		return;

	sps_disconnect(bamdata->pipe);
	sps_free_endpoint(bamdata->pipe);
	bamdata->enable = false;
}

static int tmc_pcie_hw_init(struct tmc_pcie_data *pcie_data)
{
	int ret;
	struct device *dev = pcie_data->dev;
	struct device_node *node = dev->of_node;
	struct tmc_ipa_data *ipa_data;
	struct tmc_usb_bam_data *bamdata;
	int mapping_config = 0;
	struct iommu_domain *domain;
	struct resource res;
	u32 value = 0;

	ipa_data = devm_kzalloc(dev, sizeof(*ipa_data), GFP_KERNEL);
	if (!ipa_data)
		return -ENOMEM;

	ret = of_property_read_u32(node, "ipa-conn-data-base-pa", &value);
	if (ret) {
		pr_err("%s: Invalid ipa data base address property\n",
			__func__);
		return -EINVAL;
	}
	ipa_data->ipa_qdss_in.data_fifo_base_addr = value;

	ret = of_property_read_u32(node, "ipa-conn-data-size", &value);
	if (ret) {
		pr_err("%s: Invalid ipa data base size\n", __func__);
		return  -EINVAL;
	}
	ipa_data->ipa_qdss_in.data_fifo_size = value;
	ret = of_property_read_u32(node, "ipa-conn-desc-base-pa", &value);
	if (ret) {
		pr_err("%s: Invalid ipa desc base address property\n",
			__func__);
		return  -EINVAL;
	}
	ipa_data->ipa_qdss_in.desc_fifo_base_addr = value;
	ret = of_property_read_u32(node, "ipa-conn-desc-size", &value);
	if (ret) {
		pr_err("%s: Invalid ipa desc size  property\n", __func__);
		return -EINVAL;
	}
	ipa_data->ipa_qdss_in.desc_fifo_size = value;
	ret = of_property_read_u32(node, "ipa-peer-evt-reg-pa", &value);
	if (ret) {
		pr_err("%s: Invalid ipa peer reg pa property\n", __func__);
		return -EINVAL;
	}
	ipa_data->ipa_qdss_in.bam_p_evt_dest_addr = value;
	ipa_data->ipa_qdss_in.bam_p_evt_threshold = 0x4;
	ipa_data->ipa_qdss_in.override_eot = 0x1;
	pcie_data->ipa_data = ipa_data;

	bamdata = devm_kzalloc(dev, sizeof(*bamdata), GFP_KERNEL);
	if (!bamdata)
		return -ENOMEM;
	pcie_data->bamdata = bamdata;

	ret = of_address_to_resource(node, 1, &res);
	if (ret)
		return -ENODEV;

	bamdata->props.phys_addr = res.start;
	bamdata->props.virt_addr = devm_ioremap(dev, res.start,
					resource_size(&res));
	if (!bamdata->props.virt_addr)
		return -ENOMEM;
	bamdata->props.virt_size = resource_size(&res);

	bamdata->props.event_threshold = 0x4; /* Pipe event threshold */
	bamdata->props.summing_threshold = 0x10; /* BAM event threshold */
	bamdata->props.irq = 0;
	bamdata->props.num_pipes = TMC_USB_BAM_NR_PIPES;
	domain = iommu_get_domain_for_dev(dev);
	if (domain) {
		mapping_config = qcom_iommu_get_mappings_configuration(domain);
		if (mapping_config < 0)
			return -ENOMEM;
		if (!(mapping_config & QCOM_IOMMU_MAPPING_CONF_S1_BYPASS)) {
			pr_debug("%s: setting SPS_BAM_SMMU_EN flag with (%s)\n",
					__func__, dev_name(dev));
			bamdata->props.options |= SPS_BAM_SMMU_EN;
		}
	}

	return sps_register_bam_device(&bamdata->props, &bamdata->handle);
}

static int tmc_pcie_ipa_conn(struct tmc_pcie_data *pcie_data)
{
	if (!pcie_data)
		return -ENOMEM;

	return ipa_qdss_conn_pipes(&pcie_data->ipa_data->ipa_qdss_in,
			&pcie_data->ipa_data->ipa_qdss_out);
}

static int tmc_pcie_ipa_disconn(void)
{
	return ipa_qdss_disconn_pipes();
}

static int __tmc_pcie_enable_to_bam(struct tmc_pcie_data *pcie_data)
{
	struct tmc_usb_bam_data *bamdata = pcie_data->bamdata;
	struct tmc_drvdata *drvdata = pcie_data->tmcdrvdata;
	uint32_t axictl;

	if (pcie_data->enable_to_bam)
		return 0;

	/* Configure and enable required CSR registers */
	msm_qdss_csr_enable_bam_to_usb(pcie_data->csr);

	/* Configure and enable ETR for usb bam output */

	CS_UNLOCK(drvdata->base);

	writel_relaxed(bamdata->connect.data.size / 4,
		drvdata->base + TMC_RSZ);

	writel_relaxed(TMC_MODE_CIRCULAR_BUFFER, drvdata->base + TMC_MODE);

	axictl = readl_relaxed(drvdata->base + TMC_AXICTL);
	axictl |= (0xF << 8);
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	axictl &= ~(0x1 << 7);
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);
	axictl = (axictl & ~0x3) | 0x2;
	writel_relaxed(axictl, drvdata->base + TMC_AXICTL);

	if (drvdata->out_mode == TMC_ETR_OUT_MODE_PCIE) {
		if (bamdata->props.options & SPS_BAM_SMMU_EN) {
			CS_LOCK(drvdata->base);
			dev_err(&drvdata->csdev->dev,
				"PCIE mode doesn't support smmu.\n");
			return -EINVAL;
		}

		writel_relaxed((uint32_t)bamdata->connect.data.phys_base,
			drvdata->base + TMC_DBALO);
		writel_relaxed(
			(((uint64_t)bamdata->connect.data.phys_base) >> 32)
			& 0xFF, drvdata->base + TMC_DBAHI);
	}
	/* Set FOnFlIn for periodic flush */
	writel_relaxed(0x133, drvdata->base + TMC_FFCR);
	writel_relaxed(drvdata->trigger_cntr, drvdata->base + TMC_TRG);
	tmc_enable_hw(drvdata);

	CS_LOCK(drvdata->base);

	msm_qdss_csr_enable_flush(pcie_data->csr);
	pcie_data->enable_to_bam = true;
	return 0;
}

static void tmc_wait_for_flush(struct tmc_drvdata *drvdata)
{
	int count;

	/* Ensure no flush is in progress */
	for (count = TIMEOUT_US;
	     BVAL(readl_relaxed(drvdata->base + TMC_FFSR), 0) != 0
	     && count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while waiting for TMC flush, TMC_FFSR: %#x\n",
	     readl_relaxed(drvdata->base + TMC_FFSR));
}


void __tmc_pcie_disable_to_bam(struct tmc_pcie_data *pcie_data)
{
	struct tmc_drvdata *drvdata = pcie_data->tmcdrvdata;

	if (!pcie_data->enable_to_bam)
		return;

	/* Ensure periodic flush is disabled in CSR block */
	msm_qdss_csr_disable_flush(pcie_data->csr);

	CS_UNLOCK(drvdata->base);

	tmc_wait_for_flush(drvdata);
	tmc_disable_hw(drvdata);

	CS_LOCK(drvdata->base);

	/* Disable CSR configuration */
	msm_qdss_csr_disable_bam_to_usb(pcie_data->csr);
	pcie_data->enable_to_bam = false;
}

static int tmc_pcie_hw_enable(struct tmc_pcie_data *pcie_data)
{
	int ret;

	ret = tmc_pcie_ipa_conn(pcie_data);
	if (ret)
		return ret;
	ret = tmc_pcie_bam_enable(pcie_data);
	if (ret) {
		tmc_pcie_ipa_disconn();
		return ret;
	}
	ret = __tmc_pcie_enable_to_bam(pcie_data);
	if (ret) {
		tmc_pcie_bam_disable(pcie_data);
		tmc_pcie_ipa_disconn();
		return ret;
	}

	dev_info(pcie_data->dev, "tmc pcie hw path enabled\n");
	return 0;
}

static void tmc_pcie_hw_disable(struct tmc_pcie_data *pcie_data)
{
	__tmc_pcie_disable_to_bam(pcie_data);
	tmc_pcie_bam_disable(pcie_data);
	tmc_pcie_ipa_disconn();
}

int tmc_pcie_enable(struct tmc_pcie_data *pcie_data)
{
	if (!pcie_data)
		return -ENOMEM;

	if (pcie_data->pcie_path == TMC_PCIE_SW_PATH)
		return tmc_pcie_sw_start(pcie_data);
	else
		return tmc_pcie_hw_enable(pcie_data);

}


void tmc_pcie_disable(struct tmc_pcie_data *pcie_data)
{
	if (pcie_data->pcie_path == TMC_PCIE_SW_PATH) {
		tmc_pcie_sw_stop(pcie_data);
		flush_work(&pcie_data->pcie_write_work);
		dev_info(pcie_data->dev,
		"qdss receive total irq: %ld, send data %ld\n",
		pcie_data->total_irq, pcie_data->total_size);
	} else
		return tmc_pcie_hw_disable(pcie_data);
}

int tmc_pcie_init(struct amba_device *adev,
			struct tmc_drvdata *drvdata)
{
	int ret;
	struct device *dev = &adev->dev;
	struct tmc_pcie_data *pcie_data;
	struct byte_cntr *byte_cntr_data = drvdata->byte_cntr;

	if (tmc_etr_support_pcie(dev)) {
		pcie_data = devm_kzalloc(dev, sizeof(*pcie_data), GFP_KERNEL);
		if (!pcie_data)
			return -ENOMEM;
		pcie_data->dev = dev;
		pcie_data->csr = drvdata->csr;
		pcie_data->irqctrl_offset = byte_cntr_data->irqctrl_offset;
		pcie_data->tmcdrvdata = drvdata;
		pcie_data->byte_cntr_data = byte_cntr_data;
		drvdata->pcie_data = pcie_data;
		byte_cntr_data->pcie_data = pcie_data;

		if (tmc_pcie_support_ipa(dev)) {
			ret = tmc_pcie_hw_init(pcie_data);

			if (ret)
				return ret;
		}

		ret = tmc_pcie_sw_init(pcie_data);

		if (ret)
			return ret;

		pcie_data->pcie_path = TMC_PCIE_SW_PATH;

		drvdata->mode_support |= BIT(TMC_ETR_OUT_MODE_PCIE);
		dev_info(dev, "pcie mode init success.\n");
	}

	return 0;
}
