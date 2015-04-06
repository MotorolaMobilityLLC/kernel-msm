/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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

#ifndef _WDI_EVENT_API_H_
#define _WDI_EVENT_API_H_

#include "wdi_event.h"

struct ol_txrx_pdev_t;

/**
 * @brief Subscribe to a specified WDI event.
 * @details
 *  This function adds the provided wdi_event_subscribe object to a list of
 *  subscribers for the specified WDI event.
 *  When the event in question happens, each subscriber for the event will
 *  have their callback function invoked.
 *  The order in which callback functions from multiple subscribers are
 *  invoked is unspecified.
 *
 * @param pdev - the event physical device, that maintains the event lists
 * @param event_cb_sub - the callback and context for the event subscriber
 * @param event - which event's notifications are being subscribed to
 * @return error code, or A_OK for success
 */
A_STATUS wdi_event_sub(
    struct ol_txrx_pdev_t *txrx_pdev,
    wdi_event_subscribe *event_cb_sub,
    enum WDI_EVENT event);

/**
 * @brief Unsubscribe from a specified WDI event.
 * @details
 *  This function removes the provided event subscription object from the
 *  list of subscribers for its event.
 *  This function shall only be called if there was a successful prior call
 *  to event_sub() on the same wdi_event_subscribe object.
 *
 * @param pdev - the event physical device with the list of event subscribers
 * @param event_cb_sub - the event subscription object
 * @param event - which event is being unsubscribed
 * @return error code, or A_OK for success
 */
A_STATUS wdi_event_unsub(
    struct ol_txrx_pdev_t *txrx_pdev,
    wdi_event_subscribe *event_cb_sub,
    enum WDI_EVENT event);

#ifdef WDI_EVENT_ENABLE

void wdi_event_handler(enum WDI_EVENT event,
		       struct ol_txrx_pdev_t *txrx_pdev,
		       void *data);
A_STATUS wdi_event_attach(struct ol_txrx_pdev_t *txrx_pdev);
A_STATUS wdi_event_detach(struct ol_txrx_pdev_t *txrx_pdev);

#endif /* WDI_EVENT_ENABLE */

#endif /* _WDI_EVENT_API_H_ */
