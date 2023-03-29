/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_twt.h
 *
 * WLAN Host Device Driver file for TWT (Target Wake Time) support.
 *
 */

#if !defined(WLAN_HDD_TWT_H)
#define WLAN_HDD_TWT_H

#include "qdf_types.h"
#include "qdf_status.h"
#include "qca_vendor.h"
#include <net/cfg80211.h>

struct hdd_context;
struct hdd_adapter;
struct wma_tgt_cfg;
struct wmi_twt_add_dialog_param;
struct wmi_twt_del_dialog_param;
struct wmi_twt_pause_dialog_cmd_param;
struct wmi_twt_resume_dialog_cmd_param;

extern const struct nla_policy
wlan_hdd_wifi_twt_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX + 1];

#define FEATURE_TWT_VENDOR_EVENTS                                   \
[QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX] = {                    \
	.vendor_id = QCA_NL80211_VENDOR_ID,                         \
	.subcmd = QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT,             \
},

/**
 * enum twt_role - TWT role definitions
 * @TWT_REQUESTOR: Individual/Bcast TWT requestor role
 * @TWT_REQUESTOR_INDV: Individual TWT requestor role
 * @TWT_REQUESTOR_BCAST: Broadcast TWT requestor role
 * @TWT_RESPONDER: Individual/Bcast TWT responder role
 * @TWT_RESPONDER_INDV: Individual TWT responder role
 * @TWT_RESPONDER_BCAST: Broadcast TWT responder role
 * @TWT_ROLE_ALL: All TWT roles
 */
enum twt_role {
	TWT_REQUESTOR,
	TWT_REQUESTOR_INDV,
	/* Bcast alone cannot be enabled, but can be disabled */
	TWT_REQUESTOR_BCAST,
	TWT_RESPONDER,
	TWT_RESPONDER_INDV,
	/* Bcast alone cannot be enabled, but can be disabled */
	TWT_RESPONDER_BCAST,
	TWT_ROLE_ALL,
	TWT_ROLE_MAX,
};

#ifdef WLAN_SUPPORT_TWT
/**
 * enum twt_status - TWT target state
 * @TWT_INIT: Init State
 * @TWT_DISABLED: TWT is disabled
 * @TWT_FW_TRIGGER_ENABLE_REQUESTED: FW triggered enable requested
 * @TWT_FW_TRIGGER_ENABLED: FW triggered twt enabled
 * @TWT_HOST_TRIGGER_ENABLE_REQUESTED: Host triggered TWT requested
 * @TWT_HOST_TRIGGER_ENABLED: Host triggered TWT enabled
 * @TWT_DISABLE_REQUESTED: TWT disable requested
 * @TWT_SUSPEND_REQUESTED: TWT suspend requested
 * @TWT_SUSPENDED: Successfully suspended TWT
 * @TWT_RESUME_REQUESTED: TWT Resume requested
 * @TWT_RESUMED: Successfully resumed TWT
 * @TWT_CLOSED: Deinitialized TWT feature and closed
 */
enum twt_status {
	TWT_INIT,
	TWT_DISABLED,
	TWT_FW_TRIGGER_ENABLE_REQUESTED,
	TWT_FW_TRIGGER_ENABLED,
	TWT_HOST_TRIGGER_ENABLE_REQUESTED,
	TWT_HOST_TRIGGER_ENABLED,
	TWT_DISABLE_REQUESTED,
	TWT_SUSPEND_REQUESTED,
	TWT_SUSPENDED,
	TWT_RESUME_REQUESTED,
	TWT_RESUMED,
	TWT_CLOSED,
};

/**
 * struct twt_conc_arg: TWT concurrency args
 * @ hdd_ctx: pointer to hdd context
 */
struct twt_conc_arg {
	struct hdd_context *hdd_ctx;
};

/**
 * twt_ack_info_priv - twt ack private info
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @dialog_id: dialog id
 * @twt_cmd_ack: twt ack command
 * @status: twt command status
 */
struct twt_ack_info_priv {
	uint32_t vdev_id;
	struct qdf_mac_addr peer_macaddr;
	uint32_t dialog_id;
	uint32_t twt_cmd_ack;
	uint32_t status;
};

/**
 * wlan_hdd_cfg80211_wifi_twt_config() - Wifi twt configuration
 * vendor command
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT
 *
 * Return: 0 for success, negative errno for failure.
 */
int wlan_hdd_cfg80211_wifi_twt_config(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len);

/**
 * hdd_update_tgt_twt_cap() - Update TWT target capabilities
 * @hdd_ctx: HDD Context
 * @cfg: Pointer to target configuration
 *
 * Return: None
 */
void hdd_update_tgt_twt_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg);

/**
 * hdd_send_twt_requestor_enable_cmd() - Send TWT requestor enable command to
 * target
 * @hdd_ctx: HDD Context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_send_twt_requestor_enable_cmd(struct hdd_context *hdd_ctx);

/**
 * hdd_send_twt_responder_enable_cmd() - Send TWT responder enable command to
 * target
 * @hdd_ctx: HDD Context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_send_twt_responder_enable_cmd(struct hdd_context *hdd_ctx);

/**
 * hdd_send_twt_requestor_disable_cmd() - Send TWT requestor disable command
 * to target
 * @hdd_ctx: HDD Context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_send_twt_requestor_disable_cmd(struct hdd_context *hdd_ctx);

/**
 * hdd_send_twt_responder_disable_cmd() - Send TWT responder disable command
 * to target
 * @hdd_ctx: HDD Context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_send_twt_responder_disable_cmd(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_twt_init() - Initialize TWT
 * @hdd_ctx: pointer to global HDD Context
 *
 * Initialize the TWT feature by registering the callbacks
 * with the lower layers.
 *
 * Return: None
 */
void wlan_hdd_twt_init(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_twt_deinit() - Deinitialize TWT
 * @hdd_ctx: pointer to global HDD Context
 *
 * Deinitialize the TWT feature by deregistering the
 * callbacks with the lower layers.
 *
 * Return: None
 */
void wlan_hdd_twt_deinit(struct hdd_context *hdd_ctx);

/**
 * hdd_test_config_twt_setup_session() - Process TWT setup
 * operation in the received test config vendor command and
 * send it to firmare
 * @adapter: adapter pointer
 * @tb: nl attributes
 *
 * Handles QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_SETUP
 *
 * Return: 0 for Success and negative value for failure
 */
int hdd_test_config_twt_setup_session(struct hdd_adapter *adapter,
				      struct nlattr **tb);

/**
 * hdd_test_config_twt_terminate_session() - Process TWT terminate
 * operation in the received test config vendor command and send
 * it to firmare
 * @adapter: adapter pointer
 * @tb: nl attributes
 *
 * Handles QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_TERMINATE
 *
 * Return: 0 for Success and negative value for failure
 */
int hdd_test_config_twt_terminate_session(struct hdd_adapter *adapter,
					  struct nlattr **tb);
/**
 * hdd_send_twt_role_disable_cmd() - Send a specific TWT role
 * disable to firmware
 * @hdd_ctx: hdd context pointer
 * @role : TWT role to be disabled
 *
 * Return: None
 */
void hdd_send_twt_role_disable_cmd(struct hdd_context *hdd_ctx,
				   enum twt_role role);

/**
 * hdd_send_twt_del_all_sessions_to_userspace() - Terminate all TWT sessions
 * @adapter: adapter
 *
 * This function checks if association exists and TWT session is setup,
 * then send the TWT teardown vendor NL event to the user space.
 *
 * Return: None
 */
void hdd_send_twt_del_all_sessions_to_userspace(struct hdd_adapter *adapter);

/**
 * hdd_twt_concurrency_update_on_scc_mcc() - Send TWT disable command to fw if
 * SCC/MCC exists in two vdevs
 * @pdev: pdev pointer
 * @object: object pointer
 * @arg: arg pointer
 *
 * Return: None
 */
void hdd_twt_concurrency_update_on_scc_mcc(struct wlan_objmgr_pdev *pdev,
					   void *object, void *arg);

/**
 * hdd_twt_concurrency_update_on_dbs() - Send TWT enable command to fw if DBS
 * exists in two vdevs
 * @pdev: pdev pointer
 * @object: object pointer
 * @arg: arg pointer
 *
 * Return: None
 */
void hdd_twt_concurrency_update_on_dbs(struct wlan_objmgr_pdev *pdev,
				       void *object, void *arg);

/**
 * __hdd_twt_update_work_handler() - TWT work handler to send TWT enable/disable
 * command to fw
 * @hdd_ctx: HDD context pointer
 *
 * Return: None
 */
void __hdd_twt_update_work_handler(struct hdd_context *hdd_ctx);

/**
 * hdd_twt_update_work_handler() - Wrapper function
 * @data: data pointer
 *
 * Return: None
 */
void hdd_twt_update_work_handler(void *data);

/**
 * wlan_twt_concurrency_update() - Handles twt concurrency in case of SCC/MCC
 * or DBS
 * @hdd_ctx: hdd context pointer
 *
 * Return: None
 */
void wlan_twt_concurrency_update(struct hdd_context *hdd_ctx);

/**
 * hdd_twt_del_dialog_in_ps_disable() - TWT teardown in case of ps disable
 * @hdd_ctx: hdd context pointer
 * @mac_addr: STA mac address
 * @vdev_id: vdev id
 *
 * Return: None
 */
void hdd_twt_del_dialog_in_ps_disable(struct hdd_context *hdd_ctx,
				      struct qdf_mac_addr *mac_addr,
				      uint8_t vdev_id);

#define FEATURE_VENDOR_SUBCMD_WIFI_CONFIG_TWT                            \
{                                                                        \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                         \
	.info.subcmd =                                                   \
		QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT,                    \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                            \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                           \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                           \
	.doit = wlan_hdd_cfg80211_wifi_twt_config,                       \
	vendor_command_policy(wlan_hdd_wifi_twt_config_policy,           \
			      QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX)       \
},

#else
static inline void hdd_update_tgt_twt_cap(struct hdd_context *hdd_ctx,
					  struct wma_tgt_cfg *cfg)
{
}

static inline
QDF_STATUS hdd_send_twt_requestor_enable_cmd(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS hdd_send_twt_responder_enable_cmd(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS hdd_send_twt_requestor_disable_cmd(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS hdd_send_twt_responder_disable_cmd(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void wlan_hdd_twt_init(struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_twt_deinit(struct hdd_context *hdd_ctx)
{
}

static inline
int hdd_test_config_twt_setup_session(struct hdd_adapter *adapter,
				      struct nlattr **tb)
{
	return -EINVAL;
}

static inline
int hdd_test_config_twt_terminate_session(struct hdd_adapter *adapter,
					  struct nlattr **tb)
{
	return -EINVAL;
}

static inline
void hdd_send_twt_role_disable_cmd(struct hdd_context *hdd_ctx,
				   enum twt_role role)
{
}

static inline
void hdd_send_twt_del_all_sessions_to_userspace(struct hdd_adapter *adapter)
{
}

static inline
void hdd_twt_concurrency_update_on_scc_mcc(struct wlan_objmgr_pdev *pdev,
					   void *object, void *arg)
{
}

static inline
void hdd_twt_concurrency_update_on_dbs(struct wlan_objmgr_pdev *pdev,
				       void *object, void *arg)
{
}

static inline
void __hdd_twt_update_work_handler(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_twt_update_work_handler(void *data)
{
}

static inline void wlan_twt_concurrency_update(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_twt_del_dialog_in_ps_disable(struct hdd_context *hdd_ctx,
				      struct qdf_mac_addr *mac_addr,
				      uint8_t vdev_id)
{
}

#define FEATURE_VENDOR_SUBCMD_WIFI_CONFIG_TWT

#endif

#endif /* if !defined(WLAN_HDD_TWT_H)*/
