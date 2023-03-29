/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_hdd_lro.c
 *
 * WLAN HDD LRO interface implementation
 */

#include <wlan_hdd_includes.h>
#include <qdf_types.h>
#include <qdf_lro.h>
#include <wlan_hdd_lro.h>
#include <wlan_hdd_napi.h>
#include <wma_api.h>

#include <linux/inet_lro.h>
#include <linux/list.h>
#include <linux/random.h>
#include <net/tcp.h>

#define LRO_VALID_FIELDS \
	(LRO_DESC | LRO_ELIGIBILITY_CHECKED | LRO_TCP_ACK_NUM | \
	 LRO_TCP_DATA_CSUM | LRO_TCP_SEQ_NUM | LRO_TCP_WIN)

#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750)
static qdf_lro_ctx_t wlan_hdd_get_lro_ctx(struct sk_buff *skb)
{
	return (qdf_lro_ctx_t)QDF_NBUF_CB_RX_LRO_CTX(skb);
}
#else
static qdf_lro_ctx_t wlan_hdd_get_lro_ctx(struct sk_buff *skb)
{
	struct hif_opaque_softc *hif_hdl =
		(struct hif_opaque_softc *)cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_hdl) {
		hdd_err("hif_hdl is NULL");
		return NULL;
	}

	return hif_get_lro_info(QDF_NBUF_CB_RX_CTX_ID(skb), hif_hdl);
}
#endif

/**
 * hdd_lro_rx() - LRO receive function
 * @adapter: HDD adapter
 * @skb: network buffer
 *
 * Delivers LRO eligible frames to the LRO manager
 *
 * Return: QDF_STATUS_SUCCESS - frame delivered to LRO manager
 * QDF_STATUS_E_FAILURE - frame not delivered
 */
QDF_STATUS hdd_lro_rx(struct hdd_adapter *adapter, struct sk_buff *skb)
{
	qdf_lro_ctx_t ctx;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct qdf_lro_info info;
	struct net_lro_desc *lro_desc = NULL;

	if ((adapter->dev->features & NETIF_F_LRO) != NETIF_F_LRO)
		return QDF_STATUS_E_NOSUPPORT;

	ctx = wlan_hdd_get_lro_ctx(skb);
	if (!ctx) {
		hdd_err("LRO mgr is NULL");
		return status;
	}

	info.iph = skb->data;
	info.tcph = skb->data + QDF_NBUF_CB_RX_TCP_OFFSET(skb);
	ctx->lro_mgr->dev = adapter->dev;
	if (qdf_lro_get_info(ctx, skb, &info, (void **)&lro_desc)) {
		struct net_lro_info hdd_lro_info;

		hdd_lro_info.valid_fields = LRO_VALID_FIELDS;

		hdd_lro_info.lro_desc = lro_desc;
		hdd_lro_info.lro_eligible = 1;
		hdd_lro_info.tcp_ack_num = QDF_NBUF_CB_RX_TCP_ACK_NUM(skb);
		hdd_lro_info.tcp_data_csum =
			 csum_unfold(htons(QDF_NBUF_CB_RX_TCP_CHKSUM(skb)));
		hdd_lro_info.tcp_seq_num = QDF_NBUF_CB_RX_TCP_SEQ_NUM(skb);
		hdd_lro_info.tcp_win = QDF_NBUF_CB_RX_TCP_WIN(skb);

		lro_receive_skb_ext(ctx->lro_mgr, skb, (void *)adapter,
				    &hdd_lro_info);

		if (!hdd_lro_info.lro_desc->active)
			qdf_lro_desc_free(ctx, lro_desc);

		status = QDF_STATUS_SUCCESS;
	} else {
		qdf_lro_flush_pkt(ctx, &info);
	}
	return status;
}

/**
 * hdd_lro_display_stats() - display LRO statistics
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void hdd_lro_display_stats(struct hdd_context *hdd_ctx)
{
	hdd_debug("LRO stats is broken, will fix it");
}

QDF_STATUS
hdd_lro_set_reset(struct hdd_context *hdd_ctx, struct hdd_adapter *adapter,
			       uint8_t enable_flag)
{
	if ((hdd_ctx->ol_enable != CFG_LRO_ENABLED) ||
	    (adapter->device_mode != QDF_STA_MODE)) {
		hdd_debug("LRO is already Disabled");
		return 0;
	}

	if (enable_flag) {
		qdf_atomic_set(&hdd_ctx->vendor_disable_lro_flag, 0);
		adapter->dev->features |= NETIF_F_LRO;
	} else {
		/* Disable LRO, Enable tcpdelack*/
		qdf_atomic_set(&hdd_ctx->vendor_disable_lro_flag, 1);
		adapter->dev->features &= ~NETIF_F_LRO;
		hdd_debug("LRO Disabled");

		if (hdd_ctx->config->enable_tcp_delack) {
			struct wlan_rx_tp_data rx_tp_data;

			hdd_debug("Enable TCP delack as LRO is disabled");
			rx_tp_data.rx_tp_flags = TCP_DEL_ACK_IND;
			rx_tp_data.level = GET_CUR_RX_LVL(hdd_ctx);
			wlan_hdd_update_tcp_rx_param(hdd_ctx, &rx_tp_data);
			hdd_ctx->en_tcp_delack_no_lro = 1;
		}
	}
	return 0;
}

int hdd_is_lro_enabled(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->ol_enable != CFG_LRO_ENABLED)
		return -EOPNOTSUPP;

	return 0;
}
