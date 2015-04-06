/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/version.h>
#include <linux/atomic.h>

#include "hif_usb_internal.h"
#include <athdefs.h>
#include "a_types.h"
#include "athdefs.h"
#include "a_osapi.h"
#define ATH_MODULE_NAME hif
#include "a_debug.h"
#include "a_usb_defs.h"
#include "htc.h"
#include "htc_packet.h"
#include "qwlan_version.h"
#include "if_usb.h"
#include "vos_api.h"

#define IS_BULK_EP(attr) (((attr) & 3) == 0x02)
#define IS_INT_EP(attr)  (((attr) & 3) == 0x03)
#define IS_ISOC_EP(attr)  (((attr) & 3) == 0x01)
#define IS_DIR_IN(addr)  ((addr) & 0x80)

static void usb_hif_post_recv_transfers(HIF_USB_PIPE *recv_pipe,
					int buffer_length);
static void usb_hif_post_recv_bundle_transfers(HIF_USB_PIPE *recv_pipe,
					       int buffer_length);
static void usb_hif_cleanup_recv_urb(HIF_URB_CONTEXT *urb_context);
static void usb_hif_free_urb_to_pipe(HIF_USB_PIPE *pipe,
				     HIF_URB_CONTEXT *urb_context)
{
	unsigned long flags;

	spin_lock_irqsave(&pipe->device->cs_lock, flags);
	pipe->urb_cnt++;
	DL_ListAdd(&pipe->urb_list_head, &urb_context->link);
	spin_unlock_irqrestore(&pipe->device->cs_lock, flags);
}

HIF_URB_CONTEXT *usb_hif_alloc_urb_from_pipe(HIF_USB_PIPE *pipe)
{
	HIF_URB_CONTEXT *urb_context = NULL;
	DL_LIST *item;
	unsigned long flags;

	spin_lock_irqsave(&pipe->device->cs_lock, flags);
	item = DL_ListRemoveItemFromHead(&pipe->urb_list_head);
	if (item != NULL) {
		urb_context = A_CONTAINING_STRUCT(item, HIF_URB_CONTEXT, link);
		pipe->urb_cnt--;
	}
	spin_unlock_irqrestore(&pipe->device->cs_lock, flags);

	return urb_context;
}

static HIF_URB_CONTEXT *usb_hif_dequeue_pending_transfer(HIF_USB_PIPE *pipe)
{
	HIF_URB_CONTEXT *urb_context = NULL;
	DL_LIST *item;
	unsigned long flags;

	spin_lock_irqsave(&pipe->device->cs_lock, flags);
	item = DL_ListRemoveItemFromHead(&pipe->urb_pending_list);
	if (item != NULL)
		urb_context = A_CONTAINING_STRUCT(item, HIF_URB_CONTEXT, link);
	spin_unlock_irqrestore(&pipe->device->cs_lock, flags);

	return urb_context;
}

void usb_hif_enqueue_pending_transfer(HIF_USB_PIPE *pipe,
				      HIF_URB_CONTEXT *urb_context)
{
	unsigned long flags;

	spin_lock_irqsave(&pipe->device->cs_lock, flags);
	DL_ListInsertTail(&pipe->urb_pending_list, &urb_context->link);
	spin_unlock_irqrestore(&pipe->device->cs_lock, flags);
}

void usb_hif_remove_pending_transfer(HIF_URB_CONTEXT *urb_context)
{
	unsigned long flags;

	spin_lock_irqsave(&urb_context->pipe->device->cs_lock, flags);
	DL_ListRemove(&urb_context->link);
	spin_unlock_irqrestore(&urb_context->pipe->device->cs_lock, flags);
}

static A_STATUS usb_hif_alloc_pipe_resources(HIF_USB_PIPE *pipe, int urb_cnt)
{
	A_STATUS status = A_OK;
	int i;
	HIF_URB_CONTEXT *urb_context;

	DL_LIST_INIT(&pipe->urb_list_head);
	DL_LIST_INIT(&pipe->urb_pending_list);

	for (i = 0; i < urb_cnt; i++) {
		urb_context = adf_os_mem_alloc(NULL, sizeof(*urb_context));
		if (NULL == urb_context) {
			status = A_NO_MEMORY;
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("urb_context is null\n"));
			break;
		}
		adf_os_mem_zero(urb_context, sizeof(HIF_URB_CONTEXT));
		urb_context->pipe = pipe;
		urb_context->urb = usb_alloc_urb(0, GFP_KERNEL);

		if (NULL == urb_context->urb) {
			status = A_NO_MEMORY;
			adf_os_mem_free(urb_context);
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("urb_context->urb is null\n"));
			break;
		}

		/* note we are only allocate the urb contexts here, the actual
		 * URB is
		 * allocated from the kernel as needed to do a transaction
		 */
		pipe->urb_alloc++;

		if (htc_bundle_send) {
			/* In tx bundle mode, only pre-allocate bundle buffers
			 * for data
			 * pipes
			 */
			if (pipe->logical_pipe_num >= HIF_TX_DATA_LP_PIPE &&
			    pipe->logical_pipe_num <= HIF_TX_DATA_HP_PIPE) {
				urb_context->buf = adf_nbuf_alloc(NULL,
						  HIF_USB_TX_BUNDLE_BUFFER_SIZE,
						  0, 4, FALSE);
				if (NULL == urb_context->buf) {
					status = A_NO_MEMORY;
					usb_free_urb(urb_context->urb);
					urb_context->urb = NULL;
					adf_os_mem_free(urb_context);
					AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
					 "athusb: alloc send bundle buffer %d-byte failed\n",
					 HIF_USB_TX_BUNDLE_BUFFER_SIZE));
					break;
				}
			}
			skb_queue_head_init(&urb_context->comp_queue);
		}

		usb_hif_free_urb_to_pipe(pipe, urb_context);
	}

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_ENUM, (
			 "athusb: alloc resources lpipe:%d hpipe:0x%X urbs:%d\n",
			 pipe->logical_pipe_num,
			 pipe->usb_pipe_handle,
			 pipe->urb_alloc));
	return status;
}

static void usb_hif_free_pipe_resources(HIF_USB_PIPE *pipe)
{
	HIF_URB_CONTEXT *urb_context;
	adf_nbuf_t nbuf;

	if (NULL == pipe->device) {
		/* nothing allocated for this pipe */
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("pipe->device is null\n"));
		return;
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (
			 "athusb: free resources lpipe:%d hpipe:0x%X urbs:%d avail:%d\n",
			 pipe->logical_pipe_num,
			 pipe->usb_pipe_handle, pipe->urb_alloc,
			 pipe->urb_cnt));

	if (pipe->urb_alloc != pipe->urb_cnt) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
				 "athusb: urb leak! lpipe:%d hpipe:0x%X urbs:%d avail:%d\n",
				 pipe->logical_pipe_num,
				 pipe->usb_pipe_handle, pipe->urb_alloc,
				 pipe->urb_cnt));
	}

	while (TRUE) {
		urb_context = usb_hif_alloc_urb_from_pipe(pipe);
		if (NULL == urb_context)
			break;

		if (urb_context->buf) {
			adf_nbuf_free(urb_context->buf);
			urb_context->buf = NULL;
		}

		if (htc_bundle_send) {
			while ((nbuf =
				skb_dequeue(&urb_context->comp_queue)) !=
			       NULL) {
				adf_nbuf_free(nbuf);
			}
		}

		usb_free_urb(urb_context->urb);
		urb_context->urb = NULL;
		adf_os_mem_free(urb_context);
	}

}

static A_UINT8 usb_hif_get_logical_pipe_num(HIF_DEVICE_USB *device,
					    A_UINT8 ep_address, int *urb_count)
{
	A_UINT8 pipe_num = HIF_USB_PIPE_INVALID;

	switch (ep_address) {
	case USB_EP_ADDR_APP_CTRL_IN:
		pipe_num = HIF_RX_CTRL_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_IN:
		pipe_num = HIF_RX_DATA_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_INT_IN:
		pipe_num = HIF_RX_INT_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA2_IN:
		pipe_num = HIF_RX_DATA2_PIPE;
		*urb_count = RX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_CTRL_OUT:
		pipe_num = HIF_TX_CTRL_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_LP_OUT:
		pipe_num = HIF_TX_DATA_LP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_MP_OUT:
		pipe_num = HIF_TX_DATA_MP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	case USB_EP_ADDR_APP_DATA_HP_OUT:
		pipe_num = HIF_TX_DATA_HP_PIPE;
		*urb_count = TX_URB_COUNT;
		break;
	default:
		/* note: there may be endpoints not currently used */
		break;
	}

	return pipe_num;
}

A_STATUS usb_hif_setup_pipe_resources(HIF_DEVICE_USB *device)
{
	struct usb_interface *interface = device->interface;
	struct usb_host_interface *iface_desc = interface->cur_altsetting;
	struct usb_endpoint_descriptor *endpoint;
	int i;
	int urbcount;
	A_STATUS status = A_OK;
	HIF_USB_PIPE *pipe;
	A_UINT8 pipe_num;

	/* walk decriptors and setup pipes */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (IS_BULK_EP(endpoint->bmAttributes)) {
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_ENUM, (
					 "%s Bulk Ep:0x%2.2X " "maxpktsz:%d\n",
					 IS_DIR_IN(endpoint->bEndpointAddress) ?
					 "RX" : "TX",
					 endpoint->bEndpointAddress,
					 le16_to_cpu
					 (endpoint->wMaxPacketSize)));
		} else if (IS_INT_EP(endpoint->bmAttributes)) {
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_ENUM, (
					 "%s Int Ep:0x%2.2X maxpktsz:%d interval:%d\n",
					 IS_DIR_IN(endpoint->bEndpointAddress) ?
					 "RX" : "TX",
					 endpoint->bEndpointAddress,
					 le16_to_cpu(endpoint->wMaxPacketSize),
					 endpoint->bInterval));
		} else if (IS_ISOC_EP(endpoint->bmAttributes)) {
			/* TODO for ISO */
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_ENUM, (
					 "%s ISOC Ep:0x%2.2X maxpktsz:%d interval:%d\n",
					 IS_DIR_IN(endpoint->bEndpointAddress) ?
					 "RX" : "TX",
					 endpoint->bEndpointAddress,
					 le16_to_cpu(endpoint->wMaxPacketSize),
					 endpoint->bInterval));
		}
		urbcount = 0;

		pipe_num = usb_hif_get_logical_pipe_num(device,
							endpoint->
							bEndpointAddress,
							&urbcount);
		if (HIF_USB_PIPE_INVALID == pipe_num)
			continue;

		pipe = &device->pipes[pipe_num];
		if (pipe->device != NULL) {
			/* hmmm..pipe was already setup */
			continue;
		}

		pipe->device = device;
		pipe->logical_pipe_num = pipe_num;
		pipe->ep_address = endpoint->bEndpointAddress;
		pipe->max_packet_size = le16_to_cpu(endpoint->wMaxPacketSize);

		if (IS_BULK_EP(endpoint->bmAttributes)) {
			if (IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvbulkpipe(device->udev,
						    pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndbulkpipe(device->udev,
						    pipe->ep_address);
			}
		} else if (IS_INT_EP(endpoint->bmAttributes)) {
			if (IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvintpipe(device->udev,
						   pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndintpipe(device->udev,
						   pipe->ep_address);
			}
		} else if (IS_ISOC_EP(endpoint->bmAttributes)) {
			/* TODO for ISO */
			if (IS_DIR_IN(pipe->ep_address)) {
				pipe->usb_pipe_handle =
				    usb_rcvisocpipe(device->udev,
						    pipe->ep_address);
			} else {
				pipe->usb_pipe_handle =
				    usb_sndisocpipe(device->udev,
						    pipe->ep_address);
			}
		}
		pipe->ep_desc = endpoint;

		if (!IS_DIR_IN(pipe->ep_address))
			pipe->flags |= HIF_USB_PIPE_FLAG_TX;

		status = usb_hif_alloc_pipe_resources(pipe, urbcount);
		if (A_FAILED(status))
			break;

	}

	return status;
}

void usb_hif_cleanup_pipe_resources(HIF_DEVICE_USB *device)
{
	int i;

	for (i = 0; i < HIF_USB_PIPE_MAX; i++)
		usb_hif_free_pipe_resources(&device->pipes[i]);
}

static void usb_hif_flush_pending_transfers(HIF_USB_PIPE *pipe)
{
	HIF_URB_CONTEXT *urb_context;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s pipe : %d\n", __func__,
					pipe->logical_pipe_num));

	while (1) {
		urb_context = usb_hif_dequeue_pending_transfer(pipe);
		if (NULL == urb_context) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("urb_context is NULL\n"));
			break;
		}
		AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("  pending urb ctxt: 0x%p\n",
						urb_context));
		if (urb_context->urb != NULL) {
			AR_DEBUG_PRINTF(ATH_DEBUG_TRC,
					("  killing urb: 0x%p\n",
					 urb_context->urb));
			/* killing the URB will cause the completion routines to
			 * run
			 */
			usb_kill_urb(urb_context->urb);
		}
	}
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void usb_hif_flush_all(HIF_DEVICE_USB *device)
{
	int i;
	HIF_USB_PIPE *pipe;
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
		if (device->pipes[i].device != NULL) {
			usb_hif_flush_pending_transfers(&device->pipes[i]);
			pipe = &device->pipes[i];
			flush_work(&pipe->io_complete_work);
		}
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

static void usb_hif_cleanup_recv_urb(HIF_URB_CONTEXT *urb_context)
{
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	if (urb_context->buf != NULL) {
		adf_nbuf_free(urb_context->buf);
		urb_context->buf = NULL;
	}

	usb_hif_free_urb_to_pipe(urb_context->pipe, urb_context);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void usb_hif_cleanup_transmit_urb(HIF_URB_CONTEXT *urb_context)
{
	usb_hif_free_urb_to_pipe(urb_context->pipe, urb_context);
}

static void usb_hif_usb_recv_complete(struct urb *urb)
{
	HIF_URB_CONTEXT *urb_context = (HIF_URB_CONTEXT *) urb->context;
	A_STATUS status = A_OK;
	adf_nbuf_t buf = NULL;
	HIF_USB_PIPE *pipe = urb_context->pipe;

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, (
			 "+%s: recv pipe: %d, stat:%d,len:%d urb:0x%p\n",
			 __func__,
			 pipe->logical_pipe_num,
			 urb->status, urb->actual_length,
			 urb));

	/* this urb is not pending anymore */
	usb_hif_remove_pending_transfer(urb_context);

	do {

		if (urb->status != 0) {
			status = A_ECOMM;
			switch (urb->status) {
#ifdef RX_SG_SUPPORT
			case -EOVERFLOW:
				urb->actual_length = HIF_USB_RX_BUFFER_SIZE;
				status = A_OK;
				break;
#endif
			case -ECONNRESET:
			case -ENOENT:
			case -ESHUTDOWN:
				/* NOTE: no need to spew these errors when
				 * device is removed
				 * or urb is killed due to driver shutdown
				 */
				status = A_ECANCELED;
				break;
			default:
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
						 "%s recv pipe: %d (ep:0x%2.2X), failed:%d\n",
						 __func__,
						 pipe->logical_pipe_num,
						 pipe->ep_address,
						 urb->status));
				break;
			}
			break;
		}

		if (urb->actual_length == 0)
			break;

		buf = urb_context->buf;
		/* we are going to pass it up */
		urb_context->buf = NULL;
		adf_nbuf_put_tail(buf, urb->actual_length);
		if (AR_DEBUG_LVL_CHECK(USB_HIF_DEBUG_DUMP_DATA)) {
			A_UINT8 *data;
			A_UINT32 len;
			adf_nbuf_peek_header(buf, &data, &len);
			DebugDumpBytes(data, len, "hif recv data");
		}

		/* note: queue implements a lock */
		skb_queue_tail(&pipe->io_comp_queue, buf);
		schedule_work(&pipe->io_complete_work);

	} while (FALSE);

	usb_hif_cleanup_recv_urb(urb_context);

	if (A_SUCCESS(status)) {
		if (pipe->urb_cnt >= pipe->urb_cnt_thresh) {
			/* our free urbs are piling up, post more transfers */
			usb_hif_post_recv_transfers(pipe,
						    HIF_USB_RX_BUFFER_SIZE);
		}
	}

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, ("-%s\n", __func__));
}

static void usb_hif_usb_recv_bundle_complete(struct urb *urb)
{
	HIF_URB_CONTEXT *urb_context = (HIF_URB_CONTEXT *) urb->context;
	A_STATUS status = A_OK;
	adf_nbuf_t buf = NULL;
	HIF_USB_PIPE *pipe = urb_context->pipe;
	A_UINT8 *netdata, *netdata_new;
	A_UINT32 netlen, netlen_new;
	HTC_FRAME_HDR *HtcHdr;
	A_UINT16 payloadLen;
	adf_nbuf_t new_skb = NULL;

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, (
			 "+%s: recv pipe: %d, stat:%d,len:%d urb:0x%p\n",
			 __func__,
			 pipe->logical_pipe_num,
			 urb->status, urb->actual_length,
			 urb));

	/* this urb is not pending anymore */
	usb_hif_remove_pending_transfer(urb_context);

	do {

		if (urb->status != 0) {
			status = A_ECOMM;
			switch (urb->status) {
			case -ECONNRESET:
			case -ENOENT:
			case -ESHUTDOWN:
				/* NOTE: no need to spew these errors when
				 * device is removed
				 * or urb is killed due to driver shutdown
				 */
				status = A_ECANCELED;
				break;
			default:
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
						 "%s recv pipe: %d (ep:0x%2.2X), failed:%d\n",
						 __func__,
						 pipe->logical_pipe_num,
						 pipe->ep_address,
						 urb->status));
				break;
			}
			break;
		}

		if (urb->actual_length == 0)
			break;

		buf = urb_context->buf;

		if (AR_DEBUG_LVL_CHECK(USB_HIF_DEBUG_DUMP_DATA)) {
			A_UINT8 *data;
			A_UINT32 len;
			adf_nbuf_peek_header(buf, &data, &len);
			DebugDumpBytes(data, len, "hif recv data");
		}

		adf_nbuf_peek_header(buf, &netdata, &netlen);
		netlen = urb->actual_length;

		do {
#if defined(AR6004_1_0_ALIGN_WAR)
			A_UINT8 extra_pad;
			A_UINT16 act_frame_len;
#endif
			A_UINT16 frame_len;

			/* Hack into HTC header for bundle processing */
			HtcHdr = (HTC_FRAME_HDR *) netdata;
			if (HtcHdr->EndpointID >= ENDPOINT_MAX) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("athusb: Rx: invalid EndpointID=%d\n",
					HtcHdr->EndpointID));
				break;
			}

			payloadLen = HtcHdr->PayloadLen;
			payloadLen = A_LE2CPU16(payloadLen);

#if defined(AR6004_1_0_ALIGN_WAR)
			act_frame_len = (HTC_HDR_LENGTH + payloadLen);

			if (HtcHdr->EndpointID == 0 ||
				HtcHdr->EndpointID == 1) {
				/* assumption: target won't pad on HTC endpoint
				 * 0 & 1.
				 */
				extra_pad = 0;
			} else {
				extra_pad =
				    A_GET_UINT8_FIELD((A_UINT8 *) HtcHdr,
						      HTC_FRAME_HDR,
						      ControlBytes[1]);
			}
#endif

			if (payloadLen > HIF_USB_RX_BUFFER_SIZE) {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("athusb: payloadLen too long %u\n",
					payloadLen));
				break;
			}
#if defined(AR6004_1_0_ALIGN_WAR)
			frame_len = (act_frame_len + extra_pad);
#else
			frame_len = (HTC_HDR_LENGTH + payloadLen);
#endif

			if (netlen >= frame_len) {
				/* allocate a new skb and copy */
#if defined(AR6004_1_0_ALIGN_WAR)
				new_skb =
				    adf_nbuf_alloc(NULL, act_frame_len, 0, 4,
						   FALSE);
				if (new_skb == NULL) {
					AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
							 "athusb: allocate skb (len=%u) failed\n",
							 act_frame_len));
					break;
				}

				adf_nbuf_peek_header(new_skb, &netdata_new,
						     &netlen_new);
				adf_os_mem_copy(netdata_new, netdata,
						act_frame_len);
				adf_nbuf_put_tail(new_skb, act_frame_len);
#else
				new_skb =
				    adf_nbuf_alloc(NULL, frame_len, 0, 4,
						   FALSE);
				if (new_skb == NULL) {
					AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
							 "athusb: allocate skb (len=%u) failed\n",
							 frame_len));
					break;
				}

				adf_nbuf_peek_header(new_skb, &netdata_new,
						     &netlen_new);
				adf_os_mem_copy(netdata_new, netdata,
						frame_len);
				adf_nbuf_put_tail(new_skb, frame_len);
#endif
				skb_queue_tail(&pipe->io_comp_queue, new_skb);
				new_skb = NULL;

				netdata += frame_len;
				netlen -= frame_len;
			} else {
				AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
					 "athusb: subframe length %d not fitted into bundle packet length %d\n"
					 , netlen, frame_len));
				break;
			}

		} while (netlen);

		schedule_work(&pipe->io_complete_work);

	} while (FALSE);

	if (urb_context->buf == NULL) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("athusb: buffer in urb_context is NULL\n"));
	}

	/* reset urb_context->buf ==> seems not necessary */
	usb_hif_free_urb_to_pipe(urb_context->pipe, urb_context);

	if (A_SUCCESS(status)) {
		if (pipe->urb_cnt >= pipe->urb_cnt_thresh) {
			/* our free urbs are piling up, post more transfers */
			usb_hif_post_recv_bundle_transfers(pipe, 0
			/* pass zero for not allocating urb-buffer again */
			    );
		}
	}

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, ("-%s\n", __func__));
}

/* post recv urbs for a given pipe */
static void usb_hif_post_recv_transfers(HIF_USB_PIPE *recv_pipe,
					int buffer_length)
{
	HIF_URB_CONTEXT *urb_context;
	a_uint8_t *data;
	a_uint32_t len;
	struct urb *urb;
	int usb_status;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	while (1) {

		urb_context = usb_hif_alloc_urb_from_pipe(recv_pipe);
		if (NULL == urb_context)
			break;

		urb_context->buf =
		    adf_nbuf_alloc(NULL, buffer_length, 0, 4, FALSE);
		if (NULL == urb_context->buf) {
			usb_hif_cleanup_recv_urb(urb_context);
			break;
		}

		adf_nbuf_peek_header(urb_context->buf, &data, &len);

		urb = urb_context->urb;

		usb_fill_bulk_urb(urb,
				  recv_pipe->device->udev,
				  recv_pipe->usb_pipe_handle,
				  data,
				  buffer_length,
				  usb_hif_usb_recv_complete, urb_context);

		AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, (
				 "athusb bulk recv submit:%d, 0x%X (ep:0x%2.2X), %d bytes, buf:0x%p\n",
				 recv_pipe->logical_pipe_num,
				 recv_pipe->usb_pipe_handle,
				 recv_pipe->ep_address, buffer_length,
				 urb_context->buf));

		usb_hif_enqueue_pending_transfer(recv_pipe, urb_context);

		usb_status = usb_submit_urb(urb, GFP_ATOMIC);

		if (usb_status) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("athusb : usb bulk recv failed %d\n",
					 usb_status));
			usb_hif_remove_pending_transfer(urb_context);
			usb_hif_cleanup_recv_urb(urb_context);
			break;
		}
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));

}

/* post recv urbs for a given pipe */
static void usb_hif_post_recv_bundle_transfers(HIF_USB_PIPE *recv_pipe,
					       int buffer_length)
{
	HIF_URB_CONTEXT *urb_context;
	a_uint8_t *data;
	a_uint32_t len;
	struct urb *urb;
	int usb_status;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	while (1) {

		urb_context = usb_hif_alloc_urb_from_pipe(recv_pipe);
		if (NULL == urb_context)
			break;

		if (buffer_length) {
			urb_context->buf =
			    adf_nbuf_alloc(NULL, buffer_length, 0, 4, FALSE);
			if (NULL == urb_context->buf) {
				usb_hif_cleanup_recv_urb(urb_context);
				break;
			}
		}

		adf_nbuf_peek_header(urb_context->buf, &data, &len);

		urb = urb_context->urb;
		usb_fill_bulk_urb(urb,
				  recv_pipe->device->udev,
				  recv_pipe->usb_pipe_handle,
				  data,
				  HIF_USB_RX_BUNDLE_BUFFER_SIZE,
				  usb_hif_usb_recv_bundle_complete,
				  urb_context);

		AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN, (
				 "athusb bulk recv submit:%d, 0x%X (ep:0x%2.2X), %d bytes, buf:0x%p\n",
				 recv_pipe->logical_pipe_num,
				 recv_pipe->usb_pipe_handle,
				 recv_pipe->ep_address, buffer_length,
				 urb_context->buf));

		usb_hif_enqueue_pending_transfer(recv_pipe, urb_context);

		usb_status = usb_submit_urb(urb, GFP_ATOMIC);

		if (usb_status) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("athusb : usb bulk recv failed %d\n",
					 usb_status));
			usb_hif_remove_pending_transfer(urb_context);
			usb_hif_free_urb_to_pipe(urb_context->pipe,
						 urb_context);
			break;
		}

	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));

}

void usb_hif_start_recv_pipes(HIF_DEVICE_USB *device)
{
	device->pipes[HIF_RX_DATA_PIPE].urb_cnt_thresh =
	    device->pipes[HIF_RX_DATA_PIPE].urb_alloc / 2;

	if (!htc_bundle_recv) {
		usb_hif_post_recv_transfers(&device->pipes[HIF_RX_DATA_PIPE],
					    HIF_USB_RX_BUFFER_SIZE);
	} else {
		usb_hif_post_recv_bundle_transfers(&device->pipes
					   [HIF_RX_DATA_PIPE],
					   HIF_USB_RX_BUNDLE_BUFFER_SIZE);
	}

	if (!hif_usb_disable_rxdata2) {
		device->pipes[HIF_RX_DATA2_PIPE].urb_cnt_thresh =
		    device->pipes[HIF_RX_DATA2_PIPE].urb_alloc / 2;
		usb_hif_post_recv_transfers(&device->pipes[HIF_RX_DATA2_PIPE],
					    HIF_USB_RX_BUFFER_SIZE);
	}
#ifdef USB_HIF_TEST_INTERRUPT_IN
	device->pipes[HIF_RX_INT_PIPE].urb_cnt_thresh =
	    device->pipes[HIF_RX_INT_PIPE].urb_alloc / 2;
	usb_hif_post_recv_transfers(&device->pipes[HIF_RX_INT_PIPE],
				    HIF_USB_RX_BUFFER_SIZE);
#endif
}

A_STATUS usb_hif_submit_ctrl_out(HIF_DEVICE_USB *device,
				 a_uint8_t req,
				 a_uint16_t value,
				 a_uint16_t index, void *data, a_uint32_t size)
{
	a_int32_t result = 0;
	A_STATUS ret = A_OK;
	a_uint8_t *buf = NULL;

	do {

		if (size > 0) {
			buf = kmalloc(size, GFP_KERNEL);
			if (NULL == buf) {
				ret = A_NO_MEMORY;
				break;
			}
			memcpy(buf, (a_uint8_t *) data, size);
		}

		AR_DEBUG_PRINTF(USB_HIF_DEBUG_CTRL_TRANS, (
				 "ctrl-out req:0x%2.2X, value:0x%4.4X index:0x%4.4X, datasize:%d\n",
				 req, value, index, size));

		result = usb_control_msg(device->udev,
					 usb_sndctrlpipe(device->udev, 0),
					 req,
					 USB_DIR_OUT | USB_TYPE_VENDOR |
					 USB_RECIP_DEVICE, value, index, buf,
					 size, 2 * HZ);

		if (result < 0) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
					 "%s failed,result = %d\n",
					 __func__, result));
			ret = A_ERROR;
		}

	} while (FALSE);

	if (buf != NULL)
		kfree(buf);

	return ret;
}

A_STATUS usb_hif_submit_ctrl_in(HIF_DEVICE_USB *device,
				a_uint8_t req,
				a_uint16_t value,
				a_uint16_t index, void *data, a_uint32_t size)
{
	a_int32_t result = 0;
	A_STATUS ret = A_OK;
	a_uint8_t *buf = NULL;

	do {

		if (size > 0) {
			buf = kmalloc(size, GFP_KERNEL);
			if (NULL == buf) {
				ret = A_NO_MEMORY;
				break;
			}
		}

		AR_DEBUG_PRINTF(USB_HIF_DEBUG_CTRL_TRANS, (
				 "ctrl-in req:0x%2.2X, value:0x%4.4X index:0x%4.4X, datasize:%d\n",
				 req, value, index, size));

		result = usb_control_msg(device->udev,
					 usb_rcvctrlpipe(device->udev, 0),
					 req,
					 USB_DIR_IN | USB_TYPE_VENDOR |
					 USB_RECIP_DEVICE, value, index, buf,
					 size, 2 * HZ);

		if (result < 0) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
					("%s failed,result = %d\n",
					 __func__, result));
			ret = A_ERROR;
			break;
		}

		memcpy((a_uint8_t *) data, buf, size);

	} while (FALSE);

	if (buf != NULL)
		kfree(buf);

	return ret;
}

#define IS_FW_CRASH_DUMP(x)    ((x == FW_ASSERT_PATTERN) || \
                                (x == FW_REG_PATTERN) || \
                                ((x & FW_RAMDUMP_PATTERN_MASK) == FW_RAMDUMP_PATTERN))?1:0

void usb_hif_io_comp_work(struct work_struct *work)
{
	HIF_USB_PIPE *pipe = container_of(work, HIF_USB_PIPE, io_complete_work);
	adf_nbuf_t buf;
	HIF_DEVICE_USB *device;
	HTC_FRAME_HDR *HtcHdr;
	struct hif_usb_softc *sc;
	A_UINT8 *data;
	A_UINT32 len;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	device = pipe->device;
	sc = device->sc;

	while ((buf = skb_dequeue(&pipe->io_comp_queue))) {
		a_mem_trace(buf);
		if (pipe->flags & HIF_USB_PIPE_FLAG_TX) {
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT,
					("+athusb xmit callback " "buf:0x%p\n",
					 buf));
			HtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(buf, 0);

#ifdef ATH_11AC_TXCOMPACT
#error ATH_11AC_TXCOMPACT only support for High Latency mode
#else
			device->htcCallbacks.txCompletionHandler(device->
								 htcCallbacks.
								 Context, buf,
								 HtcHdr->
								 EndpointID);
#endif
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT,
					("-athusb xmit callback\n"));
		} else {
			AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN,
					("+athusb recv callback buf:" "0x%p\n",
					 buf));
			adf_nbuf_peek_header(buf, &data, &len);

			if (IS_FW_CRASH_DUMP(*((A_UINT32 *) data))) {
				sc->fw_data = data;
				sc->fw_data_len = len;
				device->htcCallbacks.fwEventHandler(
					device->htcCallbacks.Context, A_USB_ERROR);
				dev_kfree_skb(buf);
			} else {
				device->htcCallbacks.rxCompletionHandler(
				device->htcCallbacks.Context, buf,
				pipe->logical_pipe_num);
				AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN,
					("-athusb recv callback\n"));
			}
		}
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));

}
