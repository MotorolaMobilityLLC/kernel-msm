/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

/**
 * wlan_hdd_tsf.c - WLAN Host Device Driver tsf related implementation
 */

#include "wlan_hdd_main.h"
#include "wma_api.h"

/**
 * hdd_capture_tsf() - capture tsf
 *
 * @adapter: pointer to adapter
 * @buf: pointer to uplayer buf
 * @len : the length of buf
 *
 * This function returns tsf value to uplayer.
 *
 * Return: Describe the execute result of this routine
 */
int hdd_capture_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len)
{
	int ret = 0;
	hdd_station_ctx_t *hdd_sta_ctx;

	if (adapter == NULL || buf == NULL) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("invalid pointer"));
		return -EINVAL;
	}
	if (len != 1)
		return -EINVAL;

	/* Reset TSF value for new capture */
	adapter->tsf_high = 0;
	adapter->tsf_low = 0;

	if (adapter->device_mode == WLAN_HDD_INFRA_STATION ||
		adapter->device_mode == WLAN_HDD_P2P_CLIENT) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (hdd_sta_ctx->conn_info.connState !=
			eConnectionState_Associated) {

			hddLog(VOS_TRACE_LEVEL_ERROR,
				FL("failed to cap tsf, not connect with ap"));
			buf[0] = TSF_STA_NOT_CONNECTED_NO_TSF;
			return ret;
		}
	}
	if ((adapter->device_mode == WLAN_HDD_SOFTAP ||
		adapter->device_mode == WLAN_HDD_P2P_GO) &&
		!(test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags))) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("Soft AP / P2p GO not beaconing"));
		buf[0] = TSF_SAP_NOT_STARTED_NO_TSF;
		return ret;
	}
	if (adapter->tsf_state == TSF_CAP_STATE) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("current in capture state, pls reset"));
			buf[0] = TSF_CURRENT_IN_CAP_STATE;
	} else {
		hddLog(VOS_TRACE_LEVEL_INFO, FL("+ioctl issue cap tsf cmd"));
		buf[0] = TSF_RETURN;
		adapter->tsf_state = TSF_CAP_STATE;
		ret = process_wma_set_command((int)adapter->sessionId,
				(int)GEN_PARAM_CAPTURE_TSF,
				adapter->sessionId,
				GEN_CMD);

		if (ret != VOS_STATUS_SUCCESS) {
			hddLog(VOS_TRACE_LEVEL_ERROR, FL("capture fail"));
			buf[0] = TSF_CAPTURE_FAIL;
			adapter->tsf_state = TSF_IDLE;
		}
		hddLog(VOS_TRACE_LEVEL_INFO,
			FL("-ioctl return cap tsf cmd, ret = %d"), ret);
	}
	return ret;
}

/**
 * hdd_indicate_tsf() - return tsf to uplayer
 *
 * @adapter: pointer to adapter
 * @buf: pointer to uplayer buf
 * @len : the length of buf
 *
 * This function returns tsf value to uplayer.
 *
 * Return: Describe the execute result of this routine
 */
int hdd_indicate_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len)
{
	int ret = 0;
	hdd_station_ctx_t *hdd_sta_ctx;

	if (adapter == NULL || buf == NULL) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("invalid pointer"));
		return -EINVAL;
	}

	if (len != 3)
		return -EINVAL;

	buf[1] = 0;
	buf[2] = 0;
	if (adapter->device_mode == WLAN_HDD_INFRA_STATION ||
		adapter->device_mode == WLAN_HDD_P2P_CLIENT) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (hdd_sta_ctx->conn_info.connState !=
			eConnectionState_Associated) {

			hddLog(VOS_TRACE_LEVEL_INFO,
				FL("fail to get tsf, sta in disconnected"));
			buf[0] = TSF_STA_NOT_CONNECTED_NO_TSF;
			return ret;
		}
	}
	if ((adapter->device_mode == WLAN_HDD_SOFTAP ||
		adapter->device_mode == WLAN_HDD_P2P_GO) &&
		!(test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags))) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("Soft AP / P2p GO not beaconing"));
		buf[0] = TSF_SAP_NOT_STARTED_NO_TSF;
		return ret;
	}
	if (adapter->tsf_high == 0 && adapter->tsf_low == 0) {
		hddLog(VOS_TRACE_LEVEL_INFO,
			FL("not getting tsf value"));
		buf[0] = TSF_NOT_RETURNED_BY_FW;
	} else {
		buf[0] = TSF_RETURN;
		buf[1] = adapter->tsf_low;
		buf[2] = adapter->tsf_high;
		adapter->tsf_state = TSF_IDLE;

		ret = process_wma_set_command((int)adapter->sessionId,
				(int)GEN_PARAM_RESET_TSF_GPIO,
				adapter->sessionId,
				GEN_CMD);

		if (0 != ret) {
			hddLog(VOS_TRACE_LEVEL_ERROR, FL("tsf get fail "));
			buf[0] = TSF_RESET_GPIO_FAIL;
		}
		hddLog(VOS_TRACE_LEVEL_INFO,
			FL("get tsf cmd,status=%u, tsf_low=%u, tsf_high=%u"),
			buf[0], buf[1], buf[2]);
	}
	return ret;
}

/**
 * hdd_get_tsf_cb() - handle tsf callback
 *
 * @pcb_cxt: pointer to the hdd_contex
 * @ptsf: pointer to struct stsf
 *
 * This function handle the event that reported by firmware at first.
 * The event contains the vdev_id, current tsf value of this vdev,
 * tsf value is 64bits, discripted in two varaible tsf_low and tsf_high.
 * These two values each is uint32.
 *
 * Return: Describe the execute result of this routine
 */
static int hdd_get_tsf_cb(void *pcb_cxt, struct stsf *ptsf)
{
	hdd_context_t *hddctx;
	hdd_adapter_t *adapter;
	int status;

	if (pcb_cxt == NULL || ptsf == NULL) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("HDD context is not valid"));
			return -EINVAL;
	}

	hddctx = (hdd_context_t *)pcb_cxt;
	status = wlan_hdd_validate_context(hddctx);
	if (0 != status) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("hdd context is not valid"));
		return -EINVAL;
	}

	adapter = hdd_get_adapter_by_vdev(hddctx, ptsf->vdev_id);

	if (NULL == adapter) {
		hddLog(VOS_TRACE_LEVEL_ERROR,
			FL("failed to find adapter"));
		return -EINVAL;
	}

	hddLog(VOS_TRACE_LEVEL_INFO,
		FL("tsf cb handle event, device_mode is %d"),
		adapter->device_mode);

	adapter->tsf_low = ptsf->tsf_low;
	adapter->tsf_high = ptsf->tsf_high;

	hddLog(VOS_TRACE_LEVEL_INFO,
		FL("hdd_get_tsf_cb sta=%u, tsf_low=%u, tsf_high=%u"),
		ptsf->vdev_id, ptsf->tsf_low, ptsf->tsf_high);
	return 0;
}

/**
 * wlan_hdd_tsf_init() - set callback to handle tsf value.
 * @hdd_ctx: pointer to the hdd_context_t
 *
 * This function set the callback to sme module, the callback will be
 * called when a tsf event is reported by firmware
 *
 * Return: nothing
 */
void wlan_hdd_tsf_init(hdd_context_t *hdd_ctx)
{
	sme_set_tsfcb(hdd_ctx->hHal, hdd_get_tsf_cb, hdd_ctx);
}
