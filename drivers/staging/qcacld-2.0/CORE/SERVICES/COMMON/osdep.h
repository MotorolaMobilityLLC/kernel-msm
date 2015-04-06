/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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


#ifndef _ATH_LINUX_OSDEP_H
#define _ATH_LINUX_OSDEP_H
#include "wlan_opts.h"

#ifdef ADF_SUPPORT
#include "osdep_adf.h"
#endif /* end of ADF_SUPPORT */

#ifdef HOST_OFFLOAD

void   atd_proxy_arp_send(struct sk_buff *nbuf);
void   atd_rx_from_wlan(struct sk_buff * nbuf);

static inline int _copy_to_user(void *dst ,void *src, int size)
{
    memcpy(dst,src,size);
    return 0;
}
static inline int _copy_from_user(void *dst ,void *src, int size)
{
    memcpy(dst,src,size);
    return 0;
}

#define _netif_carrier_on(dev)                                                \
                              do {                                            \
                                  union iwreq_data w;                         \
                                  netif_carrier_on(dev);                      \
                                  memset(&w, 0, sizeof(w));                   \
                                  w.data.flags = IEEE80211_EV_IF_RUNNING;     \
                                  wireless_send_event(dev,                    \
                                                     IWEVCUSTOM, &w, NULL);   \
                              } while (0)
#define _netif_carrier_off(dev)                                               \
                              do {                                            \
                                  union iwreq_data w;                         \
                                  netif_carrier_off(dev);                     \
                                  memset(&w, 0, sizeof(w));                   \
                                  w.data.flags = IEEE80211_EV_IF_NOT_RUNNING; \
                                  wireless_send_event(dev,                    \
                                                     IWEVCUSTOM, &w, NULL);   \
                              } while (0)

void
atd_event_handler(struct net_device     *dev, unsigned int  cmd,
			      union iwreq_data      *wreq, char *extra);

#define __osif_deliver_data(_osif, _skb) \
{\
    struct net_device *dev = ((osif_dev*)(_osif))->netdev;\
    _skb->dev = dev;\
    atd_rx_from_wlan(_skb);\
}

#undef wireless_send_event

#define wireless_send_event     atd_event_handler

#else

#define __osif_deliver_data(_osif, _skb) osif_deliver_data(_osif, _skb)
#define _copy_to_user     copy_to_user
#define _copy_from_user   copy_from_user
#define _netif_carrier_on     netif_carrier_on
#define _netif_carrier_off    netif_carrier_off

#endif

#ifndef OS_EXPORT_SYMBOL
#define OS_EXPORT_SYMBOL(_sym) EXPORT_SYMBOL(_sym)
#endif

#ifndef OS_WMB
#define OS_WMB() wmb()
#endif

#endif /* end of _ATH_LINUX_OSDEP_H */
