/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
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

#ifndef _WDI_EVENT_H_
#define _WDI_EVENT_H_

#include "athdefs.h"
#include "qdf_nbuf.h"
#include <cdp_txrx_stats_struct.h>

#define WDI_NO_VAL (-1)

struct wdi_event_rx_peer_invalid_msg {
	qdf_nbuf_t msdu;
	struct ieee80211_frame *wh;
	uint8_t vdev_id;
};

#define WDI_EVENT_NOTIFY_BASE   0x200
enum WDI_EVENT_NOTIFY {
	WDI_EVENT_SUB_DEALLOCATE = WDI_EVENT_NOTIFY_BASE,
	/* End of new notification types */

	WDI_EVENT_NOTIFY_LAST
};

/* Opaque event callback */
typedef void (*wdi_event_cb)(void *pdev, enum WDI_EVENT event, void *data,
					u_int16_t peer_id, uint32_t status);

/* Opaque event notify */
typedef void (*wdi_event_notify)(enum WDI_EVENT_NOTIFY notify,
				 enum WDI_EVENT event);

/**
 * @typedef wdi_event_subscribe
 * @brief Used by consumers to subscribe to WDI event notifications.
 * @details
 *  The event_subscribe struct includes pointers to other event_subscribe
 *  objects.  These pointers are simply to simplify the management of
 *  lists of event subscribers.  These pointers are set during the
 *  event_sub() function, and shall not be modified except by the
 *  WDI event management SW, until after the object's event subscription
 *  is canceled by calling event_unsub().
 */

typedef struct wdi_event_subscribe_t {
	/* subscriber event callback structure head */
	wdi_event_cb callback;
	/* subscriber object that processes the event callback */
	void *context;
	struct {
		/*
		 * private - the event subscriber SW shall not use this struct
		 */
		struct wdi_event_subscribe_t *next;
		struct wdi_event_subscribe_t *prev;
	} priv;
} wdi_event_subscribe;

#endif
