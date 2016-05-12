/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
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
#include <adf_os_types.h>
#include <adf_os_dma.h>
#include <adf_os_timer.h>
#include <adf_os_time.h>
#include <adf_os_lock.h>
#include <adf_os_io.h>
#include <adf_os_mem.h>
#include <adf_os_module.h>
#include <adf_os_util.h>
#include <adf_os_stdtypes.h>
#include <adf_os_defer.h>
#include <adf_os_atomic.h>
#include <adf_nbuf.h>
#include "adf_net_types.h"
#include <athdefs.h>
#include "a_types.h"
#include "athdefs.h"
#include "a_osapi.h"
#include <linux/usb.h>
#include "hif_usb_internal.h"
#include <hif.h>
#include <htc_services.h>
#include <ol_if_athvar.h>
#include <if_usb.h>
#define ATH_MODULE_NAME hif
#include <a_debug.h>

#define USB_HIF_USE_SINGLE_PIPE_FOR_DATA
#define USB_HIF_TARGET_CREDIT_SIZE  1664
#ifdef WLAN_DEBUG
static ATH_DEBUG_MASK_DESCRIPTION g_HIFDebugDescription[] = {
	{USB_HIF_DEBUG_CTRL_TRANS, "Control Transfers"},
	{USB_HIF_DEBUG_BULK_IN, "BULK In Transfers"},
	{USB_HIF_DEBUG_BULK_OUT, "BULK Out Transfers"},
	{USB_HIF_DEBUG_DUMP_DATA, "Dump data"},
	{USB_HIF_DEBUG_ENUM, "Enumeration"},
};

ATH_DEBUG_INSTANTIATE_MODULE_VAR(hif,
				 "hif",
				 "USB Host Interface",
				 ATH_DEBUG_MASK_DEFAULTS | ATH_DEBUG_INFO |
				 USB_HIF_DEBUG_ENUM,
				 ATH_DEBUG_DESCRIPTION_COUNT
				 (g_HIFDebugDescription),
				 g_HIFDebugDescription);

#endif

/* use credit flow control over HTC */
unsigned int htc_credit_flow;
module_param(htc_credit_flow, uint, 0644);

#ifdef USB_ISOC_SUPPORT
unsigned int hif_usb_isoch_vo = 1;
#else
unsigned int hif_usb_isoch_vo;
#endif
module_param(hif_usb_isoch_vo, uint, 0644);
unsigned int hif_usb_disable_rxdata2 = 1;
module_param(hif_usb_disable_rxdata2, uint, 0644);

unsigned int hif_usbaudioclass;
module_param(hif_usbaudioclass, uint, 0644);

static void usb_hif_destroy(HIF_DEVICE_USB *device);
static HIF_DEVICE_USB *usb_hif_create(struct usb_interface *interface);

OSDRV_CALLBACKS osDrvcallback;

int notifyDeviceInsertedHandler(void *hHIF)
{
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	osDrvcallback.deviceInsertedHandler(osDrvcallback.context, hHIF);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	complete_and_exit(NULL, 0);
	return 0;
}

int notifyDeviceSurprisedRemovedHandler(void *context)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) context;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	osDrvcallback.deviceRemovedHandler(device->claimed_context, device);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	complete_and_exit(NULL, 0);
	return 0;
}

int HIF_USBDeviceInserted(struct usb_interface *interface, hif_handle_t hif_hdl)
{
	HIF_DEVICE_USB *device = NULL;
	int retval = -1;
	struct hif_usb_softc *sc = hif_hdl;
	struct usb_device *udev = interface_to_usbdev(interface);

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	AR_DEBUG_PRINTF(ATH_DEBUG_INFO, ("BCDDevice : %x\n",
					 udev->descriptor.bcdDevice));

	do {

		device = usb_hif_create(interface);
		if (NULL == device) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("device is NULL\n"));
			break;
		}
		device->sc = sc;
		sc->hif_device = (HIF_DEVICE *) device;
		retval = 0;

	} while (FALSE);

	if ((device != NULL) && (retval < 0)) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("abnormal condition\n"));
		usb_hif_destroy(device);
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	return retval;
}

void HIF_USBDeviceDetached(struct usb_interface *interface,
			   a_uint8_t surprise_removed)
{
	HIF_DEVICE_USB *device;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	do {

		device = (HIF_DEVICE_USB *) usb_get_intfdata(interface);
		if (NULL == device) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("device is NULL\n"));
			break;
		}

		device->surpriseRemoved = surprise_removed;
		/* inform upper layer if it is still interested */
		if (surprise_removed
		    && (osDrvcallback.deviceRemovedHandler != NULL)
		    && (device->claimed_context != NULL)) {
			osDrvcallback.deviceRemovedHandler(device->
							   claimed_context,
							   device);
		}

		usb_hif_destroy(device);
		athdiag_procfs_remove();
	} while (FALSE);

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));

}

static void usb_hif_usb_transmit_complete(struct urb *urb)
{
	HIF_URB_CONTEXT *urb_context = (HIF_URB_CONTEXT *) urb->context;
	adf_nbuf_t buf;
	HIF_USB_PIPE *pipe = urb_context->pipe;
	struct HIFSendContext *pSendContext;

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT,
			("+%s: pipe: %d, stat:%d, len:%d\n", __func__,
			 pipe->logical_pipe_num, urb->status,
			 urb->actual_length));

	/* this urb is not pending anymore */
	usb_hif_remove_pending_transfer(urb_context);

	if (urb->status != 0) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("%s:  pipe: %d, failed:%d\n",
						__func__,
						pipe->logical_pipe_num,
						urb->status));
	}

	a_mem_trace(urb_context->buf);
	buf = urb_context->buf;
	pSendContext = urb_context->pSendContext;

	if (pSendContext->bNewAlloc) {
		adf_os_mem_free((void *)pSendContext);
	} else {
		adf_nbuf_pull_head(buf, pSendContext->head_data_len);
	}

	urb_context->buf = NULL;
	usb_hif_cleanup_transmit_urb(urb_context);

#if 0
	if (pipe->urbcnt >= pipe->urb_cnt_thresh)
		/* TBD */
#endif

	/* note: queue implements a lock */
	skb_queue_tail(&pipe->io_comp_queue, buf);
#ifdef HIF_USB_TASKLET
	tasklet_schedule(&pipe->io_complete_tasklet);
#else
	schedule_work(&pipe->io_complete_work);
#endif

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT, ("-%s\n", __func__));
}

static A_STATUS HIFSend_internal(HIF_DEVICE *hifDevice, a_uint8_t PipeID,
		 adf_nbuf_t hdr_buf, adf_nbuf_t buf, unsigned int nbytes)
{
	A_STATUS status = A_OK;
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;
	HIF_USB_PIPE *pipe = &device->pipes[PipeID];
	HIF_URB_CONTEXT *urb_context;
	A_UINT8 *data;
	A_UINT32 len;
	struct urb *urb;
	int usb_status;
	int i;
	struct HIFSendContext *pSendContext;
	int frag_count = 0, head_data_len, tmp_frag_count = 0;
	unsigned char *pData;

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT, ("+%s pipe : %d, buf:0x%p\n",
			__func__, PipeID, buf));

	a_mem_trace(buf);

	frag_count = adf_nbuf_get_num_frags(buf);
	if (frag_count > 1) {	/* means have extra fragment buf in skb */
		/* header data length should be total sending length substract
		 * internal data length of netbuf
		 * | HIFSendContext | fragments except internal buffer |
		 * netbuf->data
		 */
		head_data_len = sizeof(struct HIFSendContext);
		while (tmp_frag_count < (frag_count - 1)) {
			head_data_len =
				head_data_len + adf_nbuf_get_frag_len(buf,
						tmp_frag_count);
			tmp_frag_count = tmp_frag_count + 1;
		}
	} else {
		/*
		 * | HIFSendContext | netbuf->data
		 */
		head_data_len = sizeof(struct HIFSendContext);
	}

	/* Check whether head room is enough to save extra head data */
	if (head_data_len <= adf_nbuf_headroom(buf)) {
		pSendContext = (struct HIFSendContext *)adf_nbuf_push_head(buf,
				head_data_len);
		pSendContext->bNewAlloc = FALSE;
	} else {
		pSendContext = adf_os_mem_alloc(NULL,
				sizeof
				(struct
				 HIFSendContext)
				+
				head_data_len
				+
				nbytes);
		pSendContext->bNewAlloc = TRUE;
	}
	pSendContext->netbuf = buf;
	pSendContext->pDev = hifDevice;
	pSendContext->transferID = PipeID;
	pSendContext->head_data_len = head_data_len;
	/*
	 * Copy data to head part of netbuf or head of allocated buffer.
	 * if buffer is new allocated, the last buffer should be copied also.
	 * It assume last fragment is internal buffer of netbuf
	 * sometime total length of fragments larger than nbytes
	 */
	pData = (unsigned char *)pSendContext + sizeof(struct HIFSendContext);
	for (i = 0; i < (pSendContext->bNewAlloc ? frag_count : frag_count - 1);
			i++) {
		int frag_len = adf_nbuf_get_frag_len(buf, i);
		unsigned char *frag_addr = adf_nbuf_get_frag_vaddr(buf, i);
		memcpy(pData, frag_addr, frag_len);
		pData += frag_len;
	}
	/* Reset pData pointer and send out */
	pData = (unsigned char *)pSendContext + sizeof(struct HIFSendContext);

	do {
		urb_context = usb_hif_alloc_urb_from_pipe(pipe);
		if (NULL == urb_context) {
			/* TODO : note, it is possible to run out of urbs if 2
			 * endpoints map to the same pipe ID
			 */
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
				"%s pipe:%d no urbs left. URB Cnt : %d\n",
				__func__, PipeID, pipe->urb_cnt));
			status = A_NO_RESOURCE;
			break;
		}
		urb_context->pSendContext = pSendContext;
		urb = urb_context->urb;
		urb_context->buf = buf;
		data = pData;
		len = nbytes;

		usb_fill_bulk_urb(urb,
				  device->udev,
				  pipe->usb_pipe_handle,
				  data,
				  (len % pipe->max_packet_size) ==
				  0 ? (len + 1) : len,
				  usb_hif_usb_transmit_complete, urb_context);

		if ((len % pipe->max_packet_size) == 0) {
			/* hit a max packet boundary on this pipe */
			/* urb->transfer_flags |= URB_ZERO_PACKET; */
		}

		AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT, (
			"athusb bulk send submit:%d, 0x%X (ep:0x%2.2X), %d bytes\n",
			pipe->logical_pipe_num, pipe->usb_pipe_handle,
			pipe->ep_address, nbytes));

		usb_hif_enqueue_pending_transfer(pipe, urb_context);
		usb_status = usb_submit_urb(urb, GFP_ATOMIC);
		if (usb_status) {
			if (pSendContext->bNewAlloc)
				adf_os_mem_free(pSendContext);
			else
				adf_nbuf_pull_head(buf, head_data_len);
			urb_context->buf = NULL;
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
						"athusb : usb bulk transmit failed %d\n",
						usb_status));
			usb_hif_remove_pending_transfer(urb_context);
			usb_hif_cleanup_transmit_urb(urb_context);
			status = A_ECOMM;
			break;
		}

	} while (FALSE);

	if (A_FAILED(status) && (status != A_NO_RESOURCE)) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
				("athusb send failed %d\n", status));
	}

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_OUT,
			("-%s pipe : %d\n", __func__, PipeID));

	return status;
}

/* Send the entire buffer */
A_STATUS HIFSend(HIF_DEVICE *hif_device, a_uint8_t pipe, adf_nbuf_t hdr_buf,
		 adf_nbuf_t netbuf)
{
	return HIFSend_head(hif_device, pipe, 0, adf_nbuf_len(netbuf), netbuf);
}

A_STATUS
HIFSend_head(HIF_DEVICE *hif_device,
	     a_uint8_t pipe, unsigned int transfer_id, unsigned int nbytes,
	     adf_nbuf_t nbuf)
{
	A_STATUS status = EOK;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	status = HIFSend_internal(hif_device, pipe, NULL, nbuf, nbytes);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));

	return status;
}

a_uint16_t HIFGetFreeQueueNumber(HIF_DEVICE *hifDevice, a_uint8_t PipeID)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;
	return device->pipes[PipeID].urb_cnt;
}

a_uint16_t HIFGetMaxQueueNumber(HIF_DEVICE *hifDevice, a_uint8_t PipeID)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;
	return device->pipes[PipeID].urb_alloc;
}

void HIFPostInit(HIF_DEVICE *hifDevice, void *unused,
		 MSG_BASED_HIF_CALLBACKS *callbacks)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;

	A_MEMCPY(&device->htcCallbacks.Context, callbacks,
		 sizeof(MSG_BASED_HIF_CALLBACKS));
}

void HIFDetachHTC(HIF_DEVICE *hifDevice)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;

	usb_hif_flush_all(device);

	A_MEMZERO(&device->htcCallbacks, sizeof(MSG_BASED_HIF_CALLBACKS));
}

static void usb_hif_destroy(HIF_DEVICE_USB *device)
{
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	usb_hif_flush_all(device);

	usb_hif_cleanup_pipe_resources(device);

	usb_set_intfdata(device->interface, NULL);

	if (device->diag_cmd_buffer != NULL)
		adf_os_mem_free(device->diag_cmd_buffer);

	if (device->diag_resp_buffer != NULL)
		adf_os_mem_free(device->diag_resp_buffer);

	adf_os_mem_free(device);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

static HIF_DEVICE_USB *usb_hif_create(struct usb_interface *interface)
{
	HIF_DEVICE_USB *device = NULL;
	struct usb_device *dev = interface_to_usbdev(interface);
	A_STATUS status = A_OK;
	int i;
	HIF_USB_PIPE *pipe;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	do {

		device = adf_os_mem_alloc(NULL, sizeof(*device));
		if (NULL == device) {
			AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("device is NULL\n"));
			break;
		}

		adf_os_mem_zero(device, sizeof(*device));
		usb_set_intfdata(interface, device);
		spin_lock_init(&(device->cs_lock));
		spin_lock_init(&(device->rx_lock));
		spin_lock_init(&(device->tx_lock));
		device->udev = dev;
		device->interface = interface;

		for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
			pipe = &device->pipes[i];
#ifdef HIF_USB_TASKLET
			tasklet_init(&pipe->io_complete_tasklet, usb_hif_io_comp_tasklet,
						(long unsigned int)pipe);
#else
			INIT_WORK(&pipe->io_complete_work,
				  usb_hif_io_comp_work);
#endif
			skb_queue_head_init(&pipe->io_comp_queue);
		}

		device->diag_cmd_buffer = adf_os_mem_alloc(NULL,
						USB_CTRL_MAX_DIAG_CMD_SIZE);
		if (NULL == device->diag_cmd_buffer) {
			status = A_NO_MEMORY;
			break;
		}
		device->diag_resp_buffer = adf_os_mem_alloc(NULL,
						USB_CTRL_MAX_DIAG_RESP_SIZE);
		if (NULL == device->diag_resp_buffer) {
			status = A_NO_MEMORY;
			break;
		}

		status = usb_hif_setup_pipe_resources(device);

	} while (FALSE);

	if (A_FAILED(status)) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("abnormal condition\n"));
		usb_hif_destroy(device);
		device = NULL;
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	return device;
}

A_STATUS HIFStart(HIF_DEVICE *hifDevice)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;
	int i;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	usb_hif_prestart_recv_pipes(device);

	/* set the TX resource avail threshold for each TX pipe */
	for (i = HIF_TX_CTRL_PIPE; i <= HIF_TX_DATA_HP_PIPE; i++) {
		device->pipes[i].urb_cnt_thresh =
		    device->pipes[i].urb_alloc / 2;
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	return A_OK;
}

void HIFStop(HIF_DEVICE *hifDevice)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hifDevice;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	usb_hif_flush_all(device);

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void HIFGetDefaultPipe(HIF_DEVICE *hifDevice, a_uint8_t *ULPipe,
		       a_uint8_t *DLPipe)
{
	*ULPipe = HIF_TX_CTRL_PIPE;
	*DLPipe = HIF_RX_CTRL_PIPE;
}

#if defined(USB_MULTI_IN_TEST) || defined(USB_ISOC_TEST)
int
HIFMapServiceToPipe(HIF_DEVICE *hif_device, a_uint16_t ServiceId,
		    a_uint8_t *ULPipe, a_uint8_t *DLPipe, int *ul_is_polled,
		    int *dl_is_polled)
{
	A_STATUS status = A_OK;

	switch (ServiceId) {
	case HTC_CTRL_RSVD_SVC:
	case WMI_CONTROL_SVC:
	case HTC_RAW_STREAMS_SVC:
		*ULPipe = HIF_TX_CTRL_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_BE_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_BK_SVC:
		*ULPipe = HIF_TX_DATA_MP_PIPE;
		*DLPipe = HIF_RX_DATA2_PIPE;
		break;
	case WMI_DATA_VI_SVC:
		*ULPipe = HIF_TX_DATA_HP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_VO_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	default:
		status = A_ENOTSUP;
		break;
	}

	return status;
}
#else
int
HIFMapServiceToPipe(HIF_DEVICE *hif_device, a_uint16_t ServiceId,
		    a_uint8_t *ULPipe, a_uint8_t *DLPipe, int *ul_is_polled,
		    int *dl_is_polled)
{
	A_STATUS status = A_OK;

	switch (ServiceId) {
	case HTC_CTRL_RSVD_SVC:
	case WMI_CONTROL_SVC:
		*ULPipe = HIF_TX_CTRL_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case WMI_DATA_BE_SVC:
	case WMI_DATA_BK_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		if (hif_usb_disable_rxdata2)
			*DLPipe = HIF_RX_DATA_PIPE;
		else
			*DLPipe = HIF_RX_DATA2_PIPE;
		break;
	case WMI_DATA_VI_SVC:
#ifdef USB_HIF_USE_SINGLE_PIPE_FOR_DATA
		*ULPipe = HIF_TX_DATA_LP_PIPE;
#else
		*ULPipe = HIF_TX_DATA_MP_PIPE;
#endif
		if (hif_usb_disable_rxdata2)
			*DLPipe = HIF_RX_DATA_PIPE;
		else
			*DLPipe = HIF_RX_DATA2_PIPE;
		break;
	case WMI_DATA_VO_SVC:
#ifdef USB_HIF_USE_SINGLE_PIPE_FOR_DATA
		*ULPipe = HIF_TX_DATA_LP_PIPE;
#else
		*ULPipe = HIF_TX_DATA_HP_PIPE;
#endif
		if (hif_usb_disable_rxdata2)
			*DLPipe = HIF_RX_DATA_PIPE;
		else
			*DLPipe = HIF_RX_DATA2_PIPE;
#ifdef USB_HIF_TEST_INTERRUPT_IN
		*DLPipe = HIF_RX_INT_PIPE;
#endif
		break;
	case HTC_RAW_STREAMS_SVC:
		*ULPipe = HIF_TX_CTRL_PIPE;
		*DLPipe = HIF_RX_DATA_PIPE;
		break;
	case HTT_DATA_MSG_SVC:
		*ULPipe = HIF_TX_DATA_LP_PIPE;
		if (hif_usb_disable_rxdata2)
			*DLPipe = HIF_RX_DATA_PIPE;
		else
			*DLPipe = HIF_RX_DATA2_PIPE;
		break;
#ifdef QCA_TX_HTT2_SUPPORT
	case HTT_DATA2_MSG_SVC:
		*ULPipe = HIF_TX_DATA_HP_PIPE;
		if (hif_usb_disable_rxdata2)
			*DLPipe = HIF_RX_DATA_PIPE;
		else
			*DLPipe = HIF_RX_DATA2_PIPE;
		break;
#endif /* QCA_TX_HTT2_SUPPORT */
	default:
		status = A_ENOTSUP;
		break;
	}

	return status;
}
#endif

static A_STATUS HIFCtrlMsgExchange(HIF_DEVICE_USB *macp,
				   A_UINT8 SendReqVal,
				   A_UINT8 *pSendMessage,
				   A_UINT32 Length,
				   A_UINT8 ResponseReqVal,
				   A_UINT8 *pResponseMessage,
				   A_UINT32 *pResponseLength)
{
	A_STATUS status;

	do {

		/* send command */
		status = usb_hif_submit_ctrl_out(macp, SendReqVal, 0, 0,
						 pSendMessage, Length);

		if (A_FAILED(status))
			break;

		if (NULL == pResponseMessage) {
			/* no expected response */
			break;
		}

		/* get response */
		status = usb_hif_submit_ctrl_in(macp, ResponseReqVal, 0, 0,
						pResponseMessage,
						*pResponseLength);

		if (A_FAILED(status))
			break;

	} while (FALSE);

	return status;
}

A_STATUS HIFExchangeBMIMsg(HIF_DEVICE *device,
			   A_UINT8 *pSendMessage,
			   A_UINT32 Length,
			   A_UINT8 *pResponseMessage,
			   A_UINT32 *pResponseLength, A_UINT32 TimeoutMS)
{
	HIF_DEVICE_USB *macp = (HIF_DEVICE_USB *) device;

	return HIFCtrlMsgExchange(macp,
				  USB_CONTROL_REQ_SEND_BMI_CMD,
				  pSendMessage,
				  Length,
				  USB_CONTROL_REQ_RECV_BMI_RESP,
				  pResponseMessage, pResponseLength);
}

A_STATUS HIFConfigureDevice(HIF_DEVICE *hif, HIF_DEVICE_CONFIG_OPCODE opcode,
			    void *config, A_UINT32 configLen)
{
	A_STATUS status = A_OK;
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif;

	switch (opcode) {

	case HIF_DEVICE_GET_OS_DEVICE:
		{
			HIF_DEVICE_OS_DEVICE_INFO *info = config;
			info->pOSDevice = &device->udev->dev;
		}
		break;
	case HIF_DEVICE_GET_MBOX_BLOCK_SIZE:
		/* provide fake block sizes for mailboxes to satisfy upper layer
		 * software
		 */
		((A_UINT32 *) config)[0] = 16;
		((A_UINT32 *) config)[1] = 16;
		((A_UINT32 *) config)[2] = 16;
		((A_UINT32 *) config)[3] = 16;
		break;
	default:
		status = A_ENOTSUP;
		break;

	}

	return status;
}

A_STATUS hifWaitForPendingRecv(HIF_DEVICE *device)
{
	return A_OK;
}

A_STATUS HIFDiagReadAccess(HIF_DEVICE *hifDevice, A_UINT32 address,
			   A_UINT32 *data)
{
	HIF_DEVICE_USB *macp = (HIF_DEVICE_USB *) hifDevice;
	A_STATUS status;
	USB_CTRL_DIAG_CMD_READ *cmd;
	A_UINT32 respLength;

	cmd = (USB_CTRL_DIAG_CMD_READ *) macp->diag_cmd_buffer;

	A_MEMZERO(cmd, sizeof(*cmd));
	cmd->Cmd = USB_CTRL_DIAG_CC_READ;
	cmd->Address = address;
	respLength = sizeof(USB_CTRL_DIAG_RESP_READ);

	status = HIFCtrlMsgExchange(macp,
				    USB_CONTROL_REQ_DIAG_CMD,
				    (A_UINT8 *) cmd,
				    sizeof(*cmd),
				    USB_CONTROL_REQ_DIAG_RESP,
				    macp->diag_resp_buffer, &respLength);

	if (A_SUCCESS(status)) {
		USB_CTRL_DIAG_RESP_READ *pResp =
		    (USB_CTRL_DIAG_RESP_READ *) macp->diag_resp_buffer;
		*data = pResp->ReadValue;
	}

	return status;
}

A_STATUS HIFDiagWriteAccess(HIF_DEVICE *hifDevice, A_UINT32 address,
			    A_UINT32 data)
{
	HIF_DEVICE_USB *macp = (HIF_DEVICE_USB *) hifDevice;
	USB_CTRL_DIAG_CMD_WRITE *cmd;

	cmd = (USB_CTRL_DIAG_CMD_WRITE *) macp->diag_cmd_buffer;

	A_MEMZERO(cmd, sizeof(*cmd));
	cmd->Cmd = USB_CTRL_DIAG_CC_WRITE;
	cmd->Address = address;
	cmd->Value = data;

	return HIFCtrlMsgExchange(macp,
				  USB_CONTROL_REQ_DIAG_CMD,
				  (A_UINT8 *) cmd,
				  sizeof(*cmd), 0, NULL, 0);
}

void HIFShutDownDevice(HIF_DEVICE *hif)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	if (NULL == hif) {
		AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("full shutdown\n"));
		/* this is a full driver shutdown */
	} else {
		/* perform any actions to shutdown specific device */
		usb_hif_flush_all(device);
	}

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void HIFReleaseDevice(HIF_DEVICE *hif)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	device->claimed_context = NULL;
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void HIFClaimDevice(HIF_DEVICE *hif, void *claimedContext)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif;
	device->claimed_context = claimedContext;
}

A_STATUS HIFInit(OSDRV_CALLBACKS *callbacks)
{
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+HIFInit\n"));

	A_MEMZERO(&osDrvcallback, sizeof(osDrvcallback));

	A_REGISTER_MODULE_DEBUG_INFO(hif);

	osDrvcallback.deviceInsertedHandler = callbacks->deviceInsertedHandler;
	osDrvcallback.deviceRemovedHandler = callbacks->deviceRemovedHandler;
	osDrvcallback.deviceSuspendHandler = callbacks->deviceSuspendHandler;
	osDrvcallback.deviceResumeHandler = callbacks->deviceResumeHandler;
	osDrvcallback.deviceWakeupHandler = callbacks->deviceWakeupHandler;
	osDrvcallback.context = callbacks->context;
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-HIFInit\n"));
	return A_OK;
}

#ifdef ATH_BUS_PM
void HIFDeviceSuspend(HIF_DEVICE *dev)
{
	HIF_DEVICE_USB *macp = (HIF_DEVICE_USB *) dev;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	if (osDrvcallback.deviceSuspendHandler)
		osDrvcallback.deviceSuspendHandler(macp->claimed_context);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}

void HIFDeviceResume(HIF_DEVICE *dev)
{
	HIF_DEVICE_USB *macp = (HIF_DEVICE_USB *) dev;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	if (osDrvcallback.deviceResumeHandler)
		osDrvcallback.deviceResumeHandler(macp->claimed_context);
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
}
#endif

void HIFDumpInfo(HIF_DEVICE *hif)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif;
	HIF_USB_PIPE *pipe = NULL;
	struct usb_host_interface *iface_desc = NULL;
	struct usb_endpoint_descriptor *ep_desc;
	A_UINT8 i = 0;
	for (i = 0; i < HIF_USB_PIPE_MAX; i++) {
		pipe = &device->pipes[i];
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (
				 "PipeIndex : %d URB Cnt : %d PipeHandle : %x\n",
				 i, pipe->urb_cnt,
				 pipe->usb_pipe_handle));
		if (usb_pipeisoc(pipe->usb_pipe_handle))
			pr_info("Pipe Type ISOC\n");
		else if (usb_pipebulk(pipe->usb_pipe_handle))
			pr_info("Pipe Type BULK\n");
		else if (usb_pipeint(pipe->usb_pipe_handle))
			pr_info("Pipe Type INT\n");
		else if (usb_pipecontrol(pipe->usb_pipe_handle))
			pr_info("Pipe Type control\n");
	}

	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		ep_desc = &iface_desc->endpoint[i].desc;
		if (ep_desc) {
			pr_info(
			     "ep_desc : %p Index : %d: DescType : %d Addr : %d Maxp : %d Atrrib : %d\n",
			     ep_desc, i,
			     ep_desc->bDescriptorType,
			     ep_desc->bEndpointAddress, ep_desc->wMaxPacketSize,
			     ep_desc->bmAttributes);
			if ((ep_desc)
			    && (usb_endpoint_type(ep_desc) ==
				USB_ENDPOINT_XFER_ISOC)) {
				pr_info("ISOC EP Detected\n");
			}
		}
	}

}

void HIFDump(HIF_DEVICE *hif_device, u_int8_t cmd_id, bool start)
{
/* TO DO... */
}

void HIFFlushSurpriseRemove(HIF_DEVICE *hif_device)
{
/* TO DO... */
}

A_STATUS
HIFDiagReadMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data,
	       int nbytes)
{
	A_STATUS status = EOK;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	if ((address & 0x3) || ((uintptr_t)data & 0x3)) {
		return (-EIO);
	}

	while ((nbytes >= 4) &&
			(A_OK == (status = HIFDiagReadAccess(hif_device, address,
					   (A_UINT32*)data)))) {

		nbytes -= sizeof(A_UINT32);
		address += sizeof(A_UINT32);
		data   += sizeof(A_UINT32);

	}
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	return status;
}

A_STATUS
HIFDiagWriteMem(HIF_DEVICE *hif_device, A_UINT32 address, A_UINT8 *data, int nbytes)
{
	A_STATUS status = EOK;

	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));

	if ((address & 0x3) || ((uintptr_t)data & 0x3)) {
		return (-EIO);
	}

	while ((nbytes >= 4) &&
			(A_OK == (status = HIFDiagWriteAccess(hif_device, address,
					   *((A_UINT32*)data))))) {

		nbytes -= sizeof(A_UINT32);
		address += sizeof(A_UINT32);
		data   += sizeof(A_UINT32);

	}
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	return status;
}

void *hif_get_targetdef(HIF_DEVICE *hif_device)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif_device;
	struct hif_usb_softc *sc = device->sc;

	return sc->targetdef;
}

void HIFSendCompleteCheck(HIF_DEVICE *hif_device, a_uint8_t pipe, int force)
{
}

void HIFCancelDeferredTargetSleep(HIF_DEVICE *hif_device)
{
}

/* diagnostic command defnitions */
#define USB_CTRL_DIAG_CC_READ       0
#define USB_CTRL_DIAG_CC_WRITE      1
#define USB_CTRL_DIAG_CC_WARM_RESET 2
A_STATUS HIFDiagWriteWARMRESET(struct usb_interface *interface,
			       A_UINT32 address, A_UINT32 data)
{
	HIF_DEVICE_USB *hHIF_DEV;
	A_STATUS status = EOK;
	USB_CTRL_DIAG_CMD_WRITE *cmd;

	hHIF_DEV = (HIF_DEVICE_USB *) usb_get_intfdata(interface);
	cmd = (USB_CTRL_DIAG_CMD_WRITE *) hHIF_DEV->diag_cmd_buffer;
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+%s\n", __func__));
	A_MEMZERO(cmd, sizeof(USB_CTRL_DIAG_CMD_WRITE));
	cmd->Cmd = USB_CTRL_DIAG_CC_WARM_RESET;
	cmd->Address = address;
	cmd->Value = data;

	status = HIFCtrlMsgExchange(hHIF_DEV,
				    USB_CONTROL_REQ_DIAG_CMD,
				    (A_UINT8 *) cmd,
				    sizeof(USB_CTRL_DIAG_CMD_WRITE),
				    0, NULL, 0);
	if (A_FAILED(status)) {
		AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("-%s HIFCtrlMsgExchange fail\n",
						__func__));
	}
	AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-%s\n", __func__));
	return status;
}
void HIFsuspendwow(HIF_DEVICE *hif_device)
{
	printk(KERN_INFO "HIFsuspendwow TODO\n");
}

void HIFSetBundleMode(HIF_DEVICE *hif_device, bool enabled, int rx_bundle_cnt)
{
	HIF_DEVICE_USB *device = (HIF_DEVICE_USB *) hif_device;

	device->is_bundle_enabled = enabled;
	device->rx_bundle_cnt = rx_bundle_cnt;
	if (device->is_bundle_enabled && (device->rx_bundle_cnt == 0)) {
		device->rx_bundle_cnt = 1;
	}
	device->rx_bundle_buf_len = device->rx_bundle_cnt *
		                        HIF_USB_RX_BUNDLE_ONE_PKT_SIZE;

	AR_DEBUG_PRINTF(USB_HIF_DEBUG_BULK_IN,
			("athusb bundle %s cnt %d\n",
			 enabled ? "enabled" : "disabled",
			 rx_bundle_cnt));
}

/**
 * hif_is_80211_fw_wow_required() - API to check if target suspend is needed
 *
 * API determines if fw can be suspended and returns true/false to the caller.
 * Caller will call WMA WoW API's to suspend.
 * This API returns true only for SDIO bus types, for others it's a false.
 *
 * Return: bool
 */
bool hif_is_80211_fw_wow_required(void)
{
	return false;
}
