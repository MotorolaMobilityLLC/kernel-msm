/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#ifndef REMOVE_PKT_LOG
#include "adf_os_mem.h"
#include "athdefs.h"
#include "pktlog_ac_i.h"
#include "vos_api.h"
#include "wlan_qct_wda.h"

wdi_event_subscribe PKTLOG_TX_SUBSCRIBER;
wdi_event_subscribe PKTLOG_RX_SUBSCRIBER;
wdi_event_subscribe PKTLOG_RX_REMOTE_SUBSCRIBER;
wdi_event_subscribe PKTLOG_RCFIND_SUBSCRIBER;
wdi_event_subscribe PKTLOG_RCUPDATE_SUBSCRIBER;

struct ol_pl_arch_dep_funcs ol_pl_funcs = {
	.pktlog_init = pktlog_init,
	.pktlog_enable = pktlog_enable,
	.pktlog_setsize = pktlog_setsize,
	.pktlog_disable = pktlog_disable, /* valid for f/w disable */
};

struct ol_pktlog_dev_t ol_pl_dev = {
	.pl_funcs = &ol_pl_funcs,
};

void ol_pl_sethandle(ol_pktlog_dev_handle *pl_handle,
		     struct ol_softc *scn)
{
	ol_pl_dev.scn = (ol_ath_generic_softc_handle)scn;
	*pl_handle = &ol_pl_dev;
}

static A_STATUS pktlog_wma_post_msg(WMI_PKTLOG_EVENT event_types,
				    WMI_CMD_ID cmd_id)
{
	vos_msg_t msg = {0};
	VOS_STATUS status;
	struct ath_pktlog_wmi_params *param;

	param = vos_mem_malloc(sizeof(struct ath_pktlog_wmi_params));

	if (!param)
		return A_NO_MEMORY;

	param->cmd_id = cmd_id;
	param->pktlog_event = event_types;

	msg.type = WDA_PKTLOG_ENABLE_REQ;
	msg.bodyptr = param;
	msg.bodyval = 0;

	status = vos_mq_post_message(VOS_MQ_ID_WDA, &msg);

	if (status != VOS_STATUS_SUCCESS) {
		vos_mem_free(param);
		return A_ERROR;
	}

	return A_OK;
}

static inline A_STATUS
pktlog_enable_tgt(struct ol_softc *_scn, uint32_t log_state)
{
	uint32_t types = 0;

	if (log_state & ATH_PKTLOG_TX)
		types |= WMI_PKTLOG_EVENT_TX;

	if (log_state & ATH_PKTLOG_RX)
		types |= WMI_PKTLOG_EVENT_RX;

	if (log_state & ATH_PKTLOG_RCFIND)
		types |= WMI_PKTLOG_EVENT_RCF;

	if (log_state & ATH_PKTLOG_RCUPDATE)
		types |= WMI_PKTLOG_EVENT_RCU;

	return pktlog_wma_post_msg(types, WMI_PDEV_PKTLOG_ENABLE_CMDID);
}

static inline A_STATUS
wdi_pktlog_subscribe(struct ol_txrx_pdev_t *txrx_pdev, int32_t log_state)
{
	if (!txrx_pdev) {
		printk("Invalid pdev in %s\n", __func__);
		return A_ERROR;
	}
	if (log_state & ATH_PKTLOG_TX) {
		if (wdi_event_sub(txrx_pdev,
				  &PKTLOG_TX_SUBSCRIBER,
				  WDI_EVENT_TX_STATUS)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RX) {
		if (wdi_event_sub(txrx_pdev,
				  &PKTLOG_RX_SUBSCRIBER,
				  WDI_EVENT_RX_DESC)) {
			return A_ERROR;
		}
		if (wdi_event_sub(txrx_pdev,
				  &PKTLOG_RX_REMOTE_SUBSCRIBER,
				  WDI_EVENT_RX_DESC_REMOTE)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RCFIND) {
		if (wdi_event_sub(txrx_pdev,
				  &PKTLOG_RCFIND_SUBSCRIBER,
				  WDI_EVENT_RATE_FIND)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RCUPDATE) {
		if (wdi_event_sub(txrx_pdev,
				  &PKTLOG_RCUPDATE_SUBSCRIBER,
				  WDI_EVENT_RATE_UPDATE)) {
			return A_ERROR;
		}
	}
	return A_OK;
}

void
pktlog_callback(void *pdev, enum WDI_EVENT event, void *log_data)
{
	switch (event) {
	case WDI_EVENT_TX_STATUS:
	{
		/*
		 * process TX message
		 */
		if (process_tx_info(pdev, log_data)) {
			printk("Unable to process TX info\n");
			return;
		}
		break;
        }
	case WDI_EVENT_RX_DESC:
	{
		/*
		 * process RX message for local frames
		 */
		if (process_rx_info(pdev, log_data)) {
			printk("Unable to process RX info\n");
			return;
		}
		break;
	}
	case WDI_EVENT_RX_DESC_REMOTE:
	{
		/*
		 * process RX message for remote frames
		 */
		if (process_rx_info_remote(pdev, log_data)) {
			printk("Unable to process RX info\n");
			return;
		}
		break;
	}
	case WDI_EVENT_RATE_FIND:
	{
		/*
		 * process RATE_FIND message
		 */
		if (process_rate_find(pdev, log_data)) {
			printk("Unable to process RC_FIND info\n");
			return;
		}
		break;
	}
	case WDI_EVENT_RATE_UPDATE:
	{
		/*
		 * process RATE_UPDATE message
		 */
		if (process_rate_update(pdev, log_data)) {
			printk("Unable to process RC_UPDATE\n");
			return;
		}
		break;
	}
	default:
		break;
	}
}

static inline A_STATUS
wdi_pktlog_unsubscribe(struct ol_txrx_pdev_t *txrx_pdev, uint32_t log_state)
{
	if (log_state & ATH_PKTLOG_TX) {
		if (wdi_event_unsub(txrx_pdev,
				    &PKTLOG_TX_SUBSCRIBER,
				    WDI_EVENT_TX_STATUS)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RX) {
		if (wdi_event_unsub(txrx_pdev,
				    &PKTLOG_RX_SUBSCRIBER,
				    WDI_EVENT_RX_DESC)) {
			return A_ERROR;
		}
		if (wdi_event_unsub(txrx_pdev,
				    &PKTLOG_RX_REMOTE_SUBSCRIBER,
				    WDI_EVENT_RX_DESC_REMOTE)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RCFIND) {
		if (wdi_event_unsub(txrx_pdev,
				    &PKTLOG_RCFIND_SUBSCRIBER,
				    WDI_EVENT_RATE_FIND)) {
			return A_ERROR;
		}
	}
	if (log_state & ATH_PKTLOG_RCUPDATE) {
		if (wdi_event_unsub(txrx_pdev,
				    &PKTLOG_RCUPDATE_SUBSCRIBER,
				    WDI_EVENT_RATE_UPDATE)) {
			return A_ERROR;
		}
	}
	return A_OK;
}

int
pktlog_disable(struct ol_softc *scn)
{
	struct ol_pktlog_dev_t *pl_dev = scn->pdev_txrx_handle->pl_dev;
	struct ath_pktlog_info *pl_info = pl_dev->pl_info;
	struct ol_txrx_pdev_t *txrx_pdev = scn->pdev_txrx_handle;

	if (pktlog_wma_post_msg(0, WMI_PDEV_PKTLOG_DISABLE_CMDID)) {
		printk("Failed to disable pktlog in target\n");
		return -1;
	}

	if (wdi_pktlog_unsubscribe(txrx_pdev, pl_info->log_state)) {
		printk("Cannot unsubscribe pktlog from the WDI\n");
		return -1;
	}

	return 0;
}

void
pktlog_init(struct ol_softc *scn)
{
	struct ath_pktlog_info *pl_info;

	pl_info = scn->pdev_txrx_handle->pl_dev->pl_info;

	OS_MEMZERO(pl_info, sizeof(*pl_info));
	PKTLOG_LOCK_INIT(pl_info);

	pl_info->buf_size = PKTLOG_DEFAULT_BUFSIZE;
	pl_info->buf = NULL;
	pl_info->log_state = 0;
	pl_info->sack_thr = PKTLOG_DEFAULT_SACK_THR;
	pl_info->tail_length = PKTLOG_DEFAULT_TAIL_LENGTH;
	pl_info->thruput_thresh = PKTLOG_DEFAULT_THRUPUT_THRESH;
	pl_info->per_thresh = PKTLOG_DEFAULT_PER_THRESH;
	pl_info->phyerr_thresh = PKTLOG_DEFAULT_PHYERR_THRESH;
	pl_info->trigger_interval = PKTLOG_DEFAULT_TRIGGER_INTERVAL;
	pl_info->pktlen = 0;
	pl_info->start_time_thruput = 0;
	pl_info->start_time_per = 0;

	PKTLOG_TX_SUBSCRIBER.callback = pktlog_callback;
	PKTLOG_RX_SUBSCRIBER.callback = pktlog_callback;
	PKTLOG_RX_REMOTE_SUBSCRIBER.callback = pktlog_callback;
	PKTLOG_RCFIND_SUBSCRIBER.callback = pktlog_callback;
	PKTLOG_RCUPDATE_SUBSCRIBER.callback = pktlog_callback;
}

int
pktlog_enable(struct ol_softc *scn, int32_t log_state)
{
	struct ol_pktlog_dev_t *pl_dev;
	struct ath_pktlog_info *pl_info;
	struct ol_txrx_pdev_t *txrx_pdev;
	int error;

	if (!scn) {
		printk("%s: Invalid scn context\n", __func__);
		ASSERT(0);
		return -1;
	}

	txrx_pdev = scn->pdev_txrx_handle;
	if (!txrx_pdev) {
		printk("%s: Invalid txrx_pdev context\n", __func__);
		ASSERT(0);
		return -1;
	}

	pl_dev = scn->pdev_txrx_handle->pl_dev;
	if (!pl_dev) {
		printk("%s: Invalid pktlog context\n", __func__);
		ASSERT(0);
		return -1;
	}

	pl_info = pl_dev->pl_info;
	pl_dev->sc_osdev = scn->sc_osdev;

	if (!pl_info)
		return 0;

	if (log_state != 0 && !pl_dev->tgt_pktlog_enabled) {
		if (pl_info->buf == NULL) {
			error = pktlog_alloc_buf(scn);

			if (error != 0)
				return error;

			if (!pl_info->buf) {
				printk("%s: pktlog buf alloc failed\n", __func__);
				ASSERT(0);
				return -1;
			}
		}

		pl_info->buf->bufhdr.version = CUR_PKTLOG_VER;
		pl_info->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
		pl_info->buf->wr_offset = 0;
		pl_info->buf->rd_offset = -1;
		/* These below variables are used by per packet stats*/
		pl_info->buf->bytes_written = 0;
		pl_info->buf->msg_index = 1;
		pl_info->buf->offset = PKTLOG_READ_OFFSET;

		pl_info->start_time_thruput = OS_GET_TIMESTAMP();
		pl_info->start_time_per = pl_info->start_time_thruput;

		/* WDI subscribe */
		if (wdi_pktlog_subscribe(txrx_pdev, log_state)) {
			printk("Unable to subscribe to the WDI %s\n",
			       __func__);
			return -1;
		}
		/* WMI command to enable pktlog on the firmware */
		if (pktlog_enable_tgt(scn, log_state)) {
			printk("Device cannot be enabled, %s\n", __func__);
			return -1;
		} else {
			pl_dev->tgt_pktlog_enabled = true;
		}
	} else if (!log_state && pl_dev->tgt_pktlog_enabled) {
		pl_dev->pl_funcs->pktlog_disable(scn);
		pl_dev->tgt_pktlog_enabled = false;
		if (wdi_pktlog_unsubscribe(txrx_pdev, pl_info->log_state)) {
			printk("Cannot unsubscribe pktlog from the WDI\n");
			return -1;
		}
	}

	pl_info->log_state = log_state;
	return 0;
}

int
pktlog_setsize(struct ol_softc *scn, int32_t size)
{
	struct ol_pktlog_dev_t *pl_dev = scn->pdev_txrx_handle->pl_dev;
	struct ath_pktlog_info *pl_info = pl_dev->pl_info;

	if (size < 0)
		return -EINVAL;

	if (size == pl_info->buf_size)
		return 0;

	if (pl_info->log_state) {
		printk("Logging should be disabled before changing bufer size\n");
		return -EINVAL;
	}

	if (pl_info->buf != NULL)
		pktlog_release_buf(scn);

	if (size != 0)
		pl_info->buf_size = size;

	return 0;
}
#endif /* REMOVE_PKT_LOG */
