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

#include <linux/platform_device.h>
#include <linux/pci.h>
#include "cds_api.h"
#include "qdf_status.h"
#include "qdf_lock.h"
#include "cds_sched.h"
#include "osdep.h"
#include "hif.h"
#include "htc.h"
#include "epping_main.h"
#include "osif_sync.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_power.h"
#include "wlan_logging_sock_svc.h"
#include "wma_api.h"
#include "wlan_hdd_napi.h"
#include "wlan_policy_mgr_api.h"
#include "qwlan_version.h"
#include "bmi.h"
#include <ol_defines.h>
#include "cdp_txrx_bus.h"
#include "cdp_txrx_misc.h"
#include "pld_common.h"
#include "wlan_hdd_driver_ops.h"
#include "wlan_ipa_ucfg_api.h"
#include "wlan_hdd_debugfs.h"
#include "cfg_ucfg_api.h"
#include <linux/suspend.h>
#include <qdf_notifier.h>
#include <qdf_hang_event_notifier.h>
#include "wlan_hdd_thermal.h"

#ifdef MODULE
#define WLAN_MODULE_NAME  module_name(THIS_MODULE)
#else
#define WLAN_MODULE_NAME  "wlan"
#endif

#define SSR_MAX_FAIL_CNT 3
static uint8_t re_init_fail_cnt, probe_fail_cnt;

/* An atomic flag to check if SSR cleanup has been done or not */
static qdf_atomic_t is_recovery_cleanup_done;

/* firmware/host hang event data */
static uint8_t g_fw_host_hang_event[QDF_HANG_EVENT_DATA_SIZE];

/*
 * In BMI Phase we are only sending small chunk (256 bytes) of the FW image at
 * a time, and wait for the completion interrupt to start the next transfer.
 * During this phase, the KRAIT is entering IDLE/StandAlone(SA) Power Save(PS).
 * The delay incurred for resuming from IDLE/SA PS is huge during driver load.
 * So prevent APPS IDLE/SA PS durint driver load for reducing interrupt latency.
 */

static inline void hdd_request_pm_qos(struct device *dev, int val)
{
	pld_request_pm_qos(dev, val);
}

static inline void hdd_remove_pm_qos(struct device *dev)
{
	pld_remove_pm_qos(dev);
}

/**
 * hdd_get_bandwidth_level() - get current bandwidth level
 * @data: Context
 *
 * Return: current bandwidth level
 */
static int hdd_get_bandwidth_level(void *data)
{
	int ret = PLD_BUS_WIDTH_NONE;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (hdd_ctx)
		ret = hdd_get_current_throughput_level(hdd_ctx);

	return ret;
}

#ifdef DP_MEM_PRE_ALLOC

/**
 * hdd_get_consistent_mem_unaligned() - API to get consistent unaligned mem
 * @size: Size of memory required
 * @paddr: Pointer to paddr to be filled in by API
 * @ring_type: Pointer to ring type for which consistent memory is needed
 *
 * Return: Virtual address of consistent memory on success, else null
 */
static
void *hdd_get_consistent_mem_unaligned(size_t size,
				       qdf_dma_addr_t *paddr,
				       uint32_t ring_type)
{
	return dp_prealloc_get_consistent_mem_unaligned(size, paddr,
							ring_type);
}

/**
 * hdd_put_consistent_mem_unaligned() - API to put consistent unaligned mem
 * @vaddr: Virtual address of memory
 *
 * Return: None
 */
static
void hdd_put_consistent_mem_unaligned(void *vaddr)
{
	dp_prealloc_put_consistent_mem_unaligned(vaddr);
}

#else
static
void *hdd_get_consistent_mem_unaligned(size_t size,
				       qdf_dma_addr_t *paddr,
				       uint32_t ring_type)
{
	hdd_err_rl("prealloc not support!");

	return NULL;
}

static
void hdd_put_consistent_mem_unaligned(void *vaddr)
{
	hdd_err_rl("prealloc not support!");
}
#endif

/**
 * hdd_set_recovery_in_progress() - API to set recovery in progress
 * @data: Context
 * @val: Value to set
 *
 * Return: None
 */
static void hdd_set_recovery_in_progress(void *data, uint8_t val)
{
	cds_set_recovery_in_progress(val);
}

/**
 * hdd_is_driver_unloading() - API to query if driver is unloading
 * @data: Private Data
 *
 * Return: True/False
 */
static bool hdd_is_driver_unloading(void *data)
{
	return cds_is_driver_unloading();
}

/**
 * hdd_is_load_or_unload_in_progress() - API to query if driver is
 * loading/unloading
 * @data: Private Data
 *
 * Return: bool
 */
static bool hdd_is_load_or_unload_in_progress(void *data)
{
	return cds_is_load_or_unload_in_progress();
}

/**
 * hdd_is_recovery_in_progress() - API to query if recovery in progress
 * @data: Private Data
 *
 * Return: bool
 */
static bool hdd_is_recovery_in_progress(void *data)
{
	return cds_is_driver_recovering();
}

/**
 * hdd_is_target_ready() - API to query if target is in ready state
 * @data: Private Data
 *
 * Return: bool
 */
static bool hdd_is_target_ready(void *data)
{
	return cds_is_target_ready();
}

/**
 * hdd_hif_init_driver_state_callbacks() - API to initialize HIF callbacks
 * @data: Private Data
 * @cbk: HIF Driver State callbacks
 *
 * HIF should be independent of CDS calls. Pass CDS Callbacks to HIF, HIF will
 * call the callbacks.
 *
 * Return: void
 */
static void hdd_hif_init_driver_state_callbacks(void *data,
			struct hif_driver_state_callbacks *cbk)
{
	cbk->context = data;
	cbk->set_recovery_in_progress = hdd_set_recovery_in_progress;
	cbk->is_recovery_in_progress = hdd_is_recovery_in_progress;
	cbk->is_load_unload_in_progress = hdd_is_load_or_unload_in_progress;
	cbk->is_driver_unloading = hdd_is_driver_unloading;
	cbk->is_target_ready = hdd_is_target_ready;
	cbk->get_bandwidth_level = hdd_get_bandwidth_level;
	cbk->prealloc_get_consistent_mem_unaligned =
		hdd_get_consistent_mem_unaligned;
	cbk->prealloc_put_consistent_mem_unaligned =
		hdd_put_consistent_mem_unaligned;
}

#ifdef HIF_DETECTION_LATENCY_ENABLE
void hdd_hif_set_enable_detection(struct hif_opaque_softc *hif_ctx, bool value)
{
	hif_set_enable_detection(hif_ctx, value);
}
#endif

#ifdef FORCE_WAKE
void hdd_set_hif_init_phase(struct hif_opaque_softc *hif_ctx,
			    bool hal_init_phase)
{
	hif_srng_init_phase(hif_ctx, hal_init_phase);
}
#endif /* FORCE_WAKE */

/**

 * hdd_hif_set_attribute() - API to set CE attribute if memory is limited
 * @hif_ctx: hif context
 *
 * Return: None
 */
#ifdef SLUB_MEM_OPTIMIZE
static void hdd_hif_set_attribute(struct hif_opaque_softc *hif_ctx)
{
	hif_set_attribute(hif_ctx, HIF_LOWDESC_CE_NO_PKTLOG_CFG);
}
#else
static void hdd_hif_set_attribute(struct hif_opaque_softc *hif_ctx)
{}
#endif

/**
 * hdd_hif_register_shutdown_notifier() - Register HIF shutdown notifier
 * @hif_ctx: HIF Context
 *
 * Return: success/failure
 */
static QDF_STATUS
hdd_hif_register_shutdown_notifier(struct hif_opaque_softc *hif_ctx)
{
	return cds_shutdown_notifier_register(
					hif_shutdown_notifier_cb,
					hif_ctx);
}

/**

 * hdd_hif_set_ce_max_yield_time() - Wrapper API to set CE max yield time
 * @hif_ctx: hif context
 * @bus_type: underlying bus type
 * @ce_service_max_yield_time: max yield time to be set
 *
 * Return: None
 */
#if defined(CONFIG_SLUB_DEBUG_ON)
#define CE_SNOC_MAX_YIELD_TIME_US 2000

static void hdd_hif_set_ce_max_yield_time(struct hif_opaque_softc *hif_ctx,
					  enum qdf_bus_type bus_type,
					  uint32_t ce_service_max_yield_time)
{
	if (bus_type == QDF_BUS_TYPE_SNOC &&
	    ce_service_max_yield_time < CE_SNOC_MAX_YIELD_TIME_US)
		ce_service_max_yield_time = CE_SNOC_MAX_YIELD_TIME_US;

	hif_set_ce_service_max_yield_time(hif_ctx, ce_service_max_yield_time);
}

#else
static void hdd_hif_set_ce_max_yield_time(struct hif_opaque_softc *hif_ctx,
					  enum qdf_bus_type bus_type,
					  uint32_t ce_service_max_yield_time)
{
	hif_set_ce_service_max_yield_time(hif_ctx, ce_service_max_yield_time);
}
#endif

/**
 * hdd_init_cds_hif_context() - API to set CDS HIF Context
 * @hif: HIF Context
 *
 * Return: success/failure
 */
static int hdd_init_cds_hif_context(void *hif)
{
	QDF_STATUS status;

	status = cds_set_context(QDF_MODULE_ID_HIF, hif);

	if (status)
		return -ENOENT;

	return 0;
}

/**
 * hdd_deinit_cds_hif_context() - API to clear CDS HIF COntext
 *
 * Return: None
 */
static void hdd_deinit_cds_hif_context(void)
{
	QDF_STATUS status;

	status = cds_set_context(QDF_MODULE_ID_HIF, NULL);

	if (status)
		hdd_err("Failed to reset CDS HIF Context");
}

/**
 * to_bus_type() - Map PLD bus type to low level bus type
 * @bus_type: PLD bus type
 *
 * Map PLD bus type to low level bus type.
 *
 * Return: low level bus type.
 */
static enum qdf_bus_type to_bus_type(enum pld_bus_type bus_type)
{
	switch (bus_type) {
	case PLD_BUS_TYPE_PCIE_FW_SIM:
	case PLD_BUS_TYPE_PCIE:
		return QDF_BUS_TYPE_PCI;
	case PLD_BUS_TYPE_SNOC_FW_SIM:
	case PLD_BUS_TYPE_SNOC:
		return QDF_BUS_TYPE_SNOC;
	case PLD_BUS_TYPE_SDIO:
		return QDF_BUS_TYPE_SDIO;
	case PLD_BUS_TYPE_USB:
		return QDF_BUS_TYPE_USB;
	case PLD_BUS_TYPE_IPCI_FW_SIM:
	case PLD_BUS_TYPE_IPCI:
		return QDF_BUS_TYPE_IPCI;
	default:
		return QDF_BUS_TYPE_NONE;
	}
}

int hdd_hif_open(struct device *dev, void *bdev, const struct hif_bus_id *bid,
			enum qdf_bus_type bus_type, bool reinit)
{
	QDF_STATUS status;
	int ret = 0;
	struct hif_opaque_softc *hif_ctx;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	struct hif_driver_state_callbacks cbk;
	uint32_t mode = cds_get_conparam();
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx error");
		return -EFAULT;
	}

	hdd_hif_init_driver_state_callbacks(dev, &cbk);

	hif_ctx = hif_open(qdf_ctx, mode, bus_type, &cbk, hdd_ctx->psoc);
	if (!hif_ctx) {
		hdd_err("hif_open error");
		return -ENOMEM;
	}

	ret = hdd_init_cds_hif_context(hif_ctx);
	if (ret) {
		hdd_err("Failed to set global HIF CDS Context err: %d", ret);
		goto err_hif_close;
	}

	status = hdd_hif_register_shutdown_notifier(hif_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Shutdown notifier register failed: %d", status);
		goto err_deinit_hif_context;
	}

	hdd_hif_set_attribute(hif_ctx);

	status = hif_enable(hif_ctx, dev, bdev, bid, bus_type,
			    (reinit == true) ?  HIF_ENABLE_TYPE_REINIT :
			    HIF_ENABLE_TYPE_PROBE);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hif_enable failed status: %d, reinit: %d",
			status, reinit);

		ret = qdf_status_to_os_return(status);
		goto err_deinit_hif_context;
	} else {
		cds_set_target_ready(true);
		ret = hdd_napi_create();
		hdd_debug("hdd_napi_create returned: %d", ret);
		if (ret == 0)
			hdd_debug("NAPI: no instances are created");
		else if (ret < 0) {
			hdd_err("NAPI creation error, rc: 0x%x, reinit: %d",
				ret, reinit);
			ret = -EFAULT;
			goto mark_target_not_ready;
		} else {
			hdd_napi_event(NAPI_EVT_INI_FILE,
				(void *)hdd_ctx->napi_enable);
		}
	}

	hdd_hif_set_ce_max_yield_time(
				hif_ctx, bus_type,
				cfg_get(hdd_ctx->psoc,
					CFG_DP_CE_SERVICE_MAX_YIELD_TIME));
	ucfg_pmo_psoc_set_hif_handle(hdd_ctx->psoc, hif_ctx);
	hif_set_ce_service_max_rx_ind_flush(hif_ctx,
				cfg_get(hdd_ctx->psoc,
					CFG_DP_CE_SERVICE_MAX_RX_IND_FLUSH));
	return 0;

mark_target_not_ready:
	cds_set_target_ready(false);

err_deinit_hif_context:
	hdd_deinit_cds_hif_context();

err_hif_close:
	hif_close(hif_ctx);
	return ret;
}

void hdd_hif_close(struct hdd_context *hdd_ctx, void *hif_ctx)
{
	if (!hdd_ctx) {
		hdd_err("hdd_ctx error");
		return;
	}

	if (!hif_ctx)
		return;

	cds_set_target_ready(false);
	hif_disable(hif_ctx, HIF_DISABLE_TYPE_REMOVE);

	hdd_napi_destroy(true);

	hdd_deinit_cds_hif_context();
	hif_close(hif_ctx);

	ucfg_pmo_psoc_set_hif_handle(hdd_ctx->psoc, NULL);
}

/**
 * hdd_init_qdf_ctx() - API to initialize global QDF Device structure
 * @dev: Device Pointer
 * @bdev: Bus Device pointer
 * @bus_type: Underlying bus type
 * @bid: Bus id passed by platform driver
 *
 * Return: 0 - success, < 0 - failure
 */
static int hdd_init_qdf_ctx(struct device *dev, void *bdev,
			    enum qdf_bus_type bus_type,
			    const struct hif_bus_id *bid)
{
	qdf_device_t qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_dev) {
		hdd_err("Invalid QDF device");
		return -EINVAL;
	}

	qdf_dev->dev = dev;
	qdf_dev->drv_hdl = bdev;
	qdf_dev->bus_type = bus_type;
	qdf_dev->bid = bid;

	if (cds_smmu_mem_map_setup(qdf_dev, ucfg_ipa_is_ready()) !=
		QDF_STATUS_SUCCESS) {
		hdd_err("cds_smmu_mem_map_setup() failed");
		return -EFAULT;
	}

	return 0;
}

/**
 * check_for_probe_defer() - API to check return value
 * @ret: Return Value
 *
 * Return: return -EPROBE_DEFER to platform driver if return value
 * is -ENOMEM. Platform driver will try to re-probe.
 */
#ifdef MODULE
static int check_for_probe_defer(int ret)
{
	return ret;
}
#else
static int check_for_probe_defer(int ret)
{
	if (ret == -ENOMEM)
		return -EPROBE_DEFER;
	return ret;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
static void hdd_abort_system_suspend(struct device *dev)
{
	qdf_pm_system_wakeup();
}
#else
static void hdd_abort_system_suspend(struct device *dev)
{
}
#endif

int hdd_soc_idle_restart_lock(struct device *dev)
{
	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_RESTART);

	hdd_abort_system_suspend(dev);

	return 0;
}

void hdd_soc_idle_restart_unlock(void)
{
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_RESTART);
}

static void hdd_soc_load_lock(struct device *dev)
{
	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);
	hdd_request_pm_qos(dev, DISABLE_KRAIT_IDLE_PS_VAL);
}

static void hdd_soc_load_unlock(struct device *dev)
{
	hdd_remove_pm_qos(dev);
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_INIT);
}

#ifdef DP_MEM_PRE_ALLOC
/**
 * hdd_init_dma_mask() - Set the DMA mask for dma memory pre-allocation
 * @dev: device handle
 * @bus_type: Bus type for which init is being done
 *
 * Return: 0 - success, non-zero on failure
 */
static int hdd_init_dma_mask(struct device *dev, enum qdf_bus_type bus_type)
{
	return hif_init_dma_mask(dev, bus_type);
}
#else
static inline int
hdd_init_dma_mask(struct device *dev, enum qdf_bus_type bus_type)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static int __hdd_soc_probe(struct device *dev,
			   void *bdev,
			   const struct hif_bus_id *bid,
			   enum qdf_bus_type bus_type)
{
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	int errno;

	hdd_info("probing driver");

	hdd_soc_load_lock(dev);
	cds_set_load_in_progress(true);
	cds_set_driver_in_bad_state(false);
	cds_set_recovery_in_progress(false);

	errno = hdd_init_qdf_ctx(dev, bdev, bus_type, bid);
	if (errno)
		goto unlock;

	errno = hdd_init_dma_mask(dev, bus_type);
	if (errno)
		goto unlock;

	hdd_ctx = hdd_context_create(dev);
	if (IS_ERR(hdd_ctx)) {
		errno = PTR_ERR(hdd_ctx);
		goto assert_fail_count;
	}

	status = dp_prealloc_init((struct cdp_ctrl_objmgr_psoc *)hdd_ctx->psoc);

	if (status != QDF_STATUS_SUCCESS) {
		errno = qdf_status_to_os_return(status);
		goto dp_prealloc_fail;
	}

	errno = hdd_wlan_startup(hdd_ctx);
	if (errno)
		goto hdd_context_destroy;

	status = hdd_psoc_create_vdevs(hdd_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		errno = qdf_status_to_os_return(status);
		goto wlan_exit;
	}

	probe_fail_cnt = 0;
	cds_set_driver_loaded(true);
	cds_set_load_in_progress(false);
	hdd_start_complete(0);
	hdd_thermal_mitigation_register(hdd_ctx, dev);

	hdd_soc_load_unlock(dev);

	return 0;

wlan_exit:
	hdd_wlan_exit(hdd_ctx);

hdd_context_destroy:
	dp_prealloc_deinit();

dp_prealloc_fail:
	hdd_context_destroy(hdd_ctx);

assert_fail_count:
	probe_fail_cnt++;
	hdd_err("consecutive probe failures:%u", probe_fail_cnt);
	QDF_BUG(probe_fail_cnt < SSR_MAX_FAIL_CNT);

unlock:
	cds_set_load_in_progress(false);
	hdd_soc_load_unlock(dev);

	return check_for_probe_defer(errno);
}

/**
 * hdd_soc_probe() - perform SoC probe
 * @dev: kernel device being probed
 * @bdev: bus device structure
 * @bid: bus identifier for shared busses
 * @bus_type: underlying bus type
 *
 * A SoC probe indicates new SoC hardware has become available and needs to be
 * initialized.
 *
 * Return: Errno
 */
static int hdd_soc_probe(struct device *dev,
			 void *bdev,
			 const struct hif_bus_id *bid,
			 enum qdf_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	hdd_info("probing driver");

	errno = osif_psoc_sync_create_and_trans(&psoc_sync);
	if (errno)
		return errno;

	osif_psoc_sync_register(dev, psoc_sync);
	errno = __hdd_soc_probe(dev, bdev, bid, bus_type);
	if (errno)
		goto destroy_sync;

	osif_psoc_sync_trans_stop(psoc_sync);

	return 0;

destroy_sync:
	osif_psoc_sync_unregister(dev);
	osif_psoc_sync_wait_for_ops(psoc_sync);

	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);

	return errno;
}

static int __hdd_soc_recovery_reinit(struct device *dev,
				     void *bdev,
				     const struct hif_bus_id *bid,
				     enum qdf_bus_type bus_type)
{
	int errno;

	hdd_info("re-probing driver");

	hdd_soc_load_lock(dev);
	cds_set_driver_in_bad_state(false);

	errno = hdd_init_qdf_ctx(dev, bdev, bus_type, bid);
	if (errno)
		goto unlock;

	errno = hdd_wlan_re_init();
	if (errno) {
		re_init_fail_cnt++;
		goto assert_fail_count;
	}

	re_init_fail_cnt = 0;

	/*
	 * In case of SSR within SSR we have seen the race
	 * where the reinit is successful and fw down is received
	 * which sets the recovery in progress. Now as reinit is
	 * successful we reset the recovery in progress here.
	 * So check if FW is down then don't reset the recovery
	 * in progress
	 */
	if (!qdf_is_fw_down()) {
		cds_set_recovery_in_progress(false);
		hdd_handle_cached_commands();
	}

	hdd_soc_load_unlock(dev);
	hdd_start_complete(0);

	return 0;

assert_fail_count:
	hdd_err("consecutive reinit failures:%u", re_init_fail_cnt);
	QDF_BUG(re_init_fail_cnt < SSR_MAX_FAIL_CNT);

unlock:
	cds_set_driver_in_bad_state(true);
	hdd_soc_load_unlock(dev);
	hdd_start_complete(errno);

	return check_for_probe_defer(errno);
}

/**
 * hdd_soc_recovery_reinit() - perform PDR/SSR SoC reinit
 * @dev: the kernel device being re-initialized
 * @bdev: bus device structure
 * @bid: bus identifier for shared busses
 * @bus_type: underlying bus type
 *
 * When communication with firmware breaks down, a SoC recovery process kicks in
 * with two phases: shutdown and reinit.
 *
 * SSR reinit is similar to a 'probe' but happens in response to an SSR
 * shutdown. The idea is to re-initialize the SoC to as close to its old,
 * pre-communications-breakdown configuration as possible. This is completely
 * transparent from a userspace point of view.
 *
 * Return: Errno
 */
static int hdd_soc_recovery_reinit(struct device *dev,
				   void *bdev,
				   const struct hif_bus_id *bid,
				   enum qdf_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	/* if driver is unloading, there is no need to do SSR */
	if (qdf_is_driver_unloading()) {
		hdd_info("driver is unloading, avoid SSR");
		return 0;
	}

	/* SSR transition is initiated at the beginning of soc shutdown */
	errno = osif_psoc_sync_trans_resume(dev, &psoc_sync);
	QDF_BUG(!errno);
	if (errno)
		return errno;

	errno = __hdd_soc_recovery_reinit(dev, bdev, bid, bus_type);


	osif_psoc_sync_trans_stop(psoc_sync);

	return errno;
}

static void __hdd_soc_remove(struct device *dev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	void *hif_ctx;

	QDF_BUG(hdd_ctx);
	if (!hdd_ctx)
		return;

	pr_info("%s: Removing driver v%s\n", WLAN_MODULE_NAME,
		QWLAN_VERSIONSTR);

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (hif_ctx) {
		/*
		 * Trigger runtime sync resume before setting unload in progress
		 * such that resume can happen successfully
		 */
		hif_pm_runtime_sync_resume(hif_ctx);
	}

	cds_set_driver_loaded(false);
	cds_set_unload_in_progress(true);
	if (!hdd_wait_for_debugfs_threads_completion())
		hdd_warn("Debugfs threads are still active attempting driver unload anyway");

	if (hdd_get_conparam() == QDF_GLOBAL_EPPING_MODE) {
		hdd_wlan_stop_modules(hdd_ctx, false);
		qdf_nbuf_deinit_replenish_timer();
	} else {
		hdd_thermal_mitigation_unregister(hdd_ctx, dev);
		hdd_wlan_exit(hdd_ctx);
	}

	hdd_context_destroy(hdd_ctx);

	cds_set_driver_in_bad_state(false);
	cds_set_unload_in_progress(false);

	dp_prealloc_deinit();

	pr_info("%s: Driver De-initialized\n", WLAN_MODULE_NAME);
}

/**
 * hdd_soc_remove() - perform SoC remove
 * @dev: the kernel device being removed
 *
 * A SoC remove indicates the attached SoC hardware is about to go away and
 * needs to be cleaned up.
 *
 * Return: void
 */
static void hdd_soc_remove(struct device *dev)
{
	__hdd_soc_remove(dev);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * hdd_wlan_ssr_shutdown_event()- send ssr shutdown state
 *
 * This Function send send ssr shutdown state diag event
 *
 * Return: void.
 */
static void hdd_wlan_ssr_shutdown_event(void)
{
	WLAN_HOST_DIAG_EVENT_DEF(ssr_shutdown,
					struct host_event_wlan_ssr_shutdown);
	qdf_mem_zero(&ssr_shutdown, sizeof(ssr_shutdown));
	ssr_shutdown.status = SSR_SUB_SYSTEM_SHUTDOWN;
	WLAN_HOST_DIAG_EVENT_REPORT(&ssr_shutdown,
					EVENT_WLAN_SSR_SHUTDOWN_SUBSYSTEM);
}
#else
static inline void hdd_wlan_ssr_shutdown_event(void) { }
#endif

/**
 * hdd_send_hang_data() - Send hang data to userspace
 * @data: Hang data
 *
 * Return: None
 */
static void hdd_send_hang_data(uint8_t *data, size_t data_len)
{
	enum qdf_hang_reason reason = QDF_REASON_UNSPECIFIED;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	cds_get_recovery_reason(&reason);
	cds_reset_recovery_reason();
	wlan_hdd_send_hang_reason_event(hdd_ctx, reason, data, data_len);
}

/**
 * hdd_psoc_shutdown_notify() - notify the various interested parties that the
 *	soc is starting recovery shutdown
 * @hdd_ctx: the HDD context corresponding to the soc undergoing shutdown
 *
 * Return: None
 */
static void hdd_psoc_shutdown_notify(struct hdd_context *hdd_ctx)
{
	wlan_cfg80211_cleanup_scan_queue(hdd_ctx->pdev, NULL);

	if (ucfg_ipa_is_enabled()) {
		ucfg_ipa_uc_force_pipe_shutdown(hdd_ctx->pdev);

		if (pld_is_fw_rejuvenate(hdd_ctx->parent_dev) ||
		    pld_is_pdr(hdd_ctx->parent_dev))
			ucfg_ipa_fw_rejuvenate_send_msg(hdd_ctx->pdev);
	}

	cds_shutdown_notifier_call();
	cds_shutdown_notifier_purge();

	hdd_wlan_ssr_shutdown_event();
}

/**
 * hdd_soc_recovery_cleanup() - Perform SSR related cleanup activities.
 *
 * This function will perform cleanup activities related to when driver
 * undergoes SSR. Activities inclues stopping idle timer and invoking shutdown
 * notifier.
 *
 * Return: None
 */
static void hdd_soc_recovery_cleanup(void)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	/* cancel/flush any pending/active idle shutdown work */
	hdd_psoc_idle_timer_stop(hdd_ctx);
	hdd_bus_bw_compute_timer_stop(hdd_ctx);

	/* nothing to do if the soc is already unloaded */
	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_info("Driver modules are already closed");
		return;
	}

	if (cds_is_load_or_unload_in_progress()) {
		hdd_info("Load/unload in progress, ignore SSR shutdown");
		return;
	}

	hdd_psoc_shutdown_notify(hdd_ctx);
}

static void __hdd_soc_recovery_shutdown(void)
{
	struct hdd_context *hdd_ctx;
	void *hif_ctx;

	/* recovery starts via firmware down indication; ensure we got one */
	QDF_BUG(cds_is_driver_recovering());
	hdd_init_start_completion();

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is null");
		return;
	}

	/*
	 * Perform SSR related cleanup if it has not already been done as a
	 * part of receiving the uevent.
	 */
	if (!qdf_atomic_read(&is_recovery_cleanup_done))
		hdd_soc_recovery_cleanup();
	else
		qdf_atomic_set(&is_recovery_cleanup_done, 0);

	if (!hdd_wait_for_debugfs_threads_completion())
		hdd_err("Debufs threads are still pending, attempting SSR anyway");

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get HIF context, ignore SSR shutdown");
		return;
	}

	/* mask the host controller interrupts */
	hif_mask_interrupt_call(hif_ctx);

	if (!QDF_IS_EPPING_ENABLED(cds_get_conparam())) {
		hif_disable_isr(hif_ctx);
		hdd_wlan_shutdown();
	}
}

/**
 * hdd_soc_recovery_shutdown() - perform PDR/SSR SoC shutdown
 * @dev: the device to shutdown
 *
 * When communication with firmware breaks down, a SoC recovery process kicks in
 * with two phases: shutdown and reinit.
 *
 * SSR shutdown is similar to a 'remove' but without communication with
 * firmware. The idea is to retain as much SoC configuration as possible, so it
 * can be re-initialized to the same state after a reset. This is completely
 * transparent from a userspace point of view.
 *
 * Return: void
 */
static void hdd_soc_recovery_shutdown(struct device *dev)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	/* if driver is unloading, there is no need to do SSR */
	if (qdf_is_driver_unloading()) {
		hdd_info("driver is unloading, avoid SSR");
		return;
	}

	errno = osif_psoc_sync_trans_start_wait(dev, &psoc_sync);
	if (errno)
		return;

	osif_psoc_sync_wait_for_ops(psoc_sync);

	__hdd_soc_recovery_shutdown();

	/* SSR transition is concluded at the end of soc re-init */
}

/**
 * wlan_hdd_crash_shutdown() - wlan_hdd_crash_shutdown
 *
 * HDD crash shutdown function: This function is called by
 * platform driver's crash shutdown routine
 *
 * Return: void
 */
static void wlan_hdd_crash_shutdown(void)
{
	QDF_STATUS ret;
	WMA_HANDLE wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		hdd_err("wma_handle is null");
		return;
	}

	/*
	 * When kernel panic happen, if WiFi FW is still active
	 * it may cause NOC errors/memory corruption, to avoid
	 * this, inject a fw crash first.
	 * send crash_inject to FW directly, because we are now
	 * in an atomic context, and preempt has been disabled,
	 * MCThread won't be scheduled at the moment, at the same
	 * time, TargetFailure event wont't be received after inject
	 * crash due to the same reason.
	 */
	ret = wma_crash_inject(wma_handle, RECOVERY_SIM_ASSERT, 0);
	if (QDF_IS_STATUS_ERROR(ret)) {
		hdd_err("Failed to send crash inject:%d", ret);
		return;
	}

	hif_crash_shutdown(cds_get_context(QDF_MODULE_ID_HIF));
}

/**
 * wlan_hdd_notify_handler() - wlan_hdd_notify_handler
 *
 * This function is called by the platform driver to notify the
 * COEX
 *
 * @state: state
 *
 * Return: void
 */
static void wlan_hdd_notify_handler(int state)
{
	if (!QDF_IS_EPPING_ENABLED(cds_get_conparam())) {
		int ret;

		ret = hdd_wlan_notify_modem_power_state(state);
		if (ret < 0)
			hdd_err("Fail to send notify");
	}
}

static int hdd_to_pmo_interface_pause(enum wow_interface_pause hdd_pause,
				      enum pmo_wow_interface_pause *pmo_pause)
{
	switch (hdd_pause) {
	case WOW_INTERFACE_PAUSE_DEFAULT:
		*pmo_pause = PMO_WOW_INTERFACE_PAUSE_DEFAULT;
		break;
	case WOW_INTERFACE_PAUSE_ENABLE:
		*pmo_pause = PMO_WOW_INTERFACE_PAUSE_ENABLE;
		break;
	case WOW_INTERFACE_PAUSE_DISABLE:
		*pmo_pause = PMO_WOW_INTERFACE_PAUSE_DISABLE;
		break;
	default:
		hdd_err("Invalid interface pause: %d", hdd_pause);
		return -EINVAL;
	}

	return 0;
}

static int hdd_to_pmo_resume_trigger(enum wow_resume_trigger hdd_trigger,
				     enum pmo_wow_resume_trigger *pmo_trigger)
{
	switch (hdd_trigger) {
	case WOW_RESUME_TRIGGER_DEFAULT:
		*pmo_trigger = PMO_WOW_RESUME_TRIGGER_DEFAULT;
		break;
	case WOW_RESUME_TRIGGER_HTC_WAKEUP:
		*pmo_trigger = PMO_WOW_RESUME_TRIGGER_HTC_WAKEUP;
		break;
	case WOW_RESUME_TRIGGER_GPIO:
		*pmo_trigger = PMO_WOW_RESUME_TRIGGER_GPIO;
		break;
	default:
		hdd_err("Invalid resume trigger: %d", hdd_trigger);
		return -EINVAL;
	}

	return 0;
}

static int
hdd_to_pmo_wow_enable_params(struct wow_enable_params *in_params,
			     struct pmo_wow_enable_params *out_params)
{
	int err;

	/* unit-test suspend */
	out_params->is_unit_test = in_params->is_unit_test;

	/* interface pause */
	err = hdd_to_pmo_interface_pause(in_params->interface_pause,
					 &out_params->interface_pause);
	if (err)
		return err;

	/* resume trigger */
	err = hdd_to_pmo_resume_trigger(in_params->resume_trigger,
					&out_params->resume_trigger);
	if (err)
		return err;

	return 0;
}

/**
 * __wlan_hdd_bus_suspend() - handles platform supsend
 * @wow_params: collection of wow enable override parameters
 *
 * Does precondtion validation. Ensures that a subsystem restart isn't in
 * progress. Ensures that no load or unload is in progress. Does:
 *	data path suspend
 *	component (pmo) suspend
 *	hif (bus) suspend
 *
 * Return: 0 for success, -EFAULT for null pointers,
 *     -EBUSY or -EAGAIN if another opperation is in progress and
 *     wlan will not be ready to suspend in time.
 */
static int __wlan_hdd_bus_suspend(struct wow_enable_params wow_params)
{
	int err;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	void *hif_ctx;
	void *dp_soc;
	struct pmo_wow_enable_params pmo_params;
	int pending;

	hdd_info("starting bus suspend");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	err = wlan_hdd_validate_context(hdd_ctx);
	if (err)
		return err;


	/* If Wifi is off, return success for system suspend */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Module closed; skipping suspend");
		return 0;
	}


	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get hif context");
		return -EINVAL;
	}

	err = hdd_to_pmo_wow_enable_params(&wow_params, &pmo_params);
	if (err) {
		hdd_err("Invalid WoW enable parameters: %d", err);
		return err;
	}

	dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	err = qdf_status_to_os_return(cdp_bus_suspend(dp_soc, OL_TXRX_PDEV_ID));
	if (err) {
		hdd_err("Failed cdp bus suspend: %d", err);
		return err;
	}

	if (ucfg_ipa_is_tx_pending(hdd_ctx->pdev)) {
		hdd_err("failed due to pending IPA TX comps");
		err = -EBUSY;
		goto resume_cdp;
	}

	err = hif_bus_early_suspend(hif_ctx);
	if (err) {
		hdd_err("Failed hif bus early suspend");
		goto resume_cdp;
	}

	status = ucfg_pmo_psoc_bus_suspend_req(hdd_ctx->psoc,
					       QDF_SYSTEM_SUSPEND,
					       &pmo_params);
	err = qdf_status_to_os_return(status);
	if (err) {
		hdd_err("Failed pmo bus suspend: %d", status);
		goto late_hif_resume;
	}

	hif_system_pm_set_state_suspended(hif_ctx);

	err = hif_bus_suspend(hif_ctx);
	if (err) {
		hdd_err("Failed hif bus suspend: %d", err);
		goto resume_pmo;
	}

	status = ucfg_pmo_core_txrx_suspend(hdd_ctx->psoc);
	err = qdf_status_to_os_return(status);
	if (err) {
		hdd_err("Failed to suspend TXRX: %d", err);
		goto resume_hif;
	}

	pending = cdp_rx_get_pending(cds_get_context(QDF_MODULE_ID_SOC));
	if (pending) {
		hdd_debug("Prevent suspend, RX frame pending %d", pending);
		err = -EBUSY;
		goto resume_txrx;
	}

	if (hif_try_prevent_ep_vote_access(hif_ctx)) {
		hdd_debug("Prevent suspend, ep work pending");
		err = QDF_STATUS_E_BUSY;
		goto resume_txrx;
	}

	/*
	 * Remove bus votes at the very end, after making sure there are no
	 * pending bus transactions from WLAN SOC for TX/RX.
	 */
	pld_request_bus_bandwidth(hdd_ctx->parent_dev, PLD_BUS_WIDTH_NONE);

	hdd_info("bus suspend succeeded");
	return 0;

resume_txrx:
	status = ucfg_pmo_core_txrx_resume(hdd_ctx->psoc);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));

resume_hif:
	status = hif_bus_resume(hif_ctx);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));

resume_pmo:
	status = ucfg_pmo_psoc_bus_resume_req(hdd_ctx->psoc,
					      QDF_SYSTEM_SUSPEND);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));

late_hif_resume:
	status = hif_bus_late_resume(hif_ctx);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));

resume_cdp:
	status = cdp_bus_resume(dp_soc, OL_TXRX_PDEV_ID);
	QDF_BUG(QDF_IS_STATUS_SUCCESS(status));
	hif_system_pm_set_state_on(hif_ctx);

	return err;
}

int wlan_hdd_bus_suspend(void)
{
	struct wow_enable_params default_params = {0};

	return __wlan_hdd_bus_suspend(default_params);
}

#ifdef WLAN_SUSPEND_RESUME_TEST
int wlan_hdd_unit_test_bus_suspend(struct wow_enable_params wow_params)
{
	return __wlan_hdd_bus_suspend(wow_params);
}
#endif

/**
 * wlan_hdd_bus_suspend_noirq() - handle .suspend_noirq callback
 *
 * This function is called by the platform driver to complete the
 * bus suspend callback when device interrupts are disabled by kernel.
 * Call HIF and WMA suspend_noirq callbacks to make sure there is no
 * wake up pending from FW before allowing suspend.
 *
 * Return: 0 for success and -EBUSY if FW is requesting wake up
 */
int wlan_hdd_bus_suspend_noirq(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	void *hif_ctx;
	int errno;
	uint32_t pending_events;

	hdd_debug("start bus_suspend_noirq");

	if (!hdd_ctx) {
		hdd_err_rl("hdd context is NULL");
		return -ENODEV;
	}

	/* If Wifi is off, return success for system suspend */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver module closed; skip bus-noirq suspend");
		return 0;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("hif_ctx is null");
		return -EINVAL;
	}

	errno = hif_bus_suspend_noirq(hif_ctx);
	if (errno)
		goto done;

	errno = ucfg_pmo_psoc_is_target_wake_up_received(hdd_ctx->psoc);
	if (errno == -EAGAIN) {
		hdd_err("Firmware attempting wakeup, try again");
		wlan_hdd_inc_suspend_stats(hdd_ctx,
					   SUSPEND_FAIL_INITIAL_WAKEUP);
	}
	if (errno)
		goto resume_hif_noirq;

	pending_events = wma_critical_events_in_flight();
	if (pending_events) {
		hdd_err("%d critical event(s) in flight; try again",
			pending_events);
		errno = -EAGAIN;
		goto resume_hif_noirq;
	}

	hdd_ctx->suspend_resume_stats.suspends++;

	hdd_debug("bus_suspend_noirq done");
	return 0;

resume_hif_noirq:
	QDF_BUG(!hif_bus_resume_noirq(hif_ctx));

done:
	hdd_err("suspend_noirq failed, status: %d", errno);

	return errno;
}

/**
 * wlan_hdd_bus_resume() - handles platform resume
 *
 * Does precondtion validation. Ensures that a subsystem restart isn't in
 * progress.  Ensures that no load or unload is in progress.  Ensures that
 * it has valid pointers for the required contexts.
 * Calls into hif to resume the bus opperation.
 * Calls into wma to handshake with firmware and notify it that the bus is up.
 * Calls into ol_txrx for symetry.
 * Failures are treated as catastrophic.
 *
 * return: error code or 0 for success
 */
int wlan_hdd_bus_resume(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	void *hif_ctx;
	int status;
	QDF_STATUS qdf_status;
	void *dp_soc;

	if (cds_is_driver_recovering())
		return 0;

	hdd_info("starting bus resume");

	if (!hdd_ctx) {
		hdd_err_rl("hdd context is NULL");
		return -ENODEV;
	}

	/* If Wifi is off, return success for system resume */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Module closed; return success");
		return 0;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		hdd_err("Failed to get hif context");
		return -EINVAL;
	}

	/*
	 * Add bus votes at the beginning, before making sure there are any
	 * bus transactions from WLAN SOC for TX/RX.
	 */
	if (hdd_is_any_adapter_connected(hdd_ctx)) {
		pld_request_bus_bandwidth(hdd_ctx->parent_dev,
					  PLD_BUS_WIDTH_MEDIUM);
	} else {
		pld_request_bus_bandwidth(hdd_ctx->parent_dev,
					  PLD_BUS_WIDTH_NONE);
	}

	status = hif_bus_resume(hif_ctx);
	if (status) {
		hdd_err("Failed hif bus resume");
		goto out;
	}

	hif_system_pm_set_state_resuming(hif_ctx);

	qdf_status = ucfg_pmo_psoc_bus_resume_req(hdd_ctx->psoc,
						  QDF_SYSTEM_SUSPEND);
	status = qdf_status_to_os_return(qdf_status);
	if (status) {
		hdd_err("Failed pmo bus resume");
		goto out;
	}

	qdf_status = ucfg_pmo_core_txrx_resume(hdd_ctx->psoc);
	status = qdf_status_to_os_return(qdf_status);
	if (status) {
		hdd_err("Failed to resume TXRX");
		goto out;
	}

	hif_system_pm_set_state_on(hif_ctx);

	status = hif_bus_late_resume(hif_ctx);
	if (status) {
		hdd_err("Failed hif bus late resume");
		goto out;
	}

	dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	qdf_status = cdp_bus_resume(dp_soc, OL_TXRX_PDEV_ID);
	status = qdf_status_to_os_return(qdf_status);
	if (status) {
		hdd_err("Failed cdp bus resume");
		goto out;
	}

	hdd_info("bus resume succeeded");
	return 0;

out:
	hif_system_pm_set_state_suspended(hif_ctx);
	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state() ||
		cds_is_fw_down())
		return 0;

	QDF_BUG(false);

	return status;
}

/**
 * wlan_hdd_bus_resume_noirq(): handle bus resume no irq
 *
 * This function is called by the platform driver to do bus
 * resume no IRQ before calling resume callback. Call WMA and HIF
 * layers to complete the resume_noirq.
 *
 * Return: 0 for success and negative error code for failure
 */
int wlan_hdd_bus_resume_noirq(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	void *hif_ctx;
	int status;
	QDF_STATUS qdf_status;

	hdd_debug("starting bus_resume_noirq");
	if (cds_is_driver_recovering())
		return 0;

	if (!hdd_ctx) {
		hdd_err_rl("hdd context is NULL");
		return -ENODEV;
	}

	/* If Wifi is off, return success for system resume */
	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver Module closed return success");
		return 0;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx)
		return -EINVAL;

	qdf_status = ucfg_pmo_psoc_clear_target_wake_up(hdd_ctx->psoc);
	QDF_BUG(!qdf_status);

	status = hif_bus_resume_noirq(hif_ctx);
	QDF_BUG(!status);

	hdd_debug("bus_resume_noirq done");

	return status;
}

/**
 * wlan_hdd_bus_reset_resume() - resume wlan bus after reset
 *
 * This function is called to tell the driver that the device has been resumed
 * and it has also been reset. The driver should redo any necessary
 * initialization. It is mainly used by the USB bus
 *
 * Return: int 0 for success, non zero for failure
 */
static int wlan_hdd_bus_reset_resume(void)
{
	struct hif_opaque_softc *scn = cds_get_context(QDF_MODULE_ID_HIF);

	if (!scn) {
		hdd_err("Failed to get HIF context");
		return -EFAULT;
	}

	return hif_bus_reset_resume(scn);
}

#ifdef FEATURE_RUNTIME_PM
/**
 * hdd_pld_runtime_suspend_cb() - Runtime suspend callback from PMO
 *
 * Return: 0 on success or error value otherwise
 */
static int hdd_pld_runtime_suspend_cb(void)
{
	qdf_device_t qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_dev) {
		hdd_err("Invalid context");
		return -EINVAL;
	}

	return pld_auto_suspend(qdf_dev->dev);
}

/**
 * wlan_hdd_runtime_suspend() - suspend the wlan bus without apps suspend
 *
 * Each layer is responsible for its own suspend actions.  wma_runtime_suspend
 * takes care of the parts of the 802.11 suspend that we want to do for runtime
 * suspend.
 *
 * Return: 0 or errno
 */
static int wlan_hdd_runtime_suspend(struct device *dev)
{
	int err;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx;
	qdf_time_t delta;

	hdd_debug("Starting runtime suspend");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	err = wlan_hdd_validate_context(hdd_ctx);
	if (err)
		return err;

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver module closed skipping runtime suspend");
		return 0;
	}

	if (!hdd_is_runtime_pm_enabled(hdd_ctx))
		return 0;

	if (ucfg_scan_get_pdev_status(hdd_ctx->pdev) !=
	    SCAN_NOT_IN_PROGRESS) {
		hdd_debug("Scan in progress, ignore runtime suspend");
		return -EBUSY;
	}

	if (ucfg_ipa_is_tx_pending(hdd_ctx->pdev)) {
		hdd_debug("IPA TX comps pending, ignore rtpm suspend");
		return -EBUSY;
	}

	status = ucfg_pmo_psoc_bus_runtime_suspend(hdd_ctx->psoc,
						   hdd_pld_runtime_suspend_cb);
	err = qdf_status_to_os_return(status);

	hdd_ctx->runtime_suspend_done_time_stamp =
						qdf_get_log_timestamp_usecs();
	delta = hdd_ctx->runtime_suspend_done_time_stamp -
		hdd_ctx->runtime_resume_start_time_stamp;

	if (hdd_ctx->runtime_suspend_done_time_stamp >
	   hdd_ctx->runtime_resume_start_time_stamp)
		hdd_debug("Runtime suspend done result: %d total cxpc up time %lu microseconds",
			  err, delta);

	if (status == QDF_STATUS_SUCCESS)
		hdd_bus_bw_compute_timer_stop(hdd_ctx);

	hdd_debug("Runtime suspend done result: %d", err);

	return err;
}

/**
 * hdd_pld_runtime_resume_cb() - Runtime resume callback from PMO
 *
 * Return: 0 on success or error value otherwise
 */
static int hdd_pld_runtime_resume_cb(void)
{
	qdf_device_t qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_dev) {
		hdd_err("Invalid context");
		return -EINVAL;
	}

	return pld_auto_resume(qdf_dev->dev);
}

/**
 * wlan_hdd_runtime_resume() - resume the wlan bus from runtime suspend
 *
 * Sets the runtime pm state and coordinates resume between hif wma and
 * ol_txrx.
 *
 * Return: success since failure is a bug
 */
static int wlan_hdd_runtime_resume(struct device *dev)
{
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;
	qdf_time_t delta;

	hdd_debug("Starting runtime resume");

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	/*
	 * In__hdd_soc_remove, runtime_sync_resume is called before setting
	 * unload_in_progress flag. wlan_hdd_validate_context will cause
	 * resume fail, if driver load/unload in-progress, so not doing
	 * wlan_hdd_validate_context, have only SSR in progress check.
	 */
	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return 0;
	}
	if (cds_is_driver_recovering()) {
		hdd_debug("Recovery in progress, state:0x%x",
			  cds_get_driver_state());
		return 0;
	}

	if (hdd_ctx->driver_status != DRIVER_MODULES_ENABLED) {
		hdd_debug("Driver module closed skipping runtime resume");
		return 0;
	}

	if (!hdd_is_runtime_pm_enabled(hdd_ctx))
		return 0;

	hdd_ctx->runtime_resume_start_time_stamp =
						qdf_get_log_timestamp_usecs();
	delta = hdd_ctx->runtime_resume_start_time_stamp -
		hdd_ctx->runtime_suspend_done_time_stamp;
	hdd_debug("Starting runtime resume total cxpc down time %lu microseconds",
		  delta);

	status = ucfg_pmo_psoc_bus_runtime_resume(hdd_ctx->psoc,
						  hdd_pld_runtime_resume_cb);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("PMO Runtime resume failed: %d", status);
	} else {
		if (policy_mgr_get_connection_count(hdd_ctx->psoc))
			hdd_bus_bw_compute_timer_try_start(hdd_ctx);
	}

	hdd_debug("Runtime resume done");

	return 0;
}
#endif

/**
 * wlan_hdd_pld_probe() - probe function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 * @bdev: bus device structure
 * @id: bus identifier for shared busses
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_probe(struct device *dev,
			      enum pld_bus_type pld_bus_type,
			      void *bdev,
			      void *id)
{
	enum qdf_bus_type bus_type = to_bus_type(pld_bus_type);

	if (bus_type == QDF_BUS_TYPE_NONE) {
		hdd_err("Invalid bus type %d->%d", pld_bus_type, bus_type);
		return -EINVAL;
	}

	return hdd_soc_probe(dev, bdev, id, bus_type);
}

/**
 * wlan_hdd_pld_remove() - remove function registered to PLD
 * @dev: device to remove
 * @pld_bus_type: PLD bus type
 *
 * Return: void
 */
static void wlan_hdd_pld_remove(struct device *dev, enum pld_bus_type bus_type)
{
	hdd_enter();

	hdd_soc_remove(dev);

	hdd_exit();
}

static void hdd_soc_idle_shutdown_lock(struct device *dev)
{
	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_SHUTDOWN);

	hdd_abort_system_suspend(dev);
}

static void hdd_soc_idle_shutdown_unlock(void)
{
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_DRIVER_IDLE_SHUTDOWN);
}

/**
 * wlan_hdd_pld_idle_shutdown() - wifi module idle shutdown after interface
 *                                inactivity timeout has trigerred idle shutdown
 * @dev: device to remove
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 for success and negative error code for failure
 */
static int wlan_hdd_pld_idle_shutdown(struct device *dev,
				       enum pld_bus_type bus_type)
{
	int ret;

	hdd_soc_idle_shutdown_lock(dev);

	ret = hdd_psoc_idle_shutdown(dev);

	hdd_soc_idle_shutdown_unlock();

	return ret;
}

/**
 * wlan_hdd_pld_idle_restart() - wifi module idle restart after idle shutdown
 * @dev: device to remove
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 for success and negative error code for failure
 */
static int wlan_hdd_pld_idle_restart(struct device *dev,
				      enum pld_bus_type bus_type)
{
	return hdd_psoc_idle_restart(dev);
}

/**
 * wlan_hdd_pld_shutdown() - shutdown function registered to PLD
 * @dev: device to shutdown
 * @pld_bus_type: PLD bus type
 *
 * Return: void
 */
static void wlan_hdd_pld_shutdown(struct device *dev,
				  enum pld_bus_type bus_type)
{
	hdd_enter();

	hdd_soc_recovery_shutdown(dev);

	hdd_exit();
}

/**
 * wlan_hdd_pld_reinit() - reinit function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 * @bdev: bus device structure
 * @id: bus identifier for shared busses
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_reinit(struct device *dev,
			       enum pld_bus_type pld_bus_type,
			       void *bdev,
			       void *id)
{
	enum qdf_bus_type bus_type = to_bus_type(pld_bus_type);

	if (bus_type == QDF_BUS_TYPE_NONE) {
		hdd_err("Invalid bus type %d->%d", pld_bus_type, bus_type);
		return -EINVAL;
	}

	return hdd_soc_recovery_reinit(dev, bdev, id, bus_type);
}

/**
 * wlan_hdd_pld_crash_shutdown() - crash_shutdown function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Return: void
 */
static void wlan_hdd_pld_crash_shutdown(struct device *dev,
			     enum pld_bus_type bus_type)
{
	wlan_hdd_crash_shutdown();
}

/**
 * wlan_hdd_pld_suspend() - suspend function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 * @state: PM state
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_suspend(struct device *dev,
				enum pld_bus_type bus_type,
				pm_message_t state)

{
	struct osif_psoc_sync *psoc_sync;
	int errno;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;
	/*
	 * Flush the idle shutdown before ops start.This is done here to avoid
	 * the deadlock as idle shutdown waits for the dsc ops
	 * to complete.
	 */
	hdd_psoc_idle_timer_stop(hdd_ctx);


	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_bus_suspend();

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * wlan_hdd_pld_resume() - resume function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_resume(struct device *dev,
		    enum pld_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_bus_resume();

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * wlan_hdd_pld_suspend_noirq() - handle suspend no irq
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Complete the actions started by suspend().  Carry out any
 * additional operations required for suspending the device that might be
 * racing with its driver's interrupt handler, which is guaranteed not to
 * run while suspend_noirq() is being executed. Make sure to resume device
 * if FW has sent initial wake up message and expecting APPS to wake up.
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_suspend_noirq(struct device *dev,
				      enum pld_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_bus_suspend_noirq();

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * wlan_hdd_pld_resume_noirq() - handle resume no irq
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Prepare for the execution of resume() by carrying out any
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed. Make sure to clear target initial
 * wake up request such that next suspend can happen cleanly.
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_resume_noirq(struct device *dev,
				     enum pld_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_bus_resume_noirq();

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * wlan_hdd_pld_reset_resume() - reset resume function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_reset_resume(struct device *dev,
				     enum pld_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_bus_reset_resume();

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

/**
 * wlan_hdd_pld_notify_handler() - notify_handler function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 * @state: Modem power state
 *
 * Return: void
 */
static void wlan_hdd_pld_notify_handler(struct device *dev,
			     enum pld_bus_type bus_type,
			     int state)
{
	wlan_hdd_notify_handler(state);
}

/**
 * wlan_hdd_pld_uevent() - platform uevent handler
 * @dev: device on which the uevent occurred
 * @event_data: uevent parameters
 *
 * Return: None
 */
static void
wlan_hdd_pld_uevent(struct device *dev, struct pld_uevent_data *event_data)
{
	struct qdf_notifer_data hang_evt_data;
	enum qdf_hang_reason reason = QDF_REASON_UNSPECIFIED;
	uint8_t bus_type;

	bus_type = pld_get_bus_type(dev);

	switch (event_data->uevent) {
	case PLD_SMMU_FAULT:
		qdf_set_smmu_fault_state(true);
		hdd_debug("Received smmu fault indication");
		break;
	case PLD_FW_DOWN:
		hdd_debug("Received firmware down indication");

		cds_set_target_ready(false);
		cds_set_recovery_in_progress(true);

		/* Notify external threads currently waiting on firmware
		 * by forcefully completing waiting events with a "reset"
		 * status. This will cause the event to fail early instead
		 * of timing out.
		 */
		qdf_complete_wait_events();

		/*
		 * In case of some platforms, uevent will come to the driver in
		 * process context. In that case, it is safe to complete the
		 * SSR cleanup activities in the same context. In case of
		 * other platforms, it will be invoked in interrupt context.
		 * Performing the cleanup in interrupt context is not ideal,
		 * thus defer the cleanup to be done during
		 * hdd_soc_recovery_shutdown
		 */
		if (qdf_in_interrupt() || bus_type == PLD_BUS_TYPE_PCIE)
			break;

		hdd_soc_recovery_cleanup();
		qdf_atomic_set(&is_recovery_cleanup_done, 1);

		break;
	case PLD_FW_HANG_EVENT:
		hdd_info("Received firmware hang event");
		cds_get_recovery_reason(&reason);
		qdf_mem_zero(&g_fw_host_hang_event, QDF_HANG_EVENT_DATA_SIZE);
		hang_evt_data.hang_data = g_fw_host_hang_event;
		hang_evt_data.offset = 0;
		qdf_hang_event_notifier_call(reason, &hang_evt_data);
		hang_evt_data.offset = QDF_WLAN_HANG_FW_OFFSET;
		if (event_data->hang_data.hang_event_data_len >=
		    QDF_HANG_EVENT_DATA_SIZE / 2)
			event_data->hang_data.hang_event_data_len =
						QDF_HANG_EVENT_DATA_SIZE / 2;

		if (event_data->hang_data.hang_event_data_len)
			qdf_mem_copy((hang_evt_data.hang_data +
				      hang_evt_data.offset),
				     event_data->hang_data.hang_event_data,
				     event_data->hang_data.hang_event_data_len);

		hdd_send_hang_data(hang_evt_data.hang_data,
				   QDF_HANG_EVENT_DATA_SIZE);
		break;
	default:
		/* other events intentionally not handled */
		hdd_debug("Received uevent %d", event_data->uevent);
		break;
	}

}

#ifdef FEATURE_RUNTIME_PM
/**
 * wlan_hdd_pld_runtime_suspend() - runtime suspend function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_runtime_suspend(struct device *dev,
					enum pld_bus_type bus_type)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(dev, &psoc_sync);
	if (errno)
		goto out;

	errno = wlan_hdd_runtime_suspend(dev);

	osif_psoc_sync_op_stop(psoc_sync);

out:
	/* If it returns other errno to kernel, it will treat
	 * it as critical issue, so all the future runtime
	 * PM api will return error, pm runtime can't be work
	 * anymore. Such case found in SSR.
	 */
	if (errno && errno != -EAGAIN && errno != -EBUSY)
		errno = -EAGAIN;
	return errno;
}

/**
 * wlan_hdd_pld_runtime_resume() - runtime resume function registered to PLD
 * @dev: device
 * @pld_bus_type: PLD bus type
 *
 * Return: 0 on success
 */
static int wlan_hdd_pld_runtime_resume(struct device *dev,
				       enum pld_bus_type bus_type)
{
	/* As opposite to suspend, Runtime PM resume can happen
	 * synchronously during driver shutdown or idle shutown,
	 * so remove PSOC sync protection here.
	 */
	return wlan_hdd_runtime_resume(dev);
}
#endif

struct pld_driver_ops wlan_drv_ops = {
	.probe      = wlan_hdd_pld_probe,
	.remove     = wlan_hdd_pld_remove,
	.idle_shutdown = wlan_hdd_pld_idle_shutdown,
	.idle_restart = wlan_hdd_pld_idle_restart,
	.shutdown   = wlan_hdd_pld_shutdown,
	.reinit     = wlan_hdd_pld_reinit,
	.crash_shutdown = wlan_hdd_pld_crash_shutdown,
	.suspend    = wlan_hdd_pld_suspend,
	.resume     = wlan_hdd_pld_resume,
	.suspend_noirq = wlan_hdd_pld_suspend_noirq,
	.resume_noirq  = wlan_hdd_pld_resume_noirq,
	.reset_resume = wlan_hdd_pld_reset_resume,
	.modem_status = wlan_hdd_pld_notify_handler,
	.uevent = wlan_hdd_pld_uevent,
#ifdef FEATURE_RUNTIME_PM
	.runtime_suspend = wlan_hdd_pld_runtime_suspend,
	.runtime_resume = wlan_hdd_pld_runtime_resume,
#endif
	.set_curr_therm_cdev_state = wlan_hdd_pld_set_thermal_mitigation,
};

int wlan_hdd_register_driver(void)
{
	return pld_register_driver(&wlan_drv_ops);
}

void wlan_hdd_unregister_driver(void)
{
	pld_unregister_driver();
}
