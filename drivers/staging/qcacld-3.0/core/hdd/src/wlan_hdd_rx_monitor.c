/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

#include "wlan_hdd_includes.h"
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <cds_sched.h>
#include <cds_utils.h>
#include "wlan_hdd_rx_monitor.h"
#include "ol_txrx.h"
#include "cdp_txrx_mon.h"

void hdd_rx_monitor_callback(ol_osif_vdev_handle context,
				qdf_nbuf_t rxbuf,
				void *rx_status)
{
	struct hdd_adapter *adapter;
	int rxstat;
	struct sk_buff *skb;
	struct sk_buff *skb_next;
	unsigned int cpu_index;

	qdf_assert(context);
	qdf_assert(rxbuf);

	adapter = (struct hdd_adapter *)context;
	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			"invalid adapter %pK", adapter);
		return;
	}

	cpu_index = wlan_hdd_get_cpu();

	/* walk the chain until all are processed */
	skb = (struct sk_buff *)rxbuf;
	while (skb) {
		skb_next = skb->next;
		skb->dev = adapter->dev;

		++adapter->hdd_stats.tx_rx_stats.rx_packets[cpu_index];
		++adapter->stats.rx_packets;
		adapter->stats.rx_bytes += skb->len;

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(skb);

		/*
		 * If this is not a last packet on the chain
		 * Just put packet into backlog queue, not scheduling RX sirq
		 */
		if (skb->next) {
			rxstat = netif_rx(skb);
		} else {
			/*
			 * This is the last packet on the chain
			 * Scheduling rx sirq
			 */
			rxstat = netif_rx_ni(skb);
		}

		if (NET_RX_SUCCESS == rxstat)
			++adapter->
				hdd_stats.tx_rx_stats.rx_delivered[cpu_index];
		else
			++adapter->hdd_stats.tx_rx_stats.rx_refused[cpu_index];

		skb = skb_next;
	}
}

void hdd_monitor_set_rx_monitor_cb(struct ol_txrx_ops *txrx,
				ol_txrx_rx_mon_fp rx_monitor_cb)
{
	txrx->rx.mon = rx_monitor_cb;
}

int hdd_enable_monitor_mode(struct net_device *dev)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t vdev_id;

	hdd_enter_dev(dev);

	vdev_id = cdp_get_mon_vdev_from_pdev(soc, OL_TXRX_PDEV_ID);
	if (vdev_id < 0)
		return -EINVAL;

	return cdp_set_monitor_mode(soc, vdev_id, false);
}

int hdd_disable_monitor_mode(void)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	return cdp_reset_monitor_mode(soc, OL_TXRX_PDEV_ID, false);
}
