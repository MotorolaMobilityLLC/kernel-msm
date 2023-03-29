/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * DOC: Declare various api/struct which shall be used
 * by packet capture component for wmi cmd (tx path) and
 * event (rx) handling.
 */

#ifndef _TARGET_IF_PKT_CAPTURE_H_
#define _TARGET_IF_PKT_CAPTURE_H_

#include <wlan_pkt_capture_main.h>
#include <wlan_pkt_capture_ucfg_api.h>
#include <wlan_pkt_capture_mgmt_txrx.h>
#include <wlan_pkt_capture_public_structs.h>
#include <target_if.h>
#include <linux/ieee80211.h>

/**
 * target_if_pkt_capture_register_rx_ops() - Register packet capture RX ops
 * @rx_ops: packet capture component reception ops
 * Return: None
 */
void
target_if_pkt_capture_register_rx_ops(struct wlan_pkt_capture_rx_ops *rx_ops);

/**
 * target_if_pkt_capture_register_tx_ops() - Register packet capture TX ops
 * @tx_ops: pkt capture component transmit ops
 *
 * Return: None
 */
void target_if_pkt_capture_register_tx_ops(struct wlan_pkt_capture_tx_ops
					   *tx_ops);
#endif /* _TARGET_IF_PKT_CAPTURE_H_ */
