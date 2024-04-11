// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Description: CoreSight TMC USB driver
 */

#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/qcom-iommu-util.h>
#include <linux/usb/usb_qdss.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include "coresight-tmc-usb.h"
#include "coresight-priv.h"
#include "coresight-common.h"
#include "coresight-tmc.h"

#define USB_BLK_SIZE 65536
#define USB_TOTAL_IRQ (TMC_ETR_SW_USB_BUF_SIZE/USB_BLK_SIZE)
#define USB_SG_NUM (USB_BLK_SIZE / PAGE_SIZE)
#define USB_BUF_NUM 255
#define USB_TIME_OUT (5 * HZ)

#define TMC_AXICTL_VALUE	(0xf02)
#define TMC_FFCR_VALUE		(0x133)

static int usb_bypass_start(struct byte_cntr *byte_cntr_data)
{
	long offset;
	struct tmc_drvdata *tmcdrvdata;

	if (!byte_cntr_data)
		return -ENOMEM;

	tmcdrvdata = byte_cntr_data->tmcdrvdata;
	mutex_lock(&byte_cntr_data->usb_bypass_lock);


	dev_info(&tmcdrvdata->csdev->dev,
			"%s: Start usb bypass\n", __func__);
	if (tmcdrvdata->mode != CS_MODE_SYSFS) {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return -EINVAL;
	}

	offset = tmc_get_rwp_offset(tmcdrvdata);
	if (offset < 0) {
		dev_err(&tmcdrvdata->csdev->dev,
			"%s: invalid rwp offset value\n", __func__);
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return offset;
	}
	byte_cntr_data->offset = offset;
	byte_cntr_data->total_irq = 0;
	tmcdrvdata->usb_data->drop_data_size = 0;
	tmcdrvdata->usb_data->data_overwritten = false;
	tmcdrvdata->usb_data->out_data_size = 0;
	tmcdrvdata->usb_data->in_data_speed = 0;
	tmcdrvdata->usb_data->out_data_speed = 0;

	/*Ensure usbch is ready*/
	if (!tmcdrvdata->usb_data->usbch) {
		int i;

		for (i = TIMEOUT_US; i > 0; i--) {
			if (tmcdrvdata->usb_data->usbch)
				break;

			if (i - 1)
				udelay(1);
			else {
				dev_err(&tmcdrvdata->csdev->dev,
					"timeout while waiting usbch to be ready\n");
				mutex_unlock(&byte_cntr_data->usb_bypass_lock);
				return -EAGAIN;
			}
		}
	}
	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);

	byte_cntr_data->read_active = true;
	/*
	 * IRQ is a '8- byte' counter and to observe interrupt at
	 * 'block_size' bytes of data
	 */
	coresight_csr_set_byte_cntr(byte_cntr_data->csr,
			byte_cntr_data->irqctrl_offset,
			USB_BLK_SIZE / 8);

	atomic_set(&byte_cntr_data->irq_cnt, 0);
	byte_cntr_data->total_size = 0;
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

	return 0;
}

static void usb_bypass_stop(struct byte_cntr *byte_cntr_data)
{
	u64 drop_rate;

	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->usb_bypass_lock);
	if (byte_cntr_data->read_active)
		byte_cntr_data->read_active = false;
	else {
		mutex_unlock(&byte_cntr_data->usb_bypass_lock);
		return;
	}
	wake_up(&byte_cntr_data->usb_wait_wq);
	pr_info("coresight: stop usb bypass\n");
	byte_cntr_data->rwp_offset = tmc_get_rwp_offset(byte_cntr_data->tmcdrvdata);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr, byte_cntr_data->irqctrl_offset, 0);
	dev_info(&byte_cntr_data->tmcdrvdata->csdev->dev,
		"USB total size: %lld, total irq: %lld,current irq:%d, offset: %ld, rwp_offset: %ld, drop_data: %lld\n",
		byte_cntr_data->total_size, byte_cntr_data->total_irq,
		atomic_read(&byte_cntr_data->irq_cnt),
		byte_cntr_data->offset,
		byte_cntr_data->rwp_offset,
		byte_cntr_data->tmcdrvdata->usb_data->drop_data_size);
	drop_rate = byte_cntr_data->tmcdrvdata->usb_data->drop_data_size;
	drop_rate = drop_rate * 100 / (byte_cntr_data->total_size + drop_rate);
	dev_info(&byte_cntr_data->tmcdrvdata->csdev->dev,
		"Data input speed: %lldMB/s, Data output speed: %lldMB/s, drop_rate: %d%%\n",
		byte_cntr_data->tmcdrvdata->usb_data->in_data_speed,
		byte_cntr_data->tmcdrvdata->usb_data->out_data_speed,
		drop_rate);
	mutex_unlock(&byte_cntr_data->usb_bypass_lock);

}

static int usb_transfer_small_packet(struct byte_cntr *drvdata, size_t *small_size)
{
	int ret = 0;
	struct tmc_drvdata *tmcdrvdata = drvdata->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	struct qdss_request *usb_req = NULL;
	size_t req_size;
	long actual;
	long w_offset;

	w_offset = tmc_get_rwp_offset(tmcdrvdata);
	if (w_offset < 0) {
		ret = w_offset;
		dev_err_ratelimited(&tmcdrvdata->csdev->dev,
			"%s: RWP offset is invalid\n", __func__);
		goto out;
	}

	if (unlikely(atomic_read(&drvdata->irq_cnt) > USB_TOTAL_IRQ)) {
		tmcdrvdata->usb_data->data_overwritten = true;
		dev_err_ratelimited(&tmcdrvdata->csdev->dev, "ETR data is overwritten.\n");
	} else if (tmcdrvdata->usb_data->data_overwritten) {
		tmcdrvdata->usb_data->data_overwritten = false;
	}

	req_size = ((w_offset < drvdata->offset) ? etr_buf->size : 0) +
				w_offset - drvdata->offset;
	req_size = ((req_size + *small_size) < USB_BLK_SIZE) ? req_size :
		(USB_BLK_SIZE - *small_size);

	while (req_size > 0) {

		usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
		if (!usb_req) {
			ret = -EFAULT;
			goto out;
		}

		actual = tmc_etr_buf_get_data(etr_buf, drvdata->offset,
					req_size, &usb_req->buf);

		if (actual <= 0 || actual > req_size) {
			kfree(usb_req);
			usb_req = NULL;
			dev_err_ratelimited(&tmcdrvdata->csdev->dev,
				"%s: Invalid data in ETR\n", __func__);
			ret = -EINVAL;
			goto out;
		}

		usb_req->length = actual;
		drvdata->usb_req = usb_req;
		req_size -= actual;

		if ((drvdata->offset + actual) >=
				tmcdrvdata->sysfs_buf->size)
			drvdata->offset = 0;
		else
			drvdata->offset += actual;

		*small_size += actual;

		if (atomic_read(&drvdata->usb_free_buf) > 0) {
			ret = usb_qdss_write(tmcdrvdata->usb_data->usbch, usb_req);

			if (ret) {
				kfree(usb_req);
				usb_req = NULL;
				drvdata->usb_req = NULL;
				dev_err_ratelimited(&tmcdrvdata->csdev->dev,
					"Write data failed:%d\n", ret);
				goto out;
			}
			drvdata->total_size += actual;
			atomic_dec(&drvdata->usb_free_buf);
		} else {
			dev_err_ratelimited(&tmcdrvdata->csdev->dev,
			"Drop data, offset = %d, len = %d\n",
				drvdata->offset, req_size);
			tmcdrvdata->usb_data->drop_data_size += actual;
			kfree(usb_req);
			drvdata->usb_req = NULL;
		}
	}

out:
	return ret;
}

static void usb_read_work_fn(struct work_struct *work)
{
	int ret, i, seq = 0;
	struct qdss_request *usb_req = NULL;
	size_t req_size, req_sg_num, small_size = 0;
	long actual;
	ssize_t actual_total = 0;
	char *buf;
	struct byte_cntr *drvdata =
		container_of(work, struct byte_cntr, read_work);
	struct tmc_drvdata *tmcdrvdata = drvdata->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	u64 elapsed_ms;
	struct timespec64 now;

	ktime_get_real_ts64(&tmcdrvdata->usb_data->start_time);
	while (tmcdrvdata->mode == CS_MODE_SYSFS
		&& tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		if (!atomic_read(&drvdata->irq_cnt)) {
			ret =  wait_event_interruptible_timeout(
				drvdata->usb_wait_wq,
				atomic_read(&drvdata->irq_cnt) > 0
				|| tmcdrvdata->mode != CS_MODE_SYSFS || tmcdrvdata->out_mode
				!= TMC_ETR_OUT_MODE_USB
				|| !drvdata->read_active, USB_TIME_OUT);
			if (ret == -ERESTARTSYS || tmcdrvdata->mode != CS_MODE_SYSFS
			|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB
			|| !drvdata->read_active) {
				dev_err(&tmcdrvdata->csdev->dev, "usb wait for irq_cnt exit, ret=%d\n", ret);
				break;
			}
			if (ret == 0) {
				ret = usb_transfer_small_packet(drvdata, &small_size);
				if (ret && ret != -EAGAIN)
					return;
				continue;
			}
		}

		if (unlikely(atomic_read(&drvdata->irq_cnt) > USB_TOTAL_IRQ)) {
			tmcdrvdata->usb_data->data_overwritten = true;
			dev_err_ratelimited(&tmcdrvdata->csdev->dev, "ETR data is overwritten. irq_cnt=%d\n",
				atomic_read(&drvdata->irq_cnt));
		} else {
			tmcdrvdata->usb_data->data_overwritten = false;
			ret = wait_event_interruptible_timeout(drvdata->usb_wait_wq,
				atomic_read(&drvdata->usb_free_buf) > 0
				|| tmcdrvdata->mode != CS_MODE_SYSFS || tmcdrvdata->out_mode
				!= TMC_ETR_OUT_MODE_USB
				|| !drvdata->read_active, HZ / 2);
			if (!ret) {
				dev_err_ratelimited(&tmcdrvdata->csdev->dev, "wait for usb free_buf\n");
				continue;
			} else if (ret == -ERESTARTSYS || tmcdrvdata->mode != CS_MODE_SYSFS
				|| tmcdrvdata->out_mode != TMC_ETR_OUT_MODE_USB
				|| !drvdata->read_active) {
				dev_err(&tmcdrvdata->csdev->dev, "usb wait for usb free_buf exit, ret=%d\n", ret);
				break;
			}
		}

		req_size = USB_BLK_SIZE - small_size;
		small_size = 0;
		actual_total = 0;

		if (req_size > 0) {
			seq++;
			req_sg_num = (req_size - 1) / PAGE_SIZE + 1;
			usb_req = kzalloc(sizeof(*usb_req), GFP_KERNEL);
			if (!usb_req)
				return;
			usb_req->sg = kcalloc(req_sg_num,
				sizeof(*(usb_req->sg)), GFP_KERNEL);
			if (!usb_req->sg) {
				kfree(usb_req);
				usb_req = NULL;
				return;
			}

			for (i = 0; i < req_sg_num; i++) {
				actual = tmc_etr_buf_get_data(etr_buf,
							drvdata->offset,
							PAGE_SIZE, &buf);

				if (actual <= 0 || actual > PAGE_SIZE) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					dev_err_ratelimited(
						&tmcdrvdata->csdev->dev,
						"Invalid data in ETR\n");
					return;
				}

				sg_set_buf(&usb_req->sg[i], buf, actual);

				if (i == 0)
					usb_req->buf = buf;
				if (i == req_sg_num - 1)
					sg_mark_end(&usb_req->sg[i]);

				if ((drvdata->offset + actual) >=
					tmcdrvdata->sysfs_buf->size)
					drvdata->offset = 0;
				else
					drvdata->offset += actual;
				actual_total += actual;
			}

			usb_req->length = actual_total;
			drvdata->usb_req = usb_req;
			usb_req->num_sgs = i;

			if (atomic_read(&drvdata->usb_free_buf) > 0) {
				ret = usb_qdss_write(tmcdrvdata->usb_data->usbch,
						drvdata->usb_req);
				if (ret) {
					kfree(usb_req->sg);
					kfree(usb_req);
					usb_req = NULL;
					drvdata->usb_req = NULL;
					dev_err_ratelimited(
						&tmcdrvdata->csdev->dev,
						"Write data failed:%d\n", ret);
					if (ret == -EAGAIN)
						continue;
					return;
				}
				drvdata->total_size += actual_total;
				atomic_dec(&drvdata->usb_free_buf);

			} else {
				dev_err_ratelimited(&tmcdrvdata->csdev->dev,
				"Drop data, offset = %d, seq = %d, irq = %d\n",
					drvdata->offset, seq,
					atomic_read(&drvdata->irq_cnt));
				tmcdrvdata->usb_data->drop_data_size += actual_total;
				kfree(usb_req->sg);
				kfree(usb_req);
				drvdata->usb_req = NULL;
			}
		}

		if (atomic_read(&drvdata->irq_cnt) > 0)
			atomic_dec(&drvdata->irq_cnt);

		ktime_get_real_ts64(&now);
		elapsed_ms = (now.tv_sec - tmcdrvdata->usb_data->start_time.tv_sec) * 1000;
		elapsed_ms += (now.tv_nsec - tmcdrvdata->usb_data->start_time.tv_nsec) / 1000000;
		if (elapsed_ms > 0) {
			tmcdrvdata->usb_data->in_data_speed =
			(drvdata->total_size + tmcdrvdata->usb_data->drop_data_size) * 1000 / elapsed_ms / SZ_1M;
			dev_dbg_ratelimited(&tmcdrvdata->csdev->dev,
				"in_data_speed = %lldMB/s, free_buf = %d, irq_cnt = %d\n",
				tmcdrvdata->usb_data->in_data_speed,
				atomic_read(&drvdata->usb_free_buf),
				atomic_read(&drvdata->irq_cnt));
		}
	}
	dev_err(&tmcdrvdata->csdev->dev, "TMC has been stopped.\n");
}

static void usb_write_done(struct byte_cntr *drvdata,
				   struct qdss_request *d_req)
{
	u64 elapsed_ms;
	struct timespec64 now;

	drvdata->tmcdrvdata->usb_data->out_data_size += d_req->length;
	atomic_inc(&drvdata->usb_free_buf);
	if (d_req->status && d_req->status != -ECONNRESET
			&& d_req->status != -ESHUTDOWN)
		pr_err_ratelimited("USB write failed err:%d\n", d_req->status);
	kfree(d_req->sg);
	kfree(d_req);
	wake_up(&drvdata->usb_wait_wq);

	ktime_get_real_ts64(&now);
	elapsed_ms = (now.tv_sec - drvdata->tmcdrvdata->usb_data->start_time.tv_sec) * 1000;
	elapsed_ms += (now.tv_nsec - drvdata->tmcdrvdata->usb_data->start_time.tv_nsec) / 1000000;
	if (elapsed_ms > 0) {
		drvdata->tmcdrvdata->usb_data->out_data_speed =
			drvdata->tmcdrvdata->usb_data->out_data_size * 1000 / elapsed_ms / SZ_1M;
		dev_dbg_ratelimited(&drvdata->tmcdrvdata->csdev->dev,
			"out_data_speed = %lldMB/s, free_buf = %d, irq_cnt = %d\n",
			drvdata->tmcdrvdata->usb_data->out_data_speed,
			atomic_read(&drvdata->usb_free_buf),
			atomic_read(&drvdata->irq_cnt));
	}
}

static int usb_bypass_init(struct byte_cntr *byte_cntr_data)
{
	byte_cntr_data->usb_wq = create_singlethread_workqueue("byte-cntr");
	if (!byte_cntr_data->usb_wq)
		return -ENOMEM;

	byte_cntr_data->offset = 0;
	mutex_init(&byte_cntr_data->usb_bypass_lock);
	init_waitqueue_head(&byte_cntr_data->usb_wait_wq);
	atomic_set(&byte_cntr_data->usb_free_buf, USB_BUF_NUM);
	INIT_WORK(&(byte_cntr_data->read_work), usb_read_work_fn);

	return 0;
}

void usb_notifier(void *priv, unsigned int event, struct qdss_request *d_req,
		  struct usb_qdss_ch *ch)
{
	struct tmc_drvdata *drvdata = priv;
	int ret = 0;

	if (!drvdata)
		return;

	if (drvdata->out_mode != TMC_ETR_OUT_MODE_USB) {
		dev_err(&drvdata->csdev->dev,
		"%s: ETR is not USB mode.\n", __func__);
		return;
	}

	switch (event) {
	case USB_QDSS_CONNECT:
		if (drvdata->mode == CS_MODE_DISABLED) {
			dev_err_ratelimited(&drvdata->csdev->dev,
				"%s: ETR is disabled.\n", __func__);
			return;
		}

		if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW) {
			ret = usb_bypass_start(drvdata->byte_cntr);
			if (ret < 0)
				return;

			usb_qdss_alloc_req(ch, USB_BUF_NUM);
			queue_work(drvdata->byte_cntr->usb_wq, &(drvdata->byte_cntr->read_work));
		}
		break;

	case USB_QDSS_DISCONNECT:
		if (drvdata->mode == CS_MODE_DISABLED) {
			dev_err_ratelimited(&drvdata->csdev->dev,
				 "%s: ETR is disabled.\n", __func__);
			return;
		}

		if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW) {
			usb_bypass_stop(drvdata->byte_cntr);
			flush_work(&((drvdata->byte_cntr->read_work)));
			usb_qdss_free_req(drvdata->usb_data->usbch);
		}
		break;

	case USB_QDSS_DATA_WRITE_DONE:
		if (drvdata->usb_data->usb_mode == TMC_ETR_USB_SW)
			usb_write_done(drvdata->byte_cntr, d_req);
		break;

	default:
		break;
	}

}

static bool tmc_etr_support_usb_bypass(struct device *dev)
{
	return fwnode_property_present(dev->fwnode, "qcom,sw-usb");
}

int tmc_usb_enable(struct tmc_usb_data *usb_data)
{
	struct tmc_drvdata *tmcdrvdata;

	if (!usb_data)
		return -EINVAL;

	tmcdrvdata = usb_data->tmcdrvdata;
	if (usb_data->usb_mode == TMC_ETR_USB_SW)
		usb_data->usbch = usb_qdss_open(USB_QDSS_CH_SW, tmcdrvdata, usb_notifier);

	if (IS_ERR_OR_NULL(usb_data->usbch)) {
		dev_err(&tmcdrvdata->csdev->dev, "usb_qdss_open failed for qdss.\n");
		return -ENODEV;
	}
	return 0;
}

void tmc_usb_disable(struct tmc_usb_data *usb_data)
{
	struct tmc_drvdata *tmcdrvdata = usb_data->tmcdrvdata;

	if (usb_data->usb_mode == TMC_ETR_USB_SW) {
		usb_bypass_stop(tmcdrvdata->byte_cntr);
		flush_work(&tmcdrvdata->byte_cntr->read_work);
	}

	if (usb_data->usbch)
		usb_qdss_close(usb_data->usbch);
	else
		dev_err(&tmcdrvdata->csdev->dev, "usb channel is null.\n");
}

int tmc_etr_usb_init(struct amba_device *adev,
		     struct tmc_drvdata *drvdata)
{
	struct device *dev = &adev->dev;
	struct tmc_usb_data *usb_data;
	struct byte_cntr *byte_cntr_data;
	int ret;

	usb_data = devm_kzalloc(dev, sizeof(*usb_data), GFP_KERNEL);
	if (!usb_data)
		return -ENOMEM;

	drvdata->usb_data = usb_data;
	drvdata->usb_data->tmcdrvdata = drvdata;
	byte_cntr_data = drvdata->byte_cntr;

	if (tmc_etr_support_usb_bypass(dev)) {
		usb_data->usb_mode = TMC_ETR_USB_SW;
		usb_data->drop_data_size = 0;
		usb_data->data_overwritten = false;
		if (!byte_cntr_data)
			return -EINVAL;

		ret = usb_bypass_init(byte_cntr_data);
		if (ret)
			return -EINVAL;

		return 0;
	}

	usb_data->usb_mode = TMC_ETR_USB_NONE;
	pr_err("%s: ETR usb property is not configured!\n",
					dev_name(dev));
	return 0;
}
