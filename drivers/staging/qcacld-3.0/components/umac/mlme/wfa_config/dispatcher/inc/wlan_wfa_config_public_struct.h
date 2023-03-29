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
 * DOC: Declare structs and macros which can be accessed by various
 * components and modules.
 */

#ifndef _WLAN_WFA_CONFIG_PUBLIC_STRUCT_H_
#define _WLAN_WFA_CONFIG_PUBLIC_STRUCT_H_

#include <wlan_cmn.h>
#include <qdf_status.h>
#include <qdf_types.h>

/**
 * wlan_wfa_cmd_tx_ops - structure of tx function pointers for wfa test cmds
 * @send_wfa_test_cmd: TX ops function pointer to send WFA test command
 */
struct wlan_wfa_cmd_tx_ops {
	QDF_STATUS (*send_wfa_test_cmd)(struct wlan_objmgr_vdev *vdev,
					struct set_wfatest_params *wfa_test);
};

/**
 * struct wlan_mlme_wfa_cmd - WFA test command tx ops
 * @tx_ops: WFA test command Tx ops to send commands to firmware
 */
struct wlan_mlme_wfa_cmd {
	struct wlan_wfa_cmd_tx_ops tx_ops;
};

#endif /* _WLAN_WFA_CONFIG_PUBLIC_STRUCT_H_ */
