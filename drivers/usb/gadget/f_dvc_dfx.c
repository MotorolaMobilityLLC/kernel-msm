/*
 * Gadget Driver for Android DvC.Dfx Debug Capability
 *
 * Copyright (C) 2008-2010, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/usb/debug.h>
#include <linux/sdm.h>
#include <asm/intel_soc_debug.h>

#define DFX_RX_REQ_MAX 1
#define DFX_TX_REQ_MAX 2
#define DFX_BULK_REQ_SIZE 64

#define CONFIG_BOARD_MRFLD_VV

struct dvc_dfx_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;
	u8	ctrl_id, data_id;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int transfering;
	int online;
	int online_ctrl;
	int online_data;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;

	struct usb_request *rx_req[DFX_RX_REQ_MAX];

	struct list_head tx_idle;
	struct list_head tx_xfer;
};

static struct usb_interface_assoc_descriptor dfx_iad_desc = {
	.bLength		= sizeof(dfx_iad_desc),
	.bDescriptorType	= USB_DT_INTERFACE_ASSOCIATION,
	/* .bFirstInterface	= DYNAMIC, */
	.bInterfaceCount	= 2, /* debug control + data */
	.bFunctionClass		= USB_CLASS_DEBUG,
	.bFunctionSubClass	= USB_SUBCLASS_DVC_DFX,
	/* .bFunctionProtocol	= DC_PROTOCOL_VENDOR, */
	/* .iFunction		= 0, */
};

static struct usb_interface_descriptor dfx_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bNumEndpoints          = 0,
	.bInterfaceClass        = USB_CLASS_DEBUG,
	.bInterfaceSubClass     = USB_SUBCLASS_DEBUG_CONTROL,
	/* .bInterfaceProtocol     = DC_PROTOCOL_VENDOR, */
};

#define DC_DBG_ATTRI_LENGTH	DC_DBG_ATTRI_SIZE(2, 32)
/* 1 input terminal, 1 output terminal and 1 feature unit */
#define DC_DBG_TOTAL_LENGTH (DC_DBG_ATTRI_LENGTH)

DECLARE_DC_DEBUG_ATTR_DESCR(DVCD, 2, 32);

static struct DC_DEBUG_ATTR_DESCR(DVCD) dfx_debug_attri_desc = {
	.bLength		= DC_DBG_ATTRI_LENGTH,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= DC_DEBUG_ATTRIBUTES,
	.bcdDC			= __constant_cpu_to_le16(0x0100),
	.wTotalLength		= __constant_cpu_to_le16(DC_DBG_TOTAL_LENGTH),
	.bmSupportedFeatures	= 0, /* Debug Event Supported, per SAS */
	.bControlSize		= 2,
	.bmControl		= {	/* per SAS */
		[0]		= 0xFF,
		[1]		= 0x3F,
	},
	.wAuxDataSize		= __constant_cpu_to_le16(0x20),
/* per SAS v0.3*/
	.dInputBufferSize	= __constant_cpu_to_le32(0x40),
	.dOutputBufferSize	= __constant_cpu_to_le32(0x80),
	.qBaseAddress		= 0, /* revision */
	.hGlobalID		= { /* revision */
		[0]		= 0,
		[1]		= 0,
	}
};

static struct usb_interface_descriptor dfx_data_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bAlternateSetting	= 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_DEBUG,
	.bInterfaceSubClass     = USB_SUBCLASS_DVC_DFX,
	/* .bInterfaceProtocol     = DC_PROTOCOL_VENDOR, */
};

static struct usb_endpoint_descriptor dfx_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor dfx_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor dfx_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor dfx_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor dfx_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor dfx_superspeed_in_comp_desc = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst		= 0,
	.bmAttributes		= 0,
};

static struct usb_endpoint_descriptor dfx_superspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor dfx_superspeed_out_comp_desc = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst		= 0,
	.bmAttributes		= 0,
};

/* no INPUT/OUTPUT CONNECTION and UNIT descriptors for DvC.DFx */
static struct usb_descriptor_header *fs_dfx_descs[] = {
	(struct usb_descriptor_header *) &dfx_iad_desc,
	(struct usb_descriptor_header *) &dfx_data_interface_desc,
	(struct usb_descriptor_header *) &dfx_fullspeed_in_desc,
	(struct usb_descriptor_header *) &dfx_fullspeed_out_desc,

	(struct usb_descriptor_header *) &dfx_interface_desc,
	(struct usb_descriptor_header *) &dfx_debug_attri_desc,
	NULL,
};

static struct usb_descriptor_header *hs_dfx_descs[] = {
	(struct usb_descriptor_header *) &dfx_iad_desc,
	(struct usb_descriptor_header *) &dfx_data_interface_desc,
	(struct usb_descriptor_header *) &dfx_highspeed_in_desc,
	(struct usb_descriptor_header *) &dfx_highspeed_out_desc,

	(struct usb_descriptor_header *) &dfx_interface_desc,
	(struct usb_descriptor_header *) &dfx_debug_attri_desc,
	NULL,
};

static struct usb_descriptor_header *ss_dfx_descs[] = {
	(struct usb_descriptor_header *) &dfx_iad_desc,
	(struct usb_descriptor_header *) &dfx_data_interface_desc,
	(struct usb_descriptor_header *) &dfx_superspeed_in_desc,
	(struct usb_descriptor_header *) &dfx_superspeed_in_comp_desc,
	(struct usb_descriptor_header *) &dfx_superspeed_out_desc,
	(struct usb_descriptor_header *) &dfx_superspeed_out_comp_desc,

	(struct usb_descriptor_header *) &dfx_interface_desc,
	(struct usb_descriptor_header *) &dfx_debug_attri_desc,
	NULL,
};

/* string descriptors: */

#define DVCDFX_CTRL_IDX	0
#define DVCDFX_DATA_IDX	1
#define DVCDFX_IAD_IDX	2

/* static strings, in UTF-8 */
static struct usb_string dfx_string_defs[] = {
	[DVCDFX_CTRL_IDX].s = "Debug Sub-Class DvC.DFx (Control)",
	[DVCDFX_DATA_IDX].s = "Debug Sub-Class DvC.DFx (Data)",
	[DVCDFX_IAD_IDX].s = "Debug Sub-Class DvC.DFx",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings dfx_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		dfx_string_defs,
};

static struct usb_gadget_strings *dfx_strings[] = {
	&dfx_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/

/* temporary variable used between dvc_dfx_open() and dvc_dfx_gadget_bind() */
static struct dvc_dfx_dev *_dvc_dfx_dev;

static inline struct dvc_dfx_dev *func_to_dvc_dfx(struct usb_function *f)
{
	return container_of(f, struct dvc_dfx_dev, function);
}

static int dvc_dfx_is_enabled(void)
{
	if ((!cpu_has_debug_feature(DEBUG_FEATURE_USB3DFX)) ||
	    (!stm_is_enabled())) {
		pr_info("%s STM and/or USB3DFX is not enabled\n", __func__);
		return 0;
	}
	return 1;
}

static struct usb_request *dvc_dfx_request_new(struct usb_ep *ep,
					       int buffer_size, dma_addr_t dma)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;


	req->dma = dma;
	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void dvc_dfx_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void dvc_dfx_req_put(struct dvc_dfx_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *dvc_dfx_req_get(struct dvc_dfx_dev *dev,
					   struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void dvc_dfx_set_disconnected(struct dvc_dfx_dev *dev)
{
	dev->transfering = 0;
}

static void dvc_dfx_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;

	if (req->status != 0)
		dvc_dfx_set_disconnected(dev);

	dvc_dfx_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void dvc_dfx_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;

	if (req->status != 0)
		dvc_dfx_set_disconnected(dev);
	wake_up(&dev->read_wq);
}


static inline int dvc_dfx_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void dvc_dfx_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static int dfx_create_bulk_endpoints(struct dvc_dfx_dev *dev,
				     struct usb_endpoint_descriptor *in_desc,
				     struct usb_endpoint_descriptor *out_desc,
				     struct usb_ss_ep_comp_descriptor *in_comp_desc,
				     struct usb_ss_ep_comp_descriptor *out_comp_desc
	)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	pr_debug("%s dev: %p\n", __func__, dev);

	in_desc->bEndpointAddress |= 0x8;
	ep = usb_ep_autoconfig_ss(cdev->gadget, in_desc, in_comp_desc);
	if (!ep) {
		pr_debug("%s for ep_in failed\n", __func__);
		return -ENODEV;
	}
	pr_debug("%s for ep_in got %s\n", __func__, ep->name);

	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	out_desc->bEndpointAddress |= 0x8;
	ep = usb_ep_autoconfig_ss(cdev->gadget, out_desc, out_comp_desc);
	if (!ep) {
		pr_debug("%s for ep_out failed\n", __func__);
		return -ENODEV;
	}
	pr_debug("%s for ep_out got %s\n", __func__, ep->name);

	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < DFX_TX_REQ_MAX; i++) {
		if (!(i % 2))
			req = dvc_dfx_request_new(dev->ep_in,
				DFX_BULK_BUFFER_SIZE,
				(dma_addr_t)DFX_BULK_IN_BUFFER_ADDR);
		else
			req = dvc_dfx_request_new(dev->ep_in,
				DFX_BULK_BUFFER_SIZE,
				(dma_addr_t)DFX_BULK_IN_BUFFER_ADDR_2);
		if (!req)
			goto fail;
		req->complete = dvc_dfx_complete_in;
		dvc_dfx_req_put(dev, &dev->tx_idle, req);
	}
	for (i = 0; i < DFX_RX_REQ_MAX; i++) {
		req = dvc_dfx_request_new(dev->ep_out, DFX_BULK_BUFFER_SIZE,
			(dma_addr_t)DFX_BULK_OUT_BUFFER_ADDR);
		if (!req)
			goto fail;
		req->complete = dvc_dfx_complete_out;
		dev->rx_req[i] = req;
	}

	return 0;

fail:
	pr_err("%s could not allocate requests\n", __func__);
	while ((req = dvc_dfx_req_get(dev, &dev->tx_idle)))
		dvc_dfx_request_free(req, dev->ep_out);
	for (i = 0; i < DFX_RX_REQ_MAX; i++)
		dvc_dfx_request_free(dev->rx_req[i], dev->ep_out);
	return -1;
}

static ssize_t dvc_dfx_start_transfer(size_t count)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;
	struct usb_request *req = NULL;
	int r = count, xfer;
	int ret = -ENODEV;


	pr_info("%s start\n", __func__);
	if (!_dvc_dfx_dev)
		return ret;

	if (dvc_dfx_lock(&dev->read_excl)
	    && dvc_dfx_lock(&dev->write_excl))
		return -EBUSY;

	/* we will block until enumeration completes */
	while (!(dev->online || dev->error)) {
		pr_debug("%s waiting for online state\n", __func__);
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->error));

		if (ret < 0) {
			/* not at CONFIGURED state */
			pr_info("%s USB not at CONFIGURED\n", __func__);
			dvc_dfx_unlock(&dev->read_excl);
			dvc_dfx_unlock(&dev->write_excl);
			return ret;
		}
	}

	/* queue a ep_in endless request */
	while (r > 0) {
		if (dev->error) {
			pr_debug("%s dev->error\n", __func__);
			r = -EIO;
			break;
		}

		if (!dev->online) {
			pr_debug("%s !dev->online issue\n", __func__);
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
				dev->error || !dev->online ||
				(req = dvc_dfx_req_get(dev, &dev->tx_idle)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > DFX_BULK_BUFFER_SIZE)
				xfer = DFX_BULK_BUFFER_SIZE;
			else
				xfer = count;

			req->no_interrupt = 1;
			req->context = &dev->function;
			req->length = xfer;
			pr_debug("%s queue tx_idle list req to dev->ep_in\n",
				__func__);
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_err("%s xfer error %d\n", __func__, ret);
					dev->error = 1;
				r = -EIO;
				break;
			}
			pr_debug("%s xfer=%d/%d  queued req/%p\n", __func__,
				xfer, r, req);
			dvc_dfx_req_put(dev, &dev->tx_xfer, req);
			r -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}
	if (req) {
		pr_debug("%s req re-added to tx_idle on error\n", __func__);
		dvc_dfx_req_put(dev, &dev->tx_idle, req);
	}

	pr_debug("%s rx_req to dev->ep_out\n", __func__);
	/* queue a ep_out endless request */
	req = dev->rx_req[0];
	req->length = DFX_BULK_BUFFER_SIZE;
	req->no_interrupt = 1;
	req->context = &dev->function;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_err("%s failed to queue out req %p (%d)\n",
		       __func__, req, req->length);
		r = -EIO;
	} else {
		dev->transfering = 1;
	}

	dvc_dfx_unlock(&dev->read_excl);
	dvc_dfx_unlock(&dev->write_excl);
	pr_debug("%s returning\n", __func__);
	return r;
}

static int dvc_dfx_disable_transfer(void)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;
	struct usb_request *req;
	int r = 1;
	int ret;


	pr_info("%s start\n", __func__);
	if (!_dvc_dfx_dev)
		return -ENODEV;

	if (dvc_dfx_lock(&dev->read_excl)
	    && dvc_dfx_lock(&dev->write_excl))
		return -EBUSY;

	if (dev->error) {
		pr_debug("%s dev->error\n", __func__);
		r = -EIO;
		goto end;
	}

	if ((!dev->online) || (!dev->transfering)) {
		pr_debug("%s !dev->online OR !dev->transfering\n", __func__);
		r = -EIO;
		goto end;
	}

	/* get an xfer tx request to use */
	while ((req = dvc_dfx_req_get(dev, &dev->tx_xfer))) {
		ret = usb_ep_dequeue(dev->ep_in, req);
		if (ret < 0) {
			pr_err("%s dequeue error %d\n", __func__, ret);
			dev->error = 1;
			r = -EIO;
			goto end;
		}
		pr_debug("%s dequeued tx req/%p\n", __func__, req);
	}
	ret = usb_ep_dequeue(dev->ep_out, dev->rx_req[0]);
	if (ret < 0) {
		pr_err("%s dequeue rx error %d\n", __func__, ret);
		dev->error = 1;
		r = -EIO;
		goto end;
	}

end:
	dvc_dfx_unlock(&dev->read_excl);
	dvc_dfx_unlock(&dev->write_excl);
	return r;
}

static int dvc_dfx_open(struct inode *ip, struct file *fp)
{
	pr_info("%s\n", __func__);
	if (!_dvc_dfx_dev)
		return -ENODEV;

	if (dvc_dfx_lock(&_dvc_dfx_dev->open_excl))
		return -EBUSY;

	fp->private_data = _dvc_dfx_dev;

	/* clear the error latch */
	_dvc_dfx_dev->error = 0;
	_dvc_dfx_dev->transfering = 0;

	return 0;
}

static int dvc_dfx_release(struct inode *ip, struct file *fp)
{
	pr_info("%s\n", __func__);

	dvc_dfx_unlock(&_dvc_dfx_dev->open_excl);
	return 0;
}

/* file operations for DvC.Dfx device /dev/usb_dvc_dfx */
static const struct file_operations dvc_dfx_fops = {
	.owner = THIS_MODULE,
	.open = dvc_dfx_open,
	.release = dvc_dfx_release,
};

static struct miscdevice dvc_dfx_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "usb_dvc_dfx",
	.fops = &dvc_dfx_fops,
};

static int dvc_dfx_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;
	int	value = -EOPNOTSUPP;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);

	pr_debug("%s %02x.%02x v%04x i%04x l%u\n", __func__,
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	/* DC_REQUEST_SET_RESET ... stop active transfer */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| DC_REQUEST_SET_RESET:
		if (w_index != dev->data_id)
			goto invalid;

		pr_info("%s DC_REQUEST_SET_RESET v%04x i%04x l%u\n", __func__,
			w_value, w_index, w_length);

		dvc_dfx_disable_transfer();
		value = 0;
		break;

	default:
invalid:
		pr_debug("unknown class-specific control req "
			 "%02x.%02x v%04x i%04x l%u\n",
			 ctrl->bRequestType, ctrl->bRequest,
			 w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		cdev->req->zero = 0;
		cdev->req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (value < 0)
			pr_err("%s setup response queue error\n", __func__);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int
dvc_dfx_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev	*cdev = c->cdev;
	struct dvc_dfx_dev		*dev = func_to_dvc_dfx(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	pr_info("%s dev: %p\n", __func__, dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->data_id = id;
	dfx_data_interface_desc.bInterfaceNumber = id;
	dfx_iad_desc.bFirstInterface = id;

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->ctrl_id = id;
	dfx_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = dfx_create_bulk_endpoints(dev, &dfx_fullspeed_in_desc,
					&dfx_fullspeed_out_desc,
					&dfx_superspeed_in_comp_desc,
					&dfx_superspeed_out_comp_desc
		);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		dfx_highspeed_in_desc.bEndpointAddress =
			dfx_fullspeed_in_desc.bEndpointAddress;
		dfx_highspeed_out_desc.bEndpointAddress =
			dfx_fullspeed_out_desc.bEndpointAddress;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		dfx_superspeed_in_desc.bEndpointAddress =
			dfx_fullspeed_in_desc.bEndpointAddress;

		dfx_superspeed_out_desc.bEndpointAddress =
			dfx_fullspeed_out_desc.bEndpointAddress;
	}

	pr_info("%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
dvc_dfx_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct dvc_dfx_dev	*dev = func_to_dvc_dfx(f);
	struct usb_request *req;
	int i;

	dev->online = 0;
	dev->online_ctrl = 0;
	dev->online_data = 0;
	dev->transfering = 0;
	dev->error = 0;

	dfx_string_defs[DVCDFX_CTRL_IDX].id = 0;

	wake_up(&dev->read_wq);

	for (i = 0; i < DFX_RX_REQ_MAX; i++)
		dvc_dfx_request_free(dev->rx_req[i], dev->ep_out);
	while ((req = dvc_dfx_req_get(dev, &dev->tx_idle)))
		dvc_dfx_request_free(req, dev->ep_in);

}

static int dvc_dfx_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct dvc_dfx_dev	*dev = func_to_dvc_dfx(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	pr_info("%s intf: %d alt: %d\n", __func__, intf, alt);
	if (intf == dfx_data_interface_desc.bInterfaceNumber) {
		ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_by_speed in error %d\n",
				__func__, intf, alt, ret);
			return ret;
		}
		ret = usb_ep_enable(dev->ep_in);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_enable in err %d\n",
				__func__, intf, alt, ret);
			return ret;
		}

		ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_enable out error %d\n",
				__func__, intf, alt, ret);
			return ret;
		}

		ret = usb_ep_enable(dev->ep_out);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_enable out err %d\n",
				__func__, intf, alt, ret);
			usb_ep_disable(dev->ep_in);
			return ret;
		}
		dev->online_data = 1;
	}
	if (intf == dfx_interface_desc.bInterfaceNumber)
		dev->online_ctrl = 1;

	if (dev->online_data && dev->online_ctrl) {
		dev->online = 1;
		dev->error = 0;
	}

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void dvc_dfx_function_disable(struct usb_function *f)
{
	struct dvc_dfx_dev	*dev = func_to_dvc_dfx(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	pr_info("%s cdev %p\n", __func__, cdev);

	if (dev->transfering)
		dvc_dfx_disable_transfer();

	dev->online = 0;
	dev->online_ctrl = 0;
	dev->online_data = 0;
	dev->error = 0;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	pr_debug("%s disabled\n", dev->function.name);
}

static int dvc_dfx_bind_config(struct usb_configuration *c)
{
	struct dvc_dfx_dev *dev = _dvc_dfx_dev;
	int status;

	pr_info("%s\n", __func__);

	if (dfx_string_defs[DVCDFX_CTRL_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dfx_string_defs[DVCDFX_CTRL_IDX].id = status;

		dfx_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dfx_string_defs[DVCDFX_DATA_IDX].id = status;

		dfx_data_interface_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		dfx_string_defs[DVCDFX_IAD_IDX].id = status;

		dfx_iad_desc.iFunction = status;
	}

	dev->cdev = c->cdev;
	dev->function.name = "dvcdfx";
	dev->function.fs_descriptors = fs_dfx_descs;
	dev->function.hs_descriptors = hs_dfx_descs;
	dev->function.ss_descriptors = ss_dfx_descs;
	dev->function.strings = dfx_strings;
	dev->function.bind = dvc_dfx_function_bind;
	dev->function.unbind = dvc_dfx_function_unbind;
	dev->function.set_alt = dvc_dfx_function_set_alt;
	dev->function.disable = dvc_dfx_function_disable;

	return usb_add_function(c, &dev->function);
}

static int dvc_dfx_setup(void)
{
	struct dvc_dfx_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->tx_xfer);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	_dvc_dfx_dev = dev;

	ret = misc_register(&dvc_dfx_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	pr_err("DvC.Dfx gadget driver failed to initialize\n");
	return ret;
}

static void dvc_dfx_cleanup(void)
{
	misc_deregister(&dvc_dfx_device);

	kfree(_dvc_dfx_dev);
	_dvc_dfx_dev = NULL;
}
