/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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
#ifndef _HIF_USB_INTERNAL_H
#define _HIF_USB_INTERNAL_H

#include <adf_nbuf.h>
#include <adf_os_timer.h>

#include "a_types.h"
#include "athdefs.h"
#include "a_osapi.h"
#include "hif_msg_based.h"
#include "a_usb_defs.h"
#include <ol_if_athvar.h>
#include <linux/usb.h>

#define TX_URB_COUNT    32
#define RX_URB_COUNT    32

#define HIF_USB_RX_BUFFER_SIZE  (1792 + 8)
#define HIF_USB_RX_BUNDLE_ONE_PKT_SIZE  (1792 + 8)

/* USB Endpoint definition */
typedef enum {
	HIF_TX_CTRL_PIPE = 0,
	HIF_TX_DATA_LP_PIPE,
	HIF_TX_DATA_MP_PIPE,
	HIF_TX_DATA_HP_PIPE,
	HIF_RX_CTRL_PIPE,
	HIF_RX_DATA_PIPE,
	HIF_RX_DATA2_PIPE,
	HIF_RX_INT_PIPE,
	HIF_USB_PIPE_MAX
} HIF_USB_PIPE_ID;

#define HIF_USB_PIPE_INVALID HIF_USB_PIPE_MAX

/* debug masks */
#define USB_HIF_DEBUG_CTRL_TRANS            ATH_DEBUG_MAKE_MODULE_MASK(0)
#define USB_HIF_DEBUG_BULK_IN               ATH_DEBUG_MAKE_MODULE_MASK(1)
#define USB_HIF_DEBUG_BULK_OUT              ATH_DEBUG_MAKE_MODULE_MASK(2)
#define USB_HIF_DEBUG_ENUM                  ATH_DEBUG_MAKE_MODULE_MASK(3)
#define USB_HIF_DEBUG_DUMP_DATA             ATH_DEBUG_MAKE_MODULE_MASK(4)
#define USB_HIF_SUSPEND                     ATH_DEBUG_MAKE_MODULE_MASK(5)
#define USB_HIF_ISOC_SUPPORT                ATH_DEBUG_MAKE_MODULE_MASK(6)
struct _HIF_DEVICE_USB;
struct _HIF_USB_PIPE;

/*
 * Data structure to record required sending context data
 */
struct HIFSendContext {
	A_BOOL bNewAlloc;
	HIF_DEVICE *pDev;
	adf_nbuf_t netbuf;
	unsigned int transferID;
	unsigned int head_data_len;
};

typedef struct _HIF_URB_CONTEXT {
	DL_LIST link;
	struct _HIF_USB_PIPE *pipe;
	adf_nbuf_t buf;
	struct urb *urb;
	struct HIFSendContext *pSendContext;
} HIF_URB_CONTEXT;

#define HIF_USB_PIPE_FLAG_TX    (1 << 0)

typedef struct _HIF_USB_PIPE {
	DL_LIST urb_list_head;
	DL_LIST urb_pending_list;
	A_INT32 urb_alloc;
	A_INT32 urb_cnt;
	A_INT32 urb_cnt_thresh;
	unsigned int usb_pipe_handle;
	A_UINT32 flags;
	A_UINT8 ep_address;
	A_UINT8 logical_pipe_num;
	struct _HIF_DEVICE_USB *device;
	A_UINT16 max_packet_size;
#ifdef HIF_USB_TASKLET
	struct tasklet_struct io_complete_tasklet;
#else
	struct work_struct io_complete_work;
#endif
	struct sk_buff_head io_comp_queue;
	struct usb_endpoint_descriptor *ep_desc;
	A_INT32 urb_prestart_cnt;
} HIF_USB_PIPE;

typedef struct _HIF_DEVICE_USB {
	spinlock_t cs_lock;
	spinlock_t tx_lock;
	spinlock_t rx_lock;
	MSG_BASED_HIF_CALLBACKS htcCallbacks;
	struct usb_device *udev;
	struct usb_interface *interface;
	HIF_USB_PIPE pipes[HIF_USB_PIPE_MAX];
	a_uint8_t surpriseRemoved;
	a_uint8_t *diag_cmd_buffer;
	a_uint8_t *diag_resp_buffer;
	void *claimed_context;
	struct hif_usb_softc *sc;
	A_BOOL is_bundle_enabled;
	A_UINT16 rx_bundle_cnt;
	A_UINT32 rx_bundle_buf_len;
} HIF_DEVICE_USB;
extern unsigned int hif_usb_disable_rxdata2;

extern A_STATUS usb_hif_submit_ctrl_in(HIF_DEVICE_USB *macp,
				       a_uint8_t req,
				       a_uint16_t value,
				       a_uint16_t index,
				       void *data, a_uint32_t size);

extern A_STATUS usb_hif_submit_ctrl_out(HIF_DEVICE_USB *macp,
					a_uint8_t req,
					a_uint16_t value,
					a_uint16_t index,
					void *data, a_uint32_t size);

extern int HIF_USBDeviceInserted(struct usb_interface *interface,
				 hif_handle_t hif_hdl);
extern void HIF_USBDeviceDetached(struct usb_interface *interface,
				  a_uint8_t surprise_removed);
#ifdef ATH_BUS_PM
extern void HIFDeviceSuspend(HIF_DEVICE *dev);
extern void HIFDeviceResume(HIF_DEVICE *dev);
#endif
extern A_STATUS usb_hif_setup_pipe_resources(HIF_DEVICE_USB *device);
extern void usb_hif_cleanup_pipe_resources(HIF_DEVICE_USB *device);
extern void usb_hif_prestart_recv_pipes(HIF_DEVICE_USB *device);
extern void usb_hif_start_recv_pipes(HIF_DEVICE_USB *device);
extern void usb_hif_flush_all(HIF_DEVICE_USB *device);
extern void usb_hif_cleanup_transmit_urb(HIF_URB_CONTEXT *urb_context);
extern void usb_hif_enqueue_pending_transfer(HIF_USB_PIPE *pipe,
					     HIF_URB_CONTEXT *urb_context);
extern void usb_hif_remove_pending_transfer(HIF_URB_CONTEXT *urb_context);
extern HIF_URB_CONTEXT *usb_hif_alloc_urb_from_pipe(HIF_USB_PIPE *pipe);
#ifdef HIF_USB_TASKLET
extern void usb_hif_io_comp_tasklet(long unsigned int context);
#else
extern void usb_hif_io_comp_work(struct work_struct *work);
#endif
/* Support for USB Suspend / Resume */
extern void usb_hif_suspend(struct usb_interface *interface);
extern void usb_hif_resume(struct usb_interface *interface);
A_STATUS HIFDiagWriteWARMRESET(struct usb_interface *interface,
			       A_UINT32 address, A_UINT32 data);

#endif
