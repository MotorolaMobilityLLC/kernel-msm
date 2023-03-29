/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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
 * wlan_hdd_tsf.c - WLAN Host Device Driver tsf related implementation
 */

#include "osif_sync.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_tsf.h"
#include "wma_api.h"
#include "wlan_fwol_ucfg_api.h"
#include <qca_vendor.h>
#include <linux/errqueue.h>
#if defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ) || \
	defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC)
#include <linux/gpio.h>
#endif

#include "ol_txrx_api.h"

#ifdef WLAN_FEATURE_TSF_PLUS
#if !defined(WLAN_FEATURE_TSF_PLUS_NOIRQ) && \
	!defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC) && \
	!defined(WLAN_FEATURE_TSF_TIMER_SYNC)
static int tsf_gpio_irq_num = -1;
#endif
#endif
static struct completion tsf_sync_get_completion_evt;
#define WLAN_TSF_SYNC_GET_TIMEOUT 2000
#define WLAN_HDD_CAPTURE_TSF_REQ_TIMEOUT_MS 500
#define WLAN_HDD_CAPTURE_TSF_INIT_INTERVAL_MS 100
#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
#define WLAN_HDD_SOFTAP_INTERVAL_TIMES 1
#else
#define WLAN_HDD_SOFTAP_INTERVAL_TIMES 100
#endif
#define OUTPUT_HIGH 1
#define OUTPUT_LOW 0

#ifdef WLAN_FEATURE_TSF_PLUS
#if defined(WLAN_FEATURE_TSF_PLUS_NOIRQ) || \
	defined(WLAN_FEATURE_TSF_TIMER_SYNC)
static void hdd_update_timestamp(struct hdd_adapter *adapter);
#else
static void
hdd_update_timestamp(struct hdd_adapter *adapter,
		     uint64_t target_time, uint64_t host_time);
#endif
#endif

/**
 * enum hdd_tsf_op_result - result of tsf operation
 *
 * HDD_TSF_OP_SUCC:  succeed
 * HDD_TSF_OP_FAIL:  fail
 */
enum hdd_tsf_op_result {
	HDD_TSF_OP_SUCC,
	HDD_TSF_OP_FAIL
};

#ifdef WLAN_FEATURE_TSF_PLUS
#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
#define WLAN_HDD_CAPTURE_TSF_RESYNC_INTERVAL 1
#else
#define WLAN_HDD_CAPTURE_TSF_RESYNC_INTERVAL 9
#endif
static inline void hdd_set_th_sync_status(struct hdd_adapter *adapter,
					  bool initialized)
{
	qdf_atomic_set(&adapter->tsf_sync_ready_flag,
		       (initialized ? 1 : 0));
}

static inline bool hdd_get_th_sync_status(struct hdd_adapter *adapter)
{
	return qdf_atomic_read(&adapter->tsf_sync_ready_flag) != 0;
}

#else
static inline bool hdd_get_th_sync_status(struct hdd_adapter *adapter)
{
	return true;
}
#endif

static
enum hdd_tsf_get_state hdd_tsf_check_conn_state(struct hdd_adapter *adapter)
{
	enum hdd_tsf_get_state ret = TSF_RETURN;
	struct hdd_station_ctx *hdd_sta_ctx;

	if (adapter->device_mode == QDF_STA_MODE ||
			adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (hdd_sta_ctx->conn_info.conn_state !=
				eConnectionState_Associated) {
			hdd_err("failed to cap tsf, not connect with ap");
			ret = TSF_STA_NOT_CONNECTED_NO_TSF;
		}
	} else if ((adapter->device_mode == QDF_SAP_MODE ||
				adapter->device_mode == QDF_P2P_GO_MODE) &&
			!(test_bit(SOFTAP_BSS_STARTED,
					&adapter->event_flags))) {
		hdd_err("Soft AP / P2p GO not beaconing");
		ret = TSF_SAP_NOT_STARTED_NO_TSF;
	}
	return ret;
}

static bool hdd_tsf_is_initialized(struct hdd_adapter *adapter)
{
	struct hdd_context *hddctx;

	if (!adapter) {
		hdd_err("invalid adapter");
		return false;
	}

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (!hddctx) {
		hdd_err("invalid hdd context");
		return false;
	}

	if (!qdf_atomic_read(&hddctx->tsf_ready_flag) ||
	    !hdd_get_th_sync_status(adapter)) {
		hdd_err("TSF is not initialized");
		return false;
	}

	return true;
}

#if (defined(WLAN_FEATURE_TSF_PLUS_NOIRQ) && \
	defined(WLAN_FEATURE_TSF_PLUS)) || \
	defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC) || \
	defined(WLAN_FEATURE_TSF_TIMER_SYNC)
/**
 * hdd_tsf_reset_gpio() - Reset TSF GPIO used for host timer sync
 * @adapter: pointer to adapter
 *
 * This function send WMI command to reset GPIO configured in FW after
 * TSF get operation.
 *
 * Return: TSF_RETURN on Success, TSF_RESET_GPIO_FAIL on failure
 */
static int hdd_tsf_reset_gpio(struct hdd_adapter *adapter)
{
	/* No GPIO Host timer sync for integrated WIFI Device */
	return TSF_RETURN;
}

/**
 * hdd_tsf_set_gpio() - Set TSF GPIO used for host timer sync
 * @hdd_ctx: pointer to hdd context
 *
 * This function is a dummy function for adrastea arch
 *
 * Return: QDF_STATUS_SUCCESS on Success
 */

static QDF_STATUS hdd_tsf_set_gpio(struct hdd_context *hdd_ctx)
{
	return QDF_STATUS_SUCCESS;
}
#else
static int hdd_tsf_reset_gpio(struct hdd_adapter *adapter)
{
	int ret;

	ret = wma_cli_set_command((int)adapter->vdev_id,
				  (int)GEN_PARAM_RESET_TSF_GPIO,
				  adapter->vdev_id,
				  GEN_CMD);

	if (ret != 0) {
		hdd_err("tsf reset GPIO fail ");
		ret = TSF_RESET_GPIO_FAIL;
	} else {
		ret = TSF_RETURN;
	}
	return ret;
}

/**
 * hdd_tsf_set_gpio() - Set TSF GPIO used for host timer sync
 * @hdd_ctx: pointer to hdd context
 *
 * This function check GPIO and set GPIO as IRQ to FW side on
 * none Adrastea arch
 *
 * Return: QDF_STATUS_SUCCESS on Success, others on Failure.
 */
static QDF_STATUS hdd_tsf_set_gpio(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint32_t tsf_gpio_pin = TSF_GPIO_PIN_INVALID;

	status = ucfg_fwol_get_tsf_gpio_pin(hdd_ctx->psoc, &tsf_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (tsf_gpio_pin == TSF_GPIO_PIN_INVALID)
		return QDF_STATUS_E_INVAL;

	status = sme_set_tsf_gpio(hdd_ctx->mac_handle,
				  tsf_gpio_pin);

	return status;
}
#endif

#ifdef WLAN_FEATURE_TSF_PLUS
static bool hdd_tsf_is_ptp_enabled(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return !!tsf_ptp_options;
	else
		return false;
}

bool hdd_tsf_is_tx_set(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return tsf_ptp_options & CFG_SET_TSF_PTP_OPT_TX;
	else
		return false;
}

bool hdd_tsf_is_rx_set(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return tsf_ptp_options & CFG_SET_TSF_PTP_OPT_RX;
	else
		return false;
}

bool hdd_tsf_is_raw_set(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return tsf_ptp_options & CFG_SET_TSF_PTP_OPT_RAW;
	else
		return false;
}

bool hdd_tsf_is_dbg_fs_set(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return tsf_ptp_options & CFG_SET_TSF_DBG_FS;
	else
		return false;
}

bool hdd_tsf_is_tsf64_tx_set(struct hdd_context *hdd)
{
	uint32_t tsf_ptp_options;

	if (hdd && QDF_IS_STATUS_SUCCESS(
	    ucfg_fwol_get_tsf_ptp_options(hdd->psoc, &tsf_ptp_options)))
		return tsf_ptp_options & CFG_SET_TSF_PTP_OPT_TSF64_TX;
	else
		return false;
}
#else

static bool hdd_tsf_is_ptp_enabled(struct hdd_context *hdd)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_TSF_PLUS
static inline
uint64_t hdd_get_monotonic_host_time(struct hdd_context *hdd_ctx)
{
	return hdd_tsf_is_raw_set(hdd_ctx) ?
		ktime_get_ns() : ktime_get_real_ns();
}
#endif

#if defined(WLAN_FEATURE_TSF_PLUS) && \
	defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC)
#define MAX_CONTINUOUS_RETRY_CNT 10
static uint32_t
hdd_wlan_retry_tsf_cap(struct hdd_adapter *adapter)
{
	struct hdd_context *hddctx;
	int count = adapter->continuous_cap_retry_count;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (count == MAX_CONTINUOUS_RETRY_CNT) {
		hdd_debug("Max retry countr reached");
		return 0;
	}
	qdf_atomic_set(&hddctx->cap_tsf_flag, 0);
	count++;
	adapter->continuous_cap_retry_count = count;
	return (count * WLAN_HDD_CAPTURE_TSF_REQ_TIMEOUT_MS);
}

static void
hdd_wlan_restart_tsf_cap(struct hdd_adapter *adapter)
{
	struct hdd_context *hddctx;
	int count = adapter->continuous_cap_retry_count;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (count == MAX_CONTINUOUS_RETRY_CNT) {
		hdd_debug("Restart TSF CAP");
		qdf_atomic_set(&hddctx->cap_tsf_flag, 0);
		adapter->continuous_cap_retry_count = 0;
		qdf_mc_timer_start(&adapter->host_target_sync_timer,
				   WLAN_HDD_CAPTURE_TSF_INIT_INTERVAL_MS);
	}
}

static void
hdd_update_host_time(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;
	u64 host_time;
	char *name = NULL;

	hdd_ctx = adapter->hdd_ctx;

	if (!hdd_tsf_is_initialized(adapter)) {
		hdd_err("tsf is not init, exit");
		return;
	}

	host_time = hdd_get_monotonic_host_time(hdd_ctx);
	hdd_update_timestamp(adapter, 0, host_time);
	name = adapter->dev->name;

	hdd_debug("iface: %s - host_time: %llu",
		  (!name ? "none" : name), host_time);
}

static
void hdd_tsf_ext_gpio_sync_work(void *data)
{
	QDF_STATUS status;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	uint32_t tsf_sync_gpio_pin = TSF_GPIO_PIN_INVALID;

	adapter = data;
	hdd_ctx = adapter->hdd_ctx;
	status = ucfg_fwol_get_tsf_sync_host_gpio_pin(hdd_ctx->psoc,
						      &tsf_sync_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("tsf sync gpio host pin error");
		return;
	}
	gpio_set_value(tsf_sync_gpio_pin, OUTPUT_HIGH);
	hdd_update_host_time(adapter);
	usleep_range(50, 100);
	gpio_set_value(tsf_sync_gpio_pin, OUTPUT_LOW);

	status = wma_cli_set_command((int)adapter->vdev_id,
				     (int)GEN_PARAM_CAPTURE_TSF,
				     adapter->vdev_id, GEN_CMD);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("cap tsf fail");
		qdf_mc_timer_stop(&adapter->host_capture_req_timer);
		qdf_mc_timer_destroy(&adapter->host_capture_req_timer);
	}
}

static void
hdd_tsf_gpio_sync_work_init(struct hdd_adapter *adapter)
{
	qdf_create_work(0, &adapter->gpio_tsf_sync_work,
			hdd_tsf_ext_gpio_sync_work, adapter);
}

static void
hdd_tsf_gpio_sync_work_deinit(struct hdd_adapter *adapter)
{
	qdf_destroy_work(0, &adapter->gpio_tsf_sync_work);
}

static void
hdd_tsf_stop_ext_gpio_sync(struct hdd_adapter *adapter)
{
	qdf_cancel_work(&adapter->gpio_tsf_sync_work);
}

static void
hdd_tsf_start_ext_gpio_sync(struct hdd_adapter *adapter)
{
	qdf_sched_work(0, &adapter->gpio_tsf_sync_work);
}

static bool hdd_tsf_cap_sync_send(struct hdd_adapter *adapter)
{
	hdd_tsf_start_ext_gpio_sync(adapter);
	return true;
}
#elif defined(WLAN_FEATURE_TSF_PLUS) && \
	!defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC)
static void
hdd_wlan_restart_tsf_cap(struct hdd_adapter *adapter)
{
}

static void
hdd_tsf_gpio_sync_work_init(struct hdd_adapter *adapter)
{
}

static void
hdd_tsf_gpio_sync_work_deinit(struct hdd_adapter *adapter)
{
}

static void
hdd_tsf_stop_ext_gpio_sync(struct hdd_adapter *adapter)
{
}

static void
hdd_tsf_start_ext_gpio_sync(struct hdd_adapter *adapter)
{
}

static bool
hdd_tsf_cap_sync_send(struct hdd_adapter *adapter)
{
	hdd_tsf_start_ext_gpio_sync(adapter);
	return false;
}

#else
static bool hdd_tsf_cap_sync_send(struct hdd_adapter *adapter)
{
	return false;
}
#endif

static enum hdd_tsf_op_result hdd_capture_tsf_internal(
	struct hdd_adapter *adapter, uint32_t *buf, int len)
{
	int ret;
	struct hdd_context *hddctx;
	qdf_mc_timer_t *cap_timer;

	if (!adapter || !buf) {
		hdd_err("invalid pointer");
		return HDD_TSF_OP_FAIL;
	}

	if (len != 1)
		return HDD_TSF_OP_FAIL;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (!hddctx) {
		hdd_err("invalid hdd context");
		return HDD_TSF_OP_FAIL;
	}

	if (!hdd_tsf_is_initialized(adapter)) {
		buf[0] = TSF_NOT_READY;
		return HDD_TSF_OP_SUCC;
	}

	buf[0] = hdd_tsf_check_conn_state(adapter);
	if (buf[0] != TSF_RETURN)
		return HDD_TSF_OP_SUCC;

	if (qdf_atomic_inc_return(&hddctx->cap_tsf_flag) > 1) {
		hdd_err("current in capture state");
		buf[0] = TSF_CURRENT_IN_CAP_STATE;
		return HDD_TSF_OP_SUCC;
	}

	/* record adapter for cap_tsf_irq_handler  */
	hddctx->cap_tsf_context = adapter;

	hdd_info("+ioctl issue cap tsf cmd");
	cap_timer = &adapter->host_capture_req_timer;
	qdf_mc_timer_init(cap_timer, QDF_TIMER_TYPE_SW,
			  hdd_capture_req_timer_expired_handler,
			  (void *)adapter);
	qdf_mc_timer_start(cap_timer, WLAN_HDD_CAPTURE_TSF_REQ_TIMEOUT_MS);

	/* Reset TSF value for new capture */
	adapter->cur_target_time = 0;

	buf[0] = TSF_RETURN;
	init_completion(&tsf_sync_get_completion_evt);

	if (hdd_tsf_cap_sync_send(adapter))
		return HDD_TSF_OP_SUCC;

	ret = wma_cli_set_command((int)adapter->vdev_id,
				  (int)GEN_PARAM_CAPTURE_TSF,
				  adapter->vdev_id, GEN_CMD);
	if (QDF_STATUS_SUCCESS != ret) {
		hdd_err("cap tsf fail");
		buf[0] = TSF_CAPTURE_FAIL;
		hddctx->cap_tsf_context = NULL;
		qdf_atomic_set(&hddctx->cap_tsf_flag, 0);
		qdf_mc_timer_stop(&adapter->host_capture_req_timer);
		qdf_mc_timer_destroy(&adapter->host_capture_req_timer);

		return HDD_TSF_OP_SUCC;
	}
	hdd_info("-ioctl return cap tsf cmd");
	return HDD_TSF_OP_SUCC;
}

static enum hdd_tsf_op_result hdd_indicate_tsf_internal(
	struct hdd_adapter *adapter, uint32_t *buf, int len)
{
	int ret;
	struct hdd_context *hddctx;

	if (!adapter || !buf) {
		hdd_err("invalid pointer");
		return HDD_TSF_OP_FAIL;
	}

	if (len != 3)
		return HDD_TSF_OP_FAIL;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (!hddctx) {
		hdd_err("invalid hdd context");
		return HDD_TSF_OP_FAIL;
	}

	buf[1] = 0;
	buf[2] = 0;

	if (!hdd_tsf_is_initialized(adapter)) {
		buf[0] = TSF_NOT_READY;
		return HDD_TSF_OP_SUCC;
	}

	buf[0] = hdd_tsf_check_conn_state(adapter);
	if (buf[0] != TSF_RETURN)
		return HDD_TSF_OP_SUCC;

	if (adapter->cur_target_time == 0) {
		hdd_info("TSF value not received");
		buf[0] = TSF_NOT_RETURNED_BY_FW;
		return HDD_TSF_OP_SUCC;
	}

	buf[0] = TSF_RETURN;
	buf[1] = (uint32_t)(adapter->cur_target_time & 0xffffffff);
	buf[2] = (uint32_t)((adapter->cur_target_time >> 32) &
				0xffffffff);

	if (!qdf_atomic_read(&hddctx->cap_tsf_flag)) {
		hdd_info("old: status=%u, tsf_low=%u, tsf_high=%u",
			 buf[0], buf[1], buf[2]);
		return HDD_TSF_OP_SUCC;
	}

	ret = hdd_tsf_reset_gpio(adapter);
	if (0 != ret) {
		hdd_err("reset tsf gpio fail");
		buf[0] = TSF_RESET_GPIO_FAIL;
		return HDD_TSF_OP_SUCC;
	}
	hddctx->cap_tsf_context = NULL;
	qdf_atomic_set(&hddctx->cap_tsf_flag, 0);
	hdd_info("get tsf cmd,status=%u, tsf_low=%u, tsf_high=%u",
		 buf[0], buf[1], buf[2]);

	return HDD_TSF_OP_SUCC;
}

#ifdef WLAN_FEATURE_TSF_PLUS
/* unit for target time: us;  host time: ns */
#define HOST_TO_TARGET_TIME_RATIO NSEC_PER_USEC
#define MAX_ALLOWED_DEVIATION_NS (100 * NSEC_PER_USEC)
#define MAX_CONTINUOUS_ERROR_CNT 3

/* to distinguish 32-bit overflow case, this inverval should:
 * equal or less than (1/2 * OVERFLOW_INDICATOR32 us)
 */
#if defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ) || \
	defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC)
#define WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC 2
#else
#define WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC 4
#endif
#define OVERFLOW_INDICATOR32 (((int64_t)0x1) << 32)
#define CAP_TSF_TIMER_FIX_SEC 1

/**
 * TS_STATUS - timestamp status
 *
 * HDD_TS_STATUS_WAITING:  one of the stamp-pair
 *    is not updated
 * HDD_TS_STATUS_READY:  valid tstamp-pair
 * HDD_TS_STATUS_INVALID: invalid tstamp-pair
 */
enum hdd_ts_status {
	HDD_TS_STATUS_WAITING,
	HDD_TS_STATUS_READY,
	HDD_TS_STATUS_INVALID
};

static
enum hdd_tsf_op_result __hdd_start_tsf_sync(struct hdd_adapter *adapter)
{
	QDF_STATUS ret;

	if (!hdd_get_th_sync_status(adapter)) {
		hdd_err("Host Target sync has not initialized");
		return HDD_TSF_OP_FAIL;
	}

	hdd_tsf_gpio_sync_work_init(adapter);
	ret = qdf_mc_timer_start(&adapter->host_target_sync_timer,
				 WLAN_HDD_CAPTURE_TSF_INIT_INTERVAL_MS);
	if (ret != QDF_STATUS_SUCCESS && ret != QDF_STATUS_E_ALREADY) {
		hdd_err("Failed to start timer, ret: %d", ret);
		return HDD_TSF_OP_FAIL;
	}
	return HDD_TSF_OP_SUCC;
}

static
enum hdd_tsf_op_result __hdd_stop_tsf_sync(struct hdd_adapter *adapter)
{
	QDF_STATUS ret;

	if (!hdd_get_th_sync_status(adapter)) {
		hdd_err("Host Target sync has not initialized");
		return HDD_TSF_OP_SUCC;
	}

	ret = qdf_mc_timer_stop(&adapter->host_target_sync_timer);
	if (ret != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to stop timer, ret: %d", ret);
		return HDD_TSF_OP_FAIL;
	}
	hdd_tsf_stop_ext_gpio_sync(adapter);
	hdd_tsf_gpio_sync_work_deinit(adapter);
	return HDD_TSF_OP_SUCC;
}

static inline void hdd_reset_timestamps(struct hdd_adapter *adapter)
{
	qdf_spin_lock_bh(&adapter->host_target_sync_lock);
	adapter->cur_host_time = 0;
	adapter->cur_target_time = 0;
	adapter->last_host_time = 0;
	adapter->last_target_time = 0;
	qdf_spin_unlock_bh(&adapter->host_target_sync_lock);
}

/**
 * hdd_check_timestamp_status() - return the tstamp status
 *
 * @last_target_time: the last saved target time
 * @last_sync_time: the last saved sync time
 * @cur_target_time : new target time
 * @cur_sync_time : new sync time
 *
 * This function check the new timstamp-pair(cur_host_time/cur_target_time)or
 * (cur_qtime_time/cur_target_time)
 * Return:
 * HDD_TS_STATUS_WAITING: cur_sync_time or cur_sync_time is 0
 * HDD_TS_STATUS_READY: cur_target_time/cur_host_time is a valid pair,
 *    and can be saved
 * HDD_TS_STATUS_INVALID: cur_target_time/cur_sync_time is a invalid pair,
 *    should be discard
 */
static
enum hdd_ts_status hdd_check_timestamp_status(
		uint64_t last_target_time,
		uint64_t last_sync_time,
		uint64_t cur_target_time,
		uint64_t cur_sync_time)
{
	uint64_t delta_ns, delta_target_time, delta_sync_time;

	/* one or more are not updated, need to wait */
	if (cur_target_time == 0 || cur_sync_time == 0)
		return HDD_TS_STATUS_WAITING;

	/* init value, it's the first time to update the pair */
	if (last_target_time == 0 && last_sync_time == 0)
		return HDD_TS_STATUS_READY;

	/* the new values should be greater than the saved values */
	if ((cur_target_time <= last_target_time) ||
	    (cur_sync_time <= last_sync_time)) {
		hdd_err("Invalid timestamps!last_target_time: %llu;"
			"last_sync_time: %llu; cur_target_time: %llu;"
			"cur_sync_time: %llu",
			last_target_time, last_sync_time,
			cur_target_time, cur_sync_time);
		return HDD_TS_STATUS_INVALID;
	}

	delta_target_time = (cur_target_time - last_target_time) *
						NSEC_PER_USEC;
	delta_sync_time = cur_sync_time - last_sync_time;

	/*
	 * DO NOT use abs64() , a big uint64 value might be turned to
	 * a small int64 value
	 */
	delta_ns = ((delta_target_time > delta_sync_time) ?
			(delta_target_time - delta_sync_time) :
			(delta_sync_time - delta_target_time));
	hdd_warn("timestamps deviation - delta: %llu ns", delta_ns);
	/* the deviation should be smaller than a threshold */
	if (delta_ns > MAX_ALLOWED_DEVIATION_NS) {
		hdd_warn("Invalid timestamps - delta: %llu ns", delta_ns);
		return HDD_TS_STATUS_INVALID;
	}
	return HDD_TS_STATUS_READY;
}

static inline bool hdd_tsf_is_in_cap(struct hdd_adapter *adapter)
{
	struct hdd_context *hddctx;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (!hddctx)
		return false;

	return qdf_atomic_read(&hddctx->cap_tsf_flag) > 0;
}

/* define 64bit plus/minus to deal with overflow */
static inline int hdd_64bit_plus(uint64_t x, int64_t y, uint64_t *ret)
{
	if ((y < 0 && (-y) > x) ||
	    (y > 0 && (y > U64_MAX - x))) {
		*ret = 0;
		return -EINVAL;
	}

	*ret = x + y;
	return 0;
}

static inline int hdd_uint64_plus(uint64_t x, uint64_t y, uint64_t *ret)
{
	if (!ret)
		return -EINVAL;

	if (x > (U64_MAX - y)) {
		*ret = 0;
		return -EINVAL;
	}

	*ret = x + y;
	return 0;
}

static inline int hdd_uint64_minus(uint64_t x, uint64_t y, uint64_t *ret)
{
	if (!ret)
		return -EINVAL;

	if (x < y) {
		*ret = 0;
		return -EINVAL;
	}

	*ret = x - y;
	return 0;
}

static inline int32_t hdd_get_hosttime_from_targettime(
	struct hdd_adapter *adapter, uint64_t target_time,
	uint64_t *host_time)
{
	int32_t ret = -EINVAL;
	int64_t delta32_target;
	bool in_cap_state;
	int64_t normal_interval_target;

	in_cap_state = hdd_tsf_is_in_cap(adapter);

	/*
	 * To avoid check the lock when it's not capturing tsf
	 * (the tstamp-pair won't be changed)
	 */
	if (in_cap_state)
		qdf_spin_lock_bh(&adapter->host_target_sync_lock);

	hdd_wlan_restart_tsf_cap(adapter);
	/* at present, target_time is only 32bit in fact */
	delta32_target = (int64_t)((target_time & U32_MAX) -
			(adapter->last_target_time & U32_MAX));

	normal_interval_target = WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC *
		qdf_do_div(NSEC_PER_SEC, HOST_TO_TARGET_TIME_RATIO);

	if (delta32_target <
			(normal_interval_target - OVERFLOW_INDICATOR32))
		delta32_target += OVERFLOW_INDICATOR32;
	else if (delta32_target >
			(OVERFLOW_INDICATOR32 - normal_interval_target))
		delta32_target -= OVERFLOW_INDICATOR32;

	ret = hdd_64bit_plus(adapter->last_host_time,
			     HOST_TO_TARGET_TIME_RATIO * delta32_target,
			     host_time);

	if (in_cap_state)
		qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	return ret;
}

static inline int32_t hdd_get_targettime_from_hosttime(
	struct hdd_adapter *adapter, uint64_t host_time,
	uint64_t *target_time)
{
	int32_t ret = -EINVAL;
	bool in_cap_state;

	if (!adapter || host_time == 0)
		return ret;

	in_cap_state = hdd_tsf_is_in_cap(adapter);
	if (in_cap_state)
		qdf_spin_lock_bh(&adapter->host_target_sync_lock);

	if (host_time < adapter->last_host_time)
		ret = hdd_uint64_minus(adapter->last_target_time,
				       qdf_do_div(adapter->last_host_time -
						  host_time,
						  HOST_TO_TARGET_TIME_RATIO),
				       target_time);
	else
		ret = hdd_uint64_plus(adapter->last_target_time,
				      qdf_do_div(host_time -
						 adapter->last_host_time,
						 HOST_TO_TARGET_TIME_RATIO),
				      target_time);

	if (in_cap_state)
		qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	return ret;
}

/**
 * hdd_get_soctime_from_tsf64time() - return get status
 *
 * @adapter: Adapter pointer
 * @tsf64_time: current tsf64time, us
 * @soc_time: current soc time(qtime), ns
 *
 * This function get current soc time from current tsf64 time
 * Returun int32_t value to tell get success or fail.
 *
 * Return:
 * 0:        success
 * other: fail
 *
 */
static inline int32_t hdd_get_soctime_from_tsf64time(
	struct hdd_adapter *adapter, uint64_t tsf64_time,
	uint64_t *soc_time)
{
	int32_t ret = -EINVAL;
	uint64_t delta64_tsf64time;
	uint64_t delta64_soctime;
	bool in_cap_state;

	in_cap_state = hdd_tsf_is_in_cap(adapter);

	/*
	 * To avoid check the lock when it's not capturing tsf
	 * (the tstamp-pair won't be changed)
	 */
	if (in_cap_state)
		qdf_spin_lock_bh(&adapter->host_target_sync_lock);

	/* at present, target_time is 64bit (g_tsf64), us*/
	if (tsf64_time > adapter->last_target_global_tsf_time) {
		delta64_tsf64time = tsf64_time -
				    adapter->last_target_global_tsf_time;
		delta64_soctime = delta64_tsf64time * NSEC_PER_USEC;

		/* soc_time (ns)*/
		ret = hdd_uint64_plus(adapter->last_tsf_sync_soc_time,
				      delta64_soctime, soc_time);
	} else {
		delta64_tsf64time = adapter->last_target_global_tsf_time -
				    tsf64_time;
		delta64_soctime = delta64_tsf64time * NSEC_PER_USEC;

		/* soc_time (ns)*/
		ret = hdd_uint64_minus(adapter->last_tsf_sync_soc_time,
				       delta64_soctime, soc_time);
	}

	if (in_cap_state)
		qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	return ret;
}

/**
 * hdd_get_tsftime_from_qtime()
 *
 * @adapter: Adapter pointer
 * @qtime: current qtime, us
 * @tsf_sync_qtime: qtime of the tsf, us
 * @tsf_time: current tsf time(qtime), us
 *
 * This function determines current tsf time
 * using current qtime
 *
 * Return: 0 for success or non-zero negative failure code
 */
static inline int32_t
hdd_get_tsftime_from_qtime(struct hdd_adapter *adapter, uint64_t qtime,
			   uint64_t tsf_sync_qtime, uint64_t *tsf_time)
{
	int32_t ret = -EINVAL;
	uint64_t delta64_tsf64time;
	bool in_cap_state;

	in_cap_state = hdd_tsf_is_in_cap(adapter);

	/*
	 * To avoid check the lock when it's not capturing tsf
	 * (the tstamp-pair won't be changed)
	 */
	if (in_cap_state)
		qdf_spin_lock_bh(&adapter->host_target_sync_lock);

	if (qtime > tsf_sync_qtime) {
		delta64_tsf64time = qtime - tsf_sync_qtime;
		ret = hdd_uint64_plus(adapter->last_target_time,
				      delta64_tsf64time, tsf_time);
	} else {
		delta64_tsf64time = tsf_sync_qtime - qtime;
		ret = hdd_uint64_minus(adapter->last_target_time,
				       delta64_tsf64time, tsf_time);
	}

	if (in_cap_state)
		qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	return ret;
}

static void hdd_capture_tsf_timer_expired_handler(void *arg)
{
	uint32_t tsf_op_resp;
	struct hdd_adapter *adapter;

	if (!arg)
		return;

	adapter = (struct hdd_adapter *)arg;
	hdd_capture_tsf_internal(adapter, &tsf_op_resp, 1);
}

#ifndef WLAN_FEATURE_TSF_PLUS_NOIRQ
#if !defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC) && \
	!defined(WLAN_FEATURE_TSF_TIMER_SYNC)

static irqreturn_t hdd_tsf_captured_irq_handler(int irq, void *arg)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	uint64_t host_time;
	char *name = NULL;

	if (!arg)
		return IRQ_NONE;

	if (irq != tsf_gpio_irq_num)
		return IRQ_NONE;

	hdd_ctx = (struct hdd_context *)arg;
	host_time = hdd_get_monotonic_host_time(hdd_ctx);

	adapter = hdd_ctx->cap_tsf_context;
	if (!adapter)
		return IRQ_HANDLED;

	if (!hdd_tsf_is_initialized(adapter)) {
		hdd_err("tsf is not init, ignore irq");
		return IRQ_HANDLED;
	}

	hdd_update_timestamp(adapter, 0, host_time);
	if (adapter->dev)
		name = adapter->dev->name;

	hdd_info("irq: %d - iface: %s - host_time: %llu",
		 irq, (!name ? "none" : name), host_time);

	return IRQ_HANDLED;
}
#endif
#endif

void hdd_capture_req_timer_expired_handler(void *arg)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	QDF_TIMER_STATE capture_req_timer_status;
	qdf_mc_timer_t *sync_timer;
	int interval;
	int ret;

	if (!arg)
		return;
	adapter = (struct hdd_adapter *)arg;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_warn("invalid hdd context");
		return;
	}

	if (!hdd_tsf_is_initialized(adapter)) {
		qdf_mc_timer_destroy(&adapter->host_capture_req_timer);
		hdd_warn("tsf not init");
		return;
	}

	qdf_spin_lock_bh(&adapter->host_target_sync_lock);
	adapter->cur_host_time = 0;
	adapter->cur_target_time = 0;
	qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	ret = hdd_tsf_reset_gpio(adapter);
	if (0 != ret)
		hdd_info("reset tsf gpio fail");

	hdd_ctx->cap_tsf_context = NULL;
	qdf_atomic_set(&hdd_ctx->cap_tsf_flag, 0);
	qdf_mc_timer_destroy(&adapter->host_capture_req_timer);

	sync_timer = &adapter->host_target_sync_timer;
	capture_req_timer_status =
		qdf_mc_timer_get_current_state(sync_timer);

	if (capture_req_timer_status == QDF_TIMER_STATE_UNUSED) {
		hdd_warn("invalid timer status");
		return;
	}

	interval = WLAN_HDD_CAPTURE_TSF_RESYNC_INTERVAL * MSEC_PER_SEC;
	qdf_mc_timer_start(sync_timer, interval);
}

#if defined(WLAN_FEATURE_TSF_PLUS_NOIRQ) || \
	defined(WLAN_FEATURE_TSF_TIMER_SYNC)
static void hdd_update_timestamp(struct hdd_adapter *adapter)
{
	int interval = 0;
	enum hdd_ts_status sync_status;

	if (!adapter)
		return;

	/* on ADREASTEA ach, Qtime is used to sync host and tsf time as a
	 * intermedia there is no IRQ to sync up TSF-HOST, so host time in ns
	 * and target in us will be updated at the same time in WMI command
	 * callback
	 */

	qdf_spin_lock_bh(&adapter->host_target_sync_lock);
	sync_status =
		  hdd_check_timestamp_status(adapter->last_target_time,
					     adapter->last_tsf_sync_soc_time,
					     adapter->cur_target_time,
					     adapter->cur_tsf_sync_soc_time);
	hdd_info("sync_status %d", sync_status);
	switch (sync_status) {
	case HDD_TS_STATUS_INVALID:
		if (++adapter->continuous_error_count <
		    MAX_CONTINUOUS_ERROR_CNT) {
			interval =
				WLAN_HDD_CAPTURE_TSF_INIT_INTERVAL_MS;
			adapter->cur_target_time = 0;
			adapter->cur_tsf_sync_soc_time = 0;
			break;
		}
		hdd_warn("Reach the max continuous error count");
		/*
		 * fall through:
		 * If reach MAX_CONTINUOUS_ERROR_CNT, treat it as a
		 * valid pair
		 */
	case HDD_TS_STATUS_READY:
		adapter->last_target_time = adapter->cur_target_time;
		adapter->last_target_global_tsf_time =
			adapter->cur_target_global_tsf_time;
		adapter->last_tsf_sync_soc_time =
				adapter->cur_tsf_sync_soc_time;
		adapter->cur_target_time = 0;
		adapter->cur_target_global_tsf_time = 0;
		adapter->cur_tsf_sync_soc_time = 0;
		hdd_info("ts-pair updated: target: %llu; g_target:%llu, Qtime: %llu",
			 adapter->last_target_time,
			 adapter->last_target_global_tsf_time,
			 adapter->last_tsf_sync_soc_time);

		/*
		 * TSF-HOST need to be updated in at most
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC, it couldn't be achieved
		 * if the timer interval is also
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC, due to processing or
		 * schedule delay. So deduct several seconds from
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC.
		 * Without this change, hdd_get_hosttime_from_targettime() will
		 * get wrong host time when it's longer than
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC from last
		 * TSF-HOST update.
		 */
		interval = (WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC -
			    CAP_TSF_TIMER_FIX_SEC) * MSEC_PER_SEC;

		adapter->continuous_error_count = 0;
		adapter->continuous_cap_retry_count = 0;
		hdd_debug("ts-pair updated: interval: %d",
			  interval);
		break;
	case HDD_TS_STATUS_WAITING:
		interval = 0;
		hdd_warn("TS status is waiting due to one or more pair not updated");
		break;
	}
	qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	if (interval > 0)
		qdf_mc_timer_start(&adapter->host_target_sync_timer, interval);
}

static ssize_t __hdd_wlan_tsf_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct hdd_station_ctx *hdd_sta_ctx;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	uint64_t tsf_sync_qtime, host_time, reg_qtime, qtime, target_time;
	ssize_t size;

	struct net_device *net_dev = container_of(dev, struct net_device, dev);

	adapter = (struct hdd_adapter *)(netdev_priv(net_dev));
	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC)
		return scnprintf(buf, PAGE_SIZE, "Invalid device\n");

	if (!hdd_get_th_sync_status(adapter))
		return scnprintf(buf, PAGE_SIZE,
				 "TSF sync is not initialized\n");

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (eConnectionState_Associated != hdd_sta_ctx->conn_info.conn_state &&
	    (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE))
		return scnprintf(buf, PAGE_SIZE, "NOT connected\n");

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx)
		return scnprintf(buf, PAGE_SIZE, "Invalid HDD context\n");

	tsf_sync_qtime = adapter->last_tsf_sync_soc_time;
	do_div(tsf_sync_qtime, NSEC_PER_USEC);

	reg_qtime = qdf_get_log_timestamp();
	host_time = hdd_get_monotonic_host_time(hdd_ctx);

	qtime = qdf_log_timestamp_to_usecs(reg_qtime);
	do_div(host_time, NSEC_PER_USEC);
	hdd_get_tsftime_from_qtime(adapter, qtime, tsf_sync_qtime,
				   &target_time);

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		size = scnprintf(buf, PAGE_SIZE,
				 "%s%llu %llu %pM %llu %llu %llu\n",
				 buf, adapter->last_target_time,
				 tsf_sync_qtime,
				 hdd_sta_ctx->conn_info.bssid.bytes,
				 qtime, host_time, target_time);
	} else {
		size = scnprintf(buf, PAGE_SIZE,
				 "%s%llu %llu %pM %llu %llu %llu\n",
				 buf, adapter->last_target_time,
				 tsf_sync_qtime,
				 adapter->mac_addr.bytes,
				 qtime, host_time, target_time);
	}

	return size;
}

static inline void hdd_update_tsf(struct hdd_adapter *adapter, uint64_t tsf)
{
	uint32_t tsf_op_resp[3];
	struct hdd_context *hddctx;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	hdd_indicate_tsf_internal(adapter, tsf_op_resp, 3);
	hdd_update_timestamp(adapter);
}
#else
static void hdd_update_timestamp(struct hdd_adapter *adapter,
				 uint64_t target_time, uint64_t host_time)
{
	int interval = 0;
	enum hdd_ts_status sync_status;

	if (!adapter)
		return;

	/* host time is updated in IRQ context, it's always before target time,
	 * and so no need to try update last_host_time at present;
	 * since the interval of capturing TSF
	 * (WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC) is long enough, host and target
	 * time are updated in pairs, and one by one, we can return here to
	 * avoid requiring spin lock, and to speed up the IRQ processing.
	 */
	if (host_time > 0)
		adapter->cur_host_time = host_time;

	qdf_spin_lock_bh(&adapter->host_target_sync_lock);
	if (target_time > 0)
		adapter->cur_target_time = target_time;

	sync_status = hdd_check_timestamp_status(adapter->last_target_time,
						 adapter->last_host_time,
						 adapter->cur_target_time,
						 adapter->cur_host_time);
	hdd_info("sync_status %d", sync_status);
	switch (sync_status) {
	case HDD_TS_STATUS_INVALID:
		if (++adapter->continuous_error_count <
		    MAX_CONTINUOUS_ERROR_CNT) {
			interval =
				WLAN_HDD_CAPTURE_TSF_INIT_INTERVAL_MS;
			adapter->cur_target_time = 0;
			adapter->cur_host_time = 0;
			break;
		}
		hdd_warn("Reach the max continuous error count");
		/*
		 * fall through:
		 * If reach MAX_CONTINUOUS_ERROR_CNT, treat it as a
		 * valid pair
		 */
	case HDD_TS_STATUS_READY:
		adapter->last_target_time = adapter->cur_target_time;
		adapter->last_host_time = adapter->cur_host_time;
		adapter->cur_target_time = 0;
		adapter->cur_host_time = 0;
		hdd_info("ts-pair updated: target: %llu; host: %llu",
			 adapter->last_target_time,
			 adapter->last_host_time);

		/*
		 * TSF-HOST need to be updated in at most
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC, it couldn't be achieved
		 * if the timer interval is also
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC, due to processing or
		 * schedule delay. So deduct several seconds from
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC.
		 * Without this change, hdd_get_hosttime_from_targettime() will
		 * get wrong host time when it's longer than
		 * WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC from last
		 * TSF-HOST update.
		 */
		interval = (WLAN_HDD_CAPTURE_TSF_INTERVAL_SEC -
			    CAP_TSF_TIMER_FIX_SEC) * MSEC_PER_SEC;
		if (adapter->device_mode == QDF_SAP_MODE ||
		    adapter->device_mode == QDF_P2P_GO_MODE) {
			interval *= WLAN_HDD_SOFTAP_INTERVAL_TIMES;
		}

		adapter->continuous_error_count = 0;
		adapter->continuous_cap_retry_count = 0;
		hdd_debug("ts-pair updated: interval: %d",
			  interval);
		break;
	case HDD_TS_STATUS_WAITING:
		interval = 0;
		hdd_warn("TS status is waiting due to one or more pair not updated");

		if (!target_time && !host_time)
			interval = hdd_wlan_retry_tsf_cap(adapter);
		break;
	}
	qdf_spin_unlock_bh(&adapter->host_target_sync_lock);

	if (interval > 0)
		qdf_mc_timer_start(&adapter->host_target_sync_timer, interval);
}

static ssize_t __hdd_wlan_tsf_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct hdd_station_ctx *hdd_sta_ctx;
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	ssize_t size;
	uint64_t host_time, target_time;

	struct net_device *net_dev = container_of(dev, struct net_device, dev);

	adapter = (struct hdd_adapter *)(netdev_priv(net_dev));
	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC)
		return scnprintf(buf, PAGE_SIZE, "Invalid device\n");

	if (!hdd_get_th_sync_status(adapter))
		return scnprintf(buf, PAGE_SIZE,
				 "TSF sync is not initialized\n");

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (eConnectionState_Associated != hdd_sta_ctx->conn_info.conn_state &&
	    (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE))
		return scnprintf(buf, PAGE_SIZE, "NOT connected\n");

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx)
		return scnprintf(buf, PAGE_SIZE, "Invalid HDD context\n");

	host_time = hdd_get_monotonic_host_time(hdd_ctx);

	if (hdd_get_targettime_from_hosttime(adapter, host_time,
					     &target_time)) {
		size = scnprintf(buf, PAGE_SIZE, "Invalid timestamp\n");
	} else {
		if (adapter->device_mode == QDF_STA_MODE ||
		    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
			size = scnprintf(buf, PAGE_SIZE, "%s%llu %llu %pM\n",
					 buf, target_time, host_time,
					 hdd_sta_ctx->conn_info.bssid.bytes);
		} else {
			size = scnprintf(buf, PAGE_SIZE, "%s%llu %llu %pM\n",
					 buf, target_time, host_time,
					 adapter->mac_addr.bytes);
		}
	}

	return size;
}

static inline void hdd_update_tsf(struct hdd_adapter *adapter, uint64_t tsf)
{
	uint32_t tsf_op_resp[3];

	hdd_indicate_tsf_internal(adapter, tsf_op_resp, 3);
	hdd_update_timestamp(adapter, tsf, 0);
}
#endif

static ssize_t hdd_wlan_tsf_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_wlan_tsf_show(dev, attr, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(tsf, 0400, hdd_wlan_tsf_show, NULL);

static enum hdd_tsf_op_result hdd_tsf_sync_init(struct hdd_adapter *adapter)
{
	QDF_STATUS ret;
	struct hdd_context *hddctx;
	struct net_device *net_dev;

	if (!adapter)
		return HDD_TSF_OP_FAIL;

	hddctx = WLAN_HDD_GET_CTX(adapter);
	if (!hddctx) {
		hdd_err("invalid hdd context");
		return HDD_TSF_OP_FAIL;
	}

	if (!qdf_atomic_read(&hddctx->tsf_ready_flag)) {
		hdd_err("TSF feature has NOT been initialized");
		return HDD_TSF_OP_FAIL;
	}

	if (hdd_get_th_sync_status(adapter)) {
		hdd_err("Host Target sync has been initialized!!");
		return HDD_TSF_OP_SUCC;
	}

	qdf_spinlock_create(&adapter->host_target_sync_lock);

	hdd_reset_timestamps(adapter);

	ret = qdf_mc_timer_init(&adapter->host_target_sync_timer,
				QDF_TIMER_TYPE_SW,
				hdd_capture_tsf_timer_expired_handler,
				(void *)adapter);
	if (ret != QDF_STATUS_SUCCESS) {
		hdd_err("Failed to init timer, ret: %d", ret);
		goto fail;
	}

	net_dev = adapter->dev;
	if (net_dev && hdd_tsf_is_dbg_fs_set(hddctx))
		device_create_file(&net_dev->dev, &dev_attr_tsf);
	hdd_set_th_sync_status(adapter, true);

	return HDD_TSF_OP_SUCC;
fail:
	hdd_set_th_sync_status(adapter, false);
	return HDD_TSF_OP_FAIL;
}

static enum hdd_tsf_op_result hdd_tsf_sync_deinit(struct hdd_adapter *adapter)
{
	QDF_STATUS ret;
	struct hdd_context *hddctx;
	struct net_device *net_dev;

	if (!adapter)
		return HDD_TSF_OP_FAIL;

	if (!hdd_get_th_sync_status(adapter)) {
		hdd_err("Host Target sync has not been initialized!!");
		return HDD_TSF_OP_SUCC;
	}

	hdd_set_th_sync_status(adapter, false);
	ret = qdf_mc_timer_destroy(&adapter->host_target_sync_timer);
	if (ret != QDF_STATUS_SUCCESS)
		hdd_err("Failed to destroy timer, ret: %d", ret);

	hddctx = WLAN_HDD_GET_CTX(adapter);

	/* reset the cap_tsf flag and gpio if needed */
	if (hddctx && qdf_atomic_read(&hddctx->cap_tsf_flag) &&
	    hddctx->cap_tsf_context == adapter) {
		int reset_ret = hdd_tsf_reset_gpio(adapter);

		if (reset_ret)
			hdd_err("Failed to reset tsf gpio, ret:%d",
				reset_ret);
		hddctx->cap_tsf_context = NULL;
		qdf_atomic_set(&hddctx->cap_tsf_flag, 0);
	}

	hdd_reset_timestamps(adapter);

	net_dev = adapter->dev;
	if (net_dev && hdd_tsf_is_dbg_fs_set(hddctx)) {
		struct device *dev = &net_dev->dev;

		device_remove_file(dev, &dev_attr_tsf);
	}
	return HDD_TSF_OP_SUCC;
}

#ifdef CONFIG_HL_SUPPORT
static inline
enum hdd_tsf_op_result hdd_netbuf_timestamp(qdf_nbuf_t netbuf,
					    uint64_t target_time)
{
	struct hdd_adapter *adapter;
	struct net_device *net_dev = netbuf->dev;

	if (!net_dev)
		return HDD_TSF_OP_FAIL;

	adapter = (struct hdd_adapter *)(netdev_priv(net_dev));
	if (adapter && adapter->magic == WLAN_HDD_ADAPTER_MAGIC &&
	    hdd_get_th_sync_status(adapter)) {
		uint64_t host_time;
		int32_t ret = hdd_get_hosttime_from_targettime(adapter,
				target_time, &host_time);
		if (!ret) {
			netbuf->tstamp = ns_to_ktime(host_time);
			return HDD_TSF_OP_SUCC;
		}
	}

	return HDD_TSF_OP_FAIL;
}

#else
static inline
enum hdd_tsf_op_result hdd_netbuf_timestamp(qdf_nbuf_t netbuf,
					    uint64_t target_time)
{
	struct hdd_adapter *adapter;
	struct net_device *net_dev = netbuf->dev;
	struct skb_shared_hwtstamps hwtstamps;

	if (!net_dev)
		return HDD_TSF_OP_FAIL;

	adapter = (struct hdd_adapter *)(netdev_priv(net_dev));
	if (adapter && adapter->magic == WLAN_HDD_ADAPTER_MAGIC &&
	    hdd_get_th_sync_status(adapter)) {
		uint64_t tsf64_time = target_time;
		uint64_t soc_time = 0;/*ns*/
		int32_t ret = hdd_get_soctime_from_tsf64time(adapter,
				tsf64_time, &soc_time);
		if (!ret) {
			hwtstamps.hwtstamp = soc_time;
			*skb_hwtstamps(netbuf) = hwtstamps;
			netbuf->tstamp = ktime_set(0, 0);
			return HDD_TSF_OP_SUCC;
		}
	}

	return HDD_TSF_OP_FAIL;
}
#endif

int hdd_start_tsf_sync(struct hdd_adapter *adapter)
{
	enum hdd_tsf_op_result ret;

	if (!adapter)
		return -EINVAL;

	ret = hdd_tsf_sync_init(adapter);
	if (ret != HDD_TSF_OP_SUCC) {
		hdd_err("Failed to init tsf sync, ret: %d", ret);
		return -EINVAL;
	}

	return (__hdd_start_tsf_sync(adapter) ==
		HDD_TSF_OP_SUCC) ? 0 : -EINVAL;
}

int hdd_stop_tsf_sync(struct hdd_adapter *adapter)
{
	enum hdd_tsf_op_result ret;

	if (!adapter)
		return -EINVAL;

	ret = __hdd_stop_tsf_sync(adapter);
	if (ret != HDD_TSF_OP_SUCC)
		return -EINVAL;

	ret = hdd_tsf_sync_deinit(adapter);
	if (ret != HDD_TSF_OP_SUCC) {
		hdd_err("Failed to deinit tsf sync, ret: %d", ret);
		return -EINVAL;
	}
	return 0;
}

int hdd_tx_timestamp(qdf_nbuf_t netbuf, uint64_t target_time)
{
	struct sock *sk = netbuf->sk;

	if (!sk)
		return -EINVAL;

	if ((skb_shinfo(netbuf)->tx_flags & SKBTX_HW_TSTAMP) &&
	    !(skb_shinfo(netbuf)->tx_flags & SKBTX_IN_PROGRESS)) {
		struct sock_exterr_skb *serr;
		qdf_nbuf_t new_netbuf;
		int err;

		if (hdd_netbuf_timestamp(netbuf, target_time) !=
		    HDD_TSF_OP_SUCC)
			return -EINVAL;

		new_netbuf = qdf_nbuf_clone(netbuf);
		if (!new_netbuf)
			return -ENOMEM;

		serr = SKB_EXT_ERR(new_netbuf);
		memset(serr, 0, sizeof(*serr));
		serr->ee.ee_errno = ENOMSG;
		serr->ee.ee_origin = SO_EE_ORIGIN_TIMESTAMPING;

		err = sock_queue_err_skb(sk, new_netbuf);
		if (err) {
			qdf_nbuf_free(new_netbuf);
			return err;
		}

		return 0;
	}
	return -EINVAL;
}

int hdd_rx_timestamp(qdf_nbuf_t netbuf, uint64_t target_time)
{
	if (hdd_netbuf_timestamp(netbuf, target_time) ==
		HDD_TSF_OP_SUCC)
		return 0;

	/* reset tstamp when failed */
	netbuf->tstamp = ktime_set(0, 0);
	return -EINVAL;
}

static inline int __hdd_capture_tsf(struct hdd_adapter *adapter,
				    uint32_t *buf, int len)
{
	if (!adapter || !buf) {
		hdd_err("invalid pointer");
		return -EINVAL;
	}

	if (len != 1)
		return -EINVAL;

	buf[0] = TSF_DISABLED_BY_TSFPLUS;

	return 0;
}

static inline int __hdd_indicate_tsf(struct hdd_adapter *adapter,
				     uint32_t *buf, int len)
{
	if (!adapter || !buf) {
		hdd_err("invalid pointer");
		return -EINVAL;
	}

	if (len != 3)
		return -EINVAL;

	buf[0] = TSF_DISABLED_BY_TSFPLUS;
	buf[1] = 0;
	buf[2] = 0;

	return 0;
}

#if defined(WLAN_FEATURE_TSF_PLUS_NOIRQ)
static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_register_timestamp_callback(hdd_tx_timestamp);
	return HDD_TSF_OP_SUCC;
}

static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	QDF_TIMER_STATE capture_req_timer_status;
	qdf_mc_timer_t *cap_timer;
	struct hdd_adapter *adapter, *adapternode_ptr, *next_ptr;

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_deregister_timestamp_callback();

	status = hdd_get_front_adapter(hdd_ctx, &adapternode_ptr);

	while (adapternode_ptr && QDF_STATUS_SUCCESS == status) {
		adapter = adapternode_ptr;
		status =
		    hdd_get_next_adapter(hdd_ctx, adapternode_ptr, &next_ptr);
		adapternode_ptr = next_ptr;
		if (adapter->host_capture_req_timer.state == 0)
			continue;
		cap_timer = &adapter->host_capture_req_timer;
		capture_req_timer_status =
			qdf_mc_timer_get_current_state(cap_timer);

		if (capture_req_timer_status != QDF_TIMER_STATE_UNUSED) {
			qdf_mc_timer_stop(cap_timer);
			status =
				qdf_mc_timer_destroy(cap_timer);
			if (status != QDF_STATUS_SUCCESS)
				hdd_err("remove timer failed: %d", status);
		}
	}

	return HDD_TSF_OP_SUCC;
}

#elif defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC)
static
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{
	int ret;
	QDF_STATUS status;
	uint32_t tsf_sync_gpio_pin = TSF_GPIO_PIN_INVALID;

	status = ucfg_fwol_get_tsf_sync_host_gpio_pin(hdd_ctx->psoc,
						      &tsf_sync_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("tsf gpio irq host pin error");
		goto fail;
	}

	if (tsf_sync_gpio_pin == TSF_GPIO_PIN_INVALID) {
		hdd_err("gpio host pin is invalid");
		goto fail;
	}

	ret = gpio_request(tsf_sync_gpio_pin, "wlan_tsf");
	if (ret) {
		hdd_err("gpio host pin is invalid");
		goto fail;
	}

	ret = gpio_direction_output(tsf_sync_gpio_pin, OUTPUT_LOW);
	if (ret) {
		hdd_err("gpio host pin is invalid");
		goto fail_free_gpio;
	}

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_register_timestamp_callback(hdd_tx_timestamp);

	return HDD_TSF_OP_SUCC;

fail_free_gpio:
	gpio_free(tsf_sync_gpio_pin);
fail:
	return HDD_TSF_OP_FAIL;
}

static
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint32_t tsf_sync_gpio_pin = TSF_GPIO_PIN_INVALID;

	status = ucfg_fwol_get_tsf_sync_host_gpio_pin(hdd_ctx->psoc,
						      &tsf_sync_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (tsf_sync_gpio_pin == TSF_GPIO_PIN_INVALID)
		return QDF_STATUS_E_INVAL;

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_deregister_timestamp_callback();

	gpio_free(tsf_sync_gpio_pin);
	return HDD_TSF_OP_SUCC;
}

#elif defined(WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ)
static
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{
	int ret;
	QDF_STATUS status;
	uint32_t tsf_irq_gpio_pin = TSF_GPIO_PIN_INVALID;

	status = ucfg_fwol_get_tsf_irq_host_gpio_pin(hdd_ctx->psoc,
						     &tsf_irq_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("tsf gpio irq host pin error");
		goto fail;
	}

	if (tsf_irq_gpio_pin == TSF_GPIO_PIN_INVALID) {
		hdd_err("gpio host pin is invalid");
		goto fail;
	}

	ret = gpio_request(tsf_irq_gpio_pin, "wlan_tsf");
	if (ret) {
		hdd_err("gpio host pin is invalid");
		goto fail;
	}

	ret = gpio_direction_input(tsf_irq_gpio_pin);
	if (ret) {
		hdd_err("gpio host pin is invalid");
		goto fail_free_gpio;
	}

	tsf_gpio_irq_num = gpio_to_irq(tsf_irq_gpio_pin);
	if (tsf_gpio_irq_num < 0) {
		hdd_err("fail to get irq: %d", tsf_gpio_irq_num);
		goto fail_free_gpio;
	}

	ret = request_irq(tsf_gpio_irq_num, hdd_tsf_captured_irq_handler,
			  IRQF_SHARED | IRQF_TRIGGER_RISING, "wlan_tsf",
			  hdd_ctx);

	if (ret) {
		hdd_err("Failed to register irq handler: %d", ret);
		goto fail_free_gpio;
	}

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_register_timestamp_callback(hdd_tx_timestamp);

	return HDD_TSF_OP_SUCC;

fail_free_gpio:
	gpio_free(tsf_irq_gpio_pin);
fail:
	tsf_gpio_irq_num = -1;
	return HDD_TSF_OP_FAIL;
}

static
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	uint32_t tsf_irq_gpio_pin = TSF_GPIO_PIN_INVALID;

	status = ucfg_fwol_get_tsf_irq_host_gpio_pin(hdd_ctx->psoc,
						     &tsf_irq_gpio_pin);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (tsf_irq_gpio_pin == TSF_GPIO_PIN_INVALID)
		return QDF_STATUS_E_INVAL;

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_deregister_timestamp_callback();

	if (tsf_gpio_irq_num >= 0) {
		free_irq(tsf_gpio_irq_num, hdd_ctx);
		tsf_gpio_irq_num = -1;
		gpio_free(tsf_irq_gpio_pin);
	}

	return HDD_TSF_OP_SUCC;
}

#elif defined(WLAN_FEATURE_TSF_TIMER_SYNC)
static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{
	return HDD_TSF_OP_SUCC;
}

static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;
	QDF_TIMER_STATE capture_req_timer_status;
	qdf_mc_timer_t *cap_timer;
	struct hdd_adapter *adapter, *adapternode_ptr, *next_ptr;

	status = hdd_get_front_adapter(hdd_ctx, &adapternode_ptr);

	while (adapternode_ptr && QDF_STATUS_SUCCESS == status) {
		adapter = adapternode_ptr;
		status =
		    hdd_get_next_adapter(hdd_ctx, adapternode_ptr, &next_ptr);
		adapternode_ptr = next_ptr;
		if (adapter->host_capture_req_timer.state == 0)
			continue;
		cap_timer = &adapter->host_capture_req_timer;
		capture_req_timer_status =
			qdf_mc_timer_get_current_state(cap_timer);

		if (capture_req_timer_status != QDF_TIMER_STATE_UNUSED) {
			qdf_mc_timer_stop(cap_timer);
			status =
				qdf_mc_timer_destroy(cap_timer);
			if (status != QDF_STATUS_SUCCESS)
				hdd_err_rl("remove timer failed: %d", status);
		}
	}

	return HDD_TSF_OP_SUCC;
}
#else
static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{
	int ret;

	ret = cnss_common_register_tsf_captured_handler(
			hdd_ctx->parent_dev,
			hdd_tsf_captured_irq_handler,
			(void *)hdd_ctx);
	if (ret != 0) {
		hdd_err("Failed to register irq handler: %d", ret);
		return HDD_TSF_OP_FAIL;
	}

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_register_timestamp_callback(hdd_tx_timestamp);
	return HDD_TSF_OP_SUCC;
}

static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	int ret;

	if (hdd_tsf_is_tx_set(hdd_ctx))
		ol_deregister_timestamp_callback();

	ret = cnss_common_unregister_tsf_captured_handler(
				hdd_ctx->parent_dev,
				(void *)hdd_ctx);
	if (ret != 0) {
		hdd_err("Failed to unregister irq handler, ret:%d",
			ret);
		ret = HDD_TSF_OP_FAIL;
	}

	return HDD_TSF_OP_SUCC;
}
#endif

void hdd_tsf_notify_wlan_state_change(struct hdd_adapter *adapter,
				      eConnectionState old_state,
				      eConnectionState new_state)
{
	if (!adapter)
		return;

	if (old_state != eConnectionState_Associated &&
	    new_state == eConnectionState_Associated)
		hdd_start_tsf_sync(adapter);
	else if (old_state == eConnectionState_Associated &&
		 new_state != eConnectionState_Associated)
		hdd_stop_tsf_sync(adapter);
}
#else
static inline void hdd_update_tsf(struct hdd_adapter *adapter, uint64_t tsf)
{
}

static inline int __hdd_indicate_tsf(struct hdd_adapter *adapter,
				     uint32_t *buf, int len)
{
	return (hdd_indicate_tsf_internal(adapter, buf, len) ==
		HDD_TSF_OP_SUCC) ? 0 : -EINVAL;
}

static inline int __hdd_capture_tsf(struct hdd_adapter *adapter,
				    uint32_t *buf, int len)
{
	return (hdd_capture_tsf_internal(adapter, buf, len) ==
		HDD_TSF_OP_SUCC) ? 0 : -EINVAL;
}

static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_init(struct hdd_context *hdd_ctx)
{
	return HDD_TSF_OP_SUCC;
}

static inline
enum hdd_tsf_op_result wlan_hdd_tsf_plus_deinit(struct hdd_context *hdd_ctx)
{
	return HDD_TSF_OP_SUCC;
}
#endif /* WLAN_FEATURE_TSF_PLUS */

int hdd_capture_tsf(struct hdd_adapter *adapter, uint32_t *buf, int len)
{
	return __hdd_capture_tsf(adapter, buf, len);
}

int hdd_indicate_tsf(struct hdd_adapter *adapter, uint32_t *buf, int len)
{
	return __hdd_indicate_tsf(adapter, buf, len);
}

#ifdef WLAN_FEATURE_TSF_PTP
int wlan_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info)

{
	struct hdd_adapter *adapter = netdev_priv(dev);
	struct osif_vdev_sync *vdev_sync;
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return -EINVAL;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return -EAGAIN;

	info->so_timestamping = SOF_TIMESTAMPING_TX_HARDWARE |
				 SOF_TIMESTAMPING_RX_HARDWARE |
				 SOF_TIMESTAMPING_RAW_HARDWARE;
	if (hdd_ctx->ptp_clock)
		info->phc_index = ptp_clock_index(hdd_ctx->ptp_clock);
	else
		info->phc_index = -1;

	osif_vdev_sync_op_stop(vdev_sync);
	return 0;
}

#if (KERNEL_VERSION(4, 1, 0) > LINUX_VERSION_CODE)
/**
 * wlan_ptp_gettime() - return fw ts info to uplayer
 * @ptp: pointer to ptp_clock_info.
 * @ts: pointer to timespec.
 *
 * Return: Describe the execute result of this routine
 */
static int wlan_ptp_gettime(struct ptp_clock_info *ptp, struct timespec *ts)
{
	uint64_t host_time, target_time = 0;
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	uint32_t tsf_reg_read_enabled;
	struct osif_psoc_sync *psoc_sync;
	int errno, status = 0;

	hdd_ctx = qdf_container_of(ptp, struct hdd_context, ptp_cinfo);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return -EINVAL;

	errno = osif_psoc_sync_op_start(hdd_ctx->parent_dev, &psoc_sync);
	if (errno)
		return -EAGAIN;

	adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_GO_MODE);
	if (!adapter) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_CLIENT_MODE);
		if (!adapter) {
			adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
			if (!adapter)
				adapter = hdd_get_adapter(hdd_ctx,
							  QDF_STA_MODE);
				if (!adapter) {
					status = -EOPNOTSUPP;
					goto end;
				}
		}
	}

	host_time = hdd_get_monotonic_host_time(hdd_ctx);
	if (hdd_get_targettime_from_hosttime(adapter, host_time,
					     &target_time)) {
		hdd_err("get invalid target timestamp");
		status = -EINVAL;
		goto end;
	}
	*ts = ns_to_timespec(target_time * NSEC_PER_USEC);

end:
	osif_psoc_sync_op_stop(psoc_sync);
	return status;
}

/**
 * wlan_hdd_phc_init() - phc init
 * @hdd_ctx: pointer to the hdd_contex.
 *
 * Return: NULL
 */
static void wlan_hdd_phc_init(struct hdd_context *hdd_ctx)
{
	hdd_ctx->ptp_cinfo.gettime = wlan_ptp_gettime;

	hdd_ctx->ptp_clock = ptp_clock_register(&hdd_ctx->ptp_cinfo,
						hdd_ctx->parent_dev);
}

/**
 * wlan_hdd_phc_deinit() - phc deinit
 * @hdd_ctx: pointer to the hdd_contex.
 *
 * Return: NULL
 */
static void wlan_hdd_phc_deinit(struct hdd_context *hdd_ctx)
{
	hdd_ctx->ptp_cinfo.gettime = NULL;

	if (hdd_ctx->ptp_clock) {
		ptp_clock_unregister(hdd_ctx->ptp_clock);
		hdd_ctx->ptp_clock = NULL;
	}
}

#else
static int wlan_ptp_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	uint64_t host_time, target_time = 0;
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;
	struct osif_psoc_sync *psoc_sync;
	int errno, status = 0;

	hdd_ctx = qdf_container_of(ptp, struct hdd_context, ptp_cinfo);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return -EINVAL;

	errno = osif_psoc_sync_op_start(hdd_ctx->parent_dev, &psoc_sync);
	if (errno)
		return -EAGAIN;

	adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_GO_MODE);
	if (!adapter) {
		adapter = hdd_get_adapter(hdd_ctx, QDF_P2P_CLIENT_MODE);
		if (!adapter) {
			adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
			if (!adapter)
				adapter = hdd_get_adapter(hdd_ctx,
							  QDF_STA_MODE);
				if (!adapter) {
					status = -EOPNOTSUPP;
					goto end;
				}
		}
	}

	host_time = hdd_get_monotonic_host_time(hdd_ctx);
	if (hdd_get_targettime_from_hosttime(adapter, host_time,
					     &target_time)) {
		hdd_err("get invalid target timestamp");
		status = -EINVAL;
		goto end;
	}
	*ts = ns_to_timespec64(target_time * NSEC_PER_USEC);

end:
	osif_psoc_sync_op_stop(psoc_sync);
	return status;
}

static void wlan_hdd_phc_init(struct hdd_context *hdd_ctx)
{
	hdd_ctx->ptp_cinfo.gettime64 = wlan_ptp_gettime;
	hdd_ctx->ptp_clock = ptp_clock_register(&hdd_ctx->ptp_cinfo,
						hdd_ctx->parent_dev);
}

static void wlan_hdd_phc_deinit(struct hdd_context *hdd_ctx)
{
	hdd_ctx->ptp_cinfo.gettime64 = NULL;

	if (hdd_ctx->ptp_clock) {
		ptp_clock_unregister(hdd_ctx->ptp_clock);
		hdd_ctx->ptp_clock = NULL;
	}
}

#endif
#else

static void wlan_hdd_phc_init(struct hdd_context *hdd_ctx)
{
}

static void wlan_hdd_phc_deinit(struct hdd_context *hdd_ctx)
{
}
#endif /* WLAN_FEATURE_TSF_PTP */

#ifdef WLAN_FEATURE_TSF_TIMER_SYNC
/**
 * hdd_convert_qtime_to_us() - convert qtime to us
 * @time: QTIMER ticks for adrastea and us for Lithium
 *
 * This function converts qtime to us.
 *
 * Return: Time in microseconds
 */
static inline uint64_t
hdd_convert_qtime_to_us(uint64_t time)
{
	return time;
}

#else
static inline uint64_t
hdd_convert_qtime_to_us(uint64_t time)
{
	return qdf_log_timestamp_to_usecs(time);
}
#endif

/**
 * hdd_get_tsf_cb() - handle tsf callback
 * @pcb_cxt: pointer to the hdd_contex
 * @ptsf: pointer to struct stsf
 *
 * This function handle the event that reported by firmware at first.
 * The event contains the vdev_id, current tsf value of this vdev,
 * tsf value is 64bits, discripted in two varaible tsf_low and tsf_high.
 * These two values each is uint32.
 *
 * Return: 0 for success or non-zero negative failure code
 */
int hdd_get_tsf_cb(void *pcb_cxt, struct stsf *ptsf)
{
	struct hdd_context *hddctx;
	struct hdd_adapter *adapter;
	int ret;
	uint64_t tsf_sync_soc_time;
	QDF_STATUS status;
	QDF_TIMER_STATE capture_req_timer_status;
	qdf_mc_timer_t *capture_timer;

	if (!pcb_cxt || !ptsf) {
		hdd_err("HDD context is not valid");
			return -EINVAL;
	}

	hddctx = (struct hdd_context *)pcb_cxt;
	ret = wlan_hdd_validate_context(hddctx);
	if (0 != ret)
		return -EINVAL;

	adapter = hdd_get_adapter_by_vdev(hddctx, ptsf->vdev_id);

	if (!adapter) {
		hdd_err("failed to find adapter");
		return -EINVAL;
	}

	if (!hdd_tsf_is_initialized(adapter)) {
		hdd_err("tsf is not init, ignore tsf event");
		return -EINVAL;
	}

	hdd_info("tsf cb handle event, device_mode is %d",
		adapter->device_mode);

	capture_timer = &adapter->host_capture_req_timer;
	capture_req_timer_status =
		qdf_mc_timer_get_current_state(capture_timer);
	if (capture_req_timer_status == QDF_TIMER_STATE_UNUSED) {
		hdd_warn("invalid timer status");
		return -EINVAL;
	}

	qdf_mc_timer_stop(capture_timer);
	status = qdf_mc_timer_destroy(capture_timer);
	if (status != QDF_STATUS_SUCCESS)
		hdd_warn("destroy cap req timer fail, ret: %d", status);

	adapter->cur_target_time = ((uint64_t)ptsf->tsf_high << 32 |
			 ptsf->tsf_low);

	adapter->cur_target_global_tsf_time =
		((uint64_t)ptsf->global_tsf_high << 32 |
			 ptsf->global_tsf_low);
	tsf_sync_soc_time = ((uint64_t)ptsf->soc_timer_high << 32 |
			ptsf->soc_timer_low);
	adapter->cur_tsf_sync_soc_time =
		hdd_convert_qtime_to_us(tsf_sync_soc_time) * NSEC_PER_USEC;
	complete(&tsf_sync_get_completion_evt);
	hdd_update_tsf(adapter, adapter->cur_target_time);
	hdd_info("Vdev=%u, tsf_low=%u, tsf_high=%u ptsf->soc_timer_low=%u ptsf->soc_timer_high=%u",
		 ptsf->vdev_id, ptsf->tsf_low, ptsf->tsf_high,
		 ptsf->soc_timer_low, ptsf->soc_timer_high);
	return 0;
}

const struct nla_policy tsf_policy[QCA_WLAN_VENDOR_ATTR_TSF_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TSF_CMD] = {.type = NLA_U32},
};

/**
 * __wlan_hdd_cfg80211_handle_tsf_cmd(): Setup TSF operations
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Handle TSF SET / GET operation from userspace
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_handle_tsf_cmd(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_TSF_MAX + 1];
	int status, ret;
	struct sk_buff *reply_skb;
	uint32_t tsf_op_resp[3] = {0}, tsf_cmd;

	hdd_enter_dev(wdev->netdev);

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return -EINVAL;

	if (wlan_cfg80211_nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_TSF_MAX,
				    data, data_len, tsf_policy)) {
		hdd_err("Invalid TSF cmd");
		return -EINVAL;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_TSF_CMD]) {
		hdd_err("Invalid TSF cmd");
		return -EINVAL;
	}
	tsf_cmd = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TSF_CMD]);

	if (tsf_cmd == QCA_TSF_CAPTURE || tsf_cmd == QCA_TSF_SYNC_GET) {
		hdd_capture_tsf(adapter, tsf_op_resp, 1);
		switch (tsf_op_resp[0]) {
		case TSF_RETURN:
			status = 0;
			break;
		case TSF_CURRENT_IN_CAP_STATE:
			status = -EALREADY;
			break;
		case TSF_STA_NOT_CONNECTED_NO_TSF:
		case TSF_SAP_NOT_STARTED_NO_TSF:
			status = -EPERM;
			break;
		default:
		case TSF_CAPTURE_FAIL:
			status = -EINVAL;
			break;
		}
	}
	if (status < 0)
		goto end;

	if (tsf_cmd == QCA_TSF_SYNC_GET) {
		ret = wait_for_completion_timeout(&tsf_sync_get_completion_evt,
			msecs_to_jiffies(WLAN_TSF_SYNC_GET_TIMEOUT));
		if (ret == 0) {
			status = -ETIMEDOUT;
			goto end;
		}
	}

	if (tsf_cmd == QCA_TSF_GET || tsf_cmd == QCA_TSF_SYNC_GET) {
		hdd_indicate_tsf(adapter, tsf_op_resp, 3);
		switch (tsf_op_resp[0]) {
		case TSF_RETURN:
			status = 0;
			break;
		case TSF_NOT_RETURNED_BY_FW:
			status = -EINPROGRESS;
			break;
		case TSF_STA_NOT_CONNECTED_NO_TSF:
		case TSF_SAP_NOT_STARTED_NO_TSF:
			status = -EPERM;
			break;
		default:
			status = -EINVAL;
			break;
		}
		if (status != 0)
			goto end;

		reply_skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy, NULL,
					sizeof(uint64_t) * 2 + NLMSG_HDRLEN,
					QCA_NL80211_VENDOR_SUBCMD_TSF_INDEX,
					GFP_KERNEL);
		if (!reply_skb) {
			hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
			status = -ENOMEM;
			goto end;
		}
		if (hdd_wlan_nla_put_u64(reply_skb,
				QCA_WLAN_VENDOR_ATTR_TSF_TIMER_VALUE,
				adapter->cur_target_time) ||
		    hdd_wlan_nla_put_u64(reply_skb,
				QCA_WLAN_VENDOR_ATTR_TSF_SOC_TIMER_VALUE,
				adapter->cur_tsf_sync_soc_time)) {
			hdd_err("nla put fail");
			kfree_skb(reply_skb);
			status = -EINVAL;
			goto end;
		}
		status = cfg80211_vendor_cmd_reply(reply_skb);
	}

end:
	hdd_info("TSF operation %d status: %d", tsf_cmd, status);
	return status;
}

int wlan_hdd_cfg80211_handle_tsf_cmd(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_handle_tsf_cmd(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_tsf_init() - set callback to handle tsf value.
 * @hdd_ctx: pointer to the struct hdd_context
 *
 * This function set the callback to sme module, the callback will be
 * called when a tsf event is reported by firmware
 *
 * Return: none
 */
void wlan_hdd_tsf_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	if (!hdd_ctx)
		return;

	if (qdf_atomic_inc_return(&hdd_ctx->tsf_ready_flag) > 1)
		return;

	qdf_atomic_init(&hdd_ctx->cap_tsf_flag);

	status = hdd_tsf_set_gpio(hdd_ctx);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_debug("set tsf GPIO failed, status: %d", status);
		goto fail;
	}

	if (wlan_hdd_tsf_plus_init(hdd_ctx) != HDD_TSF_OP_SUCC)
		goto fail;

	if (hdd_tsf_is_ptp_enabled(hdd_ctx))
		wlan_hdd_phc_init(hdd_ctx);

	return;

fail:
	qdf_atomic_set(&hdd_ctx->tsf_ready_flag, 0);
}

void wlan_hdd_tsf_deinit(struct hdd_context *hdd_ctx)
{
	if (!hdd_ctx)
		return;

	if (!qdf_atomic_read(&hdd_ctx->tsf_ready_flag))
		return;

	if (hdd_tsf_is_ptp_enabled(hdd_ctx))
		wlan_hdd_phc_deinit(hdd_ctx);
	wlan_hdd_tsf_plus_deinit(hdd_ctx);
	qdf_atomic_set(&hdd_ctx->tsf_ready_flag, 0);
	qdf_atomic_set(&hdd_ctx->cap_tsf_flag, 0);
}
