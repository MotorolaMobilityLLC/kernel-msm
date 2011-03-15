
/*
 *  Qualcomm's Bluetooth Software In-Band Sleep UART protocol
 *
 *  HCI_SMD (HCI Shared Memory Driver) is Qualcomm's Shared memory driver
 *  for the HCI protocol.
 *
 *  Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 *  Acknowledgements:
 *  This file is based on hci_vhci.c, which was...
 *  written by Maxim Krasnyansky and Marcel Holtmann.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/poll.h>

#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/ioctl.h>
#include <linux/skbuff.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <soc/qco/smd.h>

#define VERSION "1.0"
#define BT_CMD 1
#define BT_DATA 2


struct hci_smd_data {
	struct hci_dev *hdev;
	void *data;
};

struct hci_smd_data hs;
struct smd_channel *cmd_channel;
struct smd_channel *data_channel;


/* ------- Interface to HCI layer ------ */
/* Initialize device */
static int hci_smd_open(struct hci_dev *hdev)
{
	BT_DBG("%s %p", hdev->name, hdev);

	/* Nothing to do for the SMD driver */

	set_bit(HCI_RUNNING, &hdev->flags);

	return 0;
}


static int hci_smd_flush(struct hci_dev *hdev)
{
	struct hci_smd_data *data = (struct hci_smd_data *)hdev->driver_data;
	kfree(data);
	return 0;
}


/*Closing the device*/
static int hci_smd_close(struct hci_dev *hdev)
{
	BT_DBG("hdev %p", hdev);

	if (!test_and_clear_bit(HCI_RUNNING, &hdev->flags))
		return 0;

	hci_smd_flush(hdev);
	hdev->flush = NULL;
	return 0;
}


static void hci_smd_destruct(struct hci_dev *hdev)
{
	kfree(hdev->driver_data);
}


static int hci_smd_recv_frame(struct hci_dev *hdev, int type)
{
	int len;
	struct sk_buff *skb;
	struct smd_channel *channel;
	unsigned  char *buf;

	switch (type) {
	case BT_CMD:
		channel = cmd_channel;
		break;
	case BT_DATA:
		channel = data_channel;
		break;
	default:
		return -EINVAL;
	}

	len = smd_cur_packet_size(cmd_channel);
	if (len > HCI_MAX_FRAME_SIZE)
		return -EINVAL;

	while (len) {
		skb = bt_skb_alloc(len, GFP_KERNEL);
		if (!skb)
			return -ENOMEM;

		buf = kmalloc(len, GFP_KERNEL);

		smd_read(channel, (void *)buf, len);


		if (memcpy(skb_put(skb, len), buf, len)) {
			kfree_skb(skb);
			return -EFAULT;
		}

		skb->dev = (void *)hdev;
		bt_cb(skb)->pkt_type = *((__u8 *) skb->data);
		skb_pull(skb, 1);

		hci_recv_frame(skb);

		kfree(skb);
		kfree(buf);

		len = smd_cur_packet_size(cmd_channel);
		if (len > HCI_MAX_FRAME_SIZE)
			return -EINVAL;
	}
	return 0;
}

static int hci_smd_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return -ENODEV;
	}

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		smd_write(cmd_channel, skb->data, skb->len);
		break;
	case HCI_ACLDATA_PKT:
	case HCI_SCODATA_PKT:
		smd_write(data_channel, skb->data, skb->len);
		break;
	default:
		break;
	}
	return 0;
}




static void hci_smd_notify_cmd(void *data, unsigned int event)
{
	struct hci_dev *hdev = ((struct hci_smd_data *)data)->hdev;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		hci_smd_recv_frame(hdev, BT_CMD);
		break;
	case SMD_EVENT_OPEN:
		hci_smd_open(hdev);
		break;
	case SMD_EVENT_CLOSE:
		hci_smd_close(hdev);
		break;
	default:
		break;
	}
}

static void hci_smd_notify_data(void *data, unsigned int event)
{
	struct hci_dev *hdev = ((struct hci_smd_data *)data)->hdev;

	if (!hdev) {
		BT_ERR("Frame for unknown HCI device (hdev=NULL)");
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA:
		hci_smd_recv_frame(hdev, BT_DATA);
		break;
	case SMD_EVENT_OPEN:
		hci_smd_open(hdev);
		break;
	case SMD_EVENT_CLOSE:
		hci_smd_close(hdev);
		break;
	default:
		break;
	}

}

static int hci_smd_register_dev(struct hci_smd_data *hs)
{
	struct hci_dev *hdev;

	/* Initialize and register HCI device */
	hdev = hci_alloc_dev();
	if (!hdev) {
		BT_ERR("Can't allocate HCI device");
		return -ENOMEM;
	}

	hs->hdev = hdev;

	hdev->bus = HCI_SMD;
	hdev->driver_data = hs;

	hdev->open  = hci_smd_open;
	hdev->close = hci_smd_close;
	hdev->flush = hci_smd_flush;
	hdev->send  = hci_smd_send_frame;
	hdev->destruct = hci_smd_destruct;

	hdev->owner = THIS_MODULE;

	if (hci_register_dev(hdev) < 0) {
		BT_ERR("Can't register HCI device");
		hci_free_dev(hdev);
		return -ENODEV;
	}

	/* Open the SMD Channel and device and register the callback function*/
	smd_named_open_on_edge("APPS_RIVA_BT_CMD", SMD_APPS_WCNSS, &cmd_channel,
						&hs, hci_smd_notify_cmd);
	smd_named_open_on_edge("APPS_RIVA_BT_ACL", SMD_APPS_WCNSS,
		&data_channel, &hs, hci_smd_notify_data);

	return 0;
}

static void hci_smd_deregister(void)
{
	smd_close(cmd_channel);
	smd_close(data_channel);
}

static int __init hci_smd_init(void)
{
	hci_smd_register_dev(&hs);
	return 0;
}


static void __exit hci_smd_exit(void)
{
	hci_smd_deregister();
}

module_init(hci_smd_init);
module_exit(hci_smd_exit);

MODULE_AUTHOR("Ankur Nandwani <ankurn@codeaurora.org>");
MODULE_DESCRIPTION("Bluetooth SMD driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
