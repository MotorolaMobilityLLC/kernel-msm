/*
 * Gadget Driver for Android DvC.Trace Debug Capability
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

#define TRACE_TX_REQ_MAX 3

#define CONFIG_BOARD_MRFLD_VV

struct dvc_trace_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;
	u8	ctrl_id, data_id;
	u8	class_id, subclass_id;

	struct usb_ep *ep_in;

	int online;
	int online_data;
	int online_ctrl;
	int transfering;
	int error;

	atomic_t write_excl;
	atomic_t open_excl;

	wait_queue_head_t write_wq;

	struct list_head tx_idle;
	struct list_head tx_xfer;
};

static struct usb_interface_assoc_descriptor trace_iad_desc = {
	.bLength		= sizeof(trace_iad_desc),
	.bDescriptorType	= USB_DT_INTERFACE_ASSOCIATION,
	/* .bFirstInterface	= DYNAMIC, */
	.bInterfaceCount	= 2, /* debug control + data */
	.bFunctionClass		= USB_CLASS_DEBUG,
	.bFunctionSubClass	= USB_SUBCLASS_DVC_TRACE,
	/* .bFunctionProtocol	= 0, */
	/* .iFunction		= DYNAMIC, */
};

static struct usb_interface_descriptor trace_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bNumEndpoints          = 0,
	.bInterfaceClass        = USB_CLASS_DEBUG,
	.bInterfaceSubClass     = USB_SUBCLASS_DEBUG_CONTROL,
	/* .bInterfaceProtocol     = 0, */
};

#define DC_DVCTRACE_ATTRI_LENGTH	DC_DBG_ATTRI_SIZE(2, 32)
/* 1 input terminal, 1 output terminal and 1 feature unit */
#define DC_DVCTRACE_TOTAL_LENGTH (DC_DVCTRACE_ATTRI_LENGTH \
	+ DC_OUTPUT_CONNECTION_SIZE \
	+ DC_OUTPUT_CONNECTION_SIZE \
	+ DC_DBG_UNIT_SIZE(STM_NB_IN_PINS, 2, 2, 24))

DECLARE_DC_DEBUG_ATTR_DESCR(DVCT, 2, 32);

static struct DC_DEBUG_ATTR_DESCR(DVCT) trace_debug_attri_desc = {
	.bLength		= DC_DVCTRACE_ATTRI_LENGTH,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= DC_DEBUG_ATTRIBUTES,
	.bcdDC			= __constant_cpu_to_le16(0x0100),
	.wTotalLength	= __constant_cpu_to_le16(DC_DVCTRACE_TOTAL_LENGTH),
	.bmSupportedFeatures	= 0, /* Debug Event Supported, per SAS */
	.bControlSize		= 2,
	.bmControl		= {	/* per SAS */
		[0]		= 0xFF,
		[1]		= 0x3F,
	},
	.wAuxDataSize		= __constant_cpu_to_le16(0x20),
	.dInputBufferSize	= __constant_cpu_to_le32(0x00), /* per SAS */
	.dOutputBufferSize = __constant_cpu_to_le32(TRACE_BULK_BUFFER_SIZE),
	.qBaseAddress		= 0, /* revision */
	.hGlobalID		= { /* revision */
		[0]		= 0,
		[1]		= 0,
	}
};

static struct dc_output_connection_descriptor trace_output_conn_usb_desc = {
	.bLength		= DC_OUTPUT_CONNECTION_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= DC_OUTPUT_CONNECTION,
	.bConnectionID		= 0x01, /* USB */
	.bConnectionType	= DC_CONNECTION_USB,
	.bAssocConnection	= 0, /* No related input-connection */
	.wSourceID		= __constant_cpu_to_le16(0x01),
	/* .iConnection		= DYNAMIC, */
};

static struct dc_output_connection_descriptor trace_output_conn_pti_desc = {
	.bLength		= DC_OUTPUT_CONNECTION_SIZE,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= DC_OUTPUT_CONNECTION,
	.bConnectionID		= 0, /* PTI */
	.bConnectionType	= DC_CONNECTION_DEBUG_DATA,
	.bAssocConnection	= 0, /* No related input-connection */
	.wSourceID		= __constant_cpu_to_le16(0x01),
	/* .iConnection		= DYNAMIC, */
};

#define DC_DVCTRACE_UNIT_LENGTH	DC_DBG_UNIT_SIZE(STM_NB_IN_PINS, 2, 2, 24)

DECLARE_DC_DEBUG_UNIT_DESCRIPTOR(STM_NB_IN_PINS, 2, 2, 24);

static struct DC_DEBUG_UNIT_DESCRIPTOR(STM_NB_IN_PINS, 2, 2, 24)
		trace_debug_unit_stm_desc = {
	.bLength		= DC_DVCTRACE_UNIT_LENGTH,
	.bDescriptorType	= USB_DT_CS_INTERFACE,
	.bDescriptorSubtype	= DC_DEBUG_UNIT,
	.bUnitID		= 0x01, /* per SAS */
/* STM Trace Unit processor: revision */
	.bDebugUnitType		= DC_UNIT_TYPE_TRACE_PROC,
 /* STM: Trace compressor controller */
	.bDebugSubUnitType	= DC_UNIT_SUBTYPE_TRACEZIP,
	.bAliasUnitID		= 0, /* no associated debug unit */
	.bNrInPins		= STM_NB_IN_PINS, /* p */
/* wSourceID  contains STM_NB_IN_PINS elements */
/*	.wSourceID		= {0}, */
	.bNrOutPins		= 0x02,	/* q */
	.dTraceFormat		= {
		[0]	= __constant_cpu_to_le32(DC_TRACE_MIPI_FORMATED_STPV1),
		[1]	= __constant_cpu_to_le32(DC_TRACE_MIPI_FORMATED_STPV1),
	},
	.dStreamID		= __constant_cpu_to_le32(0xFFFFFFFF),
	.bControlSize		= 0x02,	/* n */
	.bmControl		= {
		[0]		= 0xFF,
		[1]		= 0x3F,
	},
	.wAuxDataSize		= __constant_cpu_to_le16(24), /* m */
	.qBaseAddress		= 0, /* revision */
	.hIPID			= {
		[0]		= 0,
		[1]		= 0,
	},
	/* .iDebugUnitType		= DYNAMIC, */
};

static struct usb_interface_descriptor trace_data_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bAlternateSetting	= 0,
	.bNumEndpoints          = 1,
	.bInterfaceClass        = USB_CLASS_DEBUG,
	.bInterfaceSubClass     = USB_SUBCLASS_DVC_TRACE,
	/* .bInterfaceProtocol     = 0, */
};

static struct usb_endpoint_descriptor trace_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor trace_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor trace_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor trace_superspeed_in_comp_desc = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst		= 0,
	.bmAttributes		= 0,
};

static struct usb_descriptor_header *fs_trace_descs[] = {
	(struct usb_descriptor_header *) &trace_iad_desc,
	(struct usb_descriptor_header *) &trace_data_interface_desc,
	(struct usb_descriptor_header *) &trace_fullspeed_in_desc,
	(struct usb_descriptor_header *) &trace_interface_desc,
	(struct usb_descriptor_header *) &trace_debug_attri_desc,
	(struct usb_descriptor_header *) &trace_output_conn_pti_desc,
	(struct usb_descriptor_header *) &trace_output_conn_usb_desc,
	(struct usb_descriptor_header *) &trace_debug_unit_stm_desc,
	NULL,
};

static struct usb_descriptor_header *hs_trace_descs[] = {
	(struct usb_descriptor_header *) &trace_iad_desc,
	(struct usb_descriptor_header *) &trace_data_interface_desc,
	(struct usb_descriptor_header *) &trace_highspeed_in_desc,
	(struct usb_descriptor_header *) &trace_interface_desc,
	(struct usb_descriptor_header *) &trace_debug_attri_desc,
	(struct usb_descriptor_header *) &trace_output_conn_pti_desc,
	(struct usb_descriptor_header *) &trace_output_conn_usb_desc,
	(struct usb_descriptor_header *) &trace_debug_unit_stm_desc,
	NULL,
};

static struct usb_descriptor_header *ss_trace_descs[] = {
	(struct usb_descriptor_header *) &trace_iad_desc,
	(struct usb_descriptor_header *) &trace_data_interface_desc,
	(struct usb_descriptor_header *) &trace_superspeed_in_desc,
	(struct usb_descriptor_header *) &trace_superspeed_in_comp_desc,
	(struct usb_descriptor_header *) &trace_interface_desc,
	(struct usb_descriptor_header *) &trace_debug_attri_desc,
	(struct usb_descriptor_header *) &trace_output_conn_pti_desc,
	(struct usb_descriptor_header *) &trace_output_conn_usb_desc,
	(struct usb_descriptor_header *) &trace_debug_unit_stm_desc,
	NULL,
};

/* string descriptors: */
#define DVCTRACE_CTRL_IDX	0
#define DVCTRACE_DATA_IDX	1
#define DVCTRACE_IAD_IDX	2
#define DVCTRACE_CONN_PTI_IDX	3
#define DVCTRACE_CONN_USB_IDX	4
#define DVCTRACE_UNIT_STM_IDX	5

/* static strings, in UTF-8 */
static struct usb_string trace_string_defs[] = {
	[DVCTRACE_CTRL_IDX].s = "Debug Sub-Class DvC.Trace (Control)",
	[DVCTRACE_DATA_IDX].s = "Debug Sub-Class DvC.Trace (Data)",
	[DVCTRACE_IAD_IDX].s = "Debug Sub-Class DvC.Trace",
	[DVCTRACE_CONN_PTI_IDX].s = "MIPI PTIv1 output Connector ",
	[DVCTRACE_CONN_USB_IDX].s = "USB Device output connector",
	[DVCTRACE_UNIT_STM_IDX].s = "MIPI STM Debug Unit",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings trace_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		trace_string_defs,
};

static struct usb_gadget_strings *trace_strings[] = {
	&trace_string_table,
	NULL,
};

/* temporary var used between dvc_trace_open() and dvc_trace_gadget_bind() */
static struct dvc_trace_dev *_dvc_trace_dev;

static inline struct dvc_trace_dev *func_to_dvc_trace(struct usb_function *f)
{
	return container_of(f, struct dvc_trace_dev, function);
}

static int dvc_trace_is_enabled(void)
{
	if (!stm_is_enabled()) {
		pr_info("%s STM/PTI block is not enabled\n", __func__);
		return 0;
	}
	return 1;
}

static struct usb_request *dvc_trace_request_new(struct usb_ep *ep,
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

static void dvc_trace_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void dvc_trace_req_put(struct dvc_trace_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *dvc_trace_req_get(struct dvc_trace_dev *dev,
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

static void dvc_trace_set_disconnected(struct dvc_trace_dev *dev)
{
	dev->transfering = 0;
}

static void dvc_trace_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct dvc_trace_dev *dev = _dvc_trace_dev;

	if (req->status != 0)
		dvc_trace_set_disconnected(dev);

	dvc_trace_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static inline int dvc_trace_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void dvc_trace_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static int trace_create_bulk_endpoints(struct dvc_trace_dev *dev,
				       struct usb_endpoint_descriptor *in_desc,
				       struct usb_ss_ep_comp_descriptor *in_comp_desc
	)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	pr_debug("%s dev: %p\n", __func__, dev);

	in_desc->bEndpointAddress |= 0x1;
	ep = usb_ep_autoconfig_ss(cdev->gadget, in_desc, in_comp_desc);
	if (!ep) {
		pr_err("%s usb_ep_autoconfig for ep_in failed\n", __func__);
		return -ENODEV;
	}
	pr_debug("%s usb_ep_autoconfig for ep_in got %s\n", __func__, ep->name);

	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	for (i = 0; i < TRACE_TX_REQ_MAX; i++) {
		req = dvc_trace_request_new(dev->ep_in, TRACE_BULK_BUFFER_SIZE,
			(dma_addr_t)TRACE_BULK_IN_BUFFER_ADDR);
		if (!req)
			goto fail;
		req->complete = dvc_trace_complete_in;
		dvc_trace_req_put(dev, &dev->tx_idle, req);
		pr_debug("%s req= %p : for %s predefined TRB\n", __func__,
			req, ep->name);
	}

	return 0;

fail:
	pr_err("%s could not allocate requests\n", __func__);
	return -1;
}

static ssize_t dvc_trace_start_transfer(size_t count)
{
	struct dvc_trace_dev *dev = _dvc_trace_dev;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret = -EINVAL;

	pr_debug("%s\n", __func__);
	if (!_dvc_trace_dev)
		return -ENODEV;

	if (dvc_trace_lock(&dev->write_excl))
		return -EBUSY;

	/* we will block until enumeration completes */
	while (!(dev->online || dev->error)) {
		pr_debug("%s: waiting for online state\n", __func__);
		ret = wait_event_interruptible(dev->write_wq,
				(dev->online || dev->error));

		if (ret < 0) {
			/* not at CONFIGURED state */
			pr_info("%s !dev->online already\n", __func__);
			dvc_trace_unlock(&dev->write_excl);
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
			pr_debug("%s !dev->online\n", __func__);
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
				dev->error || !dev->online ||
				(req = dvc_trace_req_get(dev, &dev->tx_idle)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > TRACE_BULK_BUFFER_SIZE)
				xfer = TRACE_BULK_BUFFER_SIZE;
			else
				xfer = count;
			pr_debug("%s queue tx_idle list req to dev->ep_in\n",
				__func__);
			req->no_interrupt = 1;
			req->context = &dev->function;
			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_err("%s: xfer error %d\n", __func__, ret);
				dev->error = 1;
				dev->transfering = 0;
				r = -EIO;
				break;
			}
			pr_debug("%s: xfer=%d/%d  queued req/%p\n", __func__,
				xfer, r, req);
			dvc_trace_req_put(dev, &dev->tx_xfer, req);
			r -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}
	if (req) {
		pr_debug("%s req re-added to tx_idle on error\n", __func__);
		dvc_trace_req_put(dev, &dev->tx_idle, req);
	} else {
		dev->transfering = 1;
	}
	dvc_trace_unlock(&dev->write_excl);
	pr_debug("%s end\n", __func__);
	return ret;
}

static int dvc_trace_disable_transfer(void)
{
	struct dvc_trace_dev *dev = _dvc_trace_dev;
	struct usb_request *req = 0;
	int ret;

	pr_debug("%s\n", __func__);
	if (!_dvc_trace_dev)
		return -ENODEV;

	if (dvc_trace_lock(&dev->write_excl))
		return -EBUSY;

	if (dev->error) {
		pr_debug("%s dev->error\n", __func__);
		dvc_trace_unlock(&dev->write_excl);
		return -EIO;
	}

	if ((!dev->online) || (!dev->transfering)) {
		pr_debug("%s !dev->online OR !dev->transfering\n", __func__);
		dvc_trace_unlock(&dev->write_excl);
		return -EIO;
	}

	/* get an xfer tx request to use */
	while ((req = dvc_trace_req_get(dev, &dev->tx_xfer))) {
		ret = usb_ep_dequeue(dev->ep_in, req);
		if (ret < 0) {
			pr_err("%s: dequeue error %d\n", __func__, ret);
			dev->error = 1;
			dvc_trace_unlock(&dev->write_excl);
			return -EIO;
		}
		pr_debug("%s: dequeued req/%p\n", __func__, req);
	}

	dvc_trace_unlock(&dev->write_excl);
	return 1;
}

static int dvc_trace_open(struct inode *ip, struct file *fp)
{
	pr_debug("%s\n", __func__);
	if (!_dvc_trace_dev)
		return -ENODEV;

	if (dvc_trace_lock(&_dvc_trace_dev->open_excl))
		return -EBUSY;

	fp->private_data = _dvc_trace_dev;

	/* clear the error latch */
	_dvc_trace_dev->error = 0;
	_dvc_trace_dev->transfering = 0;

	return 0;
}

static int dvc_trace_release(struct inode *ip, struct file *fp)
{
	pr_debug("%s\n", __func__);

	dvc_trace_unlock(&_dvc_trace_dev->open_excl);
	return 0;
}

/* file operations for DvC.Trace device /dev/usb_dvc_trace */
static const struct file_operations dvc_trace_fops = {
	.owner = THIS_MODULE,
	.open = dvc_trace_open,
	.release = dvc_trace_release,
};

static struct miscdevice dvc_trace_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "usb_dvc_trace",
	.fops = &dvc_trace_fops,
};

static int dvc_trace_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{

	struct dvc_trace_dev *dev = _dvc_trace_dev;
	int	value = -EOPNOTSUPP;
	int	ret;
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

		dvc_trace_disable_transfer();
		value = 0;
		break;

	/* DC_REQUEST_SET_TRACE ... start trace transfer */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
		| DC_REQUEST_SET_TRACE:

		pr_info("%s DC_REQUEST_SET_TRACE v%04x i%04x l%u\n", __func__,
			 w_value, w_index, w_length);

		if (!w_index)
			ret = dvc_trace_disable_transfer();
		else
			ret = dvc_trace_start_transfer(4096);

		if (ret < 0)
			value = -EINVAL;
		else
			value = (int) w_index;
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
dvc_trace_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev	*cdev = c->cdev;
	struct dvc_trace_dev		*dev = func_to_dvc_trace(f);
	int			id;
	int			ret;
	int status;

	dev->cdev = cdev;
	pr_debug("%s dev: %p\n", __func__, dev);

	/* maybe allocate device-global string IDs, and patch descriptors */
	if (trace_string_defs[DVCTRACE_CTRL_IDX].id == 0) {
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_DATA_IDX].id = status;
		trace_data_interface_desc.iInterface = status;

		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_CTRL_IDX].id = status;
		trace_interface_desc.iInterface = status;

		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_IAD_IDX].id = status;
		trace_iad_desc.iFunction = status;

		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_CONN_PTI_IDX].id = status;
		trace_output_conn_pti_desc.iConnection = status;

		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_CONN_USB_IDX].id = status;
		trace_output_conn_usb_desc.iConnection = status;

		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		trace_string_defs[DVCTRACE_UNIT_STM_IDX].id = status;
		trace_debug_unit_stm_desc.iDebugUnitType = status;
	}

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->data_id = id;
	trace_data_interface_desc.bInterfaceNumber = id;
	trace_iad_desc.bFirstInterface = id;

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	dev->ctrl_id = id;
	trace_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = trace_create_bulk_endpoints(dev, &trace_fullspeed_in_desc,
					  &trace_superspeed_in_comp_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		trace_highspeed_in_desc.bEndpointAddress =
			trace_fullspeed_in_desc.bEndpointAddress;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		trace_superspeed_in_desc.bEndpointAddress =
			trace_fullspeed_in_desc.bEndpointAddress;
	}

	pr_debug("%s speed %s: IN/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name);
	return 0;
}

static void
dvc_trace_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct dvc_trace_dev	*dev = func_to_dvc_trace(f);
	struct usb_request *req;

	dev->online = 0;
	dev->online_data = 0;
	dev->online_ctrl = 0;
	dev->error = 0;
	trace_string_defs[DVCTRACE_CTRL_IDX].id = 0;

	wake_up(&dev->write_wq);

	while ((req = dvc_trace_req_get(dev, &dev->tx_idle)))
		dvc_trace_request_free(req, dev->ep_in);
}

static int dvc_trace_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct dvc_trace_dev	*dev = func_to_dvc_trace(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	pr_debug("%s intf: %d alt: %d\n", __func__, intf, alt);

	if (intf == trace_interface_desc.bInterfaceNumber)
		dev->online_ctrl = 1;

	if (intf == trace_data_interface_desc.bInterfaceNumber) {
		ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_by_speed in err %d\n",
				__func__, intf, alt, ret);
			return ret;
		}

		ret = usb_ep_enable(dev->ep_in);
		if (ret) {
			pr_err("%s intf: %d alt: %d ep_enable in err %d\n",
				__func__, intf, alt, ret);
			return ret;
		}
		dev->online_data = 1;
	}

	if (dev->online_data && dev->online_ctrl) {
		dev->online = 1;
		dev->transfering = 0;
	}

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->write_wq);
	return 0;
}

static void dvc_trace_function_disable(struct usb_function *f)
{
	struct dvc_trace_dev	*dev = func_to_dvc_trace(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	pr_debug("%s dev %p\n", __func__, cdev);

	if (dev->transfering)
		dvc_trace_disable_transfer();

	dev->online = 0;
	dev->online_data = 0;
	dev->online_ctrl = 0;
	dev->error = 0;
	usb_ep_disable(dev->ep_in);

	/* writer may be blocked waiting for us to go online */
	wake_up(&dev->write_wq);

	pr_debug("%s : %s disabled\n", __func__, dev->function.name);
}

static int dvc_trace_bind_config(struct usb_configuration *c)
{
	struct dvc_trace_dev *dev = _dvc_trace_dev;

	pr_debug("%s\n", __func__);

	dev->cdev = c->cdev;
	dev->function.name = "dvctrace";
	dev->function.strings = trace_strings;
	dev->function.fs_descriptors = fs_trace_descs;
	dev->function.hs_descriptors = hs_trace_descs;
	dev->function.ss_descriptors = ss_trace_descs;
	dev->function.bind = dvc_trace_function_bind;
	dev->function.unbind = dvc_trace_function_unbind;
	dev->function.set_alt = dvc_trace_function_set_alt;
	dev->function.disable = dvc_trace_function_disable;

	return usb_add_function(c, &dev->function);
}

static int dvc_trace_setup(void)
{
	struct dvc_trace_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->tx_xfer);

	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->write_excl, 0);

	_dvc_trace_dev = dev;

	ret = misc_register(&dvc_trace_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	pr_err("DvC.Trace gadget driver failed to initialize\n");
	return ret;
}

static void dvc_trace_cleanup(void)
{
	misc_deregister(&dvc_trace_device);

	kfree(_dvc_trace_dev);
	_dvc_trace_dev = NULL;
}
