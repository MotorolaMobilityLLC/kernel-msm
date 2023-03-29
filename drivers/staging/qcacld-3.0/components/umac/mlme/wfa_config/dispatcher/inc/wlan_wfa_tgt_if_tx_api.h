/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *  DOC: wlan_wfa_tgt_if_tx_api.h
 *
 *  This file contains connection manager tx ops related functions
 */

#ifndef WFA_TGT_IF_TX_API_H__
#define WFA_TGT_IF_TX_API_H__

#include "wlan_wfa_config_public_struct.h"

/**
 * wlan_send_wfatest_cmd() - Send WFA test command to firmware
 * @vdev: VDEV pointer
 * @wmi_wfatest:  wfa test commad pointer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_send_wfatest_cmd(struct wlan_objmgr_vdev *vdev,
		      struct set_wfatest_params *wmi_wfatest);

#endif /* WFA_TGT_IF_TX_API_H__ */
