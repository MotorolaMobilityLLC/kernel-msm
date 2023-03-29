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
 * DOC : wlan_hdd_twt.c
 *
 * WLAN Host Device Driver file for TWT (Target Wake Time) support.
 *
 */

#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_twt_param.h"
#include "wlan_hdd_twt.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_cfg.h"
#include "wlan_hdd_hostapd.h"
#include "sme_api.h"
#include "wma_twt.h"
#include "osif_sync.h"
#include "wlan_osif_request_manager.h"
#include "cfg_ucfg_api.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_mlme_twt_ucfg_api.h>
#include <target_if.h>

#define TWT_DISABLE_COMPLETE_TIMEOUT 1000
#define TWT_ENABLE_COMPLETE_TIMEOUT  1000
#define TWT_ACK_COMPLETE_TIMEOUT 1000

#define TWT_FLOW_TYPE_ANNOUNCED 0
#define TWT_FLOW_TYPE_UNANNOUNCED 1

#define TWT_SETUP_WAKE_INTVL_MANTISSA_MAX       0xFFFF
#define TWT_SETUP_WAKE_DURATION_MAX             0xFFFF
#define TWT_SETUP_WAKE_INTVL_EXP_MAX            31
#define TWT_WAKE_INTVL_MULTIPLICATION_FACTOR    1024
#define TWT_WAKE_DURATION_MULTIPLICATION_FACTOR 256
#define TWT_MAX_NEXT_TWT_SIZE                   3

static const struct nla_policy
qca_wlan_vendor_twt_add_dialog_policy[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST] = {.type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_REQ_TYPE] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER] = {.type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION] = {.type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_DURATION] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_DURATION] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_INTVL] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_INTVL] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR] = VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_RECOMMENDATION] = {
							.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_PERSISTENCE] = {.type = NLA_U8 },
};

static const struct nla_policy
qca_wlan_vendor_twt_resume_dialog_policy[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_FLOW_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT_SIZE] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT2_TWT] = {.type = NLA_U32 },
};

static const struct nla_policy
qca_wlan_vendor_twt_stats_dialog_policy[QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID] = {.type = NLA_U8 },
};

const struct nla_policy
wlan_hdd_wifi_twt_config_policy[
	QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION] = {
			.type = NLA_U8},
		[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS] = {
			.type = NLA_NESTED},
};

static const struct nla_policy
qca_wlan_vendor_twt_nudge_dialog_policy[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_FLOW_ID] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_NEXT_TWT_SIZE] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAC_ADDR] = VENDOR_NLA_POLICY_MAC_ADDR,
};

static
int hdd_send_twt_del_dialog_cmd(struct hdd_context *hdd_ctx,
				struct wmi_twt_del_dialog_param *twt_params);

/**
 * hdd_twt_setup_req_type_to_cmd() - Converts twt setup request type to twt cmd
 * @req_type: twt setup request type
 * @twt_cmd: pointer to store twt command
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_setup_req_type_to_cmd(u8 req_type, enum WMI_HOST_TWT_COMMAND *twt_cmd)
{
	if (req_type == QCA_WLAN_VENDOR_TWT_SETUP_REQUEST) {
		*twt_cmd = WMI_HOST_TWT_COMMAND_REQUEST_TWT;
	} else if (req_type == QCA_WLAN_VENDOR_TWT_SETUP_SUGGEST) {
		*twt_cmd = WMI_HOST_TWT_COMMAND_SUGGEST_TWT;
	} else if (req_type == QCA_WLAN_VENDOR_TWT_SETUP_DEMAND) {
		*twt_cmd = WMI_HOST_TWT_COMMAND_DEMAND_TWT;
	} else {
		hdd_err_rl("Invalid TWT_SETUP_REQ_TYPE %d", req_type);
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_get_add_dialog_values() - Get TWT add dialog parameter
 * values from QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS
 * @tb: nl attributes
 * @params: wmi twt add dialog parameters
 *
 * Handles QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX
 *
 * Return: 0 or -EINVAL.
 */
static
int hdd_twt_get_add_dialog_values(struct nlattr **tb,
				  struct wmi_twt_add_dialog_param *params)
{
	uint32_t wake_intvl_exp, result;
	int cmd_id;
	QDF_STATUS qdf_status;

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (tb[cmd_id]) {
		params->dialog_id = nla_get_u8(tb[cmd_id]);
		if (params->dialog_id > TWT_MAX_DIALOG_ID) {
			hdd_err_rl("Flow id (%u) invalid", params->dialog_id);
			return -EINVAL;
		}
	} else {
		params->dialog_id = 0;
		hdd_debug("TWT_SETUP_FLOW_ID not specified. set to zero");
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP;
	if (!tb[cmd_id]) {
		hdd_err_rl("TWT_SETUP_WAKE_INTVL_EXP is must");
		return -EINVAL;
	}
	wake_intvl_exp = nla_get_u8(tb[cmd_id]);
	if (wake_intvl_exp > TWT_SETUP_WAKE_INTVL_EXP_MAX) {
		hdd_err_rl("Invalid wake_intvl_exp %u > %u",
			   wake_intvl_exp,
			   TWT_SETUP_WAKE_INTVL_EXP_MAX);
		return -EINVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST;
	params->flag_bcast = nla_get_flag(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID;
	if (tb[cmd_id]) {
		params->dialog_id = nla_get_u8(tb[cmd_id]);
		hdd_debug("TWT_SETUP_BCAST_ID %d", params->dialog_id);
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_RECOMMENDATION;
	if (tb[cmd_id]) {
		params->b_twt_recommendation = nla_get_u8(tb[cmd_id]);
		hdd_debug("TWT_SETUP_BCAST_RECOMM %d",
			  params->b_twt_recommendation);
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_PERSISTENCE;
	if (tb[cmd_id]) {
		params->b_twt_persistence = nla_get_u8(tb[cmd_id]);
		hdd_debug("TWT_SETUP_BCAST_PERSIS %d",
			  params->b_twt_persistence);
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_REQ_TYPE;
	if (!tb[cmd_id]) {
		hdd_err_rl("TWT_SETUP_REQ_TYPE is must");
		return -EINVAL;
	}
	qdf_status = hdd_twt_setup_req_type_to_cmd(nla_get_u8(tb[cmd_id]),
						   &params->twt_cmd);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return qdf_status_to_os_return(qdf_status);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER;
	params->flag_trigger = nla_get_flag(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE;
	if (!tb[cmd_id]) {
		hdd_err_rl("TWT_SETUP_FLOW_TYPE is must");
		return -EINVAL;
	}
	params->flag_flow_type = nla_get_u8(tb[cmd_id]);
	if (params->flag_flow_type != TWT_FLOW_TYPE_ANNOUNCED &&
	    params->flag_flow_type != TWT_FLOW_TYPE_UNANNOUNCED)
		return -EINVAL;

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION;
	params->flag_protection = nla_get_flag(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME;
	if (tb[cmd_id])
		params->sp_offset_us = nla_get_u32(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION;
	if (!tb[cmd_id]) {
		hdd_err_rl("TWT_SETUP_WAKE_DURATION is must");
		return -EINVAL;
	}
	params->wake_dura_us = TWT_WAKE_DURATION_MULTIPLICATION_FACTOR *
			       nla_get_u32(tb[cmd_id]);
	if (params->wake_dura_us > TWT_SETUP_WAKE_DURATION_MAX) {
		hdd_err_rl("Invalid wake_dura_us %u",
			   params->wake_dura_us);
		return -EINVAL;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_DURATION;
	if (tb[cmd_id])
		params->min_wake_dura_us = nla_get_u32(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_DURATION;
	if (tb[cmd_id])
		params->max_wake_dura_us = nla_get_u32(tb[cmd_id]);

	if (params->min_wake_dura_us > params->max_wake_dura_us) {
		hdd_err_rl("Invalid wake duration range min:%d max:%d. Reset to zero",
			   params->min_wake_dura_us, params->max_wake_dura_us);
		params->min_wake_dura_us = 0;
		params->max_wake_dura_us = 0;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA;
	if (!tb[cmd_id]) {
		hdd_err_rl("SETUP_WAKE_INTVL_MANTISSA is must");
		return -EINVAL;
	}
	params->wake_intvl_mantis = nla_get_u32(tb[cmd_id]);

	/*
	 * If mantissa in microsecond is present then take precedence over
	 * mantissa in TU. And send mantissa in microsecond to firmware.
	 */
	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA;
	if (tb[cmd_id]) {
		params->wake_intvl_mantis = nla_get_u32(tb[cmd_id]);
	}

	if (params->wake_intvl_mantis >
	    TWT_SETUP_WAKE_INTVL_MANTISSA_MAX) {
		hdd_err_rl("Invalid wake_intvl_mantis %u",
			   params->wake_intvl_mantis);
		return -EINVAL;
	}

	if (wake_intvl_exp && params->wake_intvl_mantis) {
		result = 2 << (wake_intvl_exp - 1);
		if (result >
		    (UINT_MAX / params->wake_intvl_mantis)) {
			hdd_err_rl("Invalid exp %d mantissa %d",
				   wake_intvl_exp,
				   params->wake_intvl_mantis);
			return -EINVAL;
		}
		params->wake_intvl_us =
			params->wake_intvl_mantis * result;
	} else {
		params->wake_intvl_us = params->wake_intvl_mantis;
	}

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_INTVL;
	if (tb[cmd_id])
		params->min_wake_intvl_us = nla_get_u32(tb[cmd_id]);

	cmd_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_INTVL;
	if (tb[cmd_id])
		params->max_wake_intvl_us = nla_get_u32(tb[cmd_id]);

	if (params->min_wake_intvl_us > params->max_wake_intvl_us) {
		hdd_err_rl("Invalid wake intvl range min:%d max:%d. Reset to zero",
			   params->min_wake_intvl_us,
			   params->max_wake_intvl_us);
		params->min_wake_dura_us = 0;
		params->max_wake_dura_us = 0;
	}

	hdd_debug("twt: dialog_id %d, vdev %d, wake intvl_us %d, min %d, max %d, mantis %d",
		  params->dialog_id, params->vdev_id, params->wake_intvl_us,
		  params->min_wake_intvl_us, params->max_wake_intvl_us,
		  params->wake_intvl_mantis);

	hdd_debug("twt: wake dura %d, min %d, max %d, sp_offset %d, cmd %d",
		  params->wake_dura_us, params->min_wake_dura_us,
		  params->max_wake_dura_us, params->sp_offset_us,
		  params->twt_cmd);
	hdd_debug("twt: bcast %d, trigger %d, flow_type %d, prot %d",
		  params->flag_bcast, params->flag_trigger,
		  params->flag_flow_type,
		  params->flag_protection);
	hdd_debug("twt: peer mac_addr "
		  QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(params->peer_macaddr));

	return 0;
}

int hdd_test_config_twt_setup_session(struct hdd_adapter *adapter,
				      struct nlattr **tb)
{
	struct nlattr *twt_session;
	int tmp, rc;
	struct hdd_station_ctx *hdd_sta_ctx = NULL;
	struct wmi_twt_add_dialog_param params = {0};
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	uint32_t congestion_timeout = 0;
	int ret = 0;
	int cmd_id;
	QDF_STATUS qdf_status;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE) {
		return -EOPNOTSUPP;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err_rl("Invalid state, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);
		return -EINVAL;
	}

	qdf_mem_copy(params.peer_macaddr, hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;

	cmd_id = QCA_WLAN_VENDOR_ATTR_WIFI_TEST_CONFIG_TWT_SETUP;
	nla_for_each_nested(twt_session, tb[cmd_id], tmp) {
		rc = wlan_cfg80211_nla_parse(tb2,
					     QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					     nla_data(twt_session),
					     nla_len(twt_session),
					     qca_wlan_vendor_twt_add_dialog_policy);
		if (rc) {
			hdd_err_rl("Invalid twt ATTR");
			return -EINVAL;
		}

		ret = hdd_twt_get_add_dialog_values(tb2, &params);
		if (ret)
			return ret;

		ucfg_mlme_get_twt_congestion_timeout(adapter->hdd_ctx->psoc,
						     &congestion_timeout);
		if (congestion_timeout) {
			ret = qdf_status_to_os_return(
			hdd_send_twt_requestor_disable_cmd(adapter->hdd_ctx));
			if (ret) {
				hdd_err("Failed to disable TWT");
				return ret;
			}

			ucfg_mlme_set_twt_congestion_timeout(adapter->hdd_ctx->psoc, 0);

			qdf_status = hdd_send_twt_requestor_enable_cmd(
							adapter->hdd_ctx);

			ret = qdf_status_to_os_return(qdf_status);
			if (ret) {
				hdd_err("Failed to Enable TWT");
				return ret;
			}
		}

		ret = qdf_status_to_os_return(sme_test_config_twt_setup(&params));
	}
	return ret;
}

int hdd_test_config_twt_terminate_session(struct hdd_adapter *adapter,
					  struct nlattr **tb)
{
	struct hdd_station_ctx *hdd_sta_ctx = NULL;
	struct wmi_twt_del_dialog_param params = {0};
	int ret_val;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE) {
		return -EOPNOTSUPP;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err_rl("Invalid state, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);
		return -EINVAL;
	}

	qdf_mem_copy(params.peer_macaddr,
		     hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;
	params.dialog_id = 0;
	hdd_debug("twt_terminate: vdev_id %d", params.vdev_id);

	ret_val = qdf_status_to_os_return(sme_test_config_twt_terminate(&params));
	return ret_val;
}

static
QDF_STATUS hdd_twt_check_all_twt_support(struct wlan_objmgr_psoc *psoc,
					 uint32_t dialog_id)
{
	bool is_all_twt_tgt_cap_enabled = false;
	QDF_STATUS status;

	/* Cap check is check NOT required if id is for a single session*/
	if (dialog_id != TWT_ALL_SESSIONS_DIALOG_ID)
		return QDF_STATUS_SUCCESS;

	status = ucfg_mlme_get_twt_all_twt_tgt_cap(
						psoc,
						&is_all_twt_tgt_cap_enabled);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (!is_all_twt_tgt_cap_enabled) {
		hdd_debug("All TWT sessions not supported by target");
		return QDF_STATUS_E_NOSUPPORT;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_get_params_resp_len() - Calculates the length
 * of twt get_params nl response
 *
 * Return: Length of get params nl response
 */
static uint32_t hdd_twt_get_params_resp_len(void)
{
	uint32_t len = nla_total_size(0);

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR */
	len += nla_total_size(QDF_MAC_ADDR_SIZE);

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_ANNOUNCE */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION */
	len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA */
	len += nla_total_size(sizeof(u32));

	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP*/
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF */
	len += nla_total_size(sizeof(u64));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATE */
	len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA */
	len += nla_total_size(sizeof(u32));

	return len;
}

/**
 * hdd_get_converted_twt_state() - Convert the internal twt state
 * to qca_wlan_twt_setup_state type.
 * @state: Internal TWT state to be converted.
 *
 * Return: qca_wlan_twt_setup_state type state
 */
static enum qca_wlan_twt_setup_state
hdd_get_converted_twt_state(enum wlan_twt_session_state state)
{
	switch (state) {
	case WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED:
		return QCA_WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED;
	case WLAN_TWT_SETUP_STATE_ACTIVE:
		return QCA_WLAN_TWT_SETUP_STATE_ACTIVE;
	case WLAN_TWT_SETUP_STATE_SUSPEND:
		return QCA_WLAN_TWT_SETUP_STATE_SUSPEND;
	default:
		return QCA_WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED;
	}
}

/**
 * hdd_twt_pack_get_params_resp_nlmsg()- Packs and sends twt get_params response
 * @psoc: Pointer to Global psoc
 * @reply_skb: pointer to response skb buffer
 * @params: Ponter to twt peer session parameters
 * @num_twt_session: total number of valid twt session
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_pack_get_params_resp_nlmsg(
				struct wlan_objmgr_psoc *psoc,
				struct sk_buff *reply_skb,
				struct wmi_host_twt_session_stats_info *params,
				int num_twt_session)
{
	struct nlattr *config_attr, *nla_params;
	enum wlan_twt_session_state state;
	enum qca_wlan_twt_setup_state converted_state;
	uint64_t tsf_val;
	uint32_t wake_duration;
	uint32_t wake_intvl_mantis_us, wake_intvl_mantis_tu;
	int i, attr;

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("TWT: get_params nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < num_twt_session; i++) {
		if (params[i].event_type != HOST_TWT_SESSION_SETUP &&
		    params[i].event_type != HOST_TWT_SESSION_UPDATE)
			continue;

		nla_params = nla_nest_start(reply_skb, i);
		if (!nla_params) {
			hdd_err("TWT: get_params nla_nest_start error");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;
		if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
			    params[i].peer_mac)) {
			hdd_err("TWT: get_params failed to put mac_addr");
			return QDF_STATUS_E_INVAL;
		}
		hdd_debug("TWT: get_params peer mac_addr " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(params[i].peer_mac));

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
		if (nla_put_u8(reply_skb, attr, params[i].dialog_id)) {
			hdd_err("TWT: get_params failed to put dialog_id");
			return QDF_STATUS_E_INVAL;
		}

		if (params[i].bcast) {
			attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST;
			if (nla_put_flag(reply_skb, attr)) {
				hdd_err("TWT: get_params fail to put bcast");
				return QDF_STATUS_E_INVAL;
			}
		}

		if (params[i].trig) {
			attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER;
			if (nla_put_flag(reply_skb, attr)) {
				hdd_err("TWT: get_params fail to put Trigger");
				return QDF_STATUS_E_INVAL;
			}
		}

		if (params[i].announ) {
			attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE;
			if (nla_put_flag(reply_skb, attr)) {
				hdd_err("TWT: get_params fail to put Announce");
				return QDF_STATUS_E_INVAL;
			}
		}

		if (params[i].protection) {
			attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION;
			if (nla_put_flag(reply_skb, attr)) {
				hdd_err("TWT: get_params fail to put Protect");
				return QDF_STATUS_E_INVAL;
			}
		}

		if (!params[i].info_frame_disabled) {
			attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED;
			if (nla_put_flag(reply_skb, attr)) {
				hdd_err("TWT: get_params put Info Enable fail");
				return QDF_STATUS_E_INVAL;
			}
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION;
		wake_duration = (params[i].wake_dura_us /
				 TWT_WAKE_DURATION_MULTIPLICATION_FACTOR);
		if (nla_put_u32(reply_skb, attr, wake_duration)) {
			hdd_err("TWT: get_params failed to put Wake duration");
			return QDF_STATUS_E_INVAL;
		}

		wake_intvl_mantis_us = params[i].wake_intvl_us;
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA;
		if (nla_put_u32(reply_skb, attr, wake_intvl_mantis_us)) {
			hdd_err("TWT: get_params failed to put Wake Interval in us");
			return QDF_STATUS_E_INVAL;
		}
		wake_intvl_mantis_tu = params[i].wake_intvl_us /
				       TWT_WAKE_INTVL_MULTIPLICATION_FACTOR;
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA;
		if (nla_put_u32(reply_skb, attr, wake_intvl_mantis_tu)) {
			hdd_err("TWT: get_params failed to put Wake Interval");
			return QDF_STATUS_E_INVAL;
		}

		hdd_debug("TWT: Send mantissa_us:%d, mantissa_tu:%d to userspace",
			   wake_intvl_mantis_us, wake_intvl_mantis_tu);

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP;
		if (nla_put_u8(reply_skb, attr, 0)) {
			hdd_err("TWT: get_params put Wake Interval Exp failed");
			return QDF_STATUS_E_INVAL;
		}

		tsf_val = ((uint64_t)params[i].sp_tsf_us_hi << 32) |
			   params[i].sp_tsf_us_lo;

		hdd_debug("TWT: get_params dialog_id %d TSF = 0x%llx",
			  params[i].dialog_id, tsf_val);

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF;
		if (hdd_wlan_nla_put_u64(reply_skb, attr, tsf_val)) {
			hdd_err("TWT: get_params failed to put TSF Value");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATE;
		state = ucfg_mlme_get_twt_session_state(
				psoc, (struct qdf_mac_addr *)params[i].peer_mac,
				params[i].dialog_id);
		converted_state = hdd_get_converted_twt_state(state);
		if (nla_put_u32(reply_skb, attr, converted_state)) {
			hdd_err("TWT: get_params failed to put TWT state");
			return QDF_STATUS_E_INVAL;
		}

		nla_nest_end(reply_skb, nla_params);
	}
	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_pack_get_params_resp()- Obtains twt session parameters of a peer
 * and sends response to the user space via nl layer
 * @hdd_ctx: hdd context
 * @params: Pointer to store twt peer session parameters
 * @num_twt_session: number of twt session
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_pack_get_params_resp(struct hdd_context *hdd_ctx,
			     struct wmi_host_twt_session_stats_info *params,
			     int num_twt_session)
{
	struct sk_buff *reply_skb;
	uint32_t skb_len = NLMSG_HDRLEN, i;
	QDF_STATUS qdf_status;

	/* Length of attribute QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS */
	skb_len += NLA_HDRLEN;

	/* Length of twt session parameters */
	for (i = 0; i < num_twt_session; i++) {
		if (params[i].event_type == HOST_TWT_SESSION_SETUP ||
		    params[i].event_type == HOST_TWT_SESSION_UPDATE)
			skb_len += hdd_twt_get_params_resp_len();
	}

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							     skb_len);
	if (!reply_skb) {
		hdd_err("TWT: get_params alloc reply skb failed");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_status = hdd_twt_pack_get_params_resp_nlmsg(hdd_ctx->psoc,
							reply_skb, params,
							num_twt_session);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		goto fail;

	if (cfg80211_vendor_cmd_reply(reply_skb))
		qdf_status = QDF_STATUS_E_INVAL;
fail:
	if (QDF_IS_STATUS_ERROR(qdf_status) && reply_skb)
		kfree_skb(reply_skb);

	return qdf_status;
}

static int hdd_is_twt_command_allowed(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *hdd_sta_ctx;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE)
		return -EOPNOTSUPP;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err_rl("Invalid state, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);
		return -EAGAIN;
	}

	if (hdd_is_roaming_in_progress(hdd_ctx))
		return -EBUSY;

	if (ucfg_scan_get_pdev_status(hdd_ctx->pdev)) {
		hdd_err_rl("Scan in progress");
		return -EBUSY;
	}

	return 0;
}

/**
 * hdd_send_inactive_session_reply  -  Send session state as inactive for
 * dialog ID for which setup is not done.
 * @params: TWT session parameters
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
hdd_send_inactive_session_reply(struct hdd_adapter *adapter,
				struct wmi_host_twt_session_stats_info *params)
{
	QDF_STATUS qdf_status;
	int num_twt_session = 0;

	params[num_twt_session].event_type = HOST_TWT_SESSION_UPDATE;
	num_twt_session++;

	qdf_status = hdd_twt_pack_get_params_resp(adapter->hdd_ctx, params,
						  num_twt_session);

	return qdf_status;
}

/**
 * hdd_twt_get_peer_session_params() - Obtains twt session parameters of a peer
 * and sends response to the user space
 * @hdd_ctx: hdd context
 * @params: Pointer to store twt peer session parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_get_peer_session_params(struct hdd_context *hdd_ctx,
				struct wmi_host_twt_session_stats_info *params)
{
	int num_twt_session = 0;
	QDF_STATUS qdf_status = QDF_STATUS_E_INVAL;

	if (!hdd_ctx || !params)
		return qdf_status;

	num_twt_session = ucfg_twt_get_peer_session_params(hdd_ctx->psoc,
							   params);
	if (num_twt_session)
		qdf_status = hdd_twt_pack_get_params_resp(hdd_ctx, params,
							  num_twt_session);

	return qdf_status;
}

/**
 * hdd_sap_twt_get_session_params() - Parses twt nl attrributes, obtains twt
 * session parameters based on dialog_id and returns to user via nl layer
 * @adapter: hdd_adapter
 * @twt_param_attr: twt nl attributes
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_sap_twt_get_session_params(struct hdd_adapter *adapter,
					  struct nlattr *twt_param_attr)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	int max_num_peer;
	struct wmi_host_twt_session_stats_info *params;
	int ret, id, id1;
	QDF_STATUS qdf_status = QDF_STATUS_E_INVAL;
	struct qdf_mac_addr mac_addr;
	bool is_associated;

	ret = wlan_cfg80211_nla_parse_nested(
					tb, QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					twt_param_attr,
					qca_wlan_vendor_twt_add_dialog_policy);
	if (ret)
		return ret;

	max_num_peer = hdd_ctx->wiphy->max_ap_assoc_sta;
	params = qdf_mem_malloc(TWT_PEER_MAX_SESSIONS * max_num_peer *
				sizeof(*params));

	if (!params)
		return -ENOMEM;

	params[0].vdev_id = adapter->vdev_id;
	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	id1 = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;

	if (tb[id] && tb[id1]) {
		params[0].dialog_id = nla_get_u8(tb[id]);
		nla_memcpy(params[0].peer_mac, tb[id1], QDF_MAC_ADDR_SIZE);
	} else {
		hdd_err_rl("TWT: get_params dialog_id or mac_addr is missing");
		goto done;
	}

	if (QDF_IS_ADDR_BROADCAST(params[0].peer_mac) &&
	    params[0].dialog_id != TWT_ALL_SESSIONS_DIALOG_ID) {
		hdd_err_rl("TWT: get_params dialog_is is invalid");
		goto done;
	}

	if (!params[0].dialog_id)
		params[0].dialog_id = TWT_ALL_SESSIONS_DIALOG_ID;

	qdf_mem_copy(mac_addr.bytes, params[0].peer_mac, QDF_MAC_ADDR_SIZE);

	if (!qdf_is_macaddr_broadcast(&mac_addr)) {
		is_associated = hdd_is_peer_associated(adapter, &mac_addr);
		if (!is_associated) {
			hdd_err("TWT: Association doesn't exist for STA: "
				   QDF_MAC_ADDR_FMT,
				   QDF_MAC_ADDR_REF(&mac_addr));
			goto done;
		}
	}

	hdd_debug("TWT: get_params dialog_id %d and mac_addr " QDF_MAC_ADDR_FMT,
		  params[0].dialog_id, QDF_MAC_ADDR_REF(params[0].peer_mac));

	qdf_status = hdd_twt_get_peer_session_params(adapter->hdd_ctx,
						     &params[0]);
done:
	qdf_mem_free(params);
	return qdf_status_to_os_return(qdf_status);
}

/**
 * hdd_sta_twt_get_session_params() - Parses twt nl attrributes, obtains twt
 * session parameters based on dialog_id and returns to user via nl layer
 * @adapter: hdd_adapter
 * @twt_param_attr: twt nl attributes
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_sta_twt_get_session_params(struct hdd_adapter *adapter,
					  struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	struct wmi_host_twt_session_stats_info
				params[TWT_PSOC_MAX_SESSIONS] = { {0} };
	int ret, id;
	QDF_STATUS qdf_status;
	struct qdf_mac_addr bcast_addr = QDF_MAC_ADDR_BCAST_INIT;

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					     twt_param_attr,
					     qca_wlan_vendor_twt_add_dialog_policy);
	if (ret)
		return ret;

	params[0].vdev_id = adapter->vdev_id;
	/*
	 * Currently twt_get_params nl cmd is sending only dialog_id(STA), fill
	 * mac_addr of STA in params and call hdd_twt_get_peer_session_params.
	 * When twt_get_params passes mac_addr and dialog_id of STA/SAP, update
	 * both mac_addr and dialog_id in params before calling
	 * hdd_twt_get_peer_session_params. dialog_id if not received,
	 * dialog_id of value 0 will be used as default.
	 */
	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (tb[id])
		params[0].dialog_id = (uint32_t)nla_get_u8(tb[id]);
	else
		params[0].dialog_id = 0;

	if (params[0].dialog_id <= TWT_MAX_DIALOG_ID) {
		qdf_mem_copy(params[0].peer_mac,
			     hdd_sta_ctx->conn_info.bssid.bytes,
			     QDF_MAC_ADDR_SIZE);
		hdd_debug("TWT: get_params peer mac_addr " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(params[0].peer_mac));
	} else {
		qdf_mem_copy(params[0].peer_mac, &bcast_addr,
			     QDF_MAC_ADDR_SIZE);
	}

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 params[0].dialog_id)) {
		hdd_debug("vdev%d: TWT session %d setup incomplete",
			  adapter->vdev_id, params[0].dialog_id);
		qdf_status = hdd_send_inactive_session_reply(adapter, params);

		return qdf_status_to_os_return(qdf_status);
	}

	hdd_debug("TWT: get_params dialog_id %d and mac_addr " QDF_MAC_ADDR_FMT,
		  params[0].dialog_id, QDF_MAC_ADDR_REF(params[0].peer_mac));

	qdf_status = hdd_twt_get_peer_session_params(adapter->hdd_ctx,
						     &params[0]);

	return qdf_status_to_os_return(qdf_status);
}

/**
 * hdd_twt_get_session_params() - Parses twt nl attrributes, obtains twt
 * session parameters based on dialog_id and returns to user via nl layer
 * @adapter: hdd_adapter
 * @twt_param_attr: twt nl attributes
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_get_session_params(struct hdd_adapter *adapter,
				      struct nlattr *twt_param_attr)
{
	enum QDF_OPMODE device_mode = adapter->device_mode;

	switch (device_mode) {
	case QDF_STA_MODE:
		return hdd_sta_twt_get_session_params(adapter, twt_param_attr);
	case QDF_SAP_MODE:
		return hdd_sap_twt_get_session_params(adapter, twt_param_attr);
	default:
		hdd_err_rl("TWT terminate is not supported on %s",
			   qdf_opmode_str(adapter->device_mode));
	}

	return -EOPNOTSUPP;
}

/**
 * hdd_get_twt_setup_event_len() - Calculates the length of twt
 * setup nl response
 * @additional_params_present: if true, then length required for
 * fixed and additional parameters is returned. if false,
 * then length required for fixed parameters is returned.
 *
 * Return: Length of twt setup nl response
 */
static
uint32_t hdd_get_twt_setup_event_len(bool additional_params_present)
{
	uint32_t len = 0;

	len += NLMSG_HDRLEN;

	/* Length of attribute QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS */
	len += NLA_HDRLEN;

	/* QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION */
	len += nla_total_size(sizeof(u8));

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID */
	len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS */
	len += nla_total_size(sizeof(u8));

	if (!additional_params_present)
		return len;

	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESP_TYPE */
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION*/
	len += nla_total_size(sizeof(u32));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA*/
	len += nla_total_size(sizeof(u32));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF*/
	len += nla_total_size(sizeof(u64));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME*/
	len += nla_total_size(sizeof(u32));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED*/
	len += nla_total_size(sizeof(u8));
	/*QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR*/
	len += nla_total_size(QDF_MAC_ADDR_SIZE);

	return len;
}

/**
 * wmi_twt_resume_status_to_vendor_twt_status() - convert from
 * WMI_HOST_RESUME_TWT_STATUS to qca_wlan_vendor_twt_status
 * @status: WMI_HOST_RESUME_TWT_STATUS value from firmware
 *
 * Return: qca_wlan_vendor_twt_status values corresponding
 * to the firmware failure status
 */
static int
wmi_twt_resume_status_to_vendor_twt_status(enum WMI_HOST_RESUME_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_RESUME_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_RESUME_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST;
	case WMI_HOST_RESUME_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	case WMI_HOST_RESUME_TWT_STATUS_DIALOG_ID_BUSY:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY;
	case WMI_HOST_RESUME_TWT_STATUS_NOT_PAUSED:
		return QCA_WLAN_VENDOR_TWT_STATUS_NOT_SUSPENDED;
	case WMI_HOST_RESUME_TWT_STATUS_NO_RESOURCE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE;
	case WMI_HOST_RESUME_TWT_STATUS_NO_ACK:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK;
	case WMI_HOST_RESUME_TWT_STATUS_UNKNOWN_ERROR:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	case WMI_HOST_RESUME_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS;
	case WMI_HOST_RESUME_TWT_STATUS_ROAM_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_ROAMING_IN_PROGRESS;
	case WMI_HOST_RESUME_TWT_STATUS_SCAN_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * wmi_twt_pause_status_to_vendor_twt_status() - convert from
 * WMI_HOST_PAUSE_TWT_STATUS to qca_wlan_vendor_twt_status
 * @status: WMI_HOST_PAUSE_TWT_STATUS value from firmware
 *
 * Return: qca_wlan_vendor_twt_status values corresponding
 * to the firmware failure status
 */
static int
wmi_twt_pause_status_to_vendor_twt_status(enum WMI_HOST_PAUSE_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_PAUSE_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_PAUSE_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST;
	case WMI_HOST_PAUSE_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	case WMI_HOST_PAUSE_TWT_STATUS_DIALOG_ID_BUSY:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY;
	case WMI_HOST_PAUSE_TWT_STATUS_ALREADY_PAUSED:
		return QCA_WLAN_VENDOR_TWT_STATUS_ALREADY_SUSPENDED;
	case WMI_HOST_PAUSE_TWT_STATUS_NO_RESOURCE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE;
	case WMI_HOST_PAUSE_TWT_STATUS_NO_ACK:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK;
	case WMI_HOST_PAUSE_TWT_STATUS_UNKNOWN_ERROR:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	case WMI_HOST_PAUSE_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS;
	case WMI_HOST_PAUSE_TWT_STATUS_ROAM_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * wmi_twt_nudge_status_to_vendor_twt_status() - convert from
 * WMI_HOST_NUDGE_TWT_STATUS to qca_wlan_vendor_twt_status
 * @status: WMI_HOST_NUDGE_TWT_STATUS value from firmware
 *
 * Return: qca_wlan_vendor_twt_status values corresponding
 * to the firmware failure status
 */
static int
wmi_twt_nudge_status_to_vendor_twt_status(enum WMI_HOST_NUDGE_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_NUDGE_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_NUDGE_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST;
	case WMI_HOST_NUDGE_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	case WMI_HOST_NUDGE_TWT_STATUS_DIALOG_ID_BUSY:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY;
	case WMI_HOST_NUDGE_TWT_STATUS_NO_RESOURCE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE;
	case WMI_HOST_NUDGE_TWT_STATUS_NO_ACK:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK;
	case WMI_HOST_NUDGE_TWT_STATUS_UNKNOWN_ERROR:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	case WMI_HOST_NUDGE_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * wmi_twt_add_cmd_to_vendor_twt_resp_type() - convert from
 * WMI_HOST_TWT_COMMAND to qca_wlan_vendor_twt_setup_resp_type
 * @status: WMI_HOST_TWT_COMMAND value from firmare
 *
 * Return: qca_wlan_vendor_twt_setup_resp_type values for valid
 * WMI_HOST_TWT_COMMAND value and -EINVAL for invalid value
 */
static
int wmi_twt_add_cmd_to_vendor_twt_resp_type(enum WMI_HOST_TWT_COMMAND type)
{
	switch (type) {
	case WMI_HOST_TWT_COMMAND_ACCEPT_TWT:
		return QCA_WLAN_VENDOR_TWT_RESP_ACCEPT;
	case WMI_HOST_TWT_COMMAND_ALTERNATE_TWT:
		return QCA_WLAN_VENDOR_TWT_RESP_ALTERNATE;
	case WMI_HOST_TWT_COMMAND_DICTATE_TWT:
		return QCA_WLAN_VENDOR_TWT_RESP_DICTATE;
	case WMI_HOST_TWT_COMMAND_REJECT_TWT:
		return QCA_WLAN_VENDOR_TWT_RESP_REJECT;
	default:
		return -EINVAL;
	}
}

/**
 * wmi_twt_del_status_to_vendor_twt_status() - convert from
 * WMI_HOST_DEL_TWT_STATUS to qca_wlan_vendor_twt_status
 * @status: WMI_HOST_DEL_TWT_STATUS value from firmare
 *
 * Return: qca_wlan_vendor_twt_status values corresponsing
 * to the firmware failure status
 */
static
int wmi_twt_del_status_to_vendor_twt_status(enum WMI_HOST_DEL_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_DEL_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST;
	case WMI_HOST_DEL_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_BUSY:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_BUSY;
	case WMI_HOST_DEL_TWT_STATUS_NO_RESOURCE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE;
	case WMI_HOST_DEL_TWT_STATUS_NO_ACK:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK;
	case WMI_HOST_DEL_TWT_STATUS_UNKNOWN_ERROR:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	case WMI_HOST_DEL_TWT_STATUS_PEER_INIT_TEARDOWN:
		return QCA_WLAN_VENDOR_TWT_STATUS_PEER_INITIATED_TERMINATE;
	case WMI_HOST_DEL_TWT_STATUS_ROAMING:
		return QCA_WLAN_VENDOR_TWT_STATUS_ROAM_INITIATED_TERMINATE;
	case WMI_HOST_DEL_TWT_STATUS_CONCURRENCY:
		return QCA_WLAN_VENDOR_TWT_STATUS_SCC_MCC_CONCURRENCY_TERMINATE;
	case WMI_HOST_DEL_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS;
	case WMI_HOST_DEL_TWT_STATUS_SCAN_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS;
	case WMI_HOST_DEL_TWT_STATUS_PS_DISABLE_TEARDOWN:
		return QCA_WLAN_VENDOR_TWT_STATUS_POWER_SAVE_EXIT_TERMINATE;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * wmi_twt_add_status_to_vendor_twt_status() - convert from
 * WMI_HOST_ADD_TWT_STATUS to qca_wlan_vendor_twt_status
 * @status: WMI_HOST_ADD_TWT_STATUS value from firmare
 *
 * Return: qca_wlan_vendor_twt_status values corresponding
 * to WMI_HOST_ADD_TWT_STATUS.
 */
static enum qca_wlan_vendor_twt_status
wmi_twt_add_status_to_vendor_twt_status(enum WMI_HOST_ADD_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_ADD_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_ADD_TWT_STATUS_TWT_NOT_ENABLED:
		return QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_ENABLED;
	case WMI_HOST_ADD_TWT_STATUS_USED_DIALOG_ID:
		return QCA_WLAN_VENDOR_TWT_STATUS_USED_DIALOG_ID;
	case WMI_HOST_ADD_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	case WMI_HOST_ADD_TWT_STATUS_NOT_READY:
		return QCA_WLAN_VENDOR_TWT_STATUS_NOT_READY;
	case WMI_HOST_ADD_TWT_STATUS_NO_RESOURCE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE;
	case WMI_HOST_ADD_TWT_STATUS_NO_ACK:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_ACK;
	case WMI_HOST_ADD_TWT_STATUS_NO_RESPONSE:
		return QCA_WLAN_VENDOR_TWT_STATUS_NO_RESPONSE;
	case WMI_HOST_ADD_TWT_STATUS_DENIED:
		return QCA_WLAN_VENDOR_TWT_STATUS_DENIED;
	case WMI_HOST_ADD_TWT_STATUS_UNKNOWN_ERROR:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	case WMI_HOST_ADD_TWT_STATUS_AP_PARAMS_NOT_IN_RANGE:
		return QCA_WLAN_VENDOR_TWT_STATUS_PARAMS_NOT_IN_RANGE;
	case WMI_HOST_ADD_TWT_STATUS_AP_IE_VALIDATION_FAILED:
		return QCA_WLAN_VENDOR_TWT_STATUS_IE_INVALID;
	case WMI_HOST_ADD_TWT_STATUS_ROAM_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_ROAMING_IN_PROGRESS;
	case WMI_HOST_ADD_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_CHANNEL_SWITCH_IN_PROGRESS;
	case WMI_HOST_ADD_TWT_STATUS_SCAN_IN_PROGRESS:
		return QCA_WLAN_VENDOR_TWT_STATUS_SCAN_IN_PROGRESS;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * hdd_twt_setup_pack_resp_nlmsg() - pack nlmsg response for setup
 * @reply_skb: pointer to the response skb structure
 * @event: twt event buffer with firmware response
 *
 * Pack the nl response with parameters and additional parameters
 * received from firmware.
 * Firmware sends additional parameters only for 2 conditions
 * 1) TWT Negotiation is accepted by AP - Firmware sends
 * QCA_WLAN_VENDOR_TWT_STATUS_OK with appropriate response type
 * in additional parameters
 * 2) AP has proposed Alternate values - In this case firmware sends
 * QCA_WLAN_VENDOR_TWT_STATUS_DENIED with appropriate response type
 * in additional parameters
 *
 * Return: QDF_STATUS_SUCCESS on Success, other QDF_STATUS error codes
 * on failure
 */
static QDF_STATUS
hdd_twt_setup_pack_resp_nlmsg(struct sk_buff *reply_skb,
			      struct twt_add_dialog_complete_event *event)
{
	struct nlattr *config_attr;
	uint64_t sp_offset_tsf;
	enum qca_wlan_vendor_twt_status vendor_status;
	int response_type, attr;
	uint32_t wake_duration;
	uint32_t wake_intvl_mantis_us, wake_intvl_mantis_tu;

	hdd_enter();

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_SET)) {
		hdd_err("Failed to put TWT operation");
		return QDF_STATUS_E_FAILURE;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	sp_offset_tsf = event->additional_params.sp_tsf_us_hi;
	sp_offset_tsf = (sp_offset_tsf << 32) |
			 event->additional_params.sp_tsf_us_lo;

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (nla_put_u8(reply_skb, attr, event->params.dialog_id)) {
		hdd_err("Failed to put dialog_id");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
	vendor_status = wmi_twt_add_status_to_vendor_twt_status(
							event->params.status);
	if (nla_put_u8(reply_skb, attr, vendor_status)) {
		hdd_err("Failed to put setup status");
		return QDF_STATUS_E_FAILURE;
	}

	if (event->params.num_additional_twt_params == 0) {
		nla_nest_end(reply_skb, config_attr);
		return QDF_STATUS_SUCCESS;
	}

	response_type = wmi_twt_add_cmd_to_vendor_twt_resp_type(
					event->additional_params.twt_cmd);
	if (response_type == -EINVAL) {
		hdd_err("Invalid response type from firmware");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESP_TYPE;
	if (nla_put_u8(reply_skb, attr, response_type)) {
		hdd_err("Failed to put setup response type");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE;
	if (nla_put_u8(reply_skb, attr, event->additional_params.announce)) {
		hdd_err("Failed to put setup flow type");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_debug("wake_dur_us %d", event->additional_params.wake_dur_us);
	wake_duration = (event->additional_params.wake_dur_us /
			 TWT_WAKE_DURATION_MULTIPLICATION_FACTOR);

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION;
	if (nla_put_u32(reply_skb, attr, wake_duration)) {
		hdd_err("Failed to put wake duration");
		return QDF_STATUS_E_FAILURE;
	}

	wake_intvl_mantis_us = event->additional_params.wake_intvl_us;
	if (nla_put_u32(reply_skb,
			QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA,
			wake_intvl_mantis_us)) {
		hdd_err("Failed to put wake interval mantissa in us");
		return QDF_STATUS_E_FAILURE;
	}

	wake_intvl_mantis_tu = (event->additional_params.wake_intvl_us /
				 TWT_WAKE_INTVL_MULTIPLICATION_FACTOR);

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA;
	if (nla_put_u32(reply_skb, attr, wake_intvl_mantis_tu)) {
		hdd_err("Failed to put wake interval mantissa in tu");
		return QDF_STATUS_E_FAILURE;
	}
	hdd_debug("Send mantissa_us:%d, mantissa_tu:%d to userspace",
		  wake_intvl_mantis_us, wake_intvl_mantis_tu);

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP;
	if (nla_put_u8(reply_skb, attr, 0)) {
		hdd_err("Failed to put wake interval exp");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME_TSF;
	if (wlan_cfg80211_nla_put_u64(reply_skb, attr, sp_offset_tsf)) {
		hdd_err("Failed to put sp_offset_tsf");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_TIME;
	if (nla_put_u32(reply_skb, attr,
			event->additional_params.sp_offset_us)) {
		hdd_err("Failed to put sp_offset_us");
		return QDF_STATUS_E_FAILURE;
	}

	if (event->additional_params.trig_en) {
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER;
		if (nla_put_flag(reply_skb, attr)) {
			hdd_err("Failed to put trig type");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (event->additional_params.protection) {
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION;
		if (nla_put_flag(reply_skb, attr)) {
			hdd_err("Failed to put protection flag");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (event->additional_params.bcast) {
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST;
		if (nla_put_flag(reply_skb, attr)) {
			hdd_err("Failed to put bcast flag");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (!event->additional_params.info_frame_disabled) {
		attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED;
		if (nla_put_flag(reply_skb, attr)) {
			hdd_err("Failed to put twt info enable flag");
			return QDF_STATUS_E_FAILURE;
		}
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;
	if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
		    event->params.peer_macaddr)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	nla_nest_end(reply_skb, config_attr);

	hdd_exit();

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_send_twt_setup_response  - Send TWT setup response to userspace
 * @hdd_adapter: Pointer to HDD adapter. This pointer is expeceted to
 * be validated by the caller.
 * @add_dialog_comp_ev_params: Add dialog completion event structure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS hdd_send_twt_setup_response(
		struct hdd_adapter *adapter,
		struct twt_add_dialog_complete_event *add_dialog_comp_ev_params)
{
	struct hdd_context *hdd_ctx;
	struct sk_buff *twt_vendor_event;
	struct wireless_dev *wdev = adapter->dev->ieee80211_ptr;
	size_t data_len;
	QDF_STATUS status;
	bool additional_params_present = false;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (add_dialog_comp_ev_params->params.num_additional_twt_params != 0)
		additional_params_present = true;

	data_len = hdd_get_twt_setup_event_len(additional_params_present);
	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
				hdd_ctx->wiphy, wdev, data_len,
				QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
				GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("TWT: Alloc setup resp skb fail");
		return QDF_STATUS_E_NOMEM;
	}

	status = hdd_twt_setup_pack_resp_nlmsg(twt_vendor_event,
					       add_dialog_comp_ev_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl add dialog response");
		wlan_cfg80211_vendor_free_skb(twt_vendor_event);
		return status;
	}

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	return status;
}

/**
 * hdd_twt_handle_renego_failure() - Upon re-nego failure send TWT teardown
 *
 * @adapter: Adapter pointer
 * @add_dialog_event: Pointer to Add dialog complete event structure
 *
 * Upon re-negotiation failure, this function constructs TWT teardown
 * message to the target.
 *
 * Return: None
 */
static void
hdd_twt_handle_renego_failure(struct hdd_adapter *adapter,
			      struct twt_add_dialog_complete_event *add_dialog_event)
{
	struct wmi_twt_del_dialog_param params = {0};

	if (!add_dialog_event)
		return;

	qdf_mem_copy(params.peer_macaddr,
		     add_dialog_event->params.peer_macaddr,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = add_dialog_event->params.vdev_id;
	params.dialog_id = add_dialog_event->params.dialog_id;

	hdd_debug("renego: twt_terminate: vdev_id:%d dialog_id:%d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params.vdev_id, params.dialog_id,
		  QDF_MAC_ADDR_REF(params.peer_macaddr));

	hdd_send_twt_del_dialog_cmd(adapter->hdd_ctx, &params);
}

/**
 * hdd_twt_ack_comp_cb() - TWT ack complete event callback
 * @params: TWT parameters
 * @context: Context
 *
 * Return: None
 */
static void
hdd_twt_ack_comp_cb(struct wmi_twt_ack_complete_event_param *params,
		    void *context)
{
	struct osif_request *request = NULL;
	struct twt_ack_info_priv *status_priv;

	request = osif_request_get(context);
	if (!request) {
		hdd_err("obsolete request");
		return;
	}

	status_priv = osif_request_priv(request);
	if (!status_priv) {
		hdd_err("obsolete status_priv");
		return;
	}

	if (status_priv->twt_cmd_ack == params->twt_cmd_ack) {
		status_priv->vdev_id = params->vdev_id;
		qdf_copy_macaddr(&status_priv->peer_macaddr,
				 &params->peer_macaddr);
		status_priv->dialog_id = params->dialog_id;
		status_priv->status = params->status;
		osif_request_complete(request);
	} else {
		hdd_err("Invalid ack for twt command");
	}

	osif_request_put(request);
}

/**
 * hdd_twt_ack_wait_response: TWT wait for ack event if it's supported
 * @hdd_ctx: HDD context
 * @request: OSIF request cookie
 * @twt_cmd: TWT command for which ack event come
 *
 * Return: None
 */
static QDF_STATUS
hdd_twt_ack_wait_response(struct hdd_context *hdd_ctx,
			  struct osif_request *request, int twt_cmd)
{
	struct target_psoc_info *tgt_hdl;
	struct twt_ack_info_priv *ack_priv;
	int ret = 0;
	bool twt_ack_cap;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(hdd_ctx->psoc);
	if (!tgt_hdl) {
		hdd_err("tgt_hdl is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	target_psoc_get_twt_ack_cap(tgt_hdl, &twt_ack_cap);

	if (!twt_ack_cap) {
		hdd_err("TWT ack bit is not supported. No need to wait");
		return QDF_STATUS_SUCCESS;
	}

	ack_priv = osif_request_priv(request);
	ack_priv->twt_cmd_ack = twt_cmd;

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("TWT setup response timed out");
		return QDF_STATUS_E_FAILURE;
	}

	ack_priv = osif_request_priv(request);
	hdd_debug("TWT ack info: vdev_id %d dialog_id %d twt_cmd %d status %d peer_macaddr "
		  QDF_MAC_ADDR_FMT, ack_priv->vdev_id, ack_priv->dialog_id,
		  ack_priv->twt_cmd_ack, ack_priv->status,
		  QDF_MAC_ADDR_REF(ack_priv->peer_macaddr.bytes));

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_add_dialog_comp_cb() - HDD callback for twt add dialog
 * complete event
 * @psoc: Pointer to global psoc
 * @add_dialog_event: Pointer to Add dialog complete event structure
 * @renego_fail: Flag to indicate if its re-negotiation failure case
 *
 * Return: None
 */
static void
hdd_twt_add_dialog_comp_cb(struct wlan_objmgr_psoc *psoc,
			   struct twt_add_dialog_complete_event *add_dialog_event,
			   bool renego_fail)
{
	struct hdd_adapter *adapter;
	uint8_t vdev_id = add_dialog_event->params.vdev_id;

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	hdd_debug("TWT: add dialog_id:%d, status:%d vdev_id:%d renego_fail:%d peer mac_addr "
		  QDF_MAC_ADDR_FMT, add_dialog_event->params.dialog_id,
		  add_dialog_event->params.status, vdev_id, renego_fail,
		  QDF_MAC_ADDR_REF(add_dialog_event->params.peer_macaddr));

	hdd_send_twt_setup_response(adapter, add_dialog_event);

	if (renego_fail)
		hdd_twt_handle_renego_failure(adapter, add_dialog_event);
}

/**
 * hdd_send_twt_add_dialog_cmd() - Send TWT add dialog command to target
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to Add dialog cmd params structure
 *
 * Return: 0 for Success and negative value for failure
 */
static
int hdd_send_twt_add_dialog_cmd(struct hdd_context *hdd_ctx,
				struct wmi_twt_add_dialog_param *twt_params)
{
	QDF_STATUS status;
	int ret = 0, twt_cmd;
	struct osif_request *request;
	struct twt_ack_info_priv *ack_priv;
	void *context;
	static const struct osif_request_params params = {
				.priv_size = sizeof(*ack_priv),
				.timeout_ms = TWT_ACK_COMPLETE_TIMEOUT,
	};

	hdd_enter();

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -EINVAL;
	}

	context = osif_request_cookie(request);

	status = sme_add_dialog_cmd(hdd_ctx->mac_handle,
				    hdd_twt_add_dialog_comp_cb,
				    twt_params, context);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send add dialog command");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	twt_cmd = WMI_HOST_TWT_ADD_DIALOG_CMDID;

	status = hdd_twt_ack_wait_response(hdd_ctx, request, twt_cmd);

	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ack_priv = osif_request_priv(request);
	if (ack_priv->status) {
		hdd_err("Received TWT ack error. Reset twt command");
		ucfg_mlme_reset_twt_active_cmd(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);
		 ucfg_mlme_init_twt_context(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);

		switch (ack_priv->status) {
		case WMI_HOST_ADD_TWT_STATUS_INVALID_PARAM:
		case WMI_HOST_ADD_TWT_STATUS_UNKNOWN_ERROR:
		case WMI_HOST_ADD_TWT_STATUS_USED_DIALOG_ID:
			ret = -EINVAL;
			break;
		case WMI_HOST_ADD_TWT_STATUS_ROAM_IN_PROGRESS:
		case WMI_HOST_ADD_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		case WMI_HOST_ADD_TWT_STATUS_SCAN_IN_PROGRESS:
			ret = -EBUSY;
			break;
		case WMI_HOST_ADD_TWT_STATUS_TWT_NOT_ENABLED:
			ret = -EOPNOTSUPP;
			break;
		case WMI_HOST_ADD_TWT_STATUS_NOT_READY:
			ret = -EAGAIN;
			break;
		case WMI_HOST_ADD_TWT_STATUS_NO_RESOURCE:
			ret = -ENOMEM;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	ret = qdf_status_to_os_return(status);
cleanup:
	osif_request_put(request);
	hdd_exit();

	return ret;
}

static bool hdd_twt_setup_conc_allowed(struct hdd_context *hdd_ctx,
				       uint8_t vdev_id)
{
	return policy_mgr_current_concurrency_is_mcc(hdd_ctx->psoc) ||
	       policy_mgr_is_scc_with_this_vdev_id(hdd_ctx->psoc, vdev_id);
}

/**
 * hdd_twt_setup_session() - Process TWT setup operation in the
 * received vendor command and send it to firmare
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_SET
 *
 * Return: 0 for Success and negative value for failure
 *
 *    If the setup request is received:
 *        before the host driver receiving the setup response event from
 *        firmware for the previous setup request, then return -EINPROGRESS
 *
 *        after the host driver received the setup response event from
 *        firmware for the previous setup request, then setup_done is
 *        set to true and this new setup request is sent to firmware
 *        for parameter re-negotiation.
 */
static int hdd_twt_setup_session(struct hdd_adapter *adapter,
				 struct nlattr *twt_param_attr)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *hdd_sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct wmi_twt_add_dialog_param params = {0};
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	uint32_t congestion_timeout = 0;
	int ret = 0;

	ret = hdd_is_twt_command_allowed(adapter);
	if (ret)
		return ret;

	if (hdd_twt_setup_conc_allowed(hdd_ctx, adapter->vdev_id)) {
		hdd_err_rl("TWT setup reject: SCC or MCC concurrency exists");
		return -EAGAIN;
	}

	qdf_mem_copy(params.peer_macaddr,
		     hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb2,
					     QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					     twt_param_attr,
					     qca_wlan_vendor_twt_add_dialog_policy);
	if (ret)
		return ret;

	if (!ucfg_mlme_get_twt_peer_responder_capabilities(
					adapter->hdd_ctx->psoc,
					&hdd_sta_ctx->conn_info.bssid)) {
		hdd_err_rl("TWT setup reject: TWT responder not supported");
		return -EOPNOTSUPP;
	}

	ret = hdd_twt_get_add_dialog_values(tb2, &params);
	if (ret)
		return ret;

	ucfg_mlme_get_twt_congestion_timeout(adapter->hdd_ctx->psoc,
					     &congestion_timeout);

	if (congestion_timeout) {
		ret = qdf_status_to_os_return(
			hdd_send_twt_requestor_disable_cmd(adapter->hdd_ctx));
		if (ret) {
			hdd_err("Failed to disable TWT");
			return ret;
		}

		ucfg_mlme_set_twt_congestion_timeout(adapter->hdd_ctx->psoc, 0);

		ret = qdf_status_to_os_return(
			hdd_send_twt_requestor_enable_cmd(adapter->hdd_ctx));
		if (ret) {
			hdd_err("Failed to Enable TWT");
			return ret;
		}
	}

	if (ucfg_mlme_is_max_twt_sessions_reached(adapter->hdd_ctx->psoc,
					       &hdd_sta_ctx->conn_info.bssid,
						params.dialog_id)) {
		hdd_err_rl("TWT add failed(dialog_id:%d), another TWT already exists (max reached)",
			   params.dialog_id);
		return -EAGAIN;
	}

	if (ucfg_mlme_is_twt_setup_in_progress(adapter->hdd_ctx->psoc,
					       &hdd_sta_ctx->conn_info.bssid,
					params.dialog_id)) {
		hdd_err_rl("TWT setup is in progress for dialog_id:%d",
			   params.dialog_id);
		return -EINPROGRESS;
	}

	ret = hdd_send_twt_add_dialog_cmd(adapter->hdd_ctx, &params);
	if (ret < 0)
		return ret;

	return ret;
}

/**
 * hdd_get_twt_get_stats_event_len() - calculate length of skb
 * required for sending twt get statistics command responses.
 *
 * Return: length of skb
 */
static uint32_t hdd_get_twt_get_stats_event_len(void)
{
	uint32_t len = 0;

	len += NLMSG_HDRLEN;
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID */
	len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS */
	len += nla_total_size(sizeof(u8));

	return len;
}

/**
 * hdd_get_twt_event_len() - calculate length of skb
 * required for sending twt terminate, pause and resume
 * command responses.
 *
 * Return: length of skb
 */
static uint32_t hdd_get_twt_event_len(void)
{
	uint32_t len = 0;

	len += NLMSG_HDRLEN;
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID */
	len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS */
	len += nla_total_size(sizeof(u8));
	/* QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR*/
	len += nla_total_size(QDF_MAC_ADDR_SIZE);

	return len;
}

/**
 * hdd_twt_terminate_pack_resp_nlmsg() - pack the skb with
 * firmware response for twt terminate command
 * @reply_skb: skb to store the response
 * @params: Pointer to del dialog complete event buffer
 *
 * Return: QDF_STATUS_SUCCESS on Success, QDF_STATUS_E_FAILURE
 * on failure
 */
static QDF_STATUS
hdd_twt_terminate_pack_resp_nlmsg(struct sk_buff *reply_skb,
				  struct wmi_twt_del_dialog_complete_event_param *params)
{
	struct nlattr *config_attr;
	int vendor_status, attr;

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_TERMINATE)) {
		hdd_err("Failed to put TWT operation");
		return QDF_STATUS_E_FAILURE;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (nla_put_u8(reply_skb, attr, params->dialog_id)) {
		hdd_debug("Failed to put dialog_id");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
	vendor_status = wmi_twt_del_status_to_vendor_twt_status(params->status);
	if (nla_put_u8(reply_skb, attr, vendor_status)) {
		hdd_err("Failed to put QCA_WLAN_TWT_TERMINATE");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;
	if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
		    params->peer_macaddr)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_del_dialog_comp_cb() - callback function
 * to get twt terminate command complete event
 * @psoc: Pointer to global psoc
 * @params: Pointer to del dialog complete event buffer
 *
 * Return: None
 */
static void
hdd_twt_del_dialog_comp_cb(struct wlan_objmgr_psoc *psoc,
			   struct wmi_twt_del_dialog_complete_event_param *params)
{
	struct hdd_adapter *adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, params->vdev_id);
	struct wireless_dev *wdev;
	struct hdd_context *hdd_ctx;
	struct sk_buff *twt_vendor_event;
	size_t data_len;
	QDF_STATUS status;

	hdd_enter();

	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx || cds_is_load_or_unload_in_progress())
		return;

	wdev = adapter->dev->ieee80211_ptr;

	data_len = hdd_get_twt_event_len() + nla_total_size(sizeof(u8));
	data_len += NLA_HDRLEN;
	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
				hdd_ctx->wiphy, wdev, data_len,
				QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
				GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("Del dialog skb alloc failed");
		return;
	}

	hdd_debug("del dialog_id:%d, status:%d vdev_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params->dialog_id,
		  params->status, params->vdev_id,
		  QDF_MAC_ADDR_REF(params->peer_macaddr));

	status = hdd_twt_terminate_pack_resp_nlmsg(twt_vendor_event, params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_cfg80211_vendor_free_skb(twt_vendor_event);
		return;
	}

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	hdd_exit();

	return;
}

void
hdd_send_twt_del_all_sessions_to_userspace(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_psoc *psoc = adapter->hdd_ctx->psoc;
	struct hdd_station_ctx *hdd_sta_ctx = NULL;
	struct wmi_twt_del_dialog_complete_event_param params;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_debug("Not associated, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);
		return;
	}

	if (!ucfg_mlme_is_twt_setup_done(psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 WLAN_ALL_SESSIONS_DIALOG_ID)) {
		hdd_debug("No active TWT sessions, vdev_id: %d dialog_id: %d",
			  adapter->vdev_id, WLAN_ALL_SESSIONS_DIALOG_ID);
		return;
	}

	qdf_mem_zero(&params, sizeof(params));
	params.vdev_id = adapter->vdev_id;
	params.dialog_id = WLAN_ALL_SESSIONS_DIALOG_ID;
	params.status = WMI_HOST_DEL_TWT_STATUS_UNKNOWN_ERROR;
	qdf_mem_copy(params.peer_macaddr, hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);

	hdd_twt_del_dialog_comp_cb(psoc, &params);
}

/**
 * hdd_send_twt_del_dialog_cmd() - Send TWT del dialog command to target
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to del dialog cmd params structure
 *
 * Return: 0 on success, negative value on failure
 */
static
int hdd_send_twt_del_dialog_cmd(struct hdd_context *hdd_ctx,
				struct wmi_twt_del_dialog_param *twt_params)
{
	QDF_STATUS status;
	int ret = 0, twt_cmd;
	struct osif_request *request;
	struct twt_ack_info_priv *ack_priv;
	void *context;
	static const struct osif_request_params params = {
				.priv_size = sizeof(*ack_priv),
				.timeout_ms = TWT_ACK_COMPLETE_TIMEOUT,
	};

	hdd_enter();

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -EINVAL;
	}

	context = osif_request_cookie(request);

	status = sme_del_dialog_cmd(hdd_ctx->mac_handle,
				    hdd_twt_del_dialog_comp_cb,
				    twt_params, context);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send del dialog command");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	twt_cmd = WMI_HOST_TWT_DEL_DIALOG_CMDID;

	status = hdd_twt_ack_wait_response(hdd_ctx, request, twt_cmd);

	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ack_priv = osif_request_priv(request);
	if (ack_priv->status) {
		hdd_err("Received TWT ack error. Reset twt command");
		ucfg_mlme_reset_twt_active_cmd(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);

		switch (ack_priv->status) {
		case WMI_HOST_DEL_TWT_STATUS_INVALID_PARAM:
		case WMI_HOST_DEL_TWT_STATUS_UNKNOWN_ERROR:
			ret = -EINVAL;
			break;
		case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_NOT_EXIST:
			ret = -EAGAIN;
			break;
		case WMI_HOST_DEL_TWT_STATUS_DIALOG_ID_BUSY:
			ret = -EINPROGRESS;
			break;
		case WMI_HOST_DEL_TWT_STATUS_NO_RESOURCE:
			ret = -ENOMEM;
			break;
		case WMI_HOST_DEL_TWT_STATUS_ROAMING:
		case WMI_HOST_DEL_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		case WMI_HOST_DEL_TWT_STATUS_SCAN_IN_PROGRESS:
			ret = -EBUSY;
			break;
		case WMI_HOST_DEL_TWT_STATUS_CONCURRENCY:
			ret = -EAGAIN;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	ret = qdf_status_to_os_return(status);
cleanup:
	osif_request_put(request);
	hdd_exit();

	return ret;
}

/**
 * hdd_send_sap_twt_del_dialog_cmd() - Send SAP TWT del dialog command
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to del dialog cmd params structure
 *
 * Return: 0 on success, negative value on failure
 */
static
int hdd_send_sap_twt_del_dialog_cmd(struct hdd_context *hdd_ctx,
				    struct wmi_twt_del_dialog_param *twt_params)
{
	QDF_STATUS status;
	int ret = 0;

	status = sme_sap_del_dialog_cmd(hdd_ctx->mac_handle,
				    hdd_twt_del_dialog_comp_cb,
				    twt_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send del dialog command");
		ret = qdf_status_to_os_return(status);
	}

	return ret;
}


static int hdd_sap_twt_terminate_session(struct hdd_adapter *adapter,
					 struct nlattr *twt_param_attr)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	struct wmi_twt_del_dialog_param params = {0};
	QDF_STATUS status;
	int id, id1, ret;
	bool is_associated;
	struct qdf_mac_addr mac_addr;

	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					     twt_param_attr,
					     qca_wlan_vendor_twt_add_dialog_policy);
	if (ret)
		return ret;

	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	id1 = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;
	if (tb[id] && tb[id1]) {
		params.dialog_id = nla_get_u8(tb[id]);
		nla_memcpy(params.peer_macaddr, tb[id1], QDF_MAC_ADDR_SIZE);
	} else if (!tb[id] && !tb[id1]) {
		struct qdf_mac_addr bcast_addr = QDF_MAC_ADDR_BCAST_INIT;

		params.dialog_id = TWT_ALL_SESSIONS_DIALOG_ID;
		qdf_mem_copy(params.peer_macaddr, bcast_addr.bytes,
			     QDF_MAC_ADDR_SIZE);
	} else {
		hdd_err_rl("get_params dialog_id or mac_addr is missing");
		return -EINVAL;
	}

	if (!params.dialog_id)
		params.dialog_id = TWT_ALL_SESSIONS_DIALOG_ID;

	if (params.dialog_id != TWT_ALL_SESSIONS_DIALOG_ID &&
	    QDF_IS_ADDR_BROADCAST(params.peer_macaddr)) {
		hdd_err("Bcast MAC valid with dlg_id:%d but here dlg_id is:%d",
			TWT_ALL_SESSIONS_DIALOG_ID, params.dialog_id);
		return -EINVAL;
	}

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       params.dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	qdf_mem_copy(mac_addr.bytes, params.peer_macaddr, QDF_MAC_ADDR_SIZE);

	if (!qdf_is_macaddr_broadcast(&mac_addr)) {
		is_associated = hdd_is_peer_associated(adapter, &mac_addr);
		if (!is_associated) {
			hdd_err_rl("Association doesn't exist for STA: "
				   QDF_MAC_ADDR_FMT,
				   QDF_MAC_ADDR_REF(mac_addr.bytes));
			/*
			 * Return success, since STA is not associated and
			 * there is no TWT session.
			 */
			return 0;
		}
	}

	hdd_debug("vdev_id %d dialog_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params.vdev_id, params.dialog_id,
		  QDF_MAC_ADDR_REF(params.peer_macaddr));

	ret = hdd_send_sap_twt_del_dialog_cmd(adapter->hdd_ctx, &params);

	return ret;
}

static int hdd_sta_twt_terminate_session(struct hdd_adapter *adapter,
					 struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx = NULL;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	struct wmi_twt_del_dialog_param params = {0};
	QDF_STATUS status;
	int id, ret;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err_rl("Invalid state, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);

		/*
		 * Return success, since STA is not associated and there is
		 * no TWT session.
		 */
		return 0;
	}

	if (hdd_is_roaming_in_progress(hdd_ctx))
		return -EBUSY;

	if (ucfg_scan_get_pdev_status(hdd_ctx->pdev)) {
		hdd_err_rl("Scan in progress");
		return -EBUSY;
	}

	qdf_mem_copy(params.peer_macaddr,
		     hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					     twt_param_attr,
					     qca_wlan_vendor_twt_add_dialog_policy);
	if (ret)
		return ret;

	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (tb[id]) {
		params.dialog_id = nla_get_u8(tb[id]);
	} else {
		params.dialog_id = 0;
		hdd_debug("TWT_TERMINATE_FLOW_ID not specified. set to zero");
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID;
	if (tb[id]) {
		params.dialog_id = nla_get_u8(tb[id]);
		hdd_debug("TWT_SETUP_BCAST_ID %d", params.dialog_id);
	}

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       params.dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 params.dialog_id)) {
		hdd_debug("vdev%d: TWT session %d setup incomplete",
			  params.vdev_id, params.dialog_id);
		return -EAGAIN;
	}

	hdd_debug("vdev_id %d dialog_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params.vdev_id, params.dialog_id,
		  QDF_MAC_ADDR_REF(params.peer_macaddr));

	ret = hdd_send_twt_del_dialog_cmd(adapter->hdd_ctx, &params);

	return ret;
}

/**
 * hdd_twt_terminate_session - Process TWT terminate
 * operation in the received vendor command and
 * send it to firmare
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_TERMINATE
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_terminate_session(struct hdd_adapter *adapter,
				     struct nlattr *twt_param_attr)
{
	enum QDF_OPMODE device_mode = adapter->device_mode;

	switch (device_mode) {
	case QDF_STA_MODE:
		return hdd_sta_twt_terminate_session(adapter, twt_param_attr);
	case QDF_SAP_MODE:
		return hdd_sap_twt_terminate_session(adapter, twt_param_attr);
	default:
		hdd_err_rl("TWT terminate is not supported on %s",
			   qdf_opmode_str(adapter->device_mode));
		return -EOPNOTSUPP;
	}
}

/**
 * hdd_twt_nudge_pack_resp_nlmsg() - pack the skb with
 * firmware response for twt nudge command
 * @reply_skb: skb to store the response
 * @params: Pointer to nudge dialog complete event buffer
 *
 * Return: QDF_STATUS_SUCCESS on Success, QDF_STATUS_E_FAILURE
 * on failure
 */
static QDF_STATUS
hdd_twt_nudge_pack_resp_nlmsg(struct sk_buff *reply_skb,
		      struct wmi_twt_nudge_dialog_complete_event_param *params)
{
	struct nlattr *config_attr;
	int vendor_status, attr;
	uint64_t tsf_val;

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_NUDGE)) {
		hdd_err("Failed to put TWT operation");
		return QDF_STATUS_E_FAILURE;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_FLOW_ID,
		       params->dialog_id)) {
		hdd_debug("Failed to put dialog_id");
		return QDF_STATUS_E_FAILURE;
	}

	tsf_val = params->next_twt_tsf_us_hi;
	tsf_val = (tsf_val << 32) | params->next_twt_tsf_us_lo;
	if (hdd_wlan_nla_put_u64(reply_skb,
				 QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME_TSF,
				 tsf_val)) {
		hdd_err("get_params failed to put TSF Value");
		return QDF_STATUS_E_INVAL;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
	vendor_status =
		     wmi_twt_nudge_status_to_vendor_twt_status(params->status);
	if (nla_put_u8(reply_skb, attr, vendor_status)) {
		hdd_err("Failed to put QCA_WLAN_TWT_NUDGE status");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAC_ADDR;
	if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
		    params->peer_macaddr)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/*
 * hdd_twt_nudge_dialog_comp_cb() - callback function
 * to get twt nudge command complete event
 * @psoc: Pointer to global psoc
 * @params: Pointer to nudge dialog complete event buffer
 *
 * Return: None
 */
static void hdd_twt_nudge_dialog_comp_cb(
		struct wlan_objmgr_psoc *psoc,
		struct wmi_twt_nudge_dialog_complete_event_param *params)
{

	struct hdd_adapter *adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, params->vdev_id);
	struct wireless_dev *wdev;
	struct hdd_context *hdd_ctx;
	struct sk_buff *twt_vendor_event;
	size_t data_len;
	QDF_STATUS status;

	hdd_enter();
	if (hdd_validate_adapter(adapter))
		return;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	wdev = adapter->dev->ieee80211_ptr;

	hdd_debug("Nudge dialog_id:%d, status:%d vdev_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params->dialog_id,
		  params->status, params->vdev_id,
		  QDF_MAC_ADDR_REF(params->peer_macaddr));

	data_len = hdd_get_twt_event_len() + nla_total_size(sizeof(u8)) +
		   nla_total_size(sizeof(u64));
	data_len += NLA_HDRLEN;

	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
				hdd_ctx->wiphy, wdev,
				data_len,
				QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
				GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("Nudge dialog alloc skb failed");
		return;
	}

	status = hdd_twt_nudge_pack_resp_nlmsg(twt_vendor_event, params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl nudge dialog response %d", status);
		wlan_cfg80211_vendor_free_skb(twt_vendor_event);
	}

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	hdd_exit();
}

/**
 * hdd_twt_pause_pack_resp_nlmsg() - pack the skb with
 * firmware response for twt pause command
 * @reply_skb: skb to store the response
 * @params: Pointer to pause dialog complete event buffer
 *
 * Return: QDF_STATUS_SUCCESS on Success, QDF_STATUS_E_FAILURE
 * on failure
 */
static QDF_STATUS
hdd_twt_pause_pack_resp_nlmsg(struct sk_buff *reply_skb,
			      struct wmi_twt_pause_dialog_complete_event_param *params)
{
	struct nlattr *config_attr;
	int vendor_status, attr;

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_SUSPEND)) {
		hdd_err("Failed to put TWT operation");
		return QDF_STATUS_E_FAILURE;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (nla_put_u8(reply_skb, attr, params->dialog_id)) {
		hdd_debug("Failed to put dialog_id");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
	vendor_status = wmi_twt_pause_status_to_vendor_twt_status(params->status);
	if (nla_put_u8(reply_skb, attr, vendor_status)) {
		hdd_err("Failed to put QCA_WLAN_TWT_PAUSE status");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAC_ADDR;
	if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
		    params->peer_macaddr)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/*
 * hdd_twt_pause_dialog_comp_cb() - callback function
 * to get twt pause command complete event
 * @psoc: pointer to global psoc
 * @params: Pointer to pause dialog complete event buffer
 *
 * Return: None
 */
static void
hdd_twt_pause_dialog_comp_cb(
		struct wlan_objmgr_psoc *psoc,
		struct wmi_twt_pause_dialog_complete_event_param *params)
{
	struct hdd_adapter *adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, params->vdev_id);
	struct wireless_dev *wdev;
	struct hdd_context *hdd_ctx;
	struct sk_buff *twt_vendor_event;
	size_t data_len;
	QDF_STATUS status;

	hdd_enter();

	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	wdev = adapter->dev->ieee80211_ptr;

	hdd_debug("pause dialog_id:%d, status:%d vdev_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params->dialog_id,
		  params->status, params->vdev_id,
		  QDF_MAC_ADDR_REF(params->peer_macaddr));

	data_len = hdd_get_twt_event_len() + nla_total_size(sizeof(u8));
	data_len += NLA_HDRLEN;

	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
				hdd_ctx->wiphy, wdev, data_len,
				QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
				GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("pause dialog alloc skb failed");
		return;
	}

	status = hdd_twt_pause_pack_resp_nlmsg(twt_vendor_event, params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl pause dialog response");
		wlan_cfg80211_vendor_free_skb(twt_vendor_event);
	}

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	hdd_exit();
}

/**
 * hdd_send_twt_pause_dialog_cmd() - Send TWT pause dialog command to target
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to pause dialog cmd params structure
 *
 * Return: 0 on success, negative value on failure
 */
static
int hdd_send_twt_pause_dialog_cmd(struct hdd_context *hdd_ctx,
				  struct wmi_twt_pause_dialog_cmd_param *twt_params)
{
	QDF_STATUS status;
	int ret = 0, twt_cmd;
	struct osif_request *request;
	struct twt_ack_info_priv *ack_priv;
	void *context;
	static const struct osif_request_params params = {
				.priv_size = sizeof(*ack_priv),
				.timeout_ms = TWT_ACK_COMPLETE_TIMEOUT,
	};

	hdd_enter();

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -EINVAL;
	}

	context = osif_request_cookie(request);

	status = sme_pause_dialog_cmd(hdd_ctx->mac_handle,
				      twt_params, context);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send pause dialog command");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	twt_cmd = WMI_HOST_TWT_PAUSE_DIALOG_CMDID;

	status = hdd_twt_ack_wait_response(hdd_ctx, request, twt_cmd);

	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ack_priv = osif_request_priv(request);
	if (ack_priv->status) {
		hdd_err("Received TWT ack error. Reset twt command");
		ucfg_mlme_reset_twt_active_cmd(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);

		switch (ack_priv->status) {
		case WMI_HOST_PAUSE_TWT_STATUS_INVALID_PARAM:
		case WMI_HOST_PAUSE_TWT_STATUS_ALREADY_PAUSED:
		case WMI_HOST_PAUSE_TWT_STATUS_UNKNOWN_ERROR:
			ret = -EINVAL;
			break;
		case WMI_HOST_PAUSE_TWT_STATUS_DIALOG_ID_NOT_EXIST:
			ret = -EAGAIN;
			break;
		case WMI_HOST_PAUSE_TWT_STATUS_DIALOG_ID_BUSY:
			ret = -EINPROGRESS;
			break;
		case WMI_HOST_PAUSE_TWT_STATUS_NO_RESOURCE:
			ret = -ENOMEM;
			break;
		case WMI_HOST_PAUSE_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		case WMI_HOST_PAUSE_TWT_STATUS_ROAM_IN_PROGRESS:
		case WMI_HOST_PAUSE_TWT_STATUS_SCAN_IN_PROGRESS:
			ret = -EBUSY;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	ret = qdf_status_to_os_return(status);
cleanup:
	osif_request_put(request);
	hdd_exit();

	return ret;
}

/**
 * hdd_twt_pause_session - Process TWT pause operation
 * in the received vendor command and send it to firmware
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_SUSPEND
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_pause_session(struct hdd_adapter *adapter,
				 struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
		WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
	struct wmi_twt_pause_dialog_cmd_param params = {0};
	QDF_STATUS status;
	int id;
	int ret;

	ret = hdd_is_twt_command_allowed(adapter);
	if (ret)
		return ret;

	qdf_mem_copy(params.peer_macaddr, hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb,
					QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
					twt_param_attr,
					qca_wlan_vendor_twt_add_dialog_policy);
	if (ret) {
		hdd_debug("command parsing failed");
		return ret;
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
	if (tb[id]) {
		params.dialog_id = nla_get_u8(tb[id]);
	} else {
		params.dialog_id = 0;
		hdd_debug("TWT: FLOW_ID not specified. set to zero");
	}

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       params.dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 params.dialog_id)) {
		hdd_debug("vdev%d: TWT session %d setup incomplete",
			  params.vdev_id, params.dialog_id);
		return -EAGAIN;
	}

	hdd_debug("twt_pause: vdev_id %d dialog_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params.vdev_id, params.dialog_id,
		  QDF_MAC_ADDR_REF(params.peer_macaddr));

	ret = hdd_send_twt_pause_dialog_cmd(adapter->hdd_ctx, &params);

	return ret;
}

/**
 * hdd_send_twt_nudge_dialog_cmd() - Send TWT nudge dialog command to target
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to nudge dialog cmd params structure
 *
 * Return: 0 on success, negative value on failure
 */
static
int hdd_send_twt_nudge_dialog_cmd(struct hdd_context *hdd_ctx,
			struct wmi_twt_nudge_dialog_cmd_param *twt_params)
{
	QDF_STATUS status;
	int twt_cmd, ret = 0;
	struct osif_request *request;
	struct twt_ack_info_priv *ack_priv;
	void *context;
	static const struct osif_request_params params = {
				.priv_size = sizeof(*ack_priv),
				.timeout_ms = TWT_ACK_COMPLETE_TIMEOUT,
	};

	hdd_enter();

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -EINVAL;
	}

	context = osif_request_cookie(request);

	status = sme_nudge_dialog_cmd(hdd_ctx->mac_handle, twt_params, context);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send nudge dialog command");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	twt_cmd = WMI_HOST_TWT_NUDGE_DIALOG_CMDID;

	status = hdd_twt_ack_wait_response(hdd_ctx, request, twt_cmd);

	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ack_priv = osif_request_priv(request);
	if (ack_priv->status) {
		hdd_err("Received TWT ack error. Reset twt command");
		ucfg_mlme_reset_twt_active_cmd(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);

		switch (ack_priv->status) {
		case WMI_HOST_NUDGE_TWT_STATUS_INVALID_PARAM:
		case WMI_HOST_NUDGE_TWT_STATUS_UNKNOWN_ERROR:
			ret = -EINVAL;
			break;
		case WMI_HOST_NUDGE_TWT_STATUS_DIALOG_ID_NOT_EXIST:
			ret = -EAGAIN;
			break;
		case WMI_HOST_NUDGE_TWT_STATUS_DIALOG_ID_BUSY:
			ret = -EINPROGRESS;
			break;
		case WMI_HOST_NUDGE_TWT_STATUS_NO_RESOURCE:
			ret = -ENOMEM;
			break;
		case WMI_HOST_NUDGE_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		case WMI_HOST_NUDGE_TWT_STATUS_ROAM_IN_PROGRESS:
		case WMI_HOST_NUDGE_TWT_STATUS_SCAN_IN_PROGRESS:
			ret = -EBUSY;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	ret = qdf_status_to_os_return(status);
cleanup:
	osif_request_put(request);
	hdd_exit();

	return ret;
}

/**
 * hdd_twt_nudge_session - Process TWT nudge operation
 * in the received vendor command and send it to firmware
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_NUDGE
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_nudge_session(struct hdd_adapter *adapter,
				 struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
		WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAX + 1];
	struct wmi_twt_nudge_dialog_cmd_param params = {0};
	QDF_STATUS status;
	int id;
	int ret;
	bool is_nudge_tgt_cap_enabled;

	ret = hdd_is_twt_command_allowed(adapter);
	if (ret)
		return ret;

	ucfg_mlme_get_twt_nudge_tgt_cap(adapter->hdd_ctx->psoc,
					&is_nudge_tgt_cap_enabled);
	if (!is_nudge_tgt_cap_enabled) {
		hdd_debug("Nudge not supported by target");
		return -EOPNOTSUPP;
	}

	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb,
				      QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAX,
				      twt_param_attr,
				      qca_wlan_vendor_twt_nudge_dialog_policy);
	if (ret)
		return ret;

	id = QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_MAC_ADDR;
	if (tb[id]) {
		nla_memcpy(params.peer_macaddr, tb[id], QDF_MAC_ADDR_SIZE);
		hdd_debug("peer mac_addr "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(params.peer_macaddr));
	} else {
		qdf_mem_copy(params.peer_macaddr,
			     hdd_sta_ctx->conn_info.bssid.bytes,
			     QDF_MAC_ADDR_SIZE);
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_FLOW_ID;
	if (!tb[id]) {
		hdd_debug("TWT: FLOW_ID not specified");
		return -EINVAL;
	}
	params.dialog_id = nla_get_u8(tb[id]);

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       params.dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_WAKE_TIME;
	if (!tb[id]) {
		hdd_debug("TWT: WAKE_TIME not specified");
		return -EINVAL;
	}
	params.suspend_duration = nla_get_u32(tb[id]);

	id = QCA_WLAN_VENDOR_ATTR_TWT_NUDGE_NEXT_TWT_SIZE;
	if (!tb[id]) {
		hdd_debug("TWT: NEXT_TWT_SIZE not specified.");
		return -EINVAL;
	}
	params.next_twt_size = nla_get_u32(tb[id]);

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 params.dialog_id)) {
		hdd_debug("vdev%d: TWT session %d setup incomplete",
			  params.vdev_id, params.dialog_id);
		return -EAGAIN;
	}

	hdd_debug("twt_nudge: vdev_id %d dialog_id %d ", params.vdev_id,
		  params.dialog_id);
	hdd_debug("twt_nudge: suspend_duration %d next_twt_size %d",
		  params.suspend_duration, params.next_twt_size);

	ret = hdd_send_twt_nudge_dialog_cmd(adapter->hdd_ctx, &params);

	return ret;
}

/**
 * hdd_twt_resume_pack_resp_nlmsg() - pack the skb with
 * firmware response for twt resume command
 * @reply_skb: skb to store the response
 * @params: Pointer to resume dialog complete event buffer
 *
 * Return: QDF_STATUS_SUCCESS on Success, QDF_STATUS_E_FAILURE
 * on failure
 */
static QDF_STATUS
hdd_twt_resume_pack_resp_nlmsg(struct sk_buff *reply_skb,
			       struct wmi_twt_resume_dialog_complete_event_param *params)
{
	struct nlattr *config_attr;
	int vendor_status, attr;

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_RESUME)) {
		hdd_err("Failed to put TWT operation");
		return QDF_STATUS_E_FAILURE;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_TWT_RESUME_FLOW_ID,
		       params->dialog_id)) {
		hdd_debug("Failed to put dialog_id");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
	vendor_status = wmi_twt_resume_status_to_vendor_twt_status(params->status);
	if (nla_put_u8(reply_skb, attr, vendor_status)) {
		hdd_err("Failed to put QCA_WLAN_TWT_RESUME status");
		return QDF_STATUS_E_FAILURE;
	}

	attr = QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAC_ADDR;
	if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
		    params->peer_macaddr)) {
		hdd_err("Failed to put mac_addr");
		return QDF_STATUS_E_INVAL;
	}

	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_resume_dialog_comp_cb() - callback function
 * to get twt resume command complete event
 * @psoc: Pointer to global psoc
 * @vdev_id: Vdev id
 * @params: Pointer to resume dialog complete event buffer
 *
 * Return: None
 */
static void hdd_twt_resume_dialog_comp_cb(
		struct wlan_objmgr_psoc *psoc,
		struct wmi_twt_resume_dialog_complete_event_param *params)
{
	struct hdd_adapter *adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, params->vdev_id);
	struct hdd_context *hdd_ctx;
	struct wireless_dev *wdev;
	struct sk_buff *twt_vendor_event;
	size_t data_len;
	QDF_STATUS status;

	hdd_enter();

	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	wdev = adapter->dev->ieee80211_ptr;

	hdd_debug("TWT: resume dialog_id:%d status:%d vdev_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params->dialog_id,
		  params->status, params->vdev_id,
		  QDF_MAC_ADDR_REF(params->peer_macaddr));

	data_len = hdd_get_twt_event_len() + nla_total_size(sizeof(u8));
	data_len += NLA_HDRLEN;
	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy, wdev, data_len,
			QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
			GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("TWT: skb alloc failed");
		return;
	}

	status = hdd_twt_resume_pack_resp_nlmsg(twt_vendor_event, params);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to pack nl resume dialog response");

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	hdd_exit();
}

/**
 * hdd_send_twt_resume_dialog_cmd() - Send TWT resume dialog command to target
 * @hdd_ctx: HDD Context
 * @twt_params: Pointer to resume dialog cmd params structure
 *
 * Return: 0 on success, negative value on failure
 */
static int
hdd_send_twt_resume_dialog_cmd(struct hdd_context *hdd_ctx,
			       struct wmi_twt_resume_dialog_cmd_param *twt_params)
{
	QDF_STATUS status;
	int ret = 0, twt_cmd;
	struct osif_request *request;
	struct twt_ack_info_priv *ack_priv;
	void *context;
	static const struct osif_request_params params = {
				.priv_size = sizeof(*ack_priv),
				.timeout_ms = TWT_ACK_COMPLETE_TIMEOUT,
	};

	hdd_enter();

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -EINVAL;
	}

	context = osif_request_cookie(request);

	status = sme_resume_dialog_cmd(hdd_ctx->mac_handle,
				       twt_params, context);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send resume dialog command");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	twt_cmd = WMI_HOST_TWT_RESUME_DIALOG_CMDID;

	status = hdd_twt_ack_wait_response(hdd_ctx, request, twt_cmd);

	if (QDF_IS_STATUS_ERROR(status)) {
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}

	ack_priv = osif_request_priv(request);
	if (ack_priv->status) {
		hdd_err("Received TWT ack error. Reset twt command");
		ucfg_mlme_reset_twt_active_cmd(
				hdd_ctx->psoc,
				(struct qdf_mac_addr *)twt_params->peer_macaddr,
				twt_params->dialog_id);

		switch (ack_priv->status) {
		case WMI_HOST_RESUME_TWT_STATUS_INVALID_PARAM:
		case WMI_HOST_RESUME_TWT_STATUS_UNKNOWN_ERROR:
			ret = -EINVAL;
			break;
		case WMI_HOST_RESUME_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		case WMI_HOST_RESUME_TWT_STATUS_NOT_PAUSED:
			ret = -EAGAIN;
			break;
		case WMI_HOST_RESUME_TWT_STATUS_DIALOG_ID_BUSY:
			ret = -EINPROGRESS;
			break;
		case WMI_HOST_RESUME_TWT_STATUS_NO_RESOURCE:
			ret = -ENOMEM;
			break;
		case WMI_HOST_RESUME_TWT_STATUS_CHAN_SW_IN_PROGRESS:
		case WMI_HOST_RESUME_TWT_STATUS_ROAM_IN_PROGRESS:
		case WMI_HOST_RESUME_TWT_STATUS_SCAN_IN_PROGRESS:
			ret = -EBUSY;
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	ret = qdf_status_to_os_return(status);
cleanup:
	osif_request_put(request);
	hdd_exit();

	return ret;
}

/**
 * hdd_twt_pack_get_capabilities_resp  - TWT pack and send response to
 * userspace for get capabilities command
 * @adapter: Pointer to hdd adapter
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
hdd_twt_pack_get_capabilities_resp(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *sta_ctx =
		WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *config_attr;
	struct sk_buff *reply_skb;
	size_t skb_len = NLMSG_HDRLEN;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint8_t connected_band;
	uint8_t peer_cap = 0, self_cap = 0;
	bool twt_req = false, twt_bcast_req = false;
	bool is_twt_24ghz_allowed = true;

	/*
	 * Length of attribute QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF &
	 * QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_PEER
	 */
	skb_len += 2 * nla_total_size(sizeof(u16)) + NLA_HDRLEN;

	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
							     skb_len);
	if (!reply_skb) {
		hdd_err("TWT: get_caps alloc reply skb failed");
		return QDF_STATUS_E_NOMEM;
	}

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("TWT: nla_nest_start error");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto free_skb;
	}

	/*
	 * Userspace will query the TWT get capabilities before
	 * issuing a get capabilities request. If the STA is
	 * connected, then check the "enable_twt_24ghz" ini
	 * value to advertise the TWT requestor capability.
	 */
	connected_band = hdd_conn_get_connected_band(&adapter->session.station);
	if (connected_band == BAND_2G &&
	    !ucfg_mlme_is_24ghz_twt_enabled(hdd_ctx->psoc))
		is_twt_24ghz_allowed = false;

	/* fill the self_capability bitmap  */
	ucfg_mlme_get_twt_requestor(hdd_ctx->psoc, &twt_req);
	if (twt_req && is_twt_24ghz_allowed)
		self_cap |= QCA_WLAN_TWT_CAPA_REQUESTOR;

	ucfg_mlme_get_twt_bcast_requestor(hdd_ctx->psoc,
					  &twt_bcast_req);
	self_cap |= (twt_bcast_req ? QCA_WLAN_TWT_CAPA_BROADCAST : 0);

	if (ucfg_mlme_is_flexible_twt_enabled(hdd_ctx->psoc))
		self_cap |= QCA_WLAN_TWT_CAPA_FLEXIBLE;

	/* Fill the Peer capability bitmap */
	peer_cap = ucfg_mlme_get_twt_peer_capabilities(
		hdd_ctx->psoc,
		(struct qdf_mac_addr *)sta_ctx->conn_info.bssid.bytes);

	if (nla_put_u16(reply_skb, QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF,
			self_cap)) {
		hdd_err("TWT: Failed to fill capabilities");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto free_skb;
	}

	if (nla_put_u16(reply_skb, QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_PEER,
			peer_cap)) {
		hdd_err("TWT: Failed to fill capabilities");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto free_skb;
	}

	nla_nest_end(reply_skb, config_attr);

	if (cfg80211_vendor_cmd_reply(reply_skb))
		qdf_status = QDF_STATUS_E_INVAL;

free_skb:
	if (QDF_IS_STATUS_ERROR(qdf_status) && reply_skb)
		kfree_skb(reply_skb);

	return qdf_status;
}

/**
 * hdd_twt_get_capabilities() - Process TWT resume operation
 * in the received vendor command and send it to firmware
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_RESUME
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_get_capabilities(struct hdd_adapter *adapter,
				    struct nlattr *twt_param_attr)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *hdd_sta_ctx;
	QDF_STATUS status;
	int ret = 0;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret < 0)
		return -EINVAL;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE) {
		return -EOPNOTSUPP;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err_rl("Invalid state, vdev %d mode %d state %d",
			   adapter->vdev_id, adapter->device_mode,
			   hdd_sta_ctx->conn_info.conn_state);
		return -EAGAIN;
	}

	if (hdd_is_roaming_in_progress(hdd_ctx))
		return -EBUSY;

	status = hdd_twt_pack_get_capabilities_resp(adapter);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err_rl("TWT: Get capabilities failed");

	return qdf_status_to_os_return(status);
}

/**
 * hdd_twt_resume_session - Process TWT resume operation
 * in the received vendor command and send it to firmware
 * @adapter: adapter pointer
 * @twt_param_attr: nl attributes
 *
 * Handles QCA_WLAN_TWT_RESUME
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_resume_session(struct hdd_adapter *adapter,
				  struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
			WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAX + 1];
	struct wmi_twt_resume_dialog_cmd_param params = {0};
	QDF_STATUS status;
	int id, id2;
	int ret;

	ret = hdd_is_twt_command_allowed(adapter);
	if (ret)
		return ret;

	qdf_mem_copy(params.peer_macaddr, hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	params.vdev_id = adapter->vdev_id;

	ret = wlan_cfg80211_nla_parse_nested(tb,
					     QCA_WLAN_VENDOR_ATTR_TWT_RESUME_MAX,
					     twt_param_attr,
					     qca_wlan_vendor_twt_resume_dialog_policy);
	if (ret)
		return ret;

	id = QCA_WLAN_VENDOR_ATTR_TWT_RESUME_FLOW_ID;
	if (tb[id]) {
		params.dialog_id = nla_get_u8(tb[id]);
	} else {
		params.dialog_id = 0;
		hdd_debug("TWT_RESUME_FLOW_ID not specified. set to zero");
	}

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       params.dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 params.dialog_id)) {
		hdd_debug("vdev%d: TWT session %d setup incomplete",
			  params.vdev_id, params.dialog_id);
		return -EAGAIN;
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT;
	id2 = QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT2_TWT;
	if (tb[id2])
		params.sp_offset_us = nla_get_u32(tb[id2]);
	else if (tb[id])
		params.sp_offset_us = nla_get_u8(tb[id]);
	else
		params.sp_offset_us = 0;

	id = QCA_WLAN_VENDOR_ATTR_TWT_RESUME_NEXT_TWT_SIZE;
	if (tb[id]) {
		params.next_twt_size = nla_get_u32(tb[id]);
	} else {
		hdd_err_rl("TWT_RESUME NEXT_TWT_SIZE is must");
		return -EINVAL;
	}
	if (params.next_twt_size > TWT_MAX_NEXT_TWT_SIZE)
		return -EINVAL;

	hdd_debug("twt_resume: vdev_id %d dialog_id %d peer mac_addr "
		  QDF_MAC_ADDR_FMT, params.vdev_id, params.dialog_id,
		  QDF_MAC_ADDR_REF(params.peer_macaddr));

	ret = hdd_send_twt_resume_dialog_cmd(adapter->hdd_ctx, &params);

	return ret;
}

static uint32_t get_session_wake_duration(struct hdd_context *hdd_ctx,
					  uint32_t dialog_id,
					  struct qdf_mac_addr *peer_macaddr)
{
	struct wmi_host_twt_session_stats_info params = {0};
	int num_twt_session = 0;

	params.dialog_id = dialog_id;
	qdf_mem_copy(params.peer_mac,
		     peer_macaddr->bytes,
		     QDF_MAC_ADDR_SIZE);
	hdd_debug("Get_params peer mac_addr " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(params.peer_mac));

	num_twt_session = ucfg_twt_get_peer_session_params(hdd_ctx->psoc,
							   &params);
	if (num_twt_session)
		return params.wake_dura_us;

	return 0;
}

static int
wmi_twt_get_stats_status_to_vendor_twt_status(enum WMI_HOST_GET_STATS_TWT_STATUS status)
{
	switch (status) {
	case WMI_HOST_GET_STATS_TWT_STATUS_OK:
		return QCA_WLAN_VENDOR_TWT_STATUS_OK;
	case WMI_HOST_GET_STATS_TWT_STATUS_DIALOG_ID_NOT_EXIST:
		return QCA_WLAN_VENDOR_TWT_STATUS_SESSION_NOT_EXIST;
	case WMI_HOST_GET_STATS_TWT_STATUS_INVALID_PARAM:
		return QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM;
	default:
		return QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR;
	}
}

/**
 * hdd_twt_pack_get_stats_resp_nlmsg()- Packs and sends twt get stats response
 * hdd_ctx: pointer to the hdd context
 * @reply_skb: pointer to response skb buffer
 * @params: Ponter to twt session parameter buffer
 * @num_session_stats: number of twt statistics
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_pack_get_stats_resp_nlmsg(struct hdd_context *hdd_ctx,
				  struct sk_buff *reply_skb,
				  struct twt_infra_cp_stats_event *params,
				  uint32_t num_session_stats)
{
	struct nlattr *config_attr, *nla_params;
	int i, attr;
	int vendor_status;
	uint32_t duration;

	config_attr = nla_nest_start(reply_skb,
				     QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
	if (!config_attr) {
		hdd_err("get_params nla_nest_start error");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < num_session_stats; i++) {

		nla_params = nla_nest_start(reply_skb, i);
		if (!nla_params) {
			hdd_err("get_stats nla_nest_start error");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAC_ADDR;
		if (nla_put(reply_skb, attr, QDF_MAC_ADDR_SIZE,
			    params[i].peer_macaddr.bytes)) {
			hdd_err("get_stats failed to put mac_addr");
			return QDF_STATUS_E_INVAL;
		}
		hdd_debug("get_stats peer mac_addr " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(params[i].peer_macaddr.bytes));

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID;
		if (nla_put_u8(reply_skb, attr, params[i].dialog_id)) {
			hdd_err("get_stats failed to put dialog_id");
			return QDF_STATUS_E_INVAL;
		}

		duration = get_session_wake_duration(hdd_ctx,
						     params[i].dialog_id,
						     &params[i].peer_macaddr);

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_SESSION_WAKE_DURATION;
		if (nla_put_u32(reply_skb, attr, duration)) {
			hdd_err("get_params failed to put Wake duration");
			return QDF_STATUS_E_INVAL;
		}
		hdd_debug("dialog_id %d wake duration %d num sp cycles %d",
			  params[i].dialog_id, duration,
			  params[i].num_sp_cycles);

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_NUM_SP_ITERATIONS;
		if (nla_put_u32(reply_skb, attr, params[i].num_sp_cycles)) {
			hdd_err("get_params failed to put num_sp_cycles");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVG_WAKE_DURATION;
		if (nla_put_u32(reply_skb, attr, params[i].avg_sp_dur_us)) {
			hdd_err("get_params failed to put avg_sp_dur_us");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_MIN_WAKE_DURATION;
		if (nla_put_u32(reply_skb, attr, params[i].min_sp_dur_us)) {
			hdd_err("get_params failed to put min_sp_dur_us");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX_WAKE_DURATION;
		if (nla_put_u32(reply_skb, attr, params[i].max_sp_dur_us)) {
			hdd_err("get_params failed to put max_sp_dur_us");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_MPDU;
		if (nla_put_u32(reply_skb, attr, params[i].tx_mpdu_per_sp)) {
			hdd_err("get_params failed to put tx_mpdu_per_sp");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_MPDU;
		if (nla_put_u32(reply_skb, attr, params[i].rx_mpdu_per_sp)) {
			hdd_err("get_params failed to put rx_mpdu_per_sp");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_PACKET_SIZE;
		if (nla_put_u32(reply_skb, attr, params[i].tx_bytes_per_sp)) {
			hdd_err("get_params failed to put tx_bytes_per_sp");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_PACKET_SIZE;
		if (nla_put_u32(reply_skb, attr, params[i].rx_bytes_per_sp)) {
			hdd_err("get_params failed to put rx_bytes_per_sp");
			return QDF_STATUS_E_INVAL;
		}

		attr = QCA_WLAN_VENDOR_ATTR_TWT_STATS_STATUS;
		vendor_status = wmi_twt_get_stats_status_to_vendor_twt_status(params[i].status);
		if (nla_put_u32(reply_skb, attr, vendor_status)) {
			hdd_err("get_params failed to put status");
			return QDF_STATUS_E_INVAL;
		}
		nla_nest_end(reply_skb, nla_params);
	}
	nla_nest_end(reply_skb, config_attr);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_clear_session_traffic_stats() - Parses twt nl attrributes and
 * sends clear twt stats request for a single or all sessions
 * @adapter: hdd_adapter
 * @twt_param_attr: twt nl attributes
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_clear_session_traffic_stats(struct hdd_adapter *adapter,
					       struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX + 1];
	int ret, id;
	uint32_t dialog_id;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	bool is_stats_tgt_cap_enabled;
	QDF_STATUS status;

	ret = wlan_cfg80211_nla_parse_nested(
				tb,
				QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX,
				twt_param_attr,
				qca_wlan_vendor_twt_stats_dialog_policy);
	if (ret)
		return ret;

	ucfg_mlme_get_twt_statistics_tgt_cap(adapter->hdd_ctx->psoc,
					     &is_stats_tgt_cap_enabled);
	if (!is_stats_tgt_cap_enabled) {
		hdd_debug("TWT Stats not supported by target");
		return -EOPNOTSUPP;
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID;
	if (!tb[id]) {
		hdd_err_rl("TWT Clear stats - dialog id param is must");
		return -EINVAL;
	}

	dialog_id = (uint32_t)nla_get_u8(tb[id]);

	qdf_mem_copy(peer_mac,
		     hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	hdd_debug("dialog_id %d peer mac_addr " QDF_MAC_ADDR_FMT,
		  dialog_id, QDF_MAC_ADDR_REF(peer_mac));

	status = hdd_twt_check_all_twt_support(adapter->hdd_ctx->psoc,
					       dialog_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("All TWT sessions not supported by target");
		return -EOPNOTSUPP;
	}

	if (ucfg_mlme_twt_is_command_in_progress(adapter->hdd_ctx->psoc,
						 &hdd_sta_ctx->conn_info.bssid,
						 WLAN_ALL_SESSIONS_DIALOG_ID,
						 WLAN_TWT_STATISTICS, NULL) ||
	   ucfg_mlme_twt_is_command_in_progress(adapter->hdd_ctx->psoc,
						&hdd_sta_ctx->conn_info.bssid,
						WLAN_ALL_SESSIONS_DIALOG_ID,
						WLAN_TWT_CLEAR_STATISTICS,
						NULL)) {
		hdd_warn("Already TWT statistics or clear statistics exists");
		return -EALREADY;
	}

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 dialog_id)) {
		hdd_debug("TWT session %d setup incomplete", dialog_id);
		return -EAGAIN;
	}

	ret = wlan_cfg80211_mc_twt_clear_infra_cp_stats(adapter->vdev,
							dialog_id, peer_mac);

	return ret;
}

/**
 * hdd_twt_get_session_traffic_stats() - Obtains twt session traffic statistics
 * and sends response to the user space
 * @adapter: hdd_adapter
 * @dialog_id: dialog id of the twt session
 * @peer_mac: Mac address of the peer
 *
 * Return: QDF_STATUS_SUCCESS on success, else other qdf error values
 */
static QDF_STATUS
hdd_twt_request_session_traffic_stats(struct hdd_adapter *adapter,
				      uint32_t dialog_id, uint8_t *peer_mac)
{
	int errno;
	int skb_len;
	struct sk_buff *reply_skb;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct infra_cp_stats_event *event;

	if (!adapter || !peer_mac)
		return status;

	event = wlan_cfg80211_mc_twt_get_infra_cp_stats(adapter->vdev,
							dialog_id,
							peer_mac,
							&errno);
	if (!event)
		return errno;

	skb_len = hdd_get_twt_get_stats_event_len();
	reply_skb = wlan_cfg80211_vendor_cmd_alloc_reply_skb(
						adapter->hdd_ctx->wiphy,
						skb_len);
	if (!reply_skb) {
		hdd_err("Get stats - alloc reply_skb failed");
		return -ENOMEM;
	}

	status = hdd_twt_pack_get_stats_resp_nlmsg(
						adapter->hdd_ctx,
						reply_skb,
						event->twt_infra_cp_stats,
						event->num_twt_infra_cp_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Get stats - Failed to pack nl response");
		wlan_cfg80211_vendor_free_skb(reply_skb);
		return qdf_status_to_os_return(status);
	}

	qdf_mem_free(event->twt_infra_cp_stats);
	qdf_mem_free(event);

	return wlan_cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * hdd_twt_get_session_stats() - Parses twt nl attrributes, obtains twt
 * session parameters based on dialog_id and returns to user via nl layer
 * @adapter: hdd_adapter
 * @twt_param_attr: twt nl attributes
 *
 * Return: 0 on success, negative value on failure
 */
static int hdd_twt_get_session_traffic_stats(struct hdd_adapter *adapter,
					     struct nlattr *twt_param_attr)
{
	struct hdd_station_ctx *hdd_sta_ctx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX + 1];
	int ret, id;
	QDF_STATUS qdf_status;
	uint32_t dialog_id;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	bool is_stats_tgt_cap_enabled;

	ret = wlan_cfg80211_nla_parse_nested(
				tb,
				QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX,
				twt_param_attr,
				qca_wlan_vendor_twt_stats_dialog_policy);
	if (ret)
		return ret;

	ucfg_mlme_get_twt_statistics_tgt_cap(adapter->hdd_ctx->psoc,
					     &is_stats_tgt_cap_enabled);
	if (!is_stats_tgt_cap_enabled) {
		hdd_debug("TWT Stats not supported by target");
		return -EOPNOTSUPP;
	}

	if (ucfg_mlme_twt_is_command_in_progress(adapter->hdd_ctx->psoc,
						 &hdd_sta_ctx->conn_info.bssid,
						 WLAN_ALL_SESSIONS_DIALOG_ID,
						 WLAN_TWT_STATISTICS, NULL) ||
	    ucfg_mlme_twt_is_command_in_progress(adapter->hdd_ctx->psoc,
						 &hdd_sta_ctx->conn_info.bssid,
						 WLAN_ALL_SESSIONS_DIALOG_ID,
						 WLAN_TWT_CLEAR_STATISTICS,
						 NULL)) {
		hdd_warn("Already TWT statistics or clear statistics exists");
		return -EALREADY;
	}

	id = QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID;
	if (tb[id])
		dialog_id = (uint32_t)nla_get_u8(tb[id]);
	else
		dialog_id = 0;

	hdd_debug("get_stats dialog_id %d", dialog_id);

	qdf_mem_copy(peer_mac,
		     hdd_sta_ctx->conn_info.bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	hdd_debug("get_stats peer mac_addr " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(peer_mac));

	if (!ucfg_mlme_is_twt_setup_done(adapter->hdd_ctx->psoc,
					 &hdd_sta_ctx->conn_info.bssid,
					 dialog_id)) {
		hdd_debug("TWT session %d setup incomplete", dialog_id);
		return -EAGAIN;
	}

	ucfg_mlme_set_twt_command_in_progress(adapter->hdd_ctx->psoc,
					      &hdd_sta_ctx->conn_info.bssid,
					      dialog_id,
					      WLAN_TWT_STATISTICS);
	qdf_status = hdd_twt_request_session_traffic_stats(adapter,
							   dialog_id, peer_mac);
	ucfg_mlme_set_twt_command_in_progress(adapter->hdd_ctx->psoc,
					      &hdd_sta_ctx->conn_info.bssid,
					      dialog_id,
					      WLAN_TWT_NONE);

	return qdf_status_to_os_return(qdf_status);
}

/**
 * hdd_twt_notify_pack_nlmsg() - pack the skb with
 * twt notify event from firmware
 * @reply_skb: skb to store the response
 *
 * Return: QDF_STATUS_SUCCESS on Success, QDF_STATUS_E_FAILURE
 * on failure
 */
static QDF_STATUS
hdd_twt_notify_pack_nlmsg(struct sk_buff *reply_skb)
{
	if (nla_put_u8(reply_skb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
		       QCA_WLAN_TWT_SETUP_READY_NOTIFY)) {
		hdd_err("Failed to put TWT notify operation");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_twt_notify_cb() - callback function
 * to get twt notify event
 * @psoc: Pointer to global psoc
 * @params: Pointer to notify param event buffer
 *
 * Return: None
 */
static void
hdd_twt_notify_cb(struct wlan_objmgr_psoc *psoc,
		  struct wmi_twt_notify_event_param *params)
{
	struct hdd_adapter *adapter =
		wlan_hdd_get_adapter_from_vdev(psoc, params->vdev_id);
	struct wireless_dev *wdev;
	struct sk_buff *twt_vendor_event;
	size_t data_len;
	QDF_STATUS status;

	hdd_enter();

	if (hdd_validate_adapter(adapter))
		return;

	wdev = adapter->dev->ieee80211_ptr;

	data_len = NLA_HDRLEN;
	data_len += nla_total_size(sizeof(u8));

	twt_vendor_event = wlan_cfg80211_vendor_event_alloc(
				adapter->wdev.wiphy, wdev,
				data_len,
				QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT_INDEX,
				GFP_KERNEL);
	if (!twt_vendor_event) {
		hdd_err("Notify skb alloc failed");
		return;
	}

	hdd_debug("Notify vdev_id %d", params->vdev_id);

	status = hdd_twt_notify_pack_nlmsg(twt_vendor_event);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to pack nl notify event");
		wlan_cfg80211_vendor_free_skb(twt_vendor_event);
		return;
	}

	wlan_cfg80211_vendor_event(twt_vendor_event, GFP_KERNEL);

	hdd_exit();
}

/**
 * hdd_twt_configure - Process the TWT
 * operation in the received vendor command
 * @adapter: adapter pointer
 * @tb: nl attributes
 *
 * Handles QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION
 *
 * Return: 0 for Success and negative value for failure
 */
static int hdd_twt_configure(struct hdd_adapter *adapter,
			     struct nlattr **tb)
{
	enum qca_wlan_twt_operation twt_oper;
	struct nlattr *twt_oper_attr;
	struct nlattr *twt_param_attr;
	uint32_t id;
	int ret = 0;

	id = QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION;
	twt_oper_attr = tb[id];

	if (!twt_oper_attr) {
		hdd_err("TWT operation NOT specified");
		return -EINVAL;
	}

	twt_oper = nla_get_u8(twt_oper_attr);

	id = QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS;
	twt_param_attr = tb[id];

	if (!twt_param_attr &&
	    twt_oper != QCA_WLAN_TWT_GET_CAPABILITIES) {
		hdd_err("TWT parameters NOT specified");
		return -EINVAL;
	}

	hdd_debug("TWT Operation 0x%x", twt_oper);

	switch (twt_oper) {
	case QCA_WLAN_TWT_SET:
		ret = hdd_twt_setup_session(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_GET:
		ret = hdd_twt_get_session_params(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_TERMINATE:
		ret = hdd_twt_terminate_session(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_SUSPEND:
		ret = hdd_twt_pause_session(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_RESUME:
		ret = hdd_twt_resume_session(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_NUDGE:
		ret = hdd_twt_nudge_session(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_GET_CAPABILITIES:
		ret = hdd_twt_get_capabilities(adapter, twt_param_attr);
		break;
	case QCA_WLAN_TWT_GET_STATS:
		ret = hdd_twt_get_session_traffic_stats(adapter,
							twt_param_attr);
		break;
	case QCA_WLAN_TWT_CLEAR_STATS:
		ret = hdd_twt_clear_session_traffic_stats(adapter,
							  twt_param_attr);
		break;
	default:
		hdd_err("Invalid TWT Operation");
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * __wlan_hdd_cfg80211_wifi_twt_config() - Wifi TWT configuration
 * vendor command
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX.
 *
 * Return: 0 for Success and negative value for failure
 */
static int
__wlan_hdd_cfg80211_wifi_twt_config(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX + 1];
	int errno;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX,
				    data,
				    data_len,
				    wlan_hdd_wifi_twt_config_policy)) {
		hdd_err("invalid twt attr");
		return -EINVAL;
	}

	errno = hdd_twt_configure(adapter, tb);

	return errno;
}

int wlan_hdd_cfg80211_wifi_twt_config(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_wifi_twt_config(wiphy, wdev, data,
						    data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

void hdd_update_tgt_twt_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg)
{
	struct wma_tgt_services *services = &cfg->services;
	bool twt_bcast_req;
	bool twt_bcast_res;
	bool twt_req, twt_res, enable_twt;

	enable_twt = ucfg_mlme_is_twt_enabled(hdd_ctx->psoc);

	ucfg_mlme_get_twt_requestor(hdd_ctx->psoc, &twt_req);

	ucfg_mlme_get_twt_responder(hdd_ctx->psoc, &twt_res);

	ucfg_mlme_get_twt_bcast_requestor(hdd_ctx->psoc,
					  &twt_bcast_req);

	ucfg_mlme_get_twt_bcast_responder(hdd_ctx->psoc,
					  &twt_bcast_res);

	hdd_debug("ini: enable_twt=%d, bcast_req=%d, bcast_res=%d",
		  enable_twt, twt_bcast_req, twt_bcast_res);
	hdd_debug("ini: twt_req=%d, twt_res=%d", twt_req, twt_res);
	hdd_debug("svc:  req=%d, res=%d, bcast_req=%d, bcast_res=%d legacy_bcast_twt:%d",
		  services->twt_requestor, services->twt_responder,
		  cfg->twt_bcast_req_support, cfg->twt_bcast_res_support,
		  cfg->legacy_bcast_twt_support);

	/*
	 * Set the twt fw responder service capability
	 */
	ucfg_mlme_set_twt_res_service_cap(hdd_ctx->psoc,
					  services->twt_responder);
	/*
	 * The HE cap IE in frame will have intersection of
	 * "enable_twt" ini, twt requestor fw service cap and
	 * "twt_requestor" ini requestor bit after this
	 * set operation.
	 */
	ucfg_mlme_set_twt_requestor(hdd_ctx->psoc,
				    QDF_MIN(services->twt_requestor,
					    (enable_twt && twt_req)));

	/*
	 * The HE cap IE in frame will have intersection of
	 * "enable_twt" ini, twt responder fw service cap and
	 * "twt_responder" ini responder bit after this
	 * set operation.
	 */
	ucfg_mlme_set_twt_responder(hdd_ctx->psoc,
				    QDF_MIN(services->twt_responder,
					    (enable_twt && twt_res)));
	/*
	 * The HE cap IE in frame will have intersection of
	 * "enable_twt" ini, twt requestor fw service cap and
	 * "twt_bcast_req_resp_config" ini requestor bit after this
	 * set operation.
	 */
	ucfg_mlme_set_twt_bcast_requestor(
			hdd_ctx->psoc,
			QDF_MIN((cfg->twt_bcast_req_support ||
				 cfg->legacy_bcast_twt_support),
				(enable_twt && twt_bcast_req)));

	/*
	 * The HE cap IE in frame will have intersection of
	 * "enable_twt" ini, twt responder fw service cap and
	 * "twt_bcast_req_resp_config" ini responder bit after this
	 * set operation.
	 */
	ucfg_mlme_set_twt_bcast_responder(
			hdd_ctx->psoc,
			QDF_MIN((cfg->twt_bcast_res_support ||
				 cfg->legacy_bcast_twt_support),
				(enable_twt && twt_bcast_res)));

	ucfg_mlme_set_twt_nudge_tgt_cap(hdd_ctx->psoc, cfg->twt_nudge_enabled);
	ucfg_mlme_set_twt_all_twt_tgt_cap(hdd_ctx->psoc,
					  cfg->all_twt_enabled);
	ucfg_mlme_set_twt_statistics_tgt_cap(hdd_ctx->psoc,
					     cfg->twt_stats_enabled);
}

QDF_STATUS hdd_send_twt_requestor_enable_cmd(struct hdd_context *hdd_ctx)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	struct twt_enable_disable_conf twt_en_dis = {0};
	bool is_requestor_en, twt_bcast_requestor = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ucfg_mlme_get_twt_requestor(hdd_ctx->psoc, &is_requestor_en);
	ucfg_mlme_get_twt_bcast_requestor(hdd_ctx->psoc, &twt_bcast_requestor);
	twt_en_dis.bcast_en = twt_bcast_requestor;

	ucfg_mlme_get_twt_congestion_timeout(hdd_ctx->psoc,
					     &twt_en_dis.congestion_timeout);
	hdd_debug("TWT mlme cfg:req: %d, bcast:%d, cong:%d, pdev:%d",
		  is_requestor_en, twt_en_dis.bcast_en,
		  twt_en_dis.congestion_timeout, pdev_id);

	/* The below code takes care of the following :
	 * If user wants to separately enable requestor role, and also the
	 * broadcast TWT capabilities separately for each role. This is done
	 * by reusing the INI configuration to indicate the user preference
	 * and sending the command accordingly.
	 * Legacy targets did not provide this. Newer targets provide this.
	 *
	 * 1. The MLME config holds the intersection of fw cap and user config
	 * 2. This may result in two enable commands sent for legacy, but
	 *    that's fine, since the firmware returns harmlessly for the
	 *    second command.
	 * 3. The new two parameters in the enable command are ignored
	 *    by legacy targets, and honored by new targets.
	 */

	/* If requestor configured, send requestor bcast/ucast config */
	if (is_requestor_en) {
		twt_en_dis.role = WMI_TWT_ROLE_REQUESTOR;
		twt_en_dis.ext_conf_present = true;
		if (twt_bcast_requestor)
			twt_en_dis.oper = WMI_TWT_OPERATION_BROADCAST;
		else
			twt_en_dis.oper = WMI_TWT_OPERATION_INDIVIDUAL;

		ucfg_mlme_set_twt_requestor_flag(hdd_ctx->psoc, true);
		qdf_event_reset(&hdd_ctx->twt_enable_comp_evt);
		wma_send_twt_enable_cmd(pdev_id, &twt_en_dis);
		status = qdf_wait_single_event(&hdd_ctx->twt_enable_comp_evt,
					       TWT_ENABLE_COMPLETE_TIMEOUT);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_warn("TWT Requestor Enable timedout");
			ucfg_mlme_set_twt_requestor_flag(hdd_ctx->psoc, false);
		}
	}

	return status;
}

QDF_STATUS hdd_send_twt_responder_enable_cmd(struct hdd_context *hdd_ctx)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	struct twt_enable_disable_conf twt_en_dis = {0};
	bool is_responder_en, twt_bcast_responder = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ucfg_mlme_get_twt_responder(hdd_ctx->psoc, &is_responder_en);
	ucfg_mlme_get_twt_bcast_responder(hdd_ctx->psoc, &twt_bcast_responder);
	twt_en_dis.bcast_en = twt_bcast_responder;

	hdd_debug("TWT responder mlme cfg:res:%d, bcast:%d, pdev:%d",
		  is_responder_en, twt_en_dis.bcast_en, pdev_id);

	/* The below code takes care of the following :
	 * If user wants to separately enable responder roles, and also the
	 * broadcast TWT capabilities separately for each role. This is done
	 * by reusing the INI configuration to indicate the user preference
	 * and sending the command accordingly.
	 * Legacy targets did not provide this. Newer targets provide this.
	 *
	 * 1. The MLME config holds the intersection of fw cap and user config
	 * 2. This may result in two enable commands sent for legacy, but
	 *    that's fine, since the firmware returns harmlessly for the
	 *    second command.
	 * 3. The new two parameters in the enable command are ignored
	 *    by legacy targets, and honored by new targets.
	 */

	/* If responder configured, send responder bcast/ucast config */
	if (is_responder_en) {
		twt_en_dis.role = WMI_TWT_ROLE_RESPONDER;
		twt_en_dis.ext_conf_present = true;
		if (twt_bcast_responder)
			twt_en_dis.oper = WMI_TWT_OPERATION_BROADCAST;
		else
			twt_en_dis.oper = WMI_TWT_OPERATION_INDIVIDUAL;

		ucfg_mlme_set_twt_responder_flag(hdd_ctx->psoc, true);
		qdf_event_reset(&hdd_ctx->twt_enable_comp_evt);
		wma_send_twt_enable_cmd(pdev_id, &twt_en_dis);
		status = qdf_wait_single_event(&hdd_ctx->twt_enable_comp_evt,
					       TWT_ENABLE_COMPLETE_TIMEOUT);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_warn("TWT Responder Enable timedout");
			ucfg_mlme_set_twt_responder_flag(hdd_ctx->psoc, false);
		}
	}

	return status;
}

QDF_STATUS hdd_send_twt_requestor_disable_cmd(struct hdd_context *hdd_ctx)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	struct twt_enable_disable_conf twt_en_dis = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	hdd_debug("TWT requestor disable cmd: pdev:%d", pdev_id);

	/* Set MLME TWT flag */
	ucfg_mlme_set_twt_requestor_flag(hdd_ctx->psoc, false);

       /* One disable should be fine, with extended configuration
	* set to false, and extended arguments will be ignored by target
	*/
	twt_en_dis.role = WMI_TWT_ROLE_REQUESTOR;
	hdd_ctx->twt_state = TWT_DISABLE_REQUESTED;
	twt_en_dis.ext_conf_present = false;
	qdf_event_reset(&hdd_ctx->twt_disable_comp_evt);
	wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);

	status = qdf_wait_single_event(&hdd_ctx->twt_disable_comp_evt,
				       TWT_DISABLE_COMPLETE_TIMEOUT);

	if (!QDF_IS_STATUS_SUCCESS(status))
		goto timeout;

	return status;

timeout:
	hdd_warn("TWT Requestor disable timedout");
	return status;
}

QDF_STATUS hdd_send_twt_responder_disable_cmd(struct hdd_context *hdd_ctx)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	struct twt_enable_disable_conf twt_en_dis = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	hdd_debug("TWT responder disable cmd: pdev:%d", pdev_id);

	/* Set MLME TWT flag */
	ucfg_mlme_set_twt_responder_flag(hdd_ctx->psoc, false);

       /* One disable should be fine, with extended configuration
	* set to false, and extended arguments will be ignored by target
	*/
	twt_en_dis.role = WMI_TWT_ROLE_RESPONDER;
	hdd_ctx->twt_state = TWT_DISABLE_REQUESTED;
	twt_en_dis.ext_conf_present = false;
	qdf_event_reset(&hdd_ctx->twt_disable_comp_evt);
	wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);

	status = qdf_wait_single_event(&hdd_ctx->twt_disable_comp_evt,
				       TWT_DISABLE_COMPLETE_TIMEOUT);

	if (!QDF_IS_STATUS_SUCCESS(status))
		goto timeout;

	return status;

timeout:
	hdd_warn("TWT Responder disable timedout");
	return status;
}

/**
 * hdd_twt_enable_comp_cb() - TWT enable complete event callback
 * @hdd_handle: Pointer to opaque HDD handle
 * @params: Pointer to TWT enable completion parameters
 *
 * Return: None
 */
static void
hdd_twt_enable_comp_cb(hdd_handle_t hdd_handle,
		       struct wmi_twt_enable_complete_event_param *params)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	enum twt_status prev_state;
	QDF_STATUS status;

	prev_state = hdd_ctx->twt_state;
	if (params->status == WMI_HOST_ENABLE_TWT_STATUS_OK ||
	    params->status == WMI_HOST_ENABLE_TWT_STATUS_ALREADY_ENABLED) {
		switch (prev_state) {
		case TWT_FW_TRIGGER_ENABLE_REQUESTED:
			hdd_ctx->twt_state = TWT_FW_TRIGGER_ENABLED;
			break;
		case TWT_HOST_TRIGGER_ENABLE_REQUESTED:
			hdd_ctx->twt_state = TWT_HOST_TRIGGER_ENABLED;
			break;
		default:
			break;
		}
	}

	if (params->status == WMI_HOST_ENABLE_TWT_INVALID_PARAM ||
	    params->status == WMI_HOST_ENABLE_TWT_STATUS_UNKNOWN_ERROR)
		hdd_ctx->twt_state = TWT_INIT;

	hdd_debug("TWT: pdev ID:%d, status:%d State transitioned from %d to %d",
		  params->pdev_id, params->status,
		  prev_state, hdd_ctx->twt_state);

	status = qdf_event_set(&hdd_ctx->twt_enable_comp_evt);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to set twt_enable_comp_evt");
}

/**
 * hdd_twt_disable_comp_cb() - TWT disable complete event callback
 * @hdd_handle: opaque handle for the global HDD Context
 *
 * Return: None
 */
static void
hdd_twt_disable_comp_cb(hdd_handle_t hdd_handle)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	enum twt_status prev_state;
	QDF_STATUS status;

	prev_state = hdd_ctx->twt_state;
	/* Do not change the state for role specific disables */
	if (hdd_ctx->twt_state == TWT_DISABLE_REQUESTED)
		hdd_ctx->twt_state = TWT_DISABLED;

	hdd_debug("TWT: State transitioned from %d to %d",
		  prev_state, hdd_ctx->twt_state);

	status = qdf_event_set(&hdd_ctx->twt_disable_comp_evt);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to set twt_disable_comp_evt");
}

void hdd_send_twt_role_disable_cmd(struct hdd_context *hdd_ctx,
				   enum twt_role role)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	struct twt_enable_disable_conf twt_en_dis = {0};
	QDF_STATUS status;

	hdd_debug("TWT disable cmd :pdev:%d : %d", pdev_id, role);

	if ((role == TWT_REQUESTOR) || (role == TWT_REQUESTOR_INDV)) {
		twt_en_dis.ext_conf_present = true;
		twt_en_dis.role = WMI_TWT_ROLE_REQUESTOR;
		twt_en_dis.oper = WMI_TWT_OPERATION_INDIVIDUAL;
		wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);
	}

	if (role == TWT_REQUESTOR_BCAST) {
		twt_en_dis.ext_conf_present = true;
		twt_en_dis.role = WMI_TWT_ROLE_REQUESTOR;
		twt_en_dis.oper = WMI_TWT_OPERATION_BROADCAST;
		wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);
	}

	if ((role == TWT_RESPONDER) || (role == TWT_RESPONDER_INDV)) {
		twt_en_dis.ext_conf_present = true;
		twt_en_dis.role = WMI_TWT_ROLE_RESPONDER;
		twt_en_dis.oper = WMI_TWT_OPERATION_INDIVIDUAL;
		wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);
	}

	if (role == TWT_RESPONDER_BCAST) {
		twt_en_dis.ext_conf_present = true;
		twt_en_dis.role = WMI_TWT_ROLE_RESPONDER;
		twt_en_dis.oper = WMI_TWT_OPERATION_BROADCAST;
		wma_send_twt_disable_cmd(pdev_id, &twt_en_dis);
	}

	status = qdf_wait_single_event(&hdd_ctx->twt_disable_comp_evt,
				       TWT_DISABLE_COMPLETE_TIMEOUT);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_warn("TWT request disable timedout");
		return;
	}
}

void hdd_twt_concurrency_update_on_scc_mcc(struct wlan_objmgr_pdev *pdev,
					   void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct twt_conc_arg *twt_arg = arg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		hdd_debug("Concurrency exist on SAP vdev");
		status = hdd_send_twt_responder_disable_cmd(twt_arg->hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("TWT responder disable cmd to firmware failed");
			return;
		}
		sme_twt_update_beacon_template(twt_arg->hdd_ctx->mac_handle);
	}

	if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		hdd_debug("Concurrency exist on STA vdev");
		status = hdd_send_twt_requestor_disable_cmd(twt_arg->hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("TWT requestor disable cmd to firmware failed");
			return;
		}
	}
}

void hdd_twt_concurrency_update_on_dbs(struct wlan_objmgr_pdev *pdev,
				       void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct twt_conc_arg *twt_arg = arg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (vdev->vdev_mlme.vdev_opmode == QDF_SAP_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		hdd_debug("SAP vdev exist");
		status = hdd_send_twt_responder_enable_cmd(twt_arg->hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("TWT responder enable cmd to firmware failed");
			return;
		}
		sme_twt_update_beacon_template(twt_arg->hdd_ctx->mac_handle);
	}

	if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		hdd_debug("STA vdev exist");
		status = hdd_send_twt_requestor_enable_cmd(twt_arg->hdd_ctx);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("TWT requestor enable cmd to firmware failed");
			return;
		}
	}
}

void __hdd_twt_update_work_handler(struct hdd_context *hdd_ctx)
{
	struct twt_conc_arg twt_arg;
	uint32_t num_connections = 0, sap_count = 0, sta_count = 0;
	int ret;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret) {
		hdd_err("Invalid HDD context");
		return;
	}
	num_connections = policy_mgr_get_connection_count(hdd_ctx->psoc);
	sta_count = policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
							      PM_STA_MODE,
							      NULL);
	sap_count = policy_mgr_mode_specific_connection_count(hdd_ctx->psoc,
							      PM_SAP_MODE,
							      NULL);
	twt_arg.hdd_ctx = hdd_ctx;

	hdd_debug("Total connection %d, sta_count %d, sap_count %d",
		  num_connections, sta_count, sap_count);
	switch (num_connections) {
	case 1:
		if (sta_count == 1) {
			hdd_send_twt_requestor_enable_cmd(hdd_ctx);
		} else if (sap_count == 1) {
			hdd_send_twt_responder_enable_cmd(hdd_ctx);
			sme_twt_update_beacon_template(hdd_ctx->mac_handle);
		}
		break;
	case 2:
		if (policy_mgr_current_concurrency_is_scc(hdd_ctx->psoc) ||
		    policy_mgr_current_concurrency_is_mcc(hdd_ctx->psoc)) {
			status = wlan_objmgr_pdev_iterate_obj_list(
					hdd_ctx->pdev,
					WLAN_VDEV_OP,
					hdd_twt_concurrency_update_on_scc_mcc,
					&twt_arg, 0,
					WLAN_HDD_ID_OBJ_MGR);
			if (QDF_IS_STATUS_ERROR(status)) {
				hdd_err("SAP not in SCC/MCC concurrency");
				return;
			}
		} else if (policy_mgr_is_current_hwmode_dbs(hdd_ctx->psoc)) {
			status = wlan_objmgr_pdev_iterate_obj_list(
					hdd_ctx->pdev,
					WLAN_VDEV_OP,
					hdd_twt_concurrency_update_on_dbs,
					&twt_arg, 0,
					WLAN_HDD_ID_OBJ_MGR);
			if (QDF_IS_STATUS_ERROR(status)) {
				hdd_err("SAP not in DBS case");
				return;
			}
		}
		break;
	case 3:
		status = wlan_objmgr_pdev_iterate_obj_list(
					hdd_ctx->pdev,
					WLAN_VDEV_OP,
					hdd_twt_concurrency_update_on_scc_mcc,
					&twt_arg, 0,
					WLAN_HDD_ID_OBJ_MGR);
		break;
	default:
		hdd_err("Unexpected number of connection");
		break;
	}
}

void hdd_twt_update_work_handler(void *data)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *)data;
	struct osif_psoc_sync *psoc_sync;
	int ret;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return;

	__hdd_twt_update_work_handler(hdd_ctx);

	osif_psoc_sync_op_stop(psoc_sync);
}

void wlan_twt_concurrency_update(struct hdd_context *hdd_ctx)
{
	qdf_sched_work(0, &hdd_ctx->twt_en_dis_work);
}

void hdd_twt_del_dialog_in_ps_disable(struct hdd_context *hdd_ctx,
				      struct qdf_mac_addr *mac_addr,
				      uint8_t vdev_id)
{
	struct wmi_twt_del_dialog_param params = {0};
	int ret;

	params.dialog_id = WLAN_ALL_SESSIONS_DIALOG_ID;
	params.vdev_id = vdev_id;
	qdf_mem_copy(params.peer_macaddr, mac_addr->bytes, QDF_MAC_ADDR_SIZE);

	if (ucfg_mlme_is_twt_setup_done(hdd_ctx->psoc, mac_addr,
					params.dialog_id)) {
		hdd_debug("vdev%d: Terminate existing TWT session %d due to ps disable",
			  params.vdev_id, params.dialog_id);
		ret = hdd_send_twt_del_dialog_cmd(hdd_ctx, &params);
		if (ret)
			hdd_debug("TWT teardown is failed on vdev: %d", vdev_id);
	}
}

void wlan_hdd_twt_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	struct twt_callbacks twt_cb;

	hdd_ctx->twt_state = TWT_INIT;

	sme_clear_twt_complete_cb(hdd_ctx->mac_handle);
	twt_cb.twt_enable_cb = hdd_twt_enable_comp_cb;
	twt_cb.twt_disable_cb = hdd_twt_disable_comp_cb;
	twt_cb.twt_add_dialog_cb = hdd_twt_add_dialog_comp_cb;
	twt_cb.twt_del_dialog_cb = hdd_twt_del_dialog_comp_cb;
	twt_cb.twt_pause_dialog_cb = hdd_twt_pause_dialog_comp_cb;
	twt_cb.twt_resume_dialog_cb = hdd_twt_resume_dialog_comp_cb;
	twt_cb.twt_notify_cb = hdd_twt_notify_cb;
	twt_cb.twt_nudge_dialog_cb = hdd_twt_nudge_dialog_comp_cb;
	twt_cb.twt_ack_comp_cb = hdd_twt_ack_comp_cb;

	status = sme_register_twt_callbacks(hdd_ctx->mac_handle, &twt_cb);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Register twt enable complete failed");
		return;
	}

	status = qdf_event_create(&hdd_ctx->twt_enable_comp_evt);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_clear_twt_complete_cb(hdd_ctx->mac_handle);
		hdd_err("twt_enable_comp_evt init failed");
		return;
	}

	status = qdf_event_create(&hdd_ctx->twt_disable_comp_evt);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_clear_twt_complete_cb(hdd_ctx->mac_handle);
		hdd_err("twt_disable_comp_evt init failed");
		return;
	}

	hdd_send_twt_requestor_enable_cmd(hdd_ctx);
	qdf_create_work(0, &hdd_ctx->twt_en_dis_work,
			hdd_twt_update_work_handler, hdd_ctx);
}

void wlan_hdd_twt_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	qdf_flush_work(&hdd_ctx->twt_en_dis_work);
	qdf_destroy_work(NULL, &hdd_ctx->twt_en_dis_work);

	status = qdf_event_destroy(&hdd_ctx->twt_disable_comp_evt);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to destroy twt_disable_comp_evt");

	status = qdf_event_destroy(&hdd_ctx->twt_enable_comp_evt);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to destroy twt_enable_comp_evt");

	status = sme_clear_twt_complete_cb(hdd_ctx->mac_handle);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("De-register of twt disable cb failed: %d", status);

	hdd_ctx->twt_state = TWT_CLOSED;
}
