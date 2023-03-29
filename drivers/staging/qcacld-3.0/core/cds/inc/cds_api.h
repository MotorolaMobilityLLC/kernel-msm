/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__CDS_API_H)
#define __CDS_API_H

/**
 * DOC:  cds_api.h
 *
 * Connectivity driver services public API
 *
 */

#include <qdf_types.h>
#include <qdf_status.h>
#include <qdf_mem.h>
#include <qdf_debugfs.h>
#include <qdf_list.h>
#include <qdf_trace.h>
#include <qdf_event.h>
#include <qdf_lock.h>
#include "qdf_platform.h"
#include "qdf_cpuhp.h"
#include <wlan_cmn.h>
#include "reg_services_public_struct.h"
#include <cds_reg_service.h>
#include <cds_packet.h>
#include <cds_sched.h>
#include <qdf_threads.h>
#include <qdf_mc_timer.h>
#include <wlan_objmgr_psoc_obj.h>
#include <cdp_txrx_handle.h>

/* Amount of time to wait for WMA to perform an asynchronous activity.
 * This value should be larger than the timeout used by WMI to wait for
 * a response from target
 */
#define CDS_WMA_TIMEOUT  (15000)

/**
 * enum cds_driver_state - Driver state
 * @CDS_DRIVER_STATE_UNINITIALIZED: Driver is in uninitialized state.
 * CDS_DRIVER_STATE_LOADED: Driver is loaded and functional.
 * CDS_DRIVER_STATE_LOADING: Driver probe is in progress.
 * CDS_DRIVER_STATE_UNLOADING: Driver remove is in progress.
 * CDS_DRIVER_STATE_RECOVERING: Recovery in progress.
 * CDS_DRIVER_STATE_BAD: Driver in bad state.
 * CDS_DRIVER_STATE_MODULE_STOP: Module stop in progress or done.
 * CDS_DRIVER_STATE_ASSERTING_TARGET: Driver assert target in progress.
 */
enum cds_driver_state {
	CDS_DRIVER_STATE_UNINITIALIZED          = 0,
	CDS_DRIVER_STATE_LOADED                 = BIT(0),
	CDS_DRIVER_STATE_LOADING                = BIT(1),
	CDS_DRIVER_STATE_UNLOADING              = BIT(2),
	CDS_DRIVER_STATE_RECOVERING             = BIT(3),
	CDS_DRIVER_STATE_BAD                    = BIT(4),
	CDS_DRIVER_STATE_FW_READY               = BIT(5),
	CDS_DRIVER_STATE_MODULE_STOP            = BIT(6),
	CDS_DRIVER_STATE_ASSERTING_TARGET       = BIT(7),
};

/**
 * struce cds_vdev_dp_stats - vdev stats populated from DP
 * @tx_retries: packet number of successfully transmitted after more
 *              than one retransmission attempt
 * @tx_mpdu_success_with_retries: Number of MPDU transmission retries done
 *				  in case of successful transmission.
 */
struct cds_vdev_dp_stats {
	uint32_t tx_retries;
	uint32_t tx_mpdu_success_with_retries;
};

#define __CDS_IS_DRIVER_STATE(_state, _mask) (((_state) & (_mask)) == (_mask))

void cds_set_driver_state(enum cds_driver_state);
void cds_clear_driver_state(enum cds_driver_state);
enum cds_driver_state cds_get_driver_state(void);

/**
 * cds_is_driver_loading() - Is driver load in progress
 *
 * Return: true if driver is loading and false otherwise.
 */
static inline bool cds_is_driver_loading(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADING);
}

/**
 * cds_is_driver_unloading() - Is driver unload in progress
 *
 * Return: true if driver is unloading and false otherwise.
 */
static inline bool cds_is_driver_unloading(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_is_driver_recovering() - Is recovery in progress
 *
 * Return: true if recovery in progress  and false otherwise.
 */
static inline bool cds_is_driver_recovering(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_RECOVERING);
}

/**
 * cds_is_driver_in_bad_state() - is driver in bad state
 *
 * Return: true if driver is in bad state and false otherwise.
 */
static inline bool cds_is_driver_in_bad_state(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_BAD);
}

/**
 * cds_is_load_or_unload_in_progress() - Is driver load OR unload in progress
 *
 * Return: true if driver is loading OR unloading and false otherwise.
 */
static inline bool cds_is_load_or_unload_in_progress(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADING) ||
		__CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_is_target_ready() - Is target is in ready state
 *
 * Return: true if target is in ready state and false otherwise.
 */
static inline bool cds_is_target_ready(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_FW_READY);
}

/**
 * cds_is_driver_state_module_stop - Is module stop is in-progress or done
 *
 * Return: true if driver state is module stop and false otherwise.
 */
static inline bool cds_is_driver_state_module_stop(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_MODULE_STOP);
}

/**
 * cds_set_recovery_in_progress() - Set recovery in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_recovery_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_RECOVERING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_RECOVERING);
}

/**
 * cds_set_driver_in_bad_state() - Set driver state
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_driver_in_bad_state(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_BAD);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_BAD);
}

/**
 * cds_set_target_ready() - Set target ready state
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_target_ready(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_FW_READY);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_FW_READY);
}

/**
 * cds_set_load_in_progress() - Set load in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_load_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_LOADING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_LOADING);
}

/**
 * cds_set_driver_loaded() - Set load completed
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_driver_loaded(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_LOADED);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_LOADED);
}

/**
 * cds_set_unload_in_progress() - Set unload in progress
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_unload_in_progress(uint8_t value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_UNLOADING);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_UNLOADING);
}

/**
 * cds_set_driver_state_module_stop() - Setting module stop in progress or done
 *
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_driver_state_module_stop(bool value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_MODULE_STOP);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_MODULE_STOP);
}

/**
 * cds_is_driver_loaded() - Is driver loaded
 *
 * Return: true if driver is loaded or false otherwise.
 */
static inline bool cds_is_driver_loaded(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_LOADED);
}

/**
 * cds_set_assert_target_in_progress() - Setting assert target in progress
 *
 * @value: value to set
 *
 * Return: none
 */
static inline void cds_set_assert_target_in_progress(bool value)
{
	if (value)
		cds_set_driver_state(CDS_DRIVER_STATE_ASSERTING_TARGET);
	else
		cds_clear_driver_state(CDS_DRIVER_STATE_ASSERTING_TARGET);
}

/**
 * cds_is_target_asserting() - Is driver asserting target
 *
 * Return: true if driver is asserting target
 */
static inline bool cds_is_target_asserting(void)
{
	enum cds_driver_state state = cds_get_driver_state();

	return __CDS_IS_DRIVER_STATE(state, CDS_DRIVER_STATE_ASSERTING_TARGET);
}

/**
 * cds_init() - Initialize CDS
 *
 * This function allocates the resource required for CDS, but does not
 * initialize all the members. This overall initialization will happen at
 * cds_open().
 *
 * Return: QDF_STATUS_SUCCESS if CDS was initialized and an error on failure
 */
QDF_STATUS cds_init(void);

void cds_deinit(void);

QDF_STATUS cds_pre_enable(void);

QDF_STATUS cds_open(struct wlan_objmgr_psoc *psoc);

/**
 * cds_dp_open() - Open datapath module
 * @psoc - object manager soc handle
 *
 * API to map the datapath rings to interrupts
 * and also open the datapath pdev module.
 *
 * Return: QDF status
 */
QDF_STATUS cds_dp_open(struct wlan_objmgr_psoc *psoc);

/**
 * cds_enable() - start/enable cds module
 * @psoc: Psoc pointer
 *
 * Return: QDF status
 */
QDF_STATUS cds_enable(struct wlan_objmgr_psoc *psoc);

QDF_STATUS cds_disable(struct wlan_objmgr_psoc *psoc);

QDF_STATUS cds_post_disable(void);

QDF_STATUS cds_close(struct wlan_objmgr_psoc *psoc);

/**
 * cds_dp_close() - Close datapath module
 * @psoc: Object manager soc handle
 *
 * API used to detach interrupts assigned to service
 * datapath rings and close pdev module
 *
 * Return: Status
 */
QDF_STATUS cds_dp_close(struct wlan_objmgr_psoc *psoc);

void *cds_get_context(QDF_MODULE_ID module_id);

void *cds_get_global_context(void);

QDF_STATUS cds_alloc_context(QDF_MODULE_ID module_id, void **module_context,
			     uint32_t size);

QDF_STATUS cds_free_context(QDF_MODULE_ID module_id, void *module_context);

QDF_STATUS cds_set_context(QDF_MODULE_ID module_id, void *context);

void cds_flush_work(void *work);
void cds_flush_delayed_work(void *dwork);

#ifdef REMOVE_PKT_LOG
static inline
bool cds_is_packet_log_enabled(void)
{
	return false;
}
#else
bool cds_is_packet_log_enabled(void);
#endif

/**
 * cds_get_recovery_reason() - get self recovery reason
 * @reason: cds hang reason
 *
 * Return: None
 */
void cds_get_recovery_reason(enum qdf_hang_reason *reason);

/**
 * cds_reset_recovery_reason() - reset the reason to unspecified
 *
 * Return: None
 */
void cds_reset_recovery_reason(void);

/**
 * cds_trigger_recovery() - trigger self recovery
 * @reason: recovery reason
 *
 * Return: none
 */
#define cds_trigger_recovery(reason) \
	__cds_trigger_recovery(reason, __func__, __LINE__)

void cds_trigger_recovery_psoc(void *psoc, enum qdf_hang_reason reason,
			       const char *func, const uint32_t line);

void __cds_trigger_recovery(enum qdf_hang_reason reason, const char *func,
			    const uint32_t line);

void cds_set_wakelock_logging(bool value);
bool cds_is_wakelock_enabled(void);
void cds_set_ring_log_level(uint32_t ring_id, uint32_t log_level);
enum wifi_driver_log_level cds_get_ring_log_level(uint32_t ring_id);
void cds_set_multicast_logging(uint8_t value);
uint8_t cds_is_multicast_logging(void);
QDF_STATUS cds_set_log_completion(uint32_t is_fatal,
		uint32_t type,
		uint32_t sub_type,
		bool recovery_needed);
void cds_get_and_reset_log_completion(uint32_t *is_fatal,
		uint32_t *type,
		uint32_t *sub_type,
		bool *recovery_needed);
bool cds_is_log_report_in_progress(void);
bool cds_is_fatal_event_enabled(void);

#ifdef WLAN_FEATURE_TSF_PLUS
bool cds_is_ptp_rx_opt_enabled(void);
bool cds_is_ptp_tx_opt_enabled(void);
#else
static inline bool cds_is_ptp_rx_opt_enabled(void)
{
	return false;
}

static inline bool cds_is_ptp_tx_opt_enabled(void)
{
	return false;
}
#endif

uint32_t cds_get_log_indicator(void);
void cds_set_fatal_event(bool value);
void cds_wlan_flush_host_logs_for_fatal(void);

void cds_init_log_completion(void);
QDF_STATUS cds_flush_logs(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code,
		bool dump_mac_trace,
		bool recovery_needed);
void cds_logging_set_fw_flush_complete(void);
void cds_svc_fw_shutdown_ind(struct device *dev);
#ifdef FEATURE_WLAN_DIAG_SUPPORT
void cds_tdls_tx_rx_mgmt_event(uint8_t event_id, uint8_t tx_rx,
			uint8_t type, uint8_t sub_type, uint8_t *peer_mac);
#else
static inline
void cds_tdls_tx_rx_mgmt_event(uint8_t event_id, uint8_t tx_rx,
			uint8_t type, uint8_t sub_type, uint8_t *peer_mac)

{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

int cds_get_radio_index(void);
QDF_STATUS cds_set_radio_index(int radio_index);
void cds_init_ini_config(struct cds_config_info *cds_cfg);
void cds_deinit_ini_config(void);
struct cds_config_info *cds_get_ini_config(void);

bool cds_is_5_mhz_enabled(void);
bool cds_is_10_mhz_enabled(void);
bool cds_is_sub_20_mhz_enabled(void);
bool cds_is_self_recovery_enabled(void);
bool cds_is_fw_down(void);
enum QDF_GLOBAL_MODE cds_get_conparam(void);

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
void cds_pkt_stats_to_logger_thread(void *pl_hdr, void *pkt_dump, void *data);
#else
static inline
void cds_pkt_stats_to_logger_thread(void *pl_hdr, void *pkt_dump, void *data)
{
}
#endif

#ifdef FEATURE_HTC_CREDIT_HISTORY
/**
 * cds_print_htc_credit_history() - Helper function to copy HTC credit
 *				    history via htc_print_credit_history()
 *
 * @count:	Number of lines to be copied
 * @print:	Print callback to print in the buffer
 *
 * Return:	none
 */
void cds_print_htc_credit_history(uint32_t count,
				qdf_abstract_print * print,
				void *print_priv);
#else

static inline
void cds_print_htc_credit_history(uint32_t count,
				qdf_abstract_print *print,
				void *print_priv)
{
}
#endif
/**
 * cds_is_group_addr() - checks whether addr is multi cast
 * @mac_addr: address to be checked for multicast
 *
 * Check if the input mac addr is multicast addr
 *
 * Return: true if multicast addr else false
 */
static inline
bool cds_is_group_addr(uint8_t *mac_addr)
{
	if (mac_addr[0] & 0x01)
		return true;
	else
		return false;
}

/**
 * cds_get_connectivity_stats_pkt_bitmap() - get pkt-type bitmap
 * @context: osif dev context
 *
 * Return: pkt bitmap to track
 */
uint32_t cds_get_connectivity_stats_pkt_bitmap(void *context);

#ifdef FEATURE_ALIGN_STATS_FROM_DP
/**
 * cds_dp_get_vdev_stats() - get vdev stats from DP
 * @vdev_id: vdev id
 * @stats: structure of counters which CP is interested in
 *
 * Return: if get vdev stats from DP success, return true otherwise false
 */
bool cds_dp_get_vdev_stats(uint8_t vdev_id, struct cds_vdev_dp_stats *stats);
#else
static inline bool
cds_dp_get_vdev_stats(uint8_t vdev_id, struct cds_vdev_dp_stats *stats)
{
	return false;
}
#endif

/**
 * cds_smmu_mem_map_setup() - Check SMMU S1 stage enable
 *                            status and setup wlan driver
 * @osdev: Parent device instance
 * @ipa_present: IPA HW support flag
 *
 * This API checks if SMMU S1 translation is enabled in
 * platform driver or not and sets it accordingly in driver.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_smmu_mem_map_setup(qdf_device_t osdev, bool ipa_present);

/**
 * cds_smmu_map_unmap() - Map / Unmap DMA buffer to IPA UC
 * @map: Map / unmap operation
 * @num_buf: Number of buffers in array
 * @buf_arr: Buffer array of DMA mem mapping info
 *
 * This API maps/unmaps WLAN-IPA buffers if SMMU S1 translation
 * is enabled.
 *
 * Return: Status of map operation
 */
int cds_smmu_map_unmap(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr);
#endif /* if !defined __CDS_API_H */
