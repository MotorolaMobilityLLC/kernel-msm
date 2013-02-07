/*
 * Gadget Driver for Android LAF
 *
 * Copyright (C) 2012,2013 LGE Inc. All rights reserved
 * Author: DH, kang <deunghyung.kang@lge.com>
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

#undef LAF_DEBUG

#ifdef LAF_DEBUG
#undef DBG
#undef ERROR

#define lafprintk(d, level, fmt, args...) \
	printk(level "%s: " fmt , (d)->gadget->name , ## args)


#define DBG(dev, fmt, args...) \
	lafprintk(dev , KERN_ERR , fmt , ## args)
#define ERROR(dev, fmt, args...) \
	lafprintk(dev , KERN_ERR , fmt , ## args)
#else

#if 1
#undef  DBG
#undef  ERROR
#undef 	pr_info
#undef 	pr_err
#undef 	pr_debug

#define pr_info(fmt, args...) do { } while (0)
#define pr_err(fmt, args...) do { } while (0)
#define pr_debug(fmt, args...) do { } while (0)
#define DBG(dev, fmt, args...) do { } while (0)
#define ERROR(dev, fmt, args...) do { } while (0)
#endif
#endif

#define LAF_BULK_BUFFER_SIZE           (0x4000)

/* number of tx requests to allocate */
#define TX_REQ_MAX 4
#define LAF_RX_REQ_MAX 4


static const char laf_shortname[] = "laf";

struct laf_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	atomic_t online;
	atomic_t error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req[LAF_RX_REQ_MAX];
	int rx_done;
	bool notify_close;
	bool close_notified;
};

static struct usb_interface_descriptor laf_interface_desc = {
	.bLength                = sizeof laf_interface_desc,
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bInterfaceNumber       = 2,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	0xFF,
	.bInterfaceSubClass =	0xFF,
	.bInterfaceProtocol =	0xFF,
};

static struct usb_endpoint_descriptor laf_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor laf_superspeed_in_comp_desc = {
	.bLength =		sizeof laf_superspeed_in_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor laf_superspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor laf_superspeed_out_comp_desc = {
	.bLength =		sizeof laf_superspeed_out_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};


static struct usb_endpoint_descriptor laf_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
	.bInterval 			=	0,
};

static struct usb_endpoint_descriptor laf_highspeed_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(512),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor laf_fullspeed_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor laf_fullspeed_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_descriptor_header *fs_laf_descs[] = {
	(struct usb_descriptor_header *) &laf_interface_desc,
	(struct usb_descriptor_header *) &laf_fullspeed_in_desc,
	(struct usb_descriptor_header *) &laf_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_laf_descs[] = {
	(struct usb_descriptor_header *) &laf_interface_desc,
	(struct usb_descriptor_header *) &laf_highspeed_in_desc,
	(struct usb_descriptor_header *) &laf_highspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_laf_descs[] = {
	(struct usb_descriptor_header *) &laf_interface_desc,
	(struct usb_descriptor_header *) &laf_superspeed_in_desc,
	(struct usb_descriptor_header *) &laf_superspeed_in_comp_desc,
	(struct usb_descriptor_header *) &laf_superspeed_out_desc,
	(struct usb_descriptor_header *) &laf_superspeed_out_comp_desc,
	NULL,
};

static void laf_ready_callback(void);
static void laf_closed_callback(void);

/* temporary variable used between laf_open() and laf_gadget_bind() */
static struct laf_dev *_laf_dev;

static inline struct laf_dev *func_to_laf(struct usb_function *f)
{
	return container_of(f, struct laf_dev, function);
}


static struct usb_request *laf_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void laf_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int laf_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void laf_unlock(atomic_t *excl)
{
	if (atomic_dec_return(excl) < 0)
		atomic_inc(excl);
}

/* add a request to the tail of a list */
void laf_req_put(struct laf_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *laf_req_get(struct laf_dev *dev, struct list_head *head)
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

static void laf_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct laf_dev *dev = _laf_dev;

	if (req->status != 0)
		atomic_set(&dev->error, 1);

	laf_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void laf_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct laf_dev *dev = _laf_dev;
	pr_info("%s\n", __func__);

	dev->rx_done = 1;
	if (unlikely(req->status != 0 && req->status != -ECONNRESET))
		atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);
}

static int laf_create_bulk_endpoints(struct laf_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for laf ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < LAF_RX_REQ_MAX; i++) {
		req = laf_request_new(dev->ep_out, LAF_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = laf_complete_out;
		dev->rx_req[i] = req;
	}

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = laf_request_new(dev->ep_in, LAF_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = laf_complete_in;
		laf_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "laf_bind() could not allocate requests\n");
	return -1;
}

static ssize_t laf_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct laf_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	if (unlikely(!_laf_dev))
		return -ENODEV;

	if (unlikely(count > LAF_BULK_BUFFER_SIZE))
		return -EINVAL;

	if (unlikely(laf_lock(&dev->read_excl)))
		return -EBUSY;

	/* we will block until we're online */
	while (unlikely(!(atomic_read(&dev->online)) ||
				unlikely(atomic_read(&dev->error)))) {
		pr_debug("laf_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
				(atomic_read(&dev->online) ||
				 atomic_read(&dev->error)));
		if (unlikely(ret < 0)) {
			laf_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (unlikely(atomic_read(&dev->error))) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req[0];
	req->length = count;
	dev->rx_done = 0;

	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (unlikely(ret < 0)) {
		pr_debug("laf_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		atomic_set(&dev->error, 1);
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (unlikely(ret < 0)) {
		if (ret != -ERESTARTSYS)
			atomic_set(&dev->error, 1);
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (likely(!atomic_read(&dev->error))) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (unlikely(req->actual == 0))
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (unlikely(copy_to_user(buf, req->buf, xfer)))
			r = -EFAULT;
	} else {
		r = -EIO;
	}

done:
	laf_unlock(&dev->read_excl);
	pr_debug("laf_read returning %d\n", r);
	return r;
}

static ssize_t laf_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct laf_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	if (!_laf_dev)
		return -ENODEV;

	if (laf_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (atomic_read(&dev->error)) {
			pr_debug("laf_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = laf_req_get(dev, &dev->tx_idle)) ||
			 atomic_read(&dev->error)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > LAF_BULK_BUFFER_SIZE)
				xfer = LAF_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("laf_write: xfer error %d\n", ret);
				atomic_set(&dev->error, 1);
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		laf_req_put(dev, &dev->tx_idle, req);

	laf_unlock(&dev->write_excl);
	pr_debug("laf_write returning %d\n", r);
	return r;
}

static int laf_open(struct inode *ip, struct file *fp)
{
	if (!_laf_dev)
		return -ENODEV;

	if (laf_lock(&_laf_dev->open_excl))
		return -EBUSY;

	fp->private_data = _laf_dev;

	/* clear the error latch */
	atomic_set(&_laf_dev->error, 0);

	if (_laf_dev->close_notified) {
		_laf_dev->close_notified = false;
	laf_ready_callback();
	}

	_laf_dev->notify_close = true;

	return 0;
}

static int laf_release(struct inode *ip, struct file *fp)
{
	pr_info("laf_release\n");

	/*
	 * LAF daemon closes the device file after I/O error.  The
	 * I/O error happen when Rx requests are flushed during
	 * cable disconnect or bus reset in configured state.  Disabling
	 * USB configuration and pull-up during these scenarios are
	 * undesired.  We want to force bus reset only for certain
	 */
	if (_laf_dev->notify_close) {
		laf_closed_callback();
		_laf_dev->close_notified = true;
	}

	laf_unlock(&_laf_dev->open_excl);
	return 0;
}

/* file operations for LAF device /dev/android_laf */
static const struct file_operations laf_fops = {
	.owner = THIS_MODULE,
	.read = laf_read,
	.write = laf_write,
	.open = laf_open,
	.release = laf_release,
};

static struct miscdevice laf_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = laf_shortname,
	.fops = &laf_fops,
};

static int laf_function_bind(struct usb_configuration *c,
		struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct laf_dev	*dev = func_to_laf(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "laf_function_bind dev: %p\n", dev);
	printk(KERN_INFO "laf_function_bind .........\n");

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	laf_interface_desc.bInterfaceNumber = id;

	DBG(cdev, "laf_function_bind bInterfaceNumber: %d\n", id);

	/* allocate endpoints */
	ret = laf_create_bulk_endpoints(dev, &laf_fullspeed_in_desc,
			&laf_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		laf_highspeed_in_desc.bEndpointAddress =
			laf_fullspeed_in_desc.bEndpointAddress;
		laf_highspeed_out_desc.bEndpointAddress =
			laf_fullspeed_out_desc.bEndpointAddress;
	}
	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		laf_superspeed_in_desc.bEndpointAddress =
			laf_fullspeed_in_desc.bEndpointAddress;
		laf_superspeed_out_desc.bEndpointAddress =
			laf_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void laf_function_unbind(struct usb_configuration *c,
		struct usb_function *f)
{
	struct laf_dev	*dev = func_to_laf(f);
	struct usb_request *req;
	int i;

	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);

	for (i = 0; i < LAF_RX_REQ_MAX; i++)
		laf_request_free(dev->rx_req[i], dev->ep_out);
	while ((req = laf_req_get(dev, &dev->tx_idle)))
		laf_request_free(req, dev->ep_in);
}

static int laf_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct laf_dev	*dev = func_to_laf(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "laf_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret) {
		dev->ep_in->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
				dev->ep_in->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_in);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
			dev->ep_in->name, ret);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret) {
		dev->ep_out->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
			dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
				dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	atomic_set(&dev->online, 1);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void laf_function_disable(struct usb_function *f)
{
	struct laf_dev	*dev = func_to_laf(f);

	/*
	 * Bus reset happened or cable disconnected.  No
	 * need to disable the configuration now.  We will
	 * set noify_close to true when device file is re-opened.
	 */
	dev->notify_close = false;
	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
}

static int laf_bind_config(struct usb_configuration *c)
{
	struct laf_dev *dev = _laf_dev;

	dev->cdev = c->cdev;
	dev->function.name = "laf";
	dev->function.descriptors = fs_laf_descs;
	dev->function.hs_descriptors = hs_laf_descs;
	if (gadget_is_superspeed(c->cdev->gadget))
		dev->function.ss_descriptors = ss_laf_descs;
	dev->function.bind = laf_function_bind;
	dev->function.unbind = laf_function_unbind;
	dev->function.set_alt = laf_function_set_alt;
	dev->function.disable = laf_function_disable;

	return usb_add_function(c, &dev->function);
}

static int laf_setup(void)
{
	struct laf_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	/* config is disabled by default if laf is present. */
	dev->close_notified = true;

	INIT_LIST_HEAD(&dev->tx_idle);

	_laf_dev = dev;

	ret = misc_register(&laf_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	pr_info("laf gadget driver failed to initialize\n");
	return ret;
}

static void laf_cleanup(void)
{
	misc_deregister(&laf_device);

	kfree(_laf_dev);
	_laf_dev = NULL;
}

#ifndef LAF_DEBUG
#undef 	pr_info
#undef 	pr_err
#undef 	pr_debug
#undef  DBG

#define pr_info(fmt, args...) printk(KERN_ERR fmt, ## args)
#define pr_err(fmt, args...) printk(KERN_ERR fmt, ## args)
#define pr_debug(fmt, args...) printk(KERN_ERR fmt, ## args)
#define DBG(x...) pr_debug(x)

#endif
