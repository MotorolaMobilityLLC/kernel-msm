/*
 * USB2 Test Mode driver
 * Copyright (C) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/* This driver supports USB Host Test Mode initiated by test device with
 * VID 0x1a0a for USB controller in high-speed.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>

#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "../core/usb.h"

struct usb_tm_dev {
	struct usb_device	*udev;
	struct usb_hcd		*hcd;

#define TBUF_SIZE	256
	u8			*buf;
};

static int
usb_tm_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_tm_dev	*dev;
	int retval, port_num;

	dev_dbg(&intf->dev, "USB test mode is initiated.\n");
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->buf = kmalloc(TBUF_SIZE, GFP_KERNEL);
	if (!dev->buf) {
		kfree(dev);
		return -ENOMEM;
	}

	dev->udev = usb_get_dev(interface_to_usbdev(intf));
	dev->hcd = usb_get_hcd(bus_to_hcd(dev->udev->bus));
	usb_set_intfdata(intf, dev);
	port_num = dev->udev->portnum & 0xff;

	dev_dbg(&intf->dev, "test mode PID 0x%04x\n",
		le16_to_cpu(dev->udev->descriptor.idProduct));
	switch (le16_to_cpu(dev->udev->descriptor.idProduct)) {
	case 0x0101:
		/* TEST_SE0_NAK */
		dev->hcd->driver->hub_control(dev->hcd, SetPortFeature,
			USB_PORT_FEAT_TEST, 0x300 + port_num, NULL, 0);
		break;
	case 0x0102:
		/* TEST_J */
		dev->hcd->driver->hub_control(dev->hcd, SetPortFeature,
			USB_PORT_FEAT_TEST, 0x100 + port_num, NULL, 0);
		break;
	case 0x0103:
		/* TEST_K */
		dev->hcd->driver->hub_control(dev->hcd, SetPortFeature,
			USB_PORT_FEAT_TEST, 0x200 + port_num, NULL, 0);
		break;
	case 0x0104:
		/* TEST_PACKET */
		dev->hcd->driver->hub_control(dev->hcd, SetPortFeature,
			USB_PORT_FEAT_TEST, 0x400 + port_num, NULL, 0);
		break;
	case 0x0106:
		/* HS_HOST_PORT_SUSPEND_RESUME */
		msleep(15000);
		dev->hcd->driver->hub_control(dev->hcd, SetPortFeature,
			USB_PORT_FEAT_SUSPEND, port_num, NULL, 0);
		msleep(15000);
		dev->hcd->driver->hub_control(dev->hcd, ClearPortFeature,
			USB_PORT_FEAT_SUSPEND, port_num, NULL, 0);
		break;
	case 0x0107:
		/* SINGLE_STEP_GET_DEV_DESC */
		msleep(15000);
		retval = usb_control_msg(dev->udev,
				usb_rcvctrlpipe(dev->udev, 0),
				USB_REQ_GET_DESCRIPTOR,
				USB_DIR_IN | USB_RECIP_DEVICE,
				cpu_to_le16(USB_DT_DEVICE << 8),
				0, dev->buf,
				USB_DT_DEVICE_SIZE,
				USB_CTRL_GET_TIMEOUT);
		break;
	case 0x0108:
		/* SINGLE_STEP_SET_FEATURE */

		/* FIXME */
		/* set size = 0 to ignore DATA phase */
		retval = usb_control_msg(dev->udev,
				usb_rcvctrlpipe(dev->udev, 0),
				USB_REQ_GET_DESCRIPTOR,
				USB_DIR_IN | USB_RECIP_DEVICE,
				cpu_to_le16(USB_DT_DEVICE << 8),
				0, dev->buf, 0,
				USB_CTRL_GET_TIMEOUT);
		msleep(15000);
		retval = usb_control_msg(dev->udev,
				usb_rcvctrlpipe(dev->udev, 0),
				USB_REQ_GET_DESCRIPTOR,
				USB_DIR_IN | USB_RECIP_DEVICE,
				cpu_to_le16(USB_DT_DEVICE << 8),
				0, dev->buf,
				USB_DT_DEVICE_SIZE,
				USB_CTRL_GET_TIMEOUT);
		break;
	default:
		dev_info(&intf->dev, "unknown test mode with PID 0x%04x",
			id->idProduct);
	}

	return 0;
}

static void usb_tm_disconnect(struct usb_interface *intf)
{
	struct usb_tm_dev	*dev = usb_get_intfdata(intf);

	usb_put_hcd(dev->hcd);
	usb_put_dev(dev->udev);
	usb_set_intfdata(intf, NULL);
	dev_dbg(&intf->dev, "disconnect\n");
	kfree(dev->buf);
	kfree(dev);
}

static const struct usb_device_id id_table[] = {
	/* USB Test Device */
	{ .match_flags = USB_DEVICE_ID_MATCH_VENDOR,
		.idVendor = 0x1A0A,
		},

	{ }
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver usb_tm_driver = {
	.name =		"usb_tm",
	.id_table =	id_table,
	.probe =	usb_tm_probe,
	.disconnect =	usb_tm_disconnect,
};

/*-------------------------------------------------------------------------*/

static int __init usb_tm_init(void)
{
	int result;

	result = usb_register(&usb_tm_driver);
	if (result)
		pr_err("usb_tm: usb_register failed. error number %d",
			result);

	return result;
}
module_init(usb_tm_init);

static void __exit usb_tm_exit(void)
{
	usb_deregister(&usb_tm_driver);
}
module_exit(usb_tm_exit);

MODULE_DESCRIPTION("USB Test Mode Driver");
MODULE_LICENSE("GPL");
