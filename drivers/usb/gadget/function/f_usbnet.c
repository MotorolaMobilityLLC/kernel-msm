/*
 * Gadget Driver for Motorola USBNet
 *
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <linux/skbuff.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <linux/usb/ch9.h>
#include <asm/cacheflush.h>
#include <linux/switch.h>


/*
 * Macro Defines
 */

#define EP0_BUFSIZE		256


/* Vendor Request to config IP */
#define USBNET_SET_IP_ADDRESS   0x05
#define USBNET_SET_SUBNET_MASK  0x06
#define USBNET_SET_HOST_IP      0x07

/* Linux Network Interface */
#define USB_MTU                 1536
#define MAX_BULK_TX_REQ_NUM	8
#define MAX_BULK_RX_REQ_NUM	8
#define MAX_INTR_RX_REQ_NUM	8
#define STRING_INTERFACE        0

struct usbnet_context {
	spinlock_t lock;  /* For RX/TX list */
	struct net_device *dev;

	struct usb_gadget *gadget;

	struct usb_ep *bulk_in;
	struct usb_ep *bulk_out;
	struct usb_ep *intr_out;
	u16 config;		/* current USB config w_value */

	struct list_head rx_reqs;
	struct list_head tx_reqs;

	struct net_device_stats stats;
	struct work_struct usbnet_config_wq;
	u32 ip_addr;
	u32 subnet_mask;
	u32 router_ip;
	u32 iff_flag;
};


struct usbnet_device {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	struct usbnet_context *net_ctxt;
};

/* static strings, in UTF-8 */
static struct usb_string usbnet_string_defs[] = {
	[STRING_INTERFACE].s = "Motorola Test Command",
	{  /* ZEROES END LIST */ },
};

static struct usb_gadget_strings usbnet_string_table = {
	.language =             0x0409, /* en-us */
	.strings =              usbnet_string_defs,
};

static struct usb_gadget_strings *usbnet_strings[] = {
	&usbnet_string_table,
	NULL,
};



/* There is only one interface. */

static struct usb_interface_descriptor usbnet_intf_desc = {
	.bLength = sizeof(usbnet_intf_desc),
	.bDescriptorType = USB_DT_INTERFACE,
	.bNumEndpoints = 3,
	.bInterfaceClass = 0x02,
	.bInterfaceSubClass = 0x0a,
	.bInterfaceProtocol = 0x01,
};


static struct usb_endpoint_descriptor usbnet_fs_bulk_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor usbnet_fs_bulk_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor intr_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize = __constant_cpu_to_le16(64),
	.bInterval = 1,
};

static struct usb_descriptor_header *fs_function[] = {
	(struct usb_descriptor_header *) &usbnet_intf_desc,
	(struct usb_descriptor_header *) &usbnet_fs_bulk_in_desc,
	(struct usb_descriptor_header *) &usbnet_fs_bulk_out_desc,
	(struct usb_descriptor_header *) &intr_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor usbnet_hs_bulk_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
	.bInterval = 0,
};

static struct usb_endpoint_descriptor usbnet_hs_bulk_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
	.bInterval = 0,
};

static struct usb_descriptor_header *hs_function[] = {
	(struct usb_descriptor_header *) &usbnet_intf_desc,
	(struct usb_descriptor_header *) &usbnet_hs_bulk_in_desc,
	(struct usb_descriptor_header *) &usbnet_hs_bulk_out_desc,
	(struct usb_descriptor_header *) &intr_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor usbnet_ss_bulk_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(1024),
	.bInterval = 0,
};

static struct usb_ss_ep_comp_descriptor usbnet_ss_bulk_in_comp_desc = {
	.bLength =              sizeof usbnet_ss_bulk_in_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_endpoint_descriptor usbnet_ss_bulk_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(1024),
	.bInterval = 0,
};

static struct usb_ss_ep_comp_descriptor usbnet_ss_bulk_out_comp_desc = {
	.bLength =              sizeof usbnet_ss_bulk_out_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_ss_ep_comp_descriptor usbnet_ss_intr_comp_desc = {
	.bLength =              sizeof usbnet_ss_intr_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
	.wBytesPerInterval =    cpu_to_le16(64),
};

static struct usb_descriptor_header *ss_function[] = {
	(struct usb_descriptor_header *) &usbnet_intf_desc,
	(struct usb_descriptor_header *) &usbnet_ss_bulk_in_desc,
	(struct usb_descriptor_header *) &usbnet_ss_bulk_in_comp_desc,
	(struct usb_descriptor_header *) &usbnet_ss_bulk_out_desc,
	(struct usb_descriptor_header *) &usbnet_ss_bulk_out_comp_desc,
	(struct usb_descriptor_header *) &intr_out_desc,
	(struct usb_descriptor_header *) &usbnet_ss_intr_comp_desc,
	NULL,
};

#ifdef CONFIG_SWITCH
static struct switch_dev usbnet_enable_device = {
	.name = "usbnet_enable",
};
#endif

#define DO_NOT_STOP_QUEUE 0
#define STOP_QUEUE 1

#define USBNETDBG(context, fmt, args...) {\
	if (context && (context->gadget))		\
		dev_dbg(&(context->gadget->dev) , fmt , ## args); }

static const char *usb_description = "Motorola BLAN Interface";

static ssize_t usbnet_desc_show(struct device *dev,
				 struct device_attribute *attr, char *buff)
{
	ssize_t status = 0;
	status = snprintf(buff, 30, "%s\n", usb_description);
	return status;
}

static DEVICE_ATTR(description, S_IRUGO, usbnet_desc_show, NULL);

static inline struct usbnet_device *usbnet_func_to_dev(struct usb_function *f)
{
	return container_of(f, struct usbnet_device, function);
}


static int ether_queue_out(struct usb_request *req ,
				struct usbnet_context *context)
{
	unsigned long flags;
	struct sk_buff *skb;
	int ret;

	skb = alloc_skb(USB_MTU + NET_IP_ALIGN, GFP_ATOMIC);
	if (!skb) {
		USBNETDBG(context, "%s: failed to alloc skb\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	req->buf = skb->data;
	req->length = ALIGN(USB_MTU, context->bulk_out->maxpacket);
	req->context = skb;

	ret = usb_ep_queue(context->bulk_out, req, GFP_KERNEL);
	if (ret == 0)
		return 0;
	else
		kfree_skb(skb);
fail:
	spin_lock_irqsave(&context->lock, flags);
	list_add_tail(&req->list, &context->rx_reqs);
	spin_unlock_irqrestore(&context->lock, flags);

	return ret;
}

struct usb_request *usb_get_recv_request(struct usbnet_context *context)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&context->lock, flags);
	if (list_empty(&context->rx_reqs)) {
		req = NULL;
	} else {
		req = list_first_entry(&context->rx_reqs,
				       struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&context->lock, flags);

	return req;
}

struct usb_request *usb_get_xmit_request(int stop_flag, struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&context->lock, flags);
	if (list_empty(&context->tx_reqs)) {
		req = NULL;
	} else {
		req = list_first_entry(&context->tx_reqs,
				       struct usb_request, list);
		list_del(&req->list);
		if (stop_flag == STOP_QUEUE &&
			list_empty(&context->tx_reqs))
			netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&context->lock, flags);
	return req;
}

static int usb_ether_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	struct usb_request *req;
	unsigned long flags;
	unsigned len;
	unsigned pad;
	int rc;

	req = usb_get_xmit_request(STOP_QUEUE, dev);

	if (!req) {
		USBNETDBG(context, "%s: could not obtain tx request\n",
			__func__);
		return 1;
	}

	/* Add 4 bytes CRC */
	pad = 4;

	/* ensure that we end with a short packet */
	len = skb->len + pad;
	if (!(len & 63) || !(len & 511))
		pad++;

	/* ensure the added bytes are 0'd out */
	if (skb_tailroom(skb) < pad) {
		USBNETDBG(context, "%s: could not add CRC\n", __func__);
		return 1;
	}
	memset(skb_put(skb, pad), 0, pad);

	req->context = skb;
	req->buf = skb->data;
	req->length = skb->len;

	rc = usb_ep_queue(context->bulk_in, req, GFP_KERNEL);
	if (rc != 0) {
		spin_lock_irqsave(&context->lock, flags);
		list_add_tail(&req->list, &context->tx_reqs);
		spin_unlock_irqrestore(&context->lock, flags);

		dev_kfree_skb_any(skb);
		context->stats.tx_dropped++;

		USBNETDBG(context,
			  "%s: could not queue tx request\n", __func__);
	}

	return 0;
}

static int usb_ether_open(struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	USBNETDBG(context, "%s\n", __func__);
	return 0;
}

static int usb_ether_stop(struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	USBNETDBG(context, "%s\n", __func__);
	return 0;
}

static struct net_device_stats *usb_ether_get_stats(struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	USBNETDBG(context, "%s\n", __func__);
	return &context->stats;
}

static void usbnet_if_config(struct work_struct *work)
{
	struct ifreq ifr;
	mm_segment_t saved_fs;
	unsigned err;
	struct sockaddr_in *sin;
	struct usbnet_context *context = container_of(work,
				 struct usbnet_context, usbnet_config_wq);

	pr_info("%s : Configuring with config = %d, ip_addr = 0x%08x",
		__func__, context->config, context->ip_addr);
	pr_info("subnet = 0x%08x, router_ip = 0x%08x, flags = 0x%08x\n",
		context->subnet_mask, context->router_ip, context->iff_flag);
	memset(&ifr, 0, sizeof(ifr));
	sin = (void *) &(ifr.ifr_ifru.ifru_addr);
	strlcpy(ifr.ifr_ifrn.ifrn_name, context->dev->name,
		sizeof(ifr.ifr_ifrn.ifrn_name));
	sin->sin_family = AF_INET;

	sin->sin_addr.s_addr = context->ip_addr;
	saved_fs = get_fs();
	set_fs(get_ds());
	err = devinet_ioctl(dev_net(context->dev), SIOCSIFADDR, &ifr);
	if (err)
		USBNETDBG(context, "%s: Error in SIOCSIFADDR\n", __func__);

	sin->sin_addr.s_addr = context->subnet_mask;
	err = devinet_ioctl(dev_net(context->dev), SIOCSIFNETMASK, &ifr);
	if (err)
		USBNETDBG(context, "%s: Error in SIOCSIFNETMASK\n", __func__);

	sin->sin_addr.s_addr = context->ip_addr | ~(context->subnet_mask);
	err = devinet_ioctl(dev_net(context->dev), SIOCSIFBRDADDR, &ifr);
	if (err)
		USBNETDBG(context, "%s: Error in SIOCSIFBRDADDR\n", __func__);

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_ifrn.ifrn_name, context->dev->name,
		sizeof(ifr.ifr_ifrn.ifrn_name));
	ifr.ifr_flags = context->dev->flags & ~IFF_UP;
	ifr.ifr_flags |= context->iff_flag;
	err = devinet_ioctl(dev_net(context->dev), SIOCSIFFLAGS, &ifr);
	if (err)
		USBNETDBG(context, "%s: Error in SIOCSIFFLAGS\n", __func__);

	set_fs(saved_fs);
#ifdef CONFIG_SWITCH
	switch_set_state(&usbnet_enable_device, context->config);
#endif
}

static const struct net_device_ops usbnet_eth_netdev_ops = {
	.ndo_open		= usb_ether_open,
	.ndo_stop		= usb_ether_stop,
	.ndo_start_xmit		= usb_ether_xmit,
	.ndo_get_stats		= usb_ether_get_stats,
};

static void usb_ether_setup(struct net_device *dev)
{
	struct usbnet_context *context = netdev_priv(dev);
	INIT_LIST_HEAD(&context->rx_reqs);
	INIT_LIST_HEAD(&context->tx_reqs);

	spin_lock_init(&context->lock);
	context->dev = dev;

	dev->netdev_ops = &usbnet_eth_netdev_ops;
	dev->watchdog_timeo = 20;

	ether_setup(dev);

	random_ether_addr(dev->dev_addr);
}

/*-------------------------------------------------------------------------*/
static void usbnet_cleanup(struct usbnet_device *dev)
{
	struct usbnet_context *context = dev->net_ctxt;
	if (context) {
		device_remove_file(&(context->dev->dev), &dev_attr_description);
		unregister_netdev(context->dev);
		free_netdev(context->dev);
		dev->net_ctxt = NULL;
	}
}

static void usbnet_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct usbnet_device *dev = usbnet_func_to_dev(f);
	struct usb_composite_dev *cdev = c->cdev;
	struct usbnet_context *context = dev->net_ctxt;
	struct usb_request *req;

	dev->cdev = cdev;

	if (context->bulk_in)
		usb_ep_disable(context->bulk_in);

	if (context->bulk_out) {
		usb_ep_disable(context->bulk_out);

		/* Free BULK OUT Requests */
		while ((req = usb_get_recv_request(context)))
			usb_ep_free_request(context->bulk_out, req);
	}

	if (context->bulk_in) {
		/* Free BULK IN Requests */
		while ((req = usb_get_xmit_request(DO_NOT_STOP_QUEUE,
						  context->dev))) {
			usb_ep_free_request(context->bulk_in, req);
		}
	}

	context->bulk_in = NULL;
	context->bulk_out = NULL;
	context->config = 0;
}

static void ether_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff *skb = req->context;
	struct usbnet_context *context = ep->driver_data;

	if (req->status == 0) {
		skb_put(skb, req->actual);
		skb->protocol = eth_type_trans(skb, context->dev);
		context->stats.rx_packets++;
		context->stats.rx_bytes += req->actual;
		netif_rx(skb);
	} else {
		dev_kfree_skb_any(skb);
		context->stats.rx_errors++;
	}

	/* don't bother requeuing if we just went offline */
	if ((req->status == -ENODEV) || (req->status == -ESHUTDOWN)) {
		unsigned long flags;
		spin_lock_irqsave(&context->lock, flags);
		list_add_tail(&req->list, &context->rx_reqs);
		spin_unlock_irqrestore(&context->lock, flags);
	} else {
		if (ether_queue_out(req, context))
			USBNETDBG(context, "ether_out: cannot requeue\n");
	}
}

static void ether_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	unsigned long flags;
	struct sk_buff *skb = req->context;
	struct usbnet_context *context = ep->driver_data;

	if (req->status == 0) {
		context->stats.tx_packets++;
		context->stats.tx_bytes += req->actual;
	} else {
		context->stats.tx_errors++;
	}

	dev_kfree_skb_any(skb);

	spin_lock_irqsave(&context->lock, flags);
	if (list_empty(&context->tx_reqs))
		netif_start_queue(context->dev);

	list_add_tail(&req->list, &context->tx_reqs);
	spin_unlock_irqrestore(&context->lock, flags);
}

static int usbnet_bind(struct usb_configuration *c,
			struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	int n, rc, id;
	struct usb_ep *ep;
	struct usb_request *req;
	unsigned long flags;

	dev->cdev = cdev;

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	usbnet_intf_desc.bInterfaceNumber = id;
	context->gadget = cdev->gadget;

	/* Find all the endpoints we will use */
	ep = usb_ep_autoconfig(cdev->gadget, &usbnet_fs_bulk_in_desc);
	if (!ep) {
		USBNETDBG(context, "%s usbnet_hs_bulk_in_desc error\n",
			__func__);
		goto autoconf_fail;
	}
	ep->driver_data = context;
	context->bulk_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &usbnet_fs_bulk_out_desc);
	if (!ep) {
		USBNETDBG(context, "%s usbnet_hs_bulk_out_desc error\n",
			__func__);
		goto autoconf_fail;
	}
	ep->driver_data = context;
	context->bulk_out = ep;


	ep = usb_ep_autoconfig(cdev->gadget, &intr_out_desc);
	if (!ep) {
		USBNETDBG(context, "%s intr_out_desc error\n",
		      __func__);
		goto autoconf_fail;
	}
	ep->driver_data = context;
	context->intr_out = ep;

	if (gadget_is_dualspeed(cdev->gadget)) {

		/* Assume endpoint addresses are the same for both speeds */
		usbnet_hs_bulk_in_desc.bEndpointAddress =
		    usbnet_fs_bulk_in_desc.bEndpointAddress;
		usbnet_hs_bulk_out_desc.bEndpointAddress =
		    usbnet_fs_bulk_out_desc.bEndpointAddress;
	}

	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		usbnet_ss_bulk_in_desc.bEndpointAddress =
		    usbnet_fs_bulk_in_desc.bEndpointAddress;
		usbnet_ss_bulk_out_desc.bEndpointAddress =
		    usbnet_fs_bulk_out_desc.bEndpointAddress;
	}


	rc = -ENOMEM;

	for (n = 0; n < MAX_BULK_RX_REQ_NUM; n++) {
		req = usb_ep_alloc_request(context->bulk_out,
					 GFP_KERNEL);
		if (!req) {
			USBNETDBG(context, "%s: alloc request bulk_out fail\n",
				__func__);
			break;
		}
		req->complete = ether_out_complete;
		spin_lock_irqsave(&context->lock, flags);
		list_add_tail(&req->list, &context->rx_reqs);
		spin_unlock_irqrestore(&context->lock, flags);
	}
	for (n = 0; n < MAX_BULK_TX_REQ_NUM; n++) {
		req = usb_ep_alloc_request(context->bulk_in,
					 GFP_KERNEL);
		if (!req) {
			USBNETDBG(context, "%s: alloc request bulk_in fail\n",
				__func__);
			break;
		}
		req->complete = ether_in_complete;
		spin_lock_irqsave(&context->lock, flags);
		list_add_tail(&req->list, &context->tx_reqs);
		spin_unlock_irqrestore(&context->lock, flags);
	}

	return 0;

autoconf_fail:
	rc = -ENOTSUPP;
	usbnet_unbind(c, f);
	return rc;
}




static void do_set_config(struct usb_function *f, u16 new_config)
{
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	struct usb_composite_dev *cdev = f->config->cdev;
	int result = 0;
	struct usb_request *req;

	if (context->config == new_config) /* Config did not change */
		return;

	context->config = new_config;

	if (new_config == 1) { /* Enable End points */
		result = config_ep_by_speed(cdev->gadget, f, context->bulk_in);
		if (result) {
			context->bulk_in->desc = NULL;
			USBNETDBG(context,
				"config_ep_by_speed failed: ep %s, result %d\n",
				context->bulk_in->name, result);
			return;
		}

		result = usb_ep_enable(context->bulk_in);

		if (result != 0) {
			USBNETDBG(context,
				  "%s:  failed to enable BULK_IN EP ret=%d\n",
				  __func__, result);
		}

		context->bulk_in->driver_data = context;

		result = config_ep_by_speed(cdev->gadget, f, context->bulk_out);
		if (result) {
			context->bulk_out->desc = NULL;
			USBNETDBG(context,
				"config_ep_by_speed failed: ep %s, result %d\n",
				context->bulk_out->name, result);
			usb_ep_disable(context->bulk_in);
			return;
		}

		result = usb_ep_enable(context->bulk_out);

		if (result != 0) {
			USBNETDBG(context,
				  "%s: failed to enable BULK_OUT EP ret = %d\n",
				  __func__, result);
		}

		context->bulk_out->driver_data = context;

		context->intr_out->desc = &intr_out_desc;
		result = usb_ep_enable(context->intr_out);

		if (result != 0) {
			USBNETDBG(context,
				"%s: failed to enable INTR_OUT EP ret = %d\n",
				__func__, result);
		}

		context->intr_out->driver_data = context;

		/* we're online -- get all rx requests queued */
		while ((req = usb_get_recv_request(context))) {
			if (ether_queue_out(req, context)) {
				USBNETDBG(context,
					  "%s: ether_queue_out failed\n",
					  __func__);
				break;
			}
		}

	} else {/* Disable Endpoints */
		if (context->bulk_in)
			usb_ep_disable(context->bulk_in);
		if (context->bulk_out)
			usb_ep_disable(context->bulk_out);
		if (context->intr_out)
			usb_ep_disable(context->intr_out);
		context->ip_addr = 0;
		context->subnet_mask = 0;
		context->router_ip = 0;
		context->iff_flag = 0;
		schedule_work(&context->usbnet_config_wq);
	}
}


static int usbnet_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	USBNETDBG(context, "usbnet_set_alt intf: %d alt: %d\n", intf, alt);
	do_set_config(f, 1);
	return 0;
}

static int usbnet_ctrlrequest(struct usbnet_device *dev,
			struct usb_composite_dev *cdev,
			const struct usb_ctrlrequest *ctrl)
{
	struct usbnet_context *context = dev->net_ctxt;
	int rc = -EOPNOTSUPP;
	int wIndex = le16_to_cpu(ctrl->wIndex);
	int wValue = le16_to_cpu(ctrl->wValue);
	int wLength = le16_to_cpu(ctrl->wLength);
	struct usb_request      *req = cdev->req;

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		switch (ctrl->bRequest) {
		case USBNET_SET_IP_ADDRESS:
			context->ip_addr = (wValue << 16) | wIndex;
			rc = 0;
			break;
		case USBNET_SET_SUBNET_MASK:
			context->subnet_mask = (wValue << 16) | wIndex;
			rc = 0;
			break;
		case USBNET_SET_HOST_IP:
			context->router_ip = (wValue << 16) | wIndex;
			rc = 0;
			break;
		default:
			break;
		}

		if (context->ip_addr && context->subnet_mask
		    && context->router_ip) {
			context->iff_flag = IFF_UP;
			/* schedule a work queue to do this because we
				 need to be able to sleep */
			schedule_work(&context->usbnet_config_wq);
		}
	}

	/* respond with data transfer or status phase? */
	if (rc >= 0) {
		req->zero = rc < wLength;
		req->length = rc;
		rc = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (rc < 0)
			USBNETDBG(context, "usbnet setup response error\n");
	}

	return rc;
}

static void usbnet_disable(struct usb_function *f)
{
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	USBNETDBG(context, "%s\n", __func__);
	do_set_config(f, 0);
}

static void usbnet_suspend(struct usb_function *f)
{
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	USBNETDBG(context, "%s\n", __func__);
}

static void usbnet_resume(struct usb_function *f)
{
	struct usbnet_device  *dev = usbnet_func_to_dev(f);
	struct usbnet_context *context = dev->net_ctxt;
	USBNETDBG(context, "%s\n", __func__);
}

int usbnet_bind_config(struct usbnet_device *dev, struct usb_configuration *c)
{
	int ret, status;

	pr_debug("usbnet_bind_config\n");

	if (usbnet_string_defs[STRING_INTERFACE].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0) {
			pr_err("%s: failed to get string id, err:%d\n",
					__func__, status);
			return status;
		}
		usbnet_string_defs[STRING_INTERFACE].id = status;
		usbnet_intf_desc.iInterface = status;
	}

	dev->cdev = c->cdev;
	dev->function.name = "usbnet";
	dev->function.fs_descriptors = fs_function;
	dev->function.hs_descriptors = hs_function;
	if (gadget_is_superspeed(c->cdev->gadget))
		dev->function.ss_descriptors = ss_function;
	dev->function.bind = usbnet_bind;
	dev->function.unbind = usbnet_unbind;
	dev->function.set_alt = usbnet_set_alt;
	dev->function.disable = usbnet_disable;
	dev->function.suspend = usbnet_suspend;
	dev->function.resume = usbnet_resume;
	dev->function.strings = usbnet_strings;

	ret = usb_add_function(c, &dev->function);

	return ret;
}
