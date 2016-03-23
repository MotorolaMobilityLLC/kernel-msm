/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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
 * DOC: wlan_hdd_nan_datapath.c
 *
 * WLAN Host Device Driver nan datapath API implementation
 */
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include "vos_trace.h"
#include "vos_sched.h"
#include "wlan_hdd_includes.h"
#include "wlan_hdd_p2p.h"
#include "wma_api.h"

/* NLA policy */
static const struct nla_policy
qca_wlan_vendor_ndp_policy[QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR] = { .type = NLA_STRING,
					.len = IFNAMSIZ },
	[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_SPEC_CHANNEL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR] = {
						.type = NLA_BINARY,
						.len = VOS_MAC_ADDR_SIZE },
	[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS] = { .type = NLA_BINARY,
					.len = NDP_QOS_INFO_LEN },
	[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO_LEN] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO] = { .type = NLA_BINARY,
					.len = NDP_APP_INFO_LEN },
	[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_RESPONSE_CODE] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_STATUS_CODE] = { .type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR] = { .type = NLA_BINARY,
					.len = VOS_MAC_ADDR_SIZE },
};

/**
 * hdd_ndp_print_ini_config()- Print nan datapath specific INI configuration
 * @hdd_ctx: handle to hdd context
 *
 * Return: None
 */
void hdd_ndp_print_ini_config(hdd_context_t *hdd_ctx)
{
	hddLog(LOG2, "Name = [%s] Value = [%u]",
		CFG_ENABLE_NAN_DATAPATH_NAME,
		hdd_ctx->cfg_ini->enable_nan_datapath);
	hddLog(LOG2, "Name = [%s] Value = [%u]",
		CFG_ENABLE_NAN_NDI_CHANNEL_NAME,
		hdd_ctx->cfg_ini->nan_datapath_ndi_channel);
}

/**
 * hdd_nan_datapath_target_config() - Configure NAN datapath features
 * @hdd_ctx: Pointer to HDD context
 * @cfg: Pointer to target device capability information
 *
 * NAN datapath functinality is enabled if it is enabled in
 * .ini file and also supported in firmware.
 *
 * Return: None
 */
void hdd_nan_datapath_target_config(hdd_context_t *hdd_ctx,
					struct hdd_tgt_cfg *cfg)
{
	hdd_ctx->cfg_ini->enable_nan_datapath &= cfg->nan_datapath_enabled;
	hddLog(LOG1, FL("enable_nan_datapath: %d"),
		hdd_ctx->cfg_ini->enable_nan_datapath);
}

/**
 * hdd_ndi_start_bss() - Start BSS on NAN data interface
 * @adapter: adapter context
 * @operating_channel: channel on which the BSS to be started
 *
 * Return: 0 on success, error value on failure
 */
static int hdd_ndi_start_bss(hdd_adapter_t *adapter,
				uint8_t operating_channel)
{
	int ret;
	uint16_t ch_width;
	uint32_t roam_id;
	hdd_wext_state_t *wext_state =
		WLAN_HDD_GET_NDP_WEXT_STATE_PTR(adapter);
	tCsrRoamProfile *roam_profile = &wext_state->roamProfile;

	ENTER();

	if (!roam_profile) {
		hddLog(LOGE, FL("No valid roam profile"));
		return -EINVAL;
	}

	if (HDD_WMM_USER_MODE_NO_QOS ==
		(WLAN_HDD_GET_CTX(adapter))->cfg_ini->WmmMode) {
		/* QoS not enabled in cfg file*/
		roam_profile->uapsd_mask = 0;
	} else {
		/* QoS enabled, update uapsd mask from cfg file*/
		roam_profile->uapsd_mask =
			(WLAN_HDD_GET_CTX(adapter))->cfg_ini->UapsdMask;
	}

	roam_profile->csrPersona = adapter->device_mode;

	roam_profile->ChannelInfo.numOfChannels = 1;
	if (operating_channel) {
		roam_profile->ChannelInfo.ChannelList = &operating_channel;
	} else {
		roam_profile->ChannelInfo.ChannelList[0] =
			NAN_SOCIAL_CHANNEL_2_4GHZ;
	}
	hdd_select_cbmode(adapter, operating_channel, &ch_width);
	roam_profile->vht_channel_width = ch_width;

	roam_profile->SSIDs.numOfSSIDs = 1;
	roam_profile->SSIDs.SSIDList->SSID.length = 0;

	roam_profile->phyMode = eCSR_DOT11_MODE_11ac;
	roam_profile->BSSType = eCSR_BSS_TYPE_NDI;
	roam_profile->BSSIDs.numOfBSSIDs = 1;
	vos_mem_copy((void *)(roam_profile->BSSIDs.bssid),
		&adapter->macAddressCurrent.bytes[0],
		VOS_MAC_ADDR_SIZE);

	roam_profile->AuthType.numEntries = 1;
	roam_profile->AuthType.authType[0] = eCSR_AUTH_TYPE_OPEN_SYSTEM;
	roam_profile->EncryptionType.numEntries = 1;
	roam_profile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;

	ret = sme_RoamConnect(WLAN_HDD_GET_HAL_CTX(adapter),
		adapter->sessionId, roam_profile, &roam_id);
	if (eHAL_STATUS_SUCCESS != ret) {
		hddLog(LOGE,
			FL("NDI sme_RoamConnect session %d failed with status %d -> NotConnected"),
			  adapter->sessionId, ret);
		/* change back to NotConnected */
		hdd_connSetConnectionState(adapter,
			eConnectionState_NotConnected);
	} else {
		hddLog(LOG2, FL("sme_RoamConnect issued successfully for NDI"));
	}

	roam_profile->ChannelInfo.ChannelList = NULL;
	roam_profile->ChannelInfo.numOfChannels = 0;

	EXIT();

	return ret;
}

/**
 * hdd_ndi_create_req_handler() - NDI create request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return: 0 on success or error code on failure
 */
static int hdd_ndi_create_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	hdd_adapter_t *adapter;
	char *iface_name;
	uint16_t transaction_id;
	int ret;
	struct nan_datapath_ctx *ndp_ctx;
	uint8_t op_channel =
		hdd_ctx->cfg_ini->nan_datapath_ndi_channel;

	ENTER();

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		hddLog(LOGE, FL("Interface name string is unavailable"));
		return -EINVAL;
	}
	iface_name = nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		hddLog(LOGE, FL("transaction id is unavailable"));
		return -EINVAL;
	}
	transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	/* Reject if there is an existing interface with same name */
	adapter = hdd_get_adapter_by_name(hdd_ctx, iface_name);
	if (adapter) {
		hddLog(LOGE, FL("Interface %s already exists"),
			iface_name);
		return -EEXIST;
	}

	/* Check for an existing interface of NDI type */
	adapter = hdd_get_adapter(hdd_ctx, WLAN_HDD_NDI);
	if (adapter) {
		hddLog(LOGE, FL("Cannot support more than one NDI"));
		return -EINVAL;
	}

	adapter = hdd_open_adapter(hdd_ctx, WLAN_HDD_NDI, iface_name,
			wlan_hdd_get_intf_addr(hdd_ctx), VOS_TRUE);
	if (!adapter) {
		hddLog(LOGE, FL("hdd_open_adapter failed"));
		return -ENOMEM;
	}

	/*
	 * Create transaction id is required to be saved since the firmware
	 * does not honor the transaction id for create request
	 */
	ndp_ctx = WLAN_HDD_GET_NDP_CTX_PTR(adapter);
	ndp_ctx->ndp_create_transaction_id = transaction_id;
	ndp_ctx->state = NAN_DATA_NDI_CREATING_STATE;

	/*
	 * The NAN data interface has been created at this point.
	 * Unlike traditional device modes, where the higher application
	 * layer initiates connect / join / start, the NAN data interface
	 * does not have any such formal requests. The NDI create request
	 * is responsible for starting the BSS as well.
	 */
	if (op_channel != NAN_SOCIAL_CHANNEL_2_4GHZ ||
	    op_channel != NAN_SOCIAL_CHANNEL_5GHZ_LOWER_BAND ||
	    op_channel != NAN_SOCIAL_CHANNEL_5GHZ_UPPER_BAND) {
		/* start NDI on the default 2.4 GHz social channel */
		op_channel = NAN_SOCIAL_CHANNEL_2_4GHZ;
	}
	ret = hdd_ndi_start_bss(adapter, op_channel);
	if (0 > ret)
		hddLog(LOGE, FL("NDI start bss failed"));

	EXIT();
	return ret;
}

/**
 * hdd_ndi_delete_req_handler() - NDI delete request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return: 0 on success or error code on failure
 */
static int hdd_ndi_delete_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	hdd_adapter_t *adapter;
	char *iface_name;
	uint16_t transaction_id;
	struct nan_datapath_ctx *ndp_ctx;
	int ret;

	ENTER();

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		hddLog(LOGE, FL("Interface name string is unavailable"));
		return -EINVAL;
	}

	iface_name = nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		hddLog(LOGE, FL("Transaction id is unavailable"));
		return -EINVAL;
	}

	transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	/* Check if there is already an existing inteface with the same name */
	adapter = hdd_get_adapter_by_name(hdd_ctx, iface_name);
	if (!adapter) {
		hddLog(LOGE, FL("NAN data interface %s is not available"),
			iface_name);
		return -EINVAL;
	}

	/* check if adapter is in NDI mode */
	if (WLAN_HDD_NDI != adapter->device_mode) {
		hddLog(LOGE, FL("Interface %s is not in NDI mode"),
			iface_name);
		return -EINVAL;
	}

	ndp_ctx = WLAN_HDD_GET_NDP_CTX_PTR(adapter);
	if (!ndp_ctx) {
		hddLog(LOGE, FL("ndp_ctx is NULL"));
		return -EINVAL;
	}

	/* check if there are active NDP sessions on the adapter */
	if (ndp_ctx->active_ndp_sessions > 0) {
		hddLog(LOGE, FL("NDP sessions active %d, cannot delete NDI"),
			ndp_ctx->active_ndp_sessions);
		return -EINVAL;
	}

	ndp_ctx->ndp_delete_transaction_id = transaction_id;
	ndp_ctx->state = NAN_DATA_NDI_DELETING_STATE;

	/* Delete the interface */
	ret = __wlan_hdd_del_virtual_intf(hdd_ctx->wiphy, &adapter->wdev);
	if (ret < 0)
		hddLog(LOGE, FL("NDI delete request failed"));
	else
		hddLog(LOGE, FL("NDI delete request successfully issued"));

	return ret;
}


/**
 * hdd_ndp_initiator_req_handler() - NDP initiator request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return:  0 on success or error code on failure
 */
static int hdd_ndp_initiator_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	return 0;
}

/**
 * hdd_ndp_responder_req_handler() - NDP responder request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return: 0 on success or error code on failure
 */
static int hdd_ndp_responder_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	return 0;
}

/**
 * hdd_ndp_end_req_handler() - NDP end request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return: 0 on success or error code on failure
 */
static int hdd_ndp_end_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	return 0;
}

/**
 * hdd_ndp_schedule_req_handler() - NDP schedule request handler
 * @hdd_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * Return: 0 on success or error code on failure
 */
static int hdd_ndp_schedule_req_handler(hdd_context_t *hdd_ctx,
						struct nlattr **tb)
{
	return 0;
}


/**
 * hdd_ndp_iface_create_rsp_handler() - NDP iface create response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_iface_create_rsp_handler(hdd_adapter_t *adapter,
							void *rsp_params)
{
	struct sk_buff *vendor_event;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct ndi_create_rsp *ndi_rsp = (struct ndi_create_rsp *)rsp_params;
	uint32_t data_len = (2 * sizeof(uint32_t)) + sizeof(uint16_t) +
				NLMSG_HDRLEN + (3 * NLA_HDRLEN);
	struct nan_datapath_ctx *ndp_ctx = WLAN_HDD_GET_NDP_CTX_PTR(adapter);

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (!ndi_rsp) {
		hddLog(LOGE, FL("Invalid ndi create response"));
		return;
	}

	if (!ndp_ctx) {
		hddLog(LOGE, FL("ndp_ctx is NULL"));
		return;
	}

	/* notify response to the upper layer */
	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
				NULL,
				data_len,
				QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				vos_get_gfp_flags());

	if (!vendor_event) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	/* Sub vendor command */
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
		QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE)) {
		hddLog(LOGE, FL("QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD put fail"));
		goto nla_put_failure;
	}

	/* Transaction id */
	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
		ndp_ctx->ndp_create_transaction_id)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_TRANSACTION_ID put fail"));
		goto nla_put_failure;
	}

	/* Status code */
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE,
		ndi_rsp->status)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_DRV_RETURN_TYPE put fail"));
		goto nla_put_failure;
	}

	/* Status return value */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE, 0xA5)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_DRV_RETURN_VALUE put fail"));
		goto nla_put_failure;
	}

	hddLog(LOG2, FL("sub command: %d, value: %d"),
		QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
		QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE);
	hddLog(LOG2, FL("create transaction id: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
		ndp_ctx->ndp_create_transaction_id);
	hddLog(LOG2, FL("status code: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE, ndi_rsp->status);
	hddLog(LOG2, FL("Return value: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE, 0xA5);

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	ndp_ctx->ndp_create_transaction_id = 0;
	ndp_ctx->state = NAN_DATA_NDI_CREATED_STATE;

	if (ndi_rsp->status == VOS_STATUS_SUCCESS) {
		hddLog(LOGE, FL("NDI interface successfully created"));
	} else {
		hddLog(LOGE,
			FL("NDI interface creation failed with reason %d"),
			ndi_rsp->reason);
	}
	EXIT();
	return;

nla_put_failure:
	kfree_skb(vendor_event);
	return;
}

/**
 * hdd_ndp_iface_delete_rsp_handler() - NDP iface delete response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_iface_delete_rsp_handler(hdd_adapter_t *adapter,
							void *rsp_params)
{
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct ndi_delete_rsp *ndi_rsp = rsp_params;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	if (!ndi_rsp) {
		hddLog(LOGE, FL("Invalid ndi delete response"));
		return;
	}

	if (ndi_rsp->status == VOS_STATUS_SUCCESS)
		hddLog(LOGE, FL("NDI BSS successfully stopped"));
	else
		hddLog(LOGE,
			FL("NDI BSS stop failed with reason %d"),
			ndi_rsp->reason);

	complete(&adapter->disconnect_comp_var);
	return;
}

/**
 * hdd_ndp_session_end_handler() - NDI session termination handler
 * @adapter: pointer to adapter context
 *
 * Return: none
 */
void hdd_ndp_session_end_handler(hdd_adapter_t *adapter)
{
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct sk_buff *vendor_event;
	struct nan_datapath_ctx *ndp_ctx;
	uint32_t data_len = sizeof(uint32_t) * 2 + sizeof(uint16_t) +
				NLA_HDRLEN * 3 + NLMSG_HDRLEN;

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	/* Handle only if adapter is in NDI mode */
	if (WLAN_HDD_NDI != adapter->device_mode) {
		hddLog(LOGE, FL("Adapter is not in NDI mode"));
		return;
	}

	ndp_ctx = WLAN_HDD_GET_NDP_CTX_PTR(adapter);
	if (!ndp_ctx) {
		hddLog(LOGE, FL("ndp context is NULL"));
		return;
	}

	/*
	 * The virtual adapters are stopped and closed even during
	 * driver unload or stop, the service layer is not required
	 * to be informed in that case (response is not expected)
	 */
	if (NAN_DATA_NDI_DELETING_STATE != ndp_ctx->state) {
		hddLog(LOGE, FL("NDI interface %s deleted"),
			adapter->dev->name);
		return;
	}

	/* notify response to the upper layer */
	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			NULL,
			data_len,
			QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	/* Sub vendor command goes first */
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_SUBCMD put fail"));
		goto failure;
	}

	/* Transaction id */
	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
			ndp_ctx->ndp_delete_transaction_id)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_TRANSACTION_ID put fail"));
		goto failure;
	}

	/* Status code */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE, 0x0)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_DRV_RETURN_TYPE put fail"));
		goto failure;
	}

	/* Status return value */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE, 0x0)) {
		hddLog(LOGE, FL("VENDOR_ATTR_NDP_DRV_RETURN_VALUE put fail"));
		goto failure;
	}

	hddLog(LOG2, FL("sub command: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
		QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE);
	hddLog(LOG2, FL("delete transaction id: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
		ndp_ctx->ndp_delete_transaction_id);
	hddLog(LOG2, FL("status code: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE,
		true);
	hddLog(LOG2, FL("Return value: %d, value: %d"),
		QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE, 0x5A);

	ndp_ctx->ndp_delete_transaction_id = 0;
	ndp_ctx->state = NAN_DATA_NDI_DELETED_STATE;

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	EXIT();
	return;

failure:
	kfree_skb(vendor_event);
}


/**
 * hdd_ndp_initiator_rsp_handler() - NDP initiator response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_initiator_rsp_handler(hdd_adapter_t *adapter,
						void *rsp_params)
{
	return;
}

/**
 * hdd_ndp_new_peer_ind_handler() - NDP new peer indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void hdd_ndp_new_peer_ind_handler(hdd_adapter_t *adapter,
						void *ind_params)
{
	return;
}

/**
 * hdd_ndp_peer_departed_ind_handler() - NDP peer departed indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void hdd_ndp_peer_departed_ind_handler(
				hdd_adapter_t *adapter, void *ind_params)
{
	return;
}

/**
 * hdd_ndp_confirm_ind_handler() - NDP confirm indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void hdd_ndp_confirm_ind_handler(hdd_adapter_t *adapter,
						void *ind_params)
{
	return;
}

/**
 * hdd_ndp_indication_handler() - NDP indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void hdd_ndp_indication_handler(hdd_adapter_t *adapter,
						void *ind_params)
{
	return;
}

/**
 * hdd_ndp_responder_rsp_handler() - NDP responder response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_responder_rsp_handler(hdd_adapter_t *adapter,
							void *rsp_params)
{
	return;
}

/**
 * hdd_ndp_end_rsp_handler() - NDP end response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_end_rsp_handler(hdd_adapter_t *adapter,
						void *rsp_params)
{
	return;
}

/**
 * hdd_ndp_end_ind_handler() - NDP end indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void hdd_ndp_end_ind_handler(hdd_adapter_t *adapter,
						void *ind_params)
{
	return;
}

/**
 * hdd_ndp_schedule_update_rsp_handler() - NDP schedule update response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void hdd_ndp_schedule_update_rsp_handler(
				hdd_adapter_t *adapter, void *rsp_params)
{
	return;
}

/**
 * hdd_ndp_event_handler() - ndp response and indication handler
 * @adapter: adapter context
 * @roam_info: pointer to roam_info structure
 * @roam_id: roam id as indicated by SME
 * @roam_status: roam status
 * @roam_result: roam result
 *
 * Return: none
 */
void hdd_ndp_event_handler(hdd_adapter_t *adapter,
	tCsrRoamInfo *roam_info, uint32_t roam_id, eRoamCmdStatus roam_status,
	eCsrRoamResult roam_result)
{
	if (roam_status == eCSR_ROAM_NDP_STATUS_UPDATE) {
		switch (roam_result) {
		case eCSR_ROAM_RESULT_NDP_CREATE_RSP:
			hdd_ndp_iface_create_rsp_handler(adapter,
				&roam_info->ndp.ndi_create_params);
			break;
		case eCSR_ROAM_RESULT_NDP_DELETE_RSP:
			hdd_ndp_iface_delete_rsp_handler(adapter,
				&roam_info->ndp.ndi_delete_params);
			break;
		case eCSR_ROAM_RESULT_NDP_INITIATOR_RSP:
			hdd_ndp_initiator_rsp_handler(adapter,
				&roam_info->ndp.ndp_init_rsp_params);
			break;
		case eCSR_ROAM_RESULT_NDP_NEW_PEER_IND:
			hdd_ndp_new_peer_ind_handler(adapter,
				&roam_info->ndp.ndp_peer_ind_params);
			break;
		case eCSR_ROAM_RESULT_NDP_CONFIRM_IND:
			hdd_ndp_confirm_ind_handler(adapter,
				&roam_info->ndp.ndp_confirm_params);
			break;
		case eCSR_ROAM_RESULT_NDP_INDICATION:
			hdd_ndp_indication_handler(adapter,
				&roam_info->ndp.ndp_indication_params);
			break;
		case eCSR_ROAM_RESULT_NDP_SCHED_UPDATE_RSP:
			hdd_ndp_schedule_update_rsp_handler(adapter,
				&roam_info->ndp.ndp_sched_upd_rsp_params);
			break;
		case eCSR_ROAM_RESULT_NDP_RESPONDER_RSP:
			hdd_ndp_responder_rsp_handler(adapter,
				&roam_info->ndp.ndp_responder_rsp_params);
			break;
		case eCSR_ROAM_RESULT_NDP_END_RSP:
			hdd_ndp_end_rsp_handler(adapter,
				&roam_info->ndp.ndp_end_rsp_params);
			break;
		case eCSR_ROAM_RESULT_NDP_PEER_DEPARTED_IND:
			hdd_ndp_peer_departed_ind_handler(adapter,
				&roam_info->ndp.ndp_peer_ind_params);
			break;
		case eCSR_ROAM_RESULT_NDP_END_IND:
			hdd_ndp_end_ind_handler(adapter,
				&roam_info->ndp.ndp_end_ind_params);
			break;
		default:
			hddLog(LOGE,
				FL("Unknown NDP response event from SME %d"),
				roam_result);
			break;
		}
	}
}

/**
 * __wlan_hdd_cfg80211_process_ndp_cmds() - handle NDP request
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is invoked to handle vendor command
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	uint32_t ndp_cmd_type;
	uint16_t transaction_id;
	int ret_val;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];
	char *iface_name;

	ENTER();

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EPERM;
	}
	if (!hdd_ctx->cfg_ini->enable_nan_datapath) {
		hddLog(LOGE, FL("NAN datapath is not suported"));
		return -EPERM;
	}
	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX,
			data, data_len,
			qca_wlan_vendor_ndp_policy)) {
		hddLog(LOGE, FL("Invalid NDP vendor command attributes"));
		return -EINVAL;
	}

	/* Parse and fetch NDP Command Type*/
	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD]) {
		hddLog(LOGE, FL("NAN datapath cmd type failed"));
		return -EINVAL;
	}
	ndp_cmd_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		hddLog(LOGE, FL("attr transaction id failed"));
		return -EINVAL;
	}
	transaction_id = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		hddLog(LOGE, FL("Interface name string is unavailable"));
		return -EINVAL;
	}
	iface_name = nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);

	hddLog(LOG2, FL("Transaction Id: %d NDP Cmd: %d iface_name: %s"),
		transaction_id, ndp_cmd_type, iface_name);

	switch (ndp_cmd_type) {
	case QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE:
		ret_val  = hdd_ndi_create_req_handler(hdd_ctx, tb);
		break;
	case QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE:
		ret_val = hdd_ndi_delete_req_handler(hdd_ctx, tb);
		break;
	case QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST:
		ret_val = hdd_ndp_initiator_req_handler(hdd_ctx, tb);
		break;
	case QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST:
		ret_val = hdd_ndp_responder_req_handler(hdd_ctx, tb);
		break;
	case QCA_WLAN_VENDOR_ATTR_NDP_END_REQUEST:
		ret_val = hdd_ndp_end_req_handler(hdd_ctx, tb);
		break;
	case QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REQUEST:
		ret_val = hdd_ndp_schedule_req_handler(hdd_ctx, tb);
		break;
	default:
		hddLog(LOGE, FL("Unrecognized NDP vendor cmd %d"),
			ndp_cmd_type);
		ret_val = -EINVAL;
		break;
	}

	return ret_val;
}

/**
 * wlan_hdd_cfg80211_process_ndp_cmd() - handle NDP request
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is called to send a NAN request to
 * firmware. This is an SSR-protected wrapper function.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_process_ndp_cmd(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_init_nan_data_mode() - initialize nan data mode
 * @adapter: adapter context
 *
 * Returns: 0 on success negative error code on error
 */
int hdd_init_nan_data_mode(struct hdd_adapter_s *adapter)
{
	struct net_device *wlan_dev = adapter->dev;
	struct nan_datapath_ctx *ndp_ctx = WLAN_HDD_GET_NDP_CTX_PTR(adapter);
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	eHalStatus hal_status;
	VOS_STATUS status;
	uint32_t type, sub_type;
	int32_t ret_val = 0;
	unsigned long rc;
	uint32_t timeout = WLAN_WAIT_TIME_SESSIONOPENCLOSE;

	INIT_COMPLETION(adapter->session_open_comp_var);
	sme_SetCurrDeviceMode(hdd_ctx->hHal, adapter->device_mode);
	sme_set_pdev_ht_vht_ies(hdd_ctx->hHal, hdd_ctx->cfg_ini->enable2x2);
	status = vos_get_vdev_types(adapter->device_mode, &type, &sub_type);
	if (VOS_STATUS_SUCCESS != status) {
		hddLog(LOGE, "failed to get vdev type");
		goto error_sme_open;
	}

	/* open sme session for future use */
	hal_status = sme_OpenSession(hdd_ctx->hHal, hdd_smeRoamCallback,
			adapter, (uint8_t *)&adapter->macAddressCurrent,
			&adapter->sessionId, type, sub_type);
	if (!HAL_STATUS_SUCCESS(hal_status)) {
		hddLog(LOGE, "sme_OpenSession() failed with status code %d",
				hal_status);
		ret_val = -EAGAIN;
		goto error_sme_open;
	}

	/* Block on a completion variable. Can't wait forever though */
	rc = wait_for_completion_timeout(
			&adapter->session_open_comp_var,
			msecs_to_jiffies(timeout));
	if (!rc) {
		hddLog(LOGE,
			FL("Failed to open session, timeout code: %ld"), rc);
		ret_val = -ETIMEDOUT;
		goto error_sme_open;
	}

	/* Register wireless extensions */
	hal_status = hdd_register_wext(wlan_dev);
	if (eHAL_STATUS_SUCCESS != hal_status) {
		hddLog(LOGE, FL("Wext registration failed with status code %d"),
				hal_status);
		ret_val = -EAGAIN;
		goto error_register_wext;
	}

	status = hdd_init_tx_rx(adapter);
	if (VOS_STATUS_SUCCESS != status) {
		hddLog(LOGE, FL("hdd_init_tx_rx() init failed, status %d"),
				status);
		ret_val = -EAGAIN;
		goto error_init_txrx;
	}

	set_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);

	status = hdd_wmm_adapter_init(adapter);
	if (VOS_STATUS_SUCCESS != status) {
		hddLog(LOGE, FL("hdd_wmm_adapter_init() failed, status %d"),
				status);
		ret_val = -EAGAIN;
		goto error_wmm_init;
	}

	set_bit(WMM_INIT_DONE, &adapter->event_flags);

	ret_val = process_wma_set_command((int)adapter->sessionId,
			(int)WMI_PDEV_PARAM_BURST_ENABLE,
			(int)hdd_ctx->cfg_ini->enableSifsBurst,
			PDEV_CMD);
	if (0 != ret_val) {
		hddLog(LOGE, FL("WMI_PDEV_PARAM_BURST_ENABLE set failed %d"),
				ret_val);
	}

	ndp_ctx->state = NAN_DATA_NDI_CREATING_STATE;
	return ret_val;

error_wmm_init:
	clear_bit(INIT_TX_RX_SUCCESS, &adapter->event_flags);
	hdd_deinit_tx_rx(adapter);

error_init_txrx:
	hdd_UnregisterWext(wlan_dev);

error_register_wext:
	if (test_bit(SME_SESSION_OPENED, &adapter->event_flags)) {
		INIT_COMPLETION(adapter->session_close_comp_var);
		if (eHAL_STATUS_SUCCESS ==
				sme_CloseSession(hdd_ctx->hHal,
					adapter->sessionId,
					hdd_smeCloseSessionCallback, adapter)) {
			rc = wait_for_completion_timeout(
					&adapter->session_close_comp_var,
					msecs_to_jiffies(timeout));
			if (rc <= 0) {
				hddLog(LOGE,
					FL("Session close failed status %ld"),
					rc);
				ret_val = -ETIMEDOUT;
			}
		}
	}

error_sme_open:
	return ret_val;
}
