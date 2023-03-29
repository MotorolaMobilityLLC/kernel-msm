/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#include "ol_txrx_types.h"
#include "ol_txrx.h"

static inline wdi_event_subscribe *wdi_event_next_sub(wdi_event_subscribe *
						      wdi_sub)
{
	if (!wdi_sub) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid subscriber in %s\n", __func__);
		return NULL;
	}
	return wdi_sub->priv.next;
}

static inline void
wdi_event_del_subs(wdi_event_subscribe *wdi_sub, int event_index)
{
	wdi_event_notify deallocate_sub;

	while (wdi_sub) {
		wdi_event_subscribe *next = wdi_event_next_sub(wdi_sub);
		/*
		 *  Context is NULL for static allocation of subs
		 *  In dynamic allocation case notify the user
		 */
		if (wdi_sub->context) {
			deallocate_sub = wdi_sub->context;
			deallocate_sub(WDI_EVENT_SUB_DEALLOCATE,
				       WDI_EVENT_BASE + event_index);
		}
		wdi_sub = next;
	}
	/* qdf_mem_free(wdi_sub); */
}

static inline void
wdi_event_iter_sub(struct ol_txrx_pdev_t *pdev,
		   uint32_t event_index,
		   wdi_event_subscribe *wdi_sub, void *data)
{
	enum WDI_EVENT event = event_index + WDI_EVENT_BASE;

	if (wdi_sub) {
		do {
			wdi_sub->callback(pdev, event, data, 0, 0);
		} while ((wdi_sub = wdi_event_next_sub(wdi_sub)));
	}
}

void
wdi_event_handler(enum WDI_EVENT event,
		  uint8_t pdev_id, void *data)
{
	uint32_t event_index;
	wdi_event_subscribe *wdi_sub;
	struct ol_txrx_soc_t *soc = cds_get_context(QDF_MODULE_ID_SOC);
	ol_txrx_pdev_handle txrx_pdev;

	/*
	 * Input validation
	 */
	if (!event) {
		ol_txrx_err("Invalid WDI event");
		return;
	}
	if (!soc) {
		ol_txrx_err("Invalid soc");
		return;
	}

	txrx_pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	if (!txrx_pdev) {
		ol_txrx_err("Invalid pdev");
		return;
	}
	/*
	 *  There can be NULL data, so no validation for the data
	 *  Subscribers must do the sanity based on the requirements
	 */
	event_index = event - WDI_EVENT_BASE;

	wdi_sub = txrx_pdev->wdi_event_list[event_index];

	/* Find the subscriber */
	wdi_event_iter_sub(txrx_pdev, event_index, wdi_sub, data);
}

int
wdi_event_sub(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
	      wdi_event_subscribe *pevent_cb_sub, uint32_t event)
{
	uint32_t event_index;
	wdi_event_subscribe *wdi_sub;
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle txrx_pdev = ol_txrx_get_pdev_from_pdev_id(soc,
								      pdev_id);
	wdi_event_subscribe *event_cb_sub = pevent_cb_sub;

	/* Input validation */
	if (!txrx_pdev || !txrx_pdev->wdi_event_list) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid txrx_pdev or wdi_event_list in %s",
			  __func__);
		return -EINVAL;
	}
	if (!event_cb_sub) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid callback in %s", __func__);
		return -EINVAL;
	}
	if ((!event) || (event >= WDI_EVENT_LAST) || (event < WDI_EVENT_BASE)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid event in %s", __func__);
		return -EINVAL;
	}
	/* Input validation */
	event_index = event - WDI_EVENT_BASE;

	wdi_sub = txrx_pdev->wdi_event_list[event_index];
	/*
	 *  Check if it is the first subscriber of the event
	 */
	if (!wdi_sub) {
		wdi_sub = event_cb_sub;
		wdi_sub->priv.next = NULL;
		wdi_sub->priv.prev = NULL;
		txrx_pdev->wdi_event_list[event_index] = wdi_sub;
		return 0;
	}
	event_cb_sub->priv.next = wdi_sub;
	event_cb_sub->priv.prev = NULL;
	wdi_sub->priv.prev = event_cb_sub;
	txrx_pdev->wdi_event_list[event_index] = event_cb_sub;

	return 0;
}

int
wdi_event_unsub(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		wdi_event_subscribe *pevent_cb_sub, uint32_t event)
{
	uint32_t event_index = event - WDI_EVENT_BASE;
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle txrx_pdev = ol_txrx_get_pdev_from_pdev_id(soc,
								      pdev_id);

	wdi_event_subscribe *event_cb_sub = pevent_cb_sub;

	/* Input validation */
	if (!txrx_pdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid txrx_pdev in %s", __func__);
		return -EINVAL;
	}

	/* Input validation */
	if (!event_cb_sub) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid callback in %s", __func__);
		return -EINVAL;
	}
	if (!event_cb_sub->priv.prev) {
		txrx_pdev->wdi_event_list[event_index] =
			event_cb_sub->priv.next;
	} else {
		event_cb_sub->priv.prev->priv.next = event_cb_sub->priv.next;
	}
	if (event_cb_sub->priv.next)
		event_cb_sub->priv.next->priv.prev = event_cb_sub->priv.prev;

	/* qdf_mem_free(event_cb_sub); */

	return 0;
}

A_STATUS wdi_event_attach(struct ol_txrx_pdev_t *txrx_pdev)
{
	/* Input validation */
	if (!txrx_pdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid device in %s\nWDI event attach failed",
			  __func__);
		return A_ERROR;
	}
	/* Separate subscriber list for each event */
	txrx_pdev->wdi_event_list = (wdi_event_subscribe **)
				    qdf_mem_malloc(
					    sizeof(wdi_event_subscribe *) *
					    WDI_NUM_EVENTS);
	if (!txrx_pdev->wdi_event_list)
		return A_NO_MEMORY;

	return A_OK;
}

A_STATUS wdi_event_detach(struct ol_txrx_pdev_t *txrx_pdev)
{
	int i;
	wdi_event_subscribe *wdi_sub;

	if (!txrx_pdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "Invalid device in %s\nWDI detach failed",
			  __func__);
		return A_ERROR;
	}
	if (!txrx_pdev->wdi_event_list) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: wdi_event_list is NULL", __func__);
		return A_ERROR;
	}

	for (i = 0; i < WDI_NUM_EVENTS; i++) {
		wdi_sub = txrx_pdev->wdi_event_list[i];
		if (wdi_sub) {
			/* Delete all the subscribers */
			wdi_event_del_subs(wdi_sub, i);
		}
	}
	/* txrx_pdev->wdi_event_list would be non-null */
	qdf_mem_free(txrx_pdev->wdi_event_list);
	txrx_pdev->wdi_event_list = NULL;
	return A_OK;
}
