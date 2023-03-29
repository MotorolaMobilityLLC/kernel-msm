/*
 * Copyright (c) 2015-2018 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_LRO_H__
#define __WLAN_HDD_LRO_H__
/**
 * DOC: wlan_hdd_lro.h
 *
 * WLAN LRO interface module headers
 */

struct hdd_context;

#if defined(FEATURE_LRO)
/**
 * hdd_lro_rx() - Handle Rx procesing via LRO
 * @adapter: pointer to adapter context
 * @skb: pointer to sk_buff
 *
 * Return: QDF_STATUS_SUCCESS if processed via LRO or non zero return code
 */
QDF_STATUS hdd_lro_rx(struct hdd_adapter *adapter, struct sk_buff *skb);

void hdd_lro_display_stats(struct hdd_context *hdd_ctx);

/**
 * hdd_lro_set_reset() - vendor command for Disable/Enable LRO
 * @hdd_ctx: hdd context
 * @hdd_adapter_t: adapter
 * @enable_flag: enable or disable LRO.
 *
 * Return: none
 */
QDF_STATUS hdd_lro_set_reset(struct hdd_context *hdd_ctx,
			     struct hdd_adapter *adapter,
			     uint8_t enable_flag);

/**
 * hdd_is_lro_enabled() - Is LRO enabled
 * @hdd_ctx: HDD context
 *
 * This function checks if LRO is enabled in HDD context.
 *
 * Return: 0 - success, < 0 - failure
 */
int hdd_is_lro_enabled(struct hdd_context *hdd_ctx);

#else
static inline QDF_STATUS hdd_lro_rx(struct hdd_adapter *adapter,
				    struct sk_buff *skb)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void hdd_lro_display_stats(struct hdd_context *hdd_ctx)
{
}

static inline QDF_STATUS hdd_lro_set_reset(struct hdd_context *hdd_ctx,
					   struct hdd_adapter *adapter,
					   uint8_t enable_flag)
{
	return 0;
}

static inline int hdd_is_lro_enabled(struct hdd_context *hdd_ctx)
{
	return -EOPNOTSUPP;
}
#endif /* FEATURE_LRO */
#endif /* __WLAN_HDD_LRO_H__ */
