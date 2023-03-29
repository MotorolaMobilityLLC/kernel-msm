/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#ifndef __WEXT_IW_H__
#define __WEXT_IW_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/timer.h>
#include "qdf_event.h"

struct hdd_context;
struct sap_config;

/*
 * order of parameters in addTs private ioctl
 */
#define HDD_WLAN_WMM_PARAM_HANDLE                       0
#define HDD_WLAN_WMM_PARAM_TID                          1
#define HDD_WLAN_WMM_PARAM_DIRECTION                    2
#define HDD_WLAN_WMM_PARAM_APSD                         3
#define HDD_WLAN_WMM_PARAM_USER_PRIORITY                4
#define HDD_WLAN_WMM_PARAM_NOMINAL_MSDU_SIZE            5
#define HDD_WLAN_WMM_PARAM_MAXIMUM_MSDU_SIZE            6
#define HDD_WLAN_WMM_PARAM_MINIMUM_DATA_RATE            7
#define HDD_WLAN_WMM_PARAM_MEAN_DATA_RATE               8
#define HDD_WLAN_WMM_PARAM_PEAK_DATA_RATE               9
#define HDD_WLAN_WMM_PARAM_MAX_BURST_SIZE              10
#define HDD_WLAN_WMM_PARAM_MINIMUM_PHY_RATE            11
#define HDD_WLAN_WMM_PARAM_SURPLUS_BANDWIDTH_ALLOWANCE 12
#define HDD_WLAN_WMM_PARAM_SERVICE_INTERVAL            13
#define HDD_WLAN_WMM_PARAM_SUSPENSION_INTERVAL         14
#define HDD_WLAN_WMM_PARAM_BURST_SIZE_DEFN             15
#define HDD_WLAN_WMM_PARAM_ACK_POLICY                  16
#define HDD_WLAN_WMM_PARAM_INACTIVITY_INTERVAL         17
#define HDD_WLAN_WMM_PARAM_MAX_SERVICE_INTERVAL        18
#define HDD_WLAN_WMM_PARAM_COUNT                       19

#define MHZ 6

#define WE_MAX_STR_LEN                                 IW_PRIV_SIZE_MASK
#define WLAN_HDD_UI_BAND_AUTO                          0
#define WLAN_HDD_UI_BAND_5_GHZ                         1
#define WLAN_HDD_UI_BAND_2_4_GHZ                       2

enum hdd_wlan_wmm_direction {
	HDD_WLAN_WMM_DIRECTION_UPSTREAM = 0,
	HDD_WLAN_WMM_DIRECTION_DOWNSTREAM = 1,
	HDD_WLAN_WMM_DIRECTION_BIDIRECTIONAL = 2,
};

enum hdd_wlan_wmm_power_save {
	HDD_WLAN_WMM_POWER_SAVE_LEGACY = 0,
	HDD_WLAN_WMM_POWER_SAVE_UAPSD = 1,
};

typedef enum {
	/* TSPEC/re-assoc done, async */
	HDD_WLAN_WMM_STATUS_SETUP_SUCCESS = 0,
	/* no need to setup TSPEC since ACM=0 and no UAPSD desired,
	 * sync + async
	 */
	HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_NO_UAPSD = 1,
	/* no need to setup TSPEC since ACM=0 and UAPSD already exists,
	 * sync + async
	 */
	HDD_WLAN_WMM_STATUS_SETUP_SUCCESS_NO_ACM_UAPSD_EXISTING = 2,
	/* TSPEC result pending, sync */
	HDD_WLAN_WMM_STATUS_SETUP_PENDING = 3,
	/* TSPEC/re-assoc failed, sync + async */
	HDD_WLAN_WMM_STATUS_SETUP_FAILED = 4,
	/* Request rejected due to invalid params, sync + async */
	HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM = 5,
	/* TSPEC request rejected since AP!=QAP, sync */
	HDD_WLAN_WMM_STATUS_SETUP_FAILED_NO_WMM = 6,

	/* TSPEC modification/re-assoc successful, async */
	HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS = 7,
	/* TSPEC modification a no-op since ACM=0 and
	 * no change in UAPSD, sync + async
	 */
	HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_NO_UAPSD = 8,
	/* TSPEC modification a no-op since ACM=0 and
	 * requested U-APSD already exists, sync + async
	 */
	HDD_WLAN_WMM_STATUS_MODIFY_SUCCESS_NO_ACM_UAPSD_EXISTING = 9,
	/* TSPEC result pending, sync */
	HDD_WLAN_WMM_STATUS_MODIFY_PENDING = 10,
	/* TSPEC modification failed, prev TSPEC in effect, sync + async */
	HDD_WLAN_WMM_STATUS_MODIFY_FAILED = 11,
	/* TSPEC modification request rejected due to invalid params,
	 * sync + async
	 */
	HDD_WLAN_WMM_STATUS_MODIFY_FAILED_BAD_PARAM = 12,

	/* TSPEC release successful, sync and also async */
	HDD_WLAN_WMM_STATUS_RELEASE_SUCCESS = 13,
	/* TSPEC release pending, sync */
	HDD_WLAN_WMM_STATUS_RELEASE_PENDING = 14,
	/* TSPEC release failed, sync + async */
	HDD_WLAN_WMM_STATUS_RELEASE_FAILED = 15,
	/* TSPEC release rejected due to invalid params, sync */
	HDD_WLAN_WMM_STATUS_RELEASE_FAILED_BAD_PARAM = 16,
	/* TSPEC modified due to the mux'ing of requests on ACs, async */

	HDD_WLAN_WMM_STATUS_MODIFIED = 17,
	/* TSPEC revoked by AP, async */
	HDD_WLAN_WMM_STATUS_LOST = 18,
	/* some internal failure like memory allocation failure, etc, sync */
	HDD_WLAN_WMM_STATUS_INTERNAL_FAILURE = 19,

	/* U-APSD failed during setup but OTA setup (whether TSPEC exchnage or
	 * re-assoc) was done so app should release this QoS, async
	 */
	HDD_WLAN_WMM_STATUS_SETUP_UAPSD_SET_FAILED = 20,
	/* U-APSD failed during modify, but OTA setup (whether TSPEC exchnage or
	 * re-assoc) was done so app should release this QoS, async
	 */
	HDD_WLAN_WMM_STATUS_MODIFY_UAPSD_SET_FAILED = 21
} hdd_wlan_wmm_status_e;

/** Enable 11d */
#define ENABLE_11D  1

/** Disable 11d */
#define DISABLE_11D 0

#define HDD_RTSCTS_EN_MASK                  0xF
#define HDD_RTSCTS_ENABLE                   1
#define HDD_CTS_ENABLE                      2

#define HDD_AUTO_RATE_SGI    0x8

/* Packet Types. */
#define WLAN_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2
#define WLAN_KEEP_ALIVE_NULL_PKT              1

/*
 * Defines for fw_test command
 */
#define HDD_FWTEST_PARAMS 3
#define HDD_FWTEST_SU_PARAM_ID 53
#define HDD_FWTEST_MU_PARAM_ID 2
#define HDD_FWTEST_SU_DEFAULT_VALUE 100
#define HDD_FWTEST_MU_DEFAULT_VALUE 40
#define HDD_FWTEST_MAX_VALUE 500

#ifdef WLAN_WEXT_SUPPORT_ENABLE
/**
 * hdd_unregister_wext() - unregister wext context
 * @dev: net device handle
 *
 * Unregisters wext interface context for a given net device
 *
 * Returns: None
 */
void hdd_unregister_wext(struct net_device *dev);

/**
 * hdd_register_wext() - register wext context
 * @dev: net device handle
 *
 * Registers wext interface context for a given net device
 *
 * Returns: None
 */
void hdd_register_wext(struct net_device *dev);

/**
 * hdd_wext_unregister() - unregister wext context with rtnl lock dependency
 * @dev: net device from which wireless extensions are being unregistered
 * @rtnl_held: flag which indicates if caller is holding the rtnl_lock
 *
 * Unregisters wext context for a given net device. This behaves the
 * same as hdd_unregister_wext() except it does not take the rtnl_lock
 * if the caller is already holding it.
 *
 * Returns: None
 */
void hdd_wext_unregister(struct net_device *dev,
			 bool rtnl_held);

static inline
void hdd_wext_send_event(struct net_device *dev, unsigned int cmd,
			 union iwreq_data *wrqu, const char *extra)
{
	wireless_send_event(dev, cmd, wrqu, extra);
}

void hdd_wlan_get_stats(struct hdd_adapter *adapter, uint16_t *length,
		       char *buffer, uint16_t buf_len);
void hdd_wlan_list_fw_profile(uint16_t *length,
			      char *buffer, uint16_t buf_len);

int iw_set_var_ints_getnone(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra);

int iw_set_three_ints_getnone(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra);

int hdd_priv_get_data(struct iw_point *p_priv_data,
		     union iwreq_data *wrqu);

void *mem_alloc_copy_from_user_helper(const void *wrqu_data, size_t len);

/**
 * hdd_we_set_short_gi() - Set adapter Short GI
 * @adapter: adapter being modified
 * @sgi: new sgi value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_we_set_short_gi(struct hdd_adapter *adapter, int sgi);

/**
 * hdd_assemble_rate_code() - assemble rate code to be sent to FW
 * @preamble: rate preamble
 * @nss: number of streams
 * @rate: rate index
 *
 * Rate code assembling is different for targets which are 11ax capable.
 * Check for the target support and assemble the rate code accordingly.
 *
 * Return: assembled rate code
 */
int hdd_assemble_rate_code(uint8_t preamble, uint8_t nss, uint8_t rate);

/**
 * hdd_set_11ax_rate() - set 11ax rate
 * @adapter: adapter being modified
 * @value: new 11ax rate code
 * @sap_config: pointer to SAP config to check HW mode
 *              this will be NULL for call from STA persona
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_11ax_rate(struct hdd_adapter *adapter, int value,
		      struct sap_config *sap_config);

/**
 * hdd_we_update_phymode() - handle change in PHY mode
 * @adapter: adapter being modified
 * @new_phymode: new PHY mode for the device
 *
 * This function is called when the device is set to a new PHY mode.
 * It takes a holistic look at the desired PHY mode along with the
 * configured capabilities of the driver and the reported capabilities
 * of the hardware in order to correctly configure all PHY-related
 * parameters.
 *
 * Return: 0 on success, negative errno value on error
 */
int hdd_we_update_phymode(struct hdd_adapter *adapter, int new_phymode);

/**
 * wlan_hdd_update_btcoex_mode() - set BTCoex Mode
 * @adapter: adapter being modified
 * @value: new BTCoex mode for the adapter
 *
 * This function is called to set a BTCoex Operation Mode
 *
 * Return: 0 on success, negative errno value on error
 */
int wlan_hdd_set_btcoex_mode(struct hdd_adapter *adapter, int value);

/**
 * wlan_hdd_set_btcoex_rssi_threshold() - set RSSI threshold
 * @adapter: adapter being modified
 * @value: new RSSI Threshold for the adapter
 *
 * This function is called to set a new RSSI threshold for
 * change of Coex operating mode from TDD to FDD
 *
 * Return: 0 on success, negative errno value on error
 */
int wlan_hdd_set_btcoex_rssi_threshold(struct hdd_adapter *adapter, int value);

struct iw_request_info;

/**
 * hdd_check_private_wext_control() - Check to see if private
 *      wireless extensions ioctls are allowed
 * @hdd_ctx: Global HDD context
 * @info: Wireless extensions ioctl information passed by the kernel
 *
 * This function will examine the "private_wext_control" configuration
 * item to determine whether or not private wireless extensions ioctls
 * are allowed.
 *
 * Return: 0 if the ioctl is allowed to be processed, -ENOTSUPP if the
 * ioctls have been disabled. Note that in addition to returning
 * status, this function will log a message if the ioctls are disabled
 * or deprecated.
 */
int hdd_check_private_wext_control(struct hdd_context *hdd_ctx,
				   struct iw_request_info *info);

#ifdef CONFIG_DP_TRACE
void hdd_set_dump_dp_trace(uint16_t cmd_type, uint16_t count);
#else
static inline
void hdd_set_dump_dp_trace(uint16_t cmd_type, uint16_t count) {}
#endif
#else /* WLAN_WEXT_SUPPORT_ENABLE */

static inline void hdd_unregister_wext(struct net_device *dev)
{
}

static inline void hdd_register_wext(struct net_device *dev)
{
}

static inline void hdd_wext_unregister(struct net_device *dev,
				       bool rtnl_locked)
{
}

static inline
void hdd_wext_send_event(struct net_device *dev, unsigned int cmd,
			 union iwreq_data *wrqu, const char *extra)
{
}
#endif /* WLAN_WEXT_SUPPORT_ENABLE */

#if defined(WLAN_WEXT_SUPPORT_ENABLE) && defined(HASTINGS_BT_WAR)
int hdd_hastings_bt_war_enable_fw(struct hdd_context *hdd_ctx);
int hdd_hastings_bt_war_disable_fw(struct hdd_context *hdd_ctx);
#else
static inline
int hdd_hastings_bt_war_enable_fw(struct hdd_context *hdd_ctx)
{
	return -ENOTSUPP;
}

static inline
int hdd_hastings_bt_war_disable_fw(struct hdd_context *hdd_ctx)
{
	return -ENOTSUPP;
}

#endif

#endif /* __WEXT_IW_H__ */
